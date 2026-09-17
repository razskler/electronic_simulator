/* elsim - results accessors */
#include "internal.h"
#include <math.h>

void ec_results_free(EcResults *r)
{
    if (!r) return;
    free(r->sweep);
    free(r->x);
    free(r->xc);
    memset(r, 0, sizeof *r);
}

double ec_results_v(const EcResults *r, int pt, int node)
{
    if (!r || pt < 0 || pt >= r->n_pts || node < 1 || node > r->dim) return 0.0;
    int stride = r->dim + 1;
    if (r->analysis == EC_AN_AC)
        return ec_cx_abs(r->xc[(size_t)pt * stride + node]);
    return r->x[(size_t)pt * stride + node];
}

double ec_results_phase(const EcResults *r, int pt, int node)
{
    if (!r || pt < 0 || pt >= r->n_pts || r->analysis != EC_AN_AC ||
        node < 1 || node > r->dim) return 0.0;
    ec_cx v = r->xc[(size_t)pt * (r->dim + 1) + node];
    return atan2(v.im, v.re) * 180.0 / M_PI;
}

double ec_results_i(const EcResults *r, int pt, int branch)
{
    if (!r || pt < 0 || pt >= r->n_pts) return 0.0;
    int row = r->n_nodes + branch + 1;
    if (row < 1 || row > r->dim) return 0.0;
    int stride = r->dim + 1;
    if (r->analysis == EC_AN_AC)
        return ec_cx_abs(r->xc[(size_t)pt * stride + row]);
    return r->x[(size_t)pt * stride + row];
}
