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

#include "mfx_vpp_utils.h"
#include "mfx_video_analysis_vpp.h"
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"
#include "ippvc.h"

/* UMC */
using namespace UMC;

#ifndef MFX_CHECK_UMC_STS
#define MFX_CHECK_UMC_STS(err) if (err != UMC_OK) {return MFX_ERR_UNDEFINED_BEHAVIOR;}
#endif

/* ******************************************************************** */
/*                 extension of UMC SceneAnalyser                       */
/* ******************************************************************** */

VPPSceneAnalyzerParams::VPPSceneAnalyzerParams( void )
{
  return;
}

VPPSceneAnalyzerParams::~VPPSceneAnalyzerParams(void)
{
  return;
}

VPPSceneAnalyzer::VPPSceneAnalyzer( void )
{
  m_spatialComplexity  = 0;
  m_temporalComplexity = 0;
  memset(m_pSliceMV, 0, sizeof(m_pSliceMV));
  return;
}

VPPSceneAnalyzer::~VPPSceneAnalyzer(void)
{
  Close();

  return;
}

Status VPPSceneAnalyzer::Init(BaseCodecParams *pParams)
{
  Status sts = UMC_OK;

  sts = SceneAnalyzer::Init(pParams);

  return sts;
}

Status VPPSceneAnalyzer::GetFrameP(MediaData *pSource)
{
  mfxU32 scale;
  VideoData *pSrc = DynamicCast<VideoData> (pSource);
  Status umcRes;

  // check error(s)
  if (NULL == pSrc)
  {
    return UMC_ERR_NOT_ENOUGH_DATA;
  }
  if (NULL == m_pCur)
  {
    return UMC_ERR_NOT_INITIALIZED;
  }

  // if destination has incorrect type
  if (false == CheckSupportedColorFormat(pSrc))
  {
    return UMC_ERR_INVALID_PARAMS;
  }

  // initialize the current being analyzed frame
  umcRes = m_pCur->SetSource(pSrc, m_params.m_interlaceType);
  if (UMC_OK != umcRes)
    return umcRes;

  // we do I picture analysis only in the beggining.
  // In other cases we do P picture analysis to track
  // color and deviations. All statistic is reset,
  // when the real scene changing happens.

  m_spatialComplexity  = 0;
  m_temporalComplexity = 0;

  if (0 == m_frameCount)
  {
    // perform the analysis
    umcRes = AnalyzeFrame(m_pCur);
    if (UMC_OK != umcRes)
      return umcRes;
    UpdateHistory(m_pCur, I_PICTURE);

    m_pCur->m_bChangeDetected = m_pCur->m_scaledPic.m_bChangeDetected = 1;
  }
  else
  {
    // perform the analysis
    umcRes = AnalyzeFrame(m_pRef, m_pCur);
    if (UMC_OK != umcRes)
      return umcRes;
    UpdateHistory(m_pCur, P_PICTURE);

    // reset counter in case of scene changing
    if (m_pCur->m_bChangeDetected)
    {
      // reset the history
      m_framesInHistory = 0;
    }
  }

  scale = IPP_MAX(1,m_pCur->m_scaledPic.m_info.numItems[SA_INTRA]);
  m_spatialComplexity  /= scale;
  m_temporalComplexity /= scale;

  // exchange the current & the previous frame
  {
    SceneAnalyzerFrame *pTemp;

    pTemp = m_pRef;
    m_pRef = m_pCur;
    m_pCur = pTemp;
  }

  // increment frame counter(s)
  m_frameCount += 1;

  return UMC_OK;

}

