/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2007-2008 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"

#if defined (UMC_ENABLE_UMC_SCENE_ANALYZER)

#include "umc_scene_analyzer_base.h"
#include "umc_video_data.h"

namespace UMC
{

enum
{
    MAX_B_LENGTH                = 50
};

SceneAnalyzerParams::SceneAnalyzerParams(void)
{
    m_maxGOPLength = 0;
    m_maxBLength = 0;
    m_maxDelayTime = 0;
    m_interlaceType = PROGRESSIVE;
} // SceneAnalyzerParams::SceneAnalyzerParams(void)

SceneAnalyzerParams::~SceneAnalyzerParams(void)
{

} // SceneAnalyzerParams::~SceneAnalyzerParams(void)


SceneAnalyzerBase::SceneAnalyzerBase(void)
{
    m_frameNum = 0;
    m_bFrameNum = 0;
    m_frameCount = 0;

} // SceneAnalyzerBase::SceneAnalyzerBase(void)

SceneAnalyzerBase::~SceneAnalyzerBase(void)
{
    Close();

} // SceneAnalyzerBase::~SceneAnalyzerBase(void)


Status SceneAnalyzerBase::Init(BaseCodecParams *pParams)
{
    const SceneAnalyzerParams *pSAParams = DynamicCast<SceneAnalyzerParams> (pParams);

    // check error(s)
    if (NULL == pSAParams)
        return UMC_ERR_NULL_PTR;

    // release the object before initialization
    Close();

    // copy parameters
    m_params = *pSAParams;

    // correct max B frames to avoid too long GOPs
    if (MAX_B_LENGTH < m_params.m_maxBLength)
    {
        m_params.m_maxBLength = MAX_B_LENGTH;
    }

    return UMC_OK;

} // Status SceneAnalyzerBase::Init(BaseCodecParams *pParams)

Status SceneAnalyzerBase::GetFrame(MediaData *pSource, MediaData *pDestination)
{
    VideoData *pDst = DynamicCast<VideoData> (pDestination);
    VideoData *pSrc = DynamicCast<VideoData> (pSource);
    FrameType frameType;

    // check error(s)
    if ((NULL == pDst) || (NULL == pSrc))
        return UMC_ERR_NULL_PTR;

    // get frame type for this picture
    frameType = GetPlannedFrameType();
    // correct frame type in case of unknown GOP structure
    if (NONE_PICTURE == frameType)
    {
        frameType = P_PICTURE;
    }

    // initialize destination image
    {
        ColorFormat colorFormat = pSrc->GetColorFormat();
        IppiSize picSize;
        Ipp32u numPlanes, plane;
        Status umcRes;

        picSize.width = pSrc->GetWidth();
        picSize.height = pSrc->GetHeight();

        umcRes = pDst->Init(picSize.width, picSize.height, colorFormat);
        if (UMC_OK != umcRes)
            return umcRes;

        // copy planes info
        numPlanes = pSrc->GetNumPlanes();
        for (plane = 0; plane < numPlanes; plane += 1)
        {
            VideoData::PlaneInfo planeInfo;

            // get source plane info
            umcRes = pSrc->GetPlaneInfo(&planeInfo, plane);
            if (UMC_OK != umcRes)
                return umcRes;

            pDst->SetPlanePointer(planeInfo.m_pPlane, plane);
            pDst->SetPlanePitch(planeInfo.m_nPitch, plane);
            pDst->SetPlaneBitDepth(planeInfo.m_iBitDepth, plane);
            pDst->SetPlaneSampleSize(planeInfo.m_iSampleSize, plane);
        }
    }

    // set the picture type
    pDst->SetFrameType(frameType);

    // increment frame counter(s)
    m_frameCount += 1;
    m_frameNum += 1;
    if (B_PICTURE == frameType)
    {
        m_bFrameNum += 1;
    }
    else
    {
        m_bFrameNum = 0;
    }

    return UMC_OK;

} // Status SceneAnalyzerBase::GetFrame(MediaData *pSource, MediaData *pDestination)

Status SceneAnalyzerBase::Close(void)
{
    return Reset();

} // Status SceneAnalyzerBase::Close(void)

Status SceneAnalyzerBase::Reset(void)
{
    m_frameNum = 0;
    m_bFrameNum = 0;
    m_frameCount = 0;

    return UMC_OK;

} // Status SceneAnalyzerBase::Reset(void)

FrameType SceneAnalyzerBase::GetPlannedFrameType(void)
{
    FrameType plannedFrameType;

    if (0 < m_params.m_maxGOPLength)
    {
        // set the I frame - the beginning of the GOP
        if (0 == (m_frameNum % m_params.m_maxGOPLength))
        {
            plannedFrameType = I_PICTURE;
        }
        // there are only P frames
        else if (0 == m_params.m_maxBLength)
        {
            plannedFrameType = P_PICTURE;
        }
        // decide what type of inter frame to use
        else if (0 < m_params.m_maxBLength)
        {
            // the first frame after I,
            // the last frames in the GOP,
            // and all reference frames have P type
            if ((1 == m_frameNum) ||
                (0 == ((m_frameNum + 1) % m_params.m_maxGOPLength)) ||
                (m_bFrameNum >= m_params.m_maxBLength))
            {
                plannedFrameType = P_PICTURE;
            }
            // all other are B frames
            else
            {
                plannedFrameType = B_PICTURE;
            }
        }
        else
        {
            // the scene analyzer should decide the type
            plannedFrameType = NONE_PICTURE;
        }
    }
    // the GOP structure is unknown
    else
    {
        // set the I frame - the beginning of the GOP
        if (0 == m_frameNum)
        {
            plannedFrameType = I_PICTURE;
        }
        // there are only P frames
        else if (0 == m_params.m_maxBLength)
        {
            plannedFrameType = P_PICTURE;
        }
        // decide what type of inter frame to use
        else if (0 < m_params.m_maxBLength)
        {
            // the first frame after I,
            // all reference frames have P type
            if ((1 == m_frameNum) ||
                (m_bFrameNum >= m_params.m_maxBLength))
            {
                plannedFrameType = P_PICTURE;
            }
            // all other are B frames
            else
            {
                plannedFrameType = B_PICTURE;
            }
        }
        else
        {
            // the scene analyzer should decide the type
            plannedFrameType = NONE_PICTURE;
        }
    }

    return plannedFrameType;

} // FrameType SceneAnalyzerBase::GetPlannedFrameType(void)

} // namespace UMC
#endif
