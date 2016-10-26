//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_vc1_enc_defs.h"

#if defined (MFX_ENABLE_VC1_VIDEO_ENCODER)

#include "mfx_vc1_enc_enc.h"
#include "umc_vc1_enc_tables.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_tables.h"


MFXVideoEncVc1* CreateMFXVideoEncVc1(VideoCORE *core, mfxStatus *status)
{
    return (MFXVideoEncVc1*)new MFXVideoEncVc1(core, status);
}

MFXVideoEncVc1::~MFXVideoEncVc1(void)
{
    Close();
}

#define ALIGN32(X) (((mfxU32)((X)+31)) & (~ (mfxU32)31))

mfxStatus MFXVideoEncVc1::Init(mfxVideoParam *params)
{
    mfxStatus           MFXSts  = MFX_ERR_NONE;
    UMC::Status         err     = UMC::UMC_OK;
    Ipp32s              memSize = 0;
    Ipp8u*              pBuffer = 0;

    UMC::MeInitParams      MEParamsInit;
    UMC::VC1EncoderParams  UMCParams;

    bool                bVSTransform = false;
    bool                deblocking   = false;
    mfxU8               smoothing    = 0;
    bool                fastUVMC     = false;
    bool bNV12 = (params->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12);


    if(!m_pMFXCore || !params)
        return MFX_ERR_MEMORY_ALLOC;

    MFX_CHECK_COND(params->mfx.FrameInfo.Step == 1);
    MFX_CHECK_COND(params->mfx.FrameInfo.ScaleCHeight   == 1);
    MFX_CHECK_COND(params->mfx.FrameInfo.ScaleCWidth    == 1);
    //MFX_CHECK_COND(params->mfx.FrameInfo.ScaleCPitch    == 1);
    //MFX_CHECK_COND(params->mfx.FrameInfo.DeltaCStep     == 0);

    MFX_CHECK_COND(params->mfx.FrameInfo.Width <= 4080);
    MFX_CHECK_COND(params->mfx.FrameInfo.Height <= 4080);

    MFX_CHECK_COND(params->mfx.FrameInfo.CropX + params->mfx.FrameInfo.CropW
        <= params->mfx.FrameInfo.Width);
    MFX_CHECK_COND(params->mfx.FrameInfo.CropY + params->mfx.FrameInfo.CropH
        <= params->mfx.FrameInfo.Height);

    mfxU16 w = params->mfx.FrameInfo.Width;
    mfxU16 h = params->mfx.FrameInfo.Height;

    GetRealFrameSize(&params->mfx, w, h);

    Ipp32u   wMB = (w + 15)/16;
    Ipp32u   hMB = ((h/2 + 15)/16)*2;

    Close ();

    m_profile = params->mfx.CodecProfile;

    m_pPadChromaFunc     = (bNV12)? ReplicateBorderChroma_NV12 : ReplicateBorderChroma_YV12;
    GetChromaShift       = (bNV12)? GetChromaShiftNV12:GetChromaShiftYV12;
    SetFrame             = (bNV12)? SetFrameNV12 : SetFrameYV12;
    CopyFrame            = (bNV12)? copyFrameNV12:copyFrameYV12;
    CopyField            = (bNV12)? copyFieldNV12:copyFieldYV12;
    GetPlane_Prog        = (bNV12)? GetPlane_ProgNV12:GetPlane_ProgYV12;
    GetPlane_Field       = (bNV12)? GetPlane_FieldNV12:GetPlane_FieldYV12;
    PadFrameProgressive  = (bNV12)? PadFrameProgressiveNV12:PadFrameProgressiveYV12;
    PadFrameField        = (bNV12)? PadFrameFieldNV12:PadFrameFieldYV12;
    IntensityCompChroma  = (bNV12)? IntensityCompChromaNV12:IntensityCompChromaYV12;


    /*-------------- create UMC ME object -----------------------*/

    MFXSts = m_pMFXCore->AllocBuffer(sizeof(UMC::MeVC1), MFX_MEMTYPE_PERSISTENT_MEMORY, &m_MEObjID);

    if(MFXSts != MFX_ERR_NONE || !m_MEObjID)
       return MFX_ERR_MEMORY_ALLOC;

    MFXSts = m_pMFXCore->LockBuffer(m_MEObjID, &pBuffer);
    if(MFXSts != MFX_ERR_NONE)
       return MFX_ERR_MEMORY_ALLOC;

    m_pME = new (pBuffer) UMC::MeVC1 ;
    if (!m_pME)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
   /*--------------                        -----------------------*/
    if ((params->mfx.TargetUsage & 0x0F) == MFX_TARGETUSAGE_UNKNOWN)
    {
        params->mfx.TargetUsage |= 3;
    }

    /*
    Those parameters must be set using target usage or extended buffer */

    SetPROParameters (params->mfx.TargetUsage & 0x0F,
                      m_MESpeed, m_UseFB,m_FastFB, m_bIntensityCompensation,
                      m_bChangeInterpolationType, m_bChangeVLCTables,
                      m_bTrellisQuantization,m_bUsePadding,
                      bVSTransform,deblocking,smoothing,fastUVMC);

    ExtVC1SequenceParams *pSH_Ex = (ExtVC1SequenceParams *)GetExBufferIndex(params, MFX_CUC_VC1_SEQPARAMSBUF);
    if (pSH_Ex)
    {
        m_MESpeed                 =  pSH_Ex->MESpeed;
        m_UseFB                   =  pSH_Ex->useFB;
        m_FastFB                  =  pSH_Ex->fastFB;
        m_bIntensityCompensation  =  pSH_Ex->intensityCompensation;
        m_bChangeInterpolationType=  pSH_Ex->changeInterpolationType;
        m_bChangeVLCTables        =  pSH_Ex->changeVLCTables;
        m_bTrellisQuantization    =  pSH_Ex->trellisQuantization;

        bVSTransform = pSH_Ex->vsTransform;
        deblocking   = pSH_Ex->deblocking;
        smoothing    = pSH_Ex->overlapSmoothing;
        fastUVMC     = !pSH_Ex->noFastUV;
    }

     // Init UMC ME Parameters
    {
        memset(&MEParamsInit,0,sizeof(MEParamsInit));

        MEParamsInit.WidthMB         = wMB;
        MEParamsInit.HeightMB        = hMB;
        MEParamsInit.refPadding      = 32;
        MEParamsInit.MbPart          = UMC::ME_Mb16x16;
        MEParamsInit.bNV12           = bNV12;
        MEParamsInit.SearchDirection = (params->mfx.GopRefDist > 1)? UMC::ME_BidirSearch:UMC::ME_ForwardSearch;

        m_picture_width  = w;
        m_picture_height = h;

        MEParamsInit.UseStatistics   = false;
        MEParamsInit.UseDownSampling = true;

        MEParamsInit.MaxNumOfFrame = 6; // 2 for backward, 2 for forward, 2 for current

        m_numMEFrames = MEParamsInit.MaxNumOfFrame;

        assert (m_numMEFrames < VC1_MFX_MAX_FRAMES_FOR_UMC_ME);

        if(!m_pME->Init(&MEParamsInit, NULL, memSize))
               return MFX_ERR_MEMORY_ALLOC;

        MFXSts = m_pMFXCore->AllocBuffer(memSize, MFX_MEMTYPE_PERSISTENT_MEMORY, &m_MEID);

        if(MFXSts != MFX_ERR_NONE || !m_MEID)
                return MFX_ERR_MEMORY_ALLOC;


        MFXSts = m_pMFXCore->LockBuffer(m_MEID, &pBuffer);
        if(MFXSts != MFX_ERR_NONE || !pBuffer)
           return  MFX_ERR_MEMORY_ALLOC;

        if(!m_pME->Init(&MEParamsInit, pBuffer, memSize))
           return MFX_ERR_MEMORY_ALLOC;

        for(Ipp32s i = 0; i < m_numMEFrames; i++)
        {
           m_MeFrame[i] = &MEParamsInit.pFrames[i];
        }
    }

    MFX_INTERNAL_CPY(&m_VideoParam, params, sizeof(mfxVideoParam));

    /*init UMC parameters. It's needed for SH initialization. SH is used in FULL ENC profile*/
    {
        UMCParams.info.clip_info.width    =  w;
        UMCParams.info.clip_info.height   =  h;
        UMCParams.m_bInterlace            =  (m_VideoParam.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE);

        MFXSts = GetUMCProfile(m_VideoParam.mfx.CodecProfile,&UMCParams.profile);
        MFX_CHECK_STS (MFXSts)
        MFXSts = GetUMCLevel(m_VideoParam.mfx.CodecProfile, m_VideoParam.mfx.CodecLevel, &UMCParams.level);
        MFX_CHECK_STS (MFXSts)

        UMCParams.m_uiGOPLength = m_VideoParam.mfx.GopPicSize;
        UMCParams.m_uiBFrmLength = m_VideoParam.mfx.GopRefDist - 1;

        UMCParams.m_bVSTransform           =  bVSTransform;
        UMCParams.m_bDeblocking            =  deblocking;
        UMCParams.m_uiOverlapSmoothing     =  smoothing;
        UMCParams.m_bFastUVMC              =  fastUVMC ;
    }

    //Advance profile
    if(m_profile == MFX_PROFILE_VC1_ADVANCED)
    {
           //sequence header memory allocation
           memSize = sizeof(UMC_VC1_ENCODER::VC1EncoderSequenceADV);
           MFXSts = m_pMFXCore->AllocBuffer(memSize, MFX_VC1_SH_MEMORY, &m_SeqID);
           if(MFXSts != MFX_ERR_NONE || !m_SeqID)
               return MFX_ERR_MEMORY_ALLOC;

           MFXSts = m_pMFXCore->LockBuffer(m_SeqID, &pBuffer);
           if(MFXSts != MFX_ERR_NONE || !pBuffer)
             return MFX_ERR_MEMORY_ALLOC;

           m_pVC1SequenceAdv = new (pBuffer) UMC_VC1_ENCODER::VC1EncoderSequenceADV;

           //picture memory allocation
           memSize = sizeof(UMC_VC1_ENCODER::VC1EncoderPictureADV);
           MFXSts = m_pMFXCore->AllocBuffer(memSize, MFX_VC1_PICTURE_MEMORY, &m_PicID);
           if(MFXSts != MFX_ERR_NONE || !m_PicID)
               return MFX_ERR_MEMORY_ALLOC;

           MFXSts = m_pMFXCore->LockBuffer(m_PicID, &pBuffer);
           if(MFXSts != MFX_ERR_NONE || !pBuffer)
               return MFX_ERR_MEMORY_ALLOC;

           m_pVC1PictureAdv  = new (pBuffer) UMC_VC1_ENCODER::VC1EncoderPictureADV(bNV12);

           if (!m_pVC1SequenceAdv || !m_pVC1PictureAdv)
           {
                return MFX_ERR_MEMORY_ALLOC;
           }

           err = m_pVC1SequenceAdv->Init(&UMCParams);
           UMC_MFX_CHECK_STS(err)

           //MFXSts = CheckCucSeqHeaderAdv(m_pSH_Ex, m_pVC1SequenceAdv);
           //MFX_CHECK_STS(MFXSts);
    }
    else
    {
           //simple profile
           //sequence header memory allocation
           memSize = sizeof(UMC_VC1_ENCODER::VC1EncoderSequenceSM);
           MFXSts = m_pMFXCore->AllocBuffer(memSize, MFX_VC1_SH_MEMORY, &m_SeqID);
           if(MFXSts != MFX_ERR_NONE || !m_SeqID)
               return MFX_ERR_MEMORY_ALLOC;

           MFXSts = m_pMFXCore->LockBuffer(m_SeqID, &pBuffer);
           if(MFXSts != MFX_ERR_NONE ||!pBuffer)
               return MFX_ERR_MEMORY_ALLOC;

           m_pVC1SequenceSM = new (pBuffer) UMC_VC1_ENCODER::VC1EncoderSequenceSM;

           //picture memory allocation
           memSize = sizeof(UMC_VC1_ENCODER::VC1EncoderPictureSM);
           MFXSts = m_pMFXCore->AllocBuffer(memSize, MFX_VC1_PICTURE_MEMORY, &m_PicID);
           if(MFXSts != MFX_ERR_NONE || !m_PicID)
               return MFX_ERR_MEMORY_ALLOC;

           MFXSts = m_pMFXCore->LockBuffer(m_PicID, &pBuffer);
           if(MFXSts != MFX_ERR_NONE || !pBuffer)
               return MFX_ERR_MEMORY_ALLOC;

           m_pVC1PictureSM  = new (pBuffer) UMC_VC1_ENCODER::VC1EncoderPictureSM(bNV12);

           if (!m_pVC1SequenceSM || !m_pVC1PictureSM)
           {
                return MFX_ERR_MEMORY_ALLOC;
           }
           err = m_pVC1SequenceSM->Init(&UMCParams);
           UMC_MFX_CHECK_STS(err)

           //MFXSts = CheckCucSeqHeaderSM(m_pSH_Ex, m_pVC1SequenceSM);
           //MFX_CHECK_STS(MFXSts);
    }

    //coded MB memory allocation
    memSize = sizeof(UMC_VC1_ENCODER::VC1EncoderCodedMB)*wMB*hMB;
    MFXSts = m_pMFXCore->AllocBuffer(memSize, MFX_VC1_CODED_MB_MEMORY, &m_CodedMBID);
    if(MFXSts != MFX_ERR_NONE || !m_CodedMBID)
        return MFX_ERR_MEMORY_ALLOC;

    MFXSts = m_pMFXCore->LockBuffer(m_CodedMBID, &pBuffer);
    if(MFXSts != MFX_ERR_NONE || !pBuffer)
        return MFX_ERR_MEMORY_ALLOC;

    m_pVC1EncoderCodedMB  = new  (pBuffer) UMC_VC1_ENCODER::VC1EncoderCodedMB[w*h];
    if (!m_pVC1EncoderCodedMB)
         return MFX_ERR_MEMORY_ALLOC;

    //MB memory allocation
    memSize = sizeof(UMC_VC1_ENCODER::VC1EncoderMBs)*wMB*2;
    MFXSts = m_pMFXCore->AllocBuffer(memSize, MFX_VC1_MB_MEMORY,&m_MBID);
    if(MFXSts != MFX_ERR_NONE || !m_MBID)
        return MFX_ERR_MEMORY_ALLOC;

    MFXSts = m_pMFXCore->LockBuffer(m_MBID, &pBuffer);
    if(MFXSts != MFX_ERR_NONE ||!pBuffer)
        return MFX_ERR_MEMORY_ALLOC;

    m_pVC1EncoderMBs  = new  (pBuffer) UMC_VC1_ENCODER::VC1EncoderMBs[wMB*2];
    if (!m_pVC1EncoderMBs)
         return MFX_ERR_MEMORY_ALLOC;

    memSize = m_pVC1EncoderMBs->CalcAllocMemorySize(wMB,hMB,bNV12);

    MFXSts = m_pMFXCore->AllocBuffer(memSize, MFX_VC1_MB_MEMORY, &m_MBsID);
    if(MFXSts != MFX_ERR_NONE || !m_MBsID)
        return MFX_ERR_MEMORY_ALLOC;

    MFXSts = m_pMFXCore->LockBuffer(m_MBsID, &pBuffer);
    if(MFXSts != MFX_ERR_NONE ||!pBuffer)
        return MFX_ERR_MEMORY_ALLOC;

    err = m_pVC1EncoderMBs->Init(pBuffer, memSize,wMB,hMB,bNV12);
    UMC_MFX_CHECK_STS(err)

    if(m_profile == MFX_PROFILE_VC1_ADVANCED)
    {
          err = m_pVC1PictureAdv->Init(m_pVC1SequenceAdv,m_pVC1EncoderMBs,m_pVC1EncoderCodedMB);
          UMC_MFX_CHECK_STS(err)
    }
    else
    {
          err = m_pVC1PictureSM->Init(m_pVC1SequenceSM,m_pVC1EncoderMBs,m_pVC1EncoderCodedMB);
          UMC_MFX_CHECK_STS(err)
    }

    /*-------------- Create additional frames with padding -----------------------------*/
    {
        memset(&m_ExFrameData,    0, sizeof(mfxFrameData)*N_EX_FRAMES);

        memcpy (&m_ExFrameSurface.Info, &m_VideoParam.mfx.FrameInfo, sizeof (mfxFrameInfo));

        m_ExFrameSurface.Info.CropW     = w;
        m_ExFrameSurface.Info.CropH     = h;

        m_ExFrameSurface.Info.CropX     = MFX_VC1_ENCODER_PADDING_SIZE;
        m_ExFrameSurface.Info.CropY     = MFX_VC1_ENCODER_PADDING_SIZE;
        m_ExFrameSurface.Info.Width     = w + (MFX_VC1_ENCODER_PADDING_SIZE<<1);
        m_ExFrameSurface.Info.Height    = h + (MFX_VC1_ENCODER_PADDING_SIZE<<1);
        mfxU32 Pitch                    = ALIGN32(m_ExFrameSurface.Info.Width);
        mfxU32 Height2                  = ALIGN32(m_ExFrameSurface.Info.Height);
        mfxU32 nbytes                   = Pitch*Height2 + (Pitch>>1)*(Height2>>1) + (Pitch>>1)*(Height2>>1);

        for (int i = 0 ; i <N_EX_FRAMES ; i++)
        {
            MFXSts = m_pMFXCore->AllocBuffer(nbytes, MFX_VC1_CODED_MB_MEMORY, &m_ExFrameID[i]);
            if(MFXSts != MFX_ERR_NONE || !m_ExFrameID[i])
                return MFX_ERR_MEMORY_ALLOC;

            MFXSts = m_pMFXCore->LockBuffer(m_ExFrameID[i], &pBuffer);
            if(MFXSts != MFX_ERR_NONE ||!pBuffer)
                return MFX_ERR_MEMORY_ALLOC;

            m_ExFrameData[i].Pitch  = Pitch;
            if (bNV12)
            {
                m_ExFrameData[i].Y = pBuffer;
                m_ExFrameData[i].U = m_ExFrameData[i].Y + Pitch*Height2;
                m_ExFrameData[i].V = m_ExFrameData[i].U + (Pitch>>1)*(Height2>>1);
            }
            else
            {
                m_ExFrameData[i].Y = pBuffer;
                m_ExFrameData[i].U = m_ExFrameData[i].Y + Pitch*Height2;
                m_ExFrameData[i].V = m_ExFrameData[i].U + 1;
            }

            m_ExFrames[i] = &m_ExFrameData[i];
        }
        m_ExFrameSurface.Data = m_ExFrames;
        m_ExFrameSurface.Info.NumFrameData = N_EX_FRAMES;

    }

    m_InitFlag = 1;

#ifdef VC1_ENC_DEBUG_ON
    if(!UMC_VC1_ENCODER::pDebug)
    {
        UMC_VC1_ENCODER::pDebug = new UMC_VC1_ENCODER::VC1EncDebug;
        assert(UMC_VC1_ENCODER::pDebug != NULL);
        UMC_VC1_ENCODER::pDebug->Init(wMB, hMB, bNV12);
    }
#endif

    for (Ipp32s i = 0; i<VC1_MFX_MAX_FRAMES_FOR_UMC_ME; i++)
    {
        m_pMEInfo[i].m_MFXFrameOrder = 0xffff;
    }
    return MFXSts;
}

mfxStatus MFXVideoEncVc1::Reset(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxVideoParam * pOldParams = &m_VideoParam;
    mfxVideoParam * pNewParams = par;

    MFX_CHECK_NULL_PTR1(par);

    MFX_CHECK_COND(pNewParams->mfx.FrameInfo.CropX + pNewParams->mfx.FrameInfo.CropW
        <= pNewParams->mfx.FrameInfo.Width);
    MFX_CHECK_COND(pNewParams->mfx.FrameInfo.CropY + pNewParams->mfx.FrameInfo.CropH
        <= pNewParams->mfx.FrameInfo.Height);

    MFX_CHECK_COND(pNewParams->mfx.FrameInfo.Width <= 4080);
    MFX_CHECK_COND(pNewParams->mfx.FrameInfo.Height <= 4080);

    if (pOldParams->mfx.FrameInfo.Width  < pNewParams->mfx.FrameInfo.Width  ||
        pOldParams->mfx.FrameInfo.Height < pNewParams->mfx.FrameInfo.Height ||
        (pOldParams->mfx.FramePicture && pNewParams->mfx.FramePicture)     ||
        (pOldParams->mfx.GopPicSize <2 && pNewParams->mfx.GopPicSize>=2)    ||
        (pOldParams->mfx.GopRefDist <2 && pNewParams->mfx.GopRefDist>=2))
    {

        Close();
        sts = Init(par);
    }
    else
    {
        // temp solution: need to change umc part for reset without memory deleting and allocation
        Close();
        sts = Init(par);
    }

    return sts;
}
mfxStatus MFXVideoEncVc1::Close(void)
{
    mfxStatus  MFXSts = MFX_ERR_NONE;

    m_InitFlag = 0;

    if(m_MEID)
    {
        m_pMFXCore->UnlockBuffer(m_MEID);
        m_pMFXCore->FreeBuffer(m_MEID);
        m_MEID = 0;
    }

    if (m_SeqID)
    {
        m_pMFXCore->UnlockBuffer(m_SeqID);
        m_pMFXCore->FreeBuffer(m_SeqID);
        m_SeqID = 0;
    }
    if (m_pVC1PictureAdv)
    {
        m_pVC1PictureAdv->Close();
        m_pVC1PictureAdv = NULL;
    }
    if (m_pVC1PictureSM)
    {
        m_pVC1PictureSM->Close();
        m_pVC1PictureSM = NULL;
    }
    if (m_PicID)
    {
        m_pMFXCore->UnlockBuffer(m_PicID);
        m_pMFXCore->FreeBuffer(m_PicID);
        m_PicID = 0;
    }

    if (m_CodedMBID)
    {
        m_pMFXCore->UnlockBuffer(m_CodedMBID);
        m_pMFXCore->FreeBuffer(m_CodedMBID);
        m_CodedMBID = 0;
    }

    m_pVC1EncoderCodedMB = 0;
    if (m_pVC1EncoderMBs)
    {
        m_pVC1EncoderMBs->Close();
        m_pVC1EncoderMBs = 0;
    }
    if (m_MBsID)
    {
        m_pMFXCore->UnlockBuffer(m_MBsID);
        m_pMFXCore->FreeBuffer(m_MBsID);
        m_MBsID = 0;
    }

    if (m_MBID)
    {
        m_pMFXCore->UnlockBuffer(m_MBID);
        m_pMFXCore->FreeBuffer(m_MBID);
        m_MBID = 0;
    }

    if(m_MEObjID)
    {
        m_pMFXCore->UnlockBuffer(m_MEObjID);
        m_pMFXCore->FreeBuffer(m_MEObjID);
        m_MEObjID= 0;
        m_pME = NULL;
    }

    for (int i = 0 ; i < N_EX_FRAMES; i++)
    {
        if(m_ExFrameID[i])
        {
            m_pMFXCore->UnlockFrame(m_ExFrameID[i]);
            m_pMFXCore->FreeBuffer(m_ExFrameID[i]);
        }
    }

#ifdef VC1_ENC_DEBUG_ON
    if(UMC_VC1_ENCODER::pDebug)
    {
        delete UMC_VC1_ENCODER::pDebug;
        UMC_VC1_ENCODER::pDebug = NULL;
    }
#endif

    m_UseFB = true;
    m_FastFB = false;

     return MFXSts;
}


#define unlock_buff                                                                          \
if (frameDataLocked)                                                                         \
{                                                                                            \
    for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)                               \
    {                                                                                        \
      m_pMFXCore->UnlockFrame(cuc->FrameSurface->Data[i]->MemId);                            \
    }                                                                                        \
}                                                                                            \

#define UMC_MFX_CHECK_STS_1(err)    if (err != UMC::UMC_OK)                  {unlock_buff; return MFX_ERR_UNSUPPORTED;}
#define UMC_MFX_CHECK_WARN_1(err)   if (err == UMC::UMC_ERR_NOT_ENOUGH_DATA) {unlock_buff; return MFX_ERR_MORE_DATA;}

#define MFX_CHECK_STS_1(err)        if (err != MFX_ERR_NONE)                 {unlock_buff; return err;}
#define MFX_CHECK_COND_1(cond)      if  (!(cond))                            {unlock_buff; return MFX_ERR_UNSUPPORTED;}
#define MFX_CHECK_NULL_PTR1_1(pointer) if (pointer ==0)                      {unlock_buff;return MFX_ERR_NULL_PTR;}
#define MFX_CHECK_NULL_PTR2_1(p1, p2)  if (p1 ==0 || p2 ==0 )                {unlock_buff;return MFX_ERR_NULL_PTR;}

