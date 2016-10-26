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
#include "mfx_vc1_enc_pak_adv.h"

#include "umc_vc1_enc_def.h"
#include "umc_vc1_enc_planes.h"
#include "mfxdefs.h"

#ifdef VC1_ENC_DEBUG_ON
#include "umc_vc1_enc_debug.h"
#endif

mfxStatus MFXVideoPakVc1ADV::Init(mfxVideoParam *par)
{
   UMC::Status              ret = UMC::UMC_OK;
   mfxStatus                MFXSts = MFX_ERR_NONE;
   UMC::VC1EncoderParams    UMCParams;
   Ipp32u                   memSize = 0;
   UMC::MeInitParams        MEParamsInit;
   Ipp8u*                   pBuffer = 0;

   mfxU16 w = par->mfx.FrameInfo.Width;
   mfxU16 h = par->mfx.FrameInfo.Height;

   GetRealFrameSize(&par->mfx, w, h);

   Ipp32u                   wMB = (w + 15)/16;
   Ipp32u                   hMB = ((h/2 + 15)/16)*2;

   bool bNV12 = (par->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12);

   Close();

   m_pPadChromaFunc     = (bNV12)? ReplicateBorderChroma_NV12 : ReplicateBorderChroma_YV12;
   GetChromaShift       = (bNV12)? GetChromaShiftNV12:GetChromaShiftYV12;
   SetFrame             = (bNV12)? SetFrameNV12 : SetFrameYV12;
   CopyFrame            = (bNV12)? copyFrameNV12:copyFrameYV12;
   CopyField            = (bNV12)? copyFieldNV12:copyFieldYV12;
   GetPlane_Prog        = (bNV12)? GetPlane_ProgNV12:GetPlane_ProgYV12;
   GetPlane_Field       = (bNV12)? GetPlane_FieldNV12:GetPlane_FieldYV12;
   PadFrameProgressive  = (bNV12)? PadFrameProgressiveNV12:PadFrameProgressiveYV12;
   PadFrameField        = (bNV12)? PadFrameFieldNV12:PadFrameFieldYV12;


   memSize = sizeof(UMC_VC1_ENCODER::VC1EncoderPictureADV);
   MFXSts = m_pCore->AllocBuffer(memSize, MFX_VC1_PICTURE_MEMORY, &m_PicID);
   if(MFXSts != MFX_ERR_NONE || !m_PicID)
       return MFX_ERR_MEMORY_ALLOC;

   MFXSts = m_pCore->LockBuffer(m_PicID, &pBuffer);
   if(MFXSts != MFX_ERR_NONE || !pBuffer)
       return MFX_ERR_MEMORY_ALLOC;


   m_pVC1EncoderPicture  = new (pBuffer) UMC_VC1_ENCODER::VC1EncoderPictureADV (bNV12);

   //--------------------- sequence header memory allocation-----------------------

   memSize = sizeof(UMC_VC1_ENCODER::VC1EncoderSequenceADV);
   MFXSts = m_pCore->AllocBuffer(memSize, MFX_VC1_SH_MEMORY, &m_SeqID);
   if(MFXSts != MFX_ERR_NONE || !m_SeqID)
       return MFX_ERR_MEMORY_ALLOC;

   MFXSts = m_pCore->LockBuffer(m_SeqID, &pBuffer);
   if(MFXSts != MFX_ERR_NONE || !pBuffer)
       return MFX_ERR_MEMORY_ALLOC;

   m_pVC1EncoderSequence = new (pBuffer) UMC_VC1_ENCODER::VC1EncoderSequenceADV;

   //-------------------------coded MB memory allocation----------------------------

   memSize = sizeof(UMC_VC1_ENCODER::VC1EncoderCodedMB)*wMB*hMB;
   MFXSts = m_pCore->AllocBuffer(memSize, MFX_VC1_CODED_MB_MEMORY, &m_CodedMBID);
   if(MFXSts != MFX_ERR_NONE || !m_CodedMBID)
       return MFX_ERR_MEMORY_ALLOC;

   MFXSts = m_pCore->LockBuffer(m_CodedMBID, &pBuffer);
   if(MFXSts != MFX_ERR_NONE || !pBuffer)
       return MFX_ERR_MEMORY_ALLOC;

   m_pVC1EncoderCodedMB  = new  (pBuffer) UMC_VC1_ENCODER::VC1EncoderCodedMB[w*h];

   //------------------------- MB memory allocation -------------------------------

   memSize = sizeof(UMC_VC1_ENCODER::VC1EncoderMBs)*wMB*2;
   MFXSts = m_pCore->AllocBuffer(memSize, MFX_VC1_MB_MEMORY, &m_MBID);
   if(MFXSts != MFX_ERR_NONE || !m_MBID)
       return MFX_ERR_MEMORY_ALLOC;

   MFXSts = m_pCore->LockBuffer(m_MBID, &pBuffer);
   if(MFXSts != MFX_ERR_NONE || !pBuffer)
       return MFX_ERR_MEMORY_ALLOC;

   m_pVC1EncoderMBs  = new  (pBuffer) UMC_VC1_ENCODER::VC1EncoderMBs[wMB*2];

   //---------------------------------------------------------------------------------

   if (!m_pVC1EncoderPicture || !m_pVC1EncoderSequence || !m_pVC1EncoderMBs)
   {
        return MFX_ERR_MEMORY_ALLOC;
   }

   /*Those parameters must be set using target usage or extended buffer */
   {
       mfxU8 meSpeed            = 0;
       bool  useFB              = false;
       bool  fastFB             = false;
       bool  intComp            = false;
       bool  changeInterType    = false;
       bool  changeVLCTable     = false;
       bool  trellisQuant       = false;
       bool  VSTransform        = false;
       bool  deblocking         = false;
       mfxU8 smoothing          = 0;
       bool  fastUVMC           = false;

       SetPROParameters (par->mfx.TargetUsage & 0x0F,
           meSpeed, useFB,fastFB, intComp,
           changeInterType, changeVLCTable,
           trellisQuant,m_bUsePadding,
           VSTransform,deblocking,smoothing,fastUVMC);

       ExtVC1SequenceParams *pSH_Ex = (ExtVC1SequenceParams *)GetExBufferIndex(par, MFX_CUC_VC1_SEQPARAMSBUF);
       if (pSH_Ex)
       {
           meSpeed                 =  pSH_Ex->MESpeed;
           useFB                   =  pSH_Ex->useFB;
           fastFB                  =  pSH_Ex->fastFB;
           intComp                 =  pSH_Ex->intensityCompensation;
           changeInterType         =  pSH_Ex->changeInterpolationType;
           changeVLCTable          =  pSH_Ex->changeVLCTables;
           trellisQuant            =  pSH_Ex->trellisQuantization;

           VSTransform  = pSH_Ex->vsTransform;
           deblocking   = pSH_Ex->deblocking;
           smoothing    = pSH_Ex->overlapSmoothing;
           fastUVMC     = !pSH_Ex->noFastUV;
       }
       /*init UMC parameters. It's needed for SH initialization.*/
       {
           UMCParams.info.clip_info.width    =  w;
           UMCParams.info.clip_info.height   =  h;
           UMCParams.m_bInterlace            =  (par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE);

           MFXSts = GetUMCProfile(par->mfx.CodecProfile,&UMCParams.profile);
           MFX_CHECK_STS (MFXSts);
           MFXSts = GetUMCLevel(par->mfx.CodecProfile, par->mfx.CodecLevel, &UMCParams.level);
           MFX_CHECK_STS (MFXSts);

           UMCParams.m_uiGOPLength  = par->mfx.GopPicSize;
           UMCParams.m_uiBFrmLength = par->mfx.GopRefDist - 1;

           UMCParams.m_bVSTransform           =  VSTransform;
           UMCParams.m_bDeblocking            =  deblocking;
           UMCParams.m_uiOverlapSmoothing     =  smoothing;
           UMCParams.m_bFastUVMC              =  fastUVMC ;
       }
       m_bUsedTrellisQuantization = trellisQuant;
   }
   ret = m_pVC1EncoderSequence->Init(&UMCParams);
   UMC_MFX_CHECK_STS(ret)

   //ret = m_pVC1EncoderSequence->CheckParameters()
   //UMC_MFX_CHECK_STS(ret)

   memSize = m_pVC1EncoderMBs->CalcAllocMemorySize(wMB,hMB,bNV12);

   MFXSts = m_pCore->AllocBuffer(memSize, MFX_VC1_MB_MEMORY,&m_MBsID);
   if(MFXSts != MFX_ERR_NONE || !m_MBsID)
       return MFX_ERR_MEMORY_ALLOC;

   MFXSts = m_pCore->LockBuffer(m_MBsID, &pBuffer);
   if(MFXSts != MFX_ERR_NONE || !pBuffer)
       return MFX_ERR_MEMORY_ALLOC;

   ret = m_pVC1EncoderMBs->Init(pBuffer, memSize,wMB,hMB, bNV12);
   UMC_MFX_CHECK_STS(ret)

   ret = m_pVC1EncoderPicture->Init(m_pVC1EncoderSequence,m_pVC1EncoderMBs,m_pVC1EncoderCodedMB);
   UMC_MFX_CHECK_STS(ret)

   MFX_INTERNAL_CPY(&m_mfxVideoParam, par, sizeof(m_mfxVideoParam));

   //--------------------UMC ME -----------------------------------------------
    memset(&MEParamsInit,0,sizeof(MEParamsInit));

    m_pME = new UMC::MeVC1;
    if (!m_pME)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    MEParamsInit.WidthMB         = wMB;
    MEParamsInit.HeightMB        = hMB;
    MEParamsInit.refPadding      = 32;
    MEParamsInit.MbPart          = UMC::ME_Mb16x16;
    MEParamsInit.MaxNumOfFrame   = 1;
    MEParamsInit.bNV12           = bNV12;
    Ipp32s size=0;
    if(!m_pME->Init(&MEParamsInit, NULL, size))
           return MFX_ERR_UNSUPPORTED;

    MFXSts = m_pCore->AllocBuffer(size, MFX_VC1_ME_MEMORY,&m_MEID);
    if(MFXSts != MFX_ERR_NONE || !m_MEID)
        return MFX_ERR_MEMORY_ALLOC;

    MFXSts = m_pCore->LockBuffer(m_MEID, &pBuffer);
    if(MFXSts != MFX_ERR_NONE || !pBuffer)
       MFXSts = MFX_ERR_MEMORY_ALLOC;

    if(!m_pME->Init(&MEParamsInit, pBuffer, size))
           return MFX_ERR_UNSUPPORTED;;

    m_pMeFrame = &MEParamsInit.pFrames[0];

   // --------------- User Data -----------------------------------------

    m_UserDataBufferSize      = w*h;

    MFXSts = m_pCore->AllocBuffer( m_UserDataBufferSize, MFX_VC1_ME_MEMORY,&m_UDBufferID);
    if(MFXSts != MFX_ERR_NONE || !m_UDBufferID)
        MFXSts = MFX_ERR_MEMORY_ALLOC;

    MFXSts = m_pCore->LockBuffer(m_UDBufferID, &m_UserDataBuffer);
    if(MFXSts != MFX_ERR_NONE || !m_UserDataBuffer)
        MFXSts = MFX_ERR_MEMORY_ALLOC;

    m_UserDataSize        = 0;

    //--------------------------------------------------------------------

   m_InitFlag = true;

#ifdef VC1_ENC_DEBUG_ON
    if(!UMC_VC1_ENCODER::pDebug)
    {
        UMC_VC1_ENCODER::pDebug = new UMC_VC1_ENCODER::VC1EncDebug;
        assert(UMC_VC1_ENCODER::pDebug != NULL);
        UMC_VC1_ENCODER::pDebug->Init(wMB, hMB, bNV12);
    }
#endif
   return MFX_ERR_NONE;
}
mfxStatus MFXVideoPakVc1ADV::Reset(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxVideoParam * pOldParams = &m_mfxVideoParam;
    mfxVideoParam * pNewParams = par;
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
mfxStatus MFXVideoPakVc1ADV::Close(void)
{
   if (m_pVC1EncoderPicture)
   {
        m_pVC1EncoderPicture->Close();
        m_pVC1EncoderPicture = 0;
   }

   if (m_PicID)
   {
       m_pCore->UnlockBuffer(m_PicID);
       m_pCore->FreeBuffer(m_PicID);
       m_PicID = 0;
   }

   if (m_SeqID)
   {
       m_pCore->UnlockBuffer(m_SeqID);
       m_pCore->FreeBuffer(m_SeqID);
       m_SeqID = 0;
   }
   m_pVC1EncoderSequence = 0;

   if (m_CodedMBID)
   {
       m_pCore->UnlockBuffer(m_CodedMBID);
       m_pCore->FreeBuffer(m_CodedMBID);
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
       m_pCore->UnlockBuffer(m_MBsID);
       m_pCore->FreeBuffer(m_MBsID);
       m_MBsID = 0;
   }
   if (m_MBID)
   {
       m_pCore->UnlockBuffer(m_MBID);
       m_pCore->FreeBuffer(m_MBID);
       m_MBID = 0;
   }
   m_pVC1EncoderMBs     = 0;

   if (m_MEID)
   {
       m_pCore->UnlockBuffer(m_MEID);
       m_pCore->FreeBuffer(m_MEID);
       m_MEID = 0;
   }
   if (m_pME)
   {
        m_pME->Close();
        delete m_pME;
        m_pME = 0;
   }
    if (m_UDBufferID)
    {
        m_pCore->UnlockBuffer(m_UDBufferID);
        m_pCore->FreeBuffer(m_UDBufferID);
        m_UDBufferID = 0;
    }
    m_pMeFrame   = 0;
    m_InitFlag = false;
    m_UserDataBufferSize = 0;
    m_UserDataBuffer = 0;
    m_UserDataSize = 0;

#ifdef VC1_ENC_DEBUG_ON
    if(UMC_VC1_ENCODER::pDebug)
    {
        delete UMC_VC1_ENCODER::pDebug;
        UMC_VC1_ENCODER::pDebug = NULL;
    }
#endif

   return MFX_ERR_NONE;
}

mfxStatus MFXVideoPakVc1ADV::Query(mfxVideoParam *in, mfxVideoParam *out)
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
        SetMfxVideoParams(out,true);
    }

    return MFXSts;
}
mfxStatus MFXVideoPakVc1ADV::GetVideoParam(mfxVideoParam *par)
{
    mfxStatus   MFXSts = MFX_ERR_NONE;

    MFX_CHECK_INIT(m_InitFlag);

    MFX_INTERNAL_CPY(par, &m_mfxVideoParam, sizeof(mfxVideoParam));

    return MFXSts;
}
mfxStatus MFXVideoPakVc1ADV::GetFrameParam(mfxFrameParam *par)
{
    mfxStatus   MFXSts = MFX_ERR_NONE;

    MFX_CHECK_INIT(m_InitFlag);

    MFX_INTERNAL_CPY(par, &m_mfxFrameParam, sizeof(mfxFrameParam));

    return MFXSts;
}

