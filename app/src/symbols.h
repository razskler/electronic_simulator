#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

namespace Elsim {

/* Schematic symbol geometry shared by the canvas items and the palette.
 * Two-pin symbols are drawn horizontally: pin 0 (p/anode) at x = -30,
 * pin 1 (n/cathode) at x = +30. The ground symbol has a single pin on top. */

QVector<QPointF> symbolPins(const QString &symbol);
QRectF symbolBoundingRect(const QString &symbol);
void drawSymbol(QPainter *p, const QString &symbol);

/* Format a value with engineering suffix: 4700 -> "4.7k" */
QString engValue(double v, int significant = 3);

} // namespace Elsim
