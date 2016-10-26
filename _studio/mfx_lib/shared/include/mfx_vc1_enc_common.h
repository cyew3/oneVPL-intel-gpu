//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2011 Intel Corporation. All Rights Reserved.
//

#include "mfx_vc1_enc_defs.h"

#if defined (MFX_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _MFX_VC1_ENC_COMMON_H_
#define _MFX_VC1_ENC_COMMON_H_

#include "mfx_enc_common.h"
#include "mfx_vc1_enc_ex_param_buf.h"
#include "umc_vc1_enc_planes.h"
#include "umc_vc1_enc_sequence_adv.h"
#include "umc_vc1_enc_sequence_sm.h"
#include "umc_vc1_enc_mb.h"

#define MFX_VC1_MA_MEMORY           MFX_MEMTYPE_PERSISTENT_MEMORY
#define MFX_VC1_ME_MEMORY           MFX_MEMTYPE_PERSISTENT_MEMORY
#define MFX_VC1_SH_MEMORY           MFX_MEMTYPE_PERSISTENT_MEMORY
#define MFX_VC1_PICTURE_MEMORY      MFX_MEMTYPE_PERSISTENT_MEMORY
#define MFX_VC1_CODED_MB_MEMORY     MFX_MEMTYPE_PERSISTENT_MEMORY
#define MFX_VC1_MB_MEMORY           MFX_MEMTYPE_PERSISTENT_MEMORY
#define MFX_VC1_BRC_MEMORY          MFX_MEMTYPE_PERSISTENT_MEMORY
#define MFX_VC1_ENCODER_MEMORY      MFX_MEMTYPE_PERSISTENT_MEMORY
#define MFX_VC1_PIXELS_MEMORY       MFX_MEMTYPE_PERSISTENT_MEMORY

#define MFX_VC1_ENCODER_PADDING_SIZE  32
enum
{
    MFX_VC1_ENC_MIXED_MV          = 0x00,
    MFX_VC1_ENC_1MV               = 0x01,
    MFX_VC1_ENC_1MV_HALF_BICUBIC  = 0x02,
    MFX_VC1_ENC_1MV_HALF_BILINEAR = 0x03,
};
typedef struct _mfxVideoParam2 {

   mfxVideoParam    VideoParam;    // set Version.Major=0, and Version.Minor=1
   mfxU8            NumExtBuffer;
   mfxHDL*          ExtBuffer;

} mfxVideoParam2;

#define VC1_MFX_FIRST_ME_INDEX(index)   ((index)& 0x0F);
#define VC1_MFX_SECOND_ME_INDEX(index)  ((index)& 0xF0);

mfxStatus GetUMCPictureType(mfxU8 FrameType, mfxU8 FrameType2,
                            bool bField, bool bFrame, mfxU8 Pic4MvAllowed,
                            UMC_VC1_ENCODER::ePType* VC1PicType);
mfxStatus     GetUMCProfile(mfxU8 MFXprofile, Ipp32s* UMCprofile);
mfxStatus     GetUMCLevel(mfxU8 MFXprofile, mfxU8 MFXlevel, Ipp32s* UMClevel);

Ipp32u CalculateUMC_HRD_BufferSize(mfxU32 MFXSize);
Ipp32u CalculateUMC_HRD_InitDelay(mfxU32 MFXDelay);

//mfxI16      GetAdditionalFrameIndex (ExtVC1FrameLabel* pS, mfxU8 frameLabel, mfxU8 &reused);

extern Ipp8u BFractionScaleFactor[30][3];
mfxStatus  GetBFractionParameters (mfxU8 code,Ipp8u &nom, Ipp8u &denom, Ipp8u &scaleFactor);
mfxStatus  GetBFractionCode       (mfxU8 &code,Ipp8u nom, Ipp8u denom);

UMC_VC1_ENCODER::eReferenceFieldType GetRefFieldFlag(mfxU8 NumRefPic, mfxU8 RefFieldPicFlag, mfxU8 bSecond);

mfxStatus GetSavedMVFrame (mfxFrameCUC *cuc, mfxI16Pair ** pSavedMV);
mfxStatus GetSavedMVField (mfxFrameCUC *cuc, mfxI16Pair ** pSavedMV,Ipp8u** pDirection);

