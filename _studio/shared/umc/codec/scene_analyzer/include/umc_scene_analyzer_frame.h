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

#include <memory>
#include "umc_defs.h"

#if defined (UMC_ENABLE_UMC_SCENE_ANALYZER)

#ifndef __UMC_SCENE_ANALYZER_FRAME_H
#define __UMC_SCENE_ANALYZER_FRAME_H

#include "umc_video_data.h"
#include "umc_scene_info.h"

namespace UMC
{

class SceneAnalyzerPicture
{
public:
    // Default constructor
    SceneAnalyzerPicture(void);
    // Destructor
    virtual
    ~SceneAnalyzerPicture(void);

    // Initialize the picture
    Status Init(int32_t srcWidth, int32_t srcHeight,
                ColorFormat colorFormat);
    // Initialize the picture with given pointer.
    // This method is supposed to use only with GRAY pictures.
    Status SetPointer(const uint8_t *pPic, size_t picStep,
                      int32_t srcWidth, int32_t srcHeight);

    const uint8_t *(m_pPic[3]);                                   // (const uint8_t *([])) pointer to the picture's buffer
    size_t m_picSize;                                           // (size_t) size of allocated picture
    size_t m_picStep;                                           // (int32_t) picture's buffer's step
    mfxSize m_picDim;                                          // (mfxSize) picture's dimensions
    mfxSize m_mbDim;                                           // (mfxSize) picture's dimensions in units of MB
    ColorFormat m_colorFormat;
    bool m_bChangeDetected;                                     // (bool) a scene change was detected

    uint16_t m_sadBuffer[SA_ESTIMATION_WIDTH];                    // (uint16_t []) temporal buffer for SADs

    UMC_SCENE_INFO m_info;                                      // (UMC_SCENE_INFO) entire frame statistics
    UMC_SCENE_INFO *m_pSliceInfo;                               // (UMC_SCENE_INFO *) pointer to array of average slice statistics
    size_t m_numSliceInfo;

protected:
    // Release the object
    void Close(void);

    // Allocate buffers for being analyzed frame
    Status AllocateBuffer(size_t bufSize);
    Status AllocateSliceInfo(size_t numSliceInfo);
    // Release the buffers
    void ReleaseBuffer(void);
    void ReleaseSliceInfo(void);

};

class SceneAnalyzerFrame : public SceneAnalyzerPicture
{
public:
    // Default constructor
    SceneAnalyzerFrame(void);
    // Destructor
    virtual
    ~SceneAnalyzerFrame(void);

    // Initialize the frame
    Status SetSource(VideoData *pSrc, InterlaceType interlaceType);

    SceneAnalyzerPicture m_scaledPic;
    SceneAnalyzerPicture m_fields[2];

    FrameType m_frameType;                                      // (FrameType) suggested frame type

    // The following variables have a big difference in meaning.
    // frameStructure is a real type of the current frame. PROGRESSIVE means
    // that it is a real progressive frame, others mean it is a couple of
    // fields. Fielded pictures are estimated as fields always. A PROGRESSIVE
    // frame should be estimated as a field couple, if the reference frame is
    // a field couple, and as a progressive frame, if the reference frame is a
    // progressive frame too. The type of the reference frame is determined by
    // its frameStructure variable. The frameEstimation variables means the
    // type of estimation of the current PROGRESSIVE by nature frame.
    InterlaceType m_frameStructure;                             // (InterlaceType) frame structure
    InterlaceType m_frameEstimation;                            // (InterlaceType) frame estimation type

private:
    mfxSize m_srcSize;
    mfxSize m_dstSize;
    std::auto_ptr<uint8_t> m_workBuff;
};

inline
bool CheckSupportedColorFormat(VideoData *pSrc)
{
    ColorFormat colorFormat;

    // check color format
    colorFormat = pSrc->GetColorFormat();
    if ((GRAY != colorFormat) &&
        (YV12 != colorFormat) &&
        (NV12 != colorFormat) &&
        (YUV420 != colorFormat) &&
        (YUV422 != colorFormat) &&
        (YUV444 != colorFormat))
    {
        return false;
    }

    return true;

} // bool CheckSupportedColorFormat(VideoData *pSrc)

} // namespace UMC

#endif // __UMC_SCENE_ANALYZER_FRAME_H
#endif
