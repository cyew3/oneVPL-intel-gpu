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
//#define UMC_ENABLE_H265_VIDEO_DECODER
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "memory"

#include "umc_h265_task_supplier.h"
#include "umc_h265_frame_list.h"
#include "umc_automatic_mutex.h"
#include "umc_h265_nal_spl.h"
#include "umc_h265_bitstream.h"

#include "umc_h265_dec_defs_dec.h"
#include "vm_sys_info.h"
#include "umc_h265_segment_decoder_mt.h"

#include "umc_h265_task_broker.h"
#include "umc_video_processing.h"
#include "umc_structures.h"

#include "umc_frame_data.h"

#include "umc_h265_dec_debug.h"

#include <algorithm>

namespace UMC_HEVC_DECODER
{
/****************************************************************************************************/
// DecReferencePictureMarking_H265
/****************************************************************************************************/
DecReferencePictureMarking_H265::DecReferencePictureMarking_H265()
    : m_isDPBErrorFound(0)
    , m_frameCount(0)
{
}

void DecReferencePictureMarking_H265::Reset()
{
    m_frameCount = 0;
    m_isDPBErrorFound = 0;
}

void DecReferencePictureMarking_H265::ResetError()
{
    m_isDPBErrorFound = 0;
}

Ipp32u DecReferencePictureMarking_H265::GetDPBError() const
{
    return m_isDPBErrorFound;
}

UMC::Status DecReferencePictureMarking_H265::UpdateRefPicMarking(ViewItem_H265 &view, const H265Slice * pSlice)
{
    UMC::Status umcRes = UMC::UMC_OK;

    m_frameCount++;

    H265SliceHeader const * sliceHeader = pSlice->GetSliceHeader();

    if (sliceHeader->IdrPicFlag)
    {
        // mark all reference pictures as unused
        for (H265DecoderFrame *pCurr = view.pDPB->head(); pCurr; pCurr = pCurr->future())
        {
            if (pCurr->isShortTermRef() || pCurr->isLongTermRef())
            {
                pCurr->SetisShortTermRef(false);
                pCurr->SetisLongTermRef(false);
            }
        }
    }
    else
    {
        ReferencePictureSet* rps = pSlice->getRPS();
        // loop through all pictures in the reference picture buffer
        for (H265DecoderFrame *pTmp = view.pDPB->head(); pTmp; pTmp = pTmp->future())
        {
            if (pTmp->isDisposable())
                continue;

            bool isReferenced = false;
            // loop through all pictures in the Reference Picture Set
            // to see if the picture should be kept as reference picture
            Ipp32s i;
            Ipp32s count = rps->getNumberOfPositivePictures() + rps->getNumberOfNegativePictures();
            for(i = 0; i < count; i++)
            {
                if (!pTmp->isLongTermRef() && pTmp->m_PicOrderCnt == pSlice->getPOC() + rps->getDeltaPOC(i))
                {
                    isReferenced = true;
                    pTmp->SetisShortTermRef(true);
                    pTmp->SetisLongTermRef(false);
                }
            }

            count = rps->getNumberOfPictures();
            for( ; i < count; i++)
            {
                if (rps->getCheckLTMSBPresent(i) == true)
                {
                    if (pTmp->m_PicOrderCnt == rps->getPOC(i))
                    {
                        isReferenced = true;
                        pTmp->SetisLongTermRef(true);
                        pTmp->SetisShortTermRef(false);
                    }
                }
                else
                {
                    if ((pTmp->m_PicOrderCnt % (1 << sliceHeader->m_SeqParamSet->log2_max_pic_order_cnt_lsb)) == rps->getPOC(i) % (1 << sliceHeader->m_SeqParamSet->log2_max_pic_order_cnt_lsb))
                    {
                        isReferenced = true;
                        pTmp->SetisLongTermRef(true);
                        pTmp->SetisShortTermRef(false);
                    }
                }
            }

            // mark the picture as "unused for reference" if it is not in
            // the Reference Picture Set
            if(pTmp->m_PicOrderCnt != pSlice->getPOC() && !isReferenced)
            {
                pTmp->SetisShortTermRef(false);
                pTmp->SetisLongTermRef(false);
                DEBUG_PRINT1((VM_STRING("Dereferencing frame with POC %d, busy = %d\n"), pTmp->m_PicOrderCnt, pTmp->GetRefCounter()));
            }
        }
    }    // not IDR picture

    return umcRes;
}

/****************************************************************************************************/
//
/****************************************************************************************************/
//static
//bool IsNeedSPSInvalidate(const H265SeqParamSet *old_sps, const H265SeqParamSet *new_sps)
//{
//    if (!old_sps || !new_sps)
//        return false;
//
//    //if (new_sps->no_output_of_prior_pics_flag)
//      //  return true;
//
//    if (old_sps->pic_width != new_sps->pic_width)
//        return true;
//
//    if (old_sps->pic_height != new_sps->pic_height)
//        return true;
//
//    if (old_sps->max_dec_frame_buffering < new_sps->max_dec_frame_buffering)
//        return true;
//
//    /*if (old_sps->frame_cropping_rect_bottom_offset != new_sps->frame_cropping_rect_bottom_offset)
//        return true;
//
//    if (old_sps->frame_cropping_rect_left_offset != new_sps->frame_cropping_rect_left_offset)
//        return true;
//
//    if (old_sps->frame_cropping_rect_right_offset != new_sps->frame_cropping_rect_right_offset)
//        return true;
//
//    if (old_sps->frame_cropping_rect_top_offset != new_sps->frame_cropping_rect_top_offset)
//        return true;
//
//    if (old_sps->aspect_ratio_idc != new_sps->aspect_ratio_idc)
//        return true; */
//
//    return false;
//}

/****************************************************************************************************/
// MVC_Extension_H265 class routine
/****************************************************************************************************/
MVC_Extension::MVC_Extension()
    : m_temporal_id(7)
    , m_priority_id(63)
    , HighestTid(0)
    , m_level_idc(0)
{
    Reset();
}

MVC_Extension::~MVC_Extension()
{
    Close();
}

UMC::Status MVC_Extension::Init()
{
    MVC_Extension::Close();

    m_view.pDPB.reset(new H265DBPList());
    return UMC::UMC_OK;
}

void MVC_Extension::Close()
{
    MVC_Extension::Reset();
}

void MVC_Extension::Reset()
{
    m_temporal_id = 7;
    m_priority_id = 63;
    m_level_idc = 0;
    HighestTid = 0;

    m_view.Reset();
}

ViewItem_H265 * MVC_Extension::GetView()
{
    return &m_view;
}


/****************************************************************************************************/
// Skipping_H265 class routine
/****************************************************************************************************/
Skipping_H265::Skipping_H265()
    : m_VideoDecodingSpeed(0)
    , m_SkipCycle(1)
    , m_ModSkipCycle(1)
    , m_PermanentTurnOffDeblocking(0)
    , m_SkipFlag(0)
    , m_NumberOfSkippedFrames(0)
{
}

Skipping_H265::~Skipping_H265()
{
}

void Skipping_H265::Reset()
{
    m_VideoDecodingSpeed = 0;
    m_SkipCycle = 0;
    m_ModSkipCycle = 0;
    m_PermanentTurnOffDeblocking = 0;
    m_NumberOfSkippedFrames = 0;
}

void Skipping_H265::PermanentDisableDeblocking(bool disable)
{
    m_PermanentTurnOffDeblocking = disable ? 3 : 0;
}

bool Skipping_H265::IsShouldSkipDeblocking(H265DecoderFrame *)
{
    return (IS_SKIP_DEBLOCKING_MODE_PREVENTIVE || IS_SKIP_DEBLOCKING_MODE_PERMANENT);
}

bool Skipping_H265::IsShouldSkipFrame(H265DecoderFrame * )
{
    return false;
}

void Skipping_H265::ChangeVideoDecodingSpeed(Ipp32s & num)
{
    m_VideoDecodingSpeed += num;

    if (m_VideoDecodingSpeed < 0)
        m_VideoDecodingSpeed = 0;
    if (m_VideoDecodingSpeed > 7)
        m_VideoDecodingSpeed = 7;

    num = m_VideoDecodingSpeed;

    Ipp32s deblocking_off = m_PermanentTurnOffDeblocking;
    if (deblocking_off == 3)
        m_PermanentTurnOffDeblocking = 3;
}

Skipping_H265::SkipInfo Skipping_H265::GetSkipInfo() const
{
    SkipInfo info;
    info.isDeblockingTurnedOff = (m_VideoDecodingSpeed == 5) || (m_VideoDecodingSpeed == 7);
    info.numberOfSkippedFrames = m_NumberOfSkippedFrames;
    return info;
}

/****************************************************************************************************/
// Resources
/****************************************************************************************************/

LocalResources_H265::LocalResources_H265()
    : next_mb_tables(0)
    , m_ppMBIntraTypes(0)
    , m_piMBIntraProp(0)
    , m_numberOfBuffers(0)
    , m_pMemoryAllocator(0)
    , m_parsedDataLength(0)
    , m_pParsedData(0)
    , m_midParsedData(0)
    , m_currentResourceIndex(0)
{
    m_paddedParsedDataSize.width = 0;
    m_paddedParsedDataSize.height = 0;
}

LocalResources_H265::~LocalResources_H265()
{
    Close();
}

UMC::Status LocalResources_H265::Init(Ipp32s numberOfBuffers, UMC::MemoryAllocator *pMemoryAllocator)
{
    Close();

    m_numberOfBuffers = numberOfBuffers;
    m_pMemoryAllocator = pMemoryAllocator;

    next_mb_tables = new H264DecoderMBAddr *[numberOfBuffers + 1];
    return UMC::UMC_OK;
}

void LocalResources_H265::Reset()
{
    m_currentResourceIndex = 0;
}

void LocalResources_H265::Close()
{
    delete[] next_mb_tables;
    next_mb_tables = 0;

    DeallocateBuffers();

    m_numberOfBuffers = 0;
    m_pMemoryAllocator = 0;
    m_currentResourceIndex = 0;
}

Ipp32u LocalResources_H265::GetCurrentResourceIndex()
{
    Ipp32s index = m_currentResourceIndex % (m_numberOfBuffers);
    m_currentResourceIndex++;
    return index;
}

bool LocalResources_H265::LockFrameResource(H265DecoderFrame * )
{
    return true;
}

void LocalResources_H265::UnlockFrameResource(H265DecoderFrame * )
{
}

H265DecoderFrame * LocalResources_H265::IsBusyByFrame(Ipp32s )
{
    return NULL;
}

UMC::Status LocalResources_H265::AllocateBuffers(H265SeqParamSet* sps, bool exactSizeRequested)
{
    UMC::Status      umcRes = UMC::UMC_OK;
    IppiSize desiredPaddedSize;

    IppiSize  size;
    size.width = sps->pic_width_in_luma_samples;
    size.height = sps->pic_height_in_luma_samples;

    desiredPaddedSize.width  = (size.width  + 15) & ~15;
    desiredPaddedSize.height = (size.height + 15) & ~15;

    // If our buffer and internal pointers are already set up for this
    // image size, then there's nothing more to do.
    // But if exactSizeRequested, we need to see if our existing
    // buffer is oversized, and perhaps reallocate it.

    if (m_paddedParsedDataSize.width == desiredPaddedSize.width &&
        m_paddedParsedDataSize.height == desiredPaddedSize.height &&
        !exactSizeRequested)
        return umcRes;

    // Determine how much space we need
    Ipp32s     MB_Frame_Width   = desiredPaddedSize.width >> 4;
    Ipp32s     MB_Frame_Height  = desiredPaddedSize.height >> 4;

    Ipp32s     uMBMapSize   = MB_Frame_Width * MB_Frame_Height;
    Ipp32s     next_mb_size = (Ipp32s)(MB_Frame_Width*MB_Frame_Height*sizeof(H264DecoderMBAddr));

    Ipp32s     totalSize = (m_numberOfBuffers + 1)*next_mb_size + uMBMapSize + 128; // 128 used for alignments

    // Reallocate our buffer if its size is not appropriate.
    if (m_parsedDataLength < totalSize ||
        (exactSizeRequested && (m_parsedDataLength != totalSize)))
    {
        DeallocateBuffers();

        if (m_pMemoryAllocator->Alloc(&m_midParsedData,
                                      totalSize,
                                      UMC::UMC_ALLOC_PERSISTENT))
            return UMC::UMC_ERR_ALLOC;

        m_pParsedData = (Ipp8u *) m_pMemoryAllocator->Lock(m_midParsedData);
        ippsZero_8u(m_pParsedData, totalSize);

        m_parsedDataLength = totalSize;
    }

    // Reassign our internal pointers if need be
    if (m_paddedParsedDataSize.width != desiredPaddedSize.width ||
        m_paddedParsedDataSize.height != desiredPaddedSize.height)
    {
        m_paddedParsedDataSize = desiredPaddedSize;

        size_t     offset = 0;

        m_pMBMap = UMC::align_pointer<Ipp8u *> (m_pParsedData);
        offset += uMBMapSize;

        if (offset & 0x7)
            offset = (offset + 7) & ~7;
        next_mb_tables[0] = UMC::align_pointer<H264DecoderMBAddr *> (m_pParsedData + offset);

        //initialize first table
        for (Ipp32s i = 0; i < uMBMapSize; i++)
            next_mb_tables[0][i] = i + 1; // simple linear scan

        offset += next_mb_size;

        for (Ipp32s i = 1; i <= m_numberOfBuffers; i++)
        {
            if (offset & 0x7)
                offset = (offset + 7) & ~7;

            next_mb_tables[i] = UMC::align_pointer<H264DecoderMBAddr *> (m_pParsedData + offset);
            offset += next_mb_size;
        }
    }

    return umcRes;
}

void LocalResources_H265::DeallocateBuffers()
{
    if (m_pParsedData)
    {
        // Free the old buffer.
        m_pMemoryAllocator->Unlock(m_midParsedData);
        m_pMemoryAllocator->Free(m_midParsedData);
        m_pParsedData = 0;
        m_midParsedData = 0;
    }

    m_parsedDataLength = 0;
    m_paddedParsedDataSize.width = 0;
    m_paddedParsedDataSize.height = 0;
}

/****************************************************************************************************/
// SEI_Storer_H265
/****************************************************************************************************/
SEI_Storer_H265::SEI_Storer_H265()
{
    Reset();
}

SEI_Storer_H265::~SEI_Storer_H265()
{
    Close();
}

void SEI_Storer_H265::Init()
{
    Close();
    m_data.resize(MAX_BUFFERED_SIZE);
    m_payloads.resize(START_ELEMENTS);
    m_offset = 0;
    m_lastUsed = 2;
}

void SEI_Storer_H265::Close()
{
    Reset();
    m_data.clear();
    m_payloads.clear();
}

void SEI_Storer_H265::Reset()
{
    m_lastUsed = 2;
    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        m_payloads[i].isUsed = 0;
    }
}

