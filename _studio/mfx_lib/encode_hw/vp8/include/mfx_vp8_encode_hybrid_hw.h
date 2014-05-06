/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE_HW) && defined(MFX_VA)

#ifndef _MFX_VP8_ENCODE_HYBRID_HW_H_
#define _MFX_VP8_ENCODE_HYBRID_HW_H_

//#define HYBRID_ENCODE /*HW ENC + SW PAK + SW BSP*/

#include <vector>
#include <memory>

#include "mfx_vp8_encode_hybrid_bse.h"
#include "mfx_vp8_encode_utils_hybrid_hw.h"

namespace MFX_VP8ENC
{

#if defined (VP8_HYBRID_TIMING)

    typedef struct timingHybridVP8
    {
        mfxF64 fullFrameMcs;
        mfxF64 submitFrameMcs;
        mfxF64 hwAsyncMcs;
        mfxF64 bspMcs;
        mfxU64 numFrames;
    } timingStat;
#endif // VP8_HYBRID_TIMING
 
    class HybridPakDDIImpl : public VideoENCODE
    {
    public:
 
        virtual mfxStatus Close() {return MFX_ERR_UNSUPPORTED;}

        HybridPakDDIImpl(VideoCORE * core) : m_core(core) {}

        virtual ~HybridPakDDIImpl() {
#if defined (VP8_HYBRID_DUMP_WRITE) || defined (VP8_HYBRID_DUMP_READ)
            fclose(m_bse_dump);
#endif
#if defined (VP8_HYBRID_DUMP_BS_INFO)
            fclose(m_bs_info);
#endif
#if defined (VP8_HYBRID_TIMING)

            if (m_timings)
            {
                fprintf(m_timings, "\n");
                fprintf(m_timings, "Avg I, %4.0f,", m_I.fullFrameMcs / (double)(IPP_MAX(m_I.numFrames, 1)));
                fprintf(m_timings, "%4.0f,", m_I.submitFrameMcs / (double)(IPP_MAX(m_I.numFrames, 1)));
                fprintf(m_timings, "%4.0f,", m_I.hwAsyncMcs / (double)(IPP_MAX(m_I.numFrames, 1)));
                fprintf(m_timings, "%4.0f,", m_I.bspMcs / (double)(IPP_MAX(m_I.numFrames, 1)));
                fprintf(m_timings, "\n");
                fprintf(m_timings, "Avg P, %4.0f,", m_P.fullFrameMcs / (double)(IPP_MAX(m_P.numFrames, 1)));
                fprintf(m_timings, "%4.0f,", m_P.submitFrameMcs / (double)(IPP_MAX(m_P.numFrames, 1)));
                fprintf(m_timings, "%4.0f,", m_P.hwAsyncMcs / (double)(IPP_MAX(m_P.numFrames, 1)));
                fprintf(m_timings, "%4.0f,", m_P.bspMcs / (double)(IPP_MAX(m_P.numFrames, 1)));
                fprintf(m_timings, "\n");
                fclose(m_timings);
            }
            if (m_timingsPerFrame)
                fclose(m_timingsPerFrame);
#endif // VP8_HYBRID_TIMING
}

        virtual mfxStatus Init(mfxVideoParam * par);

        virtual mfxStatus Reset(mfxVideoParam * par);

        virtual mfxStatus GetVideoParam(mfxVideoParam * par);

        virtual mfxStatus EncodeFrameCheck( mfxEncodeCtrl *ctrl,
                                    mfxFrameSurface1 *surface,
                                    mfxBitstream *bs,
                                    mfxFrameSurface1 **reordered_surface,
                                    mfxEncodeInternalParams *pInternalParams,
                                    MFX_ENTRY_POINT pEntryPoints[],
                                    mfxU32 &numEntryPoints); 


       virtual mfxStatus GetEncodeStat(mfxEncodeStat *)  {return MFX_ERR_UNSUPPORTED;}
       virtual mfxStatus EncodeFrameCheck( mfxEncodeCtrl *, 
                                           mfxFrameSurface1 *, 
                                           mfxBitstream *, 
                                           mfxFrameSurface1 **, 
                                           mfxEncodeInternalParams *) 
       {return MFX_ERR_UNSUPPORTED;}

       virtual mfxStatus EncodeFrame(mfxEncodeCtrl *, mfxEncodeInternalParams *, mfxFrameSurface1 *, mfxBitstream *)
       {return MFX_ERR_UNSUPPORTED;}
       virtual mfxStatus CancelFrame(mfxEncodeCtrl *, mfxEncodeInternalParams *, mfxFrameSurface1 *, mfxBitstream *)
       {return MFX_ERR_UNSUPPORTED;}
       virtual mfxStatus GetFrameParam(mfxFrameParam *) 
       {return MFX_ERR_UNSUPPORTED;}

#if defined (VP8_HYBRID_TIMING)
        timingHybridVP8 m_I;
        timingHybridVP8 m_P;
        mfxU64           cpuFreq;
        mfxU64 m_bsp[2];
        mfxU64 m_fullFrame[2];
#endif // VP8_HYBRID_TIMING


    protected:
        static mfxStatus   TaskRoutineSubmit(void * state,  void * param, mfxU32 threadNumber, mfxU32 callNumber);
        static mfxStatus   TaskRoutineQuery(void * state,  void * param, mfxU32 threadNumber, mfxU32 callNumber);
        mfxStatus          SubmitFrame(Task *pTask);
        mfxStatus          QueryFrame(Task *pTask);

    protected:
        VideoCORE *                     m_core;
        VP8MfxParam                     m_video;
        TaskManagerHybridPakDDI         m_taskManager;
        Vp8CoreBSE                      m_BSE;
        std::auto_ptr <DriverEncoder>   m_ddi;

        UMC::Mutex                      m_taskMutex;

#if defined (VP8_HYBRID_TIMING)
        FILE*                           m_timings;
        FILE*                           m_timingsPerFrame;
#endif
#if defined (VP8_HYBRID_DUMP_WRITE) || defined (VP8_HYBRID_DUMP_READ)
        FILE*                           m_bse_dump;
        mfxU32                          m_bse_dump_size;
#endif
#if defined (VP8_HYBRID_DUMP_BS_INFO)
        FILE*                           m_bs_info;
#endif

    };
}; 
#endif 
#endif 
/* EOF */
