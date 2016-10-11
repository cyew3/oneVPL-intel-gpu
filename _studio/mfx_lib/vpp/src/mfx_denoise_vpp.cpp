/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//
//
//          DeNoise Video Pre\Post Processing
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "mfx_vpp_utils.h"
#include "mfx_denoise_vpp.h"

/* ******************************************************************** */
/*                 implementation of VPP filter [Denoise]               */
/* ******************************************************************** */
// this range are used by Query to correct application request
#define PAR_NRF_STRENGTH_MIN                (0)
#define PAR_NRF_STRENGTH_MAX                100 // real value is 63
#define PAR_NRF_STRENGTH_DEFAULT            PAR_NRF_STRENGTH_MIN

mfxStatus MFXVideoVPPDenoise::Query( mfxExtBuffer* pHint )
{
    if( NULL == pHint )
    {
        return MFX_ERR_NONE;
    }

    mfxStatus sts = MFX_ERR_NONE;

    mfxExtVPPDenoise* pParam = (mfxExtVPPDenoise*)pHint;

    if( pParam->DenoiseFactor > PAR_NRF_STRENGTH_MAX )
    {
        VPP_RANGE_CLIP(pParam->DenoiseFactor, PAR_NRF_STRENGTH_MIN, PAR_NRF_STRENGTH_MAX);

        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return sts;

} // static mfxStatus MFXVideoVPPDenoise::Query( mfxExtBuffer* pHint )

#if !defined(MFX_ENABLE_HW_ONLY_VPP) // SW is disabled

#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"
#include "ippcv.h"

#if !defined(WIN64) && (defined (WIN32) || defined (_WIN32) || defined (WIN32E) || defined (_WIN32E) || defined(__i386__) || defined(__x86_64__))
#if defined(__INTEL_COMPILER) || (_MSC_VER >= 1300) || (defined(__GNUC__) && defined(__SSE2__) && (__GNUC__ > 3))
#define VPP_SSE
#endif
#endif

#ifdef VPP_SSE
#include "emmintrin.h"
#endif

#define ALIGN32(X) (((mfxU32)((X)+31)) & (~ (mfxU32)31))

/*
#define printMatr(ptr, pitchA, height, width, name) {   \
printf("Matrix %s\n", name);                        \
for(Ipp32s i = 0; i < height; i++) {                \
for(Ipp32s j = 0; j < width; j++)               \
printf(" %4d,", ptr[i*(pitchA) + j]);       \
printf("\n");                                   \
}                                                   \
printf("\n");                                       \
}
*/
/****************************************************************************\
* NRF Constants
\****************************************************************************/
//Declare macros for NRF parameters:
#define PAR_NRF_BLOCK_WIDTH_NE              (16)
#define PAR_NRF_BLOCK_HEIGHT_NE             (4)

#define PAR_NRF_BLOCK_WIDTH_DN              (4)
#define PAR_NRF_BLOCK_HEIGHT_DN             (4)

#define PAR_NRF_SOBEL_EDGE_THR              (8)
#define PAR_NRF_LOCAL_DIFF_THR              (20)

#define PAR_NRF_LOW_NOISE_LEVEL             (200)
#define PAR_NRF_STRONG_NOISE_LEVEL          (400)

#define PAR_NRF_NUMBER_OF_MOTION_PIXELS     (0)

#define PAR_NRF_BLOCK_SIZE                  (4)

#define PAR_NRF_BLOCK_SCM_THRESHOLD         (32)
#define PAR_NRF_BLOCK_ASD_THRESHOLD         (32)

// Spatial Denoise Definitions
#define PAR_NRF_GAUSSIAN_THRESHOLD_MIN      (0)
#define PAR_NRF_GAUSSIAN_THRESHOLD_MEDIUM   (4)
#define PAR_NRF_GAUSSIAN_THRESHOLD_HIGH     (12)
#define PAR_NRF_GAUSSIAN_THRESHOLD_MAX      (16)

// Temporal Denoise Definitions
#define PAR_NRF_HISTORY_DELTA_LOW           (4)
#define PAR_NRF_HISTORY_DELTA_HIGH          (8)
#define PAR_NRF_HISTORY_DELTA_DEFAULT       PAR_NRF_HISTORY_DELTA_LOW

#define PAR_NRF_HISTORY_MAX_LOW             (144)
#define PAR_NRF_HISTORY_MAX_HIGH            (208)
#define PAR_NRF_HISTORY_MAX_DEFAULT         (192)

#define PAR_NRF_BLOCK_ASD_THRESHOLD_LOW                     (32)
#define PAR_NRF_BLOCK_ASD_THRESHOLD_HIGH                    (40)
#define PAR_NRF_BLOCK_ASD_THRESHOLD_DEFAULT                 PAR_NRF_BLOCK_ASD_THRESHOLD_LOW

#define PAR_NRF_SCM_THRESHOLD_LOW                           (32)
#define PAR_NRF_SCM_THRESHOLD_HIGH                          (40)
#define PAR_NRF_SCM_THRESHOLD_DEFAULT                       PAR_NRF_SCM_THRESHOLD_LOW

#define PAR_NRF_NUM_MOTION_PIXELS_THRESHOLD_LOW             (0)
#define PAR_NRF_NUM_MOTION_PIXELS_THRESHOLD_HIGH            (2)
#define PAR_NRF_NUM_MOTION_PIXELS_THRESHOLD_DEFAULT         PAR_NRF_NUM_MOTION_PIXELS_THRESHOLD_LOW

#define PAR_NRF_LOW_TEMPORAL_PIXEL_DIFF_THRESHOLD_LOW       (4)
#define PAR_NRF_LOW_TEMPORAL_PIXEL_DIFF_THRESHOLD_HIGH      (8)
#define PAR_NRF_LOW_TEMPORAL_PIXEL_DIFF_THRESHOLD_DEFAULT   (6)

#define PAR_NRF_TEMPORAL_PIXEL_DIFF_THRESHOLD_LOW           (10)
#define PAR_NRF_TEMPORAL_PIXEL_DIFF_THRESHOLD_HIGH          (14)
#define PAR_NRF_TEMPORAL_PIXEL_DIFF_THRESHOLD_DEFAULT       (12)

#define PAR_NRF_SUM_ABS_TEMPORAL_DIFF_THRESHOLD_LOW         (128)
#define PAR_NRF_SUM_ABS_TEMPORAL_DIFF_THRESHOLD_HIGH        (144)
#define PAR_NRF_SUM_ABS_TEMPORAL_DIFF_THRESHOLD_DEFAULT      PAR_NRF_SUM_ABS_TEMPORAL_DIFF_THRESHOLD_LOW

#define DENOISE_FACTOR_CONVERSION(x, low, high) \
    (low + ((x * (high - low)) / (float)(PAR_NRF_STRENGTH_MAX)) + 0.5)

MFXVideoVPPDenoise::MFXVideoVPPDenoise(VideoCORE *core, mfxStatus* sts) : FilterVPP(), m_stateY()
{
  //MFX_CHECK_NULL_PTR1(core);

  m_pRefSurface= NULL;

  m_core = core;

  m_lowNoiseLevel    = PAR_NRF_LOW_NOISE_LEVEL;
  m_strongNoiseLevel = PAR_NRF_STRONG_NOISE_LEVEL;

  m_sobelEdgeThreshold = PAR_NRF_SOBEL_EDGE_THR;
  m_localDiffThreshold = PAR_NRF_LOCAL_DIFF_THR;

  m_blockHeightNE = PAR_NRF_BLOCK_HEIGHT_NE;
  m_blockWidthNE  = PAR_NRF_BLOCK_WIDTH_NE;

  m_blockHeightDN = PAR_NRF_BLOCK_HEIGHT_DN;
  m_blockWidthDN  = PAR_NRF_BLOCK_WIDTH_DN;

  m_historyMax   = PAR_NRF_HISTORY_MAX_DEFAULT;
  m_historyDelta = PAR_NRF_HISTORY_DELTA_DEFAULT;

  m_pHistoryWeight = NULL;
  m_historyStep    = 0;

  // set default noise level
  m_denoiseFactor = PAR_NRF_STRENGTH_DEFAULT;
  // in default, denoiser works in auto mode
  m_bAutoAdjust = true;
  // ignore internal denoise factor if AutoAdjust = true
  m_internalDenoiseFactor = 0;

  VPP_CLEAN;

  *sts = MFX_ERR_NONE;

  m_numberOfMotionPixelsThreshold = 0;
  m_md_diff_th = 0;
  m_temporal_diff_th = 0;
  m_blockSCMThreshold = 0;
  m_blockASDThreshold = 0;
  m_temp_diff_low = 0;
  return;
}

MFXVideoVPPDenoise::~MFXVideoVPPDenoise(void)
{
  Close();

  return;
}

mfxStatus MFXVideoVPPDenoise::SetParam( mfxExtBuffer* pHint )
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  m_denoiseFactor = PAR_NRF_STRENGTH_DEFAULT;
  m_bAutoAdjust   = true;

  if( pHint )
  {
      mfxExtVPPDenoise* pDenoiseParams = (mfxExtVPPDenoise*)pHint;

      m_denoiseFactor = VPP_RANGE_CLIP(pDenoiseParams->DenoiseFactor,
                                       PAR_NRF_STRENGTH_MIN,
                                       PAR_NRF_STRENGTH_MAX);

      m_bAutoAdjust = false;
  }

   m_md_diff_th       = PAR_NRF_SUM_ABS_TEMPORAL_DIFF_THRESHOLD_DEFAULT;
   m_temporal_diff_th = PAR_NRF_TEMPORAL_PIXEL_DIFF_THRESHOLD_DEFAULT;
   m_temp_diff_low    = PAR_NRF_LOW_TEMPORAL_PIXEL_DIFF_THRESHOLD_DEFAULT;

   m_blockASDThreshold = PAR_NRF_BLOCK_ASD_THRESHOLD_DEFAULT;
   m_blockSCMThreshold = PAR_NRF_SCM_THRESHOLD_DEFAULT;

   m_numberOfMotionPixelsThreshold = PAR_NRF_NUM_MOTION_PIXELS_THRESHOLD_DEFAULT;

   m_historyMax   = PAR_NRF_HISTORY_MAX_DEFAULT;
   m_historyDelta = PAR_NRF_HISTORY_DELTA_DEFAULT;

  if( !m_bAutoAdjust )
  {
      // convert application denoise factor to internal (real) denoise factor
      m_internalDenoiseFactor = (mfxI32)DENOISE_FACTOR_CONVERSION(m_denoiseFactor, PAR_NRF_GAUSSIAN_THRESHOLD_MIN, PAR_NRF_GAUSSIAN_THRESHOLD_MAX );

      if( m_internalDenoiseFactor >= PAR_NRF_GAUSSIAN_THRESHOLD_HIGH )
      {
          m_historyDelta      = (mfxU8)DENOISE_FACTOR_CONVERSION(m_denoiseFactor, PAR_NRF_HISTORY_DELTA_LOW, PAR_NRF_HISTORY_DELTA_HIGH);
          m_historyMax        = (mfxU8)DENOISE_FACTOR_CONVERSION(m_denoiseFactor, PAR_NRF_HISTORY_MAX_LOW, PAR_NRF_HISTORY_MAX_HIGH);

          m_blockASDThreshold = (mfxI32)DENOISE_FACTOR_CONVERSION(m_denoiseFactor, PAR_NRF_BLOCK_ASD_THRESHOLD_LOW, PAR_NRF_BLOCK_ASD_THRESHOLD_HIGH);
          m_blockSCMThreshold = (mfxI32)DENOISE_FACTOR_CONVERSION(m_denoiseFactor, PAR_NRF_SCM_THRESHOLD_LOW, PAR_NRF_SCM_THRESHOLD_HIGH);
          m_numberOfMotionPixelsThreshold = (mfxU8)DENOISE_FACTOR_CONVERSION(m_denoiseFactor, PAR_NRF_NUM_MOTION_PIXELS_THRESHOLD_LOW, PAR_NRF_NUM_MOTION_PIXELS_THRESHOLD_HIGH);

          m_temp_diff_low     = (mfxI32)DENOISE_FACTOR_CONVERSION(m_denoiseFactor, PAR_NRF_LOW_TEMPORAL_PIXEL_DIFF_THRESHOLD_LOW,  PAR_NRF_LOW_TEMPORAL_PIXEL_DIFF_THRESHOLD_HIGH);
          m_temporal_diff_th  = (mfxU8)DENOISE_FACTOR_CONVERSION(m_denoiseFactor, PAR_NRF_TEMPORAL_PIXEL_DIFF_THRESHOLD_LOW, PAR_NRF_TEMPORAL_PIXEL_DIFF_THRESHOLD_HIGH);

          m_md_diff_th        = (mfxI32)DENOISE_FACTOR_CONVERSION(m_denoiseFactor, PAR_NRF_SUM_ABS_TEMPORAL_DIFF_THRESHOLD_LOW, PAR_NRF_SUM_ABS_TEMPORAL_DIFF_THRESHOLD_HIGH);
      }
  }

  /*printf("m_historyDelta = %i\n", m_historyDelta);
  printf("m_historyMax = %i\n", m_historyMax);
  printf("m_blockASDThreshold = %i\n", m_blockASDThreshold);
  printf("m_blockSCMThreshold = %i\n", m_blockSCMThreshold);
  printf("m_numberOfMotionPixelsThreshold = %i\n", m_numberOfMotionPixelsThreshold);
  printf("m_temp_diff_low = %i\n", m_temp_diff_low);
  printf("m_temporal_diff_th = %i\n", m_temporal_diff_th);
  printf("m_md_diff_th = %i\n", m_md_diff_th);*/

  return mfxSts;
}