mfxStatus MFXVideoEncVc1::RunFrameVmeENC(mfxFrameCUC *cuc)
{
    mfxStatus        MFXSts = MFX_ERR_NONE;

    UMC::MeParams MEParams;
    UMC::Status err = UMC::UMC_OK;

    bool frameDataLocked  = false;
    Ipp32u i = 0;


    MFX_CHECK_NULL_PTR1_1(cuc);

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;
    MFX_CHECK_NULL_PTR1_1(pFrameParam);


    if ((cuc->FrameSurface!= 0 && cuc->FrameSurface->Info.NumFrameData!=0))
    {
        for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)
        {
            if(cuc->FrameSurface->Data[i]->Y==NULL || cuc->FrameSurface->Data[i]->U==NULL || cuc->FrameSurface->Data[i]->V==NULL)
            {
                 frameDataLocked  =  true;
                 MFXSts =  m_pMFXCore->LockFrame(cuc->FrameSurface->Data[i]->MemId, cuc->FrameSurface->Data[i]);
                 MFX_CHECK_STS_1(MFXSts);
            }
        }
    }


    MFX_CHECK_NULL_PTR1_1   (cuc->FrameSurface);
    MFX_CHECK_COND_1        (cuc->FrameSurface->Info.NumFrameData >0);
    MFX_CHECK_NULL_PTR1_1   (cuc->FrameSurface->Data);

    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Width % 16  == 0);
    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Width >= m_VideoParam.mfx.FrameInfo.Width);
    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Height >= m_VideoParam.mfx.FrameInfo.Height);

    MFX_CHECK_NULL_PTR1_1   (pFrameParam);

    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Height% 32 == 0 ||
                  (!pFrameParam->FieldPicFlag && cuc->FrameSurface->Info.Height% 16 == 0 ));

    MFX_CHECK_COND_1(cuc->FrameSurface->Info.NumFrameData > pFrameParam->CurrFrameLabel);

    //mfxU8     lockMarker = 0;
    mfxU16    w = m_VideoParam.mfx.FrameInfo.Width;
    mfxU16    h = m_VideoParam.mfx.FrameInfo.Height;

    GetRealFrameSize(&m_VideoParam.mfx, w, h);

    mfxI32 paddingT = 0;
    mfxI32 paddingL = 0;
    mfxI32 paddingB = 0;
    mfxI32 paddingR = 0;
    mfxI32 padding  = 0;

    GetPaddingSize (&cuc->FrameSurface->Info,w,h,paddingT,paddingL,paddingB,paddingR);
    padding = min4(paddingT,paddingL,paddingB,paddingR);

    // Current frame parameters:

    mfxFrameData*   pCurrentFrame = cuc->FrameSurface->Data[pFrameParam->CurrFrameLabel];
    mfxFrameInfo*   pCurrInfo     = &cuc->FrameSurface->Info;

    mfxFrameData*   pRefFrame [3]         = {0}; // forward, backward, current reconstructed (used for second field)
    mfxFrameData*   pRefFrameOriginal [3] = {0}; // forward, backward, current reconstructed (used for second field)

    mfxFrameData*   pRefFrameOriginal [3] = {0}; // forward, backward, current reconstructed (used for second field)

    mfxFrameInfo*   pRefInfo      = &cuc->FrameSurface->Info;

    UMC::MeFrame*   pMEFrames [6] = {0}; // 0-1 - forward, 2-3 backward, 4-5 current frame

    bool            FieldPicture  = pFrameParam->FieldPicFlag;
    mfxU16          wMB = (w + 15)>>4;
    mfxU16          hMB = (FieldPicture)? ((h>>1)+15)>>4:(h+15)>>4;
    mfxU16          w16 = wMB*16;
    mfxU16          h16 = (FieldPicture)? 2*hMB*16 : hMB*16;
    mfxU8           ttf = (FieldPicture && pFrameParam->BottomFieldFlag != pFrameParam->SecondFieldFlag )? 0 : 1;
    mfxU16          diffR = w16 - w;
    mfxU16          diffB = h16 - h;


    UMC_VC1_ENCODER::ePType UMCtype = UMC_VC1_ENCODER::VC1_ENC_I_FRAME;

    if (FieldPicture && m_VideoParam.mfx.CodecProfile != MFX_PROFILE_VC1_ADVANCED)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    pFrameParam->FrameHinMbMinus1 = hMB - 1;
    pFrameParam->FrameWinMbMinus1 = wMB - 1;

    pCurrentFrame->Field = FieldPicture;

    MFXSts = GetUMCPictureType(pFrameParam->FrameType,pFrameParam->FrameType2nd,
                               pFrameParam->FieldPicFlag, pFrameParam->InterlacedFrameFlag,
                               pFrameParam->Pic4MvAllowed,&UMCtype);
    MFX_CHECK_STS_1(MFXSts);

    bool bPredictedFrame   = isPredicted(UMCtype);
    bool bBPredictedFrame  = isBPredicted(UMCtype);
    bool bReferenceFrame   = isReference(UMCtype);


    if (bPredictedFrame || bBPredictedFrame)
    {
        mfxU8 indexF = (bPredictedFrame) ? pFrameParam->RefFrameListP[0]: pFrameParam->RefFrameListB[0][0];
        MFX_CHECK_COND_1(indexF < cuc->FrameSurface->Info.NumFrameData);
        pRefFrame[0] = cuc->FrameSurface->Data[indexF];
    }
    if (bBPredictedFrame)
    {
        mfxU8 indexB = pFrameParam->RefFrameListB[1][0];
        MFX_CHECK_COND_1(indexB < cuc->FrameSurface->Info.NumFrameData) ;
        pRefFrame[1] = cuc->FrameSurface->Data[indexB];

    }
    if (FieldPicture && pFrameParam->SecondFieldFlag && (bPredictedFrame || bBPredictedFrame))
    {
        mfxU8 index = pFrameParam->DecFrameLabel;
        MFX_CHECK_COND_1(index < cuc->FrameSurface->Info.NumFrameData);
        pRefFrame[2] = cuc->FrameSurface->Data[index];
    }

    //Set ME frames:
    static VC1FrameStruct frameStruct[2][2]= {{VC1_FRAME,VC1_FRAME},{VC1_FIELD_FIRST,VC1_FIELD_SECOND}};
    int n = (FieldPicture)? 1:0;
    static mfxU8 reconData[2][3][2] = {{{1,1},{1,1},{0,0}},{{1,1},{1,1},{1,0}}};

    int z = (FieldPicture && pFrameParam->SecondFieldFlag)? 1:0;
    static mfxU8 ref[3][2] = {{1,1},{1,1},{FieldPicture,FieldPicture}};

    for (int i = 0; i<3; i++)
    {
        mfxFrameData *pFr = (i==2)? pCurrentFrame:pRefFrame[i];

        if (pFr)
        {
            for (int j = 0; j<n+1; j++) // loop on frames
            {
                mfxI32 index = GetMEFrameIndex(m_pMEInfo,pFr->FrameOrder , m_numMEFrames, frameStruct[n][j]);
                if (index!= -1) // loop on fields
                {
                    FrameInfo *pInfo = &m_pMEInfo[index].m_frameInfo;
                    pInfo->m_bLocked = 1;
                    pMEFrames[2*i + j] = m_MeFrame[index];
                    if (pInfo->m_bReconData != reconData[z][i][j] )
                    {
                        m_MeFrame[index]->processed = 0;
                    }
                }
                else
                {
                    index = GetFreeIndex(m_pMEInfo,m_numMEFrames);
                    MFX_CHECK_COND_1(index != -1);
                    pMEFrames[2*i + j] = m_MeFrame[index];
                    m_MeFrame[index]->processed = 0;

                    FrameInfo *pInfo = &m_pMEInfo[index].m_frameInfo;
                    pInfo->m_bLocked = 1;
                    pInfo->m_bReconData = reconData[z][i][j];
                    pInfo->m_bReference = ref[i][j];
                    m_pMEInfo[index].m_frameStruct = frameStruct[n][j];
                    m_pMEInfo[index].m_MFXFrameOrder = pFr->FrameOrder;
                }
            }//for j
        }
    }// for i

    // Set Reference frames
    if ((padding<32 || paddingR < (diffR + 32) || paddingB < (diffB + 32)) &&
        m_bUsePadding && (bPredictedFrame || bBPredictedFrame))
    {
        for (int i = 0; i <3; i ++)
        {
            mfxFrameData* pInnerFrame = 0;

            if (pRefFrame[i]==0)
                continue;

            mfxI32 ind = GetExFrameIndex(m_ExFrameInfo,pRefFrame[i]->FrameOrder ,N_EX_FRAMES);

            if (ind != -1)
            {
                // stored frame was found
                pInnerFrame = m_ExFrames[ind];
                ExFrameInfo* pInfo = &m_ExFrameInfo[ind];

                pInfo->firstFieldInfo.m_bLocked = 1;
                if (pInfo->firstFieldInfo.m_bReconData != reconData[z][i][0])
                {
                    pInnerFrame->Paired = 0;
                    pInfo->firstFieldInfo.m_bHasData = 0;
                    pInfo->firstFieldInfo.m_bReconData = reconData[z][i][0];
                }

                pInfo->secondFieldInfo.m_bLocked = 1;
                if (pInfo->secondFieldInfo.m_bReconData != reconData[z][i][1])
                {
                    pInnerFrame->Paired = 0;
                    pInfo->secondFieldInfo.m_bHasData = 0;
                    pInfo->secondFieldInfo.m_bReconData = reconData[z][i][1];
                }
            }
            else
            {
                //new frame is locked
                ind = GetFreeIndex(m_ExFrameInfo, N_EX_FRAMES);

                MFX_CHECK_COND_1(ind != -1);

                ExFrameInfo* pInfo = &m_ExFrameInfo[ind];
                pInnerFrame = m_ExFrames[ind];

                pInfo->m_MFXFrameOrder = pRefFrame[i]->FrameOrder;

                pInfo->firstFieldInfo.m_bLocked  = 1;
                pInfo->firstFieldInfo.m_bHasData = 0;
                pInfo->firstFieldInfo.m_bReference = ref[i][0];
                pInfo->firstFieldInfo.m_bReconData = reconData[z][i][0];

                pInfo->secondFieldInfo.m_bLocked = 1;
                pInfo->secondFieldInfo.m_bHasData = 0;
                pInfo->secondFieldInfo.m_bReference = ref[i][1];
                pInfo->secondFieldInfo.m_bReconData = reconData[z][i][1];

                pInnerFrame->FrameOrder = pRefFrame[i]->FrameOrder;
                pInnerFrame->Field      = pRefFrame[i]->Field;
                pInnerFrame->Paired     = 0;
            }

            FrameInfo* pFirst  = &m_ExFrameInfo[ind].firstFieldInfo;
            FrameInfo* pSecond = &m_ExFrameInfo[ind].secondFieldInfo;

            // copy data if is needed from reconstructed frames
            if (pFirst->m_bHasData == 0   &&  pFirst->m_bReconData  == 1 &&
                pSecond->m_bHasData== 0   &&  pSecond->m_bReconData == 1 )
            {

                MFXSts = CopyFrame(pRefInfo, pRefFrame[i], &m_ExFrameSurface.Info, pInnerFrame, w, h);
                MFX_CHECK_STS_1(MFXSts);
                pFirst->m_bHasData  = 1;
                pSecond->m_bHasData = 1;
            }
            else if (pFirst->m_bHasData == 0   &&  pFirst->m_bReconData  == 1)
            {
                MFXSts = CopyField(pRefInfo, pRefFrame[i], &m_ExFrameSurface.Info, pInnerFrame, w, h/2,!ttf);
                MFX_CHECK_STS_1(MFXSts);
                pFirst->m_bHasData  = 1;
            }
            else if (pSecond->m_bHasData == 0   &&  pSecond->m_bReconData  == 1)
            {

                MFXSts = CopyField(pRefInfo, pRefFrame[i], &m_ExFrameSurface.Info, pInnerFrame, w, h/2,ttf);
                MFX_CHECK_STS_1(MFXSts);
                pSecond->m_bHasData  = 1;

            }
            pRefFrameOriginal[i] = pRefFrame[i];
            pRefFrame[i]         = pInnerFrame;
        }// for
        pRefInfo = &m_ExFrameSurface.Info;

    }// padding<32 && m_bUsePadding

    SetIntesityCompensationParameters (pFrameParam, false,0,0,true);
    SetIntesityCompensationParameters (pFrameParam, false,0,0,false);

    if (m_bIntensityCompensation)
    {
        IppiSize            YSize      =  {w, h};

        Ipp32s              nICLoops = 0;

        Ipp8u*              pSrcY = {0};
        Ipp8u*              pSrcU = {0};
        Ipp8u*              pSrcV = {0};

        Ipp8u*              pRefY[2] = {0};
        Ipp8u*              pRefU[2] = {0};
        Ipp8u*              pRefV[2] = {0};

        Ipp8u*              pRefYOr[2] = {0};
        Ipp8u*              pRefUOr[2] = {0};
        Ipp8u*              pRefVOr[2] = {0};

        Ipp32u              srcYStep  = {0};
        Ipp32u              srcUVStep  = {0};

        Ipp32u              refYStep[2]  = {0};
        Ipp32u              refUVStep[2] = {0};

        Ipp32u              refYStepOr[2]  = {0};
        Ipp32u              refUVStepOr[2] = {0};

        bool                bBottomRef[2]={0,0};
        bool                bSecondField = pFrameParam ->SecondFieldFlag;

        if (UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_FRAME)
        {
            nICLoops = 1;
            GetPlane_Prog(pCurrInfo,pCurrentFrame, pSrcY, srcYStep, pSrcU, pSrcV, srcUVStep);

            pRefFrame[0]->Paired = false;

            if (pRefFrameOriginal[0])
            {
                GetPlane_Prog(&cuc->FrameSurface->Info, pRefFrameOriginal[0],
                              pRefYOr[0], refYStepOr[0], pRefUOr[0], pRefVOr[0], refUVStepOr[0]);
            }

            if (pRefFrameOriginal[0])
            {
                              pRefYOr[0], refYStepOr[0], pRefUOr[0], pRefVOr[0], refUVStepOr[0]);
            }

        }
        else if ((UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD) ||
                 (UMCtype == UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD &&  bSecondField)||
                 (UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD && !bSecondField))
        {

            bool bBottom = pFrameParam->BottomFieldFlag;
            UMC_VC1_ENCODER::eReferenceFieldType RefFieldType = GetRefFieldFlag(pFrameParam->NumRefPic,pFrameParam->RefFieldPicFlag,pFrameParam->SecondFieldFlag);
            YSize.height = YSize.height>>1;

            GetPlane_Field(pCurrInfo,pCurrentFrame,bBottom, pSrcY, srcYStep, pSrcU , pSrcV, srcUVStep);

            if (RefFieldType & UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_FIRST)
            {
                nICLoops ++;
                if (bSecondField)
                {
                    GetPlane_Field(pRefInfo, pRefFrame[2],!bBottom,pRefY[0], refYStep [0], pRefU [0], pRefV[0], refUVStep[0]);
                    pRefFrame[2]->Paired = false;

                    if (pRefFrameOriginal[2])
                    {
                        GetPlane_Field(&cuc->FrameSurface->Info, pRefFrameOriginal[2],!bBottom,
                                        pRefYOr[0], refYStepOr[0], pRefUOr[0], pRefVOr[0], refUVStepOr[0]);
                    }


                    if (pRefFrameOriginal[2])
                    {
                        GetPlane_Field(&cuc->FrameSurface->Info, pRefFrameOriginal[2],!bBottom,
                                        pRefYOr[0], refYStepOr[0], pRefUOr[0], pRefVOr[0], refUVStepOr[0]);
                    }

                }
                else
                {
                    GetPlane_Field(pRefInfo, pRefFrame[0],!bBottom,pRefY[0], refYStep [0], pRefU [0], pRefV[0], refUVStep[0]);
                    pRefFrame[0]->Paired = false;
                    if (pRefFrameOriginal[0])
                    {
                        GetPlane_Field(&cuc->FrameSurface->Info, pRefFrameOriginal[0],!bBottom,
                                        pRefYOr[0], refYStepOr[0], pRefUOr[0], pRefVOr[0], refUVStepOr[0]);
                    }

                    if (pRefFrameOriginal[0])
                    {
                        GetPlane_Field(&cuc->FrameSurface->Info, pRefFrameOriginal[0],!bBottom,
                                        pRefYOr[0], refYStepOr[0], pRefUOr[0], pRefVOr[0], refUVStepOr[0]);
                    }

                }

                bBottomRef[0] = !bBottom;

            }
            if (RefFieldType & UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_SECOND)
            {
                nICLoops  ++;
                Ipp32u  n = nICLoops-1;

                bBottomRef[n] = bBottom;

                GetPlane_Field(pRefInfo, pRefFrame[0],bBottom,pRefY[n], refYStep[n], pRefU[n], pRefV[n], refUVStep[n]);
                pRefFrame[0]->Paired = false;

                if (pRefFrameOriginal[0])
                {
                    GetPlane_Field(&cuc->FrameSurface->Info, pRefFrameOriginal[0],bBottom,
                                    pRefYOr[n], refYStepOr[n], pRefUOr[n], pRefVOr[n], refUVStepOr[n]);
                }


                if (pRefFrameOriginal[0])
                {
                    GetPlane_Field(&cuc->FrameSurface->Info, pRefFrameOriginal[0],bBottom,
                                    pRefYOr[n], refYStepOr[n], pRefUOr[n], pRefVOr[n], refUVStepOr[n]);
                }

            }

        }

        IppiSize            UVSize     = {YSize.width>>1, YSize.height>>1};

        // IC
        for (int i=0; i<nICLoops; i++)
        {
            IppiSize            blockSize       = {3*16,3*16};
            UMC_VC1_ENCODER::CorrelationParams   sCorrParams     = {0};
            Ipp32u              LUMSCALE        = 0;
            Ipp32u              LUMSHIFT        = 0;
            Ipp32s              iScale = 0;
            Ipp32s              iShift = 0;
            Ipp32s              LUTY[257] ;
            Ipp32s              LUTUV[257];

            UMC_VC1_ENCODER::CalculateCorrelationBlock_8u_P1R (pRefY[i] ,refYStep[i],pSrcY, srcYStep,
                                                               YSize, blockSize,&sCorrParams);

            if (CalculateIntesityCompensationParams (sCorrParams,LUMSCALE,LUMSHIFT))
            {

                SetIntesityCompensationParameters (pFrameParam, true,LUMSCALE,LUMSHIFT,bBottomRef[i]);

                UMC_VC1_ENCODER::get_iScale_iShift (LUMSCALE, LUMSHIFT, iScale, iShift);
                UMC_VC1_ENCODER::CreateICTable (iScale, iShift, LUTY, LUTUV);

                ippiLUT_8u_C1IR(pRefY[i],refYStep[i],YSize,LUTY,UMC_VC1_ENCODER::intesityCompensationTbl,257);
                IntensityCompChroma(pRefU[i],pRefV[i],refUVStep[i],UVSize,LUTUV,UMC_VC1_ENCODER::intesityCompensationTbl);

                if (pRefFrameOriginal[0])
                {
                    ippiLUT_8u_C1IR(pRefYOr[i],refYStepOr[i],YSize,LUTY,UMC_VC1_ENCODER::intesityCompensationTbl,257);
                    IntensityCompChroma(pRefUOr[i],pRefVOr[i],refUVStepOr[i],UVSize,LUTUV,UMC_VC1_ENCODER::intesityCompensationTbl);
                }
           }
        }//for
    }

    // pad current frame
    {
        mfxU32 shiftL = GetLumaShift(pCurrInfo,pCurrentFrame);
        mfxU32 shiftC = GetChromaShift(pCurrInfo,pCurrentFrame);

        if (!FieldPicture)
        {
            PadFrameProgressive(pCurrentFrame->Y + shiftL, pCurrentFrame->U + shiftC,pCurrentFrame->V + shiftC,
                                w,  h,  pCurrentFrame->Pitch, 0,0,diffB,diffR);
        }
        else
        {
            PadFrameField(pCurrentFrame->Y + shiftL, pCurrentFrame->U + shiftC,pCurrentFrame->V + shiftC,
                          w,  h,  pCurrentFrame->Pitch,0,0,diffB,diffR);
        }
    }

    // pad reference frame
    mfxU8 pad = (m_bUsePadding )? 32 : 0;
    for(int i=0; i<3; i++)
    {
        if (pRefFrame[i])
        {
            if (pRefFrame[i]->Paired || !(bPredictedFrame ||bBPredictedFrame))
                continue;

            mfxU32 shiftL = GetLumaShift(pRefInfo,pRefFrame[i]);
            mfxU32 shiftC = GetChromaShift(pRefInfo,pRefFrame[i]);

            if (!pRefFrame[i]->Field)
            {
                PadFrameProgressive(pRefFrame[i]->Y + shiftL, pRefFrame[i]->U + shiftC, pRefFrame[i]->V + shiftC,
                                    w,  h,  pRefFrame[i]->Pitch, pad,pad,pad+diffB,pad+diffR);
                pRefFrame[i]->Paired = 1;
            }
            else
            {
                PadFrameField(pRefFrame[i]->Y + shiftL, pRefFrame[i]->U + shiftC,pRefFrame[i]->V + shiftC,
                              w,  h,  pRefFrame[i]->Pitch,pad,pad,pad + diffB,pad + diffR);
                if (i!=2)
                {
                    pRefFrame[i]->Paired = 1;
                }
            }
        }
    }

    mfxI32 NumSlice = (cuc->NumSlice == 0)? 1:cuc->NumSlice;

    MFXSts = SetMEParams(cuc,&m_VideoParam ,&MEParams, pRefFrame,pRefInfo, pMEFrames);
    MFX_CHECK_STS_1(MFXSts);

    for (mfxI32 i = 0; i< NumSlice; i++)
    {
        cuc->SliceId = i;
        if (cuc->NumSlice>1)
        {
            MFXSts = GetFirstLastMB (cuc, MEParams.FirstMB, MEParams.LastMB);
        }
        if (i != 0)
        {
            MEParams.ChangeInterpPixelType = false;
            MEParams.SelectVlcTables = false;
        }

        {
            if (!m_pME->EstimateFrame(&MEParams))
                MFXSts = MFX_ERR_UNSUPPORTED;
            MFX_CHECK_STS_1(MFXSts);
        }

        MFXSts = PutMEIntoCUC(&MEParams, cuc, UMCtype);
        MFX_CHECK_STS_1(MFXSts);
    }

    UnLockFrames(m_pMEInfo,m_numMEFrames);
    UnLockFrames(m_ExFrameInfo,N_EX_FRAMES);

      if (frameDataLocked)
      {
        for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)
        {
          MFXSts =  m_pMFXCore->UnlockFrame(cuc->FrameSurface->Data[i]->MemId,cuc->FrameSurface->Data[i]);
          MFX_CHECK_STS_1(MFXSts);
        }
          frameDataLocked  =  false;
      }

    return MFX_ERR_NONE;
}
mfxStatus MFXVideoEncVc1::RunSliceVmeENC(mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    UMC::MeParams MEParams;
    UMC::Status err = UMC::UMC_OK;

    bool frameDataLocked  = false;
    Ipp32u i = 0;

    MFX_CHECK_NULL_PTR1_1(cuc);

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;
    MFX_CHECK_NULL_PTR1_1(pFrameParam);


    if ((cuc->FrameSurface!= 0 && cuc->FrameSurface->Info.NumFrameData!=0))
    {
        for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)
        {
            if(cuc->FrameSurface->Data[i]->Y==NULL || cuc->FrameSurface->Data[i]->U==NULL || cuc->FrameSurface->Data[i]->V==NULL)
            {
             frameDataLocked  =  true;
             MFXSts =  m_pMFXCore->LockFrame(cuc->FrameSurface->Data[i]->MemId, cuc->FrameSurface->Data[i]);
             MFX_CHECK_STS_1(MFXSts);
            }
        }
    }

    MFX_CHECK_NULL_PTR1_1   (cuc->FrameSurface);
    MFX_CHECK_COND_1        (cuc->FrameSurface->Info.NumFrameData >0);
    MFX_CHECK_NULL_PTR1_1   (cuc->FrameSurface->Data);


    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Width % 16  == 0);
    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Width >= m_VideoParam.mfx.FrameInfo.Width);
    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Height >= m_VideoParam.mfx.FrameInfo.Height);
    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Height% 32 == 0 ||
        (!pFrameParam->FieldPicFlag && cuc->FrameSurface->Info.Height% 16 == 0 ));

    MFX_CHECK_COND_1(cuc->FrameSurface->Info.NumFrameData > pFrameParam->CurrFrameLabel);

    //mfxU8     lockMarker = 0;
    mfxU16    w = m_VideoParam.mfx.FrameInfo.Width;
    mfxU16    h = m_VideoParam.mfx.FrameInfo.Height;

    GetRealFrameSize(&m_VideoParam.mfx, w, h);

    mfxI32 paddingT = 0;
    mfxI32 paddingL = 0;
    mfxI32 paddingB = 0;
    mfxI32 paddingR = 0;
    mfxI32 padding  = 0;

    GetPaddingSize (&cuc->FrameSurface->Info,w,h,paddingT,paddingL,paddingB,paddingR);
    padding = min4(paddingT,paddingL,paddingB,paddingR);

    // Current frame parameters:

    mfxFrameData*   pCurrentFrame = cuc->FrameSurface->Data[pFrameParam->CurrFrameLabel];
    mfxFrameInfo*   pCurrInfo     = &cuc->FrameSurface->Info;

    mfxFrameData*   pRefFrame [3] = {0}; // forward, backward, current reconstructed (used for second field)
    mfxFrameInfo*   pRefInfo      = &cuc->FrameSurface->Info;

    UMC::MeFrame*   pMEFrames [6] = {0}; // 0-1 - forward, 2-3 backward, 4-5 current frame

    bool            FieldPicture  = pFrameParam->FieldPicFlag;
    mfxU16          wMB = (w + 15)>>4;
    mfxU16          hMB = (FieldPicture)? ((h>>1)+15)>>4:(h+15)>>4;
    mfxU16          w16 = wMB*16;
    mfxU16          h16 = (FieldPicture)? 2*hMB*16 : hMB*16;
    mfxU8           ttf = (FieldPicture && pFrameParam->BottomFieldFlag != pFrameParam->SecondFieldFlag )? 0 : 1;
    mfxU16          diffR = w16 - w;
    mfxU16          diffB = h16 - h;

    UMC_VC1_ENCODER::ePType UMCtype = UMC_VC1_ENCODER::VC1_ENC_I_FRAME;

    pFrameParam->FrameHinMbMinus1 = hMB - 1;
    pFrameParam->FrameWinMbMinus1 = wMB - 1;

    pCurrentFrame->Field = FieldPicture;

    MFXSts = GetUMCPictureType(pFrameParam->FrameType,pFrameParam->FrameType2nd,
        pFrameParam->FieldPicFlag, pFrameParam->InterlacedFrameFlag,
        pFrameParam->Pic4MvAllowed,&UMCtype);
    MFX_CHECK_STS_1(MFXSts);

    bool bPredictedFrame   = isPredicted(UMCtype);
    bool bBPredictedFrame  = isBPredicted(UMCtype);
    bool bReferenceFrame   = isReference(UMCtype);


    if (bPredictedFrame || bBPredictedFrame)
    {
        mfxU8 indexF = (bPredictedFrame) ? pFrameParam->RefFrameListP[0]: pFrameParam->RefFrameListB[0][0];
        MFX_CHECK_COND_1(indexF < cuc->FrameSurface->Info.NumFrameData)
            pRefFrame[0] = cuc->FrameSurface->Data[indexF];
    }
    if (bBPredictedFrame)
    {
        mfxU8 indexB = pFrameParam->RefFrameListB[1][0];
        MFX_CHECK_COND_1(indexB < cuc->FrameSurface->Info.NumFrameData) ;
        pRefFrame[1] = cuc->FrameSurface->Data[indexB];

    }
    if (FieldPicture && pFrameParam->SecondFieldFlag && (bPredictedFrame || bBPredictedFrame))
    {
        mfxU8 index = pFrameParam->DecFrameLabel;
        MFX_CHECK_COND_1(index < cuc->FrameSurface->Info.NumFrameData)
            pRefFrame[2] = cuc->FrameSurface->Data[index];
    }

    //Set ME frames:
    static VC1FrameStruct frameStruct[2][2]= {{VC1_FRAME,VC1_FRAME},{VC1_FIELD_FIRST,VC1_FIELD_SECOND}};
    int n = (FieldPicture)? 1:0;
    static mfxU8 reconData[2][3][2] = {{{1,1},{1,1},{0,0}},{{1,1},{1,1},{1,0}}};

    int z = (FieldPicture && pFrameParam->SecondFieldFlag)? 1:0;
    static mfxU8 ref[3][2] = {{1,1},{1,1},{FieldPicture,FieldPicture}};

    for (int i = 0; i<3; i++)
    {
        mfxFrameData *pFr = (i==2)? pCurrentFrame:pRefFrame[i];

        if (pFr)
        {
            for (int j = 0; j<n+1; j++) // loop on frames
            {
                mfxI32 index = GetMEFrameIndex(m_pMEInfo,pFr->FrameOrder , m_numMEFrames, frameStruct[n][j]);
                if (index!= -1) // loop on fields
                {
                    FrameInfo *pInfo = &m_pMEInfo[index].m_frameInfo;
                    pInfo->m_bLocked = 1;
                    pMEFrames[2*i + j] = m_MeFrame[index];
                    if (pInfo->m_bReconData != reconData[z][i][j] )
                    {
                        m_MeFrame[index]->processed = 0;
                    }
                }
                else
                {
                    index = GetFreeIndex(m_pMEInfo,m_numMEFrames);
                    MFX_CHECK_COND_1(index != -1);
                    pMEFrames[2*i + j] = m_MeFrame[index];
                    m_MeFrame[index]->processed = 0;

                    FrameInfo *pInfo = &m_pMEInfo[index].m_frameInfo;
                    pInfo->m_bLocked = 1;
                    pInfo->m_bReconData = reconData[z][i][j];
                    pInfo->m_bReference = ref[i][j];
                    m_pMEInfo[index].m_frameStruct = frameStruct[n][j];
                    m_pMEInfo[index].m_MFXFrameOrder = pFr->FrameOrder;
                }
            }//for j
        }
    }// for i

    // Set Reference frames
    if ((padding<32 || paddingR < (diffR + 32) || paddingB < (diffB + 32)) &&
        m_bUsePadding && (bPredictedFrame || bBPredictedFrame))
    {
        for (int i = 0; i <3; i ++)
        {
            mfxFrameData* pInnerFrame = 0;

            if (pRefFrame[i]==0)
                continue;

            mfxI32 ind = GetExFrameIndex(m_ExFrameInfo,pRefFrame[i]->FrameOrder ,N_EX_FRAMES);

            if (ind != -1)
            {
                // stored frame was found
                pInnerFrame = m_ExFrames[ind];
                ExFrameInfo* pInfo = &m_ExFrameInfo[ind];

                pInfo->firstFieldInfo.m_bLocked = 1;
                if (pInfo->firstFieldInfo.m_bReconData != reconData[z][i][0])
                {
                    pInnerFrame->Paired = 0;
                    pInfo->firstFieldInfo.m_bHasData = 0;
                    pInfo->firstFieldInfo.m_bReconData = reconData[z][i][0];
                }

                pInfo->secondFieldInfo.m_bLocked = 1;
                if (pInfo->secondFieldInfo.m_bReconData != reconData[z][i][1])
                {
                    pInnerFrame->Paired = 0;
                    pInfo->secondFieldInfo.m_bHasData = 0;
                    pInfo->secondFieldInfo.m_bReconData = reconData[z][i][1];

                }
                //pRefFrame[i] = pInnerFrame;

            }
            else
            {
                //new frame is locked
                ind = GetFreeIndex(m_ExFrameInfo, N_EX_FRAMES);

                MFX_CHECK_COND_1(ind != -1);

                ExFrameInfo* pInfo = &m_ExFrameInfo[ind];
                pInnerFrame = m_ExFrames[ind];

                pInfo->m_MFXFrameOrder = pRefFrame[i]->FrameOrder;

                pInfo->firstFieldInfo.m_bLocked  = 1;
                pInfo->firstFieldInfo.m_bHasData = 0;
                pInfo->firstFieldInfo.m_bReference = ref[i][0];
                pInfo->firstFieldInfo.m_bReconData = reconData[z][i][0];

                pInfo->secondFieldInfo.m_bLocked = 1;
                pInfo->secondFieldInfo.m_bHasData = 0;
                pInfo->secondFieldInfo.m_bReference = ref[i][1];
                pInfo->secondFieldInfo.m_bReconData = reconData[z][i][1];

                pInnerFrame->FrameOrder = pRefFrame[i]->FrameOrder;
                pInnerFrame->Field      = pRefFrame[i]->Field;
                pInnerFrame->Paired     = 0;
                //pRefFrame[i] = pInnerFrame;
            }
            FrameInfo* pFirst  = &m_ExFrameInfo[ind].firstFieldInfo;
            FrameInfo* pSecond = &m_ExFrameInfo[ind].secondFieldInfo;

            // copy data if is needed from reconstructed frames

            if (pFirst->m_bHasData == 0   &&  pFirst->m_bReconData  == 1 &&
                pSecond->m_bHasData== 0   &&  pSecond->m_bReconData == 1 )
            {

                MFXSts = CopyFrame(pRefInfo, pRefFrame[i], &m_ExFrameSurface.Info, pInnerFrame, w, h);
                MFX_CHECK_STS_1(MFXSts);
                pFirst->m_bHasData  = 1;
                pSecond->m_bHasData = 1;
            }
            else if (pFirst->m_bHasData == 0   &&  pFirst->m_bReconData  == 1)
            {
                MFXSts = CopyField(pRefInfo, pRefFrame[i], &m_ExFrameSurface.Info, pInnerFrame, w, h/2,!ttf);
                MFX_CHECK_STS_1(MFXSts);
                pFirst->m_bHasData  = 1;
            }
            else if (pSecond->m_bHasData == 0   &&  pSecond->m_bReconData  == 1)
            {

                MFXSts = CopyField(pRefInfo, pRefFrame[i], &m_ExFrameSurface.Info, pInnerFrame, w, h/2,ttf);
                MFX_CHECK_STS_1(MFXSts);
                pSecond->m_bHasData  = 1;

            }
            pRefFrame[i] = pInnerFrame;
        }// for
        pRefInfo = &m_ExFrameSurface.Info;

    }// padding<32 && m_bUsePadding


    // pad current frame
    {
        mfxU32 shiftL = GetLumaShift(pCurrInfo,pCurrentFrame);
        mfxU32 shiftC = GetChromaShift(pCurrInfo,pCurrentFrame);

        if (!FieldPicture)
        {
            PadFrameProgressive(pCurrentFrame->Y + shiftL, pCurrentFrame->U + shiftC,pCurrentFrame->V + shiftC,
                                w,  h,  pCurrentFrame->Pitch, 0,0,diffB,diffR);
        }
        else
        {
            PadFrameField(pCurrentFrame->Y + shiftL, pCurrentFrame->U + shiftC,pCurrentFrame->V + shiftC,
                          w,  h,  pCurrentFrame->Pitch,0,0,diffB,diffR);
        }
    }

    // pad reference frame
    mfxU8 pad = (m_bUsePadding )? 32 : 0;
    for(int i=0; i<3; i++)
    {
        if (pRefFrame[i])
        {
            if (pRefFrame[i]->Paired || !(bPredictedFrame ||bBPredictedFrame))
                continue;

            mfxU32 shiftL = GetLumaShift(pRefInfo,pRefFrame[i]);
            mfxU32 shiftC = GetChromaShift(pRefInfo,pRefFrame[i]);

            if (!pRefFrame[i]->Field)
            {
                PadFrameProgressive(pRefFrame[i]->Y + shiftL, pRefFrame[i]->U + shiftC, pRefFrame[i]->V + shiftC,
                                    w,  h,  pRefFrame[i]->Pitch, pad,pad,pad+diffB,pad+diffR);
                pRefFrame[i]->Paired = 1;
            }
            else
            {
                PadFrameField(pRefFrame[i]->Y + shiftL, pRefFrame[i]->U + shiftC,pRefFrame[i]->V + shiftC,
                              w,  h,  pRefFrame[i]->Pitch,pad,pad,pad + diffB,pad + diffR);
                pRefFrame[i]->Paired = 1;

            }
        }
    }

    mfxI32 NumSlice = (cuc->NumSlice == 0)? 1:cuc->NumSlice;

    MFXSts = SetMEParams(cuc,&m_VideoParam ,&MEParams, pRefFrame,pRefInfo, pMEFrames);
    MFX_CHECK_STS_1(MFXSts);

    {
        mfxI32 i = cuc->SliceId;
        MFX_CHECK_COND_1(i < NumSlice);

        if (cuc->NumSlice>1)
        {
            mfxSliceParam *pSlice = &cuc->SliceParam[i];
            MFX_CHECK_NULL_PTR1_1(pSlice)
            memcpy (&m_SliceParam, pSlice,sizeof(mfxSliceParam));
            MFXSts = GetFirstLastMB (cuc, MEParams.FirstMB, MEParams.LastMB);
        }
        else
        {
            memset(&m_SliceParam,0,sizeof(mfxSliceParam));
            m_SliceParam.VC1.SliceId = 0;
            m_SliceParam.VC1.NumMb = wMB*hMB;
            m_SliceParam.VC1.SliceType = (pFrameParam->SecondFieldFlag)? pFrameParam->FrameType2nd:pFrameParam->FrameType;
        }

        {
            if (!m_pME->EstimateFrame(&MEParams))
                MFXSts = MFX_ERR_UNSUPPORTED;
            MFX_CHECK_STS_1(MFXSts);
        }

        MFXSts = PutMEIntoCUC(&MEParams, cuc, UMCtype);
        MFX_CHECK_STS_1(MFXSts);
    }

    UnLockFrames(m_pMEInfo,m_numMEFrames);
    UnLockFrames(m_ExFrameInfo,N_EX_FRAMES);

      if (frameDataLocked)
      {
        for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)
        {
          MFXSts =  m_pMFXCore->UnlockFrame(cuc->FrameSurface->Data[i]->MemId,cuc->FrameSurface->Data[i]);
          MFX_CHECK_STS_1(MFXSts);
        }
          frameDataLocked  =  false;
      }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoEncVc1::RunFrameFullENC(mfxFrameCUC *cuc)
 {
    mfxStatus       MFXSts = MFX_ERR_NONE;

    return MFX_ERR_UNSUPPORTED;

    UMC::MeParams MEParams;
    UMC::Status err = UMC::UMC_OK;

    bool frameDataLocked    = false;
    Ipp32u i = 0;

    MFX_CHECK_NULL_PTR1_1   (cuc);

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;
    MFX_CHECK_NULL_PTR1_1(pFrameParam);


    if ((cuc->FrameSurface!= 0 && cuc->FrameSurface->Info.NumFrameData!=0))
    {
        for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)
        {
            if(cuc->FrameSurface->Data[i]->Y==NULL || cuc->FrameSurface->Data[i]->U==NULL || cuc->FrameSurface->Data[i]->V==NULL)
            {
            frameDataLocked  =  true;
             MFXSts =  m_pMFXCore->LockFrame(cuc->FrameSurface->Data[i]->MemId, cuc->FrameSurface->Data[i]);
             MFX_CHECK_STS_1(MFXSts);
            }
        }
    }

    MFX_CHECK_NULL_PTR1_1   (cuc->FrameSurface);
    MFX_CHECK_COND_1        (cuc->FrameSurface->Info.NumFrameData >0);
    MFX_CHECK_NULL_PTR1_1   (cuc->FrameSurface->Data);


    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Width % 16  == 0);
    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Width >= m_VideoParam.mfx.FrameInfo.Width);
    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Height >= m_VideoParam.mfx.FrameInfo.Height);
    MFX_CHECK_COND_1(cuc->FrameSurface->Info.Height% 32 == 0 ||
        (!pFrameParam->FieldPicFlag && cuc->FrameSurface->Info.Height% 16 == 0 ));

    MFX_CHECK_COND_1(cuc->FrameSurface->Info.NumFrameData > pFrameParam->CurrFrameLabel);

    //mfxU8     lockMarker = 0;
    mfxU16    w = m_VideoParam.mfx.FrameInfo.Width;
    mfxU16    h = m_VideoParam.mfx.FrameInfo.Height;

    GetRealFrameSize(&m_VideoParam.mfx, w, h);

    mfxI32 paddingT = 0;
    mfxI32 paddingL = 0;
    mfxI32 paddingB = 0;
    mfxI32 paddingR = 0;
    mfxI32 padding  = 0;

    GetPaddingSize (&cuc->FrameSurface->Info,w,h,paddingT,paddingL,paddingB,paddingR);
    padding = min4(paddingT,paddingL,paddingB,paddingR);

    // Current frame parameters:

    mfxFrameData*   pCurrentFrame = cuc->FrameSurface->Data[pFrameParam->CurrFrameLabel];
    mfxFrameInfo*   pCurrInfo     = &cuc->FrameSurface->Info;

    mfxFrameData*   pRefFrame [3] = {0}; // forward, backward, current reconstructed (used for second field)
    mfxFrameInfo*   pRefInfo      = &cuc->FrameSurface->Info;

    UMC::MeFrame*   pMEFrames [6] = {0}; // 0-1 - forward, 2-3 backward, 4-5 current frame

    bool            FieldPicture  = pFrameParam->FieldPicFlag;
    mfxU16          wMB = (w + 15)>>4;
    mfxU16          hMB = (FieldPicture)? ((h>>1)+15)>>4:(h+15)>>4;
    mfxU16          w16 = wMB*16;
    mfxU16          h16 = (FieldPicture)? 2*hMB*16 : hMB*16;
    mfxU8           ttf = (FieldPicture && pFrameParam->BottomFieldFlag != pFrameParam->SecondFieldFlag )? 0 : 1;
    mfxU16          diffR = w16 - w;
    mfxU16          diffB = h16 - h;

    mfxI32 firstRow = 0, nRows = 0;

    UMC_VC1_ENCODER::ePType UMCtype = UMC_VC1_ENCODER::VC1_ENC_I_FRAME;

    pFrameParam->FrameHinMbMinus1 = hMB - 1;
    pFrameParam->FrameWinMbMinus1 = wMB - 1;

    pCurrentFrame->Field = FieldPicture;

    //For UMC PAK
    UMC_VC1_ENCODER::Frame Plane;
    UMC_VC1_ENCODER::InitPictureParams initPicParam;
    vm_char   cLastError[VC1_ENC_STR_LEN];
    //UMC_VC1_ENCODER::VLCTablesIndex    VLCIndex;

    mfxU8  curIndex     =  pFrameParam->CurrFrameLabel   & 0x7F;
    mfxU8  raisedIndex  =  pFrameParam->DecFrameLabel    & 0x7F;

    Ipp8u scaleFactor=0;

    mfxI16Pair* pSavedMV   = 0;
    Ipp8u*      pDirection = 0;

    Ipp8u doubleQuant = 2;
    mfxI16 st;

    Ipp32u    smoothing = 0;

    if(m_profile == MFX_PROFILE_VC1_ADVANCED)
    {
        if (m_pVC1SequenceAdv->IsOverlap())
        {
            smoothing = ((pFrameParam->PicDeblocked>>5)&0x01)? 3:1;
        }
    }
    else
    {
        if (m_pVC1SequenceSM->IsOverlap())
        {
            smoothing = ((pFrameParam->PicDeblocked>>5)&0x01)? 3:1;
        }
    }
    //-------------UMC ENC-------------------------

    MFXSts = GetUMCPictureType(pFrameParam->FrameType,pFrameParam->FrameType2nd,
                               pFrameParam->FieldPicFlag, pFrameParam->InterlacedFrameFlag,
                               pFrameParam->Pic4MvAllowed,&UMCtype);
    MFX_CHECK_STS_1(MFXSts);

    bool bPredictedFrame   = isPredicted(UMCtype);
    bool bBPredictedFrame  = isBPredicted(UMCtype);
    bool bReferenceFrame   = isReference(UMCtype);


    if (bPredictedFrame || bBPredictedFrame)
    {
        mfxU8 indexF = (bPredictedFrame) ? pFrameParam->RefFrameListP[0]: pFrameParam->RefFrameListB[0][0];
        MFX_CHECK_COND_1(indexF < cuc->FrameSurface->Info.NumFrameData)
            pRefFrame[0] = cuc->FrameSurface->Data[indexF];
    }
    if (bBPredictedFrame)
    {
        mfxU8 indexB = pFrameParam->RefFrameListB[1][0];
        MFX_CHECK_COND_1(indexB < cuc->FrameSurface->Info.NumFrameData) ;
        pRefFrame[1] = cuc->FrameSurface->Data[indexB];

    }
    if (FieldPicture && pFrameParam->SecondFieldFlag && (bPredictedFrame || bBPredictedFrame))
    {
        mfxU8 index = pFrameParam->DecFrameLabel;
        MFX_CHECK_COND_1(index < cuc->FrameSurface->Info.NumFrameData)
            pRefFrame[2] = cuc->FrameSurface->Data[index];
    }

    //Set ME frames:
    static VC1FrameStruct frameStruct[2][2]= {{VC1_FRAME,VC1_FRAME},{VC1_FIELD_FIRST,VC1_FIELD_SECOND}};
    int n = (FieldPicture)? 1:0;
    static mfxU8 reconData[2][3][2] = {{{1,1},{1,1},{0,0}},{{1,1},{1,1},{1,0}}};

    int z = (FieldPicture && pFrameParam->SecondFieldFlag)? 1:0;
    static mfxU8 ref[3][2] = {{1,1},{1,1},{FieldPicture,FieldPicture}};

    for (int i = 0; i<3; i++)
    {
        mfxFrameData *pFr = (i==2)? pCurrentFrame:pRefFrame[i];

        if (pFr)
        {
            for (int j = 0; j<n+1; j++) // loop on frames
            {
                mfxI32 index = GetMEFrameIndex(m_pMEInfo,pFr->FrameOrder , m_numMEFrames, frameStruct[n][j]);
                if (index!= -1) // loop on fields
                {
                    FrameInfo *pInfo = &m_pMEInfo[index].m_frameInfo;
                    pInfo->m_bLocked = 1;
                    pMEFrames[2*i + j] = m_MeFrame[index];
                    if (pInfo->m_bReconData != reconData[z][i][j] )
                    {
                        m_MeFrame[index]->processed = 0;
                    }
                }
                else
                {
                    index = GetFreeIndex(m_pMEInfo,m_numMEFrames);
                    MFX_CHECK_COND_1(index != -1);
                    pMEFrames[2*i + j] = m_MeFrame[index];
                    m_MeFrame[index]->processed = 0;

                    FrameInfo *pInfo = &m_pMEInfo[index].m_frameInfo;
                    pInfo->m_bLocked = 1;
                    pInfo->m_bReconData = reconData[z][i][j];
                    pInfo->m_bReference = ref[i][j];
                    m_pMEInfo[index].m_frameStruct = frameStruct[n][j];
                    m_pMEInfo[index].m_MFXFrameOrder = pFr->FrameOrder;
                }
            }//for j
        }
    }// for i

    // Set Reference frames

    if ((padding<32 || paddingR < (diffR + 32) || paddingB < (diffB + 32)) &&
        m_bUsePadding && (bPredictedFrame || bBPredictedFrame))
    {
        for (int i = 0; i <3; i ++)
        {
            mfxFrameData* pInnerFrame = 0;

            if (pRefFrame[i]==0)
                continue;

            mfxI32 ind = GetExFrameIndex(m_ExFrameInfo,pRefFrame[i]->FrameOrder ,N_EX_FRAMES);

            if (ind != -1)
            {
                // stored frame was found
                pInnerFrame = m_ExFrames[ind];
                ExFrameInfo* pInfo = &m_ExFrameInfo[ind];

                pInfo->firstFieldInfo.m_bLocked = 1;
                if (pInfo->firstFieldInfo.m_bReconData != reconData[z][i][0])
                {
                    pInnerFrame->Paired = 0;
                    pInfo->firstFieldInfo.m_bHasData = 0;
                    pInfo->firstFieldInfo.m_bReconData = reconData[z][i][0];
                }

                pInfo->secondFieldInfo.m_bLocked = 1;
                if (pInfo->secondFieldInfo.m_bReconData != reconData[z][i][1])
                {
                    pInnerFrame->Paired = 0;
                    pInfo->secondFieldInfo.m_bHasData = 0;
                    pInfo->secondFieldInfo.m_bReconData = reconData[z][i][1];
                }
            }
            else
            {
                //new frame is locked
                ind = GetFreeIndex(m_ExFrameInfo, N_EX_FRAMES);

                MFX_CHECK_COND_1(ind != -1);

                ExFrameInfo* pInfo = &m_ExFrameInfo[ind];
                pInnerFrame = m_ExFrames[ind];

                pInfo->m_MFXFrameOrder = pRefFrame[i]->FrameOrder;

                pInfo->firstFieldInfo.m_bLocked  = 1;
                pInfo->firstFieldInfo.m_bHasData = 0;
                pInfo->firstFieldInfo.m_bReference = ref[i][0];
                pInfo->firstFieldInfo.m_bReconData = reconData[z][i][0];

                pInfo->secondFieldInfo.m_bLocked = 1;
                pInfo->secondFieldInfo.m_bHasData = 0;
                pInfo->secondFieldInfo.m_bReference = ref[i][1];
                pInfo->secondFieldInfo.m_bReconData = reconData[z][i][1];

                pInnerFrame->FrameOrder = pRefFrame[i]->FrameOrder;
                pInnerFrame->Field      = pRefFrame[i]->Field;
                pInnerFrame->Paired     = 0;
            }

            FrameInfo* pFirst  = &m_ExFrameInfo[ind].firstFieldInfo;
            FrameInfo* pSecond = &m_ExFrameInfo[ind].secondFieldInfo;

            // copy data if is needed from reconstructed frames
            if (pFirst->m_bHasData == 0   &&  pFirst->m_bReconData  == 1 &&
                pSecond->m_bHasData== 0   &&  pSecond->m_bReconData == 1 )
            {

                MFXSts = CopyFrame(pRefInfo, pRefFrame[i], &m_ExFrameSurface.Info, pInnerFrame, w, h);
                MFX_CHECK_STS_1(MFXSts);
                pFirst->m_bHasData  = 1;
                pSecond->m_bHasData = 1;
            }
            else if (pFirst->m_bHasData == 0   &&  pFirst->m_bReconData  == 1)
            {
                MFXSts = CopyField(pRefInfo, pRefFrame[i], &m_ExFrameSurface.Info, pInnerFrame, w, h/2,!ttf);
                MFX_CHECK_STS_1(MFXSts);
                pFirst->m_bHasData  = 1;
            }
            else if (pSecond->m_bHasData == 0   &&  pSecond->m_bReconData  == 1)
            {

                MFXSts = CopyField(pRefInfo, pRefFrame[i], &m_ExFrameSurface.Info, pInnerFrame, w, h/2,ttf);
                MFX_CHECK_STS_1(MFXSts);
                pSecond->m_bHasData  = 1;

            }
            pRefFrame[i] = pInnerFrame;
        }// for
        pRefInfo = &m_ExFrameSurface.Info;

    }// padding<32 && m_bUsePadding

    // pad current frame
    {
        mfxU32 shiftL = GetLumaShift(pCurrInfo,pCurrentFrame);
        mfxU32 shiftC = GetChromaShift(pCurrInfo,pCurrentFrame);
        mfxU16 wC     = w>>1;
        mfxU16 hC     = h>>1;

        if (!FieldPicture)
        {
            PadFrameProgressive(pCurrentFrame->Y + shiftL, pCurrentFrame->U + shiftC,pCurrentFrame->V + shiftC,
                                w,  h,  pCurrentFrame->Pitch, 0,0,diffB,diffR);
        }
        else
        {
            PadFrameField(pCurrentFrame->Y + shiftL, pCurrentFrame->U + shiftC,pCurrentFrame->V + shiftC,
                          w,  h,  pCurrentFrame->Pitch,0,0,diffB,diffR);
        }
    }

    // pad reference frame
    mfxU8 pad = (m_bUsePadding )? 32 : 0;
    for(int i=0; i<3; i++)
    {
        if (pRefFrame[i])
        {
            if (pRefFrame[i]->Paired || !(bPredictedFrame ||bBPredictedFrame))
                continue;

            mfxU32 shiftL = GetLumaShift(pRefInfo,pRefFrame[i]);
            mfxU32 shiftC = GetChromaShift(pRefInfo,pRefFrame[i]);

            if (!pRefFrame[i]->Field)
            {
                PadFrameProgressive(pRefFrame[i]->Y + shiftL, pRefFrame[i]->U + shiftC, pRefFrame[i]->V + shiftC,
                    w,  h,  pRefFrame[i]->Pitch, pad,pad,pad+diffB,pad+diffR);
                pRefFrame[i]->Paired = 1;
            }
            else
            {
                PadFrameField(pRefFrame[i]->Y + shiftL, pRefFrame[i]->U + shiftC,pRefFrame[i]->V + shiftC,
                    w,  h,  pRefFrame[i]->Pitch,pad,pad,pad + diffB,pad + diffR);
                pRefFrame[i]->Paired = 1;
            }
        }
    }

    mfxI32 NumSlice = (cuc->NumSlice == 0)? 1:cuc->NumSlice;

    MFXSts = SetMEParams(cuc,&m_VideoParam ,&MEParams, pRefFrame,pRefInfo, pMEFrames);
    MFX_CHECK_STS_1(MFXSts);

    for (mfxI32 i = 0; i< NumSlice; i++)
    {
        cuc->SliceId = i;
        if (cuc->NumSlice>1)
        {
            MFXSts = GetFirstLastMB (cuc, MEParams.FirstMB, MEParams.LastMB);
        }
#ifndef VC1_ME_MB_STATICTICS
    if (!isIntra(pFrameParam->FrameType))
#endif
    {
        if (!m_pME->EstimateFrame(&MEParams))
            MFXSts = MFX_ERR_UNSUPPORTED;
        MFX_CHECK_STS_1(MFXSts);
    }
}

    //-------------UMC PAK-------------------------
    bool bSecondField = pFrameParam->SecondFieldFlag;

    MFXSts = GetBFractionParameters (pFrameParam->CodedBfraction,
                                initPicParam.uiBFraction.num,
                                initPicParam.uiBFraction.denom, scaleFactor);

    MFX_CHECK_STS_1(MFXSts)

    switch(pFrameParam->UnifiedMvMode)
    {
    case 0:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_MIXED_QUARTER_BICUBIC;
        initPicParam.bMVMixed = true;
        break;
    case 1:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_QUARTER_BICUBIC;
        initPicParam.bMVMixed = false;
        break;
    case 2:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_HALF_BICUBIC;
        initPicParam.bMVMixed = false;
        break;
    case 3:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_HALF_BILINEAR;
        initPicParam.bMVMixed = false;
        break;
    default:
        unlock_buff;
        return MFX_ERR_UNSUPPORTED;
        break;
    }

    // set initialization params for picture

    initPicParam.uiMVRangeIndex       = pFrameParam->ExtendedMvRange;
    initPicParam.nReferenceFrameDist  = pFrameParam->RefDistance;

    initPicParam.uiBFraction.num = 1;
    initPicParam.uiBFraction.denom = 2;

    initPicParam.uiReferenceFieldType = UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_FIRST;
    initPicParam.sVLCTablesIndex.uiMVTab          = pFrameParam->MvTable;
    initPicParam.sVLCTablesIndex.uiDecTypeAC      = pFrameParam->TransACTable;
    initPicParam.sVLCTablesIndex.uiCBPTab         = pFrameParam->CBPTable;


    if (((UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD)&& (!pFrameParam->SecondFieldFlag))||
        ((UMCtype == UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD)&& ( pFrameParam->SecondFieldFlag)) ||
         (UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD))
    {
        initPicParam.uiReferenceFieldType = GetRefFieldFlag(pFrameParam->NumRefPic,pFrameParam->RefFieldPicFlag,pFrameParam->SecondFieldFlag);
    }

    initPicParam.nReferenceFrameDist  = pFrameParam->RefDistance; // is used only for fields

    if(m_profile == MFX_PROFILE_VC1_ADVANCED)
    {
          err = m_pVC1PictureAdv->SetInitPictureParams(&initPicParam);
          UMC_MFX_CHECK_STS_1(err)
    }
    else
    {
          err = m_pVC1PictureSM->SetInitPictureParams(&initPicParam);
          UMC_MFX_CHECK_STS_1(err)
    }


    if (FieldPicture)
        MFXSts = GetSavedMVField (cuc, &pSavedMV, &pDirection);
    else
        MFXSts = GetSavedMVFrame (cuc, &pSavedMV);
    MFX_CHECK_STS_1(MFXSts);

    doubleQuant = pFrameParam->PQuant*2 + pFrameParam->HalfQP;
    assert(doubleQuant > 0);
    assert(doubleQuant <= 62);

    if(m_profile == MFX_PROFILE_VC1_ADVANCED)
    {
        err = m_pVC1PictureAdv->SetPictureParams(UMCtype, (Ipp16s*)pSavedMV, pDirection, pFrameParam->SecondFieldFlag, smoothing);
        UMC_MFX_CHECK_STS_1(err)

        err = m_pVC1PictureAdv->SetPictureQuantParams(doubleQuant>>1, doubleQuant&0x1);
        UMC_MFX_CHECK_STS_1(err);

        err = m_pVC1PictureAdv->CheckParameters(cLastError,pFrameParam->SecondFieldFlag);
        UMC_MFX_CHECK_STS_1(err);
    }
    else
    {
          err = m_pVC1PictureSM->SetPictureParams(UMCtype, (Ipp16s*)pSavedMV);
          UMC_MFX_CHECK_STS_1(err)

          err = m_pVC1PictureSM->SetPictureQuantParams(doubleQuant>>1, doubleQuant&0x1);
          UMC_MFX_CHECK_STS_1(err);

          err = m_pVC1PictureSM->CheckParameters(cLastError);
          UMC_MFX_CHECK_STS_1(err);
    }

    /*------------Work with planes-----------------------*/
    MFXSts = SetFrame (curIndex, cuc->FrameSurface, &m_VideoParam, UMCtype, &Plane, FieldPicture);
    MFX_CHECK_STS_1(MFXSts)

    if(m_profile == MFX_PROFILE_VC1_ADVANCED)
    {
        err = m_pVC1PictureAdv->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_CURR_PLANE);
        UMC_MFX_CHECK_STS_1(err);
    }
    else
    {
        err = m_pVC1PictureSM->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_CURR_PLANE);
        UMC_MFX_CHECK_STS_1(err);
    }


    MFXSts = SetFrame (raisedIndex, cuc->FrameSurface, &m_VideoParam, UMCtype, &Plane, FieldPicture);
    MFX_CHECK_STS_1(MFXSts)

    if(m_profile == MFX_PROFILE_VC1_ADVANCED)
    {
        err = m_pVC1PictureAdv->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_RAISED_PLANE);
        UMC_MFX_CHECK_STS_1(err);
    }
    else
    {
        err = m_pVC1PictureSM->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_RAISED_PLANE);
        UMC_MFX_CHECK_STS_1(err);
    }

    if (isPredicted(UMCtype))
    {
        MFX_CHECK_COND_1(pFrameParam->NumRefFrame>=1)
        MFX_CHECK_NULL_PTR1_1(pFrameParam->RefFrameListP)

        mfxU8  refIndexF    =  pFrameParam->RefFrameListP[0] & 0x7F;

        MFX_CHECK_COND_1(refIndexF != curIndex && refIndexF != raisedIndex)

        MFXSts = SetFrame (refIndexF, cuc->FrameSurface, &m_VideoParam, UMCtype, &Plane, FieldPicture);
        MFX_CHECK_STS_1(MFXSts)

        if(m_profile == MFX_PROFILE_VC1_ADVANCED)
        {
            err = m_pVC1PictureAdv->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_FORWARD_PLANE);
            UMC_MFX_CHECK_STS_1(err);
        }
        else
        {
            err = m_pVC1PictureSM->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_FORWARD_PLANE);
            UMC_MFX_CHECK_STS_1(err);
        }
    }
    else if (isBPredicted(UMCtype))
    {
        MFX_CHECK_COND_1(pFrameParam->NumRefFrame>=2)
        MFX_CHECK_NULL_PTR1_1(pFrameParam->RefFrameListB)
        MFX_CHECK_NULL_PTR2_1(pFrameParam->RefFrameListB[0], pFrameParam->RefFrameListB[1])

        mfxU8  refIndexF    =  pFrameParam->RefFrameListB[0][0] & 0x7F;
        mfxU8  refIndexB    =  pFrameParam->RefFrameListB[1][0] & 0x7F;

        MFX_CHECK_COND_1(refIndexF != curIndex && refIndexF != raisedIndex &&
                            refIndexB != curIndex && refIndexB != raisedIndex &&
                            refIndexB != refIndexF)

        MFXSts = SetFrame (refIndexF, cuc->FrameSurface, &m_VideoParam, UMCtype, &Plane, FieldPicture);
        MFX_CHECK_STS_1(MFXSts)

        if(m_profile == MFX_PROFILE_VC1_ADVANCED)
        {
            err = m_pVC1PictureAdv->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_FORWARD_PLANE);
            UMC_MFX_CHECK_STS_1(err);
        }
        else
        {
            err = m_pVC1PictureSM->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_FORWARD_PLANE);
            UMC_MFX_CHECK_STS_1(err);
        }
        MFXSts = SetFrame (refIndexB, cuc->FrameSurface, &m_VideoParam, UMCtype, &Plane, FieldPicture);
        MFX_CHECK_STS_1(MFXSts)

        if(m_profile == MFX_PROFILE_VC1_ADVANCED)
        {
            err = m_pVC1PictureAdv->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_BACKWARD_PLANE);
            UMC_MFX_CHECK_STS_1(err);
        }
        else
        {
            err = m_pVC1PictureSM->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_BACKWARD_PLANE);
            UMC_MFX_CHECK_STS_1(err);
        }
    }

    MFXSts = GetSliceRows (cuc, firstRow, nRows);
    UMC_MFX_CHECK_STS_1(err);

    /*----------------------------------------------------*/
    if(m_profile == MFX_PROFILE_VC1_ADVANCED)
    {
        switch (UMCtype)
        {
        case UMC_VC1_ENCODER::VC1_ENC_I_FRAME:
            {
                //----coding---------------
                err = m_pVC1PictureAdv->PAC_IFrame(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(err);

                 err = PutResidualsIntoCuc_IFrame(cuc, firstRow, nRows);
                 UMC_MFX_CHECK_STS_1(err);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_FRAME:
            if (initPicParam.bMVMixed)
            {
                //----coding---------------
                    err = m_pVC1PictureAdv->PAC_PFrame(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(err);

                    err = PutResidualsIntoCuc_PFrame(cuc, firstRow, nRows);
                 UMC_MFX_CHECK_STS_1(err);
            }
            else
            {
                //----coding---------------
                    err = m_pVC1PictureAdv->PAC_PFrameMixed(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(err);

                    err = PutResidualsIntoCuc_PMixedFrame(cuc, firstRow, nRows);
                 UMC_MFX_CHECK_STS_1(err);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_FRAME:
            {
                //----coding---------------
                    err = m_pVC1PictureAdv->PAC_BFrame(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(err);

                    err = PutResidualsIntoCuc_BFrame(cuc, firstRow, nRows);
                 UMC_MFX_CHECK_STS_1(err);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME:
                break;
        case UMC_VC1_ENCODER::VC1_ENC_I_I_FIELD:
                //----coding---------------
                    err = m_pVC1PictureAdv->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(err);
                    err = PutResidualsIntoCuc_IField(cuc, firstRow, nRows);
                 UMC_MFX_CHECK_STS_1(err);
            break;
        case UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD:
                //----coding---------------
                if(!bSecondField)
                {
                        err = m_pVC1PictureAdv->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);

                        err = PutResidualsIntoCuc_IField(cuc, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);
                }
                else
                {
                        err =  m_pVC1PictureAdv->PACPField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);

                        err = PutResidualsIntoCuc_PField(cuc, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);
                }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD:
                //----coding---------------
                if(!bSecondField)
                {
                        err =  m_pVC1PictureAdv->PACPField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);

                        err = PutResidualsIntoCuc_PField(cuc, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);
                }
                else
                {
                        err = m_pVC1PictureAdv->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);

                        err = PutResidualsIntoCuc_IField(cuc, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);
                }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD:
                //----coding---------------
                    err =  m_pVC1PictureAdv->PACPField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(err);

                    err = PutResidualsIntoCuc_PField(cuc, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(err);

                break;
        case UMC_VC1_ENCODER::VC1_ENC_BI_B_FIELD:
                //----coding---------------
                if(!bSecondField)
                {
                    err = m_pVC1PictureAdv->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);

                     err = PutResidualsIntoCuc_IField(cuc);
                    UMC_MFX_CHECK_STS_1(err);
                }
                else
                {
                    err =  m_pVC1PictureAdv->PACBField(&MEParams,pFrameParam->SecondFieldFlag);
                    UMC_MFX_CHECK_STS_1(err);

                        err = PutResidualsIntoCuc_BField(cuc, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);
                }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_BI_FIELD:
                //----coding---------------
                if(!bSecondField)
                {
                        err =  m_pVC1PictureAdv->PACBField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);

                        err = PutResidualsIntoCuc_BField(cuc, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);
                }
                else
                {
                        err = m_pVC1PictureAdv->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);

                        err = PutResidualsIntoCuc_IField(cuc, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);
                }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_B_FIELD:
                    //----coding---------------
                        err =  m_pVC1PictureAdv->PACBField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);

                        err = PutResidualsIntoCuc_BField(cuc, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(err);

                    break;
        default:
           unlock_buff;
           return MFX_ERR_UNSUPPORTED;
           break;
          }
    }
    else
    {
        switch (UMCtype)
        {
        case UMC_VC1_ENCODER::VC1_ENC_I_FRAME:
            {
                //----coding---------------
                err = m_pVC1PictureSM->PAC_IFrame(&MEParams);
                UMC_MFX_CHECK_STS_1(err);

                err = PutResidualsIntoCuc_IFrame(cuc);
                UMC_MFX_CHECK_STS_1(err);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_FRAME:
            if (initPicParam.bMVMixed)
            {
                //----coding---------------
                err = m_pVC1PictureSM->PAC_PFrame(&MEParams);
                UMC_MFX_CHECK_STS_1(err);

                err = PutResidualsIntoCuc_PFrame(cuc);
                 UMC_MFX_CHECK_STS_1(err);
            }
            else
            {
                //----coding---------------
                err = m_pVC1PictureSM->PAC_PFrameMixed(&MEParams);
                UMC_MFX_CHECK_STS_1(err);

                err = PutResidualsIntoCuc_PMixedFrame(cuc);
                 UMC_MFX_CHECK_STS_1(err);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_FRAME:
            {
                //----coding---------------
                err = m_pVC1PictureSM->PAC_BFrame(&MEParams);
                UMC_MFX_CHECK_STS_1(err);

                err = PutResidualsIntoCuc_BFrame(cuc);
                UMC_MFX_CHECK_STS_1(err);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME:
                break;
        default:
           unlock_buff;
           return MFX_ERR_UNSUPPORTED;
           break;
          }

    }

     //mfxFrameData *pFrRecon = cuc->FrameSurface->Data[pFrameParam->DecFrameLabel];
     //pFrRecon->LockAndFlagsMarker ^= lockMarker;

    UnLockFrames(m_pMEInfo,m_numMEFrames);
    UnLockFrames(m_ExFrameInfo,N_EX_FRAMES);

      if (frameDataLocked)
      {
        for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)
        {
          MFXSts =  m_pMFXCore->UnlockFrame(cuc->FrameSurface->Data[i]->MemId,cuc->FrameSurface->Data[i]);
          MFX_CHECK_STS_1(MFXSts);
        }
          frameDataLocked  =  false;
      }


    return MFX_ERR_NONE;
 }

#undef UMC_MFX_CHECK_STS_1
#undef UMC_MFX_CHECK_WARN_1
#undef MFX_CHECK_STS_1
#undef MFX_CHECK_COND_1
#undef MFX_CHECK_NULL_PTR1_1
#undef MFX_CHECK_NULL_PTR2_1
#undef unlock_buff

mfxStatus MFXVideoEncVc1::PutResidualsIntoCuc_BField(mfxFrameCUC *cuc, mfxI32 firstRow, mfxI32 nRows)

{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;
    Ipp32u       h = pFrameParam->FrameHinMbMinus1 + 1;
    Ipp32u       w = pFrameParam->FrameWinMbMinus1 + 1;

    UMC_VC1_ENCODER::sCoordinate dMV = {0};

    mfxI32       MBResLen = 64*6;
    mfxI32       MBHybridLen = 1;
    mfxI32       MBACPredLen = 1;

    mfxI32       MBResOffset = 0;
    mfxI32       MBHybridOffset = 0;
    mfxI32       MBACPredOffset = 0;

    nRows = (nRows>0)? nRows: h;

    if (cuc->NumSlice>1)
    {
        MBResOffset    = MBResLen   *firstRow*w;
        MBHybridOffset = MBHybridLen*firstRow*w;
        MBACPredOffset = MBACPredLen*firstRow*w;
    }

    //residuals
    n = GetExBufferIndex(cuc, MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;
    MFX_CHECK_NULL_PTR1(pMFXRes)

    pMFXRes += MBResOffset;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB +
                                                            w*(h*pFrameParam->SecondFieldFlag + firstRow);
    mfxMbCodeVC1*                         pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    for (i = 0; i< nRows; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pMfxCurrMB = &cuc->MbParam->Mb[j + i*w].VC1;

            TempBlock.InitBlocks(pMFXRes);
            TempBlock.Reset();

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->GetResiduals (TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }

            pMfxCurrMB->FieldMbFlag   = 0;
            pMfxCurrMB->SubMbPredMode = 0;
            pMfxCurrMB->SubMbPredMode = pCompressedMB->GetIntraPattern();

            GetMFX_CBPCY(pCompressedMB->GetMBCBPCY(), &pMfxCurrMB->CodedPattern4x4Y,
                &pMfxCurrMB->CodedPattern4x4U,  &pMfxCurrMB->CodedPattern4x4V);

            switch(pCompressedMB->GetMBType())
            {
            case UMC_VC1_ENCODER::VC1_ENC_B_MB_INTRA:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTRA_VC1;
                pMfxCurrMB->IntraMbFlag   = 1;
                pMfxCurrMB->Skip8x8Flag   = 0;

                pMfxCurrMB->AcPredFlag = (mfxU8)pCompressedMB->GetACPrediction();
                break;
            case UMC_VC1_ENCODER::VC1_ENC_B_MB_F:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_0;
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                pMfxCurrMB->RefPicSelect[0][0] = dMV.bSecond;

                break;

     case UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                pMfxCurrMB->RefPicSelect[0][0] = dMV.bSecond;

                break;

            case UMC_VC1_ENCODER::VC1_ENC_B_MB_B:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_1;
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, false, 0);

                pMfxCurrMB->MV[4][0] = dMV.x;
                pMfxCurrMB->MV[4][1] = dMV.y;

                pMfxCurrMB->RefPicSelect[1][0] = dMV.bSecond;

                break;

            case UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_1;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, false, 0);

                pMfxCurrMB->MV[4][0] = dMV.x;
                pMfxCurrMB->MV[4][1] = dMV.y;

                pMfxCurrMB->RefPicSelect[1][0] = dMV.bSecond;

                break;

            case UMC_VC1_ENCODER::VC1_ENC_B_MB_DIRECT:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_DIR;
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                pMfxCurrMB->RefPicSelect[0][0] = dMV.bSecond;

                break;

            case UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_DIRECT:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_DIR;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                pMfxCurrMB->RefPicSelect[0][0] = dMV.bSecond;

                break;
            case UMC_VC1_ENCODER::VC1_ENC_B_MB_FB:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_2;
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;
                pMfxCurrMB->RefPicSelect[0][0] = dMV.bSecond;

                pCompressedMB->GetdMV(&dMV, false, 0);

                pMfxCurrMB->MV[4][0] = dMV.x;
                pMfxCurrMB->MV[4][1] = dMV.y;
                pMfxCurrMB->RefPicSelect[1][0] = dMV.bSecond;

                break;

            case UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_FB:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_2;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                pMfxCurrMB->RefPicSelect[0][0] = dMV.bSecond;

                pCompressedMB->GetdMV(&dMV, false, 0);

                pMfxCurrMB->MV[4][0] = dMV.x;
                pMfxCurrMB->MV[4][1] = dMV.y;
                pMfxCurrMB->RefPicSelect[1][0] = dMV.bSecond;
                break;
            default:
                assert(0);
                return MFX_ERR_UNSUPPORTED;
            }

            pMFXRes += 6*64;

            pCompressedMB++;
            pMfxCurrMB++;
        }
    }

    return MFXSts;
}

