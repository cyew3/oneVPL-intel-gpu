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

PeopleDetectorProcessor::PeopleDetectorProcessor(msdkExtVPPDetectPeople *pParam) : Processor()
{
    memcpy(&m_Param, pParam, sizeof(msdkExtVPPDetectPeople));
    m_currentTimeStamp = 0;
    m_currentFrameOrder = 0;
}

PeopleDetectorProcessor::~PeopleDetectorProcessor()
{
}

mfxStatus PeopleDetectorProcessor::Process()
{
    mfxStatus sts = MFX_ERR_NONE;
    if (MFX_ERR_NONE != (sts = LockFrame(m_pIn)))return sts;
    if (MFX_ERR_NONE != (sts = LockFrame(m_pOut)))
    {
        UnlockFrame(m_pIn);
        return sts;
    }

    cv::Mat* pFrame = new cv::Mat(m_pIn->Info.Height, m_pIn->Info.Width, CV_8UC3, cv::Scalar(255,255,255));

    NV12Surface2RGBMat(m_pIn, *pFrame);

    AnalyzeFrame(*pFrame);

    RGBMat2NV12Surface(*pFrame, m_pOut);

    memcpy(&m_pOut->Info, &m_pIn->Info, sizeof(mfxFrameInfo));

    m_pOut->Data.TimeStamp = m_pIn->Data.TimeStamp;
    m_pOut->Data.FrameOrder = m_pIn->Data.FrameOrder;

    sts = UnlockFrame(m_pIn);
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, MFX_ERR_NONE);
    sts = UnlockFrame(m_pOut);
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, MFX_ERR_NONE);

    delete pFrame;

    return sts;
}

// Internal functions

bool PeopleDetectorProcessor::NV12Surface2RGBMat(mfxFrameSurface1* surface, cv::Mat &mat)
{
    if (surface == NULL)
        return false;

    unsigned char* row;
    unsigned char* pY = surface->Data.Y;
    unsigned char* pU = surface->Data.U;
    unsigned char* pV = surface->Data.V;

    unsigned char* ptr;
//    printf( "SurfaceToMat: pY=%x, pU=%x, pV=%x, row=%x\n", pY, pU, pV, row );

    int w, h;
    int Y,U,V,R,G,B;

    for(h=0;h<surface->Info.Height;h++)
    {
        row = mat.data + mat.step[0]*h;

        for(w=0;w<surface->Info.Width;w++)
        {
            Y = pY[h*surface->Data.Pitch+w];
            U = (pU[(h/2)*surface->Data.Pitch + (w/2)*2])-128;
            V = (pU[(h/2)*surface->Data.Pitch + (w/2)*2 + 1])-128;

            int rdif = V +((V*103)>>8);
            int invgdif = ((U*88)>>8) + ((V*183)>>8);
            int bdif = U + ((U*198)>>8);

            R = Y + rdif;
            G = Y - invgdif;
            B = Y + bdif;

            if( R>255 ) R=255;
            if( G>255 ) G=255;
            if( B>255 ) B=255;
            if( R<0 ) R=0;
            if( G<0 ) G=0;
            if( B<0 ) B=0;

            ptr = row + mat.step[1] * w;
            *(ptr) = B;
            *(ptr+1) = G;
            *(ptr+2) = R;
        }
    }

    return true;
}

bool PeopleDetectorProcessor::RGBMat2NV12Surface(cv::Mat &mat, mfxFrameSurface1* surface)
{
    if (surface == NULL)
        return false;

    unsigned char* row;
    unsigned char* pY = surface->Data.Y;
    unsigned char* pU = surface->Data.U;
    unsigned char* pV = surface->Data.V;

    unsigned char* ptr;

    int w, h;
    int Y,U,V,R,G,B;

    for(h=0;h<surface->Info.Height;h++)
    {
        row = mat.data + mat.step[0]*h;

        for(w=0;w<surface->Info.Width;w++)
        {
            ptr = row + mat.step[1] * w;
            B = *(ptr);
            G = *(ptr+1);
            R = *(ptr+2);

            Y = ( (  65 * R + 128 * G +  24 * B + 128) >> 8) +  16;
            U = ( ( -37 * R -  74 * G + 112 * B + 128) >> 8) + 128;
            V = ( ( 112 * R -  93 * G -  18 * B + 128) >> 8) + 128;

            pY[h*surface->Data.Pitch+w] = Y;
            (pU[(h/2)*surface->Data.Pitch + (w/2)*2]) = U;
            (pU[(h/2)*surface->Data.Pitch + (w/2)*2 + 1]) = V;
        }
    }

    return true;
}

