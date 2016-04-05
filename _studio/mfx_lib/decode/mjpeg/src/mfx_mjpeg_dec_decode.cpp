/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2016 Intel Corporation. All Rights Reserved.
//
*/

#include <limits>
#include "mfx_mjpeg_dec_decode.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)

#include "mfx_mjpeg_task.h"

#if defined(_WIN32) || defined(_WIN64)
#include <atlbase.h>
#endif

#include "mfx_common.h"
#include "mfx_common_decode_int.h"

#include "umc_mjpeg_mfx_decode.h"
#include "umc_data_pointers_copy.h"
#include <umc_automatic_mutex.h>

// debug
#include "mfx_enc_common.h"

#include "vm_sys_info.h"
#include "ippcore.h"

#include "mfx_thread_task.h"

#include "umc_jpeg_frame_constructor.h"


#include "umc_mjpeg_mfx_decode_hw.h"
#if defined (MFX_VA_WIN)
#include "libmfx_core_d3d9.h"
#include "umc_va_dxva2_protected.h"
#endif

// Declare skipping constants
enum
{
    JPEG_MIN_SKIP_RATE = 0,
    JPEG_MAX_SKIP_RATE = 9,

    JPEG_SKIP_BOUND = JPEG_MAX_SKIP_RATE + 1
};


struct ThreadTaskInfo
{
    mfxFrameSurface1 *surface_work;
    mfxFrameSurface1 *surface_out;
    UMC::FrameData   *dst;
    mfxU32            decodeTaskID;
    mfxU32            vppTaskID;
    bool              needCheckVppStatus;
    mfxU32            numDecodeTasksToCheck;
};

class MFX_JPEG_Utility
{
public:

    static eMFXPlatform GetPlatform(VideoCORE * core, mfxVideoParam * par);
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type);
    static bool CheckVideoParam(mfxVideoParam *in, eMFXHWType type);

#if defined (MFX_VA)    
    static void      AdjustFrameAllocRequest(mfxFrameAllocRequest *request,
                                             mfxInfoMFX *info,
                                             eMFXHWType hwType, 
                                             eMFXVAType vaType,
                                             bool *needVpp);
    static void      AdjustFourCC(mfxFrameInfo *requestFrameInfo, mfxInfoMFX *info, eMFXHWType hwType, eMFXVAType vaType, bool *needVpp);
#endif

private:

    static bool IsNeedPartialAcceleration(VideoCORE * core, mfxVideoParam * par);

#if defined (MFX_VA_WIN)
    static mfxStatus CheckDecodeCaps(VideoCORE * core, mfxVideoParam * par);
#endif
#if defined (MFX_VA)
    static mfxStatus CheckVPPCaps(VideoCORE * core, mfxVideoParam * par);
#endif
};

VideoDECODEMJPEG::VideoDECODEMJPEG(VideoCORE *core, mfxStatus * sts)
    : VideoDECODE()
    , m_core(core)
    , m_isInit(false)
    , m_isOpaq(false)
    , m_frameOrder((mfxU16)MFX_FRAMEORDER_UNKNOWN)
    , m_platform(MFX_PLATFORM_SOFTWARE)
    , m_va(0)
{
    memset(&m_stat, 0, sizeof(m_stat));

    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
#if defined (MFX_VA)
    m_numPic = 0;
    m_pCc    = NULL;
    m_needVpp = false;
#endif
    m_tasksCount = 0;
    pLastTask = NULL;

    m_isHeaderFound = false;
    m_isHeaderParsed = false;

    m_skipRate = 0;
    m_skipCount = 0;
}

VideoDECODEMJPEG::~VideoDECODEMJPEG(void)
{
    Close();
}

mfxStatus VideoDECODEMJPEG::Init(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (m_isInit)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(par);

    m_platform = MFX_JPEG_Utility::GetPlatform(m_core, par);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (m_platform == MFX_PLATFORM_HARDWARE)
    {
        type = m_core->GetHWType();
    }

    if (CheckVideoParamDecoders(par, m_core->IsExternalFrameAllocator(), type) < MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!MFX_JPEG_Utility::CheckVideoParam(par, type))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    m_vFirstPar = *par;
    m_vFirstPar.mfx.NumThread = 0;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vFirstPar);
    m_vPar = m_vFirstPar;

    m_vPar.mfx.NumThread = (mfxU16)(m_vPar.AsyncDepth ? m_vPar.AsyncDepth : m_core->GetAutoAsyncDepth());
    if (MFX_PLATFORM_SOFTWARE != m_platform)
        m_vPar.mfx.NumThread = 1;

    Ipp32s useInternal = (MFX_PLATFORM_SOFTWARE == m_platform) ?
        (m_vPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : 
        (m_vPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    // allocate memory
    mfxFrameAllocRequest request;
    mfxFrameAllocRequest request_internal;
    memset(&request, 0, sizeof(request));
    memset(&m_response, 0, sizeof(m_response));
    memset(&m_response_alien, 0, sizeof(m_response_alien));
    m_isOpaq = false;

    mfxStatus mfxSts = QueryIOSurfInternal(m_core, &m_vPar, &request);
    if (mfxSts != MFX_ERR_NONE)
        return mfxSts;

    if (m_vPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        mfxExtOpaqueSurfaceAlloc *pOpaqAlloc = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        if (!pOpaqAlloc)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        useInternal = (m_platform == MFX_PLATFORM_SOFTWARE) ?
                      (pOpaqAlloc->Out.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET) :
                      (pOpaqAlloc->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY);
    }

    request_internal = request;

    if (useInternal)
        request.Type |= MFX_MEMTYPE_INTERNAL_FRAME;
    else
        request.Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

    // allocates external surfaces:
    bool mapOpaq = true;
    mfxExtOpaqueSurfaceAlloc *pOpqAlloc = 0;
    mfxSts = UpdateAllocRequest(par, &request, pOpqAlloc, mapOpaq);
    if (mfxSts < MFX_ERR_NONE)
        return mfxSts;

    if (mapOpaq)
    {
        request_internal.NumFrameMin = request_internal.NumFrameSuggested = pOpqAlloc->Out.NumSurface;
        mfxSts = m_core->AllocFrames(&request,
                                      &m_response,
                                      pOpqAlloc->Out.Surfaces,
                                      pOpqAlloc->Out.NumSurface);
    }
    else
    {
        if (m_platform != MFX_PLATFORM_SOFTWARE && !useInternal)
        {
            request.AllocId = par->AllocId;
            mfxSts = m_core->AllocFrames(&request, &m_response,false);
        }
    }

    if (mfxSts < MFX_ERR_NONE)
        return mfxSts;

#if defined (MFX_VA)
    if (m_platform != MFX_PLATFORM_SOFTWARE)
    {
        MFX_JPEG_Utility::AdjustFrameAllocRequest(&request_internal, 
                                                  &m_vPar.mfx, 
                                                  m_core->GetHWType(),
                                                  m_core->GetVAType(),
                                                  &m_needVpp);
        if (m_needVpp)
        {
            if(request_internal.Type & MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)
            {
                request_internal.Type -= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
            }
            request_internal.Type |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
        }

        useInternal |= (Ipp32s)m_needVpp;
    }
#endif

    if (MFX_PLATFORM_SOFTWARE == m_platform)
    {
        m_FrameAllocator.reset(new mfx_UMC_FrameAllocator);
    }
    else
    {
#if defined (MFX_VA)
        m_pMJPEGVideoDecoder.reset(new UMC::MJPEGVideoDecoderMFX_HW()); // HW
        if(m_needVpp)
        {
            m_FrameAllocator.reset(new mfx_UMC_FrameAllocator_D3D_Converter());
        }
        else
        {
            m_FrameAllocator.reset(new mfx_UMC_FrameAllocator_D3D);
        }

#else // Not VA
        return MFX_ERR_UNSUPPORTED;
#endif
    }

    // allocates internal surfaces:
    if (useInternal)
    {
        m_response_alien = m_response;
        m_FrameAllocator->SetExternalFramesResponse(&m_response_alien);
        request_internal.Type |= MFX_MEMTYPE_INTERNAL_FRAME;
        request = request_internal;
        mfxSts = m_core->AllocFrames(&request_internal, &m_response, ((m_vPar.IOPattern&MFX_MEMTYPE_SYSTEM_MEMORY) != 0));
        if (mfxSts < MFX_ERR_NONE)
            return mfxSts;
    }
    else
    {
        m_FrameAllocator->SetExternalFramesResponse(&m_response);
    }

    mfxVideoParam decPar = *par;
    decPar.mfx.FrameInfo = request.Info;
#if defined (MFX_VA)
    if (m_platform != MFX_PLATFORM_SOFTWARE)
    {
        mfxSts = m_core->CreateVA(&decPar, &request, &m_response, m_FrameAllocator.get());
        if (mfxSts < MFX_ERR_NONE)
            return mfxSts;
    }
#endif

    UMC::Status umcSts = m_FrameAllocator->InitMfx(0, m_core, &decPar, &request, &m_response, !useInternal, m_platform == MFX_PLATFORM_SOFTWARE);
    if (umcSts != UMC::UMC_OK)
        return MFX_ERR_MEMORY_ALLOC;

    umcSts = m_MemoryAllocator.InitMem(0, m_core);
    if (umcSts != UMC::UMC_OK)
        return MFX_ERR_MEMORY_ALLOC;

#if defined (MFX_VA)
    if (m_platform != MFX_PLATFORM_SOFTWARE)
        m_pMJPEGVideoDecoder->SetFrameAllocator(m_FrameAllocator.get());
#endif

    ConvertMFXParamsToUMC(&decPar, &umcVideoParams);
    umcVideoParams.numThreads = m_vPar.mfx.NumThread;

#if defined (MFX_VA)
    if (MFX_PLATFORM_SOFTWARE != m_platform)
    {
        m_core->GetVA((mfxHDL*)&m_va, MFX_MEMTYPE_FROM_DECODE);
        umcVideoParams.pVideoAccelerator = m_va;
    }
#endif

    m_frameConstructor.reset(new UMC::JpegFrameConstructor());

    umcVideoParams.lpMemoryAllocator = &m_MemoryAllocator;

#if defined (MFX_VA)
    if (MFX_PLATFORM_SOFTWARE != m_platform)
    {
        umcSts = m_pMJPEGVideoDecoder->Init(&umcVideoParams);
        if (umcSts != UMC::UMC_OK)
        {
            return ConvertUMCStatusToMfx(umcSts);
        }
#if defined (MFX_VA_WIN)
        if (MFX_HW_D3D11 == m_core->GetVAType())
        {
            if(request_internal.Info.FourCC != DXGI_FORMAT_AYUV)
                m_pMJPEGVideoDecoder->SetFourCC(request_internal.Info.FourCC);
            else
                m_pMJPEGVideoDecoder->SetFourCC(DXGI_FORMAT_B8G8R8A8_UNORM);
        }

        m_numPic = 0;
        if(m_pCc != NULL)
        {
            delete m_pCc;
            m_pCc    = NULL;
        }
#else
        m_pMJPEGVideoDecoder->SetFourCC(request_internal.Info.FourCC);
        m_numPic = 0;
#endif
    }
#endif
    m_tasksCount = 0;
    pLastTask = NULL;
    m_isInit = true;
    m_isHeaderFound = false;
    m_isHeaderParsed = false;

    m_frameOrder = 0;//(mfxU16)MFX_FRAMEORDER_UNKNOWN;

    // m_pMJPEGVideoDecoder->SetVideoParams(par);

    if (m_platform != m_core->GetPlatformType())
    {
        VM_ASSERT(m_platform == MFX_PLATFORM_SOFTWARE);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::Reset(mfxVideoParam *par)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (m_platform == MFX_PLATFORM_HARDWARE)
    {
        type = m_core->GetHWType();
    }

    if (CheckVideoParamDecoders(par, m_core->IsExternalFrameAllocator(), type) < MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!MFX_JPEG_Utility::CheckVideoParam(par, type))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!IsSameVideoParam(par, &m_vFirstPar))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    // need to sw acceleration
    if (m_platform != MFX_JPEG_Utility::GetPlatform(m_core, par))
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

#if defined (MFX_VA)
    if (MFX_PLATFORM_SOFTWARE != m_platform)
    {
        m_pMJPEGVideoDecoder->Reset();
        m_numPic = 0;
       
#if defined (MFX_VA_WIN) 
        if(m_pCc != NULL)
        {
            delete m_pCc;
            m_pCc    = NULL;
        }
#endif
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            mfxU32 picToCollect = (MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct) ?
                (1) : (2);

            while(!m_dsts.empty())
            {
                for(mfxU32 i=0; i<picToCollect; i++)
                    m_pMJPEGVideoDecoder->CloseFrame(&(m_dsts.back()), i);
                delete [] m_dsts.back();
                m_dsts.pop_back();
            }
        }
    }
#endif

    if (m_FrameAllocator->Reset() != UMC::UMC_OK)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_frameOrder = 0;//(mfxU16)MFX_FRAMEORDER_UNKNOWN;
    memset(&m_stat, 0, sizeof(m_stat));
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(par);
    m_vPar = *par;


    // m_pMJPEGVideoDecoder->SetVideoParams(par);

    if (isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    m_tasksCount = 0;
    pLastTask = NULL;
    while(!m_freeTasks.empty())
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        delete m_freeTasks.front();
        m_freeTasks.pop();
    }

    m_isHeaderFound = false;
    m_isHeaderParsed = false;

    m_skipRate = 0;
    m_skipCount = 0;

    m_frameConstructor->Reset();

    if (m_platform != m_core->GetPlatformType())
    {
        VM_ASSERT(m_platform == MFX_PLATFORM_SOFTWARE);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::Close(void)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

#if defined (MFX_VA)
    if (MFX_PLATFORM_SOFTWARE != m_platform)
    {
        if (!m_pMJPEGVideoDecoder.get())
            return MFX_ERR_NOT_INITIALIZED;

        m_pMJPEGVideoDecoder->Close();
        m_numPic = 0;
#if defined (MFX_VA_WIN)
        if(m_pCc != NULL)
        {
            delete m_pCc;
            m_pCc    = NULL;
        }
#endif
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            mfxU32 picToCollect = (MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct) ?
                (1) : (2);

            while(!m_dsts.empty())
            {
                for(mfxU32 i=0; i<picToCollect; i++)
                    m_pMJPEGVideoDecoder->CloseFrame(&(m_dsts.back()), i);
                delete [] m_dsts.back();
                m_dsts.pop_back();
            }
        }
    }
#endif

    m_FrameAllocator->Close();

    if (m_response.NumFrameActual)
        m_core->FreeFrames(&m_response);

    if (m_response_alien.NumFrameActual)
        m_core->FreeFrames(&m_response_alien);

    m_tasksCount = 0;
    pLastTask = NULL;
    m_isOpaq = false;
    m_isInit = false;
    m_isHeaderFound = false;
    m_isHeaderParsed = false;
    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_va = 0;
    memset(&m_stat, 0, sizeof(m_stat));

    m_frameConstructor->Close();

    // delete free tasks queue
    while (false == m_freeTasks.empty())
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        delete m_freeTasks.front();
        m_freeTasks.pop();
    }

    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEMJPEG::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_INTER;
}

mfxStatus VideoDECODEMJPEG::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);

    eMFXHWType type = core->GetHWType();
    return MFX_JPEG_Utility::Query(core, in, out, type);
}

mfxStatus VideoDECODEMJPEG::GetVideoParam(mfxVideoParam *par)
{
    UMC::Status umcRes = UMC::UMC_OK;
    MFX_JPEG_Decoder* pMJPEGVideoDecoder;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    mfxExtJPEGQuantTables* jpegQT = (mfxExtJPEGQuantTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_QT );
    mfxExtJPEGHuffmanTables*  jpegHT = (mfxExtJPEGHuffmanTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );

    memcpy_s(&par->mfx, sizeof(mfxInfoMFX), &m_vPar.mfx, sizeof(mfxInfoMFX));

    par->Protected = m_vPar.Protected;
    par->IOPattern = m_vPar.IOPattern;
    par->AsyncDepth = m_vPar.AsyncDepth;

    if (!par->mfx.FrameInfo.FrameRateExtD && !par->mfx.FrameInfo.FrameRateExtN)
    {
        par->mfx.FrameInfo.FrameRateExtD = m_vPar.mfx.FrameInfo.FrameRateExtD;
        par->mfx.FrameInfo.FrameRateExtN = m_vPar.mfx.FrameInfo.FrameRateExtN;

        if (!par->mfx.FrameInfo.FrameRateExtD && !par->mfx.FrameInfo.FrameRateExtN)
        {
            par->mfx.FrameInfo.FrameRateExtN = 30;
            par->mfx.FrameInfo.FrameRateExtD = 1;
        }
    }

    if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
    {
        par->mfx.FrameInfo.AspectRatioH = m_vPar.mfx.FrameInfo.AspectRatioH;
        par->mfx.FrameInfo.AspectRatioW = m_vPar.mfx.FrameInfo.AspectRatioW;

        if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
        {
            par->mfx.FrameInfo.AspectRatioH = 1;
            par->mfx.FrameInfo.AspectRatioW = 1;
        }
    }

    if(jpegQT || jpegHT)
    {
        if (MFX_PLATFORM_SOFTWARE == m_platform)
        {
            if(!pLastTask)
                return MFX_ERR_UNSUPPORTED;

            pMJPEGVideoDecoder = (MFX_JPEG_Decoder*)pLastTask->m_pMJPEGVideoDecoder.get();
        }
        else
        {
#if defined (MFX_VA)
            pMJPEGVideoDecoder = m_pMJPEGVideoDecoder.get();
#else
            return MFX_ERR_UNSUPPORTED;
#endif
        }

        if(jpegQT)
        {
            umcRes = pMJPEGVideoDecoder->FillQuantTableExtBuf(jpegQT);
            if (umcRes != UMC::UMC_OK)
                return ConvertUMCStatusToMfx(umcRes);
        }

        if(jpegHT)
        {
            umcRes = pMJPEGVideoDecoder->FillHuffmanTableExtBuf(jpegHT);
            if (umcRes != UMC::UMC_OK)
                return ConvertUMCStatusToMfx(umcRes);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);

    mfxStatus sts = CheckBitstream(bs);
    if (sts != MFX_ERR_NONE)
        return sts;

    MFXMediaDataAdapter in(bs);

    mfx_UMC_MemAllocator  tempAllocator;
    tempAllocator.InitMem(0, core);

    mfxExtJPEGQuantTables* jpegQT = (mfxExtJPEGQuantTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_QT );
    mfxExtJPEGHuffmanTables*  jpegHT = (mfxExtJPEGHuffmanTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );

    // DecodeHeader
    MFX_JPEG_Decoder decoder;

    UMC::VideoDecoderParams umcVideoParams;
    umcVideoParams.info.clip_info.height = par->mfx.FrameInfo.Height;
    umcVideoParams.info.clip_info.width = par->mfx.FrameInfo.Width;

    umcVideoParams.lpMemoryAllocator = &tempAllocator;

    UMC::Status umcRes = decoder.Init(&umcVideoParams);
    if (umcRes != UMC::UMC_OK)
    {
        return ConvertUMCStatusToMfx(umcRes);
    }

    umcRes = decoder.DecodeHeader(&in);

    in.Save(bs);

    if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA)
        return MFX_ERR_MORE_DATA;

    if (umcRes != UMC::UMC_OK)
        return ConvertUMCStatusToMfx(umcRes);

    mfxVideoParam temp;

    umcRes = decoder.FillVideoParam(&temp, false);
    if (umcRes != UMC::UMC_OK)
        return ConvertUMCStatusToMfx(umcRes);

    if(jpegQT)
    {
        umcRes = decoder.FillQuantTableExtBuf(jpegQT);
        if (umcRes != UMC::UMC_OK)
            return ConvertUMCStatusToMfx(umcRes);
    }

    if(jpegHT)
    {
        umcRes = decoder.FillHuffmanTableExtBuf(jpegHT);
        if (umcRes != UMC::UMC_OK)
            return ConvertUMCStatusToMfx(umcRes);
    }

    decoder.Close();
    tempAllocator.Close();

    memcpy_s(&(par->mfx.FrameInfo), sizeof(mfxFrameInfo), &temp.mfx.FrameInfo, sizeof(mfxFrameInfo));

    par->mfx.JPEGChromaFormat = temp.mfx.JPEGChromaFormat;
    par->mfx.JPEGColorFormat = temp.mfx.JPEGColorFormat;
    par->mfx.Rotation  = temp.mfx.Rotation;
    par->mfx.InterleavedDec  = temp.mfx.InterleavedDec;

    //eMFXPlatform platform = MFX_JPEG_Utility::GetPlatform(core, par);
    //if (platform != core->GetPlatformType())
    //{
    //    VM_ASSERT(platform == MFX_PLATFORM_SOFTWARE);
    //    return MFX_WRN_PARTIAL_ACCELERATION;
    //}

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(par, request);

    eMFXPlatform platform = MFX_JPEG_Utility::GetPlatform(core, par);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        type = core->GetHWType();
    }

    mfxVideoParam params;
    memcpy_s(&params, sizeof(mfxVideoParam), par, sizeof(mfxVideoParam));
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&params);

    if (!(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && !(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    //if (!MFX_Utility::CheckVideoParam(par, type))
      //  return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxStatus sts = QueryIOSurfInternal(core, &params, request);
    if (sts != MFX_ERR_NONE)
        return sts;

    Ipp32s isInternalManaging = (MFX_PLATFORM_SOFTWARE == platform) ?
        (params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (isInternalManaging)
    {
        request->NumFrameSuggested = request->NumFrameMin = par->AsyncDepth ? par->AsyncDepth : core->GetAutoAsyncDepth();

        request->Type = MFX_MEMTYPE_FROM_DECODE;
        if (MFX_PLATFORM_SOFTWARE == platform)
        {
            if ((request->Info.FourCC == MFX_FOURCC_RGB4 || request->Info.FourCC == MFX_FOURCC_YUY2) && MFX_HW_D3D11 == core->GetVAType())
                request->Type |= MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
            else
                request->Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        }
        else
        {
            request->Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
        }
    }

    if (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        request->Type |= MFX_MEMTYPE_OPAQUE_FRAME;
    }
    else
    {
        request->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;
    }

    if (platform != core->GetPlatformType())
    {
        VM_ASSERT(platform == MFX_PLATFORM_SOFTWARE);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::QueryIOSurfInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    eMFXPlatform platform = MFX_JPEG_Utility::GetPlatform(core, par);

    memcpy_s(&request->Info, sizeof(mfxFrameInfo), &par->mfx.FrameInfo, sizeof(mfxFrameInfo));

    mfxU32 asyncDepth = (par->AsyncDepth ? par->AsyncDepth : core->GetAutoAsyncDepth());

    request->NumFrameMin = mfxU16 (asyncDepth);

    request->NumFrameSuggested = request->NumFrameMin;

    request->Type = MFX_MEMTYPE_FROM_DECODE;

    if(MFX_ROTATION_90 == par->mfx.Rotation || MFX_ROTATION_270 == par->mfx.Rotation)
    {
        std::swap(request->Info.Height, request->Info.Width);
        std::swap(request->Info.AspectRatioH, request->Info.AspectRatioW);
        std::swap(request->Info.CropH, request->Info.CropW);
        std::swap(request->Info.CropY, request->Info.CropX);
    }
    
    request->Info.Width = UMC::align_value<mfxU16>(request->Info.Width, 0x10);
    request->Info.Height = UMC::align_value<mfxU16>(request->Info.Height,
        (request->Info.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 0x10 : 0x20);
#if defined(UMC_VA_LINUX)
    // large pictures are not supported on Linux so far
    if ((request->Info.Width > 8192) || (request->Info.Height > 8192)) {
        return MFX_ERR_UNSUPPORTED;
    }
#endif
    if (MFX_PLATFORM_SOFTWARE == platform)
    {
        request->Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else
    {       
#if defined (MFX_VA)
        eMFXHWType type = MFX_HW_UNKNOWN;
        if (platform == MFX_PLATFORM_HARDWARE)
        {
            type = core->GetHWType();
        }

        bool needVpp = false;
        if(par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF || 
           par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
        {
            needVpp = true;
        }
        
        mfxFrameAllocRequest request_internal = *request;
#if defined (MFX_VA)
        MFX_JPEG_Utility::AdjustFourCC(&request_internal.Info, &par->mfx, core->GetHWType(), core->GetVAType(), &needVpp);

        if (needVpp && MFX_HW_D3D11 == core->GetVAType())
            request->Type |= MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
        else
            request->Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
#else
       request->Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
#endif

#else
        return MFX_ERR_UNSUPPORTED;
#endif
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::GetDecodeStat(mfxDecodeStat *stat)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(stat);

    //m_stat.NumSkippedFrame = 0;//m_pMJPEGVideoDecoder->GetSkipInfo().numberOfSkippedFrames;
    m_stat.NumCachedFrame = 0;
    m_stat.NumError = 0;

    memcpy_s(stat, sizeof(mfxDecodeStat), &m_stat, sizeof(mfxDecodeStat));
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::MJPEGDECODERoutine(void *pState, void *pParam,
                                               mfxU32 threadNumber, mfxU32 callNumber)
{
    VideoDECODEMJPEG &obj = *((VideoDECODEMJPEG *) pState);
    mfxStatus mfxRes;

    MFX::AutoTimer timer("DecodeFrame");

    if (MFX_PLATFORM_SOFTWARE == obj.m_platform)
    {
        CJpegTask *pTask = (CJpegTask *) pParam;
        mfxRes = obj.RunThread(*pTask, threadNumber, callNumber);
    }
    else
    {
#if defined (MFX_VA)
        mfxRes = obj.RunThreadHW(pParam, threadNumber);
#else
        return MFX_ERR_UNSUPPORTED;
#endif
    }

    return mfxRes;

} // mfxStatus VideoDECODEMJPEG::MJPEGDECODERoutine(void *pState, void *pParam,

mfxStatus VideoDECODEMJPEG::MJPEGCompleteProc(void *pState, void *pParam,
                                              mfxStatus taskRes)
{
    VideoDECODEMJPEG &obj = *((VideoDECODEMJPEG *) pState);

    if (MFX_PLATFORM_SOFTWARE == obj.m_platform)
    {
        CJpegTask &task = *((CJpegTask *) pParam);

        // do color conversion and other useless stuff
        if (MFX_ERR_NONE == taskRes)
        {
            UMC::Status umcRes = UMC::UMC_OK;

            umcRes = task.m_pMJPEGVideoDecoder->PostProcessing(task.GetPictureBuffer(0).timeStamp);
            if (UMC::UMC_OK != umcRes)
            {
                return ConvertUMCStatusToMfx(umcRes);
            }

            // decoding is ready. prepare to output:
            mfxStatus mfxSts = obj.m_FrameAllocator->PrepareToOutput(task.surface_out,
                                                                     task.dst->GetFrameMID(),
                                                                     &obj.m_vPar,
                                                                     obj.m_isOpaq);
            if (mfxSts < MFX_ERR_NONE)
            {
                return mfxSts;
            }

            task.m_pMJPEGVideoDecoder->CloseFrame();

            obj.m_stat.NumFrame++;
        }

        task.Reset();
        {
            UMC::AutomaticUMCMutex guard(obj.m_guard);
            obj.m_freeTasks.push(&task);
        }
    }
    else
    {
#if defined (MFX_VA)        
        ThreadTaskInfo * info = (ThreadTaskInfo *)pParam;

        UMC::AutomaticUMCMutex guard(obj.m_guard);

        mfxI32 index = -1;        
        for (size_t i = 0; i < obj.m_dsts.size(); i++)
            if(obj.m_dsts[i]->GetFrameMID() == info->dst->GetFrameMID())
            {
                index = (mfxI32)i;
            }

        if(index != -1)
        {
            mfxU32 picToCollect = (MFX_PICSTRUCT_PROGRESSIVE == obj.m_vPar.mfx.FrameInfo.PicStruct) ?
                (1) : (2);

            for(mfxU32 i=0; i<picToCollect; i++)
                obj.m_pMJPEGVideoDecoder->CloseFrame(&(obj.m_dsts[index]), i);
            delete [] obj.m_dsts[index];
            obj.m_dsts.erase(obj.m_dsts.begin() + index);
        }

        delete (ThreadTaskInfo *)pParam;
#else
        return MFX_ERR_UNSUPPORTED;
#endif
    }

    return MFX_ERR_NONE;
}

#if defined (MFX_VA)
mfxStatus VideoDECODEMJPEG::RunThreadHW(void * params, mfxU32 threadNumber)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    ThreadTaskInfo * info = (ThreadTaskInfo *)params;

#if defined (MFX_VA)
    if(m_needVpp)
    {
        if(info->needCheckVppStatus)
        {
            mfxSts = ((mfx_UMC_FrameAllocator_D3D_Converter *)m_FrameAllocator.get())->CheckPreparingToOutput(info->surface_out,
                                                                                                              info->dst,
                                                                                                              &m_vPar,
                                                                                                              &m_pCc,
                                                                                                              (mfxU16)info->vppTaskID);
            if(mfxSts != MFX_TASK_DONE)
            {
                return mfxSts;
            }
            info->needCheckVppStatus = false;
        }

        mfxU16 corruptedField = 0;
        if(info->numDecodeTasksToCheck == 2)
        {
            mfxSts = m_pMJPEGVideoDecoder->CheckStatusReportNumber(info->decodeTaskID-1, &corruptedField);
            if(mfxSts != MFX_TASK_DONE)
            {
                return mfxSts;
            }
            info->numDecodeTasksToCheck--;
            info->surface_out->Data.Corrupted |= corruptedField;
        }

        if(info->numDecodeTasksToCheck == 1)
        {
            mfxSts = m_pMJPEGVideoDecoder->CheckStatusReportNumber(info->decodeTaskID, &corruptedField);
            if(mfxSts != MFX_TASK_DONE)
            {
                return mfxSts;
            }
            info->numDecodeTasksToCheck--;
            info->surface_out->Data.Corrupted |= corruptedField;
        }
    }
    else
#endif
    {
        mfxSts = m_pMJPEGVideoDecoder->CheckStatusReportNumber(info->decodeTaskID, &(info->surface_out->Data.Corrupted));
        if(mfxSts != MFX_TASK_DONE)
        {
            return mfxSts;
        }

        mfxSts = ((mfx_UMC_FrameAllocator_D3D *)m_FrameAllocator.get())->PrepareToOutput(info->surface_out, info->dst->GetFrameMID(), &m_vPar, m_isOpaq);
        if (mfxSts < MFX_ERR_NONE)
        {
            return mfxSts;
        }
    }

    info = 0;
    threadNumber = threadNumber;

    return MFX_TASK_DONE;
}
#endif

mfxStatus VideoDECODEMJPEG::RunThread(CJpegTask &task,
                                      mfxU32 threadNumber,
                                      mfxU32 callNumber)
{
    // check the number of call. one call = one piece decoded. all extra call
    // should go exit.
    if (callNumber >= task.NumPiecesCollected())
    {
        return MFX_TASK_DONE;
    }

    // do decoding process
    task.m_pMJPEGVideoDecoder.get()->DecodePicture(task, threadNumber, callNumber);

    return ((callNumber + 1) == task.NumPiecesCollected()) ? (MFX_TASK_DONE) : (MFX_TASK_WORKING);

} // mfxStatus VideoDECODEMJPEG::RunThread(CJpegTask &task, mfxU32 threadNumber, mfxU32 callNumber)

mfxStatus VideoDECODEMJPEG::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT *pEntryPoint)
{
    UMC::Status umcRes;
    UMC::FrameData *dst;

    mfxStatus mfxSts = DecodeFrameCheck(bs, surface_work, surface_out);

    if (MFX_ERR_NONE == mfxSts) // It can be useful to run threads right after first frame receive
    {
        CJpegTask *pTask = NULL;

        if (MFX_PLATFORM_SOFTWARE == m_platform)
        {
            pTask = m_freeTasks.front();

            // prepare the decoder(s)
            umcRes = pTask->m_pMJPEGVideoDecoder->AllocateFrame();
            if (UMC::UMC_OK != umcRes)
            {
                return ConvertUMCStatusToMfx(umcRes);
            }

            // save parameters to the task
            pTask->dst = pTask->m_pMJPEGVideoDecoder.get()->GetDst();

            dst = m_freeTasks.front()->dst;
        }
        else
        {
#if defined (MFX_VA)
            dst = m_dst;
#else
            return MFX_ERR_UNSUPPORTED;
#endif
        }

        {
            // output surface is always working surface
            *surface_out = m_FrameAllocator->GetSurface(dst->GetFrameMID(),
                                                        GetOriginalSurface(surface_work),
                                                        &m_vPar);
            *surface_out = (m_isOpaq) ? m_core->GetOpaqSurface((*surface_out)->Data.MemId) : (*surface_out);

            if (!(*surface_out))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            (*surface_out)->Info.FrameId.ViewId = 0; // (mfxU16)pFrame->m_viewId;

            bool isShouldUpdate = !(m_vFirstPar.mfx.FrameInfo.AspectRatioH || m_vFirstPar.mfx.FrameInfo.AspectRatioW);

            if(MFX_ROTATION_0 == m_vFirstPar.mfx.Rotation || MFX_ROTATION_180 == m_vFirstPar.mfx.Rotation)
            {
                (*surface_out)->Info.CropH = m_vPar.mfx.FrameInfo.CropH;
                (*surface_out)->Info.CropW = m_vPar.mfx.FrameInfo.CropW;
                (*surface_out)->Info.AspectRatioH = isShouldUpdate ? (mfxU16) 1 : m_vFirstPar.mfx.FrameInfo.AspectRatioH;
                (*surface_out)->Info.AspectRatioW = isShouldUpdate ? (mfxU16) 1 : m_vFirstPar.mfx.FrameInfo.AspectRatioW;
            }
            else
            {
                (*surface_out)->Info.CropH = m_vPar.mfx.FrameInfo.CropW;
                (*surface_out)->Info.CropW = m_vPar.mfx.FrameInfo.CropH;
                (*surface_out)->Info.AspectRatioH = isShouldUpdate ? (mfxU16) 1 : m_vFirstPar.mfx.FrameInfo.AspectRatioW;
                (*surface_out)->Info.AspectRatioW = isShouldUpdate ? (mfxU16) 1 : m_vFirstPar.mfx.FrameInfo.AspectRatioH;
            }

            (*surface_out)->Info.CropX = 0;
            (*surface_out)->Info.CropY = 0;

            isShouldUpdate = !(m_vFirstPar.mfx.FrameInfo.FrameRateExtD || m_vFirstPar.mfx.FrameInfo.FrameRateExtN);

            (*surface_out)->Info.FrameRateExtD = isShouldUpdate ? m_vPar.mfx.FrameInfo.FrameRateExtD : m_vFirstPar.mfx.FrameInfo.FrameRateExtD;
            (*surface_out)->Info.FrameRateExtN = isShouldUpdate ? m_vPar.mfx.FrameInfo.FrameRateExtN : m_vFirstPar.mfx.FrameInfo.FrameRateExtN;
            (*surface_out)->Info.PicStruct = (mfxU16) ((MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct) ?
                                                       (MFX_PICSTRUCT_PROGRESSIVE) :
                                                       (MFX_PICSTRUCT_FIELD_TFF));
            if (MFX_PLATFORM_SOFTWARE == m_platform)
            {
                (*surface_out)->Data.TimeStamp = GetMfxTimeStamp(m_freeTasks.front()->GetPictureBuffer(0).timeStamp);
            }
            else
            {
#if defined (MFX_VA)
                (*surface_out)->Data.TimeStamp = GetMfxTimeStamp(dst->GetTime());
#endif
            }

            if(MFX_TIME_STAMP_INVALID == (*surface_out)->Data.TimeStamp)
            {
                (*surface_out)->Data.TimeStamp = ((mfxU64)m_frameOrder * m_vPar.mfx.FrameInfo.FrameRateExtD * MFX_TIME_STAMP_FREQUENCY) / m_vPar.mfx.FrameInfo.FrameRateExtN;
            }

            (*surface_out)->Data.FrameOrder = m_frameOrder;
            m_frameOrder++;
            
            (*surface_out)->Data.Corrupted = 0;
        }

        // prepare output structure
        pEntryPoint->pRoutine = &MJPEGDECODERoutine;
        pEntryPoint->pCompleteProc = &MJPEGCompleteProc;
        pEntryPoint->pState = this;

        if (MFX_PLATFORM_SOFTWARE == m_platform)
        {
            pTask->surface_work = GetOriginalSurface(surface_work);
            pTask->surface_out = GetOriginalSurface(*surface_out);

            pEntryPoint->requiredNumThreads = IPP_MIN(pTask->m_pMJPEGVideoDecoder->NumDecodersAllocated(),
                                                      pTask->NumPiecesCollected());
            pEntryPoint->pParam = pTask;

            pLastTask = m_freeTasks.front();

            // remove the ready task from the queue
            {
                UMC::AutomaticUMCMutex guard(m_guard);
                m_freeTasks.pop();
            }
        }
        else
        {
#if defined (MFX_VA)
            mfxU16 taskId = 0;

#if defined (MFX_VA)
            if(m_needVpp)
            {
                UMC::ConvertInfo * convertInfo = m_pMJPEGVideoDecoder->GetConvertInfo();
                mfx_UMC_FrameAllocator_D3D_Converter::JPEG_Info info;
                info.colorFormat = convertInfo->colorFormat;
                info.UOffset = convertInfo->UOffset;
                info.VOffset = convertInfo->VOffset;

                ((mfx_UMC_FrameAllocator_D3D_Converter *)m_FrameAllocator.get())->SetJPEGInfo(&info);

                // decoding is ready. prepare to output:
                mfxStatus mfxSts = ((mfx_UMC_FrameAllocator_D3D_Converter *)m_FrameAllocator.get())->StartPreparingToOutput(GetOriginalSurface(*surface_out), dst, &m_vPar, &m_pCc, &taskId, m_isOpaq);
                if (mfxSts < MFX_ERR_NONE)
                {
                    return mfxSts;
                }
            }
#endif

            ThreadTaskInfo * info = new ThreadTaskInfo();
            info->surface_work = GetOriginalSurface(surface_work);
            info->surface_out = GetOriginalSurface(*surface_out);
            info->decodeTaskID = m_pMJPEGVideoDecoder->GetStatusReportNumber();
            info->vppTaskID = (mfxU32)taskId;
            info->needCheckVppStatus = m_needVpp;
            info->numDecodeTasksToCheck = MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct ? 1 : 2;
            info->dst = dst;

            pEntryPoint->requiredNumThreads = m_vPar.mfx.NumThread;
            pEntryPoint->pParam = info;
#endif
        }

        pEntryPoint->pRoutineName = (char *)"DecodeMJPEG";
    }

    return mfxSts;
}

mfxStatus VideoDECODEMJPEG::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out)
{
    mfxStatus sts;
    mfxU32 picToCollect;
    UMC::Status umcRes = UMC::UMC_OK;
    MFX_JPEG_Decoder* pMJPEGVideoDecoder;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    if (MFX_PLATFORM_SOFTWARE == m_platform)
    {
        // make sure that there is a free task
        if (m_freeTasks.empty())
        {
            if(m_tasksCount >= (m_vPar.AsyncDepth ? m_vPar.AsyncDepth : m_core->GetAutoAsyncDepth()))
            {
                return MFX_WRN_DEVICE_BUSY;
            }

            std::auto_ptr<CJpegTask> pTask(new CJpegTask());
            m_tasksCount++;

            if (NULL == pTask.get())
            {
                return MFX_ERR_MEMORY_ALLOC;
            }

            // initialize the task
            sts = pTask.get()->Initialize(umcVideoParams, 
                                          m_FrameAllocator.get(), 
                                          m_vPar.mfx.Rotation, 
                                          m_vPar.mfx.JPEGChromaFormat, 
                                          m_vPar.mfx.JPEGColorFormat);
            if (MFX_ERR_NONE != sts)
            {
                return sts;
            }

            // save the task object into the queue
            {
                UMC::AutomaticUMCMutex guard(m_guard);
                m_freeTasks.push(pTask.release());
            }
        }
    }
    else
    {
#if defined (MFX_VA)
        UMC::AutomaticUMCMutex guard(m_guard);
        
        if(m_dsts.size() >= (m_vPar.AsyncDepth ? m_vPar.AsyncDepth : m_core->GetAutoAsyncDepth()))
        {
            return MFX_WRN_DEVICE_BUSY;
        }
#endif
    }

    MFX_CHECK_NULL_PTR2(surface_work, surface_out);

    sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;
    if (sts != MFX_ERR_NONE)
        return sts;

    *surface_out = 0;

    if (m_isOpaq)
    {
        sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_JPEG);
        if (sts != MFX_ERR_NONE)
            return MFX_ERR_UNSUPPORTED;

        if (surface_work->Data.MemId || surface_work->Data.Y || surface_work->Data.R || surface_work->Data.A || surface_work->Data.UV) // opaq surface
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        surface_work = GetOriginalSurface(surface_work);
        if (!surface_work)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_JPEG);
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_UNSUPPORTED;

    sts = CheckFrameData(surface_work);
    if (sts != MFX_ERR_NONE)
        return sts;

    sts = m_FrameAllocator->SetCurrentMFXSurface(surface_work, m_isOpaq);
    if (sts != MFX_ERR_NONE)
        return sts;

    sts = MFX_ERR_NULL_PTR;

    mfxU32 numPic = 0;
    picToCollect = (MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct) ?
                   (1) : (2);

    if (MFX_PLATFORM_SOFTWARE == m_platform)
    {
        pMJPEGVideoDecoder = (MFX_JPEG_Decoder*)m_freeTasks.front()->m_pMJPEGVideoDecoder.get();
    }
    else
    {
#if defined (MFX_VA)
        pMJPEGVideoDecoder = m_pMJPEGVideoDecoder.get();
        if(m_numPic == 0)
        {
            m_dst = new UMC::FrameData[picToCollect];
        }
#else
        return MFX_ERR_UNSUPPORTED;
#endif
    }

    do
    {
        MFXMediaDataAdapter src(bs);
        UMC::MediaDataEx *pSrcData;
        mfxU32 fieldPos;

        if(!m_isHeaderFound && bs)
        {
            umcRes = pMJPEGVideoDecoder->FindStartOfImage(&src);
            if (umcRes != UMC::UMC_OK)
            {
                if (MFX_PLATFORM_SOFTWARE == m_platform)
                {
                    if(umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA)
                    {
                        if(!m_freeTasks.empty())
                            m_freeTasks.front()->Reset();                        
                    }
                }
                else
                {
#if defined (MFX_VA)
                    if(umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA || m_numPic == 0)
                    {
                        delete[] m_dst;
                        m_numPic = 0;
                    }
#endif
                }
                return (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA) ? MFX_ERR_MORE_DATA : ConvertUMCStatusToMfx(umcRes);
            }
            src.Save(bs);
            m_isHeaderFound = true;
        }

        if(!m_isHeaderParsed && bs)
        {
            umcRes = pMJPEGVideoDecoder->_GetFrameInfo((Ipp8u*)src.GetDataPointer(), src.GetDataSize());
            if (umcRes != UMC::UMC_OK)
            {
                if (MFX_PLATFORM_SOFTWARE == m_platform)
                {
                    if(umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA)
                    {
                        if(!m_freeTasks.empty())
                            m_freeTasks.front()->Reset();                        
                    }
                }
                else
                {
#if defined (MFX_VA)
                    if(umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA || m_numPic == 0)
                    {
                        delete[] m_dst;
                        m_numPic = 0;
                    }
#endif
                }
                return (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA) ? MFX_ERR_MORE_DATA : ConvertUMCStatusToMfx(umcRes);
            }

            mfxVideoParam temp;
            pMJPEGVideoDecoder->FillVideoParam(&temp, false);

            if(m_vPar.mfx.FrameInfo.CropW != temp.mfx.FrameInfo.CropW ||
                m_vPar.mfx.FrameInfo.CropH != temp.mfx.FrameInfo.CropH)
            {
                if (MFX_PLATFORM_SOFTWARE == m_platform)
                {
                    if(!m_freeTasks.empty())
                        m_freeTasks.front()->Reset();
                }
                else
                {
#if defined (MFX_VA)
                    delete[] m_dst;
                    m_numPic = 0;
#endif
                }                
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            }
            m_isHeaderParsed = true;
        }

        mfxU32 maxBitstreamSize = m_vPar.mfx.FrameInfo.CropW * m_vPar.mfx.FrameInfo.CropH;
        switch(m_vPar.mfx.JPEGChromaFormat)
        {
        case CHROMA_TYPE_YUV411:
        case CHROMA_TYPE_YUV420:
            maxBitstreamSize = maxBitstreamSize * 3 / 2;
            break;
        case CHROMA_TYPE_YUV422H_2Y:
        case CHROMA_TYPE_YUV422V_2Y:
        case CHROMA_TYPE_YUV422H_4Y:
        case CHROMA_TYPE_YUV422V_4Y:
            maxBitstreamSize = maxBitstreamSize * 2;
            break;
        case CHROMA_TYPE_YUV444:
            maxBitstreamSize = maxBitstreamSize * 3;
            break;
        case CHROMA_TYPE_YUV400:
        default:
            break;
        };

        pSrcData = m_frameConstructor->GetFrame(bs ? &src : 0, maxBitstreamSize);

        // update the bytes used in bitstream
        src.Save(bs);
        if (NULL == pSrcData)
        {
#if defined (MFX_VA)
            if (MFX_PLATFORM_SOFTWARE != m_platform)
            {
                if(m_numPic == 0)
                {
                    delete[] m_dst;
                }
            }
#endif
            return MFX_ERR_MORE_DATA;
        }

        m_isHeaderFound = false;
        m_isHeaderParsed = false;

        if (MFX_PLATFORM_SOFTWARE == m_platform)
        {
            // select the field position. 0 means top, 1 means bottom.
            fieldPos = m_freeTasks.front()->NumPicCollected();

            if (MFX_PICSTRUCT_FIELD_BFF == m_vPar.mfx.FrameInfo.PicStruct)
            {
                // cheange field order in BFF case
                fieldPos ^= 1;
            }

            // add picture to the task
            sts = m_freeTasks.front()->AddPicture(pSrcData, fieldPos);
            if (MFX_ERR_NONE != sts)
            {
                return sts;
            }
            numPic = m_freeTasks.front()->NumPicCollected();
        }
        else
        {
#if defined (MFX_VA)
            fieldPos = m_numPic;

            if (MFX_PICSTRUCT_FIELD_BFF == m_vPar.mfx.FrameInfo.PicStruct)
            {
                // cheange field order in BFF case
                fieldPos ^= 1;
            }

            switch(m_vPar.mfx.Rotation)
            {
            case MFX_ROTATION_0:
                umcRes = pMJPEGVideoDecoder->SetRotation(0);
                break;
            case MFX_ROTATION_90:
                umcRes = pMJPEGVideoDecoder->SetRotation(90);
                break;
            case MFX_ROTATION_180:
                umcRes = pMJPEGVideoDecoder->SetRotation(180);
                break;
            case MFX_ROTATION_270:
                umcRes = pMJPEGVideoDecoder->SetRotation(270);
                break;
            }

            if (umcRes != UMC::UMC_OK)
            {
                delete[] m_dst;
                return ConvertUMCStatusToMfx(umcRes);
            }

            umcRes = pMJPEGVideoDecoder->GetFrame(pSrcData, &m_dst, fieldPos);

            sts = MFX_ERR_NONE;

            // convert status
            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA || umcRes == UMC::UMC_ERR_SYNC)
            {
                if(m_numPic == 0)
                {
                    delete[] m_dst;
                }

                sts = MFX_ERR_MORE_DATA;
            }
            else
            {
                if (umcRes != UMC::UMC_OK)
                {
                    delete[] m_dst;
                    sts = ConvertUMCStatusToMfx(umcRes);
                }
            }

            if (!(umcRes == UMC::UMC_OK && m_dst))    // return frame to display
                return sts;

            m_numPic++;
            numPic = m_numPic;
#endif
        }

    // make sure, that we collected BOTH fields
    } while (picToCollect > numPic);

#if defined (MFX_VA)
    if (MFX_PLATFORM_SOFTWARE != m_platform)
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_dsts.push_back(m_dst);
        m_numPic = 0;
    }
#endif

    // check if skipping is enabled
    m_skipCount += m_skipRate;
    if (JPEG_SKIP_BOUND <= m_skipCount)
    {
        m_skipCount -= JPEG_SKIP_BOUND;

        m_stat.NumSkippedFrame++;

        // it is time to skip a frame
#if defined (MFX_VA)
        if (MFX_PLATFORM_SOFTWARE != m_platform)
        {
            delete[] m_dst;
            m_numPic = 0;
        }
#endif
        if(!m_freeTasks.empty())
            m_freeTasks.front()->Reset();
        return MFX_ERR_MORE_DATA;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::DecodeFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out)
{
    bs = bs;
    surface_work = surface_work;
    surface_out = surface_out;
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(ud, sz, ts);
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEMJPEG::GetPayload( mfxU64 *ts, mfxPayload *payload )
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(ts, payload, payload->Data);
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEMJPEG::SetSkipMode(mfxSkipMode mode)
{
    // check error(s)
    if (!m_isInit)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    // check if we reached bounds of skipping
    if (((JPEG_MIN_SKIP_RATE == m_skipRate) && (MFX_SKIPMODE_LESS == mode)) ||
        ((JPEG_MAX_SKIP_RATE == m_skipRate) && (MFX_SKIPMODE_MORE == mode)) ||
        ((JPEG_MIN_SKIP_RATE == m_skipRate) && (MFX_SKIPMODE_NOSKIP == mode)))
    {
        return MFX_WRN_VALUE_NOT_CHANGED;
    }

    // set new skip rate
    switch (mode)
    {
    case MFX_SKIPMODE_LESS:
        m_skipRate -= 1;
        break;

    case MFX_SKIPMODE_MORE:
        m_skipRate += 1;
        break;

    default:
        m_skipRate = 0;
        break;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEMJPEG::SetSkipMode(mfxSkipMode mode)

bool VideoDECODEMJPEG::IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar)
{
    if ((newPar->IOPattern & (MFX_IOPATTERN_OUT_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY)) !=
        (oldPar->IOPattern & (MFX_IOPATTERN_OUT_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY)) )
    {
        return false;
    }

    if (newPar->Protected != oldPar->Protected)
    {
        return false;
    }

    if (newPar->AsyncDepth != oldPar->AsyncDepth)
    {
        return false;
    }

    mfxFrameAllocRequest requestOld;
    memset(&requestOld, 0, sizeof(requestOld));
    mfxFrameAllocRequest requestNew;
    memset(&requestNew, 0, sizeof(requestNew));

    mfxStatus mfxSts = QueryIOSurfInternal(m_core, oldPar, &requestOld);
    if (mfxSts != MFX_ERR_NONE)
        return false;

    mfxSts = QueryIOSurfInternal(m_core, newPar, &requestNew);
    if (mfxSts != MFX_ERR_NONE)
        return false;

    if (newPar->mfx.FrameInfo.Height > oldPar->mfx.FrameInfo.Height)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.Width > oldPar->mfx.FrameInfo.Width)
    {
        return false;
    }

    if (m_response.NumFrameActual)
    {
        if (requestNew.NumFrameMin > m_response.NumFrameActual)
            return false;
    }
    else
    {
        if (requestNew.NumFrameMin > requestOld.NumFrameMin || requestNew.Type != requestOld.Type)
            return false;
    }

    if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }

    if (oldPar->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        mfxExtOpaqueSurfaceAlloc * opaqueNew = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(newPar->ExtParam, newPar->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        mfxExtOpaqueSurfaceAlloc * opaqueOld = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(oldPar->ExtParam, oldPar->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        if (opaqueNew && opaqueOld)
        {
            if (opaqueNew->In.Type != opaqueOld->In.Type)
                return false;

            if (opaqueNew->In.NumSurface != opaqueOld->In.NumSurface)
                return false;

            for (Ipp32u i = 0; i < opaqueNew->In.NumSurface; i++)
            {
                if (opaqueNew->In.Surfaces[i] != opaqueOld->In.Surfaces[i])
                    return false;
            }

            if (opaqueNew->Out.Type != opaqueOld->Out.Type)
                return false;

            if (opaqueNew->Out.NumSurface != opaqueOld->Out.NumSurface)
                return false;

            for (Ipp32u i = 0; i < opaqueNew->Out.NumSurface; i++)
            {
                if (opaqueNew->Out.Surfaces[i] != opaqueOld->Out.Surfaces[i])
                    return false;
            }
        }
        else
            return false;
    }

    /*if ((!newPar->mfx.NumThread || !oldPar->mfx.NumThread) && oldPar->mfx.NumThread != newPar->mfx.NumThread) //  need more surfaces for efficient decoding
    {
        return false;
    }

    if (newPar->mfx.NumThread > oldPar->mfx.NumThread && oldPar->mfx.NumThread > 0) //  need more surfaces for efficient decoding
    {
        return false;
    }*/

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MFX_JPEG_Utility implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool MFX_JPEG_Utility::IsNeedPartialAcceleration(VideoCORE * core, mfxVideoParam * par)
{
    if (!par)
        return false;

    //max & min supported resolution should be checked with JPEG_DECODING_CAPS
    //if (par->mfx.FrameInfo.Width > 4096 || par->mfx.FrameInfo.Height > 4096)
    //    return true;

#if defined (MFX_VA_LINUX)
    // NV12 is supported on Linux HW at the moment
    if (par->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 &&
        par->mfx.FrameInfo.FourCC != MFX_FOURCC_RGB4)
        return true;
/* on Linux in case of multiscan turn off HW support until because some functionality is missed so far */
    if (par->mfx.InterleavedDec == MFX_SCANTYPE_NONINTERLEAVED)
            return true;
#endif

    if (par->mfx.JPEGColorFormat == MFX_JPEG_COLORFORMAT_RGB &&
        par->mfx.JPEGChromaFormat != MFX_CHROMAFORMAT_YUV444)
        return true;

    if (par->mfx.JPEGColorFormat == MFX_JPEG_COLORFORMAT_RGB &&
        core->GetHWType() == MFX_HW_VLV)
        return true;

    return false;
}

eMFXPlatform MFX_JPEG_Utility::GetPlatform(VideoCORE * core, mfxVideoParam * par)
{
    eMFXPlatform platform = core->GetPlatformType();

    if(platform != MFX_PLATFORM_SOFTWARE)
    {
        if (par && IsNeedPartialAcceleration(core, par))
        {
            return MFX_PLATFORM_SOFTWARE;
        }

#if defined (MFX_VA)
        if (MFX_ERR_NONE != core->IsGuidSupported(sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT, par))
        {
            return MFX_PLATFORM_SOFTWARE;
        }
#if defined (MFX_VA_WIN)
        if (MFX_ERR_NONE != CheckDecodeCaps(core, par))
        {
            return MFX_PLATFORM_SOFTWARE;
        }
#endif

        bool needVpp = false;
        if(par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF || 
           par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
        {
            needVpp = true;
        }
#if defined (MFX_VA)
        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));
        memcpy_s(&request.Info, sizeof(mfxFrameInfo), &par->mfx.FrameInfo, sizeof(mfxFrameInfo));

        MFX_JPEG_Utility::AdjustFourCC(&request.Info, &par->mfx, core->GetHWType(), core->GetVAType(), &needVpp);

        if(needVpp)
        {
            if (MFX_ERR_NONE != CheckVPPCaps(core, par))
            {
                return MFX_PLATFORM_SOFTWARE;
            }
        }
#endif
#else
        return MFX_PLATFORM_SOFTWARE;
#endif
    }

    return platform;
}

#if defined (MFX_VA_WIN)
mfxStatus MFX_JPEG_Utility::CheckDecodeCaps(VideoCORE * core, mfxVideoParam * par)
{
    mfxStatus sts = MFX_ERR_NONE;

    UMC::JPEG_DECODING_CAPS info = {0};

    if(MFX_HW_D3D9 == core->GetVAType())
    {
        IDirectXVideoDecoderService *pDecoderService;
        DXVA2_ConfigPictureDecode   *pConfig = NULL;
        UINT cConfigurations = 0;
        
        D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(core, MFXICORED3D_GUID);

        sts = pID3D->GetD3DService(par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height, &pDecoderService, true);
        MFX_CHECK_STS(sts);
        MFX_CHECK(pDecoderService, MFX_ERR_UNSUPPORTED);

        DXVA2_VideoDesc videoDesc = {0};
        videoDesc.SampleWidth  = par->mfx.FrameInfo.Width;
        videoDesc.SampleHeight = par->mfx.FrameInfo.Height;
        videoDesc.Format = D3DFMT_NV12;

        HRESULT hr = pDecoderService->GetDecoderConfigurations(
            sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT,
            &videoDesc,
            NULL,
            &cConfigurations,
            &pConfig );

        if (FAILED(hr))
        {
            return MFX_ERR_UNSUPPORTED;
        }

        if (pConfig)
        {
            info = *((UMC::JPEG_DECODING_CAPS*)pConfig);
            CoTaskMemFree(pConfig);
        }
    }
    else if( MFX_HW_D3D11 == core->GetVAType() )
    {
        CComQIPtr<ID3D11VideoDevice> pD11VideoDevice;
        GUID decoderGuid;
        mfxU32 count;

        D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(core);

        pD11VideoDevice = pD3d11->GetD3D11VideoDevice(true);
        MFX_CHECK(pD11VideoDevice, MFX_ERR_UNSUPPORTED);

        D3D11_VIDEO_DECODER_DESC video_desc = {0};
        D3D11_VIDEO_DECODER_CONFIG video_config = {0};
        video_desc.Guid = sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT;
        video_desc.SampleWidth = par->mfx.FrameInfo.Width;
        video_desc.SampleHeight = par->mfx.FrameInfo.Height;
        video_desc.OutputFormat = (core->GetHWType() == MFX_HW_SOFIA)? DXGI_FORMAT_NV12: DXGI_FORMAT_420_OPAQUE;

        mfxU32 cDecoderProfiles = pD11VideoDevice->GetVideoDecoderProfileCount();

        for (mfxU32 i=0; i < cDecoderProfiles; i++)
        {
            HRESULT hr = pD11VideoDevice->GetVideoDecoderProfile( i, &decoderGuid);
            if (FAILED(hr))
            {
                continue;
            }

            if(decoderGuid == sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT)
            {
                hr = pD11VideoDevice->GetVideoDecoderConfigCount(&video_desc, &count);
                if (SUCCEEDED(hr))
                {
                    for (mfxU32 j=0; j < count; j++)
                    {

                        hr = pD11VideoDevice->GetVideoDecoderConfig(&video_desc, j, &video_config);
                        if (FAILED(hr))
                        {
                            return MFX_ERR_UNSUPPORTED;
                        }

                        info = *((UMC::JPEG_DECODING_CAPS*)&video_config);
                    }
                }
            }
        }
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    if( par->mfx.FrameInfo.Width > info.MaxPicWidth ||
        par->mfx.FrameInfo.Width < info.MinPicWidth ||
        par->mfx.FrameInfo.Height > info.MaxPicHeight ||
        par->mfx.FrameInfo.Height < info.MinPicHeight)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}
#endif

#if defined (MFX_VA)
mfxStatus MFX_JPEG_Utility::CheckVPPCaps(VideoCORE * core, mfxVideoParam * par)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (core->GetHWType() == MFX_HW_SOFIA)
        return MFX_ERR_UNSUPPORTED;

    VideoVppJpegD3D9 *pCc = new VideoVppJpegD3D9(core, false, (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)?true:false);
    
    if(pCc != NULL)
    {
        sts = pCc->Init(par);
        delete pCc;
        pCc = NULL;
    }
    else
    {
        sts = MFX_ERR_MEMORY_ALLOC;
    }

    return sts;
}
#endif

#if defined (MFX_VA)
void MFX_JPEG_Utility::AdjustFrameAllocRequest(mfxFrameAllocRequest *request,
                                               mfxInfoMFX *info,
                                               eMFXHWType hwType, 
                                               eMFXVAType vaType,
                                               bool *needVpp)
{
    *needVpp = false;

    // update request in case of interlaced stream
    if(request->Info.PicStruct == MFX_PICSTRUCT_FIELD_TFF || request->Info.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
    {
        request->Info.Height >>= 1;
        request->Info.CropH >>= 1;
        request->NumFrameMin <<= 1;
        request->NumFrameSuggested <<= 1;
        
        *needVpp = true;
    }

    // set FourCC
    AdjustFourCC(&request->Info, info, hwType, vaType, needVpp);

    // WA for rotation of unaligned images
    mfxU16 mcuWidth;
    mfxU16 mcuHeight;
    mfxU16 paddingWidth;
    mfxU16 paddingHeight;

    switch(info->JPEGChromaFormat)
    {
    case MFX_CHROMAFORMAT_YUV411:
        mcuWidth  = 32;
        mcuHeight = 8;
        break;
    case MFX_CHROMAFORMAT_YUV420:
        mcuWidth  = 16;
        mcuHeight = 16;
        break;
    case MFX_CHROMAFORMAT_YUV422H:
        mcuWidth  = 16;
        mcuHeight = 8;
        break;
    case MFX_CHROMAFORMAT_YUV422V:
        mcuWidth  = 8;
        mcuHeight = 16;
        break;
    case MFX_CHROMAFORMAT_YUV400:
    case MFX_CHROMAFORMAT_YUV444:
    default:
        mcuWidth  = 8;
        mcuHeight = 8;
        break;
    }

#ifdef MFX_ENABLE_MJPEG_ROTATE_VPP
    if(info->Rotation == MFX_ROTATION_90 || info->Rotation == MFX_ROTATION_180 || info->Rotation == MFX_ROTATION_270)
    {
        *needVpp = true;
    }

    if(info->Rotation == MFX_ROTATION_90 || info->Rotation == MFX_ROTATION_270)
    {
        std::swap(request->Info.Height, request->Info.Width);
        std::swap(request->Info.AspectRatioH, request->Info.AspectRatioW);
        std::swap(request->Info.CropH, request->Info.CropW);
        std::swap(request->Info.CropY, request->Info.CropX);
    }
#endif


    if(info->Rotation == MFX_ROTATION_90 || info->Rotation == MFX_ROTATION_270)
        std::swap(mcuWidth, mcuHeight);

    paddingWidth = (mfxU16)((2<<16) - request->Info.CropW) % mcuWidth;
    paddingHeight = (mfxU16)((2<<16) - request->Info.CropH) % mcuHeight;

    switch(info->Rotation)
    {
    case MFX_ROTATION_90:
        request->Info.CropX = paddingWidth;
        request->Info.CropW = request->Info.CropW + paddingWidth;
        break;
    case MFX_ROTATION_180:
        request->Info.CropX = paddingWidth;
        request->Info.CropW = request->Info.CropW + paddingWidth;
        request->Info.CropY = paddingHeight;
        request->Info.CropH = request->Info.CropH + paddingHeight;
        break;
    case MFX_ROTATION_270:
        request->Info.CropY = paddingHeight;
        request->Info.CropH = request->Info.CropH + paddingHeight;
        break;
    }
}
#endif

#if defined (MFX_VA)
void MFX_JPEG_Utility::AdjustFourCC(mfxFrameInfo *requestFrameInfo, mfxInfoMFX *info, eMFXHWType hwType, eMFXVAType vaType, bool *needVpp)
{

    if (info->JPEGColorFormat == MFX_JPEG_COLORFORMAT_UNKNOWN || info->JPEGColorFormat == MFX_JPEG_COLORFORMAT_YCbCr)
    {
        switch(info->JPEGChromaFormat)
        {
#if defined (MFX_VA_WIN)
        case MFX_CHROMAFORMAT_MONOCHROME:
            if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_NV12 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
            }
            else if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_YUY2 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUY2;
            }
            else if (hwType >= MFX_HW_SCL && hwType != MFX_HW_SOFIA &&
                vaType == MFX_HW_D3D9 &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_RGB4 &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_RGB4;
            }
            else if (hwType >= MFX_HW_SCL && hwType != MFX_HW_SOFIA &&
                vaType == MFX_HW_D3D11 &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == DXGI_FORMAT_AYUV &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = DXGI_FORMAT_AYUV;
            }
            else
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUV400;
                *needVpp = true;
            }
#else
            if (requestFrameInfo->FourCC == MFX_FOURCC_RGB4 &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096)
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
                *needVpp = true;
            }
#endif
            break;
        case MFX_CHROMAFORMAT_YUV420:
#if defined (MFX_VA_WIN)
            if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_NV12 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
            }
            else if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_YUY2 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUY2;
            }
            else if (hwType >= MFX_HW_HSW_ULT && hwType != MFX_HW_VLV && hwType != MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_NV12 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
            }
            else if (hwType >= MFX_HW_HSW_ULT && hwType != MFX_HW_VLV && hwType != MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_YUY2 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUY2;
            }
            else if (hwType >= MFX_HW_SCL && hwType != MFX_HW_SOFIA &&
                vaType == MFX_HW_D3D9 &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_RGB4 &&
                requestFrameInfo->CropW  >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW  <= 4096 &&
                requestFrameInfo->CropH <= 4096 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_RGB4;
            }
            else if (hwType >= MFX_HW_SCL && hwType != MFX_HW_SOFIA &&
                vaType == MFX_HW_D3D11 &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == DXGI_FORMAT_AYUV &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = DXGI_FORMAT_AYUV;
            }
            else
            {
                requestFrameInfo->FourCC = MFX_FOURCC_IMC3;
                *needVpp = true;
            }
#else
            if (info->Rotation == MFX_ROTATION_0 &&
                requestFrameInfo->FourCC == MFX_FOURCC_RGB4 &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096)
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
                *needVpp = true;
            }
#endif
            break;
        case MFX_CHROMAFORMAT_YUV411:
#if defined (MFX_VA_WIN)
            if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_NV12 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
            }
            else if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_YUY2 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUY2;
            }
            else
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUV411;
                *needVpp = true;
            }
