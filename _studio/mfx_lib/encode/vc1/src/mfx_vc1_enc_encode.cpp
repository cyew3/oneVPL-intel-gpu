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

#include "mfx_vc1_enc_encode.h"
#include "umc_vc1_video_encoder.h"
#include "umc_video_data.h"
#include "new.h"
#include "mfx_vc1_enc_defs.h"

MFXVideoENCODEVC1::MFXVideoENCODEVC1(VideoCORE *core, mfxStatus *status):VideoENCODE(),
                                                                                  pEncoder(0),
                                                                                  m_pRC (0),
                                                                                  pUMC_MA(NULL),
                                                                                  m_UMC_MAID(0),
                                                                                  m_pUMCMABuffer(NULL),
                                                                                  m_VideoEncoderID(0),
                                                                                  m_pVideoEncoderBuffer(NULL),
                                                                                  m_UserDataBuffer(0),
                                                                                  m_UDBufferID (0),
                                                                                  m_UserDataSize(0),
                                                                                  m_UserDataBufferSize (0),
                                                                                  m_UMCLastFrameType(0)
  {
      m_pMFXCore = core;
      memset (&m_mfxVideoParam,0, sizeof(mfxVideoParam));
      if(!m_pMFXCore)
      {
            *status =  MFX_ERR_MEMORY_ALLOC;
      }
      *status = MFX_ERR_NONE;

      GetChromaShift = GetChromaShiftYV12;
  }
MFXVideoENCODEVC1::~MFXVideoENCODEVC1(void)
{
    Close();
}

