/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2014 Intel Corporation. All Rights Reserved.
//
//
//          ColorSpaceConversion Video Pre\Post Processing
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#include "mfx_color_space_conversion_vpp.h"
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"


#define AVX_DISP(func, ...)   func(__VA_ARGS__);

/*
 Disabled at the moment. Need to move avx2 related optimizations to the 
 separate VS project and use Intel compiler for it.
#if defined(_WIN32) || defined(_WIN64)
 // Set of AVX2 optimized color conversions
 #include "mfx_color_space_conversion_vpp_avx2.cpp"
 #define AVX_DISP(func, ...)  (( m_bAVX2 ) ? func ## _avx2(__VA_ARGS__) : func(__VA_ARGS__))
#endif
*/

/* some macros */
#ifndef CHOP
#define CHOP(x)     ((x < 0) ? 0 : ((x > 255) ? 255 : x))
#endif

#ifndef CHOP10
#define CHOP10(x)     ((x < 0) ? 0 : ((x > 1023) ? 1023 : x))
#endif

#ifndef V_POS
#define V_POS(ptr, shift, step, pos) ( (pos) >= 0  ? ( ptr + (shift) + (step) * (pos) )[0] : ( ptr + (shift) - (step) * (-1*pos) )[0])
#endif

/* ******************************************************************** */
/*                 prototype of CSC algorithms                          */
/* ******************************************************************** */
IppStatus cc_P010_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo,
                           mfxFrameData* yv12Data);

IppStatus cc_P210_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo,
                           mfxFrameData* yv12Data);

IppStatus cc_P210_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo,
                           mfxFrameData* yv12Data);

#if defined(_WIN32) || defined(_WIN64)
IppStatus cc_P010_to_A2RGB10_avx2( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                                   mfxFrameData* outData, mfxFrameInfo* outInfo);
IppStatus cc_P210_to_A2RGB10_avx2( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                                   mfxFrameData* outData, mfxFrameInfo* outInfo);
#endif
IppStatus cc_P010_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                                          mfxFrameData* outData, mfxFrameInfo* outInfo);
IppStatus cc_P210_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                                          mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_P010_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_P010_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_NV12_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_NV16_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_YV12_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_YUY2_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_NV16_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_NV12_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_RGB3_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo, mfxU16 picStruct );

IppStatus cc_RGB4_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo, mfxU16 picStruct );

IppStatus cc_NV12_to_YUY2( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

IppStatus cc_NV12_to_RGB4( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo,
                           mfxFrameData* yv12Data);

// internal feature for development
IppStatus cc_NV12_to_YV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo);

/* ******************************************************************** */
/*                 implementation of VPP filter [ColorSpaceConversion]  */
/* ******************************************************************** */

MFXVideoVPPColorSpaceConversion::MFXVideoVPPColorSpaceConversion(VideoCORE *core, mfxStatus* sts) : FilterVPP()
{

  if( core ){
    // something
  }

  VPP_CLEAN;

  *sts = MFX_ERR_NONE;

  return;

} // MFXVideoVPPColorSpaceConversion::MFXVideoVPPColorSpaceConversion(...)

MFXVideoVPPColorSpaceConversion::~MFXVideoVPPColorSpaceConversion(void)
{
  Close();

  return;

} // MFXVideoVPPColorSpaceConversion::~MFXVideoVPPColorSpaceConversion(void)

mfxStatus MFXVideoVPPColorSpaceConversion::SetParam( mfxExtBuffer* pHint )
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  if( pHint ){
    // mfxSts = something
  }

  return mfxSts;

} // mfxStatus MFXVideoVPPColorSpaceConversion::SetParam( mfxExtBuffer* pHint )