#endif
            break;
        case MFX_CHROMAFORMAT_YUV422H:
#if defined (MFX_VA_WIN)
            if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_YUY2 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUY2;
            }
            else if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_NV12 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
            }
            else if (hwType >= MFX_HW_HSW_ULT && hwType != MFX_HW_VLV && hwType != MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_NV12 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
            }
            else if (hwType >= MFX_HW_HSW_ULT && hwType != MFX_HW_VLV && hwType != MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_YUY2 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUY2;
            }
            else if (hwType >= MFX_HW_SCL && hwType != MFX_HW_SOFIA &&
                vaType == MFX_HW_D3D9 &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_RGB4 &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_RGB4;
            }
            else if (hwType >= MFX_HW_SCL && hwType != MFX_HW_SOFIA &&
                vaType == MFX_HW_D3D11 &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == DXGI_FORMAT_AYUV &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = DXGI_FORMAT_AYUV;
            }
            else
            {
                if (info->Rotation == MFX_ROTATION_0 || info->Rotation == MFX_ROTATION_180)
                    requestFrameInfo->FourCC = MFX_FOURCC_YUV422H;
                else
                    requestFrameInfo->FourCC = MFX_FOURCC_YUV422V;

                *needVpp = true;
            }
#else
            if (info->Rotation == MFX_ROTATION_0 &&
                requestFrameInfo->FourCC == MFX_FOURCC_RGB4 &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096)
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
                *needVpp = true;
            }
