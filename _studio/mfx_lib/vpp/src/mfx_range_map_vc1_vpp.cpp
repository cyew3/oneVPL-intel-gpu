/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//
//
//          RangeMapVC1 for Video Pre\Post Processing
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP) && !defined (MFX_ENABLE_HW_ONLY_VPP)

#include "mfx_range_map_vc1_vpp.h"
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"
#include "ippvc.h"

#ifndef VPP_CHECK_IPP_STS
#define VPP_CHECK_IPP_STS(err) if (err != ippStsNoErr) {return MFX_ERR_UNDEFINED_BEHAVIOR;}
#endif

MFXVideoVPPRangeMapVC1::MFXVideoVPPRangeMapVC1(VideoCORE *core, mfxStatus* sts ) : FilterVPP()
{
  //MFX_CHECK_NULL_PTR1(core);

  m_core = core;

  memID  = NULL;

  RangeMap = VPP_RM_VC1_DEFAULT_VALUE;

  GetMapCoeffs();

  *sts = MFX_ERR_NONE;

  return;
}

MFXVideoVPPRangeMapVC1::~MFXVideoVPPRangeMapVC1(void)
{
  Close();

  return;
}

mfxStatus MFXVideoVPPRangeMapVC1::Reset(mfxVideoParam *par)
{
  //mfxStatus mfxSts = MFX_ERR_NONE;

  if( NULL == par ){
    return MFX_ERR_NULL_PTR;
  }

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPRangeMapVC1::RunFrameVPP(mfxFrameSurface1 *in,
                                              mfxFrameSurface1 *out,
                                              InternalParam* pParam)
{
  // code error
  IppStatus ippSts = ippStsNoErr;
  mfxStatus mfxSts = MFX_ERR_NONE;

  mfxU32  srcFourCC = MFX_FOURCC_YV12; //default
  mfxU32  dstFourCC = MFX_FOURCC_YV12; //default

  IppiSize roiSize = {0, 0};

  mfxU16   srcW = 0, srcH = 0, dstW = 0, dstH = 0;

  // scaleFactors for YV12
  const mfxU16 ScaleCPitch  = 1;
  const mfxU16 ScaleCWidth  = 1;
  const mfxU16 ScaleCHeight = 1;

  mfxFrameData* inData  = NULL;
  mfxFrameData* outData = NULL;
  mfxFrameInfo* inInfo  = NULL;
  mfxFrameInfo* outInfo = NULL;

  MFX_CHECK_NULL_PTR2( in, out );

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
  srcFourCC = inInfo->FourCC;
  dstFourCC = outInfo->FourCC;

  if( srcFourCC != dstFourCC ){
    mfxSts = MFX_ERR_INVALID_VIDEO_PARAM;
    return mfxSts;
  }

  if( srcW != dstW || srcH != dstH ){
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  roiSize.height = srcH;
  roiSize.width  = srcW;

  pParam;

  //------------------------------------
  //       algorithm
  //------------------------------------
  if (RANGE_MAPY_FLAG){

    ippSts = ippiRangeMapping_VC1_8u_C1R(inData->Y,
                                inData->Pitch,
                                outData->Y,
                                outData->Pitch,
                                roiSize, RANGE_MAPY);

    VPP_CHECK_IPP_STS( ippSts );
  } else if( inData->Y != outData->Y ) {
    // it is expected to apply this filter as IMPLACE algorithm,
    // but we should copy data in the case of non-implace operation
    ippSts = ippiCopy_8u_C1R(inData->Y, inData->Pitch,
                             outData->Y, outData->Pitch, roiSize);

    VPP_CHECK_IPP_STS( ippSts );
  }

  if (RANGE_MAPUV_FLAG){
  mfxU16  inPitch  = inData->Pitch >> ScaleCPitch;
  mfxU16  outPitch = outData->Pitch >> ScaleCPitch;

    roiSize.height >>= ScaleCHeight;
    roiSize.width  >>= ScaleCWidth;

    ippSts = ippiRangeMapping_VC1_8u_C1R(inData->U,
                                inPitch,
                                outData->U,
                                outPitch,
                                roiSize, RANGE_MAPUV);

    VPP_CHECK_IPP_STS( ippSts );

    ippSts = ippiRangeMapping_VC1_8u_C1R(inData->V,
                                 inPitch,
                                 outData->V,
                                 outPitch,
                                 roiSize, RANGE_MAPUV);

    VPP_CHECK_IPP_STS( ippSts );
  } else if( (inData->U != outData->U) || (inData->V != outData->V) ){
    mfxU16  inPitch  = inData->Pitch >> ScaleCPitch;
    mfxU16  outPitch = outData->Pitch >> ScaleCPitch;

    roiSize.height >>= ScaleCHeight;
    roiSize.width  >>= ScaleCWidth;

    ippSts = ippiCopy_8u_C1R(inData->U, inPitch,
                             outData->U, outPitch, roiSize);

    VPP_CHECK_IPP_STS( ippSts );

    ippSts = ippiCopy_8u_C1R(inData->V, inPitch,
                             outData->V, outPitch, roiSize);

    VPP_CHECK_IPP_STS( ippSts );
  }

  if( pParam )
  {
      pParam->outPicStruct = pParam->inPicStruct;
      pParam->outTimeStamp = pParam->inTimeStamp;
  }

  return mfxSts;
}

mfxStatus MFXVideoVPPRangeMapVC1::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
{
  mfxStatus mfxSts = MFX_ERR_NONE;
  mfxI32    srcW   = 0, srcH = 0;
  mfxI32    dstW   = 0, dstH = 0;

  mfxU32  srcFourCC = MFX_FOURCC_YV12; //default
  mfxU32  dstFourCC = MFX_FOURCC_YV12; //default

  MFX_CHECK_NULL_PTR2( In, Out );

  //------------------------------

  /* IN */
  VPP_GET_WIDTH(In, srcW);
  VPP_GET_HEIGHT(In, srcH);

  /* OUT */
  VPP_GET_WIDTH(Out, dstW);
  VPP_GET_HEIGHT(Out, dstH);

  //------------------------------
  /* robustness check */
  if( srcW != dstW || srcH != dstH ){
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  srcFourCC = In->FourCC;
  dstFourCC = Out->FourCC;

  if( srcFourCC != dstFourCC ){
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  if( srcFourCC != MFX_FOURCC_YV12 ){
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  RangeMap = VPP_RM_VC1_DEFAULT_VALUE;

  GetMapCoeffs();

  return mfxSts;
}

mfxStatus MFXVideoVPPRangeMapVC1::Close(void)
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  return mfxSts;
}

mfxStatus MFXVideoVPPRangeMapVC1::SetParam( mfxExtBuffer* pHint )
{
  mfxStatus mfxSts = MFX_ERR_NONE;
  mfxU32*   pHint32 = NULL;

  pHint32 = (mfxU32*)pHint;

#if 0
  if( MFX_FOURCC_VPP_RANGE_MAP_VC1 != pHint32[0] ){
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  if( 4 != pHint32[1] ){
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  RangeMap = pHint[8];
#endif

  GetMapCoeffs();

  return mfxSts;
}

mfxStatus  MFXVideoVPPRangeMapVC1::GetMapCoeffs( void )
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  RANGE_MAPY_FLAG = (RangeMap >> 7) & 1;
  RANGE_MAPY      = (RangeMap >> 4) & 7;

  RANGE_MAPUV       = RangeMap & 7;
  RANGE_MAPUV_FLAG  = (RangeMap >> 3) & 1;

  return mfxSts;
}


// work buffer management - nothing for RM_VC1 filter
mfxStatus MFXVideoVPPRangeMapVC1::GetBufferSize( mfxU32* pBufferSize )
{
  VPP_CHECK_NOT_INITIALIZED;

  MFX_CHECK_NULL_PTR1(pBufferSize);

  *pBufferSize = 0;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPRangeMapVC1::SetBuffer( mfxU8* pBuffer )
{
  VPP_CHECK_NOT_INITIALIZED;

  // RM_VC1 dosn't require work buffer, so pBuffer == NULL is OK
  if( pBuffer )
  {
    //something
  }

  return MFX_ERR_NONE;
}

// function is called from sync part of VPP only
mfxStatus MFXVideoVPPRangeMapVC1::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
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

bool MFXVideoVPPRangeMapVC1::IsReadyOutput( mfxRequestType requestType )
{
    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
        // TO DO
    }

    return false;
}

#endif // MFX_ENABLE_VPP
/* EOF */