Status VPPSceneAnalyzer::GetFrame(MediaData *pSource, MediaData *pDestination)
{
  VideoData *pSrc = DynamicCast<VideoData> (pSource);
  VideoData *pDst = DynamicCast<VideoData> (pDestination);
  FrameType curFrameType = NONE_PICTURE;
  Status umcRes;

  // check error(s)
  if (NULL == pDst)
  {
    return UMC_ERR_NULL_PTR;
  }

  if (NULL == m_pPrev)
  {
    return UMC_ERR_NOT_INITIALIZED;
  }

  // do exchanging the buffers every time,
  // because the last analyzed frame is not a frame to show,
  // especially, when NULL source poiner is passed
  if (1 < m_frameCount && B_PICTURE != m_pPrev->m_frameType)
  {
    SceneAnalyzerFrame *pTemp;

    pTemp = m_pRef;
    m_pRef = m_pPrev;
    m_pPrev = m_pCur;
    m_pCur = pTemp;
  }
  else
  {
    SceneAnalyzerFrame *pTemp;

    pTemp = m_pPrev;
    m_pPrev = m_pCur;
    m_pCur = pTemp;
  }

  if (pSrc)
  {
    // if destination has incorrect type
    if (false == CheckSupportedColorFormat(pSrc))
    {
      return SceneAnalyzerBase::GetFrame(pSource, pDestination);
    }

    // initialize the current being analyzed frame
    umcRes = m_pCur->SetSource(pSrc, m_params.m_interlaceType);
    if (UMC_OK != umcRes)
    {
      return umcRes;
    }

    // we do I picture analysis only in the beggining.
    // In other cases we do P picture analysis to track
    // color and deviations. All statistic is reset,
    // when the real scene changing happens.

    // make 1 frame delay
    if (0 == m_frameCount)
    {
      curFrameType = I_PICTURE;

      // perform the analysis
      umcRes = AnalyzeFrame(m_pCur);
      if (UMC_OK != umcRes)
      {
          return umcRes;
      }

      UpdateHistory(m_pCur, I_PICTURE);

      // update the current frame
      m_pCur->m_frameType = I_PICTURE;

      // set the destination video data
      pDst = (VideoData *) 0;

    }
    else
    {
      // get planned frame type
      curFrameType = GetPlannedFrameType();

      // the previous frame is a reference frame.
      // So we should use it as a reference.
      if (B_PICTURE != m_pPrev->m_frameType)
      {
        // perform the analysis
        umcRes = AnalyzeFrame(m_pPrev, m_pCur);
        if (UMC_OK != umcRes)
        {
          return umcRes;
        }

        UpdateHistory(m_pCur, P_PICTURE);

        // reset counter in case of scene changing
        if (m_pCur->m_bChangeDetected)
        {
          // reset the history
          m_framesInHistory = 0;

          curFrameType = I_PICTURE;
        }

        // reset frame counter.
        // we do it in the separate IF clause,
        // because frame type could be set by the planning function.
        if (I_PICTURE == curFrameType)
        {
          // reset frame counters
          m_frameNum = 0;
          m_bFrameNum = 0;
        }

        // update the current frame
        m_pCur->m_frameType = curFrameType;
      }
      else
      {
        bool bLongChange;
        bool bShortChange;
        InterlaceType longFrameStructure, shortFrameStructure;
        InterlaceType longFrameEstimation, shortFrameEstimation;
        Ipp32u longDev, shortDev;

        // perform the analysis
        umcRes = AnalyzeFrame(m_pRef, m_pCur);
        if (UMC_OK != umcRes)
        {
            return umcRes;
        }

        bLongChange = m_pCur->m_bChangeDetected;
        longFrameStructure = m_pCur->m_frameStructure;
        longFrameEstimation = m_pCur->m_frameEstimation;
        longDev = m_pCur->m_scaledPic.m_info.averageDev[SA_INTER_ESTIMATED];

        // perform the analysis
        umcRes = AnalyzeFrame(m_pPrev, m_pCur);
        if (UMC_OK != umcRes)
        {
            return umcRes;
        }

        UpdateHistory(m_pCur, P_PICTURE);
        bShortChange = m_pCur->m_bChangeDetected;
        shortFrameStructure = m_pCur->m_frameStructure;
        shortFrameEstimation = m_pCur->m_frameEstimation;
        shortDev = m_pCur->m_scaledPic.m_info.averageDev[SA_INTER_ESTIMATED];

        if (bShortChange)
        {
          // reset counter in case of scene changing
          {
            // reset the history
            m_framesInHistory = 0;
            curFrameType = I_PICTURE;
          }

          // reset frame counter.
          // we do it in the separate IF clause,
          // because frame type could be set by the planning function.
          {
            // reset frame counter
            m_frameNum = 0;
            m_bFrameNum = 0;
          }

          // close the previous GOP
          m_pPrev->m_frameType = P_PICTURE;

          // update the current frame
          m_pCur->m_frameType = curFrameType;
          m_pCur->m_frameStructure = shortFrameStructure;
          m_pCur->m_frameEstimation = shortFrameEstimation;
        }
        // the reference and the current frames are
        // to different. It is the time to update
        // the reference
        else if ((bLongChange) || (longDev > shortDev))
        {
          // reset frame counter
          m_bFrameNum = 0;

          // close the previous GOP
          m_pPrev->m_frameType = P_PICTURE;

          // update the current frame
          curFrameType = GetPlannedFrameType();
          m_pCur->m_frameType = curFrameType;
          m_pCur->m_frameStructure = shortFrameStructure;
          m_pCur->m_frameEstimation = shortFrameEstimation;
        }
        else
        {
          // update the current frame
          m_pCur->m_frameType = curFrameType;
          m_pCur->m_frameStructure = longFrameStructure;
          m_pCur->m_frameEstimation = longFrameEstimation;
        }
      }
    }
  }
  else
  {
    // we need to show the last frame,
    // then we shall return UMC_ERR_NOT_ENOUGH_DATA
    if (0 == m_frameCount)
    {
      pDst = (VideoData *) 0;
    }

    // set frame counter into an invalid value
    m_frameCount = -1;
  }

  if (pDst)
  {
    // set the destination video data
    InitializeVideoData(pDst, m_pPrev);

    // update destination video data
    pDst->SetFrameType(m_pPrev->m_frameType);
    switch (m_pPrev->m_frameEstimation)
    {
    case INTERLEAVED_TOP_FIELD_FIRST:
      pDst->SetPictureStructure(PS_TOP_FIELD_FIRST);
      break;

    case INTERLEAVED_BOTTOM_FIELD_FIRST:
      pDst->SetPictureStructure(PS_BOTTOM_FIELD_FIRST);
      break;

    default:
      pDst->SetPictureStructure(PS_FRAME);
      break;
    }
  }

  // increment frame counter(s)
  m_frameCount += 1;
  m_frameNum += 1;
  if (B_PICTURE == curFrameType)
  {
    m_bFrameNum += 1;
  }
  else
  {
    m_bFrameNum = 0;
  }

  return (pDst) ? (UMC_OK) : (UMC_ERR_NOT_ENOUGH_DATA);
}

