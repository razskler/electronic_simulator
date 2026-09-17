#include "schematicscene.h"

#include <QApplication>
#include <QDialog>
#include <QDrag>
#include <QDoubleValidator>
#include <QGraphicsLineItem>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QStyleOption>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtMath>
#include <QtWidgets>

#include "netlistcompiler.h"
#include "symbols.h"
#include "wireitem.h"

static const char *kMimeComponent = "application/x-elsim-comp";
static const int kGrid = 20;

SchematicScene::SchematicScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-2000, -1500, 4000, 3000);
    setBackgroundBrush(QBrush(Qt::white));
}

QString SchematicScene::uniqueName(const EcPlugin *p)
{
    static const struct { const char *id; const char *prefix; } map[] = {
        { "resistor", "R" }, { "capacitor", "C" }, { "inductor", "L" },
        { "vsource", "V" }, { "isource", "I" }, { "diode", "D" },
        { "led", "D" }, { "switch", "SW" }, { "ground", "GND" },
    };
    QString prefix = QString::fromLatin1(p->id ? p->id : "X").left(2).toUpper();
    for (auto &m : map)
        if (p->id && strcmp(p->id, m.id) == 0)
            prefix = QLatin1String(m.prefix);
    if (prefix == QLatin1String("GND"))
        return prefix; /* grounds are reference symbols, no numbering */
    int n = ++m_nameCounters[prefix];
    return QString::fromLatin1("%1%2").arg(prefix).arg(n);
}

ComponentItem *SchematicScene::addComponent(const EcPlugin *plugin,
                                            const QPointF &scenePos, uint64_t uid,
                                            const QString &name)
{
    if (uid == 0)
        uid = nextUid();
    ensureUidBeyond(uid);
    ComponentItem *item = new ComponentItem(plugin, uid,
                                            name.isEmpty() ? uniqueName(plugin) : name);
    item->setPos(qRound(scenePos.x() / kGrid) * kGrid,
                 qRound(scenePos.y() / kGrid) * kGrid);
    addItem(item);
    emit sceneChanged();
    return item;
}

WireItem *SchematicScene::addWire(const PinRef &a, const PinRef &b)
{
    for (WireItem *w : wireItems()) {
        if ((w->a == a && w->b == b) || (w->a == b && w->b == a))
            return nullptr; /* duplicate */
    }
    WireItem *w = new WireItem(a, b);
    addItem(w);
    emit sceneChanged();
    return w;
}

QList<ComponentItem *> SchematicScene::componentItems() const
{
    QList<ComponentItem *> out;
    for (QGraphicsItem *it : items()) {
        if (it->type() == ComponentItem::Type)
            out.append(static_cast<ComponentItem *>(it));
    }
    return out;
}

QList<WireItem *> SchematicScene::wireItems() const
{
    QList<WireItem *> out;
    for (QGraphicsItem *it : items()) {
        if (it->type() == WireItem::Type)
            out.append(static_cast<WireItem *>(it));
    }
    return out;
}

void SchematicScene::deleteSelected()
{
    QList<QGraphicsItem *> selected = selectedItems();
    QList<QGraphicsItem *> toDelete;
    for (QGraphicsItem *it : selected) {
        if (it->type() == ComponentItem::Type) {
            auto *ci = static_cast<ComponentItem *>(it);
            toDelete.append(ci);
            for (WireItem *w : wireItems()) {
                if (w->a.comp == ci || w->b.comp == ci)
                    toDelete.append(w);
            }
        } else if (it->type() == WireItem::Type) {
            toDelete.append(it);
        }
    }
    /* dedupe (a wire may be reached twice) */
    QSet<QGraphicsItem *> seen;
    bool removed = false;
    for (QGraphicsItem *it : toDelete) {
        if (seen.contains(it))
            continue;
        seen.insert(it);
        removeItem(it);
        delete it;
        removed = true;
    }
    if (removed)
        emit sceneChanged();
}

void SchematicScene::rotateSelected()
{
    for (QGraphicsItem *it : selectedItems()) {
        if (it->type() == ComponentItem::Type) {
            auto *ci = static_cast<ComponentItem *>(it);
            ci->setRotationSteps(ci->rotationSteps() + 1);
            componentMoved(ci);
            emit sceneChanged();
        }
    }
}

QColor SchematicScene::probeColor(int net) const
{
    static const QColor colors[] = {
        QColor(200, 30, 30), QColor(20, 120, 220), QColor(20, 150, 60),
        QColor(190, 130, 0), QColor(150, 40, 180), QColor(0, 160, 160),
        QColor(240, 120, 0), QColor(90, 90, 220),
    };
    int idx = 0;
    for (int n : probedNets) {
        if (n >= net)
            continue;
        idx++;
    }
    return colors[idx % 8];
}

void SchematicScene::componentMoved(ComponentItem *c)
{
    for (WireItem *w : wireItems()) {
        if (w->a.comp == c || w->b.comp == c)
            w->sync();
    }
}

void SchematicScene::clearAll()
{
    clear();
    m_wireNet.clear();
    probedNets.clear();
    m_dcOp = nullptr;
    m_dcDim = 0;
    m_nextUid = 1;
    m_nameCounters.clear();
    emit sceneChanged();
}

int SchematicScene::pinHit(const QPointF &scenePos, PinRef *out) const
{
    for (ComponentItem *ci : componentItems()) {
        int pin = ci->pinAt(scenePos, 9.0);
        if (pin >= 0) {
            out->comp = ci;
            out->pin = pin;
            return pin;
        }
    }
    return -1;
}

void SchematicScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_mode == ProbeMode && event->button() == Qt::LeftButton) {
        PinRef ref;
        if (pinHit(event->scenePos(), &ref) >= 0) {
            emit probePinClicked(ref.comp, ref.pin);
        } else {
            QGraphicsItem *it = itemAt(event->scenePos(), QTransform());
            if (it && it->type() == WireItem::Type)
                emit probeWireClicked(static_cast<WireItem *>(it));
        }
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        PinRef ref;
        if (pinHit(event->scenePos(), &ref) >= 0) {
            m_wiring = true;
            m_wireFrom = ref;
            m_rubber = new QGraphicsLineItem(QLineF(ref.comp->pinScene(ref.pin),
                                                    event->scenePos()));
            m_rubber->setPen(QPen(QColor(30, 90, 220), 2, Qt::DashLine));
            addItem(m_rubber);
            event->accept();
            return;
        }
    }
    QGraphicsScene::mousePressEvent(event);
}

void SchematicScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_wiring && m_rubber) {
        PinRef ref;
        QPointF end = event->scenePos();
        if (pinHit(event->scenePos(), &ref) >= 0 && !(ref == m_wireFrom))
            end = ref.comp->pinScene(ref.pin);
        m_rubber->setLine(QLineF(m_wireFrom.comp->pinScene(m_wireFrom.pin), end));
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void SchematicScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_wiring && event->button() == Qt::LeftButton) {
        PinRef ref;
        if (pinHit(event->scenePos(), &ref) >= 0 &&
            !(ref.comp == m_wireFrom.comp && ref.pin == m_wireFrom.pin)) {
            addWire(m_wireFrom, ref);
        }
        m_wiring = false;
        if (m_rubber) {
            removeItem(m_rubber);
            delete m_rubber;
            m_rubber = nullptr;
        }
        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void SchematicScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_R)
        rotateSelected();
    else if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
        deleteSelected();
    else
        QGraphicsScene::keyPressEvent(event);
}

void SchematicScene::editComponent(ComponentItem *c)
{
    const EcPlugin *p = c->plugin();
    QWidget *parent = views().isEmpty() ? nullptr : views().first()->window();
    QDialog dlg(parent);
    dlg.setWindowTitle(tr("Edit %1").arg(c->name()));
    QFormLayout form(&dlg);
    QLineEdit *nameEdit = new QLineEdit(c->name(), &dlg);
    form.addRow(tr("Name:"), nameEdit);
    QList<QLineEdit *> edits;
    for (int i = 0; i < p->n_params; i++) {
        auto *edit = new QLineEdit(&dlg);
        edit->setText(QString::number(c->param(i), 'g', 12));
        auto *val = new QDoubleValidator(edit);
        val->setLocale(QLocale::c());
        val->setRange(-1e300, 1e300);
        edit->setValidator(val);
        QString label = QString::fromLatin1(p->param_names[i]);
        if (p->param_units[i] && *p->param_units[i] && strcmp(p->param_units[i], "-"))
            label += QLatin1String(" [") + QString::fromLatin1(p->param_units[i]) + QLatin1String("]");
        edits.append(edit);
        form.addRow(label, edit);
    }
    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                                  Qt::Horizontal, &dlg);
    form.addRow(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() == QDialog::Accepted) {
        c->setName(nameEdit->text().simplified());
        for (int i = 0; i < edits.size(); i++) {
            bool okv = false;
            double v = edits[i]->text().toDouble(&okv);
            if (okv)
                c->setParam(i, v);
        }
        emit sceneChanged();
    }
}

/* ---------------- SchematicView ---------------- */

SchematicView::SchematicView(QGraphicsScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent)
{
    m_scene = qobject_cast<SchematicScene *>(scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(RubberBandDrag);
    setAcceptDrops(true);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

void SchematicView::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, Qt::white);
    QPen dot(QColor(210, 210, 210), 1);
    painter->setPen(dot);
    int left = int(rect.left()) - (int(rect.left()) % kGrid);
    int top = int(rect.top()) - (int(rect.top()) % kGrid);
    for (int x = left; x <= int(rect.right()); x += kGrid)
        for (int y = top; y <= int(rect.bottom()); y += kGrid)
            painter->drawPoint(x, y);
}

void SchematicView::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF delta = event->angleDelta();
#else
    const QPoint delta = event->angleDelta();
#endif
    double factor = delta.y() > 0 ? 1.15 : 1.0 / 1.15;
    scale(factor, factor);
    event->accept();
}

static bool isComponentDrag(const QMimeData *mime)
{
    return mime && mime->hasFormat(QLatin1String(kMimeComponent));
}

void SchematicView::dragEnterEvent(QDragEnterEvent *event)
{
    if (isComponentDrag(event->mimeData()))
        event->acceptProposedAction();
}

void SchematicView::dragMoveEvent(QDragMoveEvent *event)
{
    if (isComponentDrag(event->mimeData()))
        event->acceptProposedAction();
}

void SchematicView::dropEvent(QDropEvent *event)
{
    if (!isComponentDrag(event->mimeData()))
        return;
    QByteArray id = event->mimeData()->data(QLatin1String(kMimeComponent));
    if (!m_scene)
        return;
    const EcPlugin *p = plugins.value(QString::fromUtf8(id));
    if (!p)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QPointF sp = mapToScene(event->position().toPoint());
#else
    QPointF sp = mapToScene(event->pos());
#endif
    m_scene->addComponent(p, sp);
    event->acceptProposedAction();
}

void SchematicView::keyPressEvent(QKeyEvent *event)
{
    if (m_scene && event->key() == Qt::Key_R && !event->isAutoRepeat()) {
        m_scene->rotateSelected();
        event->accept();
        return;
    }
    if (m_scene && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
        m_scene->deleteSelected();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}
