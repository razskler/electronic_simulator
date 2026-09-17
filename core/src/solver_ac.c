/* elsim - AC small-signal analysis (linearized around the DC op point) */
#include "internal.h"
#include <stdio.h>
#include <math.h>

int ec_dc_op(EcCircuit *cir, double *x, char *msg, size_t msgsz); /* solver_dc.c */

int ec_analysis_ac(EcCircuit *cir, double f1, double f2, int npts,
                   int log_sweep, EcResults *out)
{
    memset(out, 0, sizeof *out);
    out->analysis = EC_AN_AC;
    if (cir->n_comps == 0) {
        snprintf(out->message, sizeof out->message, "empty circuit");
        return -1;
    }
    if (npts < 1) npts = 1;
    if (f1 <= 0.0 || f2 <= 0.0) {
        snprintf(out->message, sizeof out->message, "AC frequencies must be > 0");
        return -1;
    }
    if (log_sweep && (f1 <= 0.0 || f2 <= 0.0)) {
        snprintf(out->message, sizeof out->message, "log sweep needs positive frequencies");
        return -1;
    }

    out->dim = cir->n_nodes + cir->n_branches;
    out->n_nodes = cir->n_nodes;
    int dim = out->dim;

    /* operating point for linearizing nonlinear devices */
    double *op = calloc((size_t)dim + 1, sizeof(double));
    if (ec_dc_op(cir, op, out->message, sizeof out->message) != 0) {
        free(op);
        return -1;
    }

    EcSys *sys = ec_sys_new(dim, 1);
    EcStampCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.mode = EC_MODE_AC;
    ctx.n_nodes = cir->n_nodes;
    ctx.n_branches = cir->n_branches;
    ctx.dim = dim;
    ctx.x = op;
    ctx.gmin = 1e-12;

    out->n_pts = npts;
    out->cap = npts;
    out->sweep = calloc((size_t)npts, sizeof(double));
    out->xc = calloc((size_t)npts * (dim + 1), sizeof(ec_cx));
    ec_cx *x = calloc((size_t)dim + 1, sizeof(ec_cx));

    int rc = 0;
    for (int i = 0; i < npts; i++) {
        double f = (npts == 1) ? f1
            : log_sweep ? f1 * pow(f2 / f1, (double)i / (npts - 1))
                        : f1 + (f2 - f1) * (double)i / (npts - 1);
        out->sweep[i] = f;
        ctx.omega = 2.0 * M_PI * f;

        ec_sys_clear(sys, cir->n_nodes, ctx.gmin);
        for (int k = 0; k < cir->n_comps; k++) {
            const EcPlugin *p = cir->comps[k]->plugin;
            if (p->stamp_ac) p->stamp_ac(cir->comps[k], &ctx, sys);
            else if (p->stamp) p->stamp(cir->comps[k], &ctx, sys);
        }
        if (ec_sys_solve_cx(sys, x) != 0) {
            snprintf(out->message, sizeof out->message,
                     "singular AC matrix at %.4g Hz", f);
            rc = -1;
            break;
        }
        memcpy(&out->xc[(size_t)i * (dim + 1)], x,
               (size_t)(dim + 1) * sizeof(ec_cx));
    }

    free(x);
    free(op);
    ec_sys_free(sys);
    return rc;
}
