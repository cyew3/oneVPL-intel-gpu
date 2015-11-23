//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_LOOKAHEAD_H__
#define __MFX_H265_LOOKAHEAD_H__

#include <list>
#include <vector>

namespace H265Enc {

    class H265Encoder;
    class Frame;
    struct H265VideoParam;

    struct StatItem {
        Ipp64s met;
        Ipp32s frameOrder;
    };

    class Lookahead : public NonCopyable
    {
    public:
        Lookahead(H265Encoder & enc);
        ~Lookahead();

        Ipp32s GetDelay();
        mfxStatus PerformThreadingTask(ThreadingTaskSpecifier action, Ipp32u region_row, Ipp32u region_col);
        int SetFrame(Frame* in, Ipp32s filedNum);
        void AverageComplexity(Frame *in);
        void ResetState();

    private:
        std::list<Frame*> &m_inputQueue;
        H265VideoParam &m_videoParam;
        H265Encoder &m_enc;// scenecut has right to modificate a GopStructure

        enum {
            ALG_PIX_DIFF = 0,
            ALG_HIST_DIFF
        };

        struct ScdConfig {
            Ipp32s M; // window size = 2*M+1
            Ipp32s N; // if (peak1 / peak2 > N) => SC Detected!
            Ipp32s algorithm; // ALG_PIX_DIFF, ALG_HIST_DIFF
            Ipp32s scaleFactor; // analysis will be done on (origW >> scaleFactor, origH >> scaleFactor) resolution
        } m_scdConfig;

        // frame granularity based on regions, where region consist of rows && row consist of blk_8x8 (Luma)
        Ipp32s m_regionCount;
        Ipp32s m_lowresRowsInRegion;
        Ipp32s m_originRowsInRegion;

        void AnalyzeSceneCut_AndUpdateState(Frame* in);
        void AnalyzeContent(Frame* in);
        void AnalyzeComplexity(Frame* in);

        void DoPersistanceAnalysis(Frame* in);
        void DoPersistanceAnalysis_OneRow(Frame* in, Ipp32s ctb_row);
        void DetermineQpMap_IFrame(Frame* in);
        void DetermineQpMap_PFrame(Frame* in, Frame* past);

        Frame* m_cmplxPrevFrame;
        std::vector<StatItem> m_slideWindowStat; // store metrics for SceneCut
        Frame* m_dqpPrevFrame;
        std::vector<Ipp32s> m_slideWindowPaq; // special win for Paq/Calq simplification. window size = 2*M+1, M = GopRefDist (numB + 1)
        std::vector<Ipp32s> m_slideWindowComplexity; // special win for Paq/Calq simplification. window size = 2*M+1, M = GopRefDist (numB + 1)

        void Build_ttGraph(Ipp32s poc);

    public:
        Frame* m_frame[2];
        Frame* m_lastAcceptedFrame[2];
        std::vector<ThreadingTask> m_threadingTasks;
        ObjectPool<ThreadingTask> m_ttHubPool;       // storage for threading tasks of type TT_HUB
    };

} // namespace

#endif // __MFX_H265_LOOKAHEAD_H__
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
