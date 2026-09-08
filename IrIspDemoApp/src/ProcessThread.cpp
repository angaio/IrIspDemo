/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "ProcessThread.h"
#include "FrameSource.h"
#include <opencv2/core.hpp>

static QImage matToGray(const cv::Mat &m8)
{
    return QImage(m8.data, m8.cols, m8.rows, int(m8.step),
                  QImage::Format_Grayscale8).copy();
}

ProcessThread::ProcessThread(QObject *parent) : QThread(parent)
{
    qRegisterMetaType<QVector<int>>("QVector<int>");
}

ProcessThread::~ProcessThread() { shutdown(); }

void ProcessThread::setSource(FrameSource *src, std::shared_ptr<IrPipeline> pipe)
{
    QMutexLocker lk(&m_mx);
    delete m_src;
    m_src = src;
    m_pipe = std::move(pipe);
    m_modelDirty = true;             // push the current model into the new pipe
    m_index = 0;
    m_state = Paused;
    const int count = m_src ? m_src->frameCount() : 0;
    const bool live = m_src && m_src->isLive();
    emit started(count, live);
    m_seek = live ? -1 : 0;
    if (live) m_state = Playing;
    m_cv.wakeAll();
}

void ProcessThread::setPipelineModel(const QVector<NodeModel> &model)
{
    QMutexLocker lk(&m_mx);
    m_model = model;
    m_modelDirty = true;
    if (m_src && !m_src->isLive() && m_state != Playing) { m_seek = m_index; m_cv.wakeAll(); }
    else m_cv.wakeAll();
}

void ProcessThread::applyModelLocked()
{
    if (!m_pipe) return;
    while (m_pipe->nodeCount() > 0) m_pipe->removeNode(0);
    for (const NodeModel &n : m_model) {
        int idx = m_pipe->addNode(n.algoId);
        if (idx < 0) continue;
        for (auto it = n.params.constBegin(); it != n.params.constEnd(); ++it)
            m_pipe->setNodeParam(idx, it.key().toLatin1().constData(), it.value());
        m_pipe->setNodeEnabled(idx, n.enabled);
    }
    m_pipe->reset();
    m_modelDirty = false;
}

void ProcessThread::closeSource()
{
    QMutexLocker lk(&m_mx);
    m_state = Stopped;
    if (m_mp4)   { m_mp4->close();   m_mp4.reset(); }
    if (m_raw16) { m_raw16->close(); m_raw16.reset(); }
    m_recMp4 = m_recRaw16 = false;
    delete m_src; m_src = nullptr;
    m_pipe.reset();
    m_index = 0; m_seek = -1;
    m_cv.wakeAll();
    emit sourceClosed();
}

void ProcessThread::play()  { QMutexLocker lk(&m_mx); if (m_src) { m_state = Playing; m_cv.wakeAll(); } }
void ProcessThread::pause() { QMutexLocker lk(&m_mx); if (m_state == Playing) m_state = Paused; }
void ProcessThread::setLoop(bool on) { QMutexLocker lk(&m_mx); m_loop = on; }

void ProcessThread::stop()
{
    QMutexLocker lk(&m_mx);
    if (!m_src) return;
    m_state = Paused;
    m_index = 0;
    if (m_pipe) m_pipe->reset();
    if (!m_src->isLive()) { m_seek = 0; m_cv.wakeAll(); }
}

void ProcessThread::seek(int index)
{
    QMutexLocker lk(&m_mx);
    if (!m_src || m_src->isLive()) return;
    if (m_pipe) m_pipe->reset();
    m_seek = index;
    m_cv.wakeAll();
}

void ProcessThread::stepFrame(int delta)
{
    QMutexLocker lk(&m_mx);
    if (!m_src || m_src->isLive()) return;
    m_state = Paused;
    const int count = m_src->frameCount();
    int t = m_index + delta;
    if (t < 0) t = 0;
    if (count > 0 && t >= count) t = count - 1;
    if (m_pipe) m_pipe->reset();
    m_seek = t;
    m_cv.wakeAll();
}

void ProcessThread::startMp4(const QString &path, int viewMode)
{
    QMutexLocker lk(&m_mx);
    m_mp4 = IrMp4Factory::create();
    m_mp4Path = path;
    m_recMode = viewMode;            // fixed for the whole recording (geometry is stable)
    m_recMp4 = (m_mp4 != nullptr);   // opened lazily on the next frame
}

void ProcessThread::stopMp4()
{
    QMutexLocker lk(&m_mx);
    m_recMp4 = false;
    if (m_mp4) { m_mp4->close(); m_mp4.reset(); }
}

bool ProcessThread::isRecordingMp4() { QMutexLocker lk(&m_mx); return m_recMp4; }

