/* elsim - transient analysis (backward euler / trapezoidal)
 *
 * Semantics (day 1): every transient run starts from zero initial
 * conditions - capacitor voltages and inductor currents are reset.
 */
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int ec_analysis_tr(EcCircuit *cir, double tstop, double dt, int method,
                   ec_progress_fn progress, void *user, EcResults *out)
{
    memset(out, 0, sizeof *out);
    out->analysis = EC_AN_TR;
    if (cir->n_comps == 0) {
        snprintf(out->message, sizeof out->message, "empty circuit");
        return -1;
    }
    if (tstop <= 0.0 || dt <= 0.0) {
        snprintf(out->message, sizeof out->message, "tstop and dt must be > 0");
        return -1;
    }

    out->dim = cir->n_nodes + cir->n_branches;
    out->n_nodes = cir->n_nodes;
    int dim = out->dim;
    long n_steps = (long)ceil(tstop / dt - 1e-9);
    if (n_steps < 1) n_steps = 1;
    double dt_eff = tstop / (double)n_steps; /* exact fit */
    int dec = (int)((n_steps + 1 + EC_MAX_STORED_POINTS - 1) / EC_MAX_STORED_POINTS);
    if (dec < 1) dec = 1;

    long cap = (n_steps + dec) / dec + 2;
    out->sweep = calloc((size_t)cap, sizeof(double));
    out->x = calloc((size_t)cap * (dim + 1), sizeof(double));

    ec_reset_states(cir);

    EcSys *sys = ec_sys_new(dim, 0);
    EcStampCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.mode = EC_MODE_TR;
    ctx.n_nodes = cir->n_nodes;
    ctx.n_branches = cir->n_branches;
    ctx.dim = dim;
    ctx.method = method;
    ctx.dt = dt_eff;
    ctx.gmin = 1e-12;

    double *x = calloc((size_t)dim + 1, sizeof(double));   /* accepted solution */
    double *x_prev = calloc((size_t)dim + 1, sizeof(double));
    long stored = 0;
    /* no t=0 row: the network is solved from the first step onward with the
     * sources applied and the reactive components at zero state */

    int rc = 0;
    for (long step = 1; step <= n_steps; step++) {
        ctx.time = (double)step * dt_eff;
        ctx.x_prev = x_prev;
        memcpy(x, x_prev, (size_t)(dim + 1) * sizeof(double));

        int iters = 0;
        rc = ec_newton_solve(cir, &ctx, sys, x, &iters);
        if (rc != 0) {
            if (rc == -2)
                snprintf(out->message, sizeof out->message,
                         "singular matrix at t=%.6g s (check ground/wiring)", ctx.time);
            else
                snprintf(out->message, sizeof out->message,
                         "no convergence at t=%.6g s", ctx.time);
            break;
        }

        ctx.x = x;
        for (int i = 0; i < cir->n_comps; i++) {
            const EcPlugin *p = cir->comps[i]->plugin;
            if (p->commit) p->commit(cir->comps[i], &ctx);
        }
        memcpy(x_prev, x, (size_t)(dim + 1) * sizeof(double));

        if (step % (long)dec == 0 || step == n_steps) {
            out->sweep[stored] = ctx.time;
            memcpy(&out->x[stored * (dim + 1)], x, (size_t)(dim + 1) * sizeof(double));
            stored++;
        }
        if (progress && (step & 0xFF) == 0)
            progress(user, (double)step / (double)n_steps);
    }

    out->n_pts = (int)stored;
    out->cap = (int)stored;
    if (progress) progress(user, 1.0);

    free(x);
    free(x_prev);
    ec_sys_free(sys);
    return rc;
}
