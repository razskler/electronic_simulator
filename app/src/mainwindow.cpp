#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextStream>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "componentitem.h"
#include "plotpane.h"
#include "schematicscene.h"
#include "symbols.h"
#include "wireitem.h"

static const char *kMimeComponent = "application/x-elsim-comp";

/* Palette tree that starts drags carrying the plugin id. */
class PaletteTree : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;

protected:
    void startDrag(Qt::DropActions) override
    {
        QTreeWidgetItem *item = currentItem();
        if (!item || item->data(0, Qt::UserRole).isNull())
            return;
        QString id = item->data(0, Qt::UserRole).toString();
        QMimeData *mime = new QMimeData;
        mime->setData(QLatin1String(kMimeComponent), id.toUtf8());
        QDrag *drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->setPixmap(item->icon(0).pixmap(44, 30));
        drag->exec(Qt::CopyAction);
    }
};

MainWindow::MainWindow()
{
    setWindowTitle(tr("elsim - electronics simulator"));
    resize(1280, 800);
    loadPlugins();
    buildUi();
    log(tr("Welcome to elsim."));
    log(tr("Drag components from the palette onto the canvas; click a pin then another "
           "pin to draw a wire; double-click a component to edit its parameters. "
           "R rotates, Del deletes. Use Probe mode to mark nets for plotting, then Run."));
}

void MainWindow::loadPlugins()
{
    m_reg = ec_registry_new();
    ec_register_builtins(m_reg);
    for (int i = 0; i < m_reg->n; i++) {
        const EcPlugin *p = m_reg->plugins[i];
        m_plugins.insert(QString::fromLatin1(p->id), p);
    }
    QStringList dirs;
    dirs << QCoreApplication::applicationDirPath() + QLatin1String("/plugins")
         << QLatin1String("plugins");
    if (const char *env = getenv("ELSIM_PLUGINS"))
        dirs << QString::fromLocal8Bit(env);
    int loaded = 0;
    for (const QString &d : qAsConst(dirs)) {
        char err[256] = { 0 };
        int n = ec_registry_load_dir(m_reg, d.toLocal8Bit().constData(), err, sizeof err);
        for (int i = m_reg->n - n; i < m_reg->n; i++) {
            const EcPlugin *p = m_reg->plugins[i];
            m_plugins.insert(QString::fromLatin1(p->id), p);
        }
        loaded += n;
        if (err[0])
            log(tr("Plugin note (%1): %2").arg(d, QString::fromUtf8(err)));
    }
    if (loaded > 0)
        log(tr("Loaded %n external plugin(s).", nullptr, loaded));
}

