#include "ec/matrix.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int ec_lu_factor(double *A, int n, int *piv)
{
    for (int k = 0; k < n; k++) piv[k] = k;
    for (int k = 0; k < n; k++) {
        int p = k;
        double mx = fabs(A[k * n + k]);
        for (int i = k + 1; i < n; i++) {
            double v = fabs(A[i * n + k]);
            if (v > mx) { mx = v; p = i; }
        }
        if (mx == 0.0) return -1;
        if (p != k) {
            for (int j = 0; j < n; j++) {
                double t = A[k * n + j]; A[k * n + j] = A[p * n + j]; A[p * n + j] = t;
            }
            int t = piv[k]; piv[k] = piv[p]; piv[p] = t;
        }
        double d = A[k * n + k];
        for (int i = k + 1; i < n; i++) {
            double m = A[i * n + k] / d;
            A[i * n + k] = m;
            if (m != 0.0) {
                for (int j = k + 1; j < n; j++)
                    A[i * n + j] -= m * A[k * n + j];
            }
        }
    }
    return 0;
}

void ec_lu_solve(const double *A, int n, const int *piv, double *b)
{
    double *y = malloc((size_t)n * sizeof(double));
    for (int i = 0; i < n; i++) y[i] = b[piv[i]];
    for (int i = 0; i < n; i++)
        for (int j = 0; j < i; j++)
            y[i] -= A[i * n + j] * y[j];
    for (int i = n - 1; i >= 0; i--) {
        for (int j = i + 1; j < n; j++)
            y[i] -= A[i * n + j] * y[j];
        y[i] /= A[i * n + i];
    }
    memcpy(b, y, (size_t)n * sizeof(double));
    free(y);
}

int ec_lu_factor_cx(ec_cx *A, int n, int *piv)
{
    for (int k = 0; k < n; k++) piv[k] = k;
    for (int k = 0; k < n; k++) {
        int p = k;
        double mx = ec_cx_abs(A[k * n + k]);
        for (int i = k + 1; i < n; i++) {
            double v = ec_cx_abs(A[i * n + k]);
            if (v > mx) { mx = v; p = i; }
        }
        if (mx == 0.0) return -1;
        if (p != k) {
            for (int j = 0; j < n; j++) {
                ec_cx t = A[k * n + j]; A[k * n + j] = A[p * n + j]; A[p * n + j] = t;
            }
            int t = piv[k]; piv[k] = piv[p]; piv[p] = t;
        }
        ec_cx d = A[k * n + k];
        for (int i = k + 1; i < n; i++) {
            ec_cx m = ec_cx_div(A[i * n + k], d);
            A[i * n + k] = m;
            if (m.re != 0.0 || m.im != 0.0) {
                for (int j = k + 1; j < n; j++)
                    A[i * n + j] = ec_cx_sub(A[i * n + j], ec_cx_mul(m, A[k * n + j]));
            }
        }
    }
    return 0;
}

void ec_lu_solve_cx(const ec_cx *A, int n, const int *piv, ec_cx *b)
{
    ec_cx *y = malloc((size_t)n * sizeof(ec_cx));
    for (int i = 0; i < n; i++) y[i] = b[piv[i]];
    for (int i = 0; i < n; i++)
        for (int j = 0; j < i; j++)
            y[i] = ec_cx_sub(y[i], ec_cx_mul(A[i * n + j], y[j]));
    for (int i = n - 1; i >= 0; i--) {
        for (int j = i + 1; j < n; j++)
            y[i] = ec_cx_sub(y[i], ec_cx_mul(A[i * n + j], y[j]));
        y[i] = ec_cx_div(y[i], A[i * n + i]);
    }
    memcpy(b, y, (size_t)n * sizeof(ec_cx));
    free(y);
}
