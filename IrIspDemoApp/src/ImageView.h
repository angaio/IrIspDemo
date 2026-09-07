/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#ifndef ___ImageView_h___
#define ___ImageView_h___

#include <QWidget>
#include <QImage>

// Aspect-correct viewer. Shows the original, the enhanced result, or both side
// by side. Painting is pure Qt; the images arrive already 8-bit.
class ImageView : public QWidget
{
    Q_OBJECT
public:
    enum Mode { Original, Enhanced, SideBySide };

    explicit ImageView(QWidget *parent = nullptr);

    void setMode(Mode m) { m_mode = m; update(); }
    Mode mode() const { return m_mode; }

public slots:
    void setImages(const QImage &original, const QImage &enhanced);
    void clear();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void drawPane(QPainter &p, const QRect &r, const QImage &img, const QString &tag);

    QImage m_original;
    QImage m_enhanced;
    Mode   m_mode = SideBySide;
};

#endif
