#include "simworker.h"

#include <QMetaObject>
#include <QtGlobal>

#include <functional>
#include <thread>

SimWorker::SimWorker(QObject *parent)
    : QObject(parent)
{
}

void SimWorker::spawn(int runId, std::function<void()> job)
{
    m_runId = runId;
    std::thread([this, runId, job]() {
        job(); /* fills m_res / m_ok */
        QMetaObject::invokeMethod(this, [this, runId]() { emit finished(runId); },
                                  Qt::QueuedConnection);
    }).detach();
}

void SimWorker::trProgressTramp(void *user, double frac)
{
    auto *self = static_cast<SimWorker *>(user);
    int pct = int(frac * 100.0);
    QMetaObject::invokeMethod(self, [self, pct]() { emit self->progress(pct); },
                              Qt::QueuedConnection);
}

void SimWorker::runDc(EcCircuit *cir)
{
    spawn(++m_runId, [this, cir]() {
        ec_results_free(&m_res);
        m_ok = ec_analysis_dc(cir, &m_res) == 0;
        ec_circuit_free(cir);
    });
}

void SimWorker::runAc(EcCircuit *cir, double f1, double f2, int npts, bool logSweep)
{
    spawn(++m_runId, [this, cir, f1, f2, npts, logSweep]() {
        ec_results_free(&m_res);
        m_ok = ec_analysis_ac(cir, f1, f2, npts, logSweep ? 1 : 0, &m_res) == 0;
        ec_circuit_free(cir);
    });
}

void SimWorker::runTr(EcCircuit *cir, double tstop, double dt, int method)
{
    spawn(++m_runId, [this, cir, tstop, dt, method]() {
        ec_results_free(&m_res);
        m_ok = ec_analysis_tr(cir, tstop, dt, method, &SimWorker::trProgressTramp,
                              this, &m_res) == 0;
        ec_circuit_free(cir);
    });
}