mfxStatus MFXVideoVPPColorSpaceConversion::Reset(mfxVideoParam *par)
{
  mfxU32    srcFourCC = MFX_FOURCC_YV12;
  mfxU32    dstFourCC = MFX_FOURCC_YV12;

  MFX_CHECK_NULL_PTR1( par );

  VPP_CHECK_NOT_INITIALIZED;

  srcFourCC = par->vpp.In.FourCC;
  dstFourCC = par->vpp.Out.FourCC;

  if( srcFourCC == dstFourCC
   && MFX_FOURCC_NV12    != dstFourCC
   && MFX_FOURCC_A2RGB10 != dstFourCC
   && par->vpp.In.Shift == par->vpp.Out.Shift)
  {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* conversion */
  switch ( srcFourCC )
  {
  case MFX_FOURCC_YV12:
  case MFX_FOURCC_YUY2:
  case MFX_FOURCC_RGB3:
  case MFX_FOURCC_RGB4:
  case MFX_FOURCC_NV12:
  case MFX_FOURCC_NV16:
  case MFX_FOURCC_IMC3:
  case MFX_FOURCC_YUV400:
  case MFX_FOURCC_YUV411:
  case MFX_FOURCC_YUV422H:
  case MFX_FOURCC_YUV422V:
  case MFX_FOURCC_YUV444:
  case MFX_FOURCC_P010:
  case MFX_FOURCC_P210:
      switch (dstFourCC)
      {
          case MFX_FOURCC_NV12:
          case MFX_FOURCC_YUY2:
          case MFX_FOURCC_RGB4:
          case MFX_FOURCC_P010:
          case MFX_FOURCC_P210:
          case MFX_FOURCC_NV16:
          case MFX_FOURCC_A2RGB10:
              break;

          default:
              return MFX_ERR_INVALID_VIDEO_PARAM;
      }
      break;

  default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* if OK, we update filter state [m_errPrtctState] */
  m_errPrtctState.In  = par->vpp.In;
  m_errPrtctState.Out = par->vpp.Out;

  VPP_RESET;

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPColorSpaceConversion::Reset(mfxVideoParam *par)

mfxStatus MFXVideoVPPColorSpaceConversion::RunFrameVPP(mfxFrameSurface1 *in,
                                                       mfxFrameSurface1 *out,
                                                       InternalParam* pParam)
{
  // code error
  IppStatus ippSts = ippStsNoErr;
  mfxStatus mfxSts = MFX_ERR_NONE;

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

  /* conversion */
  switch ( m_errPrtctState.In.FourCC )
  {
  case MFX_FOURCC_P010:
    switch (m_errPrtctState.Out.FourCC) {
    case MFX_FOURCC_NV12:
        ippSts = AVX_DISP(cc_P010_to_NV12,inData, inInfo, outData, outInfo, &m_yv12Data);
      break;
    case MFX_FOURCC_P010:
        ippSts = cc_P010_to_P010(inData, inInfo, outData, outInfo);
      break;
    case MFX_FOURCC_P210:
        ippSts = AVX_DISP(cc_P010_to_P210, inData, inInfo, outData, outInfo);
      break;
    case MFX_FOURCC_A2RGB10:
        ippSts = AVX_DISP(cc_P010_to_A2RGB10, inData, inInfo, outData, outInfo);
      break;
    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    break;

  case MFX_FOURCC_YV12:
    switch (m_errPrtctState.Out.FourCC) {
    case MFX_FOURCC_NV12:
      ippSts = cc_YV12_to_NV12(inData, inInfo, outData, outInfo);
      break;
    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    break;

  case MFX_FOURCC_YUY2:
    switch ( m_errPrtctState.Out.FourCC ) {
    case MFX_FOURCC_NV12:
      ippSts = cc_YUY2_to_NV12(inData, inInfo, outData, outInfo);
      break;
    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    break;

  case MFX_FOURCC_NV16:
    switch ( m_errPrtctState.Out.FourCC ) {
    case MFX_FOURCC_NV12:
        ippSts = AVX_DISP(cc_NV16_to_NV12, inData, inInfo, outData, outInfo);
      break;
    case MFX_FOURCC_P210:
        ippSts = AVX_DISP(cc_NV16_to_P210, inData, inInfo, outData, outInfo);
      break;
    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    break;

  case MFX_FOURCC_P210:
    switch ( m_errPrtctState.Out.FourCC ) {
    case MFX_FOURCC_A2RGB10:
        ippSts = AVX_DISP(cc_P210_to_A2RGB10, inData, inInfo, outData, outInfo);
      break;
    case MFX_FOURCC_P010:
        ippSts = AVX_DISP(cc_P210_to_P010, inData, inInfo, outData, outInfo);
      break;
    case MFX_FOURCC_NV12:
        ippSts = AVX_DISP(cc_P210_to_NV12, inData, inInfo, outData, outInfo, &m_yv12Data);
      break;
    case MFX_FOURCC_NV16:
        ippSts = AVX_DISP(cc_P210_to_NV16,inData, inInfo, outData, outInfo, &m_yv12Data);
      break;
    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    break;

  case MFX_FOURCC_RGB3:
    switch ( m_errPrtctState.Out.FourCC ) {
    case MFX_FOURCC_NV12:
        ippSts = cc_RGB3_to_NV12(inData, inInfo, outData, outInfo, pParam->inPicStruct);
      break;
    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    break;

  case MFX_FOURCC_RGB4:
    switch ( m_errPrtctState.Out.FourCC ) {
    case MFX_FOURCC_NV12:
      ippSts = cc_RGB4_to_NV12(inData, inInfo, outData, outInfo, pParam->inPicStruct);
      break;
    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    break;
    //-----------

  // internal feature
  case MFX_FOURCC_NV12:
      switch ( m_errPrtctState.Out.FourCC )
      {

      case MFX_FOURCC_P010:
          ippSts = AVX_DISP(cc_NV12_to_P010,inData, inInfo, outData, outInfo);
          break;

      case MFX_FOURCC_NV16:
          ippSts = AVX_DISP(cc_NV12_to_NV16,inData, inInfo, outData, outInfo);
          break;

      case MFX_FOURCC_YV12:
          ippSts = cc_NV12_to_YV12(inData, inInfo, outData, outInfo);
          break;

      case MFX_FOURCC_YUY2:
          ippSts = cc_NV12_to_YUY2(inData, inInfo, outData, outInfo);
          break;

      case MFX_FOURCC_RGB4:
          ippSts = cc_NV12_to_RGB4(inData, inInfo, outData, outInfo, &m_yv12Data);
          break;

      default:
          return MFX_ERR_INVALID_VIDEO_PARAM;
      }
      break;

  default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  mfxSts = (ippSts == ippStsNoErr) ? MFX_ERR_NONE : MFX_ERR_UNDEFINED_BEHAVIOR;
  MFX_CHECK_STS( mfxSts );

  if( !m_errPrtctState.isFirstFrameProcessed )
  {
      m_errPrtctState.isFirstFrameProcessed = true;
  }

  pParam->outPicStruct = pParam->inPicStruct;
  pParam->outTimeStamp = pParam->inTimeStamp;

  return mfxSts;

} // mfxStatus MFXVideoVPPColorSpaceConversion::RunFrameVPP(...)

mfxStatus MFXVideoVPPColorSpaceConversion::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
{
  mfxStatus mfxSts = MFX_ERR_NONE;

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
  if( srcW != dstW || srcH != dstH ){
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  srcFourCC = In->FourCC;
  dstFourCC = Out->FourCC;

  if( srcFourCC == dstFourCC
   && In->Shift == Out->Shift ){
    // copy src2dst
    mfxSts = MFX_ERR_NONE;
    return mfxSts;
  }

  m_bAVX2 = false;

#if defined(_WIN32) || defined(_WIN64)
  // [1] GetPlatformType()
  Ipp32u cpuIdInfoRegs[4];
  Ipp64u featuresMask;
  IppStatus sts = ippGetCpuFeatures( &featuresMask, cpuIdInfoRegs);
  if(ippStsNoErr == sts && featuresMask & (Ipp64u)(ippCPUID_AVX2) ) // means AVX2 + BMI_I + BMI_II to prevent issues with BMI
  {
    m_bAVX2 = true;
  }

#endif

  /* conversion */
  switch ( srcFourCC )
  {
  case MFX_FOURCC_YV12:
  case MFX_FOURCC_YUY2:
  case MFX_FOURCC_RGB3:
  case MFX_FOURCC_RGB4:
  case MFX_FOURCC_NV12:
  case MFX_FOURCC_IMC3:
  case MFX_FOURCC_NV16:
  case MFX_FOURCC_YUV400:
  case MFX_FOURCC_YUV411:
  case MFX_FOURCC_YUV422H:
  case MFX_FOURCC_YUV422V:
  case MFX_FOURCC_YUV444:
  case MFX_FOURCC_P010:
  case MFX_FOURCC_P210:
      switch (dstFourCC)
      {
      case MFX_FOURCC_NV12:
      case MFX_FOURCC_YUY2:
      case MFX_FOURCC_RGB4:
      case MFX_FOURCC_P010:
      case MFX_FOURCC_P210:
      case MFX_FOURCC_NV16:
      case MFX_FOURCC_A2RGB10:
          break;

      default:
          return MFX_ERR_INVALID_VIDEO_PARAM;
      }
      break;

  default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* save init params to prevent core crash */
  m_errPrtctState.In  = *In;
  m_errPrtctState.Out = *Out;

  VPP_INIT_SUCCESSFUL;

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPColorSpaceConversion::Init(mfxFrameInfo* In, mfxFrameInfo* Out)

mfxStatus MFXVideoVPPColorSpaceConversion::Close(void)
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  VPP_CHECK_NOT_INITIALIZED;

  VPP_CLEAN;

  return mfxSts;

} // mfxStatus MFXVideoVPPColorSpaceConversion::Close(void)


// work buffer management - nothing for CSC filter
mfxStatus MFXVideoVPPColorSpaceConversion::GetBufferSize( mfxU32* pBufferSize )
{
  VPP_CHECK_NOT_INITIALIZED;

  MFX_CHECK_NULL_PTR1(pBufferSize);

  *pBufferSize = 0;

  if(MFX_FOURCC_RGB4 == m_errPrtctState.Out.FourCC)
  {
      *pBufferSize = 3*(m_errPrtctState.Out.Width * m_errPrtctState.Out.Height) >> 1;
  }

  if(MFX_FOURCC_P010 == m_errPrtctState.In.FourCC ||
     MFX_FOURCC_P210 == m_errPrtctState.In.FourCC)
  {
      /// Need for shift operation
      *pBufferSize = 3*(m_errPrtctState.In.Width * m_errPrtctState.In.Height);
  }

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPColorSpaceConversion::GetBufferSize( mfxU32* pBufferSize )

mfxStatus MFXVideoVPPColorSpaceConversion::SetBuffer( mfxU8* pBuffer )
{
  mfxU32 bufSize = 0;
  mfxStatus sts;

  VPP_CHECK_NOT_INITIALIZED;

  sts = GetBufferSize( &bufSize );
  MFX_CHECK_STS( sts );

  //// CSC dosn't require work buffer, so pBuffer == NULL is OK
  //if( bufSize )
  //{
  //   MFX_CHECK_NULL_PTR1(pBuffer);
  //}

  if(MFX_FOURCC_RGB4 == m_errPrtctState.Out.FourCC)
  {
      MFX_CHECK_NULL_PTR1(pBuffer);

      m_yv12Data.Y = pBuffer;
      m_yv12Data.V = m_yv12Data.Y + (m_errPrtctState.Out.Width * m_errPrtctState.Out.Height);
      m_yv12Data.U = m_yv12Data.V + ((m_errPrtctState.Out.Width * m_errPrtctState.Out.Height) >> 2);
      m_yv12Data.Pitch = m_errPrtctState.Out.Width;
  }

  if(MFX_FOURCC_P010 == m_errPrtctState.In.FourCC ||
     MFX_FOURCC_P210 == m_errPrtctState.In.FourCC )
  {
      MFX_CHECK_NULL_PTR1(pBuffer);

      m_yv12Data.Y = pBuffer;
      m_yv12Data.UV = m_yv12Data.Y + (m_errPrtctState.In.Width * m_errPrtctState.In.Height);
      m_yv12Data.Pitch = m_errPrtctState.In.Width * 2;
  }

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPColorSpaceConversion::SetBuffer( mfxU8* pBuffer )

// function is called from sync part of VPP only
mfxStatus MFXVideoVPPColorSpaceConversion::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
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

} // mfxStatus MFXVideoVPPColorSpaceConversion::CheckProduceOutput(...)

bool MFXVideoVPPColorSpaceConversion::IsReadyOutput( mfxRequestType requestType )
{
    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
        // CSC is simple algorithm and need IN to produce OUT
    }

    return false;

} // bool MFXVideoVPPColorSpaceConversion::IsReadyOutput( mfxRequestType requestType )

/* ******************************************************************** */
/*                 implementation of algorithms [CSC]                   */
/* ******************************************************************** */

IppStatus cc_YV12_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts     = ippStsNoErr;
  IppiSize  roiSize = {0, 0};
  mfxU16  cropX = 0, cropY = 0;

  mfxU32  inOffset0 = 0, inOffset1  = 0;
  mfxU32  outOffset0= 0, outOffset1 = 0;

  VPP_GET_WIDTH(inInfo, roiSize.width);
  VPP_GET_HEIGHT(inInfo, roiSize.height);

  VPP_GET_CROPX(inInfo,  cropX);
  VPP_GET_CROPY(inInfo,  cropY);
  inOffset0  = cropX        + cropY*inData->Pitch;
  inOffset1  = (cropX >> 1) + (cropY >> 1)*(inData->Pitch >> 1);

  VPP_GET_CROPX(outInfo, cropX);
  VPP_GET_CROPY(outInfo, cropY);
  outOffset0   = cropX        + cropY*outData->Pitch;
  outOffset1   = (cropX) + (cropY>>1)*(outData->Pitch);

  const mfxU8* pSrc[3] = {(mfxU8*)inData->Y + inOffset0,
                          (mfxU8*)inData->V + inOffset1,
                          (mfxU8*)inData->U + inOffset1};
  /* [U<->V] because some reversing will be done ipp function */

  mfxI32 pSrcStep[3] = {inData->Pitch,
                        inData->Pitch >> 1,
                        inData->Pitch >> 1};

  mfxU8* pDst[2]   = {(mfxU8*)outData->Y + outOffset0,
                      (mfxU8*)outData->UV+ outOffset1};

  mfxI32 pDstStep[2] = {outData->Pitch,
                        outData->Pitch >> 0};

  sts = ippiYCrCb420ToYCbCr420_8u_P3P2R(pSrc, pSrcStep,
                                        pDst[0], pDstStep[0],
                                        pDst[1], pDstStep[1],
                                        roiSize);

  return sts;

} // IppStatus cc_YV12_to_NV12( ... )

IppStatus cc_NV16_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      // Copy Y plane as is
      sts = ippiCopy_8u_C1R(inData->Y, inData->Pitch, outData->Y, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      mfxU8  *Out;
      mfxU32 InStep  = inData->Pitch;
      mfxU32 OutStep = outData->Pitch;
      mfxU32 InShift  = 0;
      mfxU32 OutShift = 0;
      mfxU32 UShift   = 0;
      mfxU32 VShift   = 0;

      for (mfxU16 j = 0; j < roiSize.height; j+=2)
      {
          InShift  = ( InStep  ) * j;
          OutShift = ( OutStep ) * (j/2);

          for (mfxU16 i = 0; i < roiSize.width; i+=2)
          {
              UShift  = InShift + i;
              VShift  = UShift  + 1;
              Out     = outData->UV + OutShift  + i;
              if ( j == 0  )
              {
                  Out[0] =  V_POS(inData->UV, UShift, InStep, 0);
                  Out[1] =  V_POS(inData->UV, VShift, InStep, 0);
              }
              else if ( j == ( roiSize.height - 1))
              {
                  Out[0] =  V_POS(inData->UV, UShift, InStep, 1);
                  Out[1] =  V_POS(inData->UV, VShift, InStep, 1);
              }
              else
              {
                  // U
                  Out[0] = CHOP( ((V_POS(inData->UV, UShift, InStep,  0) + V_POS(inData->UV, UShift, InStep, 1) + 1)>>1));
                  // V
                  Out[1] = CHOP( ((V_POS(inData->UV, VShift, InStep,  0) + V_POS(inData->UV, VShift, InStep, 1) + 1)>>1));
              }
          }
      }
  }
  else
  {
      /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;
} // IppStatus cc_NV16_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_NV12_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo*)
{
    IppStatus sts = ippStsNoErr;
    IppiSize  roiSize = {0, 0};

    VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
    VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        // Copy Y plane as is
        sts = ippiCopy_8u_C1R((const Ipp8u *)inData->Y, inData->Pitch, (Ipp8u *)outData->Y, outData->Pitch, roiSize);
        IPP_CHECK_STS( sts );

        // Chroma is interpolated using nearest points that gives closer match with ffmpeg
        mfxU8  *Out;
        mfxU32 InStep  = inData->Pitch;
        mfxU32 OutStep = outData->Pitch;

        mfxU32 InShift  = 0;
        mfxU32 OutShift = 0;
        mfxU32 UShift   = 0;
        mfxU32 VShift   = 0;
        int main   = 0;
        int part   = 0;

        for (mfxU16 j = 0; j < roiSize.height; j++)
        {
            InShift  = ( InStep  ) * ( j / 2 );
            OutShift = ( OutStep ) * j;

            if ( 0 == j || j == (roiSize.height -1 ))
            {
                // First and last lines
                part = 0;
            }
            else if ( j % 2 == 0 )
            {
                // Odd lines
                part = -1;
            }
            else
            {
                // Even lines
                part = 1;
            }

            for (mfxU16 i = 0; i < roiSize.width; i+=2)
            {
                UShift  = InShift + i;
                VShift  = UShift  + 1;
                Out     = outData->UV + OutShift  + i;

                // U
                Out[0] = CHOP((3*V_POS(inData->UV, UShift, InStep, main) + V_POS(inData->UV, UShift, InStep, part)) >> 2);
                // V
                Out[1] = CHOP((3*V_POS(inData->UV, VShift, InStep, main) + V_POS(inData->UV, VShift, InStep, part)) >> 2);
            }
      }
  }
  else
  {
      /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_NV12_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_YUY2_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  mfxU8* pSrc[1] = {(mfxU8*)inData->Y};

  mfxI32 pSrcStep[1] = {inData->Pitch};

  mfxU8* pDst[2]   = {(mfxU8*)outData->Y,
                      (mfxU8*)outData->UV};

  mfxI32 pDstStep[2] = {outData->Pitch,
                        outData->Pitch >> 0};

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      sts = ippiYCbCr422ToYCbCr420_8u_C2P2R(pSrc[0], pSrcStep[0], pDst[0], pDstStep[0], pDst[1], pDstStep[1], roiSize);
  }
  else
  {
      roiSize.height >>= 1;

      pSrcStep[0] = inData->Pitch << 1;
      pDstStep[0] = outData->Pitch << 1;
      pDstStep[1] = outData->Pitch << 1;

      sts = ippiYCbCr422ToYCbCr420_8u_C2P2R(pSrc[0], pSrcStep[0], pDst[0], pDstStep[0], pDst[1], pDstStep[1], roiSize);

      pSrc[0] += inData->Pitch;
      pDst[0] += outData->Pitch;
      pDst[1] += outData->Pitch;

      sts = ippiYCbCr422ToYCbCr420_8u_C2P2R(pSrc[0], pSrcStep[0], pDst[0], pDstStep[0], pDst[1], pDstStep[1], roiSize);
  }

  return sts;

} // IppStatus cc_YUY2_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_P010_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo,
                            mfxFrameData* yv12Data)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      /* Initial implementation w/o rounding 
       * TODO: add rounding to match reference algorithm 
       */
      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, 2, (Ipp16u *)yv12Data->Y, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->Y,  yv12Data->Pitch, outData->Y, outData->Pitch, roiSize); 
      IPP_CHECK_STS( sts );

      roiSize.height >>= 1;
      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch, 2, (Ipp16u *)yv12Data->UV, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->UV,  yv12Data->Pitch, outData->UV, outData->Pitch, roiSize); 
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P010_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_P210_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo,
                            mfxFrameData* yv12Data)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, 2, (Ipp16u *)yv12Data->Y, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->Y,  yv12Data->Pitch, outData->Y, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch, 2, (Ipp16u *)yv12Data->UV, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->UV,  yv12Data->Pitch, outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;
} // IppStatus cc_P210_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_P210_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo,
                            mfxFrameData* yv12Data)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      /* Initial implementation w/o rounding
       * TODO: add rounding to match reference algorithm
       */
      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, 2, (Ipp16u *)yv12Data->Y, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->Y,  yv12Data->Pitch, outData->Y, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      roiSize.height >>= 1;
      sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch * 2, 2, (Ipp16u *)yv12Data->UV, yv12Data->Pitch, roiSize);
      IPP_CHECK_STS( sts );
      sts = ippiConvert_16u8u_C1R((const Ipp16u *)yv12Data->UV,  yv12Data->Pitch, outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;
} // IppStatus cc_P210_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_P010_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo*)
{
    IppStatus sts = ippStsNoErr;
    IppiSize  roiSize = {0, 0};

    VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
    VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        // Copy Y plane as is
        sts = ippiCopy_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
        IPP_CHECK_STS( sts );

        // Chroma is interpolated using nearest points that gives closer match with ffmpeg
        mfxU16 *Out;
        mfxU32 InStep  = inData->Pitch  >> 1;
        mfxU32 OutStep = outData->Pitch >> 1;

        mfxU32 InShift  = 0;
        mfxU32 OutShift = 0;
        mfxU32 UShift   = 0;
        mfxU32 VShift   = 0;
        int main   = 0;
        int part   = 0;

        for (mfxU16 j = 0; j < roiSize.height; j++)
        {
            InShift  = ( InStep  ) * ( j / 2 );
            OutShift = ( OutStep ) * j;

            if ( 0 == j || j == (roiSize.height -1 ))
            {
                // First and last lines
                part = 0;
            }
            else if ( j % 2 == 0 )
            {
                // Odd lines
                part = -1;
            }
            else
            {
                // Even lines
                part = 1;
            }

            for (mfxU16 i = 0; i < roiSize.width; i+=2)
            {
                UShift  = InShift + i;
                VShift  = UShift  + 1;
                Out     = outData->U16 + OutShift  + i;

                // U
                Out[0] = CHOP10((3*V_POS(inData->U16, UShift, InStep, main) + V_POS(inData->U16, UShift, InStep, part)) >> 2);
                // V
                Out[1] = CHOP10((3*V_POS(inData->U16, VShift, InStep, main) + V_POS(inData->U16, VShift, InStep, part)) >> 2);
            }
      }
  }
  else
  {
      /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        // Copy Y plane as is
        sts = ippiCopy_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
        IPP_CHECK_STS( sts );

        mfxU16 *Out;
        mfxU32 InStep  = inData->Pitch  >> 1;
        mfxU32 OutStep = outData->Pitch >> 1;
        mfxU32 InShift  = 0;
        mfxU32 OutShift = 0;
        mfxU32 UShift   = 0;
        mfxU32 VShift   = 0;

        for (mfxU16 j = 0; j < roiSize.height; j+=2)
        {
            InShift  = ( InStep  ) * j;
            OutShift = ( OutStep ) * (j/2);

            for (mfxU16 i = 0; i < roiSize.width; i+=2)
            {
                UShift  = InShift + i;
                VShift  = UShift  + 1;
                Out     = outData->U16 + OutShift  + i;
                if ( j == 0  )
                {
                    Out[0] =  V_POS(inData->U16, UShift, InStep, 0);
                    Out[1] =  V_POS(inData->U16, VShift, InStep, 0);
                }
                else if ( j == ( roiSize.height - 1))
                {
                    Out[0] =  V_POS(inData->U16, UShift, InStep, 1);
                    Out[1] =  V_POS(inData->U16, VShift, InStep, 1);
                }
                else
                {
                    // U
                    Out[0] = CHOP10( ((V_POS(inData->U16, UShift, InStep,  0) + V_POS(inData->U16, UShift, InStep, 1) + 1)>>1));
                    // V
                    Out[1] = CHOP10( ((V_POS(inData->U16, VShift, InStep,  0) + V_POS(inData->U16, VShift, InStep, 1) + 1)>>1));
                }
            }
        }
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;

} // IppStatus cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_P010_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                              mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};
  mfxU32 inPitch, outPitch;
  mfxU16 *ptr_y, *ptr_uv;
  mfxU32 *out;
  mfxU32 uv_offset, out_offset;
  mfxU16 y, v,u;
  mfxU16 r, g, b;
  int Y,Cb,Cr;

  inPitch  = inData->Pitch;
  outPitch = outData->Pitch;
  outPitch >>= 2; // U32 pointer is used for output
  inPitch  >>= 1; // U16 pointer is used for input
  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
        ptr_y  = (mfxU16*)inData->Y;
        ptr_uv = (mfxU16*)inData->UV;
        out = (mfxU32*)IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );

        uv_offset    = 0;
        out_offset   = 0;

        for(mfxI32 j = 0; j < roiSize.height; j++) {
            out_offset = j*outPitch;

            if ( j != 0 && (j%2) == 0 ){
                // Use the same plane twice for 4:2:0
                uv_offset += inPitch;
            }

            for (mfxI32 i = 0; i < roiSize.width; i++) {
                y = ptr_y[j*inPitch + i];
                u = ptr_uv[uv_offset + (i/2)*2];
                v = ptr_uv[uv_offset + (i/2)*2 + 1];

                Y = (int)(y - 64) * 0x000129fa;
                Cb = u - 512;
                Cr = v - 512;
                r = CHOP10((((Y + 0x00019891 * Cr + 0x00008000 )>>16)));
                g = CHOP10((((Y - 0x00006459 * Cb - 0x0000d01f * Cr + 0x00008000  ) >> 16 )));
                b = CHOP10((((Y + 0x00020458 * Cb + 0x00008000  )>>16)));
                out[out_offset++] = (r <<20 | g << 10 | b << 0 );
             }
        }
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P010_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)


IppStatus cc_P210_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                                          mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};
  mfxU32 inPitch, outPitch;
  mfxU16 *ptr_y, *ptr_uv;
  mfxU32 *out;
  mfxU32 uv_offset, out_offset;
  mfxU16 y, v,u;
  mfxU16 r, g, b;
  int Y,Cb,Cr;

  inPitch  = inData->Pitch;
  outPitch = outData->Pitch;
  outPitch >>= 2; // U32 pointer is used for output
  inPitch  >>= 1; // U16 pointer is used for input
  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
        ptr_y  = (mfxU16*)inData->Y;
        ptr_uv = (mfxU16*)inData->UV;
        out = (mfxU32*)IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );

        uv_offset    = 0;
        out_offset   = 0;

        for(mfxI32 j = 0; j < roiSize.height; j++) {
            out_offset = j*outPitch;
            uv_offset  = j*inPitch;

            for (mfxI32 i = 0; i < roiSize.width; i++) {
                y = ptr_y[j*inPitch + i];
                u = ptr_uv[uv_offset + (i/2)*2];
                v = ptr_uv[uv_offset + (i/2)*2 + 1];

                Y = (int)(y - 64) * 0x000129fa;
                Cb = u - 512;
                Cr = v - 512;
                r = CHOP10((((Y + 0x00019891 * Cr + 0x00008000 )>>16)));
                g = CHOP10((((Y - 0x00006459 * Cb - 0x0000d01f * Cr + 0x00008000  ) >> 16 )));
                b = CHOP10((((Y + 0x00020458 * Cb + 0x00008000  )>>16)));
                out[out_offset++] = (r <<20 | g << 10 | b << 0 );
             }
        }
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P210_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)


