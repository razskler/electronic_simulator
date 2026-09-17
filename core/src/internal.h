/* elsim - internal shared declarations (not part of the public API) */
#ifndef EC_INTERNAL_H
#define EC_INTERNAL_H

#include "ec/circuit.h"
#include "ec/plugin.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct EcSys {
    int dim;
    int complex_mode;
    double *A;  /* (dim+1)^2, 1-based rows/cols; index: r*(dim+1)+c */
    ec_cx *Ac;
    double *z;  /* dim+1 */
    ec_cx *zc;
    int piv[1]; /* over-allocated */
};

EcSys *ec_sys_new(int dim, int complex_mode);
void ec_sys_free(EcSys *s);
/* Zero the system and add gmin to node diagonals (rows 1..n_nodes). */
void ec_sys_clear(EcSys *s, int n_nodes, double gmin);
/* Solve in place; x is length dim+1 (x[0] unused). 0 ok, -1 singular. */
int ec_sys_solve_real(EcSys *s, double *x);
int ec_sys_solve_cx(EcSys *s, ec_cx *x);

/* Common Newton loop shared by DC and TR. Stamps every iteration.
 * Returns 0 converged, -1 no convergence, -2 singular matrix. */
int ec_newton_solve(EcCircuit *cir, const EcStampCtx *ctx, EcSys *sys,
                    double *x, int *n_iters);

/* Reset all component states (call before each analysis run). */
void ec_reset_states(EcCircuit *cir);

#endif /* EC_INTERNAL_H */