mfxStatus MFXVideoENCODEVC1::Init(mfxVideoParam *par)
{
      mfxStatus                  MFXSts = MFX_ERR_NONE;
      UMC::Status                Sts = UMC::UMC_OK;
      UMC::VC1EncoderParams      vc1Params;
      UMC::MemoryAllocatorParams pParams;

      bool bNV12 = (par->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12);

      Close();

      mfxU16 w = par->mfx.FrameInfo.Width;
      mfxU16 h = par->mfx.FrameInfo.Height;

      GetRealFrameSize(&par->mfx, w, h);
      GetChromaShift       = (bNV12)? GetChromaShiftNV12:GetChromaShiftYV12;

      GetChromaShift       = (bNV12)? GetChromaShiftNV12:GetChromaShiftYV12;


      //Allocation UMC(MFX) memory allocator
      Ipp32s memSize = sizeof(mfx_UMC_MemAllocator);

      MFXSts = m_pMFXCore->AllocBuffer(memSize, MFX_VC1_MA_MEMORY,&m_UMC_MAID);
      if(MFXSts != MFX_ERR_NONE || !m_UMC_MAID)
        return MFX_ERR_MEMORY_ALLOC;

       MFXSts = m_pMFXCore->LockBuffer(m_UMC_MAID, &m_pUMCMABuffer);
       if(MFXSts != MFX_ERR_NONE || !m_pUMCMABuffer)
           return MFX_ERR_MEMORY_ALLOC;

      pUMC_MA = new (m_pUMCMABuffer) mfx_UMC_MemAllocator;
      if (!pUMC_MA)
          return MFX_ERR_MEMORY_ALLOC;

      memset(&pParams, 0, sizeof(UMC::MemoryAllocatorParams));
      pUMC_MA->Init(&pParams, m_pMFXCore);

      //allocation VC1videoEncoder
      Ipp32s memsize = sizeof(UMC::VC1VideoEncoder);

      MFXSts = m_pMFXCore->AllocBuffer(memSize, MFX_VC1_ENCODER_MEMORY,&m_VideoEncoderID);
       if(MFXSts != MFX_ERR_NONE || !m_VideoEncoderID)
        return MFX_ERR_MEMORY_ALLOC;

       MFXSts = m_pMFXCore->LockBuffer(m_VideoEncoderID, &m_pVideoEncoderBuffer);
       if(MFXSts != MFX_ERR_NONE || !m_pVideoEncoderBuffer)
            return MFX_ERR_MEMORY_ALLOC;

      pEncoder = new (m_pVideoEncoderBuffer) UMC::VC1VideoEncoder;
      if (!pEncoder)
          return MFX_ERR_MEMORY_ALLOC;

      memcpy (&m_mfxVideoParam,par,sizeof(mfxVideoParam));

      //------------------------------------------------------------------------------------------------

      if ((par->mfx.TargetUsage& 0x0f) == MFX_TARGETUSAGE_UNKNOWN)
      {
          par->mfx.TargetUsage |= 3;
      }

      if(par->mfx.CodecProfile == 0)
      {
          par->mfx.CodecProfile = MFX_PROFILE_VC1_ADVANCED;
      }

      if(par->mfx.CodecLevel == 0)
      {
          if(par->mfx.CodecProfile == MFX_PROFILE_VC1_ADVANCED)
          {
              par->mfx.CodecLevel = MFX_LEVEL_VC1_4;
          }
          else
          {
              if(par->mfx.CodecProfile == MFX_PROFILE_VC1_MAIN)
              {
                  par->mfx.CodecLevel = MFX_LEVEL_VC1_HIGH;
              }
              else
              {
                  par->mfx.CodecLevel = MFX_LEVEL_VC1_MEDIAN;
              }
          }
      }

      /* Those parameters must be set using target usage or extended buffer */
      {
          Ipp8u MESpeed = 0;
          bool  useFB = 0;
          bool  useFastFB = 0;
          bool  intesityComp = 0;
          bool  changeInterpolationType = 0;
          bool  changeVLCTables = 0;
          bool  trellisQuant = 0;
          bool  usePadding = 0;
          bool  VSTransform = 0;
          bool  deblocking = 0;
          mfxU8 smoothing = 0;
          bool  fastUV = 0;
          bool  mixed = 0;
          Ipp32u twoRef = 0;

          SetPROParameters (par->mfx.TargetUsage & 0x0F,
              MESpeed, useFB,useFastFB, intesityComp,
              changeInterpolationType, changeVLCTables,
              trellisQuant,usePadding,
              VSTransform,deblocking,smoothing,fastUV);

          SetUFParameters(par->mfx.TargetUsage & 0x0F, mixed, twoRef);

          ExtVC1SequenceParams * pSH_Ex = (ExtVC1SequenceParams *)GetExBufferIndex(par, MFX_CUC_VC1_SEQPARAMSBUF);
          if(pSH_Ex)
          {
              MESpeed                   = pSH_Ex->MESpeed;
              useFB                     = pSH_Ex->useFB;
              useFastFB                 = pSH_Ex->fastFB;
              intesityComp              = pSH_Ex->intensityCompensation;
              changeInterpolationType   = pSH_Ex->changeInterpolationType;
              changeVLCTables           = pSH_Ex->changeVLCTables;
              trellisQuant              = pSH_Ex->trellisQuantization;
              VSTransform               = pSH_Ex->vsTransform;
              deblocking                = pSH_Ex->deblocking;
              smoothing                 = pSH_Ex->overlapSmoothing;
              fastUV                    = !pSH_Ex->noFastUV;
              mixed                     = pSH_Ex->mixed;
              twoRef                    = pSH_Ex->refFieldType;
          }
          // set UMC parameters
          {
              MFXSts = GetUMCProfile(par->mfx.CodecProfile, &vc1Params.profile);
              if(MFXSts != MFX_ERR_NONE)
                  return MFX_ERR_UNSUPPORTED;

              MFXSts = GetUMCLevel(par->mfx.CodecProfile, par->mfx.CodecLevel, &vc1Params.level);
              if(MFXSts != MFX_ERR_NONE)
                  return MFX_ERR_UNSUPPORTED;

              vc1Params.m_uiGOPLength  = CalculateUMCGOPLength(par->mfx.GopPicSize,par->mfx.TargetUsage& 0x0f);
              vc1Params.m_uiBFrmLength = CalculateMAXBFrames(par->mfx.GopRefDist);

              vc1Params.info.framerate = CalculateUMCFramerate(par->mfx.FrameInfo.FrameRateCode,
                  par->mfx.FrameInfo.FrameRateExtN,
                  par->mfx.FrameInfo.FrameRateExtD);

              vc1Params.info.bitrate = CalculateUMCBitrate(par->mfx.TargetKbps);

              vc1Params.m_uiHRDBufferSize         = CalculateUMC_HRD_BufferSize(par->mfx.BufferSizeInKB);
              vc1Params.m_uiHRDBufferInitFullness = CalculateUMC_HRD_InitDelay(par->mfx.InitialDelayInKB);

              vc1Params.info.clip_info.width  = w;
              vc1Params.info.clip_info.height = h;

              vc1Params.numThreads = par->mfx.NumThread;
              vc1Params.m_uiNumSlices = par->mfx.NumSlice;

              vc1Params.m_iConstQuant = 0; //TODO: need to check ????????????????

              // need to check those parameters
              vc1Params.m_bSceneAnalyzer = false;
              vc1Params.m_bOrigFramePred = false;
              vc1Params.m_bFrameRecoding = false;
              vc1Params.m_pStreamName = NULL;

              vc1Params.m_bInterlace = (par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE);

              vc1Params.info.stream_type = UMC::VC1_VIDEO;

              vc1Params.info.color_format = UMC::YUV420;
              vc1Params.lpMemoryAllocator = pUMC_MA;


              vc1Params.m_uiMESearchSpeed    = MESpeed ;
              vc1Params.m_bUseMeFeedback     = useFB;
              vc1Params.m_bUseFastMeFeedback = useFastFB;
              vc1Params.m_bUseUpdateMeFeedback = false;

              vc1Params.m_bVSTransform = VSTransform;
              vc1Params.m_bDeblocking  = deblocking;
              vc1Params.m_bMixed       = mixed;
              vc1Params.m_uiReferenceFieldType = twoRef;
              vc1Params.m_uiOverlapSmoothing = smoothing;
              vc1Params.m_bFastUVMC         = fastUV;
              vc1Params.m_bIntensityCompensation = intesityComp;

              vc1Params.m_bChangeInterpPixelType = changeInterpolationType;
              vc1Params.m_bSelectVLCTables = changeVLCTables;
              vc1Params.m_bUseTreillisQuantization = trellisQuant;
              vc1Params.m_bNV12 = bNV12;
          }
      }

      // --------------- User Data -----------------------------------------

      m_UserDataBufferSize      = w*h;

      MFXSts = m_pMFXCore->AllocBuffer( m_UserDataBufferSize, MFX_VC1_ME_MEMORY,&m_UDBufferID);
      if(MFXSts != MFX_ERR_NONE || !m_UDBufferID)
          return MFX_ERR_MEMORY_ALLOC;

      MFXSts = m_pMFXCore->LockBuffer(m_UDBufferID, &m_UserDataBuffer);
      if(MFXSts != MFX_ERR_NONE || !m_UserDataBuffer)
          return  MFX_ERR_MEMORY_ALLOC;

      m_UserDataSize        = 0;

      //--------------------------------------------------------------------

      Sts = pEncoder->Init(&vc1Params);
      UMC_MFX_CHECK_STS(Sts);

      return MFXSts;
}

