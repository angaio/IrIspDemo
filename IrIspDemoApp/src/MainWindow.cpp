/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "MainWindow.h"
#include "ImageView.h"
#include "ProcessThread.h"
#include "PipelinePanel.h"
#include "HistogramWidget.h"
#include "FrameSource.h"
#include "IrProcess.h"
#ifdef HAVE_HIK_SDK
#  include "HikFrameSource.h"
#endif

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QToolButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QStyle>
#include <QSignalBlocker>
#include <QDateTime>
#include <QDir>
#include <QPixmap>
#include <QPainter>
#include <QIcon>
#include <QProgressDialog>
#include <QThread>
#include <QSharedPointer>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QStyle>
#include <QShortcut>
#include <QCloseEvent>
#include <QFileInfo>
#include <QMenu>

namespace {
// Hand-drawn toolbar icons, so each button reads as its function on the dark
// toolbar (and stays legible on the blue "checked" background). Light stroke
// with a record-red accent; painted as vectors, no image assets needed.
QIcon makeToolIcon(const QString &kind)
{
    const int S = 48;
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor ink(0xd8, 0xde, 0xe6);
    const QColor rec(0xe5, 0x48, 0x4d);
    QPen pen(ink, 3.0);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    if (kind == QLatin1String("snapshot")) {          // camera
        p.drawRoundedRect(QRectF(8, 18, 32, 21), 4, 4);
        p.drawRoundedRect(QRectF(19, 13, 10, 6), 2, 2);           // viewfinder bump
        p.drawEllipse(QPointF(24, 29.5), 7, 7);                   // lens
        p.setPen(Qt::NoPen); p.setBrush(ink);
        p.drawEllipse(QPointF(24, 29.5), 2.4, 2.4);
    } else if (kind == QLatin1String("mp4")) {        // filmstrip + record dot
        p.drawRoundedRect(QRectF(8, 15, 32, 18), 3, 3);
        p.setPen(Qt::NoPen); p.setBrush(ink);
        for (double x = 11; x <= 35; x += 6) {
            p.drawRoundedRect(QRectF(x, 16.5, 3, 2), 0.8, 0.8);
            p.drawRoundedRect(QRectF(x, 29.5, 3, 2), 0.8, 0.8);
        }
        p.setBrush(rec);
        p.drawEllipse(QPointF(24, 24), 4.6, 4.6);
    } else if (kind == QLatin1String("raw16")) {      // record ring + pixel grid
        p.setPen(QPen(rec, 3.0)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(18, 24), 8, 8);
        p.setPen(Qt::NoPen); p.setBrush(rec);
        p.drawEllipse(QPointF(18, 24), 3.1, 3.1);
        p.setBrush(ink);
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j)
                p.drawRoundedRect(QRectF(31 + i * 6.5, 20 + j * 6.5, 5, 5), 1, 1);
    } else if (kind == QLatin1String("fullscreen")) { // four outward corners
        auto corner = [&](double x1, double y1, double x2, double y2, double x3, double y3) {
            p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
            p.drawLine(QPointF(x2, y2), QPointF(x3, y3));
        };
        corner(11, 20, 11, 11, 20, 11);   // top-left
        corner(37, 20, 37, 11, 28, 11);   // top-right
        corner(11, 28, 11, 37, 20, 37);   // bottom-left
        corner(37, 28, 37, 37, 28, 37);   // bottom-right
    } else if (kind == QLatin1String("play")) {       // green triangle
        p.setPen(Qt::NoPen); p.setBrush(QColor(0x3f, 0xb9, 0x50));
        const QPointF tri[3] = { QPointF(18, 13), QPointF(36, 24), QPointF(18, 35) };
        p.drawPolygon(tri, 3);
    } else if (kind == QLatin1String("pause")) {      // two light bars
        p.setPen(Qt::NoPen); p.setBrush(ink);
        p.drawRoundedRect(QRectF(17, 13, 6, 22), 1.5, 1.5);
        p.drawRoundedRect(QRectF(25, 13, 6, 22), 1.5, 1.5);
    } else if (kind == QLatin1String("stop")) {       // soft-red square
        p.setPen(Qt::NoPen); p.setBrush(QColor(0xd8, 0x5a, 0x5a));
        p.drawRoundedRect(QRectF(15, 15, 18, 18), 2, 2);
    }
    p.end();
    return QIcon(pm);
}
} // namespace

