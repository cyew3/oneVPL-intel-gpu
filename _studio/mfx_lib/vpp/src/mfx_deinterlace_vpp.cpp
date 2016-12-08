//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP) && !defined(MFX_ENABLE_HW_ONLY_VPP)
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

//IPP
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"
#include "ippvc.h"

#include "mfx_vpp_utils.h"
#include "mfx_deinterlace_vpp.h"

/* ******************************************************************** */
/*                 MACROS for DI/ITC algorithms                         */
/* ******************************************************************** */

#define VPP_ITC_FRAME_SKIP      (5)

// 3:2 pull down only
#define IVTC_NUM_PATTERNS      (6)

#define FiR_THR_FAC_DEFAULT    (0.2f)
#define FiR_THR_FAC_EXPECTED   (1.0f)
#define FiR_THR_FAC_UNEXPECTED (0.1f)
#define T2B_CHECK_WEIGHT       (1.5f)
#define IVTC_CHECK_PATTERN_LEN (10)
#define FiR_THR_FAC_AV         (0.5f)

#define VPP_ITC_POST_PROC                                                 \
{                                                                         \
  if( mfxSts == MFX_ERR_NOT_INITIALIZED ){                                \
    mfxSts = SurfaceCopy_ROI(&(pState->refList.Surface1), in);              \
    mfxSts = MFX_ERR_MORE_DATA;                                           \
  }                                                                       \
}


typedef struct _cadence_pattern {
  mfxU32 frp;
  mfxU32 pff;
  mfxU32 copy_patern;
  mfxI32 period;
} cadence_pattern;

static const cadence_pattern cadence_frp_table[IVTC_NUM_PATTERNS] =  // 2 bits per frp value
{
  {0x048, 0x054, 0x31f, 5},    // 01020 01110 T0B0 T0B1 T1B2 T2B2 T3B3 3:2
  {0x084, 0x0A8, 0x32f, 5},    // 02010 02220 T0B0 T1B0 T2B1 T2B2 T3B3 3:2
  {0x042, 0x055, 0x317, 5},    // 01002 01111 T0B0 T0B1 T1B2 T2B3 T3B3 3:2:2:3
  {0x081, 0x0AA, 0x32b, 5},    // 02001 02222 T0B0 T1B0 T2B1 T3B2 T3B3 3:2:2:3
  {0x018, 0x014, 0x3cf, 5},    // 00100 00110 T0B0 T1B1 T1B2 T2B2 T3B3 2:3:3:2
  {0x024, 0x028, 0x3cf, 5}//,    // 00200 00220 T0B0 T1B1 T2B1 T2B2 T3B3 2:3:3:2
  //  {0x048, 0x054, 6},    // 001020 001110 T0B0 T1B1 T1B2 T2B3 T3B3 T4B4 2:3:2:3:2  25p -> 60i
  //  {0x048, 0x0A8, 6}     // 002010 002220 T0B0 T1B1 T2B1 T3B2 T3B3 T4B4 2:3:2:3:2  25p -> 60i
};

/* ******************************************************************** */
/*               service function of VPP filter [DI/ITC]                */
/* ******************************************************************** */

static
mfxStatus CopyCrop( mfxFrameSurface1* in, mfxFrameSurface1* out)
{
  MFX_CHECK_NULL_PTR2(in, out);

  out->Info.CropX = in->Info.CropX;
  out->Info.CropY = in->Info.CropY;
  out->Info.CropW = in->Info.CropW;
  out->Info.CropH = in->Info.CropH;

  return MFX_ERR_NONE;

} // mfxStatus CopyCrop( mfxFrameSurface1* in, mfxFrameSurface1* out)


/* ******************************************************************** */
/*                 implementation of VPP filter [DI/ITC]                */
/* ******************************************************************** */

MFXVideoVPPDeinterlace::MFXVideoVPPDeinterlace(VideoCORE *core, mfxStatus* sts ) : FilterVPP()
{

  m_core = core;

  /* ITC algorithm */
  ippsZero_8u( (Ipp8u*)&m_itcState, sizeof(sITCState) );
  m_itcState.outBuf.mids   = NULL;
  m_itcState.outBuf.Surface1.Data.Locked = 0;
  m_itcState.isInited = 0;
  m_itcState.isFrameInBuf = 0;

  /* DI algorithm */
  m_diState.pDIBlendStateY  = NULL;
  m_diState.pDIBlendStateUV = NULL;

  m_diState.refList[0]       = NULL;
  m_diState.refList[1]       = NULL;

  m_diState.bSyncRefList[0]  = false;
  m_diState.bSyncRefList[1]  = false;

  m_diState.numProcessedFrames = 0;
  m_diState.prevTimeStamp      = 0;

  /* readiness state */
  // sync
  m_syncReadinessState.numProcessedFrames = 0;
  m_syncReadinessState.TimeStamp          = 0;
  m_syncReadinessState.frameOrder         = 0;
  m_syncReadinessState.bReadyOutput       = false;
  m_syncReadinessState.delayedTimeStamp_AdvancedMode = 0;

  //process
  m_processReadinessState.numProcessedFrames = 0;
  m_processReadinessState.TimeStamp          = 0;
  m_processReadinessState.frameOrder         = 0;
  m_processReadinessState.bReadyOutput       = false;
  m_processReadinessState.delayedTimeStamp_AdvancedMode = 0;

  /* mode of deinterlace */
  m_mode = VPP_SIMPLE_DEINTERLACE;

  m_deltaTimeStamp = 0;

  VPP_CLEAN;

  *sts = MFX_ERR_NONE;

  return;

} // MFXVideoVPPDeinterlace::MFXVideoVPPDeinterlace(...)


MFXVideoVPPDeinterlace::~MFXVideoVPPDeinterlace(void)
{
  Close();

  return;

} // MFXVideoVPPDeinterlace::~MFXVideoVPPDeinterlace(void)


mfxStatus MFXVideoVPPDeinterlace::SetParam( mfxExtBuffer* pHint )
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  if( pHint )
  {
    // mfxSts = something
  }

  return mfxSts;

} // mfxStatus MFXVideoVPPDeinterlace::SetParam( mfxExtBuffer* pHint )


mfxStatus MFXVideoVPPDeinterlace::Close(void)
{
  mfxStatus mfxSts = MFX_ERR_NONE, localSts;

  VPP_CHECK_NOT_INITIALIZED;

  // ITC
  if( m_itcState.isInited )
  {
    mfxFrameAllocResponse response;
    response.mids = m_itcState.outBuf.mids;
    response.NumFrameActual = 1;
    localSts = m_core->FreeFrames( &response );
    VPP_CHECK_STS_CONTINUE( localSts, mfxSts );

    m_itcState.outBuf.mids     = NULL;
    m_itcState.outBuf.Surface1.Data.Locked = 0;
    ippsZero_8u( (Ipp8u*)&m_itcState, sizeof(sITCState) );

    m_itcState.isInited = 0;//double check
  }


  // DI
  {
    //NV12
    {
      // Y
      if( m_diState.pDIBlendStateY )
      { // return sts ignored
        ippiDeinterlaceBlendFree_8u_C1( m_diState.pDIBlendStateY );
        m_diState.pDIBlendStateY = NULL;
      }
      // UV
      if( m_diState.pDIBlendStateUV )
      { // return sts ignored
        ippiDeinterlaceBlendFree_8u_C2( m_diState.pDIBlendStateUV );
        m_diState.pDIBlendStateUV = NULL;
      }
    }

    mfxU8  refIndex = 0;

    for( refIndex = 0; refIndex < 2; refIndex++ )
    {
      if( m_diState.refList[refIndex] )
      {
        localSts = m_core->DecreaseReference(&(m_diState.refList[refIndex]->Data));
        VPP_CHECK_STS_CONTINUE( localSts, mfxSts );
        m_diState.refList[refIndex] = NULL;
      }

      m_diState.bSyncRefList[refIndex] = false;
    }
    m_diState.numProcessedFrames = 0;
  }

  /* readiness state */
  // sync
  m_syncReadinessState.numProcessedFrames = 0;
  m_syncReadinessState.TimeStamp          = 0;
  m_syncReadinessState.frameOrder         = 0;
  m_syncReadinessState.bReadyOutput       = false;
  m_syncReadinessState.delayedTimeStamp_AdvancedMode = 0;

  //process
  m_processReadinessState.numProcessedFrames = 0;
  m_processReadinessState.TimeStamp          = 0;
  m_processReadinessState.frameOrder         = 0;
  m_processReadinessState.bReadyOutput       = false;
  m_processReadinessState.delayedTimeStamp_AdvancedMode = 0;

  /* mode of deinterlace */
  m_mode = VPP_SIMPLE_DEINTERLACE;

  m_deltaTimeStamp = 0;

  VPP_CLEAN;

  return mfxSts;

} // mfxStatus MFXVideoVPPDeinterlace::Close(void)


