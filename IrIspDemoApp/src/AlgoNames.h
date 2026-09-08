/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : AlgoNames.h
* Description : Human-readable names for the pipeline algorithms.
*
* The library (IrCore) deliberately exposes only numeric ids and generic
* parameter labels - no algorithm identity lives in the DLL. The display names
* the pipeline UI shows are defined here, in the application, and mapped by id.
******************************************************************************/
#ifndef ___AlgoNames_h___
#define ___AlgoNames_h___

#include <QString>
#include "IrProcess.h"

namespace algo {

inline QString name(int algoId)
{
    switch (algoId) {
        case 21: return QStringLiteral("Two-point NUC");
        case 10: return QStringLiteral("Non-local means");
        case 11: return QStringLiteral("Shutterless correction");
        case 12: return QStringLiteral("Bilateral denoise");
        case 13: return QStringLiteral("Temporal filter");
        case 14: return QStringLiteral("Gaussian filter");
        case 20: return QStringLiteral("De-shading");
        case 30: return QStringLiteral("DDE enhancement");
        case 31: return QStringLiteral("Log tone mapping");
        case 32: return QStringLiteral("Mixed tone mapping");
        case 33: return QStringLiteral("Histogram equalization");
        case 15: return QStringLiteral("BM3D denoise");
        case 22: return QStringLiteral("Destripe (1D-WLS)");
        case 34: return QStringLiteral("DDE balanced (open)");
        case 35: return QStringLiteral("DDE legacy (open)");
        default: return QStringLiteral("Algorithm %1").arg(algoId);
    }
}

inline QString stageName(int stage)
{
    switch (stage) {
        case IR_STAGE_NUC:            return QStringLiteral("Non-uniformity correction");
        case IR_STAGE_RANDOM_DENOISE: return QStringLiteral("Random-noise denoise");
        case IR_STAGE_FIXED_DENOISE:  return QStringLiteral("Fixed-noise denoise");
        case IR_STAGE_ENHANCE:        return QStringLiteral("Enhancement");
        default:                      return QStringLiteral("Stage %1").arg(stage);
    }
}

} // namespace algo

#endif
