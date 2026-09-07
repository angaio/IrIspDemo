/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "FrameSource.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCollator>
#include <vector>
#include <opencv2/imgproc.hpp>

// ---- helpers ----------------------------------------------------------------

static void bytesTo16u(const char *raw, cv::Mat &out16, const RawFormat &fmt)
{
    const int w = fmt.width, h = fmt.height, n = w * h;
    out16.create(h, w, CV_16UC1);
    uint16_t *d = out16.ptr<uint16_t>();
    const unsigned char *s = reinterpret_cast<const unsigned char *>(raw);
    if (fmt.littleEndian)
        for (int i = 0; i < n; ++i) d[i] = uint16_t(s[2 * i] | (s[2 * i + 1] << 8));
    else
        for (int i = 0; i < n; ++i) d[i] = uint16_t((s[2 * i] << 8) | s[2 * i + 1]);
    if (fmt.flip180) cv::flip(out16, out16, -1); // 180 deg = vertical + horizontal
}

// ---- RawFrameSource ---------------------------------------------------------

RawFrameSource *RawFrameSource::openFile(const QString &path, const RawFormat &fmt, QString *err)
{
    QFileInfo fi(path);
    const qint64 bytes = fi.size();
    const qint64 frameBytes = qint64(fmt.width) * fmt.height * 2;
    if (bytes < frameBytes) {
        if (err) *err = QStringLiteral("File is smaller than one %1x%2 16-bit frame.")
                            .arg(fmt.width).arg(fmt.height);
        return nullptr;
    }
    auto *s = new RawFrameSource;
    s->m_fmt = fmt;
    s->m_single = path;
    s->m_count = int(bytes / frameBytes);
    s->m_desc = QStringLiteral("%1  (%2 frame%3, %4x%5)")
                    .arg(fi.fileName()).arg(s->m_count).arg(s->m_count > 1 ? QStringLiteral("s") : QString())
                    .arg(fmt.width).arg(fmt.height);
    return s;
}

RawFrameSource *RawFrameSource::openFolder(const QString &dir, const RawFormat &fmt, QString *err)
{
    QDir d(dir);
    QStringList files = d.entryList(QStringList() << QStringLiteral("*.raw"), QDir::Files);
    if (files.isEmpty()) {
        if (err) *err = QStringLiteral("No .raw files found in the folder.");
        return nullptr;
    }
    // Natural sort so 2.raw precedes 10.raw.
    QCollator col; col.setNumericMode(true);
    std::sort(files.begin(), files.end(), [&](const QString &a, const QString &b) { return col.compare(a, b) < 0; });

    auto *s = new RawFrameSource;
    s->m_fmt = fmt;
    for (const QString &f : files) s->m_files << d.absoluteFilePath(f);
    s->m_count = s->m_files.size();
    s->m_desc = QStringLiteral("%1  (%2 frames, %3x%4)")
                    .arg(QFileInfo(dir).fileName()).arg(s->m_count).arg(fmt.width).arg(fmt.height);
    return s;
}

bool RawFrameSource::read(int index, cv::Mat &out16)
{
    if (index < 0 || index >= m_count) return false;
    const qint64 frameBytes = qint64(m_fmt.width) * m_fmt.height * 2;
    QByteArray buf;
    if (!m_files.isEmpty()) {
        QFile f(m_files.at(index));
        if (!f.open(QIODevice::ReadOnly)) return false;
        buf = f.read(frameBytes);
    } else {
        QFile f(m_single);
        if (!f.open(QIODevice::ReadOnly)) return false;
        if (!f.seek(qint64(index) * frameBytes)) return false;
        buf = f.read(frameBytes);
    }
    if (buf.size() < frameBytes) return false;
    bytesTo16u(buf.constData(), out16, m_fmt);
    return true;
}

// ---- MkvFrameSource (16-bit FFV1/MKV playback via IrRaw16Rec) ---------------

MkvFrameSource *MkvFrameSource::open(const QString &path, QString *err)
{
    auto rdr = IrRaw16ReaderFactory::create();
    if (!rdr || !rdr->open(path.toUtf8().constData())) {
        if (err) *err = rdr ? QString::fromLatin1(rdr->lastError())
                            : QStringLiteral("reader unavailable");
        return nullptr;
    }
    auto *s = new MkvFrameSource;
    s->m_rdr = rdr;
    s->m_w = rdr->width();
    s->m_h = rdr->height();
    s->m_count = rdr->frameCount();
    s->m_fps = rdr->fps();
    if (s->m_count < 1 || s->m_w < 1 || s->m_h < 1) {
        if (err) *err = QStringLiteral("empty or unsupported video.");
        delete s;
        return nullptr;
    }
    s->m_desc = QStringLiteral("%1  (%2 frames, %3x%4, 16-bit)")
                    .arg(QFileInfo(path).fileName()).arg(s->m_count).arg(s->m_w).arg(s->m_h);
    return s;
}

