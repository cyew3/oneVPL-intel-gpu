/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_dec.cpp

\* ****************************************************************************** */
#include "mfx_common.h"
#if defined (MFX_ENABLE_MPEG2_VIDEO_DEC)

#include "mfx_mpeg2_dec.h"
#include "mfx_mpeg2_dec_utils.h"
#include "mfx_mpeg2_dec_common.h"

using namespace MfxMpeg2Dec;

MFXVideoDECMPEG2::MFXVideoDECMPEG2(VideoCORE *core, mfxStatus *sts)
: VideoDEC()
, m_implUMC()
, m_core(core)
{
    *sts = MFX_ERR_NONE;
}

MFXVideoDECMPEG2::~MFXVideoDECMPEG2(void)
{
    Close();
}

mfxStatus MFXVideoDECMPEG2::Init(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);
    return InternalReset(par);
}

mfxStatus MFXVideoDECMPEG2::Reset(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);
    return InternalReset(par);
}

mfxStatus MFXVideoDECMPEG2::Close(void)
{
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);

    if (!in)
    { // configurability mode
        Mpeg2SetConfigurableCommon(*out);
    }
    else
    { // checking mode
        MFX_INTERNAL_CPY(out, in, sizeof(mfxVideoParam));
        Mpeg2CheckConfigurableCommon(*out);
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_vPar;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_fPar;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::GetSliceParam(mfxSliceParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_sPar;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::RunFrameFullDEC(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    for (cuc->SliceId = 0; cuc->SliceId < cuc->NumSlice; cuc->SliceId++)
    {
        mfxStatus mfxRes = RunSliceFullDEC(cuc);
        MFX_CHECK_STS(mfxRes);
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::RunFramePredDEC(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    for (cuc->SliceId = 0; cuc->SliceId < cuc->NumSlice; cuc->SliceId++)
    {
        mfxStatus mfxRes = RunSlicePredDEC(cuc);
        MFX_CHECK_STS(mfxRes);
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::RunFrameIQT(mfxFrameCUC *cuc, mfxU8 scan)
{
    MFX_CHECK_NULL_PTR1(cuc);
    for (cuc->SliceId = 0; cuc->SliceId < cuc->NumSlice; cuc->SliceId++)
    {
        mfxStatus mfxRes = RunSliceIQT(cuc, scan);
        MFX_CHECK_STS(mfxRes);
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::GetFrameRecon(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    for (cuc->SliceId = 0; cuc->SliceId < cuc->NumSlice; cuc->SliceId++)
    {
        mfxStatus mfxRes = GetSliceRecon(cuc);
        MFX_CHECK_STS(mfxRes);
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::RunFrameILDB(mfxFrameCUC*)
{
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::RunSliceFullDEC(mfxFrameCUC *cuc)
{
    mfxStatus mfxRes = RunSlicePredDEC(cuc);
    MFX_CHECK_STS(mfxRes);
    mfxRes = RunSliceIQT(cuc,0);
    MFX_CHECK_STS(mfxRes);
    mfxRes = GetSliceRecon(cuc);
    return mfxRes;
}

mfxStatus MFXVideoDECMPEG2::RunSlicePredDEC(mfxFrameCUC *cuc)
{
    MFXMPEG2_UPDATE_PIC_CNT;
    MFXMPEG2_UPDATE_FRAME_CNT;

    MFX_CHECK_NULL_PTR1(cuc);
    MFX_CHECK_NULL_PTR3(cuc->FrameParam, cuc->MbParam, cuc->MbParam->Mb);

    mfxExtMpeg2Prediction* pred = GetExtBuffer<mfxExtMpeg2Prediction>(*cuc, MFX_CUC_MPEG2_MVDATA);
    MFX_CHECK_NULL_PTR2(pred, pred->Prediction);

    Ipp32s    ScaleCPitch = (cuc->FrameSurface->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV444)?1:0;
    Ipp32s    ScaleCHeight = (cuc->FrameSurface->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV420)?1:0;
    mfxU32 pitchL = pred->Pitch;
    mfxU32 pitchC = pitchL >> ScaleCPitch;
    mfxU32 heightL = cuc->FrameSurface->Info.CropH;
    mfxU32 heightC = heightL >> ScaleCHeight;

    m_fPar = *cuc->FrameParam;
    m_sPar = cuc->SliceParam[cuc->SliceId];

    UMC::Mpeg2MbContext blkSel;
    blkSel.ResetSequenceLayer(heightL, pitchL, pitchC, m_fPar.MPEG2.ChromaFormatIdc);
    blkSel.ResetPictureLayer(
        GetUmcPicCodingType(m_fPar.MPEG2.FrameType, m_fPar.MPEG2.BottomFieldFlag),
        GetUmcPicStruct(m_fPar.MPEG2.FieldPicFlag, m_fPar.MPEG2.BottomFieldFlag),
        m_fPar.MPEG2.SecondFieldFlag);

    Ipp8u* target[3] = {
        pred->Prediction,
        pred->Prediction + pitchL * heightL,
        pred->Prediction + pitchL * heightL + pitchC * heightC
    };

    const Ipp8u* fwd[3];
    const Ipp8u* bwdOrCur[3];
    InitFirstRefFrame(fwd, *cuc);
    InitSecondRefFrame(bwdOrCur, *cuc);

    mfxU32 label = cuc->FrameParam->MPEG2.RefFrameListP[0];
    if (label != 0xff && cuc->FrameSurface->Data[label]->Pitch != pitchL)
        return MFX_ERR_UNSUPPORTED;
    label = cuc->FrameParam->MPEG2.RefFrameListB[1][0];
    if (label != 0xff && cuc->FrameSurface->Data[label]->Pitch != pitchL)
        return MFX_ERR_UNSUPPORTED;

    UMC::Mpeg2Mb mb;
    const mfxMbCode* pMfxMbBase = GetFirstMfxMbCode(*cuc);
    for (mfxU32 i = 0; i < m_sPar.MPEG2.NumMb; i++)
    {
        const mfxMbCode &mfxMb = pMfxMbBase[i];
        if (mfxMb.MPEG2.IntraMbFlag)
        {
            mfxU8* dst0 = target[0] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 0);
            mfxU8* dst1 = target[1] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 1);
            mfxU8* dst2 = target[2] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 1);
            mfxU32 pitchL = blkSel.PitchL() * (m_fPar.MPEG2.FieldPicFlag ? 2 : 1);
            mfxU32 pitchC = blkSel.PitchC() * (m_fPar.MPEG2.FieldPicFlag ? 2 : 1);

            for (mfxU32 y = 0; y < 8; y++, dst0 += 2 * pitchL, dst1 += pitchC, dst2 += pitchC)
            {
                memset(dst0, 0, 16);
                memset(dst0 + pitchL, 0, 16);
                memset(dst1, 0, 8);
                memset(dst2, 0, 8);
            }
        }
        else
        {
        //MFXMPEG2_SET_BREAKPOINT(m_frameCnt == 6 && mfxMb.MbXcnt == 0 && mfxMb.MbYcnt == 26);

            PopulateUmcMb(mb, mfxMb);
            m_fPar.MPEG2.FieldPicFlag ?
                m_implUMC.DoMotionPredictionField(mb, blkSel, fwd, bwdOrCur, target) :
                m_implUMC.DoMotionPredictionFrame(mb, blkSel, fwd, bwdOrCur, target);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::RunSliceIQT(mfxFrameCUC *cuc, mfxU8 /*scan*/)
{
    MFX_CHECK_NULL_PTR3(cuc, cuc->MbParam, cuc->MbParam->Mb);

    mfxExtMpeg2Coeffs* coeff = GetExtBuffer<mfxExtMpeg2Coeffs>(*cuc, MFX_CUC_MPEG2_RESPIXEL);
    MFX_CHECK_NULL_PTR2(coeff, coeff->Coeffs);
    mfxExtMpeg2Residual* resid = GetExtBuffer<mfxExtMpeg2Residual>(*cuc, MFX_CUC_MPEG2_RESPIXEL);
    MFX_CHECK_NULL_PTR2(resid, resid->Residual);
    mfxExtMpeg2QMatrix* qMatrix = GetExtBuffer<mfxExtMpeg2QMatrix>(*cuc, MFX_CUC_MPEG2_QMATRIX);
    MFX_CHECK_NULL_PTR1(qMatrix);
    const mfxFrameInfo& surfaceInfo = cuc->FrameSurface->Info;
    mfxU32 pitchL = resid->Pitch;

    Ipp32s    ScaleCPitch = (surfaceInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)?1:0;
    Ipp32s    ScaleCHeight = (surfaceInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420)?1:0;

    mfxU32 pitchC = pitchL >> ScaleCPitch;
    mfxU32 heightL = surfaceInfo.CropH;
    mfxU32 heightC = heightL >> ScaleCHeight;

    m_fPar = *cuc->FrameParam;
    m_sPar = cuc->SliceParam[cuc->SliceId];
    UMC::Mpeg2MbContext blkSel;
    blkSel.ResetSequenceLayer(heightL, pitchL, pitchC, m_fPar.MPEG2.ChromaFormatIdc);
    blkSel.ResetPictureLayer(
        GetUmcPicCodingType(m_fPar.MPEG2.FrameType, m_fPar.MPEG2.BottomFieldFlag),
        GetUmcPicStruct(m_fPar.MPEG2.FieldPicFlag, m_fPar.MPEG2.BottomFieldFlag),
        m_fPar.MPEG2.SecondFieldFlag);

    mfxI16* planes[3] = {
        resid->Residual,
        resid->Residual + heightL * pitchL,
        resid->Residual + heightL * pitchL + heightC * pitchC
    };

    UMC::Mpeg2Mb mb;
    const mfxMbCode* pMfxMbBase = GetFirstMfxMbCode(*cuc);
    for (mfxU32 i = 0; i < m_sPar.MPEG2.NumMb; i++)
    {
        const mfxMbCode &mfxMb = pMfxMbBase[i];
        PopulateUmcMb(mb, mfxMb);
        mb.dctCoef = GetDctCoefPtr(coeff->Coeffs, mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, m_fPar);

        //MFXMPEG2_SET_BREAKPOINT(m_frameCnt == 6 && mfxMb.MbXcnt == 0 && mfxMb.MbYcnt == 26);
        UMC::Status umcRes = mb.IsIntra() ?
            m_implUMC.DoDequantAndIdctIntra(
                mb,
                mfxMb.MPEG2.MbScanMethod,
                mfxMb.MPEG2.QpScaleType,
                m_fPar.MPEG2.IntraDCprecision,
                blkSel,
                qMatrix->IntraQMatrix,
                planes) :
            m_implUMC.DoDequantAndIdctInter(
                mb,
                mfxMb.MPEG2.MbScanMethod,
                mfxMb.MPEG2.QpScaleType,
                blkSel,
                qMatrix->InterQMatrix,
                planes);
        MFX_CHECK_UMC_STS(umcRes);
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::RunSliceILDB(mfxFrameCUC*)
{
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::GetSliceRecon(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    MFX_CHECK_NULL_PTR3(cuc->FrameParam, cuc->SliceParam, cuc->FrameSurface);
    if (cuc->FrameParam->MPEG2.CurrFrameLabel >= cuc->FrameSurface->NumFrameData)
        return MFX_ERR_NULL_PTR;
    MFX_CHECK_NULL_PTR2(cuc->FrameSurface->Data, cuc->FrameSurface->Data[cuc->FrameParam->MPEG2.CurrFrameLabel]);
    FrameLocker locker(*m_core, *cuc->FrameSurface->Data[cuc->FrameParam->MPEG2.CurrFrameLabel]);
    MFX_CHECK_NULL_PTR3(cuc->FrameSurface->Data[0]->Y, cuc->FrameSurface->Data[0]->U, cuc->FrameSurface->Data[0]->V);

    mfxExtMpeg2Prediction* predict = GetExtBuffer<mfxExtMpeg2Prediction>(*cuc, MFX_CUC_MPEG2_MVDATA);
    MFX_CHECK_NULL_PTR2(predict, predict->Prediction);
    mfxExtMpeg2Residual* resid = GetExtBuffer<mfxExtMpeg2Residual>(*cuc, MFX_CUC_MPEG2_RESPIXEL);
    MFX_CHECK_NULL_PTR2(resid, resid->Residual);

    m_fPar = *cuc->FrameParam;
    m_sPar = cuc->SliceParam[cuc->SliceId];

    Ipp32s    ScaleCHeight = (cuc->FrameSurface->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV420)?1:0;
    Ipp32s    ScaleCPitch = (cuc->FrameSurface->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV444)?1:0;

    mfxU32 pitchL = predict->Pitch;
    mfxU32 pitchC = pitchL >> ScaleCPitch;
    mfxU32 heightL = cuc->FrameSurface->Info.CropH;
    mfxU32 heightC = heightL >> ScaleCHeight;

    if (pitchL != resid->Pitch || pitchL != cuc->FrameSurface->Data[cuc->FrameParam->MPEG2.CurrFrameLabel]->Pitch)
        return MFX_ERR_UNSUPPORTED;

    mfxU8* cur[3] = {
        GetPlane(*cuc->FrameSurface, cuc->FrameParam->MPEG2.CurrFrameLabel, 0),
        GetPlane(*cuc->FrameSurface, cuc->FrameParam->MPEG2.CurrFrameLabel, 1),
        GetPlane(*cuc->FrameSurface, cuc->FrameParam->MPEG2.CurrFrameLabel, 2)
    };

    mfxU8* pred[3] = {
        predict->Prediction,
        pred[0] + pitchL * heightL,
        pred[1] + pitchC * heightC
    };

    mfxI16* res[3] = {
        resid->Residual,
        res[0] + pitchL * heightL,
        res[1] + pitchC * heightC
    };

    UMC::Mpeg2MbContext blkSel;
    blkSel.ResetSequenceLayer(heightL, pitchL, pitchC, m_fPar.MPEG2.ChromaFormatIdc);
    blkSel.ResetPictureLayer(
        GetUmcPicCodingType(m_fPar.MPEG2.FrameType, m_fPar.MPEG2.BottomFieldFlag),
        GetUmcPicStruct(m_fPar.MPEG2.FieldPicFlag, m_fPar.MPEG2.BottomFieldFlag),
        m_fPar.MPEG2.SecondFieldFlag);

    mfxU8 mbWidthC = blkSel.MbWidth(1);
    mfxU8 mbHeightC = blkSel.MbHeight(1);
    pitchL = blkSel.MbPitchL();
    pitchC = blkSel.MbPitchC();

    const mfxMbCode* pMfxMbBase = GetFirstMfxMbCode(*cuc);
    for (mfxU32 i = 0; i < m_sPar.MPEG2.NumMb; i++)
    {
        const mfxMbCode& mfxMb = pMfxMbBase[i];
        mfxU8* dst0 = cur[0] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 0);
        mfxU8* dst1 = cur[1] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 1);
        mfxU8* dst2 = cur[2] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 1);
        mfxU8* srcPred0 = pred[0] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 0);
        mfxU8* srcPred1 = pred[1] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 1);
        mfxU8* srcPred2 = pred[2] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 1);
        mfxI16* srcRes0 = res[0] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 0);
        mfxI16* srcRes1 = res[1] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 1);
        mfxI16* srcRes2 = res[2] + blkSel.MbOffset(mfxMb.MPEG2.MbXcnt, mfxMb.MPEG2.MbYcnt, 1);

        //MFXMPEG2_SET_BREAKPOINT(m_frameCnt == 6 && mfxMb.MbXcnt == 0 && mfxMb.MbYcnt == 26);
        AddPredAndResidual(dst0, pitchL, srcPred0, pitchL, srcRes0, pitchL, 16, 16);
        AddPredAndResidual(dst1, pitchC, srcPred1, pitchC, srcRes1, pitchC, mbWidthC, mbHeightC);
        AddPredAndResidual(dst2, pitchC, srcPred2, pitchC, srcRes2, pitchC, mbWidthC, mbHeightC);
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECMPEG2::InternalReset(mfxVideoParam* par)
{
    m_vPar = *par;
    ClearMfxStruct(&m_fPar);
    ClearMfxStruct(&m_sPar);
    MFXMPEG2_RESET_ALL;
    return MFX_ERR_NONE;
}

void MFXVideoDECMPEG2::PopulateUmcMb(UMC::Mpeg2Mb& mb, const mfxMbCode& mfxMb)
{
    mb.coded_block_pattern = GetUmcCodedBlockPattern(
        m_fPar.MPEG2.ChromaFormatIdc,
        mfxMb.MPEG2.CodedPattern4x4Y,
        mfxMb.MPEG2.CodedPattern4x4U,
        mfxMb.MPEG2.CodedPattern4x4V);

    mb.skipped = mfxMb.MPEG2.Skip8x8Flag;
    mb.mbX = mfxMb.MPEG2.MbXcnt;
    mb.mbY = mfxMb.MPEG2.MbYcnt;
    mb.macroblock_type = GetUmcMbType(mfxMb.MPEG2.MbType5Bits, mfxMb.MPEG2.Skip8x8Flag);
    mb.motion_type = GetUmcMotionType(mfxMb.MPEG2.MbType5Bits, m_fPar.MPEG2.FieldPicFlag);
    mb.dct_type = GetUmcDctType(m_fPar.MPEG2.FieldPicFlag, mfxMb.MPEG2.TransformFlag);
    mb.quantiser_scale_code = mfxMb.MPEG2.QpScaleCode;

   // mb.field_select[UMC::Mpeg2MvR0Fwd] = mfxMb.RefPicSelect[0][0];
   // mb.field_select[UMC::Mpeg2MvR1Fwd] = mfxMb.RefPicSelect[0][1];
  //  mb.field_select[UMC::Mpeg2MvR0Bwd] = mfxMb.RefPicSelect[1][0];
   // mb.field_select[UMC::Mpeg2MvR1Bwd] = mfxMb.RefPicSelect[1][1];

    mb.pmv[UMC::Mpeg2MvR0Fwd].x = mfxMb.MPEG2.MV[0][0];
    mb.pmv[UMC::Mpeg2MvR0Fwd].y = mfxMb.MPEG2.MV[0][1];
    mb.pmv[UMC::Mpeg2MvR1Fwd].x = mfxMb.MPEG2.MV[1][0];
    mb.pmv[UMC::Mpeg2MvR1Fwd].y = mfxMb.MPEG2.MV[1][1];
    mb.mv [UMC::Mpeg2MvR0Fwd].x = 0;//mfxMb.MV[2][0];
    mb.mv [UMC::Mpeg2MvR0Fwd].y = 0;//mfxMb.MV[2][1];
    mb.mv [UMC::Mpeg2MvR1Fwd].x = 0;//mfxMb.MV[3][0];
    mb.mv [UMC::Mpeg2MvR1Fwd].y = 0;//mfxMb.MV[3][1];

    mb.pmv[UMC::Mpeg2MvR0Bwd].x = mfxMb.MPEG2.MV[4][0];
    mb.pmv[UMC::Mpeg2MvR0Bwd].y = mfxMb.MPEG2.MV[4][1];
    mb.pmv[UMC::Mpeg2MvR1Bwd].x = mfxMb.MPEG2.MV[5][0];
    mb.pmv[UMC::Mpeg2MvR1Bwd].y = mfxMb.MPEG2.MV[5][1];
    mb.mv [UMC::Mpeg2MvR0Bwd].x = 0;//mfxMb.MV[6][0];
    mb.mv [UMC::Mpeg2MvR0Bwd].y = 0;//mfxMb.MV[6][1];
    mb.mv [UMC::Mpeg2MvR1Bwd].x = 0;//mfxMb.MV[7][0];
    mb.mv [UMC::Mpeg2MvR1Bwd].y = 0;//mfxMb.MV[7][1];

    if (mb.motion_type == IPPVC_MC_DP)
    {
        m_implUMC.DualPrimeArithmetic(
            mb.pmv[0],
            mb.pmv[1],
            GetUmcPicStruct(m_fPar.MPEG2.FieldPicFlag, m_fPar.MPEG2.BottomFieldFlag),
            m_fPar.MPEG2.TopFieldFirst,
            mb.mv);
    }
}

#endif