mfxStatus MFXVideoPakVc1ADV::GetSliceParam(mfxSliceParam *par)
{
    MFX_INTERNAL_CPY(par,&m_mfxSliceParam, sizeof(mfxSliceParam));
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPakVc1ADV::RunSeqHeader(mfxFrameCUC *cuc)
{
    UMC_VC1_ENCODER::VC1EncoderBitStreamAdv BS;
    UMC::MediaData  data;
    UMC::Status ret = UMC::UMC_OK;
    mfxBitstream *bs = cuc->Bitstream;

    MFX_CHECK_COND(bs->MaxLength - bs->DataOffset>0)
    ret = data.SetBufferPointer(bs->Data + bs->DataOffset, bs->MaxLength - bs->DataOffset);
    UMC_MFX_CHECK_STS(ret)
    BS.Init(&data);

    ret = m_pVC1EncoderSequence->WriteSeqHeader(&BS);
    UMC_MFX_CHECK_STS(ret)

    bs->DataLength = (mfxU32)BS.GetDataLen();

    return MFX_ERR_NONE;
}
mfxStatus MFXVideoPakVc1ADV::RunGopHeader(mfxFrameCUC *cuc)   { return MFX_ERR_UNSUPPORTED; }
mfxStatus MFXVideoPakVc1ADV::RunPicHeader(mfxFrameCUC *cuc)   { return MFX_ERR_UNSUPPORTED; }
mfxStatus MFXVideoPakVc1ADV::RunSliceHeader(mfxFrameCUC *cuc) { return MFX_ERR_UNSUPPORTED; }
mfxStatus MFXVideoPakVc1ADV::InsertUserData(mfxU8 *ud,mfxU32 len,mfxU64 ts,mfxBitstream *bs)
{
    if (len > m_UserDataBufferSize || CheckUserData(ud,len)!=MFX_ERR_NONE)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    m_UserDataSize = len;
    memcpy (m_UserDataBuffer,ud,len);

    return MFX_ERR_NONE;
}
mfxStatus MFXVideoPakVc1ADV::InsertBytes(mfxU8 *data, mfxU32 len, mfxBitstream *bs)            { return MFX_ERR_UNSUPPORTED; }
mfxStatus MFXVideoPakVc1ADV::AlignBytes(mfxU8 pattern, mfxU32 len, mfxBitstream *bs)           { return MFX_ERR_UNSUPPORTED; }

#define unlock_buff                                                                          \
if (frameDataLocked)                                                                         \
{                                                                                            \
    for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)                               \
    {                                                                                        \
      m_pCore->UnlockFrame(cuc->FrameSurface->Data[i]->MemId,cuc->FrameSurface->Data[i]);    \
    }                                                                                        \
    frameDataLocked  =  false;                                                               \
}

#define UMC_MFX_CHECK_STS_1(err)    if (err != UMC::UMC_OK)                  {unlock_buff; return MFX_ERR_UNSUPPORTED;}
#define UMC_MFX_CHECK_WARN_1(err)   if (err == UMC::UMC_ERR_NOT_ENOUGH_DATA) {unlock_buff; return MFX_WRN_MORE_DATA;}

#define MFX_CHECK_STS_1(err)        if (err != MFX_ERR_NONE)                 {unlock_buff; return err;}
#define MFX_CHECK_COND_1(cond)      if  (!(cond))                            {unlock_buff; return MFX_ERR_UNSUPPORTED;}
#define MFX_CHECK_NULL_PTR1_1(pointer) if (pointer ==0)                      {unlock_buff;return MFX_ERR_NULL_PTR;}
#define MFX_CHECK_NULL_PTR2_1(p1, p2)  if (p1 ==0 || p2 ==0 )                {unlock_buff;return MFX_ERR_NULL_PTR;}