bool MkvFrameSource::read(int index, cv::Mat &out16)
{
    if (!m_rdr) return false;
    out16.create(m_h, m_w, CV_16UC1);
    return m_rdr->readFrame(index, out16.ptr<uint16_t>());
}

// ---- UvcFrameSource ---------------------------------------------------------
//
// Acquisition: enumerate/open the device, negotiate a stream, then pull each
// frame's buffer upstream. The transport differs by device, so open() probes
// what the camera actually delivers:
//
//   * a true 16-bit (Y16) camera  -> the raw radiometric frame is used as-is;
//   * a preview-only UVC module    -> the 8-bit preview is promoted to 16-bit.
//
// Note: MSMF cannot open some modules, so several backends are tried.

UvcFrameSource *UvcFrameSource::open(int deviceIndex, QString *err)
{
    struct Backend { int api; const char *name; };
    std::vector<Backend> backends = {
#ifdef _WIN32
        { cv::CAP_MSMF, "MSMF" }, { cv::CAP_DSHOW, "DirectShow" }, { cv::CAP_ANY, "default" }
#else
        { cv::CAP_V4L2, "V4L2" }, { cv::CAP_ANY, "default" }
#endif
    };
    // Prefer the requested index, then scan a few others.
    std::vector<int> indices = { deviceIndex, 0, 1, 2 };

    auto *s = new UvcFrameSource;
    for (const Backend &b : backends) {
        for (int idx : indices) {
            if (!s->m_cap.open(idx, b.api)) continue;

            // First try a raw 16-bit stream (harmless if the device ignores it).
            s->m_cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
            cv::Mat probe;
            if (s->m_cap.read(probe) && !probe.empty() &&
                probe.depth() == CV_16U && probe.channels() == 1) {
                s->m_raw16 = true;
            } else {
                // Fall back to a normal (8-bit) stream and take its luminance.
                s->m_cap.set(cv::CAP_PROP_CONVERT_RGB, 1);
                if (!s->m_cap.read(probe) || probe.empty()) { s->m_cap.release(); continue; }
                s->m_raw16 = false;
            }
            s->m_w   = probe.cols;
            s->m_h   = probe.rows;
            s->m_fps = s->m_cap.get(cv::CAP_PROP_FPS);
            if (s->m_fps <= 1.0 || s->m_fps > 240.0) s->m_fps = 25.0;
            s->m_desc = QStringLiteral("Camera #%1 via %2  (%3x%4%5)")
                            .arg(idx).arg(QString::fromLatin1(b.name)).arg(s->m_w).arg(s->m_h)
                            .arg(s->m_raw16 ? QStringLiteral(", 16-bit") : QStringLiteral(", preview"));
            return s;
        }
    }
    if (err) *err = QStringLiteral(
        "No usable camera found. The device may need its vendor SDK for a raw "
        "16-bit stream, or another application is holding it.");
    delete s;
    return nullptr;
}

UvcFrameSource::~UvcFrameSource()
{
    if (m_cap.isOpened()) m_cap.release();
}

bool UvcFrameSource::read(int /*index*/, cv::Mat &out16)
{
    cv::Mat raw;
    if (!m_cap.read(raw) || raw.empty()) return false;

    if (m_raw16 && raw.depth() == CV_16U && raw.channels() == 1) {
        out16 = raw.clone();
        return true;
    }
    // 8-bit preview path: reduce to one channel, then promote to the 16-bit
    // domain the engine expects (x256 spreads 8 bits across the working range).
    cv::Mat gray;
    if (raw.channels() == 3)      cv::cvtColor(raw, gray, cv::COLOR_BGR2GRAY);
    else if (raw.channels() == 2) cv::cvtColor(raw, gray, cv::COLOR_YUV2GRAY_YUY2);
    else                          gray = raw;
    if (gray.depth() != CV_8U) gray.convertTo(gray, CV_8U);
    gray.convertTo(out16, CV_16U, 256.0);
    return true;
}