mfxStatus MFXVideoVPPDenoise::Reset(mfxVideoParam *par)
{
  mfxStatus mfxSts = MFX_ERR_NONE;
  IppiSize  roiSize;
  mfxU32 memSize = 0;

  MFX_CHECK_NULL_PTR1( par );

  VPP_CHECK_NOT_INITIALIZED;

  // in case of DN, MUST BE in == out
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

  if( m_pRefSurface )
  {
    mfxSts = m_core->DecreaseReference( &(m_pRefSurface->Data) );
    MFX_CHECK_STS(mfxSts);

    m_pRefSurface = NULL;
  }

  /* reset denoise hist buffer */
  mfxSts = GetBufferSize( &memSize );
  MFX_CHECK_STS( mfxSts );

  if( memSize )
  {
      roiSize.height = m_errPrtctState.In.Height;
      roiSize.width  = m_errPrtctState.In.Width;

      ippsZero_8u(m_pHistoryWeight, ALIGN32(roiSize.height * roiSize.width));
      ippsZero_8u(m_stateY.memPool.pBordFrame, ALIGN32((roiSize.height + 4) * (roiSize.width + 4)));
      ippsZero_8u(m_stateY.memPool.pU8Blk0, ALIGN32(roiSize.height * roiSize.width));
      ippsZero_8u(m_stateY.memPool.pU8Blk1, ALIGN32(roiSize.height * roiSize.width));
      ippsZero_8u(m_stateY.memPool.pU8Blk2, ALIGN32(roiSize.height * roiSize.width));
      ippsZero_8u(m_stateY.memPool.pU8Blk3, ALIGN32(roiSize.height * roiSize.width));
      ippsZero_8u(m_stateY.memPool.pU8Blk4, ALIGN32(roiSize.height * roiSize.width));
      ippsZero_8u(m_stateY.memPool.pS16Blk0, ALIGN32(roiSize.height * roiSize.width * sizeof(mfxI16)));
      ippsZero_8u(m_stateY.memPool.pS16Blk1, ALIGN32(roiSize.height * roiSize.width * sizeof(mfxI16)));
  }

  // first frame processed independently
  m_stateY.firstFrame = 1;

  VPP_RESET;

  mfxExtBuffer* pHint = NULL;

  GetFilterParam(par, MFX_EXTBUFF_VPP_DENOISE, &pHint);

  SetParam( pHint );

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPDenoise::RunFrameVPP(mfxFrameSurface1 *in,
                                          mfxFrameSurface1 *out,
                                          InternalParam* pParam)
{
  mfxU8* pSrcY = NULL;
  mfxU8* pRefY = NULL;

  mfxU8* pSrcU = NULL;
  mfxU8* pRefU = NULL;

  mfxU8* pSrcV = NULL;
  mfxU8* pRefV = NULL;

  mfxU8* pDstY = NULL;
  mfxU8* pDstU = NULL;
  mfxU8* pDstV = NULL;

  mfxU16 srcW = 0, srcH = 0, dstW = 0, dstH = 0, srcPitch = 0;
  mfxI32 noiseLevel = 0;
  mfxU16 dstPitch = 0;
  mfxU16 refPitch = 0;

  IppiSize roiSize;
  // code error
  IppStatus ippSts = ippStsNoErr;
  mfxStatus mfxSts = MFX_ERR_NONE;
  // scale factors YV12
  mfxU16 ScaleCPitch  = 1;
  mfxU16 ScaleCWidth  = 1;
  mfxU16 ScaleCHeight = 1;

  mfxFrameData* inData  = NULL;
  mfxFrameData* outData = NULL;
  mfxFrameInfo* inInfo  = NULL;
  mfxFrameInfo* outInfo = NULL;

  mfxU16  cropX = 0, cropY = 0;

  mfxU32  inOffset0  = 0, inOffset1  = 0;
  mfxU32  outOffset0 = 0, outOffset1 = 0;

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

  /* step [1]: Width & height have to be the same as init */
  /* IN */
  VPP_GET_WIDTH(inInfo, srcW);
  VPP_GET_HEIGHT(inInfo, srcH);

  /* OUT */
  VPP_GET_WIDTH(outInfo, dstW);
  VPP_GET_HEIGHT(outInfo, dstH);

  /* algorithm */
  roiSize.height = srcH;
  roiSize.width  = srcW;

  VPP_GET_CROPX(inInfo, cropX);
  VPP_GET_CROPY(inInfo, cropY);
  inOffset0 = cropX      + cropY*inData->Pitch;
  inOffset1 = (cropX>>1) + (cropY>>1)*(inData->Pitch>>1);
  if( MFX_FOURCC_NV12 == m_errPrtctState.In.FourCC )// in == out(info) for DN
  {
    inOffset1 = (cropX) + (cropY>>1)*(inData->Pitch);
  }

  VPP_GET_CROPX(outInfo, cropX);
  VPP_GET_CROPY(outInfo, cropY);
  outOffset0 = cropX      + cropY*outData->Pitch;
  outOffset1 = (cropX>>1) + (cropY>>1)*(outData->Pitch>>1);
  if( MFX_FOURCC_NV12 == m_errPrtctState.In.FourCC )// in == out(info) for DN
  {
    outOffset1 = (cropX) + (cropY>>1)*(outData->Pitch);
  }

  /* special branch for first frame */
  if( !m_errPrtctState.isFirstFrameProcessed )
  {
    srcPitch = inData->Pitch;
    dstPitch = outData->Pitch;

    ippSts = ippiCopy_8u_C1R(inData->Y + inOffset0, srcPitch, outData->Y + outOffset0, dstPitch, roiSize);
    VPP_CHECK_IPP_STS( ippSts );

    if( MFX_FOURCC_NV12 == m_errPrtctState.In.FourCC )
    {
      ScaleCPitch  = 0;
      ScaleCWidth  = 1;
      ScaleCHeight = 1;
    }

    roiSize.height >>= ScaleCHeight;
    roiSize.width  >>= ScaleCWidth;
    srcPitch >>= ScaleCPitch;
    dstPitch >>= ScaleCPitch;

    if( MFX_FOURCC_YV12 == m_errPrtctState.In.FourCC )// in == out(info for DN)
    {
      ippSts = ippiCopy_8u_C1R(inData->U + inOffset1, srcPitch, outData->U + outOffset1, dstPitch, roiSize);
      VPP_CHECK_IPP_STS( ippSts );

      ippSts = ippiCopy_8u_C1R(inData->V + inOffset1, srcPitch, outData->V + outOffset1, dstPitch, roiSize);
      VPP_CHECK_IPP_STS( ippSts );
    }
    else if( MFX_FOURCC_NV12 == m_errPrtctState.In.FourCC )
    {
      /* patch */
      roiSize.width <<= 1;

      ippSts = ippiCopy_8u_C1R(inData->UV + inOffset1, srcPitch, outData->UV + outOffset1, dstPitch, roiSize);
      VPP_CHECK_IPP_STS( ippSts );
    }
    else
    {
      return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    m_stateY.gaussianThresholdY = m_stateY.nextNoiseLevel;
    m_stateY.nextNoiseLevel     = noiseLevel;
    m_stateY.firstFrame         = 0;//reset flag

    // this filter required prev processed frame for next iteration
    mfxSts = m_core->IncreaseReference( outData );
    MFX_CHECK_STS( mfxSts );
    m_pRefSurface = out;// save this pointer

    pParam->outPicStruct = pParam->inPicStruct;
    pParam->outTimeStamp = pParam->inTimeStamp;

    m_errPrtctState.isFirstFrameProcessed = true;

    return mfxSts;
  }

  /* algorithm need previous improved frame & current frame. */
  /* previous improved frame is located in out->Data. see code above */
      /* reference surfaces can be through MemId */
  bool bLockedRef = false;
  mfxFrameSurface1 localRefSurface;

  if (m_pRefSurface->Data.Y == 0)
  {
    mfxSts = m_core->LockExternalFrame(m_pRefSurface->Data.MemId, &(localRefSurface.Data));
    localRefSurface.Info = m_pRefSurface->Info;
    MFX_CHECK_STS(mfxSts);
    bLockedRef = true;
  }
  else
  {
    localRefSurface.Data = m_pRefSurface->Data;
    localRefSurface.Info = m_pRefSurface->Info;
  }

  roiSize.height = srcH;
  roiSize.width  = srcW;

  srcPitch = inData->Pitch;
  dstPitch = outData->Pitch;
  refPitch = localRefSurface.Data.Pitch;

  pSrcY = (mfxU8*)inData->Y + inOffset0;
  pRefY = (mfxU8*)localRefSurface.Data.Y+ inOffset0;
  MFX_CHECK_NULL_PTR2(pSrcY, pRefY);

  pSrcU = (mfxU8*)inData->U + inOffset1;
  pRefU = (mfxU8*)localRefSurface.Data.U+ inOffset1;
  MFX_CHECK_NULL_PTR2(pSrcU, pRefU);

  if( MFX_FOURCC_YV12 == m_errPrtctState.In.FourCC )
  {
    pSrcV = (mfxU8*)inData->V + inOffset1;
    pRefV = (mfxU8*)localRefSurface.Data.V+ inOffset1;
    MFX_CHECK_NULL_PTR2(pSrcV, pRefV);

    pDstV = (mfxU8*)outData->V+ outOffset1;
    MFX_CHECK_NULL_PTR1(pDstV);
  }

  pDstY = (mfxU8*)outData->Y + outOffset0;
  pDstU = (mfxU8*)outData->U + outOffset1;
  MFX_CHECK_NULL_PTR2(pDstY, pDstU);

  IppiSize bufSize = {roiSize.width + 4, roiSize.height + 4};
  mfxI32 bufStep = roiSize.width + 4;
  mfxU8 *pBuf = m_stateY.memPool.pBordFrame;

  ippiCopyReplicateBorder_8u_C1R(pSrcY, srcPitch, roiSize, pBuf, bufStep, bufSize, 2, 2);

  m_stateY.picStruct = pParam->inPicStruct;

#if 1
  if( m_bAutoAdjust )
  {
      m_stateY.gaussianThresholdY = m_stateY.nextNoiseLevel;
      mfxSts = ownGaussNoiseEstimation_8u_C1R_v2(roiSize, &noiseLevel, &m_stateY);
      MFX_CHECK_STS(mfxSts);

      m_stateY.nextNoiseLevel = (noiseLevel + m_stateY.nextNoiseLevel + 1) >> 1;
  }
  else
  {
      // set user configured noise level. scale by 100 need for algorithm
      m_stateY.gaussianThresholdY = m_internalDenoiseFactor * 100;
  }

#endif

  mfxSts = owniFilterDenoiseCAST_8u_C1R(pRefY, refPitch,
                                        roiSize,
                                        pDstY, dstPitch,
                                        &m_stateY);
  MFX_CHECK_STS(mfxSts);


  /* processing U surface */
  if( MFX_FOURCC_NV12 == m_errPrtctState.In.FourCC )
  {
    ScaleCPitch  = 0;
    ScaleCWidth  = 1;
    ScaleCHeight = 1;
  }

  srcW     >>= ScaleCWidth; // from spec.
  srcH     >>= ScaleCHeight;// from spec.
  srcPitch >>= ScaleCPitch; // from spec.
  roiSize.height = srcH;
  roiSize.width  = srcW;

  dstPitch >>= ScaleCPitch;

  if( MFX_FOURCC_YV12 == m_errPrtctState.In.FourCC )
  {
    /* processing U surface */
    ippSts = ippiCopy_8u_C1R( pSrcU, srcPitch, pDstU, dstPitch, roiSize);
    VPP_CHECK_IPP_STS( ippSts );

    /* processing V surface */
    ippSts = ippiCopy_8u_C1R( pSrcV, srcPitch, pDstV, dstPitch, roiSize);
    VPP_CHECK_IPP_STS( ippSts );
  }
  else if ( MFX_FOURCC_NV12 == m_errPrtctState.In.FourCC )
  {
    /* patch */
    roiSize.width <<= 1;
    /* processing interleaved U/V plane */
    ippSts = ippiCopy_8u_C1R( pSrcU, srcPitch, pDstU, dstPitch, roiSize);
    VPP_CHECK_IPP_STS( ippSts );
  }
  else
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* ************************************************ */
  /* important settings part for external application */
  /* ************************************************ */
  //Unlock External Frames
  if (bLockedRef)
  {
    m_core->UnlockExternalFrame(m_pRefSurface->Data.MemId, &(localRefSurface.Data));
    bLockedRef = false;
  }
  // this filter required prev processed frame for next iteration
  mfxSts = m_core->IncreaseReference( outData );
  MFX_CHECK_STS(mfxSts);

  mfxSts = m_core->DecreaseReference( &(m_pRefSurface->Data) );
  MFX_CHECK_STS(mfxSts);

  m_pRefSurface = out;

  pParam->outPicStruct = pParam->inPicStruct;
  pParam->outTimeStamp = pParam->inTimeStamp;

  return mfxSts;
}

mfxStatus MFXVideoVPPDenoise::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
{
  IppiSize  roiSize;
  mfxStatus mfxSts = MFX_ERR_NONE;
  mfxU16    srcW   = 0, srcH = 0;
  mfxU16    dstW   = 0, dstH = 0;

  MFX_CHECK_NULL_PTR2( In, Out );

  VPP_CHECK_MULTIPLE_INIT;

  /* robustness test */
  VPP_GET_REAL_WIDTH(In, srcW);
  VPP_GET_REAL_HEIGHT(In, srcH);

  /* OUT */
  VPP_GET_REAL_WIDTH(Out, dstW);
  VPP_GET_REAL_HEIGHT(Out, dstH);

  /* step [1]: width & height */
  if( (srcH != dstH) || (srcW != dstW) )
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* save init params to prevent core crash */
  m_errPrtctState.In  = *In;
  m_errPrtctState.Out = *Out;

  /* step [2]: FourCC */
  if( In->FourCC != Out->FourCC )
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  if( (In->FourCC != MFX_FOURCC_YV12) && (In->FourCC != MFX_FOURCC_NV12) )
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* Y plane */
  roiSize.height = srcH;
  roiSize.width  = srcW;

  mfxSts = owniFilterDenoiseCASTInit(&m_stateY, roiSize);
  MFX_CHECK_STS( mfxSts );

  /* U plane */
  // something to do

  /* V plane */
  // something to do

  m_pRefSurface = NULL;

  // NULL == set default parameters
  SetParam( NULL );

  VPP_INIT_SUCCESSFUL;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPDenoise::Close(void)
{
  mfxStatus mfxSts = MFX_ERR_NONE, localSts;

  VPP_CHECK_NOT_INITIALIZED;

  if( m_pRefSurface )
  {
    localSts = m_core->DecreaseReference( &(m_pRefSurface->Data) );
    VPP_CHECK_STS_CONTINUE(localSts, mfxSts);
    m_pRefSurface = NULL;
  }

  VPP_CLEAN;

  return mfxSts;
}


mfxStatus MFXVideoVPPDenoise::GetBufferSize( mfxU32* pBufferSize )
{
  IppiSize  roiSize;
  mfxStatus mfxSts;
  mfxU32    memSize = 0;

  VPP_CHECK_NOT_INITIALIZED;

  MFX_CHECK_NULL_PTR1(pBufferSize);

  /* Y plane only */
  roiSize.height = m_errPrtctState.In.Height;
  roiSize.width  = m_errPrtctState.In.Width;

  mfxSts = owniFilterDenoiseCASTGetSize(roiSize, &memSize);
  MFX_CHECK_STS( mfxSts );

  /* U plane */
  // something to do

  /* V plane */
  // something to do

  *pBufferSize = memSize;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPDenoise::SetBuffer( mfxU8* pBuffer )
{
  IppiSize  roiSize;
  mfxU32 memSize = 0;
  mfxStatus sts;

  VPP_CHECK_NOT_INITIALIZED;

  sts = GetBufferSize( &memSize );
  MFX_CHECK_STS( sts );

  if( memSize )
  {
    MFX_CHECK_NULL_PTR1(pBuffer);
    ippsZero_8u(pBuffer, memSize);  //sts ignored

    roiSize.height = m_errPrtctState.In.Height;
    roiSize.width  = m_errPrtctState.In.Width;

    m_pHistoryWeight = pBuffer;
    m_historyStep    = m_errPrtctState.In.Width;

    m_stateY.memPool.pBordFrame = m_pHistoryWeight + ALIGN32(roiSize.height * roiSize.width);
    m_stateY.memPool.pU8Blk0 = m_stateY.memPool.pBordFrame + ALIGN32((roiSize.height + 4) * (roiSize.width + 4));
    m_stateY.memPool.pU8Blk1 = m_stateY.memPool.pU8Blk0 + ALIGN32(roiSize.height * roiSize.width);
    m_stateY.memPool.pU8Blk2 = m_stateY.memPool.pU8Blk1 + ALIGN32(roiSize.height * roiSize.width);
    m_stateY.memPool.pU8Blk3 = m_stateY.memPool.pU8Blk2 + ALIGN32(roiSize.height * roiSize.width);
    m_stateY.memPool.pU8Blk4 = m_stateY.memPool.pU8Blk3 + ALIGN32(roiSize.height * roiSize.width);
    m_stateY.memPool.pS16Blk0 = m_stateY.memPool.pU8Blk4 + ALIGN32(roiSize.height * roiSize.width);
    m_stateY.memPool.pS16Blk1 = m_stateY.memPool.pS16Blk0 + ALIGN32(roiSize.height * roiSize.width * sizeof(mfxI16));

  }

  return MFX_ERR_NONE;
}

// function is called from sync part of VPP only
mfxStatus MFXVideoVPPDenoise::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
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

bool MFXVideoVPPDenoise::IsReadyOutput( mfxRequestType requestType )
{
    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
        // DN now is simple algorithm and need IN to produce OUT
    }

    return false;
}

#ifdef VPP_SSE
Ipp32u ownSigma_8u_w7(Ipp8u* pSrc, Ipp32u step, Ipp8u threshold, Ipp8u *res)
{
    static const Ipp32u q_table[9] = {2048, 1024, 682, 512, 409, 341, 292, 256, 227};

    __m128i xmmA, xmmB, xmmC, xmmD, xmmMask, xmmAcc, xmmCnt, xmmMskL;

    xmmMskL = _mm_set1_epi16(threshold+1);

    xmmB    = _mm_loadu_si128((__m128i*)(pSrc - 1));
    xmmC    = _mm_srli_si128(xmmB, 1);
    xmmC    = _mm_unpacklo_epi8(xmmC,_mm_xor_si128(xmmC,xmmC));

    xmmAcc  = xmmC;
    xmmCnt  = _mm_set1_epi16(1);

    xmmD    = _mm_xor_si128(xmmC,xmmC);
    xmmA    = _mm_unpacklo_epi8(xmmB,xmmD);
    xmmD    = _mm_subs_epi16(xmmA,xmmC);
    xmmMask = _mm_xor_si128(xmmC,xmmC);
    xmmMask = _mm_sub_epi16(xmmMask,xmmD);
    xmmD    = _mm_max_epi16(xmmD,xmmMask);
    xmmMask = _mm_cmplt_epi16(xmmD,xmmMskL);
    xmmD    = _mm_and_si128(xmmA,xmmMask);
    xmmAcc  = _mm_adds_epi16(xmmAcc,xmmD);
    xmmD    = _mm_srli_epi16(xmmMask,15);
    xmmCnt  = _mm_adds_epi16(xmmCnt,xmmD);

    xmmB    = _mm_srli_si128(xmmB, 2);

    xmmD    = _mm_xor_si128(xmmC,xmmC);
    xmmA    = _mm_unpacklo_epi8(xmmB,xmmD);
    xmmD    = _mm_subs_epi16(xmmA,xmmC);
    xmmMask = _mm_xor_si128(xmmC,xmmC);
    xmmMask = _mm_sub_epi16(xmmMask,xmmD);
    xmmD    = _mm_max_epi16(xmmD,xmmMask);
    xmmMask = _mm_cmplt_epi16(xmmD,xmmMskL);
    xmmD    = _mm_and_si128(xmmA,xmmMask);
    xmmAcc  = _mm_adds_epi16(xmmAcc,xmmD);
    xmmD    = _mm_srli_epi16(xmmMask,15);
    xmmCnt  = _mm_adds_epi16(xmmCnt,xmmD);

    xmmB    = _mm_loadu_si128((__m128i*)(pSrc - step - 1));

    xmmD    = _mm_xor_si128(xmmC,xmmC);
    xmmA    = _mm_unpacklo_epi8(xmmB,xmmD);
    xmmD    = _mm_subs_epi16(xmmA,xmmC);
    xmmMask = _mm_xor_si128(xmmC,xmmC);
    xmmMask = _mm_sub_epi16(xmmMask,xmmD);
    xmmD    = _mm_max_epi16(xmmD,xmmMask);
    xmmMask = _mm_cmplt_epi16(xmmD,xmmMskL);
    xmmD    = _mm_and_si128(xmmA,xmmMask);
    xmmAcc  = _mm_adds_epi16(xmmAcc,xmmD);
    xmmD    = _mm_srli_epi16(xmmMask,15);
    xmmCnt  = _mm_adds_epi16(xmmCnt,xmmD);

    xmmB    = _mm_srli_si128(xmmB, 1);

    xmmD    = _mm_xor_si128(xmmC,xmmC);
    xmmA    = _mm_unpacklo_epi8(xmmB,xmmD);
    xmmD    = _mm_subs_epi16(xmmA,xmmC);
    xmmMask = _mm_xor_si128(xmmC,xmmC);
    xmmMask = _mm_sub_epi16(xmmMask,xmmD);
    xmmD    = _mm_max_epi16(xmmD,xmmMask);
    xmmMask = _mm_cmplt_epi16(xmmD,xmmMskL);
    xmmD    = _mm_and_si128(xmmA,xmmMask);
    xmmAcc  = _mm_adds_epi16(xmmAcc,xmmD);
    xmmD    = _mm_srli_epi16(xmmMask,15);
    xmmCnt  = _mm_adds_epi16(xmmCnt,xmmD);

    xmmB    = _mm_srli_si128(xmmB, 1);

    xmmD    = _mm_xor_si128(xmmC,xmmC);
    xmmA    = _mm_unpacklo_epi8(xmmB,xmmD);
    xmmD    = _mm_subs_epi16(xmmA,xmmC);
    xmmMask = _mm_xor_si128(xmmC,xmmC);
    xmmMask = _mm_sub_epi16(xmmMask,xmmD);
    xmmD    = _mm_max_epi16(xmmD,xmmMask);
    xmmMask = _mm_cmplt_epi16(xmmD,xmmMskL);
    xmmD    = _mm_and_si128(xmmA,xmmMask);
    xmmAcc  = _mm_adds_epi16(xmmAcc,xmmD);
    xmmD    = _mm_srli_epi16(xmmMask,15);
    xmmCnt  = _mm_adds_epi16(xmmCnt,xmmD);

    xmmB    = _mm_loadu_si128((__m128i*)(pSrc + step - 1));

    xmmD    = _mm_xor_si128(xmmC,xmmC);
    xmmA    = _mm_unpacklo_epi8(xmmB,xmmD);
    xmmD    = _mm_subs_epi16(xmmA,xmmC);
    xmmMask = _mm_xor_si128(xmmC,xmmC);
    xmmMask = _mm_sub_epi16(xmmMask,xmmD);
    xmmD    = _mm_max_epi16(xmmD,xmmMask);
    xmmMask = _mm_cmplt_epi16(xmmD,xmmMskL);
    xmmD    = _mm_and_si128(xmmA,xmmMask);
    xmmAcc  = _mm_adds_epi16(xmmAcc,xmmD);
    xmmD    = _mm_srli_epi16(xmmMask,15);
    xmmCnt  = _mm_adds_epi16(xmmCnt,xmmD);

    xmmB    = _mm_srli_si128(xmmB, 1);

    xmmD    = _mm_xor_si128(xmmC,xmmC);
    xmmA    = _mm_unpacklo_epi8(xmmB,xmmD);
    xmmD    = _mm_subs_epi16(xmmA,xmmC);
    xmmMask = _mm_xor_si128(xmmC,xmmC);
    xmmMask = _mm_sub_epi16(xmmMask,xmmD);
    xmmD    = _mm_max_epi16(xmmD,xmmMask);
    xmmMask = _mm_cmplt_epi16(xmmD,xmmMskL);
    xmmD    = _mm_and_si128(xmmA,xmmMask);
    xmmAcc  = _mm_adds_epi16(xmmAcc,xmmD);
    xmmD    = _mm_srli_epi16(xmmMask,15);
    xmmCnt  = _mm_adds_epi16(xmmCnt,xmmD);

    xmmB    = _mm_srli_si128(xmmB, 1);

    xmmD    = _mm_xor_si128(xmmC,xmmC);
    xmmA    = _mm_unpacklo_epi8(xmmB,xmmD);
    xmmD    = _mm_subs_epi16(xmmA,xmmC);
    xmmMask = _mm_xor_si128(xmmC,xmmC);
    xmmMask = _mm_sub_epi16(xmmMask,xmmD);
    xmmD    = _mm_max_epi16(xmmD,xmmMask);
    xmmMask = _mm_cmplt_epi16(xmmD,xmmMskL);
    xmmD    = _mm_and_si128(xmmA,xmmMask);
    xmmAcc  = _mm_adds_epi16(xmmAcc,xmmD);
    xmmD    = _mm_srli_epi16(xmmMask,15);
    xmmCnt  = _mm_adds_epi16(xmmCnt,xmmD);

    xmmAcc  = _mm_adds_epi16(xmmAcc, _mm_srli_epi16(xmmCnt,1));
    xmmCnt  = _mm_subs_epi8(xmmCnt, _mm_set1_epi16(1));

    Ipp16u   *acc, *cnt;

    acc = (Ipp16u *)(&xmmAcc); cnt = (Ipp16u *)(&xmmCnt);
    for(int i = 0; i < 8; i++)
    {
        Ipp32u sum;

        sum = (acc[i]*q_table[cnt[i]])>>11;
        res[i] = (Ipp8u)sum;
    }

    return 0;
}

Ipp32s local_sad_diff_w7(Ipp8u *src, int srcStep)
{
    __m128i xmmA, xmmB, xmmC, xmmD, xmmE, xmmF;
    Ipp32u  *result;

    xmmA = _mm_set_epi32(*(int*)(src+srcStep+srcStep+srcStep), *(int*)(src+srcStep+srcStep), *(int*)(src+srcStep), *(int*)(src));
    xmmE = _mm_cmpeq_epi32(xmmA, xmmA);
    xmmF = _mm_srli_si128(xmmE, 4);
    xmmE = _mm_srli_epi32(xmmE, 8);

    xmmB = _mm_srli_si128(xmmA, 4);
    xmmC = _mm_and_si128(xmmA, xmmF);
    xmmD = _mm_sad_epu8(xmmB, xmmC);

    xmmB = _mm_srli_epi32(xmmA, 8);
    xmmC = _mm_and_si128(xmmA, xmmE);
    xmmA = _mm_sad_epu8(xmmB, xmmC);

    xmmC = _mm_add_epi32(xmmD, xmmA);

    result = (Ipp32u*)&xmmC;

    return result[0] + result[2];
}

Ipp32u ownLocalDiff_8u_w7(Ipp8u* pSrc, Ipp32u step, Ipp8u* pMed, Ipp8u* pDstSigma, Ipp8u* pDstMaxMin, Ipp32s len)
{
    const static __m128i zero = _mm_set1_epi32(0);

    int     i, mlen = len&~0x7;
    __m128i xmmA, xmmB, xmmM, xmmMed, xmmMin, xmmMax, xmmSgm;

    for(i=0; i<mlen; i+=8)
    {
        xmmMed  = _mm_loadu_si128((__m128i*)pMed);
        xmmMed  = _mm_unpacklo_epi8(xmmMed,zero);

        xmmA = _mm_loadu_si128((__m128i*)(pSrc-1));

        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = xmmB;
        xmmMax = xmmB;
        xmmSgm = xmmB;

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA = _mm_loadu_si128((__m128i*)(pSrc-1-step));
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA = _mm_loadu_si128((__m128i*)(pSrc-1+step));
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmMax = _mm_sub_epi16(xmmMax,xmmMin);
        xmmMax = _mm_packus_epi16(xmmMax, xmmMax);
        xmmSgm = _mm_packus_epi16(xmmSgm, xmmSgm);

        _mm_storel_epi64((__m128i*)pDstMaxMin, xmmMax);
        _mm_storel_epi64((__m128i*)(pDstSigma), xmmSgm);

        pSrc+=8; pMed+=8; pDstSigma+=8; pDstMaxMin+=8;
    }
    if(mlen < len)
    {
        xmmMed  = _mm_loadu_si128((__m128i*)pMed);
        xmmMed  = _mm_unpacklo_epi8(xmmMed,zero);

        xmmA = _mm_loadu_si128((__m128i*)(pSrc-1));

        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = xmmB;
        xmmMax = xmmB;
        xmmSgm = xmmB;

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA = _mm_loadu_si128((__m128i*)(pSrc-1-step));
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA = _mm_loadu_si128((__m128i*)(pSrc-1+step));
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmA   = _mm_srli_si128(xmmA, 1);
        xmmB   = _mm_unpacklo_epi8(xmmA, zero);
        xmmM   = _mm_sub_epi16(xmmMed,xmmB);
        xmmB   = _mm_sub_epi16(xmmB,xmmMed);
        xmmB   = _mm_max_epi16(xmmB,xmmM);
        xmmMin = _mm_min_epi16(xmmMin, xmmB);
        xmmMax = _mm_max_epi16(xmmMax, xmmB);
        xmmSgm = _mm_add_epi16(xmmSgm, xmmB);

        xmmMax = _mm_sub_epi16(xmmMax,xmmMin);
        xmmMax = _mm_packus_epi16(xmmMax, xmmMax);
        xmmSgm = _mm_packus_epi16(xmmSgm, xmmSgm);

        Ipp8u *mm = (Ipp8u*)&xmmMax, *s = (Ipp8u*)&xmmSgm;
        for(i=mlen; i<len; i++) { *pDstMaxMin++ = *mm++; *pDstSigma++ = *s++; }
    }
    return 0;
}

#else

Ipp32u ownSigma_8u(Ipp8u* pSrc, Ipp32u step, Ipp8u threshold, Ipp8u *res)
{
    Ipp32u sum = 0, weight = 0, diff;
    static const Ipp32u q_table[9] = {2048, 1024, 682, 512, 409, 341, 292, 256, 227};

    for (int k = 0; k < 8; k++)
    {
        sum = 0;
        weight = 0;
        for (int j = -1; j < 2; j++)
        {
            for (int i = -1; i < 2; i++)
            {
                diff = abs(pSrc[k+(int)(step*j)+i] - pSrc[k]);
                if (diff <= threshold)
                {
                    sum += pSrc[k+(int)(step*j)+i];
                    weight++;
                }
            }
        }

        sum += (weight >> 1);
        res[k] = (Ipp8u)((sum * q_table[weight-1]) >> 11);
    }

    return 0;
}

Ipp32u ownLocalDiff_8u(Ipp8u* pSrc, Ipp32u step, Ipp8u* pMed, Ipp8u* pDstSigma, Ipp8u* pDstMaxMin, Ipp32s len)
{
    int i = 0, j = 0, x = 0;
    int sigma, block_max, block_min, med_diff;

    for (x = 0; x < len; x++)
    {
        sigma = block_max = 0;
        block_min = 255;
        for(i = -1; i <= 1; i++)
        {
            for(j = -1; j <= 1; j++)
            {
                med_diff = abs(pMed[x] - pSrc[(int)(i * step) + j + x]);

                if(med_diff > block_max)
                    block_max = med_diff;
                else if(med_diff < block_min)
                    block_min = med_diff;

                sigma += med_diff;
            }
        }
        pDstSigma[x] = (sigma > 255) ? 255 : (Ipp8u)sigma;
        pDstMaxMin[x] = (Ipp8u)(block_max - block_min);
    }

    return 0;
}

#endif

/* ******************************************************************** */
/*                 implementation of algorithms [Denoise]               */
/* ******************************************************************** */

mfxStatus MFXVideoVPPDenoise::owniFilterDenoiseCAST_8u_C1R(const mfxU8* pRef,
                                                           mfxI32 refStep,
                                                           IppiSize roiSize,
                                                           mfxU8* pDst,
                                                           mfxI32 dstStep,
                                                           owniDenoiseCASTState* pState)
{
    mfxI32 local_max, local_min, t_diff;
    mfxU8* ref_b8x8;
    mfxU8* src_b8x8;
    mfxI32 temporal_denoised;
    mfxU8 *mv_history;
    mfxU8 gaussian_th;
    mfxU8 num_motion_pixel, motion_block;
    mfxU8 history_weight;
    mfxI32 mb_diff, mb_asd, mb_scm;
    const mfxI32 q_table[16] = {1024, 512, 341, 256, 205, 171, 146, 128, 114, 102, 93, 85, 79, 73, 68, 64};
    static const mfxI32 q_tab_sigma[9] = {2048, 1024, 682, 512, 409, 341, 292, 256, 227};

    /* param */
    mfxI32 noise_level    = pState->gaussianThresholdY;

    mfxU8 *pBuf = NULL, *pImgMin = NULL, *pImgMax = NULL, *pRefAbsDif = NULL/*, *pSigma = NULL*/;
#ifndef VPP_SSE
    /*
    mfxU8 *pHDiff = NULL, *pVDiff = NULL;
    mfxI16 *pHDiff_s16 = NULL, *pVDiff_s16 = NULL;
    */
#endif
    mfxI32 bufStep = roiSize.width + 4;
    IppiSize mask={3,3};
    IppiPoint anchor = {1,1};
    mfxI32 i = 0, j = 0;

    pBuf = pState->memPool.pBordFrame + bufStep * 2 + 2;
    pImgMin = pState->memPool.pU8Blk0;
    pImgMax = pState->memPool.pU8Blk1;
    pRefAbsDif = pState->memPool.pU8Blk2;
    //    pSigma = pState->memPool.pU8Blk3;
#ifndef VPP_SSE
    /*
    pHDiff = pState->memPool.pU8Blk3;
    pVDiff = pState->memPool.pU8Blk4;
    pHDiff_s16 = (mfxI16 *)(pState->memPool.pS16Blk0);
    pVDiff_s16 = (mfxI16 *)(pState->memPool.pS16Blk1);
    */
#endif

    /* search for local min and max */
    if(MFX_PICSTRUCT_PROGRESSIVE & pState->picStruct)
    {
        ippiFilterMin_8u_C1R(pBuf, bufStep, pImgMin, roiSize.width, roiSize, mask, anchor);
        ippiFilterMax_8u_C1R(pBuf, bufStep, pImgMax, roiSize.width, roiSize, mask, anchor);
        ippiAbsDiff_8u_C1R(pBuf, bufStep, pRef, refStep, pRefAbsDif, roiSize.width, roiSize);
    }
    else
    {   /* interlaced */
        IppiSize isize;
        isize.width = roiSize.width;
        isize.height = roiSize.height >> 1;

        /* extract borders for interlaced frame */
        ippsCopy_8u(pBuf + bufStep - 2, pBuf - bufStep - 2, roiSize.width + 4);
        ippsCopy_8u(pBuf + (roiSize.height - 2) * bufStep - 2, pBuf + roiSize.height * bufStep - 2, roiSize.width + 4);

        ippiFilterMin_8u_C1R(pBuf, bufStep * 2, pImgMin, roiSize.width * 2, isize, mask, anchor);
        ippiFilterMin_8u_C1R(pBuf + bufStep, bufStep * 2, pImgMin + roiSize.width, roiSize.width * 2, isize, mask, anchor);
        ippiFilterMax_8u_C1R(pBuf, bufStep * 2, pImgMax, roiSize.width * 2, roiSize, mask, anchor);
        ippiFilterMax_8u_C1R(pBuf + bufStep, bufStep * 2, pImgMax + roiSize.width, roiSize.width * 2, isize, mask, anchor);
        ippiAbsDiff_8u_C1R(pBuf, bufStep, pRef, refStep, pRefAbsDif, roiSize.width, roiSize);
    }

    if(noise_level < m_strongNoiseLevel)
    {
        gaussian_th = 4;

        if(noise_level <= m_lowNoiseLevel)
        {
            gaussian_th = 1;
        }

        if(MFX_PICSTRUCT_PROGRESSIVE & pState->picStruct)
        {
            /* spatial_denoised is stored in dst */
            for(j = 0; j < roiSize.height; j++)
            {
                for(i = 0; i < roiSize.width; i += 8)
                {
#ifdef VPP_SSE
                    ownSigma_8u_w7(pBuf + bufStep * j + i, bufStep, gaussian_th, pDst+dstStep * j + i);
#else
                    ownSigma_8u(pBuf + bufStep * j + i, bufStep, gaussian_th, pDst+dstStep * j + i);
#endif
                }
            }

        }
        else
        {   /* interlaced */
            /* spatial_denoised is stored in dst */
            /* top field */
            for(j = 0; j < roiSize.height; j += 2)
            {
                for(i = 0; i < roiSize.width; i += 8)
                {
#ifdef VPP_SSE
                    ownSigma_8u_w7(pBuf + bufStep * j + i, bufStep * 2, gaussian_th, pDst+dstStep * j + i);
#else
                    ownSigma_8u(pBuf + bufStep * j + i, bufStep * 2, gaussian_th, pDst+dstStep * j + i);
#endif
                }
            }
            /* bottom field */
            for(j = 1; j < roiSize.height; j += 2)
            {
                for(i = 0; i < roiSize.width; i += 8)
                {
#ifdef VPP_SSE
                    ownSigma_8u_w7(pBuf + bufStep * j + i, bufStep * 2, gaussian_th, pDst+dstStep * j + i);
#else
                    ownSigma_8u(pBuf + bufStep * j + i, bufStep * 2, gaussian_th, pDst+dstStep * j + i);
#endif
                }
            }
        }

        for(mfxI32 y = 0; y < roiSize.height; y += m_blockHeightDN)
        {
            for(mfxI32 x = 0; x < roiSize.width; x += m_blockWidthDN)
            {
                mv_history = m_pHistoryWeight + y * m_historyStep + x;

                for(mfxI32 v = 0; v < m_blockHeightDN; v++)
                {
                    for(mfxI32 h = 0; h < m_blockWidthDN; h++)
                    {

                        if(mv_history[m_historyStep * v + h] >= 128)
                        {
                            mv_history[m_historyStep * v + h] >>= 1;
                        }
                        else
                        {
                            mv_history[m_historyStep * v + h] = 0;
                        }
                    }
                }
            }
        }

    }
    else
    {
        history_weight   = 128;
        //(Eric): division is once per frame, keep as integer division in HW
        gaussian_th      = (mfxU8)IPP_MIN((noise_level/100), 12);

        if(MFX_PICSTRUCT_PROGRESSIVE & pState->picStruct)
        {
            /* spatial_denoised is stored in dst */
            for(j = 0; j < roiSize.height; j++)
            {
                for(i = 0; i < roiSize.width; i += 8)
                {
#ifdef VPP_SSE
                    ownSigma_8u_w7(pBuf + bufStep * j + i, bufStep, gaussian_th, pDst+dstStep * j + i);
#else
                    ownSigma_8u(pBuf + bufStep * j + i, bufStep, gaussian_th, pDst+dstStep * j + i);
#endif
                }
            }
        }
        else
        {   /* interlaced */
            /* spatial_denoised is stored in dst */
            /* top field */
            for(j = 0; j < roiSize.height; j += 2)
            {
                for(i = 0; i < roiSize.width; i += 8)
                {
#ifdef VPP_SSE
                    ownSigma_8u_w7(pBuf + bufStep * j + i, bufStep * 2, gaussian_th, pDst+dstStep * j + i);
#else
                    ownSigma_8u(pBuf + bufStep * j + i, bufStep * 2, gaussian_th, pDst+dstStep * j + i);
#endif
                }
            }
            /* bottom field */
            for(j = 1; j < roiSize.height; j += 2)
            {
                for(i = 0; i < roiSize.width; i += 8)
                {
#ifdef VPP_SSE
                    ownSigma_8u_w7(pBuf + bufStep * j + i, bufStep * 2, gaussian_th, pDst+dstStep * j + i);
#else
                    ownSigma_8u(pBuf + bufStep * j + i, bufStep * 2, gaussian_th, pDst+dstStep * j + i);
#endif
                }
            }
        }

#ifndef VPP_SSE
        /*
        IppiSize newRoi;
        newRoi.width = roiSize.width - 1;
        newRoi.height = roiSize.height;
        ippiAbsDiff_8u_C1R(pBuf, bufStep, pBuf+1, bufStep, pHDiff, roiSize.width, newRoi);
        newRoi.width = roiSize.width;
        newRoi.height = roiSize.height - 1;
        ippiAbsDiff_8u_C1R(pBuf, bufStep, pBuf+bufStep, bufStep, pVDiff, roiSize.width, newRoi);

        ippiConvert_8u16s_C1R(pHDiff, roiSize.width, pHDiff_s16, roiSize.width * sizeof(mfxI16), roiSize);
        ippiConvert_8u16s_C1R(pVDiff, roiSize.width, pVDiff_s16, roiSize.width * sizeof(mfxI16), roiSize);

        mfxI16 *pHLine, *pVLine;
        */
#endif
        for(mfxI32 y = 0; y < roiSize.height; y += m_blockHeightDN)
        {
            /* perform 1 row of MBs */
#ifndef VPP_SSE
            /*
            pHLine = pHDiff_s16 + y * roiSize.width;
            pVLine = pVDiff_s16 + y * roiSize.width;
            for (i = 1; i < pState->m_blockHeightDN; i++)
            {   / * horizont diffs for each MB* /
                ippsAdd_16s_I(pHLine + i * roiSize.width, pHLine, roiSize.width);
            }
            for (i = pState->m_blockWidthDN - 1; i < roiSize.width; i+= pState->m_blockWidthDN)
            {
                pHLine[i] = 0;
            }
            for (i = 1; i < pState->m_blockHeightDN - 1; i++)
            {   / * 15 horizont diffs for each MB* /
                ippsAdd_16s_I(pVLine + i * roiSize.width, pVLine, roiSize.width);
            }
            ippsAdd_16s_I(pHLine, pVLine, roiSize.width);
            */
#endif

            for(mfxI32 x = 0; x < roiSize.width; x += m_blockWidthDN)
            {
                ref_b8x8   = (mfxU8*)pRef + y*refStep + x;
                src_b8x8   = (mfxU8*)pBuf + y * bufStep + x;
                mv_history = m_pHistoryWeight + y*m_historyStep + x;

                num_motion_pixel=0;
                motion_block=0;
                mb_diff=0;
                mb_asd=0;

                for(mfxI32 v = 0; v < m_blockHeightDN; ++v)
                {
                    for(mfxI32 h = 0; h < m_blockWidthDN; ++h)
                    {
                        mfxI32 diff = src_b8x8[bufStep*v+h]-ref_b8x8[refStep*v+h];
                        mfxU8 abs_diff = pRefAbsDif[roiSize.width * (y+v) + (x+h)];

                        if(abs_diff > m_temporal_diff_th)
                        {
                            ++num_motion_pixel;
                        }

                        mb_diff += abs_diff;
                        mb_asd  += diff;
                    }
                }
                mb_asd = abs(mb_asd);

                mb_scm=0;

#ifdef VPP_SSE
                mb_scm = local_sad_diff_w7(src_b8x8, bufStep);
#else
                for(i = 0; i < m_blockHeightDN; i++)
                {
                    for(j=0; j < m_blockWidthDN-1; j++)
                    {
                        mb_scm += abs(src_b8x8[bufStep * i + j] - src_b8x8[bufStep * i + j + 1]);
                    }
                }

                for(i = 0; i < m_blockHeightDN-1; ++i)
                {
                    for(j=0; j < m_blockWidthDN; ++j)
                    {
                        mb_scm += abs(src_b8x8[bufStep * i + j] - src_b8x8[bufStep *(i+1) + j]);
                    }
                }
                /*
                for (i = 0; i < m_blockWidthDN; i++)
                mb_scm += pVLine[x + i];
                */
#endif

                if((num_motion_pixel > m_numberOfMotionPixelsThreshold) || (mb_diff > m_md_diff_th))
                {
                    for(mfxI32 v = 0; v < m_blockHeightDN; ++v)
                    {
                        for(mfxI32 h = 0; h < m_blockWidthDN; ++h)
                        {
                            if(mv_history[m_historyStep * v + h] >= 128)
                            {
                                mv_history[m_historyStep * v + h] >>= 1;
                            }
                            else
                            {
                                mv_history[m_historyStep * v + h] = 0;
                            }
                        }
                    }
                }
                else
                {
                    for(mfxI32 v = 0; v < m_blockHeightDN; ++v)
                    {
                        mfxU8* pOutBlock = pDst + (y+v)*dstStep+x;

                        for(mfxI32 h = 0; h < m_blockWidthDN; ++h)
                        {
                            if(mb_asd > m_blockASDThreshold && mb_scm <m_blockSCMThreshold)
                            {
                                history_weight = 128;
                                mv_history[m_historyStep * v + h] = history_weight;
                            }
                            else
                            {
                                history_weight = mv_history[m_historyStep * v + h];

                                if(history_weight < 128)
                                {
                                    mv_history[m_historyStep * v + h] = 128;
                                    history_weight = 128;
                                }
                                else if(history_weight < (m_historyMax - m_historyDelta))
                                {
                                    history_weight = (Ipp8u)(history_weight + m_historyDelta); /* u8 is enough */
                                    mv_history[m_historyStep * v + h] = history_weight;
                                }
                            }

                            local_min = *(pImgMin + roiSize.width * (y+v) + (x+h));
                            local_max = *(pImgMax + roiSize.width * (y+v) + (x+h));

                            //                          spatial_denoised = pSigma[roiSize.width * (y+v) + (x+h)];

                            temporal_denoised = ((256-history_weight) * (mfxI32)src_b8x8[bufStep*v+h] +
                                history_weight * ref_b8x8[refStep*v+h] + 128) >> 8;

                            if(temporal_denoised >= local_max)
                            {
                                temporal_denoised = (temporal_denoised + local_max) >> 1;
                            }

                            if(temporal_denoised <= local_min)
                            {
                                temporal_denoised = (temporal_denoised + local_min) >> 1;
                            }

                            t_diff = pRefAbsDif[roiSize.width * (y+v) + (x+h)];
                            /* pOutBlock keeps already spatial_denoised */
                            if(t_diff < m_temp_diff_low)
                            {    // <8
                                pOutBlock[h] = (mfxU8)temporal_denoised;
                            }
                            else
                            {
                                if(t_diff < m_temporal_diff_th)
                                { // <12
                                    mfxI32 val = pOutBlock[h] * (t_diff - m_temp_diff_low) +
                                        temporal_denoised * (m_temporal_diff_th - t_diff) +
                                        ((m_temporal_diff_th - m_temp_diff_low) >> 1);

                                    //note denom is 4 bit -> used a table instead of division
                                    //mfxI32 denoise_blend = (val * q_table[m_temporal_diff_th-m_temp_diff_low - 1]) >> 10;
                                    int indx_q_table = (int)(m_temporal_diff_th-m_temp_diff_low - 1);
                                    indx_q_table = VPP_RANGE_CLIP(indx_q_table, 0, 15); // size(elemen count) of q_table
                                    pOutBlock[h] = (mfxU8)((val * q_table[indx_q_table]) >> 10);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return MFX_ERR_NONE;
}


/* ************************************************************************ */

mfxStatus MFXVideoVPPDenoise::owniFilterDenoiseCASTGetSize(IppiSize roiSize, mfxU32* pMemSize)
{
  MFX_CHECK_NULL_PTR1(pMemSize);

  if(!roiSize.height || !roiSize.width)
  {
    return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM; //width or height == NULL
  }

  /* required memory */
  mfxI32 sizeHistBuf = ALIGN32(roiSize.height * roiSize.width);
  mfxI32 sizeWorkMemBuf = ALIGN32((roiSize.height + 4) * (roiSize.width + 4)) +
                          ALIGN32(roiSize.height * roiSize.width) * 5 +
                          ALIGN32(roiSize.height * roiSize.width * sizeof(mfxI16)) * 2;

  *pMemSize = sizeHistBuf + sizeWorkMemBuf;

  return MFX_ERR_NONE;
}

/* ************************************************************************ */

mfxStatus MFXVideoVPPDenoise::owniFilterDenoiseCASTInit(owniDenoiseCASTState* pState, IppiSize roiSize)
{
  IppStatus ippSts = ippStsNoErr;

  MFX_CHECK_NULL_PTR1( pState );

  roiSize;

  ippSts = ippsZero_8u( (mfxU8*)pState,  sizeof(owniDenoiseCASTState) );
  VPP_CHECK_IPP_STS( ippSts );

   (pState)->gaussianThresholdY            = 20;//12;

  /* work buffers for denoise algorithms */
  (pState)->memPool.pBordFrame = NULL;
  (pState)->memPool.pS16Blk0 = NULL;
  (pState)->memPool.pS16Blk1 = NULL;
  (pState)->memPool.pU8Blk0 = NULL;
  (pState)->memPool.pU8Blk1 = NULL;
  (pState)->memPool.pU8Blk2 = NULL;
  (pState)->memPool.pU8Blk3 = NULL;
  (pState)->memPool.pU8Blk4 = NULL;

  /* additional parameters */
  /* noise level initialization */
  (pState)->nextNoiseLevel = 0;

  /* first frame is important */
  (pState)->firstFrame = 1;

  return MFX_ERR_NONE;
}

/* ************************************************************************ */

mfxStatus MFXVideoVPPDenoise::ownGaussNoiseEstimation_8u_C1R_v2(IppiSize size, mfxI32* pThres,
                                                                owniDenoiseCASTState* pState)
{
    int num_sigma_mb,sigma_mb_min_all;

    mfxU8 *tmpOut0;
    mfxU8 *tmpOut1;
    mfxU8 *blkSigma;
    mfxU8 *blkMaxMin;
    mfxU8 *medianDif;
    IppiPoint medianAnchor = {1,1};
    IppiSize medianMask = {3,3};
    IppiSize blk_size;
    mfxU8 blkSigmaMin = 0;
    blk_size.width = m_blockWidthNE;
    blk_size.height = m_blockHeightNE;
    mfxU8 *pBuf = NULL;

    mfxI16 *tmpOut0_16;
    mfxI16 *tmpOut1_16;

    mfxI32 bufStep = size.width + 4;

    pBuf = pState->memPool.pBordFrame + bufStep * 2 + 2;
    tmpOut0 = pState->memPool.pU8Blk0;
    tmpOut1 = pState->memPool.pU8Blk1;
    blkSigma = pState->memPool.pU8Blk2;
    blkMaxMin = pState->memPool.pU8Blk3;
    medianDif = pState->memPool.pU8Blk4;
    tmpOut0_16 = (mfxI16 *)(pState->memPool.pS16Blk0);
    tmpOut1_16 = (mfxI16 *)(pState->memPool.pS16Blk1);

    sigma_mb_min_all=0;
    num_sigma_mb=0;

    if(MFX_PICSTRUCT_PROGRESSIVE & pState->picStruct)
    {
        ippiFilterSobelHoriz_8u16s_C1R(pBuf, bufStep, tmpOut0_16, size.width * sizeof(mfxI16), size, ippMskSize3x3);
        ippiFilterSobelVert_8u16s_C1R(pBuf, bufStep, tmpOut1_16, size.width * sizeof(mfxI16), size, ippMskSize3x3);
        ippiAbs_16s_C1IR(tmpOut0_16, size.width * sizeof(mfxI16), size);
        ippiAbs_16s_C1IR(tmpOut1_16, size.width * sizeof(mfxI16), size);
        ippiAdd_16s_C1IRSfs(tmpOut0_16, size.width * sizeof(mfxI16), tmpOut1_16, size.width * sizeof(mfxI16), size, 0);

        ippiFilterMedian_8u_C1R(pBuf, bufStep, tmpOut0, size.width, size, medianMask, medianAnchor);

        for(int y = 0; y < size.height; y++)
        {
#ifdef VPP_SSE
            ownLocalDiff_8u_w7(pBuf + y*bufStep, bufStep, tmpOut0+y*size.width, blkSigma+y*size.width, blkMaxMin+y*size.width, size.width);
#else
            ownLocalDiff_8u(pBuf + y*bufStep, bufStep, tmpOut0+y*size.width, blkSigma+y*size.width, blkMaxMin+y*size.width, size.width);
#endif
        }

        ippiCompareC_16s_C1R (tmpOut1_16, size.width * sizeof(mfxI16), (Ipp16s)m_sobelEdgeThreshold, tmpOut0, size.width, size, ippCmpLess);
        ippiCompareC_8u_C1R (blkMaxMin, size.width, (Ipp8u)m_localDiffThreshold, tmpOut1, size.width, size, ippCmpLess);
        ippiAnd_8u_C1IR(tmpOut0, size.width, tmpOut1, size.width, size);
        ippiNot_8u_C1IR(tmpOut1, size.width, size); /* invert mask to set unused values to 0xFF */
        ippiOr_8u_C1IR(tmpOut1, size.width, blkSigma, size.width, size);

        /* collect all mins for each block */
        for(int y = 0; y < size.height; y += m_blockHeightNE)
        {
            for(int x = 0; x < size.width; x += m_blockWidthNE)
            {
                ippiMin_8u_C1R(blkSigma+y*size.width+x, size.width, blk_size, &blkSigmaMin);

                if (blkSigmaMin < 255)
                {
                    sigma_mb_min_all += blkSigmaMin;
                    num_sigma_mb++;
                }

            }
        }

    }
    else
    {   /* interlaced */
        IppiSize isize;
        isize.width = size.width;
        isize.height = size.height >> 1;

        /* extract borders for interlaced frame */
        ippsCopy_8u(pBuf + bufStep - 2, pBuf - bufStep - 2, size.width + 4);
        ippsCopy_8u(pBuf + (size.height - 2) * bufStep - 2, pBuf + size.height * bufStep - 2, size.width + 4);

        ippiFilterSobelHoriz_8u16s_C1R(pBuf, bufStep * 2, tmpOut0_16, size.width * sizeof(mfxI16) * 2, isize, ippMskSize3x3);
        ippiFilterSobelHoriz_8u16s_C1R(pBuf + bufStep, bufStep * 2, tmpOut0_16 + size.width, size.width * sizeof(mfxI16) * 2, isize, ippMskSize3x3);
        ippiFilterSobelVert_8u16s_C1R(pBuf, bufStep * 2, tmpOut1_16, size.width * sizeof(mfxI16) * 2, isize, ippMskSize3x3);
        ippiFilterSobelVert_8u16s_C1R(pBuf + bufStep, bufStep * 2, tmpOut1_16 + size.width, size.width * sizeof(mfxI16) * 2, isize, ippMskSize3x3);
        ippiAbs_16s_C1IR(tmpOut0_16, size.width * sizeof(mfxI16), size);
        ippiAbs_16s_C1IR(tmpOut1_16, size.width * sizeof(mfxI16), size);
        ippiAdd_16s_C1IRSfs(tmpOut0_16, size.width * sizeof(mfxI16), tmpOut1_16, size.width * sizeof(mfxI16), size, 0);

        ippiFilterMedian_8u_C1R(pBuf, bufStep * 2, tmpOut0, size.width * 2, isize, medianMask, medianAnchor);
        ippiFilterMedian_8u_C1R(pBuf + bufStep, bufStep * 2, tmpOut0 + size.width, size.width * 2, isize, medianMask, medianAnchor);

        for(int y = 0; y < size.height; y += 2)
        {
#ifdef VPP_SSE
            ownLocalDiff_8u_w7(pBuf + y*bufStep, bufStep*2, tmpOut0+y*size.width, blkSigma+y*size.width, blkMaxMin+y*size.width, size.width);
#else
            ownLocalDiff_8u(pBuf + y*bufStep, bufStep*2, tmpOut0+y*size.width, blkSigma+y*size.width, blkMaxMin+y*size.width, size.width);
#endif
        }

        for(int y = 1; y < size.height; y += 2)
        {
#ifdef VPP_SSE
            ownLocalDiff_8u_w7(pBuf + y*bufStep, bufStep*2, tmpOut0+y*size.width, blkSigma+y*size.width, blkMaxMin+y*size.width, size.width);
#else
            ownLocalDiff_8u(pBuf + y*bufStep, bufStep*2, tmpOut0+y*size.width, blkSigma+y*size.width, blkMaxMin+y*size.width, size.width);
#endif
        }

        ippiCompareC_16s_C1R (tmpOut1_16, size.width * sizeof(mfxI16), (Ipp16s)m_sobelEdgeThreshold, tmpOut0, size.width, size, ippCmpLess);
        ippiCompareC_8u_C1R (blkMaxMin, size.width, (Ipp8u)m_localDiffThreshold, tmpOut1, size.width, size, ippCmpLess);
        ippiAnd_8u_C1IR(tmpOut0, size.width, tmpOut1, size.width, size);
        ippiNot_8u_C1IR(tmpOut1, size.width, size); /* invert mask to set unused values to 0xFF */
        ippiOr_8u_C1IR(tmpOut1, size.width, blkSigma, size.width, size);

        /* collect all mins for each block */
        IppiSize iblk_size;
        iblk_size.width = blk_size.width;
        iblk_size.height = blk_size.height >> 1;

        for(int y = 0; y < size.height; y += m_blockHeightNE)
        {
            for(int x = 0; x < size.width; x += m_blockWidthNE)
            {
                //top field
                ippiMin_8u_C1R(blkSigma+y*size.width+x, size.width*2, iblk_size, &blkSigmaMin);

                if (blkSigmaMin < 255)
                {
                    sigma_mb_min_all += blkSigmaMin;
                    num_sigma_mb++;
                }
                //bottom field
                ippiMin_8u_C1R(blkSigma+(y+1)*size.width+x, size.width*2, iblk_size, &blkSigmaMin);

                if (blkSigmaMin < 255)
                {
                    sigma_mb_min_all += blkSigmaMin;
                    num_sigma_mb++;
                }

            }
        }

    }

    //(Eric): division is once per frame, keep as integer division in HW
    pThres[0] = (int) (sigma_mb_min_all*100) / (num_sigma_mb+1);

    return MFX_ERR_NONE;
}

/* ************************************************************************ */

#endif // MFX_ENABLE_HW_ONLY_VPP
#endif // MFX_ENABLE_VPP

/* EOF */
