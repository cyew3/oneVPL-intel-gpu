// Copyright (c) 2003-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "avc_controller.h"
#include "umc_h264_frame_list.h"
#include "umc_h264_nal_spl.h"
#include "umc_h264_bitstream.h"

#include "umc_h264_dec_defs_dec.h"
#include "umc_h264_dec_mfx.h"

#include "umc_h264_task_broker.h"
#include "umc_structures.h"

#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "vm_sys_info.h"

namespace UMC
{

class MFXFrame : public UMC::H264DecoderFrameExtension
{
public:

    MFXFrame(mfxVideoParam &videoPar, MemoryAllocator *pMemoryAllocator, H264_Heap * heap, H264_Heap_Objects * pObjHeap)
        : H264DecoderFrameExtension(pMemoryAllocator, heap, pObjHeap)
    {
        m_CUC.Init();
        m_CUC.AllocateFrameSurface(videoPar);
        m_CUC.AllocSlicesAndMbs(((videoPar.mfx.FrameInfo.Width + 15) >> 4) *
            ((videoPar.mfx.FrameInfo.Height + 15) >> 4));
        m_CUC.AllocExtBuffers(m_CUC.Extract().FrameSurface->Info, videoPar.mfx.FrameInfo.Width);

        m_RefCUC.RefList[0] = new AvcReferenceParamSlice[256];
        m_RefCUC.RefList[1] = new AvcReferenceParamSlice[256];
    }

    ~MFXFrame()
    {
        m_CUC.Close();
        delete[] m_RefCUC.RefList[0];
        delete[] m_RefCUC.RefList[1];
    }

    mfxFrameCUC * GetCUC()
    {
        return &(m_CUC.Extract());
    }

    mfxFrameReferenceCUC  * GetRefCUC()
    {
        return &m_RefCUC;
    }

    bool IsFrameCompleted() const
    {
        if (!GetAU(0)->IsCompleted())
            return false;

        H264DecoderFrameInfo::FillnessStatus status1 = GetAU(1)->GetStatus();

        bool ret;
        switch (status1)
        {
        case H264DecoderFrameInfo::STATUS_NOT_FILLED:
            ret = false;
            break;
        case H264DecoderFrameInfo::STATUS_COMPLETED:
            ret = true;
            break;
        default:
            ret = GetAU(1)->IsCompleted();
            break;
        }

        return ret;
    }

    MfxFrameCucHolder m_CUC;

    mfxFrameReferenceCUC m_RefCUC;
};

#if 1
#include <cstdarg>
inline
void Trace1(vm_char * format, ...)
{
    va_list(arglist);
    va_start(arglist, format);

    vm_char cStr[256];
    vm_string_vsprintf(cStr, format, arglist);

    //OutputDebugString(cStr);
    vm_string_printf(VM_STRING("controller - %s"), cStr);
    //fflush(stdout);
}

#if defined _DEBUG
#define DEBUG_PRINT(x) Trace1 x
#else
#define DEBUG_PRINT(x)
#endif

#else
#define DEBUG_PRINT(x)
#endif

void OnAVCSlideWindow(H264DecoderFrame *pRefFrame, H264DecoderFrame *pCurrentFrame, NotifiersChain * defaultChain, bool force)
{
    if (!pRefFrame)
        return;

    if (!pRefFrame->IsFrameExist())
    {
        pRefFrame->setWasOutputted();
        return;
    }

    if (force || (!pRefFrame->isShortTermRef() && !pRefFrame->isLongTermRef()))
    {
        NotifiersChain *notif = defaultChain;

        if (pCurrentFrame && pCurrentFrame->GetBusyState() >= BUSY_STATE_NOT_DECODED)
        {
            VM_ASSERT(pCurrentFrame);
            notif = pCurrentFrame->GetNotifiersChain();
        }

        if (notif)
        {
            pRefFrame->IncrementBusyState();
            void * buf = notif->GetObjHeap()->Allocate ((notifier0<H264DecoderFrame> *) 0);
            notifier_base * notifier = new(buf) notifier0<H264DecoderFrame> (pRefFrame, &H264DecoderFrame::DecrementBusyState);
            //notifier_base * notifier = new notifier0<H264DecoderFrame> (pRefFrame, &H264DecoderFrame::DecrementBusyState);
            notif->AddNotifier(notifier);
        }
    }
}

bool CutPlanes(H264DecoderFrame * , H264SeqParamSet * );

/****************************************************************************************************/
// Skipping class routine
/****************************************************************************************************/
AVCSkipping::AVCSkipping()
    : m_VideoDecodingSpeed(0)
    , m_SkipCycle(1)
    , m_ModSkipCycle(1)
    , m_SkipFlag(0)
    , m_PermanentTurnOffDeblocking(0)
    , m_NumberOfSkippedFrames(0)
{
}

void AVCSkipping::Reset()
{
    m_VideoDecodingSpeed = 0;
    m_SkipCycle = 0;
    m_ModSkipCycle = 0;
    m_PermanentTurnOffDeblocking = 0;
    m_NumberOfSkippedFrames = 0;
}

bool AVCSkipping::IsShouldSkipDeblocking(H264DecoderFrame * pFrame, Ipp32s field)
{
    return (IS_SKIP_DEBLOCKING_MODE_PERMANENT ||
        (IS_SKIP_DEBLOCKING_MODE_NON_REF && !pFrame->GetAU(field)->IsReference()));
}

bool AVCSkipping::IsShouldSkipFrame(H264DecoderFrame * pFrame, Ipp32s /*field*/)
{
    bool isShouldSkip = false;
    bool isReference0 = pFrame->GetAU(0)->IsReference();
    bool isReference1 = pFrame->GetAU(1)->IsReference();

    bool isReference = isReference0 || isReference1;

    if ((m_VideoDecodingSpeed > 0) && !isReference)
    {
        if ((m_SkipFlag % m_ModSkipCycle) == 0)
        {
            isShouldSkip = true;
        }

        m_SkipFlag++;

        if (m_SkipFlag >= m_SkipCycle)
            m_SkipFlag = 0;
    }

    if (isShouldSkip)
        m_NumberOfSkippedFrames++;

    return isShouldSkip;
}

void AVCSkipping::ChangeVideoDecodingSpeed(Ipp32s & num)
{
    m_VideoDecodingSpeed += num;

    if (m_VideoDecodingSpeed < 0)
        m_VideoDecodingSpeed = 0;
    if (m_VideoDecodingSpeed > 7)
        m_VideoDecodingSpeed = 7;

    num = m_VideoDecodingSpeed;

    if (m_VideoDecodingSpeed > 6)
    {
        m_SkipCycle = 1;
        m_ModSkipCycle = 1;
        m_PermanentTurnOffDeblocking = 2;
    }
    else if (m_VideoDecodingSpeed > 5)
    {
        m_SkipCycle = 1;
        m_ModSkipCycle = 1;
        m_PermanentTurnOffDeblocking = 0;
    }
    else if (m_VideoDecodingSpeed > 4)
    {
        m_SkipCycle = 3;
        m_ModSkipCycle = 2;
        m_PermanentTurnOffDeblocking = 1;
    }
    else if (m_VideoDecodingSpeed > 3)
    {
        m_SkipCycle = 3;
        m_ModSkipCycle = 2;
        m_PermanentTurnOffDeblocking = 0;
    }
    else if (m_VideoDecodingSpeed > 2)
    {
        m_SkipCycle = 2;
        m_ModSkipCycle = 2;
        m_PermanentTurnOffDeblocking = 0;
    }
    else if (m_VideoDecodingSpeed > 1)
    {
        m_SkipCycle = 3;
        m_ModSkipCycle = 3;
        m_PermanentTurnOffDeblocking = 0;
    }
    else if (m_VideoDecodingSpeed == 1)
    {
        m_SkipCycle = 4;
        m_ModSkipCycle = 4;
        m_PermanentTurnOffDeblocking = 0;
    }
    else
    {
        m_PermanentTurnOffDeblocking = 0;
    }
}

/****************************************************************************************************/
// AVCController
/****************************************************************************************************/
AVCController::AVCController()
    : m_pDecodedFramesList(0)
    , m_MaxLongTermFrameIdx(0)
    , m_pNALSplitter(0)
    , m_pFirstUncompletedFrame(0)