void ProcessThread::startRaw16(const QString &path)
{
    QMutexLocker lk(&m_mx);
    m_raw16 = IrRaw16Factory::create();
    m_raw16Path = path;
    m_recRaw16 = (m_raw16 != nullptr);
}

void ProcessThread::stopRaw16()
{
    QMutexLocker lk(&m_mx);
    m_recRaw16 = false;
    if (m_raw16) { m_raw16->close(); m_raw16.reset(); }
}

bool ProcessThread::isRecordingRaw16() { QMutexLocker lk(&m_mx); return m_recRaw16; }

void ProcessThread::shutdown()
{
    { QMutexLocker lk(&m_mx); m_abort = true; m_cv.wakeAll(); }
    wait();
    { QMutexLocker lk(&m_mx);
      if (m_mp4)   { m_mp4->close();   m_mp4.reset(); }
      if (m_raw16) { m_raw16->close(); m_raw16.reset(); }
      delete m_src; m_src = nullptr; m_pipe.reset(); }
}

bool ProcessThread::renderLocked(int idx, int count)
{
    if (!m_src || !m_pipe) return false;
    if (m_modelDirty) applyModelLocked();

    cv::Mat m16;
    const bool live = m_src->isLive();
    if (!m_src->read(live ? 0 : idx, m16)) return false;

    cv::Mat orig8, enh8;
    IrCatalog::linearView(m16, orig8);
    m_pipe->process(m16, enh8);
    const double ms = m_pipe->lastProcessMs();

    // 16-bit histogram + 1%/99% gray values + axis bounds (data min/max).
    QVector<int> bins(256);
    int lo1 = 0, hi99 = 0, axisLo = 0, axisHi = 0;
    IrCatalog::histogram(m16, bins.data(), lo1, hi99, axisLo, axisHi);

    if (!live) m_index = idx; else ++m_index;
    const int shownIdx = m_index;

    // Recording (both opened lazily once the geometry is known):
    //   MP4   = the enhanced 8-bit display image
    //   Raw16 = the untouched 16-bit source frame, losslessly
    const int fps = int(m_src->fps() > 1 ? m_src->fps() : 25.0);
    if (m_recMp4 && m_mp4) {
        // Capture what the chosen view shows: Original, Processed, or a
        // side-by-side comparison (original | processed).
        cv::Mat rec;
        if (m_recMode == 0)      rec = orig8;
        else if (m_recMode == 2) { if (!orig8.empty() && !enh8.empty()) cv::hconcat(orig8, enh8, rec); }
        else                     rec = enh8;
        if (!rec.empty()) {
            if (!rec.isContinuous()) rec = rec.clone();
            if (!m_mp4->isOpen()) m_mp4->open(m_mp4Path.toUtf8().constData(), rec.cols, rec.rows, fps);
            if (m_mp4->isOpen()) m_mp4->writeFrame(rec.ptr<uint8_t>());
        }
    }
    if (m_recRaw16 && m_raw16 && !m16.empty()) {
        cv::Mat raw = m16.isContinuous() ? m16 : m16.clone();
        if (!m_raw16->isOpen()) m_raw16->open(m_raw16Path.toUtf8().constData(), raw.cols, raw.rows, fps);
        if (m_raw16->isOpen()) m_raw16->writeFrame(raw.ptr<uint16_t>());
    }

    emit frameReady(matToGray(orig8), matToGray(enh8), shownIdx, count, ms);
    emit histogramReady(bins, lo1, hi99, axisLo, axisHi);
    return true;
}

void ProcessThread::run()
{
    for (;;) {
        m_mx.lock();
        while (!m_abort && m_state != Playing && m_seek < 0)
            m_cv.wait(&m_mx);
        if (m_abort) { m_mx.unlock(); break; }

        const int count = m_src ? m_src->frameCount() : 0;
        const bool live = m_src && m_src->isLive();
        int idx;
        bool doSleep = false;
        double interval = 40.0;

        if (m_seek >= 0) { idx = m_seek; m_seek = -1; }
        else {
            if (live) { idx = -1; doSleep = true; }
            else {
                idx = m_index + 1;
                if (idx >= count) {
                    if (m_loop) idx = 0;
                    else { m_state = Paused; m_mx.unlock(); emit reachedEnd(); continue; }
                }
                doSleep = true;
            }
            if (m_src) interval = 1000.0 / (m_src->fps() > 1 ? m_src->fps() : 25.0);
        }

        renderLocked(idx, count);
        State st = m_state;
        m_mx.unlock();

        if (st == Playing && doSleep)
            msleep(static_cast<unsigned long>(interval));
    }
}