#endif
            break;
        case MFX_CHROMAFORMAT_YUV422V:
#if defined (MFX_VA_WIN)
            if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_YUY2 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUY2;
            }
            else if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_NV12 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
            }
            else if (hwType >= MFX_HW_BDW && hwType != MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_NV12 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
            }
            else
            {
                if (info->Rotation == MFX_ROTATION_0 || info->Rotation == MFX_ROTATION_180)
                    requestFrameInfo->FourCC = MFX_FOURCC_YUV422V;
                else
                    requestFrameInfo->FourCC = MFX_FOURCC_YUV422H;

                *needVpp = true;
            }
#endif
            break;
        case MFX_CHROMAFORMAT_YUV444:
#if defined (MFX_VA_WIN)
            if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_YUY2 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUY2;
            }
            else if (hwType == MFX_HW_SOFIA &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_NV12 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
            }
            else if (hwType >= MFX_HW_SCL && hwType != MFX_HW_SOFIA &&
                vaType == MFX_HW_D3D9 &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_RGB4 &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_RGB4;
            }
            else if (hwType >= MFX_HW_SCL && hwType != MFX_HW_SOFIA &&
                vaType == MFX_HW_D3D11 &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == DXGI_FORMAT_AYUV &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = DXGI_FORMAT_AYUV;
            }
            else
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUV444;
                *needVpp = true;
            }