    , m_iThreadNum(0)
    , m_maxDecFrameBuffering(1)
    , m_dpbSize(1)
    , m_DPBSizeEx(0)
    , m_TrickModeSpeed(1)
    , m_UIDFrameCounter(0)
    , m_pLastDisplayed(0)
    , m_Headers(&m_ObjHeap)
    , m_DefaultNotifyChain(&m_ObjHeap)
    , m_pMemoryAllocator(0)
{
    memset(&m_videoPar, 0, sizeof(mfxVideoParam));
}

AVCController::~AVCController()
{
    Close();
}

Status AVCController::Init(BaseCodecParams *pInit)
{
    VideoDecoderParams *init = DynamicCast<VideoDecoderParams> (pInit);

    if (NULL == init)
        return UMC_ERR_NULL_PTR;

    Close();

    m_dpbSize = 0;
    m_DPBSizeEx = 0;
    m_pCurrentFrame = 0;
    m_iCurrentResource = 0;

    if (ABSOWN(init->dPlaybackRate - 1) > 0.0001)
    {
        m_TrickModeSpeed = 2;
    }
    else
    {
        m_TrickModeSpeed = 1;
    }

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

    m_pFirstUncompletedFrame = 0;
    m_pDecodedFramesList = new H264DBPList();
    if (!m_pDecodedFramesList)
        return UMC_ERR_ALLOC;

    if (init->info.stream_subtype == AVC1_VIDEO)
    {
        m_pNALSplitter = new NALUnitSplitterMP4(&m_Heap);
    }
    else
    {
        m_pNALSplitter = new NALUnitSplitter(&m_Heap);
    }

    m_pNALSplitter->Init();


    m_local_delta_frame_time = 1.0/30;
    m_local_frame_time       = 0;

    if (0 < init->info.framerate)
    {
        m_local_delta_frame_time = 1 / init->info.framerate;
    }

    m_DPBSizeEx = m_iThreadNum + 1;

    if (init->m_pData && init->m_pData->GetDataSize())
    {
        AddSource(init->m_pData, 0);

        UMC::H264VideoDecoderParams params;
        GetInfo(&params);

        m_videoPar.mfx.CodecId = MFX_CODEC_AVC;
        m_videoPar.mfx.FrameInfo.Width = (mfxU16) params.m_fullSize.width;
        m_videoPar.mfx.FrameInfo.Height = (mfxU16) params.m_fullSize.height;
        m_videoPar.mfx.FrameInfo.CropX = (mfxU16) params.m_CropArea.left;
        m_videoPar.mfx.FrameInfo.CropY = (mfxU16) params.m_CropArea.top;
        m_videoPar.mfx.FrameInfo.CropW = (mfxU16) (params.m_fullSize.width - params.m_CropArea.right - params.m_CropArea.left);
        m_videoPar.mfx.FrameInfo.CropH = (mfxU16) (params.m_fullSize.height - params.m_CropArea.bottom - params.m_CropArea.top);
        m_videoPar.mfx.FrameInfo.ChromaFormat = (mfxU8)(GetH264ColorFormat(params.info.color_format) + 1);
    }

    return UMC_OK;
}

Status  AVCController::SetParams(BaseCodecParams* params)
{
    VideoDecoderParams *pParams = DynamicCast<VideoDecoderParams>(params);

    if (NULL == pParams)
        return UMC_ERR_NULL_PTR;

    if (pParams->lTrickModesFlag == 7)
    {
        if (ABSOWN(pParams->dPlaybackRate - 1) > 0.0001)
            m_TrickModeSpeed = 2;
        else
            m_TrickModeSpeed = 1;
    }

    return UMC_OK;
}

void AVCController::Close()
{
    Reset();

    m_Headers.Reset();

    delete m_pNALSplitter;
    m_pNALSplitter = 0;

    delete m_pDecodedFramesList;
    m_pDecodedFramesList = 0;
    m_iThreadNum = 0;

    m_bSeqParamSetRead  = false;
    m_bPicParamSetRead  = false;
    m_dpbSize = 1;
    m_DPBSizeEx = 1;
    m_maxDecFrameBuffering = 1;
    m_CurrentSeqParamSet = -1;
    m_CurrentPicParamSet = -1;
}

void AVCController::Reset()
{
    if (m_pDecodedFramesList)
    {
        for (H264DecoderFrame *pFrame = m_pDecodedFramesList->head(); pFrame; pFrame = pFrame->future())
        {
            pFrame->Reset();
        }
    }

    if (m_pNALSplitter)
        m_pNALSplitter->Reset();

    m_Headers.Reset(true);
    AVCSkipping::Reset();
    m_ObjHeap.Release();
    m_Heap.Reset();

    m_local_frame_time         = 0;

    m_field_index       = 0;
    m_WaitForIDR        = true;

    m_pLastDisplayed = 0;
    m_pCurrentFrame = 0;
    m_pFirstUncompletedFrame = 0;
    m_iCurrentResource = 0;

    m_PicOrderCnt = 0;
    m_PicOrderCntMsb = 0;
    m_PicOrderCntLsb = 0;
    m_FrameNum = 0;
    m_PrevFrameRefNum = 0;
    m_FrameNumOffset = 0;
    m_TopFieldPOC = 0;
    m_BottomFieldPOC = 0;
}

Status AVCController::GetInfo(VideoDecoderParams *lpInfo)
{
    Ipp32s seq_index = 0, pic_index = 0;

    if (m_CurrentSeqParamSet == -1 && !m_bSeqParamSetRead)
    {
        return UMC_ERR_NOT_INITIALIZED;
    }
    else if (m_CurrentSeqParamSet != -1)
    {
        seq_index = m_CurrentSeqParamSet;
    }

    if (m_CurrentPicParamSet == -1 && !m_bPicParamSetRead)
    {
        return UMC_ERR_NOT_INITIALIZED;
    }
    else if(m_CurrentPicParamSet != -1)
    {
        pic_index = m_CurrentPicParamSet;
    }

    H264SeqParamSet *sps = m_Headers.m_SeqParams.GetHeader(seq_index);
    H264PicParamSet *pps = m_Headers.m_PicParams.GetHeader(pic_index);

    lpInfo->info.clip_info.height = sps->frame_height_in_mbs * 16 -
        SubHeightC[sps->chroma_format_idc]*(2 - sps->frame_mbs_only_flag) *
        (sps->frame_cropping_rect_top_offset + sps->frame_cropping_rect_bottom_offset);

    lpInfo->info.clip_info.width = sps->frame_width_in_mbs * 16 - SubWidthC[sps->chroma_format_idc] *
        (sps->frame_cropping_rect_left_offset + sps->frame_cropping_rect_right_offset);

    if (0.0 < m_local_delta_frame_time)
        lpInfo->info.framerate = 1.0 / m_local_delta_frame_time;
    else
        lpInfo->info.framerate = 0.0;

    lpInfo->info.stream_type     = H264_VIDEO;

    lpInfo->profile = sps->profile_idc;
    lpInfo->level = sps->level_idc;

    lpInfo->numThreads = m_iThreadNum;
    lpInfo->info.color_format = GetUMCColorFormat(sps->chroma_format_idc);

    if (sps->aspect_ratio_idc == 255)
    {
        lpInfo->info.aspect_ratio_width  = sps->sar_width;
        lpInfo->info.aspect_ratio_height = sps->sar_height;
    }
    else
    {
        lpInfo->info.aspect_ratio_width  = SAspectRatio[sps->aspect_ratio_idc][0];
        lpInfo->info.aspect_ratio_height = SAspectRatio[sps->aspect_ratio_idc][1];
    }

    Ipp32u multiplier = 1 << (6 + sps->bit_rate_scale);
    lpInfo->info.bitrate = sps->bit_rate_value[0] * multiplier;

    if (sps->frame_mbs_only_flag)
        lpInfo->info.interlace_type = PROGRESSIVE;
    else
    {
        if (0 <= sps->offset_for_top_to_bottom_field)
            lpInfo->info.interlace_type = INTERLEAVED_TOP_FIELD_FIRST;
        else
            lpInfo->info.interlace_type = INTERLEAVED_BOTTOM_FIELD_FIRST;
    }

    H264VideoDecoderParams *lpH264Info = DynamicCast<H264VideoDecoderParams> (lpInfo);
    if (lpH264Info)
    {
        m_CurrentSeqParamSet = seq_index;
        SetDPBSize();
        lpH264Info->m_DPBSize = m_dpbSize + m_DPBSizeEx;

        IppiSize sz;
        sz.width    = sps->frame_width_in_mbs * 16;
        sz.height   = sps->frame_height_in_mbs * 16;
        lpH264Info->m_fullSize = sz;

        lpH264Info->m_entropy_coding_type = pps->entropy_coding_mode;

        lpH264Info->m_CropArea.top = (Ipp16s)(SubHeightC[sps->chroma_format_idc] * sps->frame_cropping_rect_top_offset * (2 - sps->frame_mbs_only_flag));
        lpH264Info->m_CropArea.bottom = (Ipp16s)(SubHeightC[sps->chroma_format_idc] * sps->frame_cropping_rect_bottom_offset * (2 - sps->frame_mbs_only_flag));
        lpH264Info->m_CropArea.left = (Ipp16s)(SubWidthC[sps->chroma_format_idc] * sps->frame_cropping_rect_left_offset);
        lpH264Info->m_CropArea.right = (Ipp16s)(SubWidthC[sps->chroma_format_idc] * sps->frame_cropping_rect_right_offset);
    }

    return UMC_OK;
}

H264DecoderFrame *AVCController::GetFreeFrame(void)
{
    H264DecoderFrame *pFrame = 0;

    // Traverse list for next disposable frame
    if (m_pDecodedFramesList->countAllFrames() > m_dpbSize + m_DPBSizeEx)
        pFrame = m_pDecodedFramesList->GetOldestDisposable();

    VM_ASSERT(!pFrame || pFrame->GetBusyState() == 0);

    // Did we find one?
    if (NULL == pFrame)
    {
        if (m_pDecodedFramesList->countAllFrames() > m_dpbSize + m_DPBSizeEx)
        {
            return 0;
        }

        // Didn't find one. Let's try to insert a new one
        pFrame = new MFXFrame(m_videoPar, m_pMemoryAllocator, &m_Heap, &m_ObjHeap);
        if (NULL == pFrame)
            return 0;

        m_pDecodedFramesList->append(pFrame);

        pFrame->m_index = m_pDecodedFramesList->GetFreeIndex();
    }

    pFrame->Reset();

    // Set current as not displayable (yet) and not outputted. Will be
    // updated to displayable after successful decode.
    pFrame->unsetWasOutputted();
    pFrame->unSetisDisplayable();
    pFrame->SetBusyState(BUSY_STATE_NOT_DECODED);
    pFrame->SetSkipped(false);
    pFrame->SetFrameExistFlag(true);

    if (m_pCurrentFrame == pFrame)
        m_pCurrentFrame = 0;

    m_UIDFrameCounter++;
    pFrame->m_UID = m_UIDFrameCounter;
    m_pDecodedFramesList->MoveToTail(pFrame);

    ((MFXFrame*)pFrame)->m_CUC.Reset();

    return pFrame;
}

Status AVCController::DecodeHeaders(MediaDataEx::_MediaDataEx *pSource, H264MemoryPiece * pMem)
{
    Status umcRes = UMC_OK;

    H264Bitstream bitStream;

    try
    {

    Ipp32s nNALUnitCount = 0;
    for (nNALUnitCount = 0; nNALUnitCount < (Ipp32s) pSource->count; nNALUnitCount++)
    {
        bitStream.Reset((Ipp8u*)pMem->GetPointer() + pSource->offsets[nNALUnitCount],
            pSource->offsets[nNALUnitCount + 1] - pSource->offsets[nNALUnitCount]);

        while ((UMC_OK == umcRes) &&
               (0 == bitStream.GetSCP()))
        {
            umcRes = bitStream.AdvanceToNextSCP();
        }

        if (UMC_OK != umcRes)
            return UMC_ERR_INVALID_STREAM;

        NAL_Unit_Type uNALUnitType;
        Ipp8u uNALStorageIDC;

        bitStream.GetNALUnitType(uNALUnitType, uNALStorageIDC);

        switch(uNALUnitType)
        {
            // sequence parameter set
        case NAL_UT_SPS:
            {
                H264SeqParamSet sps;
                umcRes = bitStream.GetSequenceParamSet(&sps);
                if (umcRes == UMC_OK)
                {
                    DEBUG_PRINT((VM_STRING("debug headers SPS - %d \n"), sps.seq_parameter_set_id));
                    H264SeqParamSet * temp = m_Headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);
                    if (m_Headers.m_SeqParams.AddHeader(&sps,
                        (!m_pCurrentFrame || m_pCurrentFrame->GetBusyState() < BUSY_STATE_NOT_DECODED)))
                    {
                        NotifiersChain *notif = m_pCurrentFrame->GetNotifiersChain();

                        void * buf = m_ObjHeap.Allocate ((notifier2<Headers, Ipp32s, void*> *)0);
                        notifier_base * notifier = new(buf) notifier2<Headers, Ipp32s, void*>(&m_Headers, &Headers::Signal, 0, temp);
                        notif->AddNotifier(notifier);
                    }

                    sps.poffset_for_ref_frame = 0; // avoid twice deleting
                    // DEBUG : (todo - implement copy constructor and assigment operator)

                    m_bSeqParamSetRead = true;

                    // Validate the incoming bitstream's image dimensions.
                    m_CurrentSeqParamSet = sps.seq_parameter_set_id;
                    umcRes = SetDPBSize();
                    m_pDecodedFramesList->SetDPBSize(m_dpbSize);
                    m_maxDecFrameBuffering = sps.max_dec_frame_buffering ?
                        sps.max_dec_frame_buffering : m_dpbSize;

                    if (m_TrickModeSpeed != 1)
                    {
                        m_maxDecFrameBuffering = 0;
                    }
                }
                else
                    return UMC_ERR_INVALID_STREAM;
            }
            break;

        case NAL_UT_SPS_EX:
            {
                H264SeqParamSetExtension sps_ex;
                umcRes = bitStream.GetSequenceParamSetExtension(&sps_ex);

                if (umcRes == UMC_OK)
                {
                    H264SeqParamSetExtension * temp = m_Headers.m_SeqExParams.GetHeader(sps_ex.seq_parameter_set_id);
                    if (m_Headers.m_SeqExParams.AddHeader(&sps_ex,
                        (!m_pCurrentFrame || m_pCurrentFrame->GetBusyState() < BUSY_STATE_NOT_DECODED)))
                    {
                        NotifiersChain *notif = m_pCurrentFrame->GetNotifiersChain();
                        void * buf = m_ObjHeap.Allocate ((notifier2<Headers, Ipp32s, void*> *)0);
                        notifier_base * notifier = new(buf) notifier2<Headers, Ipp32s, void*>(&m_Headers, &Headers::Signal, 1, temp);
                        notif->AddNotifier(notifier);
                    }
                }
                else
                    return UMC_ERR_INVALID_STREAM;
            }
            break;

            // picture parameter set
        case NAL_UT_PPS:
            {
                H264PicParamSet pps;
                // set illegal id
                pps.pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS;

                // Get id
                umcRes = bitStream.GetPictureParamSetPart1(&pps);
                if (UMC_OK == umcRes)
                {
                    H264SeqParamSet *pRefsps = m_Headers.m_SeqParams.GetHeader(pps.seq_parameter_set_id);

                    if (!pRefsps || pRefsps ->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                        return UMC_ERR_INVALID_STREAM;

                    // Get rest of pic param set
                    umcRes = bitStream.GetPictureParamSetPart2(&pps, pRefsps);
                    if (UMC_OK == umcRes)
                    {
                        DEBUG_PRINT((VM_STRING("debug headers PPS - %d - SPS - %d\n"), pps.pic_parameter_set_id, pps.seq_parameter_set_id));
                        H264PicParamSet * temp = m_Headers.m_PicParams.GetHeader(pps.pic_parameter_set_id);
                        if (m_Headers.m_PicParams.AddHeader(&pps,
                            (!m_pCurrentFrame || m_pCurrentFrame->GetBusyState() < BUSY_STATE_NOT_DECODED)))
                        {
                            NotifiersChain *notif = m_pCurrentFrame->GetNotifiersChain();
                            void * buf = m_ObjHeap.Allocate ((notifier2<Headers, Ipp32s, void*> *)0);
                            notifier_base * notifier = new(buf) notifier2<Headers, Ipp32s, void*>(&m_Headers, &Headers::Signal, 2, temp);
                            notif->AddNotifier(notifier);
                        }

                        pps.SliceGroupInfo.t3.pSliceGroupIDMap = 0; // avoid twice deleting
                        // DEBUG : (todo - implement copy constructor and assigment operator)

                        m_bPicParamSetRead = true;

                        m_CurrentPicParamSet = pps.pic_parameter_set_id;

                        // reset current picture parameter set number
                        if (0 > m_CurrentPicParamSet)
                        {
                            m_CurrentPicParamSet = pps.pic_parameter_set_id;
                            m_CurrentSeqParamSet = pps.seq_parameter_set_id;
                        }
                    }
                }
            }
            break;

        default:
            break;
        }
    };

    }
    catch(const h264_exception & ex)
    {
        return ex.GetStatus();
    }
    catch(...)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    return UMC_OK;

} // Status AVCController::DecodeHeaders(MediaDataEx::_MediaDataEx *pSource, H264MemoryPiece * pMem)

//////////////////////////////////////////////////////////////////////////////
// ProcessFrameNumGap
//
// A non-sequential frame_num has been detected. If the sequence parameter
// set field gaps_in_frame_num_value_allowed_flag is non-zero then the gap
// is OK and "non-existing" frames will be created to correctly fill the
// gap. Otherwise the gap is an indication of lost frames and the need to
// handle in a reasonable way.
//////////////////////////////////////////////////////////////////////////////
Status AVCController::ProcessFrameNumGap(H264Slice *slice, Ipp32s field)
{
    Status umcRes = UMC_OK;

    H264SliceHeader *sliceHeader = slice->GetSliceHeader();
    H264SeqParamSet* sps = slice->GetSeqParam();

    Ipp32s uMaxFrameNum = (1<<sps->log2_max_frame_num);
    Ipp32s frameNumGap;

    if (sliceHeader->idr_flag)
        return UMC_OK;

    // Capture any frame_num gap
    if (sliceHeader->frame_num != m_PrevFrameRefNum &&
        sliceHeader->frame_num != (m_PrevFrameRefNum + 1) % uMaxFrameNum)
    {
        // note this could be negative if frame num wrapped

        if (sliceHeader->frame_num > m_PrevFrameRefNum - 1)
        {
            frameNumGap = (sliceHeader->frame_num - m_PrevFrameRefNum - 1) % uMaxFrameNum;
        }
        else
        {
            frameNumGap = (uMaxFrameNum - (m_PrevFrameRefNum + 1 - sliceHeader->frame_num)) % uMaxFrameNum;
        }

        if (frameNumGap > m_dpbSize)
        {
            frameNumGap = m_dpbSize;
        }
    }
    else
    {
        frameNumGap = 0;
        return UMC_OK;
    }

    if (sps->gaps_in_frame_num_value_allowed_flag != 1)
        return UMC_OK;

    if (m_pCurrentFrame)
    {
        m_pCurrentFrame = 0;
        return UMC_ERR_ALLOC;
    }

    DEBUG_PRINT((VM_STRING("frame gap - %d\n"), frameNumGap));

    // Fill the frame_num gap with non-existing frames. For each missing
    // frame:
    //  - allocate a frame
    //  - set frame num and pic num
    //  - update FrameNumWrap for all reference frames
    //  - use sliding window frame marking to free oldest reference
    //  - mark the frame as short-term reference
    // The picture part of the generated frames is unimportant -- it will
    // not be used for reference.

    // set to first missing frame. Note that if frame number wrapped during
    // the gap, the first missing frame_num could be larger than the
    // current frame_num. If that happened, FrameNumGap will be negative.
    //VM_ASSERT((Ipp32s)sliceHeader->frame_num > frameNumGap);
    Ipp32s frame_num = sliceHeader->frame_num - frameNumGap;

    while ((frame_num != sliceHeader->frame_num) &&
        (umcRes == UMC_OK))
    {
        // allocate a frame
        // Traverse list for next disposable frame
        H264DecoderFrame *pFrame = GetFreeFrame();

        // Did we find one?
        if (!pFrame)
        {
            return UMC_ERR_ALLOC;
        }

        Status umcRes = InitFreeFrame(pFrame, slice);

        if (umcRes != UMC_OK)
        {
            return UMC_ERR_ALLOC;
        }

        H264DecoderFrame * pRefFrame = m_pDecodedFramesList->FindClosest(pFrame);

        if (pRefFrame)
        {
            pFrame->CopyPlanes(pRefFrame);
        }
        else
        {
            pFrame->DefaultFill(2, false);
        }

        frameNumGap--;

        if (sps->pic_order_cnt_type != 0)
        {
            Ipp32s tmp1 = sliceHeader->delta_pic_order_cnt[0];
            Ipp32s tmp2 = sliceHeader->delta_pic_order_cnt[1];
            sliceHeader->delta_pic_order_cnt[0] = sliceHeader->delta_pic_order_cnt[1] = 0;

            DecodePictureOrderCount(slice, frame_num);

            sliceHeader->delta_pic_order_cnt[0] = tmp1;
            sliceHeader->delta_pic_order_cnt[1] = tmp2;
        }

        // Set frame num and pic num for the missing frame
        pFrame->setFrameNum(frame_num);
        m_PrevFrameRefNum = frame_num;
        m_FrameNum = frame_num;

        if (sliceHeader->field_pic_flag)
        {
            pFrame->setPicOrderCnt(m_PicOrderCnt,0);
            pFrame->setPicOrderCnt(m_PicOrderCnt,1);
        }
        else
        {
            pFrame->setPicOrderCnt(m_TopFieldPOC, 0);
            pFrame->setPicOrderCnt(m_BottomFieldPOC, 1);
        }

        DEBUG_PRINT((VM_STRING("frame gap - with poc %d %d added\n"), pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1]));

        if (sliceHeader->field_pic_flag == 0)
        {
            pFrame->setPicNum(frame_num, 0);
        }
        else
        {
            pFrame->setPicNum(frame_num*2+1, 0);
            pFrame->setPicNum(frame_num*2+1, 1);
        }

        // Update frameNumWrap and picNum for all decoded frames

        H264DecoderFrame *pFrm;
        H264DecoderFrame * pHead = m_pDecodedFramesList->head();
        for (pFrm = pHead; pFrm; pFrm = pFrm->future())
        {
            // TBD: modify for fields
            pFrm->UpdateFrameNumWrap(frame_num,
                uMaxFrameNum,
                pFrame->m_PictureStructureForRef+
                pFrame->m_bottom_field_flag[field]);
        }

        // sliding window ref pic marking
        SlideWindow(slice, 0, true);

        pFrame->SetisShortTermRef(0);
        pFrame->SetisShortTermRef(1);

        {
            // reset frame global data
            H264DecoderMacroblockGlobalInfo *pMBInfo = pFrame->m_mbinfo.mbs;
            memset(pMBInfo, 0, pFrame->totalMBs*sizeof(H264DecoderMacroblockGlobalInfo));

            // DEBUG : temporal fix !!! for deblocking
            /*memset(pFrame->m_mbinfo.MV[0], 0, iMBCount*sizeof(H264DecoderMacroblockMVs));
            memset(pFrame->m_mbinfo.MV[1], 0, iMBCount*sizeof(H264DecoderMacroblockMVs));
            memset(pFrame->m_mbinfo.RefIdxs[0], 0, iMBCount*sizeof(H264DecoderMacroblockRefIdxs));
            memset(pFrame->m_mbinfo.RefIdxs[0], 0, iMBCount*sizeof(H264DecoderMacroblockRefIdxs));*/
        }

        // next missing frame
        frame_num++;
        if (frame_num >= uMaxFrameNum)
            frame_num = 0;

        // Set current as displayable and was outputted.
        pFrame->SetBusyState(BUSY_STATE_NOT_DECODED);
        pFrame->SetisDisplayable();
        pFrame->SetSkipped(true);
        pFrame->SetFrameExistFlag(false);
    }   // while

