/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2011 Intel Corporation. All Rights Reserved.
//          MFX VC-1 BSD
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_BSD)

#include "mfxvideo++int.h"
#include "umc_vc1_common_defs.h"

#include "umc_vc1_video_decoder.h"
#include "mfx_memory_allocator.h"
#include "mfx_vc1_bsd_threading.h"


#ifndef _MFX_VC1_BSD_H_
#define _MFX_VC1_BSD_H_

class VC1MfxTQueueBase;
class UMC::VC1TSHeap;
class UMC::MediaDataEx;
class UMC::VC1ThreadDecoder;

class MFXVideoBSDVC1 : public VideoBSD
{
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);

    MFXVideoBSDVC1 (VideoCORE *core, mfxStatus* sts);
    virtual ~MFXVideoBSDVC1 (void);

    virtual mfxStatus Init(mfxVideoParam* par);
    virtual mfxStatus Reset(mfxVideoParam* par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam*);
    virtual mfxStatus GetFrameParam(mfxFrameParam*);
    virtual mfxStatus GetSliceParam(mfxSliceParam*);

    virtual mfxStatus RunVideoParam(mfxBitstream *bs, mfxVideoParam *par);
    virtual mfxStatus RunFrameParam(mfxBitstream *bs, mfxFrameParam *par);
    virtual mfxStatus RunSliceParam(mfxBitstream *bs, mfxSliceParam *par);
    virtual mfxStatus ExtractUserData(mfxBitstream *bs, mfxU8 *ud, mfxU32 *sz, mfxU64 *ts);

    virtual mfxStatus RunSliceBSD(mfxFrameCUC *cuc);
    virtual mfxStatus RunSliceMFX(mfxFrameCUC *cuc);
    virtual mfxStatus RunFrameBSD(mfxFrameCUC *cuc);
    virtual mfxStatus RunFrameMFX(mfxFrameCUC *cuc);

protected:
    friend class VC1TaskProcessorBSD;

    mfxStatus SetpExtBufs();
    mfxStatus FillSliceParams(VC1Context* pContext,
                              Ipp32u StartRow,
                              Ipp32u RowToDecode);


    mfxStatus CheckAndSetInitParams(mfxVideoParam* pVideoParams);
    virtual Ipp32u CalculateHeapSize();
    mfxStatus ParseMFXBitstream(mfxBitstream *bs);

    mfxStatus InternalSCProcessing(Ipp8u*   pBStream,
                                   Ipp32u   DataSize,
                                   Ipp32u*  pOffsets,
                                   Ipp32u*  pValues);
    mfxStatus SMProfilesProcessing(Ipp8u* pBitstream);

    mfxStatus ProcessFrameHeaderParams(mfxBitstream *bs);
    mfxStatus ProcessOneUnit();

    mfxStatus PictureParamsPreParsing();
    mfxStatus CreateQueueOfTasks(Ipp32u*  pOffsets,
                                 Ipp32u*  pValues);

    mfxStatus ContextAllocation(Ipp32u mbWidth,
                                Ipp32u mbHeight);

    // Get Unit size in case od Simple/Main profiles
    mfxU32 GetUnSizeSM(mfxU8* pData);

    mfxStatus CreateFrameBuffer(mfxU32 bufferSize);
    mfxStatus GetStartCodes (Ipp8u* pDataPointer,
                             Ipp32u DataSize,
                             UMC::MediaDataEx* out,
                             Ipp32u* readSize);

    VideoCORE*                   m_pCore;
    UMC::BaseCodecParams*        m_pVideoParams;
    UMC::VC1TaskProcessor*       m_pArrayTProcBSD;
    mfxFrameCUC*                 m_pCUC; // External CUC

    VC1MfxTQueueBsd              m_MxfObj;
    VC1MfxTQueueBase*            m_pMfxTQueueBsd;

    mfxVideoParam          m_VideoParams; //Internal Video parameters
    mfxFrameParam          m_FrameParams; //Internal Frame parameters
    mfxSliceParam          m_SliceParams; //Internal Slice parameters

    Ipp32u                 m_CurrSlNum;

    Ipp32u                 m_BufferSize;
    bool                   m_bIsSliceCall;

    UMC::VC1TSHeap*                 m_pHeap;
    VC1Context*                     m_pContext;
    UMC::VC1ThreadDecoder**         m_pdecoder;                              // (VC1ThreadDecoder *) pointer to array of thread decoders
    mfxU32                          m_iThreadDecoderNum;                     // number of thread decoders
    mfxU8*                          m_pDataBuffer;
    UMC::MediaDataEx*               m_frameData;                             //uses for swap data into decoder
    UMC::MediaDataEx::_MediaDataEx* m_stCodes;
    mfx_UMC_MemAllocator            m_MemoryAllocator;
    mfxMemId                        m_iMemContextID;
    mfxMemId                        m_iHeapID;
    mfxMemId                        m_iFrameBufferID;
};


#endif
#endif
