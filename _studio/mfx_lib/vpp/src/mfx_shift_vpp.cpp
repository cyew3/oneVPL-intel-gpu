//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2015 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP) && !defined(MFX_ENABLE_HW_ONLY_VPP)

#include "mfx_vpp_utils.h"
#include "mfx_shift_vpp.h"
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"

/* ******************************************************************** */
/*                 prototype of Resize algorithms                       */
/* ******************************************************************** */

IppStatus shift_P010( mfxFrameSurface1 *in, mfxFrameSurface1 *out, bool right);

IppStatus shift_P210( mfxFrameSurface1 *in, mfxFrameSurface1 *out, bool right);

/* ******************************************************************** */
/*                 implementation of VPP filter [Resize]                */
/* ******************************************************************** */

MFXVideoVPPShift::MFXVideoVPPShift(VideoCORE *core, mfxStatus* sts ) : FilterVPP()
{
  //MFX_CHECK_NULL_PTR1(core);

  m_core = core;

  VPP_CLEAN;

  *sts = MFX_ERR_NONE;

  return;
}

MFXVideoVPPShift::~MFXVideoVPPShift(void)
{
  Close();

  return;
}

mfxStatus MFXVideoVPPShift::SetParam( mfxExtBuffer* pHint )
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  if( pHint ){
    // mfxSts = something
  }

  return mfxSts;
}

mfxStatus MFXVideoVPPShift::Reset(mfxVideoParam *par)
{

  MFX_CHECK_NULL_PTR1( par );

  VPP_CHECK_NOT_INITIALIZED;

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

  VPP_RESET;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPShift::RunFrameVPP(mfxFrameSurface1 *in,
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

  bool right = m_errPrtctState.In.Shift ? true : false ;
  switch ( m_errPrtctState.In.FourCC )
  {
    case MFX_FOURCC_P010:
      ippSts = shift_P010(in, out, right);
      break;

    case MFX_FOURCC_P210:
      ippSts = shift_P210(in, out, right);
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

mfxStatus MFXVideoVPPShift::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
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

  if( In->FourCC != Out->FourCC ){
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* save init params to prevent core crash */
  m_errPrtctState.In  = *In;
  m_errPrtctState.Out = *Out;

  /* FourCC checking */
  switch ( m_errPrtctState.In.FourCC )
  {
    case MFX_FOURCC_P210:
    case MFX_FOURCC_P010:
      VPP_INIT_SUCCESSFUL;
      break;

    default:
      return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPShift::Close(void)
{

  VPP_CHECK_NOT_INITIALIZED;

  VPP_CLEAN;

  return MFX_ERR_NONE;
}


mfxStatus MFXVideoVPPShift::GetBufferSize( mfxU32* pBufferSize )
{

  VPP_CHECK_NOT_INITIALIZED;

  *pBufferSize = 0;

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPPShift::SetBuffer( mfxU8* )
{
  VPP_CHECK_NOT_INITIALIZED;

  return MFX_ERR_NONE;
}

// function is called from sync part of VPP only
mfxStatus MFXVideoVPPShift::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
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

bool MFXVideoVPPShift::IsReadyOutput( mfxRequestType )
{
    return false;
}

/* ******************************************************************** */
/*                 implementation of algorithms [Resize]                */
/* ******************************************************************** */

/* ************************************************************************* */
/* P010 not optimized resize  */
IppStatus shift_P010( mfxFrameSurface1* in, mfxFrameSurface1* out, bool right)
{
    IppStatus sts = ippStsNoErr;
    IppiSize  roiSize = {0, 0};
    Ipp32u    shift;
    mfxFrameInfo *inInfo  = &in->Info;
    mfxFrameData *inData  = &in->Data;
    mfxFrameData *outData = &out->Data;

    VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
    VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        shift = inInfo->BitDepthLuma ? 16 - inInfo->BitDepthLuma : 6;
        if ( right )
        {
            sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, shift, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
        }
        else
        {
            sts = ippiLShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, shift, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
        }
        IPP_CHECK_STS( sts );

        shift = inInfo->BitDepthChroma ? 16 - inInfo->BitDepthChroma : 6;
        roiSize.height >>= 1;
        if ( right )
        {
            sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch, shift, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
        }
        else
        {
            sts = ippiLShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch, shift, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
        }
        IPP_CHECK_STS( sts );
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;
}

IppStatus shift_P210( mfxFrameSurface1* in, mfxFrameSurface1* out, bool right)
{
    IppStatus sts = ippStsNoErr;
    IppiSize  roiSize = {0, 0};
    Ipp32u    shift;
    mfxFrameInfo *inInfo  = &in->Info;
    mfxFrameData *inData  = &in->Data;
    mfxFrameData *outData = &out->Data;

    VPP_GET_REAL_WIDTH(inInfo, roiSize.width);
    VPP_GET_REAL_HEIGHT(inInfo, roiSize.height);

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        shift = inInfo->BitDepthLuma ? 16 - inInfo->BitDepthLuma : 6;
        if ( right )
        {
            sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, shift, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
        }
        else
        {
            sts = ippiLShiftC_16u_C1R((const Ipp16u *)inData->Y, inData->Pitch, shift, (Ipp16u *)outData->Y, outData->Pitch, roiSize);
        }
        IPP_CHECK_STS( sts );

        shift = inInfo->BitDepthChroma ? 16 - inInfo->BitDepthChroma : 6;
        if ( right )
        {
            sts = ippiRShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch, shift, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
        }
        else
        {
            sts = ippiLShiftC_16u_C1R((const Ipp16u *)inData->UV, inData->Pitch, shift, (Ipp16u *)outData->UV, outData->Pitch, roiSize);
        }
        IPP_CHECK_STS( sts );
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;
}

#endif // MFX_ENABLE_VPP
/* EOF */