void SEI_Storer_H265::SetFrame(H265DecoderFrame * frame)
{
    VM_ASSERT(frame);
    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (m_payloads[i].frame == 0 && m_payloads[i].isUsed)
        {
            m_payloads[i].frame = frame;
        }
    }
}

void SEI_Storer_H265::SetTimestamp(H265DecoderFrame * frame)
{
    VM_ASSERT(frame);
    Ipp64f ts = frame->m_dFrameTime;

    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (m_payloads[i].frame == frame)
        {
            m_payloads[i].timestamp = ts;
            if (m_payloads[i].isUsed)
                m_payloads[i].isUsed = m_lastUsed;
        }
    }

    m_lastUsed++;
}

const SEI_Storer_H265::SEI_Message * SEI_Storer_H265::GetPayloadMessage()
{
    SEI_Storer_H265::SEI_Message * msg = 0;

    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (m_payloads[i].isUsed > 1)
        {
            if (!msg || msg->isUsed > m_payloads[i].isUsed)
            {
                msg = &m_payloads[i];
            }
        }
    }

    if (msg)
        msg->isUsed = 0;

    return msg;
}

SEI_Storer_H265::SEI_Message* SEI_Storer_H265::AddMessage(UMC::MediaDataEx *nalUnit, SEI_TYPE type)
{
    size_t sz = nalUnit->GetDataSize();

    if (sz > (m_data.size() >> 2))
        return 0;

    if (m_offset + sz > m_data.size())
    {
        m_offset = 0;
    }

    // clear overwriting messages:
    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (!m_payloads[i].isUsed)
            continue;

        SEI_Message & mmsg = m_payloads[i];

        if ((m_offset + sz > mmsg.offset) &&
            (m_offset < mmsg.offset + mmsg.msg_size))
        {
            m_payloads[i].isUsed = 0;
            return 0;
        }
    }

    size_t freeSlot = 0;
    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (!m_payloads[i].isUsed)
        {
            freeSlot = i;
            break;
        }
    }

    if (m_payloads[freeSlot].isUsed)
    {
        if (m_payloads.size() >= MAX_ELEMENTS)
            return 0;

        m_payloads.push_back(SEI_Message());
        freeSlot = m_payloads.size() - 1;
    }

    m_payloads[freeSlot].msg_size = sz;
    m_payloads[freeSlot].offset = m_offset;
    m_payloads[freeSlot].timestamp = 0;
    m_payloads[freeSlot].frame = 0;
    m_payloads[freeSlot].isUsed = 1;
    m_payloads[freeSlot].data = &(m_data.front()) + m_offset;
    m_payloads[freeSlot].type = type;

    memcpy(&m_data[m_offset], (Ipp8u*)nalUnit->GetDataPointer(), sz);

    m_offset += sz;

    return &m_payloads[freeSlot];
}

ViewItem_H265::ViewItem_H265()
{
    Reset();

} // ViewItem_H265::ViewItem_H265(void)

ViewItem_H265::ViewItem_H265(const ViewItem_H265 &src)
{
    Reset();

    pDPB.reset(src.pDPB.release());
    dpbSize = src.dpbSize;
    sps_max_dec_pic_buffering = src.sps_max_dec_pic_buffering;
    sps_max_num_reorder_pics = src.sps_max_num_reorder_pics;

} // ViewItem_H265::ViewItem_H265(const ViewItem_H265 &src)

ViewItem_H265::~ViewItem_H265()
{
    Close();

} // ViewItem_H265::ViewItem_H265(void)

UMC::Status ViewItem_H265::Init()
{
    // release the object before initialization
    Close();

    try
    {
        // allocate DPB and POC counter
        pDPB.reset(new H265DBPList());
    }
    catch(...)
    {
        return UMC::UMC_ERR_ALLOC;
    }

    // save the ID
    localFrameTime = 0;
    pCurFrame = 0;

    return UMC::UMC_OK;

} // Status ViewItem_H265::Init(Ipp32u view_id)

void ViewItem_H265::Close(void)
{
    // Reset the parameters before close
    Reset();

    if (pDPB.get())
    {
        pDPB.reset();
    }

    dpbSize = 0;
    sps_max_dec_pic_buffering = 1;
    sps_max_num_reorder_pics = 0;

} // void ViewItem_H265::Close(void)

void ViewItem_H265::Reset(void)
{
    if (pDPB.get())
    {
        pDPB->Reset();
    }

    pCurFrame = 0;
    localFrameTime = 0;
    sps_max_dec_pic_buffering = 1;
    sps_max_num_reorder_pics = 0;

} // void ViewItem_H265::Reset(void)

void ViewItem_H265::SetDPBSize(H265SeqParamSet *pSps, Ipp8u & level_idc)
{
    Ipp8u level = level_idc ? level_idc : pSps->level_idc;

    // calculate the new DPB size value

    // FIXME: should have correct temporal layer
    
    dpbSize = pSps->getMaxDecPicBuffering(pSps->getMaxTLayers()-1);

    if (level_idc)
    {
        level_idc = level;
    }
    else
    {
        pSps->level_idc = level;
    }

    // provide the new value to the DPBList
    if (pDPB.get())
    {
        pDPB->SetDPBSize(dpbSize);
    }

} // void ViewItem_H265::SetDPBSize(const H265SeqParamSet *pSps)
/****************************************************************************************************/
// TaskSupplier_H265
/****************************************************************************************************/
TaskSupplier_H265::TaskSupplier_H265()
    : AU_Splitter_H265(&m_Heap, &m_ObjHeap)
    , m_SliceIdxInTaskSupplier(0)
    , m_pSegmentDecoder(0)
    , m_iThreadNum(0)
    , m_use_external_framerate(false)
    , m_pLastSlice(0)
    , m_pLastDisplayed(0)
    , m_pMemoryAllocator(0)
    , m_pFrameAllocator(0)
    , m_DPBSizeEx(0)
    , m_frameOrder(0)
    , m_pTaskBroker(0)
    , m_UIDFrameCounter(0)
    , m_sei_messages(0)
    , m_isInitialized(false)
    , m_isUseDelayDPBValues(true)
{
}

