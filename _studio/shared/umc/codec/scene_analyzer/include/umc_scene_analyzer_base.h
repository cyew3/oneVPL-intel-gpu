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

#ifndef __UMC_SCENE_ANALYZER_BASE_H
#define __UMC_SCENE_ANALYZER_BASE_H

#include "umc_base_codec.h"

namespace UMC
{

class SceneAnalyzerParams : public BaseCodecParams
{
    DYNAMIC_CAST_DECL(SceneAnalyzerParams, BaseCodecParams)

public:
    // Default constructor
    SceneAnalyzerParams(void);
    // Destructor
    virtual
    ~SceneAnalyzerParams(void);

    // you may specify valid positive value or set it to any
    // non-positive value to let the encoder make the decision
    Ipp32s m_maxGOPLength;                                      // (Ipp32s) maximum distance between intra frames

    // you may specify valid non-negative value or set it to any
    // negative value to let the encoder make the decision
    Ipp32s m_maxBLength;                                        // (Ipp32s) maximum length of B frame queue

    Ipp32u m_maxDelayTime;                                      // (Ipp32u) maximum delay time to get first encoded frame
    InterlaceType m_interlaceType;                              // (InterlaceType) type of constructred frames

/*
    Ipp32u m_frameWidth;                                        // (Ipp32u) frame width
    Ipp32u m_frameHeight;                                       // (Ipp32u) frame height*/
};

// declare base class
class SceneAnalyzerBase : public BaseCodec
{
public:
    // Default constructor
    SceneAnalyzerBase(void);
    // Destructor
    virtual
    ~SceneAnalyzerBase(void);

    // Initialize the SceneAnalyzer
    virtual
    Status Init(BaseCodecParams *pParams);

    // Analyze the next frame
    virtual
    Status GetFrame(MediaData *pSource, MediaData *pDestination);

    // Get current parameter(s) of SceneAnalyzer
    virtual
    Status GetInfo(BaseCodecParams *) { return UMC_ERR_NOT_IMPLEMENTED; };

    // Release all resources
    virtual
    Status Close(void);

    // Set codec to initial state
    virtual
    Status Reset(void);

protected:

    // Get planned frame type.
    // This function can be overweighted to make
    // any private GOP structures.
    virtual
    FrameType GetPlannedFrameType(void);

    Ipp32s m_frameNum;                                          // (Ipp32s) number of a frame in the scene
    Ipp32s m_bFrameNum;                                         // (Ipp32s) number of a B frame in a row
    Ipp32s m_frameCount;                                        // (Ipp32s) total number of processed frames

    SceneAnalyzerParams m_params;                               // (SceneAnalyzerParams) working parameters
};

} // namespace UMC

#endif // __UMC_SCENE_ANALYZER_BASE_H
#endif
