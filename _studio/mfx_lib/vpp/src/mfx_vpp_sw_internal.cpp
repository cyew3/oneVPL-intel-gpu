//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2016 Intel Corporation. All Rights Reserved.
//

#include "math.h"
#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#include "mfx_enc_common.h"
#include "mfx_session.h"

#include "mfx_vpp_hw.h"

#if defined (MFX_VA)
#include "mfxpcp.h"
#endif

#include "mfx_vpp_sw.h"
#include "mfx_vpp_utils.h"

// internal filters
#include "mfx_denoise_vpp.h"
#include "mfx_color_space_conversion_vpp.h"
#include "mfx_resize_vpp.h"
#include "mfx_shift_vpp.h"
#include "mfx_range_map_vc1_vpp.h"
#include "mfx_deinterlace_vpp.h"
#include "mfx_video_analysis_vpp.h"
#include "mfx_frame_rate_conversion_vpp.h"
#include "mfx_procamp_vpp.h"
#include "mfx_detail_enhancement_vpp.h"

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
#include "mfx_gamut_compression_vpp.h"
#include "mfx_image_stabilization_vpp.h"
#include "mfx_video_signal_conversion_vpp.h"
#include "mfx_vpp_service.h"

/* IPP */
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"


#define VPP_INIT_FILTER( filterIndex, ConstructorVPP )  \
{                                                       \
  if ( !m_ppFilters[filterIndex] )                        \
  {                                                     \
    m_ppFilters[filterIndex] = new ConstructorVPP(m_core,&sts);   \
                                                                \
    sts = m_ppFilters[filterIndex]->Init( &(inFrameInfo), &(outFrameInfo) );\
    MFX_CHECK_STS( sts );                                                 \
  }                                                                       \
}

/* ******************************************************************** */
/*      implementation of SW VPP internal architecture                  */
/* ******************************************************************** */

