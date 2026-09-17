#include "internal.h"
#include <stdio.h>
#include <strings.h>

EcCircuit *ec_circuit_new(void)
{
    return calloc(1, sizeof(EcCircuit));
}

static void comp_free(EcComp *c)
{
    if (!c) return;
    if (c->plugin && c->plugin->destroy && c->state)
        c->plugin->destroy(c->state);
    free(c->meta);
    free(c);
}

void ec_circuit_free(EcCircuit *cir)
{
    if (!cir) return;
    for (int i = 0; i < cir->n_comps; i++) comp_free(cir->comps[i]);
    free(cir->comps);
    if (cir->node_names) {
        for (int i = 0; i < cir->node_names_cap; i++) free(cir->node_names[i]);
        free(cir->node_names);
    }
    free(cir);
}

EcComp *ec_circuit_add(EcCircuit *cir, const EcPlugin *p, const char *name)
{
    if (cir->n_comps == cir->cap_comps) {
        cir->cap_comps = cir->cap_comps ? cir->cap_comps * 2 : 16;
        cir->comps = realloc(cir->comps, (size_t)cir->cap_comps * sizeof(EcComp *));
    }
    EcComp *c = calloc(1, sizeof(EcComp));
    c->plugin = p;
    if (name) snprintf(c->name, sizeof(c->name), "%s", name);
    for (int i = 0; i < p->n_params && i < EC_MAX_PARAMS; i++)
        c->params[i] = p->param_defaults[i];
    c->state = p->create ? p->create() : NULL;
    cir->comps[cir->n_comps++] = c;
    return c;
}

void ec_circuit_finalize(EcCircuit *cir)
{
    cir->n_nodes = 0;
    cir->n_branches = 0;
    for (int i = 0; i < cir->n_comps; i++) {
        EcComp *c = cir->comps[i];
        for (int k = 0; k < c->plugin->n_pins; k++)
            if (c->nodes[k] > cir->n_nodes) cir->n_nodes = c->nodes[k];
        for (int k = 0; k < c->plugin->n_branches; k++)
            c->branch[k] = cir->n_branches++;
    }
}

void ec_netlist_file_free(EcNetlistFile *f)
{
    if (!f) return;
    ec_circuit_free(f->cir);
    f->cir = NULL;
}

int ec_param_idx(const EcPlugin *p, const char *name)
{
    if (!p) return -1;
    for (int i = 0; i < p->n_params && i < EC_MAX_PARAMS; i++)
        if (p->param_names[i] && strcmp(p->param_names[i], name) == 0) return i;
    return -1;
}

int ec_parse_eng(const char *s, double *out)
{
    if (!s || !*s) return -1;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return -1;
    while (*end == ' ') end++;
    if (*end == '\0') { *out = v; return 0; }
    double mult = 0.0;
    if (!strcasecmp(end, "Meg") || !strcasecmp(end, "MEG")) mult = 1e6;
    else if (!strcasecmp(end, "f"))  mult = 1e-15;
    else if (!strcasecmp(end, "p"))  mult = 1e-12;
    else if (!strcasecmp(end, "n"))  mult = 1e-9;
    else if (!strcasecmp(end, "u") || !strcmp(end, "\xc2\xb5")) mult = 1e-6;
    else if (!strcasecmp(end, "m"))  mult = 1e-3;
    else if (!strcasecmp(end, "k"))  mult = 1e3;
    else if (!strcasecmp(end, "M"))  mult = 1e6;
    else if (!strcasecmp(end, "G"))  mult = 1e9;
    else if (!strcasecmp(end, "T"))  mult = 1e12;
    if (mult == 0.0) return -1;
    *out = v * mult;
    return 0;
}

/* ---- parser ---- */

typedef struct {
    char **names; /* node name -> id (index+1) */
    int n, cap;
} NodeTab;

static int node_id(NodeTab *t, const char *name)
{
    if (!strcmp(name, "0") || !strcasecmp(name, "gnd")) return 0;
    for (int i = 0; i < t->n; i++)
        if (!strcmp(t->names[i], name)) return i + 1;
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 16;
        t->names = realloc(t->names, (size_t)t->cap * sizeof(char *));
    }
    t->names[t->n] = strdup(name);
    return ++t->n;
}