mfxStatus MFXVideoPakVc1ADV::RunSlicePAK(mfxFrameCUC *cuc)
{
    UMC_VC1_ENCODER::VC1EncoderBitStreamAdv BS;
    UMC::MediaData                          data;
    UMC::Status                             ret = UMC::UMC_OK;
    UMC_VC1_ENCODER::InitPictureParams      initPicParam;

    UMC_VC1_ENCODER::Frame                  Plane;
    mfxStatus                               MFXSts = MFX_ERR_NONE;
    Ipp8u                                   scaleFactor=0;
    UMC_VC1_ENCODER::ePType                 UMCtype;
    mfxI16Pair*                             pSavedMV   = 0;
    Ipp8u*                                  pDirection = 0;

    Ipp8u                                   doubleQuant = 2;
    //mfxI16 st;
    PadFrame                                pPadFrame= 0;
    //mfxU8                                   lockMarker = 0;

    Ipp32u                                  smoothing = 0;
    bool                                    frameDataLocked  = false;

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
             MFXSts =  m_pCore->LockFrame(cuc->FrameSurface->Data[i]->MemId, cuc->FrameSurface->Data[i]);
             MFX_CHECK_STS_1(MFXSts);
            }
        }
    }

    bool      FieldPicture = pFrameParam->FieldPicFlag;

    if (m_pVC1EncoderSequence->IsOverlap())
    {
        smoothing = ((pFrameParam->PicDeblocked>>5)&0x01)? 3:1;
    }

    MFXSts = GetUMCPictureType(pFrameParam->FrameType,pFrameParam->FrameType2nd,
        pFrameParam->FieldPicFlag, pFrameParam->InterlacedFrameFlag,
        pFrameParam->Pic4MvAllowed,&UMCtype);
    MFX_CHECK_STS_1(MFXSts);

    MFX_INTERNAL_CPY( &m_mfxFrameParam,pFrameParam, sizeof(mfxFrameParam));

    vm_char   cLastError[VC1_ENC_STR_LEN];
    UMC::MeParams MEParams;
    mfxU8  curIndex     =  pFrameParam->CurrFrameLabel   & 0x7F;
    mfxU8  raisedIndex  =  pFrameParam->DecFrameLabel    & 0x7F;

    mfxBitstream* bs = cuc->Bitstream;

    bool bSecondField = pFrameParam->SecondFieldFlag;
    cuc->FrameSurface->Data[curIndex]->Field = FieldPicture;

    MEParams.pSrc = m_pMeFrame;

    MFX_CHECK_COND_1(bs->MaxLength - bs->DataOffset>0)
        ret = data.SetBufferPointer(bs->Data + bs->DataOffset, bs->MaxLength - bs->DataOffset);
    UMC_MFX_CHECK_STS_1(ret)
        BS.Init(&data);

    MFXSts = GetBFractionParameters (pFrameParam->CodedBfraction,
        initPicParam.uiBFraction.num,
        initPicParam.uiBFraction.denom, scaleFactor);
    MFX_CHECK_STS_1(MFXSts)

    switch(pFrameParam->UnifiedMvMode)
    {
        case 0:
            initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_MIXED_QUARTER_BICUBIC;
            break;
        case 1:
            initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_QUARTER_BICUBIC;
            break;
        case 2:
            initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_HALF_BICUBIC;
            break;
        case 3:
            initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_HALF_BILINEAR;
            break;
        default:
            unlock_buff;
            return MFX_ERR_UNSUPPORTED;
            break;
    }
    initPicParam.uiMVRangeIndex       = pFrameParam->ExtendedMvRange;

    if (((UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD)&& (!pFrameParam->SecondFieldFlag))||
        ((UMCtype == UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD)&& ( pFrameParam->SecondFieldFlag)) ||
        (UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD))
    {
        initPicParam.uiReferenceFieldType = GetRefFieldFlag(pFrameParam->NumRefPic,pFrameParam->RefFieldPicFlag,pFrameParam->SecondFieldFlag);
    }

    initPicParam.nReferenceFrameDist  = pFrameParam->RefDistance; // is used only for fields

    initPicParam.sVLCTablesIndex.uiMVTab          = pFrameParam->MvTable;
    initPicParam.sVLCTablesIndex.uiDecTypeDCIntra = pFrameParam->TransDCTable;
    initPicParam.sVLCTablesIndex.uiDecTypeAC      = pFrameParam->TransACTable;
    initPicParam.sVLCTablesIndex.uiCBPTab         = pFrameParam->CBPTable;

    ret = m_pVC1EncoderPicture->SetInitPictureParams(&initPicParam);
    UMC_MFX_CHECK_STS_1(ret)

        if (FieldPicture)
            MFXSts = GetSavedMVField (cuc, &pSavedMV, &pDirection);
        else
            MFXSts = GetSavedMVFrame (cuc, &pSavedMV);

    MFX_CHECK_STS_1(MFXSts);

    ret = m_pVC1EncoderPicture->SetPictureParams(UMCtype, (Ipp16s*)pSavedMV, pDirection, pFrameParam->SecondFieldFlag, smoothing);
    UMC_MFX_CHECK_STS_1(ret);

    doubleQuant = pFrameParam->PQuant*2 + pFrameParam->HalfQP;
    assert(doubleQuant > 0);
    assert(doubleQuant <= 62);

    ret = m_pVC1EncoderPicture->SetPictureQuantParams(doubleQuant>>1, doubleQuant&0x1);
    UMC_MFX_CHECK_STS_1(ret);

    ret = m_pVC1EncoderPicture->CheckParameters(cLastError,pFrameParam->SecondFieldFlag);
    UMC_MFX_CHECK_STS_1(ret);

    /*------------Work with planes-----------------------*/
    MFXSts = SetFrame ( curIndex,
                        cuc->FrameSurface,
                        &m_mfxVideoParam,
                        UMCtype,
                        &Plane, FieldPicture);

    MFX_CHECK_STS_1(MFXSts);

    ret = m_pVC1EncoderPicture->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_CURR_PLANE);
    UMC_MFX_CHECK_STS_1(ret);

    MFXSts = SetFrame (raisedIndex,
        cuc->FrameSurface,
        &m_mfxVideoParam,
        UMCtype,
        &Plane, FieldPicture);

    MFX_CHECK_STS_1(MFXSts);

    ret = m_pVC1EncoderPicture->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_RAISED_PLANE);
    UMC_MFX_CHECK_STS_1(ret);

    mfxFrameData *pFr = cuc->FrameSurface->Data[pFrameParam->CurrFrameLabel];

    if (isPredicted(UMCtype))
    {
        MFX_CHECK_COND(pFrameParam->NumRefFrame>=1);
        MFX_CHECK_NULL_PTR1_1(pFrameParam->RefFrameListP);

        mfxU8  refIndexF    =  pFrameParam->RefFrameListP[0] & 0x7F;

        MFX_CHECK_COND_1(refIndexF != curIndex && refIndexF != raisedIndex);

        MFXSts = SetFrame (refIndexF,
        cuc->FrameSurface,
        &m_mfxVideoParam,
        UMCtype,
        &Plane, FieldPicture);

        MFX_CHECK_STS_1(MFXSts);

        ret = m_pVC1EncoderPicture->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_FORWARD_PLANE);
        UMC_MFX_CHECK_STS_1(ret);
    }
    else if (isBPredicted(UMCtype))
    {
        MFX_CHECK_COND(pFrameParam->NumRefFrame>=2);
        MFX_CHECK_NULL_PTR1_1(pFrameParam->RefFrameListB);
        MFX_CHECK_NULL_PTR2_1(pFrameParam->RefFrameListB[0], pFrameParam->RefFrameListB[1]);

        mfxU8  refIndexF    =  pFrameParam->RefFrameListB[0][0] & 0x7F;
        mfxU8  refIndexB    =  pFrameParam->RefFrameListB[1][0] & 0x7F;

        MFX_CHECK_COND_1(refIndexF != curIndex && refIndexF != raisedIndex &&
            refIndexB != curIndex && refIndexB != raisedIndex &&
            refIndexB != refIndexF)

        MFXSts = SetFrame ( refIndexF,
                            cuc->FrameSurface,
                            &m_mfxVideoParam,
                            UMCtype,
                            &Plane, FieldPicture);
        MFX_CHECK_STS_1(MFXSts);

        ret = m_pVC1EncoderPicture->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_FORWARD_PLANE);
        UMC_MFX_CHECK_STS_1(ret);

        MFXSts = SetFrame ( refIndexB,
                            cuc->FrameSurface,
                            &m_mfxVideoParam,
                            UMCtype,
                            &Plane, FieldPicture);

        MFX_CHECK_STS_1(MFXSts);

        ret = m_pVC1EncoderPicture->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_BACKWARD_PLANE);
        UMC_MFX_CHECK_STS_1(ret);
    }

    mfxI32 n = (cuc->NumSlice == 0)? 1:cuc->NumSlice;

    {
        mfxI32 i = cuc->SliceId;
        MFX_CHECK_COND_1(i < n);

        mfxI32 firstRow = 0, nRows = 0;

        if (cuc->NumSlice>1)
        {
            mfxSliceParam *pSlice = &cuc->SliceParam[i];
            MFX_CHECK_NULL_PTR1(pSlice)
            memcpy (&m_mfxSliceParam, pSlice,sizeof(mfxSliceParam));

            MFXSts = GetSliceRows (cuc, firstRow, nRows);
            MFX_CHECK_STS_1(MFXSts)
            MFXSts = GetFirstLastMB (cuc, MEParams.FirstMB, MEParams.LastMB);
            MFX_CHECK_STS_1(MFXSts)
        }
        else
        {
            memset(&m_mfxSliceParam,0,sizeof(mfxSliceParam));
            m_mfxSliceParam.VC1.SliceId   = 0;
            m_mfxSliceParam.VC1.SliceType = (pFrameParam->SecondFieldFlag)? pFrameParam->FrameType2nd:pFrameParam->FrameType;
        }

        switch (UMCtype)
        {
        case UMC_VC1_ENCODER::VC1_ENC_I_FRAME:
            {
                //----coding---------------
                ret = m_pVC1EncoderPicture->PAC_IFrame(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_IFrame(&BS, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_FRAME:
            if (pFrameParam->UnifiedMvMode)
            {
                ret = SetMEParam_P(cuc, &MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----coding---------------
                ret = m_pVC1EncoderPicture->PAC_PFrame(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_PFrame(&BS, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                ret = SetMEParam_P_Mixed(cuc, &MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----coding---------------
                ret = m_pVC1EncoderPicture->PAC_PFrameMixed(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_PFrameMixed(&BS, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_FRAME:
            {
                ret = SetMEParam_B(cuc, &MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----coding---------------
                ret = m_pVC1EncoderPicture->PAC_BFrame(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_BFrame(&BS, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME:
            {
                ret = m_pVC1EncoderPicture->VLC_SkipFrame(&BS);
                UMC_MFX_CHECK_STS_1(ret);
                break;
            }
        case UMC_VC1_ENCODER::VC1_ENC_I_I_FIELD:
            //----coding---------------

            ret = m_pVC1EncoderPicture->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
            UMC_MFX_CHECK_STS_1(ret);

            //----VLC---------------
            ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
            UMC_MFX_CHECK_STS_1(ret);
            break;
        case UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                ret = m_pVC1EncoderPicture->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                if (pFrameParam->UnifiedMvMode)
                {
                    ret = SetMEParam_P_Field(cuc, &MEParams, false, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACPField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_P_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);
                }
                else
                {
                    ret = SetMEParam_P_Field(cuc, &MEParams, true, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACPFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_P_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                }
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                if (pFrameParam->UnifiedMvMode)
                {
                    ret = SetMEParam_P_Field(cuc, &MEParams, false, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACPField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_P_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);
                }
                else
                {
                    ret = SetMEParam_P_Field(cuc, &MEParams, true, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACPFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_P_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                }
            }
            else
            {
                ret = m_pVC1EncoderPicture->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD:
            if (pFrameParam->UnifiedMvMode)
            {
                ret = SetMEParam_P_Field(cuc, &MEParams, false, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                ret =  m_pVC1EncoderPicture->PACPField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_P_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                ret = SetMEParam_P_Field(cuc, &MEParams, true, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                ret =  m_pVC1EncoderPicture->PACPFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_P_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_BI_B_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                ret = m_pVC1EncoderPicture->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                if (pFrameParam->UnifiedMvMode)
                {
                    ret = SetMEParam_B_Field(cuc, &MEParams, false, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACBField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_B_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);
                }
                else
                {
                    ret = SetMEParam_B_Field(cuc, &MEParams, true, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACBFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_B_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                }
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_BI_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                if(pFrameParam->UnifiedMvMode)
                {
                    ret = SetMEParam_B_Field(cuc, &MEParams, false, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACBField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_B_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);
                }
                else
                {
                    ret = SetMEParam_B_Field(cuc, &MEParams, true, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACBFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_B_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);
                }
            }
            else
            {
                ret = m_pVC1EncoderPicture->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_B_FIELD:
            if(pFrameParam->UnifiedMvMode)
            {
                ret = SetMEParam_B_Field(cuc, &MEParams, false, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----coding---------------
                ret =  m_pVC1EncoderPicture->PACBField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_B_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                ret = SetMEParam_B_Field(cuc, &MEParams, true, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                ret =  m_pVC1EncoderPicture->PACBFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_B_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }

            break;
        default:
            unlock_buff;
            return MFX_ERR_UNSUPPORTED;
            break;

        }
        if (m_UserDataSize)
        {
            ret = BS.AddUserData(m_UserDataBuffer,m_UserDataSize,0x0000011D);
            m_UserDataSize = 0;
        }
    }
    //mfxFrameData *pFrRecon = cuc->FrameSurface->Data[pFrameParam->DecFrameLabel];
    //pFrRecon->LockAndFlagsMarker ^= lockMarker;

    cuc->Bitstream->DataLength = cuc->Bitstream->DataLength + (mfxU32)BS.GetDataLen();
    cuc->Bitstream->DataOffset = cuc->Bitstream->DataLength;

    ret = BS.DataComplete(&data);
    UMC_MFX_CHECK_STS_1(ret);

      if (frameDataLocked)
      {
        for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)
        {
          MFXSts =  m_pCore->UnlockFrame(cuc->FrameSurface->Data[i]->MemId,cuc->FrameSurface->Data[i]);
          MFX_CHECK_STS_1(MFXSts);
        }
          frameDataLocked  =  false;
      }

#ifdef VC1_ENC_DEBUG_ON
    //debug
    if(!FieldPicture || pFrameParam->SecondFieldFlag)
    {
        UMC_VC1_ENCODER::pDebug->SetPicType(UMCtype);
        UMC_VC1_ENCODER::pDebug->SetFrameSize(cuc->Bitstream->DataLength);
        UMC_VC1_ENCODER::pDebug->WriteFrameInfo();
    }
#endif
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoPakVc1ADV::RunSliceBSP(mfxFrameCUC *cuc) { return MFX_ERR_UNSUPPORTED; }
mfxStatus MFXVideoPakVc1ADV::RunFramePAK(mfxFrameCUC *cuc)
{
    UMC_VC1_ENCODER::VC1EncoderBitStreamAdv BS;
    UMC::MediaData  data;
    UMC::Status ret = UMC::UMC_OK;
    UMC_VC1_ENCODER::InitPictureParams initPicParam;

    UMC_VC1_ENCODER::Frame Plane;
    mfxStatus MFXSts = MFX_ERR_NONE;
    Ipp8u scaleFactor=0;
    UMC_VC1_ENCODER::ePType UMCtype;
    mfxI16Pair* pSavedMV   = 0;
    Ipp8u*      pDirection = 0;

    Ipp8u doubleQuant = 2;
    mfxI16 st;
    PadFrame  pPadFrame= 0;
    Ipp32u    smoothing = 0;

    bool frameDataLocked  = false;

    Ipp32u i = 0;
    MFX_CHECK_NULL_PTR1_1   (cuc);

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;
    MFX_CHECK_NULL_PTR1_1(pFrameParam);
    bool      FieldPicture = pFrameParam->FieldPicFlag;



    if ((cuc->FrameSurface!= 0 && cuc->FrameSurface->Info.NumFrameData!=0))
    {
        for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)
        {
            if(cuc->FrameSurface->Data[i]->Y==NULL || cuc->FrameSurface->Data[i]->U==NULL || cuc->FrameSurface->Data[i]->V==NULL)
            {
            frameDataLocked  =  true;
             MFXSts =  m_pCore->LockFrame(cuc->FrameSurface->Data[i]->MemId, cuc->FrameSurface->Data[i]);
             MFX_CHECK_STS_1(MFXSts);
            }
        }
    }

    if (m_pVC1EncoderSequence->IsOverlap())
    {
        smoothing = ((pFrameParam->PicDeblocked>>5)&0x01)? 3:1;
    }

    MFXSts = GetUMCPictureType(pFrameParam->FrameType,pFrameParam->FrameType2nd,
                               pFrameParam->FieldPicFlag, pFrameParam->InterlacedFrameFlag,
                               pFrameParam->Pic4MvAllowed,&UMCtype);
    MFX_CHECK_STS_1(MFXSts);

    MFX_INTERNAL_CPY( &m_mfxFrameParam,pFrameParam, sizeof(mfxFrameParam));

    vm_char   cLastError[VC1_ENC_STR_LEN];
    UMC::MeParams MEParams;
    mfxU8  curIndex     =  pFrameParam->CurrFrameLabel   & 0x7F;
    mfxU8  raisedIndex  =  pFrameParam->DecFrameLabel    & 0x7F;

    mfxBitstream* bs = cuc->Bitstream;


    bool bSecondField = pFrameParam->SecondFieldFlag;
    cuc->FrameSurface->Data[curIndex]->Field = FieldPicture;

    MEParams.pSrc = m_pMeFrame;

    MFX_CHECK_COND_1(bs->MaxLength - bs->DataOffset>0)
    ret = data.SetBufferPointer(bs->Data + bs->DataOffset, bs->MaxLength - bs->DataOffset);
    UMC_MFX_CHECK_STS_1(ret)
    BS.Init(&data);

    MFXSts = GetBFractionParameters (pFrameParam->CodedBfraction,
                                initPicParam.uiBFraction.num,
                                initPicParam.uiBFraction.denom, scaleFactor);

    MFX_CHECK_STS_1(MFXSts)
    switch(pFrameParam->UnifiedMvMode)
    {
    case 0:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_MIXED_QUARTER_BICUBIC;
        break;
    case 1:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_QUARTER_BICUBIC;
        break;
    case 2:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_HALF_BICUBIC;
        break;
    case 3:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_HALF_BILINEAR;
        break;
    default:
        unlock_buff;
        return MFX_ERR_UNSUPPORTED;
        break;
    }
    initPicParam.uiMVRangeIndex       = pFrameParam->ExtendedMvRange;

    if (((UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD)&& (!pFrameParam->SecondFieldFlag))||
        ((UMCtype == UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD)&& ( pFrameParam->SecondFieldFlag)) ||
         (UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD))
    {
        initPicParam.uiReferenceFieldType = GetRefFieldFlag(pFrameParam->NumRefPic,pFrameParam->RefFieldPicFlag,pFrameParam->SecondFieldFlag);
    }

    initPicParam.nReferenceFrameDist  = pFrameParam->RefDistance; // is used only for fields

    initPicParam.sVLCTablesIndex.uiMVTab          = pFrameParam->MvTable;
    initPicParam.sVLCTablesIndex.uiDecTypeDCIntra = pFrameParam->TransDCTable;
    initPicParam.sVLCTablesIndex.uiDecTypeAC      = pFrameParam->TransACTable;
    initPicParam.sVLCTablesIndex.uiCBPTab         = pFrameParam->CBPTable;

    ret = m_pVC1EncoderPicture->SetInitPictureParams(&initPicParam);
    UMC_MFX_CHECK_STS_1(ret)

    if (FieldPicture)
        MFXSts = GetSavedMVField (cuc, &pSavedMV, &pDirection);
    else
        MFXSts = GetSavedMVFrame (cuc, &pSavedMV);

    MFX_CHECK_STS_1(MFXSts);

    ret = m_pVC1EncoderPicture->SetPictureParams(UMCtype, (Ipp16s*)pSavedMV, pDirection, pFrameParam->SecondFieldFlag,smoothing);
    UMC_MFX_CHECK_STS_1(ret);

    m_pVC1EncoderPicture->SetIntesityCompensationParameters(false,0,0,false);
    m_pVC1EncoderPicture->SetIntesityCompensationParameters(false,0,0,true);
    if (((UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD)&& (!pFrameParam->SecondFieldFlag))||
        ((UMCtype == UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD)&& ( pFrameParam->SecondFieldFlag)) ||
         (UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD))
    {


        if (pFrameParam->IntCompField&UMC_VC1_ENCODER::VC1_ENC_TOP_FIELD)
        {
            m_pVC1EncoderPicture->SetIntesityCompensationParameters(true,pFrameParam->LumaScale,
                                                                    pFrameParam->LumaShift, false);

        }
        if (pFrameParam->IntCompField & UMC_VC1_ENCODER::VC1_ENC_BOTTOM_FIELD)
        {
            m_pVC1EncoderPicture->SetIntesityCompensationParameters(true,pFrameParam->LumaScale2,
                                                                    pFrameParam->LumaShift2, true);
        }
    }
    else if (UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_FRAME)
    {
        m_pVC1EncoderPicture->SetIntesityCompensationParameters(pFrameParam->IntCompField,
                                                                pFrameParam->LumaScale,
                                                                pFrameParam->LumaShift,
                                                                false);
    }

    doubleQuant = pFrameParam->PQuant*2 + pFrameParam->HalfQP;
    assert(doubleQuant > 0);
    assert(doubleQuant <= 62);

    ret = m_pVC1EncoderPicture->SetPictureQuantParams(doubleQuant>>1, doubleQuant&0x1);
    UMC_MFX_CHECK_STS_1(ret);

    switch (pFrameParam->TransformFlags)
    {
    case 1:
        m_pVC1EncoderPicture->SetPictureTransformType (UMC_VC1_ENCODER::VC1_ENC_8x8_TRANSFORM);
        break;
    case 3:
        m_pVC1EncoderPicture->SetPictureTransformType (UMC_VC1_ENCODER::VC1_ENC_8x4_TRANSFORM);
        break;
    case 5:
        m_pVC1EncoderPicture->SetPictureTransformType (UMC_VC1_ENCODER::VC1_ENC_4x8_TRANSFORM);
        break;
    case 7:
        m_pVC1EncoderPicture->SetPictureTransformType (UMC_VC1_ENCODER::VC1_ENC_4x4_TRANSFORM);
        break;
    }

    ret = m_pVC1EncoderPicture->CheckParameters(cLastError,pFrameParam->SecondFieldFlag);
    UMC_MFX_CHECK_STS_1(ret);

    /*------------Work with planes-----------------------*/
    MFXSts = SetFrame (curIndex,
                       cuc->FrameSurface,
                       &m_mfxVideoParam,
                       UMCtype,
                       &Plane, FieldPicture);
    MFX_CHECK_STS_1(MFXSts)
    ret = m_pVC1EncoderPicture->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_CURR_PLANE);
    UMC_MFX_CHECK_STS_1(ret);

    MFXSts = SetFrame (raisedIndex,
                       cuc->FrameSurface,
                       &m_mfxVideoParam,
                       UMCtype,
                       &Plane, FieldPicture);
    MFX_CHECK_STS_1(MFXSts)
    ret = m_pVC1EncoderPicture->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_RAISED_PLANE);
    UMC_MFX_CHECK_STS_1(ret);
    mfxFrameData *pFr = cuc->FrameSurface->Data[pFrameParam->CurrFrameLabel];

    if (isPredicted(UMCtype))
    {
        MFX_CHECK_COND_1(pFrameParam->NumRefFrame>=1)
        MFX_CHECK_NULL_PTR1_1(pFrameParam->RefFrameListP)

        mfxU8  refIndexF    =  pFrameParam->RefFrameListP[0] & 0x7F;

        MFX_CHECK_COND(refIndexF != curIndex && refIndexF != raisedIndex)

        MFXSts = SetFrame (refIndexF,
                           cuc->FrameSurface,
                           &m_mfxVideoParam,
                           UMCtype,
                           &Plane, FieldPicture);
        MFX_CHECK_STS_1(MFXSts)
        ret = m_pVC1EncoderPicture->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_FORWARD_PLANE);
        UMC_MFX_CHECK_STS_1(ret);
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
         MFXSts = SetFrame (refIndexF,
                           cuc->FrameSurface,
                           &m_mfxVideoParam,
                           UMCtype,
                           &Plane, FieldPicture);
        MFX_CHECK_STS_1(MFXSts)
        ret = m_pVC1EncoderPicture->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_FORWARD_PLANE);
        UMC_MFX_CHECK_STS_1(ret);

         MFXSts = SetFrame (refIndexB,
                           cuc->FrameSurface,
                           &m_mfxVideoParam,
                           UMCtype,
                           &Plane, FieldPicture);
        MFX_CHECK_STS_1(MFXSts)
        ret = m_pVC1EncoderPicture->SetPlaneParams (&Plane, UMC_VC1_ENCODER::VC1_ENC_BACKWARD_PLANE);
        UMC_MFX_CHECK_STS_1(ret);
    }
    /*------------- Trellis quantization ------------------ */
    {
        mfxI8* pRC = 0;
        Ipp32u  h = (pFrameParam->FrameHinMbMinus1 + 1);
        Ipp32u  w = (pFrameParam->FrameWinMbMinus1 + 1);

        MEParams.UseTrellisQuantization = m_bUsedTrellisQuantization;

        GetMBRoundControl (cuc, &pRC);
        if (pRC)
        {
            if (MEParams.UseTrellisQuantization)
            {
                for (int i = 0; i < h*w; i++)
                {
                    for (int blk = 0; blk < 6; blk++)
                    {
                        MFX_INTERNAL_CPY(MEParams.pSrc->MBs[i].RoundControl[blk],pRC,sizeof(mfxI8)*64);
                        pRC += 64;
                    }
                }
            }
            else
            {
                for (int i = 0; i < h*w; i++)
                {
                    for (int blk = 0; blk < 6; blk++)
                    {
                        memset(MEParams.pSrc->MBs[i].RoundControl[blk],0,sizeof(mfxI8)*64);
                    }
                }
            }
        }
    }

    mfxI32 n = (cuc->NumSlice == 0)? 1:cuc->NumSlice;

    for (mfxI32 i = 0; i< n; i++)
    {
        cuc->SliceId = i;
        mfxI32 firstRow = 0, nRows = 0;

        if (cuc->NumSlice>1)
        {
            MFXSts = GetSliceRows (cuc, firstRow, nRows);
            MFX_CHECK_STS_1(MFXSts)
                MFXSts = GetFirstLastMB (cuc, MEParams.FirstMB, MEParams.LastMB);
            MFX_CHECK_STS_1(MFXSts)
        }

        switch (UMCtype)
        {
        case UMC_VC1_ENCODER::VC1_ENC_I_FRAME:
            {
                //----coding---------------
                ret = m_pVC1EncoderPicture->PAC_IFrame(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_IFrame(&BS, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_FRAME:
            if (pFrameParam->UnifiedMvMode)
            {
                ret = SetMEParam_P(cuc, &MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----coding---------------
                ret = m_pVC1EncoderPicture->PAC_PFrame(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_PFrame(&BS, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                ret = SetMEParam_P_Mixed(cuc, &MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----coding---------------
                ret = m_pVC1EncoderPicture->PAC_PFrameMixed(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_PFrameMixed(&BS, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_FRAME:
            {
                ret = SetMEParam_B(cuc, &MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----coding---------------
                ret = m_pVC1EncoderPicture->PAC_BFrame(&MEParams, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_BFrame(&BS, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME:
            {
                ret = m_pVC1EncoderPicture->VLC_SkipFrame(&BS);
                UMC_MFX_CHECK_STS_1(ret);
                break;
            }
        case UMC_VC1_ENCODER::VC1_ENC_I_I_FIELD:
            //----coding---------------

            ret = m_pVC1EncoderPicture->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
            UMC_MFX_CHECK_STS_1(ret);

            //----VLC---------------
            ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
            UMC_MFX_CHECK_STS_1(ret);
            break;
        case UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                ret = m_pVC1EncoderPicture->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                if (pFrameParam->UnifiedMvMode)
                {
                    ret = SetMEParam_P_Field(cuc, &MEParams,false, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACPField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_P_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);
                }
                else
                {
                    ret = SetMEParam_P_Field(cuc, &MEParams, true, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACPFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_P_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                }
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                if (pFrameParam->UnifiedMvMode)
                {
                    ret = SetMEParam_P_Field(cuc, &MEParams,false, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACPField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_P_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);
                }
                else
                {
                    ret = SetMEParam_P_Field(cuc, &MEParams,true, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACPFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_P_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                }
            }
            else
            {
                ret = m_pVC1EncoderPicture->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD:
            if (pFrameParam->UnifiedMvMode)
            {
                ret = SetMEParam_P_Field(cuc, &MEParams, false, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                ret =  m_pVC1EncoderPicture->PACPField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_P_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                ret = SetMEParam_P_Field(cuc, &MEParams, true, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                ret =  m_pVC1EncoderPicture->PACPFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_P_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_BI_B_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                ret = m_pVC1EncoderPicture->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                if (pFrameParam->UnifiedMvMode)
                {
                    ret = SetMEParam_B_Field(cuc, &MEParams, false, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACBField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_B_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);
                }
                else
                {
                    ret = SetMEParam_B_Field(cuc, &MEParams, true, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACBFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_B_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                }
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_BI_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                if(pFrameParam->UnifiedMvMode)
                {
                    ret = SetMEParam_B_Field(cuc, &MEParams, false, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACBField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_B_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);
                }
                else
                {
                    ret = SetMEParam_B_Field(cuc, &MEParams, true, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    ret =  m_pVC1EncoderPicture->PACBFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);

                    //----VLC---------------
                    ret =  m_pVC1EncoderPicture->VLC_B_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                    UMC_MFX_CHECK_STS_1(ret);
                }
            }
            else
            {
                ret = m_pVC1EncoderPicture->PACIField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_B_B_FIELD:
            if(pFrameParam->UnifiedMvMode)
            {
                ret = SetMEParam_B_Field(cuc, &MEParams, false, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----coding---------------
                ret =  m_pVC1EncoderPicture->PACBField(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_B_FieldPic(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                ret = SetMEParam_B_Field(cuc, &MEParams, true, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                ret =  m_pVC1EncoderPicture->PACBFieldMixed(&MEParams,pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_B_FieldPicMixed(&BS, pFrameParam->SecondFieldFlag, firstRow, nRows);
                UMC_MFX_CHECK_STS_1(ret);
            }

            break;
        default:
            unlock_buff;
            return MFX_ERR_UNSUPPORTED;
            break;

        }
        if (m_UserDataSize)
        {
            ret = BS.AddUserData(m_UserDataBuffer,m_UserDataSize,0x0000011D);
            m_UserDataSize = 0;
        }
    }
    //mfxFrameData *pFrRecon = cuc->FrameSurface->Data[pFrameParam->DecFrameLabel];
    //pFrRecon->LockAndFlagsMarker ^= lockMarker;

    cuc->Bitstream->DataLength = cuc->Bitstream->DataLength + (mfxU32)BS.GetDataLen();
    cuc->Bitstream->DataOffset = cuc->Bitstream->DataLength;

    ret = BS.DataComplete(&data);
    UMC_MFX_CHECK_STS_1(ret);

      if (frameDataLocked)
      {
        for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)
        {
          MFXSts =  m_pCore->UnlockFrame(cuc->FrameSurface->Data[i]->MemId,cuc->FrameSurface->Data[i]);
          MFX_CHECK_STS_1(MFXSts);
        }
          frameDataLocked  =  false;
      }

#ifdef VC1_ENC_DEBUG_ON
        //debug
     if(!FieldPicture || pFrameParam->SecondFieldFlag)
     {
         UMC_VC1_ENCODER::pDebug->SetPicType(UMCtype);
         UMC_VC1_ENCODER::pDebug->SetFrameSize(cuc->Bitstream->DataLength);
         UMC_VC1_ENCODER::pDebug->WriteFrameInfo();
     }
#endif
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPakVc1ADV::RunFrameBSP(mfxFrameCUC *cuc)
{
    UMC_VC1_ENCODER::VC1EncoderBitStreamAdv BS;
    UMC::MediaData  data;
    UMC::Status ret = UMC::UMC_OK;
    UMC_VC1_ENCODER::InitPictureParams initPicParam;
    UMC_VC1_ENCODER::VLCTablesIndex    VLCIndex;

    return MFX_ERR_UNSUPPORTED;
    UMC_VC1_ENCODER::Frame Plane;
    mfxStatus MFXSts = MFX_ERR_NONE;
    Ipp8u scaleFactor=0;
    UMC_VC1_ENCODER::ePType UMCtype;

    Ipp16s* pSavedMV   = 0;
    Ipp8u*  pDirection = 0;

    Ipp8u doubleQuant = 2;
    mfxI16 st;
    PadFrame  pPadFrame= 0;
    //mfxU8     lockMarker = 0;

    bool frameDataLocked  = false;

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
             MFXSts =  m_pCore->LockFrame(cuc->FrameSurface->Data[i]->MemId, cuc->FrameSurface->Data[i]);
             MFX_CHECK_STS_1(MFXSts);
            }
        }
    }

    mfxU16    step = cuc->FrameSurface->Data[0]->Pitch;

    mfxU32    shiftL  = GetLumaShift   (&cuc->FrameSurface->Info, cuc->FrameSurface->Data[0]);
    mfxU32    shiftC  = GetChromaShift (&cuc->FrameSurface->Info, cuc->FrameSurface->Data[0]);

    bool      FieldPicture = !(cuc->FrameSurface->Info.PicStruct& MFX_PICSTRUCT_PROGRESSIVE);

    MFXSts = GetUMCPictureType(pFrameParam->FrameType,pFrameParam->FrameType2nd,
                               pFrameParam->FieldPicFlag, pFrameParam->InterlacedFrameFlag,
                               pFrameParam->Pic4MvAllowed,&UMCtype);
    MFX_CHECK_STS_1(MFXSts);

    MFX_INTERNAL_CPY( &m_mfxFrameParam,pFrameParam, sizeof(mfxFrameParam));


    vm_char   cLastError[VC1_ENC_STR_LEN];

    mfxU8  curIndex     =  pFrameParam->CurrFrameLabel   & 0x7F;
    mfxU8  raisedIndex  =  pFrameParam->DecFrameLabel    & 0x7F;

    mfxBitstream* bs = cuc->Bitstream;

    bool bField = isField(UMCtype);
    bool bSecondField = pFrameParam->SecondFieldFlag;
    Ipp32u    smoothing = 0;

    if (m_pVC1EncoderSequence->IsOverlap())
    {
        smoothing = ((pFrameParam->PicDeblocked>>5)&0x01)? 3:1;
    }

    MFX_CHECK_COND_1(bs->MaxLength - bs->DataOffset>0)
    ret = data.SetBufferPointer(bs->Data + bs->DataOffset, bs->MaxLength - bs->DataOffset);
    UMC_MFX_CHECK_STS_1(ret)

    BS.Init(&data);
    //UMC_MFX_CHECK_STS(ret)

    MFXSts = GetBFractionParameters (pFrameParam->CodedBfraction,
                                initPicParam.uiBFraction.num,
                                initPicParam.uiBFraction.denom, scaleFactor);

    MFX_CHECK_STS_1(MFXSts)
    switch(pFrameParam->UnifiedMvMode)
    {
    case 0:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_MIXED_QUARTER_BICUBIC;
        break;
    case 1:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_QUARTER_BICUBIC;
        break;
    case 2:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_HALF_BICUBIC;
        break;
    case 3:
        initPicParam.uiMVMode =  UMC_VC1_ENCODER::VC1_ENC_1MV_HALF_BILINEAR;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }
    initPicParam.uiMVRangeIndex       = pFrameParam->ExtendedMvRange;
    initPicParam.uiReferenceFieldType = UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_FIRST;

    if (((UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD)&& (!pFrameParam->SecondFieldFlag))||
        ((UMCtype == UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD)&& ( pFrameParam->SecondFieldFlag)) ||
         (UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD))
    {
        initPicParam.uiReferenceFieldType = GetRefFieldFlag(pFrameParam->NumRefPic,pFrameParam->RefFieldPicFlag,pFrameParam->SecondFieldFlag);
    }

    initPicParam.nReferenceFrameDist  = pFrameParam->RefDistance; // is used only for fields
    initPicParam.sVLCTablesIndex.uiMVTab          = pFrameParam->MvTable;
    initPicParam.sVLCTablesIndex.uiDecTypeDCIntra = pFrameParam->TransDCTable;
    initPicParam.sVLCTablesIndex.uiDecTypeAC      = pFrameParam->TransACTable;
    initPicParam.sVLCTablesIndex.uiCBPTab         = pFrameParam->CBPTable;


    ret = m_pVC1EncoderPicture->SetInitPictureParams(&initPicParam);
    UMC_MFX_CHECK_STS_1(ret)



    ret = m_pVC1EncoderPicture->SetPictureParams(UMCtype, pSavedMV, pDirection, pFrameParam->SecondFieldFlag,smoothing);
    UMC_MFX_CHECK_STS_1(ret);

    doubleQuant = pFrameParam->PQuant*2 + pFrameParam->HalfQP;
    assert(doubleQuant > 0);
    assert(doubleQuant <= 62);

    ret = m_pVC1EncoderPicture->SetPictureQuantParams(doubleQuant>>1, doubleQuant&0x1);
    UMC_MFX_CHECK_STS_1(ret);

    ret = m_pVC1EncoderPicture->CheckParameters(cLastError,pFrameParam->SecondFieldFlag);
    UMC_MFX_CHECK_STS_1(ret);

    /*----------------------------------------------------*/
    switch (UMCtype)
    {
    case UMC_VC1_ENCODER::VC1_ENC_I_FRAME:
        {
            //----coding---------------
            ret = GetResiduals_IFrame(cuc, m_pVC1EncoderCodedMB);
            UMC_MFX_CHECK_STS_1(ret);

            //----VLC---------------
            ret = m_pVC1EncoderPicture->VLC_IFrame(&BS);
            UMC_MFX_CHECK_STS_1(ret);
        }
        break;
    case UMC_VC1_ENCODER::VC1_ENC_P_FRAME:
        if (pFrameParam->UnifiedMvMode)
        {
            ret = GetResiduals_PFrame(cuc, m_pVC1EncoderCodedMB);
            UMC_MFX_CHECK_STS_1(ret);

            //----VLC---------------
            ret = m_pVC1EncoderPicture->VLC_PFrame(&BS);
            UMC_MFX_CHECK_STS_1(ret);
        }
        else
        {
            ret = GetResiduals_PMixedFrame(cuc, m_pVC1EncoderCodedMB);
            UMC_MFX_CHECK_STS_1(ret);

            //----VLC---------------
            ret = m_pVC1EncoderPicture->VLC_PFrameMixed(&BS);
            UMC_MFX_CHECK_STS_1(ret);
        }
        break;
    case UMC_VC1_ENCODER::VC1_ENC_B_FRAME:
        {
            ret = GetResiduals_BFrame(cuc, m_pVC1EncoderCodedMB);
            UMC_MFX_CHECK_STS_1(ret);

            //----VLC---------------
            ret = m_pVC1EncoderPicture->VLC_BFrame(&BS);
            UMC_MFX_CHECK_STS_1(ret);
        }
        break;
    case UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME:
        {
            ret = m_pVC1EncoderPicture->VLC_SkipFrame(&BS);
            UMC_MFX_CHECK_STS_1(ret);
            break;
        }
    case UMC_VC1_ENCODER::VC1_ENC_I_I_FIELD:
            ret = GetResiduals_IField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
            UMC_MFX_CHECK_STS_1(ret);

            //----VLC---------------
            ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag);
            UMC_MFX_CHECK_STS_1(ret);
        break;
    case UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                ret = GetResiduals_IField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                ret = GetResiduals_PField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_P_FieldPic(&BS, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);
            }
        break;
    case UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                ret = GetResiduals_PField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_P_FieldPic(&BS, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                ret = GetResiduals_IField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);
            }
        break;
    case UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD:
                ret = GetResiduals_PField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_P_FieldPic(&BS, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                break;
    case UMC_VC1_ENCODER::VC1_ENC_BI_B_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                ret = GetResiduals_IField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                ret = GetResiduals_BField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_B_FieldPic(&BS, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);
            }
        break;
    case UMC_VC1_ENCODER::VC1_ENC_B_BI_FIELD:
            //----coding---------------
            if(!bSecondField)
            {
                ret = GetResiduals_BField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_B_FieldPic(&BS, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);
            }
            else
            {
                ret = GetResiduals_IField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret = m_pVC1EncoderPicture->VLC_I_FieldPic(&BS, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);
            }
        break;
    case UMC_VC1_ENCODER::VC1_ENC_B_B_FIELD:
                ret = GetResiduals_BField(cuc, m_pVC1EncoderCodedMB, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                //----VLC---------------
                ret =  m_pVC1EncoderPicture->VLC_B_FieldPic(&BS, pFrameParam->SecondFieldFlag);
                UMC_MFX_CHECK_STS_1(ret);

                break;
    default:
        return MFX_ERR_UNSUPPORTED;

        }

    //mfxFrameData *pFrRecon = cuc->FrameSurface->Data[pFrameParam->DecFrameLabel];
    //pFrRecon->LockAndFlagsMarker ^= lockMarker;

    cuc->Bitstream->DataLength = cuc->Bitstream->DataLength + (mfxU32)BS.GetDataLen();
    cuc->Bitstream->DataOffset = cuc->Bitstream->DataLength;

    //MFXSts = m_pRC->FramePakRecord(cuc->Bitstream->DataLength);
    //MFX_CHECK_STS(MFXSts)

      if (frameDataLocked)
      {
        for (i = 0; i < cuc->FrameSurface->Info.NumFrameData; i++)
        {
          MFXSts =  m_pCore->UnlockFrame(cuc->FrameSurface->Data[i]->MemId,cuc->FrameSurface->Data[i]);
          MFX_CHECK_STS_1(MFXSts);
        }
          frameDataLocked  =  false;
      }

#ifdef VC1_ENC_DEBUG_ON
        //debug
     if(!bField || pFrameParam->SecondFieldFlag)
     {
         UMC_VC1_ENCODER::pDebug->SetPicType(UMCtype);
         UMC_VC1_ENCODER::pDebug->SetFrameSize(cuc->Bitstream->DataLength);
         UMC_VC1_ENCODER::pDebug->WriteFrameInfo();
     }
#endif

    return MFX_ERR_NONE;
}

#undef UMC_MFX_CHECK_STS_1
#undef UMC_MFX_CHECK_WARN_1
#undef MFX_CHECK_STS_1
#undef MFX_CHECK_COND_1
#undef MFX_CHECK_NULL_PTR1_1
#undef MFX_CHECK_NULL_PTR2_1
#undef unlock_buff

mfxStatus MFXVideoPakVc1ADV::GetResiduals_BField(mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB,
                                                 mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus MFXSts = MFX_ERR_NONE;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;

    mfxU16   w = pFrameParam->FrameWinMbMinus1 + 1;
    mfxU16   h = pFrameParam->FrameHinMbMinus1 + 1;

    UMC_VC1_ENCODER::sCoordinate dMV = {0};

    n = GetExBufferIndex(cuc,  MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB;
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    UMC_VC1_ENCODER::eTransformType tsType[6];

    pCompressedMB = &m_pVC1EncoderCodedMB[w*h*pFrameParam->SecondFieldFlag];
    pMfxCurrMB = &cuc->MbParam->Mb[w*h*pFrameParam->SecondFieldFlag].VC1;

#ifdef VC1_ENC_DEBUG_ON
    UMC_VC1_ENCODER::pDebug->SetCurrMBFirst(pFrameParam->SecondFieldFlag);
#endif

    for (i = 0; i< h; i++)
    {
        for (j = 0; j< w; j++)
        {
            MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)
            TempBlock.InitBlocks(pMFXRes);

            switch (pMfxCurrMB->MbType)
            {
              case MFX_MBTYPE_INTRA_VC1:
                  pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_INTRA);
                  for(blk = 0; blk < 6; blk++)
                        pCompressedMB->SetIntraBlock(blk);

                  pCompressedMB->SetACPrediction(pMfxCurrMB->AcPredFlag);
                  break;
              case MFX_MBTYPE_INTER_16X16_0:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_F);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[0][0];
                  dMV.y = pMfxCurrMB->MV[0][1];
                  dMV.bSecond = pMfxCurrMB->RefPicSelect[0][0];

                  pCompressedMB->SetdMV_F(dMV);

                  break;
              case MFX_MBTYPE_INTER_16X16_1:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_B);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[4][0];
                  dMV.y = pMfxCurrMB->MV[4][1];
                  dMV.bSecond = pMfxCurrMB->RefPicSelect[1][0];

                  pCompressedMB->SetdMV_F(dMV, false);

                  break;
              case MFX_MBTYPE_INTER_16X16_2:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_FB);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_FB);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[0][0];
                  dMV.y = pMfxCurrMB->MV[0][1];
                  dMV.bSecond = pMfxCurrMB->RefPicSelect[0][0];

                  pCompressedMB->SetdMV_F(dMV, true);

                  dMV.x = pMfxCurrMB->MV[4][0];
                  dMV.y = pMfxCurrMB->MV[4][1];
                  dMV.bSecond = pMfxCurrMB->RefPicSelect[1][0];

                  pCompressedMB->SetdMV_F(dMV, false);

                  break;
              case MFX_MBTYPE_INTER_16X16_DIR:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_DIRECT);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_DIRECT);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[0][0];
                  dMV.y = pMfxCurrMB->MV[0][1];
                  dMV.bSecond = pMfxCurrMB->RefPicSelect[1][0];

                  pCompressedMB->SetdMV_F(dMV, true);
                  break;
                default:
                  assert(0);
                  return MFX_ERR_UNSUPPORTED;
            }

            GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU, pMfxCurrMB->SubMbShapeV, tsType);
            pCompressedMB->SetTSType(tsType);

            pCompressedMB->SetMBCBPCY(GetUMC_CBPCY(pMfxCurrMB->CodedPattern4x4Y, pMfxCurrMB->CodedPattern4x4U,
                pMfxCurrMB->CodedPattern4x4V));

            pCompressedMB->SetMBIntraPattern(pMfxCurrMB->IntraMbFlag);

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->SaveResidual(TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }


            pMFXRes += 6*64;

            pCompressedMB++;
            pMfxCurrMB++;

#ifdef VC1_ENC_DEBUG_ON
            UMC_VC1_ENCODER::pDebug->NextMB();
#endif
        }
    }

    return MFXSts;
}
mfxStatus MFXVideoPakVc1ADV::GetResiduals_PField(mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB,
                                                 mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus MFXSts = MFX_ERR_NONE;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU16   w = pFrameParam->FrameWinMbMinus1 + 1;
    mfxU16   h = pFrameParam->FrameHinMbMinus1 + 1;

    UMC_VC1_ENCODER::sCoordinate dMV = {0};

    n = GetExBufferIndex(cuc, MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB;
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    UMC_VC1_ENCODER::eTransformType tsType[6];

    pCompressedMB = &m_pVC1EncoderCodedMB[w*h*pFrameParam->SecondFieldFlag];
    pMfxCurrMB = &cuc->MbParam->Mb[w*h*pFrameParam->SecondFieldFlag].VC1;

#ifdef VC1_ENC_DEBUG_ON
    UMC_VC1_ENCODER::pDebug->SetCurrMBFirst(pFrameParam->SecondFieldFlag);
#endif

    for (i = 0; i< h; i++)
    {
        for (j = 0; j< w; j++)
        {
            MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)
            TempBlock.InitBlocks(pMFXRes);

            switch (pMfxCurrMB->MbType)
            {
              case MFX_MBTYPE_INTRA_VC1:
                  pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA);

                  for(blk = 0; blk < 6; blk++)
                        pCompressedMB->SetIntraBlock(blk);

                  pCompressedMB->SetACPrediction(pMfxCurrMB->AcPredFlag);
                  break;
              case MFX_MBTYPE_INTER_16X16_0:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_1MV);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_1MV);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[0][0];
                  dMV.y = pMfxCurrMB->MV[0][1];
                  dMV.bSecond = pMfxCurrMB->RefPicSelect[0][0];

                  pCompressedMB->SetdMV_F(dMV);

                  break;
              default:
                  assert(0);
                  return MFX_ERR_UNSUPPORTED;
            }

            GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU, pMfxCurrMB->SubMbShapeV, tsType);
            pCompressedMB->SetTSType(tsType);

            pCompressedMB->SetMBCBPCY(GetUMC_CBPCY(pMfxCurrMB->CodedPattern4x4Y, pMfxCurrMB->CodedPattern4x4U,
                pMfxCurrMB->CodedPattern4x4V));

            pCompressedMB->SetMBIntraPattern(pMfxCurrMB->IntraMbFlag);

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->SaveResidual(TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }


            pMFXRes += 6*64;

            pCompressedMB++;
            pMfxCurrMB++;

#ifdef VC1_ENC_DEBUG_ON
            UMC_VC1_ENCODER::pDebug->NextMB();
#endif

        }
    }

    return MFXSts;
}

mfxStatus MFXVideoPakVc1ADV::GetResiduals_BFrame( mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus MFXSts = MFX_ERR_NONE;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;


    mfxU16   w = pFrameParam->FrameWinMbMinus1 + 1;
    mfxU16   h = pFrameParam->FrameHinMbMinus1 + 1;

    UMC_VC1_ENCODER::sCoordinate dMV = {0};

    n = GetExBufferIndex(cuc, MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB;
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    UMC_VC1_ENCODER::eTransformType tsType[6];

#ifdef VC1_ENC_DEBUG_ON
    UMC_VC1_ENCODER::pDebug->SetCurrMBFirst(0);
#endif

    for (i = 0; i< h; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pMfxCurrMB = &cuc->MbParam->Mb[j + i*w].VC1;

            MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)
            TempBlock.InitBlocks(pMFXRes);

            switch (pMfxCurrMB->MbType)
            {
              case MFX_MBTYPE_INTRA_VC1:
                  pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_INTRA);

                  for(blk = 0; blk < 6; blk++)
                        pCompressedMB->SetIntraBlock(blk);

                  pCompressedMB->SetACPrediction(pMfxCurrMB->AcPredFlag);
                  break;
              case MFX_MBTYPE_INTER_16X16_0:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_F);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_F);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[0][0];
                  dMV.y = pMfxCurrMB->MV[0][1];
                  dMV.bSecond = 0;

                  pCompressedMB->SetdMV(dMV);

                  break;
              case MFX_MBTYPE_INTER_16X16_1:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_B);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_B);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[4][0];
                  dMV.y = pMfxCurrMB->MV[4][1];
                  dMV.bSecond = 0;

                  pCompressedMB->SetdMV(dMV, false);

                  break;
              case MFX_MBTYPE_INTER_16X16_2:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_FB);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_FB);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[0][0];
                  dMV.y = pMfxCurrMB->MV[0][1];

                  pCompressedMB->SetdMV(dMV, true);

                  dMV.x = pMfxCurrMB->MV[4][0];
                  dMV.y = pMfxCurrMB->MV[4][1];

                  pCompressedMB->SetdMV(dMV, false);

                  break;
              case MFX_MBTYPE_INTER_16X16_DIR:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_DIRECT);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_B_MB_SKIP_DIRECT);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[0][0];
                  dMV.y = pMfxCurrMB->MV[0][1];

                  pCompressedMB->SetdMV(dMV, true);

                  dMV.x = pMfxCurrMB->MV[4][0];
                  dMV.y = pMfxCurrMB->MV[4][1];

                  pCompressedMB->SetdMV(dMV, false);

                  break;
                default:
                  assert(0);
                  return MFX_ERR_UNSUPPORTED;
            }

            GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU, pMfxCurrMB->SubMbShapeV, tsType);
            pCompressedMB->SetTSType(tsType);

            pCompressedMB->SetMBCBPCY(GetUMC_CBPCY(pMfxCurrMB->CodedPattern4x4Y, pMfxCurrMB->CodedPattern4x4U,
                pMfxCurrMB->CodedPattern4x4V));

            pCompressedMB->SetMBIntraPattern(pMfxCurrMB->IntraMbFlag);

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->SaveResidual(TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }


            pMFXRes += 6*64;
#ifdef VC1_ENC_DEBUG_ON
            UMC_VC1_ENCODER::pDebug->NextMB();
#endif
        }
    }

    return MFXSts;
}

mfxStatus MFXVideoPakVc1ADV::GetResiduals_PMixedFrame(mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus MFXSts = MFX_ERR_NONE;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU16   w = pFrameParam->FrameWinMbMinus1 + 1;
    mfxU16   h = pFrameParam->FrameHinMbMinus1 + 1;

    UMC_VC1_ENCODER::sCoordinate dMV = {0};

    n = GetExBufferIndex(cuc, MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB;
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    UMC_VC1_ENCODER::eTransformType tsType[6];

#ifdef VC1_ENC_DEBUG_ON
    UMC_VC1_ENCODER::pDebug->SetCurrMBFirst(0);
#endif

    for (i = 0; i< h; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pMfxCurrMB = &cuc->MbParam->Mb[j + i*w].VC1;

            MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)
            TempBlock.InitBlocks(pMFXRes);

            pCompressedMB->SetMBIntraPattern(pMfxCurrMB->SubMbPredMode);

            switch (pMfxCurrMB->MbType)
            {
              case MFX_MBTYPE_INTRA_VC1:
                  pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA);

                  for(blk = 0; blk < 6; blk++)
                        pCompressedMB->SetIntraBlock(blk);

                  pCompressedMB->SetACPrediction(pMfxCurrMB->AcPredFlag);
                  break;
              case MFX_MBTYPE_INTER_16X16_0:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_1MV);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_1MV);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[0][0];
                  dMV.y = pMfxCurrMB->MV[0][1];

                  pCompressedMB->SetdMV(dMV);

                  break;
              case MFX_MBTYPE_INTER_8X8_0:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_4MV);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_4MV);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);

                  for(blk = 0; blk < 4; blk++)
                  {
                      dMV.x = pMfxCurrMB->MV[blk][0];
                      dMV.y = pMfxCurrMB->MV[blk][1];
                      dMV.bSecond = 0;

                      pCompressedMB->SetBlockdMV(dMV, blk);
                  }

                  break;

              case MFX_MBTYPE_INTER_MIX_INTRA:
                  pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_4MV);

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);

                  for(blk = 0; blk < 4; blk++)
                  {
                      dMV.x = pMfxCurrMB->MV[blk][0];
                      dMV.y = pMfxCurrMB->MV[blk][1];
                      dMV.bSecond = 0;

                      pCompressedMB->SetBlockdMV(dMV, blk);
                  }

                break;
              default:
                  assert(0);
                  return MFX_ERR_UNSUPPORTED;
            }

            GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU, pMfxCurrMB->SubMbShapeV, tsType);
            pCompressedMB->SetTSType(tsType);

            pCompressedMB->SetMBCBPCY(GetUMC_CBPCY(pMfxCurrMB->CodedPattern4x4Y, pMfxCurrMB->CodedPattern4x4U,
                pMfxCurrMB->CodedPattern4x4V));


            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->SaveResidual(TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }


            pMFXRes += 6*64;
#ifdef VC1_ENC_DEBUG_ON
            UMC_VC1_ENCODER::pDebug->NextMB();
#endif
        }
    }

    return MFXSts;
}

mfxStatus MFXVideoPakVc1ADV::GetResiduals_PFrame(mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus MFXSts = MFX_ERR_NONE;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU16   w = pFrameParam->FrameWinMbMinus1 + 1;
    mfxU16   h = pFrameParam->FrameHinMbMinus1 + 1;

    UMC_VC1_ENCODER::sCoordinate dMV = {0};

    n = GetExBufferIndex(cuc,MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB;
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    UMC_VC1_ENCODER::eTransformType tsType[6];

#ifdef VC1_ENC_DEBUG_ON
    UMC_VC1_ENCODER::pDebug->SetCurrMBFirst(0);
#endif

    for (i = 0; i< h; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pMfxCurrMB = &cuc->MbParam->Mb[j + i*w].VC1;

            MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)
            TempBlock.InitBlocks(pMFXRes);

            switch (pMfxCurrMB->MbType)
            {
              case MFX_MBTYPE_INTRA_VC1:
                  pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_INTRA);

                  for(blk = 0; blk < 6; blk++)
                        pCompressedMB->SetIntraBlock(blk);

                  pCompressedMB->SetACPrediction(pMfxCurrMB->AcPredFlag);
                  break;
              case MFX_MBTYPE_INTER_16X16_0:
                  if(!pMfxCurrMB->Skip8x8Flag)
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_1MV);
                  }
                  else
                  {
                      pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_P_MB_SKIP_1MV);
                  }

                  GetUMCHybrid(pMfxCurrMB->HybridMvFlags, pCompressedMB);
                  dMV.x = pMfxCurrMB->MV[0][0];
                  dMV.y = pMfxCurrMB->MV[0][1];

                  pCompressedMB->SetdMV(dMV);

                  break;
              default:
                  assert(0);
                  return MFX_ERR_UNSUPPORTED;
            }

            GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU, pMfxCurrMB->SubMbShapeV, tsType);
            pCompressedMB->SetTSType(tsType);

            pCompressedMB->SetMBCBPCY(GetUMC_CBPCY(pMfxCurrMB->CodedPattern4x4Y, pMfxCurrMB->CodedPattern4x4U,
                pMfxCurrMB->CodedPattern4x4V));

            pCompressedMB->SetMBIntraPattern(pMfxCurrMB->IntraMbFlag);

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->SaveResidual(TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
            }


            pMFXRes += 6*64;

