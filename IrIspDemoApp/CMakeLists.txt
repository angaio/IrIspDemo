# Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
# wechat:angaio/13707128443

#-------------------------------------------------------------------------------
# IrIspDemoApp — the Qt user interface.
#
# It sees only the public headers of the three prebuilt libraries (IrProcess.h,
# IrMp4Recorder.h, IrRaw16Recorder.h) and links their imported targets. OpenCV
# is used here purely for capture and image containers.
#-------------------------------------------------------------------------------
set(TARGET IrProcessDemo)

# Brand: Qt resource (window icon + About logo) and, on Windows, the exe icon.
set(APP_RC "")
if(WIN32)
    set(APP_RC res/app.rc)
endif()

add_executable(${TARGET}
    res/brand.qrc
    ${APP_RC}
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/ImageView.h
    src/ImageView.cpp
    src/ProcessThread.h
    src/ProcessThread.cpp
    src/FrameSource.h
    src/FrameSource.cpp
    src/AlgoNames.h
    src/HistogramWidget.h
    src/HistogramWidget.cpp
    src/PipelinePanel.h
    src/PipelinePanel.cpp
)

# Windowed app on Windows (no console window).
if(WIN32)
    set_target_properties(${TARGET} PROPERTIES WIN32_EXECUTABLE ON)
endif()

target_include_directories(${TARGET} PRIVATE ${OpenCV_INCLUDE_DIRS})

target_link_libraries(${TARGET} PRIVATE
    Qt5::Core Qt5::Gui Qt5::Widgets
    IrIspCore                # detail-enhancement / tone-mapping (prebuilt DLL)
    IrMp4Rec                 # 8-bit MP4 recorder (prebuilt DLL)
    IrRaw16Rec               # 16-bit raw FFV1/MKV recorder (prebuilt DLL)
    ${OpenCV_LIBS}           # capture + image containers only
)
