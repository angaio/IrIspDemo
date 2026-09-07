/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "ImageView.h"
#include <QPainter>

ImageView::ImageView(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(480, 320);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void ImageView::setImages(const QImage &original, const QImage &enhanced)
{
    m_original = original;
    m_enhanced = enhanced;
    update();
}

void ImageView::clear()
{
    m_original = QImage();
    m_enhanced = QImage();
    update();
}

void ImageView::drawPane(QPainter &p, const QRect &r, const QImage &img, const QString &tag)
{
    p.fillRect(r, QColor(0x10, 0x12, 0x16));
    if (!img.isNull()) {
        QSize sz = img.size();
        sz.scale(r.size(), Qt::KeepAspectRatio);
        QRect target(QPoint(0, 0), sz);
        target.moveCenter(r.center());
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(target, img);
    }
    // Corner tag.
    p.setPen(QColor(0xff, 0xff, 0xff, 200));
    QFont f = p.font(); f.setPointSize(9); f.setBold(true); p.setFont(f);
    QRect tagRect(r.left() + 10, r.top() + 8, r.width() - 20, 20);
    p.fillRect(QRect(tagRect.left() - 4, tagRect.top() - 2, p.fontMetrics().horizontalAdvance(tag) + 12, 20),
               QColor(0, 0, 0, 110));
    p.drawText(tagRect, Qt::AlignLeft | Qt::AlignVCenter, tag);
}

void ImageView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QRect full = rect();
    p.fillRect(full, QColor(0x0c, 0x0e, 0x12));

    if (m_mode == Original) {
        drawPane(p, full, m_original, QStringLiteral("Original"));
    } else if (m_mode == Enhanced) {
        drawPane(p, full, m_enhanced, QStringLiteral("Processed"));
    } else {
        const int gap = 2;
        const int w = (full.width() - gap) / 2;
        QRect left(full.left(), full.top(), w, full.height());
        QRect right(full.left() + w + gap, full.top(), full.width() - w - gap, full.height());
        drawPane(p, left, m_original, QStringLiteral("Original"));
        drawPane(p, right, m_enhanced, QStringLiteral("Processed"));
    }
}
