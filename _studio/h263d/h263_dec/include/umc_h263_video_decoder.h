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

#include "umc_defs.h"

#ifndef __UMC_H263_VIDEO_DECODER_H
#define __UMC_H263_VIDEO_DECODER_H

#include "ipps.h"
#include "ippvc.h"
#include "umc_structures.h"
#include "umc_video_decoder.h"
#include "h263.h"

namespace UMC
{

class H263VideoDecoder : public VideoDecoder
{
public:
    // Default constructor
    H263VideoDecoder(void);
    // Destructor
    virtual ~H263VideoDecoder(void);
    // Initialize for subsequent frame decoding.
    virtual Status Init(BaseCodecParams *init);
    // Get next frame
    virtual Status GetFrame(MediaData* in, MediaData* out);
    // Close decoding & free all allocated resources
    virtual Status Close(void);
    // Reset decoder to initial state
    virtual Status  Reset(void);
    // Get decoder info
    virtual Status GetInfo(BaseCodecParams* info);
    // get pointers to internal current frame
//    h263_Frame* GetCurrentFramePtr(void);
    // Get performance
    Status  GetPerformance(Ipp64f *perf);
    //reset skip frame counter
    Status  ResetSkipCount();
    // increment skip frame counter
    Status  SkipVideoFrame(Ipp32s);
    // get skip frame counter statistic
    Ipp32u  GetNumOfSkippedFrames();
protected:
    bool                    m_IsInit, m_IsInitBase;
    Ipp64f                  m_dec_time, m_dec_time_prev, m_dec_time_base, m_dec_time_fr, m_dec_time_frinc;
    Ipp32s                  m_buffered_frame, m_is_skipped_b, m_skipped_fr, m_b_prev;
    bool                    m_P_part_of_PB_pending;
    VideoDecoderParams      m_Param;
    h263_Info               *m_decInfo;
    Status                  InsideInit(VideoDecoderParams *pParam);
    // allocate frame memory
    Status AllocateInitFrame(h263_Frame* pFrame);
    // lock frame memory
    void LockFrame(h263_Frame* pFrame);
    // allocate memory for internal buffers
    Status AllocateBuffers();
    // free memory for internal buffers
    Status FreeBuffers();
    // lock memory for internal buffers
    void LockBuffers();
    // unlock memory for internal buffers
    Status UnlockBuffers();
    Status ReAlLockFrame(h263_Frame* pFrame);
    Status ReAlLockBuffers(Ipp32s resizeMask);
    void LockEnhancedLayerBuffers(Ipp32s enh_layer_num, Ipp32s ref_layer_num, Ipp32s lockedMask);
    Status UnlockEnhancedLayerBuffers(Ipp32s enh_layer_num, Ipp32s ref_layer_num);
};

} // end namespace UMC

#endif //__UMC_H263_VIDEO_DECODER_H