//mfxStatus GetUMC_QP(mfxU8 unifiedQP, Ipp8u &UMCdoubleQuant);

mfxStatus GetMBRoundControl (mfxFrameCUC *cuc, mfxI8** pRC);

typedef void (*pReplicateBorderChroma) (Ipp8u* pU,Ipp8u* pV, Ipp32u stepUV,IppiSize srcRoiSizeUV, IppiSize dstRoiSizeUV,
                                            int topBorderWidth,  int leftBorderWidth);

mfxStatus SetFrameYV12 (mfxU8               index,
                        mfxFrameSurface *   pFrameSurface,
                        mfxVideoParam*      pVideoParam,
                        UMC_VC1_ENCODER::ePType pictureType,
                        UMC_VC1_ENCODER::Frame * pFrame,
                        bool bField);

mfxStatus SetFrameNV12 (mfxU8               index,
                        mfxFrameSurface *   pFrameSurface,
                        mfxVideoParam*      pVideoParam,
                        UMC_VC1_ENCODER::ePType pictureType,
                        UMC_VC1_ENCODER::Frame * pFrame,
                        bool bField);

typedef mfxStatus (*pSetFrame) (mfxU8               index,
                                mfxFrameSurface *   pFrameSurface,
                                mfxVideoParam*      pVideoParam,
                                UMC_VC1_ENCODER::ePType pictureType,
                                UMC_VC1_ENCODER::Frame * pFrame,
                                bool bField);

inline mfxU16 GetWidth (mfxFrameInfo * pPar)
{
    return pPar->Width;
}
inline mfxU16 GetFrameWidth (mfxFrameInfo * pPar)
{
    return (pPar->CropW!=0) ? pPar->CropW : pPar->Width;
}
inline mfxU16 GetHeight (mfxFrameInfo * pPar)
{
    return pPar->Height;
}
inline mfxU16 GetFrameHeight (mfxFrameInfo * pPar)
{
    return (pPar->CropH!=0) ? pPar->CropH : pPar->Height;
}

inline void GetRealFrameSize(mfxInfoMFX *mfx, Ipp16u &w, Ipp16u &h)
{
    if(mfx->FrameInfo.CropW != 0 && mfx->FrameInfo.CropH != 0)
    {
        w = mfx->FrameInfo.CropW;
        h = mfx->FrameInfo.CropH;
    }
    else
    {
        if(mfx->FrameInfo.CropW == 0)
            w = mfx->FrameInfo.Width;

        if(mfx->FrameInfo.CropH == 0)
            h = mfx->FrameInfo.Height;
    }
}

inline void GetExpandedFrameSize(mfxInfoMFX *mfx, Ipp16u &w, Ipp16u &h)
{
     w = mfx->FrameInfo.Width;
     h = mfx->FrameInfo.Height;
}

inline mfxU32 GetLumaShift (mfxFrameInfo *pInfo, mfxFrameData* pFrameData)
{
    return pInfo->CropY*pFrameData->Pitch + pInfo->CropX;
}

inline mfxU32 GetChromaShiftYV12 (mfxFrameInfo * pPar, mfxFrameData* pFrameData)
{
   return (pPar->CropY >> 1)*(pFrameData->Pitch>>1) + (pPar->CropX>>1);
}
inline mfxU32 GetChromaShiftNV12 (mfxFrameInfo * pPar, mfxFrameData* pFrameData)
{
    return (pPar->CropY >> 1)*(pFrameData->Pitch>>pPar->ScaleCPitch) + (pPar->CropX);
}

inline mfxU32 GetChromaShiftNV12 (mfxFrameInfo * pPar, mfxFrameData* pFrameData)
{
    return (pPar->CropY >> 1)*(pFrameData->Pitch) + (pPar->CropX);
}

typedef mfxU32 (*pGetChromaShift) (mfxFrameInfo * pPar, mfxFrameData* pFrameData);

#define VC1_MFX_MAX_FRAMES_FOR_UMC_ME 16

//----UMC data -> MFX data--------------------------------------------