#if defined(_WIN32) || defined(_WIN64)

#if defined(_WIN32) || defined(_WIN64)
  #define ALIGN_DECL(X) __declspec(align(X))
#else
  #define ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif
#include <immintrin.h>

ALIGN_DECL(32) static const signed int yTab[8]  = { 0x000129fa, 0x000129fa, 0x000129fa, 0x000129fa, 0x000129fa, 0x000129fa, 0x000129fa, 0x000129fa };
ALIGN_DECL(32) static const signed int rTab[8]  = { 0x00019891, 0x00019891, 0x00019891, 0x00019891, 0x00019891, 0x00019891, 0x00019891, 0x00019891 };
ALIGN_DECL(32) static const signed int bTab[8]  = { 0x00020458, 0x00020458, 0x00020458, 0x00020458, 0x00020458, 0x00020458, 0x00020458, 0x00020458 };
ALIGN_DECL(32) static const signed int grTab[8] = { 0x0000d01f, 0x0000d01f, 0x0000d01f, 0x0000d01f, 0x0000d01f, 0x0000d01f, 0x0000d01f, 0x0000d01f };
ALIGN_DECL(32) static const signed int gbTab[8] = { 0x00006459, 0x00006459, 0x00006459, 0x00006459, 0x00006459, 0x00006459, 0x00006459, 0x00006459 };

