#pragma once

#include <QObject>

#include "ec/circuit.h"

#include <functional>

/* Runs a core analysis on a worker thread. Takes ownership of the EcCircuit
 * (frees it when done). Results are read from the GUI thread after the
 * finished() signal (delivered via a queued connection, so no locking is
 * needed beyond that sequencing). */
class SimWorker : public QObject
{
    Q_OBJECT

public:
    enum Analysis { Dc, Ac, Tr };

    explicit SimWorker(QObject *parent = nullptr);

    void runDc(EcCircuit *cir);
    void runAc(EcCircuit *cir, double f1, double f2, int npts, bool logSweep);
    void runTr(EcCircuit *cir, double tstop, double dt, int method);

    bool ok() const { return m_ok; }
    const EcResults &results() const { return m_res; }
    int runId() const { return m_runId; }

signals:
    void finished(int runId);
    void progress(int pct);

private:
    static void trProgressTramp(void *user, double frac);
    void spawn(int runId, std::function<void()> job);

    EcResults m_res;
    bool m_ok = false;
    int m_runId = 0;
};
