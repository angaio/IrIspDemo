/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : ProcessThread.h
* Description : Owns the frame source and the pipeline, and runs the
*               play / pause / resume / stop / seek state machine off the UI
*               thread. Emits the original and enhanced frame, the 16-bit
*               histogram, and drives recording.
******************************************************************************/
#ifndef ___ProcessThread_h___
#define ___ProcessThread_h___

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QImage>
#include <QVector>
#include <QString>
#include <memory>
#include "IrProcess.h"
#include "IrMp4Recorder.h"
#include "IrRaw16Recorder.h"
#include "PipelinePanel.h"     // NodeModel

class FrameSource;

class ProcessThread : public QThread
{
    Q_OBJECT
public:
    explicit ProcessThread(QObject *parent = nullptr);
    ~ProcessThread() override;

    void setSource(FrameSource *src, std::shared_ptr<IrPipeline> pipe);
    void setPipelineModel(const QVector<NodeModel> &model);
    void closeSource();     // release the current camera/file source

    void play();
    void pause();
    void stop();
    void seek(int index);
    void stepFrame(int delta);   // paused single-step (file sources only)
    void setLoop(bool on);

    // Two independent recorders (each backed by its own DLL). Paths are opened
    // lazily on the next frame, when the geometry is known.
    // viewMode selects what MP4 captures: 0=Original, 1=Processed, 2=Side by side.
    void startMp4(const QString &path, int viewMode);
    void stopMp4();
    bool isRecordingMp4();
    void startRaw16(const QString &path);
    void stopRaw16();
    bool isRecordingRaw16();

    void shutdown();

signals:
    void frameReady(QImage original, QImage enhanced, int index, int count, double ms);
    void histogramReady(QVector<int> bins, int lo1, int hi99, int axisLo, int axisHi);
    void reachedEnd();
    void started(int count, bool live);
    void sourceClosed();

protected:
    void run() override;

private:
    enum State { Stopped, Playing, Paused };

    void applyModelLocked();
    bool renderLocked(int idx, int count);

    QMutex          m_mx;
    QWaitCondition  m_cv;
    FrameSource    *m_src = nullptr;
    std::shared_ptr<IrPipeline> m_pipe;
    State  m_state = Stopped;
    int    m_index = 0;
    int    m_seek  = -1;
    bool   m_loop  = true;
    bool   m_abort = false;

    QVector<NodeModel> m_model;
    bool   m_modelDirty = false;

    std::shared_ptr<IrMp4Recorder>   m_mp4;
    std::shared_ptr<IrRaw16Recorder> m_raw16;
    bool    m_recMp4 = false;
    bool    m_recRaw16 = false;
    int     m_recMode = 1;      // 0=Original, 1=Processed, 2=Side by side
    QString m_mp4Path;
    QString m_raw16Path;
};

#endif
