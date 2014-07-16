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
    mfxFrameAllocator      *m_pAlloc;
};

class PeopleDetectorProcessor : public Processor
{
public:
    PeopleDetectorProcessor(vaExtVPPDetectPeople *pParam);
    virtual ~PeopleDetectorProcessor();

    virtual mfxStatus Process();

private:
    void AnalyzeFrame(cv::Mat &frame);
    mfxStatus NV12Surface2RGBMat(mfxFrameSurface1* surface, cv::Mat &mat);
    mfxStatus RGBMat2NV12Surface(cv::Mat &mat, mfxFrameSurface1* surface);
    mfxStatus CopyNV12Surface(mfxFrameSurface1* pSrcSurface, mfxFrameSurface1* pDstSurface);
    double ComputeDistance(std::vector<float>& src, float* ref, int length);
    int ExtractFeature(cv::Mat, std::vector<float> &feature);
    int VA_Sample(cv::Mat &img, std::vector<cv::Rect> &rect, std::vector<std::vector<float>> &f);
    std::vector<int> DoMatch(std::vector<std::vector<float>> &f, int, float* ref, int, float);
    void FilterRects(const std::vector<cv::Rect>& candidates, std::vector<cv::Rect>& objects);

    vaExtVPPDetectPeople m_Param;
    vaROIArray           m_ROIArray;
};
#endif // __FRAME_PROCESSOR_H
