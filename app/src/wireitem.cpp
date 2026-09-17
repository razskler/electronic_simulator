#include "wireitem.h"

#include <QApplication>
#include <QPainter>
#include <QStyleOption>

#include "schematicscene.h"

WireItem::WireItem(const PinRef &a_, const PinRef &b_)
    : a(a_), b(b_)
{
    setFlag(ItemIsSelectable);
    setAcceptHoverEvents(true);
    m_cachedRect = line().boundingRect().adjusted(-8, -8, 8, 8);
}

QLineF WireItem::line() const
{
    return QLineF(a.comp ? a.comp->pinScene(a.pin) : QPointF(),
                  b.comp ? b.comp->pinScene(b.pin) : QPointF());
}

QRectF WireItem::boundingRect() const
{
    return m_cachedRect;
}

void WireItem::sync()
{
    prepareGeometryChange();
    m_cachedRect = line().boundingRect().adjusted(-8, -8, 8, 8);
    update();
}

void WireItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                     QWidget *widget)
{
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing);
    QColor textColor = qApp->palette().text().color();

    SchematicScene *sc = qobject_cast<SchematicScene *>(scene());
    int net = sc ? sc->wireNet(this) : -1;
    bool probed = sc && net >= 0 && sc->isNetProbed(net);

    QPen pen(textColor, 2.0);
    if (probed) {
        QColor c = sc->probeColor(net);
        pen = QPen(c, 3.0);
    }
    if (option->state & QStyle::State_Selected)
        pen.setStyle(Qt::DashLine);
    painter->setPen(pen);
    QLineF l = line();
    painter->drawLine(l);

    /* junction dots */
    painter->setBrush(pen.color());
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(l.p1(), 2.6, 2.6);
    painter->drawEllipse(l.p2(), 2.6, 2.6);

    /* DC operating point label on probed wires */
    if (probed && sc && sc->hasDcOp()) {
        double v = sc->dcVoltage(net);
        if (!qIsNaN(v)) {
            QFont f = painter->font();
            f.setPointSizeF(8.5);
            painter->setFont(f);
            QPointF mid((l.p1().x() + l.p2().x()) / 2.0,
                        (l.p1().y() + l.p2().y()) / 2.0 - 12.0);
            painter->setPen(QPen(pen.color().darker(140), 1));
            QString txt = QString::fromUtf8("%1 V").arg(QString::number(v, 'f', 3));
            painter->drawText(QRectF(mid - QPointF(34, 8), QSizeF(68, 14)),
                              Qt::AlignHCenter | Qt::AlignVCenter, txt);
        }
    }
}
