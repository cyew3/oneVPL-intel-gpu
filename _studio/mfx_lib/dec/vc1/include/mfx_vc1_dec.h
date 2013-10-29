/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2013 Intel Corporation. All Rights Reserved.
//          MFX VC-1 DEC
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_DEC)

#include "mfxvideo++int.h"
#include "umc_vc1_common_defs.h"
#include "mfx_memory_allocator.h"
#include "mfx_vc1_dec_common.h"
#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_dec_thread.h"
#include "mfx_vc1_dec_threading.h"

#ifndef _MFX_VC1_DEC_H_
#define _MFX_VC1_DEC_H_

class MFXVideoDECVC1 : public VideoDEC
{
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);

    MFXVideoDECVC1(VideoCORE *core,mfxStatus* sts);
    virtual ~MFXVideoDECVC1(void) {};

    virtual mfxStatus Init(mfxVideoParam* par);
    virtual mfxStatus Reset(mfxVideoParam* par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    virtual mfxStatus GetSliceParam(mfxSliceParam *par);

    virtual mfxStatus RunFrameFullDEC(mfxFrameCUC *cuc);
    virtual mfxStatus RunFramePredDEC(mfxFrameCUC *cuc);
    virtual mfxStatus RunFrameIQT(mfxFrameCUC *cuc, mfxU8 scan);
    virtual mfxStatus RunFrameILDB(mfxFrameCUC *cuc);
    virtual mfxStatus GetFrameRecon(mfxFrameCUC *cuc);

    virtual mfxStatus RunSliceFullDEC(mfxFrameCUC *cuc);
    virtual mfxStatus RunSlicePredDEC(mfxFrameCUC *cuc);
    virtual mfxStatus RunSliceIQT(mfxFrameCUC *cuc, mfxU8 scan);
    virtual mfxStatus RunSliceILDB(mfxFrameCUC *cuc);
    virtual mfxStatus GetSliceRecon(mfxFrameCUC *cuc);

protected:

    //preparse CUC (seq and picture headers, create tasks for threading)
    //void FormalizeTasks() {};
    //friend class VC1TaskProcessorDEC;
    // May be need to move it to VC1TaskProcessorDEC
    mfxStatus SetInitParams(mfxVideoParam* par,
                            VC1SequenceLayerHeader*   pSeqHeadr);

    mfxStatus UnPackSeqHeader(mfxFrameCUC*  pCUC,
                              VC1SequenceLayerHeader*   pSeqHeadr);

    mfxStatus UnPackPicHeader(mfxFrameCUC*  pCUC,
                              VC1Context*   pContext);


    mfxStatus PrepareTasksForProcessing();

    mfxStatus ProcessUnit(mfxFrameCUC *cuc,
                          MFXVC1DecCommon::VC1DecEntryPoints point);

    mfxStatus SetpExtBufs(mfxFrameCUC*  pCUC);
    mfxStatus AllocatePadFrames();
    void PrepareInternalFrames(void);
    void PrepaeOutputPlanes(void);

    mfxStatus UnPackFrameHeaders(mfxFrameCUC*  pCUC);


    VideoCORE*                   m_pCore;
    UMC::VC1TaskProcessor*       m_pMfxDecTProc;
    VC1MfxTQueueDecFullDec       m_MxfDecObj;
    VC1MfxTQueueBase*            m_pMfxTQueueDec; //Concrete type of tasks depend
    mfxFrameCUC*                 m_pCUC;
    VC1Context*                  m_pContext;

    mfxHDL                       m_iMemContextID;
    mfxHDL                       m_iHeapID;
    mfxHDL                       m_iPadFramesID;
    UMC::VC1ThreadDecoder**      m_pdecoder;                              // (VC1ThreadDecoder *) pointer to array of thread decoders
    Ipp32u                       m_iThreadDecoderNum;                     // (Ipp32u) number of slice decoders

    UMC::VC1TSHeap*              m_pHeap;
    bool                         m_bIsSliceCall;
    mfxVideoParam                m_VideoParams; //Internal Video parameters


};


#endif
#endif
