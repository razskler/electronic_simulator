/* Example elsim plugin: push-button style switch.
 * Parameter state: 0 = open (Roff), 1 = closed (Ron).
 */
#include "ec/plugin.h"

static void sw_stamp(EcComp *c, const EcStampCtx *ctx, EcSys *sys)
{
    (void)ctx;
    int closed = c->params[0] >= 0.5;
    double r = closed ? c->params[1] : c->params[2];
    if (r <= 0.0) r = 1e-12;
    double g = 1.0 / r;
    int p = c->nodes[0], n = c->nodes[1];
    ec_sys_add(sys, p, p, g, 0);
    ec_sys_add(sys, n, n, g, 0);
    ec_sys_add(sys, p, n, -g, 0);
    ec_sys_add(sys, n, p, -g, 0);
}

static const EcPlugin switch_plugin = {
    .abi = EC_ABI_VERSION,
    .id = "switch", .label = "Switch", .category = "Plugins",
    .n_pins = 2, .pin_names = { "p", "n" },
    .n_params = 3,
    .param_names = { "state", "Ron", "Roff" },
    .param_defaults = { 1.0, 1e-3, 1e9 },
    .param_units = { "0/1", "ohm", "ohm" },
    .symbol = "switch",
    .stamp = sw_stamp, .stamp_ac = sw_stamp,
};

EC_EXPORT
const EcPlugin *ec_plugin_export(void)
{
    return &switch_plugin;
}