mfxStatus GetMFXProfile(Ipp8u UMCprofile, mfxU8* MFXprofile);
mfxStatus GetMFXLevel(Ipp8u UMCprofile, Ipp8u UMClevel, mfxU8* MFXlevel);
mfxU32 CalculateMFX_HRD_BufferSize(Ipp32u UMCSize);
mfxU32 CalculateMFX_HRD_InitDelay(Ipp32u UMCDelay);


//----------UMC functions -------------------------------

bool isPredicted(UMC_VC1_ENCODER::ePType UMCtype);
bool isBPredicted(UMC_VC1_ENCODER::ePType UMCtype);
bool isField(UMC_VC1_ENCODER::ePType UMCtype);
bool isIntra(UMC_VC1_ENCODER::ePType UMCtype);
bool isReference(UMC_VC1_ENCODER::ePType UMCtype);

//------------------------------------------------------------------------
void ReplicateBorderChroma_YV12 (Ipp8u* pU,Ipp8u* pV, Ipp32u stepUV,IppiSize srcRoiSizeUV, IppiSize dstRoiSizeUV,
                                        int topBorderWidth,  int leftBorderWidth);
void ReplicateBorderChroma_NV12 (Ipp8u* pUV, Ipp8u* /*pV*/, Ipp32u stepUV,IppiSize srcRoiSizeUV, IppiSize dstRoiSizeUV,
                                 int topBorderWidth,  int leftBorderWidth);

IppStatus   _own_ippiReplicateBorder_8u_C1R  (  Ipp8u * pSrc,  int srcStep,
                                                IppiSize srcRoiSize, IppiSize dstRoiSize,
                                                int topBorderWidth,  int leftBorderWidth);


typedef mfxStatus (*pPadFrameProgressive)(mfxU8* pYPlane, mfxU8* pUPlane,mfxU8* pVPlane,
                                          mfxU16 w,  mfxU16 h,  mfxU16 step,
                                          mfxI32 PadT, mfxI32 PadL, mfxI32 PadB, mfxI32 PadR);

typedef mfxStatus (*pPadFrameField)      (mfxU8* pYPlane, mfxU8* pUPlane,mfxU8* pVPlane,
                                          mfxU16 w,  mfxU16 h, mfxU16 step,
                                          mfxI32 PadT, mfxI32 PadL, mfxI32 PadB, mfxI32 PadR);

inline mfxStatus PadFrameProgressiveYV12(   mfxU8* pYPlane, mfxU8* pUPlane,mfxU8* pVPlane,
                                            mfxU16 w,  mfxU16 h,  mfxU16 step,
                                            mfxI32 PadT, mfxI32 PadL, mfxI32 PadB, mfxI32 PadR)
{

    IppiSize srcRoiSizeY   = {w ,h};
    IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
    IppiSize dstRoiSizeY   = {srcRoiSizeY.width + PadL + PadR,srcRoiSizeY.height + PadT + PadB};
    IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1, dstRoiSizeY.height>>1};

    _own_ippiReplicateBorder_8u_C1R  (pYPlane, step, srcRoiSizeY, dstRoiSizeY, PadT, PadL);
    ReplicateBorderChroma_YV12(pUPlane,pVPlane, step>>1, srcRoiSizeUV,dstRoiSizeUV,  PadT>>1, PadL>>1);

    return MFX_ERR_NONE;
}
inline mfxStatus PadFrameProgressiveNV12(   mfxU8* pYPlane, mfxU8* pUPlane,mfxU8* pVPlane,
                                            mfxU16 w,  mfxU16 h,  mfxU16 step,
                                            mfxI32 PadT, mfxI32 PadL, mfxI32 PadB, mfxI32 PadR)
{

    IppiSize srcRoiSizeY   = {w ,h};
    IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
    IppiSize dstRoiSizeY   = {srcRoiSizeY.width + PadL + PadR,srcRoiSizeY.height + PadT + PadB};
    IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1, dstRoiSizeY.height>>1};

    _own_ippiReplicateBorder_8u_C1R  (pYPlane, step, srcRoiSizeY, dstRoiSizeY, PadT, PadL);
    ReplicateBorderChroma_NV12(pUPlane,pVPlane, step, srcRoiSizeUV,dstRoiSizeUV,  PadT>>1, PadL>>1);

    return MFX_ERR_NONE;
}

