/* elsim - built-in component library
 *
 * Every built-in is a regular EcPlugin; they are registered directly instead
 * of being dlopen'ed. External plugins (see plugins/) implement the exact
 * same interface.
 */
#include "internal.h"

/* ---------------- resistor ---------------- */

static void r_stamp(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    (void)ctx;
    double r = c->params[0];
    if (r <= 0.0) r = 1e-12;
    double g = 1.0 / r;
    int p = c->nodes[0], n = c->nodes[1];
    ec_sys_add(sys, p, p, g, 0);
    ec_sys_add(sys, n, n, g, 0);
    ec_sys_add(sys, p, n, -g, 0);
    ec_sys_add(sys, n, p, -g, 0);
}

static const EcPlugin resistor_plugin = {
    .abi = EC_ABI_VERSION,
    .id = "resistor", .label = "Resistor", .category = "Passive",
    .n_pins = 2, .pin_names = { "p", "n" },
    .n_params = 1,
    .param_names = { "R" }, .param_defaults = { 1000.0 },
    .param_units = { "ohm" },
    .symbol = "resistor",
    .stamp = r_stamp, .stamp_ac = r_stamp,
};

/* ---------------- capacitor ----------------
 * state: previous voltage across, previous current (p->n)
 * The first transient step always uses the backward-euler companion so the
 * zero initial state is physically consistent (trapezoidal needs i(0),
 * which the network - not the state - determines). */

typedef struct { double v, i; int first; } CapState;

static void *cap_create(void)
{
    CapState *s = calloc(1, sizeof(CapState));
    s->first = 1;
    return s;
}
static void cap_destroy(void *s) { free(s); }

static void cap_stamp(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    int p = c->nodes[0], n = c->nodes[1];
    double C = c->params[0];
    if (ctx->mode == EC_MODE_DC) return; /* open circuit (gmin covers it) */
    if (ctx->mode == EC_MODE_AC) {
        ec_sys_add(sys, p, p, 0, ctx->omega * C);
        ec_sys_add(sys, n, n, 0, ctx->omega * C);
        ec_sys_add(sys, p, n, 0, -ctx->omega * C);
        ec_sys_add(sys, n, p, 0, -ctx->omega * C);
        return;
    }
    /* transient companion model */
    CapState *st = c->state;
    double geq, ieq;
    if (ctx->method == EC_METHOD_TRAP && !st->first) {
        geq = 2.0 * C / ctx->dt;
        ieq = geq * st->v + st->i;
    } else { /* backward euler (also the first step of a trap run) */
        geq = C / ctx->dt;
        ieq = geq * st->v;
    }
    ec_sys_add(sys, p, p, geq, 0);
    ec_sys_add(sys, n, n, geq, 0);
    ec_sys_add(sys, p, n, -geq, 0);
    ec_sys_add(sys, n, p, -geq, 0);
    ec_sys_rhs(sys, p, ieq, 0);
    ec_sys_rhs(sys, n, -ieq, 0);
}

static void cap_stamp_ac(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    int p = c->nodes[0], n = c->nodes[1];
    double C = c->params[0];
    ec_sys_add(sys, p, p, 0, ctx->omega * C);
    ec_sys_add(sys, n, n, 0, ctx->omega * C);
    ec_sys_add(sys, p, n, 0, -ctx->omega * C);
    ec_sys_add(sys, n, p, 0, -ctx->omega * C);
}

static void cap_commit(EcComp *c, const EcStampCtx *ctx)
{
    if (ctx->mode != EC_MODE_TR) return;
    CapState *st = c->state;
    double C = c->params[0];
    double v_new = ctx->x[c->nodes[0]] - ctx->x[c->nodes[1]];
    if (ctx->method == EC_METHOD_TRAP && !st->first) {
        double geq = 2.0 * C / ctx->dt;
        double ieq = geq * st->v + st->i; /* same companion source as stamp */
        st->i = geq * v_new - ieq;        /* actual current p->n */
    } else {
        double geq = C / ctx->dt;
        st->i = geq * (v_new - st->v);
        st->first = 0; /* subsequent steps may use trapezoidal */
    }
    st->v = v_new;
}

static const EcPlugin capacitor_plugin = {
    .abi = EC_ABI_VERSION,
    .id = "capacitor", .label = "Capacitor", .category = "Passive",
    .n_pins = 2, .pin_names = { "p", "n" },
    .n_params = 1,
    .param_names = { "C" }, .param_defaults = { 1e-6 },
    .param_units = { "F" },
    .symbol = "capacitor",
    .create = cap_create, .destroy = cap_destroy,
    .stamp = cap_stamp, .stamp_ac = cap_stamp_ac, .commit = cap_commit,
};

