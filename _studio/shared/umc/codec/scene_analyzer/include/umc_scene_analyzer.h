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

#ifndef __UMC_SCENE_ANALYZER_H
#define __UMC_SCENE_ANALYZER_H

#include "umc_scene_analyzer_p.h"

namespace UMC
{

class SceneAnalyzer : public SceneAnalyzerP
{
public:
    // Default constructor
    SceneAnalyzer(void);
    // Destructor
    virtual
    ~SceneAnalyzer(void);

    // Initialize the analyzer
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

    SceneAnalyzerFrame *m_pPrev;                                // (SceneAnalyzerFrame *) pointer to the previous non-reference frame

};

} // namespace UMC

#endif // __UMC_SCENE_ANALYZER_H
#endif
