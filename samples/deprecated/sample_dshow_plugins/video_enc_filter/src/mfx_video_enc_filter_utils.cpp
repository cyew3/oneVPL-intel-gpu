/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mfx_samples_config.h"

#include "mfx_video_enc_filter_utils.h"
#include "mfx_filter_guid.h"
#include "mfx_filter_defs.h"

mfxStatus InitMfxFrameSurface( mfxFrameSurface1* pSurface, mfxFrameInfo* pInfo, IMediaSample *pSample )
{
    //check input params
    MSDK_CHECK_POINTER(pSurface,   MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pInfo,      MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pSample,    MFX_ERR_NULL_PTR);

    //prepare pSurface->Info
    memset(&pSurface->Info, 0, sizeof(mfxFrameInfo));                               // zero memory

    pSurface->Info.FourCC           = pInfo->FourCC;                                // FourCC type
    pSurface->Info.Width            = pInfo->Width;                                 // width, should be multiplier of 16
    pSurface->Info.Height           = pInfo->Height;                                // height, should be multiplier of 16 or 32 (interlaced frame)
    pSurface->Info.PicStruct        = pInfo->PicStruct;                             // picture structure
    pSurface->Info.ChromaFormat     = MFX_CHROMAFORMAT_YUV420;                      // picture chroma format
    pSurface->Info.CropH            = pInfo->CropH;                                 // height crop
    pSurface->Info.CropW            = pInfo->CropW;                                 // width crop

    mfxU32 nSurfaceSize = pSample->GetActualDataLength();
    mfxU8 bitsPerSample = 0;

    //Set Pitch
    if(pInfo->FourCC == MFX_FOURCC_NV12)
    {
        pSurface->Data.Pitch            = (mfxU16)(nSurfaceSize / pSurface->Info.CropH / 3 * 2);  //factor = 1 for NV12 and YV12
        bitsPerSample = 12;
    }
    else if(pInfo->FourCC == MFX_FOURCC_RGB4)
    {
        pSurface->Data.Pitch            = (mfxU16)(nSurfaceSize / pSurface->Info.CropH);
        bitsPerSample = 32;
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUY2)
    {
        pSurface->Data.Pitch            = (mfxU16)(nSurfaceSize / pSurface->Info.CropH);
        bitsPerSample = 16;
    }

    // check that upstream filter has allocated the buffer of requested on connection size ()
    if (pSample->GetSize() < pInfo->Width * pInfo->Height * bitsPerSample / 8)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    return MFX_ERR_NONE;
}

mfxStatus CopyMFXToEncoderParams(IConfigureVideoEncoder::Params* pParams, mfxVideoParam* pmfxParams)
{
    //check input params
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pmfxParams, MFX_ERR_NULL_PTR);

    mfxInfoMFX* pMFXInfo  = &pmfxParams->mfx;

    pParams->level_idc                      = (IConfigureVideoEncoder::Params::Level)
                                              pMFXInfo->CodecLevel;
    pParams->profile_idc                    = (IConfigureVideoEncoder::Params::Profile)
                                              pMFXInfo->CodecProfile;
    pParams->pc_control                     = (IConfigureVideoEncoder::Params::PCControl)
                                              pMFXInfo->FrameInfo.PicStruct;
    pParams->rc_control.bitrate             = pMFXInfo->TargetKbps;

    pParams->rc_control.rc_method           = (IConfigureVideoEncoder::Params::RCControl::RCMethod)
                                              pMFXInfo->RateControlMethod;
    pParams->ps_control.NumSlice            = pMFXInfo->NumSlice;
    pParams->ps_control.GopRefDist          = pMFXInfo->GopRefDist;
    pParams->ps_control.GopPicSize          = pMFXInfo->GopPicSize;
    pParams->ps_control.BufferSizeInKB      = pMFXInfo->BufferSizeInKB;
    pParams->target_usage                   = pMFXInfo->TargetUsage;

    pParams->frame_control.width            = pMFXInfo->FrameInfo.CropW;
    pParams->frame_control.height           = pMFXInfo->FrameInfo.CropH;

    return MFX_ERR_NONE;
}

mfxStatus CopyEncoderToMFXParams(IConfigureVideoEncoder::Params* pParams, mfxVideoParam* pmfxParams)
{
    //check input params
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pmfxParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = CodecPreset::VParamsFromPreset(*pmfxParams, pParams->preset);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //prepare mfxVideoParam
    mfxInfoMFX* pMFXInfo  = &pmfxParams->mfx;

    pMFXInfo->CodecLevel                = (mfxU16)pParams->level_idc;
    pMFXInfo->CodecProfile              = (mfxU16)pParams->profile_idc;

    pMFXInfo->FrameInfo.PicStruct       = MFX_PICSTRUCT_PROGRESSIVE;
    pMFXInfo->RateControlMethod         = (mfxU16)pParams->rc_control.rc_method;

    pMFXInfo->GopRefDist                = (mfxU8)pParams->ps_control.GopRefDist;
    pMFXInfo->GopPicSize                = (mfxU16)pParams->ps_control.GopPicSize;
    pMFXInfo->BufferSizeInKB            = (mfxU16)pParams->ps_control.BufferSizeInKB;
    pMFXInfo->NumSlice                  = (mfxU16)pParams->ps_control.NumSlice;

    pMFXInfo->TargetUsage               = (mfxU8)pParams->target_usage;
    pMFXInfo->MaxKbps                   = (mfxU16)pParams->rc_control.bitrate;
    pMFXInfo->TargetKbps                = (mfxU16)pParams->rc_control.bitrate;

    pMFXInfo->FrameInfo.CropW           = (mfxU16)(0 == pParams->frame_control.width ? pMFXInfo->FrameInfo.CropW : pParams->frame_control.width);
    pMFXInfo->FrameInfo.CropH           = (mfxU16)(0 == pParams->frame_control.height ? pMFXInfo->FrameInfo.CropH : pParams->frame_control.height);

    CodecPreset::VParamsFromPreset(*pmfxParams, pParams->preset);

    return MFX_ERR_NONE;
}