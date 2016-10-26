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

#include "mfx_vpp_utils.h"
#include "mfx_resize_vpp.h"
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"

/* ******************************************************************** */
/*                 prototype of Resize algorithms                       */
/* ******************************************************************** */

IppStatus rs_YV12( mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxU8* pWorkBuf, mfxU16 picStruct );

IppStatus rs_NV12_v2( mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxU8* pWorkBuf, mfxU16 picStruct );

IppStatus rs_P010( mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxU8* pWorkBuf, mfxU16 picStruct );

mfxStatus owniResizeGetBufSize_UpEstimation( IppiSize srcSize, IppiSize dstSize, int *pBufferSize );

// aya: msdk 3.0 requirements: rgb32 as output
IppStatus rs_RGB32( mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxU8* pWorkBuf, mfxU16 picStruct );
/* ******************************************************************** */
/*                 implementation of VPP filter [Resize]                */
/* ******************************************************************** */

MFXVideoVPPResize::MFXVideoVPPResize(VideoCORE *core, mfxStatus* sts ) : FilterVPP()
{
  //MFX_CHECK_NULL_PTR1(core);

  m_core = core;

  m_pWorkBuf          = NULL;

  VPP_CLEAN;

  *sts = MFX_ERR_NONE;

  return;
}

MFXVideoVPPResize::~MFXVideoVPPResize(void)
{
  Close();

  return;
}

mfxStatus MFXVideoVPPResize::SetParam( mfxExtBuffer* pHint )
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  if( pHint ){
    // mfxSts = something
  }

  return mfxSts;
}

