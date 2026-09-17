/* elsim - dense LU solvers (real and complex) */
#ifndef EC_MATRIX_H
#define EC_MATRIX_H

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double re, im;
} ec_cx;

static inline ec_cx ec_cx_make(double re, double im) { ec_cx r = { re, im }; return r; }
static inline ec_cx ec_cx_add(ec_cx a, ec_cx b) { return ec_cx_make(a.re + b.re, a.im + b.im); }
static inline ec_cx ec_cx_sub(ec_cx a, ec_cx b) { return ec_cx_make(a.re - b.re, a.im - b.im); }
static inline ec_cx ec_cx_mul(ec_cx a, ec_cx b) {
    return ec_cx_make(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}
static inline ec_cx ec_cx_div(ec_cx a, ec_cx b) {
    double d = b.re * b.re + b.im * b.im;
    return ec_cx_make((a.re * b.re + a.im * b.im) / d, (a.im * b.re - a.re * b.im) / d);
}
static inline double ec_cx_abs(ec_cx a) { return hypot(a.re, a.im); }

/* Factor A (n*n row-major) in place with partial pivoting. piv[i] = original
 * index of the row that ended up at i. Returns 0, or -1 if singular. */
int ec_lu_factor(double *A, int n, int *piv);
/* Solve using factored A; b (length n) is replaced by the solution. */
void ec_lu_solve(const double *A, int n, const int *piv, double *b);

int ec_lu_factor_cx(ec_cx *A, int n, int *piv);
void ec_lu_solve_cx(const ec_cx *A, int n, const int *piv, ec_cx *b);

#ifdef __cplusplus
}
#endif

#endif /* EC_MATRIX_H */
