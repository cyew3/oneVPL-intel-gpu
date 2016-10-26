//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2011 Intel Corporation. All Rights Reserved.
//

#ifndef __BASE_VIDEO_RENDER_H__
#define __BASE_VIDEO_RENDER_H__

#define MAX_FRAME_BUFFERS 8
#define MIN_FRAME_BUFFERS 4

#include "vm_debug.h"
#include "umc_mutex.h"
#include "umc_semaphore.h"
#include "umc_video_render.h"
#include "drv.h"

namespace UMC
{

class BaseVideoRender : public VideoRender
{
public:
    // Default constructor
    BaseVideoRender();

    // Destructor
    virtual ~BaseVideoRender();

    // Initialize the render, return false if failed, call Close anyway
    virtual Status Init(MediaReceiverParams* pInit);

    // Release all receiver resources
    virtual Status Close();

    // Lock input buffer
    virtual Status LockInputBuffer(MediaData *in);

    // Unlock input buffer
    virtual Status UnLockInputBuffer(MediaData *in,
                                     Status StreamStatus = UMC_OK);
    // Break waiting(s)
    virtual Status Stop();

    // Warning:
    // Outdated functions - don't use it, use MediaReceiver interface instead

    //  Temporary stub, will be removed in future
    virtual Status Init(VideoRenderParams& rInit)
    {   return Init(static_cast<MediaReceiverParams*>(&rInit)); }

    // VideoRender interface extension above MediaReceiver

    // Peek presentation of next frame, return presentation time
    virtual Status GetRenderFrame(Ipp64f *time);

    // Resize the display rectangular
    virtual Status ResizeDisplay(UMC::sRECT &disp, UMC::sRECT &range);

    // Show/Hide Surface
    virtual void ShowSurface();

    virtual void HideSurface();

    virtual Status Reset();

    virtual Status PrepareForRePosition();

protected:
    Status ResizeSource();
    struct sFrameBuffer
    {
        Ipp64f    frame_time;
        FrameType frame_type;
        void      *surface;
    };

    sFrameBuffer m_Buffers[MAX_FRAME_BUFFERS];

    Semaphore    m_hDoneBufSema;
    Semaphore    m_hFreeBufSema;
    Mutex        m_SuncMut;
    Mutex        m_LockBufMut;

    volatile Ipp32s m_hFreeBufSemaCount;

    Ipp32s       m_iBuffersNum;
    Ipp32s       m_iReadIndex;
    Ipp32s       m_iLastFrameIndex;
    Ipp32s       m_iWriteIndex;
    bool         m_bPrevIp;
    ClipInfo     m_SrcInfo;
    bool         m_bShow;
    UMC::sRECT    m_src_rect;
    UMC::sRECT    m_dst_rect;
    UMC::sRECT    m_disp;
    UMC::sRECT    m_range;
    bool         m_bReorder;
    bool         m_bStop;

    bool         m_bPrepareForReposition;
    // Flag for critical error in RenderFrame function - rendering couldn't
    // be continued.
    bool         m_bRenderingError;
};

} // namespace UMC

#endif // __BASE_VIDEO_RENDER_H__
