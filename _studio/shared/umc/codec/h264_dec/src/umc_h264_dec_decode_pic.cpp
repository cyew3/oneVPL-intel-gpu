/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include <ippi.h>
#include "umc_h264_dec.h"
#include "umc_h264_bitstream.h"

#include "umc_default_frame_allocator.h"

#include "umc_h264_task_supplier.h"
#include "umc_h264_timing.h"

#ifndef UMC_RESTRICTED_CODE_VA
#include "umc_h264_va_supplier.h"
#endif //UMC_RESTRICTED_CODE_VA

#ifndef UMC_RESTRICTED_CODE_RA
#include "umc_h264_ra_supplier.h"
#endif

namespace UMC
{

#ifdef USE_DETAILED_H264_TIMING
    TimingInfo* clsTimingInfo;
#endif

// Create instance of H264 decoder
VideoDecoder *CreateH264Decoder() { return (new H264VideoDecoder()); }

//////////////////////////////////////////////////////////////////////////////
// H264Decoder constructor
//////////////////////////////////////////////////////////////////////////////
H264VideoDecoder::H264VideoDecoder ()
    : m_pFrameAllocator(0)
    , m_IsOwnPostProcessing(false)
    , m_IsInitialized(false)
{
    // allocator tools
    INIT_TIMING;
}

//////////////////////////////////////////////////////////////////////////////
// Start_Sequence: This method should be called if there are significant
//                 changes in the input format in the compressed bit stream.
//                 This method must be called between construction
//                 of the H264Decoder object and the first time a decompress
//                 is done.
//////////////////////////////////////////////////////////////////////////////
Status H264VideoDecoder::Init(BaseCodecParams *pInit)
{
    Status umcRes = UMC_OK;
    VideoDecoderParams *init = DynamicCast<VideoDecoderParams> (pInit);

    // check error(s)
    if (NULL == init)
        return UMC_ERR_NULL_PTR;

    // release object before initialization
    Close();

    // initialize memory control tools
    umcRes = VideoDecoder::Init(pInit);
    if (UMC_OK != umcRes)
        return umcRes;

    m_PostProcessing = init->pPostProcessing;

    m_IsOwnPostProcessing = false;
    if (!m_PostProcessing)
    {
        m_PostProcessing = createVideoProcessing();
        m_IsOwnPostProcessing = true;
        init->pPostProcessing = m_PostProcessing;
    }

#ifndef  UMC_RESTRICTED_CODE_RA
    m_pTaskSupplier.reset(new RATaskSupplier());
#elif !(defined (UMC_RESTRICTED_CODE_VA))
    m_pTaskSupplier.reset(init->pVideoAccelerator ? new VATaskSupplier() : new TaskSupplier());
    static_cast<VATaskSupplier*>(m_pTaskSupplier.get())->SetVideoHardwareAccelerator(init->pVideoAccelerator);
#else
    m_pTaskSupplier.reset(new TaskSupplier());
#endif

    m_pFrameAllocator.reset(new DefaultFrameAllocator);
    m_pTaskSupplier->SetFrameAllocator(m_pFrameAllocator.get());
    m_pTaskSupplier->SetMemoryAllocator(m_pMemoryAllocator);

    try
    {
        m_pTaskSupplier->Init(init);

        if (init->m_pData && init->m_pData->GetDataSize())
        {
            umcRes = m_pTaskSupplier->GetFrame((MediaData *)init->m_pData, 0);
        }
    }
    catch(...)
    {
        return UMC_ERR_FAILED;
    }

    if (UMC_ERR_NOT_ENOUGH_DATA == umcRes || UMC_ERR_NULL_PTR == umcRes)
        umcRes = UMC_OK;

    m_IsInitialized = (umcRes == UMC_OK);

    return umcRes;
}

Status  H264VideoDecoder::SetParams(BaseCodecParams* params)
{
    if (!m_IsInitialized)
        return UMC_ERR_NOT_INITIALIZED;

    VideoDecoderParams *pParams = DynamicCast<VideoDecoderParams>(params);

    if (NULL == pParams)
        return UMC_ERR_NULL_PTR;

    m_pTaskSupplier->SetParams(pParams);

    return VideoDecoder::SetParams(params);
}

Status H264VideoDecoder::Close()
{
    // release memory control tools
    try
    {
        m_pTaskSupplier.reset(0);
        m_pFrameAllocator.reset(0);

        if (m_IsOwnPostProcessing)
        {
            delete m_PostProcessing;
            m_PostProcessing = 0;
            m_IsOwnPostProcessing = false;
        }

        VideoDecoder::Close();
    }catch(...)
    {
    }

    m_IsInitialized = false;

    return UMC_OK;
}

//////////////////////////////////////////////////////////////////////////////
// H264Decoder Destructor
//////////////////////////////////////////////////////////////////////////////
H264VideoDecoder::~H264VideoDecoder(void)
{
    Close();
}

#ifndef UMC_RESTRICTED_CODE_RA
Status H264VideoDecoder::GetRandomFrame(MediaData *pSource, MediaData *pDst, ReferenceList* reflist)
{
    return ((RATaskSupplier*)m_pTaskSupplier)->GetRandomFrame(pSource, pDst, reflist);
}
#endif // UMC_RESTRICTED_CODE_RA

Status H264VideoDecoder::GetFrame(MediaData* src, MediaData* dst)
{
    Status umcRes = UMC_OK;

    try
    {
        if (!m_IsInitialized)
            return UMC_ERR_NOT_INITIALIZED;

        if (!dst)
            return UMC_ERR_NULL_PTR;

        dst->SetDataSize(0);

        // initialize bit stream data
        if (src && MINIMAL_DATA_SIZE > src->GetDataSize())
        {
            src->MoveDataPointer((Ipp32s) src->GetDataSize());
            return UMC_ERR_NOT_ENOUGH_DATA;
        }

        if (!src)
        {
            return GetFrameInternal(src, dst);

            // disable all errors - error's silent mode
            //if (UMC_OK != umcRes && UMC_ERR_NOT_ENOUGH_DATA != umcRes && UMC_ERR_NOT_ENOUGH_BUFFER != umcRes)
              //  return umcRes;
        }

        src->SetFlags(MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME);

        umcRes = GetFrameInternal(src, dst);
        return umcRes;
    } catch(const h264_exception & ex)
    {
        m_pTaskSupplier->AfterErrorRestore();
        return ex.GetStatus();
    }
    catch(...)
    {
        m_pTaskSupplier->AfterErrorRestore();
        return UMC_ERR_INVALID_STREAM;
    }

} // Status H264VideoDecoder::GetFrame(MediaData* src, MediaData* dst)

Status H264VideoDecoder::GetFrameInternal(MediaData* src, MediaData* dst)
{
    VM_ASSERT(dst);

    bool force = (src == 0);

    Status umcRes;
    do
    {
        umcRes = m_pTaskSupplier->GetFrame(src, dst);

        if (umcRes == UMC_ERR_NOT_ENOUGH_BUFFER)
        {
            umcRes = UMC_ERR_NOT_ENOUGH_DATA;
            force = true;
        }

        if (UMC_WRN_INVALID_STREAM == umcRes)
        {
            umcRes = UMC_ERR_NOT_ENOUGH_DATA;
        }

        if (UMC_OK != umcRes && UMC_ERR_NOT_ENOUGH_DATA != umcRes)
        {
            return umcRes;
        }

        if (dst->GetDataSize())
            return UMC_OK;

        umcRes = m_pTaskSupplier->GetFrameToDisplay(dst, force) ? UMC_OK : UMC_ERR_NOT_ENOUGH_DATA;

    } while ((UMC_ERR_NOT_ENOUGH_DATA == umcRes) &&
            src &&
            (MINIMAL_DATA_SIZE < src->GetDataSize()));

    return umcRes;
}

Status H264VideoDecoder::GetInfo(BaseCodecParams* params)
{
    Status umcRes = UMC_OK;
    VideoDecoderParams *lpInfo = DynamicCast<VideoDecoderParams> (params);

    if (!m_IsInitialized)
    {
        if (params->m_pData)
        {
            umcRes = VideoDecoder::Init(params);
            if (UMC_OK != umcRes)
                return umcRes;

            m_pTaskSupplier.reset(new TaskSupplier());
            m_pTaskSupplier->SetMemoryAllocator(m_pMemoryAllocator);
            m_pTaskSupplier->PreInit(params);

            if (lpInfo->m_pData && lpInfo->m_pData->GetDataSize())
            {
                umcRes = m_pTaskSupplier->GetFrame((MediaData *) lpInfo->m_pData, 0);
            }

            umcRes = m_pTaskSupplier->GetInfo(lpInfo);
            Close();
            return umcRes;
        }

        return UMC_ERR_NOT_INITIALIZED;
    }

    if (NULL == lpInfo)
        return UMC_ERR_NULL_PTR;

    umcRes = VideoDecoder::GetInfo(params);

    if (UMC_OK == umcRes)
    {
        return m_pTaskSupplier->GetInfo(lpInfo);
    }

    return umcRes;
}

Status  H264VideoDecoder::Reset()
{
    Status umcRes = UMC_OK;

    if (!m_IsInitialized)
        return UMC_ERR_NOT_INITIALIZED;

    m_pTaskSupplier->Reset();

    return umcRes;
}

Status  H264VideoDecoder::ChangeVideoDecodingSpeed(Ipp32s& num)
{
    if (!m_IsInitialized)
        return UMC_ERR_NOT_INITIALIZED;

    m_pTaskSupplier->ChangeVideoDecodingSpeed(num);
    return UMC_OK;
}

H264VideoDecoder::SkipInfo H264VideoDecoder::GetSkipInfo() const
{
    if (!m_IsInitialized)
    {
        H264VideoDecoder::SkipInfo defSkip = {false, 0};

        return defSkip;
    }

    return m_pTaskSupplier->GetSkipInfo();
}

Status H264VideoDecoder::GetUserData(MediaData * pUD)
{
    if (!m_IsInitialized)
        return UMC_ERR_NOT_INITIALIZED;

    if (!pUD)
        return UMC_ERR_NULL_PTR;

    return m_pTaskSupplier->GetUserData(pUD);
}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
