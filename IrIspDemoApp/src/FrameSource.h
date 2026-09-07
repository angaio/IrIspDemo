/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : FrameSource.h
* Description : Abstractions over the two kinds of 16-bit infrared input the
*               demo understands.
*
*   RawFrameSource  a .raw file or a folder of numbered .raw frames. Each frame
*                   is width*height little/big-endian uint16 samples, optionally
*                   rotated 180 deg to undo a sensor "hardware flip".
*
*   UvcFrameSource  a 16-bit UVC camera: enumerate, open, then take each frame's
*                   raw 16-bit buffer and deliver it upstream. The transport is a
*                   Y16 UVC stream via OpenCV.
******************************************************************************/
#ifndef ___FrameSource_h___
#define ___FrameSource_h___

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <QString>
#include <QStringList>
#include <memory>
#include "IrRaw16Recorder.h"   // IrRaw16Reader (16-bit MKV playback)

class FrameSource
{
public:
    virtual ~FrameSource() {}
    virtual bool   isLive() const = 0;     // camera vs. file
    virtual int    frameCount() const = 0; // seekable frames; 0 for live
    virtual int    width() const = 0;
    virtual int    height() const = 0;
    virtual double fps() const = 0;
    // File: fill out16 with frame [index]. Live: grab the next frame.
    virtual bool   read(int index, cv::Mat &out16) = 0;
    virtual QString describe() const = 0;
};

// ---- raw file / sequence ----------------------------------------------------

struct RawFormat
{
    int  width = 320;
    int  height = 256;
    bool littleEndian = true;  // MATLAB/x86 default for the sample data
    bool flip180 = true;       // undo the sensor hardware flip
    double fps = 25.0;
};

class RawFrameSource : public FrameSource
{
public:
    // A single .raw (possibly holding several frames) - frameCount derives from
    // the file size. If width*height*2 does not divide the file, one frame is
    // read and the extra bytes ignored.
    static RawFrameSource *openFile(const QString &path, const RawFormat &fmt, QString *err);
    // A folder of numbered frames (1.raw, 2.raw, ... or any *.raw, sorted).
    static RawFrameSource *openFolder(const QString &dir, const RawFormat &fmt, QString *err);

    bool   isLive() const override { return false; }
    int    frameCount() const override { return m_count; }
    int    width() const override { return m_fmt.width; }
    int    height() const override { return m_fmt.height; }
    double fps() const override { return m_fmt.fps; }
    bool   read(int index, cv::Mat &out16) override;
    QString describe() const override { return m_desc; }

private:
    RawFormat   m_fmt;
    QStringList m_files;   // non-empty: folder/multi-file mode
    QString     m_single;  // non-empty: single-file mode
    int         m_count = 0;
    QString     m_desc;
};

// ---- 16-bit MKV (FFV1) playback --------------------------------------------

class MkvFrameSource : public FrameSource
{
public:
    static MkvFrameSource *open(const QString &path, QString *err);
    ~MkvFrameSource() override {}

    bool   isLive() const override { return false; }
    int    frameCount() const override { return m_count; }
    int    width() const override { return m_w; }
    int    height() const override { return m_h; }
    double fps() const override { return m_fps > 1 ? m_fps : 25.0; }
    bool   read(int index, cv::Mat &out16) override;
    QString describe() const override { return m_desc; }

private:
    std::shared_ptr<IrRaw16Reader> m_rdr;
    int    m_w = 0, m_h = 0, m_count = 0;
    double m_fps = 25.0;
    QString m_desc;
};

// ---- 16-bit UVC camera ------------------------------------------------------

class UvcFrameSource : public FrameSource
{
public:
    // Opens camera at deviceIndex and negotiates a 16-bit (Y16) stream.
    static UvcFrameSource *open(int deviceIndex, QString *err);
    ~UvcFrameSource() override;

    bool   isLive() const override { return true; }
    int    frameCount() const override { return 0; }
    int    width() const override { return m_w; }
    int    height() const override { return m_h; }
    double fps() const override { return m_fps; }
    bool   read(int index, cv::Mat &out16) override;
    QString describe() const override { return m_desc; }

private:
    cv::VideoCapture m_cap;
    int    m_w = 0, m_h = 0;
    double m_fps = 25.0;
    bool   m_raw16 = false;   // true: device delivers a true 16-bit (Y16) stream
    QString m_desc;
};

#endif
