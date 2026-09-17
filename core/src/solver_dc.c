/* elsim - DC operating point analysis */
#include "internal.h"
#include <stdio.h>

static const double gmin_ladder[] = { 1e-12, 1e-9, 1e-6, 1e-3 };

static int run_dc(EcCircuit *cir, double *x, char *msg, size_t msgsz)
{
    int dim = cir->n_nodes + cir->n_branches;
    EcSys *sys = ec_sys_new(dim, 0);
    EcStampCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.mode = EC_MODE_DC;
    ctx.n_nodes = cir->n_nodes;
    ctx.n_branches = cir->n_branches;
    ctx.dim = dim;

    int rc = -1;
    for (size_t i = 0; i < sizeof gmin_ladder / sizeof gmin_ladder[0]; i++) {
        ctx.gmin = gmin_ladder[i];
        memset(x, 0, (size_t)(dim + 1) * sizeof(double));
        ec_reset_states(cir);
        int iters = 0;
        rc = ec_newton_solve(cir, &ctx, sys, x, &iters);
        if (rc == 0) break;
        if (rc == -2 && i + 1 == sizeof gmin_ladder / sizeof gmin_ladder[0])
            snprintf(msg, msgsz, "singular matrix: check that the circuit has a ground "
                                 "and no floating nodes");
    }
    if (rc == 0) {
        ctx.x = x;
        for (int i = 0; i < cir->n_comps; i++) {
            const EcPlugin *p = cir->comps[i]->plugin;
            if (p->commit) p->commit(cir->comps[i], &ctx);
        }
    } else if (rc == -1) {
        snprintf(msg, msgsz, "DC analysis failed to converge");
    }
    ec_sys_free(sys);
    return rc;
}

int ec_analysis_dc(EcCircuit *cir, EcResults *out)
{
    memset(out, 0, sizeof *out);
    out->analysis = EC_AN_DC;
    if (cir->n_comps == 0) {
        snprintf(out->message, sizeof out->message, "empty circuit");
        return -1;
    }
    out->dim = cir->n_nodes + cir->n_branches;
    out->n_nodes = cir->n_nodes;
    double *x = calloc((size_t)out->dim + 1, sizeof(double));
    if (run_dc(cir, x, out->message, sizeof out->message) != 0) {
        free(x);
        return -1;
    }
    out->n_pts = 1;
    out->cap = 1;
    out->sweep = calloc(1, sizeof(double));
    out->x = x;
    return 0;
}

/* Internal helper used by AC (op point with DC sources). */
int ec_dc_op(EcCircuit *cir, double *x, char *msg, size_t msgsz)
{
    return run_dc(cir, x, msg, msgsz);
}
