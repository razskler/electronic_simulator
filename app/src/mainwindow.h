#pragma once

#include <QHash>
#include <QList>
#include <QMainWindow>
#include <QVector>

#include "ec/circuit.h"
#include "netlistcompiler.h"
#include "simworker.h"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QAction;
class PlotPane;
class SchematicScene;
class SchematicView;
class QTreeWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();

private slots:
    void newFile();
    void openFile();
    void saveFile();
    void runAnalysis();
    void onFinished(int runId);
    void onProgress(int pct);
    void onSceneChanged();
    void showAnalysisSettings();

private:
    void buildUi();
    void loadPlugins();
    void fillPalette();
    void presentDc();
    void presentTr();
    void presentAc();
    void probeWire(WireItem *w);
    void probePin(ComponentItem *c, int pin);
    bool ensureCompiled();
    void log(const QString &msg);
    bool maybeSave();

    SchematicScene *m_scene = nullptr;
    SchematicView *m_view = nullptr;
    QTreeWidget *m_palette = nullptr;
    PlotPane *m_plot = nullptr;
    QPlainTextEdit *m_console = nullptr;
    QComboBox *m_analysisCombo = nullptr;
    QComboBox *m_acModeCombo = nullptr;
    QAction *m_probeAction = nullptr;
    QProgressBar *m_progress = nullptr;

    EcPluginRegistry *m_reg = nullptr;
    QHash<QString, const EcPlugin *> m_plugins;

    SimWorker m_worker;
    int m_startedRunId = 0;
    SimWorker::Analysis m_pendingAnalysis = SimWorker::Dc;

    Compiled m_lastCompile; /* mapping from the most recent run */
    bool m_compileDirty = true;
    QVector<double> m_dcOp; /* DC op point copy for canvas labels */

    struct {
        double f1 = 10.0, f2 = 100000.0;
        int n = 200;
        bool log = true;
    } m_ac;
    struct {
        double tstop = 5e-3, dt = 1e-5;
        int method = EC_METHOD_TRAP;
    } m_tr;

    QString m_filePath;
};
