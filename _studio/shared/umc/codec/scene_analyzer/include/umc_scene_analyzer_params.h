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

#ifndef __UMC_SCENE_ANALYZER_PARAMS_H
#define __UMC_SCENE_ANALYZER_PARAMS_H

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
    int32_t m_maxGOPLength;                                      // (int32_t) maximum distance between intra frames

    // you may specify valid non-negative value or set it to any
    // negative value to let the encoder make the decision
    int32_t m_maxBLength;                                        // (int32_t) maximum length of B frame queue

    uint32_t m_maxDelayTime;                                      // (uint32_t) maximum delay time to get first encoded frame
    InterlaceType m_interlaceType;                              // (InterlaceType) type of constructred frames

/*
    uint32_t m_frameWidth;                                        // (uint32_t) frame width
    uint32_t m_frameHeight;                                       // (uint32_t) frame height*/
};

} // namespace UMC

#endif // __UMC_SCENE_ANALYZER_PARAMS_H
#endif