static QString pipelineCfgPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) dir = QDir::homePath();
    return dir + QStringLiteral("/pipeline.json");
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("WAVEFRONT  \xC2\xB7  IR Image Processing Studio"));
    resize(1280, 800);

    m_thread = new ProcessThread(this);
    connect(m_thread, &ProcessThread::started,        this, &MainWindow::onStarted);
    connect(m_thread, &ProcessThread::frameReady,      this, &MainWindow::onFrameReady);
    connect(m_thread, &ProcessThread::histogramReady,  this, &MainWindow::onHistogram);
    connect(m_thread, &ProcessThread::reachedEnd,      this, &MainWindow::onReachedEnd);
    connect(m_thread, &ProcessThread::sourceClosed,    this, &MainWindow::onSourceClosed);
    m_thread->start();

    buildUi();
    buildMenus();
    buildDocks();
    applyTheme();
    setPlaying(false);

    if (!m_pipePanel->loadModel(pipelineCfgPath()))
        m_pipePanel->seedDefault();     // emits onPipelineChanged
    statusBar()->showMessage(QStringLiteral("Open a raw file or connect a camera (File menu) to begin."));
}

MainWindow::~MainWindow() { m_thread->shutdown(); }

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (m_pipePanel) m_pipePanel->saveModel(pipelineCfgPath());
    QMainWindow::closeEvent(e);
}

// ---- UI ---------------------------------------------------------------------