mfxStatus MFXVideoVPPResize::Reset(mfxVideoParam *par)
{
  mfxStatus mfxSts = MFX_ERR_NONE;
  IppStatus ippSts = ippStsNoErr;

  mfxU32 sizeBuf = 0;

  MFX_CHECK_NULL_PTR1( par );

  VPP_CHECK_NOT_INITIALIZED;

  // in case of RS, MUST BE in == out excluding PicStruct.
  // so, some check is redundant.
  // But m_errPrtctState contains correct info.

  // simple checking wo analysis
  if( ( m_errPrtctState.In.FourCC  != par->vpp.In.FourCC || m_errPrtctState.Out.FourCC != par->vpp.Out.FourCC )
     && m_errPrtctState.In.FourCC  != MFX_FOURCC_P010
     && m_errPrtctState.In.FourCC  != MFX_FOURCC_P210
     && m_errPrtctState.Out.FourCC != MFX_FOURCC_A2RGB10)
  {
      return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  if( m_errPrtctState.Out.Width < par->vpp.Out.Width || m_errPrtctState.Out.Height < par->vpp.Out.Height )
  {
      return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  if( m_errPrtctState.In.Width < par->vpp.In.Width || m_errPrtctState.In.Height < par->vpp.In.Height )
  {
      return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
  }

  if( (m_errPrtctState.In.PicStruct != par->vpp.In.PicStruct) || (m_errPrtctState.Out.PicStruct != par->vpp.Out.PicStruct) )
  {
      return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* reset resize work buffer */
  mfxSts = GetBufferSize( &sizeBuf );
  MFX_CHECK_STS( mfxSts );

  if( sizeBuf )
  {
    MFX_CHECK_NULL_PTR1(m_pWorkBuf);

    ippSts = ippsZero_8u(m_pWorkBuf, sizeBuf );
    VPP_CHECK_IPP_STS( ippSts );
  }

  VPP_RESET;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPResize::RunFrameVPP(mfxFrameSurface1 *in,
                                         mfxFrameSurface1 *out,
                                         InternalParam* pParam)
{
  // code error
  IppStatus ippSts = ippStsNoErr;
  mfxStatus mfxSts = MFX_ERR_NONE;

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

  /* in case of cropping fast copy can be used in case of inInfo == outInfo */
  if( in->Info.CropH == out->Info.CropH &&
      in->Info.CropW == out->Info.CropW /*&&
      in->Info.CropX == out->Info.CropX &&
      in->Info.CropY == out->Info.CropY &&
      in->Info.CropH && in->Info.CropW*/){

      //mfxSts = m_core->DoFastCopy(out, in);
      mfxSts = SurfaceCopy_ROI(out, in);

      pParam->outPicStruct = pParam->inPicStruct;
      pParam->outTimeStamp = pParam->inTimeStamp;

      return mfxSts;
  }

  switch ( m_errPrtctState.In.FourCC )
  {
    case MFX_FOURCC_NV12:
      ippSts = rs_NV12_v2(in, out, m_pWorkBuf, pParam->inPicStruct);

      break;

    case MFX_FOURCC_P010:
      ippSts = rs_P010(in, out, m_pWorkBuf, pParam->inPicStruct);

      break;

    case MFX_FOURCC_YV12:
      ippSts = rs_YV12(in, out, m_pWorkBuf, pParam->inPicStruct);

      break;

    case MFX_FOURCC_RGB4:
      ippSts = rs_RGB32(in, out, m_pWorkBuf, pParam->inPicStruct);
      break;

    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;

  }

  /* common part */
  mfxSts = (ippSts == ippStsNoErr) ? MFX_ERR_NONE : MFX_ERR_UNDEFINED_BEHAVIOR;
  MFX_CHECK_STS( mfxSts );

  if( !m_errPrtctState.isFirstFrameProcessed )
  {
      m_errPrtctState.isFirstFrameProcessed = true;
  }

  pParam->outPicStruct = pParam->inPicStruct;
  pParam->outTimeStamp = pParam->inTimeStamp;

  return mfxSts;
}

mfxStatus MFXVideoVPPResize::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
{
  mfxU16    srcW   = 0, srcH = 0;
  mfxU16    dstW   = 0, dstH = 0;

  MFX_CHECK_NULL_PTR2( In, Out );

  VPP_CHECK_MULTIPLE_INIT;

  /* IN */
  VPP_GET_REAL_WIDTH(In,  srcW);
  VPP_GET_REAL_HEIGHT(In, srcH);

  /* OUT */
  VPP_GET_REAL_WIDTH(Out,  dstW);
  VPP_GET_REAL_HEIGHT(Out, dstH);

  //------------------------------
  /* robustness check */
  if( srcW == dstW && srcH == dstH ){
    //return MFX_ERR_INVALID_VIDEO_PARAM;
    // cropping possible
  }

  if( In->FourCC != Out->FourCC ){
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* save init params to prevent core crash */
  m_errPrtctState.In  = *In;
  m_errPrtctState.Out = *Out;

  /* FourCC checking */
  switch ( m_errPrtctState.In.FourCC )
  {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_P210:
    case MFX_FOURCC_A2RGB10:
    case MFX_FOURCC_NV16:
    case MFX_FOURCC_YUY2:
      VPP_INIT_SUCCESSFUL;
      break;

    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPResize::Close(void)
{

  VPP_CHECK_NOT_INITIALIZED;

  m_pWorkBuf = NULL;

  VPP_CLEAN;

  return MFX_ERR_NONE;
}


mfxStatus MFXVideoVPPResize::GetBufferSize( mfxU32* pBufferSize )
{
  IppiRect  srcRect, dstRect;
  IppStatus ippSts = ippStsNoErr;
  mfxStatus mfxSts = MFX_ERR_NONE;

  VPP_CHECK_NOT_INITIALIZED;

  srcRect.x = 0;
  srcRect.y = 0;
  srcRect.width = m_errPrtctState.In.Width;
  srcRect.height= m_errPrtctState.In.Height;

  dstRect.x = 0;
  dstRect.y = 0;
  dstRect.width = m_errPrtctState.Out.Width;
  dstRect.height= m_errPrtctState.Out.Height;

  IppiSize srcSize = {srcRect.width, srcRect.height};
  IppiSize dstSize = {dstRect.width, dstRect.height};

  mfxI32 bufSizeLanczos = 0, bufSizeSuper = 0, bufSize = 0;

  if( (MFX_FOURCC_YV12 == m_errPrtctState.In.FourCC) || (MFX_FOURCC_RGB4 == m_errPrtctState.In.FourCC) )
  {
      int channelCount = (MFX_FOURCC_YV12 == m_errPrtctState.In.FourCC) ? 1 : 4;
      //ignore sts. super_sampling not valid in case upscale. sts == ippStsSizeErr, bufSize=0
      ippSts = ippiResizeGetBufSize( srcRect, dstRect, channelCount, IPPI_INTER_SUPER, &bufSizeSuper );

      ippSts = ippiResizeGetBufSize( srcRect, dstRect, channelCount, IPPI_INTER_LANCZOS, &bufSizeLanczos );
  }
  else if(MFX_FOURCC_NV12 == m_errPrtctState.In.FourCC)
  {
      //ignore sts. super_sampling not valid in case upscale. sts == ippStsSizeErr, bufSize=0
      ippSts = ippiResizeYUV420GetBufSize(srcSize, dstSize, IPPI_INTER_SUPER, &bufSizeSuper);

      ippSts = ippiResizeYUV420GetBufSize(srcSize, dstSize, IPPI_INTER_LANCZOS, &bufSizeLanczos);
  }
  else if (MFX_FOURCC_P010 == m_errPrtctState.In.FourCC)
  {
        ippSts = ippiResizeGetBufSize( srcRect, dstRect, 1, IPPI_INTER_SUPER, &bufSizeSuper );
        ippSts = ippiResizeGetBufSize( srcRect, dstRect, 1, IPPI_INTER_LANCZOS, &bufSizeLanczos );
        /* Expect that x2 bytes needed. Potential bug. */
        bufSizeSuper   <<= 1;
        bufSizeLanczos <<= 1;

        // Additional space for resize operation
        mfxU32 add = IPP_MAX(srcRect.width, dstRect.width) * IPP_MAX(srcRect.height, dstRect.height) * 4;
        bufSizeSuper   += add;
        bufSizeLanczos += add;
  }

  VPP_CHECK_IPP_STS( ippSts );

  bufSize = IPP_MAX(bufSizeSuper, bufSizeLanczos);

  // correction: super_sampling method can require more work buffer memory in case of cropping
  mfxI32 bufSize_UpEstimation = 0;

  mfxSts = owniResizeGetBufSize_UpEstimation( srcSize, dstSize, &bufSize_UpEstimation );
  MFX_CHECK_STS( mfxSts );

  bufSize = IPP_MAX(bufSize, bufSize_UpEstimation);


  *pBufferSize = bufSize;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPResize::SetBuffer( mfxU8* pBuffer )
{
  mfxU32 bufSize = 0;
  mfxStatus sts;

  VPP_CHECK_NOT_INITIALIZED;

  sts = GetBufferSize( &bufSize );
  MFX_CHECK_STS( sts );

  if( bufSize )
  {
    MFX_CHECK_NULL_PTR1(pBuffer);

    m_pWorkBuf = pBuffer;

    ippsZero_8u( m_pWorkBuf, bufSize);//sts ignored
  }

  return MFX_ERR_NONE;
}

// function is called from sync part of VPP only
mfxStatus MFXVideoVPPResize::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
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

bool MFXVideoVPPResize::IsReadyOutput( mfxRequestType requestType )
{
    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
        // RS is simple algorithm and need IN to produce OUT
    }

    return false;
}

/* ******************************************************************** */
/*                 implementation of algorithms [Resize]                */
/* ******************************************************************** */

IppStatus rs_YV12( mfxFrameSurface1* in, mfxFrameSurface1* out, mfxU8* pWorkBuf, mfxU16 picStruct )
{
  IppStatus sts     = ippStsNotSupportedModeErr;
  IppiSize  srcSize = {0,  0};
  IppiSize  dstSize = {0, 0};
  mfxF64    xFactor = 0.0, yFactor = 0.0;
  mfxF64    xShift = 0.0, yShift = 0.0;
  IppiRect  srcRect, dstRect;

  mfxU16 cropX = 0, cropY = 0;

  mfxU32 inOffset0  = 0, inOffset1  = 0;
  mfxU32 outOffset0 = 0, outOffset1 = 0;

  mfxFrameData* inData = &in->Data;
  mfxFrameInfo* inInfo = &in->Info;
  mfxFrameData* outData= &out->Data;
  mfxFrameInfo* outInfo= &out->Info;

  /* [IN] */
  VPP_GET_CROPX(inInfo, cropX);
  VPP_GET_CROPY(inInfo, cropY);
  inOffset0 = cropX + cropY*inData->Pitch;
  inOffset1 = (cropX>>1) + (cropY>>1)*(inData->Pitch >> 1);

  /* [OUT] */
  VPP_GET_CROPX(outInfo, cropX);
  VPP_GET_CROPY(outInfo, cropY);
  outOffset0 = cropX + cropY*outData->Pitch;
  outOffset1 = (cropX>>1) + (cropY>>1)*(outData->Pitch >> 1);

  const mfxU8* pSrc[3] = {(mfxU8*)inData->Y + inOffset0,
                          (mfxU8*)inData->V + inOffset1,
                          (mfxU8*)inData->U + inOffset1};

  mfxI32 pSrcStep[3] = {inData->Pitch,
                        inData->Pitch >> 1,
                        inData->Pitch >> 1};

  mfxU8* pDst[3]   = {(mfxU8*)outData->Y + outOffset0,
                      (mfxU8*)outData->V + outOffset1,
                      (mfxU8*)outData->U + outOffset1};

  mfxI32 pDstStep[3] = {outData->Pitch,
                        outData->Pitch >> 1,
                        outData->Pitch >> 1};

  /* common part */
  VPP_GET_WIDTH(inInfo,  srcSize.width);
  VPP_GET_HEIGHT(inInfo, srcSize.height);

  VPP_GET_WIDTH(outInfo,  dstSize.width);
  VPP_GET_HEIGHT(outInfo, dstSize.height);

  /* tune part */
  mfxI32 interpolation;
  xFactor = (mfxF64)dstSize.width  / (mfxF64)srcSize.width;
  yFactor = (mfxF64)dstSize.height / (mfxF64)srcSize.height;

  if( xFactor < 1.0 && yFactor < 1.0 )
  {
    interpolation = IPPI_INTER_SUPER;
  }
  else
  {
    interpolation = IPPI_INTER_LANCZOS;
  }

  if( MFX_PICSTRUCT_PROGRESSIVE & picStruct)
  {
    /* Y */
    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.height = srcSize.height;
    srcRect.width  = srcSize.width;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.height = dstSize.height;
    dstRect.width  = dstSize.width;

    xShift = 0.0, yShift = 0.0;

    sts = ippiResizeSqrPixel_8u_C1R(pSrc[0], srcSize, pSrcStep[0], srcRect,
                                   pDst[0], pDstStep[0], dstRect,
                                   xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

    if( ippStsNoErr != sts ) return sts;

    /* UV common */
    srcSize.width  = srcSize.width  >> 1;
    srcSize.height = srcSize.height >> 1;

    dstSize.width = dstSize.width  >> 1;
    dstSize.height= dstSize.height >> 1;

    /* V */
    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.height = srcSize.height;
    srcRect.width  = srcSize.width;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.height = dstSize.height;
    dstRect.width  = dstSize.width;

    sts = ippiResizeSqrPixel_8u_C1R(pSrc[1], srcSize, pSrcStep[1], srcRect,
                                    pDst[1], pDstStep[1], dstRect,
                                    xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

    if( ippStsNoErr != sts ) return sts;

    /* U */
    // srcRect - the same
    sts = ippiResizeSqrPixel_8u_C1R(pSrc[2], srcSize, pSrcStep[2], srcRect,
                                    pDst[2], pDstStep[2], dstRect,
                                    xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

  }
  else //interlaced video
  {
    IppiSize  srcSizeIntr = {0, 0};
    IppiSize  dstSizeIntr = {0, 0};
    mfxI32    srcStepIntr = 0;
    mfxI32    dstStepIntr = 0;

    /* Y */
    /* common */
    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.height = srcSize.height >> 1;
    srcRect.width  = srcSize.width;

    srcSizeIntr.width = srcRect.width;
    srcSizeIntr.height= srcRect.height;

    srcStepIntr = pSrcStep[0] << 1;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.height = dstSize.height >> 1;
    dstRect.width  = dstSize.width;

    dstSizeIntr.width = dstSize.width;
    dstSizeIntr.height= dstSize.height >> 1;

    dstStepIntr = pDstStep[0] << 1;

    /* top field */
    sts = ippiResizeSqrPixel_8u_C1R(pSrc[0], srcSizeIntr, srcStepIntr, srcRect,
                                    pDst[0], dstStepIntr, dstRect,
                                    xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

    if( ippStsNoErr != sts ) return sts;

    /* bottom field */
    sts = ippiResizeSqrPixel_8u_C1R(pSrc[0]+pSrcStep[0], srcSizeIntr, srcStepIntr, srcRect,
                                    pDst[0]+pDstStep[0], dstStepIntr, dstRect,
                                    xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

    if( ippStsNoErr != sts ) return sts;

    /* UV common */
    srcSizeIntr.width  = srcSize.width  >> 1;
    srcSizeIntr.height = (srcSize.height >> 1) >> 1;

    dstSizeIntr.width  = dstSize.width  >> 1;
    dstSizeIntr.height = (dstSize.height >> 1) >> 1;

    srcStepIntr = pSrcStep[1] << 1;
    dstStepIntr = pDstStep[1] << 1;

    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.height = srcSizeIntr.height;
    srcRect.width  = srcSizeIntr.width;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.height = dstSizeIntr.height;
    dstRect.width  = dstSizeIntr.width;

    /* V top field */
    /*sts = ippiResize_8u_C1R(pSrc[1], srcSizeIntr, srcStepIntr, srcRect,
                            pDst[1], dstStepIntr, dstSizeIntr,
                            xFactor, yFactor, mInterpolation);*/

    sts = ippiResizeSqrPixel_8u_C1R(pSrc[1], srcSizeIntr, srcStepIntr, srcRect,
                                    pDst[1], dstStepIntr, dstRect,
                                    xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

    if( ippStsNoErr != sts ) return sts;

    /* V bottom field */
    sts = ippiResizeSqrPixel_8u_C1R(pSrc[1]+pSrcStep[1], srcSizeIntr, srcStepIntr, srcRect,
                                    pDst[1]+pDstStep[1], dstStepIntr, dstRect,
                                    xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

    if( ippStsNoErr != sts ) return sts;

    /* U */
    srcStepIntr = pSrcStep[2] << 1;
    dstStepIntr = pDstStep[2] << 1;

    /* top field */
    sts = ippiResizeSqrPixel_8u_C1R(pSrc[2], srcSizeIntr, srcStepIntr, srcRect,
                                    pDst[2], dstStepIntr, dstRect,
                                    xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

    if( ippStsNoErr != sts ) return sts;

    /* bottom field */
    sts = ippiResizeSqrPixel_8u_C1R(pSrc[2]+pSrcStep[2], srcSizeIntr, srcStepIntr, srcRect,
                                    pDst[2]+pDstStep[2], dstStepIntr, dstRect,
                                    xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

    if( ippStsNoErr != sts ) return sts;
  }

  return sts;
}

/* ************************************************************************* */
/* NV12 optimization */
IppStatus rs_NV12_v2( mfxFrameSurface1* in, mfxFrameSurface1* out, mfxU8* pWorkBuf, mfxU16 picStruct )
{
    IppStatus sts     = ippStsNotSupportedModeErr;
    IppiSize  srcSize = {0,  0};
    IppiSize  dstSize = {0, 0};

    mfxU16 cropX = 0, cropY = 0;

    mfxU32 inOffset0  = 0, inOffset1  = 0;
    mfxU32 outOffset0 = 0, outOffset1 = 0;

    mfxFrameData* inData = &in->Data;
    mfxFrameInfo* inInfo = &in->Info;
    mfxFrameData* outData= &out->Data;
    mfxFrameInfo* outInfo= &out->Info;

    /* [IN] */
    VPP_GET_CROPX(inInfo, cropX);
    VPP_GET_CROPY(inInfo, cropY);
    inOffset0 = cropX + cropY*inData->Pitch;
    inOffset1 = (cropX) + (cropY>>1)*(inData->Pitch);

    /* [OUT] */
    VPP_GET_CROPX(outInfo, cropX);
    VPP_GET_CROPY(outInfo, cropY);
    outOffset0 = cropX + cropY*outData->Pitch;
    outOffset1 = (cropX) + (cropY>>1)*(outData->Pitch);

    const mfxU8* pSrc[2] = {(mfxU8*)inData->Y + inOffset0,
                            (mfxU8*)inData->UV + inOffset1};

    mfxI32 pSrcStep[2] = {inData->Pitch, inData->Pitch};

    mfxU8* pDst[2]   = {(mfxU8*)outData->Y + outOffset0,
                        (mfxU8*)outData->UV + outOffset1};

    mfxI32 pDstStep[2] = {outData->Pitch, outData->Pitch};

    /* common part */
    VPP_GET_WIDTH(inInfo,  srcSize.width);
    VPP_GET_HEIGHT(inInfo, srcSize.height);

    VPP_GET_WIDTH(outInfo,  dstSize.width);
    VPP_GET_HEIGHT(outInfo, dstSize.height);

    // tune part - disabled. reason - BufferSize is critical for
    mfxF64 xFactor, yFactor;
    mfxI32 interpolation;

    xFactor = (mfxF64)dstSize.width  / (mfxF64)srcSize.width;
    yFactor = (mfxF64)dstSize.height / (mfxF64)srcSize.height;

    if( xFactor < 1.0 && yFactor < 1.0 )
    {
        interpolation = IPPI_INTER_SUPER;
    }
    else
    {
        interpolation = IPPI_INTER_LANCZOS;
    }

    if( MFX_PICSTRUCT_PROGRESSIVE & picStruct)
    {
        sts = ippiResizeYUV420_8u_P2R(
                                pSrc[0], pSrcStep[0],
                                pSrc[1], pSrcStep[1],
                                srcSize,

                                pDst[0], pDstStep[0],
                                pDst[1], pDstStep[1],
                                dstSize,

                                interpolation, pWorkBuf);

        if( ippStsNoErr != sts ) return sts;

    }
    else //interlaced video
    {
        IppiSize  srcSizeIntr = {0, 0};
        IppiSize  dstSizeIntr = {0, 0};
        mfxI32    srcStepIntr[2] = {pSrcStep[0]<<1, pSrcStep[1]<<1};
        mfxI32    dstStepIntr[2] = {pDstStep[0]<<1, pDstStep[1]<<1};

        srcSizeIntr.width = srcSize.width;
        srcSizeIntr.height= srcSize.height >> 1;

        dstSizeIntr.width = dstSize.width;
        dstSizeIntr.height= dstSize.height >> 1;

        // height of interlace content must be alligned on 4
        srcSizeIntr.height = ((srcSizeIntr.height + 3) >> 2) << 2;
        dstSizeIntr.height = ((dstSizeIntr.height + 3) >> 2) << 2;

        // top field
        sts = ippiResizeYUV420_8u_P2R(
                                pSrc[0], srcStepIntr[0],
                                pSrc[1], srcStepIntr[1],
                                srcSizeIntr,

                                pDst[0], dstStepIntr[0],
                                pDst[1], dstStepIntr[1],
                                dstSizeIntr,

                                interpolation, pWorkBuf);

        if( ippStsNoErr != sts ) return sts;

        /* bottom field */
        sts = ippiResizeYUV420_8u_P2R(
            pSrc[0]+pSrcStep[0], srcStepIntr[0],
            pSrc[1]+pSrcStep[1], srcStepIntr[1],
            srcSizeIntr,

            pDst[0]+pDstStep[0], dstStepIntr[0],
            pDst[1]+pDstStep[1], dstStepIntr[1],
            dstSizeIntr,

            interpolation, pWorkBuf);
    }

    return sts;
}

/* ************************************************************************* */
/* P010 not optimized resize  */
IppStatus rs_P010( mfxFrameSurface1* in, mfxFrameSurface1* out, mfxU8* pWorkBuf, mfxU16 picStruct )
{
    IppStatus sts     = ippStsNotSupportedModeErr;
    IppiSize  srcSize = {0, 0};
    IppiSize  dstSize = {0, 0};

    IppiRect  srcRect = {0,0,0,0};
    IppiRect  dstRect = {0,0,0,0};
    IppiSize  roi     = {0, 0};

    mfxU16 cropX = 0, cropY = 0;

    mfxFrameData* inData = &in->Data;
    mfxFrameInfo* inInfo = &in->Info;
    mfxFrameData* outData= &out->Data;
    mfxFrameInfo* outInfo= &out->Info;

    /* [IN] */
    VPP_GET_CROPX(inInfo, cropX);
    VPP_GET_CROPY(inInfo, cropY);

    /* [OUT] */
    VPP_GET_CROPX(outInfo, cropX);
    VPP_GET_CROPY(outInfo, cropY);


    /* common part */
    VPP_GET_WIDTH(inInfo,  srcSize.width);
    VPP_GET_HEIGHT(inInfo, srcSize.height);

    VPP_GET_WIDTH(outInfo,  dstSize.width);
    VPP_GET_HEIGHT(outInfo, dstSize.height);

    VPP_GET_WIDTH(inInfo,  srcRect.width);
    VPP_GET_HEIGHT(inInfo, srcRect.height);

    VPP_GET_WIDTH(outInfo,  dstRect.width);
    VPP_GET_HEIGHT(outInfo, dstRect.height);

    // tune part - disabled. reason - BufferSize is critical for
    mfxF64 xFactor, yFactor;
    mfxI32 interpolation;

    xFactor = (mfxF64)dstSize.width  / (mfxF64)srcSize.width;
    yFactor = (mfxF64)dstSize.height / (mfxF64)srcSize.height;

    if( xFactor < 1.0 && yFactor < 1.0 )
    {
        interpolation = IPPI_INTER_SUPER;
    }
    else
    {
        interpolation = IPPI_INTER_LANCZOS;
    }

    mfxU8 *interPtr = pWorkBuf + inData->Pitch * (inInfo->Height) + inData->Pitch * (inInfo->Height/2);

    if( MFX_PICSTRUCT_PROGRESSIVE & picStruct)
    {
        /* Resize Y */
        sts = ippiResizeSqrPixel_16u_C1R((const Ipp16u *) inData->Y, srcSize,  inData->Pitch, srcRect,
                                               (Ipp16u *)outData->Y, outData->Pitch, dstRect,
                                              xFactor, yFactor, 0.0, 0.0, interpolation, pWorkBuf);
        if( ippStsNoErr != sts ) return sts;

        /* Clip Y */
        sts = ippiThreshold_16u_C1IR((Ipp16u*)outData->Y, outData->Pitch, dstSize, 0x03ff, ippCmpGreater);
        if( ippStsNoErr != sts ) return sts;

        /* Temporary NV12->YV12 conversion. At the moment IPP has no
         * function to resize interleaved UV 10bit.
         * Need to remove as soon as IPP has such support
         */
        roi.width  = 2;
        roi.height = (srcSize.width / 2);
        for ( int i = 0 ; i < inInfo->Height /2; i++)
        {
            sts = ippiCopy_8u_C1R(inData->UV + i*inData->Pitch, 4, pWorkBuf + i*inData->Pitch, roi.width, roi);
            if( ippStsNoErr != sts ) return sts;
        }


        mfxU8 *ptr = pWorkBuf + srcSize.width;
        for ( int i = 0 ; i < inInfo->Height /2; i++)
        {
            sts = ippiCopy_8u_C1R(inData->UV + 2 + i*inData->Pitch, 4, ptr + i*inData->Pitch, roi.width, roi);
            if( ippStsNoErr != sts ) return sts;
        }

        roi.width  = srcSize.width;
        roi.height = srcSize.height/2;

        sts = ippiCopy_16u_C1R((mfxU16 *)pWorkBuf, inData->Pitch, (mfxU16*)interPtr, inData->Pitch, roi);
        if( ippStsNoErr != sts ) return sts;

        srcRect.height = srcSize.height >>= 1;
        dstRect.height = dstSize.height >>= 1;

        srcRect.width = srcSize.width >>= 1;
        dstRect.width = dstSize.width >>= 1;

        /* Resize U */
        sts = ippiResizeSqrPixel_16u_C1R((const Ipp16u *) interPtr, srcSize, inData->Pitch, srcRect,
                                               (Ipp16u *) outData->UV, outData->Pitch , dstRect,
                                              xFactor, yFactor, 0.0, 0.0, interpolation, pWorkBuf);
        if( ippStsNoErr != sts ) return sts;

        /* Clip U */
        sts = ippiThreshold_16u_C1IR((Ipp16u*)outData->UV, outData->Pitch, dstSize, 0x03ff, ippCmpGreater);
        if( ippStsNoErr != sts ) return sts;

        /* Resize V */
        sts = ippiResizeSqrPixel_16u_C1R((const Ipp16u *)(&interPtr[srcSize.width*2]), srcSize, inData->Pitch, srcRect,
                                         (Ipp16u *)(&outData->UV[dstSize.width*2]), outData->Pitch, dstRect,
                                              xFactor, yFactor, 0.0, 0.0, interpolation, pWorkBuf);
        if( ippStsNoErr != sts ) return sts;

        /* Clip V */
        sts = ippiThreshold_16u_C1IR((Ipp16u *)(&outData->UV[dstSize.width*2]), outData->Pitch, dstSize, 0x03ff, ippCmpGreater);
        if( ippStsNoErr != sts ) return sts;

        /* Convert back to NV12 layout
         */

        srcRect.height = srcSize.height <<= 1;
        dstRect.height = dstSize.height <<= 1;

        srcRect.width = srcSize.width <<= 1;
        dstRect.width = dstSize.width <<= 1;

        roi.width  = 2;
        roi.height = dstSize.width / 2;
        for ( int i = 0 ; i < outInfo->Height /2; i++)
        {
            sts = ippiCopy_8u_C1R(outData->UV + i*outData->Pitch, roi.width, pWorkBuf + i*outData->Pitch, 4, roi);
            if( ippStsNoErr != sts ) return sts;
        }

        ptr = outData->UV + dstSize.width;

        for(int i = 0; i < outInfo->Height/2; i++)
        {
            sts = ippiCopy_8u_C1R(ptr + i*outData->Pitch, roi.width, pWorkBuf + 2 + i*outData->Pitch, 4, roi);
            if( ippStsNoErr != sts ) return sts;
        }

        roi.width  = dstSize.width;
        roi.height = ( dstSize.height / 2 );
        sts = ippiCopy_16u_C1R((mfxU16 *)pWorkBuf, outData->Pitch, (mfxU16 *)outData->UV, outData->Pitch, roi);
        if( ippStsNoErr != sts ) return sts;
    }
    else //interlaced video
    {
        /* Not supported... yet */
        return ippStsErr;
    }

    return sts;
}

IppStatus rs_RGB32( mfxFrameSurface1* in, mfxFrameSurface1* out, mfxU8* pWorkBuf, mfxU16 picStruct )
{
  picStruct;

  IppStatus sts     = ippStsNotSupportedModeErr;
  IppiSize  srcSize = {0,  0};
  IppiSize  dstSize = {0, 0};
  mfxF64    xFactor = 0.0, yFactor = 0.0;
  mfxF64    xShift = 0.0, yShift = 0.0;
  IppiRect  srcRect, dstRect;

  mfxU16 cropX = 0, cropY = 0;

  mfxU32 inOffset0  = 0;
  mfxU32 outOffset0 = 0;

  /* [IN] */
  mfxFrameData* inData = &in->Data;
  mfxFrameInfo* inInfo = &in->Info;

  IPP_CHECK_NULL_PTR1(inData->R);
  IPP_CHECK_NULL_PTR1(inData->G);
  IPP_CHECK_NULL_PTR1(inData->B);
  // alpha channel is ignore due to d3d issue

  VPP_GET_CROPX(inInfo, cropX);
  VPP_GET_CROPY(inInfo, cropY);
  inOffset0  = cropX*4  + cropY*inData->Pitch;

  mfxU8* ptrStart = IPP_MIN( IPP_MIN(inData->R, inData->G), inData->B );
  const mfxU8* pSrc[1] = {ptrStart + inOffset0};
  mfxI32 pSrcStep[1] = {inData->Pitch};

  /* OUT */
  mfxFrameData* outData= &out->Data;
  mfxFrameInfo* outInfo= &out->Info;

  VPP_GET_CROPX(outInfo, cropX);
  VPP_GET_CROPY(outInfo, cropY);
  outOffset0  = cropX*4  + cropY*outData->Pitch;

  ptrStart = IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );

  mfxU8* pDst[1]   = {ptrStart + outOffset0};

  mfxI32 pDstStep[1] = {outData->Pitch};

  /* common part */
  VPP_GET_WIDTH(inInfo,  srcSize.width);
  VPP_GET_HEIGHT(inInfo, srcSize.height);

  VPP_GET_WIDTH(outInfo,  dstSize.width);
  VPP_GET_HEIGHT(outInfo, dstSize.height);

  /* tune part */
  mfxI32 interpolation;
  xFactor = (mfxF64)dstSize.width  / (mfxF64)srcSize.width;
  yFactor = (mfxF64)dstSize.height / (mfxF64)srcSize.height;

  if( xFactor < 1.0 && yFactor < 1.0 )
  {
      interpolation = IPPI_INTER_SUPER;
  }
  else
  {
      interpolation = IPPI_INTER_LANCZOS;
  }

  //if( MFX_PICSTRUCT_PROGRESSIVE & picStruct)
  {
      srcRect.x = 0;
      srcRect.y = 0;
      srcRect.height = srcSize.height;
      srcRect.width  = srcSize.width;

      dstRect.x = 0;
      dstRect.y = 0;
      dstRect.height = dstSize.height;
      dstRect.width  = dstSize.width;

      xShift = 0.0, yShift = 0.0;

      sts = ippiResizeSqrPixel_8u_C4R(
          pSrc[0], srcSize, pSrcStep[0], srcRect,
          pDst[0], pDstStep[0], dstRect,
          xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

      sts = ippStsNoErr;
  }
  //else //interlaced video
  //{
  //  IppiSize  srcSizeIntr = {0, 0};
  //  IppiSize  dstSizeIntr = {0, 0};
  //  mfxI32    srcStepIntr = 0;
  //  mfxI32    dstStepIntr = 0;

  //  srcRect.x = 0;
  //  srcRect.y = 0;
  //  srcRect.height = srcSize.height >> 1;
  //  srcRect.width  = srcSize.width;

  //  srcSizeIntr.width = srcRect.width;
  //  srcSizeIntr.height= srcRect.height;

  //  srcStepIntr = pSrcStep[0] << 1;

  //  dstRect.x = 0;
  //  dstRect.y = 0;
  //  dstRect.height = dstSize.height >> 1;
  //  dstRect.width  = dstSize.width;

  //  dstSizeIntr.width = dstRect.width;
  //  dstSizeIntr.height= dstRect.height;

  //  dstStepIntr = pDstStep[0] << 1;

  //  /* top field */
  //  sts = ippiResizeSqrPixel_8u_C4R(pSrc[0], srcSizeIntr, srcStepIntr, srcRect,
  //                                  pDst[0], dstStepIntr, dstRect,
  //                                  xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);

  //  if( ippStsNoErr != sts ) return sts;

  //  /* bottom field */
  //  sts = ippiResizeSqrPixel_8u_C1R(pSrc[0]+pSrcStep[0], srcSizeIntr, srcStepIntr, srcRect,
  //                                  pDst[0]+pDstStep[0], dstStepIntr, dstRect,
  //                                  xFactor, yFactor, xShift, yShift, interpolation, pWorkBuf);
  //}

  return sts;

} // IppStatus rs_RGB32( mfxFrameSurface1* in, mfxFrameSurface1* out, mfxU8* pWorkBuf, mfxU16 picStruct )

/*  */
mfxStatus owniResizeGetBufSize_UpEstimation( IppiSize srcSize, IppiSize dstSize, int *pBufferSize )
{
    Ipp64u maxWidth, maxHeight, bufSize;

    maxWidth = (Ipp64u)IPP_MAX( srcSize.width, dstSize.width );
    maxHeight= (Ipp64u)IPP_MAX( srcSize.height, dstSize.height );

    bufSize = 2 * maxWidth  * (maxWidth+2)  * sizeof(int) +
              3 * maxHeight * (maxHeight+2) * sizeof(int) +
              maxHeight * sizeof(float*) +
              2 * maxWidth * maxHeight * sizeof(float);

    if(bufSize > INT_MAX)
    {
        *pBufferSize = 0;
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    *pBufferSize = (int)bufSize;

    return MFX_ERR_NONE;
}

#endif // MFX_ENABLE_VPP
/* EOF */