static void node_tab_free(NodeTab *t, EcCircuit *cir)
{
    if (cir) {
        cir->node_names_cap = t->n + 1;
        cir->node_names = calloc((size_t)t->n + 1, sizeof(char *));
        cir->node_names[0] = strdup("gnd");
        for (int i = 0; i < t->n; i++) cir->node_names[i + 1] = t->names[i];
        free(t->names);
    } else {
        for (int i = 0; i < t->n; i++) free(t->names[i]);
        free(t->names);
    }
}

static void meta_append(EcComp *c, const char *kv)
{
    size_t len = c->meta ? strlen(c->meta) : 0;
    size_t add = strlen(kv) + 2;
    c->meta = realloc(c->meta, len + add);
    c->meta[len] = ' ';
    memcpy(c->meta + len + 1, kv, strlen(kv) + 1);
}

int ec_parse_netlist(const char *text, struct EcPluginRegistry *reg,
                     EcNetlistFile *out, char *err, size_t errsz)
{
    memset(out, 0, sizeof(*out));
    out->cir = ec_circuit_new();
    NodeTab tab = {0};
    char *buf = strdup(text);
    char *save = NULL;
    int lineno = 0, rc = 0;

    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        lineno++;
        /* trim */
        while (*line == ' ' || *line == '\t' || *line == '\r') line++;
        if (!*line || *line == '#' || *line == ';' || *line == '*') continue;

        /* tokenize in place */
        char *tok[64]; int ntok = 0;
        for (char *p = strtok(line, " \t\r"); p && ntok < 64; p = strtok(NULL, " \t\r"))
            tok[ntok++] = p;
        if (!ntok) continue;

        if (tok[0][0] == '.') {
            if (!strcasecmp(tok[0], ".op")) out->has_op = 1;
            else if (!strcasecmp(tok[0], ".ac") && ntok >= 4) {
                double f1, f2;
                if (ec_parse_eng(tok[1], &f1) != 0 || ec_parse_eng(tok[2], &f2) != 0) {
                    snprintf(err, errsz, "line %d: bad .ac frequency", lineno);
                    rc = -1;
                    break;
                }
                out->has_ac = 1;
                out->ac_f1 = f1;
                out->ac_f2 = f2;
                out->ac_n = atoi(tok[3]);
                out->ac_log = (ntok < 5 || strcasecmp(tok[4], "lin")) ? 1 : 0;
            } else if (!strcasecmp(tok[0], ".tran") && ntok >= 3) {
                double ts, ttf;
                if (ec_parse_eng(tok[1], &ts) != 0 || ec_parse_eng(tok[2], &ttf) != 0) {
                    snprintf(err, errsz, "line %d: bad .tran time", lineno);
                    rc = -1;
                    break;
                }
                out->has_tran = 1;
                out->tran_tstep = ts;
                out->tran_tstop = ttf;
                out->tran_method = (ntok >= 4 && !strcasecmp(tok[3], "trap"))
                                 ? EC_METHOD_TRAP : EC_METHOD_BE;
            }
            continue;
        }

        EcPlugin *p = ec_registry_find(reg, tok[0]);
        if (!p) { snprintf(err, errsz, "line %d: unknown component '%s'", lineno, tok[0]); rc = -1; break; }
        if (ntok < 2 + p->n_pins) {
            snprintf(err, errsz, "line %d: '%s' needs %d node name(s)", lineno, p->id, p->n_pins);
            rc = -1; break;
        }
        EcComp *c = ec_circuit_add(out->cir, p, tok[1]);
        for (int k = 0; k < p->n_pins; k++)
            c->nodes[k] = node_id(&tab, tok[2 + k]);
        for (int k = 2 + p->n_pins; k < ntok; k++) {
            char *eq = strchr(tok[k], '=');
            if (!eq) continue;
            *eq = '\0';
            int pi = ec_param_idx(p, tok[k]);
            if (pi >= 0) {
                if (ec_parse_eng(eq + 1, &c->params[pi]) != 0) {
                    snprintf(err, errsz, "line %d: bad value '%s=%s'", lineno, tok[k], eq + 1);
                    rc = -1; break;
                }
            } else {
                *eq = '='; /* restore token */
                meta_append(c, tok[k]);
            }
        }
        if (rc) break;
    }

    free(buf);
    if (rc) {
        node_tab_free(&tab, NULL);
        ec_netlist_file_free(out);
        return rc;
    }
    ec_circuit_finalize(out->cir);
    node_tab_free(&tab, out->cir);
    return 0;
}