void MainWindow::buildUi()
{
    m_scene = new SchematicScene(this);
    m_view = new SchematicView(m_scene);
    m_view->plugins = m_plugins;

    QToolBar *tb = addToolBar(tr("Main"));
    tb->setMovable(false);
    QAction *newAct = tb->addAction(tr("New"));
    QAction *openAct = tb->addAction(tr("Open"));
    QAction *saveAct = tb->addAction(tr("Save"));
    tb->addSeparator();
    m_analysisCombo = new QComboBox;
    m_analysisCombo->addItem(tr("DC operating point"));
    m_analysisCombo->addItem(tr("AC sweep"));
    m_analysisCombo->addItem(tr("Transient"));
    tb->addWidget(m_analysisCombo);
    QAction *settingsAct = tb->addAction(tr("Settings..."));
    tb->addSeparator();
    QAction *runAct = tb->addAction(tr("Run"));
    runAct->setToolTip(tr("Run the selected analysis (F5)"));
    QAction *probeAct = tb->addAction(tr("Probe"));
    probeAct->setCheckable(true);
    probeAct->setToolTip(tr("Probe mode: click wires or pins to mark nets for plotting"));
    tb->addSeparator();
    QAction *rotateAct = tb->addAction(tr("Rotate"));
    QAction *deleteAct = tb->addAction(tr("Delete"));

    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);
    connect(settingsAct, &QAction::triggered, this, &MainWindow::showAnalysisSettings);
    connect(runAct, &QAction::triggered, this, &MainWindow::runAnalysis);
    connect(probeAct, &QAction::toggled, this, [this](bool on) {
        m_scene->setMode(on ? SchematicScene::ProbeMode : SchematicScene::SelectMode);
    });
    connect(rotateAct, &QAction::triggered, this, [this]() { m_scene->rotateSelected(); });
    connect(deleteAct, &QAction::triggered, this, [this]() { m_scene->deleteSelected(); });

    QAction *runShortcut = new QAction(this);
    runShortcut->setShortcut(Qt::Key_F5);
    connect(runShortcut, &QAction::triggered, this, &MainWindow::runAnalysis);
    addAction(runShortcut);

    /* palette */
    m_palette = new PaletteTree;
    m_palette->setHeaderLabel(tr("Components"));
    m_palette->setDragEnabled(true);
    m_palette->setRootIsDecorated(true);
    fillPalette();

    QWidget *left = new QWidget;
    QVBoxLayout *leftLay = new QVBoxLayout(left);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->addWidget(m_palette);

    QSplitter *center = new QSplitter(Qt::Horizontal);
    center->addWidget(left);
    center->addWidget(m_view);
    center->setStretchFactor(0, 0);
    center->setStretchFactor(1, 1);
    center->setSizes({ 210, 1000 });

    /* bottom: plot + console */
    m_plot = new PlotPane;
    m_console = new QPlainTextEdit;
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(2000);

    m_acModeCombo = new QComboBox;
    m_acModeCombo->addItem(tr("Magnitude (dB)"));
    m_acModeCombo->addItem(tr("Magnitude (linear)"));
    m_acModeCombo->addItem(tr("Phase (deg)"));
    m_acModeCombo->setEnabled(false);

    QWidget *plotTab = new QWidget;
    QVBoxLayout *plotLay = new QVBoxLayout(plotTab);
    plotLay->setContentsMargins(4, 4, 4, 4);
    QHBoxLayout *comboLay = new QHBoxLayout;
    comboLay->addWidget(m_acModeCombo);
    comboLay->addStretch(1);
    plotLay->addLayout(comboLay);
    plotLay->addWidget(m_plot);

    QTabWidget *bottom = new QTabWidget;
    bottom->addTab(plotTab, tr("Plot"));
    bottom->addTab(m_console, tr("Console"));

    QSplitter *main = new QSplitter(Qt::Vertical);
    main->addWidget(center);
    main->addWidget(bottom);
    main->setStretchFactor(0, 3);
    main->setStretchFactor(1, 2);
    setCentralWidget(main);

    m_progress = new QProgressBar;
    m_progress->setMaximumWidth(200);
    m_progress->setRange(0, 100);
    m_progress->hide();
    statusBar()->addPermanentWidget(m_progress);
    statusBar()->showMessage(tr("Ready"));

    connect(m_scene, &SchematicScene::probeWireClicked, this,
            [this](WireItem *w) { probeWire(w); });
    connect(m_scene, &SchematicScene::probePinClicked, this,
            [this](ComponentItem *c, int pin) { probePin(c, pin); });
    connect(m_scene, &SchematicScene::sceneChanged, this, &MainWindow::onSceneChanged);
    connect(&m_worker, &SimWorker::finished, this, &MainWindow::onFinished);
    connect(&m_worker, &SimWorker::progress, this, &MainWindow::onProgress);
    connect(m_acModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (m_worker.ok() && m_pendingAnalysis == SimWorker::Ac &&
                        m_worker.results().analysis == EC_AN_AC && m_worker.results().n_pts > 0)
                    presentAc();
            });
}