/* assume width = multiple of 8 */
IppStatus cc_P010_to_A2RGB10_avx2( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo)
{
    __m128i xmm0, xmm1;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
    __m256i ymmMin, ymmMax, ymmRnd, ymmOff, y1mmOff;

    mfxU32 inPitch, outPitch;
    mfxU16 *ptr_y, *ptr_uv;
    mfxU32 *out, *out_ptr;
    mfxI32 i, j, width, height;

    VM_ASSERT( (inInfo->Width & 0x07) == 0 );
    outInfo;

    if ((MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct) == 0)
        return ippStsErr;  /* interlaced not supported */

    _mm256_zeroupper();

    width  = inInfo->Width;
    height = inInfo->Height;

    inPitch  = (inData->Pitch >> 1);      /* 16-bit input */
    outPitch = (outData->Pitch >> 2);     /* 32-bit output */

    ptr_y  = (mfxU16*)inData->Y;
    ptr_uv = (mfxU16*)inData->UV;
    out    = (mfxU32*)IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );

    ymmMin  = _mm256_set1_epi32(0);
    ymmMax  = _mm256_set1_epi32(1023);
    ymmRnd  = _mm256_set1_epi32(1 << 15);
    ymmOff  = _mm256_set1_epi32(512);
    y1mmOff = _mm256_set1_epi32(64);

    for (j = 0; j < height; j++) {
        out_ptr = out;
        for (i = 0; i < width; i += 8) {
            /* inner loop should fit entirely in registers in x64 build */
            xmm0 = _mm_loadu_si128((__m128i *)(ptr_y + i));     // [Y0 Y1 Y2 Y3...] (16-bit)
            xmm1 = _mm_loadu_si128((__m128i *)(ptr_uv + i));    // [U0 V0 U1 V1...] (16-bit)

            ymm0 = _mm256_cvtepu16_epi32(xmm0);                 // [Y0 Y1 Y2 Y3...] (32-bit)
            ymm0 = _mm256_sub_epi32(ymm0, y1mmOff);             // -64
            ymm0 = _mm256_shuffle_epi32(ymm0, 0xF5);            // Y = [Y0 Y1 Y2 Y3...] - 64
            ymm0 = _mm256_shuffle_epi32(ymm0, 0xA0);
            ymm0 = _mm256_mullo_epi32(ymm0, *(__m256i *)yTab);  // Y = [Y0 Y1 Y2 Y3...] * 0x000129fa
            ymm0 = _mm256_add_epi32(ymm0, ymmRnd);              // include round factor

            ymm1 = _mm256_cvtepu16_epi32(xmm1);                 // [U0 V0 U1 V1...] (32-bit)
            ymm1 = _mm256_sub_epi32(ymm1, ymmOff);              // -512
            ymm2 = _mm256_shuffle_epi32(ymm1, 0xF5);            // Cr = [V0 V0 V1 V1...] - 512
            ymm1 = _mm256_shuffle_epi32(ymm1, 0xA0);            // Cb = [U0 U0 U1 U1...] - 512

            /* R = Y + 0x00019891*Cr + rnd */
            ymm3 = _mm256_mullo_epi32(ymm2, *(__m256i *)rTab);
            ymm3 = _mm256_add_epi32(ymm3, ymm0);

            /* G = Y - 0x00006459*Cb - 0x0000d01f*Cr + rnd */
            ymm4 = _mm256_mullo_epi32(ymm1, *(__m256i *)gbTab);
            ymm5 = _mm256_mullo_epi32(ymm2, *(__m256i *)grTab);
            ymm4 = _mm256_sub_epi32(ymm0, ymm4);
            ymm4 = _mm256_sub_epi32(ymm4, ymm5);

            /* B = Y + 0x00020458*Cb + rnd */
            ymm5 = _mm256_mullo_epi32(ymm1, *(__m256i *)bTab);
            ymm5 = _mm256_add_epi32(ymm5, ymm0);

            /* scale back to 16-bit, clip to [0, 2^10 - 1] */
            ymm3 = _mm256_srai_epi32(ymm3, 16);
            ymm4 = _mm256_srai_epi32(ymm4, 16);
            ymm5 = _mm256_srai_epi32(ymm5, 16);

            ymm3 = _mm256_max_epi32(ymm3, ymmMin);
            ymm4 = _mm256_max_epi32(ymm4, ymmMin);
            ymm5 = _mm256_max_epi32(ymm5, ymmMin);

            ymm3 = _mm256_min_epi32(ymm3, ymmMax);
            ymm4 = _mm256_min_epi32(ymm4, ymmMax);
            ymm5 = _mm256_min_epi32(ymm5, ymmMax);

            /* 32-bit output = (R << 20) | (G << 10) | (B << 0) */
            ymm3 = _mm256_slli_epi32(ymm3, 20);
            ymm4 = _mm256_slli_epi32(ymm4, 10);
            ymm5 = _mm256_or_si256(ymm5, ymm3);
            ymm5 = _mm256_or_si256(ymm5, ymm4);

            /* store 8 32-bit RGB values */
            _mm256_storeu_si256((__m256i *)(out_ptr), ymm5);
            out_ptr += 8;
        }

        /* update pointers (UV plane subsampled by 2) */
        out   += outPitch;
        ptr_y += inPitch;
        if (j & 0x01)
            ptr_uv += inPitch;
    }

    return ippStsNoErr;

} // IppStatus cc_P010_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

