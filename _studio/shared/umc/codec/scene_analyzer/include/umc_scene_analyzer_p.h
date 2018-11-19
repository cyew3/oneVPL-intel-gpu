// Copyright (c) 2007-2018 Intel Corporation
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

#include "umc_defs.h"

#if defined (UMC_ENABLE_UMC_SCENE_ANALYZER)

#ifndef __UMC_SCENE_ANALYZER_P_H
#define __UMC_SCENE_ANALYZER_P_H

#include "umc_scene_analyzer_base.h"
#include "umc_scene_analyzer_frame.h"

namespace UMC
{

class SceneAnalyzerP : public SceneAnalyzerBase
{
public:
    // Default constructor
    SceneAnalyzerP(void);
    // Destructor
    virtual
    ~SceneAnalyzerP(void);

    // Initialize the analyzer,
    // parameter should has type SceneAnalyzerParams
    virtual
    Status Init(BaseCodecParams *pParams);

    // Decompress the next frame
    virtual
    Status GetFrame(MediaData *pSource, MediaData *pDestination);

    // Release all resources
    virtual
    Status Close(void);

protected:

    // Initialize the destination video data
    Status InitializeVideoData(VideoData *pDst, const SceneAnalyzerFrame *pFrame);
    // Update the history with given frame
    void UpdateHistory(const SceneAnalyzerFrame *pFrame, FrameType analysisType);

    // These functions are supposed to be overweighted for
    // every standard to make a custom weighting functions.

    // the main difference between a picture and a frame, that the picture is
    // just a picture. A frame is a complex structure, containing at least
    // 1 picture (3 pictures in field mode). So picture operating functions are
    // pure calculating functions. Frame operating functions do a little more -
    // they make a decision to assign specified type to a frame, basing on the
    // results of analyzing frames and thier fields.

    // Picture analysis functions for INTRA and INTER analysis
    virtual
    Status AnalyzePicture(SceneAnalyzerPicture *pSrc);
    virtual
    Status AnalyzePicture(SceneAnalyzerPicture *pRef, SceneAnalyzerPicture *pSrc);

    // Frame analysis functions for INTRA and INTER analysis
    virtual
    Status AnalyzeFrame(SceneAnalyzerFrame *pSrc);
    virtual
    Status AnalyzeFrame(SceneAnalyzerFrame *pRef, SceneAnalyzerFrame *pSrc);

    virtual
    void AnalyzeIntraMB(const uint8_t *pSrc, int32_t srcStep,
                        UMC_SCENE_INFO *pMbInfo);
    virtual
    void AnalyzeInterMB(const uint8_t *pRef, int32_t refStep,
                        const uint8_t *pSrc, int32_t srcStep,
                        UMC_SCENE_INFO *pMbInfo);
    virtual
    void AnalyzeInterMBMotion(const uint8_t *pRef, int32_t refStep,
                              mfxSize refMbDim,
                              const uint8_t *pSrc, int32_t srcStep,
                              uint32_t mbX, uint32_t mbY, uint16_t *pSADs,
                              UMC_SCENE_INFO *pMbInfo);

    // Make decision over the source frame coding type
    virtual
    void MakePPictureCodingDecision(SceneAnalyzerPicture *pRef, SceneAnalyzerPicture *pSrc);

    SceneAnalyzerFrame *m_pCur;                                 // (SceneAnalyzerFrame *) pointer to the current frame
    SceneAnalyzerFrame *m_pRef;                                 // (SceneAnalyzerFrame *) pointer to the previous reference frame

    struct
    {
        UMC_SCENE_INFO info;
        FrameType analyzedFrameType;
        bool m_bChangeDetected;

    } m_history[8];
    int32_t m_framesInHistory;

};

} // namespace UMC

#endif // __UMC_SCENE_ANALYZER_P_H
#endif
