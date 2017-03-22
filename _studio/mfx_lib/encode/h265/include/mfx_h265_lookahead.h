//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_LOOKAHEAD_H__
#define __MFX_H265_LOOKAHEAD_H__

#include <list>
#include <vector>

#include "mfx_h265_defs.h"
#include "mfx_h265_frame.h"

#ifdef AMT_HROI_PSY_AQ
#include "FSapi.h"
#endif

namespace H265Enc {

    class H265Encoder;
    class Frame;
    struct H265VideoParam;

    struct StatItem {
        StatItem()
            : met (0)
            , frameOrder(-1)
        {}
        Ipp64s met;
        Ipp32s frameOrder;
    };

    class Lookahead : public NonCopyable
    {
    public:
        Lookahead(H265Encoder & enc);
        ~Lookahead();

        int ConfigureLookaheadFrame(Frame* in, Ipp32s filedNum);
        mfxStatus Execute(ThreadingTask& task);
        void ResetState();

    private:
        std::list<Frame*> &m_inputQueue;
        H265VideoParam &m_videoParam;
        H265Encoder &m_enc;// scenecut has right to modificate a GopStructure

        void DetectSceneCut(FrameIter begin, FrameIter end, /*FrameIter input*/ Frame* in, Ipp32s updateGop, Ipp32s updateState);

        // frame granularity based on regions, where region consist of rows && row consist of blk_8x8 (Luma)
        Ipp32s m_regionCount;
        Ipp32s m_lowresRowsInRegion;
        Ipp32s m_originRowsInRegion;
        
        void AnalyzeSceneCut_AndUpdateState_Atul(Frame* in);
        Ipp32s m_bufferingPaq; // paq need buffering = refDist + M (scenecut buffering)
        Frame* m_lastAcceptedFrame[2];
        Ipp8u m_pendingSceneCut;// don't break B-pyramid for the best quality
        ObjectPool<ThreadingTask> m_ttHubPool;       // storage for threading tasks of type TT_HUB
    };

    Frame* GetNextAnchor(FrameIter curr, FrameIter end);
    Frame* GetPrevAnchor(FrameIter begin, FrameIter end, const Frame* curr);

    void GetLookaheadGranularity(const H265VideoParam& videoParam, Ipp32s & regionCount, Ipp32s & lowRowsInRegion, Ipp32s & originRowsInRegion, Ipp32s & numTasks);

    void AverageComplexity(Frame *in, H265VideoParam& videoParam, Frame *next);

    void AverageRsCs(Frame *in);
    void BackPropagateAvgTsc(FrameIter prevRef, FrameIter currRef);

    Ipp32s BuildQpMap(FrameIter begin, FrameIter end, Ipp32s frameOrderCentral, H265VideoParam& videoParam, Ipp32s doUpdateState);
    void DetermineQpMap_PFrame(FrameIter begin, FrameIter curr, FrameIter end, H265VideoParam & videoParam);
    void DetermineQpMap_IFrame(FrameIter curr, FrameIter end, H265VideoParam& videoParam);

    void DoPDistInterAnalysis_OneRow(Frame* curr, Frame* prevP, Frame* nextP, Ipp32s region_row, Ipp32s lowresRowsInRegion, Ipp32s originRowsInRegion, Ipp8u LowresFactor);

    int  DetectSceneCut_AMT(Frame* input, Frame* prev);

} // namespace

#endif // __MFX_H265_LOOKAHEAD_H__
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