inline mfxStatus PadFrameFieldYV12(     mfxU8* pYPlane, mfxU8* pUPlane,mfxU8* pVPlane,
                                        mfxU16 w,  mfxU16 h, mfxU16 step,
                                        mfxI32 PadT, mfxI32 PadL, mfxI32 PadB, mfxI32 PadR)
{

    IppiSize srcRoiSizeY   = {w ,h>>1};
    mfxU16   stepC = step >>1;
    IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};

    IppiSize dstRoiSizeY   = {srcRoiSizeY.width  + PadL + PadR,srcRoiSizeY.height + (PadT>>1) + (PadB>>1) };

    IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};

    /* ----------------- Luma --------------------------------- */

    _own_ippiReplicateBorder_8u_C1R  (pYPlane,        step<<1, srcRoiSizeY, dstRoiSizeY,  PadT>>1, PadL);
    _own_ippiReplicateBorder_8u_C1R  (pYPlane + step, step<<1, srcRoiSizeY, dstRoiSizeY,  PadT>>1, PadL);

    /*-------------------- Chroma --------------------------------*/

    ReplicateBorderChroma_YV12(pUPlane, pVPlane, stepC<<1, srcRoiSizeUV,dstRoiSizeUV, PadT>>2, PadL>>1);
    ReplicateBorderChroma_YV12(pUPlane + stepC, pVPlane + stepC, stepC<<1, srcRoiSizeUV,dstRoiSizeUV,PadT>>2, PadL>>1);


    return MFX_ERR_NONE;
}
inline mfxStatus PadFrameFieldNV12(    mfxU8* pYPlane, mfxU8* pUPlane,mfxU8* pVPlane,
                                       mfxU16 w,  mfxU16 h, mfxU16 step,
                                       mfxI32 PadT, mfxI32 PadL, mfxI32 PadB, mfxI32 PadR)
{
    mfxU16   stepC = step;

    IppiSize srcRoiSizeY   = {w ,h>>1};
    IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};

    IppiSize dstRoiSizeY   = {srcRoiSizeY.width  + PadL + PadR, srcRoiSizeY.height + (PadT>>1) + (PadB>>1) };
    IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};

    /* ----------------- Luma --------------------------------- */

    _own_ippiReplicateBorder_8u_C1R  (pYPlane,        step<<1, srcRoiSizeY, dstRoiSizeY,  PadT>>1, PadL);
    _own_ippiReplicateBorder_8u_C1R  (pYPlane + step, step<<1, srcRoiSizeY, dstRoiSizeY,  PadT>>1, PadL);

    /*-------------------- Chroma --------------------------------*/

    ReplicateBorderChroma_NV12(pUPlane, pVPlane, stepC<<1, srcRoiSizeUV,dstRoiSizeUV, PadT>>2, PadL>>1);
    ReplicateBorderChroma_NV12(pUPlane + stepC, pVPlane + stepC, stepC<<1, srcRoiSizeUV,dstRoiSizeUV,PadT>>2, PadL>>1);


    return MFX_ERR_NONE;
}
typedef   mfxStatus (*PadFrame) (mfxU8* pYPlane, mfxU8* pUPlane,mfxU8* pVPlane,
                       mfxU16 w,  mfxU16 h, mfxU16 step,
                       mfxU16 wC, mfxU16 hC,mfxU16 stepC,
                       mfxI32 paddingT, mfxI32 paddingL, mfxI32 paddingB, mfxI32 paddingR);




mfxStatus   CheckCucSeqHeaderAdv(ExtVC1SequenceParams * m_pSH, UMC_VC1_ENCODER::VC1EncoderSequenceADV*   m_pVC1SH);
mfxStatus   CheckCucSeqHeaderSM (ExtVC1SequenceParams * m_pSH, UMC_VC1_ENCODER::VC1EncoderSequenceSM*    m_pVC1SH);

mfxStatus GetMFXTransformType(Ipp32s UMCTSType, mfxU8* MFXLuma, mfxU8* MFXU, mfxU8* MFXV);
mfxStatus GetUMCTransformType(mfxU8 MFXLuma, mfxU8 MFXU, mfxU8 MFXV, Ipp32s* UMCTSType);

