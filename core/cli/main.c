/* elsim - command line netlist runner */
#include "ec/circuit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void print_dc(EcNetlistFile *f, EcResults *r, FILE *csv)
{
    EcCircuit *cir = f->cir;
    printf("== DC operating point ==\n");
    printf("%-12s %14s\n", "node", "voltage");
    if (csv) fprintf(csv, "node,voltage\n");
    for (int i = 1; i <= cir->n_nodes; i++) {
        const char *nm = cir->node_names && cir->node_names[i]
                       ? cir->node_names[i] : "?";
        double v = ec_results_v(r, 0, i);
        printf("%-12s %14.6g\n", nm, v);
        if (csv) fprintf(csv, "%s,%.10g\n", nm, v);
    }
    printf("%-12s %14s\n", "branch", "current");
    for (int i = 0; i < cir->n_comps; i++) {
        EcComp *c = cir->comps[i];
        if (c->plugin->n_branches > 0) {
            char nm[48];
            snprintf(nm, sizeof nm, "%s#br", c->name);
            printf("%-12s %14.6g\n", nm, ec_results_i(r, 0, c->branch[0]));
            if (csv) fprintf(csv, "%s,%.10g\n", nm, ec_results_i(r, 0, c->branch[0]));
        }
    }
}

static void print_ac(EcNetlistFile *f, EcResults *r, FILE *csv)
{
    EcCircuit *cir = f->cir;
    printf("== AC analysis ==\n");
    printf("%-10s", "freq[Hz]");
    if (csv) fprintf(csv, "freq");
    int shown = cir->n_nodes < 8 ? cir->n_nodes : 8;
    for (int i = 1; i <= shown; i++) {
        const char *nm = cir->node_names && cir->node_names[i] ? cir->node_names[i] : "?";
        printf(" %-8s[V]      %-8s[deg]", nm, nm);
        if (csv) fprintf(csv, ",%s_mag,%s_ph", nm, nm);
    }
    printf("\n");
    if (csv) fprintf(csv, "\n");
    for (int p = 0; p < r->n_pts; p++) {
        printf("%-10.4g", r->sweep[p]);
        if (csv) fprintf(csv, "%.10g", r->sweep[p]);
        for (int i = 1; i <= shown; i++) {
            printf(" %-13.5g %-13.5g", ec_results_v(r, p, i), ec_results_phase(r, p, i));
            if (csv) fprintf(csv, ",%.10g,%.10g", ec_results_v(r, p, i), ec_results_phase(r, p, i));
        }
        printf("\n");
        if (csv) fprintf(csv, "\n");
    }
}

static void print_tran(EcNetlistFile *f, EcResults *r, FILE *csv)
{
    EcCircuit *cir = f->cir;
    printf("== transient (%s) ==\n",
           f->tran_method == EC_METHOD_TRAP ? "trapezoidal" : "backward euler");
    if (csv) {
        fprintf(csv, "time");
        for (int i = 1; i <= cir->n_nodes; i++) {
            const char *nm = cir->node_names && cir->node_names[i] ? cir->node_names[i] : "?";
            fprintf(csv, ",V(%s)", nm);
        }
        fprintf(csv, "\n");
        for (int p = 0; p < r->n_pts; p++) {
            fprintf(csv, "%.10g", r->sweep[p]);
            for (int i = 1; i <= cir->n_nodes; i++)
                fprintf(csv, ",%.10g", ec_results_v(r, p, i));
            fprintf(csv, "\n");
        }
        return;
    }
    int skip = r->n_pts > 40 ? (r->n_pts + 39) / 40 : 1;
    for (int p = 0; p < r->n_pts; p += skip) {
        printf("t=%-10.4g", r->sweep[p]);
        for (int i = 1; i <= cir->n_nodes && i <= 6; i++)
            printf(" %-13.6g", ec_results_v(r, p, i));
        printf("\n");
    }
    if ((r->n_pts - 1) % skip != 0) { /* always show the final point */
        int p = r->n_pts - 1;
        printf("t=%-10.4g", r->sweep[p]);
        for (int i = 1; i <= cir->n_nodes && i <= 6; i++)
            printf(" %-13.6g", ec_results_v(r, p, i));
        printf("\n");
    }
}

static void progress(void *user, double frac)
{
    (void)user;
    fprintf(stderr, "\rtransient: %3.0f%%", frac * 100.0);
    if (frac >= 1.0) fprintf(stderr, "\n");
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: elsim <netlist.cir> [-o out.csv]\n");
        return 1;
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *text = malloc((size_t)sz + 1);
    size_t rd = fread(text, 1, (size_t)sz, fp);
    text[rd] = '\0';
    fclose(fp);

    FILE *csv = NULL;
    if (argc >= 4 && !strcmp(argv[2], "-o")) {
        csv = fopen(argv[3], "w");
        if (!csv) { fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
    }

    EcPluginRegistry *reg = ec_registry_new();
    ec_register_builtins(reg);
    int nplug = ec_registry_load_dir(reg, "plugins", NULL, 0);
    if (const char *env = getenv("ELSIM_PLUGINS"))
        nplug += ec_registry_load_dir(reg, env, NULL, 0);
    if (nplug > 0) fprintf(stderr, "loaded %d plugin(s)\n", nplug);

    EcNetlistFile f;
    char err[256] = { 0 };
    if (ec_parse_netlist(text, reg, &f, err, sizeof err) != 0) {
        fprintf(stderr, "parse error: %s\n", err);
        return 1;
    }
    free(text);

    int rc = 0;
    EcResults r;
    if (f.has_op) {
        if (ec_analysis_dc(f.cir, &r) == 0) print_dc(&f, &r, csv);
        else { fprintf(stderr, "error: %s\n", r.message); rc = 1; }
        ec_results_free(&r);
    }
    if (f.has_ac) {
        if (ec_analysis_ac(f.cir, f.ac_f1, f.ac_f2, f.ac_n, f.ac_log, &r) == 0)
            print_ac(&f, &r, csv);
        else { fprintf(stderr, "error: %s\n", r.message); rc = 1; }
        ec_results_free(&r);
    }
    if (f.has_tran) {
        if (ec_analysis_tr(f.cir, f.tran_tstop, f.tran_tstep, f.tran_method,
                           csv ? NULL : progress, NULL, &r) == 0)
            print_tran(&f, &r, csv);
        else { fprintf(stderr, "error: %s\n", r.message); rc = 1; }
        ec_results_free(&r);
    }
    if (!f.has_op && !f.has_ac && !f.has_tran)
        fprintf(stderr, "no analysis directive (.op / .ac / .tran) in file\n");

    if (csv) fclose(csv);
    ec_netlist_file_free(&f);
    ec_registry_free(reg);
    return rc;
}
