#pragma once

#include <QGraphicsItem>

#include "componentitem.h"

class SchematicScene;

class WireItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 2 };

    WireItem(const PinRef &a, const PinRef &b);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    PinRef a, b;
    QLineF line() const;
    /* Called when an endpoint component moved */
    void sync();

private:
    QRectF m_cachedRect;
};
