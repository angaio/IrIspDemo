/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : IrProcess.h
* Description : The only public interface of IrCoreLib.
*
* IrCoreLib turns a single-channel 16-bit infrared frame into an 8-bit display
* image through a configurable pipeline. The pipeline is an ordered list of
* nodes; each node runs exactly one algorithm, and the frame flows node to node.
* Algorithms fall into three stages:
*
*   IR_STAGE_RANDOM_DENOISE   temporal/spatial noise      (16-bit -> 16-bit)
*   IR_STAGE_FIXED_DENOISE    fixed-pattern / shading      (16-bit -> 16-bit)
*   IR_STAGE_ENHANCE          tone mapping to display       (16-bit -> 8-bit)
*
* The library never names an algorithm: IrCatalog exposes numeric ids, their
* stage, and the tunable parameters of each (key, label, range, default) so a
* UI can build a panel automatically. What each id *is* stays with the caller.
*
* Usage:
*   auto pipe = IrPipelineFactory::create();
*   int n = pipe->addNode(algoId);            // build the chain
*   pipe->setNodeParam(n, "detail", 60);
*   pipe->process(src16, dst8);               // per frame
*
* Threading: one instance is not re-entrant; use one per thread.
******************************************************************************/
#ifndef ___IrProcess_h___
#define ___IrProcess_h___

#include <memory>
#include <opencv2/core.hpp>

#ifdef _WIN32
#  ifdef __IR_DLL_EXPORTS__
#    define IRAPI __declspec(dllexport)
#  else
#    define IRAPI __declspec(dllimport)
#  endif
#else
#  define IRAPI __attribute__((visibility("default")))
#endif

/* Processing stages, in pipeline order. */
enum IrStage
{
    IR_STAGE_NUC            = 0,   /* non-uniformity correction */
    IR_STAGE_RANDOM_DENOISE = 1,
    IR_STAGE_FIXED_DENOISE  = 2,
    IR_STAGE_ENHANCE        = 3
};

/* One tunable parameter of an algorithm. Integer domain keeps the UI simple;
 * an algorithm maps it internally. */
struct IrParamDesc
{
    const char *key;    /* stable identifier, e.g. "detail" */
    const char *label;  /* generic UI label, e.g. "Detail"  */
    int minV;
    int maxV;
    int defV;
};

/* Static catalog of the algorithms the library ships. Ids are stable. */
class IRAPI IrCatalog
{
public:
    static int count();
    static int idAt(int index);
    static int stageOf(int algoId);        /* IrStage */
    static int paramCount(int algoId);
    static IrParamDesc paramOf(int algoId, int index);

    /* Plain 1%-clipped linear auto-gain of a 16-bit frame, for a "before"
     * pane. src: CV_16UC1, dst: CV_8UC1. */
    static void linearView(const cv::Mat &src16, cv::Mat &dst8);

    /* 16-bit histogram helper. Fills a 256-bin count table spanning the frame's
     * actual [min,max] (infrared data occupies a narrow slice of the 16-bit
     * range, so binning over 0..65535 would collapse it into one bin). Reports
     * the 1st/99th-percentile gray values (lo1/hi99) and the axis bounds the
     * bins span (axisLo/axisHi), so a viewer can place the bars and guides. */
    static void histogram(const cv::Mat &src16, int bins[256],
                          int &lo1, int &hi99, int &axisLo, int &axisHi);
};

class IRAPI IrPipeline
{
public:
    virtual ~IrPipeline() {}

    /* ---- node management (ordered) -------------------------------------- */
    virtual int  nodeCount() const = 0;
    virtual int  addNode(int algoId) = 0;              /* append; returns index */
    virtual void insertNode(int index, int algoId) = 0;
    virtual void removeNode(int index) = 0;
    virtual void moveNode(int from, int to) = 0;
    virtual int  nodeAlgo(int index) const = 0;
    virtual void setNodeAlgo(int index, int algoId) = 0;
    virtual bool nodeEnabled(int index) const = 0;
    virtual void setNodeEnabled(int index, bool on) = 0;

    /* Parameters carry the algorithm's defaults until overridden. */
    virtual void setNodeParam(int index, const char *key, int value) = 0;
    virtual int  nodeParam(int index, const char *key) const = 0;

    /* ---- run ------------------------------------------------------------ */

    /* Push one frame through every enabled node. src: CV_16UC1. dst: CV_8UC1.
     * If no enhance node is present, a linear view is produced. */
    virtual bool process(const cv::Mat &src16, cv::Mat &dst8) = 0;

    /* Drop temporal state (call on a new source or after a seek). */
    virtual void reset() = 0;

    virtual double lastProcessMs() const = 0;
    virtual const char *lastError() const = 0;
};

class IRAPI IrPipelineFactory
{
public:
    static std::shared_ptr<IrPipeline> create();
    static const char *version();
};

#endif