    return m_pDecodedFramesList->IsDisposableExist() ? UMC_OK : UMC_ERR_ALLOC;
}   // ProcessFrameNumGap

Status AVCController::SetDPBSize()
{
    Status umcRes = UMC_OK;
    Ipp32u MaxDPBx2;
    Ipp32u dpbLevel;
    Ipp32u dpbSize;

    H264SeqParamSet* sps = m_Headers.m_SeqParams.GetHeader(m_CurrentSeqParamSet);

    // MaxDPB, per Table A-1, Level Limits
    switch (sps->level_idc)
    {
    case 10:
        MaxDPBx2 = 297;
        break;
    case 11:
        MaxDPBx2 = 675;
        break;
    case 12:
    case 13:
    case 20:
        MaxDPBx2 = 891*2;
        break;
    case 21:
        MaxDPBx2 = 1782*2;
        break;
    case 22:
    case 30:
        MaxDPBx2 = 6075;
        break;
    case 31:
        MaxDPBx2 = 6750*2;
        break;
    case 32:
        MaxDPBx2 = 7680*2;
        break;
    case 40:
    case 41:
    case 42:
        MaxDPBx2 = 12288*2;
        break;
    case 50:
        MaxDPBx2 = 41400*2;
        break;
    case 51:
        MaxDPBx2 = 69120*2;
        break;
    default:
        MaxDPBx2 = 69120*2; //get as much as we may
        //umcRes = UMC_ERR_INVALID_STREAM;
    }

    if (umcRes == UMC_OK)
    {
        Ipp32u width, height;

        width = sps->frame_width_in_mbs*16;
        height = sps->frame_height_in_mbs*16;

        dpbLevel = (MaxDPBx2 * 512) / ((width * height) + ((width * height)>>1));
        dpbSize = IPP_MIN(16, dpbLevel);

        // we must have enough additional frames to do good frame parallelization
        m_dpbSize = dpbSize;
    }

    return umcRes;

}    // setDPBSize

