#include "internal.h"
#include <stdlib.h>

EcSys *ec_sys_new(int dim, int complex_mode)
{
    EcSys *s = calloc(1, sizeof(EcSys) + (size_t)dim * sizeof(int));
    s->dim = dim;
    s->complex_mode = complex_mode;
    size_t n = (size_t)(dim + 1) * (dim + 1);
    if (complex_mode) {
        s->Ac = calloc(n, sizeof(ec_cx));
        s->zc = calloc((size_t)dim + 1, sizeof(ec_cx));
    } else {
        s->A = calloc(n, sizeof(double));
        s->z = calloc((size_t)dim + 1, sizeof(double));
    }
    return s;
}

void ec_sys_free(EcSys *s)
{
    if (!s) return;
    free(s->A); free(s->Ac); free(s->z); free(s->zc);
    free(s);
}

void ec_sys_clear(EcSys *s, int n_nodes, double gmin)
{
    int d = s->dim;
    if (s->complex_mode) {
        memset(s->Ac, 0, (size_t)(d + 1) * (d + 1) * sizeof(ec_cx));
        memset(s->zc, 0, (size_t)(d + 1) * sizeof(ec_cx));
        for (int i = 1; i <= n_nodes && i <= d; i++)
            s->zc[i] = ec_cx_make(0, 0); /* no-op, keeps parity with real path */
    } else {
        memset(s->A, 0, (size_t)(d + 1) * (d + 1) * sizeof(double));
        memset(s->z, 0, (size_t)(d + 1) * sizeof(double));
    }
    if (gmin > 0.0) {
        for (int i = 1; i <= n_nodes && i <= d; i++) {
            if (s->complex_mode) s->Ac[(size_t)i * (d + 1) + i].re += gmin;
            else s->A[(size_t)i * (d + 1) + i] += gmin;
        }
    }
}

void ec_sys_add(EcSys *s, int row, int col, double re, double im)
{
    if (!s || row <= 0 || col <= 0 || row > s->dim || col > s->dim) return;
    int d = s->dim;
    if (s->complex_mode) {
        ec_cx *e = &s->Ac[(size_t)row * (d + 1) + col];
        e->re += re; e->im += im;
    } else {
        if (re != 0.0) s->A[(size_t)row * (d + 1) + col] += re;
    }
}

void ec_sys_rhs(EcSys *s, int row, double re, double im)
{
    if (!s || row <= 0 || row > s->dim) return;
    if (s->complex_mode) {
        s->zc[row].re += re; s->zc[row].im += im;
    } else {
        s->z[row] += re;
    }
}

int ec_sys_solve_real(EcSys *s, double *x)
{
    int n = s->dim;
    double *a = malloc((size_t)n * n * sizeof(double));
    for (int i = 1; i <= n; i++) {
        x[i] = s->z[i];
        for (int j = 1; j <= n; j++)
            a[(size_t)(i - 1) * n + (j - 1)] = s->A[(size_t)i * (n + 1) + j];
    }
    int rc = ec_lu_factor(a, n, s->piv);
    if (rc == 0) ec_lu_solve(a, n, s->piv, x + 1);
    free(a);
    return rc;
}

int ec_sys_solve_cx(EcSys *s, ec_cx *x)
{
    int n = s->dim;
    ec_cx *a = malloc((size_t)n * n * sizeof(ec_cx));
    for (int i = 1; i <= n; i++) {
        x[i] = s->zc[i];
        for (int j = 1; j <= n; j++)
            a[(size_t)(i - 1) * n + (j - 1)] = s->Ac[(size_t)i * (n + 1) + j];
    }
    int rc = ec_lu_factor_cx(a, n, s->piv);
    if (rc == 0) ec_lu_solve_cx(a, n, s->piv, x + 1);
    free(a);
    return rc;
}

void ec_reset_states(EcCircuit *cir)
{
    for (int i = 0; i < cir->n_comps; i++) {
        EcComp *c = cir->comps[i];
        const EcPlugin *p = c->plugin;
        if (p->destroy && c->state) p->destroy(c->state);
        c->state = p->create ? p->create() : NULL;
    }
}

static void stamp_all(EcCircuit *cir, const EcStampCtx *ctx, EcSys *sys)
{
    for (int i = 0; i < cir->n_comps; i++) {
        const EcPlugin *p = cir->comps[i]->plugin;
        if (ctx->mode == EC_MODE_AC && p->stamp_ac)
            p->stamp_ac(cir->comps[i], ctx, sys);
        else if (p->stamp)
            p->stamp(cir->comps[i], ctx, sys);
    }
}

int ec_newton_solve(EcCircuit *cir, const EcStampCtx *ctx, EcSys *sys,
                    double *x, int *n_iters)
{
    double *xn = calloc((size_t)sys->dim + 1, sizeof(double));
    int converged = 0, rc = 0;
    for (int it = 0; it < EC_MAX_ITER; it++) {
        EcStampCtx cctx = *ctx;
        cctx.x = x;
        ec_sys_clear(sys, cir->n_nodes, ctx->gmin);
        stamp_all(cir, &cctx, sys);
        int src = ec_sys_solve_real(sys, xn);
        if (src != 0) { rc = -2; break; }
        if (it > 0) {
            converged = 1;
            for (int k = 1; k <= sys->dim; k++) {
                double tol = EC_RELTOL * fmax(fabs(xn[k]), fabs(x[k]))
                           + (k <= cir->n_nodes ? EC_VNTOL : EC_ATOL);
                if (fabs(xn[k] - x[k]) > tol) { converged = 0; break; }
            }
            /* device-level residual: reject spurious convergence where the
             * solution stops moving but nonlinear devices are still far from
             * their linearization point */
            if (converged) {
                cctx.x = xn;
                for (int i = 0; i < cir->n_comps; i++) {
                    const EcPlugin *p = cir->comps[i]->plugin;
                    if (p->residual &&
                        p->residual(cir->comps[i], &cctx) > EC_RESIDUAL_TOL) {
                        converged = 0;
                        break;
                    }
                }
            }
        }
        memcpy(x, xn, (size_t)(sys->dim + 1) * sizeof(double));
        if (n_iters) (*n_iters)++;
        if (converged) break;
    }
    free(xn);
    return converged ? 0 : (rc ? rc : -1);
}
