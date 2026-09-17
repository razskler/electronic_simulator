#include "componentitem.h"

#include <QApplication>
#include <QPainter>
#include <QStyleOption>

#include "schematicscene.h"
#include "symbols.h"

static const int kGrid = 20;

ComponentItem::ComponentItem(const EcPlugin *plugin, uint64_t uid,
                             const QString &name, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_plugin(plugin), m_name(name), m_uid(uid)
{
    for (int i = 0; i < plugin->n_params && i < EC_MAX_PARAMS; i++)
        m_params.append(plugin->param_defaults[i]);
    setFlag(ItemIsMovable);
    setFlag(ItemIsSelectable);
    setFlag(ItemSendsGeometryChanges);
    setRotationSteps(0);
}

QRectF ComponentItem::boundingRect() const
{
    QRectF r = Elsim::symbolBoundingRect(QString::fromLatin1(m_plugin->symbol ? m_plugin->symbol : "box"));
    return r.adjusted(-6, -22, 6, 22); /* room for name/value labels */
}

void ComponentItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                          QWidget *widget)
{
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing);
    QColor textColor = qApp->palette().text().color();

    QPen pen(textColor, 2.0);
    if (option->state & QStyle::State_Selected) {
        pen = QPen(Qt::darkBlue, 2.0);
        painter->setPen(QPen(QColor(30, 90, 220, 90), 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(boundingRect().adjusted(2, 16, -2, -16));
    }
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    Elsim::drawSymbol(painter, QString::fromLatin1(m_plugin->symbol ? m_plugin->symbol : "box"));

    /* labels stay in item orientation */
    QFont f = painter->font();
    f.setPointSizeF(8.5);
    painter->setFont(f);
    painter->setPen(QPen(textColor, 1));
    QRectF nameRect(-48, -34, 96, 14);
    painter->drawText(nameRect, Qt::AlignHCenter | Qt::AlignVCenter, m_name);
    if (m_plugin->n_params > 0) {
        QString unit = m_plugin->param_units[0] ? QString::fromLatin1(m_plugin->param_units[0]) : QString();
        QString val = Elsim::engValue(m_params.value(0));
        if (!unit.isEmpty() && unit != QLatin1String("-"))
            val += QLatin1Char(' ') + unit;
        QRectF valRect(-48, 22, 96, 14);
        painter->drawText(valRect, Qt::AlignHCenter | Qt::AlignVCenter, val);
    }
}

void ComponentItem::setName(const QString &name)
{
    m_name = name;
    update();
}

void ComponentItem::setParam(int i, double v)
{
    if (i >= 0 && i < m_params.size()) {
        m_params[i] = v;
        update();
    }
}

void ComponentItem::setRotationSteps(int steps)
{
    m_rot = ((steps % 4) + 4) % 4;
    setRotation(m_rot * 90.0);
}

QPointF ComponentItem::pinLocal(int i) const
{
    const QVector<QPointF> pins = Elsim::symbolPins(QString::fromLatin1(m_plugin->symbol ? m_plugin->symbol : "box"));
    if (i >= 0 && i < pins.size())
        return pins[i];
    return QPointF();
}

QPointF ComponentItem::pinScene(int i) const
{
    return mapToScene(pinLocal(i));
}

int ComponentItem::pinAt(const QPointF &scenePos, double maxDist) const
{
    const int n = m_plugin->n_pins;
    for (int i = 0; i < n; i++) {
        QPointF d = pinScene(i) - scenePos;
        if (qSqrt(d.x() * d.x() + d.y() * d.y()) <= maxDist)
            return i;
    }
    return -1;
}

void ComponentItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    SchematicScene *sc = qobject_cast<SchematicScene *>(scene());
    if (sc)
        sc->editComponent(this);
}

QVariant ComponentItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {
        QPointF pos = value.toPointF();
        pos.setX(qRound(pos.x() / kGrid) * kGrid);
        pos.setY(qRound(pos.y() / kGrid) * kGrid);
        return pos;
    }
    if (change == ItemPositionHasChanged && scene()) {
        SchematicScene *sc = qobject_cast<SchematicScene *>(scene());
        if (sc)
            sc->componentMoved(this);
    }
    return QGraphicsItem::itemChange(change, value);
}
