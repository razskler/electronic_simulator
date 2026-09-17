#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

struct PlotSeries
{
    QString label;
    QColor color;
    QVector<QPointF> pts;
};

/* Lightweight oscilloscope-style plotter: autoscale, nice ticks, zoom on
 * wheel, pan on drag, crosshair readout, optional log-x (Bode). */
class PlotPane : public QWidget
{
    Q_OBJECT

public:
    explicit PlotPane(QWidget *parent = nullptr);

    void setSeries(const QVector<PlotSeries> &series, const QString &xLabel,
                   const QString &yLabel, bool logX = false);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    struct Margins { int left = 68, right = 16, top = 30, bottom = 42; };

    QRectF plotArea() const;
    void autoscale();
    double mapX(double x) const;
    double unmapX(double px) const;
    double mapY(double y) const;
    void drawSeries(QPainter *p);
    void drawLegend(QPainter *p);

    QVector<PlotSeries> m_series;
    QString m_xLabel, m_yLabel;
    bool m_logX = false;
    bool m_autoscale = true;
    double m_xmin = 0, m_xmax = 1, m_ymin = -1, m_ymax = 1;
    bool m_dragging = false;
    QPointF m_dragStart;
    double m_dragXmin = 0, m_dragXmax = 1;
    QPointF m_hover;
    bool m_hoverValid = false;
    Margins m_m;
};
