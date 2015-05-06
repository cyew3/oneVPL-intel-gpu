//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_FRAME_H__
#define __MFX_H265_FRAME_H__

#include <list>
#include <vector>
#include "vm_interlocked.h"
#include "umc_mutex.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_set.h"

namespace H265Enc {

    struct H265VideoParam;

    struct H265MV
    {
        Ipp16s  mvx;
        Ipp16s  mvy;
    };

    class State
    {
    public:
        State()
            : m_free(true)
        {
        }

        bool IsFree() const
        {
            return m_free;
        }

        void SetFree(bool free)
        {
            m_free = free;
        }

    private:
        bool m_free; // resource in use or could be reused (m_free == true means could be reused).
    };

    struct RefCounter
    {
        RefCounter() 
            : m_refCounter(0) 
        {}
        volatile Ipp32u m_refCounter; // to prevent race condition in case of frame threading
        void AddRef(void)  { vm_interlocked_inc32(&m_refCounter);};
        void Release(void) { vm_interlocked_dec32(&m_refCounter);}; // !!! here not to delete in case of refCounter == 0 !!!
    };

    struct Statistics : public RefCounter
    {
        std::vector<Ipp32s> m_interSad;
        std::vector<Ipp32s> m_interSad_pdist_past;
        std::vector<Ipp32s> m_interSad_pdist_future;

        std::vector<H265MV> m_mv;
        std::vector<H265MV> m_mv_pdist_future;
        std::vector<H265MV> m_mv_pdist_past;
        std::vector<Ipp64f> m_rs;
        std::vector<Ipp64f> m_cs;
        
        std::vector<Ipp32s> sc_mask;
        std::vector<Ipp32s> qp_mask;
        std::vector<Ipp32s> coloc_past;
        std::vector<Ipp32s> coloc_futr;

        std::vector<Ipp32s> m_intraSatd;
        std::vector<Ipp32s> m_interSatd;

        Ipp64f m_frameRs;
        Ipp64f m_frameCs;
        Ipp64f SC;
        Ipp64f TSC;
        Ipp64f avgsqrSCpp;
        Ipp64f avgTSC;
        Ipp32f m_avgBestSatd; //= sum( min{ interSatd[i], intraSatd[i] } ) / {winth*height};
        Ipp32f m_avgIntraSatd;
        Ipp32f m_intraRatio;
        Ipp32s m_sceneCut;
        Ipp64s m_metric;// special metric per frame for sceneCut

        void ResetAvgMetrics()
        {
            m_frameRs = 0.0;
            m_frameCs = 0.0;
            SC = 0.0;
            TSC = 0.0;
            avgsqrSCpp = 0.0;
            avgTSC = 0.0;
            m_avgBestSatd = 0.f;
            m_avgIntraSatd = 0.f;
            m_intraRatio = 0.f;
            m_sceneCut = 0;
            m_metric = 0;
        }

        ~Statistics() { Destroy(); }
        void Create(H265VideoParam *par);// {}
        void Destroy();// {}
    };

    struct H265CUData;
    struct FrameData : public RefCounter
    {
        Ipp8u *y;
        Ipp8u *uv;

        Ipp32s width;
        Ipp32s height;
        Ipp32s padding;
        Ipp32s pitch_luma_pix;
        Ipp32s pitch_luma_bytes;
        Ipp32s pitch_chroma_pix;
        Ipp32s pitch_chroma_bytes;

        FrameData()
        : y(NULL)
        , uv(NULL)
        , mem_plane(NULL)
        {}
        ~FrameData() { Destroy();}

        void Create(H265VideoParam *par);
        void Destroy();

    private:
        void *mem_plane;
    };

    class Frame
    {
    public:
        FrameData* m_origin;
        FrameData* m_recon;
        FrameData* m_luma_8bit;//used by FEI in enc:p010 mode
        FrameData* m_lowres; // used by lookahead

        H265CUData *cu_data;

        Ipp8u  m_bitDepthLuma;
        Ipp8u  m_bdLumaFlag;
        Ipp8u  m_bitDepthChroma;
        Ipp8u  m_bdChromaFlag;
        Ipp8u  m_chromaFormatIdc;

        mfxU64 m_timeStamp;
        Ipp32u m_picCodeType;
        Ipp32s m_RPSIndex;

        // flags indicate that the stages were done on the frame
        Ipp8u  m_wasLookAheadProcessed;
        Ipp32u m_lookaheadRefCounter;

