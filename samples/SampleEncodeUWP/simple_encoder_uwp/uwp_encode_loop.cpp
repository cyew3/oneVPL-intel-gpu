/******************************************************************************\
Copyright (c) 2005-2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/
#include "pch.h"

#include "uwp_encode_loop.h"
#include "sysmem_uwp_allocator.h"

mfxU16 FourCCToChroma(mfxU32 fourCC)
{
    switch (fourCC)
    {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
        return MFX_CHROMAFORMAT_YUV420;
    case MFX_FOURCC_NV16:
    case MFX_FOURCC_P210:
#ifdef ENABLE_PS
    case MFX_FOURCC_Y210:
#endif
    case MFX_FOURCC_YUY2:
        return MFX_CHROMAFORMAT_YUV422;
    case MFX_FOURCC_RGB4:
        return MFX_CHROMAFORMAT_YUV444;
    }

    return MFX_CHROMAFORMAT_YUV420;
}


mfxU32 FourCCToBPP(mfxU32 fourCC)
{
    switch (fourCC)
    {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
        return 12;
    case MFX_FOURCC_NV16:
    case MFX_FOURCC_P210:
#ifdef ENABLE_PS
    case MFX_FOURCC_Y210:
#endif
    case MFX_FOURCC_YUY2:
        return 16;
    case MFX_FOURCC_RGB4:
        return 32;
    }

    return 0;
}

mfxStatus CEncoder::SetEncParams(mfxVideoParam& pInParams)
{
    m_mfxEncParams.mfx.CodecId = pInParams.mfx.CodecId;
    m_mfxEncParams.mfx.TargetUsage = pInParams.mfx.TargetUsage; // trade-off between quality and speed
    m_mfxEncParams.mfx.RateControlMethod = pInParams.mfx.RateControlMethod;
    m_mfxEncParams.mfx.GopRefDist = pInParams.mfx.GopRefDist;
    m_mfxEncParams.mfx.GopPicSize = pInParams.mfx.GopPicSize;
    m_mfxEncParams.mfx.NumRefFrame = pInParams.mfx.NumRefFrame;
    m_mfxEncParams.mfx.IdrInterval = pInParams.mfx.IdrInterval;
    m_mfxEncParams.mfx.CodecProfile = pInParams.mfx.CodecProfile;
    m_mfxEncParams.mfx.CodecLevel = pInParams.mfx.CodecLevel;
    m_mfxEncParams.mfx.MaxKbps = pInParams.mfx.MaxKbps;
    m_mfxEncParams.mfx.InitialDelayInKB = pInParams.mfx.InitialDelayInKB;
    m_mfxEncParams.mfx.GopOptFlag = pInParams.mfx.GopOptFlag;
    m_mfxEncParams.mfx.BufferSizeInKB = pInParams.mfx.BufferSizeInKB;
    m_mfxEncParams.mfx.TargetKbps = pInParams.mfx.TargetKbps;
    m_mfxEncParams.mfx.NumSlice = pInParams.mfx.NumSlice;
    m_mfxEncParams.mfx.FrameInfo.FrameRateExtN = pInParams.mfx.FrameInfo.FrameRateExtN;
    m_mfxEncParams.mfx.FrameInfo.FrameRateExtD = pInParams.mfx.FrameInfo.FrameRateExtD;
    m_mfxEncParams.mfx.EncodedOrder = 0; // binary flag, 0 signals encoder to take frames in display order

    m_mfxEncParams.IOPattern = pInParams.IOPattern;

    // frame info parameters
    m_mfxEncParams.mfx.FrameInfo.FourCC = pInParams.mfx.FrameInfo.FourCC;
    m_mfxEncParams.mfx.FrameInfo.ChromaFormat = FourCCToChroma(pInParams.mfx.FrameInfo.FourCC);
    m_mfxEncParams.mfx.FrameInfo.PicStruct = pInParams.mfx.FrameInfo.PicStruct;
    m_mfxEncParams.mfx.FrameInfo.Shift = 0;

    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxEncParams.mfx.FrameInfo.Width = MSDK_ALIGN16(pInParams.mfx.FrameInfo.Width);
    m_mfxEncParams.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxEncParams.mfx.FrameInfo.PicStruct) ?
        MSDK_ALIGN16(pInParams.mfx.FrameInfo.Height) : MSDK_ALIGN32(pInParams.mfx.FrameInfo.Height);

    m_mfxEncParams.mfx.FrameInfo.CropX = 0;
    m_mfxEncParams.mfx.FrameInfo.CropY = 0;
    m_mfxEncParams.mfx.FrameInfo.CropW = pInParams.mfx.FrameInfo.CropW;
    m_mfxEncParams.mfx.FrameInfo.CropH = pInParams.mfx.FrameInfo.CropH;

    if (!m_EncExtParams.empty())
    {
        m_mfxEncParams.ExtParam = &m_EncExtParams[0]; // vector is stored linearly in memory
        m_mfxEncParams.NumExtParam = (mfxU16)m_EncExtParams.size();
    }

    m_mfxEncParams.AsyncDepth = pInParams.AsyncDepth;

    return MFX_ERR_NONE;
}


mfxStatus CEncoder::CreateAllocator()
{
    mfxStatus sts = MFX_ERR_NONE;

    // create system memory allocator
    m_pMFXAllocator = std::make_unique<SysMemFrameAllocator>();
    MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

    // initialize memory allocator
    sts = m_pMFXAllocator->Init(m_pmfxAllocatorParams.get());

    return sts;
}

void CEncoder::DeleteFrames()
{
    // delete surfaces array
    m_nEncSurfaces = 0; 
    m_pEncSurfaces.reset();

    // delete frames
    if (m_pMFXAllocator)
    {
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_EncResponse);
    }
}

void CEncoder::DeleteAllocator()
{
    // delete allocator
    m_pMFXAllocator.reset();
    m_pmfxAllocatorParams.reset();
}

CEncoder::CEncoder():
    m_nEncSurfaces(0)
    , m_nFramesRead(0)
    , m_mfxBS({ 0 })
    , m_mfxEncParams({ 0 })
    , m_EncResponse({ 0 })
    , m_encCtrl({ 0 })
    , _CTS(cancellation_token_source())
{
} 

CEncoder::~CEncoder()
{
    Close();
}

std::shared_ptr<CEncoder> CEncoder::Instantiate(mfxVideoParam &pParams)
{
    std::shared_ptr<CEncoder> enc = std::make_shared<CEncoder>();

    mfxStatus sts = MFX_ERR_NONE;

    mfxInitParam initPar = { 0 };

    // minimum API version with which library is initialized
    // set to 1.0 and later we will query actual version of 
    // the library which will got leaded
    mfxVersion version = { {20, 1} };
    initPar.Version = version;
    // any implementation of library suites our needs
    initPar.Implementation = MFX_IMPL_AUTO_ANY;

    // Library should pick first available compatible adapter during InitEx call with MFX_IMPL_HARDWARE_ANY
    sts = enc->m_mfxSession.InitEx(initPar);

    if (sts >= MFX_ERR_NONE)
        sts = MFXQueryVersion(enc->m_mfxSession, &version); // get real API version of the loaded library

    if (sts >= MFX_ERR_NONE)
        enc->m_pmfxENC = std::make_unique<MFXVideoENCODE>(enc->m_mfxSession);

    if (sts >= MFX_ERR_NONE && !enc->m_pmfxENC)
        sts = MFX_ERR_MEMORY_ALLOC;
    
    if (sts >= MFX_ERR_NONE)   
        sts = enc->CreateAllocator();

    if (sts >= MFX_ERR_NONE)
        sts = enc->SetEncParams(pParams);

    if (sts >= MFX_ERR_NONE)
        sts = enc->AllocEncoderInputFrames();

    if (sts >= MFX_ERR_NONE)
        sts = enc->AllocEncoderOutputBitream();

    if (sts < MFX_ERR_NONE) {
        enc.reset();
    }

    return enc;
}

void CEncoder::Close()
{
    DeleteFrames();

    m_pmfxENC.reset();
    m_mfxSession.Close();

    // allocator if used as external for MediaSDK must be deleted after SDK components
    DeleteAllocator();
}

mfxStatus CEncoder::AllocEncoderInputFrames()
{
    //MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest EncRequest = { 0 };

    // Querying encoder
    sts = m_pmfxENC->Query(&m_mfxEncParams, &m_mfxEncParams);
    MSDK_CHECK_STATUS(sts, "Query (for encoder) failed");

    // Calculate the number of surfaces for components.
    // QueryIOSurf functions tell how many surfaces are required to produce at least 1 output.
    // To achieve better performance we provide extra surfaces.
    // 1 extra surface at input allows to get 1 extra output.
    sts = m_pmfxENC->QueryIOSurf(&m_mfxEncParams, &EncRequest);
    MSDK_CHECK_STATUS(sts, "QueryIOSurf (for encoder) failed");

    m_mfxEncParams.AsyncDepth = EncRequest.NumFrameMin;

    if (EncRequest.NumFrameSuggested < m_mfxEncParams.AsyncDepth)
        return MFX_ERR_MEMORY_ALLOC;

    // correct initially requested and prepare allocation request
    m_mfxEncParams.mfx.FrameInfo = EncRequest.Info;

    // alloc frames for encoder
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &EncRequest, &m_EncResponse);
    MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Alloc failed");

    // prepare mfxFrameSurface1 array for encoder
    m_pEncSurfaces = std::make_unique<mfxFrameSurface1[]>(m_EncResponse.NumFrameActual);
    m_nEncSurfaces = m_EncResponse.NumFrameActual;
    MSDK_CHECK_POINTER(m_pEncSurfaces, MFX_ERR_MEMORY_ALLOC);

    for (int i = 0; i < m_EncResponse.NumFrameActual; i++)
    {
        m_pEncSurfaces[i] = mfxFrameSurface1({ 0 });
        m_pEncSurfaces[i].Info = m_mfxEncParams.mfx.FrameInfo;

        // get YUV pointers
        sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_EncResponse.mids[i], &(m_pEncSurfaces[i].Data));
        MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Lock failed");
    }

    sts = m_pmfxENC->Init(&m_mfxEncParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);

    return sts;
}

mfxU32 CEncoder::GetPictureSize() 
{
    return m_mfxEncParams.mfx.FrameInfo.Height * m_mfxEncParams.mfx.FrameInfo.Width * FourCCToBPP(m_mfxEncParams.mfx.FrameInfo.FourCC) / 8;
}

void CEncoder::CancelEncoding()
{
    _CTS.cancel();
    _CTS = cancellation_token_source();
}

IAsyncActionWithProgress<double>^
CEncoder::ReadAndEncodeAsync(StorageFile ^ storageSource, StorageFile ^ storageSink)
{
    using namespace std;

    return create_async([=](progress_reporter<double> reporter) {

        return create_task(storageSource->OpenAsync(FileAccessMode::Read), _CTS.get_token())
            .then([=](IRandomAccessStream^ istr) {

            auto streamReader = ref new DataReader(istr);

            return create_task(streamReader->LoadAsync(mfxU32(istr->Size)), _CTS.get_token())
                .then([=](mfxU32 bytes) {

                const mfxU32 picSize = GetPictureSize();

                auto bufferData = ref new Platform::Array<byte>(picSize);

                StorageStreamTransaction^ transaction = create_task(storageSink->OpenTransactedWriteAsync(), _CTS.get_token()).get();

                mfxStatus sts = MFX_ERR_NONE;
                auto UnconsumedBufferLength = streamReader->UnconsumedBufferLength;
                auto dataWriter = ref new DataWriter(transaction->Stream);

                while (((UnconsumedBufferLength >= picSize) && (MFX_ERR_NONE == sts)))
                {
                    if (_CTS.get_token().is_canceled()) {

                        cancel_current_task();
                    }

                    mfxSyncPoint EncSyncP = nullptr;

                    streamReader->ReadBytes(bufferData);
                    UnconsumedBufferLength = streamReader->UnconsumedBufferLength;

                    sts = ProceedFrame(bufferData->begin(), bufferData->Length, EncSyncP);

                    if (EncSyncP) {

                        sts = m_mfxSession.SyncOperation(EncSyncP, MSDK_WAIT_INTERVAL);

                        if (MFX_ERR_NONE == sts) {

                            dataWriter->WriteBytes(ref new Platform::Array<byte>(m_mfxBS.Data + m_mfxBS.DataOffset, m_mfxBS.DataLength));

                            m_mfxBS.DataOffset = m_mfxBS.DataLength = 0;
                            reporter.report(mfxU32(100 - double(UnconsumedBufferLength * 100) / (istr->Size)));

                            if (create_task(transaction->Stream->FlushAsync(), _CTS.get_token()).get()) {
                                mfxU32 bytesStored = create_task(dataWriter->StoreAsync(), _CTS.get_token()).get();
                            }
                        }
                    }
                }

                transaction->CommitAsync();
            }, _CTS.get_token());

            //
            // please note that continuation context should not be the same as UI thread, that is why use_arbitrary() is used
            // otherwise it will not be possible to wait for asynchronous operations completing by calling get() or wait()
            //
        }, _CTS.get_token(), task_continuation_context::use_arbitrary()).then([](task<void> tsk) {
            tsk.get();
        }, _CTS.get_token());

    });
}

void WipeMfxBitstream(mfxBitstream& pBitstream)
{
    MSDK_SAFE_DELETE_ARRAY(pBitstream.Data);
}

mfxStatus ExtendMfxBitstream(mfxBitstream& pBitstream, mfxU32 nSize)
{
    if (nSize <= pBitstream.MaxLength)
        nSize = std::max<mfxU32>(pBitstream.MaxLength * 2, nSize);

    mfxU8* pData = new (std::nothrow) mfxU8[nSize];
    MSDK_CHECK_POINTER(pData, MFX_ERR_MEMORY_ALLOC);

    if (pBitstream.Data) {
        memmove(pData, pBitstream.Data + pBitstream.DataOffset, pBitstream.DataLength);
    }
    
    WipeMfxBitstream(pBitstream);

    pBitstream.Data = pData;
    pBitstream.DataOffset = 0;
    pBitstream.MaxLength = nSize;

    return MFX_ERR_NONE;
}

mfxStatus CEncoder::AllocEncoderOutputBitream()
{
    mfxVideoParam par = { 0 };

    // find out the required buffer size
    mfxStatus sts = m_pmfxENC->GetVideoParam(&par);
    MSDK_CHECK_STATUS(sts, "GetFirstEncoder failed");

    // reallocate bigger buffer for output
    sts = ExtendMfxBitstream(m_mfxBS, par.mfx.BufferSizeInKB * 1000);

    return sts;
}

mfxStatus CEncoder::ProceedFrame(const mfxU8* bufferSrc, mfxU32 buffersize, mfxSyncPoint& EncSyncP)
{
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    static mfxU16 nEncSurfIdx = 0;     // index of free surface for encoder input (vpp output)
    mfxU32 nFramesProcessed = 0;

    sts = MFX_ERR_NONE;
    MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);

    mfxFrameInfo& pInfo = m_pEncSurfaces[nEncSurfIdx].Info;
    mfxFrameData& pData = m_pEncSurfaces[nEncSurfIdx].Data;

    mfxU32 w = 0;
    mfxU32 h = 0;
    mfxU32 pitch = 0;

    // read luma plane
    if (pInfo.CropH > 0 && pInfo.CropW > 0) {
        w = pInfo.CropW;
        h = pInfo.CropH;
    }
    else {
        w = pInfo.Width;
        h = pInfo.Height;
    }

    pitch = pData.Pitch;

    auto src = bufferSrc;
    auto dst = pData.Y + pInfo.CropX + pInfo.CropY * pData.Pitch;    

    // load Y
    for (mfxU32 Yh = 0; Yh < h; Yh++) {
        memcpy_s(dst, w, src, w);
        dst += pitch;
        src += w;
    }

    // read crmoma planes
    w /= 2;
    h /= 2;

    dst = pData.UV + pInfo.CropX + (pInfo.CropY / 2) * pitch;

    auto srcU = src;
    auto srcV = srcU + h * w;

    // load UV interlieved bcos encoder understands NV12
    for (mfxU32 UVh = 0; UVh < h; UVh++) {

        for (mfxU32 UVw = 0; UVw < w; UVw++) {
            dst[UVw*2] = srcU[UVw];
            dst[UVw*2+1] = srcV[UVw];
        }

        dst += pitch;

        srcU += w;
        srcV += w;
    }

    // at this point surface for encoder contains either a frame from file or a frame processed by vpp
    sts = m_pmfxENC->EncodeFrameAsync(&m_encCtrl, &m_pEncSurfaces[nEncSurfIdx], &m_mfxBS, &EncSyncP);

    nEncSurfIdx = (nEncSurfIdx + 1) % m_nEncSurfaces;

    if ((!EncSyncP) && (MFX_ERR_NONE != sts)) {

        // repeat the call if warning and no output
        switch (sts) {
            case MFX_WRN_DEVICE_BUSY:
                Sleep(1); // wait if device is busy
                break;

            case MFX_ERR_NOT_ENOUGH_BUFFER:
                sts = AllocEncoderOutputBitream();
                MSDK_CHECK_STATUS(sts, "AllocEncoderOutputBitream failed");
                break;

            case MFX_ERR_MORE_DATA:
                // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
                // indicates that there are no more buffered frames         
                sts = MFX_ERR_NONE;
                break;
        }
    }

    nFramesProcessed++;

    return sts;
}