mfxStatus GetMFXTransformType(UMC_VC1_ENCODER::eTransformType* tsType, mfxU8* MFXLuma, mfxU8* MFXU, mfxU8* MFXV);
mfxStatus GetUMCTransformType(mfxU8 MFXLuma, mfxU8 MFXU, mfxU8 MFXV, UMC_VC1_ENCODER::eTransformType* tsType);

mfxStatus GetMFXIntraPattern(UMC::MeMB *pMECurrMB, mfxU8* SubMbPredMode);
mfxStatus GetUMCIntraPattern(mfxU8 SubMbPredMode, UMC::MeMB *pMECurrMB);

mfxStatus GetMFXHybrid(UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB, mfxU8* pMFXHybrid);
mfxStatus GetUMCHybrid(mfxU8 pMFXHybrid, UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB);

mfxStatus GetMFX_CBPCY(Ipp8u UMC_CPBCY, mfxU16* CBPCY_Y, mfxU16* CBPCY_U, mfxU16* CBPCY_V);
Ipp8u GetUMC_CBPCY(mfxU16 CBPCY_Y, mfxU16 CBPCY_U, mfxU16 CBPCY_V);

mfxStatus GetFirstLastMB (mfxFrameCUC *cuc, Ipp32s &firstMB, Ipp32s &lastMB, bool bOneSlice = false);
mfxStatus GetSliceRows (mfxFrameCUC *cuc, mfxI32 &firstRow, mfxI32 &nRows, bool bOneSlice = false);

void*  GetExBufferIndex (mfxVideoParam *par, mfxU32 cucID);

typedef mfxStatus (*pCopyFrame)(mfxFrameInfo *srcInfo, mfxFrameData* src, mfxFrameInfo *dstInfo, mfxFrameData* dst, mfxU32 w, mfxU32 h);
typedef mfxStatus (*pCopyField)(mfxFrameInfo *srcInfo, mfxFrameData* src, mfxFrameInfo *dstInfo, mfxFrameData* dst, mfxU32 w, mfxU32 h, bool bBottom );


inline mfxStatus copyFrameYV12(mfxFrameInfo *srcInfo, mfxFrameData* src, mfxFrameInfo *dstInfo, mfxFrameData* dst, mfxU32 w, mfxU32 h )
{
    mfxU32 shiftSrc = GetLumaShift(srcInfo,src);
    mfxU32 shiftDst = GetLumaShift(dstInfo,dst);
    IppiSize size = {w, h};

    if (srcInfo->CropX + w > srcInfo->Width  || dstInfo->CropX + w > dstInfo->Width ||
        srcInfo->CropY + h > srcInfo->Height || dstInfo->CropY + h > dstInfo->Height)
        return MFX_ERR_UNSUPPORTED;

    ippiCopy_8u_C1R(src->Y + shiftSrc,src->Pitch,dst->Y + shiftDst,dst->Pitch,size);

    shiftSrc = GetChromaShiftYV12(srcInfo,src);
    shiftDst = GetChromaShiftYV12(dstInfo,dst);

    size.width  = size.width>>1;
    size.height = size.height>>1;

    ippiCopy_8u_C1R(src->U + shiftSrc,src->Pitch>>1,dst->U + shiftDst,dst->Pitch>>1,size);
    ippiCopy_8u_C1R(src->V + shiftSrc,src->Pitch>>1,dst->V + shiftDst,dst->Pitch>>1,size);

    return MFX_ERR_NONE;
}

