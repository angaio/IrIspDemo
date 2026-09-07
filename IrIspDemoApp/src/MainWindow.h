/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#ifndef ___MainWindow_h___
#define ___MainWindow_h___

#include <QMainWindow>
#include <QImage>
#include <QVector>
#include <memory>
#include "FrameSource.h"          // RawFormat

class ImageView;
class ProcessThread;
class PipelinePanel;
class HistogramWidget;
class QSlider;
class QToolButton;
class QLabel;
class QComboBox;
class QAction;
class QMenu;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onOpenFile();
    void onConnectCamera();
    void onCloseVideo();
    void onAbout();

    void onPlayPause();
    void onStop();
    void onSliderPressed();
    void onSliderMoved(int v);
    void onSliderReleased();

    void onViewModeChanged(int);
    void onPipelineChanged();

    void onSnapshot();
    void onToggleRecord(bool on);       // MP4 (8-bit enhanced)
    void onToggleRecordRaw(bool on);    // 16-bit raw (FFV1/MKV)
    void onToggleFullScreen(bool on);

    void onStarted(int count, bool live);
    void onFrameReady(QImage original, QImage enhanced, int index, int count, double ms);
    void onHistogram(QVector<int> bins, int lo1, int hi99, int axisLo, int axisHi);
    void onReachedEnd();
    void onSourceClosed();

private:
    void buildUi();
    void buildMenus();
    void buildDocks();
    void applyTheme();
    void startSource(FrameSource *src);
    void setPlaying(bool playing);

    // Recent-video support (persisted via QSettings).
    void openVideoPath(const QString &path);
    void addRecentFile(const QString &path);
    void updateRecentFilesMenu();

    ImageView       *m_view = nullptr;
    ProcessThread   *m_thread = nullptr;
    PipelinePanel   *m_pipePanel = nullptr;
    HistogramWidget *m_hist = nullptr;

    QToolButton *m_playBtn = nullptr;
    QToolButton *m_stopBtn = nullptr;
    QSlider     *m_slider = nullptr;
    QLabel      *m_timeLabel = nullptr;
    QComboBox   *m_viewMode = nullptr;
    QAction     *m_recAct = nullptr;     // MP4
    QAction     *m_recRawAct = nullptr;  // 16-bit raw
    QAction     *m_fsAct = nullptr;
    QMenu       *m_recentMenu = nullptr; // File > Open Recent

    QLabel *m_srcLabel = nullptr;
    QLabel *m_statLabel = nullptr;

    QImage m_lastEnh, m_lastOrig;
    bool m_playing = false;
    bool m_live = false;
    bool m_scrubbing = false;
    bool m_wasPlaying = false;
    int  m_count = 0;
    RawFormat m_lastFmt;
};

#endif