#else
            if (info->Rotation == MFX_ROTATION_0 &&
                requestFrameInfo->FourCC == MFX_FOURCC_RGB4 &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096)
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
                *needVpp = true;
            }
#endif
            break;
        default:
            VM_ASSERT(false);
            break;
        }
    }
#if defined (MFX_VA_WIN)
    else if(info->JPEGColorFormat == MFX_JPEG_COLORFORMAT_RGB)
    {
        switch(info->JPEGChromaFormat)
        {
        case MFX_CHROMAFORMAT_YUV444:
            if (hwType >= MFX_HW_SCL && hwType != MFX_HW_SOFIA &&
                vaType == MFX_HW_D3D9 &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == MFX_FOURCC_RGB4 &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_RGB4;
            }
            else if (hwType >= MFX_HW_SCL && hwType != MFX_HW_SOFIA &&
                vaType == MFX_HW_D3D11 &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                requestFrameInfo->FourCC == DXGI_FORMAT_AYUV &&
                requestFrameInfo->CropW >= 128 &&
                requestFrameInfo->CropH >= 128 &&
                requestFrameInfo->CropW <= 4096 &&
                requestFrameInfo->CropH <= 4096 &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = DXGI_FORMAT_AYUV;
            }
            else
            {
                requestFrameInfo->FourCC = MFX_FOURCC_RGBP;
                *needVpp = true;
            }
            break;
        default:
            VM_ASSERT(false);
            break;
        }
    }
    else
    {
        VM_ASSERT(false);
    }
