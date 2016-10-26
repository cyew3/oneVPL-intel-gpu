//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#include "base_video_render.h"

namespace UMC
{

#define MODULE          "BaseVideoRender:"
#define FUNCTION        "Unknown"
#define DBG_SET(msg)    VIDEO_DRV_DBG_SET   (MODULE, FUNCTION, msg)

BaseVideoRender::BaseVideoRender():
    m_iBuffersNum(0),
    m_iReadIndex(-1),
    m_iLastFrameIndex(-1),
    m_iWriteIndex(-1),
    m_bPrevIp(false),
    m_bShow(true),
    m_bReorder(false),
    m_bRenderingError(false)
{
    memset(m_Buffers, 0, sizeof(m_Buffers));
    assert(!"Using memset on m_Buffers is incorrect");
    memset(&m_dst_rect, 0, sizeof(m_dst_rect));
    memset(&m_src_rect, 0, sizeof(m_src_rect));

    m_SrcInfo.width = m_SrcInfo.height = 10;
    m_bPrepareForReposition = false;
} // BaseVideoRender::BaseVideoRender():

BaseVideoRender::~BaseVideoRender()
{
    Close();

} // BaseVideoRender::~BaseVideoRender()

// Initialize the render, return false if failed, call Close anyway
Status BaseVideoRender::Init(MediaReceiverParams *pInit)
{
    Status umcRes = UMC_OK;

    VideoRenderParams* pParams = DynamicCast<VideoRenderParams>(pInit);

    if (NULL == pParams)
        umcRes = UMC_ERR_NULL_PTR;

    m_bPrepareForReposition = false;
    m_bPrevIp = false;
    m_bStop   = false;
    if (NONE == pParams->out_data_template.GetColorFormat())
        m_SrcInfo = pParams->info;
    else
    {
        m_SrcInfo.width  = pParams->out_data_template.GetWidth();
        m_SrcInfo.height = pParams->out_data_template.GetHeight();
    }
    m_bShow = true;
    m_iWriteIndex = -1;
    m_iReadIndex = -1;
    m_iLastFrameIndex = -1;
    m_bReorder = pParams->lFlags & FLAG_VREN_REORDER;

    VM_ASSERT(2 < MIN_FRAME_BUFFERS);
    if (UMC_OK == umcRes)
        umcRes = m_hDoneBufSema.Init(0);

    if (UMC_OK == umcRes)
    {
        umcRes = m_hFreeBufSema.Init(m_iBuffersNum - 2);
        m_hFreeBufSemaCount = m_iBuffersNum - 2;
    }

    if (UMC_OK == umcRes)
        umcRes = ResizeDisplay(pParams->disp, pParams->range);

    if (UMC_OK != umcRes)
        Close();

    return umcRes;

} // Status BaseVideoRender::Init(MediaReceiverParams *pInit)

Status BaseVideoRender::Stop()
{
    Status umcRes = UMC_OK;
    VideoData InData;

    if (m_bStop)
        return umcRes;

    m_SuncMut.Lock();

    if (UMC_OK == umcRes)
    {
        if (m_iWriteIndex < 0)
            m_Buffers[0].frame_time = -1;
        else
        {
            //Lock buffer for end
#ifndef SINGLE_THREAD_VIDEO_RENDER
            if (UMC_OK == umcRes)
                umcRes = m_hFreeBufSema.Wait(MAX_FRAME_BUFFERS * 40);
#endif

            if (UMC_OK == umcRes)
            {
                // m_SuncMut.Lock();
                m_hFreeBufSemaCount--;

                if (++m_iWriteIndex >= m_iBuffersNum)
                    m_iWriteIndex = 0;

                // m_SuncMut.Unlock();
            }

            // Unlock it immidiatly
            // m_SuncMut.Lock();
            m_Buffers[m_iWriteIndex].frame_time = -1;
            // m_SuncMut.Unlock();
        }
        m_hDoneBufSema.Signal();
    }
    m_bStop   = true;
    m_SuncMut.Unlock();
    return umcRes;

}

Status BaseVideoRender::LockInputBuffer(MediaData *pInData)
{
#undef  FUNCTION
#define FUNCTION "LockInputBuffer"
    DBG_SET ("+");
    Status umcRes = UMC_OK;

    VideoData *pVideoData = DynamicCast<VideoData> (pInData);

    if (NULL == pVideoData)
        umcRes = UMC_ERR_NULL_PTR;

    m_LockBufMut.Lock();
    if (UMC_OK == umcRes)
    {
        if (m_bRenderingError)
        {
            DBG_SET(VM_STRING("critical error in RenderFrame"));
            umcRes = UMC_ERR_FAILED;
        }
    }
#ifndef SINGLE_THREAD_VIDEO_RENDER
    if (UMC_OK == umcRes)
    {
        umcRes = m_hFreeBufSema.Wait(MAX_FRAME_BUFFERS * 40);
    }
#endif
    Ipp32s iPitch = 0;
    Ipp8u *pbVideoMemory = NULL;

    if (UMC_OK == umcRes)
    {
        m_SuncMut.Lock();

        Ipp32s saved_FreeBufSemaCount = m_hFreeBufSemaCount;
        Ipp32s saved_WriteIndex = m_iWriteIndex;

        m_hFreeBufSemaCount--;
        if (++m_iWriteIndex >= m_iBuffersNum)
            m_iWriteIndex = 0;

        iPitch = LockSurface(&pbVideoMemory);

        if (0 == iPitch)
        {
            /* Unlocking buffer if render can't lock surface (example - SDL). */
            m_hFreeBufSemaCount = saved_FreeBufSemaCount;
            m_iWriteIndex = saved_WriteIndex;
            m_hFreeBufSema.Signal();
            umcRes = UMC_ERR_ALLOC;
        }
        m_SuncMut.Unlock();
    }

    if (UMC_OK == umcRes)
    {
        umcRes = pVideoData->SetSurface(pbVideoMemory, iPitch);
        //pVideoData->SetPlanePointer(pbVideoMemory, 0);
        //pVideoData->SetPlanePitch(iPitch, 0);
    }
    if (UMC_OK != umcRes)
    {
        m_LockBufMut.Unlock();
    }
    DBG_SET ("-");
    return umcRes;
} // Status BaseVideoRender::LockInputBuffer(MediaData *pInData)

Status BaseVideoRender::UnLockInputBuffer(MediaData* pInData, Status StreamStatus)
{
    Status umcRes = (m_iWriteIndex >= 0) ? UMC_OK : UMC_ERR_NOT_INITIALIZED;
    VideoData *pVideoData = DynamicCast<VideoData> (pInData);

    if (NULL == pVideoData)
        return UMC_ERR_NULL_PTR;
    if (UMC_OK == umcRes)
    {
        if (m_bRenderingError)
        {
            DBG_SET(VM_STRING("critical error in RenderFrame"));
            umcRes = UMC_ERR_FAILED;
        }
    }
    if (UMC_OK == umcRes)
    {
        Ipp32s width = pVideoData->GetWidth();
        Ipp32s height = pVideoData->GetHeight();

        if(m_SrcInfo.width != width)
        {
            m_SrcInfo.width = width;
            ResizeSource();
        }
        if(m_SrcInfo.height != height)
        {
            m_SrcInfo.height = height;
            ResizeSource();
        }

        if (UMC_OK == umcRes)
        {
            m_SuncMut.Lock();

            Ipp64f dfEndTime;
            pVideoData->GetTime(m_Buffers[m_iWriteIndex].frame_time, dfEndTime);
            m_Buffers[m_iWriteIndex].frame_type = pVideoData->GetFrameType();

            UnlockSurface(0);
            if (m_bReorder)
            {
                if (!m_bPrevIp)
                {
                    if (pVideoData->GetFrameType() == B_PICTURE)
                        m_hDoneBufSema.Signal();
                    else
                        m_bPrevIp = true;
                }
                else
                {
                    if (pVideoData->GetFrameType() == B_PICTURE)
                    {
                        Ipp32s prev_index = (!m_iWriteIndex) ?
                                        (m_iBuffersNum - 1) : (m_iWriteIndex - 1);
                        sFrameBuffer tmp = m_Buffers[m_iWriteIndex];
                        m_Buffers[m_iWriteIndex] = m_Buffers[prev_index];
                        m_Buffers[prev_index] = tmp;
                    }
                    m_hDoneBufSema.Signal();
                }
            }
            else
                m_hDoneBufSema.Signal();

            m_SuncMut.Unlock();
        }
    }
    if (UMC_ERR_END_OF_STREAM == StreamStatus)
    {
        Stop();
    }
    if (UMC_OK == umcRes)
    {
        m_LockBufMut.Unlock();
    }
    return umcRes;

} // Status BaseVideoRender::UnLockInputBuffer(MediaData* pInData, Status StreamStatus)

// Peek presentation of next frame, return presentation time
Status BaseVideoRender::GetRenderFrame(Ipp64f *pTime)
{
    Status umcRes   = UMC_OK;
    Ipp64f dTime    = -1;

    if (UMC_OK == umcRes)
    {
        if (NULL == pTime) umcRes = UMC_ERR_NULL_PTR;
    }
    if (UMC_OK == umcRes)
    {
        if (m_bStop || m_bPrepareForReposition || m_bRenderingError)
            umcRes = UMC_ERR_FAILED;
    }
#ifndef SINGLE_THREAD_VIDEO_RENDER
    if (UMC_OK == umcRes)
    {
        umcRes = m_hDoneBufSema.Wait(MAX_FRAME_BUFFERS * 40);
    }
#endif
    if (UMC_OK == umcRes)
    {
        m_SuncMut.Lock();
        if (++m_iReadIndex >= m_iBuffersNum)
            m_iReadIndex = 0;
        dTime = m_Buffers[m_iReadIndex].frame_time;
        m_SuncMut.Unlock();
    }
    if (NULL != pTime) *pTime = dTime;
    return umcRes;
} // Status BaseVideoRender::GetRenderFrame(Ipp64f*)

// Resize the display rectangular
Status BaseVideoRender::ResizeDisplay(UMC::sRECT &disp, UMC::sRECT &range)
{
    m_SuncMut.Lock();

    m_disp = disp;
    m_range= range;
    // Scale the display while keeping the size ratio
    Ipp64f scaled_width = m_SrcInfo.width;
    Ipp64f scaled_height = m_SrcInfo.height;
    Ipp16s width = (Ipp16s)(disp.right - disp.left);
    Ipp16s height = (Ipp16s)(disp.bottom - disp.top);
    Ipp64f rxy = IPP_MIN((Ipp64f)width/scaled_width,(Ipp64f)height/scaled_height);
    scaled_width *= rxy;
    scaled_height *= rxy;

    UMC::sRECT tmp_dst;
    tmp_dst.left = (Ipp16s)(disp.left + (width - (Ipp16s)scaled_width)/2);
    tmp_dst.right = (Ipp16s)(tmp_dst.left + (Ipp16s)scaled_width);
    tmp_dst.top = (Ipp16s)(disp.top + (height - (Ipp16s)scaled_height)/2);
    tmp_dst.bottom = (Ipp16s)(tmp_dst.top + (Ipp16s)scaled_height);

    UMC::sRECT tmp_src;
    tmp_src.right = (Ipp16s)m_SrcInfo.width;
    tmp_src.bottom = (Ipp16s)m_SrcInfo.height;
    tmp_src.left= 0;
    tmp_src.top = 0;
    if(tmp_dst.left < 0)
    {
        tmp_dst.right = (Ipp16s)(tmp_dst.right - tmp_dst.left);
        tmp_dst.left   = 0;
    }
    if(m_disp.left < 0)
    {
        m_disp.right = (Ipp16s)(m_disp.right - m_disp.left);
        m_disp.left   = 0;
    }

    // Remove caption information from the clips
    if (tmp_src.bottom == 1088)
        tmp_src.bottom = 1080;
    if (tmp_src.bottom == 544)
        tmp_src.bottom = 540;
    if (tmp_src.bottom == 272)
        tmp_src.bottom = 270;

    // Adjust m_src_rect & m_dst_rect to the work area
    if (((tmp_dst.left + 15) & (~15)) >= range.right ||
        (tmp_dst.right & (~15)) <= range.left ||
        tmp_dst.top >= range.bottom ||
        tmp_dst.bottom <= range.top)
    {
        //  Out of screen
        m_bShow = false;
    }
    else
    {
        if (tmp_dst.left < range.left)
        {
            tmp_src.left = (Ipp16s)(tmp_src.left +
                                   (range.left - tmp_dst.left) / rxy);
            tmp_dst.left = range.left;
        }
        if (tmp_dst.right > range.right)
        {
            tmp_src.right = (Ipp16s)(tmp_src.right -
                                    (tmp_dst.right - range.right) / rxy);
            tmp_dst.right = range.right;
        }
        if (tmp_dst.top < range.top)
        {
            tmp_src.top = (Ipp16s)(tmp_src.top +
                                  (range.top - tmp_dst.top) / rxy);
            tmp_dst.top = range.top;
        }
        if (tmp_dst.bottom > range.bottom)
        {
            tmp_src.bottom = (Ipp16s)(tmp_src.bottom -
                                     (tmp_dst.bottom - range.bottom) / rxy);
            tmp_dst.bottom = range.bottom;
        }
    }

    // Align the width to multiples of 16
    //tmp_src.left  = (Ipp16s)((tmp_src.left + 15) & (~15));
    //tmp_src.right = (Ipp16s)((tmp_src.right + 15) & (~15));

    if ((tmp_dst.left & 15) < 8)
        tmp_dst.left = (Ipp16s)(tmp_dst.left  & (~15));
    else
        tmp_dst.left = (Ipp16s)((tmp_dst.left + 15)  & (~15));

    if ((tmp_dst.right & 15) < 8)
        tmp_dst.right = (Ipp16s)(tmp_dst.right & (~15));
    else
        tmp_dst.right = (Ipp16s)((tmp_dst.right + 15)  & (~15));

    m_src_rect = tmp_src;
    m_dst_rect = tmp_dst;

    m_SuncMut.Unlock();

    return UMC_OK;

} // Status BaseVideoRender::ResizeDisplay(UMC::sRECT &disp, UMC::sRECT &range)

Status BaseVideoRender::ResizeSource()
{
    m_SuncMut.Lock();

    UMC::sRECT disp = m_disp;
    UMC::sRECT range = m_range;
    // Scale the display while keeping the size ratio
    Ipp64f scaled_width = m_SrcInfo.width;
    Ipp64f scaled_height = m_SrcInfo.height;
    Ipp16s width = (Ipp16s)(disp.right - disp.left);
    Ipp16s height = (Ipp16s)(disp.bottom - disp.top);
    Ipp64f rxy = IPP_MIN((Ipp64f)width/scaled_width,(Ipp64f)height/scaled_height);
    scaled_width *= rxy;
    scaled_height *= rxy;

    UMC::sRECT tmp_src;
    tmp_src.right = (Ipp16s)m_SrcInfo.width;
    tmp_src.bottom = (Ipp16s)m_SrcInfo.height;
    tmp_src.left= 0;
    tmp_src.top = 0;

    // Remove caption information from the clips
    if (tmp_src.bottom == 1088)
        tmp_src.bottom = 1080;
    if (tmp_src.bottom == 544)
        tmp_src.bottom = 540;
    if (tmp_src.bottom == 272)
        tmp_src.bottom = 270;

    // Align the width to multiples of 16
    tmp_src.left  = (Ipp16s)((tmp_src.left + 15) & (~15));
    tmp_src.right = (Ipp16s)((tmp_src.right + 15) & (~15));

    m_src_rect = tmp_src;

    m_SuncMut.Unlock();

    return UMC_OK;

} // Status BaseVideoRender::ResizeSource()

// Show/Hide Surface
void BaseVideoRender::ShowSurface()
{
    m_bShow = true;

} // void BaseVideoRender::ShowSurface()

void BaseVideoRender::HideSurface()
{
    m_bShow = false;

} // void BaseVideoRender::HideSurface()

// Terminate the render
Status BaseVideoRender::Close()
{
    m_SuncMut.Lock();
    m_iReadIndex = -1;
    m_iWriteIndex = -1;
    m_iLastFrameIndex = -1;
    m_iBuffersNum = 0;
    m_bShow = true;
    m_bReorder = false;
    m_OutDataTemplate.Close();
    m_SuncMut.Unlock();

    return UMC_OK;

} // Status BaseVideoRender::Close()

Status BaseVideoRender::Reset()
{
    m_bPrevIp = false;
    m_bStop   = false;
    m_bShow = true;
    m_iWriteIndex = -1;
    m_iReadIndex = -1;
    m_iLastFrameIndex = -1;

    m_hDoneBufSema.Init(0);

    for(Ipp32s i = 0; i < 8; i++)
        m_Buffers[i].frame_time = -1;
    m_hFreeBufSema.Init(m_iBuffersNum - 2);
    m_hFreeBufSemaCount = m_iBuffersNum - 2;

    m_bPrepareForReposition = false;

    return UMC_OK;

} // Status BaseVideoRender::Reset()

UMC::Status UMC::BaseVideoRender::PrepareForRePosition()
{
    m_bPrepareForReposition = true;

    return UMC_OK;
}

} // namespace UMC