/* assume width = multiple of 8 */
IppStatus cc_P210_to_A2RGB10_avx2( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo)
{
    __m128i xmm0, xmm1;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
    __m256i ymmMin, ymmMax, ymmRnd, ymmOff, y1mmOff;

    mfxU32 inPitch, outPitch;
    mfxU16 *ptr_y, *ptr_uv;
    mfxU32 *out, *out_ptr;
    mfxI32 i, j, width, height;

    VM_ASSERT( (inInfo->Width & 0x07) == 0 );
    outInfo;

    if ((MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct) == 0)
        return ippStsErr;  /* interlaced not supported */

    _mm256_zeroupper();

    width  = inInfo->Width;
    height = inInfo->Height;

    inPitch  = (inData->Pitch >> 1);      /* 16-bit input */
    outPitch = (outData->Pitch >> 2);     /* 32-bit output */

    ptr_y  = (mfxU16*)inData->Y;
    ptr_uv = (mfxU16*)inData->UV;
    out    = (mfxU32*)IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );

    ymmMin  = _mm256_set1_epi32(0);
    ymmMax  = _mm256_set1_epi32(1023);
    ymmRnd  = _mm256_set1_epi32(1 << 15);
    ymmOff  = _mm256_set1_epi32(512);
    y1mmOff = _mm256_set1_epi32(64);

    for (j = 0; j < height; j++) {
        out_ptr = out;
        for (i = 0; i < width; i += 8) {
            /* inner loop should fit entirely in registers in x64 build */
            xmm0 = _mm_loadu_si128((__m128i *)(ptr_y + i));     // [Y0 Y1 Y2 Y3...] (16-bit)
            xmm1 = _mm_loadu_si128((__m128i *)(ptr_uv + i));    // [U0 V0 U1 V1...] (16-bit)

            ymm0 = _mm256_cvtepu16_epi32(xmm0);                 // [Y0 Y1 Y2 Y3...] (32-bit)
            ymm0 = _mm256_sub_epi32(ymm0, y1mmOff);             // -64
            ymm0 = _mm256_shuffle_epi32(ymm0, 0xF5);            // Y = [Y0 Y1 Y2 Y3...] - 64
            ymm0 = _mm256_shuffle_epi32(ymm0, 0xA0);
            ymm0 = _mm256_mullo_epi32(ymm0, *(__m256i *)yTab);  // Y = [Y0 Y1 Y2 Y3...] * 0x000129fa
            ymm0 = _mm256_add_epi32(ymm0, ymmRnd);              // include round factor

            ymm1 = _mm256_cvtepu16_epi32(xmm1);                 // [U0 V0 U1 V1...] (32-bit)
            ymm1 = _mm256_sub_epi32(ymm1, ymmOff);              // -512
            ymm2 = _mm256_shuffle_epi32(ymm1, 0xF5);            // Cr = [V0 V0 V1 V1...] - 512
            ymm1 = _mm256_shuffle_epi32(ymm1, 0xA0);            // Cb = [U0 U0 U1 U1...] - 512

            /* R = Y + 0x00019891*Cr + rnd */
            ymm3 = _mm256_mullo_epi32(ymm2, *(__m256i *)rTab);
            ymm3 = _mm256_add_epi32(ymm3, ymm0);

            /* G = Y - 0x00006459*Cb - 0x0000d01f*Cr + rnd */
            ymm4 = _mm256_mullo_epi32(ymm1, *(__m256i *)gbTab);
            ymm5 = _mm256_mullo_epi32(ymm2, *(__m256i *)grTab);
            ymm4 = _mm256_sub_epi32(ymm0, ymm4);
            ymm4 = _mm256_sub_epi32(ymm4, ymm5);

            /* B = Y + 0x00020458*Cb + rnd */
            ymm5 = _mm256_mullo_epi32(ymm1, *(__m256i *)bTab);
            ymm5 = _mm256_add_epi32(ymm5, ymm0);

            /* scale back to 16-bit, clip to [0, 2^10 - 1] */
            ymm3 = _mm256_srai_epi32(ymm3, 16);
            ymm4 = _mm256_srai_epi32(ymm4, 16);
            ymm5 = _mm256_srai_epi32(ymm5, 16);

            ymm3 = _mm256_max_epi32(ymm3, ymmMin);
            ymm4 = _mm256_max_epi32(ymm4, ymmMin);
            ymm5 = _mm256_max_epi32(ymm5, ymmMin);

            ymm3 = _mm256_min_epi32(ymm3, ymmMax);
            ymm4 = _mm256_min_epi32(ymm4, ymmMax);
            ymm5 = _mm256_min_epi32(ymm5, ymmMax);

            /* 32-bit output = (R << 20) | (G << 10) | (B << 0) */
            ymm3 = _mm256_slli_epi32(ymm3, 20);
            ymm4 = _mm256_slli_epi32(ymm4, 10);
            ymm5 = _mm256_or_si256(ymm5, ymm3);
            ymm5 = _mm256_or_si256(ymm5, ymm4);

            /* store 8 32-bit RGB values */
            _mm256_storeu_si256((__m256i *)(out_ptr), ymm5);
            out_ptr += 8;
        }

        /* update pointers (UV plane subsampled by 2) */
        out   += outPitch;
        ptr_y += inPitch;
        ptr_uv += inPitch;
    }

    return ippStsNoErr;

} // IppStatus cc_P210_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)


