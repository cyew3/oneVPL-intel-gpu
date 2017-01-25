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

#ifndef __UMC_VC1_VIDEO_DECODER_HW_H_
#define __UMC_VC1_VIDEO_DECODER_HW_H_

#include "umc_vc1_video_decoder.h"
#include "umc_vc1_dec_frame_descr.h"
#include "umc_media_data_ex.h"
#include "umc_frame_allocator.h"

#include "umc_va_base.h"

#include "umc_vc1_dec_skipping.h"
#include "umc_vc1_dec_task_store.h"

using namespace UMC::VC1Exceptions;

#ifdef UMC_VA_DXVA
enum
{
    VC1_MAX_REPORTS = 32
};

#endif

class MFXVC1VideoDecoderHW;

namespace UMC
{

    class VC1TSHeap;
    class VC1VideoDecoderHW : public VC1VideoDecoder
    {
        friend class VC1TaskStore;

    public:
        // Default constructor
        VC1VideoDecoderHW();
        // Default destructor
        virtual ~VC1VideoDecoderHW();

        // Initialize for subsequent frame decoding.
        virtual Status Init(BaseCodecParams *init);

        virtual Status Reset(void);

        // Close  decoding & free all allocated resources
        virtual Status Close(void);

        void SetVideoHardwareAccelerator            (VideoAccelerator* va);

#ifdef UMC_VA_DXVA
        virtual Status GetStatusReport(DXVA_Status_VC1 *pStatusReport);
#endif

    protected:

        virtual   bool    InitAlloc                   (VC1Context* pContext, Ipp32u MaxFrameNum);

        virtual Status VC1DecodeFrame                 (MediaData* in, VideoData* out_data);


        virtual Ipp32u CalculateHeapSize();

        virtual bool   InitVAEnvironment            ();

        // HW i/f support
        virtual Status FillAndExecute(MediaData* in);
        void GetStartCodes_HW(MediaData* in, Ipp32u &sShift);

        MediaDataEx::_MediaDataEx* m_stCodes_VA;

        template <class Descriptor>
        Status VC1DecodeFrame_VLD(MediaData* in, VideoData* out_data)
        {
            Ipp32s SCoffset = 0;
            Descriptor* pPackDescriptorChild = NULL;
            bool skip = false;

            if ((VC1_PROFILE_ADVANCED != m_pContext->m_seqLayerHeader.PROFILE))
                // special header (with frame size) in case of .rcv format
                SCoffset = -VC1FHSIZE;

            m_pStore = m_pStore;
            Status umcRes = UMC_ERR_NOT_ENOUGH_DATA;
            try
            {
                // Get FrameDescriptor for Packing Data
                m_pStore->GetReadyDS(&pPackDescriptorChild);

                if (NULL == pPackDescriptorChild)
                    throw vc1_exception(internal_pipeline_error);


                pPackDescriptorChild->m_pContext->m_FrameSize = (Ipp32u)in->GetDataSize() + SCoffset;


                umcRes = pPackDescriptorChild->preProcData(m_pContext,
                    (Ipp32u)(m_frameData->GetDataSize() + SCoffset),
                    m_lFrameCount,
                    skip);
                if (UMC_OK != umcRes)
                {

                    if (UMC_ERR_NOT_ENOUGH_DATA == umcRes)
                        throw vc1_exception(invalid_stream);
                    else
                        throw vc1_exception(internal_pipeline_error);
                }
            }
            catch (vc1_exception ex)
            {
                if (invalid_stream == ex.get_exception_type())
                {
                    if (fast_err_detect == vc1_except_profiler::GetEnvDescript().m_Profile)
                        m_pStore->AddInvalidPerformedDS(pPackDescriptorChild);
                    else if (fast_decoding == vc1_except_profiler::GetEnvDescript().m_Profile)
                        m_pStore->ResetPerformedDS(pPackDescriptorChild);
                    else
                    {
                        // Error - let speak about it 
                        m_pStore->ResetPerformedDS(pPackDescriptorChild);
                        // need to free surfaces
                        if (!skip)
                        {
                            m_pStore->UnLockSurface(m_pContext->m_frmBuff.m_iCurrIndex);
                            m_pStore->UnLockSurface(m_pContext->m_frmBuff.m_iToSkipCoping);
                        }
                        // for smart decoding we should decode and reconstruct frame according to standard pipeline
                    }

                    return  UMC_ERR_NOT_ENOUGH_DATA;
                }
                else
                    return UMC_ERR_FAILED;
            }

            // if no RM - lets execute
            if ((!pPackDescriptorChild->m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG) &&
                (!pPackDescriptorChild->m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG) &&
                (!pPackDescriptorChild->m_pContext->m_seqLayerHeader.RANGERED))

            {
                umcRes = FillAndExecute(in);
            }

            // Update round ctrl field of seq header.
            m_pContext->m_seqLayerHeader.RNDCTRL = pPackDescriptorChild->m_pContext->m_seqLayerHeader.RNDCTRL;

            m_bLastFrameNeedDisplay = true;

            if (VC1_SKIPPED_FRAME == pPackDescriptorChild->m_pContext->m_picLayerHeader->PTYPE)
                out_data->SetFrameType(D_PICTURE); // means skipped 
            else
                out_data->SetFrameType(I_PICTURE);

            if (UMC_OK == umcRes)
            {
                if (1 == m_lFrameCount)
                    umcRes = UMC_ERR_NOT_ENOUGH_DATA;
            }
            return umcRes;
        }

        virtual UMC::FrameMemID     ProcessQueuesForNextFrame(bool& isSkip, mfxU16& Corrupted);
        virtual UMC::Status  SetRMSurface();
        virtual UMC::FrameMemID GetSkippedIndex(bool isIn = true);
    };

}

#endif //__UMC_VC1_VIDEO_DECODER_HW_H_
#endif //UMC_ENABLE_VC1_VIDEO_DECODER

