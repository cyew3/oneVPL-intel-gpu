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

class MFXVideoENCODEH265;

namespace H265Enc {

    class H265Frame;
    struct Task;
    struct H265VideoParam;

    struct StatItem {
        Ipp64s met;
        Ipp32s frameOrder;
    };

    class Lookahead
    {
    public:
        Lookahead(std::list<Task*> & inputQueue, H265VideoParam & par, MFXVideoENCODEH265 & enc);
        ~Lookahead();

        Ipp32s GetDelay();
        void DoLookaheadAnalysis(Task* curr);

    private:
        std::list<Task*> &m_inputQueue;
        H265VideoParam& m_videoParam;
        MFXVideoENCODEH265 & m_enc;// scenecut has right to modificate a GopStructure

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

        void AnalyzeSceneCut_AndUpdateState(Task* in);
        void AnalyzeContent(Task* in);
        void AnalyzeComplexity(Task* in);

        void DoPersistanceAnalysis(Task* curr);
        void IPicDetermineQpMap(H265Frame *inFrame);
        void PPicDetermineQpMap(H265Frame* inFrame, H265Frame *past_frame);

        H265Frame* m_cmplxPrevFrame;
        std::vector<Ipp8u> m_workBuf;
        std::vector<StatItem> m_slideWindowStat; // store metrics for SceneCut
        std::vector<Ipp32s> m_slideWindowPaq; // special win for Paq/Calq simplification. window size = 2*M+1, M = GopRefDist (numB + 1)
        std::vector<Ipp32s> m_slideWindowComplexity; // special win for Paq/Calq simplification. window size = 2*M+1, M = GopRefDist (numB + 1)
    };

} // namespace

#endif // __MFX_H265_LOOKAHEAD_H__
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