mfxStatus VideoVPP_SW::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 filterIndex, filterIndexStart;
    mfxU32 filterBreakIndex = 0;
    bool   isPipelineBreak  = false;

    /* ***************************************************************** */
    /*              simulation of processing pipeline                    */
    /* ***************************************************************** */

    filterIndexStart = GetFilterIndexStart( MFX_REQUEST_FROM_VPP_CHECK );

    mfxFrameSurface1* inputSurf = in;
    mfxFrameSurface1* outputSurf= (filterIndexStart > 0 ) ? NULL : in;
    bool isMultipleOutput = false;

    for( filterIndex = filterIndexStart; filterIndex < GetNumUsedFilters(); filterIndex++ )
    {
        inputSurf = outputSurf;

        if( filterIndex >= GetNumConnections() )
        {
            outputSurf = out;
        }
        else
        {
            sts = m_connectionFramesPool[filterIndex].GetFreeSurface( &outputSurf );
            MFX_CHECK_STS( sts );
        }

        sts = m_ppFilters[filterIndex]->CheckProduceOutput(inputSurf, outputSurf);

        if( MFX_ERR_MORE_SURFACE == sts )// start from ISV2_Beta it mean OK but there are > 1 outputs
        {
            isMultipleOutput = true;
            sts = MFX_ERR_NONE;
        }

        if( MFX_ERR_MORE_DATA == sts )
        {
            if( !isPipelineBreak )
            {
                isPipelineBreak = true;
                filterBreakIndex = filterIndex;
            }

            /* downsampleFRC breaks pipeline in case of MFX_ERR_MORE_DATA
              it is reason to stop processing given moment */
            if( MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION == m_pipelineList[filterIndex] )
            {
                break;
            }
        }

        /* mechanism to produce buffered frames */
        if( MFX_ERR_MORE_DATA == sts || (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == sts)
        {
            outputSurf = NULL;
        }
        else
        {
            MFX_CHECK_STS( sts );
        }
    }

    /* ***************************************************************** */
    /*              make decision of processing pipeline                 */
    /* ***************************************************************** */

    /* if there are filters before pipeline is broken we should run VPP */
    if( !(MFX_ERR_NONE == sts || (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == sts) )
    {
        if( isPipelineBreak && filterBreakIndex > 0 )
        {
            sts = (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK;
        }
        else if( isPipelineBreak && (NULL != in) ) // DI or FRC broke pipeline
        {
            sts = (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK;
        }
    }

    if(MFX_ERR_NONE == sts && isMultipleOutput)
    {
        sts = MFX_ERR_MORE_SURFACE;
    }

    return sts;

} // mfxStatus VideoVPPBase::CheckProduceOutput(...)

 bool VideoVPP_SW::IsReadyOutput( mfxRequestType requestType )
{
    mfxU32 filterIndex;

    for( filterIndex = 0; filterIndex < GetNumUsedFilters(); filterIndex++ )
    {
        if( m_ppFilters[filterIndex]->IsReadyOutput( requestType ) )
        {
            return true;
        }
    }

    return false;

} //  bool VideoVPPBase::IsReadyOutput( mfxRequestType requestType )

 mfxU32  VideoVPP_SW::GetFilterIndexStart( mfxRequestType requestType )
 {
     mfxU32 filterIndex;

     for( filterIndex = 0; filterIndex < GetNumUsedFilters(); filterIndex++ )
     {
         if( m_ppFilters[filterIndex]->IsReadyOutput( requestType ) )
         {
             return filterIndex;
         }
     }

     return 0;

} //  mfxU32  VideoVPPBase::GetFilterIndexStart( mfxRequestType requestType )
 
mfxStatus VideoVPP_SW::SetCrop(mfxFrameSurface1 *in, mfxFrameSurface1 *out)
{
  mfxStatus mfxSts = MFX_ERR_NONE;
  mfxU32    connectIndex = 0;

  if( in )
  {
      mfxSts = CheckCropParam( &(in->Info) );
      MFX_CHECK_STS( mfxSts );
  }

  mfxSts = CheckCropParam( &(out->Info) );
  MFX_CHECK_STS( mfxSts );

  // save cropX/Y/W/H
  if( in )
  {
      m_errPrtctState.In.CropH = in->Info.CropH;
      m_errPrtctState.In.CropW = in->Info.CropW;
      m_errPrtctState.In.CropX = in->Info.CropX;
      m_errPrtctState.In.CropY = in->Info.CropY;
  }

  m_errPrtctState.Out.CropH = out->Info.CropH;
  m_errPrtctState.Out.CropW = out->Info.CropW;
  m_errPrtctState.Out.CropX = out->Info.CropX;
  m_errPrtctState.Out.CropY = out->Info.CropY;

  mfxFrameInfo cropInfo = m_errPrtctState.In;

  for( connectIndex = 0; connectIndex < GetNumConnections(); connectIndex++ )
  {
    mfxU32       filterIndex = connectIndex;

    switch( m_pipelineList[filterIndex] )
    {
      case MFX_EXTBUFF_VPP_RESIZE:
        {
            cropInfo = m_errPrtctState.Out;
            break;
        }

      default:
        {
            /* DI/DN/SA/FRC/CSC specific: Pass Through for crop: in_crop -> out_crop */
            break;
        }

    }// CASE

    mfxSts = m_connectionFramesPool[connectIndex].SetCrop( &(cropInfo) );
    MFX_CHECK_STS( mfxSts );

  }//end for( connectIndex = 0;

  return mfxSts;

} // mfxStatus VideoVPPBase::SetCrop(mfxFrameSurface1 *in, mfxFrameSurface1 *out)

mfxStatus VideoVPP_SW::CreatePipeline(mfxFrameInfo* In, mfxFrameInfo* Out)
{
    In; Out;
#if !defined(MFX_ENABLE_HW_ONLY_VPP)
    mfxFrameInfo inFrameInfo;
    mfxFrameInfo outFrameInfo;
    mfxU32       filterIndex;
    mfxStatus    sts;

    outFrameInfo = *In;
    for( filterIndex = 0; filterIndex < GetNumUsedFilters(); filterIndex++ )
    {
        inFrameInfo = outFrameInfo;

        switch( m_pipelineList[filterIndex] )
        {
            case (mfxU32)MFX_EXTBUFF_VPP_DENOISE:
            {
                sts = MFX_ERR_NONE;
                // DENOISE specific - nothing
                VPP_INIT_FILTER( filterIndex, MFXVideoVPPDenoise );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_RESIZE:
            {
                sts = MFX_ERR_NONE;
                /* RESIZE specific */
                outFrameInfo.Height = Out->Height;
                outFrameInfo.Width  = Out->Width;

                VPP_INIT_FILTER( filterIndex, MFXVideoVPPResize );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
            {
                sts = MFX_ERR_NONE;
                /* FRC specific */
                outFrameInfo.FrameRateExtD = Out->FrameRateExtD;
                outFrameInfo.FrameRateExtN = Out->FrameRateExtN;

                VPP_INIT_FILTER( filterIndex, MFXVideoVPPFrameRateConversion );
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DI_30i60p:
            case (mfxU32)MFX_EXTBUFF_VPP_ITC:
            {
                outFrameInfo.FrameRateExtD = Out->FrameRateExtD;
                outFrameInfo.FrameRateExtN = Out->FrameRateExtN;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DI:
            case (mfxU32)MFX_EXTBUFF_VPP_DEINTERLACING:
            {
                sts = MFX_ERR_NONE;
                /* DEINTERLACE specific */
                outFrameInfo.PicStruct = Out->PicStruct;

                VPP_INIT_FILTER( filterIndex, MFXVideoVPPDeinterlace );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_RSHIFT_IN:
            {
                sts = MFX_ERR_NONE;

                inFrameInfo.FourCC = In->FourCC;
                inFrameInfo.Shift  = In->Shift;

                outFrameInfo.FourCC = In->FourCC;
                outFrameInfo.Shift  = 0;

                VPP_INIT_FILTER( filterIndex, MFXVideoVPPShift );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_LSHIFT_OUT:
            {
                sts = MFX_ERR_NONE;
                inFrameInfo.FourCC = Out->FourCC;
                inFrameInfo.Shift  = 0;

                outFrameInfo.FourCC = Out->FourCC;
                outFrameInfo.Shift  = Out->Shift;

                VPP_INIT_FILTER( filterIndex, MFXVideoVPPShift );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_CSC:
            {
                sts = MFX_ERR_NONE;
                /* COLOR SPACE_ CONVERSION specific */
                outFrameInfo.FourCC = MFX_FOURCC_NV12;

                if ( Out->FourCC == MFX_FOURCC_P010 ||
                     Out->FourCC == MFX_FOURCC_P210 ||
                     Out->FourCC == MFX_FOURCC_NV16 ||
                     Out->FourCC == MFX_FOURCC_A2RGB10)
                {
                    outFrameInfo.FourCC = Out->FourCC;
                    outFrameInfo.Shift  = Out->Shift;
                }
                VPP_INIT_FILTER( filterIndex, MFXVideoVPPColorSpaceConversion );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_CSC_OUT_RGB4:
            {
                sts = MFX_ERR_NONE;
                /* COLOR SPACE_ CONVERSION specific */
                outFrameInfo.FourCC = MFX_FOURCC_RGB4;

                VPP_INIT_FILTER( filterIndex, MFXVideoVPPColorSpaceConversion );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10:
            {
                sts = MFX_ERR_NONE;
                /* COLOR SPACE_ CONVERSION specific */
                outFrameInfo.FourCC = MFX_FOURCC_A2RGB10;

                VPP_INIT_FILTER( filterIndex, MFXVideoVPPColorSpaceConversion );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO:
            {
                sts = MFX_ERR_NONE;
                /* VIDEO SIGNAL CONVERSION specific */

                VPP_INIT_FILTER( filterIndex, MFXVideoVPPVideoSignalConversion ); /* RunFrameVPP in not implemented yet. Init for behavior tests only */

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_SCENE_ANALYSIS:
            {
                sts = MFX_ERR_NONE;
                // SCENE ANALYSIS specific - nothing
                VPP_INIT_FILTER( filterIndex, MFXVideoVPPVideoAnalysis );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_PROCAMP:
            {
                sts = MFX_ERR_NONE;
                // PROCAMP specific - nothing
                VPP_INIT_FILTER( filterIndex, MFXVideoVPPProcAmp );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DETAIL:
            {
                sts = MFX_ERR_NONE;
                // Detail specific - nothing
                VPP_INIT_FILTER( filterIndex, MFXVideoVPPDetailEnhancement );

                break;
            }

            //case (mfxU32)MFX_EXTBUFF_VPP_GAMUT_MAPPING:
            //{
            //    sts = MFX_ERR_NONE;
            //    // Detail specific - nothing
            //    VPP_INIT_FILTER( filterIndex, MFXVideoVPPGamutCompression );

            //    break;
            //}

#if defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)
            case (mfxU32)MFX_EXTBUFF_VPP_IMAGE_STABILIZATION:
            {
                sts = MFX_ERR_NONE;
                // ImgStab specific - nothing
                VPP_INIT_FILTER( filterIndex, MFXVideoVPPImgStab );

                break;
            }
#endif

#ifdef MFX_UNDOCUMENTED_VPP_VARIANCE_REPORT
            case (mfxU32)MFX_EXTBUFF_VPP_VARIANCE_REPORT://fake only!!!
            {
                sts = MFX_ERR_NONE;
                // specific - nothing
                VPP_INIT_FILTER( filterIndex, MFXVideoVPPProcAmp );

                break;
            }
#endif
            default:
            {
                sts = MFX_ERR_INVALID_VIDEO_PARAM;
                MFX_CHECK_STS( sts );
            }

        }// CASE
    }//end of creation files
#endif
    return MFX_ERR_NONE;

} // mfxStatus VideoVPPBase::CreatePipeline(mfxFrameInfo* In, mfxFrameInfo* Out)

mfxStatus VideoVPP_SW::DestroyPipeline( void )
{
    mfxU32 filterIndex;

    for( filterIndex = 0; filterIndex < GetNumUsedFilters(); filterIndex++ )
    {
        if( m_ppFilters[filterIndex] )
        {
            delete m_ppFilters[filterIndex];
            m_ppFilters[filterIndex] = NULL;
        }
        //m_pipelineList[filterIndex] = 0;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPBase::DestroyPipeline( void )

mfxStatus VideoVPP_SW::CreateWorkBuffer( void )
{
    mfxU32 filterIndex;
    mfxU32 totalBufSize = 0, bufSize;
    IppStatus ippSts;
    mfxStatus sts = MFX_ERR_NONE;
    mfxU8*    pWorkBuffer = NULL;


    for( filterIndex = 0; filterIndex < GetNumUsedFilters(); filterIndex++ )
    {
        sts = m_ppFilters[filterIndex]->GetBufferSize( &bufSize );
        MFX_CHECK_STS( sts );

        totalBufSize += bufSize;
    }

    /* we don't distinction btw re-usable work buffer & buffer for tables */
    if( totalBufSize )
    {
        sts = m_core->AllocBuffer( totalBufSize, MFX_MEMTYPE_SYSTEM_MEMORY, &m_memIdWorkBuf );
        MFX_CHECK_STS( sts );

        sts = m_core->LockBuffer(m_memIdWorkBuf, &pWorkBuffer );
        MFX_CHECK_STS( sts );

        ippSts = ippsZero_8u( pWorkBuffer, totalBufSize);
        VPP_CHECK_IPP_STS(ippSts);

        for( filterIndex = 0; filterIndex < GetNumUsedFilters(); filterIndex++ )
        {
            sts = m_ppFilters[filterIndex]->GetBufferSize( &bufSize );
            MFX_CHECK_STS( sts );

            sts = m_ppFilters[filterIndex]->SetBuffer( pWorkBuffer );
            MFX_CHECK_STS( sts );

            pWorkBuffer += bufSize;
        }
    }

    return sts;

} // mfxStatus VideoVPPBase::CreateWorkBuffer( void )

mfxStatus VideoVPP_SW::DestroyWorkBuffer( void )
{
    mfxStatus sts = MFX_ERR_NONE;

    if( m_memIdWorkBuf )
    {
        sts = m_core->UnlockBuffer( m_memIdWorkBuf );
        sts = m_core->FreeBuffer( m_memIdWorkBuf );
        m_memIdWorkBuf = NULL;
    }

    return sts;

} // mfxStatus VideoVPPBase::DestroyWorkBuffer( void )

mfxU32 VideoVPP_SW::GetNumConnections()
{
  return (mfxU32)(m_pipelineList.size() - 1);

} // mfxU32 VideoVPPBase::GetNumConnections()


mfxStatus VideoVPP_SW::CreateInternalSystemFramesPool( mfxVideoParam *par )
{
  mfxFrameAllocRequest  request[2];
  mfxStatus sts;

  MFX_CHECK_NULL_PTR1( par );

  sts = QueryIOSurf(m_core, par, request);
  VPP_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
  MFX_CHECK_STS( sts );

  if( m_bOpaqMode[VPP_IN] )
  {
      request[VPP_IN] = m_requestOpaq[VPP_IN];
  }
  if( m_bOpaqMode[VPP_OUT] )
  {
      request[VPP_OUT] = m_requestOpaq[VPP_OUT];
  }

  if( request[VPP_IN].Type & (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET|MFX_MEMTYPE_DXVA2_DECODER_TARGET) )
  {
      /* internal sw surfaces */
      request[VPP_IN].Type = MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_VPPIN;

      sts = m_internalSystemFramesPool[VPP_IN].Init(m_core, &request[VPP_IN], true);
      MFX_CHECK_STS( sts );

      m_bDoFastCopyFlag[VPP_IN] = true;
  }
  else
  {
      sts = m_externalFramesPool[VPP_IN].Init(m_core, &request[VPP_IN], false);
      MFX_CHECK_STS( sts );
  }

  if( request[VPP_OUT].Type & (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET|MFX_MEMTYPE_DXVA2_DECODER_TARGET) )
  {
      /* internal sw surfaces */
      request[VPP_OUT].Type = MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_VPPOUT;

      sts = m_internalSystemFramesPool[VPP_OUT].Init(m_core, &request[VPP_OUT], true);
      MFX_CHECK_STS( sts );

      m_bDoFastCopyFlag[VPP_OUT] = true;
  }
  else
  {
      sts = m_externalFramesPool[VPP_OUT].Init(m_core, &request[VPP_OUT], false);
      MFX_CHECK_STS( sts );
  }

  return MFX_ERR_NONE;

} // mfxStatus VideoVPPBase::CreateInternalSystemFramesPool( mfxVideoParam *par )

mfxStatus VideoVPP_SW::DestroyInternalSystemFramesPool( void )
{
  if( m_bDoFastCopyFlag[VPP_IN] )
  {
    m_internalSystemFramesPool[VPP_IN].Close();
    m_bDoFastCopyFlag[VPP_IN] = false;
  }
  else
  {
    m_externalFramesPool[VPP_IN].Close();
  }

  if( m_bDoFastCopyFlag[VPP_OUT] )
  {
    m_internalSystemFramesPool[VPP_OUT].Close();
    m_bDoFastCopyFlag[VPP_OUT] = false;
  }
  else
  {
    m_externalFramesPool[VPP_OUT].Close();
  }

  return MFX_ERR_NONE;

} // mfxStatus VideoVPPBase::DestroyInternalSystemFramesPool( void )


mfxStatus VideoVPP_SW::PreProcessOfInputSurface(mfxFrameSurface1 *in, mfxFrameSurface1** ppOut)
{
  mfxStatus sts;

  if( NULL == in )
  {
    *ppOut         = NULL;
    return MFX_ERR_NONE;
  }

  // normal process start here

  if( !m_bDoFastCopyFlag[VPP_IN] )
  {
    sts = m_externalFramesPool[VPP_IN].GetFilledSurface(in, ppOut);
    MFX_CHECK_STS( sts );
  }
  else
  {
    sts = m_internalSystemFramesPool[VPP_IN].GetFreeSurface(ppOut);
    MFX_CHECK_STS( sts );

    sts = m_core->IncreaseReference(&(*ppOut)->Data);
    MFX_CHECK_STS( sts );

    sts = m_core->DoFastCopyWrapper(*ppOut,
                                    MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                    in,
                                    MFX_MEMTYPE_EXTERNAL_FRAME | (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_DXVA2_DECODER_TARGET));

    MFX_CHECK_STS( sts );

    (*ppOut)->Info = in->Info;

    sts = m_core->DecreaseReference(&in->Data);
    MFX_CHECK_STS(sts);
  }

  return MFX_ERR_NONE;

} // mfxStatus VideoVPPBase::PreProcessOfInputSurface(...)


mfxStatus VideoVPP_SW::PreProcessOfOutputSurface(mfxFrameSurface1 *out, mfxFrameSurface1** ppOut)
{
  mfxStatus sts;

  if( NULL == out )
  {
      *ppOut = NULL;
      return MFX_ERR_NONE;
  }

  // normal process start here

  if( !m_bDoFastCopyFlag[VPP_OUT] )
  {
      sts = m_externalFramesPool[VPP_OUT].GetFilledSurface(out, ppOut);
      MFX_CHECK_STS( sts );
  }
  else
  {
    sts = m_internalSystemFramesPool[VPP_OUT].Lock();
    MFX_CHECK_STS( sts );

    sts = m_internalSystemFramesPool[VPP_OUT].GetFreeSurface( ppOut );
    MFX_CHECK_STS( sts );

    (*ppOut)->Info = out->Info;
    sts = m_core->IncreaseReference(&((*ppOut)->Data));
  }

  return MFX_ERR_NONE;

} // mfxStatus VideoVPPBase::PreProcessOfOutputSurface(...)


mfxStatus VideoVPP_SW::PostProcessOfOutputSurface(mfxFrameSurface1 *out, mfxFrameSurface1 *pOutSurf, mfxStatus processingSts)
{
    mfxStatus sts = MFX_ERR_NONE;

    if( m_bDoFastCopyFlag[VPP_OUT] )
    {
        if( MFX_ERR_NONE == processingSts )
        {
            sts = m_core->DoFastCopyWrapper(out,
                                            MFX_MEMTYPE_EXTERNAL_FRAME | (MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_DXVA2_DECODER_TARGET),
                                            pOutSurf,
                                            MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY);
            MFX_CHECK_STS( sts );

        }

        sts = m_internalSystemFramesPool[VPP_OUT].Unlock();
        MFX_CHECK_STS( sts );
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVPPBase::PostProcessOfOutputSurface(


mfxStatus VideoVPP_SW::CreateConnectionFramesPool( void )
{
  mfxStatus  sts;
  mfxU32     connectIndex, filterIndex;
  mfxFrameInfo frameInfo;
  mfxFrameAllocRequest  request;
  mfxU16     framesCount;

  for( connectIndex = 0; connectIndex < GetNumConnections(); connectIndex++)
  {
    framesCount = m_ppFilters[connectIndex]->GetOutFramesCount() + m_ppFilters[connectIndex+1]->GetInFramesCount() - 1;

    filterIndex = connectIndex;
    sts         = m_ppFilters[filterIndex]->GetFrameInfo(NULL, &frameInfo);// out info interested only
    MFX_CHECK_STS(sts);

    request.Info        = frameInfo;
    request.NumFrameMin = request.NumFrameSuggested = framesCount;
    request.Type        = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_VPPIN;

    sts = m_connectionFramesPool[connectIndex].Init(m_core, &request, true);
    MFX_CHECK_STS( sts );
  }

  return MFX_ERR_NONE;

} // mfxStatus VideoVPPBase::CreateConnectionFramesPool( void )


mfxStatus VideoVPP_SW::DestroyConnectionFramesPool( void )
{
  mfxU32 connectIndex;

  for( connectIndex = 0; connectIndex < GetNumConnections(); connectIndex++ )
  {
     m_connectionFramesPool[connectIndex].Close();
  }

  return MFX_ERR_NONE;

} // mfxStatus VideoVPPBase::DestroyConnectionFramesPool( void )

#endif // #if !defined (MFX_ENABLE_HW_ONLY_VPP)

//-----------------------------------------------------------------------------
//            independent functions
//-----------------------------------------------------------------------------

// all check must be done before call
mfxStatus GetExternalFramesCount(mfxVideoParam* pParam,
                                 mfxU32* pListID,
                                 mfxU32 len,
                                 mfxU16 framesCountMin[2],
                                 mfxU16 framesCountSuggested[2])
{
    mfxU32 filterIndex;
    mfxU16 inputFramesCount[MAX_NUM_VPP_FILTERS]  = {0};
    mfxU16 outputFramesCount[MAX_NUM_VPP_FILTERS] = {0};

    for( filterIndex = 0; filterIndex < len; filterIndex++ )
    {
        switch( pListID[filterIndex] )
        {

            case (mfxU32)MFX_EXTBUFF_VPP_RSHIFT_IN:
            case (mfxU32)MFX_EXTBUFF_VPP_RSHIFT_OUT:
            case (mfxU32)MFX_EXTBUFF_VPP_LSHIFT_IN:
            case (mfxU32)MFX_EXTBUFF_VPP_LSHIFT_OUT:
            {
                inputFramesCount[filterIndex]  = MFXVideoVPPShift::GetInFramesCountExt();
                outputFramesCount[filterIndex] = MFXVideoVPPShift::GetOutFramesCountExt();
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DENOISE:
            {
                inputFramesCount[filterIndex]  = MFXVideoVPPDenoise::GetInFramesCountExt();
                outputFramesCount[filterIndex] = MFXVideoVPPDenoise::GetOutFramesCountExt();
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_RESIZE:
            {
                inputFramesCount[filterIndex]  = MFXVideoVPPResize::GetInFramesCountExt();
                outputFramesCount[filterIndex] = MFXVideoVPPResize::GetOutFramesCountExt();
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_ROTATION:
            {
                inputFramesCount[filterIndex]  = 1;
                outputFramesCount[filterIndex] = 1;
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_SCALING:
            {
                break;
            }

#ifndef MFX_FUTURE_FEATURE_DISABLE
            case (mfxU32)MFX_EXTBUFF_VPP_COLOR_CONVERSION:
            {
                break;
            }
#endif

            case (mfxU32)MFX_EXTBUFF_VPP_MIRRORING:
            {
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DEINTERLACING:
            {
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_ITC:
            case (mfxU32)MFX_EXTBUFF_VPP_DI:
            {
                inputFramesCount[filterIndex]  = MFXVideoVPPDeinterlace::GetInFramesCountExt();
                outputFramesCount[filterIndex] = MFXVideoVPPDeinterlace::GetOutFramesCountExt();
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DI_30i60p:
            {
                inputFramesCount[filterIndex]  = MFXVideoVPPDeinterlace::GetInFramesCountExt();
                outputFramesCount[filterIndex] = MFXVideoVPPDeinterlace::GetOutFramesCountExt() << 1;
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DI_WEAVE:
            {
                inputFramesCount[filterIndex]  = MFXVideoVPPDeinterlace::GetInFramesCountExt() << 1;
                outputFramesCount[filterIndex] = MFXVideoVPPDeinterlace::GetOutFramesCountExt();
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_CSC:
            case (mfxU32)MFX_EXTBUFF_VPP_CSC_OUT_RGB4:
            case (mfxU32)MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10:
            case (mfxU32)MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO:
            {
                inputFramesCount[filterIndex]  = MFXVideoVPPColorSpaceConversion::GetInFramesCountExt();
                outputFramesCount[filterIndex] = MFXVideoVPPColorSpaceConversion::GetOutFramesCountExt();
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_SCENE_ANALYSIS:
            {
                inputFramesCount[filterIndex]  = MFXVideoVPPVideoAnalysis::GetInFramesCountExt();

                outputFramesCount[filterIndex] = MFXVideoVPPVideoAnalysis::GetOutFramesCountExt();
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
            {
                // call Get[In/Out]FramesCountExt not correct for external application
                // result must be based on in/out frame rates
                mfxFrameInfo info;

                info = pParam->vpp.In;
                mfxF64 inFrameRate  = CalculateUMCFramerate(info.FrameRateExtN, info.FrameRateExtD);
                if (fabs(inFrameRate-0) < 0.01)
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                info = pParam->vpp.Out;
                mfxF64 outFrameRate = CalculateUMCFramerate(info.FrameRateExtN, info.FrameRateExtD);

                outputFramesCount[ filterIndex ] = (mfxU16)(ceil(outFrameRate / inFrameRate));
                outputFramesCount[ filterIndex ] = IPP_MAX(outputFramesCount[ filterIndex ], 1);//robustness

                // numInFrames = inFrameRate / inFrameRate = 1;
                inputFramesCount[ filterIndex ] = 1;

                // after analysis for correct FRC processing we require following equations
                inputFramesCount[ filterIndex ]  = IPP_MAX( inputFramesCount[ filterIndex ],  MFXVideoVPPFrameRateConversion::GetInFramesCountExt() );
                outputFramesCount[ filterIndex ] = IPP_MAX( outputFramesCount[ filterIndex ], MFXVideoVPPFrameRateConversion::GetOutFramesCountExt() );

                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_PROCAMP:
            {
                inputFramesCount[filterIndex]  = MFXVideoVPPProcAmp::GetInFramesCountExt();
                outputFramesCount[filterIndex] = MFXVideoVPPProcAmp::GetOutFramesCountExt();
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_DETAIL:
            {
                inputFramesCount[filterIndex]  = MFXVideoVPPDetailEnhancement::GetInFramesCountExt();
                outputFramesCount[filterIndex] = MFXVideoVPPDetailEnhancement::GetOutFramesCountExt();
                break;
            }

#if defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)
            case (mfxU32)MFX_EXTBUFF_VPP_IMAGE_STABILIZATION:
            {
                // fake for SW compatibility
                inputFramesCount[filterIndex]  = MFXVideoVPPImgStab::GetInFramesCountExt();
                outputFramesCount[filterIndex] = MFXVideoVPPImgStab::GetOutFramesCountExt();
                break;
            }
#endif

#ifdef MFX_UNDOCUMENTED_VPP_VARIANCE_REPORT
            case (mfxU32)MFX_EXTBUFF_VPP_VARIANCE_REPORT:
            {
                // fake for SW compatibility
                inputFramesCount[filterIndex]  = 2;
                outputFramesCount[filterIndex] = 1;
                break;
            }
#endif

            case (mfxU32)MFX_EXTBUFF_VPP_COMPOSITE:
            {
                for (mfxU32 i = 0; i < pParam->NumExtParam; i++)
                {
                    if (pParam->ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_COMPOSITE)
                    {
                        mfxExtVPPComposite* extComp = (mfxExtVPPComposite*) pParam->ExtParam[i];
                        if (extComp->NumInputStream > MAX_NUM_OF_VPP_COMPOSITE_STREAMS)
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }
                        else
                        {
                            inputFramesCount[filterIndex] = extComp->NumInputStream;
                        }

                        for(mfxU32 j = 0; j < extComp->NumInputStream; j++)
                        {
                            if (pParam->vpp.Out.Width  < (extComp->InputStream[j].DstX + extComp->InputStream[j].DstW))
                                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
                            if (pParam->vpp.Out.Height < (extComp->InputStream[j].DstY + extComp->InputStream[j].DstH))
                                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
                        }
                    }
                } /*for (mfxU32 i = 0; i < pParam->NumExtParam; i++)*/

                /* for output always one */
                outputFramesCount[filterIndex] = 1;
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_FIELD_PROCESSING:
            {
                inputFramesCount[filterIndex]  = 1;
                outputFramesCount[filterIndex] = 1;
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_FIELD_WEAVING:
            {
                inputFramesCount[filterIndex]  = 2;
                outputFramesCount[filterIndex] = 1;
                break;
            }

            case (mfxU32)MFX_EXTBUFF_VPP_FIELD_SPLITTING:
            {
                inputFramesCount[filterIndex]  = 1;
                outputFramesCount[filterIndex] = 2;
                break;
            }

            default:
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }

        }// CASE
    }//end of

    framesCountSuggested[VPP_IN]  = vppMax_16u(inputFramesCount, len);
    framesCountSuggested[VPP_OUT] = vppMax_16u(outputFramesCount, len);

#if 0
    // arch of SW VPP is complicated.
    // there is some scenario, when minFrames not enough due to internal MFX_ERR_MORE_DATA_SUBMIT_TASK
    framesCountMin[VPP_IN]  = inputFramesCount[0];// input from first filter
    framesCountMin[VPP_OUT] = outputFramesCount[len-1];// output from last filter
#endif

    // so, SW min frames must be equal MAX(filter0, filter1, ..., filterN-1)
    framesCountMin[VPP_IN]  = framesCountSuggested[VPP_IN];
    framesCountMin[VPP_OUT] = framesCountSuggested[VPP_OUT];

    //robustness
    if( VPP_MAX_REQUIRED_FRAMES_COUNT < framesCountMin[VPP_IN] )
    {
        framesCountMin[VPP_IN] = VPP_MAX_REQUIRED_FRAMES_COUNT;
    }

    if( VPP_MAX_REQUIRED_FRAMES_COUNT < framesCountMin[VPP_OUT] )
    {
        framesCountMin[VPP_OUT] = VPP_MAX_REQUIRED_FRAMES_COUNT;
    }

    return MFX_ERR_NONE;

} // mfxStatus GetExternalFramesCount(...)

// all check must be done before call
mfxStatus GetCompositionEnabledStatus(mfxVideoParam* pParam )
{
    mfxU32 bufferIndex;

    if( pParam->ExtParam && pParam->NumExtParam > 0 )
    {
        for( bufferIndex = 0; bufferIndex < pParam->NumExtParam; bufferIndex++ )
        {
            mfxExtBuffer *pExtBuffer = pParam->ExtParam[bufferIndex];
            if (pExtBuffer->BufferId == (mfxU32)MFX_EXTBUFF_VPP_COMPOSITE)
                return MFX_ERR_NONE;
        }
    }

    /* default case */
    return MFX_ERR_NOT_FOUND;

} // mfxStatus GetCompositionEnabledStatus(mfxVideoParam* pParam,

mfxStatus ExtendedQuery(VideoCORE * core, mfxU32 filterName, mfxExtBuffer* pHint)
{
    mfxStatus sts;
    /* Lets find out VA type (Linux, Win or Android) and platform type */
    /* It can be different behaviour for Linux and IVB, Linux and HSW*/
    bool bLinuxAndIVB_HSW_BDW = false;
    if ( (NULL != core) &&
        (core->GetVAType() == MFX_HW_VAAPI) &&
        ( (core->GetHWType() == MFX_HW_IVB) || (core->GetHWType() == MFX_HW_HSW) ||
          (core->GetHWType() == MFX_HW_BDW)) )
    {
        bLinuxAndIVB_HSW_BDW = true;
    }

    if( MFX_EXTBUFF_VPP_DENOISE == filterName )
    {
        sts = MFXVideoVPPDenoise::Query( pHint );
    }
    else if( MFX_EXTBUFF_VPP_DETAIL == filterName )
    {
        sts = MFXVideoVPPDetailEnhancement::Query( pHint );
    }
    else if( MFX_EXTBUFF_VPP_PROCAMP == filterName )
    {
        sts = MFXVideoVPPProcAmp::Query( pHint );
    }
    else if( MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION == filterName )
    {
        sts = MFXVideoVPPFrameRateConversion::Query( pHint );
    }
#if defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)
    else if( MFX_EXTBUFF_VPP_IMAGE_STABILIZATION == filterName )
    {
        if (false == bLinuxAndIVB_HSW_BDW)
            sts = MFXVideoVPPImgStab::Query( pHint );
        else
        {
            // This filter is not supported in Linux
            sts = MFX_WRN_FILTER_SKIPPED;
        }
    }
#endif
#if 0
    else if( MFX_EXTBUFF_VPP_GAMUT_MAPPING == filterName )
    {
        sts = MFXVideoVPPGamutCompression::Query( pHint );
    }
#endif
    else if( MFX_EXTBUFF_VPP_SCENE_ANALYSIS == filterName )
    {
        sts = MFX_ERR_UNSUPPORTED;
    }
    else if( MFX_EXTBUFF_VPP_COMPOSITE == filterName )
    {
        if ((NULL != core) && (MFX_PLATFORM_SOFTWARE == core->GetPlatformType()))
        {
            sts = MFX_ERR_UNSUPPORTED;
        }
        else
        {
            sts = MFX_ERR_NONE;
        }
    }
    else if( MFX_EXTBUFF_VPP_FIELD_PROCESSING == filterName )
    {
#if defined(_WIN32) || defined(_WIN64)
        sts = MFX_ERR_UNSUPPORTED;
#else
        if ((NULL != core) && (MFX_PLATFORM_SOFTWARE == core->GetPlatformType()))
        {
            sts = MFX_ERR_UNSUPPORTED;
        }
        else
        {
            sts = MFX_ERR_NONE;
        }
#endif
    }
    else // ignore
    {
        sts = MFX_ERR_NONE;
    }

    return sts;

} // mmfxStatus ExtendedQuery(VideoCORE * core, mfxU32 filterName, mfxExtBuffer* pHint)

#endif // MFX_ENABLE_VPP
/* EOF */
