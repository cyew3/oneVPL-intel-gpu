//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VP8_MFX_DECODE_HW__
#define __UMC_VP8_MFX_DECODE_HW__

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "ipps.h"
#include "umc_structures.h"

#include "umc_va_base.h"
#include "umc_vp8_mfx_decode.h"

#ifdef UMC_VA_DXVA

namespace UMC
{

class VP8VideoDecoderHardware: public VP8VideoDecoderSoftware
{

public:
   // Default constructor
  VP8VideoDecoderHardware(void);

  // Default destructor
  virtual ~VP8VideoDecoderHardware(void);

  // Initialize for subsequent frame decoding.
  virtual Status Init(BaseCodecParams *init);

  // Get next frame
  virtual Status GetFrame(MediaData* in, FrameData** out);

  // Close  decoding & free all allocated resources
  virtual Status Close(void);

  // Reset decoder to initial state
  virtual Status Reset(void);

  virtual void SetFrameAllocator(FrameAllocator * frameAllocator);

private:

  void RefreshFrames(void);

  Status PackHeaders(MediaData* src);

  Ipp8u                  m_IsInitialized;

  VideoDecoderParams    m_Params;

  vp8_FrameData m_FrameData[VP8_NUM_OF_REF_FRAMES];
  vp8_FrameData *m_CurrFrame;
  
  Ipp8u m_RefFrameIndx[VP8_NUM_OF_REF_FRAMES];

  VideoAccelerator *m_pVideoAccelerator;

};

} // namespace UMC

#endif // UMC_VA_DXVA

#endif // __UMC_VP8_MFX_DECODE_HW__
#endif // UMC_ENABLE_VP8_VIDEO_DECODER