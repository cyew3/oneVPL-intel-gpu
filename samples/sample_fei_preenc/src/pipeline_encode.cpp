//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
//


#include "pipeline_encode.h"
#include "sysmem_allocator.h"

#if D3D_SURFACES_SUPPORT
#include "d3d_allocator.h"
#include "d3d11_allocator.h"

#include "d3d_device.h"
#include "d3d11_device.h"
#endif

#ifdef LIBVA_SUPPORT
#include "vaapi_allocator.h"
#include "vaapi_device.h"
#endif

#if _DEBUG
#define mdprintf fprintf
#else
#define mdprintf(...)
#endif

static FILE* mbstatout=NULL;
static FILE* mvout = NULL;

CEncTaskPool::CEncTaskPool() {
    m_pTasks = NULL;
    m_pmfxSession = NULL;
    m_nTaskBufferStart = 0;
    m_nPoolSize = 0;
}

CEncTaskPool::~CEncTaskPool() {
    Close();
}

mfxStatus CEncTaskPool::Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter) {
    MSDK_CHECK_POINTER(pmfxSession, MFX_ERR_NULL_PTR);

    MSDK_CHECK_ERROR(nPoolSize, 0, MFX_ERR_UNDEFINED_BEHAVIOR);
    MSDK_CHECK_ERROR(nBufferSize, 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    // nPoolSize must be even in case of 2 output bitstreams
    if (pOtherWriter && (0 != nPoolSize % 2))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    m_pmfxSession = pmfxSession;
    m_nPoolSize = nPoolSize;

    m_pTasks = new sTask [m_nPoolSize];
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_MEMORY_ALLOC);

    mfxStatus sts = MFX_ERR_NONE;

    if (pOtherWriter) // 2 bitstreams on output
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i += 2) {
            sts = m_pTasks[i + 0].Init(nBufferSize, pWriter);
            sts = m_pTasks[i + 1].Init(nBufferSize, pOtherWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    } else {
        for (mfxU32 i = 0; i < m_nPoolSize; i++) {
            sts = m_pTasks[i].Init(nBufferSize, pWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncTaskPool::SynchronizeFirstTask() {
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(m_pmfxSession, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    // non-null sync point indicates that task is in execution
    if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP) {
        sts = m_pmfxSession->SyncOperation(m_pTasks[m_nTaskBufferStart].EncSyncP, MSDK_WAIT_INTERVAL);

        if (MFX_ERR_NONE == sts) {
            sts = m_pTasks[m_nTaskBufferStart].WriteBitstream();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            sts = m_pTasks[m_nTaskBufferStart].Reset();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // move task buffer start to the next executing task
            // the first transform frame to the right with non zero sync point
            for (mfxU32 i = 0; i < m_nPoolSize; i++) {
                m_nTaskBufferStart = (m_nTaskBufferStart + 1) % m_nPoolSize;
                if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP) {
                    break;
                }
            }
        }

        return sts;
    } else {
        return MFX_ERR_NOT_FOUND; // no tasks left in task buffer
    }
}

mfxU32 CEncTaskPool::GetFreeTaskIndex() {
    mfxU32 off = 0;

    if (m_pTasks) {
        for (off = 0; off < m_nPoolSize; off++) {
            if (NULL == m_pTasks[(m_nTaskBufferStart + off) % m_nPoolSize].EncSyncP) {
                break;
            }
        }
    }

    if (off >= m_nPoolSize)
        return m_nPoolSize;

    return (m_nTaskBufferStart + off) % m_nPoolSize;
}

mfxStatus CEncTaskPool::GetFreeTask(sTask **ppTask) {
    MSDK_CHECK_POINTER(ppTask, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);

    mfxU32 index = GetFreeTaskIndex();

    if (index >= m_nPoolSize) {
        return MFX_ERR_NOT_FOUND;
    }

    // return the address of the task
    *ppTask = &m_pTasks[index];

    return MFX_ERR_NONE;
}

void CEncTaskPool::Close() {
    if (m_pTasks) {
        for (mfxU32 i = 0; i < m_nPoolSize; i++) {
            m_pTasks[i].Close();
        }
    }

    MSDK_SAFE_DELETE_ARRAY(m_pTasks);

    m_pmfxSession = NULL;
    m_nTaskBufferStart = 0;
    m_nPoolSize = 0;
}

sTask::sTask()
: EncSyncP(0)
, pWriter(NULL)
{
    //    MSDK_ZERO_MEMORY(mfxBS);
}

mfxStatus sTask::Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pwriter) {
    Close();

    pWriter = pwriter;

    mfxStatus sts = Reset();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //    sts = InitMfxBitstream(&mfxBS, nBufferSize);
    //    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(&mfxBS));

    return sts;
}

mfxStatus sTask::Close() {
    //    WipeMfxBitstream(&mfxBS);
    EncSyncP = 0;

    return MFX_ERR_NONE;
}

mfxStatus sTask::WriteBitstream() {
    mfxStatus sts = MFX_ERR_NONE;
    if (!pWriter)
        sts = MFX_ERR_NOT_INITIALIZED;
#if 0
    sts = pWriter->WriteNextFrame(&mfxBS);
#endif

    return sts;
}

mfxStatus sTask::Reset() {
    // mark sync point as free
    EncSyncP = NULL;

    // prepare bit stream
    //    mfxBS.DataOffset = 0;
    //    mfxBS.DataLength = 0;

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitMfxEncParams(sInputParams *pInParams) {
    m_mfxEncParams.mfx.CodecId = pInParams->CodecId;
    m_mfxEncParams.mfx.TargetUsage = 0;
    m_mfxEncParams.mfx.TargetKbps = 0;
    m_mfxEncParams.mfx.RateControlMethod = 0;
    m_mfxEncParams.mfx.EncodedOrder = 0; // binary flag, 0 signals encoder to take frames in display order

    // specify memory type
    if (D3D9_MEMORY == pInParams->memType || D3D11_MEMORY == pInParams->memType) {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    } else {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }

    // frame info parameters
    m_mfxEncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    m_mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_mfxEncParams.mfx.FrameInfo.PicStruct = pInParams->nPicStruct;

    // set frame size and crops
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxEncParams.mfx.FrameInfo.Width = MSDK_ALIGN16(pInParams->nWidth);
    m_mfxEncParams.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxEncParams.mfx.FrameInfo.PicStruct) ?
            MSDK_ALIGN16(pInParams->nHeight) : MSDK_ALIGN32(pInParams->nHeight);

    m_mfxEncParams.mfx.FrameInfo.CropX = 0;
    m_mfxEncParams.mfx.FrameInfo.CropY = 0;
    m_mfxEncParams.mfx.FrameInfo.CropW = pInParams->nWidth;
    m_mfxEncParams.mfx.FrameInfo.CropH = pInParams->nHeight;
    m_mfxEncParams.AsyncDepth = m_nAsyncDepth;
    m_mfxEncParams.mfx.GopRefDist = m_refDist;

#if 0
    if (!m_EncExtParams.empty()) {
        m_mfxEncParams.ExtParam = &m_EncExtParams[0]; // vector is stored linearly in memory
        m_mfxEncParams.NumExtParam = (mfxU16) m_EncExtParams.size();
    }
#endif

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::CreateHWDevice() {
    mfxStatus sts = MFX_ERR_NONE;
#if D3D_SURFACES_SUPPORT
    POINT point = {0, 0};
    HWND window = WindowFromPoint(point);

#if MFX_D3D11_SUPPORT
    if (D3D11_MEMORY == m_memType)
        m_hwdev = new CD3D11Device();
    else
#endif // #if MFX_D3D11_SUPPORT
        m_hwdev = new CD3D9Device();

    if (NULL == m_hwdev)
        return MFX_ERR_MEMORY_ALLOC;

    sts = m_hwdev->Init(
            window,
            0,
            MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

#elif LIBVA_SUPPORT
    m_hwdev = CreateVAAPIDevice();
    if (NULL == m_hwdev) {
        return MFX_ERR_MEMORY_ALLOC;
    }
    sts = m_hwdev->Init(NULL, 0, MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#endif
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::ResetDevice() {
    if (D3D9_MEMORY == m_memType || D3D11_MEMORY == m_memType) {
        return m_hwdev->Reset();
    }
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocFrames() {
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest EncRequest;

    mfxU16 nEncSurfNum = 0; // number of surfaces for encoder

    MSDK_ZERO_MEMORY(EncRequest);

    //Calculate the number of surfaces for components.
    //Pre allocate all required surfaces in advance
    nEncSurfNum = m_refDist * 2;

    // prepare allocation requests
    EncRequest.NumFrameMin = nEncSurfNum;
    EncRequest.NumFrameSuggested = nEncSurfNum;
    EncRequest.Info.Width = m_mfxEncParams.mfx.FrameInfo.Width;
    EncRequest.Info.Height = m_mfxEncParams.mfx.FrameInfo.Height;
    EncRequest.Info.CropW = m_mfxEncParams.mfx.FrameInfo.Width;
    EncRequest.Info.CropH = m_mfxEncParams.mfx.FrameInfo.Height;
    EncRequest.Info.FourCC = MFX_FOURCC_NV12;

//    EncRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_ENCODE;
    EncRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
    // add info about memory type to request^M
    EncRequest.Type |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    MSDK_MEMCPY_VAR(EncRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof (mfxFrameInfo));

    // alloc frames for encoder
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &EncRequest, &m_EncResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // prepare mfxFrameSurface1 array for encoder
    m_pEncSurfaces = new mfxFrameSurface1 [m_EncResponse.NumFrameActual];
    MSDK_CHECK_POINTER(m_pEncSurfaces, MFX_ERR_MEMORY_ALLOC);

    for (int i = 0; i < m_EncResponse.NumFrameActual; i++) {
        MSDK_ZERO_MEMORY(m_pEncSurfaces[i]);
        MSDK_MEMCPY_VAR(m_pEncSurfaces[i].Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof (mfxFrameInfo));

        if (m_bExternalAlloc) {
            m_pEncSurfaces[i].Data.MemId = m_EncResponse.mids[i];
        } else {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_EncResponse.mids[i], &(m_pEncSurfaces[i].Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::CreateAllocator() {
    mfxStatus sts = MFX_ERR_NONE;

    if (D3D9_MEMORY == m_memType || D3D11_MEMORY == m_memType) {
#if D3D_SURFACES_SUPPORT
        sts = CreateHWDevice();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        mfxHDL hdl = NULL;
        mfxHandleType hdl_t =
#if MFX_D3D11_SUPPORT
                D3D11_MEMORY == m_memType ? MFX_HANDLE_D3D11_DEVICE :
#endif // #if MFX_D3D11_SUPPORT
                MFX_HANDLE_D3D9_DEVICE_MANAGER;

        sts = m_hwdev->GetHandle(hdl_t, &hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // handle is needed for HW library only
        mfxIMPL impl = 0;
        m_mfxSession.QueryIMPL(&impl);
        if (impl != MFX_IMPL_SOFTWARE) {
            sts = m_mfxSession.SetHandle(hdl_t, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        // create D3D allocator
#if MFX_D3D11_SUPPORT
        if (D3D11_MEMORY == m_memType) {
            m_pMFXAllocator = new D3D11FrameAllocator;
            MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

            D3D11AllocatorParams *pd3dAllocParams = new D3D11AllocatorParams;
            MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
            pd3dAllocParams->pDevice = reinterpret_cast<ID3D11Device *> (hdl);

            m_pmfxAllocatorParams = pd3dAllocParams;
        } else
#endif // #if MFX_D3D11_SUPPORT
        {
            m_pMFXAllocator = new D3DFrameAllocator;
            MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

            D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;
            MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
            pd3dAllocParams->pManager = reinterpret_cast<IDirect3DDeviceManager9 *> (hdl);

            m_pmfxAllocatorParams = pd3dAllocParams;
        }

        /* In case of video memory we must provide MediaSDK with external allocator
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to Media SDK */
        sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        m_bExternalAlloc = true;
#endif
#ifdef LIBVA_SUPPORT
        sts = CreateHWDevice();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        /* It's possible to skip failed result here and switch to SW implementation,
        but we don't process this way */

        mfxHDL hdl = NULL;
        sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
        // provide device manager to MediaSDK
        sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // create VAAPI allocator
        m_pMFXAllocator = new vaapiFrameAllocator;
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        vaapiAllocatorParams *p_vaapiAllocParams = new vaapiAllocatorParams;
        MSDK_CHECK_POINTER(p_vaapiAllocParams, MFX_ERR_MEMORY_ALLOC);

        p_vaapiAllocParams->m_dpy = (VADisplay) hdl;
        m_pmfxAllocatorParams = p_vaapiAllocParams;

        /* In case of video memory we must provide MediaSDK with external allocator
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to mediasdk */
        sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        m_bExternalAlloc = true;
#endif
    } else {
#ifdef LIBVA_SUPPORT
        //in case of system memory allocator we also have to pass MFX_HANDLE_VA_DISPLAY to HW library
        mfxIMPL impl;
        m_mfxSession.QueryIMPL(&impl);

        if (MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(impl)) {
            sts = CreateHWDevice();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            mfxHDL hdl = NULL;
            sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
            // provide device manager to MediaSDK
            sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
#endif

        // create system memory allocator
        m_pMFXAllocator = new SysMemFrameAllocator;
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        /* In case of system memory we demonstrate "no external allocator" usage model.
        We don't call SetAllocator, Media SDK uses internal allocator.
        We use system memory allocator simply as a memory manager for application*/
    }

    // initialize memory allocator
    sts = m_pMFXAllocator->Init(m_pmfxAllocatorParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

void CEncodingPipeline::AllocMVPredictors()
{

}

void CEncodingPipeline::DeleteFrames() {
    // delete surfaces array
    MSDK_SAFE_DELETE_ARRAY(m_pEncSurfaces);

    // delete frames
    if (m_pMFXAllocator) {
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_EncResponse);
    }
}

void CEncodingPipeline::DeleteHWDevice() {
    MSDK_SAFE_DELETE(m_hwdev);
}

void CEncodingPipeline::DeleteAllocator() {
    // delete allocator
    MSDK_SAFE_DELETE(m_pMFXAllocator);
    MSDK_SAFE_DELETE(m_pmfxAllocatorParams);

    DeleteHWDevice();
}

CEncodingPipeline::CEncodingPipeline() {
    m_pmfxENC = NULL;
    m_pMFXAllocator = NULL;
    m_pmfxAllocatorParams = NULL;
    m_memType = D3D9_MEMORY;
    m_bExternalAlloc = true;
    m_pEncSurfaces = NULL;
    m_nAsyncDepth = 0;

    MSDK_ZERO_MEMORY(m_CodingOption);
    m_CodingOption.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    m_CodingOption.Header.BufferSz = sizeof (m_CodingOption);

    MSDK_ZERO_MEMORY(m_CodingOption2);
    m_CodingOption2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    m_CodingOption2.Header.BufferSz = sizeof (m_CodingOption2);

#if D3D_SURFACES_SUPPORT
    m_hwdev = NULL;
#endif

    MSDK_ZERO_MEMORY(m_mfxEncParams);

    MSDK_ZERO_MEMORY(m_EncResponse);
}

CEncodingPipeline::~CEncodingPipeline() {
    Close();
}

mfxStatus CEncodingPipeline::InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename) {
    MSDK_CHECK_ERROR(ppWriter, NULL, MFX_ERR_NULL_PTR);

    MSDK_SAFE_DELETE(*ppWriter);
    *ppWriter = new CSmplBitstreamWriter;
    MSDK_CHECK_POINTER(*ppWriter, MFX_ERR_MEMORY_ALLOC);
    mfxStatus sts = (*ppWriter)->Init(filename);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus CEncodingPipeline::InitFileWriters(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    return sts;
}

mfxStatus CEncodingPipeline::Init(sInputParams *pParams) {
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    m_params = *pParams;

    m_nAsyncDepth = 4; // this number can be tuned for better performance
    m_refDist = pParams->refDist > 0 ? pParams->refDist : 1;
    m_gopSize = pParams->gopSize > 0 ? pParams->gopSize : 1;

    mfxStatus sts = MFX_ERR_NONE;

    // prepare input file reader
    sts = m_FileReader.Init(pParams->strSrcFile,
            pParams->ColorFormat,
            1 /* number of views */,
            pParams->srcFileBuff);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitFileWriters(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    APIChangeFeatures features = {};
    features.LookAheadBRC = (pParams->bLABRC || pParams->nLADepth);
    mfxVersion version = getMinimalRequiredVersion(features);

    // Init session
    if (!pParams->useSw) {
        // try searching on all display adapters
        mfxIMPL impl = MFX_IMPL_HARDWARE_ANY;

        // if d3d11 surfaces are used ask the library to run acceleration through D3D11
        // feature may be unsupported due to OS or MSDK API version
        if (D3D11_MEMORY == pParams->memType)
            impl |= MFX_IMPL_VIA_D3D11;

        sts = m_mfxSession.Init(impl, &version);

        // MSDK API version may not support multiple adapters - then try initialize on the default
        if (MFX_ERR_NONE != sts)
            sts = m_mfxSession.Init(impl & (!MFX_IMPL_HARDWARE_ANY | MFX_IMPL_HARDWARE), &version);
    } else
        sts = m_mfxSession.Init(MFX_IMPL_SOFTWARE, &version);

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // set memory type
    m_memType = pParams->memType;

    // create and init frame allocator
    sts = CreateAllocator();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitMfxEncParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // create encoder
    m_pmfxENC = new MFXVideoENC(m_mfxSession);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);

    sts = ResetMFXComponents(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

void CEncodingPipeline::Close() {
    if (m_FileWriters.first)
        msdk_printf(MSDK_STRING("Written %u number of frames\r"), m_FileWriters.first->m_nProcessedFramesNum);

    MSDK_SAFE_DELETE(m_pmfxENC);

    DeleteFrames();
    // allocator if used as external for MediaSDK must be deleted after SDK components
    DeleteAllocator();

    m_TaskPool.Close();
    m_mfxSession.Close();

    m_FileReader.Close();
    FreeFileWriters();
}

void CEncodingPipeline::FreeFileWriters() {
    if (m_FileWriters.second == m_FileWriters.first) {
        m_FileWriters.second = NULL; // second do not own the writer - just forget pointer
    }

    if (m_FileWriters.first)
        m_FileWriters.first->Close();
    MSDK_SAFE_DELETE(m_FileWriters.first);

    if (m_FileWriters.second)
        m_FileWriters.second->Close();
    MSDK_SAFE_DELETE(m_FileWriters.second);
}

mfxStatus CEncodingPipeline::ResetMFXComponents(sInputParams* pParams) {
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    sts = m_pmfxENC->Close();
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // free allocated frames
    DeleteFrames();

    m_TaskPool.Close();

    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //Create extended buffer to Init FEI
    mfxExtFeiParam extInit;
    MSDK_ZERO_MEMORY(extInit);
    extInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
    extInit.Header.BufferSz = sizeof (mfxExtFeiParam);
    extInit.Func = MFX_FEI_FUNCTION_PREENC;

    mfxExtBuffer * encExtParams[1];
    encExtParams[0] = (mfxExtBuffer*) & extInit;

    m_mfxEncParams.ExtParam = &encExtParams[0];
    m_mfxEncParams.NumExtParam = 1;

    sts = m_pmfxENC->Init(&m_mfxEncParams);
    if (MFX_WRN_PARTIAL_ACCELERATION == sts) {
        msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    }
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxU32 nEncodedDataBufferSize = m_mfxEncParams.mfx.FrameInfo.Width * m_mfxEncParams.mfx.FrameInfo.Height * 4;
    sts = m_TaskPool.Init(&m_mfxSession, m_FileWriters.first, m_nAsyncDepth * 2, nEncodedDataBufferSize, m_FileWriters.second);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocateSufficientBuffer(mfxBitstream* pBS) {
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    //    mfxStatus sts = m_pmfxENC->GetVideoParam(&par);
    //    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // reallocate bigger buffer for output
    //    sts = ExtendMfxBitstream(pBS, par.mfx.BufferSizeInKB * 1000);
    //    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(pBS));

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::GetFreeTask(sTask **ppTask) {
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_TaskPool.GetFreeTask(ppTask);
    if (MFX_ERR_NOT_FOUND == sts) {
        sts = SynchronizeFirstTask();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // try again
        sts = m_TaskPool.GetFreeTask(ppTask);
    }

    return sts;
}

mfxU16 CEncodingPipeline::GetFrameType(mfxU32 pos) {
    if ((pos % m_gopSize) == 0) {
        return MFX_FRAMETYPE_I;
    } else if ((pos % m_gopSize % m_refDist) == 0) {
        return MFX_FRAMETYPE_P;
    } else
        return MFX_FRAMETYPE_B;
}

mfxStatus CEncodingPipeline::SynchronizeFirstTask() {
    mfxStatus sts = m_TaskPool.SynchronizeFirstTask();

    return sts;
}

mfxStatus CEncodingPipeline::Run() {
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    mfxFrameSurface1* pSurf = NULL; // dispatching pointer

    sTask *pCurrentTask = NULL; // a pointer to the current task
    mfxU16 nEncSurfIdx = 0; // index of free surface for encoder input

    std::list<iTask*> inputTasks;

    sts = MFX_ERR_NONE;

    mfxI32 heightMB = ((m_params.nHeight + 15) & ~15);
    mfxI32 widthMB = ((m_params.nWidth + 15) & ~15);
    mfxI32 numMB = heightMB * widthMB / 256;

    //setup control structures
    bool disableMVoutput = m_params.dstMVFileBuff == NULL;
    bool disableMBoutput = m_params.dstMBFileBuff == NULL;
    bool enableMVpredictor = m_params.inMVPredFile != NULL;
    bool enableMBQP = m_params.inQPFile != NULL;

    mfxExtFeiPreEncCtrl preENCCtr;
    MSDK_ZERO_MEMORY(preENCCtr);
    preENCCtr.Header.BufferId = MFX_EXTBUFF_FEI_PREENC_CTRL;
    preENCCtr.Header.BufferSz = sizeof (mfxExtFeiPreEncCtrl);
    preENCCtr.DisableMVOutput = disableMVoutput;
    preENCCtr.DisableStatisticsOutput = disableMBoutput;
    preENCCtr.FTEnable = 0;
    preENCCtr.AdaptiveSearch = 1;
    preENCCtr.LenSP = 57;
    preENCCtr.MBQp = enableMBQP;
    preENCCtr.MVPredictor = enableMVpredictor;
    preENCCtr.RefHeight = 40;
    preENCCtr.RefWidth = 48;
    preENCCtr.SubPelMode = 3;
    preENCCtr.SearchWindow = 0;
    preENCCtr.MaxLenSP = 57;
    preENCCtr.Qp = 27;
    preENCCtr.InterSAD = 2;
    preENCCtr.IntraSAD = 2;
    preENCCtr.SubMBPartMask = 0x77;

    int numExtInParams = 0;
    mfxExtBuffer * inBufs[3];
    inBufs[numExtInParams++] = (mfxExtBuffer*) &preENCCtr;

    //we have to allocate in buffers
    mfxExtFeiPreEncMVPredictors mvPreds;
    if(preENCCtr.MVPredictor)
    {
        preENCCtr.MVPredictor = 0;
        mvPreds.Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV_PRED;
        mvPreds.Header.BufferSz = sizeof (mfxExtFeiPreEncMVPredictors);
        mvPreds.NumMBAlloc = numMB;
        mvPreds.MB = new mfxExtFeiPreEncMVPredictors::mfxMB [numMB];
        inBufs[numExtInParams++] = (mfxExtBuffer*) &mvPreds;
        //read mvs from file
        FILE* fmv = fopen(m_params.inMVPredFile,"rb");
        if(fmv == NULL)
        {
            fprintf(stderr,"Can't open file %s\n", m_params.inQPFile);
            exit(-1);
        }

        size_t fsize = fseek(fmv, 0, SEEK_END);

        fclose(fmv);
    }

    mfxExtFeiEncQP qps;
    if(preENCCtr.MBQp)
    {
        qps.Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
        qps.Header.BufferSz = sizeof (mfxExtFeiEncQP);
        qps.NumQPAlloc = numMB;
        // TODO: to look later on it
        qps.QP = (mfxU8*) new mfxU16[numMB];
        inBufs[numExtInParams++] = (mfxExtBuffer*) &qps;
        //read mvs from file
        FILE* fqp = fopen(m_params.inQPFile,"rb");
        if(fqp == NULL)
        {
            fprintf(stderr,"Can't open file %s\n", m_params.inQPFile);
            exit(-1);
        }

        //size_t fsize = fseek(fqp, 0, SEEK_END);
        fseek(fqp, 0, SEEK_SET);
        fread(qps.QP, sizeof(mfxU16)*numMB, 1, fqp);
        fclose(fqp);
    }


    //we have to allocate output buffers
    int numExtOutParams = 0;
    mfxExtBuffer * outBufs[2];
    mfxExtBuffer * outBufsI[1];
    mfxExtFeiPreEncMV mvs;
    MSDK_ZERO_MEMORY(mvs);
    if(!preENCCtr.DisableMVOutput)
    {
        mvs.Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV;
        mvs.Header.BufferSz = sizeof (mfxExtFeiPreEncMV);
        mvs.NumMBAlloc = numMB;
        mvs.MB = new mfxExtFeiPreEncMV::mfxMB [numMB];
        outBufs[numExtOutParams] = (mfxExtBuffer*) &mvs;
        numExtOutParams++;

        printf("Use MV output file: %s\n", m_params.dstMVFileBuff);
        mvout = fopen(m_params.dstMVFileBuff, "wb");
        if (mvout == NULL) {
            fprintf(stderr, "Can't open file %s\n", m_params.dstMVFileBuff);
            exit(-1);
        }
    }

    mfxExtFeiPreEncMBStat mbdata;
    MSDK_ZERO_MEMORY(mbdata);
    if(!preENCCtr.DisableStatisticsOutput)
    {
        mbdata.Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MB;
        mbdata.Header.BufferSz = sizeof (mfxExtFeiPreEncMBStat);
        mbdata.NumMBAlloc = numMB;
        mbdata.MB = new mfxExtFeiPreEncMBStat::mfxMB [numMB];
        outBufs[numExtOutParams] = (mfxExtBuffer*) &mbdata;
        outBufsI[0] = (mfxExtBuffer*) &mbdata; //special case for I frames
        numExtOutParams++;

        printf("Use MB distortion output file: %s\n", m_params.dstMBFileBuff);
        mbstatout = fopen(m_params.dstMBFileBuff, "wb");
        if (mbstatout == NULL) {
            fprintf(stderr, "Can't open file %s\n", m_params.dstMBFileBuff);
            exit(-1);
        }
    }

    // main loop, preprocessing and encoding
    mfxI32 frameCount = 0;
    while (MFX_ERR_NONE <= sts) {
        if(m_params.nNumFrames && frameCount >= m_params.nNumFrames)
            break;
        // get a pointer to a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);

        iTask* eTask = new iTask;

        eTask->frameType = GetFrameType(frameCount);
        mdprintf(stderr,"frame: %d  t:%d pt=%x ", frameCount, eTask->frameType, eTask);

        // find free surface for encoder input
        nEncSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
        MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);

        // point pSurf to encoder surface
        pSurf = &m_pEncSurfaces[nEncSurfIdx];
        // load frame from file to surface data
        // if we share allocator with Media SDK we need to call Lock to access surface data and...
        if (m_bExternalAlloc) {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
            MSDK_BREAK_ON_ERROR(sts);
        }

        pSurf->Info.FrameId.ViewId = 0;
        sts = m_FileReader.LoadNextFrame(pSurf);
        MSDK_BREAK_ON_ERROR(sts);

        // ... after we're done call Unlock
        if (m_bExternalAlloc) {
            sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
            MSDK_BREAK_ON_ERROR(sts);
        }

        pSurf->Data.Locked = 1;
        eTask->in.InSurface = pSurf;
        eTask->frameDisplayOrder = frameCount;
        eTask->encoded = 0;

        inputTasks.push_back(eTask);  //inputTasks in display order
        frameCount++;

        // at this point surface for encoder contains a frame from file
        iTask* prevTask = NULL;
        iTask* nextTask = NULL;
        //find task to encode
        std::list<iTask*>::iterator encTask = inputTasks.begin();
        mdprintf(stderr,"[");
        for(; encTask != inputTasks.end(); ++encTask )
        {
            mdprintf(stderr,"%d ", (*encTask)->frameType);
        }
        mdprintf(stderr,"] ");
        encTask = inputTasks.begin();
        for(; encTask != inputTasks.end(); ++encTask )
        {
            if((*encTask)->encoded) continue;
            prevTask = NULL;
            nextTask = NULL;

            if((*encTask)->frameType & MFX_FRAMETYPE_I)
            {
                break;
            }else if((*encTask)->frameType & MFX_FRAMETYPE_P)
            { //find previous ref
                std::list<iTask*>::iterator it = encTask;
                while((it--) != inputTasks.begin())
                {
                    iTask* curTask = *it;
                    if(curTask->frameType & (MFX_FRAMETYPE_P | MFX_FRAMETYPE_I) &&
                            curTask->encoded){
                        prevTask = curTask;
                        break;
                    }
                }
                if(prevTask) break;
            }else if((*encTask)->frameType & MFX_FRAMETYPE_B)
            {//find previous ref
                std::list<iTask*>::iterator it = encTask;
                while((it--) != inputTasks.begin())
                {
                    iTask* curTask = *it;
                    if(curTask->frameType & (MFX_FRAMETYPE_P | MFX_FRAMETYPE_I) &&
                            curTask->encoded){
                        prevTask = curTask;
                        break;
                    }
                }
                mdprintf(stderr,"pT=%x ", prevTask);

              //find next ref
                it = encTask;
                while((++it) != inputTasks.end())
                {
                    iTask* curTask = *it;
                    if(curTask->frameType & (MFX_FRAMETYPE_P | MFX_FRAMETYPE_I) &&
                            curTask->encoded){
                        nextTask = curTask;
                        break;
                    }
                }
                mdprintf(stderr,"nT=%x ", nextTask);
                if(prevTask && nextTask) break;
            }
        }

        mdprintf(stderr,"eT= %x in_size=%d prevTask=%x nextTask=%x ", encTask, inputTasks.size(),prevTask, nextTask);
        if(encTask == inputTasks.end())
        {
            mdprintf(stderr,"\n");
            continue; //skip B without refs
        }

        eTask = *encTask;
        eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;

        preENCCtr.DisableMVOutput = m_params.dstMVFileBuff == NULL;
        preENCCtr.DisableStatisticsOutput = m_params.dstMBFileBuff == NULL;

        switch (eTask->frameType) {
            case MFX_FRAMETYPE_I:
                eTask->in.NumFrameL0 = 0;//1;  //just workaround to support I frames
                eTask->in.NumFrameL1 = 0;
                eTask->in.L0Surface = NULL;//eTask->in.InSurface;
                //in data
                eTask->in.NumExtParam = numExtInParams;
                eTask->in.ExtParam = inBufs;
                //out data
                //exclude MV output from output buffers
                if(preENCCtr.DisableStatisticsOutput)
                {
                        eTask->out.NumExtParam = 0;
                        eTask->out.ExtParam = NULL;
                }else{
                        eTask->out.NumExtParam = 1;
                        eTask->out.ExtParam = outBufsI;
                }

                preENCCtr.DisableMVOutput = 1;
                break;
            case MFX_FRAMETYPE_P:
                eTask->in.NumFrameL0 = 1;
                eTask->in.NumFrameL1 = 0;
                eTask->in.L0Surface = &prevTask->in.InSurface;
                //in data
                eTask->in.NumExtParam = numExtInParams;
                eTask->in.ExtParam = inBufs;
                //out data
                eTask->out.NumExtParam = numExtOutParams;
                eTask->out.ExtParam = outBufs;
                break;
            case MFX_FRAMETYPE_B:
                eTask->in.NumFrameL0 = 1;
                eTask->in.NumFrameL1 = 1;
                eTask->in.L0Surface = &prevTask->in.InSurface;
                eTask->in.L1Surface = &nextTask->in.InSurface;
                //in data
                eTask->in.NumExtParam = numExtInParams;
                eTask->in.ExtParam = inBufs;
                //out data
                eTask->out.NumExtParam = numExtOutParams;
                eTask->out.ExtParam = outBufs;
                break;
        }
        mdprintf(stderr,"enc: %d t: %d\n", eTask->frameDisplayOrder, eTask->frameType);

        for (;;) {
            fprintf(stderr,"frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);

            sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
            sts = MFX_ERR_NONE;

            sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
            fprintf(stderr,"synced : %d\n",sts);


            if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1); // wait if device is busy
            } else if (MFX_ERR_NONE < sts && eTask->EncSyncP) {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
                break;
            } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                //                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                //                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            } else {
                // get next surface and new task for 2nd bitstream in ViewOutput mode
                MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                break;
            }
        }

        eTask->encoded = 1;
        if((this->m_refDist*2) == inputTasks.size())
        {
            inputTasks.front()->in.InSurface->Data.Locked = 0;
            delete inputTasks.front();
            //remove prevTask
            inputTasks.pop_front();
        }

        //drop output data to output file
        //pCurrentTask->WriteBitstream();
        if(!preENCCtr.DisableStatisticsOutput)
            fwrite(mbdata.MB, sizeof(mbdata.MB[0])*mbdata.NumMBAlloc, 1, mbstatout);
        if(!preENCCtr.DisableMVOutput)
            fwrite(mvs.MB, sizeof(mvs.MB[0])*mvs.NumMBAlloc, 1, mvout);
        //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    mdprintf(stderr,"input_tasks_size=%d\n", inputTasks.size());
    //encode last frames
    std::list<iTask*>::iterator it = inputTasks.begin();
    mfxI32 numUnencoded = 0;
    for(;it != inputTasks.end(); ++it){
        if((*it)->encoded == 0){
            numUnencoded++;
        }
    }
    //run processing on last frames
    if(numUnencoded > 0)
    {
        if(inputTasks.back()->frameType & MFX_FRAMETYPE_B){
            inputTasks.back()->frameType = MFX_FRAMETYPE_P;
        }
        mdprintf(stderr,"run processing on last frames: %d\n", numUnencoded);
    while (numUnencoded != 0) {
        // get a pointer to a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);

        // at this point surface for encoder contains a frame from file
        iTask* prevTask = NULL;
        iTask* nextTask = NULL;
        //find task to encode
        std::list<iTask*>::iterator encTask = inputTasks.begin();
        mdprintf(stderr,"[");
        for(; encTask != inputTasks.end(); ++encTask )
        {
            mdprintf(stderr,"%d ", (*encTask)->frameType);
        }
        mdprintf(stderr,"] ");
        encTask = inputTasks.begin();
        for(; encTask != inputTasks.end(); ++encTask )
        {
            if((*encTask)->encoded) continue;
            prevTask = NULL;
            nextTask = NULL;

            if((*encTask)->frameType & MFX_FRAMETYPE_I)
            {
                break;
            }else if((*encTask)->frameType & MFX_FRAMETYPE_P)
            { //find previous ref
                std::list<iTask*>::iterator it = encTask;
                while((it--) != inputTasks.begin())
                {
                    iTask* curTask = *it;
                    if(curTask->frameType & (MFX_FRAMETYPE_P | MFX_FRAMETYPE_I) &&
                            curTask->encoded){
                        prevTask = curTask;
                        break;
                    }
                }
                if(prevTask) break;
            }else if((*encTask)->frameType & MFX_FRAMETYPE_B)
            {//find previous ref
                std::list<iTask*>::iterator it = encTask;
                while((it--) != inputTasks.begin())
                {
                    iTask* curTask = *it;
                    if(curTask->frameType & (MFX_FRAMETYPE_P | MFX_FRAMETYPE_I) &&
                            curTask->encoded){
                        prevTask = curTask;
                        break;
                    }
                }
                mdprintf(stderr,"pT=%x ", prevTask);

              //find next ref
                it = encTask;
                while((++it) != inputTasks.end())
                {
                    iTask* curTask = *it;
                    if(curTask->frameType & (MFX_FRAMETYPE_P | MFX_FRAMETYPE_I) &&
                            curTask->encoded){
                        nextTask = curTask;
                        break;
                    }
                }
                mdprintf(stderr,"nT=%x ", nextTask);
                if(prevTask && nextTask) break;
            }
        }

        mdprintf(stderr,"in_size=%d prevTask=%x nextTask=%x ", inputTasks.size(),prevTask, nextTask);
        if(encTask == inputTasks.end())
        {
            mdprintf(stderr," skip B\n");
            continue; //skip B without refs
        }

        iTask* eTask = *encTask;

        preENCCtr.DisableMVOutput = m_params.dstMVFileBuff == NULL;
        preENCCtr.DisableStatisticsOutput = m_params.dstMBFileBuff == NULL;

        switch (eTask->frameType) {
            case MFX_FRAMETYPE_I:  //this is hack for now - it uses self referenced P frame instead of I frame
                eTask->in.NumFrameL0 = 0;
                eTask->in.NumFrameL1 = 0;
                //in data
                eTask->in.NumExtParam = numExtInParams;
                eTask->in.ExtParam = inBufs;
                //out data
                //exclude MV output from output buffers
                if(preENCCtr.DisableStatisticsOutput)
                {
                        eTask->out.NumExtParam = 0;
                        eTask->out.ExtParam = NULL;
                }else{
                        eTask->out.NumExtParam = 1;
                        eTask->out.ExtParam = outBufsI;
                }

                preENCCtr.DisableMVOutput = 1;
                break;
            case MFX_FRAMETYPE_P:
                eTask->in.NumFrameL0 = 1;
                eTask->in.NumFrameL1 = 0;
                eTask->in.L0Surface = &prevTask->in.InSurface;
                //in data
                eTask->in.NumExtParam = numExtInParams;
                eTask->in.ExtParam = inBufs;
                //out data
                eTask->out.NumExtParam = numExtOutParams;
                eTask->out.ExtParam = outBufs;
                break;
            case MFX_FRAMETYPE_B:
                eTask->in.NumFrameL0 = 1;
                eTask->in.NumFrameL1 = 1;
                eTask->in.L0Surface = &prevTask->in.InSurface;
                eTask->in.L1Surface = &nextTask->in.InSurface;
                //in data
                eTask->in.NumExtParam = numExtInParams;
                eTask->in.ExtParam = inBufs;
                //out data
                eTask->out.NumExtParam = numExtOutParams;
                eTask->out.ExtParam = outBufs;
                break;
        }
        mdprintf(stderr,"enc: %d t: %d\n", eTask->frameDisplayOrder, eTask->frameType);

        for (;;) {
            //Only synced operation supported for now
            fprintf(stderr,"frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);
            sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
            sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
            fprintf(stderr,"synced : %d\n",sts);

            sts = MFX_ERR_NONE;

            if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1); // wait if device is busy
            } else if (MFX_ERR_NONE < sts && eTask->EncSyncP) {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
                break;
            } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                //                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                //                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            } else {
                // get next surface and new task for 2nd bitstream in ViewOutput mode
                MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                break;
            }
        }

        eTask->encoded = 1;

        //drop output data to output file
        //pCurrentTask->WriteBitstream();
        if(!preENCCtr.DisableStatisticsOutput)
            fwrite(mbdata.MB, sizeof(mbdata.MB[0])*mbdata.NumMBAlloc, 1, mbstatout);
        if(!preENCCtr.DisableMVOutput)
            fwrite(mvs.MB, sizeof(mvs.MB[0])*mvs.NumMBAlloc, 1, mvout);


        //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        numUnencoded--;
    }
    }
    //unlock last frames
    {
    std::list<iTask*>::iterator it = inputTasks.begin();
    mfxI32 numUnencoded = 0;
    for(;it != inputTasks.end(); ++it){
        (*it)->in.InSurface->Data.Locked = 0;
    }
    }

    // means that the input file has ended, need to go to buffering loops
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    if(mbstatout) fclose(mbstatout);
    if(mvout) fclose(mvout);

    printf("number of processed frames: %d\n", frameCount);

    return sts;
}

void CEncodingPipeline::PrintInfo() {
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Encoding Sample Version %s\n"), MSDK_SAMPLE_VERSION);
    msdk_printf(MSDK_STRING("\nInput file format\t%s\n"), ColorFormatToStr(m_FileReader.m_ColorFormat));

    mfxFrameInfo SrcPicInfo = m_mfxEncParams.mfx.FrameInfo;

    msdk_printf(MSDK_STRING("Source picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), SrcPicInfo.Width, SrcPicInfo.Height);
    msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), SrcPicInfo.CropX, SrcPicInfo.CropY, SrcPicInfo.CropW, SrcPicInfo.CropH);

    const msdk_char* sMemType = m_memType == D3D9_MEMORY ? MSDK_STRING("d3d")
            : (m_memType == D3D11_MEMORY ? MSDK_STRING("d3d11")
            : MSDK_STRING("system"));
    msdk_printf(MSDK_STRING("Memory type\t%s\n"), sMemType);

    mfxIMPL impl;
    m_mfxSession.QueryIMPL(&impl);

    const msdk_char* sImpl = (MFX_IMPL_VIA_D3D11 == MFX_IMPL_VIA_MASK(impl)) ? MSDK_STRING("hw_d3d11")
            : (MFX_IMPL_HARDWARE & impl) ? MSDK_STRING("hw")
            : MSDK_STRING("sw");
    msdk_printf(MSDK_STRING("Media SDK impl\t\t%s\n"), sImpl);

    mfxVersion ver;
    m_mfxSession.QueryVersion(&ver);
    msdk_printf(MSDK_STRING("Media SDK version\t%d.%d\n"), ver.Major, ver.Minor);

    msdk_printf(MSDK_STRING("\n"));
}