TaskSupplier_H265::~TaskSupplier_H265()
{
    Close();
}

UMC::Status TaskSupplier_H265::Init(UMC::BaseCodecParams *pInit)
{
    UMC::VideoDecoderParams *init = DynamicCast<UMC::VideoDecoderParams> (pInit);

    if (NULL == init)
        return UMC::UMC_ERR_NULL_PTR;

    Close();

    m_DPBSizeEx = 0;

    m_initializationParams = *init;

    Ipp32s nAllowedThreadNumber = init->numThreads;
    if(nAllowedThreadNumber < 0) nAllowedThreadNumber = 0;
    if(nAllowedThreadNumber > 16) nAllowedThreadNumber = 16;

    // calculate number of slice decoders.
    // It should be equal to CPU number
    m_iThreadNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);

    AU_Splitter_H265::Init(init);
    MVC_Extension::Init();

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
    };

    m_pTaskBroker->Init(m_iThreadNum, true);

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

        if (!i)
            continue;

        H265Thread * thread = new H265Thread();
        if (thread->Init(i, m_pSegmentDecoder[i]) != UMC::UMC_OK)
        {
            delete thread;
            return UMC::UMC_ERR_INIT;
        }

        m_threadGroup.AddThread(thread);
    }

    LocalResources_H265::Init(m_iThreadNum, m_pMemoryAllocator);

    m_isUseDelayDPBValues = true;
    m_local_delta_frame_time = 1.0/30;
    m_frameOrder = 0;
    m_use_external_framerate = 0 < init->info.framerate;

    if (m_use_external_framerate)
    {
        m_local_delta_frame_time = 1 / init->info.framerate;
    }

    m_DPBSizeEx = m_iThreadNum;

    m_isInitialized = true;

    return UMC::UMC_OK;
}

UMC::Status TaskSupplier_H265::PreInit(UMC::BaseCodecParams *pInit)
{
    if (m_isInitialized)
        return UMC::UMC_OK;

    UMC::VideoDecoderParams *init = DynamicCast<UMC::VideoDecoderParams> (pInit);

    if (NULL == init)
        return UMC::UMC_ERR_NULL_PTR;

    Close();

    m_DPBSizeEx = 0;

    MVC_Extension::Init();

    Ipp32s nAllowedThreadNumber = init->numThreads;
    if(nAllowedThreadNumber < 0) nAllowedThreadNumber = 0;
    if(nAllowedThreadNumber > 8) nAllowedThreadNumber = 8;
#ifdef _DEBUG
    if (!nAllowedThreadNumber)
        nAllowedThreadNumber = 1;
#endif // _DEBUG

    // calculate number of slice decoders.
    // It should be equal to CPU number
    m_iThreadNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);

    AU_Splitter_H265::Init(init);
    LocalResources_H265::Init(m_iThreadNum, m_pMemoryAllocator);

    m_isUseDelayDPBValues = true;
    m_local_delta_frame_time = 1.0/30;
    m_frameOrder             = 0;
    m_use_external_framerate = 0 < init->info.framerate;

    if (m_use_external_framerate)
    {
        m_local_delta_frame_time = 1 / init->info.framerate;
    }

    m_DPBSizeEx = m_iThreadNum;

    return UMC::UMC_OK;
}

void TaskSupplier_H265::Close()
{
    if (m_pTaskBroker)
    {
        m_pTaskBroker->Release();
    }

// from reset

    m_threadGroup.Release();

    if (GetView()->pDPB.get())
    {
        for (H265DecoderFrame *pFrame = GetView()->pDPB->head(); pFrame; pFrame = pFrame->future())
        {
            pFrame->FreeResources();
        }
    }

    if (m_pSegmentDecoder)
    {
        for (Ipp32u i = 0; i < m_iThreadNum; i += 1)
        {
            delete m_pSegmentDecoder[i];
            m_pSegmentDecoder[i] = 0;
        }
    }

    MVC_Extension::Close();
    AU_Splitter_H265::Close();
    DecReferencePictureMarking_H265::Reset();

    if (m_pLastSlice)
    {
        m_pLastSlice->Release();
        m_ObjHeap.FreeObject(m_pLastSlice);
        m_pLastSlice = 0;
    }

    m_Headers.Reset(false);
    Skipping_H265::Reset();
    m_ObjHeap.Release();
    m_Heap.Reset();

    m_frameOrder               = 0;

    m_WaitForIDR        = true;
    m_isUseDelayDPBValues = true;

    m_pLastDisplayed = 0;

    delete m_sei_messages;
    m_sei_messages = 0;

// from reset

    LocalResources_H265::Close();

    delete[] m_pSegmentDecoder;
    m_pSegmentDecoder = 0;

    delete m_pTaskBroker;
    m_pTaskBroker = 0;

    m_iThreadNum = 0;

    m_DPBSizeEx = 1;

    m_isInitialized = false;

} // void TaskSupplier_H265::Close()

void TaskSupplier_H265::Reset()
{
    if (m_pTaskBroker)
        m_pTaskBroker->Reset();

    m_threadGroup.Reset();

    {
        for (H265DecoderFrame *pFrame = GetView()->pDPB->head(); pFrame; pFrame = pFrame->future())
        {
            pFrame->FreeResources();
        }
    }

    if (m_sei_messages)
        m_sei_messages->Reset();

    MVC_Extension::Reset();
    AU_Splitter_H265::Reset();
    DecReferencePictureMarking_H265::Reset();

    if (m_pLastSlice)
    {
        m_pLastSlice->Release();
        m_ObjHeap.FreeObject(m_pLastSlice);
        m_pLastSlice = 0;
    }

    m_Headers.Reset(true);
    Skipping_H265::Reset();
    m_ObjHeap.Release();
    m_Heap.Reset();

    m_frameOrder               = 0;

    m_WaitForIDR        = true;

    m_isUseDelayDPBValues = true;
    m_pLastDisplayed = 0;

    LocalResources_H265::Reset();

    if (m_pTaskBroker)
        m_pTaskBroker->Init(m_iThreadNum, true);
}

void TaskSupplier_H265::AfterErrorRestore()
{
    if (m_pTaskBroker)
        m_pTaskBroker->Reset();

    m_threadGroup.Reset();

    GetView()->pDPB->Reset();

    MVC_Extension::Reset();
    AU_Splitter_H265::Reset();

    Skipping_H265::Reset();
    m_ObjHeap.Release();
    m_Heap.Reset();
    m_Headers.Reset(true);

    m_pLastDisplayed = 0;

    if (m_pTaskBroker)
        m_pTaskBroker->Init(m_iThreadNum, true);
}

UMC::Status TaskSupplier_H265::GetInfo(UMC::VideoDecoderParams *lpInfo)
{
    ViewItem_H265 &view = *GetView();

    H265SeqParamSet *sps = m_Headers.m_SeqParams.GetCurrentHeader();
    if (!sps)
    {
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    lpInfo->info.clip_info.height = sps->pic_height_in_luma_samples -
        SubHeightC[sps->chroma_format_idc] * (sps->frame_cropping_rect_top_offset + sps->frame_cropping_rect_bottom_offset);

    lpInfo->info.clip_info.width = sps->pic_width_in_luma_samples - SubWidthC[sps->chroma_format_idc] *
        (sps->frame_cropping_rect_left_offset + sps->frame_cropping_rect_right_offset);

    if (0.0 < m_local_delta_frame_time)
    {
        lpInfo->info.framerate = 1.0 / m_local_delta_frame_time;
    }
    else
    {
        lpInfo->info.framerate = 0;
    }

    lpInfo->info.stream_type = UMC::HEVC_VIDEO;

    lpInfo->profile = sps->profile_idc;
    lpInfo->level = sps->level_idc;

    lpInfo->numThreads = m_iThreadNum;
    lpInfo->info.color_format = GetUMCColorFormat_H265(sps->chroma_format_idc);

    lpInfo->info.profile = sps->profile_idc;
    lpInfo->info.level = sps->level_idc;

    if (sps->getAspectRatioIdc() == 255)
    {
        lpInfo->info.aspect_ratio_width  = sps->getSarWidth();
        lpInfo->info.aspect_ratio_height = sps->getSarHeight();
    }

    Ipp32u multiplier = 1 << (6 + sps->getHrdParameters()->getBitRateScale());
    lpInfo->info.bitrate = (sps->getHrdParameters()->getBitRateValue(0, 0, 0) - 1) * multiplier;

    lpInfo->info.interlace_type = UMC::PROGRESSIVE;

    H265VideoDecoderParams *lpH264Info = DynamicCast<H265VideoDecoderParams> (lpInfo);
    if (lpH264Info)
    {
        Ipp8u temp_level_idc = 0;
        view.SetDPBSize(sps, temp_level_idc);
        lpH264Info->m_DPBSize = view.dpbSize + m_DPBSizeEx;

        IppiSize sz;
        sz.width    = sps->pic_width_in_luma_samples;
        sz.height   = sps->pic_height_in_luma_samples;
        lpH264Info->m_fullSize = sz;

        lpH264Info->m_entropy_coding_type = 1;

        lpH264Info->m_cropArea.top = (Ipp16s)(SubHeightC[sps->chroma_format_idc] * sps->frame_cropping_rect_top_offset);
        lpH264Info->m_cropArea.bottom = (Ipp16s)(SubHeightC[sps->chroma_format_idc] * sps->frame_cropping_rect_bottom_offset);
        lpH264Info->m_cropArea.left = (Ipp16s)(SubWidthC[sps->chroma_format_idc] * sps->frame_cropping_rect_left_offset);
        lpH264Info->m_cropArea.right = (Ipp16s)(SubWidthC[sps->chroma_format_idc] * sps->frame_cropping_rect_right_offset);
    }

    return UMC::UMC_OK;
}

H265DecoderFrame *TaskSupplier_H265::GetFreeFrame()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    H265DecoderFrame *pFrame = 0;

    ViewItem_H265 *pView = GetView();
    H265DBPList *pDPB = pView->pDPB.get();

    // Traverse list for next disposable frame
    if (pDPB->countAllFrames() >= pView->dpbSize + m_DPBSizeEx)
        pFrame = pDPB->GetOldestDisposable();

    pDPB->printDPB();

    VM_ASSERT(!pFrame || pFrame->GetRefCounter() == 0);

    // Did we find one?
    if (NULL == pFrame)
    {
        if (pDPB->countAllFrames() >= pView->dpbSize + m_DPBSizeEx)
        {
            return 0;
        }

        // Didn't find one. Let's try to insert a new one
        pFrame = new H265DecoderFrame(m_pMemoryAllocator, &m_Heap, &m_ObjHeap);
        if (NULL == pFrame)
            return 0;

        pDPB->append(pFrame);

        pFrame->m_index = pDPB->GetFreeIndex();
    }

    pFrame->Reset();

    // Set current as not displayable (yet) and not outputted. Will be
    // updated to displayable after successful decode.
    pFrame->unsetWasOutputted();
    pFrame->unSetisDisplayable();
    pFrame->SetSkipped(false);
    pFrame->SetFrameExistFlag(true);
    pFrame->IncrementReference();

    m_UIDFrameCounter++;
    pFrame->m_UID = m_UIDFrameCounter;
    return pFrame;

}

