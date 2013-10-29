/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_BSD)

#include "mfx_h264_bsd.h"

#include "umc_data_pointers_copy.h"
#include "umc_h264_mfx_pro_supplier.h"

#include "umc_h264_frame_list.h"

#include "ippcore.h"

mfxStatus ConvertMfxBSToMediaData(mfxBitstream *pBitstream, UMC::MediaData * data)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    data->SetBufferPointer(pBitstream->Data, pBitstream->MaxLength);
    data->SetDataSize(pBitstream->DataOffset + pBitstream->DataLength);
    data->MoveDataPointer(pBitstream->DataOffset);
    return MFXSts;
}

mfxStatus ConvertMediaDataToMfxBS(UMC::MediaData * data, mfxBitstream *pBitstream)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    if (data->GetDataSize())
    {
        pBitstream->DataOffset = (mfxU32)((mfxU8*)data->GetDataPointer() - (mfxU8*)data->GetBufferPointer());
        pBitstream->DataLength = (mfxU32)data->GetDataSize();
    }
    else
    {
        pBitstream->DataOffset = 0;
        pBitstream->DataLength = 0;
    }
    return MFXSts;
}


VideoBSDH264::VideoBSDH264(VideoCORE *core, mfxStatus * sts)
    : VideoBSD()
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
        *sts = MFX_ERR_NONE;
    }
}

VideoBSDH264::~VideoBSDH264(void)
{
    Close();
    delete m_pH264VideoDecoder;
    delete m_pPostProcessing;
}

mfxStatus VideoBSDH264::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);
    return MFX_Utility::Query(0, in, out);
}

mfxStatus VideoBSDH264::Init(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    mfxStatus mfxRes = InternalReset(par);
    MFX_CHECK_STS(mfxRes);

    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::Reset(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);
    return InternalReset(par);
}

mfxStatus VideoBSDH264::Close(void)
{
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_vPar;
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_fPar;
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::GetSliceParam(mfxSliceParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_sPar;
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::RunVideoParam(mfxBitstream *bs, mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);

    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::RunFrameParam(mfxBitstream *bs, mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);

    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::RunSliceParam(mfxBitstream *bs, mfxSliceParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);

    //mfxSliceParam *par;

    return MFX_ERR_NONE;
}

inline mfxU8 ConvertH264SliceTypeToMFX(Ipp32s slice_type)
{
    mfxU8 mfx_type;
    switch(slice_type)
    {
    case UMC::INTRASLICE:
        mfx_type = MFX_SLICETYPE_I;
        break;
    case UMC::PREDSLICE:
        mfx_type = MFX_SLICETYPE_P;
        break;
    case UMC::BPREDSLICE:
        mfx_type = MFX_SLICETYPE_B;
        break;
    case UMC::S_PREDSLICE:
        mfx_type = MFX_SLICETYPE_P;//MFX_SLICE_SP;
        break;
    case UMC::S_INTRASLICE:
        mfx_type = MFX_SLICETYPE_I;//MFX_SLICE_SI;
        break;
    default:
        VM_ASSERT(false);
        mfx_type = MFX_SLICETYPE_I;
    }

    return mfx_type;
}

mfxStatus VideoBSDH264::RunSliceBSD(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR3(cuc, cuc->MbParam, cuc->MbParam->Mb);
    MFX_CHECK_NULL_PTR2(cuc->Bitstream, cuc->SliceParam);

    MFX_INTERNAL_CPY(&m_fPar, cuc->FrameParam, sizeof(m_fPar));
    MFX_INTERNAL_CPY(&m_sPar, cuc->SliceParam, sizeof(m_sPar));

    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::RunFrameBSD(mfxFrameCUC *cuc)
{
    try
    {
        MFX_CHECK_NULL_PTR1(cuc);
        MFX_CHECK_NULL_PTR1(cuc->Bitstream);

        UMC::MediaData m_pInternMediaDataIn;

        ConvertMfxBSToMediaData(cuc->Bitstream, &m_pInternMediaDataIn);

        m_pH264VideoDecoder->Reset();
        m_pH264VideoDecoder->SetCUC(cuc);
        m_pH264VideoDecoder->ParseSource(&m_pInternMediaDataIn);

        MFX_INTERNAL_CPY(&m_fPar, cuc->FrameParam, sizeof(m_fPar));
        MFX_INTERNAL_CPY(&m_sPar, cuc->SliceParam, sizeof(m_sPar));

        ConvertMediaDataToMfxBS(&m_pInternMediaDataIn, cuc->Bitstream);

    } catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::RunSliceMFX(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::RunFrameMFX(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::ExtractUserData(mfxBitstream *, mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    MFX_CHECK_NULL_PTR3(ud, sz, ts);

    return MFX_ERR_NONE;
}

mfxStatus VideoBSDH264::InternalReset(mfxVideoParam* par)
{
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
    m_pH264VideoDecoder->SetMemoryAllocator(&m_MemoryAllocator);
    m_InternMediaDataOut.Init(par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height,UMC::YV12);

    MFX_INTERNAL_CPY(&m_vPar, par, sizeof(m_vPar));

    if (umcRes != UMC::UMC_OK)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    return MFX_ERR_NONE;
}

#endif
