// Copyright (c) 2003-2018 Intel Corporation
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
