/* elsim - circuit container + text netlist parser */
#ifndef EC_NETLIST_H
#define EC_NETLIST_H

#include "ec/plugin.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct EcPluginRegistry;

typedef struct EcCircuit {
    EcComp **comps;
    int n_comps, cap_comps;
    int n_nodes;    /* highest non-ground node id */
    int n_branches; /* sum of per-component branch counts */
    char **node_names; /* optional names, index 0..n_nodes (may be NULL) */
    int node_names_cap;
} EcCircuit;

EcCircuit *ec_circuit_new(void);
EcComp *ec_circuit_add(EcCircuit *cir, const EcPlugin *p, const char *name);
void ec_circuit_free(EcCircuit *cir);
/* Recompute n_nodes/n_branches and assign branch ids. Call after all
 * components and node ids are set. */
void ec_circuit_finalize(EcCircuit *cir);

/* Parsed netlist file (analysis directives included). */
typedef struct EcNetlistFile {
    EcCircuit *cir; /* owned */
    int has_op, has_ac, has_tran;
    double ac_f1, ac_f2;
    int ac_n, ac_log;
    double tran_tstep, tran_tstop;
    int tran_method; /* EC_METHOD_BE / EC_METHOD_TRAP */
} EcNetlistFile;

void ec_netlist_file_free(EcNetlistFile *f);

/* Parse SPICE-flavoured netlist text. Component line:
 *   <plugin-id> <name> <node1> <node2> ... [key=value]...
 * Directives: .op | .ac f1 f2 n [log|lin] | .tran tstep tstop [be|trap]
 * Node names "0", "gnd", "GND" map to ground. Unknown key=value pairs are
 * appended to EcComp.meta (used by the GUI for x/y/rot round-tripping).
 * Values accept engineering suffixes: 1k 2.2u 100n 1Meg ...
 * Returns 0 on success; on failure returns -1 and fills err. */
int ec_parse_netlist(const char *text, struct EcPluginRegistry *reg,
                     EcNetlistFile *out, char *err, size_t errsz);

/* Parse a numeric string with optional engineering suffix. Returns 0 ok. */
int ec_parse_eng(const char *s, double *out);

#ifdef __cplusplus
}
#endif

#endif /* EC_NETLIST_H */
