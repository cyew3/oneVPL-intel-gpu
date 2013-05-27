/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_dec_utils.h

\* ****************************************************************************** */
#ifndef __MFX_MPEG2_DEC_UTILS_H__
#define __MFX_MPEG2_DEC_UTILS_H__

#include "mfx_common.h"
#if defined (MFX_ENABLE_MPEG2_VIDEO_DEC)

#include "mfxvideo.h"
#include "umc_mpeg2_dec_defs.h"

//#define MFXMPEG2_BREAKPOINT
#ifdef MFXMPEG2_BREAKPOINT

#define MFXMPEG2_DECLARE_COUNTERS mfxU32 m_frameCnt, m_picCnt, m_isBottomFld, m_isSecondFld, m_sliceCnt, m_sliceY, m_sliceX, m_mbX, m_mbY
#define MFXMPEG2_UPDATE_PIC_CNT if (cuc->SliceId == 0) ++m_picCnt;
#define MFXMPEG2_UPDATE_FRAME_CNT if (cuc->SliceId == 0 && !cuc->FrameParam->SecondFieldFlag) ++m_frameCnt;
#define MFXMPEG2_RESET_ALL m_frameCnt = m_picCnt = -1; m_isBottomFld = m_isSecondFld = m_sliceCnt = m_sliceY = m_sliceX = m_mbX = m_mbY = 0;
#define MFXMPEG2_SET_BREAKPOINT(COND) if (COND) __asm { int 3 };

#else //MFXMPEG2_BREAKPOINT

#define MFXMPEG2_DECLARE_COUNTERS
#define MFXMPEG2_UPDATE_PIC_CNT
#define MFXMPEG2_UPDATE_FRAME_CNT
#define MFXMPEG2_RESET_ALL
#define MFXMPEG2_SET_BREAKPOINT(COND)

#endif //MFXMPEG2_BREAKPOINT

namespace MfxMpeg2Dec
{
    void ClearMfxStruct(mfxVideoParam *par);
    void ClearMfxStruct(mfxFrameParam *par);
    void ClearMfxStruct(mfxSliceParam *par);
    mfxU8 GetUmcPicStruct(mfxU8 FieldPicFlag, mfxU8 BottomFieldFlag);
    UMC::MPEG2FrameType GetUmcPicCodingType(mfxU8 FrameType, mfxU8 BottomFieldFlag);
    mfxU8 GetUmcTopFieldFirst(mfxU8 FieldPicFlag, mfxU8 BottomFieldFlag, mfxU8 SecondFieldFlag);
    mfxU8 GetUmcDctType(mfxU8 FieldPicFlag, mfxU8 TransformFlag);
    mfxU16 GetUmcCodedBlockPattern(mfxU8 ChromaFormatIdc, mfxU16 cp4x4Y, mfxU16 cp4x4U, mfxU16 cp4x4V);
    mfxU8 GetUmcNumCoef(mfxU8 MbDataSize128b);
    mfxU8 GetUmcMbType(mfxU8 MbType5Bits, mfxU8 MbSkipFlag);
    mfxU8 GetUmcMotionType(mfxU8 MbType5Bits, mfxU8 FieldPicFlag);
    mfxI32 GetUmcMbAddrInc(mfxU32 mbCnt, mfxU32 MbXcnt);
    void InitFirstRefFrame(const Ipp8u* ref[3], const mfxFrameCUC& cuc);
    void InitSecondRefFrame(const Ipp8u* ref[3], const mfxFrameCUC& cuc);

} // namespace

#endif
#endif
