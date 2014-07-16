/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#ifndef __FRAME_PROCESSOR_H
#define __FRAME_PROCESSOR_H

#include <vector>
#include <math.h>
#include <stdio.h>

#include "sample_defs.h"
#include "plugin_utils.h"
#include "mfxstructures.h"
#include "detect_people_plugin_api.h"

#include <opencv2/opencv.hpp>
using namespace cv;

#define NUM_FEATURE_PER_OBJECT 6

class Processor
{
public:
    Processor();
    virtual ~Processor();
    virtual mfxStatus SetAllocator(mfxFrameAllocator *pAlloc);
    virtual mfxStatus Init(mfxFrameSurface1 *frame_in, mfxFrameSurface1 *frame_out);
    virtual mfxStatus Process() = 0;

protected:
    //locks frame or report of an error
    mfxStatus LockFrame(mfxFrameSurface1 *frame);
    mfxStatus UnlockFrame(mfxFrameSurface1 *frame);

    mfxFrameSurface1       *m_pIn;
    mfxFrameSurface1       *m_pOut;
    vaROIArray              m_ROIArray;
    mfxFrameAllocator      *m_pAlloc;

    std::vector<mfxU8> m_YIn, m_UVIn;
    std::vector<mfxU8> m_YOut, m_UVOut;
};

class PeopleDetectorProcessor : public Processor
{
public:
    PeopleDetectorProcessor(vaExtVPPDetectPeople *pParam);
    virtual ~PeopleDetectorProcessor();

    virtual mfxStatus Process();

private:
    void AnalyzeFrame(cv::Mat &frame);
    bool NV12Surface2RGBMat(mfxFrameSurface1* surface, cv::Mat &mat);
    bool RGBMat2NV12Surface(cv::Mat &mat, mfxFrameSurface1* surface);
    double ComputeDistance(float* src, float* ref, int length);
    int ExtractFeature(cv::Mat, float* v);
    int VA_Sample(cv::Mat &img, std::vector<cv::Rect> &rect, std::vector<float*> &f);
    std::vector<int> DoMatch(std::vector<float*> &f, int, float* ref, int, float);
    void filter_rects(const std::vector<cv::Rect>& candidates, std::vector<cv::Rect>& objects);

    vaExtVPPDetectPeople m_Param;
    mfxU64               m_currentTimeStamp;
    mfxU32               m_currentFrameOrder;
};
#endif // __FRAME_PROCESSOR_H