UMC::Status TaskSupplier_H265::DecodeSEI(UMC::MediaDataEx *nalUnit)
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

            /*Ipp32s target_sps =*/ bitStream.ParseSEI(m_Headers.m_SeqParams,
                m_Headers.m_SeqParams.GetCurrentID(), &m_SEIPayLoads);

            if (m_SEIPayLoads.payLoadType == SEI_USER_DATA_REGISTERED_TYPE)
            {
                m_UserData = m_SEIPayLoads;
            }
            else
            {
                m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
            }

        } while (bitStream.More_RBSP_Data());

    } catch(...)
    {
        // nothing to do just catch it
    }

    return UMC::UMC_OK;
}

UMC::Status TaskSupplier_H265::xDecodeVPS(H265Bitstream &bs)
{
    H265VideoParamSet vps;

    UMC::Status s = bs.GetVideoParamSet(&vps);
    if(s == UMC::UMC_OK)
        m_Headers.m_VideoParams.AddHeader(&vps);

    return s;
}

UMC::Status TaskSupplier_H265::xDecodeSPS(H265Bitstream &bs)
{
    H265SeqParamSet sps;
    sps.Reset();

    //sps.m_RPSList = &m_RefPicSetList;
    UMC::Status s = bs.GetSequenceParamSet(&sps);
    if(s == UMC::UMC_OK)
    {
        sps.WidthInCU = (sps.pic_width_in_luma_samples % sps.MaxCUWidth) ? sps.pic_width_in_luma_samples / sps.MaxCUWidth + 1 : sps.pic_width_in_luma_samples / sps.MaxCUWidth;
        sps.HeightInCU = (sps.pic_height_in_luma_samples % sps.MaxCUHeight) ? sps.pic_height_in_luma_samples / sps.MaxCUHeight  + 1 : sps.pic_height_in_luma_samples / sps.MaxCUHeight;
        sps.NumPartitionsInCUSize = 1 << sps.getMaxCUDepth();
        sps.NumPartitionsInCU = 1 << (sps.getMaxCUDepth() << 1);
        sps.NumPartitionsInFrameWidth = sps.WidthInCU * sps.NumPartitionsInCUSize;
        sps.NumPartitionsInFrameHeight = sps.HeightInCU * sps.NumPartitionsInCUSize;

        m_Headers.m_SeqParams.AddHeader(&sps);

        Ipp8u newDPBsize = (Ipp8u)CalculateDPBSize(sps.getPTL()->getGeneralPTL()->getLevelIdc(),
                                   sps.pic_width_in_luma_samples,
                                   sps.pic_height_in_luma_samples);

        HighestTid = sps.getMaxTLayers() - 1;
        sps.setMaxDecPicBuffering(sps.getMaxDecPicBuffering(HighestTid) ? sps.getMaxDecPicBuffering(HighestTid) : newDPBsize, 0);

        if (ViewItem_H265 *view = GetView())
        {
            view->SetDPBSize(&sps, m_level_idc);
            view->sps_max_dec_pic_buffering = sps.getMaxDecPicBuffering(HighestTid) ? sps.getMaxDecPicBuffering(HighestTid) : view->dpbSize;
            view->sps_max_num_reorder_pics = sps.getNumReorderPics(HighestTid);
        }

        m_pNALSplitter->SetSuggestedSize(CalculateSuggestedSize(&sps));
        return s;
    }

    return UMC::UMC_ERR_INVALID_STREAM;
}

UMC::Status TaskSupplier_H265::xDecodePPS(H265Bitstream &bs)
{
    H265PicParamSet pps;

    pps.Reset();
    UMC::Status s = bs.GetPictureParamSetFull(&pps);
    if(s != UMC::UMC_OK)
        return s;

    // Initialize tiles data
    H265SeqParamSet *sps = m_Headers.m_SeqParams.GetHeader(pps.getSPSId());
    VM_ASSERT(sps != 0);
    Ipp32u WidthInLCU = sps->WidthInCU;
    Ipp32u HeightInLCU = sps->HeightInCU;

    pps.m_CtbAddrRStoTS = new Ipp32u[WidthInLCU * HeightInLCU + 1];
    if (NULL == pps.m_CtbAddrRStoTS)
    {
        pps.Reset();
        return UMC::UMC_ERR_ALLOC;
    }

    pps.m_CtbAddrTStoRS = new Ipp32u[WidthInLCU * HeightInLCU + 1];
    if (NULL == pps.m_CtbAddrTStoRS)
    {
        pps.Reset();
        return UMC::UMC_ERR_ALLOC;
    }

    pps.m_TileIdx = new Ipp32u[WidthInLCU * HeightInLCU + 1];
    if (NULL == pps.m_TileIdx)
    {
        pps.Reset();
        return UMC::UMC_ERR_ALLOC;
    }

    if (pps.getTilesEnabledFlag())
    {
        Ipp32u colums = pps.getNumColumns();
        Ipp32u rows = pps.getNumRows();

        Ipp32u *colBd = new Ipp32u[colums];
        if (NULL == colBd)
        {
            pps.Reset();
            return UMC::UMC_ERR_ALLOC;
        }

        Ipp32u *rowBd = new Ipp32u[rows];
        if (NULL == rowBd)
        {
            pps.Reset();
            return UMC::UMC_ERR_ALLOC;
        }

        if (pps.getUniformSpacingFlag())
        {
            Ipp32u lastDiv, i;

            pps.m_ColumnWidthArray = new Ipp32u[colums];
            if (NULL == pps.m_ColumnWidthArray)
            {
                pps.Reset();
                return UMC::UMC_ERR_ALLOC;
            }
            pps.m_RowHeightArray = new Ipp32u[rows];
            if (NULL == pps.m_RowHeightArray)
            {
                pps.Reset();
                return UMC::UMC_ERR_ALLOC;
            }

            lastDiv = 0;
            for (i = 0; i < colums; i++)
            {
                Ipp32u tmp = ((i + 1) * WidthInLCU) / colums;
                pps.m_ColumnWidthArray[i] = tmp - lastDiv;
                lastDiv = tmp;
            }

            lastDiv = 0;
            for (i = 0; i < rows; i++)
            {
                Ipp32u tmp = ((i + 1) * HeightInLCU) / rows;
                pps.m_RowHeightArray[i] = tmp - lastDiv;
                lastDiv = tmp;
            }
        }
        else
        {
            // Initialize last column/row values, all previous values were parsed from PPS header
            Ipp32u i;
            Ipp32u tmp = 0;

            for (i = 0; i < pps.getNumColumns() - 1; i++)
                tmp += pps.getColumnWidth(i);

            pps.m_ColumnWidthArray[pps.getNumColumns() - 1] = WidthInLCU - tmp;

            tmp = 0;
            for (i = 0; i < pps.getNumRows() - 1; i++)
                tmp += pps.getRowHeight(i);

            pps.m_RowHeightArray[pps.getNumRows() - 1] = HeightInLCU - tmp;
        }

        Ipp32u tbX, tbY;
        Ipp32u i, j;

        colBd[0] = 0;
        for (i = 0; i < pps.getNumColumns() - 1; i++)
        {
            colBd[i + 1] = colBd[i] + pps.getColumnWidth(i);
        }

        rowBd[0] = 0;
        for (i = 0; i < pps.getNumRows() - 1; i++)
        {
            rowBd[i + 1] = rowBd[i] + pps.getRowHeight(i);
        }

        tbX = tbY = 0;


        for (i = 0; i < WidthInLCU * HeightInLCU; i++)
        {
            Ipp32u tileX = 0, tileY = 0, CtbAddrRStoTS;

            for (j = 0; j < colums; j++)
            {
                if (tbX >= colBd[j])
                {
                    tileX = j;
                }
            }

            for (j = 0; j < rows; j++)
            {
                if (tbY >= rowBd[j])
                {
                    tileY = j;
                }
            }

            CtbAddrRStoTS = WidthInLCU * rowBd[tileY] + pps.getRowHeight(tileY) * colBd[tileX];
            CtbAddrRStoTS += (tbY - rowBd[tileY]) * pps.getColumnWidth(tileX) + tbX - colBd[tileX];

            pps.m_CtbAddrRStoTS[i] = CtbAddrRStoTS;
            pps.m_TileIdx[i] = tileY * colums + tileX;

            tbX++;
            if (tbX == WidthInLCU)
            {
                tbX = 0;
                tbY++;
            }
        }

        for (i = 0; i < WidthInLCU * HeightInLCU; i++)
        {
            Ipp32u CtbAddrRStoTS = pps.m_CtbAddrRStoTS[i];
            pps.m_CtbAddrTStoRS[CtbAddrRStoTS] = i;
        }

        pps.m_CtbAddrRStoTS[WidthInLCU * HeightInLCU] = WidthInLCU * HeightInLCU;
        pps.m_CtbAddrTStoRS[WidthInLCU * HeightInLCU] = WidthInLCU * HeightInLCU;
        pps.m_TileIdx[WidthInLCU * HeightInLCU] = WidthInLCU * HeightInLCU;

        delete[] colBd;
        delete[] rowBd;
    }
    else
    {
        for (Ipp32u i = 0; i < WidthInLCU * HeightInLCU + 1; i++)
        {
            pps.m_CtbAddrTStoRS[i] = i;
            pps.m_CtbAddrRStoTS[i] = i;
        }
        memset(pps.m_TileIdx, 0, sizeof(Ipp32u) * WidthInLCU * HeightInLCU);
    }

    Ipp32s numberOfTiles = pps.getTilesEnabledFlag() ? pps.num_tile_columns * pps.num_tile_rows : 0;

    pps.tilesInfo.resize(numberOfTiles);

    if (!numberOfTiles)
    {
        pps.tilesInfo.resize(1);
        pps.tilesInfo[0].firstCUAddr = 0;
        pps.tilesInfo[0].endCUAddr = WidthInLCU*HeightInLCU;
    }

    for (Ipp32s i = 0; i < numberOfTiles; i++)
    {
        Ipp32s tileY = i / pps.num_tile_columns;
        Ipp32s tileX = i % pps.num_tile_columns;

        Ipp32s startY = 0;
        Ipp32s startX = 0;

        for (Ipp32s j = 0; j < tileX; j++)
        {
            startX += pps.m_ColumnWidthArray[j];
        }

        for (Ipp32s j = 0; j < tileY; j++)
        {
            startY += pps.m_RowHeightArray[j];
        }

        pps.tilesInfo[i].endCUAddr = (startY + pps.m_RowHeightArray[tileY] - 1)* WidthInLCU + startX + pps.m_ColumnWidthArray[tileX] - 1;
        pps.tilesInfo[i].firstCUAddr = startY * WidthInLCU + startX;
    }

    m_Headers.m_PicParams.AddHeader(&pps);

    return s;
}