mfxStatus MFXVideoEncVc1::PutResidualsIntoCuc_BFrame(mfxFrameCUC *cuc, mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;
    Ipp32u       h = pFrameParam->FrameHinMbMinus1 + 1;
    Ipp32u       w = pFrameParam->FrameWinMbMinus1 + 1;

    UMC_VC1_ENCODER::sCoordinate dMV = {0};
    mfxI32       MBResLen = 64*6;
    mfxI32       MBHybridLen = 1;
    mfxI32       MBACPredLen = 1;

    mfxI32       MBResOffset = 0;
    mfxI32       MBHybridOffset = 0;
    mfxI32       MBACPredOffset = 0;

    nRows = (nRows>0)? nRows: h;

    if (cuc->NumSlice>1)
    {
        MBResOffset    = MBResLen   *firstRow*w;
        MBHybridOffset = MBHybridLen*firstRow*w;
        MBACPredOffset = MBACPredLen*firstRow*w;
    }

    //residuals
    n = GetExBufferIndex(cuc, MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;
    MFX_CHECK_NULL_PTR1(pMFXRes)

    pMFXRes += MBResOffset;


    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB+
                                                            w*firstRow;
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    for (i = 0; i< nRows; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pMfxCurrMB = &cuc->MbParam->Mb[j + i*w].VC1;

            TempBlock.InitBlocks(pMFXRes);
            TempBlock.Reset();

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->GetResiduals (TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }

            pMfxCurrMB->FieldMbFlag   = 0;
            pMfxCurrMB->SubMbPredMode = 0;
            pMfxCurrMB->SubMbPredMode = pCompressedMB->GetIntraPattern();

            GetMFX_CBPCY(pCompressedMB->GetMBCBPCY(), &pMfxCurrMB->CodedPattern4x4Y,
                &pMfxCurrMB->CodedPattern4x4U,  &pMfxCurrMB->CodedPattern4x4V);

            switch(pCompressedMB->GetMBType())
            {
            case UMC_VC1_ENCODER::VC1_ENC_B_MB_INTRA:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTRA_VC1;
                pMfxCurrMB->IntraMbFlag   = 1;
                pMfxCurrMB->Skip8x8Flag   = 0;

                pMfxCurrMB->AcPredFlag = (mfxU8)pCompressedMB->GetACPrediction();
                break;
            case UMC_VC1_ENCODER::VC1_ENC_B_MB_F:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_0;
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                break;

     case UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                break;

            case UMC_VC1_ENCODER::VC1_ENC_B_MB_B:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_1;
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, false, 0);

                pMfxCurrMB->MV[4][0] = dMV.x;
                pMfxCurrMB->MV[4][1] = dMV.y;

                break;

            case UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_1;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, false, 0);

                pMfxCurrMB->MV[4][0] = dMV.x;
                pMfxCurrMB->MV[4][1] = dMV.y;

                break;

            case UMC_VC1_ENCODER::VC1_ENC_B_MB_DIRECT:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_DIR;
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, false, 0);

                pMfxCurrMB->MV[4][0] = dMV.x;
                pMfxCurrMB->MV[4][1] = dMV.y;

                break;

            case UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_DIRECT:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_DIR;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                pCompressedMB->GetdMV(&dMV, false, 0);

                pMfxCurrMB->MV[4][0] = dMV.x;
                pMfxCurrMB->MV[4][1] = dMV.y;

                break;
            case UMC_VC1_ENCODER::VC1_ENC_B_MB_FB:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_2;
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                pCompressedMB->GetdMV(&dMV, false, 0);

                pMfxCurrMB->MV[4][0] = dMV.x;
                pMfxCurrMB->MV[4][1] = dMV.y;

                break;

            case UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_FB:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_2;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                pCompressedMB->GetdMV(&dMV, false, 0);

                pMfxCurrMB->MV[4][0] = dMV.x;
                pMfxCurrMB->MV[4][1] = dMV.y;
                break;
            default:
                assert(0);
                return MFX_ERR_UNSUPPORTED;
            }

            pMFXRes     += MBResLen;
        }
    }

    return MFXSts;
}