/* ---------------- inductor ----------------
 * state: previous current (branch), previous voltage; first transient step
 * uses backward euler for the same consistency reason as the capacitor. */

typedef struct { double i, v; int first; } IndState;

static void *ind_create(void)
{
    IndState *s = calloc(1, sizeof(IndState));
    s->first = 1;
    return s;
}
static void ind_destroy(void *s) { free(s); }

static void ind_stamp(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    int p = c->nodes[0], n = c->nodes[1];
    int r = EC_BRANCH_ROW(ctx, c, 0);
    double L = c->params[0];
    IndState *st = c->state;

    ec_sys_add(sys, p, r, 1, 0);
    ec_sys_add(sys, r, p, 1, 0);
    ec_sys_add(sys, n, r, -1, 0);
    ec_sys_add(sys, r, n, -1, 0);

    if (ctx->mode == EC_MODE_AC) {
        ec_sys_add(sys, r, r, 0, -ctx->omega * L); /* v = jwL i */
        return;
    }
    if (ctx->mode == EC_MODE_DC) {
        ec_sys_rhs(sys, r, 0, 0); /* ideal short: v = 0 */
        return;
    }
    double R, rhs;
    if (ctx->method == EC_METHOD_TRAP && !((IndState *)c->state)->first) {
        R = 2.0 * L / ctx->dt;
        rhs = -(R * st->i + st->v);
    } else {
        R = L / ctx->dt;
        rhs = -R * st->i;
    }
    ec_sys_add(sys, r, r, -R, 0);
    ec_sys_rhs(sys, r, rhs, 0);
}

static void ind_commit(EcComp *c, const EcStampCtx *ctx)
{
    if (ctx->mode != EC_MODE_TR) return;
    IndState *st = c->state;
    st->i = ctx->x[EC_BRANCH_ROW(ctx, c, 0)];
    st->v = ctx->x[c->nodes[0]] - ctx->x[c->nodes[1]];
    st->first = 0;
}

static const EcPlugin inductor_plugin = {
    .abi = EC_ABI_VERSION,
    .id = "inductor", .label = "Inductor", .category = "Passive",
    .n_pins = 2, .pin_names = { "p", "n" },
    .n_params = 1,
    .param_names = { "L" }, .param_defaults = { 1e-3 },
    .param_units = { "H" },
    .n_branches = 1,
    .symbol = "inductor",
    .create = ind_create, .destroy = ind_destroy,
    .stamp = ind_stamp, .commit = ind_commit,
};

/* ---------------- voltage source ---------------- */

static void v_stamp(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    int p = c->nodes[0], n = c->nodes[1];
    int r = EC_BRANCH_ROW(ctx, c, 0);
    double e = c->params[0]; /* dc value (constant in time, day 1) */
    ec_sys_add(sys, p, r, 1, 0);
    ec_sys_add(sys, r, p, 1, 0);
    ec_sys_add(sys, n, r, -1, 0);
    ec_sys_add(sys, r, n, -1, 0);
    ec_sys_rhs(sys, r, e, 0);
}

static void v_stamp_ac(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    int p = c->nodes[0], n = c->nodes[1];
    int r = EC_BRANCH_ROW(ctx, c, 0);
    double ac = c->params[1];
    ec_sys_add(sys, p, r, 1, 0);
    ec_sys_add(sys, r, p, 1, 0);
    ec_sys_add(sys, n, r, -1, 0);
    ec_sys_add(sys, r, n, -1, 0);
    ec_sys_rhs(sys, r, ac, 0);
}

static const EcPlugin vsource_plugin = {
    .abi = EC_ABI_VERSION,
    .id = "vsource", .label = "Voltage source", .category = "Sources",
    .n_pins = 2, .pin_names = { "p", "n" },
    .n_params = 2,
    .param_names = { "dc", "ac" }, .param_defaults = { 5.0, 1.0 },
    .param_units = { "V", "V" },
    .n_branches = 1,
    .symbol = "vsource",
    .stamp = v_stamp, .stamp_ac = v_stamp_ac,
};

/* ---------------- current source ----------------
 * Positive dc value injects current into pin p. */

static void i_stamp(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    (void)ctx;
    ec_sys_rhs(sys, c->nodes[0], c->params[0], 0);
    ec_sys_rhs(sys, c->nodes[1], -c->params[0], 0);
}

static void i_stamp_ac(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    (void)ctx;
    ec_sys_rhs(sys, c->nodes[0], c->params[1], 0);
    ec_sys_rhs(sys, c->nodes[1], -c->params[1], 0);
}

static const EcPlugin isource_plugin = {
    .abi = EC_ABI_VERSION,
    .id = "isource", .label = "Current source", .category = "Sources",
    .n_pins = 2, .pin_names = { "p", "n" },
    .n_params = 2,
    .param_names = { "dc", "ac" }, .param_defaults = { 1e-3, 1e-3 },
    .param_units = { "A", "A" },
    .symbol = "isource",
    .stamp = i_stamp, .stamp_ac = i_stamp_ac,
};