inline mfxStatus copyFrameNV12(mfxFrameInfo *srcInfo, mfxFrameData* src, mfxFrameInfo *dstInfo, mfxFrameData* dst, mfxU32 w, mfxU32 h )
{
    mfxU32 shiftSrc = GetLumaShift(srcInfo,src);
    mfxU32 shiftDst = GetLumaShift(dstInfo,dst);
    IppiSize size = {w, h};

    if (srcInfo->CropX + w > srcInfo->Width  || dstInfo->CropX + w > dstInfo->Width ||
        srcInfo->CropY + h > srcInfo->Height || dstInfo->CropY + h > dstInfo->Height)
        return MFX_ERR_UNSUPPORTED;

    ippiCopy_8u_C1R(src->Y + shiftSrc,src->Pitch,dst->Y + shiftDst,dst->Pitch,size);

    shiftSrc = GetChromaShiftNV12(srcInfo,src);
    shiftDst = GetChromaShiftNV12(dstInfo,dst);
    size.width  = size.width;
    size.height = size.height>>1;

    ippiCopy_8u_C1R(src->U + shiftSrc,src->Pitch,dst->U + shiftDst,dst->Pitch,size);

    return MFX_ERR_NONE;
}
inline mfxStatus copyFieldYV12(mfxFrameInfo *srcInfo, mfxFrameData* src, mfxFrameInfo *dstInfo, mfxFrameData* dst, mfxU32 w, mfxU32 h, bool bBottom )
{
    mfxU32 shiftSrc = GetLumaShift(srcInfo,src) + bBottom*src->Pitch;
    mfxU32 shiftDst = GetLumaShift(dstInfo,dst) + bBottom*dst->Pitch;
    IppiSize size = {w, h};

    if (srcInfo->CropX + w > srcInfo->Width  || dstInfo->CropX + w > dstInfo->Width ||
        srcInfo->CropY + h > srcInfo->Height || dstInfo->CropY + h > dstInfo->Height)
        return MFX_ERR_UNSUPPORTED;

    ippiCopy_8u_C1R(src->Y + shiftSrc,src->Pitch<<1,dst->Y + shiftDst,dst->Pitch<<1,size);

    shiftSrc = GetChromaShiftYV12(srcInfo,src) + bBottom*(src->Pitch>>1);
    shiftDst = GetChromaShiftYV12(dstInfo,dst) + bBottom*(dst->Pitch>>1);

    size.width  = size.width>>1;
    size.height = size.height>>1;

    ippiCopy_8u_C1R(src->U + shiftSrc,(src->Pitch>>1)<<1,
                    dst->U + shiftDst,(dst->Pitch>>1)<<1,size);
    ippiCopy_8u_C1R(src->V + shiftSrc,(src->Pitch>>1)<<1,
                    dst->V + shiftDst,(dst->Pitch>>1)<<1,size);

    return MFX_ERR_NONE;
}
inline mfxStatus copyFieldNV12(mfxFrameInfo *srcInfo, mfxFrameData* src, mfxFrameInfo *dstInfo, mfxFrameData* dst, mfxU32 w, mfxU32 h, bool bBottom )
{
    mfxU32 shiftSrc = GetLumaShift(srcInfo,src) + bBottom*src->Pitch;
    mfxU32 shiftDst = GetLumaShift(dstInfo,dst) + bBottom*dst->Pitch;
    IppiSize size = {w, h};

    if (srcInfo->CropX + w > srcInfo->Width  || dstInfo->CropX + w > dstInfo->Width ||
        srcInfo->CropY + h > srcInfo->Height || dstInfo->CropY + h > dstInfo->Height)
        return MFX_ERR_UNSUPPORTED;

    ippiCopy_8u_C1R(src->Y + shiftSrc,src->Pitch<<1,dst->Y + shiftDst,dst->Pitch<<1,size);

    shiftSrc = GetChromaShiftNV12(srcInfo,src) + bBottom*(src->Pitch);
    shiftDst = GetChromaShiftNV12(dstInfo,dst) + bBottom*(dst->Pitch);

    size.width  = size.width;
    size.height = size.height>>1;

    ippiCopy_8u_C1R(src->U + shiftSrc,(src->Pitch)<<1,
                    dst->U + shiftDst,(dst->Pitch)<<1,size);

    return MFX_ERR_NONE;
}
inline void GetPaddingSize (mfxFrameInfo* pInfo, mfxU32 w, mfxU32 h, mfxI32 &top, mfxI32  &left, mfxI32 &bottom, mfxI32 &right)
{
   left  = pInfo->CropX;
   right = (w < (pInfo->Width - left))? pInfo->Width - w - left : 0;

   top    =  pInfo->CropY;
   bottom = (h < (pInfo->Height - top))? pInfo->Height - h - top : 0;
}