void MainWindow::fillPalette()
{
    QMap<QString, QList<const EcPlugin *>> groups;
    for (int i = 0; i < m_reg->n; i++) {
        const EcPlugin *p = m_reg->plugins[i];
        QString cat = p->category ? QString::fromLatin1(p->category) : tr("Other");
        groups[cat].append(p);
    }
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        QTreeWidgetItem *top = new QTreeWidgetItem(m_palette);
        top->setText(0, it.key());
        top->setFlags(Qt::ItemIsEnabled);
        for (const EcPlugin *p : qAsConst(it.value())) {
            QTreeWidgetItem *child = new QTreeWidgetItem(top);
            child->setText(0, QString::fromLatin1(p->label ? p->label : p->id));
            child->setData(0, Qt::UserRole, QString::fromLatin1(p->id));
            child->setFlags(Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            QString symbol = QString::fromLatin1(p->symbol ? p->symbol : "box");
            QRectF br = Elsim::symbolBoundingRect(symbol);
            QPixmap pm(56, 36);
            pm.fill(Qt::transparent);
            QPainter painter(&pm);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(palette().color(QPalette::Text), 1.6));
            painter.translate(28 - br.center().x(), 18 - br.center().y());
            Elsim::drawSymbol(&painter, symbol);
            painter.end();
            child->setIcon(0, QIcon(pm));
        }
        m_palette->expandItem(top);
    }
}

void MainWindow::log(const QString &msg)
{
    m_console->appendPlainText(msg);
}

void MainWindow::newFile()
{
    if (!maybeSave())
        return;
    m_scene->clearAll();
    m_plot->setSeries({}, QString(), QString());
    m_dcOp.clear();
    m_lastCompile.reset();
    m_filePath.clear();
    setWindowTitle(tr("elsim - electronics simulator"));
    statusBar()->showMessage(tr("Ready"));
}

bool MainWindow::maybeSave()
{
    if (m_scene->items().isEmpty())
        return true;
    auto ret = QMessageBox::question(this, tr("Discard changes?"),
                                     tr("The schematic will be cleared. Continue?"),
                                     QMessageBox::Yes | QMessageBox::No);
    return ret == QMessageBox::Yes;
}

void MainWindow::openFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open schematic"),
                                                QString(),
                                                tr("elsim schematics (*.elsim);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log(tr("Cannot open %1").arg(path));
        return;
    }
    if (!maybeSave())
        return;
    m_scene->clearAll();
    m_lastCompile.reset();
    QTextStream ts(&f);
    const QString body = ts.readAll();
    QHash<uint64_t, ComponentItem *> byUid;
    const QStringList lines = body.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        QStringList tok = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tok.isEmpty())
            continue;

        if (tok[0] == QLatin1String("wire") && tok.size() >= 5) {
            bool ok1 = false, ok2 = false;
            uint64_t u1 = tok[1].toULongLong(&ok1);
            uint64_t u2 = tok[3].toULongLong(&ok2);
            int p1 = tok[2].toInt();
            int p2 = tok[4].toInt();
            if (ok1 && ok2 && byUid.contains(u1) && byUid.contains(u2))
                m_scene->addWire(PinRef(byUid[u1], p1), PinRef(byUid[u2], p2));
            continue;
        }

        const EcPlugin *p = m_plugins.value(tok[0]);
        if (!p) {
            log(tr("Unknown component '%1' (plugin not loaded?)").arg(tok[0]));
            continue;
        }
        if (tok.size() < 2 + p->n_pins)
            continue;
        QString name = tok[1];
        double x = 0, y = 0;
        int rot = 0;
        QHash<int, double> params;
        for (int i = 2 + p->n_pins; i < tok.size(); i++) {
            const int eq = tok[i].indexOf(QLatin1Char('='));
            if (eq < 1)
                continue;
            QString key = tok[i].left(eq);
            QString val = tok[i].mid(eq + 1);
            int pi = ec_param_idx(p, key.toLatin1().constData());
            double v = 0;
            if (pi >= 0 && ec_parse_eng(val.toLatin1().constData(), &v) == 0)
                params.insert(pi, v);
            else if (key == QLatin1String("x"))
                x = val.toDouble();
            else if (key == QLatin1String("y"))
                y = val.toDouble();
            else if (key == QLatin1String("rot"))
                rot = val.toInt();
        }
        ComponentItem *item = m_scene->addComponent(p, QPointF(x, y), 0, name);
        for (auto pit = params.begin(); pit != params.end(); ++pit)
            item->setParam(pit.key(), pit.value());
        item->setRotationSteps(rot);
        byUid.insert(item->uid(), item);
    }
    m_filePath = path;
    setWindowTitle(tr("elsim - %1").arg(QFileInfo(path).fileName()));
    log(tr("Loaded %1").arg(path));
}

