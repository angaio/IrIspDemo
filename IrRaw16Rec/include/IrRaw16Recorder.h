/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : IrRaw16Recorder.h
* Description : Public interface of IrRaw16Rec.dll.
*
* Records a stream of single-channel 16-bit infrared frames losslessly into an
* FFV1-in-Matroska (.mkv) file. FFV1 is a lossless, all-intra codec, so every
* raw AD value round-trips exactly; the container is a widely supported,
* industry-standard format that VLC / FFmpeg / MediaInfo read directly. Only
* raw pointers cross the boundary - no OpenCV or Qt types.
*
* Usage:
*   auto rec = IrRaw16Factory::create();
*   rec->open("out.mkv", w, h, 25);
*   rec->writeFrame(frame16);      // per frame, w*h uint16 samples
*   rec->close();
******************************************************************************/
#ifndef ___IrRaw16Recorder_h___
#define ___IrRaw16Recorder_h___

#include <memory>
#include <cstdint>

#ifdef _WIN32
#  ifdef __IRRAW16_EXPORTS__
#    define IRRAW16API __declspec(dllexport)
#  else
#    define IRRAW16API __declspec(dllimport)
#  endif
#else
#  define IRRAW16API __attribute__((visibility("default")))
#endif

class IRRAW16API IrRaw16Recorder
{
public:
    virtual ~IrRaw16Recorder() {}

    /* Begin a recording. path should end in ".mkv". Returns false on failure
     * (see lastError()). */
    virtual bool open(const char *path, int width, int height, int fps) = 0;

    /* Append one frame: width*height contiguous uint16 samples, row-major. */
    virtual bool writeFrame(const uint16_t *data) = 0;

    /* Finalise the file (flush the encoder, write the index). Safe to call
     * more than once. */
    virtual void close() = 0;

    virtual bool isOpen() const = 0;
    virtual long frameCount() const = 0;
    virtual const char *lastError() const = 0;
};

class IRRAW16API IrRaw16Factory
{
public:
    static std::shared_ptr<IrRaw16Recorder> create();
    /* File extension this recorder writes, e.g. ".mkv". */
    static const char *fileExtension();
};

/* Playback of a 16-bit FFV1/Matroska file (as written above, or any FFV1 .mkv
 * carrying a GRAY16LE track - e.g. ThermoAnalyser recordings). Decodes frames
 * back to raw uint16 for the viewer. Random access is supported (FFV1 is
 * all-intra). */
class IRRAW16API IrRaw16Reader
{
public:
    virtual ~IrRaw16Reader() {}

    virtual bool open(const char *path) = 0;
    virtual int  width() const = 0;
    virtual int  height() const = 0;
    virtual int  frameCount() const = 0;
    virtual double fps() const = 0;

    /* Decode frame [index] into out (width*height contiguous uint16). */
    virtual bool readFrame(int index, uint16_t *out) = 0;

    virtual void close() = 0;
    virtual const char *lastError() const = 0;
};

class IRRAW16API IrRaw16ReaderFactory
{
public:
    static std::shared_ptr<IrRaw16Reader> create();
};

#endif
