//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

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
        virtual   bool    InitAlloc                   (VC1Context* pContext, Ipp32u MaxFrameNum);

        virtual Status VC1DecodeFrame                 (MediaData* in, VideoData* out_data);


        virtual Ipp32u CalculateHeapSize();

        virtual bool InitVAEnvironment();

        VC1ThreadDecoder**         m_pdecoder;                              // (VC1ThreadDecoder *) pointer to array of thread decoders
        bool                       m_bIsFrameToOut;
        Ipp32s                     m_iRefFramesDst; // destination for reference frames
        Ipp32s                     m_iBFramesDst; // destination for B/BI frames
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
#endif //UMC_ENABLE_VC1_VIDEO_DECODER