void PeopleDetectorProcessor::AnalyzeFrame(cv::Mat &frame)
{
    mfxStatus   sts = MFX_ERR_NONE;
    std::vector<float*> img_vectors;
    std::vector<cv::Rect> rect_out;
    std::vector<int> obj_matched;

    int num_objects;
    float ref[3];
    float* obj;

    ref[0] = 0.0;
    ref[1] = 0.0;
    ref[2] = 255.0;
    num_objects = VA_Sample( frame, rect_out, img_vectors );
    //printf( "Found %d candidates do matching\n", num_objects);

    for (int i = 0; i < num_objects; i++)
    {
        obj = img_vectors[i];
        //printf("upper color: %6f:%6f:%6f\n", obj[0], obj[1], obj[2]);
        //printf("lower color: %6f:%6f:%6f\n", obj[3], obj[4], obj[5]);
    }
    obj_matched = DoMatch( img_vectors, num_objects, ref, 3, m_Param.Treshold);
    if( obj_matched.size() > 0 )
    {
        //printf( "matched %d objects\n", obj_matched.size() );
        for(int kk=0; (kk < obj_matched.size()); kk++)
        {
            cv::Rect r_out = rect_out[obj_matched[kk]];
            cv::rectangle( frame, r_out.tl(), r_out.br(), cv::Scalar(0,255,0), 1 );
        }
    }
    obj_matched.clear();
    rect_out.clear();

    for (int i = 0; i < img_vectors.size(); i++)
    {
        if (img_vectors.at(i) != NULL)
            free(img_vectors.at(i));
    }
}

void PeopleDetectorProcessor::filter_rects(const std::vector<cv::Rect>& candidates, std::vector<cv::Rect>& objects)
{
    int i,j;

    for( i=0;i<candidates.size();i++ )
    {
        cv::Rect rec = candidates[i];
        for( j=0;j<candidates.size();j++ )
            if(j!=i && (rec & candidates[j])==rec)
                break;
        if(j==candidates.size())
            objects.push_back(rec);
    }
}

int PeopleDetectorProcessor::VA_Sample( cv::Mat &img, std::vector<cv::Rect> &rect, std::vector<float*> &f )
{
    std::vector<cv::Rect> found;
    float* features = NULL;
    int num_objects=0;

    if(img.empty())
        return -1;

    cv::HOGDescriptor hog;
    hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
    hog.detectMultiScale(img, found, 0, cv::Size(8,8), cv::Size(32,32), 1.05, 2);

    filter_rects(found, rect);

    if (rect.size() > 0)
        features = (float*)malloc( rect.size()*sizeof(float)*NUM_FEATURE_PER_OBJECT );

    for(int i=0;i<rect.size();i++ )
    {
        cv::Rect r = rect[i];
        r.x += cvRound(r.width*0.1);
        r.width = cvRound(r.width*0.8);
        r.y += cvRound(r.height*0.07);
        r.height = cvRound(r.height*0.85);

        if(r.x<0 || r.y<0) {
            rect.erase(rect.begin()+i);
            continue;
        }
        if((r.x+r.width)>img.cols || (r.y+r.height)>img.rows) {
            rect.erase(rect.begin()+i);
            continue;
        }

        cv::Mat roi( img, r );
        ExtractFeature( roi, &features[num_objects * NUM_FEATURE_PER_OBJECT] );
        f.push_back(&features[num_objects * NUM_FEATURE_PER_OBJECT]);

        num_objects++;
    }

    return num_objects;
}

int PeopleDetectorProcessor::ExtractFeature( cv::Mat roi, float* roi_vector )
{
    int sum_b, sum_g, sum_r;
    sum_b=sum_g=sum_r=0;
    int i, j, x, y;
    unsigned char* row;
    int ch = roi.channels();

    //get the upper body color at cols/2, rows/4
    x=(int)(roi.cols/2)-4;
    y=(int)(roi.rows/4)-4;

    for(i=0;i<8;i++)
    {
        row = roi.ptr<unsigned char>(y+i);
        for(j=0;j<8;j++)
        {
            sum_b += (int)row[x*ch+j*ch];
            sum_g += (int)row[x*ch+j*ch+1];
            sum_r += (int)row[x*ch+j*ch+2];
        }
    }

    roi_vector[0]=((float)sum_b/64);
    roi_vector[1]=((float)sum_g/64);
    roi_vector[2]=((float)sum_r/64);

    //get the lower part of the body color at cols/2, rows/4*3
    sum_b = 0;
    sum_g = 0;
    sum_r = 0;

    x=(int)(roi.cols/2)-4;
    y=(int)(roi.rows/4*3)-4;
    for(i=0;i<8;i++)
    {
        row = roi.ptr<unsigned char>(y+i);
        for(j=0;j<8;j++)
        {
            sum_b += (int)row[x*ch+j*ch];
            sum_g += (int)row[x*ch+j*ch+1];
            sum_r += (int)row[x*ch+j*ch+2];
        }
    }

    roi_vector[3]=((float)sum_b/64);
    roi_vector[4]=((float)sum_g/64);
    roi_vector[5]=((float)sum_r/64);

    return NUM_FEATURE_PER_OBJECT;
}

std::vector<int> PeopleDetectorProcessor::DoMatch(std::vector<float*> &features, int num_objects, float* ref, int feature_len, float threshold)
{
    int i;
    int objects;
    double distance;
    float* one_object;
    std::vector<int> matched;

    objects = num_objects<=features.size()? num_objects:features.size();
    for(i=0;i<objects;i++)
    {
        one_object = features[i];

        distance = ComputeDistance( one_object, ref, feature_len );
        if(distance<=threshold)
        {
            matched.push_back(i);
            //printf( "Distance for object %d=%f\n", i, distance );
        }

    }
    features.clear();

    return matched;
}

double PeopleDetectorProcessor::ComputeDistance( float* src, float* ref, int length )
{
    int i;
    double sqre_sum = 0;
    double distance = 0.0;

    for(i=0;i<length;i++)
    {
        sqre_sum += pow(abs(src[i]-ref[i]),2);
    }

    distance = sqrt( sqre_sum );
    return distance;
}
