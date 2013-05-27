/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.


\* ****************************************************************************** */
#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_DEC)

#include "mfx_h264_dec.h"

#include "umc_data_pointers_copy.h"
#include "umc_h264_mfx_pro_supplier.h"

#include "umc_h264_frame_list.h"

#include "ippcore.h"

MFXVideoDECH264::MFXVideoDECH264(VideoCORE *core, mfxStatus * sts)
    : VideoDEC()
    , m_core(core)
    , m_pH264VideoDecoder(NULL)
    , m_pPostProcessing(0)
{
    m_MemoryAllocator.Init(NULL, (VideoCORE*)core);
    m_pPostProcessing = new UMC::DataPointersCopy();
    m_pH264VideoDecoder = new MFX_AVC_Decoder();

    memset(&m_vPar, 0, sizeof(m_vPar));
    memset(&m_fPar, 0, sizeof(m_fPar));
    memset(&m_sPar, 0, sizeof(m_sPar));

    if (sts)
    {
        *sts= MFX_ERR_NONE;
    }
}

MFXVideoDECH264::~MFXVideoDECH264(void)
{
    Close();
}

mfxStatus MFXVideoDECH264::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);
    return MFX_Utility::Query(0, in, out);
}

mfxStatus MFXVideoDECH264::Init(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);
    mfxStatus mfxRes = InternalReset(par);
    MFX_CHECK_STS(mfxRes);
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::Reset(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);
    return InternalReset(par);
}

mfxStatus MFXVideoDECH264::Close(void)
{
    // deallocate resources
    if (m_pH264VideoDecoder)
    {
        delete m_pH264VideoDecoder;
    }
    if (m_pPostProcessing)
    {
        delete m_pPostProcessing;
    }

    // reset pointers
    m_pH264VideoDecoder = NULL;
    m_pPostProcessing = NULL;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoDECH264::Close(void)

mfxStatus MFXVideoDECH264::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_vPar;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_fPar;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::GetSliceParam(mfxSliceParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_sPar;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::RunFrameFullDEC(mfxFrameCUC *cuc)
{
    try
    {
        MFX_CHECK_NULL_PTR1(cuc);

        m_pH264VideoDecoder->SetCUC(cuc);
        m_pH264VideoDecoder->RunSliceFullDEC(cuc);

        memcpy(&m_fPar, cuc->FrameParam, sizeof(m_fPar));
        memcpy(&m_sPar, cuc->SliceParam, sizeof(m_sPar));

        for (cuc->SliceId = 0; cuc->SliceId < cuc->NumSlice; cuc->SliceId++)
        {
            mfxStatus mfxRes = RunSliceFullDEC(cuc);
            MFX_CHECK_STS(mfxRes);
        }

    } catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::RunFramePredDEC(mfxFrameCUC *cuc)
{
    try
    {
        MFX_CHECK_NULL_PTR1(cuc);
        for (cuc->SliceId = 0; cuc->SliceId < cuc->NumSlice; cuc->SliceId++)
        {
            mfxStatus mfxRes = RunSlicePredDEC(cuc);
            MFX_CHECK_STS(mfxRes);
        }

    } catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::RunFrameIQT(mfxFrameCUC *cuc, mfxU8 scan)
{
    MFX_CHECK_NULL_PTR1(cuc);
    for (cuc->SliceId = 0; cuc->SliceId < cuc->NumSlice; cuc->SliceId++)
    {
        mfxStatus mfxRes = RunSliceIQT(cuc, scan);
        MFX_CHECK_STS(mfxRes);
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::GetFrameRecon(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    for (cuc->SliceId = 0; cuc->SliceId < cuc->NumSlice; cuc->SliceId++)
    {
        mfxStatus mfxRes = GetSliceRecon(cuc);
        MFX_CHECK_STS(mfxRes);
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::RunFrameILDB(mfxFrameCUC *)
{
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::RunSliceFullDEC(mfxFrameCUC *cuc)
{
    mfxStatus mfxRes = RunSlicePredDEC(cuc);
    MFX_CHECK_STS(mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoDECH264::RunSlicePredDEC(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    MFX_CHECK_NULL_PTR3(cuc->FrameParam, cuc->MbParam, cuc->MbParam->Mb);


    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::RunSliceIQT(mfxFrameCUC *cuc, mfxU8 )
{
    MFX_CHECK_NULL_PTR3(cuc, cuc->MbParam, cuc->MbParam->Mb);

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::RunSliceILDB(mfxFrameCUC *)
{
    return MFX_ERR_NONE;
}

mfxU8 GetMbHeightC(mfxU8 ColorFormat)
{
    switch (ColorFormat)
    {
    case MFX_CHROMAFORMAT_YUV420: return 8;
    case MFX_CHROMAFORMAT_YUV422: return 16;
    case MFX_CHROMAFORMAT_YUV444: return 16;
    default: return 0;
    }
}

mfxU8 GetMbWidthC(mfxU8 ColorFormat)
{
    switch (ColorFormat)
    {
    case MFX_CHROMAFORMAT_YUV420: return 8;
    case MFX_CHROMAFORMAT_YUV422: return 8;
    case MFX_CHROMAFORMAT_YUV444: return 16;
    default: return 0;
    }
}

mfxStatus MFXVideoDECH264::GetSliceRecon(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    MFX_CHECK_NULL_PTR3(cuc->FrameParam, cuc->SliceParam, cuc->FrameSurface);

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECH264::InternalReset(mfxVideoParam* par)
{
    m_vPar = *par;

    m_pH264VideoDecoder->Close();
    UMC::Status umcRes;
    UMC::VideoDecoderParams init;

    init.info.clip_info.height = par->mfx.FrameInfo.Height;
    init.info.clip_info.width = par->mfx.FrameInfo.Width;

    init.info.stream_subtype = UMC::UNDEF_VIDEO_SUBTYPE;
    init.info.stream_type = UMC::H264_VIDEO;
    init.numThreads = 1;

    init.lpMemoryAllocator = &m_MemoryAllocator;
    umcRes = m_pH264VideoDecoder->Init(&init);
    m_InternMediaDataOut.Init(par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height,UMC::YV12);

    memcpy(&m_vPar, par, sizeof(m_vPar));

    if (umcRes != UMC::UMC_OK)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    return MFX_ERR_NONE;
}

#endif // MFX_ENABLE_H264_VIDEO_DEC