#endif

IppStatus cc_P010_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};
  Ipp32u    shift;
  mfxU8     right_shift;
  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);


  if ( inInfo->Shift > outInfo->Shift )
  {
      shift = inInfo->Shift - outInfo->Shift;
      right_shift = 0;
  }
  else
  {
      shift = outInfo->Shift - inInfo->Shift;
      right_shift = 1;
  }

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {

      if ( right_shift )
      {
          sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch /* * 2*/, shift, (Ipp16u *)outData->Y, outData->Pitch /** 2*/, roiSize);
      }
      else
      {
          sts = ippiLShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch /** 2*/, shift, (Ipp16u *)outData->Y, outData->Pitch /** 2*/, roiSize);
      }
      IPP_CHECK_STS( sts );

      roiSize.height >>= 1;
      if ( right_shift )
      {
          sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch /** 2*/, shift, (Ipp16u *)outData->UV, outData->Pitch * 2, roiSize);
      }
      else
      {
          sts = ippiLShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch/** 2*/, shift, (Ipp16u *)outData->UV, outData->Pitch/** 2*/, roiSize);
      }
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P010_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_NV12_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      sts = ippiConvert_8u16u_C1R(inData->Y,  inData->Pitch, (Ipp16u *)outData->Y,  outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiLShiftC_16u_C1IR(2, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      roiSize.height >>= 1;
      sts = ippiConvert_8u16u_C1R(inData->UV, inData->Pitch, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiLShiftC_16u_C1IR(2, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_NV12_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_NV16_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};

  // cropping was removed
  outInfo;

  VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
  VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

  if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
  {
      sts = ippiConvert_8u16u_C1R(inData->Y,  inData->Pitch, (Ipp16u *)outData->Y,  outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiLShiftC_16u_C1IR(2, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiConvert_8u16u_C1R(inData->UV, inData->Pitch, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );

      sts = ippiLShiftC_16u_C1IR(2, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
      IPP_CHECK_STS( sts );
  }
  else
  {
     /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_NV16_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)


////-------------------------------------------------------------------
#define kry0  0x000041cb
#define kry1  0x00008106
#define kry2  0x00001917
#define kry3  0x000025e3
#define kry4  0x00004a7f
#define kry5  0x00007062
#define kry6  0x00005e35
#define kry7  0x0000122d

static
IppStatus  ownBGRToYCbCr420_8u_C3P2R( const mfxU8* pSrc, mfxI32 srcStep, mfxU8* pDst[2],mfxI32 dstStep[2], IppiSize roiSize )
{
  mfxI32 h,w;
  mfxI32 dstStepY = dstStep[0];
  mfxI32 width2  = roiSize.width  & ~1;
  mfxI32 height2 = roiSize.height >> 1;
  mfxU8* pDstU = pDst[1];
  mfxU8* pDstV = pDstU + 1;

  IppStatus sts = ippStsNoErr;

  for( h = 0; h < height2; h++ ){
    const mfxU8* src;

    mfxU8* dsty, *dstu, *dstv;

    src  = pSrc   + h * 2 * srcStep;
    dsty = pDst[0]+ h * 2 * dstStepY;

    dstu = pDstU  + h * dstStep[1];
    dstv = pDstV  + h * dstStep[1];

    for( w = 0; w < width2; w += 2 ) {
      mfxI32 g,g1,g2,g3;
      mfxI32 r,r1,r2,r3;
      mfxI32 b,b1,b2,b3;
      b = src[0];g = src[1];r = src[2];
      b1= src[3];g1= src[4];r1= src[5];
      b2= src[0+srcStep];g2= src[1+srcStep];r2= src[2+srcStep];
      b3= src[3+srcStep];g3= src[4+srcStep];r3= src[5+srcStep];
      src += 6;
      dsty[0] = ( mfxU8 )(( kry0 * r  + kry1 *  g + kry2 *  b + 0x108000) >> 16 );
      dsty[1] = ( mfxU8 )(( kry0 * r1 + kry1 * g1 + kry2 * b1 + 0x108000) >> 16 );
      dsty[0+dstStepY] = ( mfxU8 )(( kry0 * r2 + kry1 * g2 + kry2 * b2 + 0x108000) >> 16 );
      dsty[1+dstStepY] = ( mfxU8 )(( kry0 * r3 + kry1 * g3 + kry2 * b3 + 0x108000) >> 16 );
      dsty += 2;

      r += r1;r += r2;r += r3;
      b += b1;b += b2;b += b3;
      g += g1;g += g2;g += g3;

      *dstu = ( Ipp8u )((-kry3 * r - kry4 * g + kry5 * b + 0x2008000)>> 18 ); /* Cb */
      *dstv = ( Ipp8u )(( kry5 * r - kry6 * g - kry7 * b + 0x2008000)>> 18 ); /* Cr */

      dstu += 2;
      dstv += 2;
    }
  }

  return sts;

} // IppStatus  ownBGRToYCbCr420_8u_C3P2R( const mfxU8* pSrc, mfxI32 srcStep, ...)

IppStatus cc_RGB3_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo, mfxU16 picStruct )
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};
  mfxU16  cropX = 0, cropY = 0;

  mfxU32  inOffset0 = 0;
  mfxU32  outOffset0= 0, outOffset1 = 0;

  VPP_GET_WIDTH(inInfo, roiSize.width);
  VPP_GET_HEIGHT(inInfo, roiSize.height);

  VPP_GET_CROPX(inInfo,  cropX);
  VPP_GET_CROPY(inInfo,  cropY);
  inOffset0  = cropX*3  + cropY*inData->Pitch;

  VPP_GET_CROPX(outInfo, cropX);
  VPP_GET_CROPY(outInfo, cropY);
  outOffset0  = cropX        + cropY*outData->Pitch;
  outOffset1  = (cropX) + (cropY>>1)*(outData->Pitch);

  IPP_CHECK_NULL_PTR1(inData->R);
  IPP_CHECK_NULL_PTR1(inData->G);
  IPP_CHECK_NULL_PTR1(inData->B);

  mfxU8* ptrStart = IPP_MIN( IPP_MIN(inData->R, inData->G), inData->B );

  const mfxU8* pSrc[1] = {ptrStart + inOffset0};

  mfxI32 pSrcStep[1] = {inData->Pitch};

  mfxU8* pDst[2]   = {(mfxU8*)outData->Y  + outOffset0,
                      (mfxU8*)outData->UV + outOffset1};

  mfxI32 pDstStep[2] = {outData->Pitch, outData->Pitch >> 0};

  if( MFX_PICSTRUCT_PROGRESSIVE & picStruct )
  {
    sts = ownBGRToYCbCr420_8u_C3P2R( pSrc[0], pSrcStep[0], pDst, pDstStep, roiSize );
  }
  else
  {
    mfxI32 pDstFieldStep[2] = {pDstStep[0]<<1, pDstStep[1]<<1};
    IppiSize  roiFieldSize = {roiSize.width, roiSize.height >> 1};
    mfxU8* pDstSecondField[2] = {pDst[0]+pDstStep[0], pDst[1]+pDstStep[1]};

    /* first field */
    sts = ownBGRToYCbCr420_8u_C3P2R( pSrc[0], pSrcStep[0]<<1, pDst, pDstFieldStep, roiFieldSize );
    if( ippStsNoErr != sts ) return sts;
    /* second field */
    sts = ownBGRToYCbCr420_8u_C3P2R( pSrc[0]+pSrcStep[0], pSrcStep[0]<<1,
                                     pDstSecondField, pDstFieldStep, roiFieldSize );
  }

  return sts;

} // IppStatus cc_RGB3_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

// internal feature
IppStatus cc_NV12_to_YV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* outInfo )
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};
  mfxU16  cropX = 0, cropY = 0;

  mfxU32  inOffset0 = 0, inOffset1  = 0;
  mfxU32  outOffset0= 0, outOffset1 = 0;

  VPP_GET_WIDTH(inInfo, roiSize.width);
  VPP_GET_HEIGHT(inInfo, roiSize.height);

  VPP_GET_CROPX(inInfo,  cropX);
  VPP_GET_CROPY(inInfo,  cropY);
  inOffset0  = cropX  + cropY*inData->Pitch;
  inOffset1  = (cropX) + (cropY>>1)*(inData->Pitch);

  VPP_GET_CROPX(outInfo, cropX);
  VPP_GET_CROPY(outInfo, cropY);
  outOffset0  = cropX        + cropY*outData->Pitch;
  outOffset1  = (cropX >> 1) + (cropY >> 1)*(outData->Pitch >> 1);

  const mfxU8* pSrc[2] = {(mfxU8*)inData->Y + inOffset0,
                          (mfxU8*)inData->UV+ inOffset1};

  mfxI32 pSrcStep[2] = {inData->Pitch,
                        inData->Pitch >> 0};

  mfxU8* pDst[3]   = {(mfxU8*)outData->Y + outOffset0,
                      (mfxU8*)outData->U + outOffset1,
                      (mfxU8*)outData->V + outOffset1};

  mfxI32 pDstStep[3] = {outData->Pitch,
                        outData->Pitch >> 1,
                        outData->Pitch >> 1};

  sts = ippiYCbCr420_8u_P2P3R(pSrc[0], pSrcStep[0],
                              pSrc[1], pSrcStep[1],
                              pDst,    pDstStep,
                              roiSize);

  return sts;

} // IppStatus cc_NV12_to_YV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)


