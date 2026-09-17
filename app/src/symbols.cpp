#include "symbols.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>

namespace Elsim {

QVector<QPointF> symbolPins(const QString &symbol)
{
    if (symbol == QLatin1String("ground"))
        return { QPointF(0, -14) };
    return { QPointF(-30, 0), QPointF(30, 0) };
}

QRectF symbolBoundingRect(const QString &symbol)
{
    if (symbol == QLatin1String("ground"))
        return QRectF(-16, -16, 32, 28);
    return QRectF(-32, -16, 64, 32);
}

static void drawLeads(QPainter *p, double x0, double x1)
{
    p->drawLine(QPointF(x0, 0), QPointF(x1, 0));
}

void drawSymbol(QPainter *p, const QString &symbol)
{
    QPen pen = p->pen();
    pen.setWidthF(2.0);
    pen.setCapStyle(Qt::RoundCap);
    p->setPen(pen);

    if (symbol == QLatin1String("resistor")) {
        drawLeads(p, -30, -18);
        drawLeads(p, 18, 30);
        QPolygonF z;
        z << QPointF(-18, 0) << QPointF(-14, -8) << QPointF(-6, 8)
          << QPointF(2, -8) << QPointF(10, 8) << QPointF(14, 0) << QPointF(18, 0);
        p->drawPolyline(z);
    } else if (symbol == QLatin1String("capacitor")) {
        drawLeads(p, -30, -6);
        drawLeads(p, 6, 30);
        p->drawLine(QPointF(-6, -10), QPointF(-6, 10));
        p->drawLine(QPointF(6, -10), QPointF(6, 10));
    } else if (symbol == QLatin1String("inductor")) {
        drawLeads(p, -30, -18);
        drawLeads(p, 18, 30);
        for (int i = 0; i < 3; i++)
            p->drawArc(QRectF(-18.0 + i * 12.0, -6.0, 12.0, 12.0), 180 * 16, -180 * 16);
    } else if (symbol == QLatin1String("vsource") || symbol == QLatin1String("isource")) {
        drawLeads(p, -30, -13);
        drawLeads(p, 13, 30);
        p->drawEllipse(QRectF(-13, -13, 26, 26));
        if (symbol == QLatin1String("vsource")) {
            p->drawLine(QPointF(-9, 0), QPointF(-3, 0));
            p->drawLine(QPointF(-6, -3), QPointF(-6, 3));
            p->drawLine(QPointF(3, 0), QPointF(9, 0));
        } else {
            p->drawLine(QPointF(-7, 0), QPointF(7, 0));
            p->drawLine(QPointF(7, 0), QPointF(2, -4));
            p->drawLine(QPointF(7, 0), QPointF(2, 4));
        }
    } else if (symbol == QLatin1String("diode") || symbol == QLatin1String("led")) {
        drawLeads(p, -30, -10);
        drawLeads(p, 10, 30);
        QPolygonF tri;
        tri << QPointF(-10, -9) << QPointF(-10, 9) << QPointF(10, 0);
        p->setBrush(p->pen().color());
        p->drawPolygon(tri);
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(10, -9), QPointF(10, 9));
        if (symbol == QLatin1String("led")) {
            p->drawLine(QPointF(0, -11), QPointF(6, -17));
            p->drawLine(QPointF(6, -17), QPointF(2, -17));
            p->drawLine(QPointF(6, -17), QPointF(6, -13));
            p->drawLine(QPointF(-6, -11), QPointF(0, -17));
            p->drawLine(QPointF(0, -17), QPointF(-4, -17));
            p->drawLine(QPointF(0, -17), QPointF(0, -13));
        }
    } else if (symbol == QLatin1String("switch")) {
        drawLeads(p, -30, -10);
        drawLeads(p, 10, 30);
        p->drawEllipse(QRectF(-12, -2, 4, 4));
        p->drawEllipse(QRectF(8, -2, 4, 4));
        p->drawLine(QPointF(-10, 0), QPointF(8, -10));
    } else if (symbol == QLatin1String("ground")) {
        p->drawLine(QPointF(0, -14), QPointF(0, -4));
        p->drawLine(QPointF(-12, -4), QPointF(12, -4));
        p->drawLine(QPointF(-8, 1), QPointF(8, 1));
        p->drawLine(QPointF(-4, 6), QPointF(4, 6));
    } else { /* box / unknown */
        drawLeads(p, -30, -14);
        drawLeads(p, 14, 30);
        p->drawRect(QRectF(-14, -10, 28, 20));
    }
}

QString engValue(double v, int significant)
{
    if (v == 0.0)
        return QStringLiteral("0");
    bool neg = v < 0;
    double a = qAbs(v);
    static const struct { double scale; char suf; } table[] = {
        { 1e9, 'G' }, { 1e6, 'M' }, { 1e3, 'k' }, { 1.0, 0 },
        { 1e-3, 'm' }, { 1e-6, 'u' }, { 1e-9, 'n' }, { 1e-12, 'p' },
    };
    for (auto &t : table) {
        if (a >= t.scale) {
            double m = a / t.scale;
            QString s = QString::number(m, 'f', significant - 1);
            if (s.endsWith(QLatin1String(".0")))
                s.chop(2);
            if (t.suf)
                s += QChar(t.suf);
            return neg ? QStringLiteral("-") + s : s;
        }
    }
    return QString::number(v, 'g', significant);
}

} // namespace Elsim