#ifdef VC1_ENC_DEBUG_ON
            UMC_VC1_ENCODER::pDebug->NextMB();
#endif
        }
    }

    return MFXSts;
}

mfxStatus MFXVideoPakVc1ADV::GetResiduals_IField(mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus MFXSts = MFX_ERR_NONE;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;


    mfxU16   w = pFrameParam->FrameWinMbMinus1 + 1;
    mfxU16   h = pFrameParam->FrameHinMbMinus1 + 1;

    n = GetExBufferIndex(cuc, MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB;
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    UMC_VC1_ENCODER::eTransformType tsType[6];

    pCompressedMB = &m_pVC1EncoderCodedMB[w*h*pFrameParam->SecondFieldFlag];
    pMfxCurrMB = &cuc->MbParam->Mb[w*h*pFrameParam->SecondFieldFlag].VC1;

#ifdef VC1_ENC_DEBUG_ON
    UMC_VC1_ENCODER::pDebug->SetCurrMBFirst(pFrameParam->SecondFieldFlag);
#endif

    for (i = 0; i< h; i++)
    {
        for (j = 0; j< w; j++)
        {
            MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

            TempBlock.InitBlocks(pMFXRes);

            pCompressedMB->Init(UMC_VC1_ENCODER::VC1_ENC_I_MB);

            GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU, pMfxCurrMB->SubMbShapeV, tsType);
            pCompressedMB->SetTSType(tsType);

            pCompressedMB->SetMBCBPCY(GetUMC_CBPCY(pMfxCurrMB->CodedPattern4x4Y, pMfxCurrMB->CodedPattern4x4U,
                pMfxCurrMB->CodedPattern4x4V));

            pCompressedMB->SetMBIntraPattern(pMfxCurrMB->IntraMbFlag);

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->SaveResidual(TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
                pCompressedMB->SetIntraBlock(blk);
            }

            pCompressedMB->SetACPrediction(pMfxCurrMB->AcPredFlag);

            pMFXRes += 6*64;
            pCompressedMB++;
            pMfxCurrMB++;

#ifdef VC1_ENC_DEBUG_ON
            UMC_VC1_ENCODER::pDebug->NextMB();
#endif
        }
    }

    return MFXSts;
}