//-------------------------------------------------------------------------

static
IppStatus  ownBGRToYCbCr420_8u_AC4P3R( const Ipp8u* pSrc, int srcStep, Ipp8u* pDst[2],int dstStep[2], IppiSize roiSize )
{
  IppStatus sts = ippStsNoErr;
  int h,w;
  int dstStepY = dstStep[0];
  int width2  = roiSize.width  & ~1;
  int height2 = roiSize.height >> 1;
  Ipp8u* pDstUV = pDst[1];

  for( h = 0; h < height2; h++ ){
    const Ipp8u* src;
    Ipp8u* dsty,*dstu,*dstv;
    src  = pSrc   + h * 2 * srcStep;
    dsty = pDst[0]+ h * 2 * dstStepY;

    dstu = pDstUV  + h * dstStep[1];
    dstv = dstu + 1;

    for( w = 0; w < width2; w += 2 ){
      int g,g1,g2,g3;
      int r,r1,r2,r3;
      int b,b1,b2,b3;
      b = src[0];g = src[1];r = src[2];
      b1= src[4];g1= src[5];r1= src[6];
      b2= src[0+srcStep];g2= src[1+srcStep];r2= src[2+srcStep];
      b3= src[4+srcStep];g3= src[5+srcStep];r3= src[6+srcStep];
      src += 8;
      dsty[0] = ( Ipp8u )(( kry0 * r  + kry1 *  g + kry2 *  b + 0x108000) >> 16 );
      dsty[1] = ( Ipp8u )(( kry0 * r1 + kry1 * g1 + kry2 * b1 + 0x108000) >> 16 );
      dsty[0+dstStepY] = ( Ipp8u )(( kry0 * r2 + kry1 * g2 + kry2 * b2 + 0x108000) >> 16 );
      dsty[1+dstStepY] = ( Ipp8u )(( kry0 * r3 + kry1 * g3 + kry2 * b3 + 0x108000) >> 16 );
      dsty += 2;
      r += r1;r += r2;r += r3;
      b += b1;b += b2;b += b3;
      g += g1;g += g2;g += g3;

      *dstu = ( Ipp8u )((-kry3 * r - kry4 * g + kry5 * b + 0x2008000)>> 18 ); /* Cb */
      *dstv = ( Ipp8u )(( kry5 * r - kry6 * g - kry7 * b + 0x2008000)>> 18 ); /* Cr */

      dstu += 2;
      dstv += 2;
    }
  }

  return sts;

} // IppStatus  ownBGRToYCbCr420_8u_AC4P3R( ... )

IppStatus cc_RGB4_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                          mfxFrameData* outData, mfxFrameInfo* outInfo, mfxU16 picStruct)
{
  IppStatus sts = ippStsNoErr;
  IppiSize  roiSize = {0, 0};
  mfxU16  cropX = 0, cropY = 0;

  mfxU32  inOffset0 = 0;
  mfxU32  outOffset0= 0, outOffset1 = 0;

  VPP_GET_WIDTH(inInfo, roiSize.width);
  VPP_GET_HEIGHT(inInfo, roiSize.height);

  VPP_GET_CROPX(inInfo,  cropX);
  VPP_GET_CROPY(inInfo,  cropY);
  inOffset0  = cropX*4  + cropY*inData->Pitch;

  VPP_GET_CROPX(outInfo, cropX);
  VPP_GET_CROPY(outInfo, cropY);
  outOffset0  = cropX        + cropY*outData->Pitch;
  outOffset1  = (cropX) + (cropY>>1)*(outData->Pitch);

  IPP_CHECK_NULL_PTR1(inData->R);
  IPP_CHECK_NULL_PTR1(inData->G);
  IPP_CHECK_NULL_PTR1(inData->B);
  // alpha channel is ignore due to d3d issue

  mfxU8* ptrStart = IPP_MIN( IPP_MIN(inData->R, inData->G), inData->B );

  const mfxU8* pSrc[1] = {ptrStart + inOffset0};

  mfxI32 pSrcStep[1] = {inData->Pitch >> 0};

  mfxU8* pDst[2]   = {(mfxU8*)outData->Y + outOffset0, (mfxU8*)outData->UV + outOffset1};

  mfxI32 pDstStep[2] = {outData->Pitch, outData->Pitch >> 0};

  if( MFX_PICSTRUCT_PROGRESSIVE & picStruct )
  {
    sts = ownBGRToYCbCr420_8u_AC4P3R( pSrc[0], pSrcStep[0], pDst, pDstStep, roiSize );
  }
  else
  {
    mfxI32 pDstFieldStep[2] = {pDstStep[0]<<1, pDstStep[1]<<1};
    IppiSize  roiFieldSize = {roiSize.width, roiSize.height >> 1};
    mfxU8* pDstSecondField[2] = {pDst[0]+pDstStep[0], pDst[1]+pDstStep[1]};

    /* first field */
    sts = ownBGRToYCbCr420_8u_AC4P3R( pSrc[0], pSrcStep[0]<<1,
                                      pDst, pDstFieldStep, roiFieldSize );

    if( ippStsNoErr != sts ) return sts;
    /* second field */
    sts = ownBGRToYCbCr420_8u_AC4P3R( pSrc[0] + pSrcStep[0], pSrcStep[0]<<1,
                                      pDstSecondField, pDstFieldStep, roiFieldSize );

  }

  return sts;

} // IppStatus cc_RGB4_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo, ... )

