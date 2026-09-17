#include "plotpane.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

PlotPane::PlotPane(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(300, 200);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
}

void PlotPane::setSeries(const QVector<PlotSeries> &series, const QString &xLabel,
                         const QString &yLabel, bool logX)
{
    m_series = series;
    m_xLabel = xLabel;
    m_yLabel = yLabel;
    m_logX = logX;
    m_autoscale = true;
    update();
}

QRectF PlotPane::plotArea() const
{
    return QRectF(m_m.left, m_m.top, width() - m_m.left - m_m.right,
                  height() - m_m.top - m_m.bottom);
}

void PlotPane::autoscale()
{
    m_xmin = qQNaN();
    m_xmax = qQNaN();
    m_ymin = qQNaN();
    m_ymax = qQNaN();
    for (const PlotSeries &s : qAsConst(m_series)) {
        for (const QPointF &pt : s.pts) {
            double x = m_logX ? (pt.x() > 0 ? qLn(pt.x()) : -1e30) : pt.x();
            if (qIsNaN(m_xmin) || x < m_xmin) m_xmin = x;
            if (qIsNaN(m_xmax) || x > m_xmax) m_xmax = x;
            if (qIsNaN(m_ymin) || pt.y() < m_ymin) m_ymin = pt.y();
            if (qIsNaN(m_ymax) || pt.y() > m_ymax) m_ymax = pt.y();
        }
    }
    if (qIsNaN(m_xmin)) { m_xmin = 0; m_xmax = 1; m_ymin = -1; m_ymax = 1; }
    if (m_xmax - m_xmin < 1e-12) { m_xmax = m_xmin + 1; }
    double pad = 0.05 * (m_ymax - m_ymin);
    if (pad < 1e-9) pad = 0.5;
    m_ymin -= pad;
    m_ymax += pad;
}

double PlotPane::mapX(double x) const
{
    double v = m_logX ? (x > 0 ? qLn(x) : m_xmin) : x;
    double f = plotArea().left() + (v - m_xmin) / (m_xmax - m_xmin) * plotArea().width();
    return f;
}

double PlotPane::unmapX(double px) const
{
    double v = m_xmin + (px - plotArea().left()) / plotArea().width() * (m_xmax - m_xmin);
    return m_logX ? qExp(v) : v;
}

double PlotPane::mapY(double y) const
{
    return plotArea().bottom() - (y - m_ymin) / (m_ymax - m_ymin) * plotArea().height();
}

static double niceStep(double range, int target)
{
    double raw = qAbs(range) / qMax(1, target);
    double mag = qPow(10.0, qFloor(qLn(raw) / qLn(10.0)));
    double norm = raw / mag;
    double nice;
    if (norm < 1.5) nice = 1;
    else if (norm < 3.5) nice = 2;
    else if (norm < 7.5) nice = 5;
    else nice = 10;
    return nice * mag;
}

static QString fmtNum(double v)
{
    if (qAbs(v) >= 1e6 || (qAbs(v) < 1e-3 && v != 0))
        return QString::number(v, 'e', 1);
    return QString::number(v, 'g', 4);
}