mfxStatus MFXVideoENCODEVC1::Reset(mfxVideoParam *par)
{
      mfxStatus       MFXSts = MFX_ERR_NONE;
      UMC::Status     Sts = UMC::UMC_OK;

      MFX_CHECK_NULL_PTR1(par);

      MFX_CHECK_COND(par->mfx.FrameInfo.CropX + par->mfx.FrameInfo.CropW
          <= par->mfx.FrameInfo.Width);
      MFX_CHECK_COND(par->mfx.FrameInfo.CropY + par->mfx.FrameInfo.CropH
          <= par->mfx.FrameInfo.Height);


      MFX_CHECK_COND(par->mfx.FrameInfo.Width <= 4080);
      MFX_CHECK_COND(par->mfx.FrameInfo.Height <= 4080);

      Close();
      Sts = Init(par);
      return MFXSts;
}

mfxStatus MFXVideoENCODEVC1::Close(void)
{
      mfxStatus       MFXSts = MFX_ERR_NONE;
      UMC::Status     Sts = UMC::UMC_OK;

      if(pEncoder)
      {
          Sts = pEncoder->Close();
          UMC_MFX_CHECK_STS(Sts);
          pEncoder = NULL;
      }

      if(m_VideoEncoderID)
      {
         m_pMFXCore->UnlockBuffer(m_VideoEncoderID);
         m_pMFXCore->FreeBuffer(m_VideoEncoderID);
         m_VideoEncoderID       = 0;
      }
      m_pVideoEncoderBuffer  = NULL;
      m_VideoEncoderID = 0;

      if(pUMC_MA)
      {
          Sts = pUMC_MA->Close();
          UMC_MFX_CHECK_STS(Sts);
          pUMC_MA = 0;
      }

      if(m_UMC_MAID)
      {
         m_pMFXCore->UnlockBuffer(m_UMC_MAID);
         m_pMFXCore->FreeBuffer(m_UMC_MAID);
         m_UMC_MAID = NULL;
      }
      m_pUMCMABuffer = 0;

      if (m_UDBufferID)
      {
          m_pMFXCore->UnlockBuffer(m_UDBufferID);
          m_pMFXCore->FreeBuffer(m_UDBufferID);
          m_UDBufferID = 0;
      }
      m_UserDataBufferSize = 0;
      m_UserDataBuffer = 0;
      m_UserDataSize = 0;

      pUMC_MA = NULL;
      m_UMC_MAID = 0;

      return MFXSts;
}

