/*
 * elsim - plugin ABI v1
 *
 * Every component type (built-in or dynamically loaded) implements this
 * interface. A component stamps its contribution into the Modified Nodal
 * Analysis (MNA) system through the ec_sys_add()/ec_sys_rhs() helpers.
 *
 * Dynamically loaded plugins are shared objects (.so / .dll) placed in a
 * plugins/ directory that export:
 *
 *     const EcPlugin *ec_plugin_export(void);
 *
 * Matrix layout: unknowns are x[1..n_nodes] (node voltages, node 0 is
 * ground) followed by x[n_nodes+1..n_nodes+n_branches] (branch currents).
 * A component that reserved branch k stamps at row/col
 * (n_nodes + c->branch[k] + 1).
 *
 * Limits (keep the ABI simple): max 8 pins, 16 params, 4 branches.
 */
#ifndef EC_PLUGIN_H
#define EC_PLUGIN_H

#include <stdint.h>

#define EC_ABI_VERSION 1
#define EC_MAX_PINS 8
#define EC_MAX_PARAMS 16
#define EC_MAX_BRANCHES 4

#if defined(_WIN32) && defined(EC_PLUGIN_BUILD)
  #define EC_EXPORT __declspec(dllexport)
#else
  #define EC_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
    EC_MODE_DC = 0, /* operating point; nonlinear devices use ctx->x */
    EC_MODE_AC = 1, /* small-signal; linearized around the DC op point in ctx->x */
    EC_MODE_TR = 2  /* transient; companion models use ctx->x_prev and ctx->dt */
};

enum {
    EC_METHOD_BE = 0,   /* backward euler */
    EC_METHOD_TRAP = 1  /* trapezoidal (2nd order) */
};

typedef struct EcComp EcComp;
typedef struct EcSys EcSys;

typedef struct EcStampCtx {
    int mode;             /* EC_MODE_* */
    int n_nodes;          /* count of non-ground nodes */
    int n_branches;       /* total branch count */
    int dim;              /* n_nodes + n_branches */
    const double *x;      /* latest Newton guess / accepted op point (AC) */
    const double *x_prev; /* previous accepted timepoint (TR only, may be NULL) */
    double dt;            /* timestep (TR) */
    double omega;         /* angular frequency (AC) */
    int method;           /* EC_METHOD_* (TR) */
    double time;          /* simulation time (TR) */
    double gmin;          /* minimum conductance the device should clamp to */
} EcStampCtx;

/* Stamp helpers. Row/column 0 (ground) is silently ignored, so plugins can
 * stamp unconditionally. For real modes the imaginary part is ignored. */
void ec_sys_add(EcSys *sys, int row, int col, double re, double im);
void ec_sys_rhs(EcSys *sys, int row, double re, double im);

struct EcComp {
    const struct EcPlugin *plugin;
    char name[32];
    int nodes[EC_MAX_PINS];       /* node id per pin; 0 = ground */
    double params[EC_MAX_PARAMS]; /* values matching plugin->param_names */
    int branch[EC_MAX_BRANCHES];  /* 0-based branch ids, assigned at finalize */
    void *state;                  /* plugin-private instance data */
    char *meta;                   /* free-form GUI metadata (owned), core ignores */
    uint64_t uid;
};

typedef struct EcPlugin {
    uint32_t abi;            /* must equal EC_ABI_VERSION */
    const char *id;          /* unique registry key, e.g. "resistor" */
    const char *label;       /* display name, e.g. "Resistor" */
    const char *category;    /* palette group, e.g. "Passive" */
    int n_pins;
    const char *pin_names[EC_MAX_PINS];     /* e.g. {"p","n"} */
    int n_params;
    const char *param_names[EC_MAX_PARAMS]; /* e.g. {"R"} */
    double param_defaults[EC_MAX_PARAMS];
    const char *param_units[EC_MAX_PARAMS]; /* e.g. "ohm", "V"; entries may be NULL */
    int n_branches;          /* extra MNA rows this component type needs */
    int nonlinear;           /* nonzero: re-stamped every Newton iteration */
    const char *symbol;      /* GUI drawing hint: "resistor","capacitor",
                                "inductor","vsource","isource","diode","led",
                                "switch","ground","box" */
    void *(*create)(void);   /* allocate per-instance state; may be NULL */
    void (*destroy)(void *state);          /* may be NULL */
    void (*stamp)(EcComp *c, const EcStampCtx *ctx, EcSys *sys);     /* required */
    void (*stamp_ac)(EcComp *c, const EcStampCtx *ctx, EcSys *sys);  /* optional; NULL = DC stamp */
    void (*commit)(EcComp *c, const EcStampCtx *ctx); /* called after each
                                accepted TR step; update state from ctx->x */
    /* Optional, nonlinear devices only: with ctx->x set to the candidate
     * solution, return |i_true(x) - i_linearized(x)| in amperes. Used by
     * the Newton loop to reject spurious convergence. */
    double (*residual)(const EcComp *c, const EcStampCtx *ctx);
} EcPlugin;

/* Find a parameter index by name; returns -1 if absent. */
int ec_param_idx(const EcPlugin *p, const char *name);

/* MNA row for branch k of component c. */
#define EC_BRANCH_ROW(ctx, c, k) (((ctx)->n_nodes + (c)->branch[k] + 1))

#ifdef __cplusplus
}
#endif

#endif /* EC_PLUGIN_H */