mfxStatus MFXVideoEncVc1::PutResidualsIntoCuc_PField(mfxFrameCUC *cuc, mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;


    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;
    Ipp32u       h = pFrameParam->FrameHinMbMinus1 + 1;
    Ipp32u       w = pFrameParam->FrameWinMbMinus1 + 1;

    UMC_VC1_ENCODER::sCoordinate dMV = {0};

    mfxI32       MBResLen = 64*6;
    mfxI32       MBHybridLen = 1;
    mfxI32       MBACPredLen = 1;

    mfxI32       MBResOffset = 0;
    mfxI32       MBHybridOffset = 0;
    mfxI32       MBACPredOffset = 0;

    nRows = (nRows>0)? nRows: h;

    if (cuc->NumSlice>1)
    {
        MBResOffset    = MBResLen   *firstRow*w;
        MBHybridOffset = MBHybridLen*firstRow*w;
        MBACPredOffset = MBACPredLen*firstRow*w;
    }

    n = GetExBufferIndex(cuc,  MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB+
                                                            w*(h*pFrameParam->SecondFieldFlag + firstRow);
    mfxMbCodeVC1*                           pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    pCompressedMB = &m_pVC1EncoderCodedMB[w*h*pFrameParam->SecondFieldFlag];
    pMfxCurrMB = &cuc->MbParam->Mb[w*h*pFrameParam->SecondFieldFlag].VC1;

    for (i = 0; i< nRows; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pMfxCurrMB = &cuc->MbParam->Mb[j + i*w].VC1;

            TempBlock.InitBlocks(pMFXRes);
            TempBlock.Reset();

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->GetResiduals (TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }

            pMfxCurrMB->FieldMbFlag   = 0;
            pMfxCurrMB->SubMbPredMode = 0;
            pMfxCurrMB->SubMbPredMode = pCompressedMB->GetIntraPattern();

            GetMFX_CBPCY(pCompressedMB->GetMBCBPCY(), &pMfxCurrMB->CodedPattern4x4Y,
                &pMfxCurrMB->CodedPattern4x4U,  &pMfxCurrMB->CodedPattern4x4V);

            switch(pCompressedMB->GetMBType())
            {
            case UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTRA_VC1;
                pMfxCurrMB->IntraMbFlag   = 1;
                pMfxCurrMB->Skip8x8Flag   = 0;

                pMfxCurrMB->AcPredFlag = (mfxU8)pCompressedMB->GetACPrediction();
                break;
            case UMC_VC1_ENCODER::VC1_ENC_P_MB_1MV:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_0;
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                pMfxCurrMB->RefPicSelect[0][0] = dMV.bSecond;
                break;

     case UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_1MV:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                pMfxCurrMB->RefPicSelect[0][0] = dMV.bSecond;

                break;

            default:
                assert(0);
                return MFX_ERR_UNSUPPORTED;
            }

            pMFXRes += 6*64;

            pCompressedMB++;
            pMfxCurrMB++;

        }
    }

    return MFXSts;
}


mfxStatus MFXVideoEncVc1::PutResidualsIntoCuc_PMixedFrame(mfxFrameCUC *cuc, mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;
    Ipp32u       h = pFrameParam->FrameHinMbMinus1 + 1;
    Ipp32u       w = pFrameParam->FrameWinMbMinus1 + 1;

    UMC_VC1_ENCODER::sCoordinate dMV = {0};

    mfxI32       MBResLen = 64*6;
    mfxI32       MBHybridLen = 1;
    mfxI32       MBACPredLen = 1;

    mfxI32       MBResOffset = 0;
    mfxI32       MBHybridOffset = 0;
    mfxI32       MBACPredOffset = 0;

    nRows = (nRows>0)? nRows: h;

    if (cuc->NumSlice>1)
    {
        MBResOffset    = MBResLen   *firstRow*w;
        MBHybridOffset = MBHybridLen*firstRow*w;
        MBACPredOffset = MBACPredLen*firstRow*w;
    }

    //residuals
    n = GetExBufferIndex(cuc,  MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;
    MFX_CHECK_NULL_PTR1(pMFXRes)

    pMFXRes += MBResOffset;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB+
                                                            w*firstRow;
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    for (i = 0; i< nRows; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pMfxCurrMB = &cuc->MbParam->Mb[j + i*w].VC1;

            TempBlock.InitBlocks(pMFXRes);
            TempBlock.Reset();

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->GetResiduals (TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }

            pMfxCurrMB->FieldMbFlag   = 0;
            pMfxCurrMB->SubMbPredMode = pCompressedMB->GetIntraPattern();

            GetMFX_CBPCY(pCompressedMB->GetMBCBPCY(), &pMfxCurrMB->CodedPattern4x4Y,
                &pMfxCurrMB->CodedPattern4x4U,  &pMfxCurrMB->CodedPattern4x4V);

            switch(pCompressedMB->GetMBType())
            {
            case UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTRA_VC1;
                pMfxCurrMB->IntraMbFlag   = 1;
                pMfxCurrMB->Skip8x8Flag   = 0;

                pMfxCurrMB->AcPredFlag = (mfxU8)pCompressedMB->GetACPrediction();
                break;
            case UMC_VC1_ENCODER::VC1_ENC_P_MB_1MV:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_0;
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                break;

     case UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_1MV:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                break;
            case UMC_VC1_ENCODER::VC1_ENC_P_MB_4MV:

                if(!pMfxCurrMB->SubMbPredMode)
                    pMfxCurrMB->MbType    = MFX_MBTYPE_INTER_8X8_0;
                else
                    pMfxCurrMB->MbType    = MFX_MBTYPE_INTER_MIX_INTRA;

                pMfxCurrMB->IntraMbFlag   = 0;

                pMfxCurrMB->Skip8x8Flag   = 0;
                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                for(mfxU8 blk = 0; blk < 4; blk++)
                {
                    pCompressedMB->GetdMV(&dMV, true, blk);

                    pMfxCurrMB->MV[blk][0] = dMV.x;
                    pMfxCurrMB->MV[blk][1] = dMV.y;
                }

                break;
            case UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_4MV:
                pMfxCurrMB->MbType    = MFX_MBTYPE_INTER_8X8_0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                pMfxCurrMB->Skip8x8Flag   = 15;

                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;

                pCompressedMB->GetdMV(&dMV, true, 0);

                for(mfxU8 blk = 0; blk < 4; blk++)
                {
                    pCompressedMB->GetdMV(&dMV, true, blk);

                    pMfxCurrMB->MV[blk][0] = dMV.x;
                    pMfxCurrMB->MV[blk][1] = dMV.y;
                }

                break;
            default:
                assert(0);
                return MFX_ERR_UNSUPPORTED;
            }

            pMFXRes     += MBResLen;
        }
    }

    return MFXSts;
}