void MainWindow::saveFile()
{
    QString path = m_filePath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, tr("Save schematic"),
                                            QString(), tr("elsim schematics (*.elsim)"));
        if (path.isEmpty())
            return;
        if (!path.endsWith(QLatin1String(".elsim"), Qt::CaseInsensitive))
            path += QLatin1String(".elsim");
        m_filePath = path;
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        log(tr("Cannot write %1").arg(path));
        return;
    }
    QTextStream ts(&f);
    ts << "#elsim v1\n";
    for (ComponentItem *ci : m_scene->componentItems()) {
        const EcPlugin *p = ci->plugin();
        ts << QString::fromLatin1(p->id) << QLatin1Char(' ') << ci->name();
        for (int i = 0; i < p->n_pins; i++)
            ts << QLatin1Char(' ') << QLatin1Char('n') << i;
        for (int i = 0; i < p->n_params; i++)
            ts << QLatin1Char(' ') << QString::fromLatin1(p->param_names[i])
               << QLatin1Char('=') << QString::number(ci->param(i), 'g', 12);
        ts << " x=" << int(ci->pos().x()) << " y=" << int(ci->pos().y())
           << " rot=" << ci->rotationSteps() << QLatin1Char('\n');
    }
    for (WireItem *w : m_scene->wireItems())
        ts << "wire " << w->a.comp->uid() << " " << w->a.pin << " "
           << w->b.comp->uid() << " " << w->b.pin << QLatin1Char('\n');
    log(tr("Saved %1").arg(path));
    setWindowTitle(tr("elsim - %1").arg(QFileInfo(path).fileName()));
}

void MainWindow::showAnalysisSettings()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Analysis settings"));
    QFormLayout form(&dlg);
    const int which = m_analysisCombo->currentIndex();

    QDoubleSpinBox *f1 = new QDoubleSpinBox(&dlg);
    QDoubleSpinBox *f2 = new QDoubleSpinBox(&dlg);
    QSpinBox *npts = new QSpinBox(&dlg);
    QCheckBox *logck = new QCheckBox(tr("logarithmic sweep"), &dlg);
    QDoubleSpinBox *tstop = new QDoubleSpinBox(&dlg);
    QDoubleSpinBox *dt = new QDoubleSpinBox(&dlg);
    QComboBox *method = new QComboBox(&dlg);

    f1->setDecimals(6); f1->setRange(1e-6, 1e9); f1->setValue(m_ac.f1);
    f2->setDecimals(6); f2->setRange(1e-6, 1e12); f2->setValue(m_ac.f2);
    npts->setRange(2, 20000); npts->setValue(m_ac.n);
    logck->setChecked(m_ac.log);
    tstop->setDecimals(9); tstop->setRange(1e-12, 1e3); tstop->setValue(m_tr.tstop);
    tstop->setSuffix(tr(" s"));
    dt->setDecimals(12); dt->setRange(1e-15, 1e3); dt->setValue(m_tr.dt);
    dt->setSuffix(tr(" s"));
    method->addItem(tr("trapezoidal (2nd order)"));
    method->addItem(tr("backward euler (robust)"));
    method->setCurrentIndex(m_tr.method == EC_METHOD_TRAP ? 0 : 1);

    if (which == 1) {
        form.addRow(tr("Start frequency [Hz]:"), f1);
        form.addRow(tr("Stop frequency [Hz]:"), f2);
        form.addRow(tr("Points:"), npts);
        form.addRow(QString(), logck);
    } else {
        form.addRow(tr("Stop time [s]:"), tstop);
        form.addRow(tr("Time step [s]:"), dt);
        form.addRow(tr("Method:"), method);
    }
    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                  Qt::Horizontal, &dlg);
    form.addRow(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() == QDialog::Accepted) {
        m_ac.f1 = f1->value();
        m_ac.f2 = f2->value();
        m_ac.n = npts->value();
        m_ac.log = logck->isChecked();
        m_tr.tstop = tstop->value();
        m_tr.dt = dt->value();
        m_tr.method = method->currentIndex() == 0 ? EC_METHOD_TRAP : EC_METHOD_BE;
    }
}

