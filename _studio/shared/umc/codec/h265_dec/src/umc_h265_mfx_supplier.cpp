/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_mfx_supplier.h"

#ifndef UMC_RESTRICTED_CODE_MFX

#include "umc_h265_frame_list.h"
#include "umc_h265_nal_spl.h"
#include "umc_h265_bitstream.h"

#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_dec_mfx.h"

#include "vm_sys_info.h"

#include "umc_h265_dec_debug.h"
#include "mfxpcp.h"

#if defined (MFX_VA)
#include "umc_va_dxva2.h"
#endif

namespace UMC_HEVC_DECODER
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////
RawHeader_H265::RawHeader_H265()
{
    Reset();
}

void RawHeader_H265::Reset()
{
    m_id = -1;
    m_buffer.clear();
}

Ipp32s RawHeader_H265::GetID() const
{
    return m_id;
}

size_t RawHeader_H265::GetSize() const
{
    return m_buffer.size();
}

Ipp8u * RawHeader_H265::GetPointer()
{
    return &m_buffer[0];
}

void RawHeader_H265::Resize(Ipp32s id, size_t newSize)
{
    m_id = id;
    m_buffer.resize(newSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RawHeaders_H265::Reset()
{
    m_sps.Reset();
    m_pps.Reset();
}

RawHeader_H265 * RawHeaders_H265::GetSPS()
{
    return &m_sps;
}

RawHeader_H265 * RawHeaders_H265::GetPPS()
{
    return &m_pps;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////
MFXTaskSupplier_H265::MFXTaskSupplier_H265()
    : TaskSupplier_H265()
{
    memset(&m_firstVideoParams, 0, sizeof(mfxVideoParam));
}

MFXTaskSupplier_H265::~MFXTaskSupplier_H265()
{
    Close();
}

UMC::Status MFXTaskSupplier_H265::Init(UMC::BaseCodecParams *pInit)
{
    UMC::VideoDecoderParams *init = DynamicCast<UMC::VideoDecoderParams> (pInit);
    UMC::Status umcRes;

    if (NULL == init)
        return UMC::UMC_ERR_NULL_PTR;

    Close();

    m_initializationParams = *init;
    m_pMemoryAllocator = init->lpMemoryAllocator;
    m_DPBSizeEx = 0;

    m_sei_messages = new SEI_Storer_H265();
    m_sei_messages->Init();

    Ipp32s nAllowedThreadNumber = init->numThreads;
    if(nAllowedThreadNumber < 0) nAllowedThreadNumber = 0;
    if(nAllowedThreadNumber > 16) nAllowedThreadNumber = 16;

    // calculate number of slice decoders.
    // It should be equal to CPU number
    m_iThreadNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);

    umcRes = MVC_Extension::Init();
    if (UMC::UMC_OK != umcRes)
    {
        return umcRes;
    }

    AU_Splitter_H265::Init(init);

    switch(m_iThreadNum)
    {
    case 1:
        m_pTaskBroker = new TaskBrokerSingleThread_H265(this);
        break;
    case 4:
    case 3:
    case 2:
        m_pTaskBroker = new TaskBrokerTwoThread_H265(this);
        break;
    default:
        m_pTaskBroker = new TaskBrokerTwoThread_H265(this);
        break;
    }

    m_pTaskBroker->Init(m_iThreadNum, false);

    // create slice decoder(s)
    m_pSegmentDecoder = new H265SegmentDecoderMultiThreaded *[m_iThreadNum];
    if (NULL == m_pSegmentDecoder)
        return UMC::UMC_ERR_ALLOC;
    memset(m_pSegmentDecoder, 0, sizeof(H265SegmentDecoderMultiThreaded *) * m_iThreadNum);

    Ipp32u i;
    for (i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H265SegmentDecoderMultiThreaded(m_pTaskBroker);

        if (NULL == m_pSegmentDecoder[i])
            return UMC::UMC_ERR_ALLOC;
    }

    for (i = 0; i < m_iThreadNum; i += 1)
    {
        if (UMC::UMC_OK != m_pSegmentDecoder[i]->Init(i))
            return UMC::UMC_ERR_INIT;
    }

    LocalResources_H265::Init(m_iThreadNum, m_pMemoryAllocator);

    m_isUseDelayDPBValues = true;
    m_local_delta_frame_time = 1.0/30;
    m_frameOrder             = 0;
    m_use_external_framerate = 0 < init->info.framerate;

    if (m_use_external_framerate)
    {
        m_local_delta_frame_time = 1 / init->info.framerate;
    }

    GetView()->dpbSize = 16;
    m_DPBSizeEx = m_iThreadNum;

    return UMC::UMC_OK;
}

void MFXTaskSupplier_H265::Reset()
{
    TaskSupplier_H265::Reset();

    m_Headers.Reset();

    if (m_pTaskBroker)
        m_pTaskBroker->Init(m_iThreadNum, false);
}

bool MFXTaskSupplier_H265::CheckDecoding(bool should_additional_check, H265DecoderFrame * outputFrame)
{
    ViewItem_H265 &view = *GetView();

    if (!outputFrame->IsDecodingStarted())
        return false;

    if (outputFrame->IsDecodingCompleted())
    {
        if (!should_additional_check)
            return true;

        UMC::AutomaticUMCMutex guard(m_mGuard);

        Ipp32u count = 0;
        Ipp32u notDecoded = 0;
        for (H265DecoderFrame * pTmp = view.pDPB->head(); pTmp; pTmp = pTmp->future())
        {
            //if (!pTmp->GetRefCounter())
              //  return true;

            if (!pTmp->m_isShortTermRef &&
                !pTmp->m_isLongTermRef &&
                ((pTmp->m_wasOutputted != 0) || (pTmp->m_isDisplayable == 0)))
            {
                count++;
            }

            if (!pTmp->IsDecoded() && !pTmp->IsDecodingCompleted() && pTmp->IsFullFrame())
                notDecoded++;
        }

        DEBUG_PRINT1((VM_STRING("output frame - %d, notDecoded - %u, count - %u\n"), outputFrame->m_PicOrderCnt, notDecoded, count));

        if (notDecoded > m_DPBSizeEx)
            return false;

        if (count || (view.pDPB->countAllFrames() < view.dpbSize + m_DPBSizeEx))
            return true;

        if (!notDecoded)
            return true;
    }

    return false;
}

mfxStatus MFXTaskSupplier_H265::RunThread(mfxU32 threadNumber)
{
    if (m_pSegmentDecoder[threadNumber]->ProcessSegment() == UMC::UMC_ERR_NOT_ENOUGH_DATA)
        return MFX_TASK_BUSY;

    return MFX_TASK_WORKING;
}

void MFXTaskSupplier_H265::CompleteFrame(H265DecoderFrame * pFrame)
{
    if (!pFrame)
        return;

    H265DecoderFrameInfo * slicesInfo = pFrame->GetAU();
    if (slicesInfo->GetStatus() > H265DecoderFrameInfo::STATUS_NOT_FILLED)
        return;

    TaskSupplier_H265::CompleteFrame(pFrame);
}

void MFXTaskSupplier_H265::SetVideoParams(mfxVideoParam * par)
{
    m_firstVideoParams = *par;
}

UMC::Status MFXTaskSupplier_H265::DecodeHeaders(UMC::MediaDataEx *nalUnit)
{
    UMC::Status sts = TaskSupplier_H265::DecodeHeaders(nalUnit);
    if (sts != UMC::UMC_OK)
        return sts;

    H265SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();

    if (currSPS)
    {
        if (currSPS->bit_depth_luma > 8 || currSPS->bit_depth_chroma > 8 || currSPS->chroma_format_idc > 1)
            throw h265_exception(UMC::UMC_ERR_UNSUPPORTED);
    }

    {
        // save sps/pps
        Ipp32u nal_unit_type = nalUnit->GetExData()->values[0];
        switch(nal_unit_type)
        {
            case NAL_UNIT_SPS:
            case NAL_UNIT_PPS:
                {
                    static Ipp8u start_code_prefix[] = {0, 0, 0, 1};

                    size_t size = nalUnit->GetDataSize();
                    bool isSPS = (nal_unit_type == NAL_UNIT_SPS);
                    RawHeader_H265 * hdr = isSPS ? GetSPS() : GetPPS();
                    Ipp32s id = isSPS ? m_Headers.m_SeqParams.GetCurrentID() : m_Headers.m_PicParams.GetCurrentID();
                    hdr->Resize(id, size + sizeof(start_code_prefix));
                    memcpy(hdr->GetPointer(), start_code_prefix,  sizeof(start_code_prefix));
                    memcpy(hdr->GetPointer() + sizeof(start_code_prefix), (Ipp8u*)nalUnit->GetDataPointer(), size);
                }
            break;
        }
    }

    UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();
    if ((NalUnitType)pMediaDataEx->values[0] == NAL_UNIT_SPS && m_firstVideoParams.mfx.FrameInfo.Width)
    {
        H265SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();

        if (currSPS)
        {
            if (m_firstVideoParams.mfx.FrameInfo.Width < (currSPS->pic_width_in_luma_samples) ||
                m_firstVideoParams.mfx.FrameInfo.Height < (currSPS->pic_height_in_luma_samples) ||
                (currSPS->level_idc && m_firstVideoParams.mfx.CodecLevel && m_firstVideoParams.mfx.CodecLevel < currSPS->level_idc))
            {
                return UMC::UMC_NTF_NEW_RESOLUTION;
            }
        }

        return UMC::UMC_WRN_REPOSITION_INPROGRESS;
    }

    return UMC::UMC_OK;
}

UMC::Status MFXTaskSupplier_H265::DecodeSEI(UMC::MediaDataEx *nalUnit)
{
    if (m_Headers.m_SeqParams.GetCurrentID() == -1)
        return UMC::UMC_OK;

    H265Bitstream bitStream;

    try
    {
        MemoryPiece mem;
        mem.SetData(nalUnit);

        MemoryPiece * pMem = m_Heap.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);
        notifier1<Heap, MemoryPiece*> memory_leak_preventing(&m_Heap, &Heap::Free, pMem);

        memset(pMem->GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);

        SwapperBase * swapper = m_pNALSplitter->GetSwapper();
        swapper->SwapMemory(pMem, &mem, NULL);

        // Ipp32s nalIndex = nalUnit->GetExData()->index;

        bitStream.Reset((Ipp8u*)pMem->GetPointer(), (Ipp32u)pMem->GetDataSize());

        //bitStream.Reset((Ipp8u*)nalUnit->GetDataPointer() + nalUnit->GetExData()->offsets[nalIndex],
          //  nalUnit->GetExData()->offsets[nalIndex + 1] - nalUnit->GetExData()->offsets[nalIndex]);

        NalUnitType nal_unit_type;
        Ipp32u temporal_id, reserved_zero_6bits;

        bitStream.GetNALUnitType(nal_unit_type, temporal_id, reserved_zero_6bits);
        VM_ASSERT(0 == reserved_zero_6bits);

        do
        {
            H265SEIPayLoad    m_SEIPayLoads;

            size_t decoded1 = bitStream.BytesDecoded();

            /*Ipp32s target_sps =*/ bitStream.ParseSEI(m_Headers.m_SeqParams,
                m_Headers.m_SeqParams.GetCurrentID(), &m_SEIPayLoads);

            if (m_SEIPayLoads.payLoadType == SEI_USER_DATA_REGISTERED_TYPE)
            {
                m_UserData = m_SEIPayLoads;
            }
            else
            {
                if (m_SEIPayLoads.payLoadType == SEI_RESERVED)
                    continue;

                m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
            }

            size_t decoded2 = bitStream.BytesDecoded();

            // calculate payload size
            size_t size = decoded2 - decoded1;

            VM_ASSERT(size == m_SEIPayLoads.payLoadSize + 2 + (m_SEIPayLoads.payLoadSize / 255) + (m_SEIPayLoads.payLoadType / 255));

            UMC::MediaDataEx nalUnit1;

            nalUnit1.SetBufferPointer((Ipp8u*)nalUnit->GetDataPointer(), decoded1 + size);
            nalUnit1.SetDataSize(decoded1 + size);
            nalUnit1.MoveDataPointer((Ipp32s)decoded1);
            m_sei_messages->AddMessage(&nalUnit1, m_SEIPayLoads.payLoadType);

        } while (bitStream.More_RBSP_Data());

    } catch(...)
    {
        // nothing to do just catch it
    }

    return UMC::UMC_OK;
}

void MFXTaskSupplier_H265::AddFakeReferenceFrame(H265Slice * )
{
}

bool MFX_Utility::IsNeedPartialAcceleration_H265(mfxVideoParam * par, eMFXHWType type)
{
    if (!par)
        return false;

    if (type == MFX_HW_LAKE || type == MFX_HW_SNB)
    {
        if (par->mfx.FrameInfo.Width > 1920 || par->mfx.FrameInfo.Height > 1088)
            return true;
    }
    else
    {
        if (par->mfx.FrameInfo.Width > 4096 || par->mfx.FrameInfo.Height > 4096)
            return true;
    }

    mfxExtMVCSeqDesc * points = (mfxExtMVCSeqDesc*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    if (points && points->NumRefsTotal > 16 && type != MFX_HW_HSW)
        return true;

    return false;
}

inline bool isMVCProfile(mfxU32 profile)
{
    return (profile == MFX_PROFILE_AVC_MULTIVIEW_HIGH || profile == MFX_PROFILE_AVC_STEREO_HIGH);
}

eMFXPlatform MFX_Utility::GetPlatform_H265(VideoCORE * core, mfxVideoParam * par)
{
    eMFXPlatform platform = core->GetPlatformType();

    eMFXHWType typeHW = MFX_HW_UNKNOWN;
#if defined (MFX_VA)
    typeHW = core->GetHWType();
#endif

    if (par && IsNeedPartialAcceleration_H265(par, typeHW) && platform != MFX_PLATFORM_SOFTWARE)
    {
        return MFX_PLATFORM_SOFTWARE;
    }

#if defined (MFX_VA)
    GUID name = DXVA_ModeHEVC_VLD_MainProfile;

    if (MFX_ERR_NONE != core->IsGuidSupported(name, par) &&
        platform != MFX_PLATFORM_SOFTWARE)
    {
        return MFX_PLATFORM_SOFTWARE;
    }
#endif

    return platform;
}

UMC::Status MFX_Utility::FillVideoParam(TaskSupplier_H265 * supplier, mfxVideoParam *par, bool full)
{
    const H265SeqParamSet * seq = supplier->GetHeaders()->m_SeqParams.GetCurrentHeader();
    if (!seq)
        return UMC::UMC_ERR_FAILED;

    if (UMC_HEVC_DECODER::FillVideoParam(seq, par, full) != UMC::UMC_OK)
        return UMC::UMC_ERR_FAILED;

    return UMC::UMC_OK;
}

UMC::Status MFX_Utility::DecodeHeader(TaskSupplier_H265 * supplier, UMC::BaseCodecParams* params, mfxBitstream *bs, mfxVideoParam *out)
{
    UMC::VideoDecoderParams *lpInfo = DynamicCast<UMC::VideoDecoderParams> (params);

    UMC::Status umcRes = UMC::UMC_OK;

    if (!params->m_pData)
        return UMC::UMC_ERR_NULL_PTR;

    if (!params->m_pData->GetDataSize())
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;

    umcRes = supplier->PreInit(params);
    if (umcRes != UMC::UMC_OK)
        return UMC::UMC_ERR_FAILED;

    bool header_was_started = false;
    bool isMVC = GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC) != 0;

    struct HeadersFoundFlag
    {
        bool isVPS;
        bool isSPS;
        bool isPPS;

        HeadersFoundFlag()
            : isVPS(false)
            , isSPS(false)
            , isPPS(false)
        {
        }

        bool IsAll()
        {
            return isVPS && isSPS && isPPS;
        }
    };

    HeadersFoundFlag flags;

    H265Slice * lastSlice = 0;

    notifier2<Heap_Objects, void*, bool> memory_leak_preventing_slice(supplier->GetObjHeap(), &Heap_Objects::FreeObject, lastSlice, false);

    umcRes = UMC::UMC_ERR_NOT_ENOUGH_DATA;
    for ( ; lpInfo->m_pData->GetDataSize() > 4; )
    {
        try
        {
            if (flags.IsAll())
                break;

            bool quite = false;

            Ipp32s startCode = supplier->CheckNalUnitType(lpInfo->m_pData);

            if (!header_was_started) // move point to first start code
            {
                bs->DataOffset = (mfxU32)((mfxU8*)lpInfo->m_pData->GetDataPointer() - (mfxU8*)lpInfo->m_pData->GetBufferPointer());
                bs->DataLength = (mfxU32)lpInfo->m_pData->GetDataSize();
            }

            bool needProcess = false;

            switch ((NalUnitType)startCode)
            {
            case NAL_UNIT_CODED_SLICE_TRAIL_R:
            case NAL_UNIT_CODED_SLICE_TRAIL_N:
            case NAL_UNIT_CODED_SLICE_TLA:
            case NAL_UNIT_CODED_SLICE_TSA_N:
            case NAL_UNIT_CODED_SLICE_STSA_R:
            case NAL_UNIT_CODED_SLICE_STSA_N:
            case NAL_UNIT_CODED_SLICE_BLA:
            case NAL_UNIT_CODED_SLICE_BLANT:
            case NAL_UNIT_CODED_SLICE_BLA_N_LP:
            case NAL_UNIT_CODED_SLICE_IDR:
            case NAL_UNIT_CODED_SLICE_IDR_N_LP:
            case NAL_UNIT_CODED_SLICE_CRA:
            case NAL_UNIT_CODED_SLICE_DLP:
            case NAL_UNIT_CODED_SLICE_TFD:
                {
                    if (header_was_started)
                    {
                        if (!isMVC)
                        {
                            quite = true;
                            break;
                        }
                    }
                    else
                        break; // skip nal unit

                    UMC::MediaDataEx *nalUnit = supplier->GetNalUnit(lpInfo->m_pData);
                    if (!nalUnit)
                    {
                        supplier->Close();
                        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
                    }

                    H265Slice * pSlice = supplier->DecodeSliceHeader(nalUnit);
                    if (pSlice)
                    {
                        if (!lastSlice)
                        {
                            lastSlice = pSlice;
                            memory_leak_preventing_slice.SetParam1(lastSlice);
                            lastSlice->Release();
                            continue;
                        }

                        if ((false == IsPictureTheSame(pSlice, lastSlice)))
                        {
                            pSlice->Release();
                            supplier->GetObjHeap()->FreeObject(pSlice);
                            quite = true;
                            break;
                        }

                        pSlice->Release();
                        supplier->GetObjHeap()->FreeObject(lastSlice);

                        lastSlice = pSlice;
                        memory_leak_preventing_slice.SetParam1(lastSlice);
                        continue;
                    }
                }
                break;

            case NAL_UNIT_VPS:
                flags.isVPS = true;
                header_was_started = true;
                needProcess = true;
                break;

            case NAL_UNIT_SPS:
                flags.isSPS = true;
                header_was_started = true;
                needProcess = true;
                break;

            case NAL_UNIT_PPS:
                flags.isPPS = true;
                header_was_started = flags.isSPS;
                needProcess = true;
                break;

            //case UMC::NAL_UT_SUBSET_SPS:
            //    flags.isSPS_MVC = true;
            //    header_was_started = flags.isSPS;
            //    needProcess = true;
            //    break;

            case NAL_UNIT_ACCESS_UNIT_DELIMITER:
                if (header_was_started)
                    quite = true;
                break;

            };
            if (quite)
                break;

            UMC::MediaDataEx *nalUnit = supplier->GetNalUnit(lpInfo->m_pData);
            if (!nalUnit)
            {
                supplier->Close();
                return UMC::UMC_ERR_NOT_ENOUGH_DATA;
            }

            if (NAL_UNIT_SEI == startCode)
                needProcess = true;

            if (needProcess)
                umcRes = supplier->ProcessNalUnit(nalUnit);
        }
        catch(const h265_exception & ex)
        {
            umcRes = ex.GetStatus();
        }

        if (umcRes == UMC::UMC_ERR_SYNC) // move pointer
        {
            bs->DataOffset = (mfxU32)((mfxU8*)lpInfo->m_pData->GetDataPointer() - (mfxU8*)lpInfo->m_pData->GetBufferPointer());
            bs->DataLength = (mfxU32)lpInfo->m_pData->GetDataSize();
            supplier->Close();
            return UMC::UMC_ERR_NOT_ENOUGH_DATA;
        }

        if (umcRes == UMC::UMC_OK || umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS || umcRes == UMC::UMC_NTF_NEW_RESOLUTION)
        {
            umcRes = UMC::UMC_OK;
        }

        if (umcRes != UMC::UMC_OK) // move pointer
        {
            umcRes = UMC::UMC_OK;
        }
    }

    if (umcRes != UMC::UMC_OK)
    {
        return umcRes;
    }

    umcRes = supplier->GetInfo(lpInfo);
    if (umcRes == UMC::UMC_OK)
    {
        FillVideoParam(supplier, out, false);
    }
    else
    {
    }

    return umcRes;
}

#if (HEVC_OPT_CHANGES & 1)
// ML: OPT: to allow the use of __fastcall default inside of a project below needs to be explicitely __cdecl
mfxStatus MFX_CDECL MFX_Utility::Query_H265(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type)
#else
mfxStatus MFX_Utility::Query_H265(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type)
#endif
{
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (in == out)
    {
        mfxVideoParam in1;
        memcpy(&in1, in, sizeof(mfxVideoParam));
        return MFX_Utility::Query_H265(core, &in1, out, type);
    }

    memset(&out->mfx, 0, sizeof(mfxInfoMFX));

    if (in)
    {
        if (in->mfx.CodecId == MFX_CODEC_HEVC)
            out->mfx.CodecId = in->mfx.CodecId;

        if ((MFX_PROFILE_UNKNOWN == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_BASELINE == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_MAIN == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_HIGH == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_MULTIVIEW_HIGH == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_STEREO_HIGH == in->mfx.CodecProfile))
            out->mfx.CodecProfile = in->mfx.CodecProfile;
        else
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        switch (in->mfx.CodecLevel)
        {
        case MFX_LEVEL_UNKNOWN:
        case MFX_LEVEL_AVC_1:
        case MFX_LEVEL_AVC_1b:
        case MFX_LEVEL_AVC_11:
        case MFX_LEVEL_AVC_12:
        case MFX_LEVEL_AVC_13:
        case MFX_LEVEL_AVC_2:
        case MFX_LEVEL_AVC_21:
        case MFX_LEVEL_AVC_22:
        case MFX_LEVEL_AVC_3:
        case MFX_LEVEL_AVC_31:
        case MFX_LEVEL_AVC_32:
        case MFX_LEVEL_AVC_4:
        case MFX_LEVEL_AVC_41:
        case MFX_LEVEL_AVC_42:
        case MFX_LEVEL_AVC_5:
        case MFX_LEVEL_AVC_51:
            out->mfx.CodecLevel = in->mfx.CodecLevel;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        if (in->mfx.NumThread < 128)
        {
            out->mfx.NumThread = in->mfx.NumThread;
        }
        else
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->AsyncDepth < MFX_MAX_ASYNC_DEPTH_VALUE) // Actually AsyncDepth > 5-7 is for debugging only.
            out->AsyncDepth = in->AsyncDepth;

        if (in->mfx.DecodedOrder)
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        if ((in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) ||
            (in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        {
            Ipp32u mask = in->IOPattern & 0xf0;
            if (mask != MFX_IOPATTERN_OUT_VIDEO_MEMORY || mask != MFX_IOPATTERN_OUT_SYSTEM_MEMORY || mask != MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
                out->IOPattern = in->IOPattern;
        }

        if (in->mfx.FrameInfo.FourCC)
        {
            // mfxFrameInfo
            if (in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12)
                out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420 || (!in->mfx.FrameInfo.FourCC && !in->mfx.FrameInfo.ChromaFormat))
            out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
        else
            sts = MFX_ERR_UNSUPPORTED;

        // FIXME: Add check that width is multiple of minimal CU size
        if (/* in->mfx.FrameInfo.Width % 16 == 0 && */ in->mfx.FrameInfo.Width <= 16384)
            out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
        else
            sts = MFX_ERR_UNSUPPORTED;

        // FIXME: Add check that height is multiple of minimal CU size
        if (/* in->mfx.FrameInfo.Height % 16 == 0 && */ in->mfx.FrameInfo.Height <= 16384)
            out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;
        else
            sts = MFX_ERR_UNSUPPORTED;

        if ((in->mfx.FrameInfo.Width || in->mfx.FrameInfo.Height) && !(in->mfx.FrameInfo.Width && in->mfx.FrameInfo.Height))
        {
            out->mfx.FrameInfo.Width = 0;
            out->mfx.FrameInfo.Height = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        /*if (in->mfx.FrameInfo.CropX <= out->mfx.FrameInfo.Width)
            out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;

        if (in->mfx.FrameInfo.CropY <= out->mfx.FrameInfo.Height)
            out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;

        if (out->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW <= out->mfx.FrameInfo.Width)
            out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;

        if (out->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH <= out->mfx.FrameInfo.Height)
            out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;*/

        out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
        out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;

        if ((in->mfx.FrameInfo.FrameRateExtN || in->mfx.FrameInfo.FrameRateExtD) && !(in->mfx.FrameInfo.FrameRateExtN && in->mfx.FrameInfo.FrameRateExtD))
        {
            out->mfx.FrameInfo.FrameRateExtN = 0;
            out->mfx.FrameInfo.FrameRateExtD = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
        out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

        if ((in->mfx.FrameInfo.AspectRatioW || in->mfx.FrameInfo.AspectRatioH) && !(in->mfx.FrameInfo.AspectRatioW && in->mfx.FrameInfo.AspectRatioH))
        {
            out->mfx.FrameInfo.AspectRatioW = 0;
            out->mfx.FrameInfo.AspectRatioH = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        switch (in->mfx.FrameInfo.PicStruct)
        {
        case MFX_PICSTRUCT_UNKNOWN:
        case MFX_PICSTRUCT_PROGRESSIVE:
        case MFX_PICSTRUCT_FIELD_TFF:
        case MFX_PICSTRUCT_FIELD_BFF:
        case MFX_PICSTRUCT_FIELD_REPEATED:
        case MFX_PICSTRUCT_FRAME_DOUBLING:
        case MFX_PICSTRUCT_FRAME_TRIPLING:
            out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        mfxStatus stsExt = CheckDecodersExtendedBuffers(in);
        if (stsExt < MFX_ERR_NONE)
            sts = MFX_ERR_UNSUPPORTED;

        if (in->Protected)
        {
            out->Protected = in->Protected;

            if (type == MFX_HW_UNKNOWN)
            {
                sts = MFX_ERR_UNSUPPORTED;
                out->Protected = 0;
            }

            if (!IS_PROTECTION_PAVP_ANY(in->Protected))
            {
                sts = MFX_ERR_UNSUPPORTED;
                out->Protected = 0;
            }

            if (!(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
            {
                out->IOPattern = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            mfxExtPAVPOption * pavpOptIn = (mfxExtPAVPOption*)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
            mfxExtPAVPOption * pavpOptOut = (mfxExtPAVPOption*)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);

            if (!pavpOptIn)
                sts = MFX_ERR_UNSUPPORTED;

            if (pavpOptIn && pavpOptOut)
            {
                pavpOptOut->Header.BufferId = pavpOptIn->Header.BufferId;
                pavpOptOut->Header.BufferSz = pavpOptIn->Header.BufferSz;

                switch(pavpOptIn->CounterType)
                {
                case MFX_PAVP_CTR_TYPE_A:
                    pavpOptOut->CounterType = pavpOptIn->CounterType;
                    break;
                case MFX_PAVP_CTR_TYPE_B:
                    if (in->Protected == MFX_PROTECTION_PAVP)
                    {
                        pavpOptOut->CounterType = pavpOptIn->CounterType;
                        break;
                    }
                    pavpOptOut->CounterType = 0;
                    sts = MFX_ERR_UNSUPPORTED;
                    break;
                case MFX_PAVP_CTR_TYPE_C:
                    pavpOptOut->CounterType = pavpOptIn->CounterType;
                    break;
                default:
                    pavpOptOut->CounterType = 0;
                    sts = MFX_ERR_UNSUPPORTED;
                    break;
                }

                if (pavpOptIn->EncryptionType == MFX_PAVP_AES128_CBC || pavpOptIn->EncryptionType == MFX_PAVP_AES128_CTR)
                {
                    pavpOptOut->EncryptionType = pavpOptIn->EncryptionType;
                    if (pavpOptIn->EncryptionType == MFX_PAVP_AES128_CBC)
                    {
                        pavpOptOut->EncryptionType = 0;
                        sts = MFX_ERR_UNSUPPORTED;
                    }
                }
                else
                {
                    pavpOptOut->EncryptionType = 0;
                    sts = MFX_ERR_UNSUPPORTED;
                }

                pavpOptOut->CounterIncrement = 0;
                memset(&pavpOptOut->CipherCounter, 0, sizeof(mfxAES128CipherCounter));
                memset(pavpOptOut->reserved, 0, sizeof(pavpOptOut->reserved));
            }
            else
            {
                if (pavpOptOut || pavpOptIn)
                {
                    sts = MFX_ERR_UNDEFINED_BEHAVIOR;
                    /*mfxExtBuffer header = pavpOptOut->Header;
                    memset(pavpOptOut, 0, sizeof(mfxExtPAVPOption));
                    pavpOptOut->Header = header;*/
                }
            }
        }
        else
        {
            mfxExtPAVPOption * pavpOptIn = (mfxExtPAVPOption*)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
            if (pavpOptIn)
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (GetPlatform_H265(core, out) != core->GetPlatformType() && sts == MFX_ERR_NONE)
        {
            VM_ASSERT(GetPlatform_H265(core, out) == MFX_PLATFORM_SOFTWARE);
            sts = MFX_WRN_PARTIAL_ACCELERATION;
        }
    }
    else
    {
        out->mfx.CodecId = MFX_CODEC_HEVC;
        out->mfx.CodecProfile = 1;
        out->mfx.CodecLevel = 1;

        out->mfx.NumThread = 1;

        out->mfx.DecodedOrder = 0;

        out->AsyncDepth = 1;

        // mfxFrameInfo
        out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        out->mfx.FrameInfo.Width = 16;
        out->mfx.FrameInfo.Height = 16;

        //out->mfx.FrameInfo.CropX = 1;
        //out->mfx.FrameInfo.CropY = 1;
        //out->mfx.FrameInfo.CropW = 1;
        //out->mfx.FrameInfo.CropH = 1;

        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;

        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;

        out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

        if (type == MFX_HW_SNB || type == MFX_HW_IVB || type == MFX_HW_HSW)
        {
            out->Protected = MFX_PROTECTION_GPUCP_PAVP;

            mfxExtPAVPOption * pavpOptOut = (mfxExtPAVPOption*)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
            if (pavpOptOut)
            {
                mfxExtBuffer header = pavpOptOut->Header;
                memset(pavpOptOut, 0, sizeof(mfxExtPAVPOption));
                pavpOptOut->Header = header;
                pavpOptOut->CounterType = (mfxU16)(type == MFX_HW_LAKE ? MFX_PAVP_CTR_TYPE_C : MFX_PAVP_CTR_TYPE_B);
                pavpOptOut->EncryptionType = MFX_PAVP_AES128_CTR;
                pavpOptOut->CounterIncrement = 0;
                pavpOptOut->CipherCounter.Count = 0;
                pavpOptOut->CipherCounter.IV = 0;
            }
        }

        mfxExtMVCTargetViews * targetViewsOut = (mfxExtMVCTargetViews *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
        if (targetViewsOut)
        {
        }

        mfxExtMVCSeqDesc * mvcPointsOut = (mfxExtMVCSeqDesc *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
        if (mvcPointsOut)
        {
        }

        mfxExtOpaqueSurfaceAlloc * opaqueOut = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        if (opaqueOut)
        {
        }

        out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        if (type == MFX_HW_UNKNOWN)
        {
            out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        else
        {
            out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }
    }

    return sts;
}

#if (HEVC_OPT_CHANGES & 1)
// ML: OPT: to allow the use of __fastcall default inside of a project below needs to be explicitely __cdecl
bool MFX_CDECL MFX_Utility::CheckVideoParam_H265(mfxVideoParam *in, eMFXHWType type)
#else
bool MFX_Utility::CheckVideoParam_H265(mfxVideoParam *in, eMFXHWType type)
#endif
{
    if (!in)
        return false;

    if (in->Protected)
    {
        if (type == MFX_HW_UNKNOWN || !IS_PROTECTION_PAVP_ANY(in->Protected))
            return false;
    }

    if (MFX_CODEC_HEVC != in->mfx.CodecId)
        return false;

    // FIXME: Add check that width is multiple of minimal CU size
    if (in->mfx.FrameInfo.Width > 16384 /* || (in->mfx.FrameInfo.Width % in->mfx.FrameInfo.reserved[0]) */)
        return false;

    // FIXME: Add check that height is multiple of minimal CU size
    if (in->mfx.FrameInfo.Height > 16384 /* || (in->mfx.FrameInfo.Height % in->mfx.FrameInfo.reserved[0]) */)
        return false;

#if 0
    // ignore Crop parameters at Init/Reset stage
    if (in->mfx.FrameInfo.CropX > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY > in->mfx.FrameInfo.Height)
        return false;

    if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height)
        return false;
#endif

    if (in->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
        return false;

    // both zero or not zero
    if ((in->mfx.FrameInfo.AspectRatioW || in->mfx.FrameInfo.AspectRatioH) && !(in->mfx.FrameInfo.AspectRatioW && in->mfx.FrameInfo.AspectRatioH))
        return false;

    switch (in->mfx.FrameInfo.PicStruct)
    {
    case MFX_PICSTRUCT_UNKNOWN:
    case MFX_PICSTRUCT_PROGRESSIVE:
    case MFX_PICSTRUCT_FIELD_TFF:
    case MFX_PICSTRUCT_FIELD_BFF:
    case MFX_PICSTRUCT_FIELD_REPEATED:
    case MFX_PICSTRUCT_FRAME_DOUBLING:
    case MFX_PICSTRUCT_FRAME_TRIPLING:
        break;
    default:
        return false;
    }

    if (in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
        return false;

    if (!(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && !(in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        return false;

    return true;
}

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_MFX
#endif // UMC_ENABLE_H265_VIDEO_DECODER