/* ---------------- diode ----------------
 * Id = Is (exp(V/(N Vt)) - 1), Newton-linearized with step limiting. */

#define EC_VT 0.025852 /* kT/q at 300 K */

typedef struct { double vlast, geq, ieq; } DiodeState;

static void *d_create(void) { return calloc(1, sizeof(DiodeState)); }
static void d_destroy(void *s) { free(s); }

static double diode_limit(double vnew, double vold, double vte)
{
    if (vnew > vold + 2.0 * vte) return vold + 2.0 * vte;
    if (vnew < vold - 2.0 * vte) return vold - 2.0 * vte;
    return vnew;
}

static void diode_stamps(EcComp *c, const EcStampCtx *ctx, EcSys *sys, int ac)
{
    int p = c->nodes[0], n = c->nodes[1];
    double Is = c->params[0], Nn = c->params[1];
    double vte = Nn * EC_VT;
    DiodeState *st = c->state;
    double v = ctx->x ? ctx->x[p] - ctx->x[n] : 0.0;

    if (!ac && st) {
        v = diode_limit(v, st->vlast, vte);
        st->vlast = v;
    }
    double arg = v / vte;
    if (arg > 200.0) arg = 200.0;
    if (arg < -200.0) arg = -200.0;
    double e = exp(arg);
    double geq = Is * e / vte;
    if (geq < ctx->gmin) geq = ctx->gmin;
    double i = Is * (e - 1.0);
    /* companion source J = geq*v0 - i0 so that i_lin(v) = geq*v - J passes
     * through the operating point (v0, i0); stamped rhs[p] += J (same
     * convention as the capacitor companion) */
    double ieq = geq * v - i;
    if (st) { st->geq = geq; st->ieq = ieq; }

    if (ac) {
        ec_sys_add(sys, p, p, geq, 0);
        ec_sys_add(sys, n, n, geq, 0);
        ec_sys_add(sys, p, n, -geq, 0);
        ec_sys_add(sys, n, p, -geq, 0);
        return;
    }
    ec_sys_add(sys, p, p, geq, 0);
    ec_sys_add(sys, n, n, geq, 0);
    ec_sys_add(sys, p, n, -geq, 0);
    ec_sys_add(sys, n, p, -geq, 0);
    ec_sys_rhs(sys, p, ieq, 0);
    ec_sys_rhs(sys, n, -ieq, 0);
}

static double d_residual(const EcComp *c, const EcStampCtx *ctx)
{
    const DiodeState *st = c->state;
    if (!st) return 0.0;
    double Is = c->params[0], Nn = c->params[1];
    double vte = Nn * EC_VT;
    double v = ctx->x ? ctx->x[c->nodes[0]] - ctx->x[c->nodes[1]] : 0.0;
    double arg = v / vte;
    if (arg > 200.0) arg = 200.0;
    if (arg < -200.0) arg = -200.0;
    double itrue = Is * (exp(arg) - 1.0);
    double ilin = st->geq * v - st->ieq;
    return fabs(itrue - ilin);
}

static void d_stamp(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    diode_stamps(c, ctx, sys, 0);
}

static void d_stamp_ac(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    diode_stamps(c, ctx, sys, 1);
}

static const EcPlugin diode_plugin = {
    .abi = EC_ABI_VERSION,
    .id = "diode", .label = "Diode", .category = "Semiconductors",
    .n_pins = 2, .pin_names = { "anode", "cathode" },
    .n_params = 2,
    .param_names = { "Is", "N" }, .param_defaults = { 1e-14, 1.0 },
    .param_units = { "A", "-" },
    .nonlinear = 1,
    .symbol = "diode",
    .create = d_create, .destroy = d_destroy,
    .stamp = d_stamp, .stamp_ac = d_stamp_ac,
    .residual = d_residual,
};

/* ---------------- ground ---------------- */

static const EcPlugin ground_plugin = {
    .abi = EC_ABI_VERSION,
    .id = "ground", .label = "Ground", .category = "Power",
    .n_pins = 1, .pin_names = { "gnd" },
    .symbol = "ground",
};

void ec_register_builtins(EcPluginRegistry *reg)
{
    ec_registry_add(reg, &resistor_plugin);
    ec_registry_add(reg, &capacitor_plugin);
    ec_registry_add(reg, &inductor_plugin);
    ec_registry_add(reg, &vsource_plugin);
    ec_registry_add(reg, &isource_plugin);
    ec_registry_add(reg, &diode_plugin);
    ec_registry_add(reg, &ground_plugin);
}