        Ipp32s m_pyramidLayer;
        Ipp32s m_miniGopCount;
        Ipp32s m_biFramesInMiniGop;
        Ipp32s m_frameOrder;
        Ipp32s m_frameOrderOfLastIdr;
        Ipp32s m_frameOrderOfLastIntra;
        Ipp32s m_frameOrderOfLastIntraInEncOrder;
        Ipp32s m_frameOrderOfLastAnchor;
        Ipp32s m_poc;
        Ipp32s m_encOrder;
        Ipp8u  m_isShortTermRef;
        Ipp8u  m_isLongTermRef;
        Ipp8u  m_isIdrPic;
        Ipp8u  m_isRef;
        Ipp8u  m_doPostProc;
        RefPicList m_refPicList[2]; // 2 reflists containing reference frames used by current frame
        Ipp32s m_mapRefIdxL1ToL0[MAX_NUM_REF_IDX];
        Ipp32s m_mapListRefUnique[2][MAX_NUM_REF_IDX];
        Ipp32s m_numRefUnique;
        Ipp32s m_allRefFramesAreFromThePast;
        Ipp32u m_frameType;// full info for bs. m_frameType = m_codeType | isIDR ? | isRef ?

        std::vector<H265Slice> m_slices;
        Frame* m_dpb[16];
        Ipp32s     m_dpbSize;

        Ipp8s     m_sliceQpY;
        std::vector<Ipp8s> m_lcuQps; // array for LCU QPs
        H265Slice m_dqpSlice[2*MAX_DQP+1];

        H265ShortTermRefPicSet m_shortRefPicSet[66];

        // for (frame) threading
        volatile Ipp32s m_codedRow; // sync info in case of frame threading
        ThreadingTask *m_threadingTasks;
        ThreadingTask  m_threadingTaskLast;
        Ipp32u m_numThreadingTasks;
        volatile Ipp32u m_numFinishedThreadingTasks;
        volatile Ipp32u m_statusReport; // 0 - frame submitted to FrameEncoder (not run!!), 1 - FrameEncoder was run (resource assigned), 2 - frame ready, 7 - need repack
        Ipp32s m_encIdx; // we have "N" frameEncoders. this index indicates owner of the frame [0, ..., N-1]
        
        // complexity/content statistics
        // 0 - original resolution, 1 - lowres
        Statistics* m_stats[2];

        // BRC info
        Ipp64f m_avCmplx;
        Ipp64f m_CmplxQstep;
        Ipp32s m_qpBase;
        Ipp64f m_totAvComplx;
        Ipp32s m_totComplxCnt;
        Ipp64f m_complxSum;
        Ipp32s m_predBits;
        //Ipp64f m_cmplx;
        std::vector<Frame*> m_futureFrames;

        // quick wa for GAAC
        void*      m_extParam;

        mfxPayload **m_userSeiMessages;
        Ipp32u       m_numUserSeiMessages;

        Frame()
        {
            ResetMemInfo();
            ResetEncInfo();
            ResetCounters();
        }

        ~Frame() { Destroy(); mem = NULL;}

        Ipp8u wasLAProcessed()    { return m_wasLookAheadProcessed; }
        void setWasLAProcessed() { m_wasLookAheadProcessed = true; }
        void unsetWasLAProcessed() { m_wasLookAheadProcessed = false; }

        void Create(H265VideoParam *par);
        void Destroy();
        void CopyFrameData(const mfxFrameSurface1 *in, Ipp8u have8bitCopy);
        void doPadding();

        void ResetMemInfo();
        void ResetEncInfo();
        void ResetCounters();

        private:
            void *mem;
    };

    typedef std::list<Frame*> FrameList;
    typedef std::list<Frame*>::iterator   FrameIter;

    void Dump(H265VideoParam *par, Frame* frame, FrameList & dpb);
    void PadOneReconRow(FrameData* frame, Ipp32u ctb_row, Ipp32u maxCuSize, Ipp32u PicHeightInCtbs, const H265VideoParam& par);
    void PadOneReconCtu(Frame* frame, Ipp32u ctb_row, Ipp32u ctb_col, Ipp32u maxCuSize, Ipp32u PicHeightInCtbs, Ipp32u PicWidthInCtbs);

    template <class T> 
    T* Allocate(typename std::vector<T*> & queue, H265VideoParam *par);

} // namespace

#endif // __MFX_H265_FRAME_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