/*mfxStatus MFXVideoENCODEVC1::Query(mfxU8 *chklist, mfxU32 chksz)
{
      mfxStatus       MFXSts = MFX_ERR_NONE;
      return MFXSts;
}*/

mfxStatus MFXVideoENCODEVC1::GetVideoParam(mfxVideoParam *par)
{
      mfxStatus       MFXSts = MFX_ERR_NONE;
      UMC::Status     Sts = UMC::UMC_OK;
      UMC::VC1EncoderParams vc1Params;

      MFX_CHECK_NULL_PTR1(par);

      if(!pEncoder)
          return MFX_ERR_NOT_INITIALIZED;

      Sts = pEncoder->GetInfo(&vc1Params);
      UMC_MFX_CHECK_STS(Sts);

      par->mfx.CodecId          = MFX_CODEC_VC1;

      par->mfx.FrameInfo.Height = (((vc1Params.info.clip_info.height + 15)>>4)<<4);
      par->mfx.FrameInfo.Width  = (((vc1Params.info.clip_info.width  + 15)>>4)<<4);

      MFXSts = GetMFXProfile(vc1Params.profile, &par->mfx.CodecProfile);
      MFX_CHECK_STS(MFXSts);
      MFXSts = GetMFXLevel(vc1Params.profile, vc1Params.level, &par->mfx.CodecLevel);
      MFX_CHECK_STS(MFXSts);

      par->mfx.TargetKbps = vc1Params.info.bitrate/1000;

      par->mfx.BufferSizeInKB   = CalculateMFX_HRD_BufferSize(vc1Params.m_uiHRDBufferSize);
      par->mfx.InitialDelayInKB = CalculateMFX_HRD_InitDelay(vc1Params.m_uiHRDBufferInitFullness);

      CalculateMFXFramerate(&par->mfx, vc1Params.info.framerate);

      par->mfx.FramePicture= !vc1Params.m_bInterlace;

      par->mfx.GopPicSize = CalculateMFXGOPLength(vc1Params.m_uiGOPLength);
      par->mfx.GopRefDist = CalculateGopRefDist(vc1Params.m_uiBFrmLength);

      par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

      par->mfx.NumThread = vc1Params.numThreads;


      return MFXSts;
}

