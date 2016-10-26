//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_FILE_WRITER)

#include "fw_video_render.h"
#include "umc_color_space_conversion.h"
#include "umc_video_data.h"
#include "umc_file_writer.h"
#include "drv.h"
#include "vm_time.h"

namespace
{

vm_char DEFAULT_FILE_NAME[] = VM_STRING("output.yuv");

enum
{
    NUMBER_OF_BUFFERS           = 4
};

void ResetMediaBufferParams(UMC::MediaBufferParams &Param)
{
    Param.m_numberOfFrames = 0;
    Param.m_prefInputBufferSize = 0;
    Param.m_prefOutputBufferSize = 0;

} // void ResetMediaBufferParams(UMC::MediaBufferParams &Param)

} // namespace

namespace UMC
{

// -------------------------------------------------------------------------

#define MODULE          "<Undefined Module>"
#define FUNCTION        "<Undefined Function>"
#define DBG_SET(msg)    VIDEO_DRV_DBG_SET   (MODULE, FUNCTION, msg)

// -------------------------------------------------------------------------

#undef  MODULE
#define MODULE  "FWVideoRenderParams:"

FWVideoRenderParams::FWVideoRenderParams(void)
{
#undef  FUNCTION
#define FUNCTION    "FWVideoRenderParams"
    DBG_SET("+");

    pOutFile = NULL;

    DBG_SET("-");

} // FWVideoRenderParams::FWVideoRenderParams(void)

#undef  MODULE
#define MODULE  "FWVideoRender:"

FWVideoRender::FWVideoRender(void)
{
#undef  FUNCTION
#define FUNCTION    "FWVideoRender"
    DBG_SET("+");

    m_bCriticalError = false;
    m_bStop = false;
    m_pFileWriter = NULL;

    m_pConverter = NULL;

    DBG_SET("-");

} // FWVideoRender::FWVideoRender(void)

FWVideoRender::~FWVideoRender(void)
{
#undef  FUNCTION
#define FUNCTION    "~FWVideoRender"
    DBG_SET("+");

    Close();

    DBG_SET("-");

} // FWVideoRender::~FWVideoRender(void)

Status FWVideoRender::Init(MediaReceiverParams *pInit)
{
#undef  FUNCTION
#define FUNCTION    "Init"
    DBG_SET("+");

    Status umcRes = UMC_OK;
    FileWriterParams InitParams;
    VideoRenderParams *pParams = DynamicCast<VideoRenderParams> (pInit);

    // check error(s)
    if (NULL == pParams)
    {
        DBG_SET("Null pointer passed as parameter");
        return UMC_ERR_NULL_PTR;
    }

    // release object before initialization
    Close();

    // call parent's method
    umcRes = VideoRender::Init(pInit);
    if (UMC_OK != umcRes)
        return umcRes;

    // save initialization parameters
    m_RenderParam = *pParams;

    // try to set user-defined file name first
    {
        FWVideoRenderParams *pFWParams = DynamicCast<FWVideoRenderParams> (pParams);
        FileWriterParams writerParam;
        vm_char *pFile;

        // internal FW Video Render error checking. Has no effect.
        VM_ASSERT(MAXIMUM_PATH > vm_string_strlen(DEFAULT_FILE_NAME));

        // there is no user defined filename
        if ((NULL == pFWParams) ||
            (NULL == pFWParams->pOutFile) ||
            (MAXIMUM_PATH <= vm_string_strlen(pFWParams->pOutFile)))
            pFile = DEFAULT_FILE_NAME;
        else
            pFile = pFWParams->pOutFile;

        vm_string_strcpy(writerParam.m_file_name, pFile);
        writerParam.m_portion_size = 0;

        m_pFileWriter = new FileWriter();
        if (NULL == m_pFileWriter)
        {
            DBG_SET("File writer wasn't created");
            return UMC_ERR_ALLOC;
        }
        umcRes = m_pFileWriter->Init(&writerParam);
        if (UMC_OK != umcRes)
        {
            DBG_SET("File writer failed initialization");
            return umcRes;
        }
    }

    // allocate internal buffer
    if (NONE != m_RenderParam.out_data_template.GetColorFormat())
    {
        VideoData videoData;

        videoData.Init(m_RenderParam.out_data_template.GetWidth(),
                       m_RenderParam.out_data_template.GetHeight(),
                       m_RenderParam.out_data_template.GetColorFormat());

        umcRes = AllocateInternalBuffer(&videoData);
        if (UMC_OK != umcRes)
        {
            DBG_SET("Internal buffers weren't allocated");
            return umcRes;
        }
    }
    else
    {
        BaseCodecParams params;

        // allocate and ...
        m_pConverter = new ColorSpaceConversion();
        if (NULL == m_pConverter)
        {
            DBG_SET("Can't allocate color space converter");
            return umcRes;
        }

        // ... initialize the color converter
        umcRes = m_pConverter->Init(&params);
        if (UMC_OK != umcRes)
        {
            DBG_SET("Can't initialize the color converter");
            return umcRes;
        }
    }

    DBG_SET("-");
    return umcRes;

} // Status FWVideoRender::Init(MediaReceiverParams *pInit)

Status FWVideoRender::Close(void)
{
#undef  FUNCTION
#define FUNCTION    "Close"
    DBG_SET("+");

    m_bStop = true;

    // delete file writer
    if (m_pFileWriter)
        delete m_pFileWriter;

    // delete color converter
    if (m_pConverter)
        delete m_pConverter;

    // delete internal buffers
    ResetMediaBufferParams(m_BufferParam);
    m_Frames.Close();

    // reset variables
    m_bCriticalError = false;
    m_bStop = false;
    m_pFileWriter = NULL;

    m_pConverter = NULL;

    // call parent's method
    VideoRender::Close();

    DBG_SET("-");
    return UMC_OK;

} // Status FWVideoRender::Close(void)

Status FWVideoRender::AllocateInternalBuffer(VideoData *pData)
{
    Status umcRes = UMC_OK;

    // initialize only when it is required
    if (0 == m_BufferParam.m_prefInputBufferSize)
    {
        VideoData minVideoData;

        // set minimal allowed alignment
        minVideoData.SetAlignment(1);
        if (UMC_OK != umcRes)
            return umcRes;
        // all planes shall have equal bitdepth value
        umcRes = minVideoData.Init(pData->GetWidth(),
                                   pData->GetHeight(),
                                   pData->GetNumPlanes(),
                                   pData->GetPlaneBitDepth(0));
        if (UMC_OK != umcRes)
            return umcRes;
        // set used color format
        minVideoData.SetColorFormat(pData->GetColorFormat());
        if (UMC_OK != umcRes)
            return umcRes;

        // set buffer parameters
        m_BufferParam.m_numberOfFrames = NUMBER_OF_BUFFERS;
        m_BufferParam.m_prefInputBufferSize =
        m_BufferParam.m_prefOutputBufferSize = minVideoData.GetMappingSize();

        // initialize buffer
        umcRes = m_Frames.Init(&m_BufferParam);
    }

    return umcRes;

} // Status FWVideoRender::AllocateInternalBuffer(VideoData *pData)

Status FWVideoRender::LockInputBuffer(MediaData *in)
{
#undef  FUNCTION
#define FUNCTION    "LockInputBuffer"
    DBG_SET("+");

    Status umcRes;
    VideoData *pSrc = DynamicCast<VideoData> (in);

    // check internal status
    if (m_bCriticalError)
        return UMC_ERR_FAILED;

    // check working algorithm
    if (NONE == m_RenderParam.out_data_template.GetColorFormat())
    {
        // we don't need to return any data pointers
        // set to zero just for sure, that memory will not be read
        pSrc->SetBufferPointer(NULL, 0);
        pSrc->SetDataSize(0);

        DBG_SET("-");
        return UMC_OK;
    }

    // initialize video data before obtaining the memory
    umcRes = pSrc->SetAlignment(1);
    if (UMC_OK != umcRes)
    {
        DBG_SET("-");
        return umcRes;
    }
    umcRes = pSrc->Init(m_RenderParam.out_data_template.GetWidth(),
                        m_RenderParam.out_data_template.GetHeight(),
                        m_RenderParam.out_data_template.GetColorFormat());
    if (UMC_OK != umcRes)
    {
        DBG_SET("-");
        return umcRes;
    }

    // get data pointer
    umcRes = m_Frames.LockInputBuffer(in);

    DBG_SET("-");
    return umcRes;

} // Status FWVideoRender::LockInputBuffer(MediaData *in)

Status FWVideoRender::UnLockInputBuffer(MediaData *in, Status StreamStatus)
{
#undef  FUNCTION
#define FUNCTION    "UnLockInputBuffer"
    DBG_SET("+");

    Status umcRes;
    VideoData *pSrc = DynamicCast<VideoData> (in);

    // the render doesn't accept zero-length frames
    if ((pSrc) && (0 == pSrc->GetDataSize()))
    {
        return UMC_OK;
    }

    // we need to recalculate buffer's parameters
    // and copy frame into buffer
    if (NONE == m_RenderParam.out_data_template.GetColorFormat())
    {
        umcRes = AllocateInternalBuffer(pSrc);
        if (UMC_OK != umcRes)
        {
            DBG_SET("Internal buffers weren't allocated");
            return umcRes;
        }

        // copy frame into frame buffer
        {
            VideoData dst;

            // set minimal allowed alignment
            dst.SetAlignment(1);
            if (UMC_OK != umcRes)
                return umcRes;
            // all planes shall have equal bitdepth value
            umcRes = dst.Init(pSrc->GetWidth(),
                              pSrc->GetHeight(),
                              pSrc->GetNumPlanes(),
                              pSrc->GetPlaneBitDepth(0));
            if (UMC_OK != umcRes)
                return umcRes;

            // set used color format
            dst.SetColorFormat(pSrc->GetColorFormat());
            if (UMC_OK != umcRes)
                return umcRes;

            // get data pointer
            do
            {
                umcRes = m_Frames.LockInputBuffer(&dst);

            } while ((UMC_ERR_NOT_ENOUGH_BUFFER == umcRes) &&
                     (false == m_bStop));
            if (UMC_OK != umcRes)
                return umcRes;

            // copy data
            m_pConverter->GetFrame(pSrc, &dst);
            dst.SetDataSize(dst.GetMappingSize());
            dst.SetTime(pSrc->GetTime());

            // unlock written memory
            umcRes = m_Frames.UnLockInputBuffer(&dst, StreamStatus);
        }
    }
    else
    {
        // just unlock
        umcRes = m_Frames.UnLockInputBuffer(in, StreamStatus);
    }

    DBG_SET("-");
    return umcRes;

} // Status FWVideoRender::UnLockInputBuffer(MediaData *in, Status StreamStatus)

Status FWVideoRender::Stop(void)
{
#undef  FUNCTION
#define FUNCTION    "Stop"
    DBG_SET("+");

    m_bStop = true;

    DBG_SET("-");
    return UMC_OK;

} // Status FWVideoRender::Stop(void)

Status FWVideoRender::Reset(void)
{
#undef  FUNCTION
#define FUNCTION    "Reset"
    DBG_SET("+");

    m_Frames.Reset();

    DBG_SET("-");
    return UMC_OK;

} // Status FWVideoRender::Reset(void)

Ipp64f FWVideoRender::GetRenderFrame(void)
{
#undef  FUNCTION
#define FUNCTION    "GetRenderFrame"
    DBG_SET("+");

    MediaData mediaTime;
    Status umcRes;
    Ipp64f dTime = -1.0;

    // get latest frame time stamp
    do
    {

        umcRes = m_Frames.LockOutputBuffer(&mediaTime);
        if (UMC_ERR_NOT_ENOUGH_DATA == umcRes)
            vm_time_sleep(1);

    } while ((false == m_bStop) &&
             (UMC_ERR_NOT_ENOUGH_DATA == umcRes));

    if (UMC_OK == umcRes)
    {
        dTime = mediaTime.GetTime();
        // call UnLock to handle EOS gently
        m_Frames.UnLockOutputBuffer(&mediaTime);
    }

    DBG_SET("-");
    return dTime;

} // Ipp64f FWVideoRender::GetRenderFrame(void)

Status FWVideoRender::GetRenderFrame(Ipp64f *pTime)
{
#undef  FUNCTION
#define FUNCTION    "GetRenderFrame"
    DBG_SET("+");

    MediaData mediaTime;
    Status umcRes;
    Ipp64f dTime = -1.0;

    // check error(s)
    if (NULL == pTime)
        return UMC_ERR_NULL_PTR;

    // get latest frame time stamp
    umcRes = m_Frames.LockOutputBuffer(&mediaTime);
    if (UMC_OK == umcRes)
    {
        dTime = mediaTime.GetTime();
        // call UnLock to handle EOS gently
        m_Frames.UnLockOutputBuffer(&mediaTime);
    }
    else if (UMC_ERR_NOT_ENOUGH_DATA == umcRes)
    {
        umcRes = m_bStop ? UMC_ERR_END_OF_STREAM : UMC_ERR_TIMEOUT;
    }

    // copy time
    *pTime = dTime;

    DBG_SET("-");
    return umcRes;

} // Status FWVideoRender::GetRenderFrame(Ipp64f *pTime)

Status FWVideoRender::RenderFrame(void)
{
#undef  FUNCTION
#define FUNCTION    "RenderFrame"
    DBG_SET("+");

    MediaData mediaSrc;
    Status umcRes;

    // obtain buffer to  write
    umcRes = m_Frames.LockOutputBuffer(&mediaSrc);
    if (UMC_OK == umcRes)
    {
        // write data to the file
        Ipp32s iSize = (Ipp32s)mediaSrc.GetDataSize();
        umcRes = m_pFileWriter->PutData(mediaSrc.GetDataPointer(), iSize);
        if (UMC_OK != umcRes)
        {
            m_bCriticalError = true;
            DBG_SET("-");
            return UMC_ERR_FAILED;
        }

        // skip written data
        mediaSrc.MoveDataPointer((Ipp32s)mediaSrc.GetDataSize());
        m_Frames.UnLockOutputBuffer(&mediaSrc);
    }

    DBG_SET("-");
    return UMC_OK;

} // Status FWVideoRender::RenderFrame(void)

} // namespace UMC

#endif // UMC_ENABLE_FILE_WRITER