void MainWindow::runAnalysis()
{
    Compiled c;
    QString err;
    if (!compileCircuit(m_scene, &c, &err)) {
        log(err);
        statusBar()->showMessage(err, 5000);
        return;
    }
    /* adopt the scene mappings so probes stay resolvable; the circuit
     * itself moves into the worker (which frees it after the run) */
    m_lastCompile.reset();
    m_lastCompile.netsByComp = c.netsByComp;
    m_lastCompile.nameByUid = c.nameByUid;
    m_lastCompile.netByWire = c.netByWire;
    m_lastCompile.pluginByUid = c.pluginByUid;
    m_scene->setWireNets(m_lastCompile.netByWire);
    m_compileDirty = false;

    m_pendingAnalysis = static_cast<SimWorker::Analysis>(m_analysisCombo->currentIndex());
    EcCircuit *cir = c.cir;
    c.cir = nullptr;

    m_startedRunId = m_worker.runId() + 1;
    m_progress->setValue(0);
    m_progress->setVisible(m_pendingAnalysis == SimWorker::Tr);
    statusBar()->showMessage(tr("Running analysis..."));

    switch (m_pendingAnalysis) {
    case SimWorker::Dc:
        m_worker.runDc(cir);
        break;
    case SimWorker::Ac:
        m_worker.runAc(cir, m_ac.f1, m_ac.f2, m_ac.n, m_ac.log);
        break;
    case SimWorker::Tr:
        m_worker.runTr(cir, m_tr.tstop, m_tr.dt, m_tr.method);
        break;
    }
}

void MainWindow::onProgress(int pct)
{
    m_progress->setValue(pct);
}

void MainWindow::onFinished(int runId)
{
    if (runId != m_startedRunId)
        return; /* stale */
    m_progress->hide();
    if (!m_worker.ok()) {
        QString msg = QString::fromUtf8(m_worker.results().message);
        log(tr("Analysis failed: %1").arg(msg.isEmpty() ? tr("unknown error") : msg));
        statusBar()->showMessage(tr("Analysis failed"), 5000);
        return;
    }
    statusBar()->showMessage(tr("Done"), 3000);
    switch (m_pendingAnalysis) {
    case SimWorker::Dc: presentDc(); break;
    case SimWorker::Ac: presentAc(); break;
    case SimWorker::Tr: presentTr(); break;
    }
}

static QColor probeColorAt(int idx)
{
    static const QColor colors[] = {
        QColor(200, 30, 30), QColor(20, 120, 220), QColor(20, 150, 60),
        QColor(190, 130, 0), QColor(150, 40, 180), QColor(0, 160, 160),
        QColor(240, 120, 0), QColor(90, 90, 220),
    };
    return colors[idx % 8];
}

void MainWindow::presentDc()
{
    const EcResults &r = m_worker.results();
    m_dcOp.clear();
    m_dcOp.resize(r.dim + 1);
    for (int i = 0; i <= r.dim; i++)
        m_dcOp[i] = r.x[i];
    m_scene->setDcOp(m_dcOp.constData(), r.dim);

    QString txt = tr("== DC operating point ==");
    log(txt);
    for (int n = 1; n <= r.n_nodes; n++)
        log(tr("node %1: %2 V").arg(n).arg(QString::number(ec_results_v(&r, 0, n), 'g', 6)));

    m_acModeCombo->setEnabled(false);
    QVector<PlotSeries> series;
    int ci = 0;
    for (int net : m_scene->probedNets) {
        PlotSeries s;
        s.label = tr("V(node %1)").arg(net);
        s.color = probeColorAt(ci++);
        s.pts.append(QPointF(0, ec_results_v(&r, 0, net)));
        series.append(s);
    }
    if (!series.isEmpty())
        m_plot->setSeries(series, QString(), tr("voltage [V]"));
}