UMC::Status TaskSupplier_H265::DecodeHeaders(UMC::MediaDataEx *nalUnit)
{
    //ViewItem_H265 *view = GetView(BASE_VIEW);
    //UMC::Status umcRes = UMC::UMC_OK;

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

        bitStream.Reset((Ipp8u*)pMem->GetPointer(), (Ipp32u)pMem->GetDataSize());

        NalUnitType nal_unit_type;
        Ipp32u temporal_id, reserved_zero_6bits;

        bitStream.GetNALUnitType(nal_unit_type, temporal_id, reserved_zero_6bits);
        VM_ASSERT(0 == reserved_zero_6bits);

        switch(nal_unit_type)
        {
        case NAL_UNIT_VPS:  xDecodeVPS(bitStream);      break;
        case NAL_UNIT_SPS:  xDecodeSPS(bitStream);      break;
        case NAL_UNIT_PPS:  xDecodePPS(bitStream);      break;

        default:
            break;
        }
    }
    catch(const h265_exception & ex)
    {
        return ex.GetStatus();
    }
    catch(...)
    {
        return UMC::UMC_ERR_INVALID_STREAM;
    }

    return UMC::UMC_OK;

}

bool TaskSupplier_H265::IsWantToShowFrame(bool force)
{
    ViewItem_H265 &view = *GetView();

    if ((view.pDPB->countNumDisplayable() > view.sps_max_num_reorder_pics) ||
        force)
    {
        H265DecoderFrame * pTmp;

        pTmp = view.pDPB->findOldestDisplayable(view.dpbSize);
        return !!pTmp;
    }

    return false;
}

void TaskSupplier_H265::PostProcessDisplayFrame(UMC::MediaData *dst, H265DecoderFrame *pFrame)
{
    if (!pFrame || pFrame->post_procces_complete)
        return;

    ViewItem_H265 &view = *GetView();

    pFrame->m_isOriginalPTS = pFrame->m_dFrameTime > -1.0;
    if (pFrame->m_isOriginalPTS)
    {
        view.localFrameTime = pFrame->m_dFrameTime;
    }
    else
    {
        pFrame->m_dFrameTime = view.localFrameTime;
    }

    pFrame->m_frameOrder = m_frameOrder;
    switch (pFrame->m_DisplayPictureStruct_H265)
    {
    case DPS_TOP_BOTTOM_TOP_H265:
    case DPS_BOTTOM_TOP_BOTTOM_H265:
        if (m_initializationParams.lFlags & UMC::FLAG_VDEC_TELECINE_PTS)
        {
            view.localFrameTime += (m_local_delta_frame_time / 2);
        }
        break;

    }

    view.localFrameTime += m_local_delta_frame_time;

    m_frameOrder++;

    pFrame->post_procces_complete = true;

    DEBUG_PRINT((VM_STRING("Outputted POC - %d, busyState - %d, uid - %d, pppp - %d\n"), pFrame->m_PicOrderCnt, pFrame->GetRefCounter(), pFrame->m_UID, pppp++));
    dst->SetTime(pFrame->m_dFrameTime);
}

H265DecoderFrame *TaskSupplier_H265::GetFrameToDisplayInternal(bool force)
{
    H265DecoderFrame *pFrame = GetAnyFrameToDisplay(force);
    return pFrame;
}

H265DecoderFrame *TaskSupplier_H265::GetAnyFrameToDisplay(bool force)
{
    ViewItem_H265 &view = *GetView();
        
    for (;;)
    {
        // show oldest frame
        DEBUG_PRINT1((VM_STRING("GetAnyFrameToDisplay DPB displayable %d, maximum %d, force = %d\n"), view.pDPB->countNumDisplayable(), view.maxDecFrameBuffering, force));
        if (view.pDPB->countNumDisplayable() > view.sps_max_num_reorder_pics || force)
        {
            view.pDPB->countNumDisplayable();
            H265DecoderFrame *pTmp = view.pDPB->findOldestDisplayable(view.dpbSize);

            if (pTmp)
            {
                if (!pTmp->IsFrameExist())
                {
                    pTmp->setWasOutputted();
                    pTmp->setWasDisplayed();
                    continue;
                }

                return pTmp;
            }

            break;
        }
        else
        {
            if (m_isUseDelayDPBValues)
            {
                H265DecoderFrame *pTmp = view.pDPB->findDisplayableByDPBDelay();
                if (pTmp)
                    return pTmp;
            }
            break;
        }
    }

    return 0;
}

void TaskSupplier_H265::PreventDPBFullness()
{
    VM_ASSERT(false);
    AfterErrorRestore();
}

UMC::Status TaskSupplier_H265::CompleteDecodedFrames(H265DecoderFrame ** decoded)
{
    bool existCompleted = false;

    if (decoded)
    {
        *decoded = 0;
    }

    ViewItem_H265 &view = *GetView();
    for (;;) //add all ready to decoding
    {
        bool isOneToAdd = true;
        H265DecoderFrame * frameToAdd = 0;

        for (H265DecoderFrame * frame = view.pDPB->head(); frame; frame = frame->future())
        {
            if (frame->IsFrameExist() && !frame->IsDecoded())
            {
                if (!frame->IsDecodingStarted() && frame->IsFullFrame())
                {
                    if (frameToAdd)
                    {
                        isOneToAdd = false;
                        if (frameToAdd->m_UID < frame->m_UID) // add first with min UID
                            continue;
                    }

                    frameToAdd = frame;
                }

                if (!frame->IsDecodingCompleted())
                {
                    continue;
                }

                DEBUG_PRINT((VM_STRING("Decode POC - %d, uid - %d, \n"), frame->m_PicOrderCnt, frame->m_UID));
                frame->OnDecodingCompleted();
                existCompleted = true;
            }
        }

        if (frameToAdd)
        {
            if (m_pTaskBroker->AddFrameToDecoding(frameToAdd))
            {
                if (decoded)
                {
                    *decoded = frameToAdd;
                }
            }
            else
                break;
        }

        if (isOneToAdd)
            break;
    }

    return existCompleted ? UMC::UMC_OK : UMC::UMC_ERR_NOT_ENOUGH_DATA;
}

UMC::Status TaskSupplier_H265::AddSource(UMC::MediaData * pSource, UMC::MediaData *dst)
{
    UMC::Status umcRes = UMC::UMC_OK;

    CompleteDecodedFrames(0);

    umcRes = AddOneFrame(pSource, dst); // construct frame

    if (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes)
    {
        ViewItem_H265 &view = *GetView();

        // in the following case(s) we can lay on the base view only,
        // because the buffers are managed synchronously.

        // frame is being processed. Wait for asynchronous end of operation.
        if (view.pDPB->IsDisposableExist())
        {
            return UMC::UMC_WRN_INFO_NOT_READY;
        }

        // frame is finished, but still referenced.
        // Wait for asynchronous complete.
        if (view.pDPB->IsAlmostDisposableExist())
        {
            return UMC::UMC_WRN_INFO_NOT_READY;
        }

        // some more hard reasons of frame lacking.
        if (!m_pTaskBroker->IsEnoughForStartDecoding(true))
        {
            if (CompleteDecodedFrames(0) == UMC::UMC_OK)
                return UMC::UMC_WRN_INFO_NOT_READY;

            if (GetFrameToDisplayInternal(true))
                return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

            PreventDPBFullness();
            return UMC::UMC_WRN_INFO_NOT_READY;
        }

        return UMC::UMC_WRN_INFO_NOT_READY;
    }

    return umcRes;
}

UMC::Status TaskSupplier_H265::ProcessNalUnit(UMC::MediaDataEx *nalUnit)
{
    UMC::Status umcRes = UMC::UMC_OK;
    UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();
    NalUnitType unitType = (NalUnitType)pMediaDataEx->values[pMediaDataEx->index];

    switch(unitType)
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
        if (H265Slice * pSlice = DecodeSliceHeader(nalUnit))
            umcRes = AddSlice(pSlice, false);
        break;

    case NAL_UNIT_VPS:
    case NAL_UNIT_SPS:
    case NAL_UNIT_PPS:
        umcRes = DecodeHeaders(nalUnit);
        break;

    case NAL_UNIT_SEI:
        umcRes = DecodeSEI(nalUnit);
        break;

    case NAL_UNIT_ACCESS_UNIT_DELIMITER:
        umcRes = AddSlice(0, false);
        break;

    default:
        break;
    };

    return umcRes;
}