bool AVCController::GetFrameToDisplay(MediaData *dst, bool force)
{
    // Perform output color conversion and video effects, if we didn't
    // already write our output to the application's buffer.
    VideoData *pVData = DynamicCast<VideoData> (dst);
    if (!pVData)
        return false;

    m_pLastDisplayed = 0;

    H264DecoderFrame * pFrame = GetFrameToDisplayInternal(dst, force);
    if (!pFrame)
    {
        return false;
    }

    m_pLastDisplayed = pFrame;
    pVData->SetInvalid(pFrame->GetError());

    /*if (m_pLastDisplayed->IsSkipped())
    {
        m_LastDecodedFrame.SetDataSize(1);
        pFrame->setWasOutputted();
        return true;
    }

    m_LastDecodedFrame.SetTime(dst->GetTime());

    m_LastDecodedFrame.SetDataSize(m_LastDecodedFrame.GetMappingSize());*/
    pVData->SetDataSize(pVData->GetMappingSize());
    pFrame->setWasOutputted();

    return true;
}

bool AVCController::IsWantToShowFrame(bool force)
{
    if (m_pDecodedFramesList->countNumDisplayable() > m_maxDecFrameBuffering ||
        force)
    {
        H264DecoderFrame * pTmp = m_pDecodedFramesList->findOldestDisplayable(m_dpbSize);
        return !!pTmp;
    }

    return false;
}

H264DecoderFrame *AVCController::GetFrameToDisplayInternal(MediaData *dst, bool force)
{
    H264DecoderFrame *pTmp;

    if (!dst)
        return 0;

    for (;;)
    {
        // show oldest frame
        if (m_pDecodedFramesList->countNumDisplayable() >= m_maxDecFrameBuffering ||
            force)
        {
            if (m_maxDecFrameBuffering)
            {
                pTmp = m_pDecodedFramesList->findOldestDisplayable(m_dpbSize);
            }
            else
            {
                pTmp = m_pDecodedFramesList->findFirstDisplayable();
            }

            if (pTmp && pTmp->GetBusyState() < BUSY_STATE_NOT_DECODED)
            {
                Ipp64f time;

                if (!pTmp->IsFrameExist())
                {
                    pTmp->setWasOutputted();
                    continue;
                }

                // set time
                if (pTmp->m_dFrameTime > -1.0)
                {
                    time = pTmp->m_dFrameTime;
                    dst->SetTime(time);
                    //m_LastDecodedFrame.SetTime(time);
                    m_local_frame_time = time;
                }
                else
                {
                    //m_LastDecodedFrame.SetTime(m_local_frame_time);
                    pTmp->m_dFrameTime = m_local_frame_time;
                    dst->SetTime(m_local_frame_time);
                }

                if (!pTmp->post_procces_complete)
                {
                    //static Ipp32s count = 0;
                    m_local_frame_time += m_local_delta_frame_time;
                    pTmp->post_procces_complete = true;

                    static int pppp = 0;
                    DEBUG_PRINT((VM_STRING("Outputted POC - %d, busyState - %d, uid - %d, pppp - %d\n"), pTmp->m_PicOrderCnt[0], pTmp->GetBusyState(), pTmp->m_UID, pppp++));
                }

                if (pTmp->IsSkipped())
                {
                    return pTmp;
                }
            }
            else
            {
                return 0;
            }

            return pTmp;
        }
        else // there are no frames to show
            return NULL;
    }
}

void AVCController::SlideWindow(H264Slice * pSlice, Ipp32s field_index, bool force)
{
    Ipp32u NumShortTermRefs, NumLongTermRefs;
    H264SeqParamSet* sps = pSlice->GetSeqParam();

    // find out how many active reference frames currently in decoded
    // frames buffer
    m_pDecodedFramesList->countActiveRefs(NumShortTermRefs, NumLongTermRefs);
    while (NumShortTermRefs > 0 &&
        (NumShortTermRefs + NumLongTermRefs >= (Ipp32s)sps->num_ref_frames) &&
        !field_index)
    {
        // mark oldest short-term reference as unused
        VM_ASSERT(NumShortTermRefs > 0);

        H264DecoderFrame * pFrame = m_pDecodedFramesList->freeOldestShortTermRef();

        if (!pFrame)
            break;

        NumShortTermRefs--;

        if (!force)
            OnAVCSlideWindow(pFrame, m_pCurrentFrame, &m_DefaultNotifyChain, false);
        else
            OnAVCSlideWindow(pFrame, 0, &m_DefaultNotifyChain, false);
    }
}

//////////////////////////////////////////////////////////////////////////////
// updateRefPicMarking
//  Called at the completion of decoding a frame to update the marking of the
//  reference pictures in the decoded frames buffer.
//////////////////////////////////////////////////////////////////////////////
Status AVCController::UpdateRefPicMarking(H264DecoderFrame * pFrame, H264Slice * pSlice, Ipp32s field_index)
{
    Status umcRes = UMC_OK;
    Ipp32u arpmmf_idx;
    Ipp32s picNum;
    Ipp32s LongTermFrameIdx;
    bool bCurrentisST = true;

    H264SliceHeader const * sliceHeader = pSlice->GetSliceHeader();

    if (pFrame->m_bIDRFlag)
    {
        // mark all reference pictures as unused
        m_pDecodedFramesList->removeAllRef(m_pCurrentFrame);

        if (sliceHeader->long_term_reference_flag)
        {
            pFrame->SetisLongTermRef(field_index);
            pFrame->setLongTermFrameIdx(0);
            m_MaxLongTermFrameIdx = 0;
            bCurrentisST = false;
        }
        else
        {
            pFrame->SetisShortTermRef(field_index);
            m_MaxLongTermFrameIdx = -1;        // no long term frame indices
        }
    }
    else
    {
        AdaptiveMarkingInfo* pAdaptiveMarkingInfo = pSlice->GetAdaptiveMarkingInfo();
        // adaptive ref pic marking
        if (pAdaptiveMarkingInfo && pAdaptiveMarkingInfo->num_entries > 0)
        {
            for (arpmmf_idx=0; arpmmf_idx<pAdaptiveMarkingInfo->num_entries;
                 arpmmf_idx++)
            {
                H264DecoderFrame * pRefFrame = 0;

                switch (pAdaptiveMarkingInfo->mmco[arpmmf_idx])
                {
                case 1:
                    // mark a short-term picture as unused for reference
                    // Value is difference_of_pic_nums_minus1
                    picNum = pFrame->PicNum(field_index) -
                        (pAdaptiveMarkingInfo->value[arpmmf_idx*2] + 1);
                    pRefFrame = m_pDecodedFramesList->freeShortTermRef(picNum);
                    break;
                case 2:
                    // mark a long-term picture as unused for reference
                    // value is long_term_pic_num
                    picNum = pAdaptiveMarkingInfo->value[arpmmf_idx*2];
                    pRefFrame = m_pDecodedFramesList->freeLongTermRef(picNum);
                    break;
                case 3:
                    // Assign a long-term frame idx to a short-term picture
                    // Value is difference_of_pic_nums_minus1 followed by
                    // long_term_frame_idx. Only this case uses 2 value entries.
                    picNum = pFrame->PicNum(field_index) -
                        (pAdaptiveMarkingInfo->value[arpmmf_idx*2] + 1);
                    LongTermFrameIdx = pAdaptiveMarkingInfo->value[arpmmf_idx*2+1];

                    pRefFrame = m_pDecodedFramesList->findShortTermPic(picNum, 0);

                    // First free any existing LT reference with the LT idx
                    pRefFrame = m_pDecodedFramesList->freeLongTermRefIdx(LongTermFrameIdx, pRefFrame);

                    m_pDecodedFramesList->changeSTtoLTRef(picNum, LongTermFrameIdx);
                    break;
                case 4:
                    // Specify max long term frame idx
                    // Value is max_long_term_frame_idx_plus1
                    // Set to "no long-term frame indices" (-1) when value == 0.
                    m_MaxLongTermFrameIdx = pAdaptiveMarkingInfo->value[arpmmf_idx*2] - 1;

                    // Mark any long-term reference frames with a larger LT idx
                    // as unused for reference.
                    m_pDecodedFramesList->freeOldLongTermRef(m_MaxLongTermFrameIdx, m_pCurrentFrame);
                    break;
                case 5:
                    // Mark all as unused for reference
                    // no value
                    m_WaitForIDR = false;
                    m_pDecodedFramesList->removeAllRef(m_pCurrentFrame);
                    m_pDecodedFramesList->IncreaseRefPicListResetCount(pFrame);
                    m_MaxLongTermFrameIdx = -1;        // no long term frame indices
                    // set "previous" picture order count vars for future

                    if (pFrame->m_PictureStructureForDec < 0)
                    {
                        pFrame->setPicOrderCnt(0, field_index);
                        pFrame->setPicNum(0, field_index);
                    }
                    else
                    {
                        Ipp32s poc = pFrame->PicOrderCnt(0, 3);
                        pFrame->setPicOrderCnt(pFrame->PicOrderCnt(0, 1) - poc, 0);
                        pFrame->setPicOrderCnt(pFrame->PicOrderCnt(1, 1) - poc, 1);
                        pFrame->setPicNum(0, 0);
                        pFrame->setPicNum(0, 1);
                    }

                    m_FrameNumOffset = 0;
                    m_FrameNum = 0;
                    m_PrevFrameRefNum = 0;
                    // set frame_num to zero for this picture, for correct
                    // FrameNumWrap
                    pFrame->setFrameNum(0);

                    {
                    mfxFrameCUC * cuc = ((MFXFrame *)pFrame)->GetCUC();

                    mfxExtAvcDxvaPicParam *picParam = GetExtBuffer<mfxExtAvcDxvaPicParam>(cuc, MFX_LABEL_EXTHEADER, MFX_CUC_AVC_EXTHEADER);
                    if (!picParam)
                        throw UMC_ERR_ALLOC;

                    picParam->CurrFieldOrderCnt[0] = (mfxU16) pFrame->m_PicOrderCnt[pFrame->GetNumberByParity(0)];
                    picParam->CurrFieldOrderCnt[1] = (mfxU16) pFrame->m_PicOrderCnt[pFrame->GetNumberByParity(1)];
                    cuc->FrameCnt = (mfxU16) pFrame->m_FrameNum;
                    }

                    break;
                case 6:
                    // Assign long term frame idx to current picture
                    // Value is long_term_frame_idx
                    LongTermFrameIdx = pAdaptiveMarkingInfo->value[arpmmf_idx*2];

                    // First free any existing LT reference with the LT idx
                    pRefFrame = m_pDecodedFramesList->freeLongTermRefIdx(LongTermFrameIdx, pFrame);

                    // Mark current
                    pFrame->SetisLongTermRef(field_index);
                    pFrame->setLongTermFrameIdx(LongTermFrameIdx);
                    bCurrentisST = false;
                    break;
                case 0:
                default:
                    // invalid mmco command in bitstream
                    VM_ASSERT(0);
                    umcRes = UMC_ERR_INVALID_STREAM;
                }    // switch

                OnAVCSlideWindow(pRefFrame, m_pCurrentFrame, &m_DefaultNotifyChain, false);
            }    // for arpmmf_idx
        }
    }    // not IDR picture

    if (bCurrentisST)
    { // set current as
        if (sliceHeader->field_pic_flag && field_index)
        {
        }
        else
        {
            SlideWindow(pSlice, field_index);
        }

        pFrame->SetisShortTermRef(field_index);
    }

    return umcRes;
}    // updateRefPicMarking

