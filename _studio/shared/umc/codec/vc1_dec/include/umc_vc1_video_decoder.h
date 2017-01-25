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

#ifndef __UMC_VC1_VIDEO_DECODER_H_
#define __UMC_VC1_VIDEO_DECODER_H_

#include "umc_video_decoder.h"
#include "umc_vc1_dec_frame_descr.h"
#include "umc_media_data_ex.h"
#include "umc_frame_allocator.h"

#include "umc_va_base.h"

#include "umc_vc1_dec_skipping.h"

class MFXVideoDECODEVC1;
namespace UMC
{

    class VC1TSHeap;
    class VC1TaskStoreSW;

    class VC1VideoDecoder
    {
        friend class VC1TaskStore;
        friend class VC1TaskStoreSW;
        friend class ::MFXVideoDECODEVC1;

    public:
        // Default constructor
        VC1VideoDecoder();
        // Default destructor
        virtual ~VC1VideoDecoder();

        // Initialize for subsequent frame decoding.
        virtual Status Init(BaseCodecParams *init);

        // Get next frame
        virtual Status GetFrame(MediaData* in, MediaData* out);
        // Close  decoding & free all allocated resources
        virtual Status Close(void);

        // Reset decoder to initial state
        virtual Status   Reset(void);

        // Perfomance tools. Improve speed or quality.
        //Accelarate Decoder (remove some features like deblockng, smoothing or change InvTransform and Quant)
        // speed_mode - return current mode

        // Change Decoding speed
        Status ChangeVideoDecodingSpeed(Ipp32s& speed_shift);

        void SetExtFrameAllocator(FrameAllocator*  pFrameAllocator) {m_pExtFrameAllocator = pFrameAllocator;}

        void SetVideoHardwareAccelerator            (VideoAccelerator* va);

    protected:

        Status CreateFrameBuffer(Ipp32u bufferSize);

        void      GetFrameSize                         (MediaData* in);
        void      GetPTS                               (Ipp64f in_pts);
        bool      GetFPS                               (VC1Context* pContext);

        virtual void FreeTables(VC1Context* pContext);
        virtual bool InitTables(VC1Context* pContext);
        virtual bool InitAlloc(VC1Context* pContext, Ipp32u MaxFrameNum) = 0;
        virtual bool InitVAEnvironment() = 0;
        void    FreeAlloc(VC1Context* pContext);

        virtual Status VC1DecodeFrame                 (MediaData* in, VideoData* out_data);


        virtual Ipp32u CalculateHeapSize() = 0;

        Status GetStartCodes                  (Ipp8u* pDataPointer,
                                               Ipp32u DataSize,
                                               MediaDataEx* out,
                                               Ipp32u* readSize);
        Status SMProfilesProcessing(Ipp8u* pBitstream);
        virtual Status ContextAllocation(Ipp32u mbWidth,Ipp32u mbHeight);

        Status StartCodesProcessing(Ipp8u*   pBStream,
                                    Ipp32u*  pOffsets,
                                    Ipp32u*  pValues,
                                    bool     IsDataPrepare);

        Status ParseStreamFromMediaData     ();
        Status ParseStreamFromMediaDataEx   (MediaDataEx *in_ex);
        Status ParseInputBitstream          ();
        Status InitSMProfile                ();

        Ipp32u  GetCurrentFrameSize()
        {
            if (m_dataBuffer)
                return (Ipp32u)m_frameData->GetDataSize();
            else
                return (Ipp32u)m_pCurrentIn->GetDataSize();
        }

        Status CheckLevelProfileOnly(VideoDecoderParams *pParam);

        VideoStreamInfo         m_ClipInfo;
        MemoryAllocator        *m_pMemoryAllocator;
        
        VC1Context*                m_pContext;
        VC1Context                 m_pInitContext;
        Ipp32u                     m_iThreadDecoderNum;                     // (Ipp32u) number of slice decoders
        Ipp8u*                     m_dataBuffer;                            //uses for swap data into decoder
        MediaDataEx*               m_frameData;                             //uses for swap data into decoder
        MediaDataEx::_MediaDataEx* m_stCodes;
        Ipp32u                     m_decoderInitFlag;
        Ipp32u                     m_decoderFlags;

        UMC::MemID                 m_iMemContextID;
        UMC::MemID                 m_iHeapID;
        UMC::MemID                 m_iFrameBufferID;

        Ipp64f                     m_pts;
        Ipp64f                     m_pts_dif;

        Ipp32u                     m_iMaxFramesInProcessing;

        Ipp64u                     m_lFrameCount;
        bool                       m_bLastFrameNeedDisplay;
        VC1TaskStore*              m_pStore;
        VideoAccelerator*          m_va;
        VC1TSHeap*                 m_pHeap;

        bool                       m_bIsReorder;
        MediaData*                 m_pCurrentIn;
        VideoData*                 m_pCurrentOut;

        bool                       m_bIsNeedToFlush;
        Ipp32u                     m_AllocBuffer;

        FrameAllocator*            m_pExtFrameAllocator;
        Ipp32u                     m_SurfaceNum;

        bool                       m_bIsExternalFR;

        static const Ipp32u        NumBufferedFrames = 0;
        static const Ipp32u        NumReferenceFrames = 3;

        virtual UMC::FrameMemID     ProcessQueuesForNextFrame(bool& isSkip, mfxU16& Corrupted) = 0;

        void SetCorrupted(UMC::VC1FrameDescriptor *pCurrDescriptor, mfxU16& Corrupted);
        bool IsFrameSkipped();
        virtual FrameMemID  GetDisplayIndex(bool isDecodeOrder, bool isSamePolarSurf);
        FrameMemID  GetLastDisplayIndex();
        virtual UMC::Status  SetRMSurface();
        void UnlockSurfaces();
        virtual UMC::FrameMemID GetSkippedIndex(bool isIn = true) = 0;
        FrameMemID GetFrameOrder(bool isLast, bool isSamePolar, Ipp32u & frameOrder);
        virtual Status RunThread(int threadNumber) { threadNumber; return UMC_OK; }

        UMC::VC1FrameDescriptor* m_pDescrToDisplay;
        mfxU32                   m_frameOrder;
        UMC::FrameMemID               m_RMIndexToFree;
        UMC::FrameMemID               m_CurrIndexToFree;

    protected:
        UMC::FrameMemID GetSkippedIndex(UMC::VC1FrameDescriptor *desc, bool isIn);
    };

}

#endif //__UMC_VC1_VIDEO_DECODER_H
#endif //UMC_ENABLE_VC1_VIDEO_DECODER