#endif
    return;
}
#endif

mfxStatus MFX_JPEG_Utility::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type)
{
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (in == out)
    {
        mfxVideoParam in1;
        memcpy_s(&in1, sizeof(mfxVideoParam), in, sizeof(mfxVideoParam));
        return Query(core, &in1, out, type);
    }

    memset(&out->mfx, 0, sizeof(mfxInfoMFX));

    if (in)
    {
        if (in->mfx.CodecId == MFX_CODEC_JPEG)
            out->mfx.CodecId = in->mfx.CodecId;

        if ((MFX_PROFILE_JPEG_BASELINE == in->mfx.CodecProfile))
            out->mfx.CodecProfile = in->mfx.CodecProfile;

        switch (in->mfx.CodecLevel)
        {
        case MFX_LEVEL_UNKNOWN:
            out->mfx.CodecLevel = in->mfx.CodecLevel;
            break;
        }

        if (in->mfx.NumThread < 128)
            out->mfx.NumThread = in->mfx.NumThread;

        if (in->AsyncDepth < MFX_MAX_ASYNC_DEPTH_VALUE) // Actually AsyncDepth > 5-7 is for debugging only.
            out->AsyncDepth = in->AsyncDepth;

        if ((in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        {
            if ( !((in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) )
                out->IOPattern = in->IOPattern;
        }

        mfxU32 fourCC = in->mfx.FrameInfo.FourCC;
        mfxU16 chromaFormat = in->mfx.FrameInfo.ChromaFormat;

        if ((fourCC == 0 && chromaFormat == 0) ||
            (fourCC == MFX_FOURCC_NV12 && chromaFormat == MFX_CHROMAFORMAT_MONOCHROME) ||
            (fourCC == MFX_FOURCC_NV12 && chromaFormat == MFX_CHROMAFORMAT_YUV420) ||
            (fourCC == MFX_FOURCC_RGB4 && chromaFormat == MFX_CHROMAFORMAT_YUV444) ||
            (fourCC == MFX_FOURCC_YUY2 && chromaFormat == MFX_CHROMAFORMAT_YUV422H))
        {
            out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;
            out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
        }
#if defined (MFX_VA_WIN)
        else if (fourCC == DXGI_FORMAT_AYUV && chromaFormat == MFX_CHROMAFORMAT_YUV444)
        {
            out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;
            out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
        }
#endif
        else
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        out->mfx.FrameInfo.Width = UMC::align_value<mfxU16>(in->mfx.FrameInfo.Width, 0x10);

        out->mfx.FrameInfo.Height = UMC::align_value<mfxU16>(in->mfx.FrameInfo.Height, 
            (in->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 0x10 : 0x20);
        
        if (in->mfx.FrameInfo.CropX <= out->mfx.FrameInfo.Width)
            out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;

        if (in->mfx.FrameInfo.CropY <= out->mfx.FrameInfo.Height)
            out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;

        if (out->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW <= out->mfx.FrameInfo.Width)
            out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;

        if (out->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH <= out->mfx.FrameInfo.Height)
            out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;

        out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
        out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;

        out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
        out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

        switch (in->mfx.FrameInfo.PicStruct)
        {
        case MFX_PICSTRUCT_PROGRESSIVE:
        case MFX_PICSTRUCT_FIELD_TFF:
        case MFX_PICSTRUCT_FIELD_BFF:
        //case MFX_PICSTRUCT_FIELD_REPEATED:
        //case MFX_PICSTRUCT_FRAME_DOUBLING:
        //case MFX_PICSTRUCT_FRAME_TRIPLING:
            out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        switch (in->mfx.JPEGChromaFormat)
        {
        case MFX_CHROMAFORMAT_MONOCHROME:
        case MFX_CHROMAFORMAT_YUV420:
        case MFX_CHROMAFORMAT_YUV422:
        case MFX_CHROMAFORMAT_YUV444:
        case MFX_CHROMAFORMAT_YUV411:
        case MFX_CHROMAFORMAT_YUV422V:
            out->mfx.JPEGChromaFormat = in->mfx.JPEGChromaFormat;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        switch (in->mfx.JPEGColorFormat)
        {
        case MFX_JPEG_COLORFORMAT_UNKNOWN:
        case MFX_JPEG_COLORFORMAT_YCbCr:
        case MFX_JPEG_COLORFORMAT_RGB:
            out->mfx.JPEGColorFormat = in->mfx.JPEGColorFormat;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        switch (in->mfx.InterleavedDec)
        {
        case MFX_SCANTYPE_UNKNOWN:
        case MFX_SCANTYPE_INTERLEAVED:
        case MFX_SCANTYPE_NONINTERLEAVED:
            out->mfx.InterleavedDec = in->mfx.InterleavedDec;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        switch (in->mfx.Rotation)
        {
        case MFX_ROTATION_0:
        case MFX_ROTATION_90:
        case MFX_ROTATION_180:
        case MFX_ROTATION_270:
            out->mfx.Rotation = in->mfx.Rotation;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        if(in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE && 
           in->mfx.Rotation != MFX_ROTATION_0)
            sts = MFX_ERR_UNSUPPORTED;

        mfxStatus stsExt = CheckDecodersExtendedBuffers(in);
        if (stsExt < MFX_ERR_NONE)
            sts = MFX_ERR_UNSUPPORTED;

        if (in->Protected)
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (GetPlatform(core, out) != core->GetPlatformType() && sts == MFX_ERR_NONE)
        {
            VM_ASSERT(GetPlatform(core, out) == MFX_PLATFORM_SOFTWARE);
            sts = MFX_WRN_PARTIAL_ACCELERATION;
        }
    }
    else
    {
        out->mfx.CodecId = MFX_CODEC_JPEG;
        out->mfx.CodecProfile = 1;
        out->mfx.CodecLevel = 1;

        out->mfx.NumThread = 1;

        out->AsyncDepth = 1;

        // mfxFrameInfo
        out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.FrameInfo.Height = 1;

        //out->mfx.FrameInfo.CropX = 1;
        //out->mfx.FrameInfo.CropY = 1;
        //out->mfx.FrameInfo.CropW = 1;
        //out->mfx.FrameInfo.CropH = 1;

        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;

        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;

        out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        out->mfx.JPEGChromaFormat = 1;
        out->mfx.JPEGColorFormat = 1;
        out->mfx.Rotation = 1;

        if (type == MFX_HW_UNKNOWN)
        {
            out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        else
        {
            out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }

        mfxExtOpaqueSurfaceAlloc * opaqueOut = (mfxExtOpaqueSurfaceAlloc *)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        if (opaqueOut)
        {
        }
    }

    return sts;
}

bool MFX_JPEG_Utility::CheckVideoParam(mfxVideoParam *in, eMFXHWType )
{
    if (!in)
        return false;

    if (in->Protected)
       return false;

    if (MFX_CODEC_JPEG != in->mfx.CodecId)
        return false;

    if (in->mfx.FrameInfo.Width % 16)
        return false;

    if (in->mfx.FrameInfo.Height % 16)
        return false;

#if 0
    // ignore Crop parameters at Init/Reset stage
    if (in->mfx.FrameInfo.CropX > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY > in->mfx.FrameInfo.Height)
        return false;

    if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height)
        return false;
#endif

    // both zero or not zero
    if ((in->mfx.FrameInfo.AspectRatioW || in->mfx.FrameInfo.AspectRatioH) && !(in->mfx.FrameInfo.AspectRatioW && in->mfx.FrameInfo.AspectRatioH))
        return false;

    switch (in->mfx.FrameInfo.PicStruct)
    {
    case MFX_PICSTRUCT_PROGRESSIVE:
    case MFX_PICSTRUCT_FIELD_TFF:
    case MFX_PICSTRUCT_FIELD_BFF:
        break;
    default:
        return false;
    }

    if(in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE && 
       in->mfx.Rotation != MFX_ROTATION_0)
        return false;

    mfxU32 fourCC = in->mfx.FrameInfo.FourCC;
    mfxU16 chromaFormat = in->mfx.FrameInfo.ChromaFormat;

#if defined (MFX_VA_WIN)
    if ((fourCC != MFX_FOURCC_NV12 || chromaFormat != MFX_CHROMAFORMAT_MONOCHROME) &&
        (fourCC != MFX_FOURCC_NV12 || chromaFormat != MFX_CHROMAFORMAT_YUV420) &&
        (fourCC != MFX_FOURCC_RGB4 || chromaFormat != MFX_CHROMAFORMAT_YUV444) &&
        (fourCC != DXGI_FORMAT_AYUV || chromaFormat != MFX_CHROMAFORMAT_YUV444) &&
        (fourCC != MFX_FOURCC_YUY2 || chromaFormat != MFX_CHROMAFORMAT_YUV422H))
        return false;
#elif defined (MFX_VA_LINUX)
    if ((fourCC != MFX_FOURCC_NV12 || chromaFormat != MFX_CHROMAFORMAT_MONOCHROME) &&
        (fourCC != MFX_FOURCC_NV12 || chromaFormat != MFX_CHROMAFORMAT_YUV420) &&
        (fourCC != MFX_FOURCC_RGB4 || chromaFormat != MFX_CHROMAFORMAT_YUV444) &&
        (fourCC != MFX_FOURCC_YUY2 || chromaFormat != MFX_CHROMAFORMAT_YUV422H))
           return false;
#else
    if ((fourCC != MFX_FOURCC_NV12 || chromaFormat != MFX_CHROMAFORMAT_MONOCHROME) &&
        (fourCC != MFX_FOURCC_NV12 || chromaFormat != MFX_CHROMAFORMAT_YUV420) &&
        (fourCC != MFX_FOURCC_RGB4 || chromaFormat != MFX_CHROMAFORMAT_YUV444) &&
        (fourCC != MFX_FOURCC_YUY2 || chromaFormat != MFX_CHROMAFORMAT_YUV422H))
        return false;

#endif

    if (!(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && !(in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        return false;

    return true;
}

mfxStatus VideoDECODEMJPEG::UpdateAllocRequest(mfxVideoParam *par,
                                                mfxFrameAllocRequest *request,
                                                mfxExtOpaqueSurfaceAlloc * &pOpaqAlloc,
                                                bool &mapping)
{
    mapping = false;
    if (!(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return MFX_ERR_NONE;

    m_isOpaq = true;

    pOpaqAlloc = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
    if (!pOpaqAlloc)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (request->NumFrameMin > pOpaqAlloc->Out.NumSurface)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_FROM_DECODE;
    switch (pOpaqAlloc->Out.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET))
    {
    case MFX_MEMTYPE_SYSTEM_MEMORY:
        request->Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
        break;
    case MFX_MEMTYPE_DXVA2_DECODER_TARGET:
        request->Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        break;
    case MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET:
        request->Type |= MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
        break;
    default:
        return MFX_ERR_INVALID_VIDEO_PARAM;        
    }
    request->NumFrameMin = request->NumFrameSuggested = pOpaqAlloc->Out.NumSurface;
    mapping = true;
    return MFX_ERR_NONE;
}

mfxFrameSurface1 *VideoDECODEMJPEG::GetOriginalSurface(mfxFrameSurface1 *surface)
{
    if (m_isOpaq)
        return m_core->GetNativeSurface(surface);
    return surface;
}
#endif // MFX_ENABLE_MJPEG_VIDEO_DECODE