UMC::Status TaskSupplier_H265::AddOneFrame(UMC::MediaData * pSource, UMC::MediaData *dst)
{
    if (m_pLastSlice)
    {
        UMC::Status sts = AddSlice(m_pLastSlice, !pSource);
        if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
            return sts;
    }

    bool is_header_readed = false;

    do
    {
        if (!dst && is_header_readed)
        {
            switch (m_pNALSplitter->CheckNalUnitType(pSource))
            {
            case NAL_UNIT_CODED_SLICE_RASL_N:
            case NAL_UNIT_CODED_SLICE_RADL_N:
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

            case NAL_UNIT_SEI:
                return UMC::UMC_OK;
            }
        }

        UMC::MediaDataEx *nalUnit = m_pNALSplitter->GetNalUnits(pSource);
        if (!nalUnit)
        {
            if (pSource)
            {
                if(!(pSource->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME))
                {
                    VM_ASSERT(!m_pLastSlice);
                    return AddSlice(0, true);
                }

                return is_header_readed && !dst ? UMC::UMC_OK : UMC::UMC_ERR_SYNC;
            }
            else
                return AddSlice(0, true);
        }

        UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();

        for (Ipp32s i = 0; i < (Ipp32s)pMediaDataEx->count; i++, pMediaDataEx->index ++)
        {
            if (!dst)
            {
                switch ((NalUnitType)pMediaDataEx->values[i]) // skip data at DecodeHeader mode
                {
                case NAL_UNIT_CODED_SLICE_RASL_N:
                case NAL_UNIT_CODED_SLICE_RADL_N:
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

                case NAL_UNIT_SEI:
                    if (is_header_readed)
                        return UMC::UMC_OK;
                    continue;
                }
            }

            switch ((NalUnitType)pMediaDataEx->values[i])
            {
            case NAL_UNIT_CODED_SLICE_RASL_N:
            case NAL_UNIT_CODED_SLICE_RADL_N:
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
                if(H265Slice *pSlice = DecodeSliceHeader(nalUnit))
                {
                    UMC::Status sts = AddSlice(pSlice, !pSource);
                    if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
                        return sts;
                }
                break;

            case NAL_UNIT_SEI:
                DecodeSEI(nalUnit);
                break;

            case NAL_UNIT_SEI_SUFFIX:
                break;

            case NAL_UNIT_VPS:
            case NAL_UNIT_SPS:
            case NAL_UNIT_PPS:
                {
                    UMC::Status umsRes = DecodeHeaders(nalUnit);
                    if (umsRes != UMC::UMC_OK)
                    {
                        if (umsRes == UMC::UMC_NTF_NEW_RESOLUTION)
                        {
                            Ipp32s nalIndex = pMediaDataEx->index;
                            Ipp32s size = pMediaDataEx->offsets[nalIndex + 1] - pMediaDataEx->offsets[nalIndex];

                            pSource->MoveDataPointer(- size - 3);
                        }

                        return umsRes;
                    }

                    is_header_readed = true;
                }
                break;

            case NAL_UNIT_ACCESS_UNIT_DELIMITER:
                if (AddSlice(0, !pSource) == UMC::UMC_OK)
                    return UMC::UMC_OK;
                break;

            default:
                break;
            };
        }

    } while ((pSource) && (MINIMAL_DATA_SIZE_H265 < pSource->GetDataSize()));

    if (!pSource)
    {
        return AddSlice(0, true);
    }
    else
    {
        Ipp32u flags = pSource->GetFlags();

        if (!(flags & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME))
        {
            return AddSlice(0, true);
        }
    }

    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
}

H265Slice *TaskSupplier_H265::DecodeSliceHeader(UMC::MediaDataEx *nalUnit)
{
    if ((0 > m_Headers.m_SeqParams.GetCurrentID()) ||
        (0 > m_Headers.m_PicParams.GetCurrentID()))
    {
        return 0;
    }

    H265Slice * pSlice = m_ObjHeap.AllocateObject<H265Slice>();
    pSlice->SetHeap(&m_ObjHeap, &m_Heap);
    pSlice->IncrementReference();

    notifier0<H265Slice> memory_leak_preventing_slice(pSlice, &H265Slice::DecrementReference);

    MemoryPiece memCopy;
    memCopy.SetData(nalUnit);

    MemoryPiece * pMemCopy = &memCopy;

    pMemCopy->SetDataSize(nalUnit->GetDataSize());
    pMemCopy->SetTime(nalUnit->GetTime());

    MemoryPiece * pMem = m_Heap.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);
    notifier1<Heap, MemoryPiece*> memory_leak_preventing(&m_Heap, &Heap::Free, pMem);
    memset(pMem->GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);

    std::vector<Ipp32u> removed_offsets(0);
    SwapperBase * swapper = m_pNALSplitter->GetSwapper();
    swapper->SwapMemory(pMem, pMemCopy, &removed_offsets);

    Ipp32s pps_pid = pSlice->RetrievePicParamSetNumber(pMem->GetPointer(), pMem->GetSize());
    if (pps_pid == -1)
    {
        return 0;
    }

    pSlice->m_pPicParamSet = m_Headers.m_PicParams.GetHeader(pps_pid);
    if (!pSlice->m_pPicParamSet)
    {
        return 0;
    }

    Ipp32s seq_parameter_set_id = pSlice->m_pPicParamSet->seq_parameter_set_id;

    pSlice->m_pSeqParamSet = m_Headers.m_SeqParams.GetHeader(seq_parameter_set_id);
    if (!pSlice->m_pSeqParamSet)
    {
        return 0;
    }

    pSlice->m_pVideoParamSet = m_Headers.m_VideoParams.GetHeader(pSlice->m_pSeqParamSet->sps_video_parameter_set_id);
    if (!pSlice->m_pVideoParamSet)
    {
        return 0;
    }

    m_Headers.m_SeqParams.SetCurrentID(pSlice->m_pPicParamSet->seq_parameter_set_id);
    m_Headers.m_PicParams.SetCurrentID(pSlice->m_pPicParamSet->pic_parameter_set_id);

    ActivateHeaders(const_cast<H265SeqParamSet *>(pSlice->m_pSeqParamSet), const_cast<H265PicParamSet *>(pSlice->m_pPicParamSet));

    pSlice->m_pCurrentFrame = NULL;

    memory_leak_preventing.ClearNotification();
    pSlice->m_pSource = pMem;
    pSlice->m_dTime = pMem->GetTime();

    if (!pSlice->Reset(pMem->GetPointer(), pMem->GetDataSize(), m_iThreadNum))
    {
        return 0;
    }

    if (m_WaitForIDR)
    {
        if (pSlice->GetSliceHeader()->slice_type != I_SLICE)
        {
            return 0;
        }
    }

    Ipp32u currOffset = pSlice->GetSliceHeader()->m_HeaderBitstreamOffset;
    for (Ipp32s tile = 0; tile < pSlice->getTileLocationCount(); tile++)
    {
        pSlice->setTileLocation(tile, pSlice->getTileLocation(tile) + currOffset);
    }

    // Update entry points
    size_t offsets = removed_offsets.size();
    if (pSlice->m_pPicParamSet->getTilesEnabledFlag() && pSlice->getTileLocationCount() > 0 && offsets > 0)
    {
        Ipp32u removed_bytes = 0;
        std::vector<Ipp32u>::iterator it = removed_offsets.begin();
        Ipp32u offset = *it;

        for (Ipp32s tile = 0; tile < (Ipp32s)pSlice->getTileLocationCount(); tile++)
        {
            while (pSlice->getTileLocation(tile) > offset && removed_bytes < offsets)
            {
                removed_bytes++;
                if (removed_bytes < offsets)
                {
                    it++;
                    offset = *it;
                }
                else
                    break;
            }

            pSlice->setTileLocation(tile, pSlice->getTileLocation(tile) - removed_bytes);
        }
    }

    m_WaitForIDR = false;
    memory_leak_preventing_slice.ClearNotification();

    //for SliceIdx m_SliceIdx m_iNumber
    pSlice->m_iNumber = m_SliceIdxInTaskSupplier;
    m_SliceIdxInTaskSupplier++;
    return pSlice;
}

void TaskSupplier_H265::ActivateHeaders(H265SeqParamSet *sps, H265PicParamSet *pps)
{
    pps->MinCUDQPSize = sps->MaxCUWidth >> pps->diff_cu_qp_delta_depth;

    for (Ipp32u i = 0; i < sps->MaxCUDepth - g_AddCUDepth; i++)
    {
        sps->m_AMPAcc[i] = sps->amp_enabled_flag;
    }
    for (Ipp32u i = sps->MaxCUDepth - g_AddCUDepth; i < sps->MaxCUDepth; i++)
    {
        sps->m_AMPAcc[i] = 0;
    }
}

UMC::Status TaskSupplier_H265::AddSlice(H265Slice * pSlice, bool )
{
    m_pLastSlice = 0;

    if (!pSlice) // complete frame
    {
        UMC::Status umcRes = UMC::UMC_ERR_NOT_ENOUGH_DATA;

        if (NULL == GetView()->pCurFrame)
        {
            return umcRes;
        }

        CompleteFrame(GetView()->pCurFrame);

        OnFullFrame(GetView()->pCurFrame);
        GetView()->pCurFrame = NULL;
        umcRes = UMC::UMC_OK;
        return umcRes;
    }

    ViewItem_H265 &view = *GetView();
    view.SetDPBSize(const_cast<H265SeqParamSet*>(pSlice->m_pSeqParamSet), m_level_idc);
    view.sps_max_dec_pic_buffering = pSlice->m_pSeqParamSet->getMaxDecPicBuffering(pSlice->getTLayer()) ?
                                    pSlice->m_pSeqParamSet->getMaxDecPicBuffering(pSlice->getTLayer()) :
                                    view.dpbSize;

    view.sps_max_num_reorder_pics = pSlice->m_pSeqParamSet->getNumReorderPics(HighestTid);

    H265DecoderFrame * pFrame = view.pCurFrame;

    if (pFrame)
    {
        H265Slice * pFirstFrameSlice = pFrame->GetAU()->GetAnySlice();
        VM_ASSERT(pFirstFrameSlice);

        if (pSlice->getDependentSliceSegmentFlag())
        {
            pSlice->CopyFromBaseSlice(pFirstFrameSlice);
        }

        // if the slices belong to different AUs,
        // close the current AU and start new one.
        if ((false == IsPictureTheSame(pFirstFrameSlice, pSlice)))
        {
            CompleteFrame(view.pCurFrame);
            OnFullFrame(view.pCurFrame);
            view.pCurFrame = NULL;
            m_pLastSlice = pSlice;
            return UMC::UMC_OK;
        }
    }
    // there is no free frames.
    // try to allocate a new frame.
    else
    {
        // allocate a new frame, initialize it with slice's parameters.
        pFrame = AllocateNewFrame(pSlice);
        if (!pFrame)
        {
            view.pCurFrame = NULL;
            m_pLastSlice = pSlice;
            return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
        }

        // set the current being processed frame
        view.pCurFrame = pFrame;
    }

    // add the next slice to the initialized frame.
    pSlice->m_pCurrentFrame = pFrame;
    AddSliceToFrame(pFrame, pSlice);

    if (pSlice->m_SliceHeader.slice_type != I_SLICE)
    {
        Ipp32u NumShortTermRefs, NumLongTermRefs;
        view.pDPB->countActiveRefs(NumShortTermRefs, NumLongTermRefs);

        if (NumShortTermRefs + NumLongTermRefs == 0)
            AddFakeReferenceFrame(pSlice);
    }

    //if (!pSlice->getDependentSliceSegmentFlag())
    {
        // pSlice->checkCRA(pSlice->getRPS(), m_pocCRA, m_prevRAPisBLA, m_cListPic);
        // Set reference list
        pSlice->UpdateReferenceList(GetView()->pDPB.get());
    }

    pSlice->setRefPOCList();

    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
}