void AVCController::DecodePictureOrderCount(H264Slice *slice, Ipp32s frame_num)
{
    H264SliceHeader *sliceHeader = slice->GetSliceHeader();
    H264SeqParamSet* sps = slice->GetSeqParam();

    Ipp32s uMaxFrameNum = (1<<sps->log2_max_frame_num);

    if (sps->pic_order_cnt_type == 0)
    {
        // pic_order_cnt type 0
        Ipp32s CurrPicOrderCntMsb;
        Ipp32s MaxPicOrderCntLsb = sps->MaxPicOrderCntLsb;

        if ((sliceHeader->pic_order_cnt_lsb < m_PicOrderCntLsb) &&
             ((m_PicOrderCntLsb - sliceHeader->pic_order_cnt_lsb) >= (MaxPicOrderCntLsb >> 1)))
            CurrPicOrderCntMsb = m_PicOrderCntMsb + MaxPicOrderCntLsb;
        else if ((sliceHeader->pic_order_cnt_lsb > m_PicOrderCntLsb) &&
                ((sliceHeader->pic_order_cnt_lsb - m_PicOrderCntLsb) > (MaxPicOrderCntLsb >> 1)))
            CurrPicOrderCntMsb = m_PicOrderCntMsb - MaxPicOrderCntLsb;
        else
            CurrPicOrderCntMsb = m_PicOrderCntMsb;

        if (sliceHeader->nal_ref_idc)
        {
            // reference picture
            m_PicOrderCntMsb = CurrPicOrderCntMsb & (~(MaxPicOrderCntLsb - 1));
            m_PicOrderCntLsb = sliceHeader->pic_order_cnt_lsb;
        }
        m_PicOrderCnt = CurrPicOrderCntMsb + sliceHeader->pic_order_cnt_lsb;
        if (sliceHeader->field_pic_flag == 0)
        {
             m_TopFieldPOC = CurrPicOrderCntMsb + sliceHeader->pic_order_cnt_lsb;
             m_BottomFieldPOC = m_TopFieldPOC + sliceHeader->delta_pic_order_cnt_bottom;
        }

    }    // pic_order_cnt type 0
    else if (sps->pic_order_cnt_type == 1)
    {
        // pic_order_cnt type 1
        Ipp32u i;
        Ipp32u uAbsFrameNum;    // frame # relative to last IDR pic
        Ipp32u uPicOrderCycleCnt = 0;
        Ipp32u uFrameNuminPicOrderCntCycle = 0;
        Ipp32s ExpectedPicOrderCnt = 0;
        Ipp32s ExpectedDeltaPerPicOrderCntCycle;
        Ipp32u uNumRefFramesinPicOrderCntCycle = sps->num_ref_frames_in_pic_order_cnt_cycle;

        if (frame_num < m_FrameNum)
            m_FrameNumOffset += uMaxFrameNum;

        if (uNumRefFramesinPicOrderCntCycle != 0)
            uAbsFrameNum = m_FrameNumOffset + frame_num;
        else
            uAbsFrameNum = 0;

        if ((sliceHeader->nal_ref_idc == false)  && (uAbsFrameNum > 0))
            uAbsFrameNum--;

        if (uAbsFrameNum)
        {
            uPicOrderCycleCnt = (uAbsFrameNum - 1) /
                    uNumRefFramesinPicOrderCntCycle;
            uFrameNuminPicOrderCntCycle = (uAbsFrameNum - 1) %
                    uNumRefFramesinPicOrderCntCycle;
        }

        ExpectedDeltaPerPicOrderCntCycle = 0;
        for (i=0; i < uNumRefFramesinPicOrderCntCycle; i++)
        {
            ExpectedDeltaPerPicOrderCntCycle +=
                sps->poffset_for_ref_frame[i];
        }

        if (uAbsFrameNum)
        {
            ExpectedPicOrderCnt = uPicOrderCycleCnt * ExpectedDeltaPerPicOrderCntCycle;
            for (i=0; i<=uFrameNuminPicOrderCntCycle; i++)
            {
                ExpectedPicOrderCnt +=
                    sps->poffset_for_ref_frame[i];
            }
        }
        else
            ExpectedPicOrderCnt = 0;

        if (sliceHeader->nal_ref_idc == false)
            ExpectedPicOrderCnt += sps->offset_for_non_ref_pic;
        m_PicOrderCnt = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[0];
        if( sliceHeader->field_pic_flag==0)
        {
            m_TopFieldPOC = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[ 0 ];
            m_BottomFieldPOC = m_TopFieldPOC +
                sps->offset_for_top_to_bottom_field + sliceHeader->delta_pic_order_cnt[ 1 ];
        }
        else if( ! sliceHeader->bottom_field_flag)
            m_PicOrderCnt = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[ 0 ];
        else
            m_PicOrderCnt  = ExpectedPicOrderCnt + sps->offset_for_top_to_bottom_field + sliceHeader->delta_pic_order_cnt[ 0 ];
    }    // pic_order_cnt type 1
    else if (sps->pic_order_cnt_type == 2)
    {
        // pic_order_cnt type 2
        Ipp32s iMaxFrameNum = (1<<sps->log2_max_frame_num);
        Ipp32u uAbsFrameNum;    // frame # relative to last IDR pic

        if (frame_num < m_FrameNum)
            m_FrameNumOffset += iMaxFrameNum;
        uAbsFrameNum = frame_num + m_FrameNumOffset;
        m_PicOrderCnt = uAbsFrameNum*2;
        if (sliceHeader->nal_ref_idc == false)
            m_PicOrderCnt--;
            m_TopFieldPOC = m_PicOrderCnt;
            m_BottomFieldPOC = m_PicOrderCnt;

    }    // pic_order_cnt type 2

    if (sliceHeader->nal_ref_idc)
    {
        m_PrevFrameRefNum = frame_num;
    }

    m_FrameNum = frame_num;
}    // decodePictureOrderCount

Status AVCController::GetFrame(MediaData * pSource, MediaData *dst)
{
    Status umcRes = AddSource(pSource, dst);
    //if (UMC_OK == umcRes && dst && m_LastDecodedFrame.GetDataSize())
      //  return UMC_OK;

    //if (UMC_OK != umcRes && UMC_ERR_NOT_ENOUGH_BUFFER != umcRes)

    return umcRes;

    //bool force = (umcRes == UMC_ERR_NOT_ENOUGH_BUFFER);
    //return ;
}

