/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "frameprocessor.h"

/* Processor class implementation */
Processor::Processor()
    : m_pIn(NULL)
    , m_pOut(NULL)
    , m_pAlloc(NULL)
{
}

Processor::~Processor()
{
}

mfxStatus Processor::SetAllocator(mfxFrameAllocator *pAlloc)
{
    m_pAlloc = pAlloc;
    return MFX_ERR_NONE;
}

mfxStatus Processor::Init(mfxFrameSurface1 *frame_in, mfxFrameSurface1 *frame_out)
{
    MSDK_CHECK_POINTER(frame_in, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(frame_out, MFX_ERR_NULL_PTR);

    m_pIn = frame_in;
    m_pOut = frame_out;

    return MFX_ERR_NONE;
}

mfxStatus Processor::LockFrame(mfxFrameSurface1 *frame)
{
    MSDK_CHECK_POINTER(frame, MFX_ERR_NULL_PTR);
    //double lock impossible
    if (frame->Data.Y != 0 && frame->Data.MemId !=0)
        return MFX_ERR_UNSUPPORTED;
    //no allocator used, no need to do lock
    if (frame->Data.Y != 0)
        return MFX_ERR_NONE;
    //lock required
    MSDK_CHECK_POINTER(m_pAlloc, MFX_ERR_NULL_PTR);
    return m_pAlloc->Lock(m_pAlloc->pthis, frame->Data.MemId, &frame->Data);
}

mfxStatus Processor::UnlockFrame(mfxFrameSurface1 *frame)
{
    MSDK_CHECK_POINTER(frame, MFX_ERR_NULL_PTR);
    //unlock not possible, no allocator used
    if (frame->Data.Y != 0 && frame->Data.MemId ==0)
        return MFX_ERR_NONE;
    //already unlocked
    if (frame->Data.Y == 0)
        return MFX_ERR_NONE;
    //unlock required
    MSDK_CHECK_POINTER(m_pAlloc, MFX_ERR_NULL_PTR);
    return m_pAlloc->Unlock(m_pAlloc->pthis, frame->Data.MemId, &frame->Data);
}

//=====================================//
//=== PeopleDetectorProcessor class ===//
//=====================================//

PeopleDetectorProcessor::PeopleDetectorProcessor(vaExtVPPDetectPeople *pParam) : Processor()
{
    MSDK_MEMCPY(&m_Param, pParam, sizeof(vaExtVPPDetectPeople));
}

PeopleDetectorProcessor::~PeopleDetectorProcessor()
{
}

mfxStatus PeopleDetectorProcessor::Process()
{
    mfxStatus sts = MFX_ERR_NONE;
    int i;

    if (MFX_ERR_NONE != (sts = LockFrame(m_pIn)))return sts;
    if (MFX_ERR_NONE != (sts = LockFrame(m_pOut)))
    {
        UnlockFrame(m_pIn);
        return sts;
    }
    MSDK_ZERO_MEMORY(m_ROIArray);

    cv::Mat* pFrame = new cv::Mat(m_pIn->Info.Height, m_pIn->Info.Width, CV_8UC3, cv::Scalar(255,255,255));

    sts = NV12Surface2RGBMat(m_pIn, *pFrame);
    if (sts == MFX_ERR_NONE)
    {
        AnalyzeFrame(*pFrame);
        if ((m_Param.RenderFlag == MFX_RENDER_ALWAYS || m_Param.RenderFlag == MFX_RENDER_IF_PRESENT) && m_ROIArray.NumROI > 0)
        {
            sts = RGBMat2NV12Surface(*pFrame, m_pOut);
        }
        else if (m_Param.RenderFlag == MFX_RENDER_ALWAYS && m_ROIArray.NumROI == 0)
        {
            sts = CopyNV12Surface(m_pIn, m_pOut);
        }
    }

    if (sts == MFX_ERR_NONE)
    {
        m_pOut->Data.TimeStamp = m_pIn->Data.TimeStamp;
        m_pOut->Data.FrameOrder = m_pIn->Data.FrameOrder;
        if (m_ROIArray.NumROI > 0 && m_pOut->Data.ExtParam != NULL)
        {
            for (i = 0; i < m_pOut->Data.NumExtParam; i++)
            {
                if (m_pOut->Data.ExtParam[i] != NULL)
                {
                    if (m_pOut->Data.ExtParam[i]->BufferId == VA_EXTBUFF_VPP_ROI_ARRAY && m_pOut->Data.ExtParam[i]->BufferSz == sizeof(vaROIArray))
                    {
                        vaROIArray *roiArray = (vaROIArray*) m_pOut->Data.ExtParam[i];
                        roiArray->NumROI = m_ROIArray.NumROI;
                        MSDK_MEMCPY_VAR(roiArray->ROI, &m_ROIArray.ROI, sizeof(roiArray->ROI));
                    }
                }
            }
        }
    }

    delete pFrame;

    sts = UnlockFrame(m_pIn);
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, MFX_ERR_NONE);
    sts = UnlockFrame(m_pOut);
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, MFX_ERR_NONE);

    return sts;
}