mfxStatus MFXVideoVPPDeinterlace::Reset(mfxVideoParam *par)
{
  mfxStatus mfxSts;

  MFX_CHECK_NULL_PTR1(par);

  VPP_CHECK_NOT_INITIALIZED;

  // in case of DI, MUST BE in == out excluding PicStruct.
  // so, some check is redundant.
  // But m_errPrtctState contains correct info.

  if(m_errPrtctState.In.FourCC != par->vpp.In.FourCC || m_errPrtctState.Out.FourCC != par->vpp.Out.FourCC)
  {
    return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  if( m_errPrtctState.Out.Width != par->vpp.Out.Width || m_errPrtctState.Out.Height != par->vpp.Out.Height )
  {
      return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  if( m_errPrtctState.In.Width != par->vpp.In.Width || m_errPrtctState.In.Height != par->vpp.In.Height )
  {
      return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  // simple checking wo analysis
  if( (m_errPrtctState.In.PicStruct != par->vpp.In.PicStruct) || (m_errPrtctState.Out.PicStruct != par->vpp.Out.PicStruct) )
  {
    return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  // ITC algorithm
  if( m_itcState.outBuf.Surface1.Data.Locked )
  {
      mfxSts = m_core->DecreaseReference( &(m_itcState.outBuf.Surface1.Data) );
      MFX_CHECK_STS( mfxSts );
  }

  // DI algorithm
  m_diState.numProcessedFrames = 0;
  mfxU8 refIndex;

  for( refIndex = 0; refIndex < 2; refIndex++ )
  {
    if( m_diState.refList[refIndex] )
    {
      mfxSts = m_core->DecreaseReference( &(m_diState.refList[refIndex]->Data) );
      MFX_CHECK_STS( mfxSts );
      m_diState.refList[refIndex] = NULL;
    }

    m_diState.bSyncRefList[refIndex] = false;
  }

  /* readiness state */
  // sync
  m_syncReadinessState.numProcessedFrames = 0;
  m_syncReadinessState.TimeStamp          = 0;
  m_syncReadinessState.frameOrder         = 0;
  m_syncReadinessState.bReadyOutput       = false;
  m_syncReadinessState.delayedTimeStamp_AdvancedMode = 0;

  //process
  m_processReadinessState.numProcessedFrames = 0;
  m_processReadinessState.TimeStamp          = 0;
  m_processReadinessState.frameOrder         = 0;
  m_processReadinessState.bReadyOutput       = false;
  m_processReadinessState.delayedTimeStamp_AdvancedMode = 0;

  //m_deltaTimeStamp = (Out->FrameRateExtD * 90000L)/ Out->FrameRateExtN;;

  VPP_RESET;

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::Reset(mfxVideoParam *par)


mfxStatus MFXVideoVPPDeinterlace::RunFrameVPP(mfxFrameSurface1 *in,
                                              mfxFrameSurface1 *out,
                                              InternalParam* pParam)
{
  // code error
  mfxStatus mfxSts = MFX_ERR_NONE;

  VPP_CHECK_NOT_INITIALIZED;

  if( NULL == pParam )
  {
      return MFX_ERR_NULL_PTR;
  }

  switch ( m_errPrtctState.In.FourCC )// in==out(FourCC) for DI
  {
    case MFX_FOURCC_NV12:

      if( VPP_INVERSE_TELECINE == m_mode )
      {
        mfxSts = itc_NV12(in, out, &m_itcState, &m_diState, pParam);
      }
      else
      {
        mfxSts = di_Processing(in, out, &m_diState, pParam);
      }

      break;

    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  if( !m_errPrtctState.isFirstFrameProcessed )
  {
      m_errPrtctState.isFirstFrameProcessed = true;
  }

  pParam->outPicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  /* every frame */
  IncrementNumRealProcessedFrames();

  return mfxSts;

} // mfxStatus MFXVideoVPPDeinterlace::RunFrameVPP(...)


mfxStatus MFXVideoVPPDeinterlace::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
{
  mfxStatus mfxSts = MFX_ERR_NONE;
  IppStatus ippSts = ippStsNoErr;
  mfxU16    srcW   = 0, srcH = 0;
  mfxU16    dstW   = 0, dstH = 0;

  MFX_CHECK_NULL_PTR2( In, Out );

  VPP_CHECK_MULTIPLE_INIT;

  /* IN */
  VPP_GET_REAL_WIDTH(In, srcW);
  VPP_GET_REAL_HEIGHT(In, srcH);

  /* OUT */
  VPP_GET_REAL_WIDTH(Out, dstW);
  VPP_GET_REAL_HEIGHT(Out, dstH);

  //------------------------------
  /* robustness check */
  if( srcW != dstW || srcH != dstH )
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  if( In->FourCC != Out->FourCC )
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  if( !(In->PicStruct & MFX_PICSTRUCT_FIELD_TFF) &&
      !(In->PicStruct & MFX_PICSTRUCT_FIELD_BFF) &&
      !(In->PicStruct == MFX_PICSTRUCT_UNKNOWN))
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  if( !(Out->PicStruct & MFX_PICSTRUCT_PROGRESSIVE) )
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  mfxSts = CheckFrameRate( In, Out );
  MFX_CHECK_STS( mfxSts );

  /* save init params to prevent core crash */
  m_errPrtctState.In  = *In;
  m_errPrtctState.Out = *Out;

  bool bIsPredefinedDI = true; //(In->PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF)) ? true : false;

  /* select algorithm */
  if( bIsPredefinedDI &&
      IsFrameRatesCorrespondITC(In->FrameRateExtN,
                                In->FrameRateExtD,
                                Out->FrameRateExtN,
                                Out->FrameRateExtD) )
  {
      m_mode = VPP_INVERSE_TELECINE;
  }
  else if( bIsPredefinedDI &&
           IsFrameRatesCorrespondMode30i60p(
                In->FrameRateExtN,
                In->FrameRateExtD,
                Out->FrameRateExtN,
                Out->FrameRateExtD) )
  {
      m_mode = VPP_DEINTERLACE_MODE30i60p;
  }
  else
  {
      m_mode = VPP_SIMPLE_DEINTERLACE;
  }

  ippsZero_8u( (Ipp8u*)&m_itcState, sizeof(sITCState) );

  /* temporary solution for ref frame of ITC */
  {
    mfxFrameAllocRequest  request;
    mfxFrameAllocResponse response;

    /* surfaces approach */
    request.Info        = *Out;
    request.NumFrameMin = request.NumFrameSuggested = 1;
    request.Type        = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_FROM_VPPIN;

    mfxSts = m_core->AllocFrames(&request, &response);
    MFX_CHECK_STS( mfxSts );

    m_itcState.outBuf.mids = response.mids;
    m_itcState.outBuf.Surface1.Info = request.Info;

    m_itcState.isInited++;
  }

  /* DI */
  {
    IppiSize planeSize = {m_errPrtctState.In.Width, m_errPrtctState.In.Height};
    int blendThresh[2] = {5, 9};
    double blendConstants[2] = {0.3, 0.7};

    /* Y */
    ippSts = ippiDeinterlaceBlendInitAlloc_8u_C1(planeSize, blendThresh, blendConstants,
                                                 &(m_diState.pDIBlendStateY));
    VPP_CHECK_IPP_STS( ippSts );

    /* FourCC checking */
    switch ( m_errPrtctState.In.FourCC )
    {
      case MFX_FOURCC_NV12:

        /* UV */
        planeSize.height = planeSize.height >> 1;
        planeSize.width  = planeSize.width  >> 1;

        ippSts = ippiDeinterlaceBlendInitAlloc_8u_C2(planeSize, blendThresh, blendConstants, &(m_diState.pDIBlendStateUV));

        VPP_CHECK_IPP_STS( ippSts );

        break;

      default:
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
  }

  m_diState.numProcessedFrames = 0;

  m_diState.refList[0] = NULL;
  m_diState.refList[1] = NULL;

  m_diState.bSyncRefList[0] = false;
  m_diState.bSyncRefList[1] = false;

  /* readiness state */
  // sync
  m_syncReadinessState.numProcessedFrames = 0;
  m_syncReadinessState.TimeStamp          = 0;
  m_syncReadinessState.frameOrder         = 0;
  m_syncReadinessState.bReadyOutput       = false;
  m_syncReadinessState.delayedTimeStamp_AdvancedMode = 0;

  //process
  m_processReadinessState.numProcessedFrames = 0;
  m_processReadinessState.TimeStamp          = 0;
  m_processReadinessState.frameOrder         = 0;
  m_processReadinessState.bReadyOutput       = false;
  m_processReadinessState.delayedTimeStamp_AdvancedMode = 0;

  m_deltaTimeStamp = (Out->FrameRateExtD * 90000L)/ Out->FrameRateExtN;

  VPP_INIT_SUCCESSFUL;

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::Init(mfxFrameInfo* In, mfxFrameInfo* Out)


// work buffer management - nothing for DI filter
mfxStatus MFXVideoVPPDeinterlace::GetBufferSize( mfxU32* pBufferSize )
{
  VPP_CHECK_NOT_INITIALIZED;

  MFX_CHECK_NULL_PTR1(pBufferSize);

  *pBufferSize = 0;

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::GetBufferSize( mfxU32* pBufferSize )


mfxStatus MFXVideoVPPDeinterlace::SetBuffer( mfxU8* pBuffer )
{
  mfxU32 bufSize = 0;
  mfxStatus sts;

  VPP_CHECK_NOT_INITIALIZED;

  sts = GetBufferSize( &bufSize );
  MFX_CHECK_STS( sts );

  // DI dosn't require work buffer, so pBuffer == NULL is OK
  if( bufSize )
  {
    MFX_CHECK_NULL_PTR1(pBuffer);
  }

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::SetBuffer( mfxU8* pBuffer )


mfxStatus MFXVideoVPPDeinterlace::CheckProduceOutputForMode60i60p(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
{
    /* [OUT == NULL] */
    if( NULL == out )
    {
        return MFX_ERR_NULL_PTR;
    }

    if( IsReadyOutput(MFX_REQUEST_FROM_VPP_CHECK) )
    {
        m_syncReadinessState.bReadyOutput = false;

        out->Data.TimeStamp = MFX_TIME_STAMP_INVALID;
        out->Data.FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;

        out->Info.AspectRatioW = m_syncReadinessState.defferedFrameInfo.AspectRatioW;
        out->Info.AspectRatioH = m_syncReadinessState.defferedFrameInfo.AspectRatioH;

        if( MFX_PICSTRUCT_PROGRESSIVE & m_syncReadinessState.defferedFrameInfo.PicStruct )
        {
          out->Info.PicStruct = m_syncReadinessState.defferedFrameInfo.PicStruct;
        }
        else
        {
          out->Info.PicStruct =  MFX_PICSTRUCT_PROGRESSIVE;//wo any decorate flags
        }

        return MFX_ERR_NONE;
    }

    if( in )
    {
        if( !m_diState.bSyncRefList[0] )
        {
            m_syncReadinessState.bReadyOutput = true;
            m_diState.bSyncRefList[0] = true;

            m_syncReadinessState.TimeStamp = out->Data.TimeStamp = in->Data.TimeStamp;//pass through
            m_syncReadinessState.frameOrder = out->Data.FrameOrder = in->Data.FrameOrder;
            m_syncReadinessState.defferedFrameInfo.AspectRatioW = out->Info.AspectRatioW = in->Info.AspectRatioW;
            m_syncReadinessState.defferedFrameInfo.AspectRatioH = out->Info.AspectRatioH = in->Info.AspectRatioH;
            if( MFX_PICSTRUCT_PROGRESSIVE & in->Info.PicStruct )
            {
              out->Info.PicStruct = m_syncReadinessState.defferedFrameInfo.PicStruct = in->Info.PicStruct;
            }
            else
            {
              out->Info.PicStruct =  m_syncReadinessState.defferedFrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;//wo any decorate flags
            }

            return MFX_ERR_MORE_SURFACE;
        }
        else if( !m_diState.bSyncRefList[1] )
        {
            m_diState.bSyncRefList[1] = true;
            m_syncReadinessState.bReadyOutput = false;

            m_syncReadinessState.TimeStamp = in->Data.TimeStamp;//1 frame delay
            m_syncReadinessState.frameOrder = in->Data.FrameOrder; 

            m_syncReadinessState.defferedFrameInfo.AspectRatioW = in->Info.AspectRatioW;
            m_syncReadinessState.defferedFrameInfo.AspectRatioH = in->Info.AspectRatioH;
            m_syncReadinessState.defferedFrameInfo.PicStruct = in->Info.PicStruct;
            return MFX_ERR_MORE_DATA;
        }
        else
        {
            m_syncReadinessState.bReadyOutput = true;
            m_diState.bSyncRefList[0] = true;
            m_diState.bSyncRefList[1] = true;

            // delayed path through
            out->Data.TimeStamp = m_syncReadinessState.TimeStamp;
            //m_syncReadinessState.delayedTimeStamp_AdvancedMode = in->Data.TimeStamp;
            m_syncReadinessState.TimeStamp = in->Data.TimeStamp;
            out->Data.FrameOrder = m_syncReadinessState.frameOrder;
            m_syncReadinessState.frameOrder = in->Data.FrameOrder;

            out->Info.AspectRatioW = m_syncReadinessState.defferedFrameInfo.AspectRatioW;
            out->Info.AspectRatioH = m_syncReadinessState.defferedFrameInfo.AspectRatioH;

            if( MFX_PICSTRUCT_PROGRESSIVE & m_syncReadinessState.defferedFrameInfo.PicStruct )
            {
              out->Info.PicStruct = m_syncReadinessState.defferedFrameInfo.PicStruct;
            }
            else
            {
              out->Info.PicStruct =  MFX_PICSTRUCT_PROGRESSIVE;//wo any decorate flags
            }

            m_syncReadinessState.defferedFrameInfo.AspectRatioW = in->Info.AspectRatioW;
            m_syncReadinessState.defferedFrameInfo.AspectRatioH = in->Info.AspectRatioH;
            m_syncReadinessState.defferedFrameInfo.PicStruct    = in->Info.PicStruct;

            return MFX_ERR_MORE_SURFACE;
        }
    }

    if( !IsReadyOutput(MFX_REQUEST_FROM_VPP_CHECK) && (NULL == in) )
    {
        if( !m_diState.bSyncRefList[0] )
        {
            return MFX_ERR_MORE_DATA;
        }
        else
        {
            m_syncReadinessState.bReadyOutput = true;
            m_diState.bSyncRefList[0] = false;

            out->Data.TimeStamp = m_syncReadinessState.TimeStamp;
            out->Data.FrameOrder = m_syncReadinessState.frameOrder;
            out->Info.AspectRatioW = m_syncReadinessState.defferedFrameInfo.AspectRatioW;
            out->Info.AspectRatioH = m_syncReadinessState.defferedFrameInfo.AspectRatioH;
            if( MFX_PICSTRUCT_PROGRESSIVE & m_syncReadinessState.defferedFrameInfo.PicStruct )
            {
              out->Info.PicStruct = m_syncReadinessState.defferedFrameInfo.PicStruct;
            }
            else
            {
              out->Info.PicStruct =  MFX_PICSTRUCT_PROGRESSIVE;//wo any decorate flags
            }

            return MFX_ERR_MORE_SURFACE;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::CheckProduceOutputForMode60i60p(


// function is called from sync part of VPP only
mfxStatus MFXVideoVPPDeinterlace::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
{
  if( VPP_DEINTERLACE_MODE30i60p == m_mode )
  {
      return CheckProduceOutputForMode60i60p(in, out);
  }

  /* [OUT == NULL] */
  if( NULL == out )
  {
    return MFX_ERR_NULL_PTR;
  }

  /* [IN == NULL] */
  if( NULL == in )
  {
    //if( VPP_INVERSE_TELECINE == m_mode )
    //{
    //    return MFX_ERR_MORE_DATA;//in case of ITC VPP doesn't have buffered frame
    //}

    if( m_syncReadinessState.numProcessedFrames >= 2 )
    {
      //DI will push out buffered frame
      out->Data.TimeStamp = m_syncReadinessState.TimeStamp;
      out->Data.FrameOrder = m_syncReadinessState.frameOrder;
      m_syncReadinessState.numProcessedFrames = 0;

      out->Info.AspectRatioW = m_syncReadinessState.defferedFrameInfo.AspectRatioW;
      out->Info.AspectRatioH = m_syncReadinessState.defferedFrameInfo.AspectRatioH;

      if( MFX_PICSTRUCT_PROGRESSIVE & m_syncReadinessState.defferedFrameInfo.PicStruct )
      {
          out->Info.PicStruct = m_syncReadinessState.defferedFrameInfo.PicStruct;
      }
      else
      {
          out->Info.PicStruct =  MFX_PICSTRUCT_PROGRESSIVE;//wo any decorate flags
      }

      return MFX_ERR_NONE;
    }
    else
    {
      return MFX_ERR_MORE_DATA;
    }
  }

  /* ****************************************** */
  /* normal processing. in != NULL, out != NULL */
  /* ****************************************** */
  {
    if( 0 == m_syncReadinessState.numProcessedFrames )//first frame processed by simple DI
    {
        m_syncReadinessState.numProcessedFrames++;

        out->Data.TimeStamp = in->Data.TimeStamp;//pass through
        out->Data.FrameOrder = in->Data.FrameOrder;

        out->Info.AspectRatioW = in->Info.AspectRatioW;
        out->Info.AspectRatioH = in->Info.AspectRatioH;
        if( MFX_PICSTRUCT_PROGRESSIVE & in->Info.PicStruct )
        {
            out->Info.PicStruct = in->Info.PicStruct;
        }
        else
        {
            out->Info.PicStruct =  MFX_PICSTRUCT_PROGRESSIVE;//wo any decorate flags
        }

        if( VPP_INVERSE_TELECINE == m_mode )
        {
            m_syncReadinessState.TimeStamp = in->Data.TimeStamp;
        }

        return MFX_ERR_NONE;
    }

    if( 1 == m_syncReadinessState.numProcessedFrames )
    {
        m_syncReadinessState.numProcessedFrames++;

        if( VPP_INVERSE_TELECINE != m_mode )
        {
            m_syncReadinessState.defferedFrameInfo = in->Info; // 1 frame delay
            m_syncReadinessState.TimeStamp = in->Data.TimeStamp;//1 frame delay
            m_syncReadinessState.frameOrder= in->Data.FrameOrder;
        }

        return MFX_ERR_MORE_DATA;
    }

    if( VPP_INVERSE_TELECINE == m_mode )
    {
        if(0 == (m_syncReadinessState.numProcessedFrames + 1) % VPP_ITC_FRAME_SKIP)
        {
            m_syncReadinessState.numProcessedFrames++;
            return MFX_ERR_MORE_DATA;
        }
        else
        {
            m_syncReadinessState.numProcessedFrames++;

            /*if( MFX_TIME_STAMP_INVALID == m_syncReadinessState.TimeStamp )
            {
                out->Data.TimeStamp = MFX_TIME_STAMP_INVALID;
            }
            else*/
            {
                // it is way to produce smooth output PTS.
                //m_syncReadinessState.TimeStamp += m_deltaTimeStamp;
                // but since ISV3 decided using "path through" approach. (ITC hase 1 frame delay)
                out->Data.TimeStamp = m_syncReadinessState.TimeStamp;
                m_syncReadinessState.TimeStamp = in->Data.TimeStamp;
                out->Data.FrameOrder = m_syncReadinessState.frameOrder;
                m_syncReadinessState.frameOrder = in->Data.FrameOrder;
            }

            //pass through
            out->Info.PicStruct    = MFX_PICSTRUCT_PROGRESSIVE;
            out->Info.AspectRatioW = in->Info.AspectRatioW; // no matter delay in case of ITC
            out->Info.AspectRatioH = in->Info.AspectRatioH;

            return MFX_ERR_NONE;
        }
    }

    /* ****************************************** */
    /* regular DI processing. frames > 2          */
    /* ****************************************** */
    //correct for DI only
    out->Data.TimeStamp = m_syncReadinessState.TimeStamp;
    m_syncReadinessState.TimeStamp = in->Data.TimeStamp;
    out->Data.FrameOrder = m_syncReadinessState.frameOrder;
    m_syncReadinessState.frameOrder = in->Data.FrameOrder;

    if( MFX_PICSTRUCT_PROGRESSIVE & m_syncReadinessState.defferedFrameInfo.PicStruct )
    {
        out->Info.PicStruct    = m_syncReadinessState.defferedFrameInfo.PicStruct;
    }
    else
    {
        out->Info.PicStruct =  MFX_PICSTRUCT_PROGRESSIVE;//wo any decorate flags
    }
    out->Info.AspectRatioW = m_syncReadinessState.defferedFrameInfo.AspectRatioW; // no matter delay in case of ITC
    out->Info.AspectRatioH = m_syncReadinessState.defferedFrameInfo.AspectRatioH;

    m_syncReadinessState.defferedFrameInfo.PicStruct    = in->Info.PicStruct;
    m_syncReadinessState.defferedFrameInfo.AspectRatioW = in->Info.AspectRatioW;
    m_syncReadinessState.defferedFrameInfo.AspectRatioH = in->Info.AspectRatioH;

    m_syncReadinessState.numProcessedFrames++;
  }

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::CheckProduceOutput(...)


bool MFXVideoVPPDeinterlace::IsReadyOutput( mfxRequestType requestType )
{
    sReadinessState readyState = m_processReadinessState;

    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
      readyState = m_syncReadinessState;
    }

    return ( readyState.bReadyOutput ) ? true : false;

} // bool MFXVideoVPPDeinterlace::IsReadyOutput( mfxRequestType requestType )


bool MFXVideoVPPDeinterlace::IsSkippedFrame( mfxU32 frame )
{
    if( (VPP_INVERSE_TELECINE == m_mode)  && (0 == ( (frame + 1) % VPP_ITC_FRAME_SKIP)))
    {
        return true;
    }

    return false;

} // bool MFXVideoVPPDeinterlace::IsSkippedFrame( mfxU32 frame )


mfxStatus MFXVideoVPPDeinterlace::IncrementNumRealProcessedFrames( void )
{
    m_processReadinessState.numProcessedFrames++;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::IncrementNumRealProcessedFrames( void )

/* ******************************************************************** */
/*                 implementation of algorithms [DI]                    */
/* ******************************************************************** */

mfxStatus MFXVideoVPPDeinterlace::di_AdvancedProcessing( mfxFrameSurface1* in,
                                                         mfxFrameSurface1* out,
                                                         mfxU16 picStruct)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxFrameSurface1 localRefSurface;
    bool bLockedOutBuf = false;

    if( NULL == out )
    {
        return MFX_ERR_NULL_PTR;
    }

    if( IsReadyOutput(MFX_REQUEST_FROM_VPP_PROCESS) )
    {
        m_processReadinessState.bReadyOutput = false;

        if( NULL == m_itcState.outBuf.Surface1.Data.Y )
        {
            mfxSts = m_core->LockFrame(m_itcState.outBuf.mids[0], &(m_itcState.outBuf.Surface1.Data));
            MFX_CHECK_STS( mfxSts );
            bLockedOutBuf = true;
        }

        CopyCrop( out, &(m_itcState.outBuf.Surface1));

        mfxSts = SurfaceCopy_ROI(out, &(m_itcState.outBuf.Surface1));//using ITC out buffer. in is correct
        MFX_CHECK_STS( mfxSts );

        if( bLockedOutBuf )
        {
            mfxSts = m_core->UnlockFrame(m_itcState.outBuf.mids[0], &(m_itcState.outBuf.Surface1.Data));
            MFX_CHECK_STS( mfxSts );
            bLockedOutBuf = false;
        }

        return MFX_ERR_NONE;
    }

    if( in )
    {
        if( NULL == m_diState.refList[0] ) // first frame processing
        {
            mfxSts = di_LQ_dispatcher( in, out, picStruct );
            MFX_CHECK_STS( mfxSts );

            m_diState.refList[0] = in;
            m_diState.refPicStructList[0] = picStruct;

            mfxSts = m_core->IncreaseReference( &(m_diState.refList[0]->Data) );
            MFX_CHECK_STS( mfxSts );

            m_diState.numProcessedFrames++;

            m_processReadinessState.bReadyOutput = true;

            return MFX_ERR_MORE_SURFACE;
        }
        else if( NULL == m_diState.refList[1] ) // second frame processing (delay)
        {
            // surfaces
            m_diState.refList[1] = m_diState.refList[0];
            m_diState.refList[0] = in;
            // picstruct
             m_diState.refPicStructList[1] = m_diState.refPicStructList[0];
            m_diState.refPicStructList[0]  = picStruct;

            mfxSts = m_core->IncreaseReference( &(m_diState.refList[0]->Data) );
            MFX_CHECK_STS( mfxSts );

            m_diState.numProcessedFrames++;

            m_processReadinessState.bReadyOutput = false;

            return MFX_ERR_MORE_DATA;
        }
        else // normal processing: deinterlace
        {
            if( IsROIConstant(m_diState.refList[1], m_diState.refList[0], in) )
            {
                mfxSts = di_HQ_dispatcher( m_diState.refList[1], m_diState.refList[0], in,
                                           out,
                                           m_diState.refPicStructList[0]);
                MFX_CHECK_STS( mfxSts );
            }
            else
            {
                mfxSts = di_LQ_dispatcher( in, out, picStruct ); // use BOB instead of adv DI
                MFX_CHECK_STS( mfxSts );

                // prework for next output
                if( NULL == m_itcState.outBuf.Surface1.Data.Y )
                {
                    mfxSts = m_core->LockFrame(m_itcState.outBuf.mids[0], &(m_itcState.outBuf.Surface1.Data));
                    MFX_CHECK_STS( mfxSts );
                    bLockedOutBuf = true;
                }

                CopyCrop( out, &(m_itcState.outBuf.Surface1));

                mfxSts = SurfaceCopy_ROI(&(m_itcState.outBuf.Surface1), out);//using ITC out buffer. in is correct
                MFX_CHECK_STS( mfxSts );

                if( bLockedOutBuf )
                {
                    mfxSts = m_core->UnlockFrame(m_itcState.outBuf.mids[0], &(m_itcState.outBuf.Surface1.Data));
                    MFX_CHECK_STS( mfxSts );
                    bLockedOutBuf = false;
                }
            }

            mfxSts = m_core->DecreaseReference( &(m_diState.refList[1]->Data) );
            MFX_CHECK_STS( mfxSts );

            // surfaces
            m_diState.refList[1] = m_diState.refList[0];
            m_diState.refList[0] = in;
            // picstruct
            m_diState.refPicStructList[1] = m_diState.refPicStructList[0];
            m_diState.refPicStructList[0] = picStruct;

            mfxSts = m_core->IncreaseReference( &(m_diState.refList[0]->Data) );
            MFX_CHECK_STS( mfxSts );

            m_diState.numProcessedFrames++;

            m_processReadinessState.bReadyOutput = true;

            return MFX_ERR_MORE_SURFACE;
        }
    }

    if( !IsReadyOutput(MFX_REQUEST_FROM_VPP_PROCESS) && (NULL == in) )
    {
        bool  bLockedRef = false;

        if(NULL == m_diState.refList[0])
        {
            return MFX_ERR_MORE_DATA;
        }
        else
        {
            if (m_diState.refList[0]->Data.Y == NULL)
            {
                MFX_CHECK_STS( m_core->LockExternalFrame(m_diState.refList[0]->Data.MemId, &(localRefSurface.Data)) );
                localRefSurface.Info = m_diState.refList[0]->Info;
                bLockedRef = true;
            }
            else
            {
                localRefSurface.Data = m_diState.refList[0]->Data;
                localRefSurface.Info = m_diState.refList[0]->Info;
            }

            mfxSts = di_LQ_dispatcher( &(localRefSurface), out, m_diState.refPicStructList[0] );
            MFX_CHECK_STS( mfxSts );

            if (bLockedRef)
            {
                MFX_CHECK_STS( m_core->UnlockExternalFrame(m_diState.refList[0]->Data.MemId, &(localRefSurface.Data)) );
                bLockedRef = false;
            }

            if( m_diState.refList[1] )
            {
                MFX_CHECK_STS( m_core->DecreaseReference( &(m_diState.refList[1]->Data) ) );
            }

            m_diState.refList[1] = m_diState.refList[0];//will be decreased in Close()
            m_diState.refList[0] = NULL;//in

            m_processReadinessState.bReadyOutput = true;

            return MFX_ERR_MORE_SURFACE;
        }
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;

} // mfxStatus MFXVideoVPPDeinterlace::di_AdvancedProcessing(...)


mfxStatus MFXVideoVPPDeinterlace::di_Processing( mfxFrameSurface1* in,
                                                 mfxFrameSurface1* out,
                                                 sDIState* pState,
                                                 InternalParam* pParam)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxFrameSurface1 localRefSurface;

    if( VPP_DEINTERLACE_MODE30i60p == m_mode )
    {
        mfxSts = di_AdvancedProcessing(in, out, pParam->inPicStruct);

        return mfxSts;
    }

    /* ************************************************** */
    /*   if DI instead of ITC out can be NULL: debug only */
    /* ************************************************** */
    /*if( NULL == out && !IsSkippedFrame( m_processReadinessState.numProcessedFrames ) )
    {
        return MFX_ERR_NULL_PTR;
    }*/

    /* ********************************************* */
    /*                push buffered frame            */
    /* ********************************************* */
    if( NULL == in )
    {
        bool  bLockedRef = false;
        if(pState->refList[0]) //DI by simple method
        {
            if (pState->refList[0]->Data.Y == NULL)
            {
                MFX_CHECK_STS( m_core->LockExternalFrame(pState->refList[0]->Data.MemId, &(localRefSurface.Data)) );
                localRefSurface.Info = pState->refList[0]->Info;
                bLockedRef = true;
            }
            else
            {
                localRefSurface.Data = pState->refList[0]->Data;
                localRefSurface.Info = pState->refList[0]->Info;
            }

            mfxSts = di_LQ_dispatcher( &(localRefSurface), out, pState->refPicStructList[0]);
            MFX_CHECK_STS( mfxSts );

            if (bLockedRef)
            {
                MFX_CHECK_STS( m_core->UnlockExternalFrame(pState->refList[0]->Data.MemId, &(localRefSurface.Data)) );
                bLockedRef = false;
            }

            if( pState->refList[1] )
            {
                MFX_CHECK_STS( m_core->DecreaseReference( &(pState->refList[1]->Data) ) );
            }

            pState->refList[1] = pState->refList[0];//will be decreased in Close()
            pState->refList[0] = in;//NULL

            pParam->outTimeStamp = pState->prevTimeStamp;
            return mfxSts;
        }
        else
        {
            return MFX_ERR_MORE_DATA;
        }
    }

    /* ********************************************* */
    /*      process 1 frame by simple method         */
    /* ********************************************* */
    if( 0 == pState->numProcessedFrames )
    {
        mfxSts = di_LQ_dispatcher( in, out, pParam->inPicStruct );
        MFX_CHECK_STS( mfxSts );

        pState->refList[0] = in;
        pState->refPicStructList[0] = pParam->inPicStruct;

        mfxSts = m_core->IncreaseReference( &(pState->refList[0]->Data) );
        MFX_CHECK_STS( mfxSts );

        pState->numProcessedFrames++;
        pParam->outTimeStamp = pParam->inTimeStamp;

        return mfxSts;
    }

    /* ********************************************* */
    /*             frame buffering only              */
    /* ********************************************* */
    if( 1 == pState->numProcessedFrames )
    {
        // surfaces
        pState->refList[1] = pState->refList[0];
        pState->refList[0] = in;
        // picstruct
        pState->refPicStructList[1] = pState->refPicStructList[0];
        pState->refPicStructList[0] = pParam->inPicStruct;

        mfxSts = m_core->IncreaseReference( &(pState->refList[0]->Data) );
        MFX_CHECK_STS( mfxSts );

        pState->numProcessedFrames++;
        pState->prevTimeStamp = pParam->inTimeStamp;

        return MFX_ERR_MORE_DATA;
    }

    /* ********************************************* */
    /*             normal processing: skip frame     */
    /* ********************************************* */
    if( IsSkippedFrame( m_processReadinessState.numProcessedFrames ) )
    {
        mfxSts = m_core->DecreaseReference( &(pState->refList[1]->Data) );
        MFX_CHECK_STS( mfxSts );

        // surfaces
        pState->refList[1] = pState->refList[0];
        pState->refList[0] = in;
        // picstruct
        pState->refPicStructList[1] = pState->refPicStructList[0];
        pState->refPicStructList[0] = pParam->inPicStruct;

        mfxSts = m_core->IncreaseReference( &(in->Data) );
        MFX_CHECK_STS( mfxSts );

        return MFX_ERR_MORE_DATA;
    }

    /* ********************************************* */
    /*        normal processing: deinterlace         */
    /* ********************************************* */
    {
        if( IsROIConstant(m_diState.refList[1], m_diState.refList[0], in) )
        {
            mfxSts = di_HQ_dispatcher( m_diState.refList[1], m_diState.refList[0], in,
                                       out,
                                       m_diState.refPicStructList[0]);
            MFX_CHECK_STS( mfxSts );
        }
        else
        {
            mfxSts = di_LQ_dispatcher( in, out, pParam->inPicStruct ); // use BOB instead of adv DI
            MFX_CHECK_STS( mfxSts );
        }

        // update
        mfxSts = m_core->DecreaseReference( &(pState->refList[1]->Data) );
        MFX_CHECK_STS( mfxSts );

        // surfaces
        pState->refList[1] = pState->refList[0];
        pState->refList[0] = in;
        // picstruct
        pState->refPicStructList[1] = pState->refPicStructList[0];
        pState->refPicStructList[0] = pParam->inPicStruct;

        mfxSts = m_core->IncreaseReference( &(pState->refList[0]->Data) );
        MFX_CHECK_STS( mfxSts );
        
        pParam->outTimeStamp = pState->prevTimeStamp;
        pState->prevTimeStamp = pParam->inTimeStamp;
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::di_Processing(...)


mfxStatus MFXVideoVPPDeinterlace::di_LQ_NV12( mfxFrameSurface1* in, mfxFrameSurface1* out, mfxU16 picStruct)
{

  // priority(PROGRESSIVE) > priority(TFF/BFF)
  if( MFX_PICSTRUCT_PROGRESSIVE & picStruct )
  {
      mfxStatus mfxSts = SurfaceCopy_ROI(out, in);
      MFX_CHECK_STS( mfxSts );
  }
  else
  {
      IppStatus sts     = ippStsNotSupportedModeErr;
      IppiSize  roiSize;

      mfxU32  inOffset0 = 0, inOffset1  = 0;
      mfxU32  outOffset0= 0, outOffset1 = 0;
      mfxU32  cropX = 0, cropY = 0;

      mfxFrameData* inData  = &in->Data;
      mfxFrameData* outData = &out->Data;
      mfxFrameInfo* inInfo  = &in->Info;
      mfxFrameInfo* outInfo = &out->Info;

      VPP_GET_CROPX(inInfo,  cropX);
      VPP_GET_CROPY(inInfo,  cropY);
      inOffset0  = cropX  + cropY*inData->Pitch;
      inOffset1  = (cropX) + (cropY >> 1)*(inData->Pitch);

      VPP_GET_CROPX(outInfo, cropX);
      VPP_GET_CROPY(outInfo, cropY);
      outOffset0  = cropX        + cropY*outData->Pitch;
      outOffset1  = (cropX) + (cropY >> 1)*(outData->Pitch);
      //-----------------------------------------

      const mfxU8* pSrc[2] = {(mfxU8*)inData->Y + inOffset0,
                              (mfxU8*)inData->UV + inOffset1};

      mfxI32 pSrcStep[2] = {inData->Pitch, inData->Pitch};

      mfxU8* pDst[2]   = {(mfxU8*)outData->Y + outOffset0,
                          (mfxU8*)outData->UV + outOffset1};

      mfxI32 pDstStep[2] = {outData->Pitch, outData->Pitch};

      /* common part */
      VPP_GET_WIDTH(inInfo,  roiSize.width);
      VPP_GET_HEIGHT(inInfo, roiSize.height);

      // in case of interlace content height must be mulpiple 4
      roiSize.height = ((roiSize.height + 3) >> 2) << 2;
      /* Y */
      sts = ippiDeinterlaceFilterTriangle_8u_C1R(pSrc[0], pSrcStep[0],
        pDst[0], pDstStep[0],
        roiSize,
        128,
        IPP_LOWER | IPP_UPPER | IPP_CENTER);

      VPP_CHECK_IPP_STS(sts);

      /* UV common */
      /* in the nv12 case UV width is the same as for Y component*/
      roiSize.height >>= 1;

      /* UV */
      /* ippiDeinterlaceFilterTriangle_8u_C1R can be used for nv12 deinterlace with no changes */
      sts = ippiDeinterlaceFilterTriangle_8u_C1R(pSrc[1], pSrcStep[1],
                                                 pDst[1], pDstStep[1],
                                                 roiSize,
                                                 128,
                                                 IPP_LOWER | IPP_UPPER | IPP_CENTER);
      VPP_CHECK_IPP_STS(sts);
  }

  if( VPP_DEINTERLACE_MODE30i60p == m_mode )
  {
      bool bLockedOutBuf = false;
      mfxStatus mfxSts;

      if( NULL == m_itcState.outBuf.Surface1.Data.Y )
      {
          mfxSts = m_core->LockFrame(m_itcState.outBuf.mids[0], &(m_itcState.outBuf.Surface1.Data));
          MFX_CHECK_STS( mfxSts );
          bLockedOutBuf = true;
      }

      CopyCrop( out, &(m_itcState.outBuf.Surface1));

      mfxSts = SurfaceCopy_ROI( &(m_itcState.outBuf.Surface1), out);
      MFX_CHECK_STS( mfxSts );

      if( bLockedOutBuf )
      {
          mfxSts = m_core->UnlockFrame(m_itcState.outBuf.mids[0], &(m_itcState.outBuf.Surface1.Data));
          MFX_CHECK_STS( mfxSts );
          bLockedOutBuf = false;
      }
  }

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::di_LQ_NV12(...)


mfxStatus MFXVideoVPPDeinterlace::di_HQ_NV12(mfxFrameSurface1* ref1,
                                             mfxFrameSurface1* ref0,
                                             mfxFrameSurface1* in,
                                             mfxFrameSurface1* out,
                                             mfxU16 picStruct)
{
    IppiSize  roiSize;

    mfxU32  inOffset0 = 0, inOffset1  = 0;
    mfxU32  outOffset0= 0, outOffset1 = 0;

    mfxU16  cropX = 0, cropY = 0;

    mfxFrameData* inData  = &in->Data;
    mfxFrameData* outData = &out->Data;
    mfxFrameInfo* inInfo  = &in->Info;
    mfxFrameInfo* outInfo = &out->Info;

    bool          bLockedRefList[2] = {false, false};
    bool          bLockedInSurface  = false;

    mfxFrameSurface1 localRefSurfaces[2];
    mfxFrameSurface1 localInSurface;

    mfxStatus mfxSts = MFX_ERR_NONE;


    VPP_GET_CROPX(inInfo,  cropX);
    VPP_GET_CROPY(inInfo,  cropY);
    inOffset0  = cropX  + cropY*inData->Pitch;
    inOffset1  = (cropX) + (cropY >> 1)*(inData->Pitch);

    VPP_GET_CROPX(outInfo, cropX);
    VPP_GET_CROPY(outInfo, cropY);
    outOffset0  = cropX        + cropY*outData->Pitch;
    outOffset1  = (cropX) + (cropY >> 1)*(outData->Pitch);

    /* reference surfaces can be through MemId */
    if (ref0->Data.Y == NULL)
    {
        mfxSts = m_core->LockExternalFrame(ref0->Data.MemId, &(localRefSurfaces[0].Data));
        localRefSurfaces[0].Info = ref0->Info;
        MFX_CHECK_STS(mfxSts);
        bLockedRefList[0] = true;
    }
    else
    {
        localRefSurfaces[0].Data = ref0->Data;
        localRefSurfaces[0].Info = ref0->Info;
    }

    if (ref1->Data.Y == NULL)
    {
        mfxSts = m_core->LockExternalFrame(ref1->Data.MemId, &(localRefSurfaces[1].Data));
        localRefSurfaces[1].Info = ref1->Info;
        MFX_CHECK_STS(mfxSts);
        bLockedRefList[1] = true;
    }
    else
    {
        localRefSurfaces[1].Data = ref1->Data;
        localRefSurfaces[1].Info = ref1->Info;
    }
    if (in->Data.Y == NULL)
    {
        mfxSts = m_core->LockExternalFrame(in->Data.MemId, &(localInSurface.Data) );
        localInSurface.Info = in->Info;
        MFX_CHECK_STS(mfxSts);
        bLockedInSurface = true;
    }
    else
    {
        localInSurface.Data = in->Data;
        localInSurface.Info = in->Info;
    }
    //---------------------------------------------------

    const mfxU8* pSrc[2] = {(mfxU8*)localInSurface.Data.Y + inOffset0,
        (mfxU8*)localInSurface.Data.UV + inOffset1};

    mfxI32 pSrcStep[2] = {localInSurface.Data.Pitch, localInSurface.Data.Pitch};

    const mfxU8* pRef0[2] = {(mfxU8*)localRefSurfaces[0].Data.Y + inOffset0,
                             (mfxU8*)localRefSurfaces[0].Data.UV + inOffset1};
    mfxI32 pRef0Step[2] = {localRefSurfaces[0].Data.Pitch, localRefSurfaces[0].Data.Pitch};

    const mfxU8* pRef1[2] = {(mfxU8*)localRefSurfaces[1].Data.Y + inOffset0,
                             (mfxU8*)localRefSurfaces[1].Data.UV + inOffset1};
    mfxI32 pRef1Step[2] = {localRefSurfaces[1].Data.Pitch, localRefSurfaces[1].Data.Pitch};
    mfxU8* pDst[2]   = {(mfxU8*)outData->Y + outOffset0,
                       (mfxU8*)outData->UV + outOffset1};

    mfxI32 pDstStep[2] = {outData->Pitch,  outData->Pitch};

    int passIndexLast = (VPP_DEINTERLACE_MODE30i60p == m_mode) ? 2 : 1;

    for( int passIndex = 0; passIndex < passIndexLast; passIndex++ )
    {
        int copyField = 1;
        int topFirst  = ( MFX_PICSTRUCT_FIELD_TFF & picStruct ) ? 1 : 0;
        int topField  = (1 == topFirst) ? 0 : 1;
        bool bLockedOutBuf = false;

        VPP_GET_WIDTH(inInfo,  roiSize.width);
        VPP_GET_HEIGHT(inInfo, roiSize.height);

        // in case of interlace content height must be mulpiple 4
        roiSize.height = ((roiSize.height + 3) >> 2) << 2;
        if( passIndex > 0 )// in case of second pass DI should process second field
        {
            topField = (1 == topField) ? 0 : 1;

            if( NULL == m_itcState.outBuf.Surface1.Data.Y )
            {
                mfxSts = m_core->LockFrame(m_itcState.outBuf.mids[0], &(m_itcState.outBuf.Surface1.Data));
                MFX_CHECK_STS( mfxSts );
                bLockedOutBuf = true;
            }

            VPP_GET_CROPX(outInfo, cropX);
            VPP_GET_CROPY(outInfo, cropY);
            outOffset0  = cropX        + cropY*m_itcState.outBuf.Surface1.Data.Pitch;
            outOffset1  = (cropX) + (cropY >> 1)*(m_itcState.outBuf.Surface1.Data.Pitch);

            pDst[0] = (mfxU8*)m_itcState.outBuf.Surface1.Data.Y + outOffset0;
            pDst[1] = (mfxU8*)m_itcState.outBuf.Surface1.Data.UV + outOffset1;

            pDstStep[0] = m_itcState.outBuf.Surface1.Data.Pitch;
            pDstStep[1] = m_itcState.outBuf.Surface1.Data.Pitch;
        }

        /* Y */
        {
            const Ipp8u* pSrcPlane[3] = {pRef1[0], pRef0[0], pSrc[0]};
            IppiSize planeSize        = { roiSize.width, roiSize.height };
            IppStatus ippSts = ippStsNoErr;

            // priority(PROGRESSIVE) > priority(TFF/BFF)
            if( MFX_PICSTRUCT_PROGRESSIVE & picStruct )
            {
                ippSts = ippiCopy_8u_C1R(pRef0[0], pSrcStep[0], pDst[0], pDstStep[0], planeSize);
            }
            else
            {
                ippSts = ippiDeinterlaceBlend_8u_C1(pSrcPlane, pSrcStep[0],pDst[0], pDstStep[0], planeSize,
                                                topFirst, topField, copyField, m_diState.pDIBlendStateY);
            }
            VPP_CHECK_IPP_STS( ippSts );
        }

        /* UV */
        roiSize.height >>= 1;
        roiSize.width  >>= 1;
        {
            IppiSize planeSize        = { roiSize.width, roiSize.height };
            IppStatus ippSts = ippStsNoErr;

            IppvcFrameFieldFlag fieldFirst   = (0 == topFirst) ? IPPVC_BOTTOM_FIELD : IPPVC_TOP_FIELD;
            IppvcFrameFieldFlag fieldProcess = (IPPVC_BOTTOM_FIELD == fieldFirst) ? IPPVC_TOP_FIELD : IPPVC_BOTTOM_FIELD;
            IppBool             fieldCopy    = ippTrue;

            // priority(PROGRESSIVE) > priority(TFF/BFF)
            if( MFX_PICSTRUCT_PROGRESSIVE & picStruct )
            {
                IppiSize roiSizeUV = {roiSize.width << 1, roiSize.height}; // width(UV) == width(Y)
                ippSts = ippiCopy_8u_C1R(pRef0[1], pRef0Step[1], pDst[1],  pDstStep[1], roiSizeUV);
            }
            else
            {
                ippSts = ippiDeinterlaceBlend_8u_C2(
                                          pRef1[1], pRef1Step[1],
                                          pRef0[1], pRef0Step[1],
                                          pSrc[1],  pSrcStep[1],
                                          pDst[1],  pDstStep[1],

                                          planeSize,

                                          fieldFirst,
                                          fieldProcess,
                                          fieldCopy,

                                          m_diState.pDIBlendStateUV);
            }
            VPP_CHECK_IPP_STS( ippSts );
        }


        if( bLockedOutBuf )
        {
            mfxSts = m_core->UnlockFrame(m_itcState.outBuf.mids[0], &(m_itcState.outBuf.Surface1.Data));
            MFX_CHECK_STS( mfxSts );
            bLockedOutBuf = false;
        }
    }//end for(passIndex...
    //Unlock External Frames
    if (bLockedRefList[0])
    {
        MFX_CHECK_STS( m_core->UnlockExternalFrame(ref0->Data.MemId, &(localRefSurfaces[0].Data)) );
        bLockedRefList[0] = false;
    }

    if (bLockedRefList[1])
    {
        MFX_CHECK_STS( m_core->UnlockExternalFrame(ref1->Data.MemId, &(localRefSurfaces[1].Data)) );
        bLockedRefList[1] = false;
    }
    if (bLockedInSurface)
    {
        MFX_CHECK_STS( m_core->UnlockExternalFrame(in->Data.MemId, &(localInSurface.Data) ) );
        bLockedInSurface = false;
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::di_HQ_NV12(...)

/* ******************************************************************** */
/*                 implementation of dispatchers for DI                 */
/* ******************************************************************** */

mfxStatus MFXVideoVPPDeinterlace::di_LQ_dispatcher( mfxFrameSurface1* in, mfxFrameSurface1* out, mfxU16 picStruct)
{
    if( MFX_FOURCC_NV12 == m_errPrtctState.In.FourCC )
    {
        return di_LQ_NV12( in, out, picStruct);
    }
    /*else if( MFX_FOURCC_YV12 == m_errPrtctState.In.FourCC )
    {
        return di_LQ_YV12( in, out);
    }*/
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

} // mfxStatus MFXVideoVPPDeinterlace::di_LQ_dispatcher(...)


mfxStatus MFXVideoVPPDeinterlace::di_HQ_dispatcher( mfxFrameSurface1* ref1,
                                                    mfxFrameSurface1* ref0,
                                                    mfxFrameSurface1* in,
                                                    mfxFrameSurface1* out,
                                                    mfxU16 picStruct)
{
    if( MFX_FOURCC_NV12 == m_errPrtctState.In.FourCC )
    {
        return di_HQ_NV12( ref1, ref0, in, out, picStruct);
    }
    /*else if( MFX_FOURCC_YV12 == m_errPrtctState.In.FourCC )
    {
        return di_HQ_YV12( ref1, ref0, in, out);
    }*/
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

} // mfxStatus MFXVideoVPPDeinterlace::di_HQ_dispatcher(...)

/* ******************************************************************** */
/*                 implementation of algorithms [ITC]                   */
/* ******************************************************************** */

void itc_getStatistic(mfxU8 *pCT, mfxU8 *pCB, mfxI32 stepCur,
                      mfxU8 *pPT, mfxU8 *pPB, mfxI32 stepPrev,
                      mfxI32 h, mfxI32 w, mfxI32 *var)
{
    mfxI32 i, j;
    mfxI32 diff;
    mfxI32 j0, j1, j2, j3, j4, j5, j6;
    mfxI32 scale = h * w;

    if ((h <= 0) || (w <= 0))
    {
        return;
    }

    j0 = 0; j1 = 0; j2 = 0; j3 = 0; j4 = 0; j5 = 0; j6 = 0;

    for (i = 0; i < h; i++)
    {
        for (j = 0; j < w; j++)
        {
            diff = pCT[j] - pCT[j + stepCur]; // top field complexity measure
            j5 += diff * diff;

            diff = pCB[j] - pCB[j + stepCur]; // bottom field complexity measure
            j6 += diff * diff;

            diff = pCT[j] - pCB[j]; // current top with current bottom
            j0 += diff * diff;

            diff = pCT[j] - pPT[j]; // current top with previous top
            j1 += diff * diff;

            diff = pCB[j] - pPB[j]; // current bottom with previous bottom
            j2 += diff * diff;

            diff = pCT[j] - pPB[j]; // current top with previous bottom
            j3 += diff * diff;

            diff = pCB[j] - pPT[j]; // current bottom with previous top
            j4 += diff * diff;
        }
        pCT += stepCur;
        pCB += stepCur;
        pPT += stepPrev;
        pPB += stepPrev;
    }
    j0 = j0/scale; if (var[0] < j0) var[0] = j0;
    j1 = j1/scale; if (var[1] < j1) var[1] = j1;
    j2 = j2/scale; if (var[2] < j2) var[2] = j2;
    j3 = j3/scale; if (var[3] < j3) var[3] = j3;
    j4 = j4/scale; if (var[4] < j4) var[4] = j4;
    j5 = j5/scale; if (var[5] < j5) var[5] = j5;
    j6 = j6/scale; if (var[6] < j6) var[6] = j6;

} // void itc_getStatistic(...);


mfxStatus itc_CompareFourFields(mfxU8 *pCT, mfxU8 *pCB, mfxI32 stepCur,
                                mfxU8 *pPT, mfxU8 *pPB, mfxI32 stepPrev,
                                IppiSize roiSize, mfxI32 *var)
{
  Ipp32s i, j;
  Ipp32s width = roiSize.width, height = roiSize.height;

  for (i = 0; i < height - 8; i += 8)
  {
      for (j = 0; j < width - 7; j += 8)
      {
          itc_getStatistic(pCT+j, pCB+j, stepCur, pPT+j, pPB+j, stepPrev, 8, 8, var);
      }
      itc_getStatistic(pCT+j, pCB+j, stepCur, pPT+j, pPB+j, stepPrev, 8, width-j, var);
      pCT += 8*stepCur;
      pCB += 8*stepCur;
      pPT += 8*stepPrev;
      pPB += 8*stepPrev;
  }

  for (j = 0; j < width - 7; j += 8)
  {
      itc_getStatistic(pCT+j, pCB+j, stepCur, pPT+j, pPB+j, stepPrev, height-1-i, 8, var);
  }
  itc_getStatistic(pCT+j, pCB+j, stepCur, pPT+j, pPB+j, stepPrev, height-1-i, width-j, var);

  return MFX_ERR_NONE;

} // mfxStatus itc_CompareFourFields(...)


mfxStatus itc_Frame_NV12(const mfxU8 *pCFrame[2], mfxI32 stepC[2],
                         mfxU8 *pPFrame[2], mfxI32 stepP[2],
                         IppiSize roiSize, mfxI32 *var)
{
  mfxU8 *pYc, *pUVc;
  mfxU8 *pYp, *pUVp;
  mfxI32 i;
  IppiSize fieldSize;

  fieldSize.width = roiSize.width;
  fieldSize.height = roiSize.height >> 1;

  pYc = (mfxU8*)pCFrame[0]; pUVc = (mfxU8*)pCFrame[1];
  pYp = pPFrame[0]; pUVp = pPFrame[1];

  for (i = 0; i < VAR_ERR_SIZE; i++)
  {
    var[i] = 0;
  }

  itc_CompareFourFields(pYc, pYc + stepC[0], 2*stepC[0],
                          pYp, pYp + stepP[0], 2*stepP[0], fieldSize, var);

  //for (i = 0; i < VAR_ERR_SIZE; i++) {
    //    if (i < 7)
  //  var[i] /= fieldSize.width * fieldSize.height;
  //}

  return MFX_ERR_NONE;

} // mfxStatus itc_Frame_NV12(...)


mfxI32 itc_CheckPattern(mfxU32 cur_pat, mfxI32 hist_len, mfxI32 ind)
{
  mfxI32 i;
  mfxI32 len = cadence_frp_table[ind].period;
  mfxU32 pat = cadence_frp_table[ind].frp;
  mfxI32 msk = (1 << len*2) - 1;
  mfxU32 cpat = cur_pat;
  mfxI32 phase = -1;
  mfxI32 ilen;

  for (i = 0; i < len; i++)
  {
    if ((cpat & msk) == pat)
    {
      phase = i*2;
      break;
    }
    cpat >>= 2;
  }

  if (phase >= 0)
  {
    pat = pat << phase;
    pat = (pat & msk) | pat >> (len*2);
  }

  ilen = 0;
  cpat = cur_pat;
  for (i = 0; i + len <= hist_len; i += len)
  {
    cpat >>= 2*i;
    if ((cpat & msk) != pat)
    {
      phase = -1;
      break;
    }
    ilen += len;
  }
  if (phase >= 0)
  {
    if (hist_len - ilen != 0)
    {
      ilen = hist_len - ilen;
      msk = (1 << ilen*2) - 1;
      if ((cpat & msk) != (pat & msk))
      {
        phase = -1;
      }
    }
  }

  return phase;

} // mfxI32 itc_CheckPattern(mfxU32 cur_pat, mfxI32 hist_len, mfxI32 ind);


mfxStatus itc_FrameAssemble_NV12(mfxU8 *pTopField[2], mfxI32 stepTop[2],
                                 mfxU8 *pBotField[2], mfxI32 stepBot[2],
                                 mfxU8 *pDst[2], mfxI32 stepDst[2],
                                 mfxI32 width, mfxI32 height)
{
  mfxI32 i;
  IppStatus sts = ippStsNoErr;


    for (i = 0; i < height; i++)
    {
      sts = ippsCopy_8u(pTopField[0], pDst[0], width);
      VPP_CHECK_IPP_STS( sts );

      sts = ippsCopy_8u(pBotField[0], pDst[0]+stepDst[0], width);
      VPP_CHECK_IPP_STS( sts );

      pTopField[0] += (stepTop[0]*2);
      pBotField[0] += (stepBot[0]*2);
      pDst[0]      += (stepDst[0]*2);
    }

    height >>= 1;

    for (i = 0; i < height; i++)
    {
      sts = ippsCopy_8u(pTopField[1], pDst[1], width);
      VPP_CHECK_IPP_STS( sts );

      sts = ippsCopy_8u(pBotField[1], pDst[1]+stepDst[1], width);
      VPP_CHECK_IPP_STS( sts );

      pTopField[1] += (stepTop[1]*2);
      pBotField[1] += (stepBot[1]*2);
      pDst[1]      += (stepDst[1]*2);
    }

    return MFX_ERR_NONE;

} // mfxStatus itc_FrameAssemble_NV12(...)


mfxStatus MFXVideoVPPDeinterlace::itc_GetThreshold(sITCState* pState)
{
  if (pState->cadence_locked)
  {
    pState->expected_frp = (cadence_frp_table[pState->pat_ind].frp >> pState->patPhase) & 3;

    //pState->pending_b = ((cadence_frp_table[pState->pat_ind].pff >> pState->patPhase) & 3) == 1 && pState->expected_frp != 1;
    //pState->pending_t = ((cadence_frp_table[pState->pat_ind].pff >> pState->patPhase) & 3) == 2 && pState->expected_frp != 2;
    pState->pending_b = ((cadence_frp_table[pState->pat_ind].copy_patern >> pState->patPhase) & 3);

    if (pState->expected_frp & 1)
    {
      pState->t2t_thresh_factor = FiR_THR_FAC_EXPECTED;
    }
    else
    {
      pState->t2t_thresh_factor = FiR_THR_FAC_UNEXPECTED;
    }

    if (pState->expected_frp & 2)
    {
      pState->b2b_thresh_factor = FiR_THR_FAC_EXPECTED;
    }
    else
    {
      pState->b2b_thresh_factor = FiR_THR_FAC_UNEXPECTED;
    }

    pState->patPhase -= 2;
    if (pState->patPhase < 0)
    {
      pState->patPhase = (cadence_frp_table[pState->pat_ind].period  - 1) * 2;
    }
  }
  else
  {
    pState->t2t_thresh_factor = pState->b2b_thresh_factor = FiR_THR_FAC_DEFAULT;
  }

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::itc_GetThreshold(...)


mfxStatus MFXVideoVPPDeinterlace::itc_NV12( mfxFrameSurface1* in, mfxFrameSurface1* out,
                                            sITCState* pState,
                                            sDIState* pDIState,
                                            InternalParam* pParam)
{
    /* ***************************************** */
    /*          first frame processing           */
    /* ***************************************** */
    if( !pState->firstFrame )
    {
        pState->firstFrame++;
        return di_Processing(in, out, pDIState, pParam);
    }

    /* ******************************************************* */
    /* push out buffered data. Controlled by VPP_FrameCheck()  */
    /* ******************************************************* */
    if( NULL == in )
    {
        if( pState->cadence_locked ) // last frame processing in case of real ITC
        {
            bool bLockedOutBuf = false;
            mfxStatus mfxSts;

            if( NULL == pState->outBuf.Surface1.Data.Y )
            {
                mfxSts = m_core->LockFrame(pState->outBuf.mids[0], &(pState->outBuf.Surface1.Data));
                MFX_CHECK_STS( mfxSts );
                bLockedOutBuf = true;
            }

            mfxSts = SurfaceCopy_ROI(out, &(m_itcState.outBuf.Surface1));//using ITC out buffer. in is correct
            MFX_CHECK_STS( mfxSts );

            if( bLockedOutBuf )
            {
                mfxSts = m_core->UnlockFrame(m_itcState.outBuf.mids[0], &(m_itcState.outBuf.Surface1.Data));
                MFX_CHECK_STS( mfxSts );
                bLockedOutBuf = false;
            }

            return MFX_ERR_NONE;
        }
        else // // last frame processing in case of pseudo ITC
        {
            return di_Processing(in, out, pDIState, pParam);
        }
    }

    /* ******************************************************* */
    /*          preparation for normal processing              */
    /* ******************************************************* */

    mfxStatus mfxSts  = MFX_ERR_NOT_INITIALIZED;

    mfxU16  cropX = 0, cropY = 0;
    IppiSize roiSize = {0, 0};
    mfxFrameSurface1 localRefSurface;

    /* [OUT] */
    mfxU32  outOffset0= 0, outOffset1 = 0;
    mfxFrameData* outData= &out->Data;
    mfxFrameInfo* outInfo= &out->Info;

    VPP_GET_CROPX(outInfo, cropX);
    VPP_GET_CROPY(outInfo, cropY);
    outOffset0  = cropX   + cropY*outData->Pitch;
    outOffset1  = (cropX) + (cropY >> 1)*(outData->Pitch);

    mfxU8* pDst[2]   = {(mfxU8*)outData->Y + outOffset0,
                        (mfxU8*)outData->UV + outOffset1};
    mfxI32 pDstStep[2] = {outData->Pitch, outData->Pitch};

    bool bLockedRef    = false;
    bool bLockedOutBuf = false;

    if (pDIState->refList[0]->Data.Y == NULL)
    {
        mfxSts = m_core->LockExternalFrame(pDIState->refList[0]->Data.MemId, &(localRefSurface.Data));
        localRefSurface.Info = pDIState->refList[0]->Info;
        MFX_CHECK_STS(mfxSts);
        bLockedRef = true;
    }
    else
    {
        localRefSurface.Data = pDIState->refList[0]->Data;
        localRefSurface.Info = pDIState->refList[0]->Info;
    }

    if( NULL == pState->outBuf.Surface1.Data.Y )
    {
        mfxSts = m_core->LockFrame(pState->outBuf.mids[0], &(pState->outBuf.Surface1.Data));
        MFX_CHECK_STS( mfxSts );
        bLockedOutBuf = true;
    }

    mfxU32  refOffset0= 0, refOffset1 = 0;
    mfxFrameData* refData= &(localRefSurface.Data);
    mfxFrameInfo* refInfo= &(localRefSurface.Info);

    VPP_GET_CROPX(refInfo, cropX);
    VPP_GET_CROPY(refInfo, cropY);
    refOffset0  = cropX        + cropY*refData->Pitch;
    refOffset1  = (cropX) + (cropY >> 1)*(refData->Pitch);

    mfxU8* pRef[2] = {NULL, NULL};
    mfxI32 pRefStep[2] = {refData->Pitch, refData->Pitch};

    pRef[0] = (mfxU8*)refData->Y + refOffset0;
    pRef[1] = (mfxU8*)refData->UV + refOffset1;

    mfxU32  bufOffset0= 0, bufOffset1 = 0;
    mfxFrameData* bufData= &(pState->outBuf.Surface1.Data);
    mfxFrameInfo* bufInfo= &(pState->outBuf.Surface1.Info);

    m_itcState.outBuf.Surface1.Info = out->Info;

    VPP_GET_CROPX(bufInfo, cropX);
    VPP_GET_CROPY(bufInfo, cropY);
    bufOffset0  = cropX        + cropY*bufData->Pitch;
    bufOffset1  = (cropX) + (cropY >> 1)*(bufData->Pitch);

    mfxU8* pBuf[2]   = {(mfxU8*)bufData->Y + bufOffset0,
                        (mfxU8*)bufData->UV + bufOffset1};
    mfxI32 pBufStep[2] = {bufData->Pitch, bufData->Pitch};

    /* ***************************************** */
    /*              normal processing            */
    /* ***************************************** */

    mfxFrameData* inData = NULL;
    mfxFrameInfo* inInfo = NULL;
    const mfxU8* pSrc[2] = {NULL, NULL};
    mfxI32 pSrcStep[2]   = {0, 0};

    mfxU32  inOffset0 = 0, inOffset1  = 0;

    inData = &in->Data;
    inInfo = &in->Info;

    VPP_GET_CROPX(inInfo,  cropX);
    VPP_GET_CROPY(inInfo,  cropY);
    inOffset0  = cropX  + cropY*inData->Pitch;
    inOffset1  = (cropX) + (cropY >> 1)*(inData->Pitch);

    pSrc[0] = (mfxU8*)inData->Y + inOffset0;
    pSrc[1] = (mfxU8*)inData->UV + inOffset1;

    pSrcStep[0] = inData->Pitch;
    pSrcStep[1] = inData->Pitch;

    VPP_GET_WIDTH(inInfo, roiSize.width);
    VPP_GET_HEIGHT(inInfo, roiSize.height);


    mfxU8 *pTopField[2], *pBotField[2];

    mfxI32 t2t;
    mfxI32 b2b;
    mfxI32 t2b;
    mfxI32 b2t;
    mfxI32 tt;
    mfxI32 tb;
    mfxI32 bb;
    mfxI32 min;

    /* FirstFrame has been passed */
    pState->frp = 0;

    itc_Frame_NV12(pSrc, pSrcStep, pRef, pRefStep, roiSize, pState->var);

    tb  = pState->var[0];
    tt  = pState->var[5];
    bb  = pState->var[6];
    t2t = pState->var[1];
    b2b = pState->var[2];
    t2b = pState->var[3];
    b2t = pState->var[4];

    itc_GetThreshold( pState );

    min = tb;
    if (tt < tb) min = tt;
    if (bb < min) min = bb;
    if (t2t < min * pState->t2t_thresh_factor)
    {
        pState->frp |= 1;
    }
    else if (pState->cadence_locked && (pState->expected_frp & 1))
    {
        Ipp32f av = (tb + tt + bb + t2b) * 0.25f;
        if (t2t < av * FiR_THR_FAC_AV)
        {
            pState->frp |= 1;
        }
    }
    if (b2b < min * pState->b2b_thresh_factor)
    {
        pState->frp |= 2;
    }
    else if (pState->cadence_locked && (pState->expected_frp & 2))
    {
        Ipp32f av = (tb + tt + bb + b2t) * 0.25f;
        if (b2b < av * FiR_THR_FAC_AV)
        {
            pState->frp |= 2;
        }
    }

    /* reset cadence */
    //if (pState->cadence_locked && (pState->expected_frp != pState->frp))
    if (pState->cadence_locked &&
        (((pState->expected_frp & 1) > (pState->frp & 1)) ||
        ((pState->expected_frp & 2) > (pState->frp & 2))))
    {
        pState->cadence_locked = 0;
        pState->histLength = 0;
        pState->cur_frpattern = 0;
        pState->pending_b = pState->pending_t = 0;
        pState->isFrameInBuf = 0;
    }
    /* cadence locked */
    if ( !pState->cadence_locked )
    {
        pState->cur_frpattern = (pState->cur_frpattern << 2) | pState->frp;
        pState->histLength++;
        if (pState->histLength >= IVTC_CHECK_PATTERN_LEN)
        {
            Ipp32s l;
            for (l = 0; l < IVTC_NUM_PATTERNS; l++)
            {
                pState->patPhase = itc_CheckPattern(pState->cur_frpattern, IVTC_CHECK_PATTERN_LEN, l);
                if (pState->patPhase >= 0)
                {
                    Ipp32s tmpPatPhase;
                    pState->pat_ind = l;
                    pState->cadence_locked = 1;
                    pState->patPhase = (cadence_frp_table[pState->pat_ind].period - 1) * 2 - pState->patPhase;
                    tmpPatPhase = pState->patPhase + 2;

                    if (tmpPatPhase >= (cadence_frp_table[pState->pat_ind].period - 1) * 2)
                    {
                        tmpPatPhase -= (cadence_frp_table[pState->pat_ind].period - 1) * 2;
                    }

                    pState->expected_frp = (cadence_frp_table[pState->pat_ind].frp >> tmpPatPhase) & 3;

                    //pState->pending_b = ((cadence_frp_table[pState->pat_ind].pff >> tmpPatPhase) & 3) == 1 && pState->expected_frp != 1;
                    //pState->pending_t = ((cadence_frp_table[pState->pat_ind].pff >> tmpPatPhase) & 3) == 2 && pState->expected_frp != 2;

                    pState->pending_b = ((cadence_frp_table[pState->pat_ind].copy_patern >> pState->patPhase) & 3);

                    break;
                }
            }
        }
    }

    if (!pState->cadence_locked)
    {
        //Unlock External Frames
        if (bLockedRef)
        {
            m_core->UnlockExternalFrame(pDIState->refList[0]->Data.MemId, &(localRefSurface.Data));
            bLockedRef = false;
        }

        if( bLockedOutBuf )
        {
            mfxSts = m_core->UnlockFrame(pState->outBuf.mids[0], &(pState->outBuf.Surface1.Data));
            MFX_CHECK_STS( mfxSts );
            bLockedOutBuf = false;
        }

        return di_Processing(in, out, pDIState, pParam);
    }

    bool skipped = IsSkippedFrame(m_processReadinessState.numProcessedFrames);
    /* ************** */
    /* final decision */
    /* ************** */
    mfxStatus mfxStsITC  = MFX_ERR_MORE_DATA;

    //if (pState->pending_b) /* CASE 1 */
    if (pState->pending_b == 1) /* Bot field - last frame, Top field - current frame */
    {
        pTopField[0] = (mfxU8*)pSrc[0];
        pTopField[1] = (mfxU8*)pSrc[1];

        pBotField[0] = pRef[0] + pRefStep[0];
        pBotField[1] = pRef[1] + pRefStep[1];

        if (pState->isFrameInBuf)
        {
            if (!skipped)
            {
                /* buf->dst */
                mfxSts = SurfaceCopy_ROI(out, &(pState->outBuf.Surface1));
                MFX_CHECK_STS(mfxSts);
            }
            /* to buf */
            mfxStsITC = itc_FrameAssemble_NV12(pTopField, pSrcStep,
                pBotField, pRefStep,
                pBuf, pBufStep,
                roiSize.width, roiSize.height >> 1);
        }
        else
        {
            if (!skipped)
            {
                mfxSts = itc_FrameAssemble_NV12(pTopField, pSrcStep,
                    pBotField, pRefStep,
                    pDst, pDstStep,
                    roiSize.width, roiSize.height >> 1);

            }
            else
            {
                /* to buf */
                mfxSts = itc_FrameAssemble_NV12(pTopField, pSrcStep,
                    pBotField, pRefStep,
                    pBuf, pBufStep,
                    roiSize.width, roiSize.height >> 1);
                pState->isFrameInBuf = 1;

            }
        }
    }
    //else if (pState->pending_t) /* CASE 2 */
    else if (pState->pending_b == 2) /* Bot field - current frame, Top field - last frame */
    {
        pTopField[0] = pRef[0];
        pTopField[1] = pRef[1];

        pBotField[0] = (mfxU8*)pSrc[0] + pSrcStep[0];
        pBotField[1] = (mfxU8*)pSrc[1] + pSrcStep[1];

        if (pState->isFrameInBuf)
        {
            if (!skipped)
            {
                /* buf->dst */
                mfxSts = SurfaceCopy_ROI(out, &(pState->outBuf.Surface1));
                MFX_CHECK_STS(mfxSts);
            }
            /* to buf */
            mfxSts = itc_FrameAssemble_NV12(pTopField, pRefStep,
                pBotField, pSrcStep,
                pBuf, pBufStep,
                roiSize.width, roiSize.height >> 1);
        }
        else
        {
            if (!skipped)
            {
                mfxSts = itc_FrameAssemble_NV12(pTopField, pRefStep,
                    pBotField, pSrcStep,
                    pDst, pDstStep,
                    roiSize.width, roiSize.height >> 1);

            }
            else
            {
                /* to buf */
                mfxSts = itc_FrameAssemble_NV12(pTopField, pRefStep,
                    pBotField, pSrcStep,
                    pBuf, pBufStep,
                    roiSize.width, roiSize.height >> 1);

                pState->isFrameInBuf = 1;
            }
        }
    }
    //else if (pState->expected_frp == 3 || pState->expected_frp == 0) /* CASE3 */
    else if (pState->pending_b == 3) /* copy frame */
    {
        if (pState->isFrameInBuf)
        {
            if (!skipped)
            {
                /* buf->dst */
                mfxSts = SurfaceCopy_ROI(out, &(pState->outBuf.Surface1));
                MFX_CHECK_STS(mfxSts);
            }

            /* to buf */
            mfxSts = SurfaceCopy_ROI(&(pState->outBuf.Surface1), in);
            MFX_CHECK_STS( mfxSts );
        }
        else
        {
            if (!skipped)
            {
                /* src -> dst. Clean Copy */
                mfxSts = SurfaceCopy_ROI(out, in);
                MFX_CHECK_STS( mfxSts );
            }
            else
            {
                /* to buf */
                mfxSts = SurfaceCopy_ROI(&(pState->outBuf.Surface1), in);
                MFX_CHECK_STS( mfxSts );

                pState->isFrameInBuf = 1;
            }
        }
    }
    else /* skip frame */
    {
        if (!skipped)
        {
            if (pState->isFrameInBuf)
            {
                /* buf->dst */
                mfxSts = SurfaceCopy_ROI(out, &(pState->outBuf.Surface1));
                MFX_CHECK_STS(mfxSts);

                pState->isFrameInBuf = 0;

            }
            else
            {
                //Unlock External Frames
                if (bLockedRef)
                {
                    m_core->UnlockExternalFrame(pDIState->refList[0]->Data.MemId, &(localRefSurface.Data));
                    bLockedRef = false;
                }

                if( bLockedOutBuf )
                {
                    mfxSts = m_core->UnlockFrame(pState->outBuf.mids[0], &(pState->outBuf.Surface1.Data));
                    MFX_CHECK_STS( mfxSts );
                    bLockedOutBuf = false;
                }

                return di_Processing(in, out, pDIState, pParam);
            }
        }
    }

    if (!skipped)
    {
        mfxStsITC = MFX_ERR_NONE;
    }
    else
    {
        mfxStsITC = MFX_ERR_MORE_DATA;
    }

    //Unlock External Frames
    if (bLockedRef)
    {
        m_core->UnlockExternalFrame(pDIState->refList[0]->Data.MemId, &(localRefSurface.Data));
        bLockedRef = false;
    }

    mfxSts = m_core->DecreaseReference( &(pDIState->refList[1]->Data) );
    MFX_CHECK_STS( mfxSts );

    pDIState->refList[1] = pDIState->refList[0];
    pDIState->refList[0] = in;

    mfxSts = m_core->IncreaseReference( &(in->Data) );
    MFX_CHECK_STS( mfxSts );

    if( bLockedOutBuf )
    {
        mfxSts = m_core->UnlockFrame(pState->outBuf.mids[0], &(pState->outBuf.Surface1.Data));
        MFX_CHECK_STS( mfxSts );
        bLockedOutBuf = false;
    }

    return mfxStsITC;

} // mfxStatus MFXVideoVPPDeinterlace::itc_NV12(...)

/* ********************************************************************* */
/*        Frame Rate affect ITC algorithm                                */
/* ********************************************************************* */

mfxStatus MFXVideoVPPDeinterlace::CheckFrameRate(mfxFrameInfo* In, mfxFrameInfo* Out)
{
  if( 0 == In->FrameRateExtN || 0 == In->FrameRateExtD)
  {
    return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  if( 0 == Out->FrameRateExtN || 0 == Out->FrameRateExtD)
  {
      return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPDeinterlace::CheckFrameRate(...)


#endif // MFX_ENABLE_VPP
/* EOF */
