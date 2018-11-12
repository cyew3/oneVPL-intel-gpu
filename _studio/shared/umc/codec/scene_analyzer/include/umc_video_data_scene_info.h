// Copyright (c) 2008-2018 Intel Corporation
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

#ifndef __UMC_VIDEO_DATA_SCENE_INFO_H
#define __UMC_VIDEO_DATA_SCENE_INFO_H

#include "umc_video_data.h"
#include "umc_scene_info.h"

namespace UMC
{

// declare types of statistics
enum
{
    SA_FRAME_DATA               = 0,
    SA_SLICE_DATA               = 1,
    SA_MB_DATA                  = 2,

    // declare number of items in the enum
    SA_TYPES_OF_DATA
};

class VideoDataSceneInfo : public VideoData
{
    DYNAMIC_CAST_DECL(VideoDataSceneInfo, VideoData);

public:
    // Default constructor
    VideoDataSceneInfo(void);
    // Destructor
    virtual
    ~VideoDataSceneInfo(void);

    // Set the pointer to array of data
    inline
    void SetAnalysisData(const UMC_SCENE_INFO *pData, Ipp32u dataType);
    // Get the pointer to array of data
    inline
    const UMC_SCENE_INFO *GetAnalysisData(Ipp32u dataType);

protected:

    const UMC_SCENE_INFO *(m_pData[SA_TYPES_OF_DATA]);
};

inline
void VideoDataSceneInfo::SetAnalysisData(const UMC_SCENE_INFO *pData, Ipp32u dataType)
{
    // check error(s)
    if (SA_TYPES_OF_DATA <= dataType)
        return;

    m_pData[dataType] = pData;

} // void VideoDataSceneInfo::SetAnalysisData(const UMC_SCENE_INFO *pData, Ipp32u dataType)

inline
const UMC_SCENE_INFO *VideoDataSceneInfo::GetAnalysisData(Ipp32u dataType)
{
    // check error(s)
    if (SA_TYPES_OF_DATA <= dataType)
        return (UMC_SCENE_INFO *) 0;

    // return the pointer
    return m_pData[dataType];

} // const UMC_SCENE_INFO *VideoDataSceneInfo::GetAnalysisData(Ipp32u dataType)

} // namespace UMC

#endif // __UMC_VIDEO_DATA_SCENE_INFO_H
#endif