Status AVCController::AddSource(MediaData * pSource, MediaData *dst)
{
    Status umsRes = UMC_OK;

    bool isDPBFull = false;

    if (!m_pDecodedFramesList->IsDisposableExist())
    {
        if (m_pDecodedFramesList->countAllFrames() > m_dpbSize + m_DPBSizeEx)
        {
            if (GetFrameToDisplayInternal(dst, false))
                return UMC_OK;
            else
            {
                isDPBFull = true;
            }
        }
    }

    MediaDataEx *out;

    bool is_header_readed;

    do
    {
        is_header_readed = false;

        if (!dst)
        {
            Ipp32s iCode = m_pNALSplitter->CheckNalUnitType(pSource);
            switch (iCode)
            {
            case NAL_UT_IDR_SLICE:
            case NAL_UT_SLICE:
            case NAL_UT_AUXILIARY:
            case NAL_UT_DPA: //ignore it
            case NAL_UT_DPB:
            case NAL_UT_DPC:
                return UMC_ERR_NOT_ENOUGH_DATA;
            }
        }

        H264MemoryPiece * pMemCopy = 0;
        H264MemoryPiece * pMem = m_pNALSplitter->GetNalUnits(pSource, out, &pMemCopy);

        if (!pMem && pSource)
            return UMC_ERR_SYNC;

        if (!pMem)
            return UMC_OK;

        MediaDataEx::_MediaDataEx* pMediaDataEx = out->GetExData();

        bool isShouldCommit = true;

        for (Ipp32s i = 0; i < (Ipp32s)pMediaDataEx->count; i++)
        {
            switch ((NAL_Unit_Type)pMediaDataEx->values[i])
            {
            case NAL_UT_IDR_SLICE:
            case NAL_UT_SLICE:
            //case NAL_UT_AUXILIARY:
                if (!dst)
                    return UMC_OK;

                if (!AddSlice(pMem, pSource))
                {
                    if (isDPBFull && m_pNALSplitter->GetCurrent() && !m_pDecodedFramesList->IsDisposableExist())
                    {
                        m_pNALSplitter->Commit();
                        m_pFirstUncompletedFrame = m_pDecodedFramesList->head();
                        if (!GetCUCToDecode())
                        {
                            if (!m_DefaultNotifyChain.IsEmpty())
                            {
                                m_DefaultNotifyChain.Notify();
                                break;
                            }

                            isShouldCommit = false;
                            break;
                        }
                    }

                    m_pNALSplitter->Commit();
                    isShouldCommit = false;

                    return UMC_ERR_NOT_ENOUGH_BUFFER;
                }
                break;

            case NAL_UT_SPS:
            case NAL_UT_PPS:
            case NAL_UT_SPS_EX:
                umsRes = DecodeHeaders(pMediaDataEx, pMem);
                if (umsRes != UMC_OK)
                {
                    m_pNALSplitter->Commit();
                    return umsRes;
                }
                is_header_readed = true;
                break;

            case NAL_UT_SEI:
                is_header_readed = true;
                break;
            case NAL_UT_AUD:
                is_header_readed = true;
                if (dst)
                {
                    AddSlice(0, pSource);
                }
                break;

            case NAL_UT_DPA: //ignore it
            case NAL_UT_DPB:
            case NAL_UT_DPC:
            case NAL_UT_FD:
            case NAL_END_OF_SEQ:
            case NAL_END_OF_STREAM:
            case NAL_UT_UNSPECIFIED:
                break;

            default:
                is_header_readed = true;
                break;
            };
        }

        if (isShouldCommit)
        {
            m_pNALSplitter->Commit();
        }
        else
        {
            break;
        }

    } while ((is_header_readed || dst) &&
            (pSource) &&
            (MINIMAL_DATA_SIZE < pSource->GetDataSize()));

    return UMC_OK;

} // AddSource(MediaData * (&pSource))

bool IsAVCFieldOfOneFrame(H264DecoderFrame *pFrame, H264Slice * pSlice1, H264Slice *pSlice2)
{
    if (!pFrame)
        return false;

    if (pFrame && pFrame->GetAU(0)->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED
        && pFrame->GetAU(1)->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED)
        return false;

    if ((pSlice1->GetSliceHeader()->nal_ref_idc && !pSlice2->GetSliceHeader()->nal_ref_idc) ||
        (!pSlice1->GetSliceHeader()->nal_ref_idc && pSlice2->GetSliceHeader()->nal_ref_idc))
        return false;

    if (pSlice1->GetSliceHeader()->field_pic_flag != pSlice2->GetSliceHeader()->field_pic_flag)
        return false;

//    if (pSlice1->GetSliceHeader()->frame_num != pSlice2->GetSliceHeader()->frame_num)
  //      return false;

    if (pSlice1->GetSliceHeader()->bottom_field_flag == pSlice2->GetSliceHeader()->bottom_field_flag)
        return false;

    return true;
}

bool AVCController::AddSlice(H264MemoryPiece * pMem, MediaData * pSource)
{
    H264Slice * pSlice;

    if (!m_bSeqParamSetRead || !m_bPicParamSetRead)
    {
        m_pNALSplitter->Commit();
        return false;
    }

    if (pMem)
    {
        pSlice = m_ObjHeap.AllocateObject((H264Slice*)0);
        pSlice->SetHeap(&m_ObjHeap);

        Ipp32s pps_pid = pSlice->RetrievePicParamSetNumber(pMem->GetPointer(), pMem->GetSize());
        if (pps_pid == -1)
        {
            m_ObjHeap.FreeObject(pSlice);
            m_pNALSplitter->Commit();
            return false;
        }

        H264SEIPayLoad * spl = m_Headers.m_SEIParams.GetHeader(SEI_RECOVERY_POINT_TYPE);

        if (m_WaitForIDR)
        {
            if (pSlice->GetSliceHeader()->slice_type != INTRASLICE && !spl)
            {
                m_pNALSplitter->Commit();
                m_ObjHeap.FreeObject(pSlice);
                return false;
            }
        }

        pSlice->SetPicParam(m_Headers.m_PicParams.GetHeader(pps_pid));
        if (!pSlice->GetPicParam())
        {
            m_ObjHeap.FreeObject(pSlice);
            m_pNALSplitter->Commit();
            return false;
        }

        pSlice->SetSeqParam(m_Headers.m_SeqParams.GetHeader(pSlice->GetPicParam()->seq_parameter_set_id));
        if (!pSlice->GetSeqParam())
        {
            m_ObjHeap.FreeObject(pSlice);
            m_pNALSplitter->Commit();
            return false;
        }

        pSlice->SetSeqExParam(m_Headers.m_SeqExParams.GetHeader(pSlice->GetPicParam()->seq_parameter_set_id));
        pSlice->SetCurrentFrame(m_pCurrentFrame);

        m_CurrentSeqParamSet = pSlice->GetPicParam()->seq_parameter_set_id;
        m_CurrentPicParamSet = pSlice->GetPicParam()->pic_parameter_set_id;

        pSlice->m_pSource = pMem;
        pSlice->m_pSource_DXVA = 0;
        pSlice->m_dTime = pSource ? pSource->GetTime() : -1;
        pSlice->m_pSource_DXVA = m_pNALSplitter->GetCurrentCopyVersion();

        if (!pSlice->Reset(pMem->GetPointer(), pMem->GetDataSize(), m_iThreadNum))
        {
            // should ignored slice
            m_ObjHeap.FreeObject(pSlice);
            m_pNALSplitter->Commit();
            return false;
        }

        if (spl && (pSlice->GetSliceHeader()->slice_type != INTRASLICE))
        {
            m_Headers.Signal(3, spl);
        }

        m_WaitForIDR = false;
    }
    else
    {
        if (!m_pCurrentFrame)
            return true;

        CompleteFrame(m_pCurrentFrame, m_field_index);

        /*if (!pSource && m_pCurrentFrame && m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE)
        {
            ProcessNonPairedField(m_pCurrentFrame);
        }*/

        return true;
    }

    H264DecoderFrame * pFrame = 0;

    if (m_pCurrentFrame)
    {
        H264Slice * pFirstFrameSlice = m_pCurrentFrame->GetAU(m_field_index)->GetAnySlice();
        VM_ASSERT(pFirstFrameSlice);

        if ((false == IsPictureTheSame(pFirstFrameSlice, pSlice)))
        {
            bool isField = pFirstFrameSlice->IsField();
            CompleteFrame(m_pCurrentFrame, m_field_index);

            if (pSlice->IsField())
            {
                if (IsAVCFieldOfOneFrame(m_pCurrentFrame, pFirstFrameSlice, pSlice))
                {
                    m_field_index = 1;
                    //VM_ASSERT(!pFirstFrameSlice || pFirstFrameSlice->GetSliceHeader()->frame_num == pSlice->GetSliceHeader()->frame_num);
                    InitFrame(m_pCurrentFrame, pSlice);
                    if (!m_pFirstUncompletedFrame)
                        m_pFirstUncompletedFrame = m_pCurrentFrame;
                }
                else
                {
                    ProcessNonPairedField(m_pCurrentFrame);
                    m_field_index = 0;

                    if (ProcessFrameNumGap(pSlice, m_field_index) != UMC_OK)
                    {
                        m_ObjHeap.FreeObject(pSlice);
                        return false;
                    }

                    pFrame = AddFrame(pSlice);
                    if (!pFrame)
                    {
                        m_pCurrentFrame = 0;
                        m_ObjHeap.FreeObject(pSlice);
                        return false;
                    }
                    m_pCurrentFrame = pFrame;
                }
            }
            else
            {
                if (isField) // only one field
                {
                    ProcessNonPairedField(m_pCurrentFrame);
                }

                m_field_index = 0;

                if (ProcessFrameNumGap(pSlice, m_field_index) != UMC_OK)
                {
                    m_ObjHeap.FreeObject(pSlice);
                    return false;
                }

                pFrame = AddFrame(pSlice);
                if (!pFrame)
                {
                    m_pCurrentFrame = 0;
                    m_ObjHeap.FreeObject(pSlice);
                    return false;
                }

                m_pCurrentFrame = pFrame;
            }
        }
    }
    else
    {
        if (ProcessFrameNumGap(pSlice, m_field_index) != UMC_OK)
        {
            m_ObjHeap.FreeObject(pSlice);
            return false;
        }

        pFrame = AddFrame(pSlice);
        if (!pFrame)
        {
            m_pCurrentFrame = 0;
            m_ObjHeap.FreeObject(pSlice);
            return false;
        }

        m_pCurrentFrame = pFrame;
    }

    // set IDR for auxiliary
    //

    // Init refPicList
    AddSliceToFrame(m_pCurrentFrame, pSlice);

    if (pSlice->GetSliceHeader()->slice_type != INTRASLICE)
    {
        Ipp32u NumShortTermRefs, NumLongTermRefs;
        m_pDecodedFramesList->countActiveRefs(NumShortTermRefs, NumLongTermRefs);
        if (NumShortTermRefs + NumLongTermRefs == 0)
        {
            H264DecoderFrame *pFrame = GetFreeFrame();
            if (!pFrame)
                return false;

            Status umsRes = InitFreeFrame(pFrame, pSlice);
            if (umsRes == UMC_OK)
            {
                Ipp32s frame_num = pSlice->GetSliceHeader()->frame_num;
                if (pSlice->GetSliceHeader()->field_pic_flag == 0)
                {
                    pFrame->setPicNum(frame_num, 0);
                }
                else
                {
                    pFrame->setPicNum(frame_num*2+1, 0);
                    pFrame->setPicNum(frame_num*2+1, 1);
                }

                pFrame->SetisShortTermRef(0);
                pFrame->SetisShortTermRef(1);

                pFrame->SetSkipped(true);
                pFrame->SetFrameExistFlag(false);
                pFrame->SetBusyState(BUSY_STATE_NOT_DECODED);
                pFrame->SetisDisplayable();

                pFrame->DefaultFill(2, false);

                H264SliceHeader* sliceHeader = pSlice->GetSliceHeader();
                if (pSlice->GetSeqParam()->pic_order_cnt_type != 0)
                {
                    Ipp32s tmp1 = sliceHeader->delta_pic_order_cnt[0];
                    Ipp32s tmp2 = sliceHeader->delta_pic_order_cnt[1];
                    sliceHeader->delta_pic_order_cnt[0] = sliceHeader->delta_pic_order_cnt[1] = 0;

                    DecodePictureOrderCount(pSlice, frame_num);

                    sliceHeader->delta_pic_order_cnt[0] = tmp1;
                    sliceHeader->delta_pic_order_cnt[1] = tmp2;
                }

                if (sliceHeader->field_pic_flag)
                {
                    pFrame->setPicOrderCnt(m_PicOrderCnt,0);
                    pFrame->setPicOrderCnt(m_PicOrderCnt,1);
                }
                else
                {
                    pFrame->setPicOrderCnt(m_TopFieldPOC, 0);
                    pFrame->setPicOrderCnt(m_BottomFieldPOC, 1);
                }

                // mark generated frame as short-term reference
                {
                    // reset frame global data
                    H264DecoderMacroblockGlobalInfo *pMBInfo = pFrame->m_mbinfo.mbs;
                    memset(pMBInfo, 0, pFrame->totalMBs*sizeof(H264DecoderMacroblockGlobalInfo));
                }
            }
        }
    }

    pSlice->UpdateReferenceList(m_pDecodedFramesList);
    m_pNALSplitter->DropMemory();
    return true;
}