void MainWindow::buildUi()
{
    QToolBar *bar = addToolBar(QStringLiteral("Main"));
    bar->setObjectName(QStringLiteral("mainbar"));
    bar->setMovable(false);
    bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    bar->setIconSize(QSize(20, 20));

    QAction *aSnap = bar->addAction(makeToolIcon(QStringLiteral("snapshot")),
                                    QStringLiteral("Snapshot"));
    connect(aSnap, &QAction::triggered, this, &MainWindow::onSnapshot);

    m_recAct = bar->addAction(makeToolIcon(QStringLiteral("mp4")),
                              QStringLiteral("Record MP4"));
    m_recAct->setCheckable(true);
    m_recAct->setToolTip(QStringLiteral("Record the current view to MP4 (H.264)"));
    connect(m_recAct, &QAction::toggled, this, &MainWindow::onToggleRecord);

    m_recRawAct = bar->addAction(makeToolIcon(QStringLiteral("raw16")),
                                 QStringLiteral("Record RAW"));
    m_recRawAct->setCheckable(true);
    m_recRawAct->setToolTip(QStringLiteral("Record the untouched 16-bit stream losslessly (FFV1/MKV)"));
    connect(m_recRawAct, &QAction::toggled, this, &MainWindow::onToggleRecordRaw);

    m_fsAct = bar->addAction(makeToolIcon(QStringLiteral("fullscreen")),
                             QStringLiteral("Full Screen"));
    m_fsAct->setCheckable(true);
    m_fsAct->setShortcut(Qt::Key_F11);
    connect(m_fsAct, &QAction::toggled, this, &MainWindow::onToggleFullScreen);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(spacer);
    bar->addWidget(new QLabel(QStringLiteral("View  "), this));
    m_viewMode = new QComboBox(this);
    m_viewMode->addItems(QStringList() << QStringLiteral("Original")
                         << QStringLiteral("Processed") << QStringLiteral("Side by side"));
    m_viewMode->setCurrentIndex(2);
    connect(m_viewMode, SIGNAL(currentIndexChanged(int)), this, SLOT(onViewModeChanged(int)));
    bar->addWidget(m_viewMode);

    // Central: image + transport.
    m_view = new ImageView(this);
    m_view->setMode(ImageView::SideBySide);

    QFrame *transport = new QFrame(this);
    transport->setObjectName(QStringLiteral("transportbar"));
    QHBoxLayout *tl = new QHBoxLayout(transport);
    tl->setContentsMargins(10, 6, 10, 6);

    m_playBtn = new QToolButton(this);
    m_playBtn->setObjectName(QStringLiteral("transport"));
    m_playBtn->setIconSize(QSize(22, 22));
    m_playBtn->setIcon(makeToolIcon(QStringLiteral("play")));
    m_stopBtn = new QToolButton(this);
    m_stopBtn->setObjectName(QStringLiteral("transport"));
    m_stopBtn->setIconSize(QSize(22, 22));
    m_stopBtn->setIcon(makeToolIcon(QStringLiteral("stop")));
    connect(m_playBtn, &QToolButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_stopBtn, &QToolButton::clicked, this, &MainWindow::onStop);

    auto mkStep = [this](QStyle::StandardPixmap ic, const QString &tip) {
        QToolButton *b = new QToolButton(this);
        b->setObjectName(QStringLiteral("transport"));
        b->setIconSize(QSize(22, 22));
        b->setIcon(style()->standardIcon(ic));
        b->setToolTip(tip);
        b->setEnabled(false);
        return b;
    };
    m_prevBtn = mkStep(QStyle::SP_MediaSkipBackward, QStringLiteral("Previous frame (,)"));
    m_nextBtn = mkStep(QStyle::SP_MediaSkipForward,  QStringLiteral("Next frame (.)"));
    connect(m_prevBtn, &QToolButton::clicked, this, [this]{ if (!m_live) { m_thread->stepFrame(-1); setPlaying(false); } });
    connect(m_nextBtn, &QToolButton::clicked, this, [this]{ if (!m_live) { m_thread->stepFrame(+1); setPlaying(false); } });
    { auto *sp = new QShortcut(QKeySequence(Qt::Key_Comma),  this); connect(sp, &QShortcut::activated, this, [this]{ if (!m_live) { m_thread->stepFrame(-1); setPlaying(false); } });
      auto *sn = new QShortcut(QKeySequence(Qt::Key_Period), this); connect(sn, &QShortcut::activated, this, [this]{ if (!m_live) { m_thread->stepFrame(+1); setPlaying(false); } }); }

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 0);
    connect(m_slider, &QSlider::sliderPressed,  this, &MainWindow::onSliderPressed);
    connect(m_slider, &QSlider::sliderMoved,    this, &MainWindow::onSliderMoved);
    connect(m_slider, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);

    m_timeLabel = new QLabel(QStringLiteral("-- / --"), this);
    m_timeLabel->setObjectName(QStringLiteral("time"));

    tl->addWidget(m_prevBtn);
    tl->addWidget(m_playBtn);
    tl->addWidget(m_nextBtn);
    tl->addWidget(m_stopBtn);
    tl->addWidget(m_slider, 1);
    tl->addWidget(m_timeLabel);

    QWidget *root = new QWidget(this);
    QVBoxLayout *rl = new QVBoxLayout(root);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);
    rl->addWidget(m_view, 1);
    rl->addWidget(transport, 0);
    setCentralWidget(root);

    m_srcLabel = new QLabel(QStringLiteral("No source"), this);
    m_statLabel = new QLabel(QString(), this);
    statusBar()->addWidget(m_srcLabel, 1);
    statusBar()->addPermanentWidget(m_statLabel, 0);
}