void PlotPane::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_autoscale)
        autoscale();

    QRectF pa = plotArea();

    /* frame + grid */
    p.setPen(QPen(palette().color(QPalette::Mid), 1));
    p.setBrush(Qt::white);
    p.drawRect(pa);

    double xstep = niceStep(m_xmax - m_xmin, 8);
    p.setPen(QPen(QColor(0, 0, 0, 25)));
    for (double t = qCeil(m_xmin / xstep) * xstep; t <= m_xmax; t += xstep) {
        double px = pa.left() + (t - m_xmin) / (m_xmax - m_xmin) * pa.width();
        p.drawLine(QPointF(px, pa.top()), QPointF(px, pa.bottom()));
    }
    double ystep = niceStep(m_ymax - m_ymin, 6);
    for (double t = qCeil(m_ymin / ystep) * ystep; t <= m_ymax; t += ystep) {
        double py = pa.bottom() - (t - m_ymin) / (m_ymax - m_ymin) * pa.height();
        p.drawLine(QPointF(pa.left(), py), QPointF(pa.right(), py));
    }

    /* axes labels */
    p.setPen(palette().color(QPalette::Text));
    QFont f = p.font();
    f.setPointSizeF(8.0);
    p.setFont(f);
    for (double t = qCeil(m_xmin / xstep) * xstep; t <= m_xmax; t += xstep) {
        double px = pa.left() + (t - m_xmin) / (m_xmax - m_xmin) * pa.width();
        QString label = m_logX ? fmtNum(qPow(10.0, t)) : fmtNum(t);
        p.drawText(QRectF(px - 40, pa.bottom() + 4, 80, 14),
                   Qt::AlignHCenter | Qt::AlignTop, label);
    }
    for (double t = qCeil(m_ymin / ystep) * ystep; t <= m_ymax; t += ystep) {
        double py = pa.bottom() - (t - m_ymin) / (m_ymax - m_ymin) * pa.height();
        p.drawText(QRectF(0, py - 7, m_m.left - 8, 14),
                   Qt::AlignRight | Qt::AlignVCenter, fmtNum(t));
    }
    p.drawText(QRectF(pa.left(), height() - 16, pa.width(), 14),
               Qt::AlignHCenter, m_xLabel);
    p.save();
    p.translate(14, pa.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-80, -8, 160, 14), Qt::AlignHCenter, m_yLabel);
    p.restore();

    /* series */
    for (const PlotSeries &s : qAsConst(m_series)) {
        QPainterPath path;
        bool started = false;
        for (const QPointF &pt : s.pts) {
            double px = mapX(pt.x());
            double py = mapY(pt.y());
            if (!started) {
                path.moveTo(px, py);
                started = true;
            } else {
                path.lineTo(px, py);
            }
        }
        p.setPen(QPen(s.color, 2.0));
        p.drawPath(path);
    }

    drawLegend(&p);

    /* crosshair readout */
    if (m_hoverValid) {
        p.setPen(QPen(QColor(0, 0, 0, 60), 1, Qt::DashLine));
        p.drawLine(QPointF(m_hover.x(), pa.top()), QPointF(m_hover.x(), pa.bottom()));
        double xv = unmapX(m_hover.x());
        QString txt = fmtNum(xv);
        for (const PlotSeries &s : qAsConst(m_series)) {
            if (s.pts.isEmpty())
                continue;
            /* nearest sample */
            int best = 0;
            double bd = qInf();
            for (int i = 0; i < s.pts.size(); i++) {
                double d = qAbs(mapX(s.pts[i].x()) - m_hover.x());
                if (d < bd) { bd = d; best = i; }
            }
            txt += QString::fromUtf8("   %1=%2").arg(s.label).arg(fmtNum(s.pts[best].y()));
        }
        p.setPen(QPen(palette().color(QPalette::Text)));
        QRectF box(pa.left() + 6, pa.top() + 4, pa.width() - 12, 16);
        p.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, txt);
    }
}

void PlotPane::drawLegend(QPainter *p)
{
    if (m_series.isEmpty())
        return;
    QFont f = p->font();
    f.setPointSizeF(8.5);
    p->setFont(f);
    QFontMetrics fm(f);
    int w = 0;
    for (const PlotSeries &s : qAsConst(m_series))
        w = qMax(w, fm.horizontalAdvance(s.label));
    int rowH = fm.height() + 2;
    QRectF box(m_m.left + 8, m_m.top - rowH, w + 34, rowH * m_series.size() + 6);
    p->setPen(QPen(palette().color(QPalette::Mid)));
    p->setBrush(QColor(255, 255, 255, 220));
    p->drawRect(box);
    int y = box.top() + 3;
    for (const PlotSeries &s : qAsConst(m_series)) {
        p->setPen(QPen(s.color, 3));
        p->drawLine(QPointF(box.left() + 6, y + rowH / 2), QPointF(box.left() + 22, y + rowH / 2));
        p->setPen(QPen(palette().color(QPalette::Text)));
        p->drawText(QRectF(box.left() + 26, y, box.width() - 28, rowH),
                    Qt::AlignLeft | Qt::AlignVCenter, s.label);
        y += rowH;
    }
}

void PlotPane::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QPointF pos = event->position();
#else
    QPointF pos = QPointF(event->pos());
#endif
    double f = event->angleDelta().y() > 0 ? 0.8 : 1.25;
    double xc = m_xmin + (pos.x() - plotArea().left()) / plotArea().width() * (m_xmax - m_xmin);
    m_xmin = xc - (xc - m_xmin) * f;
    m_xmax = xc + (m_xmax - xc) * f;
    m_autoscale = false;
    update();
    event->accept();
}

void PlotPane::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && plotArea().contains(event->pos())) {
        m_dragging = true;
        m_dragStart = event->pos();
        m_dragXmin = m_xmin;
        m_dragXmax = m_xmax;
        event->accept();
    }
}

void PlotPane::mouseMoveEvent(QMouseEvent *event)
{
    m_hover = event->pos();
    m_hoverValid = plotArea().contains(event->pos());
    if (m_dragging) {
        double dx = -(event->pos().x() - m_dragStart.x()) / plotArea().width() * (m_dragXmax - m_dragXmin);
        m_xmin = m_dragXmin + dx;
        m_xmax = m_dragXmax + dx;
        m_autoscale = false;
    }
    update();
}

void PlotPane::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
}

void PlotPane::mouseDoubleClickEvent(QMouseEvent *event)
{
    m_autoscale = true;
    update();
    event->accept();
}