void AVCController::ProcessNonPairedField(H264DecoderFrame * pFrame)
{
    if (pFrame && pFrame->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_NOT_FILLED)
    {
        /*pFrame->setPicOrderCnt(pFrame->PicOrderCnt(0, 1), 1);
        pFrame->GetAU(1)->SetStatus(H264DecoderFrameInfo::STATUS_NONE);
        pFrame->m_bottom_field_flag[1] = !pFrame->m_bottom_field_flag[0];

        pFrame->m_isShortTermRef[1] = pFrame->m_isShortTermRef[0];
        pFrame->m_isLongTermRef[1] = pFrame->m_isLongTermRef[0];

        H264Slice * pSlice = pFrame->GetAU(0)->GetAnySlice();
        pFrame->setPicNum(pSlice->GetSliceHeader()->frame_num*2 + 1, 1);

        Ipp32s isBottom = pSlice->IsBottomField() ? 0 : 1;
        pFrame->DefaultFill(isBottom, false);*/
    }
}

void AVCController::CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field)
{
    H264DecoderFrameInfo * slicesInfo = pFrame->GetAU(field);

    DEBUG_PRINT((VM_STRING("Complete frame POC - (%d,%d) type - %d, field - %d, count - %d, m_uid - %d, IDR - %d\n"), pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1], pFrame->m_FrameType, m_field_index, pFrame->GetAU(m_field_index)->GetSliceCount(), pFrame->m_UID, pFrame->m_bIDRFlag));
    if (slicesInfo->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED)
        return;

    mfxFrameCUC * cuc = ((MFXFrame *)pFrame)->GetCUC();

    mfxExtAvcDxvaPicParam *picParam = GetExtBuffer<mfxExtAvcDxvaPicParam>(cuc, MFX_LABEL_EXTHEADER, MFX_CUC_AVC_EXTHEADER);
    if (!picParam)
        throw UMC_ERR_ALLOC;

    FillFrameParam(slicesInfo, cuc->FrameParam, picParam, m_pDecodedFramesList);
    cuc->FrameCnt = (mfxU16) pFrame->m_FrameNum;

    /* Ipp32s NumMb = cuc->NumMb;
    if (cuc)
    {
        cuc->MbParam += NumMb;
    } */

    mfxExtCUCRefList * cucRefList = GetExtBuffer<mfxExtCUCRefList>(cuc, MFX_LABEL_CUCREFLIST, MFX_CUC_AVC_CUC_REFLIST);
    if (!cucRefList)
        throw UMC_ERR_ALLOC;

    if (field)
    {
        mfxFrameReferenceCUC * refCUC = ((MFXFrame *)pFrame)->GetRefCUC();
        Ipp32s number = pFrame->m_bottom_field_flag[field];
        memcpy(&(refCUC->PicParam[number]), &(refCUC->PicParam[!number]), sizeof(refCUC->PicParam[0]));
    }

    Ipp32s j = 0;
    for (H264DecoderFrame * pFrm = m_pDecodedFramesList->head(); pFrm; pFrm = pFrm->future())
    {
        //VM_ASSERT(j < dpbSize);
        if (!pFrm->IsFrameExist())
            continue;

        if (!pFrm->isShortTermRef() && !pFrm->isLongTermRef())
            continue;

        cucRefList->Refs[j] = ((MFXFrame *)pFrm)->GetRefCUC();

        cucRefList->Refs[j]->IsTopShortRef  = pFrm->isShortTermRef(pFrm->GetNumberByParity(0));
        cucRefList->Refs[j]->IsBottomShortRef  = pFrm->isShortTermRef(pFrm->GetNumberByParity(1));
        cucRefList->Refs[j]->IsTopLongRef  = pFrm->isLongTermRef(pFrm->GetNumberByParity(0));
        cucRefList->Refs[j]->IsBottomLongRef  = pFrm->isLongTermRef(pFrm->GetNumberByParity(1));

        cucRefList->Refs[j]->LongFrameNum = pFrm->LongTermFrameIdx();

        cuc->FrameSurface->Data[j + 1] = ((MFXFrame *)pFrm)->GetCUC()->FrameSurface->Data[0];
        j++;
    }

    DBPUpdate(pFrame, field);

    // skipping algorithm
    {
        if ((slicesInfo->IsField() && field || !slicesInfo->IsField()) &&
            IsShouldSkipFrame(pFrame, field))
        {
            if (slicesInfo->IsField())
            {
                pFrame->GetAU(0)->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
                pFrame->GetAU(1)->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
            }
            else
            {
                pFrame->GetAU(0)->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
            }

            pFrame->OnDecodingCompleted();

            pFrame->unSetisShortTermRef(0);
            pFrame->unSetisShortTermRef(1);
            pFrame->unSetisLongTermRef(0);
            pFrame->unSetisLongTermRef(1);
            pFrame->SetBusyState(0);
            pFrame->SetSkipped(true);
            return;
        }
        else
        {
            if (IsShouldSkipDeblocking(pFrame, field))
            {
                pFrame->GetAU(field)->SkipDeblocking();
            }
        }
    }

    if (!slicesInfo->GetSlice(0)->IsSliceGroups())
    {
        Ipp32s count = slicesInfo->GetSliceCount();

        H264Slice * pFirstSlice = 0;
        for (Ipp32s j = 0; j < count; j ++)
        {
            H264Slice * pSlice = slicesInfo->GetSlice(j);
            if (!pFirstSlice || pSlice->GetStreamFirstMB() < pFirstSlice->GetStreamFirstMB())
            {
                pFirstSlice = pSlice;
            }
        }

        if (pFirstSlice->GetStreamFirstMB())
        {
            //m_pSegmentDecoder[0]->RestoreErrorRect(0, pFirstSlice->m_iFirstMB, pFirstSlice);
        }

        for (Ipp32s i = 0; i < count; i ++)
        {
            H264Slice * pCurSlice = slicesInfo->GetSlice(i);

    #define MAX_MB_NUMBER 0x7fffffff

            Ipp32s minFirst = MAX_MB_NUMBER;
            for (Ipp32s j = 0; j < count; j ++)
            {
                H264Slice * pSlice = slicesInfo->GetSlice(j);
                if (pSlice->GetStreamFirstMB() > pCurSlice->GetStreamFirstMB() &&
                    minFirst > pSlice->GetStreamFirstMB())
                {
                    minFirst = pSlice->GetStreamFirstMB();
                }
            }

            if (minFirst != MAX_MB_NUMBER)
            {
                pCurSlice->SetMaxMB(minFirst);
            }
        }
    }

    slicesInfo->SetStatus(H264DecoderFrameInfo::STATUS_FILLED);
}

Status AVCController::InitFreeFrame(H264DecoderFrame * pFrame, H264Slice *pSlice)
{
    Status umcRes = UMC_OK;
    H264SeqParamSet *pSeqParam = pSlice->GetSeqParam();
    IppiSize dimensions;
    Ipp32s iMBWidth = pSeqParam->frame_width_in_mbs;
    Ipp32s iMBHeight = pSeqParam->frame_height_in_mbs;

    dimensions.width = iMBWidth * 16;
    dimensions.height = iMBHeight * 16;

    Ipp32s iMBCount = pSeqParam->frame_width_in_mbs * pSeqParam->frame_height_in_mbs;
    pFrame->totalMBs = iMBCount;

    Ipp32s chroma_format_idc = pFrame->IsAuxiliaryFrame() ? 0 : pSeqParam->chroma_format_idc;

    Ipp8u bit_depth_luma, bit_depth_chroma;
    if (pFrame->IsAuxiliaryFrame())
    {
        bit_depth_luma = pSlice->GetSeqParamEx()->bit_depth_aux;
        bit_depth_chroma = 8;
    } else {
        bit_depth_luma = pSeqParam->bit_depth_luma;
        bit_depth_chroma = pSeqParam->bit_depth_chroma;
    }

    pFrame->m_FrameType = SliceTypeToFrameType(pSlice->GetSliceHeader()->slice_type);
    pFrame->m_dFrameTime = pSlice->m_dTime;
    pFrame->m_crop_left = SubWidthC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_left_offset;
    pFrame->m_crop_right = SubWidthC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_right_offset;
    pFrame->m_crop_top = SubHeightC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_top_offset * (2 - pSeqParam->frame_mbs_only_flag);
    pFrame->m_crop_bottom = SubHeightC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_bottom_offset * (2 - pSeqParam->frame_mbs_only_flag);
    pFrame->m_crop_flag = pSeqParam->frame_cropping_flag;

    pFrame->setFrameNum(pSlice->GetSliceHeader()->frame_num);

    if (pSeqParam->aspect_ratio_idc == 255)
    {
        pFrame->m_aspect_width  = pSeqParam->sar_width;
        pFrame->m_aspect_height = pSeqParam->sar_height;
    }
    else
    {
        pFrame->m_aspect_width  = SAspectRatio[pSeqParam->aspect_ratio_idc][0];
        pFrame->m_aspect_height = SAspectRatio[pSeqParam->aspect_ratio_idc][1];
    }

    if (pSlice->GetSliceHeader()->field_pic_flag)
    {
        pFrame->m_bottom_field_flag[0] = pSlice->GetSliceHeader()->bottom_field_flag;
        pFrame->m_bottom_field_flag[1] = !pSlice->GetSliceHeader()->bottom_field_flag;

        pFrame->m_PictureStructureForRef =
        pFrame->m_PictureStructureForDec = FLD_STRUCTURE;
    }
    else
    {
        pFrame->m_bottom_field_flag[0] = 0;
        pFrame->m_bottom_field_flag[1] = 1;

        if (pSlice->GetSliceHeader()->MbaffFrameFlag)
        {
            pFrame->m_PictureStructureForRef =
            pFrame->m_PictureStructureForDec = AFRM_STRUCTURE;
        }
        else
        {
            pFrame->m_PictureStructureForRef =
            pFrame->m_PictureStructureForDec = FRM_STRUCTURE;
        }
    }

    // Allocate the frame data
    umcRes = pFrame->allocate(dimensions,
                            IPP_MAX(bit_depth_luma, bit_depth_chroma),
                            chroma_format_idc);
    if (umcRes == UMC_OK)
        umcRes = pFrame->allocateParsedFrameData();

    return umcRes;
}

