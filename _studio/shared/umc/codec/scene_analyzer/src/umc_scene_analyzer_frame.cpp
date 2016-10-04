/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2007-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"

#if defined (UMC_ENABLE_UMC_SCENE_ANALYZER)
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <ippi.h>
#include <ipps.h>
#include "umc_scene_analyzer_frame.h"

namespace UMC
{

enum
{
    SA_STEP_ALIGN               = 64
};

SceneAnalyzerPicture::SceneAnalyzerPicture(void) :m_info()
{
    memset(&m_pPic, 0, sizeof(m_pPic));
    m_picSize = 0;
    m_picStep = 0;
    m_picDim.width = m_picDim.height = 0;
    m_mbDim.width = m_mbDim.height = 0;
    m_bChangeDetected = true;

    m_pSliceInfo = (UMC_SCENE_INFO *) 0;
    m_numSliceInfo = 0;
    m_colorFormat = NONE;
} // SceneAnalyzerPicture::SceneAnalyzerPicture(void)

SceneAnalyzerPicture::~SceneAnalyzerPicture(void)
{
    // deinitialize the object
    Close();

    // release the buffer
    ReleaseBuffer();
    ReleaseSliceInfo();

} // SceneAnalyzerPicture::~SceneAnalyzerPicture(void)

void SceneAnalyzerPicture::Close(void)
{
    m_picStep = 0;
    m_picDim.width = m_picDim.height = 0;
    m_mbDim.width = m_mbDim.height = 0;
    m_bChangeDetected = true;

    m_numSliceInfo = 0;

} // void SceneAnalyzerPicture::Close(void)

Status SceneAnalyzerPicture::Init(Ipp32s srcWidth, Ipp32s srcHeight,
                                  ColorFormat colorFormat)
{
    size_t picStep, picSize;
    IppiSize picDim = {srcWidth, srcHeight};
    Status umcRes;

    // check error
    if ((0 == srcWidth) || (0 == srcHeight))
        return UMC_ERR_INVALID_PARAMS;

    // close the object before the initialization
    Close();

    // calculate step
    picStep = align_value<size_t> (srcWidth, SA_STEP_ALIGN);

    // calculate picture size
    switch (colorFormat)
    {
    case YV12:
    case NV12:
    case YUV420:
        picSize = (picStep * srcHeight * 3) / 2;
        break;

    case YUV422:
        picSize = picStep * srcHeight * 2;
        break;

    case YUV444:
        picSize = picStep * srcHeight * 3;
        break;

    case GRAY:
        picSize = picStep * srcHeight;
        break;

    default:
        return UMC_ERR_INVALID_PARAMS;
    }

    // allocate the buffer
    umcRes = AllocateBuffer(picSize);
    if (UMC_OK != umcRes)
        return umcRes;

    // set the chroma pointers
    switch (colorFormat)
    {
    case YV12:
    case NV12:
    case YUV420:
    case YUV422:
        m_pPic[1] = m_pPic[0] + picStep * srcHeight;
        m_pPic[2] = m_pPic[1] + picStep / 2;
        break;

    case YUV444:
        m_pPic[1] = m_pPic[0] + picStep * srcHeight;
        m_pPic[2] = m_pPic[1] + picStep * srcHeight;
        break;

    case GRAY:
        break;
    default:
        break;
    }

    // save parameters
    m_picStep = picStep;
    m_picDim = picDim;
    m_mbDim.width = picDim.width / 4;
    m_mbDim.height = picDim.height / 4;
    m_colorFormat = colorFormat;

    // allocate slice info items
    umcRes = AllocateSliceInfo(m_mbDim.height);
    if (UMC_OK != umcRes)
        return umcRes;

    return UMC_OK;

} // Status SceneAnalyzerPicture::Init(Ipp32s srcWidth, Ipp32s srcHeight,

Status SceneAnalyzerPicture::SetPointer(const Ipp8u *pPic, size_t picStep,
                                        Ipp32s srcWidth, Ipp32s srcHeight)
{
    IppiSize picDim = {srcWidth, srcHeight};

    // close the object before the initialization
    Close();

    // release the allocated buffer
    ReleaseBuffer();

    // save parameters
    m_pPic[0] = pPic;
    m_picStep = picStep;
    m_picDim = picDim;
    m_mbDim.width = picDim.width / 4;
    m_mbDim.height = picDim.height / 4;
    m_colorFormat = GRAY;

    return UMC_OK;

} // Status SceneAnalyzerPicture::SetPointer(const Ipp8u *pPic, size_t picStep,

Status SceneAnalyzerPicture::AllocateBuffer(size_t bufSize)
{
    // check size of allocated buffer
    if (m_picSize >= bufSize)
        return UMC_OK;

    // release the buffer before allocation of new one
    ReleaseBuffer();

    // allocate new buffer
    m_pPic[0] = ippsMalloc_8u((int) bufSize);
    if (NULL == m_pPic[0])
    {
        return UMC_ERR_ALLOC;
    }

    // save allocate size
    m_picSize = bufSize;

    return UMC_OK;

} // Status SceneAnalyzerPicture::AllocateBuffer(size_t bufSize)

void SceneAnalyzerPicture::ReleaseBuffer(void)
{
    // we identify the allocated buffer by its size.
    // Note, that the buffer can be set to the object externally.
    if ((m_picSize) && (m_pPic[0]))
    {
        ippsFree((void *) m_pPic[0]);
    }

    memset(&m_pPic, 0, sizeof(m_pPic));
    m_picSize = 0;

} // void SceneAnalyzerPicture::ReleaseBuffer(void)

Status SceneAnalyzerPicture::AllocateSliceInfo(size_t numSliceInfo)
{
    // check number of already allocated buffers
    if (m_numSliceInfo >= numSliceInfo)
    {
        return UMC_OK;
    }

    // release the allocated slice info strucures before allocation
    ReleaseSliceInfo();

    // allocate new slice info structures
    m_pSliceInfo = (UMC_SCENE_INFO *) ippsMalloc_8u((int) (sizeof(UMC_SCENE_INFO) * numSliceInfo));
    if (NULL == m_pSliceInfo)
    {
        return UMC_ERR_ALLOC;
    }

    // save allocate size
    m_numSliceInfo = numSliceInfo;

    return UMC_OK;

} // Status SceneAnalyzerPicture::AllocateSliceInfo(size_t numSliceInfo)

void SceneAnalyzerPicture::ReleaseSliceInfo(void)
{
    if (m_pSliceInfo)
    {
        ippsFree(m_pSliceInfo);
    }

    m_pSliceInfo = (UMC_SCENE_INFO *) 0;
    m_numSliceInfo = 0;

} // void SceneAnalyzerPicture::ReleaseSliceInfo(void)


SceneAnalyzerFrame::SceneAnalyzerFrame(void)
{
    m_frameStructure = PROGRESSIVE;
    m_frameEstimation = PROGRESSIVE;

    m_workBuff.reset(0);
    m_srcSize.width  = m_dstSize.width  = 0;
    m_srcSize.height = m_dstSize.height = 0;

    m_frameType = NONE_PICTURE;

} // SceneAnalyzerFrame::SceneAnalyzerFrame(void)

SceneAnalyzerFrame::~SceneAnalyzerFrame(void)
{
    m_workBuff.reset(0);
    m_srcSize.width  = m_dstSize.width  = 0;
    m_srcSize.height = m_dstSize.height = 0;

} // SceneAnalyzerFrame::~SceneAnalyzerFrame(void)

static
IppiSize GetScaledImageSize(IppiSize srcDim)
{
    IppiSize dstDim;

    // try to scale to something ~300 X ~200
    switch (srcDim.width)
    {
    case 640:
    case 704:
    case 720:
    case 768:
        dstDim.width = srcDim.width / 2;
        dstDim.height = srcDim.height / 2;
        break;

    case 960:
        dstDim.width = srcDim.width / 3;
        dstDim.height = srcDim.height / 3;
        break;

    case 1280:
        dstDim.width = srcDim.width / 4;
        dstDim.height = srcDim.height / 3;
        break;

    case 1440:
        dstDim.width = srcDim.width / 4;
        dstDim.height = srcDim.height / 4;
        break;

    case 1920:
        dstDim.width = srcDim.width / 6;
        dstDim.height = srcDim.height / 4;
        break;

    default:
        // too small image,
        // do not down scale
        if (352 >= srcDim.width)
        {
            dstDim = srcDim;
        }
        else
        {
            dstDim.width = 320;
            dstDim.height = 240;
        }
    }

    // align destination dimensions
    dstDim.width = dstDim.width & -4;
    dstDim.height = dstDim.height & -4;

    return dstDim;

} // IppiSize GetScaledImageSize(IppiSize srcDim)

Status SceneAnalyzerFrame::SetSource(VideoData *pSrc, InterlaceType interlaceType)
{
    VideoData::PlaneInfo planeInfo;
    Status umcRes;
    Ipp32s i;

    // check error(s)
    if (NULL == pSrc)
    {
        return UMC_ERR_NULL_PTR;
    }

    // check color format
    if (false == CheckSupportedColorFormat(pSrc))
    {
        return UMC_ERR_INVALID_PARAMS;
    }

    // get plane parameters
    umcRes = pSrc->GetPlaneInfo(&planeInfo, 0);
    if (UMC_OK != umcRes)
        return umcRes;
    if (8 != planeInfo.m_iBitDepth)
        return UMC_ERR_INVALID_PARAMS;

    // set frame & fields pointers
    umcRes = SceneAnalyzerPicture::Init(planeInfo.m_ippSize.width,
                                        planeInfo.m_ippSize.height,
                                        pSrc->GetColorFormat());
    if (UMC_OK != umcRes)
        return umcRes;
    if (PROGRESSIVE != interlaceType)
    {
        umcRes = m_fields[0].SetPointer(m_pPic[0],
                                        m_picStep * 2,
                                        m_picDim.width,
                                        m_picDim.height / 2);
        if (UMC_OK != umcRes)
            return umcRes;
        umcRes = m_fields[1].SetPointer(m_pPic[0] + m_picStep,
                                        m_picStep * 2,
                                        m_picDim.width,
                                        m_picDim.height / 2);
        if (UMC_OK != umcRes)
            return umcRes;
    }
    // copy picture
    for (i = 0; i < pSrc->GetNumPlanes(); i += 1)
    {
        VideoData::PlaneInfo tmpPlaneInfo;

        // get plane parameters
        umcRes = pSrc->GetPlaneInfo(&tmpPlaneInfo, i);
        if (UMC_OK != umcRes)
            return umcRes;

        // copy data
        ippiCopy_8u_C1R(tmpPlaneInfo.m_pPlane,
                        (int) tmpPlaneInfo.m_nPitch,
                        (Ipp8u *) m_pPic[i],
                        (int) m_picStep,
                        tmpPlaneInfo.m_ippSize);
    }

    // initialize the scaled picture
    {
        IppiSize srcDim = {m_picDim.width, m_picDim.height / 2};srcDim;
        IppiRect srcRoi = {0, 0, srcDim.width, srcDim.width};srcRoi;
        IppiSize dstDim = GetScaledImageSize(m_picDim);dstDim;

        // initialize the scaled image
        umcRes = m_scaledPic.Init(dstDim.width, dstDim.height, GRAY);
        if (UMC_OK != umcRes)
            return umcRes;

        IppStatus ippSts;
        // work buffer
        if( (m_srcSize.width  == srcDim.width) && 
            (m_srcSize.height == srcDim.height) && 
            (m_dstSize.width  == dstDim.width) && 
            (m_dstSize.height == dstDim.height) && 
            m_workBuff.get())
        {
            // nothing
        }
        else
        {
            m_workBuff.reset(0);

            m_srcSize.width  = srcDim.width;
            m_srcSize.height = srcDim.height;
            m_dstSize.width  = dstDim.width;
            m_dstSize.height = dstDim.height;

            IppiRect  srcRect, dstRect;
            srcRect.x = 0;
            srcRect.y = 0;
            srcRect.width = m_srcSize.width;
            srcRect.height= m_srcSize.height;

            dstRect.x = 0;
            dstRect.y = 0;
            dstRect.width = m_dstSize.width;
            dstRect.height= m_dstSize.height;

            int bufSize = 0;

            ippSts = ippiResizeGetBufSize( srcRect, dstRect, 1, IPPI_INTER_CUBIC, &bufSize );
            if( ippSts != 0)//IppStsErrNone )
            {
                return UMC_ERR_ALLOC;
            }

            m_workBuff.reset(new Ipp8u[bufSize]);
        }

        // AYA: should be implemented to support MSDK_SW_VPP_SA
        // convert the picture
        //ippiResize_8u_C1R(
        //    m_pPic[0], 
        //    srcDim, 
        //    (int) m_picStep * 2,
        //    srcRoi,
        //    (Ipp8u *) m_scaledPic.m_pPic[0],
        //    (int) m_scaledPic.m_picStep, 
        //     dstDim,
        //    ((double) dstDim.width) / ((double) srcDim.width),
        //    ((double) dstDim.height) / ((double) srcDim.height),
        //    IPPI_INTER_CUBIC/*IPPI_INTER_LINEAR*/);

        int interpolation = IPPI_INTER_CUBIC;
        Ipp64f    xFactor = 0.0, yFactor = 0.0;
        Ipp64f    xShift = 0.0, yShift = 0.0;

        xFactor = (Ipp64f)m_dstSize.width  / (Ipp64f)m_srcSize.width;
        yFactor = (Ipp64f)m_dstSize.height / (Ipp64f)m_srcSize.height;

        IppiRect  dstRect;
        dstRect.x = 0;
        dstRect.y = 0;
        dstRect.width = m_dstSize.width;
        dstRect.height= m_dstSize.height;

        ippSts = ippiResizeSqrPixel_8u_C1R(
            m_pPic[0], 
            srcDim, 
            (int) m_picStep * 2, 
            srcRoi,

            (Ipp8u *) m_scaledPic.m_pPic[0], 
            (int) m_scaledPic.m_picStep, 
            dstRect,

            xFactor, 
            yFactor, 
            xShift, 
            yShift, 
            interpolation, 
            m_workBuff.get());

        if( ippSts != 0 )
        {
            return UMC_ERR_ALLOC;
        }
    }

    // save frame structure
    m_frameStructure = interlaceType;
    m_frameEstimation = interlaceType;

    return UMC_OK;

} // Status SceneAnalyzerFrame::SetSource(VideoData *pSrc, InterlaceType interlaceType)

} // namespace UMC
#endif