void MainWindow::buildMenus()
{
    QMenu *file = menuBar()->addMenu(QStringLiteral("&File"));
    file->addAction(QStringLiteral("Open &Video..."), this, &MainWindow::onOpenFile);
    m_recentMenu = file->addMenu(QStringLiteral("Open &Recent"));
    updateRecentFilesMenu();
    file->addAction(QStringLiteral("Connect &Camera"),   this, &MainWindow::onConnectCamera);
    file->addAction(QStringLiteral("Cl&ose Video"),      this, &MainWindow::onCloseVideo);
    file->addSeparator();
    file->addAction(QStringLiteral("E&xit"), this, &QWidget::close);

    QMenu *view = menuBar()->addMenu(QStringLiteral("&View"));
    view->addAction(QStringLiteral("Original"),     [this] { m_viewMode->setCurrentIndex(0); });
    view->addAction(QStringLiteral("Processed"),    [this] { m_viewMode->setCurrentIndex(1); });
    view->addAction(QStringLiteral("Side by side"), [this] { m_viewMode->setCurrentIndex(2); });

    QMenu *help = menuBar()->addMenu(QStringLiteral("&Help"));
    help->addAction(QStringLiteral("&About"), this, &MainWindow::onAbout);
}

void MainWindow::buildDocks()
{
    // Pipeline editor (right).
    QDockWidget *pipeDock = new QDockWidget(QStringLiteral("Pipeline"), this);
    pipeDock->setObjectName(QStringLiteral("pipeDock"));
    m_pipePanel = new PipelinePanel(pipeDock);
    connect(m_pipePanel, &PipelinePanel::modelChanged, this, &MainWindow::onPipelineChanged);
    pipeDock->setWidget(m_pipePanel);
    pipeDock->setMinimumWidth(260);
    addDockWidget(Qt::RightDockWidgetArea, pipeDock);

    // 16-bit histogram (floating panel).
    QDockWidget *histDock = new QDockWidget(QStringLiteral("16-bit Histogram"), this);
    histDock->setObjectName(QStringLiteral("histDock"));
    m_hist = new HistogramWidget(histDock);
    histDock->setWidget(m_hist);
    histDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                          QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::BottomDockWidgetArea, histDock);
    histDock->setFloating(true);
    histDock->resize(360, 200);
}