// Internal functions

mfxStatus PeopleDetectorProcessor::NV12Surface2RGBMat(mfxFrameSurface1* surface, cv::Mat &mat)
{
    MSDK_CHECK_POINTER(surface, MFX_ERR_NULL_PTR);

    unsigned char* row;
    unsigned char* pY = surface->Data.Y;
    unsigned char* pU = surface->Data.U;
    unsigned char* pV = surface->Data.V;

    unsigned char* ptr;

    int w, h;
    int Y,U,V,R,G,B;

    for(h = 0; h < surface->Info.Height; h++)
    {
        row = mat.data + mat.step[0]*h;

        for(w = 0; w < surface->Info.Width; w++)
        {
            Y = pY[h * surface->Data.Pitch + w];
            U = (pU[(h / 2) * surface->Data.Pitch + (w / 2) * 2]);
            V = (pU[(h / 2) * surface->Data.Pitch + (w / 2) * 2 + 1]);

            B = 1.164 * (Y - 16) + 2.018 * (U - 128);
            G = 1.164 * (Y - 16) - 0.813 * (V - 128) - 0.391 * (U - 128);
            R = 1.164 * (Y - 16) + 1.596 * (V - 128);

            if(R > 255) R=255;
            if(G > 255) G=255;
            if(B > 255) B=255;
            if(R < 0) R=0;
            if(G < 0) G=0;
            if(B < 0) B=0;

            ptr = row + mat.step[1] * w;
            *(ptr) = B;
            *(ptr + 1) = G;
            *(ptr + 2) = R;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectorProcessor::RGBMat2NV12Surface(cv::Mat &mat, mfxFrameSurface1* surface)
{
    MSDK_CHECK_POINTER(surface, MFX_ERR_NULL_PTR);

    unsigned char* row, *ptr;
    unsigned char* pY = surface->Data.Y;
    unsigned char* pUV = surface->Data.UV;

    int w, h;
    int Y,U,V,R,G,B;

    for(h = 0; h < surface->Info.Height; h++)
    {
        row = mat.data + mat.step[0]*h;

        for(w = 0; w < surface->Info.Width; w++)
        {
            ptr = row + mat.step[1] * w;
            B = *(ptr);
            G = *(ptr + 1);
            R = *(ptr + 2);

            Y = ( (  65 * R + 128 * G +  24 * B + 128) >> 8) +  16;
            U = ( ( -37 * R -  74 * G + 112 * B + 128) >> 8) + 128;
            V = ( ( 112 * R -  93 * G -  18 * B + 128) >> 8) + 128;

            pY[h * surface->Data.Pitch + w] = Y;
            (pUV[(h / 2) * surface->Data.Pitch + (w / 2) * 2]) = U;
            (pUV[(h / 2) * surface->Data.Pitch + (w / 2) * 2 + 1]) = V;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus PeopleDetectorProcessor::CopyNV12Surface(mfxFrameSurface1* pSrcSurface, mfxFrameSurface1* pDstSurface)
{
    MSDK_CHECK_POINTER(pSrcSurface, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pDstSurface, MFX_ERR_NULL_PTR);

    unsigned char* pSrcY = pSrcSurface->Data.Y;
    unsigned char* pSrcUV = pSrcSurface->Data.UV;
    unsigned char* pDstY = pDstSurface->Data.Y;
    unsigned char* pDstUV = pDstSurface->Data.UV;

    for(int h = 0; h < pSrcSurface->Info.Height; h++)
    {
        MSDK_MEMCPY(pDstY + h * pDstSurface->Data.Pitch, pSrcY + h * pSrcSurface->Data.Pitch, pSrcSurface->Info.Width);
        MSDK_MEMCPY(pDstUV + (h / 2)*pDstSurface->Data.Pitch, pSrcUV + (h / 2) * pSrcSurface->Data.Pitch, pSrcSurface->Info.Width);
    }

    return MFX_ERR_NONE;
}

void PeopleDetectorProcessor::AnalyzeFrame(cv::Mat &frame)
{
    mfxStatus   sts = MFX_ERR_NONE;
    std::vector<std::vector<float>> img_vectors;
    std::vector<cv::Rect> rect_out;
    std::vector<int> obj_matched;

    int num_objects;
    float ref[3];
    int i;

    ref[0] = 0.0;
    ref[1] = 0.0;
    ref[2] = 255.0;
    num_objects = VA_Sample(frame, rect_out, img_vectors);

    obj_matched = DoMatch(img_vectors, num_objects, ref, 3, m_Param.Treshold);
    if(obj_matched.size() > 0)
    {
        for(i = 0; i < obj_matched.size(); i++)
        {
            cv::Rect r_out = rect_out[obj_matched[i]];
            if (m_ROIArray.NumROI < MAX_ROI_NUM)
            {
                m_ROIArray.ROI[m_ROIArray.NumROI].Top = r_out.y;
                m_ROIArray.ROI[m_ROIArray.NumROI].Left = r_out.x;
                m_ROIArray.ROI[m_ROIArray.NumROI].Right = r_out.x + r_out.width;
                m_ROIArray.ROI[m_ROIArray.NumROI].Bottom = r_out.y + r_out.height;
                m_ROIArray.NumROI++;
            }
            cv::rectangle(frame, r_out.tl(), r_out.br(), cv::Scalar(0, 255, 0), 1);
        }
    }
    obj_matched.clear();
    rect_out.clear();

    if (img_vectors.size() > 0)
    {
        for (i = 0; i < img_vectors.size(); i++)
        {
            img_vectors.at(i).clear();
        }
        img_vectors.clear();
    }

}

void PeopleDetectorProcessor::FilterRects(const std::vector<cv::Rect>& candidates, std::vector<cv::Rect>& objects)
{
    int i,j;
    for(i = 0; i < candidates.size(); i++)
    {
        cv::Rect rec = candidates[i];
        for(j = 0; j < candidates.size(); j++ )
            if(j != i && (rec & candidates[j]) == rec)
                break;
        if(j == candidates.size())
            objects.push_back(rec);
    }
}

int PeopleDetectorProcessor::VA_Sample(cv::Mat &img, std::vector<cv::Rect> &rect, std::vector<std::vector<float>> &f)
{
    std::vector<cv::Rect> found;
    int num_objects=0;

    if(img.empty())
        return -1;

    cv::HOGDescriptor hog;
    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
    hog.detectMultiScale(img, found, 0, cv::Size(8,8), cv::Size(32,32), 1.05, 2);

    FilterRects(found, rect);

    for(int i = 0; i < rect.size(); i++ )
    {
        cv::Rect r = rect[i];
        r.x += cvRound(r.width * 0.1);
        r.width = cvRound(r.width * 0.8);
        r.y += cvRound(r.height * 0.07);
        r.height = cvRound(r.height * 0.85);

        if(r.x < 0 || r.y < 0)
        {
            rect.erase(rect.begin() + i);
            continue;
        }
        if((r.x + r.width) > img.cols || (r.y + r.height) > img.rows)
        {
            rect.erase(rect.begin() + i);
            continue;
        }

        cv::Mat roi(img, r);
        std::vector<float> feature;
        feature.resize(NUM_FEATURE_PER_OBJECT);
        ExtractFeature(roi, feature);
        f.push_back(feature);

        num_objects++;
    }

    return num_objects;
}

int PeopleDetectorProcessor::ExtractFeature(cv::Mat roi, std::vector<float> &feature)
{
    int sum_b, sum_g, sum_r;
    sum_b = sum_g = sum_r = 0;
    int i, j, x, y;
    unsigned char* row;
    int ch = roi.channels();

    //get the upper body color at cols/2, rows/4
    x=(int)(roi.cols / 2) - 4;
    y=(int)(roi.rows / 4) - 4;

    for(i = 0; i < 8; i++)
    {
        row = roi.ptr<unsigned char>(y + i);
        for(j = 0; j < 8; j++)
        {
            sum_b += (int)row[x * ch + j * ch];
            sum_g += (int)row[x * ch + j * ch+1];
            sum_r += (int)row[x * ch + j * ch + 2];
        }
    }

    feature.at(0) = ((float)sum_b / 64);
    feature.at(1) = ((float)sum_g / 64);
    feature.at(2) = ((float)sum_r / 64);

    //get the lower part of the body color at cols/2, rows/4*3
    sum_b = 0;
    sum_g = 0;
    sum_r = 0;

    x = (int)(roi.cols / 2) - 4;
    y = (int)(roi.rows / 4 * 3) - 4;
    for(i = 0; i < 8; i++)
    {
        row = roi.ptr<unsigned char>(y+i);
        for(j = 0; j < 8; j++)
        {
            sum_b += (int)row[x * ch + j * ch];
            sum_g += (int)row[x * ch + j * ch + 1];
            sum_r += (int)row[x * ch + j * ch + 2];
        }
    }

    feature.at(3) = ((float)sum_b / 64);
    feature.at(4) = ((float)sum_g / 64);
    feature.at(5) = ((float)sum_r / 64);

    return NUM_FEATURE_PER_OBJECT;
}

std::vector<int> PeopleDetectorProcessor::DoMatch(std::vector<std::vector<float>> &features, int num_objects, float* ref, int feature_len, float threshold)
{
    int i;
    int objects;
    double distance;
    std::vector<float> one_object;
    std::vector<int> matched;

    objects = num_objects <= features.size() ? num_objects:features.size();
    for(i = 0; i < objects; i++)
    {
        one_object = features.at(i);

        distance = ComputeDistance(one_object, ref, feature_len);
        if(distance <= threshold)
        {
            matched.push_back(i);
        }
    }

    return matched;
}

double PeopleDetectorProcessor::ComputeDistance(std::vector<float>& src, float* ref, int length )
{
    int i;
    double sqre_sum = 0;
    double distance = 0.0;

    for(i = 0; i < length; i++)
    {
        sqre_sum += pow(abs(src.at(i) - ref[i]), 2);
    }

    distance = sqrt(sqre_sum);
    return distance;
}
