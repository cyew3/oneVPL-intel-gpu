// Copyright (c) 2004-2019 Intel Corporation
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

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#ifndef __UMC_VC1_VIDEO_DECODER_SW_H_
#define __UMC_VC1_VIDEO_DECODER_SW_H_

#include "umc_vc1_video_decoder.h"
#include "umc_vc1_dec_frame_descr.h"
#include "umc_vc1_dec_thread.h"
#include "umc_media_data_ex.h"
#include "umc_frame_allocator.h"

#include "umc_va_base.h"

#include "umc_vc1_dec_skipping.h"

namespace UMC
{

    class VC1TSHeap;
    class VC1VideoDecoderSW : public VC1VideoDecoder
    {
        friend class VC1TaskStore;

    public:
        // Default constructor
        VC1VideoDecoderSW();
        // Default destructor
        virtual ~VC1VideoDecoderSW();

        // Initialize for subsequent frame decoding.
        virtual Status Init(BaseCodecParams *init);

        // Close  decoding & free all allocated resources
        virtual Status Close(void);

        // Reset decoder to initial state
        virtual Status   Reset(void);

    protected:

        virtual void FreeTables(VC1Context* pContext);
        virtual bool InitTables(VC1Context* pContext);
        virtual   bool    InitAlloc                   (VC1Context* pContext, uint32_t MaxFrameNum);

        virtual Status VC1DecodeFrame                 (MediaData* in, VideoData* out_data);


        virtual uint32_t CalculateHeapSize();

        virtual bool InitVAEnvironment();

        VC1ThreadDecoder**         m_pdecoder;                              // (VC1ThreadDecoder *) pointer to array of thread decoders
        bool                       m_bIsFrameToOut;
        int32_t                     m_iRefFramesDst; // destination for reference frames
        int32_t                     m_iBFramesDst; // destination for B/BI frames
        VC1FrameDescriptor*        m_pPrevDescriptor;

        virtual UMC::Status GetAndProcessPerformedDS(UMC::MediaData* in, UMC::VideoData* out_data, UMC::VC1FrameDescriptor **pPerfDescriptor);
        virtual UMC::Status ProcessPrevFrame(UMC::VC1FrameDescriptor *pCurrDescriptor, UMC::MediaData* in, UMC::VideoData* out_dat);
        virtual UMC::FrameMemID  ProcessQueuesForNextFrame(bool& isSkip, mfxU16& Corrupted);
        virtual UMC::FrameMemID GetSkippedIndex(bool isIn = true);
        virtual Status RunThread(int threadNumber);
        virtual FrameMemID GetDisplayIndex(bool isDecodeOrder, bool isSamePolarSurf);
    private:
        VC1TaskStoreSW*              m_pStoreSW;
    };
}

#endif //__UMC_VC1_VIDEO_DECODER_SW_H_
#endif //MFX_ENABLE_VC1_VIDEO_DECODE