void CorrectMfxVideoParams(mfxVideoParam *out);
void SetMfxVideoParams(mfxVideoParam *out, bool bAdvance);
inline mfxStatus CheckUserData(mfxU8 *ud,mfxU32 len)
{
    for (int i = 0 ; i < len - 2; i++)
    {
        if (ud[0]==0 && ud[1] == 0 && ud[2] == 1)
            return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}
mfxStatus GetMFXPictureType(mfxU8 &FrameType, mfxU8 &FrameType2, bool &bField, bool &bFrame,
                            UMC_VC1_ENCODER::ePType VC1PicType);

typedef mfxStatus  (*pGetPlane_Prog)     (  mfxFrameInfo *pFrameInfo, mfxFrameData *pFrame,
                                            Ipp8u *& pY, Ipp32u& YStep,
                                            Ipp8u *& pU, Ipp8u*& pV,
                                            Ipp32u&  UVStep);

typedef mfxStatus   (*pGetPlane_Field)(  mfxFrameInfo *pFrameInfo, mfxFrameData *pFrame, bool     bBottom,
                                         Ipp8u *& pY, Ipp32u& YStep,
                                         Ipp8u *& pU, Ipp8u*& pV,
                                         Ipp32u&  UVStep);


inline mfxStatus  GetPlane_ProgNV12 (   mfxFrameInfo *pFrameInfo, mfxFrameData *pFrame,
                                        Ipp8u *& pY, Ipp32u& YStep,
                                        Ipp8u *& pU, Ipp8u*& pV,
                                        Ipp32u&  UVStep)
{
    mfxU32 shiftLuma   = GetLumaShift(pFrameInfo,pFrame);
    mfxU32 shiftChroma = GetChromaShiftNV12(pFrameInfo,pFrame);

    if (!pFrame->Y || !pFrame->U || !pFrame->V)
        return MFX_ERR_NULL_PTR;

    pY       = pFrame->Y + shiftLuma;
    pU       = pFrame->U + shiftChroma;
    pV       = pFrame->V + shiftChroma;
    YStep    = pFrame->Pitch;
    UVStep   = pFrame->Pitch;
    return MFX_ERR_NONE;
}
inline mfxStatus  GetPlane_ProgYV12 (   mfxFrameInfo *pFrameInfo, mfxFrameData *pFrame,
                                        Ipp8u *& pY, Ipp32u& YStep,
                                        Ipp8u *& pU, Ipp8u*& pV,
                                        Ipp32u&  UVStep)
{
    mfxU32 shiftLuma   = GetLumaShift(pFrameInfo,pFrame);
    mfxU32 shiftChroma = GetChromaShiftYV12(pFrameInfo,pFrame);
    return MFX_ERR_NONE;
}
inline mfxStatus  GetPlane_ProgYV12 (   mfxFrameInfo *pFrameInfo, mfxFrameData *pFrame,
                                        Ipp8u *& pY, Ipp32u& YStep,
                                        Ipp8u *& pU, Ipp8u*& pV,
                                        Ipp32u&  UVStep)
{
    mfxU32 shiftLuma   = GetLumaShift(pFrameInfo,pFrame);
    mfxU32 shiftChroma = GetChromaShiftYV12(pFrameInfo,pFrame);

    if (!pFrame->Y || !pFrame->U || !pFrame->V)
        return MFX_ERR_NULL_PTR;

    pY       = pFrame->Y + shiftLuma;
    pU       = pFrame->U + shiftChroma;
    pV       = pFrame->V + shiftChroma;
    YStep    = pFrame->Pitch;
    UVStep   = pFrame->Pitch>>1;
    return MFX_ERR_NONE;
}
inline mfxStatus  GetPlane_FieldYV12 (  mfxFrameInfo *pFrameInfo, mfxFrameData *pFrame, bool     bBottom,
                                        Ipp8u *& pY, Ipp32u& YStep,
                                        Ipp8u *& pU, Ipp8u*& pV,
                                        Ipp32u&  UVStep)
{
    mfxU32 shiftLuma   = GetLumaShift(pFrameInfo,pFrame);
    mfxU32 shiftChroma = GetChromaShiftYV12(pFrameInfo,pFrame);

    if (!pFrame->Y || !pFrame->U || !pFrame->V)
        return MFX_ERR_NULL_PTR;

    pY       = pFrame->Y + shiftLuma;
    pU       = pFrame->U + shiftChroma;
    pV       = pFrame->V + shiftChroma;
    YStep    = pFrame->Pitch;
    UVStep   = pFrame->Pitch>>1;

    pY       += ((bBottom)? YStep:0);
    pU       += ((bBottom)? UVStep:0);
    pV       += ((bBottom)? UVStep:0);
    YStep    = YStep<< 1;
    UVStep   = UVStep << 1;

    return MFX_ERR_NONE;
}
inline mfxStatus  GetPlane_FieldNV12 (  mfxFrameInfo *pFrameInfo, mfxFrameData *pFrame, bool     bBottom,
                                        Ipp8u *& pY, Ipp32u& YStep,
                                        Ipp8u *& pU, Ipp8u*& pV,
                                        Ipp32u&  UVStep)
{
    mfxU32 shiftLuma   = GetLumaShift(pFrameInfo,pFrame);
    mfxU32 shiftChroma = GetChromaShiftNV12(pFrameInfo,pFrame);

    if (!pFrame->Y || !pFrame->U || !pFrame->V)
        return MFX_ERR_NULL_PTR;

    pY       = pFrame->Y + shiftLuma;
    pU       = pFrame->U + shiftChroma;
    pV       = pFrame->V + shiftChroma;
    YStep    = pFrame->Pitch;
    UVStep   = pFrame->Pitch;

    pY       += ((bBottom)? YStep:0);
    pU       += ((bBottom)? UVStep:0);
    pV       += ((bBottom)? UVStep:0);
    YStep    = YStep<< 1;
    UVStep   = UVStep << 1;

    return MFX_ERR_NONE;
}
inline void SetIntesityCompensationParameters (mfxFrameParamVC1* pFrameParam, bool bIntensity, Ipp32u scale, Ipp32u shift, bool bBottom = false)
{
    if (!bBottom)
    {
        pFrameParam->LumaScale = scale;
        pFrameParam->LumaShift = shift;
        pFrameParam->IntCompField = (bIntensity)?
            (pFrameParam->IntCompField| UMC_VC1_ENCODER::VC1_ENC_TOP_FIELD) : (pFrameParam->IntCompField & ~UMC_VC1_ENCODER::VC1_ENC_TOP_FIELD);
    }
    else
    {
        pFrameParam->LumaScale2 = scale;
        pFrameParam->LumaShift2 = shift;
        pFrameParam->IntCompField = (bIntensity)?
            (pFrameParam->IntCompField | UMC_VC1_ENCODER::VC1_ENC_BOTTOM_FIELD) : (pFrameParam->IntCompField & ~UMC_VC1_ENCODER::VC1_ENC_BOTTOM_FIELD);
    }
}

inline void IntensityCompChromaYV12 (Ipp8u *pSrcU,Ipp8u *pSrcV, Ipp32u UVStep,IppiSize size, Ipp32s* pLUTUV, const Ipp32s* pIntensityCompensationTbl)
{
    ippiLUT_8u_C1IR(pSrcU,UVStep,size,pLUTUV,pIntensityCompensationTbl,257);
    ippiLUT_8u_C1IR(pSrcV,UVStep,size,pLUTUV,pIntensityCompensationTbl,257);
}
inline void IntensityCompChromaNV12 (Ipp8u *pSrcUV,Ipp8u */*pSrcV*/, Ipp32u UVStep,IppiSize size, Ipp32s* pLUTUV, const Ipp32s* pIntensityCompensationTbl)
{
    size.width = size.width << 1;
    ippiLUT_8u_C1IR(pSrcUV,UVStep,size,pLUTUV,pIntensityCompensationTbl,257);
}
typedef void (*pIntensityCompChroma)(Ipp8u *pSrcU,Ipp8u *pSrcV, Ipp32u UVStep,IppiSize size, Ipp32s* pLUTUV, const Ipp32s* pIntensityCompensationTbl);

#endif //_MFX_VC1_ENC_COMMON_H_
#endif //MFX_ENABLE_VC1_VIDEO_ENCODER
