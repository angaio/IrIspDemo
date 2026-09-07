/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#ifndef ___HistogramWidget_h___
#define ___HistogramWidget_h___

#include <QWidget>
#include <QVector>

// Draws the 16-bit grayscale histogram (256 log-scaled bins over 0..65535) and
// marks the 1st- and 99th-percentile gray values with labelled guides.
class HistogramWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HistogramWidget(QWidget *parent = nullptr);

public slots:
    void setData(const QVector<int> &bins, int lo1, int hi99, int axisLo, int axisHi);
    void clear();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QVector<int> m_bins;   // 256 counts
    int m_lo1 = 0;
    int m_hi99 = 0;
    int m_axisLo = 0;      // gray value at bins[0]
    int m_axisHi = 1;      // gray value at bins[255]
    int m_peak = 1;
};

#endif
