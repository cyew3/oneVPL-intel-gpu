// Copyright (c) 2014-2019 Intel Corporation
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


#include "mfx_common.h"

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#ifndef __MFX_AV1_LOOKAHEAD_H__
#define __MFX_AV1_LOOKAHEAD_H__

#include <list>
#include <vector>

#include "mfx_av1_defs.h"
#include "mfx_av1_frame.h"

namespace AV1Enc {

    class AV1Encoder;
    class Frame;
    struct AV1VideoParam;

    struct StatItem {
        StatItem()
            : met (0)
            , frameOrder(-1)
        {}
        int64_t met;
        int32_t frameOrder;
    };

    class Lookahead : public NonCopyable
    {
    public:
        Lookahead(AV1Encoder & enc);
        ~Lookahead();

        int ConfigureLookaheadFrame(Frame* in, int32_t filedNum);
        mfxStatus Execute(ThreadingTask& task);
        void ResetState();

    private:
        std::list<Frame*> &m_inputQueue;
        AV1VideoParam &m_videoParam;
        AV1Encoder &m_enc;// scenecut has right to modificate a GopStructure

        int  DetectSceneCut_AMT(Frame* input, bool hasPrev, bool hasLTR, bool detectSceneTransition = false);

        // frame granularity based on regions, where region consist of rows && row consist of blk_8x8 (Luma)
        int32_t m_regionCount;
        int32_t m_lowresRowsInRegion;
        int32_t m_originRowsInRegion;

        void AnalyzeSceneCut_AndUpdateState(Frame* in);
        int32_t m_bufferingPaq; // paq need buffering = refDist + M (scenecut buffering)
        Frame* m_lastAcceptedFrame[2];
        int32_t m_lastAcceptedFrameOrder;
#ifdef ZERO_DELAY_ANALYZE_CMPLX
        FrameData *m_prev_origin;
        FrameData *m_prev_lowres;
#endif
        uint8_t m_pendingSceneCut;// don't break B-pyramid for the best quality
        ObjectPool<ThreadingTask> m_ttHubPool;       // storage for threading tasks of type TT_HUB

        int32_t m_frameOrderPrev;
        SceneStats* m_sceneStatsPrev;

#if defined(ENABLE_AV1_ALTR)
        SceneStats* m_sceneStatsLtr;
        SceneStats* m_sceneStatsTmp;
        uint8_t m_sceneTransFlag;
        int32_t m_numFramesAvg;
        int32_t m_avgMV0;
        void getSubSampleImageLtr(Frame* in);
#endif
    };

    const Frame* GetNextAnchor(ConstFrameIter curr, ConstFrameIter end);
    const Frame* GetPrevAnchor(ConstFrameIter begin, ConstFrameIter end, const Frame* curr);

    void GetLookaheadGranularity(const AV1VideoParam& videoParam, int32_t & regionCount, int32_t & lowRowsInRegion, int32_t & originRowsInRegion, int32_t & numTasks);

    void AverageComplexity(Frame *in, AV1VideoParam& videoParam);
    void AverageRsCs(Frame *in);
    void BackPropagateAvgTsc(ConstFrameIter prevRef, ConstFrameIter currRef);

#if ENABLE_QPMAP
    int32_t BuildQpMap(ConstFrameIter begin, ConstFrameIter end, int32_t frameOrderCentral, AV1VideoParam& videoParam, int32_t doUpdateState);
    void DetermineQpMap_PFrame(ConstFrameIter begin, ConstFrameIter curr, ConstFrameIter end, AV1VideoParam & videoParam);
    void DetermineQpMap_IFrame(ConstFrameIter curr, ConstFrameIter end, AV1VideoParam& videoParam);

    void DoPDistInterAnalysis_OneRow(Frame* curr, Frame* prevP, Frame* nextP, int32_t region_row, int32_t lowresRowsInRegion, int32_t originRowsInRegion, uint8_t LowresFactor, uint8_t FullresMetrics);
#endif


} // namespace

#endif // __MFX_H265_LOOKAHEAD_H__
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
