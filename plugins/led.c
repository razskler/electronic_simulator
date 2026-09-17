/* Example elsim plugin: LED (diode with LED-typical parameters).
 *
 * Build: compiled into led.so by the top-level Makefile / CMake.
 * This is the reference example of the component plugin ABI.
 */
#include "ec/plugin.h"
#include <math.h>
#include <stdlib.h>

#define EC_VT 0.025852

typedef struct { double vlast, geq, ieq; } LedState;

static void *led_create(void) { return calloc(1, sizeof(LedState)); }
static void led_destroy(void *s) { free(s); }

static double led_limit(double vnew, double vold, double vte)
{
    if (vnew > vold + 2.0 * vte) return vold + 2.0 * vte;
    if (vnew < vold - 2.0 * vte) return vold - 2.0 * vte;
    return vnew;
}

static void led_stamp(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    int p = c->nodes[0], n = c->nodes[1];
    double Is = c->params[0], Nn = c->params[1];
    double vte = Nn * EC_VT;
    LedState *st = c->state;

    double v = ctx->x ? ctx->x[p] - ctx->x[n] : 0.0;
    v = led_limit(v, st->vlast, vte);
    st->vlast = v;

    double arg = v / vte;
    if (arg > 200.0) arg = 200.0;
    if (arg < -200.0) arg = -200.0;
    double e = exp(arg);
    double geq = Is * e / vte;
    if (geq < ctx->gmin) geq = ctx->gmin;
    double i = Is * (e - 1.0);
    double ieq = geq * v - i; /* companion source, see diode in builtin.c */
    st->geq = geq;
    st->ieq = ieq;

    ec_sys_add(sys, p, p, geq, 0);
    ec_sys_add(sys, n, n, geq, 0);
    ec_sys_add(sys, p, n, -geq, 0);
    ec_sys_add(sys, n, p, -geq, 0);
    ec_sys_rhs(sys, p, ieq, 0);
    ec_sys_rhs(sys, n, -ieq, 0);
}

static void led_stamp_ac(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    int p = c->nodes[0], n = c->nodes[1];
    double Is = c->params[0], Nn = c->params[1];
    double vte = Nn * EC_VT;
    double v = ctx->x ? ctx->x[p] - ctx->x[n] : 0.0;
    double arg = v / vte;
    if (arg > 200.0) arg = 200.0;
    double geq = Is * exp(arg) / vte;
    if (geq < ctx->gmin) geq = ctx->gmin;
    ec_sys_add(sys, p, p, geq, 0);
    ec_sys_add(sys, n, n, geq, 0);
    ec_sys_add(sys, p, n, -geq, 0);
    ec_sys_add(sys, n, p, -geq, 0);
}

static double led_residual(const EcComp *c, const EcStampCtx *ctx)
{
    const LedState *st = c->state;
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

static const EcPlugin led_plugin = {
    .abi = EC_ABI_VERSION,
    .id = "led", .label = "LED", .category = "Plugins",
    .n_pins = 2, .pin_names = { "anode", "cathode" },
    .n_params = 2,
    .param_names = { "Is", "N" }, .param_defaults = { 1e-18, 2.0 },
    .param_units = { "A", "-" },
    .nonlinear = 1,
    .symbol = "led",
    .create = led_create, .destroy = led_destroy,
    .stamp = led_stamp, .stamp_ac = led_stamp_ac,
    .residual = led_residual,
};

EC_EXPORT
const EcPlugin *ec_plugin_export(void)
{
    return &led_plugin;
}
