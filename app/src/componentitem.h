#pragma once

#include <QGraphicsItem>
#include <QString>
#include <QVector>

#include "ec/plugin.h"

class SchematicScene;

class ComponentItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 1 };

    ComponentItem(const EcPlugin *plugin, uint64_t uid, const QString &name,
                  QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;

    const EcPlugin *plugin() const { return m_plugin; }
    QString name() const { return m_name; }
    void setName(const QString &name);
    double param(int i) const { return m_params.value(i); }
    void setParam(int i, double v);
    int paramCount() const { return m_plugin->n_params; }
    uint64_t uid() const { return m_uid; }

    int rotationSteps() const { return m_rot; }
    void setRotationSteps(int steps);

    QPointF pinLocal(int i) const;
    QPointF pinScene(int i) const;
    /* Returns pin index if scenePos is within maxDist of a pin, else -1 */
    int pinAt(const QPointF &scenePos, double maxDist) const;

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    const EcPlugin *m_plugin;
    QString m_name;
    QVector<double> m_params;
    int m_rot = 0;
    uint64_t m_uid;
};