H264DecoderFrame * AVCController::AddFrame(H264Slice *pSlice)
{
    if (!pSlice)
        return 0;

    H264DecoderFrame *pFrame = GetFreeFrame();

    if (!pFrame)
    {
        return 0;
    }

    pFrame->SetNotifiersChain(m_DefaultNotifyChain);
    m_DefaultNotifyChain.Abort();

    pFrame->SetBusyState(BUSY_STATE_NOT_DECODED);

    m_field_index = 0;

    if (pSlice->IsField())
    {
        pFrame->GetAU(1)->SetStatus(H264DecoderFrameInfo::STATUS_NOT_FILLED);
    }

    if (!m_pFirstUncompletedFrame)
        m_pFirstUncompletedFrame = pFrame;

    Status umcRes = InitFreeFrame(pFrame, pSlice);
    if (umcRes != UMC_OK)
    {
        return 0;
    }

    Ipp32s index = m_iCurrentResource % (m_iThreadNum);

    m_iCurrentResource++;
    m_iCurrentResource = m_iCurrentResource % (m_iThreadNum);

    index += (Ipp32s)pFrame->IsAuxiliaryFrame()*(m_iThreadNum);

    // allocate decoding data
    pFrame->m_iResourceNumber = index;

    //fill chroma planes in case of 4:0:0
    if (pFrame->m_chroma_format == 0)
    {
        pFrame->DefaultFill(2, true);
    }

    InitFrame(pFrame, pSlice);
    // frame gaps analysis
    return pFrame;
} // H264DecoderFrame * AVCController::AddFrame(H264Slice *pSlice)

void AVCController::InitFrame(H264DecoderFrame * pFrame, H264Slice *pSlice)
{
    H264SliceHeader *sliceHeader = pSlice->GetSliceHeader();
    if (sliceHeader->idr_flag)
    {
        m_PicOrderCnt = 0;
        m_PicOrderCntMsb = 0;
        m_PicOrderCntLsb = 0;
        m_FrameNum = sliceHeader->frame_num;
        m_PrevFrameRefNum = sliceHeader->frame_num;
        m_FrameNumOffset = 0;
        m_TopFieldPOC = 0;
        m_BottomFieldPOC = 0;
    }

    DecodePictureOrderCount(pSlice, sliceHeader->frame_num);

    pFrame->m_bIDRFlag = sliceHeader->idr_flag != 0;

    if (pFrame->m_bIDRFlag)
    {
        m_pDecodedFramesList->IncreaseRefPicListResetCount(pFrame);
    }

    pFrame->setFrameNum(sliceHeader->frame_num);

    if (sliceHeader->field_pic_flag == 0)
        pFrame->setPicNum(sliceHeader->frame_num, 0);
    else
        pFrame->setPicNum(sliceHeader->frame_num*2+1, m_field_index);

    //transfer previosly calculated PicOrdeCnts into current Frame
    if (pFrame->m_PictureStructureForRef < FRM_STRUCTURE)
    {
        pFrame->setPicOrderCnt(m_PicOrderCnt, m_field_index);
        if (!m_field_index) // temporally set same POC for second field
            pFrame->setPicOrderCnt(m_PicOrderCnt, 1);
    }
    else
    {
        pFrame->setPicOrderCnt(m_TopFieldPOC, 0);
        pFrame->setPicOrderCnt(m_BottomFieldPOC, 1);
    }

    DEBUG_PRINT((VM_STRING("Init frame POC - %d, %d, field - %d, uid - %d, frame_num - %d\n"), pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1], m_field_index, pFrame->m_UID, pFrame->m_FrameNum));

    pFrame->InitRefPicListResetCount(m_field_index);
} // void AVCController::InitFrame(H264DecoderFrame * pFrame, H264Slice *pSlice)

void AVCController::AddSliceToFrame(H264DecoderFrame *pFrame, H264Slice *pSlice)
{
    H264DecoderFrameInfo * au_info = pFrame->GetAU(m_field_index);
    Ipp32s iSliceNumber = au_info->GetSliceCount() + 1;

    if (m_field_index)
    {
        iSliceNumber += pFrame->m_TopSliceCount;
    }
    else
    {
        pFrame->m_TopSliceCount++;
    }

    pFrame->m_iNumberOfSlices++;

    pSlice->SetSliceNumber(iSliceNumber);
    pSlice->SetCurrentFrame(pFrame);
    au_info->AddSlice(pSlice);
}

void AVCController::DBPUpdate(H264DecoderFrame * pFrame, Ipp32s field)
{
    H264Slice * pSlice = pFrame->GetAU(field)->GetSlice(0);
    if (pSlice->IsReference())
        UpdateRefPicMarking(pFrame, pSlice, field);
}

void AVCController::AnalyzeSource(mfxBitstream *in)
{
    UMC::MediaData src, dst;

    ConvertMfxBSToMediaData(in, &src);
    AddSource(&src, &dst);
    AddSlice(0, 0);
    //m_pCurrentFrame = 0;
}

mfxFrameCUC * AVCController::GetCUCToDecode()
{
    m_pFirstUncompletedFrame = m_pDecodedFramesList->head();
    for (; m_pFirstUncompletedFrame; m_pFirstUncompletedFrame = m_pFirstUncompletedFrame->future())
    {
        if (!((MFXFrame *)m_pFirstUncompletedFrame)->IsFrameCompleted())
        {
            //DEBUG_PRINT(("Not completed POC - %d, busy - %d, umcRes - %d \n", m_pFirstUncompletedFrame->m_PicOrderCnt[0], m_pFirstUncompletedFrame->GetBusyState(), umcRes));
            break;
        }

        /*if (m_pFirstUncompletedFrame->GetBusyState() >= BUSY_STATE_NOT_DECODED)
        {
            DEBUG_PRINT((VM_STRING("Decode POC - %d completed - %d\n"), m_pFirstUncompletedFrame->m_PicOrderCnt[0], frame_count++));
            m_pFirstUncompletedFrame->OnDecodingCompleted();
        }*/
    }

    if (!m_pFirstUncompletedFrame)
        return 0;

    mfxFrameCUC * cuc = ((MFXFrame *)m_pFirstUncompletedFrame)->GetCUC();

    return cuc;
}

void AVCController::AfterDecoding(mfxFrameCUC * cuc)
{
    MFXFrame * pFrame = (MFXFrame *)m_pDecodedFramesList->head();
    for (; pFrame; pFrame = (MFXFrame *)(pFrame->future()))
    {
        if (pFrame->GetCUC() == cuc)
        {
            PrepareReferenceCUC(cuc, (MFXFrame *)pFrame);

            mfxU32 field = cuc->FrameParam->FieldPicFlag ? pFrame->GetNumberByParity(cuc->FrameParam->MbaffFrameFlag) : 0;
            pFrame->GetAU(field)->SetStatus(UMC::H264DecoderFrameInfo::STATUS_COMPLETED);

            if (((MFXFrame *)pFrame)->IsFrameCompleted())
            {
                pFrame->OnDecodingCompleted();
                m_pCurrentFrame = 0;
                m_pFirstUncompletedFrame = 0;
            }
        }
    }
}

mfxFrameCUC * AVCController::GetCUCToDisplay(bool force)
{
    UMC::MediaData dst;

    H264DecoderFrame * pFrame = GetFrameToDisplayInternal(&dst, force);
    if (pFrame)
    {
        pFrame->setWasOutputted();
        return ((MFXFrame *)pFrame)->GetCUC();
    }

    return 0;
}

void AVCController::PrepareReferenceCUC(mfxFrameCUC * cuc, MFXFrame * frm)
{
    mfxFrameReferenceCUC *refCUC = frm->GetRefCUC();

    mfxU32 bottom = cuc->FrameParam->FieldPicFlag ? cuc->FrameParam->MbaffFrameFlag : 0;
    refCUC->MbParam = cuc->MbParam;
    refCUC->NumMb = cuc->NumMb;
    refCUC->NumSlice[bottom] = cuc->NumSlice;

    refCUC->FieldPicFlag = cuc->FrameParam->FieldPicFlag;
    refCUC->MbaffFrameFlag = cuc->FrameParam->MbaffFrameFlag;

    mfxExtAvcDxvaPicParam *picParam = GetExtBuffer<mfxExtAvcDxvaPicParam>(cuc, MFX_LABEL_EXTHEADER, MFX_CUC_AVC_EXTHEADER);
    if (!picParam)
        throw 1;

    memcpy(&(refCUC->PicParam[bottom]), picParam, sizeof(refCUC->PicParam[0]));

    if (cuc->FrameParam->FieldPicFlag)
    {
        memcpy(&(refCUC->PicParam[bottom]), picParam, sizeof(refCUC->PicParam[0]));
    }
    else
    {
        memcpy(&(refCUC->PicParam[0]), picParam, sizeof(refCUC->PicParam[0]));
        memcpy(&(refCUC->PicParam[1]), picParam, sizeof(refCUC->PicParam[0]));
    }

    AvcReferenceParamSlice *refParams = GetExtBuffer<AvcReferenceParamSlice>(cuc, MFX_LABEL_REFINDEX, MFX_CUC_AVC_REFLABEL);
 
    for (Ipp32s i = 0; i < refCUC->NumSlice[bottom]; i++)
    {
        memcpy(&(refCUC->RefList[bottom][i]), &(refParams[i]), sizeof(AvcReferenceParamSlice));
    }

    ExtH264MVValues * mvs = GetExtBuffer<ExtH264MVValues>(cuc, MFX_LABEL_MVDATA, MFX_CUC_AVC_MV);

    refCUC->MvData = mvs;

    refCUC->FrameNum = cuc->FrameCnt;
}
} // namespace UMC

#endif // MFX_ENABLE_H264_VIDEO_DECODE
