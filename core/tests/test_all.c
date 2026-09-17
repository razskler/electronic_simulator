/* elsim - core unit tests (assert-based, no framework) */
#include "ec/circuit.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_failed = 0, g_total = 0;

#define CHECK(cond) do { \
    g_total++; \
    if (!(cond)) { g_failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

static void close_to(double a, double b, double rel, const char *what)
{
    double tol = rel * fmax(fabs(b), 1e-12) + 1e-12;
    g_total++;
    if (fabs(a - b) > tol) {
        g_failed++;
        printf("FAIL %s: %.10g != %.10g (tol %.3g)\n", what, a, b, tol);
    }
}

static EcPluginRegistry *make_registry(void)
{
    EcPluginRegistry *r = ec_registry_new();
    ec_register_builtins(r);
    return r;
}

static void test_matrix(void)
{
    double A[9] = {
        2, 1, -1,
        -3, -1, 2,
        -2, 1, 2,
    };
    double b[3] = { 8, -11, -3 };
    int piv[3];
    CHECK(ec_lu_factor(A, 3, piv) == 0);
    ec_lu_solve(A, 3, piv, b);
    close_to(b[0], 2, 1e-12, "lu x0");
    close_to(b[1], 3, 1e-12, "lu x1");
    close_to(b[2], -1, 1e-12, "lu x2");

    double S[4] = { 0, 1, 1, 0 };
    CHECK(ec_lu_factor(S, 2, piv) == 0);
    double sb[2] = { 1, 2 };
    ec_lu_solve(S, 2, piv, sb);
    close_to(sb[0], 2, 1e-12, "lu pivot x0");
    close_to(sb[1], 1, 1e-12, "lu pivot x1");

    double Z[4] = { 1, 2, 2, 4 };
    CHECK(ec_lu_factor(Z, 2, piv) == -1);

    ec_cx C[4] = { {0,0}, {1,0}, {1,0}, {0,0} };
    CHECK(ec_lu_factor_cx(C, 2, piv) == 0);
    ec_cx cb[2] = { {1,0}, {2,0} };
    ec_lu_solve_cx(C, 2, piv, cb);
    close_to(cb[0].re, 2, 1e-12, "lu cx x0");
    close_to(cb[1].re, 1, 1e-12, "lu cx x1");
}

static void test_eng(void)
{
    double v;
    CHECK(ec_parse_eng("1k", &v) == 0 && v == 1000.0);
    CHECK(ec_parse_eng("2.2u", &v) == 0 && fabs(v - 2.2e-6) < 1e-18);
    CHECK(ec_parse_eng("100n", &v) == 0 && fabs(v - 1e-7) < 1e-19);
    CHECK(ec_parse_eng("1Meg", &v) == 0 && v == 1e6);
    CHECK(ec_parse_eng("1e3", &v) == 0 && v == 1000.0);
    CHECK(ec_parse_eng("47", &v) == 0 && v == 47.0);
    CHECK(ec_parse_eng("3m3", &v) != 0); /* unsupported notation */
}

/* Build: V1 1 0 dc ; R1 1 2 ; R2 2 0 */
static void test_dc_divider(void)
{
    EcPluginRegistry *reg = make_registry();
    EcPlugin *rp = ec_registry_find(reg, "resistor");
    EcPlugin *vp = ec_registry_find(reg, "vsource");
    EcCircuit *cir = ec_circuit_new();

    EcComp *v = ec_circuit_add(cir, vp, "V1");
    v->nodes[0] = 1; v->nodes[1] = 0; v->params[0] = 5.0;
    EcComp *r1 = ec_circuit_add(cir, rp, "R1");
    r1->nodes[0] = 1; r1->nodes[1] = 2; r1->params[0] = 1000.0;
    EcComp *r2 = ec_circuit_add(cir, rp, "R2");
    r2->nodes[0] = 2; r2->nodes[1] = 0; r2->params[0] = 1000.0;
    ec_circuit_finalize(cir);
    CHECK(cir->n_nodes == 2);
    CHECK(cir->n_branches == 1);

    EcResults r;
    CHECK(ec_analysis_dc(cir, &r) == 0);
    close_to(ec_results_v(&r, 0, 1), 5.0, 1e-9, "divider v1");
    close_to(ec_results_v(&r, 0, 2), 2.5, 1e-9, "divider v2");
    /* SPICE convention: branch current is positive flowing into the +
     * terminal, so a source driving a load reads negative */
    close_to(ec_results_i(&r, 0, v->branch[0]), -2.5e-3, 1e-6, "divider i");
    ec_results_free(&r);
    ec_circuit_free(cir);
    ec_registry_free(reg);
}

static void test_dc_current_source(void)
{
    EcPluginRegistry *reg = make_registry();
    EcCircuit *cir = ec_circuit_new();
    EcComp *i1 = ec_circuit_add(cir, ec_registry_find(reg, "isource"), "I1");
    i1->nodes[0] = 1; i1->nodes[1] = 0; i1->params[0] = 1.0;
    EcComp *r1 = ec_circuit_add(cir, ec_registry_find(reg, "resistor"), "R1");
    r1->nodes[0] = 1; r1->nodes[1] = 0; r1->params[0] = 1.0;
    ec_circuit_finalize(cir);
    EcResults r;
    CHECK(ec_analysis_dc(cir, &r) == 0);
    close_to(ec_results_v(&r, 0, 1), 1.0, 1e-9, "isource v");
    ec_results_free(&r);
    ec_circuit_free(cir);
    ec_registry_free(reg);
}

static void test_parser(void)
{
    EcPluginRegistry *reg = make_registry();
    const char *text =
        "; divider\n"
        "vsource V1 in 0 dc=5\n"
        "resistor R1 in mid R=1k\n"
        "resistor R2 mid 0 R=1k x=100 y=200 rot=1\n"
        ".op\n";
    EcNetlistFile f;
    char err[256] = { 0 };
    CHECK(ec_parse_netlist(text, reg, &f, err, sizeof err) == 0);
    CHECK(f.cir->n_comps == 3);
    CHECK(f.has_op);
    CHECK(f.cir->n_nodes == 2);
    EcResults r;
    CHECK(ec_analysis_dc(f.cir, &r) == 0);
    close_to(ec_results_v(&r, 0, 2), 2.5, 1e-9, "parser divider");
    ec_results_free(&r);
    ec_netlist_file_free(&f);
    ec_registry_free(reg);
}

static void test_diode_dc(void)
{
    EcPluginRegistry *reg = make_registry();
    EcCircuit *cir = ec_circuit_new();
    EcComp *v = ec_circuit_add(cir, ec_registry_find(reg, "vsource"), "V1");
    v->nodes[0] = 1; v->nodes[1] = 0; v->params[0] = 5.0;
    EcComp *r1 = ec_circuit_add(cir, ec_registry_find(reg, "resistor"), "R1");
    r1->nodes[0] = 1; r1->nodes[1] = 2; r1->params[0] = 1000.0;
    EcComp *d = ec_circuit_add(cir, ec_registry_find(reg, "diode"), "D1");
    d->nodes[0] = 2; d->nodes[1] = 0;
    ec_circuit_finalize(cir);
    EcResults r;
    CHECK(ec_analysis_dc(cir, &r) == 0);
    /* verify the device equation: I = (Vs - Vd)/R must equal Is*exp(Vd/Vt) */
    double vd = ec_results_v(&r, 0, 2);
    double ires = (5.0 - vd) / 1000.0 - 1e-14 * (exp(vd / 0.025852) - 1.0);
    close_to(ires, 0.0, 1e-6, "diode residual");
    CHECK(vd > 0.5 && vd < 0.8); /* plausible silicon drop */
    ec_results_free(&r);
    ec_circuit_free(cir);
    ec_registry_free(reg);
}

/* RC lowpass: at f = 1/(2pi RC), |Vout/Vin| = 1/sqrt(2) */
static void test_ac_rc(void)
{
    EcPluginRegistry *reg = make_registry();
    EcCircuit *cir = ec_circuit_new();
    EcComp *v = ec_circuit_add(cir, ec_registry_find(reg, "vsource"), "V1");
    v->nodes[0] = 1; v->nodes[1] = 0; v->params[0] = 0.0; v->params[1] = 1.0;
    EcComp *r1 = ec_circuit_add(cir, ec_registry_find(reg, "resistor"), "R1");
    r1->nodes[0] = 1; r1->nodes[1] = 2; r1->params[0] = 1000.0;
    EcComp *c1 = ec_circuit_add(cir, ec_registry_find(reg, "capacitor"), "C1");
    c1->nodes[0] = 2; c1->nodes[1] = 0; c1->params[0] = 1e-6;
    ec_circuit_finalize(cir);

    double fc = 1.0 / (2.0 * M_PI * 1000.0 * 1e-6);
    EcResults r;
    CHECK(ec_analysis_ac(cir, fc, fc, 1, 0, &r) == 0);
    close_to(ec_results_v(&r, 0, 2), 1.0 / sqrt(2.0), 1e-6, "ac rc mag");
    close_to(ec_results_phase(&r, 0, 2), -45.0, 1e-4, "ac rc phase");
    ec_results_free(&r);
    ec_circuit_free(cir);
    ec_registry_free(reg);
}

/* RC charging: v(t) = V(1 - exp(-t/RC)) */
static void test_tran_rc(int method, const char *tag)
{
    EcPluginRegistry *reg = make_registry();
    EcCircuit *cir = ec_circuit_new();
    EcComp *v = ec_circuit_add(cir, ec_registry_find(reg, "vsource"), "V1");
    v->nodes[0] = 1; v->nodes[1] = 0; v->params[0] = 5.0;
    EcComp *r1 = ec_circuit_add(cir, ec_registry_find(reg, "resistor"), "R1");
    r1->nodes[0] = 1; r1->nodes[1] = 2; r1->params[0] = 1000.0;
    EcComp *c1 = ec_circuit_add(cir, ec_registry_find(reg, "capacitor"), "C1");
    c1->nodes[0] = 2; c1->nodes[1] = 0; c1->params[0] = 1e-6;
    ec_circuit_finalize(cir);

    double tau = 1000.0 * 1e-6;
    double dt = tau / 200.0, tstop = 3.0 * tau;
    EcResults r;
    CHECK(ec_analysis_tr(cir, tstop, dt, method, NULL, NULL, &r) == 0);
    int ok = 0;
    for (int p = 0; p < r.n_pts; p++) {
        double t = r.sweep[p];
        if (fabs(t - tau) < dt / 2.0) {
            double want = 5.0 * (1.0 - exp(-t / tau));
            close_to(ec_results_v(&r, p, 2), want, method == EC_METHOD_TRAP ? 1e-4 : 5e-3, tag);
            ok = 1;
        }
    }
    CHECK(ok); /* found the sample at t = tau */
    ec_results_free(&r);
    ec_circuit_free(cir);
    ec_registry_free(reg);
}

/* RL: current ramps as i = (V/R)(1-exp(-tR/L)) through branch */
static void test_tran_rl(void)
{
    EcPluginRegistry *reg = make_registry();
    EcCircuit *cir = ec_circuit_new();
    EcComp *v = ec_circuit_add(cir, ec_registry_find(reg, "vsource"), "V1");
    v->nodes[0] = 1; v->nodes[1] = 0; v->params[0] = 5.0;
    EcComp *r1 = ec_circuit_add(cir, ec_registry_find(reg, "resistor"), "R1");
    r1->nodes[0] = 1; r1->nodes[1] = 2; r1->params[0] = 10.0;
    EcComp *l1 = ec_circuit_add(cir, ec_registry_find(reg, "inductor"), "L1");
    l1->nodes[0] = 2; l1->nodes[1] = 0; l1->params[0] = 1e-3;
    ec_circuit_finalize(cir);

    double tau = 1e-3 / 10.0;
    double dt = tau / 200.0, tstop = 3.0 * tau;
    EcResults r;
    CHECK(ec_analysis_tr(cir, tstop, dt, EC_METHOD_TRAP, NULL, NULL, &r) == 0);
    int ok = 0;
    for (int p = 0; p < r.n_pts; p++) {
        if (fabs(r.sweep[p] - tau) < dt / 2.0) {
            double want = 0.5 * (1.0 - exp(-1.0));
            close_to(ec_results_i(&r, p, l1->branch[0]), want, 1e-4, "rl i(tau)");
            ok = 1;
        }
    }
    CHECK(ok);
    ec_results_free(&r);
    ec_circuit_free(cir);
    ec_registry_free(reg);
}

static void test_inductor_dc_short(void)
{
    /* DC: inductor is an ideal short, so i = V/R */
    EcPluginRegistry *reg = make_registry();
    EcCircuit *cir = ec_circuit_new();
    EcComp *v = ec_circuit_add(cir, ec_registry_find(reg, "vsource"), "V1");
    v->nodes[0] = 1; v->nodes[1] = 0; v->params[0] = 5.0;
    EcComp *r1 = ec_circuit_add(cir, ec_registry_find(reg, "resistor"), "R1");
    r1->nodes[0] = 1; r1->nodes[1] = 2; r1->params[0] = 10.0;
    EcComp *l1 = ec_circuit_add(cir, ec_registry_find(reg, "inductor"), "L1");
    l1->nodes[0] = 2; l1->nodes[1] = 0; l1->params[0] = 1e-3;
    ec_circuit_finalize(cir);
    EcResults r;
    CHECK(ec_analysis_dc(cir, &r) == 0);
    close_to(ec_results_i(&r, 0, l1->branch[0]), 0.5, 1e-9, "ind dc i");
    close_to(ec_results_v(&r, 0, 2), 0.0, 1e-6, "ind dc v2");
    ec_results_free(&r);
    ec_circuit_free(cir);
    ec_registry_free(reg);
}

int main(void)
{
    test_matrix();
    test_eng();
    test_dc_divider();
    test_dc_current_source();
    test_parser();
    test_diode_dc();
    test_ac_rc();
    test_tran_rc(EC_METHOD_BE, "tran rc BE");
    test_tran_rc(EC_METHOD_TRAP, "tran rc TRAP");
    test_tran_rl();
    test_inductor_dc_short();
    printf("%d/%d checks passed\n", g_total - g_failed, g_total);
    return g_failed ? 1 : 0;
}
