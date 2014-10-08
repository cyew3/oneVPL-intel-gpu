/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_debug.h"
#include "umc_h265_timing.h"
#include <cstdarg>

#ifdef ENABLE_TRACE
#include "umc_h265_frame.h"
#endif

#ifdef ENABLE_STAT
#include "umc_h265_frame.h"
#include "umc_h265_frame_info.h"
#include "umc_h265_prediction.h"
#endif

namespace UMC_HEVC_DECODER
{

#ifdef __EXCEPTION_HANDLER_
ExceptionHandlerInitializer exceptionHandler;
#endif // __EXCEPTION_HANDLER_

#ifdef USE_DETAILED_H265_TIMING
    TimingInfo* clsTimingInfo;
#endif

#ifdef ENABLE_TRACE
// Debug output
void Trace(vm_char * format, ...)
{
    va_list(arglist);
    va_start(arglist, format);

    vm_char cStr[256];
    vm_string_vsprintf(cStr, format, arglist);

    //OutputDebugString(cStr);
    vm_string_printf(VM_STRING("%s"), cStr);
    //fflush(stdout);
}

const vm_char* GetFrameInfoString(H265DecoderFrame * frame)
{
    static vm_char str[256];
    vm_string_sprintf(str, VM_STRING("POC - %d, uid - %d"), frame->m_PicOrderCnt, frame->m_UID);
    return str;
}

#endif // ENABLE_TRACE

#ifdef ENABLE_STAT
CUSStat::CUSStat()
    : bwForFrameAccum(0)
    , numFrames(0)
{
}

CUSStat::~CUSStat()
{
    printf("\nnumFrames- %d, average bandwidth bpp - %f\n", numFrames, bwForFrameAccum/numFrames);
}

void CUSStat::CalculateStat(H265DecoderFrame * frame)
{
    m_sps = frame->GetAU()->GetAnySlice()->GetSeqParam();
    bwForFrame = 0;

    Ipp32s numCU = frame->getCD()->m_NumCUsInFrame;
    for (Ipp32s i = 0; i < numCU; i++)
    {
        CalculateCU(frame->getCU(i));
    }

    bwForFrameAccum += (float)bwForFrame / (float)(m_sps->pic_width_in_luma_samples*m_sps->pic_height_in_luma_samples);
    numFrames++;
}

void CUSStat::CalculateCU(H265CodingUnit* cu)
{
    CalculateCURecur(cu, 0, 0);
}

void CUSStat::CalculateCURecur(H265CodingUnit* cu, Ipp32u absPartIdx, Ipp32u depth)
{
    if (cu->GetDepth(absPartIdx) > depth)
    {
        depth++;
        Ipp32u partOffset = cu->m_NumPartition >> (depth << 1);
        for (Ipp32s i = 0; i < 4; i++, absPartIdx += partOffset)
        {
            CalculateCURecur(cu, absPartIdx, depth);
        }
        return;
    }

    if (cu->GetPredictionMode(absPartIdx) != MODE_INTER)
        return;

    Ipp32s countPart = cu->getNumPartInter(absPartIdx);
    Ipp32s PUOffset = 0;
    if (countPart > 1)
    {
        EnumPartSize PartSize = cu->GetPartitionSize(absPartIdx);
        PUOffset = (g_PUOffset[PartSize] << ((m_sps->MaxCUDepth - depth) << 1)) >> 4;
    }

    // Loop over prediction units
    for (Ipp32s PartIdx = 0, subPartIdx = absPartIdx; PartIdx < countPart; PartIdx++, subPartIdx += PUOffset)
    {
        H265PUInfo PUi;

        cu->getPartIndexAndSize(absPartIdx, PartIdx, PUi.Width, PUi.Height);
        PUi.PartAddr = subPartIdx;

        Ipp32s LPelX = cu->m_CUPelX + cu->m_rasterToPelX[subPartIdx];
        Ipp32s TPelY = cu->m_CUPelY + cu->m_rasterToPelY[subPartIdx];
        Ipp32s PartX = LPelX >> m_sps->log2_min_transform_block_size;
        Ipp32s PartY = TPelY >> m_sps->log2_min_transform_block_size;

        const H265MVInfo & mvInfo = cu->m_Frame->getCD()->GetTUInfo(m_sps->NumPartitionsInFrameWidth * PartY + PartX);

        Ipp32s numRefs = (mvInfo.m_refIdx[REF_PIC_LIST_0] >= 0) + (mvInfo.m_refIdx[REF_PIC_LIST_1] >= 0);

        bwForFrame += PUi.Height*PUi.Width * numRefs;
    }
}

CUSStat* CUSStat::GetCUStat()
{
    static CUSStat stat;
    return &stat;
}
#endif
} // namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER

