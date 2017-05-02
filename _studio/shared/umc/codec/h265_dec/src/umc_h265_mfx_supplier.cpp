//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_mfx_supplier.h"

#include "umc_h265_frame_list.h"
#include "umc_h265_nal_spl.h"
#include "umc_h265_bitstream_headers.h"

#include "umc_h265_dec_defs.h"

#include "vm_sys_info.h"

#include "umc_h265_debug.h"

#include "umc_h265_mfx_utils.h"

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

// Initialize task supplier
UMC::Status MFXTaskSupplier_H265::Init(UMC::VideoDecoderParams *init)
{
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

    // calculate number of slice decoders.
    // It should be equal to CPU number
    m_iThreadNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);

    umcRes = MVC_Extension::Init();
    if (UMC::UMC_OK != umcRes)
    {
        return umcRes;
    }

    AU_Splitter_H265::Init(init);

    m_pSegmentDecoder = new H265SegmentDecoderBase *[m_iThreadNum];
    memset(m_pSegmentDecoder, 0, sizeof(H265SegmentDecoderBase *) * m_iThreadNum);

    CreateTaskBroker();
    m_pTaskBroker->Init(m_iThreadNum);

    for (Ipp32u i = 0; i < m_iThreadNum; i += 1)
    {
        if (UMC::UMC_OK != m_pSegmentDecoder[i]->Init(i))
            return UMC::UMC_ERR_INIT;
    }

    m_local_delta_frame_time = 1.0/30;
    m_frameOrder             = 0;
    m_use_external_framerate = 0 < init->info.framerate;

    if (m_use_external_framerate)
    {
        m_local_delta_frame_time = 1 / init->info.framerate;
    }

    GetView()->dpbSize = 16;
    m_DPBSizeEx = m_iThreadNum + init->info.bitrate;

#ifndef MFX_VA
    MFX_HEVC_PP::InitDispatcher();
#endif
    return UMC::UMC_OK;
}

// Check whether specified frame has been decoded, and if it was,
// whether it is necessary to display some frame
bool MFXTaskSupplier_H265::CheckDecoding(bool should_additional_check, H265DecoderFrame * outputFrame)
{
    ViewItem_H265 &view = *GetView();

    if (!outputFrame->IsDecodingStarted())
        return false;

    if (outputFrame->IsDecodingCompleted())
    {
        if (!should_additional_check)
            return true;

        Ipp32s maxReadyUID = outputFrame->m_UID;
        Ipp32u inDisplayStage = 0;

        UMC::AutomaticUMCMutex guard(m_mGuard);

        for (H265DecoderFrame * pTmp = view.pDPB->head(); pTmp; pTmp = pTmp->future())
        {
            if (pTmp->m_wasOutputted != 0 && pTmp->m_wasDisplayed == 0 && pTmp->m_maxUIDWhenWasDisplayed)
            {
                inDisplayStage++; // number of outputted frames at this moment
            }

            if ((pTmp->IsDecoded() || pTmp->IsDecodingCompleted()) && maxReadyUID < pTmp->m_UID)
                maxReadyUID = pTmp->m_UID;
        }

        DEBUG_PRINT1((VM_STRING("output frame - %d, notDecoded - %u, count - %u\n"), outputFrame->m_PicOrderCnt, notDecoded, count));
        if (inDisplayStage > 1 || m_maxUIDWhenWasDisplayed <= maxReadyUID)
        {
            return true;
        }
    }

    return false;
}

// Perform decoding task for thread number threadNumber
mfxStatus MFXTaskSupplier_H265::RunThread(mfxU32 threadNumber)
{
    UMC::Status sts = m_pSegmentDecoder[threadNumber]->ProcessSegment();

    if (sts == UMC::UMC_ERR_NOT_ENOUGH_DATA)
        return MFX_TASK_BUSY;
#if defined (MFX_VA)
    else if(sts == UMC::UMC_ERR_DEVICE_FAILED)
        return MFX_ERR_DEVICE_FAILED;
    else if (sts == UMC::UMC_ERR_GPU_HANG)
        return MFX_ERR_GPU_HANG;
#endif

    if (sts != UMC::UMC_OK)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    return MFX_TASK_WORKING;
}

// Check whether all slices for the frame were found
void MFXTaskSupplier_H265::CompleteFrame(H265DecoderFrame * pFrame)
{
    if (!pFrame)
        return;

    H265DecoderFrameInfo * slicesInfo = pFrame->GetAU();
    if (slicesInfo->GetStatus() > H265DecoderFrameInfo::STATUS_NOT_FILLED)
        return;

    TaskSupplier_H265::CompleteFrame(pFrame);
}

// Set initial video params from application
void MFXTaskSupplier_H265::SetVideoParams(mfxVideoParam * par)
{
    m_firstVideoParams = *par;
    m_decodedOrder     = m_firstVideoParams.mfx.DecodedOrder != 0;
}

UMC::Status MFXTaskSupplier_H265::FillVideoParam(mfxVideoParam *par, bool full)
{
    const H265SeqParamSet * seq = GetHeaders()->m_SeqParams.GetCurrentHeader();
    if (!seq)
        return UMC::UMC_ERR_FAILED;

    if (MFX_Utility::FillVideoParam(seq, par, full) != UMC::UMC_OK)
        return UMC::UMC_ERR_FAILED;

    return UMC::UMC_OK;
}

// Decode headers nal unit
UMC::Status MFXTaskSupplier_H265::DecodeHeaders(UMC::MediaDataEx *nalUnit)
{
    UMC::Status sts = TaskSupplier_H265::DecodeHeaders(nalUnit);
    if (sts != UMC::UMC_OK)
        return sts;

    {
        // save sps/pps
        Ipp32u nal_unit_type = nalUnit->GetExData()->values[0];
        switch(nal_unit_type)
        {
            case NAL_UT_SPS:
            case NAL_UT_PPS:
                {
                    static const Ipp8u start_code_prefix[] = {0, 0, 0, 1};

                    size_t size = nalUnit->GetDataSize();
                    bool isSPS = (nal_unit_type == NAL_UT_SPS);
                    RawHeader_H265 * hdr = isSPS ? GetSPS() : GetPPS();
                    Ipp32s id = isSPS ? m_Headers.m_SeqParams.GetCurrentID() : m_Headers.m_PicParams.GetCurrentID();
                    hdr->Resize(id, size + sizeof(start_code_prefix));
                    MFX_INTERNAL_CPY(hdr->GetPointer(), start_code_prefix,  sizeof(start_code_prefix));
                    MFX_INTERNAL_CPY(hdr->GetPointer() + sizeof(start_code_prefix), (Ipp8u*)nalUnit->GetDataPointer(), size);
                }
            break;
        }
    }

    UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();
    if ((NalUnitType)pMediaDataEx->values[0] == NAL_UT_SPS && m_firstVideoParams.mfx.FrameInfo.Width)
    {
        H265SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();

        if (currSPS)
        {
            if (m_firstVideoParams.mfx.FrameInfo.Width < (currSPS->pic_width_in_luma_samples) ||
                m_firstVideoParams.mfx.FrameInfo.Height < (currSPS->pic_height_in_luma_samples) ||
                (currSPS->m_pcPTL.GetGeneralPTL()->level_idc && m_firstVideoParams.mfx.CodecLevel && m_firstVideoParams.mfx.CodecLevel < currSPS->m_pcPTL.GetGeneralPTL()->level_idc))
            {
                return UMC::UMC_NTF_NEW_RESOLUTION;
            }
        }

        return UMC::UMC_WRN_REPOSITION_INPROGRESS;
    }

    return UMC::UMC_OK;
}

// Decode SEI nal unit
UMC::Status MFXTaskSupplier_H265::DecodeSEI(UMC::MediaDataEx *nalUnit)
{
    if (m_Headers.m_SeqParams.GetCurrentID() == -1)
        return UMC::UMC_OK;

    H265HeadersBitstream bitStream;

    try
    {
        MemoryPiece mem;
        mem.SetData(nalUnit);

        MemoryPiece swappedMem;
        swappedMem.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

        SwapperBase * swapper = m_pNALSplitter->GetSwapper();
        swapper->SwapMemory(&swappedMem, &mem, 0);

        bitStream.Reset((Ipp8u*)swappedMem.GetPointer(), (Ipp32u)swappedMem.GetDataSize());

        NalUnitType nal_unit_type;
        Ipp32u temporal_id;

        bitStream.GetNALUnitType(nal_unit_type, temporal_id);
        nalUnit->MoveDataPointer(2); // skip[ [NAL unit header = 16 bits]

        do
        {
            H265SEIPayLoad    m_SEIPayLoads;

            size_t decoded1 = bitStream.BytesDecoded();

            bitStream.ParseSEI(m_Headers.m_SeqParams, m_Headers.m_SeqParams.GetCurrentID(), &m_SEIPayLoads);

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

            if (m_sei_messages)
            {
                UMC::MediaDataEx nalUnit1;

                size_t nal_u_size = size;
                for(Ipp8u *ptr = (Ipp8u*)nalUnit->GetDataPointer(); ptr < (Ipp8u*)nalUnit->GetDataPointer() + nal_u_size; ptr++)
                    if (ptr[0]==0 && ptr[1]==0 && ptr[2]==3)
                        nal_u_size += 1;

                nalUnit1.SetBufferPointer((Ipp8u*)nalUnit->GetDataPointer(), nal_u_size);
                nalUnit1.SetDataSize(nal_u_size);
                nalUnit1.SetExData(nalUnit->GetExData());

                Ipp64f start, stop;
                nalUnit->GetTime(start, stop);
                nalUnit1.SetTime(start, stop);

                nalUnit->MoveDataPointer((Ipp32s)nal_u_size);

                SEI_Storer_H265::SEI_Message* msg =
                    m_sei_messages->AddMessage(&nalUnit1, m_SEIPayLoads.payLoadType);
                //frame is bound to SEI prefix payloads w/ the first slice
                //here we bind SEI suffix payloads
                if (msg && msg->nal_type == NAL_UT_SEI_SUFFIX)
                    msg->frame = GetView()->pCurFrame;
            }

        } while (bitStream.More_RBSP_Data());

    } catch(...)
    {
        // nothing to do just catch it
    }

    return UMC::UMC_OK;
}

// Do something in case reference frame is missing
void MFXTaskSupplier_H265::AddFakeReferenceFrame(H265Slice * )
{
}

} // namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER
