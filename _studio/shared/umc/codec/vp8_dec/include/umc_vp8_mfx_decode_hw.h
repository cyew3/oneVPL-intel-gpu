// Copyright (c) 2011-2018 Intel Corporation
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