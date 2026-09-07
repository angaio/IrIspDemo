/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : IrMp4Recorder.h
* Description : Public interface of IrMp4Rec.dll.
*
* Records a stream of 8-bit grayscale frames (the enhanced display image) into
* a standard .mp4 file - H.264 when the FFmpeg build provides an H.264 encoder,
* otherwise MPEG-4 Part 2. The result plays anywhere. Only raw pointers cross
* the boundary - no OpenCV or Qt types.
*
* Usage:
*   auto rec = IrMp4Factory::create();
*   rec->open("out.mp4", w, h, 25);
*   rec->writeFrame(gray8);        // per frame, w*h uint8 samples
*   rec->close();
******************************************************************************/
#ifndef ___IrMp4Recorder_h___
#define ___IrMp4Recorder_h___

#include <memory>
#include <cstdint>

#ifdef _WIN32
#  ifdef __IRMP4_EXPORTS__
#    define IRMP4API __declspec(dllexport)
#  else
#    define IRMP4API __declspec(dllimport)
#  endif
#else
#  define IRMP4API __attribute__((visibility("default")))
#endif

class IRMP4API IrMp4Recorder
{
public:
    virtual ~IrMp4Recorder() {}

    /* Begin a recording. path should end in ".mp4". Returns false on failure
     * (see lastError()). */
    virtual bool open(const char *path, int width, int height, int fps) = 0;

    /* Append one frame: width*height contiguous uint8 grayscale samples. */
    virtual bool writeFrame(const uint8_t *gray) = 0;

    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual long frameCount() const = 0;
    virtual const char *lastError() const = 0;
};

class IRMP4API IrMp4Factory
{
public:
    static std::shared_ptr<IrMp4Recorder> create();
    static const char *fileExtension();   /* ".mp4" */
};

#endif
