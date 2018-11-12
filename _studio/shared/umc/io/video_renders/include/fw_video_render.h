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

#ifndef __FW_VIDEO_RENDER_H__
#define __FW_VIDEO_RENDER_H__

#include "umc_defs.h"

#if defined (UMC_ENABLE_FILE_WRITER)

#include "umc_video_render.h"
#include "umc_event.h"
#include "umc_mutex.h"
#include "umc_thread.h"
#include "umc_cyclic_buffer.h"

namespace UMC
{

class FWVideoRenderParams : public VideoRenderParams
{
    DYNAMIC_CAST_DECL(FWVideoRenderParams, VideoRenderParams)

public:
    // Default constructor
    FWVideoRenderParams(void);

    vm_char *pOutFile;                                          // (vm_char *) output file name
};

// forward declaration of used type(s)
class BaseCodec;
class VideoData;
class FileWriter;

class FWVideoRender: public VideoRender
{
public:
    // Default constructor
    FWVideoRender(void);
    // Destructor
    virtual
    ~FWVideoRender(void);

    // Initialize the render, return false if failed, call Close anyway
    virtual
    Status Init(MediaReceiverParams* pInit);

    // Terminate the render
    virtual
    Status Close(void);

    // Lock input buffer
    virtual
    Status LockInputBuffer(MediaData *in);
    // Unlock input buffer
    virtual
    Status UnLockInputBuffer(MediaData *in, Status StreamStatus = UMC_OK);

    // Break waiting(s)
    virtual
    Status Stop(void);

    // Reset media receiver
    virtual
    Status Reset(void);

    // VideoRender interface extension above MediaReceiver

    // Peek presentation of next frame, return presentation time
    virtual
    Ipp64f GetRenderFrame(void);
    virtual
    Status GetRenderFrame(Ipp64f *pTime);

    // Rendering the current frame
    virtual
    Status RenderFrame(void);

    // Show the last rendered frame
    virtual
    Status ShowLastFrame(void) {return UMC_OK;}

    // Set/Reset full screen playback
    virtual
    Status SetFullScreen(ModuleContext &, bool) {return UMC_OK;}

    // Resize the display rectangular
    virtual
    Status ResizeDisplay(UMC::sRECT &, UMC::sRECT &) {return UMC_OK;}

    // Show/Hide Surface
    virtual
    void ShowSurface(void) {}
    virtual
    void HideSurface(void) {}

    // Do preparation before changing stream position
    virtual
    Status PrepareForRePosition(void) {return UMC_OK;}

protected:
    // Obsolete methods
    virtual
    Ipp32s LockSurface(Ipp8u **) {return 0;}
    virtual
    Ipp32s UnlockSurface(Ipp8u **) {return 0;}

    // Allocate internal buffers
    Status AllocateInternalBuffer(VideoData *pData);

    VideoRenderParams m_RenderParam;                            // (VideoRenderParams) renderer initialization parameters

    MediaBufferParams m_BufferParam;                            // (MediaBufferParams) buffer initialization parameters
    SampleBuffer m_Frames;                                      // (SampleBuffer) buffer to store decoded frames

    bool m_bCriticalError;                                      // (bool) critical error occured
    bool m_bStop;                                               // (bool) splitter was stopped
    FileWriter *m_pFileWriter;                                  // (FileWriter *) pointer to a file writer

    BaseCodec *m_pConverter;                                    // (BaseCodec *) pointer to a color converter
};

} // namespace UMC

#endif // defined (UMC_ENABLE_FILE_WRITER)

#endif // __FW_VIDEO_RENDER_H__