mfxStatus MFXVideoEncVc1::PutResidualsIntoCuc_PFrame(mfxFrameCUC *cuc, mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;
    Ipp32u       h = pFrameParam->FrameHinMbMinus1 + 1;
    Ipp32u       w = pFrameParam->FrameWinMbMinus1 + 1;

    UMC_VC1_ENCODER::sCoordinate dMV = {0};

    mfxI32       MBResLen = 64*6;
    mfxI32       MBHybridLen = 1;
    mfxI32       MBACPredLen = 1;

    mfxI32       MBResOffset = 0;
    mfxI32       MBHybridOffset = 0;
    mfxI32       MBACPredOffset = 0;

    nRows = (nRows>0)? nRows: h;

    if (cuc->NumSlice>1)
    {
        MBResOffset    = MBResLen   *firstRow*w;
        MBHybridOffset = MBHybridLen*firstRow*w;
        MBACPredOffset = MBACPredLen*firstRow*w;
    }

    n = GetExBufferIndex(cuc,  MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;
    MFX_CHECK_NULL_PTR1(pMFXRes)

    pMFXRes += MBResOffset;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB+
                                                            w*firstRow;
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    for (i = 0; i< nRows; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pMfxCurrMB = &cuc->MbParam->Mb[j + i*w].VC1;

            TempBlock.InitBlocks(pMFXRes);
            TempBlock.Reset();

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->GetResiduals (TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }

            pMfxCurrMB->FieldMbFlag   = 0;
            pMfxCurrMB->SubMbPredMode = 0;
            pMfxCurrMB->SubMbPredMode = pCompressedMB->GetIntraPattern();

            GetMFX_CBPCY(pCompressedMB->GetMBCBPCY(), &pMfxCurrMB->CodedPattern4x4Y,
                &pMfxCurrMB->CodedPattern4x4U,  &pMfxCurrMB->CodedPattern4x4V);

            switch(pCompressedMB->GetMBType())
            {
            case UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTRA_VC1;
                pMfxCurrMB->IntraMbFlag   = 1;
                pMfxCurrMB->Skip8x8Flag   = 0;

                pMfxCurrMB->AcPredFlag = (mfxU8)pCompressedMB->GetACPrediction();
                break;
            case UMC_VC1_ENCODER::VC1_ENC_P_MB_1MV:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_0;

                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                break;
            case UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_1MV:
                pMfxCurrMB->MbType = MFX_MBTYPE_INTER_16X16_0;

                GetMFXHybrid(pCompressedMB, &pMfxCurrMB->HybridMvFlags);
                GetMFXTransformType(pCompressedMB->GetTSType(), &pMfxCurrMB->SubMbShape,
                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                pMfxCurrMB->IntraMbFlag   = 0;
                pMfxCurrMB->Skip8x8Flag   = 15;

                pCompressedMB->GetdMV(&dMV, true, 0);

                pMfxCurrMB->MV[0][0] = dMV.x;
                pMfxCurrMB->MV[0][1] = dMV.y;

                break;
            default:
                assert(0);
                return MFX_ERR_UNSUPPORTED;
            }

            pMFXRes     += MBResLen;
        }
    }
    return MFXSts;
}


mfxStatus MFXVideoEncVc1::PutResidualsIntoCuc_IField(mfxFrameCUC *cuc, mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    Ipp32u       h = pFrameParam->FrameHinMbMinus1 + 1;
    Ipp32u       w = pFrameParam->FrameWinMbMinus1 + 1;


    mfxI32       MBResLen = 64*6;
    mfxI32       MBHybridLen = 1;
    mfxI32       MBACPredLen = 1;

    mfxI32       MBResOffset = 0;
    mfxI32       MBHybridOffset = 0;
    mfxI32       MBACPredOffset = 0;

    nRows = (nRows>0)? nRows: h;

    if (cuc->NumSlice>1)
    {
        MBResOffset    = MBResLen   *firstRow*w;
        MBHybridOffset = MBHybridLen*firstRow*w;
        MBACPredOffset = MBACPredLen*firstRow*w;
    }

    //residuals

    n = GetExBufferIndex(cuc,  MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;
    MFX_CHECK_NULL_PTR1(pMFXRes)

    pMFXRes += MBResOffset;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;

    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB +
                                                            w*(h*pFrameParam->SecondFieldFlag + firstRow);
    mfxMbCodeVC1*                         pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    for (i = 0; i< nRows; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pMfxCurrMB = &cuc->MbParam->Mb[j + i*w].VC1;

            TempBlock.InitBlocks(pMFXRes);
            TempBlock.Reset();

            pMfxCurrMB->MbType        = MFX_MBTYPE_INTRA_VC1;

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->GetResiduals (TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }

            pMfxCurrMB->HybridMvFlags = 0;
            pMfxCurrMB->SubMbShape = 0;
            pMfxCurrMB->SubMbShapeU = 0;
            pMfxCurrMB->SubMbShapeV = 0;

            pMfxCurrMB->FieldMbFlag   = 0;
            pMfxCurrMB->SubMbPredMode = 0;
            pMfxCurrMB->SubMbPredMode = pCompressedMB->GetIntraPattern();
            pMfxCurrMB->IntraMbFlag   = 1;
            pMfxCurrMB->Skip8x8Flag   = 0;

            pMfxCurrMB->AcPredFlag = (mfxU8)pCompressedMB->GetACPrediction();

            GetMFX_CBPCY(pCompressedMB->GetMBCBPCY(), &pMfxCurrMB->CodedPattern4x4Y,
                &pMfxCurrMB->CodedPattern4x4U,  &pMfxCurrMB->CodedPattern4x4V);

            pMFXRes     += MBResLen;
        }
    }
    return MFXSts;
}

mfxStatus MFXVideoEncVc1::PutResidualsIntoCuc_IFrame(mfxFrameCUC *cuc, mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;
    Ipp32u       h = pFrameParam->FrameHinMbMinus1 + 1;
    Ipp32u       w = pFrameParam->FrameWinMbMinus1 + 1;
    mfxI32       MBResLen = 64*6;
    mfxI32       MBHybridLen = 1;
    mfxI32       MBACPredLen = 1;

    mfxI32       MBResOffset = 0;
    mfxI32       MBHybridOffset = 0;
    mfxI32       MBACPredOffset = 0;

    nRows = (nRows>0)? nRows: h;

    if (cuc->NumSlice>1)
    {
        MBResOffset    = MBResLen   *firstRow*w;
        MBHybridOffset = MBHybridLen*firstRow*w;
        MBACPredOffset = MBACPredLen*firstRow*w;
    }
    n = GetExBufferIndex(cuc, MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;
    MFX_CHECK_NULL_PTR1(pMFXRes)

    pMFXRes += MBResOffset;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB+
                                                            w*(h*pFrameParam->SecondFieldFlag + firstRow);
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    for (i = 0; i< nRows; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pMfxCurrMB    = &cuc->MbParam->Mb[j + i*w].VC1;

            TempBlock.InitBlocks(pMFXRes);
            TempBlock.Reset();

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->GetResiduals (TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }
            pMfxCurrMB->HybridMvFlags = 0;
            pMfxCurrMB->SubMbShape = 0;
            pMfxCurrMB->SubMbShapeU = 0;
            pMfxCurrMB->SubMbShapeV = 0;

            pMfxCurrMB->MbType        = MFX_MBTYPE_INTRA_VC1;
            pMfxCurrMB->FieldMbFlag   = 0;
            pMfxCurrMB->SubMbPredMode = 0;
            pMfxCurrMB->SubMbPredMode = pCompressedMB->GetIntraPattern();
            pMfxCurrMB->IntraMbFlag   = 1;
            pMfxCurrMB->Skip8x8Flag   = 0;

            pMfxCurrMB->AcPredFlag = (mfxU8)pCompressedMB->GetACPrediction();

            GetMFX_CBPCY(pCompressedMB->GetMBCBPCY(), &pMfxCurrMB->CodedPattern4x4Y,
                &pMfxCurrMB->CodedPattern4x4U,  &pMfxCurrMB->CodedPattern4x4V);

            pMFXRes     += MBResLen;
        }
    }
    return MFXSts;
}

mfxStatus MFXVideoEncVc1::PutMEIntoCUC(UMC::MeParams* MEParams, mfxFrameCUC *cuc, UMC_VC1_ENCODER::ePType UMCtype)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    if(MEParams->ChangeInterpPixelType)
    {
        if(MEParams->MbPart == UMC::ME_Mb16x16 && MEParams->PixelType == UMC::ME_HalfPixel
            && MEParams->Interpolation == UMC::ME_VC1_Bilinear)
            pFrameParam->UnifiedMvMode = MFX_VC1_ENC_1MV_HALF_BILINEAR;
        else
            if(MEParams->MbPart == UMC::ME_Mb16x16 && MEParams->PixelType == UMC::ME_QuarterPixel
                && MEParams->Interpolation == UMC::ME_VC1_Bicubic)
                 pFrameParam->UnifiedMvMode = MFX_VC1_ENC_1MV;
            else
                if(MEParams->MbPart == UMC::ME_Mb16x16 && MEParams->PixelType == UMC::ME_HalfPixel
                    && MEParams->Interpolation == UMC::ME_VC1_Bicubic)
                    pFrameParam->UnifiedMvMode = MFX_VC1_ENC_1MV_HALF_BICUBIC;
                else
                    if(MEParams->MbPart == UMC::ME_Mb8x8 && MEParams->PixelType == UMC::ME_QuarterPixel
                        && MEParams->Interpolation == UMC::ME_VC1_Bicubic)
                        pFrameParam->UnifiedMvMode = MFX_VC1_ENC_MIXED_MV;
    }
    if(MEParams->SelectVlcTables)
    {
        pFrameParam->MvTable        = MEParams->OutMvTableIndex;
        pFrameParam->TransACTable   = MEParams->OutAcTableIndex;
        pFrameParam->CBPTable       = MEParams->OutCbpcyTableIndex;
    }

    /*------------- Trellis quantization ------------------ */
    {
        mfxI8* pRC = 0;
        Ipp32u  h = (pFrameParam->FrameHinMbMinus1 + 1);
        Ipp32u  w = (pFrameParam->FrameWinMbMinus1 + 1);

        GetMBRoundControl (cuc, &pRC);
        if (pRC)
        {
            if (MEParams->UseTrellisQuantization)
            {
                for (int i = 0; i < h*w; i++)
                {
                    for (int blk = 0; blk < 6; blk++)
                    {
                        MFX_INTERNAL_CPY(pRC,MEParams->pSrc->MBs[i].RoundControl[blk],sizeof(mfxI8)*64);
                        pRC += 64;
                    }
                }
            }
            else
            {
                memset (pRC,0,h*w*6*64*sizeof(mfxI8));
            }
        }
    }



    if(m_profile == MFX_PROFILE_VC1_ADVANCED)
    {
        switch(UMCtype)
        {
        case UMC_VC1_ENCODER::VC1_ENC_I_FRAME:
        case UMC_VC1_ENCODER::VC1_ENC_I_I_FIELD:
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_FRAME:
            MFXSts = PutMEIntoCUC_P(MEParams, cuc);
            break;
         case UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD:
             MFXSts = PutMEIntoCUC_PField(MEParams, cuc);
            break;
         case UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD:
             if(pFrameParam->SecondFieldFlag)
                 MFXSts = PutMEIntoCUC_PField(MEParams, cuc);
            break;
         case UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD:
             if(!pFrameParam->SecondFieldFlag)
                 MFXSts = PutMEIntoCUC_PField(MEParams, cuc);
            break;
         case UMC_VC1_ENCODER::VC1_ENC_B_FRAME:
             MFXSts = PutMEIntoCUC_B(MEParams, cuc);
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_B_FIELD:
            MFXSts = PutMEIntoCUC_BField(MEParams, cuc);
            break;
         case UMC_VC1_ENCODER::VC1_ENC_BI_B_FIELD:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
         case UMC_VC1_ENCODER::VC1_ENC_B_BI_FIELD:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
         case UMC_VC1_ENCODER::VC1_ENC_BI_FRAME:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
         case UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
         default:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
        }
    }
    else
    {
        switch(UMCtype)
        {
        case UMC_VC1_ENCODER::VC1_ENC_I_FRAME:
            break;
         case UMC_VC1_ENCODER::VC1_ENC_P_FRAME:
             MFXSts = PutMEIntoCUC_P(MEParams, cuc);
             break;
         case UMC_VC1_ENCODER::VC1_ENC_B_FRAME:
             MFXSts = PutMEIntoCUC_B(MEParams, cuc);
            break;
         case UMC_VC1_ENCODER::VC1_ENC_BI_FRAME:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
         case UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
         default:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
        }
    }

    return MFXSts;
}

mfxStatus MFXVideoEncVc1::PutMEIntoCUC_P(UMC::MeParams* pMEParams, mfxFrameCUC *cuc)
{
    mfxU32 i=0,j =0;

    MFX_CHECK_COND(pMEParams->LastMB < cuc->MbParam->NumMb && pMEParams->LastMB > pMEParams->FirstMB)

    for (i = pMEParams->FirstMB; i<= pMEParams->LastMB; i++)
    {
      mfxMbCodeVC1      *pMfxCurrMB = &cuc->MbParam->Mb[i].VC1;
      UMC::MeMB         *pMECurrMB  = &pMEParams->pSrc->MBs[i];

          pMfxCurrMB->TransformFlag = 0;
          pMfxCurrMB->FieldMbFlag   = 0;
          pMfxCurrMB->SubMbPredMode = 0;

          switch (pMECurrMB->MbType)
          {
            case UMC::ME_MbIntra:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTRA_VC1;
                pMfxCurrMB->Skip8x8Flag   = 0;
                pMfxCurrMB->SubMbShape    = 0;
                pMfxCurrMB->SubMbShapeU   = 0;
                pMfxCurrMB->SubMbShapeV   = 0;
                break;
            case UMC::ME_MbFrw:
                if (pMECurrMB->MbPart == UMC::ME_Mb8x8)
                {
                    pMfxCurrMB->MbType    = MFX_MBTYPE_INTER_8X8_0;
                    pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
                    pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
                    pMfxCurrMB->MV[1][0] = pMECurrMB->MV[0][1].x;
                    pMfxCurrMB->MV[1][1] = pMECurrMB->MV[0][1].y;
                    pMfxCurrMB->MV[2][0] = pMECurrMB->MV[0][2].x;
                    pMfxCurrMB->MV[2][1] = pMECurrMB->MV[0][2].y;
                    pMfxCurrMB->MV[3][0] = pMECurrMB->MV[0][3].x;
                    pMfxCurrMB->MV[3][1] = pMECurrMB->MV[0][3].y;
                }
                else
                {
                    pMfxCurrMB->MbType   = MFX_MBTYPE_INTER_16X16_0; //forward
                    pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
                    pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
                }

                pMfxCurrMB->Skip8x8Flag   = 0;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                break;
            case UMC::ME_MbMixed:

                pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_MIX_INTRA;
                pMfxCurrMB->Skip8x8Flag = 0;

                pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
                pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
                pMfxCurrMB->MV[1][0] = pMECurrMB->MV[0][1].x;
                pMfxCurrMB->MV[1][1] = pMECurrMB->MV[0][1].y;
                pMfxCurrMB->MV[2][0] = pMECurrMB->MV[0][2].x;
                pMfxCurrMB->MV[2][1] = pMECurrMB->MV[0][2].y;
                pMfxCurrMB->MV[3][0] = pMECurrMB->MV[0][3].x;
                pMfxCurrMB->MV[3][1] = pMECurrMB->MV[0][3].y;


                GetMFXIntraPattern(pMECurrMB, &pMfxCurrMB->SubMbPredMode);
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                break;
            case UMC::ME_MbFrwSkipped:
                if (pMECurrMB->MbPart == UMC::ME_Mb8x8)
                {
                    pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_8X8_0;

                    pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
                    pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
                    pMfxCurrMB->MV[1][0] = pMECurrMB->MV[0][1].x;
                    pMfxCurrMB->MV[1][1] = pMECurrMB->MV[0][1].y;
                    pMfxCurrMB->MV[2][0] = pMECurrMB->MV[0][2].x;
                    pMfxCurrMB->MV[2][1] = pMECurrMB->MV[0][2].y;
                    pMfxCurrMB->MV[3][0] = pMECurrMB->MV[0][3].x;
                    pMfxCurrMB->MV[3][1] = pMECurrMB->MV[0][3].y;
                }
                else
                {
                    pMfxCurrMB->MbType   = MFX_MBTYPE_INTER_16X16_0; //forward
                    pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
                    pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
                }

                pMfxCurrMB->Skip8x8Flag   = 15;
                pMfxCurrMB->SubMbShape    = 0; //Transform type for luma:   ((Y3<<6)| (Y2<<4)| (Y1<<2) | Y0)
                                                                         //Values are on the table on p. 89
                pMfxCurrMB->SubMbShapeU   = 0;
                pMfxCurrMB->SubMbShapeV   = 0;

                break;

            default:
                return MFX_ERR_UNSUPPORTED;
          }
        }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoEncVc1::PutMEIntoCUC_B(UMC::MeParams* pMEParams, mfxFrameCUC *cuc)
{
    mfxU32 i=0,j =0;

    MFX_CHECK_COND(pMEParams->LastMB < cuc->MbParam->NumMb && pMEParams->LastMB > pMEParams->FirstMB)

    for (i = pMEParams->FirstMB; i<=pMEParams->LastMB; i++)
    {
      mfxMbCodeVC1      *pMfxCurrMB = &cuc->MbParam->Mb[i].VC1;
      UMC::MeMB         *pMECurrMB  = &pMEParams->pSrc->MBs[i];

          pMfxCurrMB->TransformFlag = 0;
          pMfxCurrMB->FieldMbFlag   = 0;
          pMfxCurrMB->SubMbPredMode = 0;

          pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
          pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
          pMfxCurrMB->MV[4][0] = pMECurrMB->MV[1][0].x;
          pMfxCurrMB->MV[4][1] = pMECurrMB->MV[1][0].y;

          switch (pMECurrMB->MbType)
          {
            case UMC::ME_MbIntra:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTRA_VC1;
                pMfxCurrMB->Skip8x8Flag   = 0;
                pMfxCurrMB->SubMbShape    = 0;
                pMfxCurrMB->SubMbShapeU   = 0;
                pMfxCurrMB->SubMbShapeV   = 0;
                break;

            case UMC::ME_MbFrw:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTER_16X16_0; //forward
                pMfxCurrMB->Skip8x8Flag   = 0;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                break;

            case UMC::ME_MbFrwSkipped:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTER_16X16_0; //forward
                pMfxCurrMB->Skip8x8Flag   = 15;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                break;

            case UMC::ME_MbBkw:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTER_16X16_1;
                pMfxCurrMB->Skip8x8Flag   = 0;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                break;

            case UMC::ME_MbBkwSkipped:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTER_16X16_1;
                pMfxCurrMB->Skip8x8Flag   = 15;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                break;
            case UMC::ME_MbBidir:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTER_16X16_2;
                pMfxCurrMB->Skip8x8Flag   = 0;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                break;

            case UMC::ME_MbBidirSkipped:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTER_16X16_2;
                pMfxCurrMB->Skip8x8Flag   = 15;
                 GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                break;

            case UMC::ME_MbDirect:

                pMfxCurrMB->MbType        = MFX_MBTYPE_INTER_16X16_DIR;
                pMfxCurrMB->Skip8x8Flag   = 0;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                break;
            case UMC::ME_MbDirectSkipped:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTER_16X16_DIR;
                pMfxCurrMB->Skip8x8Flag   = 15;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                break;

            default:
                return MFX_ERR_UNSUPPORTED;
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncVc1::PutMEIntoCUC_PField(UMC::MeParams* pMEParams, mfxFrameCUC *cuc)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    mfxI32 i = 0;
   // mfxI32 j = 0; //index for MV pred

    MFX_CHECK_COND(pMEParams->LastMB < cuc->MbParam->NumMb && pMEParams->LastMB > pMEParams->FirstMB)

        for (i = pMEParams->FirstMB; i<= pMEParams->LastMB; i++)
        {
          //  j = 4*i*sizeof(sMV);

            mfxMbCodeVC1      *pMfxCurrMB = &cuc->MbParam->Mb[i].VC1;
            UMC::MeMB         *pMECurrMB  = &pMEParams->pSrc->MBs[i];

            pMfxCurrMB->TransformFlag = 0;
            pMfxCurrMB->FieldMbFlag   = 0;
            pMfxCurrMB->SubMbPredMode = 0;

            switch (pMECurrMB->MbType)
            {
            case UMC::ME_MbIntra:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTRA_VC1;
                pMfxCurrMB->Skip8x8Flag   = 0;
                pMfxCurrMB->SubMbShape    = 0;
                pMfxCurrMB->SubMbShapeU   = 0;
                pMfxCurrMB->SubMbShapeV   = 0;

                break;
            case UMC::ME_MbFrw:
                if (pMECurrMB->MbPart == UMC::ME_Mb8x8)
                {
                    pMfxCurrMB->MbType    = MFX_MBTYPE_INTER_8X8_0;

                    pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
                    pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
                    pMfxCurrMB->MV[1][0] = pMECurrMB->MV[0][1].x;
                    pMfxCurrMB->MV[1][1] = pMECurrMB->MV[0][1].y;
                    pMfxCurrMB->MV[2][0] = pMECurrMB->MV[0][2].x;
                    pMfxCurrMB->MV[2][1] = pMECurrMB->MV[0][2].y;
                    pMfxCurrMB->MV[3][0] = pMECurrMB->MV[0][3].x;
                    pMfxCurrMB->MV[3][1] = pMECurrMB->MV[0][3].y;

                    pMfxCurrMB->RefPicSelect[0][0] = pMECurrMB->Refindex[0][0];
                    pMfxCurrMB->RefPicSelect[0][1] = pMECurrMB->Refindex[0][1];
                    pMfxCurrMB->RefPicSelect[0][2] = pMECurrMB->Refindex[0][2];
                    pMfxCurrMB->RefPicSelect[0][3] = pMECurrMB->Refindex[0][3];


                }
                else
                {

                    pMfxCurrMB->MbType   = MFX_MBTYPE_INTER_16X16_0; //forward
                    pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
                    pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
                    pMfxCurrMB->RefPicSelect[0][0] = pMECurrMB->Refindex[0][0];
                }

                pMfxCurrMB->Skip8x8Flag   = 0;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                break;
            case UMC::ME_MbFrwSkipped:
                pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_16X16_0; //forward
                pMfxCurrMB->Skip8x8Flag = 15;

                if (pMECurrMB->MbPart == UMC::ME_Mb8x8)
                {
                    pMfxCurrMB->MbType    = MFX_MBTYPE_INTER_8X8_0;

                    pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
                    pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
                    pMfxCurrMB->MV[1][0] = pMECurrMB->MV[0][1].x;
                    pMfxCurrMB->MV[1][1] = pMECurrMB->MV[0][1].y;
                    pMfxCurrMB->MV[2][0] = pMECurrMB->MV[0][2].x;
                    pMfxCurrMB->MV[2][1] = pMECurrMB->MV[0][2].y;
                    pMfxCurrMB->MV[3][0] = pMECurrMB->MV[0][3].x;
                    pMfxCurrMB->MV[3][1] = pMECurrMB->MV[0][3].y;

                    pMfxCurrMB->RefPicSelect[0][0] = pMECurrMB->Refindex[0][0];
                    pMfxCurrMB->RefPicSelect[0][1] = pMECurrMB->Refindex[0][1];
                    pMfxCurrMB->RefPicSelect[0][2] = pMECurrMB->Refindex[0][2];
                    pMfxCurrMB->RefPicSelect[0][3] = pMECurrMB->Refindex[0][3];

                }
                else
                {

                    pMfxCurrMB->MbType   = MFX_MBTYPE_INTER_16X16_0; //forward

                    pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
                    pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;

                    pMfxCurrMB->RefPicSelect[0][0] = pMECurrMB->Refindex[0][0];
                }

                pMfxCurrMB->SubMbShape    = 0;
                pMfxCurrMB->SubMbShapeU   = 0;
                pMfxCurrMB->SubMbShapeV   = 0;
                break;
            default:
                return MFX_ERR_UNSUPPORTED;
            }
        }
        return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncVc1::PutMEIntoCUC_BField(UMC::MeParams* pMEParams, mfxFrameCUC *cuc)
{
    mfxI32 i=0;

    MFX_CHECK_COND(pMEParams->LastMB < cuc->MbParam->NumMb && pMEParams->LastMB > pMEParams->FirstMB)

    for (i = pMEParams->FirstMB; i<= pMEParams->LastMB; i++)
    {
      mfxMbCodeVC1      *pMfxCurrMB = &cuc->MbParam->Mb[i].VC1;
      UMC::MeMB         *pMECurrMB  = &pMEParams->pSrc->MBs[i];

          pMfxCurrMB->TransformFlag = 0;
          pMfxCurrMB->FieldMbFlag   = 0;
          pMfxCurrMB->SubMbPredMode = 0;
          pMfxCurrMB->SubMbPredMode = 0;

          if (pMECurrMB->MbPart == UMC::ME_Mb8x8)
          {
              //forward
              pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
              pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
              pMfxCurrMB->MV[1][0] = pMECurrMB->MV[0][1].x;
              pMfxCurrMB->MV[1][1] = pMECurrMB->MV[0][1].y;
              pMfxCurrMB->MV[2][0] = pMECurrMB->MV[0][2].x;
              pMfxCurrMB->MV[2][1] = pMECurrMB->MV[0][2].y;
              pMfxCurrMB->MV[3][0] = pMECurrMB->MV[0][3].x;
              pMfxCurrMB->MV[3][1] = pMECurrMB->MV[0][3].y;

              pMfxCurrMB->RefPicSelect[0][0] = pMECurrMB->Refindex[0][0];
              pMfxCurrMB->RefPicSelect[0][1] = pMECurrMB->Refindex[0][1];
              pMfxCurrMB->RefPicSelect[0][2] = pMECurrMB->Refindex[0][2];
              pMfxCurrMB->RefPicSelect[0][3] = pMECurrMB->Refindex[0][3];

              //backward
              pMfxCurrMB->MV[4][0] = pMECurrMB->MV[1][0].x;
              pMfxCurrMB->MV[4][1] = pMECurrMB->MV[1][0].y;
              pMfxCurrMB->MV[5][0] = pMECurrMB->MV[1][1].x;
              pMfxCurrMB->MV[5][1] = pMECurrMB->MV[1][1].y;
              pMfxCurrMB->MV[6][0] = pMECurrMB->MV[1][2].x;
              pMfxCurrMB->MV[6][1] = pMECurrMB->MV[1][2].y;
              pMfxCurrMB->MV[7][0] = pMECurrMB->MV[1][3].x;
              pMfxCurrMB->MV[7][1] = pMECurrMB->MV[1][3].y;

              pMfxCurrMB->RefPicSelect[1][0] = pMECurrMB->Refindex[1][0];
              pMfxCurrMB->RefPicSelect[1][1] = pMECurrMB->Refindex[1][1];
              pMfxCurrMB->RefPicSelect[1][2] = pMECurrMB->Refindex[1][2];
              pMfxCurrMB->RefPicSelect[1][3] = pMECurrMB->Refindex[1][3];
          }
          else
          {
              pMfxCurrMB->MV[0][0] = pMECurrMB->MV[0][0].x;
              pMfxCurrMB->MV[0][1] = pMECurrMB->MV[0][0].y;
              pMfxCurrMB->MV[4][0] = pMECurrMB->MV[1][0].x;
              pMfxCurrMB->MV[4][1] = pMECurrMB->MV[1][0].y;

              pMfxCurrMB->RefPicSelect[0][0] = pMECurrMB->Refindex[0][0];
              pMfxCurrMB->RefPicSelect[1][0] = pMECurrMB->Refindex[1][0];
          }

          switch (pMECurrMB->MbType)
          {
            case UMC::ME_MbIntra:
                pMfxCurrMB->MbType        = MFX_MBTYPE_INTRA_VC1;
                pMfxCurrMB->Skip8x8Flag   = 0;
                pMfxCurrMB->SubMbShape    = 0;
                pMfxCurrMB->SubMbShapeU   = 0;
                pMfxCurrMB->SubMbShapeV   = 0;

                break;

            case UMC::ME_MbFrw:
                if(pMECurrMB->MbPart == UMC::ME_Mb8x8)
                {
                    pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_FIELD_8X8_00; //forward
                    pMfxCurrMB->SubMbPredMode = 0x0;
                }
                else
                    pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_16X16_0; //forward

                pMfxCurrMB->Skip8x8Flag   = 0;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                break;

            case UMC::ME_MbFrwSkipped:
                if(pMECurrMB->MbPart == UMC::ME_Mb8x8)
                {
                    pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_FIELD_8X8_00; //forward
                    pMfxCurrMB->SubMbPredMode = 0x0;
                }
                else
                    pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_16X16_0; //forward

                pMfxCurrMB->Skip8x8Flag = 15;

                pMfxCurrMB->SubMbShape    = 0;
                pMfxCurrMB->SubMbShapeU   = 0;
                pMfxCurrMB->SubMbShapeV   = 0;
                break;

            case UMC::ME_MbBkw:
                if(pMECurrMB->MbPart == UMC::ME_Mb8x8)
                {
                    pMfxCurrMB->MbType        = MFX_MBTYPE_INTER_FIELD_8X8_00; //backward
                    pMfxCurrMB->SubMbPredMode = 0x55;
                }
                else
                    pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_16X16_1; //backward

                pMfxCurrMB->Skip8x8Flag = 0;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                break;

            case UMC::ME_MbBkwSkipped:
                if(pMECurrMB->MbPart == UMC::ME_Mb8x8)
                {
                    pMfxCurrMB->MbType        = MFX_MBTYPE_INTER_FIELD_8X8_00; //backward
                    pMfxCurrMB->SubMbPredMode = 0x55;
                }
                else
                    pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_16X16_1; //backward

                pMfxCurrMB->Skip8x8Flag = 15;
                pMfxCurrMB->SubMbShape    = 0;
                pMfxCurrMB->SubMbShapeU   = 0;
                pMfxCurrMB->SubMbShapeV   = 0;
                break;
            case UMC::ME_MbBidir:
                pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_16X16_2;
                pMfxCurrMB->Skip8x8Flag = 0;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);

                break;

            case UMC::ME_MbBidirSkipped:
                pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_16X16_2;
                pMfxCurrMB->Skip8x8Flag = 15;
                pMfxCurrMB->SubMbShape    = 0;
                pMfxCurrMB->SubMbShapeU   = 0;
                pMfxCurrMB->SubMbShapeV   = 0;
                break;

            case UMC::ME_MbDirect:
                pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_16X16_DIR;
                pMfxCurrMB->Skip8x8Flag = 0;
                GetMFXTransformType(pMECurrMB->BlockTrans,    &pMfxCurrMB->SubMbShape,
                                    &pMfxCurrMB->SubMbShapeU, &pMfxCurrMB->SubMbShapeV);
                break;
            case UMC::ME_MbDirectSkipped:

                pMfxCurrMB->MbType      = MFX_MBTYPE_INTER_16X16_DIR;
                pMfxCurrMB->Skip8x8Flag = 15;
                pMfxCurrMB->SubMbShape    = 0;
                pMfxCurrMB->SubMbShapeU   = 0;
                pMfxCurrMB->SubMbShapeV   = 0;
                break;

            default:
                return MFX_ERR_UNSUPPORTED;
          }


    }
    return MFX_ERR_NONE;
}




mfxStatus MFXVideoEncVc1::SetMEParams(mfxFrameCUC *cuc, mfxVideoParam * pPar,UMC::MeParams* MEParams,mfxFrameData** refData, mfxFrameInfo* pRefInfo, UMC::MeFrame**  pMEFrames)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    bool FieldPicture = false;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    MFX_INTERNAL_CPY(&m_FrameParam.VC1, pFrameParam, sizeof(mfxFrameParamVC1));

    // TO DO: add parameter into ME which show VS transform for frame
    // if (pFrameParam->TTMBF) then ME should calculate TS types for each block;
    UMC_VC1_ENCODER::ePType UMCtype;

    MFXSts = GetUMCPictureType(pFrameParam->FrameType,pFrameParam->FrameType2nd,
                               pFrameParam->FieldPicFlag, pFrameParam->InterlacedFrameFlag,
                               pFrameParam->Pic4MvAllowed,&UMCtype);
    MFX_CHECK_STS(MFXSts);

    FieldPicture = IsFieldPicture(UMCtype);

    if(m_profile == MFX_PROFILE_VC1_ADVANCED)
    {
        MFXSts = SetMEParams_ADV(cuc, pPar, MEParams, FieldPicture);
        MFX_CHECK_STS(MFXSts);


        switch(UMCtype)
        {
        case UMC_VC1_ENCODER::VC1_ENC_I_FRAME:
        case UMC_VC1_ENCODER::VC1_ENC_I_I_FIELD:
            MFXSts = SetMEParams_I_ADV(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames);
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_FRAME:
            if (pFrameParam->UnifiedMvMode)
            {
                MFXSts = SetMEParams_P_ADV(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames);
            }
            else
            {
                MFXSts = SetMEParams_PMixed_ADV(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames);
            }
            break;
         case UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD:
             MFXSts = SetMEParams_P_Field_ADV(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames, !pFrameParam->UnifiedMvMode);
            break;
         case UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD:
             if(!pFrameParam->SecondFieldFlag)
                 MFXSts = SetMEParams_I_Field_ADV(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames);
             else
             {
                MFXSts = SetMEParams_P_Field_ADV(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames, !pFrameParam->UnifiedMvMode);
             }
            break;
         case UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD:
             if(!pFrameParam->SecondFieldFlag)
             {
                MFXSts = SetMEParams_P_Field_ADV(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames, !pFrameParam->UnifiedMvMode);
             }
             else
                 MFXSts = SetMEParams_I_Field_ADV(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames);
            break;
         case UMC_VC1_ENCODER::VC1_ENC_B_FRAME:
             MFXSts = SetMEParams_B_ADV(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames);
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_B_FIELD:
            MFXSts = SetMEParams_B_Field_ADV(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames, !pFrameParam->UnifiedMvMode);
            break;
         case UMC_VC1_ENCODER::VC1_ENC_BI_B_FIELD:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
         case UMC_VC1_ENCODER::VC1_ENC_B_BI_FIELD:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
         case UMC_VC1_ENCODER::VC1_ENC_BI_FRAME:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
          case UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
         default:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
        }
    }
    else
    {
        MFXSts = SetMEParams_SM(cuc, pPar, MEParams);
        MFX_CHECK_STS(MFXSts);

        switch(UMCtype)
        {
        case UMC_VC1_ENCODER::VC1_ENC_I_FRAME:
            MFXSts = SetMEParams_I_SM(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames);
            break;
         case UMC_VC1_ENCODER::VC1_ENC_P_FRAME:
            if (pFrameParam->UnifiedMvMode)
            {
                MFXSts = SetMEParams_P_SM(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames);
            }
            else
            {
                MFXSts = SetMEParams_PMixed_SM(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames);
            }
            break;
         case UMC_VC1_ENCODER::VC1_ENC_B_FRAME:
             MFXSts = SetMEParams_B_SM(cuc, pPar, MEParams,refData,pRefInfo,pMEFrames);
            break;
         default:
             assert(0);
             return MFX_ERR_UNSUPPORTED;
            break;
        }
    }
    /*-----------------------------------------------------------------------------*/

    return MFXSts;
}

mfxStatus MFXVideoEncVc1::SetMEParams_ADV(mfxFrameCUC *cuc, mfxVideoParam * pPar,UMC::MeParams* MEParams, bool FieldPicture)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    Ipp8u  doubleQuant = 2;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;


    MEParams->SetSearchSpeed(m_MESpeed);
    MEParams->UseFastFeedback = m_FastFB;
    MEParams->UseFeedback     = m_UseFB;
    MEParams->IsSMProfile     = false;

    MEParams->UseTrellisQuantization    = m_bTrellisQuantization;
    MEParams->SelectVlcTables           = m_bChangeVLCTables;
    MEParams->ChangeInterpPixelType     = m_bChangeInterpolationType;

    doubleQuant = pFrameParam->PQuant*2 + pFrameParam->HalfQP;
    assert(doubleQuant > 0);
    assert(doubleQuant <= 62);

    MEParams->Quant   = doubleQuant;


    switch(m_pVC1SequenceAdv->GetQuantType())
    {
        case UMC_VC1_ENCODER::VC1_ENC_QTYPE_IMPL:
            if(doubleQuant > 16)
                MEParams->UniformQuant = false;
            else
                MEParams->UniformQuant =  true;
            break;
        case UMC_VC1_ENCODER::VC1_ENC_QTYPE_EXPL:
            assert(0);
                MEParams->UniformQuant = true;
            break;
        case UMC_VC1_ENCODER::VC1_ENC_QTYPE_NUF:
            MEParams->UniformQuant = false;
            break;
        case UMC_VC1_ENCODER::VC1_ENC_QTYPE_UF:
            MEParams->UniformQuant = true;
            break;
        default:
            assert(0);
            MEParams->UniformQuant = true;
            break;
    }


    MEParams->CostMetric     = UMC::ME_Sad;
    MEParams->SkippedMetrics = UMC::ME_Sad;
    MEParams->ProcessSkipped  = true;
    MEParams->ProcessDirect   = true;

    MEParams ->MVRangeIndex = pFrameParam->ExtendedMvRange;

    MEParams->MVSizeOffset = UMC_VC1_ENCODER::MVSizeOffset;
    MEParams->MVLengthLong = UMC_VC1_ENCODER::longMVLength;

    MEParams->DcTableLuma   = UMC_VC1_ENCODER::DCTables[pFrameParam->TransDCTable][0];
    MEParams->DcTableChroma = UMC_VC1_ENCODER::DCTables[pFrameParam->TransDCTable][1];

    if (!m_pVC1SequenceAdv->IsVSTransform())
    {
        pFrameParam->TransformFlags = 1; // 8x8
    }
    MEParams->UseVarSizeTransform  = (pFrameParam->TransformFlags==0);

    MEParams->FastChroma = pFrameParam->FastUVMCflag = m_pVC1SequenceAdv->IsFastUVMC();

    if(!FieldPicture)
    {
        MEParams->ScanTable[0] = UMC_VC1_ENCODER::VC1_Inter_8x8_Scan;
        MEParams->ScanTable[1] = UMC_VC1_ENCODER::VC1_Inter_8x4_Scan_Adv;
        MEParams->ScanTable[2] = UMC_VC1_ENCODER::VC1_Inter_4x8_Scan_Adv;
        MEParams->ScanTable[3] = UMC_VC1_ENCODER::VC1_Inter_4x4_Scan;
    }
    else
    {
        MEParams->ScanTable[0] = UMC_VC1_ENCODER::VC1_Inter_InterlaceIntra_8x8_Scan_Adv;
        MEParams->ScanTable[1] = UMC_VC1_ENCODER::VC1_Inter_Interlace_8x4_Scan_Adv;
        MEParams->ScanTable[2] = UMC_VC1_ENCODER::VC1_Inter_Interlace_4x8_Scan_Adv;
        MEParams->ScanTable[3] = UMC_VC1_ENCODER::VC1_Inter_Interlace_4x4_Scan_Adv;
    }

    MEParams->ChromaInterpolation = UMC::ME_VC1_Bilinear;

    return MFXSts;
}

mfxStatus MFXVideoEncVc1::SetMEParams_SM(mfxFrameCUC *cuc, mfxVideoParam * pPar,UMC::MeParams* MEParams)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    Ipp8u           doubleQuant = 2;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;


    MEParams->SetSearchSpeed(m_MESpeed);
    MEParams->UpdateFeedback  = false;
    MEParams->UseFastFeedback = m_FastFB;
    MEParams->UseFeedback     = m_UseFB;
    MEParams->IsSMProfile     = true;

    doubleQuant = pFrameParam->PQuant*2 + pFrameParam->HalfQP;
    assert(doubleQuant > 0);
    assert(doubleQuant <= 62);

    MEParams->UseTrellisQuantization    = m_bTrellisQuantization;
    MEParams->SelectVlcTables           = m_bChangeVLCTables;
    MEParams->ChangeInterpPixelType     = m_bChangeInterpolationType;

    MEParams->Quant = doubleQuant;

    switch(m_pVC1SequenceSM->GetQuantType())
    {
        case UMC_VC1_ENCODER::VC1_ENC_QTYPE_IMPL:
            if(doubleQuant > 16)
                MEParams->UniformQuant = false;
            else
                MEParams->UniformQuant =  true;
            break;
        case UMC_VC1_ENCODER::VC1_ENC_QTYPE_EXPL:
            assert(0);
            MEParams->UniformQuant = true;
            break;
        case UMC_VC1_ENCODER::VC1_ENC_QTYPE_NUF:
            MEParams->UniformQuant = false;
            break;
        case UMC_VC1_ENCODER::VC1_ENC_QTYPE_UF:
            MEParams->UniformQuant = true;
            break;
        default:
            assert(0);
            MEParams->UniformQuant = true;
            break;
    }


    MEParams->CostMetric     = UMC::ME_Sad;
    MEParams->SkippedMetrics = UMC::ME_Sad;
    MEParams->ProcessSkipped  = true;
    MEParams->ProcessDirect   = true;


    MEParams->MVRangeIndex = pFrameParam->ExtendedMvRange;


    MEParams->MVSizeOffset = UMC_VC1_ENCODER::MVSizeOffset;
    MEParams->MVLengthLong = UMC_VC1_ENCODER::longMVLength;

    MEParams->FastChroma   = pFrameParam->FastUVMCflag = m_pVC1SequenceSM->IsFastUVMC();
    if (!m_pVC1SequenceSM->IsVSTransform())
    {
        pFrameParam->TransformFlags = 1; // 8x8
    }
    MEParams->UseVarSizeTransform  = (pFrameParam->TransformFlags==0);


    MEParams->DcTableLuma   = UMC_VC1_ENCODER::DCTables[pFrameParam->TransDCTable][0];
    MEParams->DcTableChroma = UMC_VC1_ENCODER::DCTables[pFrameParam->TransDCTable][1];

    MEParams->ScanTable[0] = UMC_VC1_ENCODER::VC1_Inter_8x8_Scan;
    MEParams->ScanTable[1] = UMC_VC1_ENCODER::VC1_Inter_8x4_Scan;
    MEParams->ScanTable[2] = UMC_VC1_ENCODER::VC1_Inter_4x8_Scan;
    MEParams->ScanTable[3] = UMC_VC1_ENCODER::VC1_Inter_4x4_Scan;

    MEParams->ChromaInterpolation = UMC::ME_VC1_Bilinear;

    return MFXSts;
}