void MainWindow::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget      { background: #171a1f; color: #d5dbe3; }
        QMenuBar                  { background: #1e222a; color: #c8d0da; border-bottom: 1px solid #2c333d; }
        QMenuBar::item            { background: transparent; padding: 5px 12px; }
        QMenuBar::item:selected   { background: #2b313a; }
        QMenu                     { background: #232830; color: #c8d0da; border: 1px solid #333b46; }
        QMenu::item:selected      { background: #2f6df6; color: #ffffff; }
        QToolBar#mainbar          { background: #1e222a; border: 0; border-bottom: 1px solid #2c333d; padding: 5px; spacing: 4px; }
        QToolBar QToolButton      { color: #d5dbe3; padding: 5px 12px; border-radius: 4px; }
        QToolBar QToolButton:hover    { background: #2b313a; }
        QToolBar QToolButton:checked  { background: #2f6df6; color: #ffffff; }
        QDockWidget               { color: #98a3b1; }
        QDockWidget::title        { background: #1e222a; padding: 6px 10px; border-bottom: 1px solid #2c333d; }
        QListWidget               { background: #12151b; border: 1px solid #2c333d; border-radius: 4px; }
        QListWidget::item         { padding: 5px 4px; }
        QListWidget::item:selected { background: #2f6df6; color: #ffffff; }
        QGroupBox                 { border: 1px solid #2c333d; border-radius: 6px; margin-top: 14px; padding: 8px; }
        QGroupBox::title          { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #8b95a3; }
        QLabel#panelTitle         { color: #e8edf4; font-weight: bold; padding: 2px; }
        QLabel                    { color: #98a3b1; }
        QComboBox, QSpinBox       { background: #252a33; color: #d5dbe3; border: 1px solid #333b46; border-radius: 4px; padding: 3px 6px; }
        QComboBox QAbstractItemView { background: #252a33; color: #d5dbe3; selection-background-color: #2f6df6; selection-color: #ffffff; }
        QCheckBox                 { color: #b7c0cc; }
        QFrame#transportbar       { background: #1e222a; border-top: 1px solid #2c333d; }
        QToolButton#transport     { background: #313a48; border: 1px solid #3d4756; border-radius: 5px; padding: 6px 14px; }
        QToolButton#transport:hover     { background: #3b4655; }
        QToolButton#transport:disabled  { background: #232830; border-color: #2c333d; }
        QLabel#time               { color: #8b95a3; min-width: 84px; }
        QSlider::groove:horizontal { height: 4px; background: #2c333d; border-radius: 2px; }
        QSlider::handle:horizontal { width: 14px; margin: -6px 0; border-radius: 7px; background: #2f6df6; }
        QSlider::sub-page:horizontal { background: #2f6df6; border-radius: 2px; }
        QStatusBar                { background: #1e222a; color: #8b95a3; border-top: 1px solid #2c333d; }
    )"));
}

// ---- source opening ---------------------------------------------------------

void MainWindow::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open a 16-bit video"),
                    QString(), QStringLiteral("16-bit video (*.mkv);;All files (*.*)"));
    if (path.isEmpty()) return;
    openVideoPath(path);
}

// Shared open path used by both the file dialog and the "Open Recent" entries.
// On success the file is promoted to the top of the recent list; on failure it
// is dropped from the list so stale/moved files don't linger in the menu.
void MainWindow::openVideoPath(const QString &path)
{
    if (path.isEmpty()) return;
    QString err;
    FrameSource *src = MkvFrameSource::open(path, &err);
    if (!src) {
        QMessageBox::warning(this, QStringLiteral("Open Video"),
            err.isEmpty() ? QStringLiteral("Cannot open this video.") : err);
        // Remove a file we could not open (e.g. deleted or moved) from history.
        QSettings s;
        QStringList files = s.value(QStringLiteral("recentFiles")).toStringList();
        if (files.removeAll(QDir::toNativeSeparators(path)) > 0 ||
            files.removeAll(path) > 0) {
            s.setValue(QStringLiteral("recentFiles"), files);
            updateRecentFilesMenu();
        }
        return;
    }
    addRecentFile(path);
    startSource(src);
}

void MainWindow::addRecentFile(const QString &path)
{
    const int kMaxRecent = 8;
    const QString native = QDir::toNativeSeparators(path);
    QSettings s;
    QStringList files = s.value(QStringLiteral("recentFiles")).toStringList();
    files.removeAll(native);
    files.removeAll(path);          // guard against separator-style duplicates
    files.prepend(native);
    while (files.size() > kMaxRecent) files.removeLast();
    s.setValue(QStringLiteral("recentFiles"), files);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    if (!m_recentMenu) return;
    m_recentMenu->clear();

    QSettings s;
    const QStringList files = s.value(QStringLiteral("recentFiles")).toStringList();

    if (files.isEmpty()) {
        QAction *none = m_recentMenu->addAction(QStringLiteral("(No recent files)"));
        none->setEnabled(false);
        return;
    }

    int n = 0;
    for (const QString &f : files) {
        const QString label = QStringLiteral("&%1  %2")
                                  .arg(++n)
                                  .arg(QFileInfo(f).fileName());
        QAction *a = m_recentMenu->addAction(label);
        a->setData(f);
        a->setStatusTip(f);
        a->setEnabled(QFileInfo::exists(f));
        connect(a, &QAction::triggered, this,
                [this, f] { openVideoPath(f); });
    }
    m_recentMenu->addSeparator();
    QAction *clear = m_recentMenu->addAction(QStringLiteral("&Clear Recent"));
    connect(clear, &QAction::triggered, this, [this] {
        QSettings st;
        st.remove(QStringLiteral("recentFiles"));
        updateRecentFilesMenu();
    });
}

void MainWindow::onConnectCamera()
{
    // Opening a camera can take seconds (SDK login + stream start, or several
    // UVC backend probes), so do it on a worker thread behind a busy dialog so
    // the UI stays responsive instead of freezing.
    QProgressDialog *dlg = new QProgressDialog(
        QStringLiteral("Connecting to camera, please wait\xE2\x80\xA6"),
        QString(), 0, 0, this);
    dlg->setWindowTitle(QStringLiteral("Connect Camera"));
    dlg->setWindowModality(Qt::ApplicationModal);
    dlg->setCancelButton(nullptr);
    dlg->setMinimumDuration(0);
    dlg->setAutoClose(false);
    dlg->setAutoReset(false);
    dlg->setValue(0);   // range (0,0) -> indeterminate busy indicator

    struct CamResult { FrameSource *src = nullptr; QString err; };
    auto res = QSharedPointer<CamResult>::create();

    QThread *th = QThread::create([res] {
        QString err;
        FrameSource *src = nullptr;
#ifdef HAVE_HIK_SDK
        src = HikFrameSource::open(&err);
#endif
        if (!src) {
            QString e2;
            src = UvcFrameSource::open(0, &e2);
            if (!src) err = err.isEmpty() ? e2 : (err + QStringLiteral("\n") + e2);
        }
        res->src = src;
        res->err = err;
    });

    connect(th, &QThread::finished, this, [this, dlg, th, res] {
        dlg->close();
        dlg->deleteLater();
        th->deleteLater();
        if (!res->src) {
            QMessageBox::warning(this, QStringLiteral("Connect Camera"),
                res->err.isEmpty() ? QStringLiteral("No camera available.") : res->err);
            return;
        }
        startSource(res->src);
    });

    dlg->show();
    th->start();
}

void MainWindow::onCloseVideo()
{
    m_thread->closeSource();
    if (m_recAct->isChecked())    { QSignalBlocker b(m_recAct);    m_recAct->setChecked(false);    m_recAct->setText(QStringLiteral("Record MP4")); }
    if (m_recRawAct->isChecked()) { QSignalBlocker b(m_recRawAct); m_recRawAct->setChecked(false); m_recRawAct->setText(QStringLiteral("Record RAW")); }
}

void MainWindow::onSourceClosed()
{
    m_view->clear();
    m_hist->clear();
    m_lastOrig = QImage();
    m_lastEnh = QImage();
    m_count = 0;
    m_live = false;
    m_scrubbing = false;
    setPlaying(false);
    { QSignalBlocker b(m_slider); m_slider->setRange(0, 0); m_slider->setValue(0); }
    m_slider->setEnabled(false);
    m_timeLabel->setText(QStringLiteral("-- / --"));
    m_srcLabel->setText(QStringLiteral("No source"));
    m_statLabel->clear();
    statusBar()->showMessage(QStringLiteral("Video closed."), 3000);
}

void MainWindow::startSource(FrameSource *src)
{
    auto pipe = IrPipelineFactory::create();
    if (!pipe) { QMessageBox::critical(this, QStringLiteral("Error"),
                 QStringLiteral("Failed to create the processing engine.")); delete src; return; }
    m_srcLabel->setText(src->describe());
    m_thread->setSource(src, pipe);
    m_thread->setPipelineModel(m_pipePanel->model());
}

// ---- transport --------------------------------------------------------------

void MainWindow::setPlaying(bool playing)
{
    m_playing = playing;
    m_playBtn->setIcon(makeToolIcon(playing ? QStringLiteral("pause") : QStringLiteral("play")));
}

void MainWindow::onPlayPause()
{
    if (m_count == 0 && !m_live) return;
    if (m_playing) { m_thread->pause(); setPlaying(false); }
    else           { m_thread->play();  setPlaying(true); }
}

void MainWindow::onStop()
{
    m_thread->stop();
    setPlaying(false);
    QSignalBlocker b(m_slider);
    m_slider->setValue(0);
}

void MainWindow::onSliderPressed()
{
    if (m_live) return;
    m_scrubbing = true;
    m_wasPlaying = m_playing;
    if (m_playing) { m_thread->pause(); setPlaying(false); }
}

void MainWindow::onSliderMoved(int v)
{
    if (m_live) return;
    m_thread->seek(v);
    m_timeLabel->setText(QStringLiteral("%1 / %2").arg(v + 1).arg(m_count));
}

void MainWindow::onSliderReleased()
{
    if (m_live) return;
    m_scrubbing = false;
    if (m_wasPlaying) { m_thread->play(); setPlaying(true); }
}

// ---- view / pipeline --------------------------------------------------------

void MainWindow::onViewModeChanged(int i)
{
    m_view->setMode(static_cast<ImageView::Mode>(i));
}

void MainWindow::onPipelineChanged()
{
    m_thread->setPipelineModel(m_pipePanel->model());
}

// ---- snapshot / record / fullscreen ----------------------------------------

void MainWindow::onSnapshot()
{
    if (m_lastEnh.isNull()) { statusBar()->showMessage(QStringLiteral("No frame to snapshot."), 3000); return; }
    QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/snapshots");
    QDir().mkpath(dir);
    QString file = dir + QStringLiteral("/IR_") +
                   QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz")) +
                   QStringLiteral(".png");
    if (m_lastEnh.save(file))
        statusBar()->showMessage(QStringLiteral("Snapshot saved: %1").arg(file), 5000);
    else
        statusBar()->showMessage(QStringLiteral("Snapshot failed."), 3000);
}

static QString makeRecordPath(const QString &ext)
{
    QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/records");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/IR_") +
           QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")) + ext;
}

void MainWindow::onToggleRecord(bool on)
{
    if (on) {
        QString file = makeRecordPath(QStringLiteral(".mp4"));
        const char *modeName[] = { "Original", "Processed", "Side by side" };
        const int mode = int(m_view->mode());
        m_thread->startMp4(file, mode);
        m_recAct->setText(QStringLiteral("Stop MP4"));
        statusBar()->showMessage(QStringLiteral("Recording MP4 (%1) to %2")
                                 .arg(QString::fromLatin1(modeName[mode])).arg(file));
    } else {
        m_thread->stopMp4();
        m_recAct->setText(QStringLiteral("Record MP4"));
        statusBar()->showMessage(QStringLiteral("MP4 recording stopped."), 4000);
    }
}

void MainWindow::onToggleRecordRaw(bool on)
{
    if (on) {
        QString file = makeRecordPath(QStringLiteral(".mkv"));
        m_thread->startRaw16(file);
        m_recRawAct->setText(QStringLiteral("Stop RAW"));
        statusBar()->showMessage(QStringLiteral("Recording 16-bit raw to %1").arg(file));
    } else {
        m_thread->stopRaw16();
        m_recRawAct->setText(QStringLiteral("Record RAW"));
        statusBar()->showMessage(QStringLiteral("Raw recording stopped."), 4000);
    }
}

void MainWindow::onToggleFullScreen(bool on)
{
    if (on) showFullScreen();
    else    showNormal();
}

// ---- thread callbacks -------------------------------------------------------

void MainWindow::onStarted(int count, bool live)
{
    m_count = count;
    m_live = live;
    {
        QSignalBlocker b(m_slider);
        m_slider->setRange(0, live ? 0 : qMax(0, count - 1));
        m_slider->setValue(0);
    }
    m_slider->setEnabled(!live && count > 1);
    m_prevBtn->setEnabled(!live && count > 1);
    m_nextBtn->setEnabled(!live && count > 1);
    setPlaying(live);
    m_timeLabel->setText(live ? QStringLiteral("live") : QStringLiteral("1 / %1").arg(count));
}

void MainWindow::onFrameReady(QImage original, QImage enhanced, int index, int count, double ms)
{
    m_lastOrig = original;
    m_lastEnh = enhanced;
    m_view->setImages(original, enhanced);
    if (m_live) {
        m_timeLabel->setText(QStringLiteral("live  #%1").arg(index));
    } else {
        if (!m_scrubbing) { QSignalBlocker b(m_slider); m_slider->setValue(index); }
        m_timeLabel->setText(QStringLiteral("%1 / %2").arg(index + 1).arg(count));
    }
    QString rec;
    if (m_thread->isRecordingMp4())   rec += QStringLiteral("  \xE2\x97\x8f MP4");
    if (m_thread->isRecordingRaw16()) rec += QStringLiteral("  \xE2\x97\x8f RAW16");
    m_statLabel->setText(QStringLiteral("%1 x %2    %3 ms%4")
                         .arg(enhanced.width()).arg(enhanced.height()).arg(ms, 0, 'f', 1).arg(rec));
}

void MainWindow::onHistogram(QVector<int> bins, int lo1, int hi99, int axisLo, int axisHi)
{
    m_hist->setData(bins, lo1, hi99, axisLo, axisHi);
}

void MainWindow::onReachedEnd() { setPlaying(false); }

// ---- about ------------------------------------------------------------------

void MainWindow::onAbout()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("About"));
    dlg.setFixedWidth(440);
    QVBoxLayout *l = new QVBoxLayout(&dlg);
    l->setContentsMargins(24, 22, 24, 20);
    l->setSpacing(8);

    QLabel *logo = new QLabel(&dlg);
    logo->setPixmap(QPixmap(QStringLiteral(":/brand/logo.png")).scaledToWidth(268, Qt::SmoothTransformation));

    QLabel *company = new QLabel(QStringLiteral("Wuhan Wavefront Technology Co., Ltd."), &dlg);
    company->setObjectName(QStringLiteral("aboutTitle"));
    QLabel *tag = new QLabel(QStringLiteral("Infrared \xC2\xB7 Optoelectronics \xC2\xB7 AI"), &dlg);
    tag->setObjectName(QStringLiteral("aboutSub"));

    QLabel *product = new QLabel(QStringLiteral("IR Image Processing Studio  \xE2\x80\x94  %1")
                                 .arg(QString::fromLatin1(IrPipelineFactory::version())), &dlg);
    product->setObjectName(QStringLiteral("aboutSub"));

    QFrame *rule = new QFrame(&dlg);
    rule->setObjectName(QStringLiteral("aboutRule"));
    rule->setFrameShape(QFrame::HLine);

    QLabel *body = new QLabel(&dlg);
    body->setObjectName(QStringLiteral("aboutBody"));
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setText(QStringLiteral(
        "Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. "
        "All rights reserved.\nwechat:angaio/13707128443"));

    l->addWidget(logo);
    l->addSpacing(6);
    l->addWidget(company);
    l->addWidget(tag);
    l->addWidget(product);
    l->addWidget(rule); l->addSpacing(4); l->addWidget(body); l->addSpacing(10);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    l->addWidget(bb);

    dlg.setStyleSheet(QStringLiteral(
        "QDialog { background: #1e222a; }"
        "QLabel { color: #c8d0da; }"
        "QLabel#aboutTitle { color: #e8edf4; font-size: 16px; font-weight: bold; }"
        "QLabel#aboutSub { color: #8b95a3; }"
        "QLabel#aboutBody { color: #b7c0cc; }"
        "QFrame#aboutRule { color: #2c333d; }"
        "QPushButton { background: #2f6df6; color: #ffffff; border: 0; padding: 6px 18px; border-radius: 4px; }"
        "QPushButton:hover { background: #3d78f7; }"));
    dlg.exec();
}