Status VPPSceneAnalyzer::Close(void)
{
  Status sts = UMC_OK;

  sts = SceneAnalyzer::Close();

  return sts;
}

Status VPPSceneAnalyzer::GetMetrics(mfxExtVppAuxData *aux)
{
  Status sts = UMC_OK;

  SceneAnalyzerPicture *pPic = &(m_pRef->m_scaledPic);

  if (NULL == aux)
  {
      return UMC_ERR_NULL_PTR;
  }

  if (NULL == m_pRef) return UMC_ERR_NOT_INITIALIZED;


  aux->SpatialComplexity  = m_spatialComplexity;
  aux->TemporalComplexity = m_temporalComplexity;

  aux->SceneChangeRate    = ( pPic->m_bChangeDetected ) ? 100 : 0;

  return sts;
}
/* ******************************************************************** */
/*           implementation of VPP filter [VideoAnalysis]               */
/* ******************************************************************** */

MFXVideoVPPVideoAnalysis::MFXVideoVPPVideoAnalysis(VideoCORE *core, mfxStatus* sts) : FilterVPP()
{
  m_core = core;

  VPP_CLEAN;
  *sts = MFX_ERR_NONE;

  return;
}

MFXVideoVPPVideoAnalysis::~MFXVideoVPPVideoAnalysis(void)
{
  Close();

  return;
}

mfxStatus MFXVideoVPPVideoAnalysis::SetParam( mfxExtBuffer* pHint )
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  if( pHint )
  {
    // mfxSts = something
  }

  return mfxSts;
}

