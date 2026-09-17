/* elsim - plugin registry + analysis orchestration */
#ifndef EC_CIRCUIT_H
#define EC_CIRCUIT_H

#include "ec/netlist.h"
#include "ec/matrix.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- registry ---------------- */

typedef struct EcPluginRegistry {
    EcPlugin **plugins;
    int n, cap;
} EcPluginRegistry;

EcPluginRegistry *ec_registry_new(void);
void ec_registry_free(EcPluginRegistry *reg); /* does not unload plugin code */
void ec_registry_add(EcPluginRegistry *reg, const EcPlugin *p);
EcPlugin *ec_registry_find(EcPluginRegistry *reg, const char *id);

/* Register the built-in component set (resistor, capacitor, inductor,
 * vsource, isource, diode, ground). */
void ec_register_builtins(EcPluginRegistry *reg);

/* Load every shared object in dir that exports ec_plugin_export().
 * Returns the number of plugins loaded; malformed plugins are skipped.
 * err (optional) receives the first error description. */
int ec_registry_load_dir(EcPluginRegistry *reg, const char *dir,
                         char *err, size_t errsz);

/* ---------------- analyses ---------------- */

enum { EC_AN_DC = 0, EC_AN_AC = 1, EC_AN_TR = 2 };

#define EC_MAX_STORED_POINTS 20000

typedef struct EcResults {
    int analysis;  /* EC_AN_* */
    int dim;       /* n_nodes + n_branches */
    int n_nodes;
    int n_pts, cap;
    double *sweep; /* time (TR) or frequency (AC) per point */
    double *x;     /* DC/TR solutions, [pt*(dim+1) + k], k=0 unused */
    ec_cx *xc;     /* AC complex solutions, same layout */
    char message[256];
} EcResults;

void ec_results_free(EcResults *r);

/* Node voltage of stored point (DC/TR). For AC returns the magnitude. */
double ec_results_v(const EcResults *r, int pt, int node);
/* AC phase in degrees. */
double ec_results_phase(const EcResults *r, int pt, int node);
/* Branch current of stored point (1-based branch id). */
double ec_results_i(const EcResults *r, int pt, int branch);

typedef void (*ec_progress_fn)(void *user, double frac);

/* All analyses start from zero initial conditions (states are reset).
 * Return 0 on success, -1 on failure (message in out->message). */
int ec_analysis_dc(EcCircuit *cir, EcResults *out);
int ec_analysis_ac(EcCircuit *cir, double f1, double f2, int npts,
                   int log_sweep, EcResults *out);
int ec_analysis_tr(EcCircuit *cir, double tstop, double dt, int method,
                   ec_progress_fn progress, void *user, EcResults *out);

/* Shared solver tolerances. */
#define EC_RELTOL 1e-3
#define EC_VNTOL 1e-6
#define EC_ATOL 1e-12
#define EC_MAX_ITER 200
#define EC_RESIDUAL_TOL 1e-9 /* max device current mismatch at convergence */

#ifdef __cplusplus
}
#endif

#endif /* EC_CIRCUIT_H */