#define FOR_IND    0
#define BACK_IND   2
#define CUR_IND    4
#define FOR_F_IND  0
#define BACK_F_IND 2
#define CUR_F_IND  4
#define FOR_S_IND  1
#define BACK_S_IND 3
#define CUR_S_IND  5

#define REF_DATA_FRW 0
#define REF_DATA_BKW 1
#define REF_DATA_CUR 2

mfxStatus MFXVideoEncVc1::SetMEParams_I_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams,mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames)
{
    mfxFrameParamVC1*   pFrameParam = &cuc->FrameParam->VC1;
    mfxU8               curIndex = pFrameParam->CurrFrameLabel & 0x7F;
    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);

    MFX_CHECK_NULL_PTR1 (cuc->FrameSurface->Data[curIndex]->Y)
    MFX_CHECK_COND(curIndex < cuc->FrameSurface->Info.NumFrameData)
    MFX_CHECK_NULL_PTR1 (pMEFrames)
    MFX_CHECK_NULL_PTR1 (pMEFrames[CUR_IND])

    MEParams->pSrc          = pMEFrames[CUR_IND];
    MEParams->pSrc->plane[0].ptr[0]  = cuc->FrameSurface->Data[curIndex]->Y + GetLumaShift(&cuc->FrameSurface->Info,   cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[0] = cuc->FrameSurface->Data[curIndex]->Pitch;
    MEParams->pSrc->plane[0].ptr[1]  = cuc->FrameSurface->Data[curIndex]->U + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[1] = cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].ptr[2]  = cuc->FrameSurface->Data[curIndex]->V + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[2] = cuc->FrameSurface->Data[curIndex]->Pitch/2;

    MEParams->pSrc->type    = UMC::ME_FrmIntra;

    MEParams->BRefFramesNum = 0;
    MEParams->FRefFramesNum = 0;
    MEParams->pSrc->WidthMB = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pSrc->HeightMB = pFrameParam->FrameHinMbMinus1+1;

    MEParams->SearchDirection =  UMC::ME_IntraSearch;

    return MFX_ERR_NONE;
}


mfxStatus MFXVideoEncVc1::SetMEParams_P_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar ,UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames)
{
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU8       curIndex = pFrameParam->CurrFrameLabel   & 0x7F;
    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);

    MFX_CHECK_COND  (pFrameParam->NumRefFrame >= 1)
    MFX_CHECK_COND  (curIndex < cuc->FrameSurface->Info.NumFrameData)
    MFX_CHECK_NULL_PTR1 (pMEFrames)
    MFX_CHECK_NULL_PTR1 (pMEFrames[CUR_IND])
    MFX_CHECK_NULL_PTR1 (pMEFrames[FOR_IND])


    MEParams->pSrc = pMEFrames[CUR_IND];
    MFX_CHECK_NULL_PTR1(MEParams->pSrc->plane);
    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data[curIndex]->Y);

    MEParams->pSrc->plane[0].ptr[0] =  cuc->FrameSurface->Data[curIndex]->Y + GetLumaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[0]=  cuc->FrameSurface->Data[curIndex]->Pitch;
    MEParams->pSrc->plane[0].ptr[1] =  cuc->FrameSurface->Data[curIndex]->U + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[1]=  cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].ptr[2] =  cuc->FrameSurface->Data[curIndex]->V + GetChromaShift(&cuc->FrameSurface->Info,cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[2]=  cuc->FrameSurface->Data[curIndex]->Pitch/2;

    MEParams->pSrc->WidthMB  = pFrameParam->FrameWinMbMinus1+1 ;
    MEParams->pSrc->HeightMB = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pSrc->type    = UMC::ME_FrmFrw;

    MEParams->pRefF[0] = pMEFrames[FOR_IND] ;
    MFX_CHECK_NULL_PTR1(MEParams->pRefF[0]->plane);


    mfxFrameData *pForw = refData[REF_DATA_FRW];
    MFX_CHECK_NULL_PTR1(pForw);

    MEParams->pRefF[0]->plane[0].ptr[0]  =  pForw->Y + GetLumaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[0] =  pForw->Pitch;
    MEParams->pRefF[0]->plane[0].ptr[1]  =  pForw->U + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[1] =  pForw->Pitch>>1;
    MEParams->pRefF[0]->plane[0].ptr[2]  =  pForw->V + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[2] =  pForw->Pitch>>1;

    MEParams->pRefF[0]->padding = (m_bUsePadding)? 32:0;

    MEParams->pRefF[0]->WidthMB  = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pRefF[0]->HeightMB = pFrameParam->FrameHinMbMinus1+1;

    MEParams->Interpolation = (pFrameParam->UnifiedMvMode == MFX_VC1_ENC_1MV_HALF_BILINEAR)?
                                    UMC::ME_VC1_Bilinear :UMC::ME_VC1_Bicubic;

    MEParams->SearchDirection =  UMC::ME_ForwardSearch;

    MEParams->PixelType =  (pFrameParam->UnifiedMvMode == MFX_VC1_ENC_1MV_HALF_BILINEAR ||
                            pFrameParam->UnifiedMvMode == MFX_VC1_ENC_1MV_HALF_BICUBIC)?
                            UMC::ME_HalfPixel : UMC::ME_QuarterPixel;

    MEParams->MbPart            =  UMC::ME_Mb16x16;

    MEParams->PredictionType    = UMC::ME_VC1Hybrid;

    if (m_bUsePadding)
    {
        MEParams->PicRange.top_left.x       = -14;
        MEParams->PicRange.top_left.y       = -14;
        MEParams->PicRange.bottom_right.x   = w + 13;
        MEParams->PicRange.bottom_right.y   = h + 13;
    }
    else
    {
        MEParams->PicRange.top_left.x       = 1;
        MEParams->PicRange.top_left.y       = 1;
        MEParams->PicRange.bottom_right.x   = w - 3;
        MEParams->PicRange.bottom_right.y   = h - 3;
    }

    MEParams->SearchRange.x             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    MEParams->SearchRange.y             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1] - 1;
    MEParams->BRefFramesNum             = 0;
    MEParams->FRefFramesNum             = 1;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncVc1::SetMEParams_PMixed_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames)
{
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU8  curIndex       =    pFrameParam->CurrFrameLabel   & 0x7F;
    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);

    MFX_CHECK_COND  (pFrameParam->NumRefFrame >= 1)
    MFX_CHECK_COND  (curIndex < cuc->FrameSurface->Info.NumFrameData)
    MFX_CHECK_NULL_PTR1 (pMEFrames)
    MFX_CHECK_NULL_PTR1 (pMEFrames[CUR_IND])
    MFX_CHECK_NULL_PTR1 (pMEFrames[FOR_IND])
    MFX_CHECK_COND(pFrameParam->Pic4MvAllowed == (pFrameParam->UnifiedMvMode == 0));

    MEParams->pSrc = pMEFrames[CUR_IND];
    MFX_CHECK_NULL_PTR1(MEParams->pSrc->plane);
    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data[curIndex]->Y);

    MEParams->pSrc->plane[0].ptr[0]      =  cuc->FrameSurface->Data[curIndex]->Y + GetLumaShift  (&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[0]     =  cuc->FrameSurface->Data[curIndex]->Pitch;
    MEParams->pSrc->plane[0].ptr[1]      =  cuc->FrameSurface->Data[curIndex]->U + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[1]     =  cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].ptr[2]      =  cuc->FrameSurface->Data[curIndex]->V + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[2]     =  cuc->FrameSurface->Data[curIndex]->Pitch/2;

    MEParams->pSrc->WidthMB  = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pSrc->HeightMB = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pSrc->type    = UMC::ME_FrmFrw;

    MEParams->pRefF[0]      = pMEFrames[FOR_IND] ;
    MFX_CHECK_NULL_PTR1(MEParams->pRefF[0]->plane);
    mfxFrameData *pForw = refData[REF_DATA_FRW];
    MFX_CHECK_NULL_PTR1(pForw);

    MEParams->pRefF[0]->plane[0].ptr[0]  =  pForw->Y + GetLumaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[0] =  pForw->Pitch;
    MEParams->pRefF[0]->plane[0].ptr[1]  =  pForw->U + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[1] =  pForw->Pitch>>1;
    MEParams->pRefF[0]->plane[0].ptr[2]  =  pForw->V + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[2] =  pForw->Pitch>>1;

    MEParams->pRefF[0]->WidthMB  = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pRefF[0]->HeightMB = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pRefF[0]->padding = (m_bUsePadding)? 32:0;

    MEParams->Interpolation     = UMC::ME_VC1_Bicubic;

    MEParams->SearchDirection    =  UMC::ME_ForwardSearch;
    MEParams->PixelType          =  UMC::ME_QuarterPixel;

    MEParams->MbPart             =  UMC::ME_Mb8x8;

    MEParams->PredictionType     = UMC::ME_VC1Hybrid;
    if (m_bUsePadding)
    {
        MEParams->PicRange.top_left.x       = -7;
        MEParams->PicRange.top_left.y       = -7;
        MEParams->PicRange.bottom_right.x   = w  + 7;
        MEParams->PicRange.bottom_right.y   = h  + 7;
    }
    else
    {
        MEParams->PicRange.top_left.x       = 1;
        MEParams->PicRange.top_left.y       = 1;
        MEParams->PicRange.bottom_right.x   = w - 3;
        MEParams->PicRange.bottom_right.y   = h - 3;
    }

    MEParams->SearchRange.x             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    MEParams->SearchRange.y             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1] - 1;
    MEParams->BRefFramesNum = 0;
    MEParams->FRefFramesNum = 1;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncVc1::SetMEParams_B_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames)
{
    mfxStatus   MFXSts = MFX_ERR_NONE;
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU8       curIndex     =  pFrameParam->CurrFrameLabel   & 0x7F;
    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);

    Ipp32u      hMB = pFrameParam->FrameHinMbMinus1+1;
    Ipp32u      wMB = pFrameParam->FrameWinMbMinus1+1;
    mfxI16      n = 0;


    //BFraction params
    Ipp8u       nom = 0;
    Ipp8u       denom = 0;
    Ipp8u       scaleFactor = 0;

    MFXSts = GetBFractionParameters (pFrameParam->CodedBfraction, nom, denom,  scaleFactor);
    MFX_CHECK_STS(MFXSts);

    void        (* GetMVDirect) (Ipp16s x, Ipp16s y, Ipp32s scaleFactor,
        UMC_VC1_ENCODER::sCoordinate * mvF, UMC_VC1_ENCODER::sCoordinate *mvB);

    MFX_CHECK_COND  (pFrameParam->NumRefFrame >= 2)
    MFX_CHECK_COND  (curIndex < cuc->FrameSurface->Info.NumFrameData)
    MFX_CHECK_NULL_PTR1 (pMEFrames)
    MFX_CHECK_NULL_PTR1 (pMEFrames[CUR_IND])
    MFX_CHECK_NULL_PTR1 (pMEFrames[FOR_IND])
    MFX_CHECK_NULL_PTR1 (pMEFrames[BACK_IND])

    MEParams->pSrc = pMEFrames[CUR_IND];
    MFX_CHECK_NULL_PTR1(MEParams->pSrc->plane);
    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data[curIndex]->Y);

    MEParams->pSrc->plane[0].ptr[0]      =  cuc->FrameSurface->Data[curIndex]->Y + GetLumaShift(&cuc->FrameSurface->Info,cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[0]     =  cuc->FrameSurface->Data[curIndex]->Pitch;
    MEParams->pSrc->plane[0].ptr[1]      =  cuc->FrameSurface->Data[curIndex]->U + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[1]     =  cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].ptr[2]      =  cuc->FrameSurface->Data[curIndex]->V + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[2]     =  cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->WidthMB   = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pSrc->HeightMB  = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pSrc->type    = UMC::ME_FrmBidir;

    MEParams->pRefF[0]      = pMEFrames[FOR_IND] ;
    MFX_CHECK_NULL_PTR1(MEParams->pRefF[0]->plane);

    mfxFrameData *pForw = refData[REF_DATA_FRW];
    MFX_CHECK_NULL_PTR1(pForw);

    MEParams->pRefF[0]->plane[0].ptr[0]  =  pForw->Y + GetLumaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[0] =  pForw->Pitch;
    MEParams->pRefF[0]->plane[0].ptr[1]  =  pForw->U + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[1] =  pForw->Pitch>>1;
    MEParams->pRefF[0]->plane[0].ptr[2]  =  pForw->V + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[2] =  pForw->Pitch>>1;
    MEParams->pRefF[0]->WidthMB   = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pRefF[0]->HeightMB  = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pRefF[0]->padding = (m_bUsePadding)? 32:0;

    MEParams->pRefB[0]      = pMEFrames[BACK_IND];

    MFX_CHECK_NULL_PTR1 (MEParams->pRefB[0]->plane);

    mfxFrameData *pBackw = refData[REF_DATA_BKW];
    MFX_CHECK_NULL_PTR1(pBackw);

    MEParams->pRefB[0]->plane[0].ptr[0]  =  pBackw->Y + GetLumaShift(pRefInfo, pBackw);
    MEParams->pRefB[0]->plane[0].step[0] =  pBackw->Pitch;
    MEParams->pRefB[0]->plane[0].ptr[1]  =  pBackw->U + GetChromaShift(pRefInfo, pBackw);
    MEParams->pRefB[0]->plane[0].step[1] =  pBackw->Pitch>>1;
    MEParams->pRefB[0]->plane[0].ptr[2]  =  pBackw->V + GetChromaShift(pRefInfo, pBackw);
    MEParams->pRefB[0]->plane[0].step[2] =  pBackw->Pitch>>1;
    MEParams->pRefB[0]->WidthMB   = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pRefB[0]->HeightMB  = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pRefB[0]->padding = (m_bUsePadding)? 32:0;

    UMC_VC1_ENCODER::sCoordinate MVFDirect    = {0,0};
    UMC_VC1_ENCODER::sCoordinate MVBDirect    = {0,0};

    UMC_VC1_ENCODER::sCoordinate                 MVPredMin = {-60,-60};
    UMC_VC1_ENCODER::sCoordinate                 MVPredMax = {(w - 1)*4, (h - 1)*4};

    mfxI16Pair*   pSavedMV = 0;

    Ipp32u i = 0;
    Ipp32u j = 0;

    n = GetExBufferIndex(cuc, MFX_CUC_VC1_MVDATA);
    MFX_CHECK_COND (n>=0)

    mfxExtVc1MvData* pMVs = (mfxExtVc1MvData*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMVs->NumMv >= wMB*hMB)
    pSavedMV = pMVs->Mv;

    GetMVDirect = (pFrameParam->UnifiedMvMode != MFX_VC1_ENC_1MV)?
        UMC_VC1_ENCODER::GetMVDirectHalf : UMC_VC1_ENCODER::GetMVDirectQuarter;

    MEParams->Interpolation      =  (MFX_VC1_ENC_1MV_HALF_BILINEAR == pFrameParam->UnifiedMvMode) ?
                                            UMC::ME_VC1_Bilinear :UMC::ME_VC1_Bicubic;
    MEParams->SearchDirection    = UMC::ME_BidirSearch;

    MEParams->PixelType          = (MFX_VC1_ENC_1MV_HALF_BILINEAR == pFrameParam->UnifiedMvMode
                                 || MFX_VC1_ENC_1MV_HALF_BICUBIC == pFrameParam->UnifiedMvMode) ?
                                   UMC::ME_HalfPixel : UMC::ME_QuarterPixel;
    MEParams->MbPart             = UMC::ME_Mb16x16;

    MEParams->PredictionType     = UMC::ME_VC1;

    if (m_bUsePadding)
    {
        MEParams->PicRange.top_left.x = -14;
        MEParams->PicRange.top_left.y = -14;
        MEParams->PicRange.bottom_right.x = w + 13;
        MEParams->PicRange.bottom_right.y = h + 13;
    }
    else
    {
        MEParams->PicRange.top_left.x = 1;
        MEParams->PicRange.top_left.y = 1;
        MEParams->PicRange.bottom_right.x = w - 3;
        MEParams->PicRange.bottom_right.y = h - 3;
    }

    MEParams->SearchRange.x   = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    MEParams->SearchRange.y   = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1] - 1;
    MEParams->BRefFramesNum  = 1;
    MEParams->FRefFramesNum  = 1;

    for (i=0; i < hMB; i++)
    {
        for (j=0; j < wMB; j++)
        {
            GetMVDirect (pSavedMV->x,pSavedMV->y,scaleFactor, &MVFDirect, &MVBDirect);
            ScalePredict(&MVFDirect, j*16*4,i*16*4,MVPredMin,MVPredMax);
            ScalePredict(&MVBDirect, j*16*4,i*16*4,MVPredMin,MVPredMax);

            MEParams->pRefF[0]->MVDirect[(j + i*wMB)].x = MVFDirect.x;
            MEParams->pRefF[0]->MVDirect[(j + i*wMB)].y = MVFDirect.y;

            MEParams->pRefB[0]->MVDirect[(j + i*wMB)].x = MVBDirect.x;
            MEParams->pRefB[0]->MVDirect[(j + i*wMB)].y = MVBDirect.y;

            pSavedMV ++;
        }
    }
    MEParams->FirstMB = 0;
    MEParams->LastMB  = MEParams->pSrc->WidthMB*MEParams->pSrc->HeightMB - 1;
    return MFX_ERR_NONE;
}


mfxStatus MFXVideoEncVc1::SetMEParams_I_Field_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames)
{
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU8   curIndex = pFrameParam->CurrFrameLabel & 0x7F;
    bool    bBottom  =  pFrameParam->BottomFieldFlag;

    mfxI32   meCurrIndex = (pFrameParam->SecondFieldFlag)? CUR_S_IND: CUR_F_IND;

    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);

    MFX_CHECK_COND(curIndex < cuc->FrameSurface->Info.NumFrameData)
    MFX_CHECK_NULL_PTR1 (cuc->FrameSurface->Data[curIndex]->Y)

    MFX_CHECK_NULL_PTR1 (pMEFrames)
    MFX_CHECK_NULL_PTR1 (pMEFrames[meCurrIndex])

    MEParams->pSrc          = pMEFrames[meCurrIndex];

    MEParams->pSrc->plane[0].ptr[0]  = cuc->FrameSurface->Data[curIndex]->Y
        + GetLumaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex])
                                       + bBottom * cuc->FrameSurface->Data[curIndex]->Pitch;
    MEParams->pSrc->plane[0].step[0] = cuc->FrameSurface->Data[curIndex]->Pitch<<1;

    MEParams->pSrc->plane[0].ptr[1]  = cuc->FrameSurface->Data[curIndex]->U
        + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex])
                                       + bBottom * cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].step[1] = cuc->FrameSurface->Data[curIndex]->Pitch;

    MEParams->pSrc->plane[0].ptr[2]  = cuc->FrameSurface->Data[curIndex]->V
        + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex])
                                       + bBottom * cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].step[2] = cuc->FrameSurface->Data[curIndex]->Pitch;

    MEParams->pSrc->type    = UMC::ME_FrmIntra;

    MEParams->BRefFramesNum = 0;
    MEParams->FRefFramesNum = 0;
    MEParams->pSrc->WidthMB = pFrameParam->FrameWinMbMinus1+1 ;
    MEParams->pSrc->HeightMB = pFrameParam->FrameHinMbMinus1+1;

    MEParams->SearchDirection =  UMC::ME_IntraSearch;

    return MFX_ERR_NONE;
}


