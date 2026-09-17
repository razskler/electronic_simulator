#pragma once

#include <QGraphicsScene>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QHash>
#include <QKeyEvent>
#include <QSet>

#include "componentitem.h"

class QGraphicsLineItem;
class WireItem;

class SchematicScene : public QGraphicsScene
{
    Q_OBJECT

public:
    enum Mode { SelectMode, ProbeMode };

    explicit SchematicScene(QObject *parent = nullptr);

    void setMode(Mode m) { m_mode = m; }
    Mode mode() const { return m_mode; }

    ComponentItem *addComponent(const EcPlugin *plugin, const QPointF &scenePos,
                                uint64_t uid = 0, const QString &name = QString());
    WireItem *addWire(const PinRef &a, const PinRef &b);
    QList<ComponentItem *> componentItems() const;
    QList<WireItem *> wireItems() const;
    void deleteSelected();
    void rotateSelected();
    void editComponent(ComponentItem *c);
    void componentMoved(ComponentItem *c);

    /* probe + result state (net ids are assigned by the netlist compiler) */
    QSet<int> probedNets;
    bool isNetProbed(int net) const { return probedNets.contains(net); }
    void setWireNets(const QHash<WireItem *, int> &nets) { m_wireNet = nets; update(); }
    int wireNet(const WireItem *w) const { return m_wireNet.value(const_cast<WireItem *>(w), -1); }
    void setDcOp(const double *x, int dim) { m_dcOp = x; m_dcDim = dim; update(); }
    bool hasDcOp() const { return m_dcOp != nullptr; }
    double dcVoltage(int net) const
    { return (m_dcOp && net >= 0 && net <= m_dcDim) ? m_dcOp[net] : qQNaN(); }
    QColor probeColor(int net) const;

    QString uniqueName(const EcPlugin *p);
    void clearAll();
    uint64_t nextUid() { return m_nextUid++; }
    void ensureUidBeyond(uint64_t uid) { m_nextUid = qMax(m_nextUid, uid + 1); }

signals:
    void sceneChanged();
    void probeWireClicked(WireItem *wire);
    void probePinClicked(ComponentItem *comp, int pin);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    int pinHit(const QPointF &scenePos, PinRef *out) const;

    Mode m_mode = SelectMode;
    bool m_wiring = false;
    PinRef m_wireFrom;
    QGraphicsLineItem *m_rubber = nullptr;
    QHash<WireItem *, int> m_wireNet;
    const double *m_dcOp = nullptr;
    int m_dcDim = 0;
    uint64_t m_nextUid = 1;
    QHash<QString, int> m_nameCounters;
};

/* Grid-drawing zoomable view; also the drop target for palette drags. */
class SchematicView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit SchematicView(QGraphicsScene *scene, QWidget *parent = nullptr);

    /* filled in by MainWindow; resolves plugin ids on drop */
    QHash<QString, const EcPlugin *> plugins;

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    SchematicScene *m_scene = nullptr;
};