mfxStatus MFXVideoVPPVideoAnalysis::Reset(mfxVideoParam *par)
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  MFX_CHECK_NULL_PTR1( par );

  VPP_CHECK_NOT_INITIALIZED;

  // in case of SA, MUST BE in == out
  // so, some check is redundant.
  // But m_errPrtctState contains correct info.

  // simple checking wo analysis
  if(m_errPrtctState.In.FourCC != par->vpp.In.FourCC || m_errPrtctState.Out.FourCC != par->vpp.Out.FourCC)
  {
      return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  if( m_errPrtctState.Out.Width != par->vpp.Out.Width || m_errPrtctState.Out.Height != par->vpp.Out.Height )
  {
      return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  if( m_errPrtctState.In.Width != par->vpp.In.Width || m_errPrtctState.In.Height != par->vpp.In.Height )
  {
      return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  if( (m_errPrtctState.In.PicStruct != par->vpp.In.PicStruct) || (m_errPrtctState.Out.PicStruct != par->vpp.Out.PicStruct) )
  {
      return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  VPP_RESET;

  return mfxSts;
}

mfxStatus MFXVideoVPPVideoAnalysis::RunFrameVPP(mfxFrameSurface1 *in,
                                                mfxFrameSurface1 *out,
                                                InternalParam* pParam)
{
  mfxI32    srcW   = 0, srcH = 0;
  mfxI32    dstW   = 0, dstH = 0;

  mfxStatus mfxSts = MFX_ERR_NONE;

  mfxU32  srcFourCC = MFX_FOURCC_YV12; //default
  mfxU32  dstFourCC = MFX_FOURCC_YV12; //default

  mfxFrameData* inData  = NULL;
  mfxFrameData* outData = NULL;
  mfxFrameInfo* inInfo  = NULL;
  mfxFrameInfo* outInfo = NULL;

  VPP_CHECK_NOT_INITIALIZED;

  if( NULL == in )
  {
    return MFX_ERR_MORE_DATA;
  }
  if( NULL == out )
  {
    return MFX_ERR_NULL_PTR;
  }
  if( NULL == pParam )
  {
      return MFX_ERR_NULL_PTR;
  }

  inData  = &(in->Data);
  outData = &(out->Data);

  inInfo  = &(in->Info);
  outInfo = &(out->Info);

  /* IN */
  VPP_GET_WIDTH(inInfo, srcW);
  VPP_GET_HEIGHT(inInfo, srcH);

  /* OUT */
  VPP_GET_WIDTH(outInfo, dstW);
  VPP_GET_HEIGHT(outInfo, dstH);

  /* robustness check */
  if( srcW != dstW || srcH != dstH )
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  srcFourCC = inInfo->FourCC;
  dstFourCC = outInfo->FourCC;

  if( srcFourCC != dstFourCC )
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  // in accordance with ROI processing every frame can have own cropX/Y/W/H
  if( m_errPrtctState.In.Height < dstH || m_errPrtctState.In.Width < dstW)
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  mfxSts = SceneAnalysis(inData, inInfo, outData, outInfo, pParam->aux);
  MFX_CHECK_STS( mfxSts );

  if( !m_errPrtctState.isFirstFrameProcessed )
  {
      m_errPrtctState.isFirstFrameProcessed = true;
  }

  pParam->outPicStruct = pParam->inPicStruct;
  pParam->outTimeStamp = pParam->inTimeStamp;

  return mfxSts;
}

mfxStatus MFXVideoVPPVideoAnalysis::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
{
  Status  umcSts   = UMC_OK;

  mfxI32    srcW   = 0, srcH = 0;//, srcPitch = 0;
  mfxI32    dstW   = 0, dstH = 0;//, dstPitch = 0;

  mfxU32  srcFourCC = MFX_FOURCC_YV12; //default
  mfxU32  dstFourCC = MFX_FOURCC_YV12; //default

  MFX_CHECK_NULL_PTR2( In, Out );

  VPP_CHECK_MULTIPLE_INIT;

  /* IN */
  VPP_GET_REAL_WIDTH(In, srcW);
  VPP_GET_REAL_HEIGHT(In, srcH);

  /* OUT */
  VPP_GET_REAL_WIDTH(Out, dstW);
  VPP_GET_REAL_HEIGHT(Out, dstH);

  /* robustness check */
  if( srcW != dstW || srcH != dstH )
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  srcFourCC = In->FourCC;
  dstFourCC = Out->FourCC;

  if( srcFourCC != dstFourCC )
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* FourCC checking */
  switch ( srcFourCC )
  {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_YV12:

      //VPP_INIT_SUCCESSFUL;

      break;

    default:

      return MFX_ERR_INVALID_VIDEO_PARAM;

  }

  m_analyserParams.m_interlaceType = PROGRESSIVE;
  m_analyserParams.m_maxGOPLength  = 0;// if scene_change_detection is defined => I frame will be used
  m_analyserParams.m_maxBLength    = 2;

  umcSts = m_analyser.Init( &m_analyserParams );
  MFX_CHECK_UMC_STS(umcSts);

  /* save init params to prevent core crash */
  m_errPrtctState.In  = *In;
  m_errPrtctState.Out = *Out;

  VPP_INIT_SUCCESSFUL;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPVideoAnalysis::Close(void)
{
  VPP_CHECK_NOT_INITIALIZED;

  m_analyser.Close();// ignore in according with spec

  VPP_CLEAN;

  return MFX_ERR_NONE;
}


// work buffer management - nothing for VA filter
mfxStatus MFXVideoVPPVideoAnalysis::GetBufferSize( mfxU32* pBufferSize )
{
  VPP_CHECK_NOT_INITIALIZED;

  MFX_CHECK_NULL_PTR1(pBufferSize);

  *pBufferSize = 0;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPVideoAnalysis::SetBuffer( mfxU8* pBuffer )
{
  mfxU32 bufSize = 0;
  mfxStatus sts;

  VPP_CHECK_NOT_INITIALIZED;

  sts = GetBufferSize( &bufSize );
  MFX_CHECK_STS( sts );

  // VA dosn't require work buffer, so pBuffer == NULL is OK
  if( bufSize )
  {
    MFX_CHECK_NULL_PTR1(pBuffer);
  }

  return MFX_ERR_NONE;
}

// function is called from sync part of VPP only
mfxStatus MFXVideoVPPVideoAnalysis::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
{
  if( NULL == in )
  {
    return MFX_ERR_MORE_DATA;
  }
  else if( NULL == out )
  {
    return MFX_ERR_NULL_PTR;
  }

  PassThrough(in, out);

  return MFX_ERR_NONE;
}

bool MFXVideoVPPVideoAnalysis::IsReadyOutput( mfxRequestType requestType )
{
    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
        // SA now is simple algorithm and need IN to produce OUT
        // possible, will be designed later
    }

    return false;
}

/* ******************************************************************** */
/*                 implementation of algorithms [VA]                    */
/* ******************************************************************** */

mfxStatus MFXVideoVPPVideoAnalysis::SceneAnalysis(mfxFrameData* inData,
                                                  mfxFrameInfo* inInfo,
                                                  mfxFrameData* outData,
                                                  mfxFrameInfo* outInfo,
                                                  mfxExtVppAuxData *aux)
{
  mfxStatus mfxSts = MFX_ERR_NONE;
  VideoData umcInData;
  IppiSize  roiSize;
  Status    umcSts;

  mfxFrameSurface1 inSurface;
  mfxFrameSurface1 outSurface;

  VPP_GET_REAL_WIDTH(inInfo,  roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  /* [IN] */
  umcSts = umcInData.Init(roiSize.width, roiSize.height, GRAY);
  MFX_CHECK_UMC_STS( umcSts );

  umcSts = umcInData.SetPlanePointer( (Ipp8u*)inData->Y, 0);
  MFX_CHECK_UMC_STS( umcSts );

  umcSts = umcInData.SetPlanePitch( inData->Pitch, 0);
  MFX_CHECK_UMC_STS( umcSts );

  /* algorithm */

  //analyzer doesn't change data,i.e. just gets info about frame complexity
  umcSts = m_analyser.GetFrameP( &umcInData);

  if( UMC_ERR_NOT_ENOUGH_DATA  == umcSts )
  {
      return MFX_ERR_MORE_DATA;
  }
  MFX_CHECK_UMC_STS( umcSts );

  if( aux )
  {
    umcSts = m_analyser.GetMetrics(aux);
    MFX_CHECK_UMC_STS( umcSts );
  }

  /* copy in -> out */
  inSurface.Data = *inData;
  inSurface.Info = *inInfo;

  outSurface.Data = *outData;
  outSurface.Info = *outInfo;

  mfxSts = SurfaceCopy_ROI(&outSurface, &inSurface);

  return mfxSts;
}

#define GET_SAD(X,Y)                                                   \
    ippiSAD4x4_8u32s(pSrc,srcStep,pRef+refStep*(Y)+(X),refStep,&sad,0); \
    if(sad_min>sad){                                                     \
      sad_min=sad;                                                        \
      x_min=(X);                                                           \
      y_min=(Y);                                                            \
      min_trig=1;                                                            \
    }

Status VPPSceneAnalyzer::AnalyzePicture(SceneAnalyzerPicture *pRef, SceneAnalyzerPicture *pSrc)
{
    UMC_SCENE_INFO *pSliceInfo;
    const Ipp8u *pbSrc;
    Ipp32s srcStep;
    const Ipp8u *pbRef;
    Ipp32s refStep;
    Ipp32u mbY;

    IppiPoint prevMV[1] = {{0,0}};
    memset(m_pSliceMV,0,sizeof(IppiPoint)*pSrc->m_mbDim.width);

    // reset the variables
    pSliceInfo = pSrc->m_pSliceInfo;
    memset(&(pSrc->m_info), 0, sizeof(pSrc->m_info));

    // get the source pointer
    pbSrc = (const Ipp8u *) pSrc->m_pPic[0];
    srcStep = (Ipp32s) pSrc->m_picStep;
    pbRef = (const Ipp8u *) pRef->m_pPic[0];
    refStep = (Ipp32s) pRef->m_picStep;

    // cycle over rows
    for (mbY = 0; mbY < (Ipp32u) pSrc->m_mbDim.height; mbY += 1)
    {
        UMC_SCENE_INFO sliceInfo;
        Ipp32u mbX;

        // reset the variables
        memset(&sliceInfo, 0, sizeof(sliceInfo));

        // cycle in the row
        for (mbX = 0; mbX < (Ipp32u) pSrc->m_mbDim.width; mbX += 1)
        {
            UMC_SCENE_INFO mbInfo;

            // reset MB info
            memset(&mbInfo, 0, sizeof(mbInfo));
            mbInfo.numItems[SA_INTRA] = 1;
            mbInfo.numItems[SA_COLOR] = 1;
            mbInfo.numItems[SA_INTER] = 1;
            mbInfo.numItems[SA_INTER_ESTIMATED] = 1;

            // get the dispersion
            AnalyzeInterMB(pbRef + mbX * 4, refStep,
                           pbSrc + mbX * 4, srcStep,
                           &mbInfo);

            // the difference is to big,
            // try to find a better match
            if (mbInfo.sumDev[SA_INTRA] < mbInfo.sumDev[SA_INTER])
            {
                AnalyzeInterMBMotionOpt(pbRef + mbX * 4, refStep,
                                        pRef->m_mbDim,
                                        pbSrc + mbX * 4, srcStep,
                                        mbX, mbY,
                                        prevMV,
                                        &mbInfo);
            }

            // update slice info
            if ((mbInfo.sumDev[SA_INTRA]) || (mbInfo.sumDev[SA_INTER]))
            {
                AddIntraDeviation(&sliceInfo, &mbInfo);
                AddInterDeviation(&sliceInfo, &mbInfo);
            }
        }

        // update frame info
        AddIntraDeviation(&(pSrc->m_info), &sliceInfo);
        AddInterDeviation(&(pSrc->m_info), &sliceInfo);

        // get average for slice & update slice info
        if (pSliceInfo)
        {
            GetAverageIntraDeviation(&sliceInfo);
            GetAverageInterDeviation(&sliceInfo);
            *pSliceInfo = sliceInfo;
            pSliceInfo += 1;
        }

        // advance the pointers
        pbSrc += 4 * srcStep;
        pbRef += 4 * refStep;
    }

    // get average for frame
    GetAverageIntraDeviation(&(pSrc->m_info));
    GetAverageInterDeviation(&(pSrc->m_info));

    pSrc->m_info.averageDev[SA_INTER_ESTIMATED]/=2;// compensate fast ME

    //
    // make the frame type decision
    //
    MakePPictureCodingDecision(pRef, pSrc);

    return UMC_OK;

} // VPPSceneAnalyzer::AnalyzePicture

void VPPSceneAnalyzer::AnalyzeIntraMB(const Ipp8u *pSrc, Ipp32s srcStep,
                                    UMC_SCENE_INFO *pMbInfo)
{
  Ipp32u blockDev;

  // get block deviations
  blockDev = ippiGetIntraBlockDeviation_4x4_8u(pSrc, srcStep);
  pMbInfo->sumDev[SA_INTRA] = (blockDev + 8) / 16;

  // spatial frame mean complexity
  m_spatialComplexity += blockDev;

  // get average color
  pMbInfo->sumDev[SA_COLOR] = ippiGetAverage4x4_8u(pSrc, srcStep);
} // void VPPSceneAnalyzer::AnalyzeIntraMB

void VPPSceneAnalyzer::AnalyzeInterMBMotionOpt(const Ipp8u *pRef, Ipp32s refStep,
                                               IppiSize refMbDim,
                                               const Ipp8u *pSrc, Ipp32s srcStep,
                                               Ipp32u mbX, Ipp32u mbY,
                                               IppiPoint *prevMV,
                                               UMC_SCENE_INFO *pMbInfo)
{
  Ipp32s k,min_trig,num_steps;
  Ipp32s top,left,right,bottom,searchHeight,searchWidth,frameWidth,frameHeight; //borders
  Ipp32s x,y,x_cur=mbX*4,y_cur=mbY*4,x_mid,y_mid,x_min = 0,y_min = 0; //coordinates
  Ipp32s searchStep; //search step
  Ipp32s sad_min=INT_MAX,sad;// like MAD

  //init borders
  frameWidth=refMbDim.width*4;
  frameHeight=refMbDim.height*4;
  searchHeight = SA_ESTIMATION_HEIGHT/2;
  searchWidth = SA_ESTIMATION_WIDTH/2;

  top = y_cur-searchHeight;
  if(top < 0)top=0;

  bottom = y_cur+searchHeight;
  if(bottom > frameHeight-4)bottom=frameHeight-4;

  left = x_cur-searchWidth;
  if(left < 0)left=0;

  right = x_cur+searchWidth;
  if(right > frameWidth-4)right=frameWidth-4;

  right-=x_cur;
  left-=x_cur;
  top-=y_cur;
  bottom-=y_cur;

  //init search step
  searchStep=IPP_MAX(SA_ESTIMATION_HEIGHT>>3,SA_ESTIMATION_WIDTH>>3);
  if(searchStep<1)
    searchStep=1;

  //start
  //check initial search point
  if(prevMV[0].x<=right && prevMV[0].x >= left && prevMV[0].y<=bottom && prevMV[0].y>=top)
  {
    GET_SAD(prevMV[0].x,prevMV[0].y);
  }
  if(mbY>0)
  {
    if(m_pSliceMV[mbX].x<=right && m_pSliceMV[mbX].x >= left && m_pSliceMV[mbX].y<=bottom && m_pSliceMV[mbX].y>=top)
    {
      GET_SAD(m_pSliceMV[mbX].x,m_pSliceMV[mbX].y);
    }
  }
  GET_SAD(0,0);

  if(sad_min > (Ipp32s)pMbInfo->sumDev[SA_INTRA])
  {
    x_mid=x_min;
    y_mid=y_min;

    num_steps=IPP_MAX(SA_ESTIMATION_HEIGHT,SA_ESTIMATION_WIDTH);

    while(searchStep > 0)
    {
      for(k=0;k<num_steps;k++)
      {
        min_trig=0;

        x=x_mid-searchStep;
        y=y_mid;
        if(x >= left)
        {
          GET_SAD(x,y);
        }

        x=x_mid+searchStep;
        y=y_mid;
        if(x <= right)
        {
          GET_SAD(x,y);
        }

        x=x_mid;
        y=y_mid-searchStep;
        if(y >= top)
        {
          GET_SAD(x,y);
        }

        x=x_mid;
        y=y_mid+searchStep;
        if(y <= bottom)
        {
          GET_SAD(x,y);
        }

        x_mid=x_min;
        y_mid=y_min;

        if(!min_trig)break;
      }
      if(sad_min==0)break;

      searchStep >>= 1;
    }
  }

  m_pSliceMV[mbX].x=prevMV[0].x=x_min;
  m_pSliceMV[mbX].y=prevMV[0].y=y_min;

  pMbInfo->sumDev[SA_INTER_ESTIMATED] = (sad_min+8)/16;

  m_temporalComplexity += sad_min;

}//VPPSceneAnalyzer::AnalyzeInterMBMotion

#endif // MFX_ENABLE_VPP
/* EOF */