void MainWindow::presentTr()
{
    const EcResults &r = m_worker.results();
    m_acModeCombo->setEnabled(false);
    QVector<PlotSeries> series;
    int ci = 0;
    for (int net : m_scene->probedNets) {
        PlotSeries s;
        s.label = tr("V(node %1)").arg(net);
        s.color = probeColorAt(ci++);
        s.pts.reserve(r.n_pts);
        for (int p = 0; p < r.n_pts; p++)
            s.pts.append(QPointF(r.sweep[p], ec_results_v(&r, p, net)));
        series.append(s);
    }
    if (series.isEmpty())
        log(tr("Transient finished - switch to Probe mode and click wires to plot them."));
    m_plot->setSeries(series, tr("time [s]"), tr("voltage [V]"));
    log(tr("Transient done: %1 points over %2 - %3 s")
            .arg(r.n_pts)
            .arg(QString::number(r.sweep[0], 'g', 4))
            .arg(QString::number(r.sweep[r.n_pts - 1], 'g', 4)));
}

void MainWindow::presentAc()
{
    const EcResults &r = m_worker.results();
    m_acModeCombo->setEnabled(true);
    const int mode = m_acModeCombo->currentIndex();
    QVector<PlotSeries> series;
    int ci = 0;
    for (int net : m_scene->probedNets) {
        PlotSeries s;
        s.label = tr("V(node %1)").arg(net);
        s.color = probeColorAt(ci++);
        s.pts.reserve(r.n_pts);
        for (int p = 0; p < r.n_pts; p++) {
            double y;
            if (mode == 2)
                y = ec_results_phase(&r, p, net);
            else if (mode == 1)
                y = ec_results_v(&r, p, net);
            else
                y = 20.0 * log10(qMax(ec_results_v(&r, p, net), 1e-30));
            s.pts.append(QPointF(r.sweep[p], y));
        }
        series.append(s);
    }
    if (series.isEmpty())
        log(tr("AC sweep finished - switch to Probe mode and click wires to plot them."));
    QString ylabel = mode == 2 ? tr("phase [deg]")
                  : (mode == 1 ? tr("|V| [V]") : tr("|V| [dB]"));
    m_plot->setSeries(series, tr("frequency [Hz]"), ylabel, m_ac.log);
    log(tr("AC sweep done: %1 points").arg(r.n_pts));
}

bool MainWindow::ensureCompiled()
{
    if (!m_compileDirty)
        return true;
    Compiled c;
    QString err;
    if (!compileCircuit(m_scene, &c, &err)) {
        log(err);
        return false;
    }
    m_lastCompile.reset();
    m_lastCompile.netsByComp = c.netsByComp;
    m_lastCompile.nameByUid = c.nameByUid;
    m_lastCompile.netByWire = c.netByWire;
    m_lastCompile.pluginByUid = c.pluginByUid;
    m_scene->setWireNets(m_lastCompile.netByWire);
    m_compileDirty = false;
    return true;
}

void MainWindow::probeWire(WireItem *w)
{
    if (!ensureCompiled())
        return;
    int net = m_lastCompile.netByWire.value(w, -1);
    if (net < 0)
        return;
    if (m_scene->probedNets.contains(net))
        m_scene->probedNets.remove(net);
    else
        m_scene->probedNets.insert(net);
    m_scene->update();
    log(tr("Probe V(node %1): %2").arg(net)
            .arg(m_scene->probedNets.contains(net) ? tr("on") : tr("off")));
}

void MainWindow::probePin(ComponentItem *c, int pin)
{
    if (!ensureCompiled())
        return;
    QVector<int> nets = m_lastCompile.netsByComp.value(c->uid());
    if (pin >= nets.size())
        return;
    int net = nets.at(pin);
    if (m_scene->probedNets.contains(net))
        m_scene->probedNets.remove(net);
    else
        m_scene->probedNets.insert(net);
    m_scene->update();
    log(tr("Probe V(node %1): %2").arg(net)
            .arg(m_scene->probedNets.contains(net) ? tr("on") : tr("off")));
}

void MainWindow::onSceneChanged()
{
    /* net numbering may have shifted; drop stale probe/net info */
    m_scene->setWireNets({});
    m_scene->probedNets.clear();
    m_scene->setDcOp(nullptr, 0);
    m_dcOp.clear();
    m_lastCompile.reset();
    m_compileDirty = true;
}