mfxStatus MFXVideoPakVc1ADV::GetResiduals_IFrame(mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB,
                                                 mfxI32 firstRow, mfxI32 nRows)
{
    mfxStatus MFXSts = MFX_ERR_NONE;

    mfxI32       i = 0, j = 0, blk = 0;
    mfxI16       n = 0;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU16   w = pFrameParam->FrameWinMbMinus1 + 1;
    mfxU16   h = pFrameParam->FrameHinMbMinus1 + 1;

    n = GetExBufferIndex(cuc,MFX_CUC_VC1_RESIDAUALBUF);
    MFX_CHECK_COND (n>=0)

    //residuals
    ExtVC1Residual* pMFXResiduals = (ExtVC1Residual*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

    mfxI16* pMFXRes = pMFXResiduals->pRes;

    UMC_VC1_ENCODER::VC1EncoderMBData    TempBlock;
    UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB = m_pVC1EncoderCodedMB;
    mfxMbCodeVC1*                        pMfxCurrMB = &cuc->MbParam->Mb[0].VC1;

    UMC_VC1_ENCODER::eTransformType tsType[6];

#ifdef VC1_ENC_DEBUG_ON
    UMC_VC1_ENCODER::pDebug->SetCurrMBFirst(0);
#endif

    for (i = 0; i< h; i++)
    {
        for (j = 0; j< w; j++)
        {
            pCompressedMB = &m_pVC1EncoderCodedMB[w*i+j];
            pCompressedMB ->Init(UMC_VC1_ENCODER::VC1_ENC_I_MB);

            pMfxCurrMB = &cuc->MbParam->Mb[j + i*w].VC1;

            MFX_CHECK_COND (pMFXResiduals->numMbs >= h*w)

            TempBlock.InitBlocks(pMFXRes);

            GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU, pMfxCurrMB->SubMbShapeV, tsType);
            pCompressedMB->SetTSType(tsType);

            pCompressedMB->SetMBCBPCY(GetUMC_CBPCY(pMfxCurrMB->CodedPattern4x4Y, pMfxCurrMB->CodedPattern4x4U,
                pMfxCurrMB->CodedPattern4x4V));

            pCompressedMB->SetMBIntraPattern(pMfxCurrMB->IntraMbFlag);

            for(blk = 0; blk < 6; blk++)
            {
                pCompressedMB->SaveResidual(TempBlock.m_pBlock[blk], TempBlock.m_uiBlockStep[blk],
                    UMC_VC1_ENCODER::ZagTables_NoScan[0], blk);
                pCompressedMB->SetIntraBlock(blk);
            }

            pCompressedMB->SetACPrediction(pMfxCurrMB->AcPredFlag);

            pMFXRes += 6*64;

#ifdef VC1_ENC_DEBUG_ON
            UMC_VC1_ENCODER::pDebug->NextMB();
#endif
        }
    }

    return MFXSts;
}