mfxStatus MFXVideoENCODEVC1::GetFrameParam(mfxFrameParam *p)
{
      mfxStatus       MFXSts = MFX_ERR_NONE;
      bool bField;
      bool bFrame;

      mfxU16 w = m_mfxVideoParam.mfx.FrameInfo.Width;
      mfxU16 h = m_mfxVideoParam.mfx.FrameInfo.Height;

      GetRealFrameSize(&m_mfxVideoParam.mfx, w, h);
      mfxFrameParamVC1 *par = &p->VC1;

      MFX_CHECK_NULL_PTR1(par);

      memset((Ipp8u*)par+8,0,sizeof(mfxFrameParam)-8);

      MFXSts = GetMFXPictureType(par->FrameType, par->FrameType2nd, bField , bFrame,(UMC_VC1_ENCODER::ePType)m_UMCLastFrameType);
      if (MFXSts == MFX_ERR_NONE)
      {
          par->FieldPicFlag = (bField)?1:0;
          par->InterlacedFrameFlag = (bFrame)? 1:0;

          par->FrameWinMbMinus1 = (w + 15)/16;
          if (par->FieldPicFlag)
          {
            par->FrameHinMbMinus1 = (w/2 + 15)/16 -1;
          }
          else
          {
            par->FrameHinMbMinus1 = (w + 15)/16 -1;
          }
          par->NumMb = (par->FrameWinMbMinus1 + 1)*(par->FrameHinMbMinus1+1);
      }
      return MFXSts;
}

mfxStatus MFXVideoENCODEVC1::GetSliceParam(mfxSliceParam *par)
{
      mfxStatus       MFXSts = MFX_ERR_UNSUPPORTED;
      return MFXSts;
}

#define unlock_buff                                                             \
if (frameDataLocked)                                                            \
{                                                                               \
    m_pMFXCore->UnlockFrame(surface->Data[0]->MemId,surface->Data[0]);          \
    frameDataLocked  =  false;                                                  \
}


#define UMC_MFX_CHECK_STS_1(err)    if (err != UMC::UMC_OK)                  {unlock_buff; return MFX_ERR_UNSUPPORTED;}
#define UMC_MFX_CHECK_WARN_1(err)   if (err == UMC::UMC_ERR_NOT_ENOUGH_DATA) {unlock_buff; return MFX_ERR_MORE_DATA;}

#define MFX_CHECK_STS_1(err)        if (err != MFX_ERR_NONE)                 {unlock_buff; return err;}
#define MFX_CHECK_COND_1(cond)      if  (!(cond))                            {unlock_buff; return MFX_ERR_UNSUPPORTED;}

