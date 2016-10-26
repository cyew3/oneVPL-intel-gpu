//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2014 Intel Corporation. All Rights Reserved.
//

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