mfxStatus MFXVideoPakVc1ADV::SetMEParam_P( mfxFrameCUC *cuc, UMC::MeParams* pMEParams, mfxI32 firstRow, mfxI32 nRows)
{
    Ipp32u i = 0, j = 0;

    mfxU16 w = m_mfxVideoParam.mfx.FrameInfo.Width;
    mfxU16 h = m_mfxVideoParam.mfx.FrameInfo.Height;

    GetRealFrameSize(&m_mfxVideoParam.mfx, w, h);

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;


    Ipp32u   wMB = pFrameParam->FrameWinMbMinus1 + 1;
    Ipp32u   hMB = pFrameParam->FrameHinMbMinus1 + 1;

    Ipp32u hRow = (nRows > 0 && (firstRow + nRows) < h)? firstRow + nRows : hMB;

    MFX_CHECK_COND(hMB*wMB <= cuc->MbParam->NumMb)

    if (m_bUsePadding)
    {
        pMEParams->PicRange.top_left.x       = -14;
        pMEParams->PicRange.top_left.y       = -14;
        pMEParams->PicRange.bottom_right.x   = w + 13;
        pMEParams->PicRange.bottom_right.y   = h + 13;
    }
    else
    {
        pMEParams->PicRange.top_left.x       = 1;
        pMEParams->PicRange.top_left.y       = 1;
        pMEParams->PicRange.bottom_right.x   = w - 3;
        pMEParams->PicRange.bottom_right.y   = h - 3;
    }

    pMEParams->SearchRange.x             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    pMEParams->SearchRange.y             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1] - 1;

    for (i = firstRow; i < hRow ; i++)
    {
        for (j = 0; j< wMB; j++)
        {
          mfxMbCodeVC1 *pMfxCurrMB = &cuc->MbParam->Mb[j + i*wMB].VC1;
          UMC::MeMB   *pMECurrMB  = &pMEParams->pSrc->MBs[j + i*wMB];

          switch (pMfxCurrMB->MbType)
          {
            case MFX_MBTYPE_INTRA_VC1:
                pMECurrMB->MbType     = UMC::ME_MbIntra;
                pMECurrMB->BlockTrans = 0;
                break;
            case MFX_MBTYPE_INTER_16X16_0:
                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType     = UMC::ME_MbFrw;
                    pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
                    pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];

                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType     = UMC::ME_MbFrwSkipped;
                    pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
                    pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];
                }
                break;
            default:
                return MFX_ERR_UNSUPPORTED;
          }
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPakVc1ADV::SetMEParam_P_Mixed( mfxFrameCUC *cuc, UMC::MeParams* pMEParams, mfxI32 firstRow, mfxI32 nRows)
{
    Ipp32u i = 0,j = 0;

    mfxU16 w = m_mfxVideoParam.mfx.FrameInfo.Width;
    mfxU16 h = m_mfxVideoParam.mfx.FrameInfo.Height;

    GetRealFrameSize(&m_mfxVideoParam.mfx, w, h);

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    Ipp32u   wMB = pFrameParam->FrameWinMbMinus1 + 1;
    Ipp32u   hMB = pFrameParam->FrameHinMbMinus1 + 1;
    Ipp32u   hRow = (nRows > 0 && (firstRow + nRows) < hMB)? firstRow + nRows : hMB;


    MFX_CHECK_COND(hMB*wMB <= cuc->MbParam->NumMb)

    if (m_bUsePadding)
    {
        pMEParams->PicRange.top_left.x       = -7;
        pMEParams->PicRange.top_left.y       = -7;
        pMEParams->PicRange.bottom_right.x   = w  + 7;
        pMEParams->PicRange.bottom_right.y   = h  + 7;
    }
    else
    {
        pMEParams->PicRange.top_left.x       = 0;
        pMEParams->PicRange.top_left.y       = 0;
        pMEParams->PicRange.bottom_right.x   = w-1;
        pMEParams->PicRange.bottom_right.y   = h-1;
    }

    pMEParams->SearchRange.x             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    pMEParams->SearchRange.y             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1] - 1;

    for (i = firstRow; i< hRow; i++)
    {
        for (j = 0; j< wMB; j++)
        {
          mfxMbCodeVC1      *pMfxCurrMB = &cuc->MbParam->Mb[j + i*wMB].VC1;
          UMC::MeMB         *pMECurrMB  = &pMEParams->pSrc->MBs[j + i*wMB];

          switch (pMfxCurrMB->MbType)
          {
            case MFX_MBTYPE_INTRA_VC1:

                pMECurrMB->MbType     = UMC::ME_MbIntra;
                pMECurrMB->BlockTrans = 0;

               break;
            case MFX_MBTYPE_INTER_16X16_0:

                pMECurrMB->MbPart = UMC::ME_Mb16x16;
                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbFrw;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbFrwSkipped;
                    pMECurrMB->BlockTrans = 0;
                }

                pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
                pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];


                break;
            case MFX_MBTYPE_INTER_8X8_0:
                pMECurrMB->MbPart = UMC::ME_Mb8x8;
               if(!pMfxCurrMB->Skip8x8Flag)
               {
                    pMECurrMB->MbType = UMC::ME_MbFrw;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
               }
               else
               {
                    pMECurrMB->MbType = UMC::ME_MbFrwSkipped;
                    pMECurrMB->BlockTrans = 0;
               }

                    pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
                    pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];
                    pMECurrMB->MV[0][1].x = pMfxCurrMB->MV[1][0];
                    pMECurrMB->MV[0][1].y = pMfxCurrMB->MV[1][1];
                    pMECurrMB->MV[0][2].x = pMfxCurrMB->MV[2][0];
                    pMECurrMB->MV[0][2].y = pMfxCurrMB->MV[2][1];
                    pMECurrMB->MV[0][3].x = pMfxCurrMB->MV[3][0];
                    pMECurrMB->MV[0][3].y = pMfxCurrMB->MV[3][1];

                break;
            case MFX_MBTYPE_INTER_MIX_INTRA:
                pMECurrMB->MbType = UMC::ME_MbMixed;
                pMECurrMB->MbPart = UMC::ME_Mb8x8;

                pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
                pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];
                pMECurrMB->MV[0][1].x = pMfxCurrMB->MV[1][0];
                pMECurrMB->MV[0][1].y = pMfxCurrMB->MV[1][1];
                pMECurrMB->MV[0][2].x = pMfxCurrMB->MV[2][0];
                pMECurrMB->MV[0][2].y = pMfxCurrMB->MV[2][1];
                pMECurrMB->MV[0][3].x = pMfxCurrMB->MV[3][0];
                pMECurrMB->MV[0][3].y = pMfxCurrMB->MV[3][1];

                GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                GetUMCIntraPattern(pMfxCurrMB->SubMbPredMode, pMECurrMB);
               break;
            default:
                return MFX_ERR_UNSUPPORTED;
          }
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPakVc1ADV::SetMEParam_B( mfxFrameCUC *cuc, UMC::MeParams* pMEParams, mfxI32 firstRow, mfxI32 nRows)
{
    mfxU32  i=0, j =0;
    mfxU16 w = m_mfxVideoParam.mfx.FrameInfo.Width;
    mfxU16 h = m_mfxVideoParam.mfx.FrameInfo.Height;

    GetRealFrameSize(&m_mfxVideoParam.mfx, w, h);

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    Ipp32u   wMB = pFrameParam->FrameWinMbMinus1 + 1;
    Ipp32u   hMB = pFrameParam->FrameHinMbMinus1 + 1;

    Ipp32u hRow = (nRows > 0 && (firstRow + nRows) < hMB)? firstRow + nRows : hMB;

    if (m_bUsePadding)
    {
        pMEParams->PicRange.top_left.x = -14;
        pMEParams->PicRange.top_left.y = -14;
        pMEParams->PicRange.bottom_right.x = w + 13;
        pMEParams->PicRange.bottom_right.y = h + 13;
    }
    else
    {
        pMEParams->PicRange.top_left.x = 1;
        pMEParams->PicRange.top_left.y = 1;
        pMEParams->PicRange.bottom_right.x = w - 3;
        pMEParams->PicRange.bottom_right.y = h - 3;
    }

    pMEParams->SearchRange.x             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    pMEParams->SearchRange.y             = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1] - 1;

    MFX_CHECK_COND(hMB*wMB <= cuc->MbParam->NumMb)

    for (i = firstRow; i< hRow; i++)
    {
        for (j = 0; j< wMB; j++)
        {
          mfxMbCodeVC1 *pMfxCurrMB = &cuc->MbParam->Mb[j + i*wMB].VC1;
          UMC::MeMB *pMECurrMB     = &pMEParams->pSrc->MBs[j + i*wMB];

          pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
          pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];
          pMECurrMB->MV[1][0].x = pMfxCurrMB->MV[4][0];
          pMECurrMB->MV[1][0].y = pMfxCurrMB->MV[4][1];

          switch (pMfxCurrMB->MbType)
          {
            case MFX_MBTYPE_INTRA_VC1:
                pMECurrMB->MbType     = UMC::ME_MbIntra;
                pMECurrMB->BlockTrans = 0;
                break;

            case MFX_MBTYPE_INTER_16X16_0:
                pMECurrMB->MbPart = UMC::ME_Mb16x16;
                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbFrw;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbFrwSkipped;
                    pMECurrMB->BlockTrans = 0;
                }

                break;

            case MFX_MBTYPE_INTER_16X16_1:
                pMECurrMB->MbPart = UMC::ME_Mb16x16;
                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbBkw;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbBkwSkipped;
                    pMECurrMB->BlockTrans = 0;
                }

                break;
                break;
            case MFX_MBTYPE_INTER_16X16_2:
                pMECurrMB->MbPart = UMC::ME_Mb16x16;
                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbBidir;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbBidirSkipped;
                    pMECurrMB->BlockTrans = 0;
                }
                break;

            case MFX_MBTYPE_INTER_16X16_DIR:
               pMECurrMB->MbPart = UMC::ME_Mb16x16;
                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbDirect;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbDirectSkipped;
                    pMECurrMB->BlockTrans = 0;
                }

                break;

            default:
                return MFX_ERR_UNSUPPORTED;
          }
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPakVc1ADV::SetMEParam_P_Field( mfxFrameCUC *cuc, UMC::MeParams* pMEParams,
                                                 bool mixed, mfxI32 firstRow, mfxI32 nRows)
{
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU16 w = m_mfxVideoParam.mfx.FrameInfo.Width;
    mfxU16 h = m_mfxVideoParam.mfx.FrameInfo.Height;

    GetRealFrameSize(&m_mfxVideoParam.mfx, w, h);

   if (!mixed)
   {
        if (m_bUsePadding)
        {
            pMEParams->PicRange.top_left.x = -14;
            pMEParams->PicRange.top_left.y = -14;
            pMEParams->PicRange.bottom_right.x = w + 13;
            pMEParams->PicRange.bottom_right.y = h + 13;
        }
        else
        {
            pMEParams->PicRange.top_left.x = 1;
            pMEParams->PicRange.top_left.y = 1;
            pMEParams->PicRange.bottom_right.x = w - 3;
            pMEParams->PicRange.bottom_right.y = h - 3;
        }
   }
   else
   {
        if (m_bUsePadding)
        {
            pMEParams->PicRange.top_left.x = -7;
            pMEParams->PicRange.top_left.y = -7;
            pMEParams->PicRange.bottom_right.x = w + 7;
            pMEParams->PicRange.bottom_right.y = h + 7;
        }
        else
        {
            pMEParams->PicRange.top_left.x = 1;
            pMEParams->PicRange.top_left.y = 1;
            pMEParams->PicRange.bottom_right.x = w - 3;
            pMEParams->PicRange.bottom_right.y = h - 3;
        }
   }

    pMEParams->SearchRange.x = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    pMEParams->SearchRange.y = ((UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1])>>1) - 1;

    mfxU32 i=0, j =0;

   // mfxI32 k = 0; //index for MV pred

    Ipp32u   wMB = pFrameParam->FrameWinMbMinus1 + 1;
    Ipp32u   hMB = pFrameParam->FrameHinMbMinus1 + 1;

    Ipp32u hRow = (nRows > 0 && (firstRow + nRows) < hMB)? firstRow + nRows : hMB;

    MFX_CHECK_COND(hMB*wMB*2 <= cuc->MbParam->NumMb)

    for (i = firstRow; i < hRow; i++)
    {
        for (j = 0; j< wMB; j++)
        {
          //k = 4*(j + i*wMB)*sizeof(sMV);

          mfxMbCodeVC1      *pMfxCurrMB = &cuc->MbParam->Mb[j + i*wMB].VC1;
          UMC::MeMB         *pMECurrMB  = &pMEParams->pSrc->MBs[j + i*wMB];

          switch (pMfxCurrMB->MbType)
          {
            case MFX_MBTYPE_INTRA_VC1:
                pMECurrMB->MbType     = UMC::ME_MbIntra;
                pMECurrMB->BlockTrans = 0;

                break;

            case MFX_MBTYPE_INTER_16X16_0:
                pMECurrMB->MbPart = UMC::ME_Mb16x16;
                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbFrw;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbFrwSkipped;
                    pMECurrMB->BlockTrans = 0;
                }

              pMECurrMB->MV[0][0].x  = pMfxCurrMB->MV[0][0];
              pMECurrMB->MV[0][0].y  = pMfxCurrMB->MV[0][1];

              pMECurrMB->Refindex[0][0] = pMfxCurrMB->RefPicSelect[0][0];


                break;
            case MFX_MBTYPE_INTER_8X8_0:
                pMECurrMB->MbPart = UMC::ME_Mb8x8;
                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbFrw;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbFrwSkipped;
                    pMECurrMB->BlockTrans = 0;
                }

              pMECurrMB->MV[0][0].x  = pMfxCurrMB->MV[0][0];
              pMECurrMB->MV[0][0].y  = pMfxCurrMB->MV[0][1];
              pMECurrMB->MV[0][1].x  = pMfxCurrMB->MV[1][0];
              pMECurrMB->MV[0][1].y  = pMfxCurrMB->MV[1][1];
              pMECurrMB->MV[0][2].x  = pMfxCurrMB->MV[2][0];
              pMECurrMB->MV[0][2].y  = pMfxCurrMB->MV[2][1];
              pMECurrMB->MV[0][3].x  = pMfxCurrMB->MV[3][0];
              pMECurrMB->MV[0][3].y  = pMfxCurrMB->MV[3][1];

              pMECurrMB->Refindex[0][0] = pMfxCurrMB->RefPicSelect[0][0];
              pMECurrMB->Refindex[0][1] = pMfxCurrMB->RefPicSelect[0][1];
              pMECurrMB->Refindex[0][2] = pMfxCurrMB->RefPicSelect[0][2];
              pMECurrMB->Refindex[0][3] = pMfxCurrMB->RefPicSelect[0][3];



                break;
            default:
                return MFX_ERR_UNSUPPORTED;
          }
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoPakVc1ADV::SetMEParam_B_Field( mfxFrameCUC *cuc, UMC::MeParams* pMEParams,
                                                bool mixed, mfxI32 firstRow, mfxI32 nRows)
{
    mfxU32 i=0,j =0;

    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    mfxU16 w = m_mfxVideoParam.mfx.FrameInfo.Width;
    mfxU16 h = m_mfxVideoParam.mfx.FrameInfo.Height;

    GetRealFrameSize(&m_mfxVideoParam.mfx, w, h);

    Ipp32u   wMB = pFrameParam->FrameWinMbMinus1 + 1;
    Ipp32u   hMB = pFrameParam->FrameHinMbMinus1 + 1;

    Ipp32u hRow = (nRows > 0 && (firstRow + nRows) < hMB)? firstRow + nRows : hMB;

   if (!mixed)
   {
        if (m_bUsePadding)
        {
            pMEParams->PicRange.top_left.x = -14;
            pMEParams->PicRange.top_left.y = -14;
            pMEParams->PicRange.bottom_right.x = w + 13;
            pMEParams->PicRange.bottom_right.y = h + 13;
        }
        else
        {
            pMEParams->PicRange.top_left.x = 1;
            pMEParams->PicRange.top_left.y = 1;
            pMEParams->PicRange.bottom_right.x = w - 3;
            pMEParams->PicRange.bottom_right.y = h - 3;
        }
         pMEParams->MbPart =  UMC::ME_Mb16x16;
   }
   else
   {
        if (m_bUsePadding)
        {
            pMEParams->PicRange.top_left.x = -7;
            pMEParams->PicRange.top_left.y = -7;
            pMEParams->PicRange.bottom_right.x = w + 7;
            pMEParams->PicRange.bottom_right.y = h + 7;
        }
        else
        {
            pMEParams->PicRange.top_left.x = 1;
            pMEParams->PicRange.top_left.y = 1;
            pMEParams->PicRange.bottom_right.x = w - 3;
            pMEParams->PicRange.bottom_right.y = h - 3;
        }
        pMEParams->MbPart =  UMC::ME_Mb8x8;
   }

    pMEParams->SearchRange.x = UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange] - 1;
    pMEParams->SearchRange.y = ((UMC_VC1_ENCODER::MVRange[2*pFrameParam->ExtendedMvRange + 1])>>1) - 1;

    MFX_CHECK_COND(hMB*wMB*2 <= cuc->MbParam->NumMb)

    for (i = firstRow; i< hRow; i++)
    {
        for (j = 0; j< wMB; j++)
        {
          mfxMbCodeVC1      *pMfxCurrMB = &cuc->MbParam->Mb[j + i*wMB].VC1;
          UMC::MeMB         *pMECurrMB  = &pMEParams->pSrc->MBs[j + i*wMB];

          switch (pMfxCurrMB->MbType)
          {
            case MFX_MBTYPE_INTRA_VC1:
                pMECurrMB->MbType     = UMC::ME_MbIntra;
                pMECurrMB->BlockTrans = 0;

                break;

            case MFX_MBTYPE_INTER_16X16_0:
                pMECurrMB->MbPart = UMC::ME_Mb16x16;
                pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
                pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];
                pMECurrMB->MV[1][0].x = pMfxCurrMB->MV[4][0];
                pMECurrMB->MV[1][0].y = pMfxCurrMB->MV[4][1];

                pMECurrMB->Refindex[0][0] = pMfxCurrMB->RefPicSelect[0][0];
                pMECurrMB->Refindex[1][0] = pMfxCurrMB->RefPicSelect[1][0];

                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbFrw;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbFrwSkipped;
                    pMECurrMB->BlockTrans = 0;
                }

                break;

            case MFX_MBTYPE_INTER_16X16_1:
                pMECurrMB->MbPart = UMC::ME_Mb16x16;
                pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
                pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];
                pMECurrMB->MV[1][0].x = pMfxCurrMB->MV[4][0];
                pMECurrMB->MV[1][0].y = pMfxCurrMB->MV[4][1];

                pMECurrMB->Refindex[0][0] = pMfxCurrMB->RefPicSelect[0][0];
                pMECurrMB->Refindex[1][0] = pMfxCurrMB->RefPicSelect[1][0];

                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbBkw;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbBkwSkipped;
                    pMECurrMB->BlockTrans = 0;
                }
                break;

            case MFX_MBTYPE_INTER_16X16_2:
                pMECurrMB->MbPart = UMC::ME_Mb16x16;

                pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
                pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];
                pMECurrMB->MV[1][0].x = pMfxCurrMB->MV[4][0];
                pMECurrMB->MV[1][0].y = pMfxCurrMB->MV[4][1];

                pMECurrMB->Refindex[0][0] = pMfxCurrMB->RefPicSelect[0][0];
                pMECurrMB->Refindex[1][0] = pMfxCurrMB->RefPicSelect[1][0];

                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbBidir;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbBidirSkipped;
                    pMECurrMB->BlockTrans = 0;
                }
                break;

            case MFX_MBTYPE_INTER_16X16_DIR:
               pMECurrMB->MbPart = UMC::ME_Mb16x16;

                pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
                pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];
                pMECurrMB->MV[1][0].x = pMfxCurrMB->MV[4][0];
                pMECurrMB->MV[1][0].y = pMfxCurrMB->MV[4][1];

                pMECurrMB->Refindex[0][0] = pMfxCurrMB->RefPicSelect[0][0];
                pMECurrMB->Refindex[1][0] = pMfxCurrMB->RefPicSelect[1][0];

                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    pMECurrMB->MbType = UMC::ME_MbDirect;
                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    pMECurrMB->MbType = UMC::ME_MbDirectSkipped;
                    pMECurrMB->BlockTrans = 0;
                }

                break;
            case MFX_MBTYPE_INTER_FIELD_8X8_00:
                pMECurrMB->MbPart = UMC::ME_Mb8x8;

                pMECurrMB->MV[0][0].x = pMfxCurrMB->MV[0][0];
                pMECurrMB->MV[0][0].y = pMfxCurrMB->MV[0][1];
                pMECurrMB->MV[0][1].x = pMfxCurrMB->MV[1][0];
                pMECurrMB->MV[0][1].y = pMfxCurrMB->MV[1][1];
                pMECurrMB->MV[0][2].x = pMfxCurrMB->MV[2][0];
                pMECurrMB->MV[0][2].y = pMfxCurrMB->MV[2][1];
                pMECurrMB->MV[0][3].x = pMfxCurrMB->MV[3][0];
                pMECurrMB->MV[0][3].y = pMfxCurrMB->MV[3][1];
                pMECurrMB->MV[1][0].x = pMfxCurrMB->MV[4][0];
                pMECurrMB->MV[1][0].y = pMfxCurrMB->MV[4][1];
                pMECurrMB->MV[1][1].x = pMfxCurrMB->MV[5][0];
                pMECurrMB->MV[1][1].y = pMfxCurrMB->MV[5][1];
                pMECurrMB->MV[1][2].x = pMfxCurrMB->MV[6][0];
                pMECurrMB->MV[1][2].y = pMfxCurrMB->MV[6][1];
                pMECurrMB->MV[1][3].x = pMfxCurrMB->MV[7][0];
                pMECurrMB->MV[1][3].y = pMfxCurrMB->MV[7][1];

                pMECurrMB->Refindex[0][0] = pMfxCurrMB->RefPicSelect[0][0];
                pMECurrMB->Refindex[0][1] = pMfxCurrMB->RefPicSelect[0][1];
                pMECurrMB->Refindex[0][2] = pMfxCurrMB->RefPicSelect[0][2];
                pMECurrMB->Refindex[0][3] = pMfxCurrMB->RefPicSelect[0][3];

                pMECurrMB->Refindex[1][0] = pMfxCurrMB->RefPicSelect[1][0];
                pMECurrMB->Refindex[1][1] = pMfxCurrMB->RefPicSelect[1][1];
                pMECurrMB->Refindex[1][2] = pMfxCurrMB->RefPicSelect[1][2];
                pMECurrMB->Refindex[1][3] = pMfxCurrMB->RefPicSelect[1][3];

                if(!pMfxCurrMB->Skip8x8Flag)
                {
                    if(pMfxCurrMB->SubMbPredMode == 0)
                        pMECurrMB->MbType = UMC::ME_MbFrw;
                    else
                        pMECurrMB->MbType = UMC::ME_MbBkw;

                    GetUMCTransformType(pMfxCurrMB->SubMbShape, pMfxCurrMB->SubMbShapeU,
                                        pMfxCurrMB->SubMbShapeV, &pMECurrMB->BlockTrans);
                }
                else
                {
                    if(pMfxCurrMB->SubMbPredMode == 0)
                        pMECurrMB->MbType = UMC::ME_MbFrwSkipped;
                    else
                        pMECurrMB->MbType = UMC::ME_MbBkwSkipped;

                    pMECurrMB->BlockTrans = 0;
                }

                break;
            default:
                assert(0);
                return MFX_ERR_UNSUPPORTED;
          }

        }
    }
    return MFX_ERR_NONE;
}


#endif //MFX_ENABLE_VC1_VIDEO_ENCODER