mfxStatus MFXVideoENCODEVC1::EncodeFrame(mfxFrameSurface *surface, mfxBitstream *bs)
{

      mfxStatus       MFXSts = MFX_ERR_NONE;
      UMC::Status     Sts = UMC::UMC_OK;
      UMC::MediaDataUD  out;
      UMC::MediaDataUD *p_dataOut = &out;

      bool frameDataLocked  = false;

      if(!pEncoder)
          return MFX_ERR_NOT_INITIALIZED;

      MFX_CHECK_NULL_PTR1(surface);
      MFX_CHECK_NULL_PTR1(bs);


      if ((surface!= 0 && surface->Info.NumFrameData!=0) &&
          (surface->Data[0]->Y==NULL || surface->Data[0]->U==NULL || surface->Data[0]->V==NULL))
      {
            MFXSts =  m_pMFXCore->LockFrame(surface->Data[0]->MemId, surface->Data[0]);
            MFX_CHECK_STS_1(MFXSts);
            frameDataLocked  =  true;
      }


      MFX_CHECK_COND_1(bs->MaxLength - bs->DataOffset>0)

      out.SetBufferPointer(bs->Data + bs->DataOffset,
                           bs->MaxLength - bs->DataOffset);

      if(surface!= 0 && surface->Info.NumFrameData!=0)
      {
          mfxU32          Width       = GetWidth(&m_mfxVideoParam.mfx.FrameInfo);
          mfxU32          Height      = GetHeight(&m_mfxVideoParam.mfx.FrameInfo);

          mfxU32          ShiftLuma   = GetLumaShift(&surface->Info,surface->Data[0]);
          mfxU32          ChromaPitch = 0;

          if(surface->Info.FourCC == MFX_FOURCC_NV12)
              ChromaPitch = surface->Data[0]->Pitch;
          else
              ChromaPitch = surface->Data[0]->Pitch>>1;
          mfxU32          ShiftChroma = GetChromaShift(&surface->Info,surface->Data[0]);

          UMC::VideoDataUD in;
          UMC::VideoDataUD *p_dataIn = &in;

          //MFX_CHECK_COND_1(surface->Info.Step == 1 && surface->Info.DeltaCStep == 0); // planar format

          out.SetTime((Ipp64f)surface->Data[0]->TimeStamp); //ToDo???????????????

          p_dataIn->Init(Width, Height, 3, 8);

          p_dataIn->SetPlanePointer(surface->Data[0]->Y + ShiftLuma, 0);
          p_dataIn->SetPlanePitch(surface->Data[0]->Pitch, 0);
          p_dataIn->SetPlanePointer(surface->Data[0]->U + ShiftChroma, 1);
          p_dataIn->SetPlanePitch(ChromaPitch, 1);
          p_dataIn->SetPlanePointer(surface->Data[0]->V + ShiftChroma, 2);
          p_dataIn->SetPlanePitch(ChromaPitch, 2);

          p_dataIn->SetDataSize(surface->Data[0]->DataLength);

          if (m_UserDataSize)
          {
            p_dataIn->pUserData = m_UserDataBuffer;
            p_dataIn->userDataSize = m_UserDataSize;
            m_UserDataSize = 0;
          }

          Sts = pEncoder->GetFrame(p_dataIn, p_dataOut);
          UMC_MFX_CHECK_WARN_1(Sts);
          UMC_MFX_CHECK_STS_1(Sts);
      }
      else
      {
          Sts = pEncoder->GetFrame(NULL, p_dataOut);
          UMC_MFX_CHECK_WARN_1(Sts);
          UMC_MFX_CHECK_STS_1(Sts);
      }
      bs->DataLength = (mfxU32)p_dataOut->GetDataSize();
      m_UMCLastFrameType = p_dataOut->pictureCode;

      if (frameDataLocked)
      {
          MFXSts =  m_pMFXCore->UnlockFrame(surface->Data[0]->MemId,surface->Data[0]);
          MFX_CHECK_STS_1(MFXSts);
          frameDataLocked  =  false;
      }
      surface->Data[0]->Locked --;
      return MFXSts;
}
#undef UMC_MFX_CHECK_STS_1
#undef UMC_MFX_CHECK_WARN_1
#undef MFX_CHECK_STS_1
#undef MFX_CHECK_COND_1
#undef unlock_buff

mfxStatus MFXVideoENCODEVC1::InsertUserData(mfxU8 *ud, mfxU32 len, mfxU64 ts)
{
    if (len > m_UserDataBufferSize || CheckUserData(ud,len)!=MFX_ERR_NONE)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    m_UserDataSize = len;
    memcpy (m_UserDataBuffer,ud,len);

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEVC1::Query(mfxVideoParam *in, mfxVideoParam *out)
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

#endif