void TaskSupplier_H265::AddFakeReferenceFrame(H265Slice *)
{
// need to add absent ref frame logic
#if 0
    H265DecoderFrame *pFrame = GetFreeFrame();
    if (!pFrame)
        return;

    UMC::Status umcRes = InitFreeFrame(pFrame, pSlice);
    if (umcRes != UMC::UMC_OK)
    {
        return;
    }

    umcRes = AllocateFrameData(pFrame, pFrame->lumaSize(), pFrame->m_bpp, pSlice->m_pSeqParamSet, pSlice->m_pPicParamSet);
    if (umcRes != UMC::UMC_OK)
    {
        return;
    }

    pFrame->SetisShortTermRef(true);

    pFrame->SetSkipped(true);
    pFrame->SetFrameExistFlag(false);
    pFrame->SetisDisplayable();

    pFrame->DefaultFill(2, false);

    H265SliceHeader* sliceHeader = pSlice->GetSliceHeader();
    ViewItem_H265 &view = *GetView();

    view.pPOCDec->DecodePictureOrderCountFakeFrames(pFrame, sliceHeader);
#endif
}

void TaskSupplier_H265::OnFullFrame(H265DecoderFrame * pFrame)
{
    pFrame->SetFullFrame(true);

    if (pFrame->GetAU()->GetSlice(0)->GetSliceHeader()->IdrPicFlag && !(pFrame->GetError() & UMC::ERROR_FRAME_DPB))
    {
        DecReferencePictureMarking_H265::ResetError();
    }

    if (DecReferencePictureMarking_H265::GetDPBError())
    {
        pFrame->SetErrorFlagged(UMC::ERROR_FRAME_DPB);
    }
}

void TaskSupplier_H265::CompleteFrame(H265DecoderFrame * pFrame)
{
    if (!pFrame)
        return;

    H265DecoderFrameInfo * slicesInfo = pFrame->GetAU();

    if (slicesInfo->GetStatus() > H265DecoderFrameInfo::STATUS_NOT_FILLED)
        return;

    DEBUG_PRINT((VM_STRING("Complete frame POC - (%d) type - %d, count - %d, m_uid - %d, IDR - %d\n"), pFrame->m_PicOrderCnt, pFrame->m_FrameType, slicesInfo->GetSliceCount(), pFrame->m_UID, slicesInfo->GetAnySlice()->GetSliceHeader()->IdrPicFlag));

    //DPBUpdate(slicesInfo->GetAnySlice());

    pFrame->m_iResourceNumber = LocalResources_H265::GetCurrentResourceIndex();

    // skipping algorithm
    {
        if (IsShouldSkipFrame(pFrame))
        {
            pFrame->GetAU()->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);

            pFrame->SetisShortTermRef(false);
            pFrame->SetisLongTermRef(false);
            pFrame->SetSkipped(true);
            pFrame->OnDecodingCompleted();
            return;
        }
        else
        {
            if (IsShouldSkipDeblocking(pFrame))
            {
                pFrame->GetAU()->SkipDeblocking();
                pFrame->GetAU()->SkipSAO();
            }
        }
    }

    Ipp32s count = slicesInfo->GetSliceCount();

    H265Slice * pFirstSlice = 0;
    for (Ipp32s j = 0; j < count; j ++)
    {
        H265Slice * pSlice = slicesInfo->GetSlice(j);
        if (!pFirstSlice || pSlice->m_iFirstMB < pFirstSlice->m_iFirstMB)
        {
            pFirstSlice = pSlice;
        }
    }

    if (pFirstSlice->m_iFirstMB)
    {
        m_pSegmentDecoder[0]->RestoreErrorRect(0, pFirstSlice->m_iFirstMB, pFirstSlice);
    }

    for (Ipp32s i = 0; i < count; i ++)
    {
        H265Slice * pCurSlice = slicesInfo->GetSlice(i);

#define MAX_MB_NUMBER 0x7fffffff

        Ipp32s minFirst = MAX_MB_NUMBER;
        for (Ipp32s j = 0; j < count; j ++)
        {
            H265Slice * pSlice = slicesInfo->GetSlice(j);
            if (pSlice->m_iFirstMB > pCurSlice->m_iFirstMB && minFirst > pSlice->m_iFirstMB)
            {
                minFirst = pSlice->m_iFirstMB;
            }
        }

        if (minFirst != MAX_MB_NUMBER)
        {
            pCurSlice->m_iMaxMB = minFirst;
        }
    }

    slicesInfo->SetStatus(H265DecoderFrameInfo::STATUS_FILLED);
}

UMC::Status TaskSupplier_H265::InitFreeFrame(H265DecoderFrame * pFrame, const H265Slice *pSlice)
{
    UMC::Status umcRes = UMC::UMC_OK;
    const H265SeqParamSet *pSeqParam = pSlice->GetSeqParam();

//    Ipp32s iMBCount = pSeqParam->frame_width_in_mbs * pSeqParam->frame_height_in_mbs;
    //Ipp32s iCUCount = pSeqParam->WidthInCU * pSeqParam->HeightInCU;
    //pFrame->m_CodingData->m_NumCUsInFrame = iCUCount;

    pFrame->m_FrameType = SliceTypeToFrameType(pSlice->GetSliceHeader()->slice_type);
    pFrame->m_dFrameTime = pSlice->m_dTime;
    pFrame->m_crop_left = SubWidthC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_left_offset;
    pFrame->m_crop_right = SubWidthC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_right_offset;
    pFrame->m_crop_top = SubHeightC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_top_offset;
    pFrame->m_crop_bottom = SubHeightC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_bottom_offset;
    pFrame->m_crop_flag = pSeqParam->frame_cropping_flag;

    pFrame->m_aspect_width  = pSeqParam->getSarWidth();
    pFrame->m_aspect_height = pSeqParam->getSarHeight();

    Ipp32s chroma_format_idc = pSeqParam->chroma_format_idc;

    Ipp8u bit_depth_luma, bit_depth_chroma;
    bit_depth_luma = (Ipp8u) pSeqParam->bit_depth_luma;
    bit_depth_chroma = (Ipp8u) pSeqParam->bit_depth_chroma;

    Ipp32s bit_depth = IPP_MAX(bit_depth_luma, bit_depth_chroma);

//    Ipp32s iMBWidth = pSeqParam->frame_width_in_mbs;
    //Ipp32s iCUWidth = pSeqParam->WidthInCU;
//    Ipp32s iMBHeight = pSeqParam->frame_height_in_mbs;
    //Ipp32s iCUHeight = pSeqParam->HeightInCU;
//    IppiSize dimensions = {iMBWidth * 16, iMBHeight * 16};
    //IppiSize dimensions = {iCUWidth * pSeqParam->MaxCUWidth, iCUHeight * pSeqParam->MaxCUHeight};
    IppiSize dimensions = {pSeqParam->pic_width_in_luma_samples, pSeqParam->pic_height_in_luma_samples};

    UMC::ColorFormat cf = GetUMCColorFormat_H265(chroma_format_idc);

    if (cf == UMC::YUV420) //  msdk !!!
        cf = UMC::NV12;

    UMC::VideoDataInfo info;
    info.Init(dimensions.width, dimensions.height, cf, bit_depth);

    pFrame->Init(&info);

    return umcRes;
}

UMC::Status TaskSupplier_H265::AllocateFrameData(H265DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, const H265SeqParamSet* pSeqParamSet, const H265PicParamSet *pPicParamSet)
{
    UMC::ColorFormat color_format = pFrame->GetColorFormat();
        //(ColorFormat) pSeqParamSet->chroma_format_idc;
    UMC::VideoDataInfo info;
    info.Init(dimensions.width, dimensions.height, color_format, bit_depth);

    UMC::FrameMemID frmMID;
    UMC::Status sts = m_pFrameAllocator->Alloc(&frmMID, &info, 0);

    if (sts != UMC::UMC_OK)
    {
        throw h265_exception(UMC::UMC_ERR_ALLOC);
    }

    const UMC::FrameData *frmData = m_pFrameAllocator->Lock(frmMID);

    if (!frmData)
        throw h265_exception(UMC::UMC_ERR_LOCK);

    pFrame->allocate(frmData, &info);
    pFrame->m_index = frmMID;

    if (pFrame->m_CodingData == NULL)
    {
        pFrame->allocateCodingData(pSeqParamSet);
    }

    pFrame->m_CodingData->m_CUOrderMap = pPicParamSet->m_CtbAddrTStoRS;
    pFrame->m_CodingData->m_InverseCUOrderMap = pPicParamSet->m_CtbAddrRStoTS;
    pFrame->m_CodingData->m_TileIdxMap = pPicParamSet->m_TileIdx;

    if (pSeqParamSet->sample_adaptive_offset_enabled_flag)
    {
        Ipp32s size = pFrame->m_CodingData->m_WidthInCU * pFrame->m_CodingData->m_HeightInCU;
        if (pFrame->m_sizeOfSAOData < size)
        {
            if (pFrame->m_sizeOfSAOData != 0)
            {
                delete[] pFrame->m_saoLcuParam[0];
                pFrame->m_saoLcuParam[0] = NULL;

                delete[] pFrame->m_saoLcuParam[1];
                pFrame->m_saoLcuParam[1] = NULL;

                delete[] pFrame->m_saoLcuParam[2];
                pFrame->m_saoLcuParam[2] = NULL;
            }

            pFrame->m_saoLcuParam[0] = new SAOLCUParam[size];
            pFrame->m_saoLcuParam[1] = new SAOLCUParam[size];
            pFrame->m_saoLcuParam[2] = new SAOLCUParam[size];

            pFrame->m_sizeOfSAOData = size;
        }
    }

    return UMC::UMC_OK;
}