mfxStatus MFXVideoEncVc1::SetMEParams_B_Field_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams,
                         mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames, bool mixed)
{
    UMC::Status Sts = UMC::UMC_OK;
    mfxStatus   MFXSts = MFX_ERR_NONE;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    bool bBottom        =  pFrameParam->BottomFieldFlag;
    bool bSecondField   =  pFrameParam->SecondFieldFlag;

    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);

    Ipp32u  hMB = pFrameParam->FrameHinMbMinus1+1;
    Ipp32u  wMB = pFrameParam->FrameWinMbMinus1+1;

    mfxU16  nMB = wMB * hMB;

    Ipp32u i = 0;
    Ipp32u j = 0;
    Ipp32u RefNum = 0;

    //BFraction params
    mfxU8       nom = 0;
    mfxU8       denom = 0;
    mfxU8       scaleFactor = 0;
    mfxU8       scaleFactor1 = 0;
    mfxU8 refDist = m_pVC1SequenceAdv->GetRefDist(pFrameParam->RefDistance);

    mfxU8       curIndex     =  pFrameParam->CurrFrameLabel   & 0x7F;

    mfxI16      n = 0;


    MFXSts = GetBFractionParameters (pFrameParam->CodedBfraction, nom, denom,  scaleFactor1);
    MFX_CHECK_STS(MFXSts);

    //(BFractionScaleFactor[m_InitPicParam.uiBFraction.denom][m_InitPicParam.uiBFraction.num]*(Ipp32s)m_nReferenceFrameDist)>>8;
    scaleFactor  = (scaleFactor1 * refDist)>>8;

    UMC_VC1_ENCODER::sScaleInfo  scInfoForw;
    UMC_VC1_ENCODER::sScaleInfo  scInfoBackw;


    void        (* GetMVDirect) (Ipp16s x, Ipp16s y, Ipp32s scaleFactor,
                                 UMC_VC1_ENCODER::sCoordinate * mvF, UMC_VC1_ENCODER::sCoordinate *mvB);

    UMC_VC1_ENCODER::sCoordinate MVFDirect    = {0,0};
    UMC_VC1_ENCODER::sCoordinate MVBDirect    = {0,0};

    UMC_VC1_ENCODER::sCoordinate                 MVPredMin = {-60,-60};
    UMC_VC1_ENCODER::sCoordinate                 MVPredMax = {((Ipp16s)wMB*16 - 1)*4, ((Ipp16s)hMB*16 - 1)*4};

    mfxI32   meCurrIndex ;
    mfxI32   meForw0Index ;
    mfxI32   meForw1Index ;
    mfxI32   meBackw0Index ;
    mfxI32   meBackw1Index ;

    if (pFrameParam->SecondFieldFlag)
    {
        meCurrIndex     = CUR_S_IND;
        meForw0Index    = CUR_F_IND;
        meForw1Index    = FOR_S_IND;
        meBackw0Index   = BACK_F_IND;
        meBackw1Index   = BACK_S_IND;
    }
    else
    {
        meCurrIndex    = CUR_F_IND;
        meForw0Index   = FOR_S_IND;
        meForw1Index   = FOR_F_IND;
        meBackw0Index  = BACK_F_IND;
        meBackw1Index  = BACK_S_IND;

    }


    MFX_CHECK_COND  (pFrameParam->NumRefFrame >= 2)
    MFX_CHECK_COND  (curIndex < cuc->FrameSurface->Info.NumFrameData)
    MFX_CHECK_NULL_PTR1 (pMEFrames)
    MFX_CHECK_NULL_PTR1 (pMEFrames[meCurrIndex])
    MFX_CHECK_NULL_PTR1 (pMEFrames[meForw0Index])
    MFX_CHECK_NULL_PTR1 (pMEFrames[meForw1Index])
    MFX_CHECK_NULL_PTR1 (pMEFrames[meBackw0Index])
    MFX_CHECK_NULL_PTR1 (pMEFrames[meBackw1Index])


    MEParams->pSrc          = pMEFrames[meCurrIndex];
    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data[curIndex]->Y);

    MEParams->pSrc->plane[0].ptr[0]      =  cuc->FrameSurface->Data[curIndex]->Y +
                                            GetLumaShift(&cuc->FrameSurface->Info,cuc->FrameSurface->Data[curIndex]) +
                                            bBottom * cuc->FrameSurface->Data[curIndex]->Pitch;
    MEParams->pSrc->plane[0].step[0]     =  cuc->FrameSurface->Data[curIndex]->Pitch<<1;

    MEParams->pSrc->plane[0].ptr[1]      =  cuc->FrameSurface->Data[curIndex]->U +
                                            GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]) +
                                            bBottom * cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].step[1]     =  cuc->FrameSurface->Data[curIndex]->Pitch;

    MEParams->pSrc->plane[0].ptr[2]      =  cuc->FrameSurface->Data[curIndex]->V +
                                            GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]) +
                                            bBottom * cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].step[2]     =  cuc->FrameSurface->Data[curIndex]->Pitch;

    MEParams->pSrc->WidthMB = wMB;      //m_picture_width;
    MEParams->pSrc->HeightMB =hMB;    //m_picture_height;
    MEParams->pSrc->type    = UMC::ME_FrmBidir;
    MEParams->pSrc->bBottom = bBottom;

    MEParams->pRefF[0]      = pMEFrames[meForw0Index];

    MEParams->pRefF[0]->WidthMB  = wMB;     //m_picture_width;
    MEParams->pRefF[0]->HeightMB = hMB; //m_picture_height;
    MEParams->pRefF[0]->padding  = (m_bUsePadding)? 16:0;
    MEParams->pRefF[0]->bBottom = !bBottom;

    MEParams->pRefF[1]      = pMEFrames[meForw1Index];

    MEParams->pRefF[1]->WidthMB  = wMB;      //m_picture_width;
    MEParams->pRefF[1]->HeightMB = hMB;    //m_picture_height;
    MEParams->pRefF[1]->padding = (m_bUsePadding)? 16:0;
    MEParams->pRefF[1]->bBottom = bBottom;


    MEParams->pRefB[0]      = pMEFrames[meBackw0Index];

    MEParams->pRefB[0]->WidthMB  = wMB;      //m_picture_width;
    MEParams->pRefB[0]->HeightMB = hMB;    //m_picture_height;
    MEParams->pRefB[0]->padding = (m_bUsePadding)? 16:0;
    MEParams->pRefB[0]->bBottom = !bBottom;

    MEParams->pRefB[1]      = pMEFrames[meBackw1Index];

    MEParams->pRefB[1]->WidthMB  = wMB;      //m_picture_width;
    MEParams->pRefB[1]->HeightMB = hMB;    //m_picture_height;
    MEParams->pRefB[1]->padding = (m_bUsePadding)? 16:0;
    MEParams->pRefB[1]->bBottom = bBottom;


    MEParams->FRefFramesNum = 2;
    MEParams->BRefFramesNum = 2;

    MEParams->PredictionType     = UMC::ME_VC1Field2;


    mfxFrameData *pForw  = refData[REF_DATA_FRW];
    mfxFrameData *pRaised = (refData[REF_DATA_CUR])?refData[REF_DATA_CUR]:refData[REF_DATA_FRW];
    MFX_CHECK_NULL_PTR1(pForw);
    MFX_CHECK_NULL_PTR1(pRaised);

    //forward planes
    //Y
    Sts = UMC_VC1_ENCODER::Set2RefFrwFieldPlane(
                              &MEParams->pRefF[0]->plane[0].ptr[0],&MEParams->pRefF[0]->plane[0].step[0],
                              &MEParams->pRefF[1]->plane[0].ptr[0],&MEParams->pRefF[1]->plane[0].step[0],
                              pForw->Y +  GetLumaShift(pRefInfo, pForw), pForw->Pitch,
                              pRaised->Y + GetLumaShift(pRefInfo, pRaised),pRaised->Pitch,
                              bSecondField, bBottom, 0);
    UMC_MFX_CHECK_STS(Sts);


    //U
    Sts = UMC_VC1_ENCODER::Set2RefFrwFieldPlane(
                              &MEParams->pRefF[0]->plane[0].ptr[1],&MEParams->pRefF[0]->plane[0].step[1],
                              &MEParams->pRefF[1]->plane[0].ptr[1],&MEParams->pRefF[1]->plane[0].step[1],
                              pForw->U + GetChromaShift(pRefInfo, pForw),  pForw->Pitch>>1,
                              pRaised->U + GetChromaShift(pRefInfo, pRaised),pRaised->Pitch>>1,
                              bSecondField, bBottom, 0);
    UMC_MFX_CHECK_STS(Sts);



    //V
    Sts = UMC_VC1_ENCODER::Set2RefFrwFieldPlane(
                              &MEParams->pRefF[0]->plane[0].ptr[2],&MEParams->pRefF[0]->plane[0].step[2],
                              &MEParams->pRefF[1]->plane[0].ptr[2],&MEParams->pRefF[1]->plane[0].step[2],
                              pForw->V + GetChromaShift(pRefInfo, pForw),  pForw->Pitch>>1,
                              pRaised->V + GetChromaShift(pRefInfo, pRaised),pRaised->Pitch>>1,
                              bSecondField, bBottom, 0);
    UMC_MFX_CHECK_STS(Sts);


    //backward planes
    //Y
    mfxFrameData *pBackw  = refData[REF_DATA_BKW];
    MFX_CHECK_NULL_PTR1(pBackw);

    Sts = UMC_VC1_ENCODER::SetBkwFieldPlane(
                              &MEParams->pRefB[0]->plane[0].ptr[0],   &MEParams->pRefB[0]->plane[0].step[0],
                              &MEParams->pRefB[1]->plane[0].ptr[0],   &MEParams->pRefB[1]->plane[0].step[0],
                              pBackw->Y + GetLumaShift(pRefInfo, pBackw),  pBackw->Pitch, bBottom, 0);
    UMC_MFX_CHECK_STS(Sts);

    //U
    Sts = UMC_VC1_ENCODER::SetBkwFieldPlane(
                              &MEParams->pRefB[0]->plane[0].ptr[1],   &MEParams->pRefB[0]->plane[0].step[1],
                              &MEParams->pRefB[1]->plane[0].ptr[1],   &MEParams->pRefB[1]->plane[0].step[1],
                              pBackw->U + GetChromaShift(pRefInfo, pBackw),  pBackw->Pitch>>1, bBottom, 0);
    UMC_MFX_CHECK_STS(Sts);

    //V
    Sts = UMC_VC1_ENCODER::SetBkwFieldPlane(
                              &MEParams->pRefB[0]->plane[0].ptr[2],   &MEParams->pRefB[0]->plane[0].step[2],
                              &MEParams->pRefB[1]->plane[0].ptr[2],   &MEParams->pRefB[1]->plane[0].step[2],
                              pBackw->V + GetChromaShift(pRefInfo, pBackw),  pBackw->Pitch>>1, bBottom, 0);
    UMC_MFX_CHECK_STS(Sts);

    n = GetExBufferIndex(cuc, MFX_CUC_VC1_MVDATA);
    MFX_CHECK_COND (n>=0)

    mfxExtVc1MvData* pMVs = (mfxExtVc1MvData*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMVs->NumMv >= (nMB))
    mfxI16Pair* pSavedMV = pMVs->Mv + wMB*hMB*bBottom;

    n = GetExBufferIndex(cuc,  MFX_CUC_VC1_MVDIR);
    MFX_CHECK_COND (n>=0)

    mfxExtVc1MvDir* pRefType = (mfxExtVc1MvDir*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pRefType->NumMv >= nMB)
    Ipp8u* pDirection = pRefType->MvDir + wMB*hMB*bBottom;

    if (pFrameParam->UnifiedMvMode != MFX_VC1_ENC_1MV && pFrameParam->UnifiedMvMode != MFX_VC1_ENC_MIXED_MV)
            GetMVDirect =  UMC_VC1_ENCODER::GetMVDirectCurrHalfBackQuarter;
    else
            GetMVDirect = UMC_VC1_ENCODER::GetMVDirectCurrQuarterBackQuarter;

    MEParams->bSecondField       =  bSecondField;

    MEParams->Interpolation      =  (MFX_VC1_ENC_1MV_HALF_BILINEAR == pFrameParam->UnifiedMvMode) ?
                                            UMC::ME_VC1_Bilinear :UMC::ME_VC1_Bicubic;
    MEParams->SearchDirection    = UMC::ME_BidirSearch;

    MEParams->PixelType          = (MFX_VC1_ENC_1MV_HALF_BILINEAR == pFrameParam->UnifiedMvMode
                                 || MFX_VC1_ENC_1MV_HALF_BICUBIC == pFrameParam->UnifiedMvMode) ?
                                   UMC::ME_HalfPixel : UMC::ME_QuarterPixel;

    MEParams->PredictionType     = UMC::ME_VC1Field2;

    if(!mixed)
    {
        if (m_bUsePadding)
        {
            MEParams->PicRange.top_left.x = -14;
            MEParams->PicRange.top_left.y = -14;
            MEParams->PicRange.bottom_right.x = w + 13;
            MEParams->PicRange.bottom_right.y = (h>>1) +13;
        }
        else
        {
            MEParams->PicRange.top_left.x = 1;
            MEParams->PicRange.top_left.y = 1;
            MEParams->PicRange.bottom_right.x = w - 3;
            MEParams->PicRange.bottom_right.y = (h>>1) - 3;
        }

        MEParams->MbPart =  UMC::ME_Mb16x16;

    }
    else
    {
        if (m_bUsePadding)
        {
            MEParams->PicRange.top_left.x = -7;
            MEParams->PicRange.top_left.y = -7;
            MEParams->PicRange.bottom_right.x = w + 7;
            MEParams->PicRange.bottom_right.y = (h>>1)+ 7;
        }
        else
        {
            MEParams->PicRange.top_left.x = 1;
            MEParams->PicRange.top_left.y = 1;
            MEParams->PicRange.bottom_right.x = w - 3;
            MEParams->PicRange.bottom_right.y = (h>>1)-3;
        }

        MEParams->MbPart =  UMC::ME_Mb8x8;
    }

    InitScaleInfo(&scInfoForw,bSecondField,bBottom,scaleFactor, pFrameParam->ExtendedMvRange);

    MEParams->ScaleInfo[0].ME_Bottom = scInfoForw.bBottom;

    MEParams->ScaleInfo[0].ME_RangeX = scInfoForw.rangeX;
    MEParams->ScaleInfo[0].ME_RangeY = scInfoForw.rangeY;

    MEParams->ScaleInfo[0].ME_ScaleOpp = scInfoForw.scale_opp;

    MEParams->ScaleInfo[0].ME_ScaleSame1 = scInfoForw.scale_same1;
    MEParams->ScaleInfo[0].ME_ScaleSame2 = scInfoForw.scale_same2;

    MEParams->ScaleInfo[0].ME_ScaleZoneX = scInfoForw.scale_zoneX;
    MEParams->ScaleInfo[0].ME_ScaleZoneY = scInfoForw.scale_zoneY;

    MEParams->ScaleInfo[0].ME_ZoneOffsetX = scInfoForw.zone_offsetX;
    MEParams->ScaleInfo[0].ME_ZoneOffsetY = scInfoForw.zone_offsetY;


    InitScaleInfoBackward(&scInfoBackw,bSecondField,bBottom,
                  ((refDist - scaleFactor-1)>=0)?(refDist - scaleFactor-1):0,  pFrameParam->ExtendedMvRange);

    MEParams->ScaleInfo[1].ME_Bottom = scInfoBackw.bBottom;

    MEParams->ScaleInfo[1].ME_RangeX = scInfoBackw.rangeX;
    MEParams->ScaleInfo[1].ME_RangeY = scInfoBackw.rangeY;

    MEParams->ScaleInfo[1].ME_ScaleOpp = scInfoBackw.scale_opp;

    MEParams->ScaleInfo[1].ME_ScaleSame1 = scInfoBackw.scale_same1;
    MEParams->ScaleInfo[1].ME_ScaleSame2 = scInfoBackw.scale_same2;

    MEParams->ScaleInfo[1].ME_ScaleZoneX = scInfoBackw.scale_zoneX;
    MEParams->ScaleInfo[1].ME_ScaleZoneY = scInfoBackw.scale_zoneY;

    MEParams->ScaleInfo[1].ME_ZoneOffsetX = scInfoBackw.zone_offsetX;
    MEParams->ScaleInfo[1].ME_ZoneOffsetY = scInfoBackw.zone_offsetY;


    MEParams->FieldInfo.ME_bExtendedX = (pFrameParam->ExtendedMvRange & 0x01)!=0;
    MEParams->FieldInfo.ME_bExtendedY = (pFrameParam->ExtendedMvRange & 0x02)!=0;

    MEParams->FieldInfo.ME_limitX = (MEParams->FieldInfo.ME_bExtendedX)? 511:256;
    MEParams->FieldInfo.ME_limitY = (MEParams->FieldInfo.ME_bExtendedY)? 511:256;

    MEParams->FieldInfo.ME_pMVModeField1RefTable_VLC = UMC_VC1_ENCODER::MVModeField2RefTable_VLC[pFrameParam->MvTable];

    MEParams->FieldInfo.ME_pMVSizeOffsetFieldIndexX = (MEParams->FieldInfo.ME_bExtendedX)?
                            UMC_VC1_ENCODER::MVSizeOffsetFieldExIndex:UMC_VC1_ENCODER::MVSizeOffsetFieldIndex;
    MEParams->FieldInfo.ME_pMVSizeOffsetFieldIndexY = (MEParams->FieldInfo.ME_bExtendedY)?
                            UMC_VC1_ENCODER::MVSizeOffsetFieldExIndex:UMC_VC1_ENCODER::MVSizeOffsetFieldIndex;
    MEParams->FieldInfo.ME_pMVSizeOffsetFieldX = (MEParams->FieldInfo.ME_bExtendedX)?
                            UMC_VC1_ENCODER::MVSizeOffsetFieldEx:UMC_VC1_ENCODER::MVSizeOffsetField;
    MEParams->FieldInfo.ME_pMVSizeOffsetFieldY = (MEParams->FieldInfo.ME_bExtendedY)?
                            UMC_VC1_ENCODER::MVSizeOffsetFieldEx:UMC_VC1_ENCODER::MVSizeOffsetField;

    MEParams->SearchRange.x   = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    MEParams->SearchRange.y   = (UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1]>>1) - 1;


    MEParams->BRefFramesNum = 2;
    MEParams->FRefFramesNum = 2;

    for (i=0; i < hMB; i++)
    {
        for (j=0; j < wMB; j++)
        {
            GetMVDirect (pSavedMV->x,pSavedMV->y,scaleFactor1, &MVFDirect, &MVBDirect);

            ScalePredict(&MVFDirect, j*16*4,i*16*4,MVPredMin,MVPredMax);
            ScalePredict(&MVBDirect, j*16*4,i*16*4,MVPredMin,MVPredMax);

            if(pDirection[i*wMB +  j] == 0)
                RefNum = 1; // the same field
            else if (pDirection[i*wMB +  j] == 1)
                RefNum = 0;
            else
                RefNum= 1;

            MEParams->pRefF[RefNum]->MVDirect[(j + i*wMB)].x = MVFDirect.x;
            MEParams->pRefF[RefNum]->MVDirect[(j + i*wMB)].y = MVFDirect.y;

            MEParams->pRefF[1-RefNum]->MVDirect[(j + i*wMB)].SetInvalid();
            MEParams->pRefF[1-RefNum]->MVDirect[(j + i*wMB)].SetInvalid();

            MEParams->pRefB[RefNum]->MVDirect[(j + i*wMB)].x = MVBDirect.x;
            MEParams->pRefB[RefNum]->MVDirect[(j + i*wMB)].y = MVBDirect.y;

            MEParams->pRefB[1-RefNum]->MVDirect[(j + i*wMB)].SetInvalid();
            MEParams->pRefB[1-RefNum]->MVDirect[(j + i*wMB)].SetInvalid();

            pSavedMV ++;

        }
    }
    return MFX_ERR_NONE;
}