IppStatus cc_NV12_to_YUY2( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                          mfxFrameData* outData, mfxFrameInfo* outInfo )
{
    IppStatus sts = ippStsNoErr;
    IppiSize  roiSize = {0, 0};
    /*mfxU16    cropX = 0, cropY = 0;
    mfxU32    inOffset0, inOffset1;
    mfxU32    outOffset0;

    VPP_GET_WIDTH(inInfo, roiSize.width);
    VPP_GET_HEIGHT(inInfo, roiSize.height);

    VPP_GET_CROPX(inInfo,  cropX);
    VPP_GET_CROPY(inInfo,  cropY);

    inOffset0  = cropX    + cropY*inData->Pitch;
    inOffset1  = (cropX) + (cropY>>1)*(inData->Pitch);

    VPP_GET_CROPX(outInfo,  cropX);
    VPP_GET_CROPY(outInfo,  cropY);

    outOffset0 =  cropX*2 + cropY*inData->Pitch;

    mfxU8* pSrc[2]   = {(mfxU8*)inData->Y + inOffset0, (mfxU8*)inData->UV + inOffset1};

    mfxI32 srcStep[2] = {inData->Pitch, inData->Pitch >> 0};

    mfxU8* pDst = (mfxU8*)outData->Y + outOffset0;

    mfxI32 dstStep = outData->Pitch;

    sts = ippiYCbCr420ToYCbCr422_8u_P2C2R(pSrc[0], srcStep[0],
                                          pSrc[1], srcStep[1],
                                          pDst, dstStep,
                                          roiSize);
    return sts;*/

    outInfo;

    VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
    VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

    mfxU8* pSrc[2]   = {(mfxU8*)inData->Y, (mfxU8*)inData->UV};

    mfxI32 srcStep[2] = {inData->Pitch, inData->Pitch >> 0};

    mfxU8* pDst = (mfxU8*)outData->Y;

    mfxI32 dstStep = outData->Pitch;

    sts = ippiYCbCr420ToYCbCr422_8u_P2C2R(pSrc[0], srcStep[0],
                                          pSrc[1], srcStep[1],
                                          pDst, dstStep,
                                          roiSize);
    return sts;

} // IppStatus cc_NV12_to_YUY2( mfxFrameData* inData,  mfxFrameInfo* inInfo, ... )


//static void tst_ycbcr420_rgb_p2c4(
//    Ipp8u* src,
//    int sStep,
//    Ipp8u* ref,
//    int rStep,
//    IppiSize size,
//    Ipp8u aValue,
//    int typeRGB,
//    int typeCBCR)
//{
//    Ipp8u *redL1, *greenL1, *blueL1, *alphaL1, *redH1, *greenH1, *blueH1, *alphaH1;
//    Ipp8u *redL2, *greenL2, *blueL2, *alphaL2, *redH2, *greenH2, *blueH2, *alphaH2;
//    Ipp8u *srcYL1, *srcYH1, *srcYL2, *srcYH2, *srcCb, *srcCr, *srcCbCr;
//    double rL1, gL1, bL1, rH1, gH1, bH1, rL2, gL2, bL2, rH2, gH2, bH2;
//    double yL1, yH1, yL2, yH2, cb, cr;
//    int i, j, s_y, s_cbcr, d, width, height;
//
//    width  = size.width >> 1;
//    height = size.height >> 1;
//
//    srcCbCr = src;
//    for(j = 0; j < height; j++) {
//        srcYL1 = src;
//        srcYH1 = src + 1;
//        srcYL2 = src + sStep;
//        srcYH2 = src + sStep + 1;
//        if (typeCBCR == CBCR) {
//            srcCb = srcCbCr + sStep * size.height;
//            srcCr = srcCbCr + sStep * size.height + 1;
//        }
//        else {
//            srcCb = srcCbCr + sStep * size.height + 1;
//            srcCr = srcCbCr + sStep * size.height;
//        }
//        if(typeRGB == RGB) {
//            redL1 = ref;
//            blueL1 = ref + 2;
//            redH1 = ref + 4;
//            blueH1 = ref + 6;
//            redL2 = ref + rStep;
//            blueL2 = ref + rStep + 2;
//            redH2 = ref + rStep + 4;
//            blueH2 = ref + rStep + 6;
//        } /* if */
//        else {
//            redL1 = ref + 2;
//            blueL1 = ref;
//            redH1 = ref + 6;
//            blueH1 = ref + 4;
//            redL2 = ref + rStep + 2;
//            blueL2 = ref + rStep;
//            redH2 = ref + rStep + 6;
//            blueH2 = ref + rStep + 4;
//        } /* else */
//        greenL1 = ref + 1;
//        alphaL1 = ref + 3;
//        greenH1 = ref + 5;
//        alphaH1 = ref + 7;
//        greenL2 = ref + rStep + 1;
//        alphaL2 = ref + rStep + 3;
//        greenH2 = ref + rStep + 5;
//        alphaH2 = ref + rStep + 7;
//        for(i = s_y = s_cbcr = d = 0; i < width; i++, s_y += 2, s_cbcr += 2, d += 8) {
//            yL1 = (double)srcYL1[s_y] - 16;
//            yH1 = (double)srcYH1[s_y] - 16;
//            yL2 = (double)srcYL2[s_y] - 16;
//            yH2 = (double)srcYH2[s_y] - 16;
//            cb = (double)srcCb[s_cbcr] - 128;
//            cr = (double)srcCr[s_cbcr] - 128;
//
//            rL1 =  1.164 * yL1 + 1.596 * cr;
//            gL1 =  1.164 * yL1 - 0.392 * cb - 0.813 * cr;
//            bL1 =  1.164 * yL1 + 2.017 * cb;
//            rH1 =  1.164 * yH1 + 1.596 * cr;
//            gH1 =  1.164 * yH1 - 0.392 * cb - 0.813 * cr;
//            bH1 =  1.164 * yH1 + 2.017 * cb;
//
//            redL1[d] = SAT8UNEAR(rL1, double);
//            greenL1[d] = SAT8UNEAR(gL1, double);
//            blueL1[d] = SAT8UNEAR(bL1, double);
//            alphaL1[d] = aValue;
//            redH1[d] = SAT8UNEAR(rH1, double);
//            greenH1[d] = SAT8UNEAR(gH1, double);
//            blueH1[d] = SAT8UNEAR(bH1, double);
//            alphaH1[d] = aValue;
//
//            rL2 =  1.164 * yL2 + 1.596 * cr;
//            gL2 =  1.164 * yL2 - 0.392 * cb - 0.813 * cr;
//            bL2 =  1.164 * yL2 + 2.017 * cb;
//            rH2 =  1.164 * yH2 + 1.596 * cr;
//            gH2 =  1.164 * yH2 - 0.392 * cb - 0.813 * cr;
//            bH2 =  1.164 * yH2 + 2.017 * cb;
//
//            redL2[d] = SAT8UNEAR(rL2, double);
//            greenL2[d] = SAT8UNEAR(gL2, double);
//            blueL2[d] = SAT8UNEAR(bL2, double);
//            alphaL2[d] = aValue;
//            redH2[d] = SAT8UNEAR(rH2, double);
//            greenH2[d] = SAT8UNEAR(gH2, double);
//            blueH2[d] = SAT8UNEAR(bH2, double);
//            alphaH2[d] = aValue;
//
//        } /* for */
//        src += (2 * sStep);
//        srcCbCr += sStep;
//        ref += (2 * rStep);
//    } /* for */
//
//    return;
//
//} // void tst_ycbcr420_rgb_p2c4(...)


IppStatus cc_NV12_to_RGB4(
    mfxFrameData* inData,
    mfxFrameInfo* inInfo,
    mfxFrameData* outData,
    mfxFrameInfo* outInfo,

    mfxFrameData* yv12Data)
{
    IppStatus ippSts;

    ippSts = cc_NV12_to_YV12(inData,  inInfo, yv12Data, inInfo );
    if(ippStsNoErr != ippSts) return ippSts;

    //const mfxU8* pSrc[3]    = {(mfxU8*)yv12Data->Y,     (mfxU8*)yv12Data->V, (mfxU8*)yv12Data->U};
    const mfxU8* pSrc[3]    = {(mfxU8*)yv12Data->Y,     (mfxU8*)yv12Data->U, (mfxU8*)yv12Data->V};
    mfxI32 srcStep[3] = {yv12Data->Pitch, yv12Data->Pitch >> 1, yv12Data->Pitch >> 1};

    IPP_CHECK_NULL_PTR1(outData->R);
    IPP_CHECK_NULL_PTR1(outData->G);
    IPP_CHECK_NULL_PTR1(outData->B);
    // alpha channel is ignore due to d3d issue

    mfxU8*   ptrStart = IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );
    IppiSize roiSize = {outInfo->Width, outInfo->Height};
    const mfxU8  AVAL = 0;

    //ippSts = ippiYCrCb420ToRGB_8u_P3C4R(pSrc, srcStep, ptrStart, outData->Pitch, roiSize, AVAL);
    ippSts = ippiYCbCr420ToBGR_8u_P3C4R(pSrc, srcStep, ptrStart, outData->Pitch, roiSize, AVAL);

    return ippSts;

} // IppStatus cc_NV12_to_RGB4(...)

#endif // MFX_ENABLE_VPP
/* EOF */