H265DecoderFrame * TaskSupplier_H265::AllocateNewFrame(const H265Slice *pSlice)
{
    H265DecoderFrame *pFrame = NULL;

    if (!pSlice)
    {
        return NULL;
    }

    DPBUpdate(pSlice);

    pFrame = GetFreeFrame();
    if (NULL == pFrame)
    {
        return NULL;
    }

    pFrame->SetisShortTermRef(true);

    UMC::Status umcRes = InitFreeFrame(pFrame, pSlice);
    if (umcRes != UMC::UMC_OK)
    {
        return 0;
    }

    //umcRes = AllocateFrameData(pFrame, pFrame->lumaSize(), pFrame->m_bpp, pFrame->GetColorFormat());
    umcRes = AllocateFrameData(pFrame, pFrame->lumaSize(), pFrame->m_bpp, pSlice->m_pSeqParamSet, pSlice->m_pPicParamSet);
    if (umcRes != UMC::UMC_OK)
    {
        return 0;
    }

    if (m_UserData.user_data.size())
    {
        pFrame->m_UserData = m_UserData;
        m_UserData.user_data.clear();
    }

    if (m_sei_messages)
    {
        m_sei_messages->SetFrame(pFrame);
    }

    H265SEIPayLoad * payload = m_Headers.m_SEIParams.GetHeader(SEI_PIC_TIMING_TYPE);
    if (payload && pSlice->GetSeqParam()->getFrameFieldInfoPresentFlag())
    {
        pFrame->m_DisplayPictureStruct_H265 = payload->SEI_messages.pic_timing.pic_struct;
    }
    else
    {
        pFrame->m_DisplayPictureStruct_H265 = DPS_FRAME_H265;
    }

    if (payload)
    {
        pFrame->m_dpb_output_delay = payload->SEI_messages.pic_timing.dpb_output_delay;
        m_isUseDelayDPBValues = m_isUseDelayDPBValues ? (pFrame->m_dpb_output_delay == 0) : false;
    }

    //fill chroma planes in case of 4:0:0
    if (pFrame->m_chroma_format == 0)
    {
        pFrame->DefaultFill(2, true);
    }

    InitFrameCounter(pFrame, pSlice);
    return pFrame;

} // H265DecoderFrame * TaskSupplier_H265::AllocateNewFrame(const H265Slice *pSlice)

void TaskSupplier_H265::InitFrameCounter(H265DecoderFrame * pFrame, const H265Slice *pSlice)
{
    const H265SliceHeader *sliceHeader = pSlice->GetSliceHeader();
    ViewItem_H265 &view = *GetView();

    if (sliceHeader->IdrPicFlag)
    {
        view.pDPB->IncreaseRefPicListResetCount(pFrame);
    }

    pFrame->setPicOrderCnt(sliceHeader->pic_order_cnt_lsb);

    DEBUG_PRINT((VM_STRING("Init frame POC - %d, uid - %d\n"), pFrame->m_PicOrderCnt, pFrame->m_UID));

    pFrame->InitRefPicListResetCount();

} // void TaskSupplier_H265::InitFrameCounter(H265DecoderFrame * pFrame, const H265Slice *pSlice)

void TaskSupplier_H265::AddSliceToFrame(H265DecoderFrame *pFrame, H265Slice *pSlice)
{
    if (pFrame->m_FrameType < SliceTypeToFrameType(pSlice->GetSliceHeader()->slice_type))
        pFrame->m_FrameType = SliceTypeToFrameType(pSlice->GetSliceHeader()->slice_type);

    H265DecoderFrameInfo * au_info = pFrame->GetAU();
    Ipp32s iSliceNumber = au_info->GetSliceCount() + 1;

    pFrame->m_iNumberOfSlices++;

    pSlice->SetSliceNumber(iSliceNumber);
    pSlice->m_pCurrentFrame = pFrame;
    au_info->AddSlice(pSlice);
}

void TaskSupplier_H265::DPBUpdate(const H265Slice * slice)
{
    ViewItem_H265 &view = *GetView();
    DecReferencePictureMarking_H265::UpdateRefPicMarking(view, slice);
}

H265DecoderFrame * TaskSupplier_H265::FindSurface(UMC::FrameMemID id)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    H265DecoderFrame *pFrame = GetView()->pDPB->head();
    for (; pFrame; pFrame = pFrame->future())
    {
        if (pFrame->GetFrameData()->GetFrameMID() == id)
            return pFrame;
    }

    return 0;
}

UMC::Status TaskSupplier_H265::RunDecoding()
{
    CompleteDecodedFrames(0);

    H265DecoderFrame *pFrame = GetView()->pDPB->head();

    for (; pFrame; pFrame = pFrame->future())
    {
        if (!pFrame->IsDecodingCompleted())
        {
            break;
        }
    }

    m_pTaskBroker->Start();

    if (!pFrame)
        return UMC::UMC_OK;

    //DEBUG_PRINT((VM_STRING("Decode POC - %d\n"), pFrame->m_PicOrderCnt[0]));

    return UMC::UMC_OK;
}

UMC::Status TaskSupplier_H265::RunDecodingAndWait(bool should_additional_check, H265DecoderFrame ** decoded)
{
    H265DecoderFrame * outputFrame = *decoded;
    ViewItem_H265 &view = *GetView();

    UMC::Status umcRes = UMC::UMC_OK;

    if (!outputFrame->IsDecodingStarted())
        return UMC::UMC_ERR_FAILED;

    if (outputFrame->IsDecodingCompleted())
        return UMC::UMC_OK;

    for(;;)
    {
        m_pTaskBroker->WaitFrameCompletion();

        if (outputFrame->IsDecodingCompleted())
        {
            if (!should_additional_check)
                break;

            UMC::AutomaticUMCMutex guard(m_mGuard);

            Ipp32s count = 0;
            Ipp32s notDecoded = 0;
            for (H265DecoderFrame * pTmp = view.pDPB->head(); pTmp; pTmp = pTmp->future())
            {
                if (!pTmp->m_isShortTermRef &&
                    !pTmp->m_isLongTermRef &&
                    ((pTmp->m_wasOutputted != 0) || (pTmp->m_isDisplayable == 0)))
                {
                    count++;
                    break;
                }

                if (!pTmp->IsDecoded() && !pTmp->IsDecodingCompleted() && pTmp->IsFullFrame())
                    notDecoded++;
            }

            if (count || (view.pDPB->countAllFrames() < view.dpbSize + m_DPBSizeEx))
                break;

            if (!notDecoded)
                break;
        }
    }

    VM_ASSERT(outputFrame->IsDecodingCompleted());

    return umcRes;
}

UMC::Status TaskSupplier_H265::GetUserData(UMC::MediaData * pUD)
{
    if(!pUD)
        return UMC::UMC_ERR_NULL_PTR;

    if (!m_pLastDisplayed)
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;

    if (m_pLastDisplayed->m_UserData.user_data.size() && m_pLastDisplayed->m_UserData.payLoadSize &&
        m_pLastDisplayed->m_UserData.payLoadType == SEI_USER_DATA_REGISTERED_TYPE)
    {
        pUD->SetTime(m_pLastDisplayed->m_dFrameTime);
        pUD->SetBufferPointer(&m_pLastDisplayed->m_UserData.user_data[0],
            m_pLastDisplayed->m_UserData.payLoadSize);
        pUD->SetDataSize(m_pLastDisplayed->m_UserData.payLoadSize);
        //m_pLastDisplayed->m_UserData.Reset();
        return UMC::UMC_OK;
    }

    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
}

bool TaskSupplier_H265::IsShouldSuspendDisplay()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (m_iThreadNum <= 1)
        return true;

    ViewItem_H265 &view = *GetView();

    if (view.pDPB->GetDisposable() || view.pDPB->countAllFrames() < view.dpbSize + m_DPBSizeEx)
        return false;

    return true;
}

Ipp32u GetLevelIDCIndex(Ipp32u level_idc)
{
    Ipp32u levelIndexArray[] = {
        H265VideoDecoderParams::H265_LEVEL_1,
        H265VideoDecoderParams::H265_LEVEL_2,
        H265VideoDecoderParams::H265_LEVEL_21,

        H265VideoDecoderParams::H265_LEVEL_3,
        H265VideoDecoderParams::H265_LEVEL_31,

        H265VideoDecoderParams::H265_LEVEL_4,
        H265VideoDecoderParams::H265_LEVEL_41,

        H265VideoDecoderParams::H265_LEVEL_5,
        H265VideoDecoderParams::H265_LEVEL_51,
        H265VideoDecoderParams::H265_LEVEL_52,

        H265VideoDecoderParams::H265_LEVEL_6,
        H265VideoDecoderParams::H265_LEVEL_61,
        H265VideoDecoderParams::H265_LEVEL_62
    };

    for (Ipp32u i = 0; i < sizeof(levelIndexArray)/sizeof(levelIndexArray[0]); i++)
    {
        if (levelIndexArray[i] == level_idc)
            return i;
    }

    return sizeof(levelIndexArray)/sizeof(levelIndexArray[0]) - 1;
}

// can increase level_idc to hold num_ref_frames
Ipp32s __CDECL CalculateDPBSize(Ipp32u level_idc, Ipp32s width, Ipp32s height)
{
    Ipp32u lumaPsArray[] = {36864, 122880, 245760, 552960, 983040, 2228224, 2228224, 8912896, 8912896, 8912896, 35651584, 35651584, 35651584};
    Ipp32u index = GetLevelIDCIndex(level_idc);

    Ipp32u MaxLumaPs = lumaPsArray[index];
    Ipp32u maxDpbPicBuf = 6;

    Ipp32u PicSizeInSamplesY = width * height;
    Ipp32u MaxDpbSize = 16;

    if (PicSizeInSamplesY  <=  (MaxLumaPs  >>  2 ))
        MaxDpbSize = IPP_MIN(4 * maxDpbPicBuf, 16);
    else if (PicSizeInSamplesY  <=  (MaxLumaPs  >>  1 ))
        MaxDpbSize = IPP_MIN(2 * maxDpbPicBuf, 16);
    else if (PicSizeInSamplesY  <=  ((3 * MaxLumaPs)  >>  2 ))
        MaxDpbSize = IPP_MIN((4 * maxDpbPicBuf) / 3, 16);
    else
        MaxDpbSize = maxDpbPicBuf;

    return MaxDpbSize;
}


} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
