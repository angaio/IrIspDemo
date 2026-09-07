/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "HistogramWidget.h"
#include <QPainter>
#include <QtMath>

HistogramWidget::HistogramWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(150);
    setMinimumWidth(240);
}

void HistogramWidget::setData(const QVector<int> &bins, int lo1, int hi99, int axisLo, int axisHi)
{
    m_bins = bins;
    m_lo1 = lo1;
    m_hi99 = hi99;
    m_axisLo = axisLo;
    m_axisHi = (axisHi > axisLo) ? axisHi : axisLo + 1;
    m_peak = 1;
    for (int v : m_bins) if (v > m_peak) m_peak = v;
    update();
}

void HistogramWidget::clear()
{
    m_bins.clear();
    update();
}

void HistogramWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QRect r = rect().adjusted(8, 8, -8, -24);
    p.fillRect(rect(), QColor(0x1a, 0x1e, 0x25));
    p.fillRect(r, QColor(0x12, 0x15, 0x1b));

    if (m_bins.size() < 256) {
        p.setPen(QColor(0x6b, 0x74, 0x82));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No image"));
        return;
    }

    // Log scale keeps small populations visible next to a dominant peak.
    const double logPeak = std::log(1.0 + m_peak);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x5a, 0x9c, 0xff));
    const double bw = double(r.width()) / 256.0;
    for (int i = 0; i < 256; ++i) {
        double h = std::log(1.0 + m_bins[i]) / logPeak * r.height();
        QRectF bar(r.left() + i * bw, r.bottom() - h, bw + 0.6, h);
        p.drawRect(bar);
    }

    // 1% / 99% guides. The x-axis spans the frame's actual [min,max], so map
    // gray values through that range (not the full 0..65535).
    const double span = double(m_axisHi - m_axisLo);
    auto xOf = [&](int gray) {
        double t = span > 0 ? (double(gray) - m_axisLo) / span : 0.0;
        if (t < 0) t = 0; else if (t > 1) t = 1;
        return r.left() + t * r.width();
    };
    auto guide = [&](int gray, const QString &tag, bool left) {
        double x = xOf(gray);
        p.setPen(QPen(QColor(0xff, 0xb1, 0x4a), 1, Qt::DashLine));
        p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        p.setPen(QColor(0xff, 0xc4, 0x76));
        QRectF t(x - (left ? 70 : -4), r.top() + 2, 68, 14);
        p.drawText(t, (left ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, tag);
    };
    guide(m_lo1, QStringLiteral("1%: %1").arg(m_lo1), false);
    guide(m_hi99, QStringLiteral("99%: %1").arg(m_hi99), true);

    // Axis-extent ticks (min / max of the data).
    p.setPen(QColor(0x6b, 0x74, 0x82));
    p.drawText(QRectF(r.left(), r.bottom() + 2, 80, 14), Qt::AlignLeft | Qt::AlignVCenter,
               QString::number(m_axisLo));
    p.drawText(QRectF(r.right() - 80, r.bottom() + 2, 80, 14), Qt::AlignRight | Qt::AlignVCenter,
               QString::number(m_axisHi));

    // Footer readout.
    p.setPen(QColor(0x9a, 0xa4, 0xb2));
    p.drawText(QRect(rect().left(), r.bottom() + 4, rect().width(), 20),
               Qt::AlignCenter,
               QStringLiteral("16-bit  |  range %1..%2    1% = %3    99% = %4")
                   .arg(m_axisLo).arg(m_axisHi).arg(m_lo1).arg(m_hi99));
}