mfxStatus MFXVideoEncVc1::SetMEParams_P_Field_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams,
                          mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames, bool mixed)
{
    UMC::Status Sts = UMC::UMC_OK;
    UMC_VC1_ENCODER::sScaleInfo             scInfo;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    bool bSecondField   =  pFrameParam->SecondFieldFlag;
    bool bBottom        =  pFrameParam->BottomFieldFlag;

    mfxU8       curIndex       =  pFrameParam->CurrFrameLabel   & 0x7F;

    mfxI16      n = 0;

    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);

    MFX_CHECK_COND  (pFrameParam->NumRefFrame >= 1)
    MFX_CHECK_COND  (curIndex < cuc->FrameSurface->Info.NumFrameData)

    UMC_VC1_ENCODER::eReferenceFieldType RefFieldType = UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_FIRST;

    RefFieldType = GetRefFieldFlag(pFrameParam->NumRefPic,pFrameParam->RefFieldPicFlag,pFrameParam->SecondFieldFlag);

    MEParams->SearchRange.x = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    MEParams->SearchRange.y = ((UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1])>>1) - 1;

    if(RefFieldType != UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_BOTH)
    {
        mfxI32 meForwIndex;
        mfxI32 meCurrIndex = (bSecondField)? CUR_S_IND:CUR_F_IND;

        if(RefFieldType == UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_FIRST)
        {
            meForwIndex = (bSecondField)? CUR_F_IND:FOR_S_IND;
        }
        else
        {
            meForwIndex = (bSecondField)? FOR_S_IND:FOR_F_IND;
        }

        MFX_CHECK_NULL_PTR1 (pMEFrames[meCurrIndex])
        MFX_CHECK_NULL_PTR1 (pMEFrames[meForwIndex])


        MEParams->pSrc          = pMEFrames[meCurrIndex];
        MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data[curIndex]->Y);

        MEParams->pSrc->plane[0].ptr[0]      =  cuc->FrameSurface->Data[curIndex]->Y +
                                                GetLumaShift(&cuc->FrameSurface->Info,cuc->FrameSurface->Data[curIndex]) +
                                                bBottom * cuc->FrameSurface->Data[curIndex]->Pitch;
        MEParams->pSrc->plane[0].step[0]     =  cuc->FrameSurface->Data[curIndex]->Pitch<<1;

        MEParams->pSrc->plane[0].ptr[1]      =  cuc->FrameSurface->Data[curIndex]->U +
                                                GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]) +
                                                bBottom * cuc->FrameSurface->Data[curIndex]->Pitch/2;
        MEParams->pSrc->plane[0].step[1]     =  cuc->FrameSurface->Data[curIndex]->Pitch;

        MEParams->pSrc->plane[0].ptr[2]      =  cuc->FrameSurface->Data[curIndex]->V +
                                                GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]) +
                                                bBottom * cuc->FrameSurface->Data[curIndex]->Pitch/2;
        MEParams->pSrc->plane[0].step[2]     =  cuc->FrameSurface->Data[curIndex]->Pitch;

        MEParams->pSrc->WidthMB  = pFrameParam->FrameWinMbMinus1+1;      //m_picture_width;
        MEParams->pSrc->HeightMB = pFrameParam->FrameHinMbMinus1+1;    //m_picture_height;
        MEParams->pSrc->type    = UMC::ME_FrmFrw;
        MEParams->pSrc->bBottom = bBottom;

        MEParams->FRefFramesNum = 1;
        MEParams->PredictionType     = UMC::ME_VC1Field1;

        MEParams->pRefF[0]      = pMEFrames[meForwIndex];

        MEParams->pRefF[0]->padding = (m_bUsePadding)? 16:0;
        MEParams->pRefF[0]->bBottom = IsFrwFieldBottom(RefFieldType, bSecondField, bBottom);

        mfxFrameData *pForw  = refData[REF_DATA_FRW];
        mfxFrameData *pRaised = (refData[REF_DATA_CUR])?refData[REF_DATA_CUR]:refData[REF_DATA_FRW];

        MFX_CHECK_NULL_PTR1(pForw);
        MFX_CHECK_NULL_PTR1(pRaised);


        Sts = UMC_VC1_ENCODER::Set1RefFrwFieldPlane(
            &MEParams->pRefF[0]->plane[0].ptr[0],          &MEParams->pRefF[0]->plane[0].step[0],
            pForw->Y +  GetLumaShift(pRefInfo, pForw), pForw->Pitch,
            pRaised->Y  + GetLumaShift(pRefInfo, pRaised),pRaised->Pitch,
            RefFieldType, bSecondField, bBottom, 0);

        UMC_MFX_CHECK_STS(Sts);
        assert(MEParams->pRefF[0]->plane[0].ptr[0]!= NULL);

        Sts = UMC_VC1_ENCODER::Set1RefFrwFieldPlane(
            &MEParams->pRefF[0]->plane[0].ptr[1],          &MEParams->pRefF[0]->plane[0].step[1],
            pForw->U + GetChromaShift(pRefInfo, pForw),  pForw->Pitch>>1,
            pRaised->U + GetChromaShift(pRefInfo, pRaised),pRaised->Pitch>>1,
            RefFieldType, bSecondField, bBottom, 0);

        UMC_MFX_CHECK_STS(Sts);
        assert(MEParams->pRefF[0]->plane[0].ptr[1]!= NULL);

        Sts = UMC_VC1_ENCODER::Set1RefFrwFieldPlane(
            &MEParams->pRefF[0]->plane[0].ptr[2], &MEParams->pRefF[0]->plane[0].step[2],
            pForw->V + GetChromaShift(pRefInfo, pForw),  pForw->Pitch>>1,
            pRaised->V + GetChromaShift(pRefInfo, pRaised),pRaised->Pitch>>1,
            RefFieldType, bSecondField, bBottom, 0);

        UMC_MFX_CHECK_STS(Sts);

        MEParams->pRefF[0]->WidthMB  = pFrameParam->FrameWinMbMinus1+1;    //m_picture_width;
        MEParams->pRefF[0]->HeightMB = pFrameParam->FrameHinMbMinus1+1;    //m_picture_height;
    }
    else
    {
        mfxI32 meForw0Index;
        mfxI32 meForw1Index;
        mfxI32 meCurrIndex;

        if (pFrameParam->SecondFieldFlag)
        {
            meCurrIndex     = CUR_S_IND;
            meForw0Index    = CUR_F_IND;
            meForw1Index    = FOR_S_IND;
        }
        else
        {
            meCurrIndex    = CUR_F_IND;
            meForw0Index   = FOR_S_IND;
            meForw1Index   = FOR_F_IND;
        }

        MFX_CHECK_NULL_PTR1 (pMEFrames[meCurrIndex])
        MFX_CHECK_NULL_PTR1 (pMEFrames[meForw0Index])
        MFX_CHECK_NULL_PTR1 (pMEFrames[meForw1Index])


        MEParams->pSrc = pMEFrames[meCurrIndex];

        MEParams->pSrc->plane[0].ptr[0]      =  cuc->FrameSurface->Data[curIndex]->Y +
                                                GetLumaShift(&cuc->FrameSurface->Info,cuc->FrameSurface->Data[curIndex]) +
                                                bBottom * cuc->FrameSurface->Data[curIndex]->Pitch;
        MEParams->pSrc->plane[0].step[0]     =  cuc->FrameSurface->Data[curIndex]->Pitch <<1;

        MEParams->pSrc->plane[0].ptr[1]      =  cuc->FrameSurface->Data[curIndex]->U +
                                                GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]) +
                                                bBottom * cuc->FrameSurface->Data[curIndex]->Pitch/2;
        MEParams->pSrc->plane[0].step[1]     =  cuc->FrameSurface->Data[curIndex]->Pitch;

        MEParams->pSrc->plane[0].ptr[2]      =  cuc->FrameSurface->Data[curIndex]->V +
                                                GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]) +
                                                bBottom * cuc->FrameSurface->Data[curIndex]->Pitch/2;
        MEParams->pSrc->plane[0].step[2]     =  cuc->FrameSurface->Data[curIndex]->Pitch;

        MEParams->pSrc->WidthMB = pFrameParam->FrameWinMbMinus1+1;      //m_picture_width;
        MEParams->pSrc->HeightMB =pFrameParam->FrameHinMbMinus1+1;    //m_picture_height;
        MEParams->pSrc->type    = UMC::ME_FrmFrw;
        MEParams->pSrc->bBottom = bBottom;

        MEParams->FRefFramesNum = 2;
        MEParams->PredictionType     = UMC::ME_VC1Field2Hybrid;

        MEParams->pRefF[0] = pMEFrames[meForw0Index];
        MEParams->pRefF[0]->padding = (m_bUsePadding)? 16:0;

        MEParams->pRefF[1] = pMEFrames[meForw1Index];
        MEParams->pRefF[1]->padding = (m_bUsePadding)? 16:0;
        MEParams->pRefF[0]->bBottom = !bBottom;
        MEParams->pRefF[1]->bBottom =  bBottom;

        mfxFrameData *pForw  = refData[REF_DATA_FRW];
        mfxFrameData *pRaised = (refData[REF_DATA_CUR])?refData[REF_DATA_CUR]:refData[REF_DATA_FRW];
        MFX_CHECK_NULL_PTR1(pForw);
        MFX_CHECK_NULL_PTR1(pRaised);

        Sts = UMC_VC1_ENCODER::Set2RefFrwFieldPlane(
                    &MEParams->pRefF[0]->plane[0].ptr[0], &MEParams->pRefF[0]->plane[0].step[0],
                    &MEParams->pRefF[1]->plane[0].ptr[0], &MEParams->pRefF[1]->plane[0].step[0],
                    pForw->Y +  GetLumaShift(pRefInfo, pForw), pForw->Pitch,
                    pRaised->Y + GetLumaShift(pRefInfo, pRaised),pRaised->Pitch,
                    bSecondField, bBottom, 0);

        UMC_MFX_CHECK_STS(Sts);

        Sts = UMC_VC1_ENCODER::Set2RefFrwFieldPlane(
                    &MEParams->pRefF[0]->plane[0].ptr[1], &MEParams->pRefF[0]->plane[0].step[1],
                    &MEParams->pRefF[1]->plane[0].ptr[1], &MEParams->pRefF[1]->plane[0].step[1],
                    pForw->U + GetChromaShift(pRefInfo, pForw),  pForw->Pitch>>1,
                    pRaised->U + GetChromaShift(pRefInfo, pRaised),pRaised->Pitch>>1,
                    bSecondField, bBottom, 0);

        UMC_MFX_CHECK_STS(Sts);

        Sts = UMC_VC1_ENCODER::Set2RefFrwFieldPlane(
                    &MEParams->pRefF[0]->plane[0].ptr[2], &MEParams->pRefF[0]->plane[0].step[2],
                    &MEParams->pRefF[1]->plane[0].ptr[2], &MEParams->pRefF[1]->plane[0].step[2],
                    pForw->V + GetChromaShift(pRefInfo, pForw),  pForw->Pitch>>1,
                    pRaised->V + GetChromaShift(pRefInfo, pRaised),pRaised->Pitch>>1,
                    bSecondField, bBottom, 0);

        UMC_MFX_CHECK_STS(Sts);

        MEParams->pRefF[0]->WidthMB  = pFrameParam->FrameWinMbMinus1+1;      //m_picture_width;
        MEParams->pRefF[0]->HeightMB = pFrameParam->FrameHinMbMinus1+1;    //m_picture_height;
        MEParams->pRefF[1]->WidthMB  = pFrameParam->FrameWinMbMinus1+1;      //m_picture_width;
        MEParams->pRefF[1]->HeightMB = pFrameParam->FrameHinMbMinus1+1;    //m_picture_height;
    }

    MEParams->bSecondField       = bSecondField;

    MEParams->Interpolation      = (pFrameParam->UnifiedMvMode == MFX_VC1_ENC_1MV_HALF_BILINEAR) ?
                                     UMC::ME_VC1_Bilinear :UMC::ME_VC1_Bicubic;

    MEParams->SearchDirection    =  UMC::ME_ForwardSearch;
    MEParams->PixelType          =  (pFrameParam->UnifiedMvMode == MFX_VC1_ENC_1MV_HALF_BILINEAR ||
                                     pFrameParam->UnifiedMvMode == MFX_VC1_ENC_1MV_HALF_BICUBIC) ?
                                        UMC::ME_HalfPixel : UMC::ME_QuarterPixel;

    if(!mixed)
    {
        if (m_bUsePadding)
        {
            MEParams->PicRange.top_left.x = -14;
            MEParams->PicRange.top_left.y = -14;
            MEParams->PicRange.bottom_right.x = w + 13;
            MEParams->PicRange.bottom_right.y = (h>>1) +13;
        }
        else
        {
            MEParams->PicRange.top_left.x = 1;
            MEParams->PicRange.top_left.y = 1;
            MEParams->PicRange.bottom_right.x = w - 3;
            MEParams->PicRange.bottom_right.y = (h>>1) - 3;
        }

        MEParams->MbPart =  UMC::ME_Mb16x16;

    }
    else
    {
        if (m_bUsePadding)
        {
            MEParams->PicRange.top_left.x = -7;
            MEParams->PicRange.top_left.y = -7;
            MEParams->PicRange.bottom_right.x = w + 7;
            MEParams->PicRange.bottom_right.y = (h>>1)+ 7;
        }
        else
        {
            MEParams->PicRange.top_left.x = 1;
            MEParams->PicRange.top_left.y = 1;
            MEParams->PicRange.bottom_right.x = w - 3;
            MEParams->PicRange.bottom_right.y = (h>>1)-3;
        }

        MEParams->MbPart =  UMC::ME_Mb8x8;
    }


    mfxU8 refDist = m_pVC1SequenceAdv->GetRefDist(pFrameParam->RefDistance);

    InitScaleInfo(&scInfo,bSecondField,bBottom,refDist,pFrameParam->ExtendedMvRange);

    //scale info
    MEParams->ScaleInfo[0].ME_Bottom = scInfo.bBottom;

    MEParams->ScaleInfo[0].ME_RangeX = scInfo.rangeX;
    MEParams->ScaleInfo[0].ME_RangeY = scInfo.rangeY;

    MEParams->ScaleInfo[0].ME_ScaleOpp = scInfo.scale_opp;

    MEParams->ScaleInfo[0].ME_ScaleSame1 = scInfo.scale_same1;
    MEParams->ScaleInfo[0].ME_ScaleSame2 = scInfo.scale_same2;

    MEParams->ScaleInfo[0].ME_ScaleZoneX = scInfo.scale_zoneX;
    MEParams->ScaleInfo[0].ME_ScaleZoneY = scInfo.scale_zoneY;

    MEParams->ScaleInfo[0].ME_ZoneOffsetX = scInfo.zone_offsetX;
    MEParams->ScaleInfo[0].ME_ZoneOffsetY = scInfo.zone_offsetY;

    MEParams->FieldInfo.ME_bExtendedX = (pFrameParam->ExtendedMvRange & 0x01)!=0;
    MEParams->FieldInfo.ME_bExtendedY = (pFrameParam->ExtendedMvRange & 0x02)!=0;

    MEParams->FieldInfo.ME_limitX = (MEParams->FieldInfo.ME_bExtendedX)? 511:256;
    MEParams->FieldInfo.ME_limitY = (MEParams->FieldInfo.ME_bExtendedY)? 511:256;

    MEParams->FieldInfo.ME_pMVModeField1RefTable_VLC = UMC_VC1_ENCODER::MVModeField2RefTable_VLC[pFrameParam->MvTable];

    MEParams->FieldInfo.ME_pMVSizeOffsetFieldIndexX = (MEParams->FieldInfo.ME_bExtendedX)?
                            UMC_VC1_ENCODER::MVSizeOffsetFieldExIndex:UMC_VC1_ENCODER::MVSizeOffsetFieldIndex;
    MEParams->FieldInfo.ME_pMVSizeOffsetFieldIndexY = (MEParams->FieldInfo.ME_bExtendedY)?
                            UMC_VC1_ENCODER::MVSizeOffsetFieldExIndex:UMC_VC1_ENCODER::MVSizeOffsetFieldIndex;
    MEParams->FieldInfo.ME_pMVSizeOffsetFieldX = (MEParams->FieldInfo.ME_bExtendedX)?
                            UMC_VC1_ENCODER::MVSizeOffsetFieldEx:UMC_VC1_ENCODER::MVSizeOffsetField;
    MEParams->FieldInfo.ME_pMVSizeOffsetFieldY = (MEParams->FieldInfo.ME_bExtendedY)?
                            UMC_VC1_ENCODER::MVSizeOffsetFieldEx:UMC_VC1_ENCODER::MVSizeOffsetField;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncVc1::SetMEParams_I_SM(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams,  mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames)
{
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU8   curIndex = pFrameParam->CurrFrameLabel & 0x7F;
    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);

    MFX_CHECK_NULL_PTR1 (cuc->FrameSurface->Data[curIndex]->Y)
    MFX_CHECK_COND(curIndex < cuc->FrameSurface->Info.NumFrameData)
    MFX_CHECK_NULL_PTR1 (pMEFrames)
    MFX_CHECK_NULL_PTR1 (pMEFrames[CUR_IND])

    MEParams->pSrc          = pMEFrames[CUR_IND];
    MEParams->pSrc->plane[0].ptr[0]  = cuc->FrameSurface->Data[curIndex]->Y + GetLumaShift(&cuc->FrameSurface->Info,   cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[0] = cuc->FrameSurface->Data[curIndex]->Pitch;
    MEParams->pSrc->plane[0].ptr[1]  = cuc->FrameSurface->Data[curIndex]->U + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[1] = cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].ptr[2]  = cuc->FrameSurface->Data[curIndex]->V + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[2] = cuc->FrameSurface->Data[curIndex]->Pitch/2;

    MEParams->pSrc->type    = UMC::ME_FrmIntra;

    MEParams->BRefFramesNum = 0;
    MEParams->FRefFramesNum = 0;
    MEParams->pSrc->WidthMB  = pFrameParam->FrameWinMbMinus1+1 ;
    MEParams->pSrc->HeightMB = pFrameParam->FrameHinMbMinus1+1;
    MEParams->FirstMB = 0;
    MEParams->LastMB = MEParams->pSrc->WidthMB*MEParams->pSrc->HeightMB-1;

    MEParams->SearchDirection =  UMC::ME_IntraSearch;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncVc1::SetMEParams_P_SM(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams,  mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames)
{
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU8  curIndex = pFrameParam->CurrFrameLabel   & 0x7F;
    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);

    MFX_CHECK_COND  (pFrameParam->NumRefFrame >= 1)
    MFX_CHECK_COND  (curIndex < cuc->FrameSurface->Info.NumFrameData)
    MFX_CHECK_NULL_PTR1 (pMEFrames)
    MFX_CHECK_NULL_PTR1 (pMEFrames[CUR_IND])
    MFX_CHECK_NULL_PTR1 (pMEFrames[FOR_IND])


    MEParams->pSrc = pMEFrames[CUR_IND];
    MFX_CHECK_NULL_PTR1(MEParams->pSrc->plane);
    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data[curIndex]->Y);

    MEParams->pSrc->plane[0].ptr[0] =  cuc->FrameSurface->Data[curIndex]->Y + GetLumaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[0]=  cuc->FrameSurface->Data[curIndex]->Pitch;
    MEParams->pSrc->plane[0].ptr[1] =  cuc->FrameSurface->Data[curIndex]->U + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[1]=  cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].ptr[2] =  cuc->FrameSurface->Data[curIndex]->V + GetChromaShift(&cuc->FrameSurface->Info,cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[2]=  cuc->FrameSurface->Data[curIndex]->Pitch/2;

    MEParams->pSrc->type    = UMC::ME_FrmFrw;

    MEParams->pRefF[0]      = pMEFrames[FOR_IND] ;
    MFX_CHECK_NULL_PTR1(MEParams->pRefF[0]->plane);


    mfxFrameData *pForw = refData[REF_DATA_FRW];
    MFX_CHECK_NULL_PTR1(pForw);

    MEParams->pRefF[0]->plane[0].ptr[0]  =  pForw->Y + GetLumaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[0] =  pForw->Pitch;
    MEParams->pRefF[0]->plane[0].ptr[1]  =  pForw->U + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[1] =  pForw->Pitch>>1;
    MEParams->pRefF[0]->plane[0].ptr[2]  =  pForw->V + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[2] =  pForw->Pitch>>1;

    MEParams->pRefF[0]->padding = (m_bUsePadding)? 32:0;

    MEParams->pSrc->WidthMB = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pSrc->HeightMB = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pRefF[0]->WidthMB  = pFrameParam->FrameWinMbMinus1+1;      //m_picture_width;
    MEParams->pRefF[0]->HeightMB = pFrameParam->FrameHinMbMinus1+1;    //m_picture_height;

    MEParams->Interpolation    = (pFrameParam->UnifiedMvMode == MFX_VC1_ENC_1MV_HALF_BILINEAR)?
                                    UMC::ME_VC1_Bilinear :UMC::ME_VC1_Bicubic;

    MEParams->SearchDirection =  UMC::ME_ForwardSearch;

    MEParams->PixelType =  (pFrameParam->UnifiedMvMode == MFX_VC1_ENC_1MV_HALF_BILINEAR ||
                                     pFrameParam->UnifiedMvMode == MFX_VC1_ENC_1MV_HALF_BICUBIC)?
                                     UMC::ME_HalfPixel : UMC::ME_QuarterPixel;

    MEParams->MbPart            =  UMC::ME_Mb16x16;

    MEParams->PredictionType    = UMC::ME_VC1Hybrid;

    if (m_bUsePadding)
    {
        MEParams->PicRange.top_left.x       = -14;
        MEParams->PicRange.top_left.y       = -14;
        MEParams->PicRange.bottom_right.x   = w + 13 ;
        MEParams->PicRange.bottom_right.y   = h + 13;
    }
    else
    {
        MEParams->PicRange.top_left.x       = 1;
        MEParams->PicRange.top_left.y       = 1;
        MEParams->PicRange.bottom_right.x   = w - 3;
        MEParams->PicRange.bottom_right.y   = h - 3;
    }

    MEParams->SearchRange.x             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    MEParams->SearchRange.y             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1] - 1;
    MEParams->BRefFramesNum             = 0;
    MEParams->FRefFramesNum             = 1;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncVc1::SetMEParams_PMixed_SM(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame* *  pMEFrames)
{
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU8  curIndex  = pFrameParam->CurrFrameLabel   & 0x7F;
    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);

    MFX_CHECK_COND  (pFrameParam->NumRefFrame >= 1)
    MFX_CHECK_COND  (curIndex < cuc->FrameSurface->Info.NumFrameData)
    MFX_CHECK_NULL_PTR1 (pMEFrames)
    MFX_CHECK_NULL_PTR1 (pMEFrames[CUR_IND])
    MFX_CHECK_NULL_PTR1 (pMEFrames[FOR_IND])


    MEParams->pSrc = pMEFrames[CUR_IND];
    MFX_CHECK_NULL_PTR1(MEParams->pSrc->plane);
    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data[curIndex]->Y);

    MEParams->pSrc->plane[0].ptr[0]      =  cuc->FrameSurface->Data[curIndex]->Y + GetLumaShift  (&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[0]     =  cuc->FrameSurface->Data[curIndex]->Pitch;
    MEParams->pSrc->plane[0].ptr[1]      =  cuc->FrameSurface->Data[curIndex]->U + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[1]     =  cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].ptr[2]      =  cuc->FrameSurface->Data[curIndex]->V + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[2]     =  cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->WidthMB  = pFrameParam->FrameWinMbMinus1+1 ;
    MEParams->pSrc->HeightMB = pFrameParam->FrameHinMbMinus1+1;
    MEParams->pSrc->type    = UMC::ME_FrmFrw;

    MEParams->pRefF[0]      = pMEFrames[FOR_IND] ;
    MFX_CHECK_NULL_PTR1(MEParams->pRefF[0]->plane);
    mfxFrameData *pForw = refData[REF_DATA_FRW];
    MFX_CHECK_NULL_PTR1(pForw);

    MEParams->pRefF[0]->plane[0].ptr[0]  =  pForw->Y + GetLumaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[0] =  pForw->Pitch;
    MEParams->pRefF[0]->plane[0].ptr[1]  =  pForw->U + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[1] =  pForw->Pitch>>1;
    MEParams->pRefF[0]->plane[0].ptr[2]  =  pForw->V + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[2] =  pForw->Pitch>>1;
    MEParams->pRefF[0]->WidthMB  = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pRefF[0]->HeightMB = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pRefF[0]->padding = (m_bUsePadding)? 32:0;

    MEParams->Interpolation     = UMC::ME_VC1_Bicubic;

    MEParams->SearchDirection    =  UMC::ME_ForwardSearch;
    MEParams->PixelType          =  UMC::ME_QuarterPixel;

    MEParams->MbPart             =  UMC::ME_Mb8x8;

    MEParams->PredictionType     = UMC::ME_VC1Hybrid;
    if (m_bUsePadding)
    {
        MEParams->PicRange.top_left.x       = -7;
        MEParams->PicRange.top_left.y       = -7;
        MEParams->PicRange.bottom_right.x   = w  + 7;
        MEParams->PicRange.bottom_right.y   = h  + 7;
    }
    else
    {
        MEParams->PicRange.top_left.x       = 1;
        MEParams->PicRange.top_left.y       = 1;
        MEParams->PicRange.bottom_right.x   = w - 3;
        MEParams->PicRange.bottom_right.y   = h - 3;
    }

    MEParams->SearchRange.x             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    MEParams->SearchRange.y             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1] - 1;
    MEParams->BRefFramesNum = 0;
    MEParams->FRefFramesNum = 1;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncVc1::SetMEParams_B_SM(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames)
{
    mfxStatus   MFXSts = MFX_ERR_NONE;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU8       curIndex     =  pFrameParam->CurrFrameLabel   & 0x7F;
    mfxU16 w = pPar->mfx.FrameInfo.Width;
    mfxU16 h = pPar->mfx.FrameInfo.Height;

    GetRealFrameSize(&pPar->mfx, w, h);
    Ipp32u hMB = pFrameParam->FrameHinMbMinus1+1;
    Ipp32u wMB = pFrameParam->FrameWinMbMinus1+1;
    mfxI16 n = 0;


    //BFraction params
    Ipp8u       nom = 0;
    Ipp8u       denom = 0;
    Ipp8u       scaleFactor = 0;

    MFXSts = GetBFractionParameters (pFrameParam->CodedBfraction, nom, denom,  scaleFactor);
    MFX_CHECK_STS(MFXSts);

    void        (* GetMVDirect) (Ipp16s x, Ipp16s y, Ipp32s scaleFactor,
        UMC_VC1_ENCODER::sCoordinate * mvF, UMC_VC1_ENCODER::sCoordinate *mvB);

    MFX_CHECK_COND  (pFrameParam->NumRefFrame >= 2)
    MFX_CHECK_COND  (curIndex < cuc->FrameSurface->Info.NumFrameData)
    MFX_CHECK_NULL_PTR1 (pMEFrames)
    MFX_CHECK_NULL_PTR1 (pMEFrames[CUR_IND])
    MFX_CHECK_NULL_PTR1 (pMEFrames[FOR_IND])
    MFX_CHECK_NULL_PTR1 (pMEFrames[BACK_IND])

    MEParams->pSrc = pMEFrames[CUR_IND];
    MFX_CHECK_NULL_PTR1(MEParams->pSrc->plane);
    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data[curIndex]->Y);

    MEParams->pSrc->plane[0].ptr[0]      =  cuc->FrameSurface->Data[curIndex]->Y + GetLumaShift(&cuc->FrameSurface->Info,cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[0]     =  cuc->FrameSurface->Data[curIndex]->Pitch;
    MEParams->pSrc->plane[0].ptr[1]      =  cuc->FrameSurface->Data[curIndex]->U + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[1]     =  cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->plane[0].ptr[2]      =  cuc->FrameSurface->Data[curIndex]->V + GetChromaShift(&cuc->FrameSurface->Info, cuc->FrameSurface->Data[curIndex]);
    MEParams->pSrc->plane[0].step[2]     =  cuc->FrameSurface->Data[curIndex]->Pitch/2;
    MEParams->pSrc->WidthMB   = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pSrc->HeightMB  = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pSrc->type    = UMC::ME_FrmBidir;

    MEParams->FirstMB = 0;
    MEParams->LastMB = MEParams->pSrc->WidthMB*MEParams->pSrc->HeightMB-1;

    MEParams->pRefF[0]      = pMEFrames[FOR_IND] ;
    MFX_CHECK_NULL_PTR1(MEParams->pRefF[0]->plane);

    mfxFrameData *pForw = refData[REF_DATA_FRW];
    MFX_CHECK_NULL_PTR1(pForw);

    MEParams->pRefF[0]->plane[0].ptr[0]  =  pForw->Y + GetLumaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[0] =  pForw->Pitch;
    MEParams->pRefF[0]->plane[0].ptr[1]  =  pForw->U + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[1] =  pForw->Pitch>>1;
    MEParams->pRefF[0]->plane[0].ptr[2]  =  pForw->V + GetChromaShift(pRefInfo, pForw);
    MEParams->pRefF[0]->plane[0].step[2] =  pForw->Pitch>>1;
    MEParams->pRefF[0]->WidthMB   = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pRefF[0]->HeightMB  = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pRefF[0]->padding = (m_bUsePadding)? 32:0;

    MEParams->pRefB[0]      = pMEFrames[BACK_IND];

    MFX_CHECK_NULL_PTR1 (MEParams->pRefB[0]->plane);

    mfxFrameData *pBackw = refData[REF_DATA_BKW];
    MFX_CHECK_NULL_PTR1(pBackw);

    MEParams->pRefB[0]->plane[0].ptr[0]  =  pBackw->Y + GetLumaShift(pRefInfo, pBackw);
    MEParams->pRefB[0]->plane[0].step[0] =  pBackw->Pitch;
    MEParams->pRefB[0]->plane[0].ptr[1]  =  pBackw->U + GetChromaShift(pRefInfo, pBackw);
    MEParams->pRefB[0]->plane[0].step[1] =  pBackw->Pitch>>1;
    MEParams->pRefB[0]->plane[0].ptr[2]  =  pBackw->V + GetChromaShift(pRefInfo, pBackw);
    MEParams->pRefB[0]->plane[0].step[2] =  pBackw->Pitch>>1;
    MEParams->pRefB[0]->WidthMB   = pFrameParam->FrameWinMbMinus1+1;
    MEParams->pRefB[0]->HeightMB  = pFrameParam->FrameHinMbMinus1+1;

    MEParams->pRefB[0]->padding = (m_bUsePadding)? 32:0;

    UMC_VC1_ENCODER::sCoordinate MVFDirect    = {0,0};
    UMC_VC1_ENCODER::sCoordinate MVBDirect    = {0,0};

    UMC_VC1_ENCODER::sCoordinate                 MVPredMin = {-60,-60};
    UMC_VC1_ENCODER::sCoordinate                 MVPredMax = {(w - 1)*4, (h - 1)*4};

    mfxI16Pair*    pSavedMV = 0;

    Ipp32u i = 0;
    Ipp32u j = 0;

    n = GetExBufferIndex(cuc,  MFX_CUC_VC1_MVDATA);
    MFX_CHECK_COND (n>=0)

    mfxExtVc1MvData* pMVs = (mfxExtVc1MvData*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMVs->NumMv >= wMB*hMB)
    pSavedMV = pMVs->Mv;

    GetMVDirect = (pFrameParam->UnifiedMvMode != MFX_VC1_ENC_1MV)?
        UMC_VC1_ENCODER::GetMVDirectHalf : UMC_VC1_ENCODER::GetMVDirectQuarter;

    MEParams->Interpolation      =  (MFX_VC1_ENC_1MV_HALF_BILINEAR == pFrameParam->UnifiedMvMode) ?
                                            UMC::ME_VC1_Bilinear :UMC::ME_VC1_Bicubic;
    MEParams->SearchDirection    = UMC::ME_BidirSearch;

    MEParams->PixelType          = (MFX_VC1_ENC_1MV_HALF_BILINEAR == pFrameParam->UnifiedMvMode
                                 || MFX_VC1_ENC_1MV_HALF_BICUBIC == pFrameParam->UnifiedMvMode) ?
                                   UMC::ME_HalfPixel : UMC::ME_QuarterPixel;
    MEParams->MbPart             = UMC::ME_Mb16x16;

    MEParams->PredictionType     = UMC::ME_VC1;

    if (m_bUsePadding)
    {
        MEParams->PicRange.top_left.x = -14;
        MEParams->PicRange.top_left.y = -14;
        MEParams->PicRange.bottom_right.x = w + 13;
        MEParams->PicRange.bottom_right.y = h + 13;
    }
    else
    {
        MEParams->PicRange.top_left.x = 1;
        MEParams->PicRange.top_left.y = 1;
        MEParams->PicRange.bottom_right.x = w - 3;
        MEParams->PicRange.bottom_right.y = h - 3;
    }

    MEParams->SearchRange.x   = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    MEParams->SearchRange.y   = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1] - 1;
    MEParams->BRefFramesNum  = 1;
    MEParams->FRefFramesNum  = 1;

    for (i=0; i < hMB; i++)
    {
        for (j=0; j < wMB; j++)
        {
            GetMVDirect (pSavedMV->x,pSavedMV->y,scaleFactor, &MVFDirect, &MVBDirect);
            ScalePredict(&MVFDirect, j*16*4,i*16*4,MVPredMin,MVPredMax);
            ScalePredict(&MVBDirect, j*16*4,i*16*4,MVPredMin,MVPredMax);

            MEParams->pRefF[0]->MVDirect[(j + i*wMB)].x = MVFDirect.x;
            MEParams->pRefF[0]->MVDirect[(j + i*wMB)].y = MVFDirect.y;

            MEParams->pRefB[0]->MVDirect[(j + i*wMB)].x = MVBDirect.x;
            MEParams->pRefB[0]->MVDirect[(j + i*wMB)].y = MVBDirect.y;

            pSavedMV ++;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncVc1::GetVideoParam(mfxVideoParam *par)
{
    mfxStatus   MFXSts = MFX_ERR_NONE;

    MFX_CHECK_INIT(m_InitFlag);

    MFX_INTERNAL_CPY(par, &m_VideoParam, sizeof(mfxVideoParam));

    return MFXSts;
}

mfxStatus MFXVideoEncVc1::GetFrameParam(mfxFrameParam *par)
{
    mfxStatus   MFXSts = MFX_ERR_NONE;

    MFX_CHECK_INIT(m_InitFlag);

    MFX_INTERNAL_CPY(par, &m_FrameParam, sizeof(mfxFrameParam));

    return MFXSts;
}

mfxStatus MFXVideoEncVc1::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    if (out == NULL)
        return MFX_ERR_NULL_PTR;
    if (in)
    {
        memcpy (out,in, sizeof (mfxVideoParam));
        CorrectMfxVideoParams(out);
    }
    else
    {
        SetMfxVideoParams(out, true);
    }

    return MFXSts;
}
mfxStatus MFXVideoEncVc1::GetSliceParam(mfxSliceParam *par)
{
    MFX_INTERNAL_CPY(par,&m_SliceParam, sizeof(mfxSliceParam));
    return MFX_ERR_NONE;
}

#undef FOR_IND
#undef BACK_IND
#undef CUR_IND
#undef FOR_F_IND
#undef BACK_F_IND
#undef CUR_F_IND
#undef FOR_S_IND
#undef BACK_S_IND
#undef CUR_S_IND

#endif //MFX_ENABLE_VC1_VIDEO_ENCODER
