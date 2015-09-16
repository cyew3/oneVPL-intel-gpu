//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2015 Intel Corporation. All Rights Reserved.
//


#include "pipeline_fei.h"
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

#define MFX_FRAMETYPE_IPB (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B)

mfxU8 GetDefaultPicOrderCount(mfxVideoParam const & par)
{
    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        return 0;
    if (par.mfx.GopRefDist > 1)
        return 0;

    return 2;
}

static FILE* mbstatout   = NULL;
static FILE* mvout       = NULL;
static FILE* mvENCPAKout = NULL;
static FILE* mbcodeout   = NULL;
static FILE* pMvPred     = NULL;
static FILE* pEncMBs     = NULL;
static FILE* pPerMbQP    = NULL;


CEncTaskPool::CEncTaskPool()
{
    m_pTasks            = NULL;
    m_pmfxSession       = NULL;
    m_nTaskBufferStart  = 0;
    m_nPoolSize         = 0;
}

CEncTaskPool::~CEncTaskPool()
{
    Close();
}

mfxStatus CEncTaskPool::Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter)
{
    MSDK_CHECK_POINTER(pmfxSession, MFX_ERR_NULL_PTR);
    //MSDK_CHECK_POINTER(pWriter, MFX_ERR_NULL_PTR);

    MSDK_CHECK_ERROR(nPoolSize,   0, MFX_ERR_UNDEFINED_BEHAVIOR);
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
        for (mfxU32 i = 0; i < m_nPoolSize; i+=2)
        {
            sts = m_pTasks[i+0].Init(nBufferSize, pWriter);
            sts = m_pTasks[i+1].Init(nBufferSize, pOtherWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }
    else
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i++)
        {
            sts = m_pTasks[i].Init(nBufferSize, pWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncTaskPool::SynchronizeFirstTask()
{
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(m_pmfxSession, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts  = MFX_ERR_NONE;

    // non-null sync point indicates that task is in execution
    if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP)
    {
        sts = m_pmfxSession->SyncOperation(m_pTasks[m_nTaskBufferStart].EncSyncP, MSDK_WAIT_INTERVAL);

        if (MFX_ERR_NONE == sts)
        {
            //Write output for encode
            mfxBitstream& bs = m_pTasks[m_nTaskBufferStart].mfxBS;
            mfxExtFeiEncMV*     mvBuf     = NULL;
            mfxExtFeiEncMBStat* mbstatBuf = NULL;
            mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;

            for(int i=0; i<bs.NumExtParam; i++){
                if (bs.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV && mvENCPAKout){
                    if (!(bs.FrameType & MFX_FRAMETYPE_I)){
                        mvBuf = (mfxExtFeiEncMV*)bs.ExtParam[i];
                        fwrite(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                    }
                    else{
                        mfxExtFeiEncMV::mfxMB tmpMB;
                        memset(&tmpMB, 0x8000, sizeof(tmpMB));
                        mvBuf = (mfxExtFeiEncMV*)bs.ExtParam[i];
                        for (mfxU32 k = 0; k < mvBuf->NumMBAlloc; k++)
                            fwrite(&tmpMB, sizeof(tmpMB), 1, mvENCPAKout);
                    }
                }
                if(bs.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT && mbstatout){
                    mbstatBuf = (mfxExtFeiEncMBStat*)bs.ExtParam[i];
                    fwrite(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, mbstatout);
                }
                if(bs.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL && mbcodeout){
                    mbcodeBuf = (mfxExtFeiPakMBCtrl*)bs.ExtParam[i];
                    fwrite(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
                }
            }

            sts = m_pTasks[m_nTaskBufferStart].WriteBitstream();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            sts = m_pTasks[m_nTaskBufferStart].Reset();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // move task buffer start to the next executing task
            // the first transform frame to the right with non zero sync point
            for (mfxU32 i = 0; i < m_nPoolSize; i++)
            {
                m_nTaskBufferStart = (m_nTaskBufferStart + 1) % m_nPoolSize;
                if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP)
                {
                    break;
                }
            }
        }

        return sts;
    }
    else
    {
        return MFX_ERR_NOT_FOUND; // no tasks left in task buffer
    }
}

mfxU32 CEncTaskPool::GetFreeTaskIndex()
{
    mfxU32 off = 0;

    if (m_pTasks)
    {
        for (off = 0; off < m_nPoolSize; off++)
        {
            if (NULL == m_pTasks[(m_nTaskBufferStart + off) % m_nPoolSize].EncSyncP)
            {
                break;
            }
        }
    }

    if (off >= m_nPoolSize)
        return m_nPoolSize;

    return (m_nTaskBufferStart + off) % m_nPoolSize;
}

mfxStatus CEncTaskPool::GetFreeTask(sTask **ppTask)
{
    MSDK_CHECK_POINTER(ppTask, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);

    mfxU32 index = GetFreeTaskIndex();

    if (index >= m_nPoolSize)
    {
        return MFX_ERR_NOT_FOUND;
    }

    // return the address of the task
    *ppTask = &m_pTasks[index];

    return MFX_ERR_NONE;
}

void CEncTaskPool::Close()
{
    if (m_pTasks)
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i++)
        {
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
    MSDK_ZERO_MEMORY(mfxBS);
}

mfxStatus sTask::Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pwriter)
{
    Close();

    pWriter = pwriter;

    mfxStatus sts = Reset();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitMfxBitstream(&mfxBS, nBufferSize);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(&mfxBS));

    return sts;
}

mfxStatus sTask::Close()
{
    WipeMfxBitstream(&mfxBS);
    EncSyncP = 0;

    return MFX_ERR_NONE;
}

mfxStatus sTask::WriteBitstream()
{
    if (!pWriter)
        return MFX_ERR_NOT_INITIALIZED;

    return pWriter->WriteNextFrame(&mfxBS);
}

mfxStatus sTask::Reset()
{
    // mark sync point as free
    EncSyncP = NULL;

    // prepare bit stream
    mfxBS.DataOffset = 0;
    mfxBS.DataLength = 0;

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitMfxEncParams(sInputParams *pInParams)
{
    m_encpakParams = *pInParams;

    m_mfxEncParams.mfx.CodecId                 = pInParams->CodecId;
    m_mfxEncParams.mfx.TargetUsage             = pInParams->nTargetUsage; // trade-off between quality and speed
    m_mfxEncParams.mfx.TargetKbps              = pInParams->nBitRate; // in Kbps
    /*For now FEI work with RATECONTROL_CQP only*/
    m_mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    ConvertFrameRate(pInParams->dFrameRate, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtN, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);
    m_mfxEncParams.mfx.EncodedOrder            = 0; // binary flag, 0 signals encoder to take frames in display order

    if (0 != pInParams->QP)
        m_mfxEncParams.mfx.QPI = m_mfxEncParams.mfx.QPP = m_mfxEncParams.mfx.QPB = pInParams->QP;

    // specify memory type
    if (D3D9_MEMORY == pInParams->memType || D3D11_MEMORY == pInParams->memType)
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }
    else
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }

    // frame info parameters
    m_mfxEncParams.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
    m_mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_mfxEncParams.mfx.FrameInfo.PicStruct    = pInParams->nPicStruct;

    // set frame size and crops
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxEncParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(pInParams->nDstWidth);
    m_mfxEncParams.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxEncParams.mfx.FrameInfo.PicStruct)?
        MSDK_ALIGN16(pInParams->nDstHeight) : MSDK_ALIGN32(pInParams->nDstHeight);

    m_mfxEncParams.mfx.FrameInfo.CropX = 0;
    m_mfxEncParams.mfx.FrameInfo.CropY = 0;
    m_mfxEncParams.mfx.FrameInfo.CropW = pInParams->nDstWidth;
    m_mfxEncParams.mfx.FrameInfo.CropH = pInParams->nDstHeight;
    m_mfxEncParams.AsyncDepth = 1;  //current limitation
    m_mfxEncParams.mfx.GopRefDist = pInParams->refDist > 0 ? pInParams->refDist : 1;
    m_mfxEncParams.mfx.GopPicSize = pInParams->gopSize > 0 ? pInParams->gopSize : 1;
    m_mfxEncParams.mfx.IdrInterval = pInParams->nIdrInterval;

    /* Multi references and multi slices*/
    if (pInParams->numRef == 0)
    {
        m_mfxEncParams.mfx.NumRefFrame = 1;
    }
    else
    {
        m_mfxEncParams.mfx.NumRefFrame = pInParams->numRef;
    }

    if (pInParams->numSlices == 0)
    {
        m_mfxEncParams.mfx.NumSlice = 1;
    }
    else
    {
        m_mfxEncParams.mfx.NumSlice = pInParams->numSlices;
    }

    // configure the depth of the look ahead BRC if specified in command line
    if (pInParams->nLADepth || pInParams->bRefType)
    {
        m_CodingOption2.LookAheadDepth = pInParams->nLADepth;
        m_CodingOption2.BRefType = pInParams->bRefType;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_CodingOption2);
    }

    if(pInParams->bENCODE)
    {
        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_ENCPAK;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_encpakInit);
    }

    if(pInParams->bPREENC)
    {
#if 0          //find better way
        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_PREENC;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_encpakInit);
#endif
    }

    //mfxExtBuffer * test_ext_buf = (mfxExtBuffer *)&m_encpakInit;

    if (!m_EncExtParams.empty())
    {
        m_mfxEncParams.ExtParam = &m_EncExtParams[0]; // vector is stored linearly in memory
        //m_mfxEncParams.ExtParam = &test_ext_buf;
        m_mfxEncParams.NumExtParam = (mfxU16)m_EncExtParams.size();
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::CreateHWDevice()
{
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
    if (NULL == m_hwdev)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    sts = m_hwdev->Init(NULL, 0, MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#endif
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::ResetDevice()
{
    if (D3D9_MEMORY == m_memType || D3D11_MEMORY == m_memType)
    {
        return m_hwdev->Reset();
    }
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocFrames()
{
    //MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest EncRequest;
    mfxFrameAllocRequest ReconRequest;

    mfxU16 nEncSurfNum = 0; // number of surfaces for encoder

    MSDK_ZERO_MEMORY(EncRequest);
    MSDK_ZERO_MEMORY(ReconRequest);

    // Calculate the number of surfaces for components.
    // QueryIOSurf functions tell how many surfaces are required to produce at least 1 output.
    // To achieve better performance we provide extra surfaces.
    // 1 extra surface at input allows to get 1 extra output.

    if (m_pmfxENCODE)
    {
        sts = m_pmfxENCODE->QueryIOSurf(&m_mfxEncParams, &EncRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    //else
    //{
        /* Looks like m_pmfxENCODE is not Initialized */
    //    sts = MFX_ERR_NULL_PTR;
    //    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    //}

    // The number of surfaces shared by vpp output and encode input.
    // When surfaces are shared 1 surface at first component output contains output frame that goes to next component input
    nEncSurfNum = EncRequest.NumFrameSuggested + (m_nAsyncDepth - 1);
    if ((m_encpakParams.bPREENC) || (m_encpakParams.bENCPAK) || (m_encpakParams.bENCODE) ||
        (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK) )
    {
        bool twoEncoders = (m_encpakParams.bPREENC) && ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK)); // need to double task pool size
                                                                                                                                             // in case of PREENC + ENC
        nEncSurfNum = m_refDist * (twoEncoders? 4 : 2);
    }

    // prepare allocation requests

    EncRequest.NumFrameMin       = nEncSurfNum;
    EncRequest.NumFrameSuggested = nEncSurfNum;
    MSDK_MEMCPY_VAR(EncRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

    //EncRequest.AllocId = PREENC_ALLOC;
    if ((m_pmfxPREENC) )
    {
        EncRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    }
    else if ( (m_pmfxPAK) || (m_pmfxENC) )
        EncRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;

    // alloc frames for encoder
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &EncRequest, &m_EncResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    /* Alloc reconstructed surfaces for PAK */
    if ((m_pmfxENC)|| (m_pmfxPAK))
    {
        //ReconRequest.AllocId = PAK_ALLOC;
        MSDK_MEMCPY_VAR(ReconRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
        ReconRequest.NumFrameMin = m_mfxEncParams.mfx.GopRefDist *2 + m_mfxEncParams.AsyncDepth;
        ReconRequest.NumFrameSuggested = m_mfxEncParams.mfx.GopRefDist *2 + m_mfxEncParams.AsyncDepth;
        /* type of reconstructed surfaces for PAK should be same as for Media SDK's decoders!!!
         * Because libVA required reconstructed surfaces for vaCreateContext */
        ReconRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &ReconRequest, &m_ReconResponse);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // prepare mfxFrameSurface1 array for encoder
    m_pEncSurfaces = new mfxFrameSurface1 [m_EncResponse.NumFrameActual];
    MSDK_CHECK_POINTER(m_pEncSurfaces, MFX_ERR_MEMORY_ALLOC);
    if ((m_pmfxENC) || (m_pmfxPAK))
    {
        m_pReconSurfaces = new mfxFrameSurface1 [m_ReconResponse.NumFrameActual];
        MSDK_CHECK_POINTER(m_pReconSurfaces, MFX_ERR_MEMORY_ALLOC);
    }

    for (int i = 0; i < m_EncResponse.NumFrameActual; i++)
    {
        memset(&(m_pEncSurfaces[i]), 0, sizeof(mfxFrameSurface1));
        MSDK_MEMCPY_VAR(m_pEncSurfaces[i].Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        if (m_bExternalAlloc)
        {
            m_pEncSurfaces[i].Data.MemId = m_EncResponse.mids[i];
        }
        else
        {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_EncResponse.mids[i], &(m_pEncSurfaces[i].Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    for (int i = 0; i < m_ReconResponse.NumFrameActual; i++)
    {
        memset(&(m_pReconSurfaces[i]), 0, sizeof(mfxFrameSurface1));
        MSDK_MEMCPY_VAR(m_pReconSurfaces[i].Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        if (m_bExternalAlloc)
        {
            m_pReconSurfaces[i].Data.MemId = m_ReconResponse.mids[i];
        }
        else
        {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_ReconResponse.mids[i], &(m_pReconSurfaces[i].Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::CreateAllocator()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (D3D9_MEMORY == m_memType || D3D11_MEMORY == m_memType)
    {
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

        if (impl != MFX_IMPL_SOFTWARE)
        {
            sts = m_mfxSession.SetHandle(hdl_t, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            if (m_encpakParams.bPREENC){
                sts = m_preenc_mfxSession.SetHandle(hdl_t, hdl);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
        }

        // create D3D allocator
#if MFX_D3D11_SUPPORT
        if (D3D11_MEMORY == m_memType)
        {
            m_pMFXAllocator = new D3D11FrameAllocator;
            MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

            D3D11AllocatorParams *pd3dAllocParams = new D3D11AllocatorParams;
            MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
            pd3dAllocParams->pDevice = reinterpret_cast<ID3D11Device *>(hdl);

            m_pmfxAllocatorParams = pd3dAllocParams;
        }
        else
#endif // #if MFX_D3D11_SUPPORT
        {
            m_pMFXAllocator = new D3DFrameAllocator;
            MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

            D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;
            MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
            pd3dAllocParams->pManager = reinterpret_cast<IDirect3DDeviceManager9 *>(hdl);

            m_pmfxAllocatorParams = pd3dAllocParams;
        }

        /* In case of video memory we must provide MediaSDK with external allocator
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to Media SDK */
        sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        if (m_encpakParams.bPREENC){
            sts = m_preenc_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

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
        if (m_encpakParams.bPREENC){
            sts = m_preenc_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        // create VAAPI allocator
        m_pMFXAllocator = new vaapiFrameAllocator;
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        vaapiAllocatorParams *p_vaapiAllocParams = new vaapiAllocatorParams;
        MSDK_CHECK_POINTER(p_vaapiAllocParams, MFX_ERR_MEMORY_ALLOC);

        p_vaapiAllocParams->m_dpy = (VADisplay)hdl;
        m_pmfxAllocatorParams = p_vaapiAllocParams;

        /* In case of video memory we must provide MediaSDK with external allocator
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to mediasdk */
        sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        if (m_encpakParams.bPREENC){
            sts = m_preenc_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        m_bExternalAlloc = true;
#endif
    }
    else
    {
#ifdef LIBVA_SUPPORT
        //in case of system memory allocator we also have to pass MFX_HANDLE_VA_DISPLAY to HW library
        mfxIMPL impl;
        m_mfxSession.QueryIMPL(&impl);

        if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(impl))
        {
            sts = CreateHWDevice();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            mfxHDL hdl = NULL;
            sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
            // provide device manager to MediaSDK
            sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            if (m_encpakParams.bPREENC){
                sts = m_preenc_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
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

void CEncodingPipeline::DeleteFrames()
{
    // delete surfaces array
    MSDK_SAFE_DELETE_ARRAY(m_pEncSurfaces);
    MSDK_SAFE_DELETE_ARRAY(m_pReconSurfaces);

    // delete frames
    if (m_pMFXAllocator)
    {
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_EncResponse);
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_ReconResponse);
    }
}

void CEncodingPipeline::DeleteHWDevice()
{
    MSDK_SAFE_DELETE(m_hwdev);
}

void CEncodingPipeline::DeleteAllocator()
{
    // delete allocator
    MSDK_SAFE_DELETE(m_pMFXAllocator);
    MSDK_SAFE_DELETE(m_pmfxAllocatorParams);

    DeleteHWDevice();
}

CEncodingPipeline::CEncodingPipeline()
{
    //m_pUID = NULL;
    m_pmfxPREENC = NULL;
    m_pmfxENC    = NULL;
    m_pmfxPAK    = NULL;
    m_pmfxENCODE = NULL;
    m_pMFXAllocator = NULL;
    m_pmfxAllocatorParams = NULL;
    m_memType = D3D9_MEMORY; //only hw memory is supported
    m_bExternalAlloc = false;
    m_pEncSurfaces = NULL;
    m_pReconSurfaces = NULL;
    m_nAsyncDepth = 0;
    m_refFrameCounter = 0;

    m_nNumView = 0;

    m_FileWriters.first = m_FileWriters.second = NULL;

    MSDK_ZERO_MEMORY(m_CodingOption2);
    m_CodingOption2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    m_CodingOption2.Header.BufferSz = sizeof(m_CodingOption2);

#if D3D_SURFACES_SUPPORT
    m_hwdev = NULL;
#endif

    MSDK_ZERO_MEMORY(m_mfxEncParams);
    MSDK_ZERO_MEMORY(m_EncResponse);
}

CEncodingPipeline::~CEncodingPipeline()
{
    Close();
}

mfxStatus CEncodingPipeline::InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename)
{
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

    // prepare output file writers
    if((pParams->bENCODE) || (pParams->bENCPAK) || (pParams->bOnlyPAK)){ //need only for ENC+PAK, only ENC + only PAK, only PAK
        sts = InitFileWriter(&m_FileWriters.first, pParams->dstFileBuff[0]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return sts;
}

mfxStatus CEncodingPipeline::Init(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    m_refDist = pParams->refDist > 0 ? pParams->refDist : 1;
    m_gopSize = pParams->gopSize > 0 ? pParams->gopSize : 1;

    // prepare input file reader
    sts = m_FileReader.Init(pParams->strSrcFile,
                            pParams->ColorFormat,
                            0,
                            pParams->srcFileBuff);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitFileWriters(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Init session
    if (pParams->bUseHWLib)
    {
        // try searching on all display adapters
        mfxIMPL impl = MFX_IMPL_HARDWARE_ANY;

        // if d3d11 surfaces are used ask the library to run acceleration through D3D11
        // feature may be unsupported due to OS or MSDK API version
        if (D3D11_MEMORY == pParams->memType)
            impl |= MFX_IMPL_VIA_D3D11;

        sts = m_mfxSession.Init(impl, NULL);

        // MSDK API version may not support multiple adapters - then try initialize on the default
        if (MFX_ERR_NONE != sts)
            sts = m_mfxSession.Init((impl & (!MFX_IMPL_HARDWARE_ANY)) | MFX_IMPL_HARDWARE, NULL);

        if(pParams->bPREENC){
            sts = m_preenc_mfxSession.Init(impl, NULL);

            // MSDK API version may not support multiple adapters - then try initialize on the default
            if (MFX_ERR_NONE != sts)
               sts = m_preenc_mfxSession.Init((impl & (!MFX_IMPL_HARDWARE_ANY)) | MFX_IMPL_HARDWARE, NULL);
        }
    }
    else
    {
        sts = m_mfxSession.Init(MFX_IMPL_SOFTWARE, NULL);

        if(pParams->bPREENC){
            sts = m_preenc_mfxSession.Init(MFX_IMPL_SOFTWARE, NULL);
        }
    }

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // set memory type
    m_memType = pParams->memType;

    sts = InitMfxEncParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // create and init frame allocator
    sts = CreateAllocator();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //if(pParams->bENCODE){
        // create encoder
        m_pmfxENCODE = new MFXVideoENCODE(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_MEMORY_ALLOC);
    //}

    if(pParams->bPREENC){
        // create encoder
        m_pmfxPREENC = new MFXVideoENC(m_preenc_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPREENC, MFX_ERR_MEMORY_ALLOC);
    }

    if(pParams->bENCPAK){
        // create encoder
        m_pmfxENC = new MFXVideoENC(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);

        m_pmfxPAK = new MFXVideoPAK(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPAK, MFX_ERR_MEMORY_ALLOC);
    }

    if(pParams->bOnlyENC){
        // create ENC
        m_pmfxENC = new MFXVideoENC(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);
    }

    if(pParams->bOnlyPAK){
        // create PAK
        m_pmfxPAK = new MFXVideoPAK(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPAK, MFX_ERR_MEMORY_ALLOC);
    }

    m_nAsyncDepth = 1; // this number can be tuned for better performance

    sts = ResetMFXComponents(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

void CEncodingPipeline::Close()
{
    if (m_FileWriters.first)
        msdk_printf(MSDK_STRING("Frame number: %u\r"), m_FileWriters.first->m_nProcessedFramesNum);

    MSDK_SAFE_DELETE(m_pmfxENCODE);
    MSDK_SAFE_DELETE(m_pmfxENC);
    MSDK_SAFE_DELETE(m_pmfxPAK);
    MSDK_SAFE_DELETE(m_pmfxPREENC);

    DeleteFrames();

    m_TaskPool.Close();
    m_mfxSession.Close();
    if (m_encpakParams.bPREENC){
        m_preenc_mfxSession.Close();
    }

    // allocator if used as external for MediaSDK must be deleted after SDK components
    DeleteAllocator();

    m_FileReader.Close();
    FreeFileWriters();
}

void CEncodingPipeline::FreeFileWriters()
{
    if (m_FileWriters.second == m_FileWriters.first)
    {
        m_FileWriters.second = NULL; // second do not own the writer - just forget pointer
    }

    if (m_FileWriters.first)
        m_FileWriters.first->Close();
    MSDK_SAFE_DELETE(m_FileWriters.first);

    if (m_FileWriters.second)
        m_FileWriters.second->Close();
    MSDK_SAFE_DELETE(m_FileWriters.second);
}

mfxStatus CEncodingPipeline::ResetMFXComponents(sInputParams* pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    if(m_pmfxENCODE)
    {
        sts = m_pmfxENCODE->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // free allocated frames
    DeleteFrames();

    m_TaskPool.Close();

    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //use encoded order if both is used
    if(m_encpakParams.bPREENC && (m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC)){
        m_mfxEncParams.mfx.EncodedOrder = 1;
    }

    if ((m_encpakParams.bENCODE) && (m_pmfxENCODE) )
    {
        sts = m_pmfxENCODE->Init(&m_mfxEncParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxPREENC) {
        mfxVideoParam preEncParams = m_mfxEncParams;

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_PREENC;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        preEncParams.NumExtParam = 1;
        preEncParams.ExtParam    = buf;
        //preEncParams.AllocId     = PREENC_ALLOC;

        sts = m_pmfxPREENC->Init(&preEncParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts) {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxENC)
    {
        mfxVideoParam encParams = m_mfxEncParams;

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_ENC;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        encParams.NumExtParam = 1;
        encParams.ExtParam    = buf;
        //encParams.AllocId     = m_pmfxPREENC ? PAK_ALLOC : PREENC_ALLOC;

        sts = m_pmfxENC->Init(&encParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxPAK)
    {
        mfxVideoParam pakParams = m_mfxEncParams;

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_PAK;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        pakParams.NumExtParam = 1;
        pakParams.ExtParam    = buf;
        //pakParams.AllocId     = PAK_ALLOC;

        sts = m_pmfxPAK->Init(&pakParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    bool twoEncoders = m_encpakParams.bPREENC && ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) || (m_encpakParams.bOnlyENC));
    mfxU32 nEncodedDataBufferSize = m_mfxEncParams.mfx.FrameInfo.Width * m_mfxEncParams.mfx.FrameInfo.Height * 4;
    sts = m_TaskPool.Init(&m_mfxSession, m_FileWriters.first, m_nAsyncDepth * (twoEncoders ? 4 : 2), nEncodedDataBufferSize, m_FileWriters.second);
    //sts = m_TaskPool.Init(&m_mfxSession, m_FileWriters.first, 1, nEncodedDataBufferSize, m_FileWriters.second);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocateSufficientBuffer(mfxBitstream* pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_NOT_INITIALIZED);

    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    mfxStatus sts = m_pmfxENCODE->GetVideoParam(&par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // reallocate bigger buffer for output
    sts = ExtendMfxBitstream(pBS, par.mfx.BufferSizeInKB * 1000);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(pBS));

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::GetFreeTask(sTask **ppTask)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_TaskPool.GetFreeTask(ppTask);
    if (MFX_ERR_NOT_FOUND == sts)
    {
        sts = SynchronizeFirstTask();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // try again
        sts = m_TaskPool.GetFreeTask(ppTask);
    }

    return sts;
}

mfxU16 CEncodingPipeline::GetFrameType(mfxU32 pos) {
    if ((pos % m_gopSize) == 0) {
        return MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
    } else if ((pos % m_gopSize % m_refDist) == 0) {
        return MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
    } else
        return MFX_FRAMETYPE_B;
}


mfxStatus CEncodingPipeline::SynchronizeFirstTask()
{
    mfxStatus sts = m_TaskPool.SynchronizeFirstTask();

    return sts;
}

mfxStatus CEncodingPipeline::Run()
{
    //MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    mfxFrameSurface1* pSurf = NULL; // dispatching pointer
    mfxFrameSurface1* pReconSurf = NULL;

    sTask *pCurrentTask = NULL; // a pointer to the current task
    mfxU16 nEncSurfIdx = 0;     // index of free surface for encoder input (vpp output)
    mfxU16 nReconSurfIdx = 0;     // index of free surface for reconstruct pool

    mfxU32 fieldId = 0, numOfFields = 1; // default is progressive case
    mfxI32 heightMB = ((m_encpakParams.nHeight + 15) & ~15);
    mfxI32 widthMB  = ((m_encpakParams.nWidth  + 15) & ~15);
    mfxI32 numMB = heightMB * widthMB / 256;
    mfxI16 *tmpForMedian = NULL;
    mfxExtFeiPreEncMV::mfxMB* tmpForReading = NULL, *tmpMBpreenc = NULL;
    mfxExtFeiEncMV::mfxMB* tmpMBenc = NULL;

    ctr = NULL;

    //FEI buffers init
    feiEncMbStat[0].MB = NULL;
    feiEncMbStat[1].MB = NULL;
    feiEncMVPredictors[0].MB = NULL;
    feiEncMVPredictors[1].MB = NULL;
    feiEncMBCtrl[0].MB = NULL;
    feiEncMBCtrl[1].MB = NULL;
    feiEncMV[0].MB = NULL;
    feiEncMV[1].MB = NULL;
    feiEncMBCode[0].MB = NULL;
    feiEncMBCode[1].MB = NULL;

    numExtInParams   = 0;
    numExtInParamsI  = 0;
    numExtOutParams  = 0;

    numExtOutParamsPreEnc = 0;
    numExtInParamsPreEnc  = 0;

    /* if TFF or BFF*/
    if ( (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
         (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF) )
    {
        numOfFields = 2;
        // For interlaced mode, may need an extra MB vertically
        // For example if the progressive mode has 45 MB vertically
        // The interlace should have 23 MB for each field
        numMB = widthMB/16 * (((heightMB/16)+1) / 2);
    }

    if (m_encpakParams.bPREENC) {
        inputTasks.clear();  //for reordering

        //setup control structures
        bool disableMVoutput = m_encpakParams.mvoutFile == NULL && !(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC);
        bool disableMBoutput = (m_encpakParams.mbstatoutFile == NULL) || m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC; //couple with ENC+PAK
        bool enableMVpredictor = m_encpakParams.mvinFile != NULL;
        bool enableMBQP = m_encpakParams.mbQpFile != NULL && !(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC);

        disableMVoutPreENC  = disableMVoutput;
        disableMBStatPreENC = disableMBoutput;

        for (fieldId = 0; fieldId < numOfFields; fieldId++)
        {
            memset(&preENCCtr[fieldId], 0, sizeof (mfxExtFeiPreEncCtrl));
            preENCCtr[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_CTRL;
            preENCCtr[fieldId].Header.BufferSz = sizeof (mfxExtFeiPreEncCtrl);
            preENCCtr[fieldId].PictureType = numOfFields == 1 ? MFX_PICTYPE_FRAME :
                fieldId == 0 ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD;

            preENCCtr[fieldId].DisableMVOutput         = disableMVoutput;
            preENCCtr[fieldId].DisableStatisticsOutput = disableMBoutput;
            preENCCtr[fieldId].FTEnable       = m_encpakParams.FTEnable;//1;//0;
            preENCCtr[fieldId].AdaptiveSearch = m_encpakParams.AdaptiveSearch;//1;
            preENCCtr[fieldId].LenSP = m_encpakParams.LenSP;//57;
            preENCCtr[fieldId].MBQp = enableMBQP;
            preENCCtr[fieldId].MVPredictor  = enableMVpredictor;
            preENCCtr[fieldId].RefHeight    = m_encpakParams.RefHeight;//40;
            preENCCtr[fieldId].RefWidth     = m_encpakParams.RefWidth;//48;
            preENCCtr[fieldId].SubPelMode   = m_encpakParams.SubPelMode;//3;
            preENCCtr[fieldId].SearchWindow = m_encpakParams.SearchWindow;//0;
            preENCCtr[fieldId].SearchPath   = m_encpakParams.SearchPath;
            preENCCtr[fieldId].Qp = m_encpakParams.QP;
            preENCCtr[fieldId].IntraSAD = m_encpakParams.IntraSAD;//2;
            preENCCtr[fieldId].InterSAD = m_encpakParams.InterSAD;//2;
            preENCCtr[fieldId].SubMBPartMask = m_encpakParams.SubMBPartMask;//0;//0x77;
            preENCCtr[fieldId].IntraPartMask = m_encpakParams.IntraPartMask;//0
            preENCCtr[fieldId].Enable8x8Stat = m_encpakParams.Enable8x8Stat;

            //inBufsPreEnc[numExtInParamsPreEnc++] = (mfxExtBuffer*) & preENCCtr;

            if (preENCCtr[fieldId].MVPredictor) {
                mvPreds[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV_PRED;
                mvPreds[fieldId].Header.BufferSz = sizeof (mfxExtFeiPreEncMVPredictors);
                mvPreds[fieldId].NumMBAlloc = numMB;
                mvPreds[fieldId].MB = new mfxExtFeiPreEncMVPredictors::mfxMB [numMB];
                //inBufsPreEnc[numExtInParamsPreEnc++] = (mfxExtBuffer*) & mvPreds;
                //read mvs from file

                printf("Using MV input file: %s\n", m_encpakParams.mvinFile);
                MSDK_FOPEN(pMvPred, m_encpakParams.mvinFile, MSDK_CHAR("rb"));
                if (pMvPred == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mvinFile);
                    exit(-1);
                }
            }

            if (preENCCtr[fieldId].MBQp) {
                qps[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
                qps[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncQP);
                qps[fieldId].NumQPAlloc = numMB;
                qps[fieldId].QP = new mfxU8[numMB];
                //inBufsPreEnc[numExtInParamsPreEnc++] = (mfxExtBuffer*) & qps;
                //read mvs from file

                printf("Using QP input file: %s\n", m_encpakParams.mbQpFile);
                MSDK_FOPEN(pPerMbQP, m_encpakParams.mbQpFile, MSDK_CHAR("rb"));
                if (pPerMbQP == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbQpFile);
                    exit(-1);
                }
            }

            if (!preENCCtr[fieldId].DisableMVOutput) {
                mvs[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV;
                mvs[fieldId].Header.BufferSz = sizeof (mfxExtFeiPreEncMV);
                mvs[fieldId].NumMBAlloc = numMB;
                mvs[fieldId].MB = new mfxExtFeiPreEncMV::mfxMB [numMB];
                memset(mvs[fieldId].MB, 0, sizeof(mvs[fieldId].MB[0])*mvs[fieldId].NumMBAlloc);
                //outBufsPreEnc[numExtOutParamsPreEnc++] = (mfxExtBuffer*) & mvs;

                if (m_encpakParams.mvoutFile != NULL &&
                        !(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC)) {
                    printf("Using MV output file: %s\n", m_encpakParams.mvoutFile);
                    tmpMBpreenc = new mfxExtFeiPreEncMV::mfxMB;
                    memset(tmpMBpreenc, 0x8000, sizeof(*tmpMBpreenc));
                    MSDK_FOPEN(mvout, m_encpakParams.mvoutFile, MSDK_CHAR("wb"));
                    if (mvout == NULL) {
                        fprintf(stderr, "Can't open file %s\n", m_encpakParams.mvoutFile);
                        exit(-1);
                    }
                }
            }

            if (!preENCCtr[fieldId].DisableStatisticsOutput) {
                mbdata[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MB;
                mbdata[fieldId].Header.BufferSz = sizeof (mfxExtFeiPreEncMBStat);
                mbdata[fieldId].NumMBAlloc = numMB;
                mbdata[fieldId].MB = new mfxExtFeiPreEncMBStat::mfxMB [numMB];
                //outBufsPreEnc[numExtOutParamsPreEnc++] = (mfxExtBuffer*) & mbdata;
                //outBufsPreEncI[0] = (mfxExtBuffer*) & mbdata; //special case for I frames

                printf("Using MB distortion output file: %s\n", m_encpakParams.mbstatoutFile);
                MSDK_FOPEN(mbstatout, m_encpakParams.mbstatoutFile, MSDK_CHAR("wb"));
                if (mbstatout == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbstatoutFile);
                    exit(-1);
                }
            } // if (!preENCCtr.DisableStatisticsOutput)

            // Let's count all control and Ext buffers...
            inBufsPreEnc[numExtInParamsPreEnc++] = (mfxExtBuffer*) & preENCCtr;
            if (preENCCtr[fieldId].MVPredictor)
                inBufsPreEnc[numExtInParamsPreEnc++] = (mfxExtBuffer*) & mvPreds;
            if (preENCCtr[fieldId].MBQp)
                inBufsPreEnc[numExtInParamsPreEnc++] = (mfxExtBuffer*) & qps;
            if (!preENCCtr[fieldId].DisableMVOutput)
                outBufsPreEnc[numExtOutParamsPreEnc++] = (mfxExtBuffer*) & mvs;
            if (!preENCCtr[fieldId].DisableStatisticsOutput)
            {
                outBufsPreEnc[numExtOutParamsPreEnc++] = (mfxExtBuffer*) & mbdata;
                outBufsPreEncI[0] = (mfxExtBuffer*) & mbdata; //special case for I frames
            }
        } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
    } // if (m_encpakParams.bPREENC) {

    if ((m_encpakParams.bENCODE)  || (m_encpakParams.bENCPAK)||
        (m_encpakParams.bOnlyPAK) || (m_encpakParams.bOnlyENC) ){
        feiCtrl.FrameType = MFX_FRAMETYPE_UNKNOWN; //GetFrameType(frameCount);
        feiCtrl.QP = m_encpakParams.QP;
        //feiCtrl.SkipFrame = 0;
        feiCtrl.NumPayload = 0;
        feiCtrl.Payload = NULL;

        bool MVPredictors = (m_encpakParams.mvinFile != NULL) || m_encpakParams.bPREENC; //couple with PREENC
        bool MBCtrl = m_encpakParams.mbctrinFile != NULL;
        bool MBQP = m_encpakParams.mbQpFile != NULL;

        bool MBStatOut = m_encpakParams.mbstatoutFile != NULL;
        bool MVOut = m_encpakParams.mvoutFile != NULL;
        bool MBCodeOut = m_encpakParams.mbcodeoutFile != NULL;

        /*SPS header */
        m_feiSPS.Header.BufferId = MFX_EXTBUFF_FEI_SPS;
        m_feiSPS.Header.BufferSz = sizeof(mfxExtFeiSPS);
        m_feiSPS.Pack = m_encpakParams.bPassHeaders ? 1 : 0; /* MSDK will use this data to do packing*/
        m_feiSPS.SPSId = 0;
        m_feiSPS.Profile = 100; //MFX_PROFILE_AVC_HIGH
        m_feiSPS.Level = 42; //MFX_LEVEL_AVC_42

        m_feiSPS.NumRefFrame = m_mfxEncParams.mfx.NumRefFrame;
        m_feiSPS.WidthInMBs = m_mfxEncParams.mfx.FrameInfo.Width/16;
        m_feiSPS.HeightInMBs = m_mfxEncParams.mfx.FrameInfo.Height/16;

        m_feiSPS.ChromaFormatIdc = m_mfxEncParams.mfx.FrameInfo.ChromaFormat;
        m_feiSPS.FrameMBsOnlyFlag = (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;
        m_feiSPS.MBAdaptiveFrameFieldFlag = 0;
        m_feiSPS.Direct8x8InferenceFlag = 1;
        m_feiSPS.Log2MaxFrameNum = 4;
        m_feiSPS.PicOrderCntType = GetDefaultPicOrderCount(m_mfxEncParams);
        m_feiSPS.Log2MaxPicOrderCntLsb = 0;
        m_feiSPS.DeltaPicOrderAlwaysZeroFlag = 1;

        for (fieldId = 0; fieldId < numOfFields; fieldId++)
        {
            memset(&feiEncCtrl[fieldId], 0, sizeof (mfxExtFeiEncFrameCtrl));
            feiEncCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
            feiEncCtrl[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncFrameCtrl);
            feiEncCtrl[fieldId].SearchPath = m_encpakParams.SearchPath;//1;
            feiEncCtrl[fieldId].LenSP      = m_encpakParams.LenSP;//57;
            feiEncCtrl[fieldId].SubMBPartMask = m_encpakParams.SubMBPartMask;//0x77;
            feiEncCtrl[fieldId].MultiPredL0 = m_encpakParams.MultiPredL0;
            feiEncCtrl[fieldId].MultiPredL1 = m_encpakParams.MultiPredL1;
            feiEncCtrl[fieldId].SubPelMode = m_encpakParams.SubPelMode;//3;
            feiEncCtrl[fieldId].InterSAD = m_encpakParams.InterSAD;//2;
            feiEncCtrl[fieldId].IntraSAD = m_encpakParams.IntraSAD;//2;
            feiEncCtrl[fieldId].DistortionType = m_encpakParams.DistortionType;
            feiEncCtrl[fieldId].RepartitionCheckEnable = m_encpakParams.RepartitionCheckEnable;
            feiEncCtrl[fieldId].AdaptiveSearch = m_encpakParams.AdaptiveSearch;//1;
            feiEncCtrl[fieldId].MVPredictor = MVPredictors;
            feiEncCtrl[fieldId].NumMVPredictors = m_encpakParams.NumMVPredictors; //always 4 predictors
            feiEncCtrl[fieldId].PerMBQp = MBQP;
            feiEncCtrl[fieldId].PerMBInput = MBCtrl;
            feiEncCtrl[fieldId].MBSizeCtrl = m_encpakParams.bMBSize;
            feiEncCtrl[fieldId].ColocatedMbDistortion = m_encpakParams.ColocatedMbDistortion;
            //Note:
            //(RefHeight x RefWidth) should not exceed 2048 for P frames and 1024 for B frames
            feiEncCtrl[fieldId].RefHeight = m_encpakParams.RefHeight;//40;
            feiEncCtrl[fieldId].RefWidth  = m_encpakParams.RefWidth; //48;
            feiEncCtrl[fieldId].SearchWindow = m_encpakParams.SearchWindow;//1;

            /* PPS header */
            m_feiPPS[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PPS;
            m_feiPPS[fieldId].Header.BufferSz = sizeof(mfxExtFeiPPS);
            m_feiPPS[fieldId].Pack = m_encpakParams.bPassHeaders ? 1 : 0;

            m_feiPPS[fieldId].SPSId = 0;
            m_feiPPS[fieldId].PPSId = 0;

            m_feiPPS[fieldId].FrameNum = 0;

            if (0 != m_encpakParams.QP)
                m_feiPPS[fieldId].PicInitQP = m_encpakParams.QP;
            else
                m_feiPPS[fieldId].PicInitQP = 26;
            m_feiPPS[fieldId].NumRefIdxL0Active = 1;
            m_feiPPS[fieldId].NumRefIdxL1Active = 1;

            m_feiPPS[fieldId].ChromaQPIndexOffset = 0;
            m_feiPPS[fieldId].SecondChromaQPIndexOffset = 0;

            m_feiPPS[fieldId].IDRPicFlag = 0;
            m_feiPPS[fieldId].ReferencePicFlag = 0;
            m_feiPPS[fieldId].EntropyCodingModeFlag = 1;
            m_feiPPS[fieldId].ConstrainedIntraPredFlag = 0;
            m_feiPPS[fieldId].Transform8x8ModeFlag = 1;

            m_feiSliceHeader[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_SLICE;
            m_feiSliceHeader[fieldId].Header.BufferSz = sizeof(mfxExtFeiSliceHeader);
            m_feiSliceHeader[fieldId].Pack = m_encpakParams.bPassHeaders ? 1 : 0;
            m_feiSliceHeader[fieldId].NumSlice =
            m_feiSliceHeader[fieldId].NumSliceAlloc = m_encpakParams.numSlices;
            m_feiSliceHeader[fieldId].Slice = new mfxExtFeiSliceHeader::mfxSlice [m_encpakParams.numSlices];
            // TODO: Implement real slice divider
            // For now only one slice is supported
            for (int numSlice = 0; numSlice < m_feiSliceHeader[fieldId].NumSlice; numSlice++ )
            {
                m_feiSliceHeader[fieldId].Slice->MBAaddress = 0;
                m_feiSliceHeader[fieldId].Slice->NumMBs = m_feiSPS.WidthInMBs * m_feiSPS.HeightInMBs;
                m_feiSliceHeader[fieldId].Slice->SliceType = 0;
                m_feiSliceHeader[fieldId].Slice->PPSId = m_feiPPS[fieldId].PPSId;
                m_feiSliceHeader[fieldId].Slice->IdrPicId = 0;

                m_feiSliceHeader[fieldId].Slice->CabacInitIdc = 0;

                m_feiSliceHeader[fieldId].Slice->SliceQPDelta = 26 - m_feiPPS[fieldId].PicInitQP;
                m_feiSliceHeader[fieldId].Slice->DisableDeblockingFilterIdc = 0;
                m_feiSliceHeader[fieldId].Slice->SliceAlphaC0OffsetDiv2 = 0;
                m_feiSliceHeader[fieldId].Slice->SliceBetaOffsetDiv2 = 0;
                /* This part will be filled in reference frame management */
                //m_feiSliceHeader[fieldId].Slice->RefL0[0].Index = 0;
                //m_feiSliceHeader[fieldId].Slice->RefL0[0].PictureType = 0;
                //m_feiSliceHeader[fieldId].Slice->RefL1[0].Index = 0;
                //m_feiSliceHeader[fieldId].Slice->RefL1[0].PictureType = 0;
            }

            //feiEncMVPredictors;
            if (MVPredictors) {
                feiEncMVPredictors[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV_PRED;
                feiEncMVPredictors[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncMVPredictors);
                feiEncMVPredictors[fieldId].NumMBAlloc = numMB;
                feiEncMVPredictors[fieldId].MB = new mfxExtFeiEncMVPredictors::mfxMB [numMB];
                //inBufs[numExtInParams++] = (mfxExtBuffer*) &feiEncMVPredictors;

                if (!m_encpakParams.bPREENC) { //not load if we couple with PREENC
                    printf("Using MV input file: %s\n", m_encpakParams.mvinFile);
                    MSDK_FOPEN(pMvPred,m_encpakParams.mvinFile, MSDK_CHAR("rb"));
                    if (pMvPred == NULL) {
                        fprintf(stderr, "Can't open file %s\n", m_encpakParams.mvinFile);
                        exit(-1);
                    }
                    if (m_encpakParams.bRepackPreencMV){
                        tmpForMedian  = new mfxI16[16];
                        tmpForReading = new mfxExtFeiPreEncMV::mfxMB[numMB];
                    }
                }else {
                    tmpForMedian = new mfxI16[16];
                }
            }

            if (MBCtrl) {
                feiEncMBCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB;
                feiEncMBCtrl[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncMBCtrl);
                feiEncMBCtrl[fieldId].NumMBAlloc = numMB;
                feiEncMBCtrl[fieldId].MB = new mfxExtFeiEncMBCtrl::mfxMB [numMB];
                //inBufs[numExtInParams++] = (mfxExtBuffer*) &feiEncMBCtrl;

                printf("Using MB control input file: %s\n", m_encpakParams.mbctrinFile);
                MSDK_FOPEN(pEncMBs, m_encpakParams.mbctrinFile, MSDK_CHAR("rb"));

                if ( pEncMBs == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbctrinFile);
                    exit(-1);
                }
            }

            if (MBQP) {
                feiEncMbQp[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
                feiEncMbQp[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncQP);
                feiEncMbQp[fieldId].NumQPAlloc = numMB;
                feiEncMbQp[fieldId].QP = new mfxU8 [numMB];
                //inBufs[numExtInParams++] = (mfxExtBuffer*) &feiEncMbQp;

                printf("Use MB QP input file: %s\n", m_encpakParams.mbQpFile);
                MSDK_FOPEN(pPerMbQP, m_encpakParams.mbQpFile, MSDK_CHAR("rb"));

                if ( pPerMbQP == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbQpFile);
                    exit(-1);
                }
            }

            //Open output files if any
            //distortion buffer have to be always provided - current limitation
            if(MBStatOut){
                feiEncMbStat[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB_STAT;
                feiEncMbStat[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncMBStat);
                feiEncMbStat[fieldId].NumMBAlloc = numMB;
                feiEncMbStat[fieldId].MB = new mfxExtFeiEncMBStat::mfxMB [numMB];
                //outBufs[numExtOutParams++] = (mfxExtBuffer*) &feiEncMbStat;

                printf("Use MB distortion output file: %s\n", m_encpakParams.mbstatoutFile);
                MSDK_FOPEN(mbstatout, m_encpakParams.mbstatoutFile, MSDK_CHAR("wb"));

                if ( mbstatout == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbstatoutFile);
                    exit(-1);
                }
            }

            //distortion buffer have to be always provided - current limitation
            if(MVOut){
                feiEncMV[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV;
                feiEncMV[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncMV);
                feiEncMV[fieldId].NumMBAlloc = numMB;
                feiEncMV[fieldId].MB = new mfxExtFeiEncMV::mfxMB [numMB];
                //outBufs[numExtOutParams++] = (mfxExtBuffer*) &feiEncMV;

                printf("Use MV output file: %s\n", m_encpakParams.mvoutFile);
                if (m_encpakParams.bOnlyPAK)
                    MSDK_FOPEN(mvENCPAKout, m_encpakParams.mvoutFile, MSDK_CHAR("rb"));
                else { /*for all other cases need to wtite into this file*/
                    MSDK_FOPEN(mvENCPAKout, m_encpakParams.mvoutFile, MSDK_CHAR("wb"));
                    tmpMBenc = new mfxExtFeiEncMV::mfxMB;
                    memset(tmpMBenc, 0x8000, sizeof(*tmpMBenc));
                }

                if ( mvENCPAKout == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mvoutFile);
                    exit(-1);
                }
            }

            //distortion buffer have to be always provided - current limitation
            if(MBCodeOut){
                feiEncMBCode[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PAK_CTRL;
                feiEncMBCode[fieldId].Header.BufferSz = sizeof(mfxExtFeiPakMBCtrl);
                feiEncMBCode[fieldId].NumMBAlloc = numMB;
                feiEncMBCode[fieldId].MB = new mfxFeiPakMBCtrl [numMB];
                //outBufs[numExtOutParams++] = (mfxExtBuffer*) &feiEncMBCode;

                printf("Use MB code output file: %s\n", m_encpakParams.mbcodeoutFile);
                if (m_encpakParams.bOnlyPAK)
                    MSDK_FOPEN(mbcodeout, m_encpakParams.mbcodeoutFile, MSDK_CHAR("rb"));
                else
                    MSDK_FOPEN(mbcodeout, m_encpakParams.mbcodeoutFile, MSDK_CHAR("wb"));

                if ( mbcodeout == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbcodeoutFile);
                    exit(-1);
                }
            } // if(MBCodeOut){
        } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

        // lets count all control and Ext buffers...
        inBufsI[numExtInParamsI++] = (mfxExtBuffer*) & feiEncCtrl;
        inBufs[numExtInParams++]   = (mfxExtBuffer*) & feiEncCtrl;
        if (MBCtrl){
            inBufsI[numExtInParamsI++] = (mfxExtBuffer*) &feiEncMBCtrl;
            inBufs[numExtInParams++]   = (mfxExtBuffer*) &feiEncMBCtrl;
        }
        if (MBQP){
            inBufsI[numExtInParamsI++] = (mfxExtBuffer*) &feiEncMbQp;
            inBufs[numExtInParams++]   = (mfxExtBuffer*) &feiEncMbQp;
        }
        /*For SPS, PPS and slice headers*/
        if (m_feiSPS.Pack)
            inBufs[numExtInParams++] = (mfxExtBuffer*) &m_feiSPS;
        for (fieldId = 0; fieldId < numOfFields; fieldId++)
        {
            if (m_feiPPS[fieldId].Pack)
                inBufs[numExtInParams++] = (mfxExtBuffer*) &m_feiPPS[fieldId];
        }

        if (MVPredictors)
            inBufs[numExtInParams++] = (mfxExtBuffer*) &feiEncMVPredictors;

        ctr = &feiCtrl;
        feiCtrl.NumExtParam = numExtInParams;
        feiCtrl.ExtParam = &inBufs[0];

        if(MBStatOut)
            outBufs[numExtOutParams++]   = (mfxExtBuffer*) &feiEncMbStat;
        if(MBCodeOut)
            outBufs[numExtOutParams++]   = (mfxExtBuffer*) &feiEncMBCode;
        if(MVOut)
            outBufs[numExtOutParams++] = (mfxExtBuffer*) &feiEncMV;
    } // if (m_encpakParams.bENCODE) {

    // Since in sample we support just 2 views
    // we will change this value between 0 and 1 in case of MVC
    mfxU16 currViewNum = 0;

    sts = MFX_ERR_NONE;

    bool twoEncoders = m_encpakParams.bPREENC && (m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC);
    // main loop, preprocessing and encoding
    mfxU32 frameCount = 0;
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
    {
        if(m_encpakParams.nNumFrames && frameCount >= m_encpakParams.nNumFrames)
            break;

        // get a pointer to a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);

        iTask* eTask = NULL;
        if ((m_encpakParams.bPREENC) || (m_encpakParams.bENCPAK) ||
                (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK) ){
            eTask = new iTask;
            eTask->frameType = GetFrameType(frameCount);
            eTask->frameDisplayOrder = frameCount;
            eTask->encoded = 0;
            mdprintf(stderr,"frame: %d  t:%d pt=%p ", frameCount, eTask->frameType, eTask);
        }

        pCurrentTask->mfxBS.FrameType = GetFrameType(frameCount);

        // correct ref window size if search window is 0
        if ((m_encpakParams.SearchWindow == 0) && (m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC || m_encpakParams.bENCODE)){
            // note that for preenc max size is 2048 because preenc doesn't do bidirectional prediction
            int frame_type = GetFrameType(frameCount);
            for (fieldId = 0; fieldId < numOfFields; fieldId++)
            {
                feiEncCtrl[fieldId].RefHeight = m_encpakParams.RefHeight;
                feiEncCtrl[fieldId].RefWidth  = m_encpakParams.RefWidth;
                int sqr = feiEncCtrl[fieldId].RefHeight * feiEncCtrl[fieldId].RefWidth;
                if ((frame_type & MFX_FRAMETYPE_B) && sqr > 1024){
                    feiEncCtrl[fieldId].RefHeight = 32;
                    feiEncCtrl[fieldId].RefWidth  = 32;
                }
                else if ((frame_type & MFX_FRAMETYPE_P) && sqr > 2048){
                    feiEncCtrl[fieldId].RefHeight = 32;
                    feiEncCtrl[fieldId].RefWidth  = 64;
                }
            }
        }

        // find free surface for encoder input
        nEncSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
        MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
        // point pSurf to encoder surface
        pSurf = &m_pEncSurfaces[nEncSurfIdx];

        if ((m_pmfxENC) ||(m_pmfxPAK))
        {
            nReconSurfIdx = GetFreeSurface(m_pReconSurfaces, m_ReconResponse.NumFrameActual);
            MSDK_CHECK_ERROR(nReconSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
            pReconSurf = &m_pReconSurfaces[nReconSurfIdx];
        }
//        else
//        {
//            nReconSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
//            MSDK_CHECK_ERROR(nReconSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
//            pReconSurf = &m_pEncSurfaces[nReconSurfIdx];
//        }

        frameCount++;


        // load frame from file to surface data
        // if we share allocator with Media SDK we need to call Lock to access surface data and...
        if (m_bExternalAlloc)
        {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
            MSDK_BREAK_ON_ERROR(sts);
        }

        pSurf->Info.FrameId.ViewId = currViewNum;
        sts = m_FileReader.LoadNextFrame(pSurf);
        MSDK_BREAK_ON_ERROR(sts);

        // ... after we're done call Unlock
        if (m_bExternalAlloc)
        {
            sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
            MSDK_BREAK_ON_ERROR(sts);
        }


        if (m_encpakParams.bPREENC) {
            pSurf->Data.Locked++;
            eTask->in.InSurface = pSurf;

            inputTasks.push_back(eTask); //inputTasks in display order

            eTask = findFrameToEncode();
            if(eTask == NULL) continue; //not found frame to encode

            eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;
            initFrameParams(eTask);

            for (fieldId = 0; fieldId < numOfFields; fieldId++)
            {
                mfxExtFeiPreEncMVPredictors* pMvPredBuf = NULL;
                mfxExtFeiEncQP*              pMbQP      = NULL;
                for(int i=0; i<eTask->in.NumExtParam; i++){
                    if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MV_PRED && feiEncMVPredictors){
                        if (!(eTask->frameType & MFX_FRAMETYPE_I)){
                            pMvPredBuf = &((mfxExtFeiPreEncMVPredictors*)(eTask->in.ExtParam[i]))[fieldId];
                            fread(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred);
                        } else {
                            fseek(pMvPred, sizeof(mfxExtFeiPreEncMVPredictors::mfxMB)*numMB, SEEK_CUR);
                        }
                    }
                    if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP && feiEncMbQp){
                        pMbQP = &((mfxExtFeiEncQP*)(eTask->in.ExtParam[i]))[fieldId];
                        fseek(pPerMbQP, (frameCount-1)*pMbQP->NumQPAlloc * numOfFields + pMbQP->NumQPAlloc * fieldId, SEEK_SET);
                        fread(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP);
                    }
                } //
            } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

            for (;;) {
                fprintf(stderr, "frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);

                //time_t timer; time(&timer);
                sts = m_pmfxPREENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                sts = MFX_ERR_NONE;
                /*PRE-ENC is running in separate session */
                sts = m_preenc_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                //fprintf(stderr, "synced : %d time spent: %d\n", sts, time(NULL)-timer);
                fprintf(stderr, "synced : %d\n", sts);


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
            if ( (mfxU32)(this->m_refDist * (twoEncoders? 4 : 2)) == inputTasks.size()) {
                inputTasks.front()->in.InSurface->Data.Locked--;
                if (twoEncoders)
                    inputTasks.front()->in.InSurface->Data.Locked--;
                delete inputTasks.front();
                //remove prevTask
                inputTasks.pop_front();
                if (twoEncoders)
                    inputTasks.pop_front();
            }

            //drop output data to output file
            //pCurrentTask->WriteBitstream();

            for (fieldId = 0; fieldId < numOfFields; fieldId++)
            {
                if (mbstatout)
                    fwrite(mbdata[fieldId].MB, sizeof (mbdata[fieldId].MB[0]) * mbdata[fieldId].NumMBAlloc, 1, mbstatout);

                if (mvout){
                    if (!(eTask->frameType&MFX_FRAMETYPE_I)){
                        fwrite(mvs[fieldId].MB, sizeof(mvs[fieldId].MB[0]) * mvs[fieldId].NumMBAlloc, 1, mvout);
                    } else {
                        for (int k = 0; k < numMB; k++)
                            fwrite(tmpMBpreenc, sizeof(*tmpMBpreenc), 1, mvout);
                    }
                }
            }

            if(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC) {
                ctr->FrameType = eTask->frameType;
                //m_pEncSurfaces[nEncSurfIdx].Data.FrameOrder = eTask->frameDisplayOrder;
                eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;

                if (!(ctr->FrameType & MFX_FRAMETYPE_I)){
                    //init MV predictors
                    for (fieldId = 0; fieldId < numOfFields; fieldId++)
                    {
                        repackPreenc2Enc(mvs[fieldId].MB, feiEncMVPredictors[fieldId].MB, mvs[fieldId].NumMBAlloc, tmpForMedian);
                    }
                }
            }
        } // if (m_encpakParams.bPREENC)

        /* ENC_and_PAK or ENC only or PAK only call */
        if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) || (m_encpakParams.bOnlyENC) ){
            pSurf->Data.Locked++;
            eTask->in.InSurface    = pSurf;
            eTask->inPAK.InSurface = pSurf;
            eTask->encoded = 0;
            /* this is Recon surface which will be generated by FEI PAK*/
            pReconSurf->Data.Locked++;
            eTask->out.OutSurface = eTask->outPAK.OutSurface = pReconSurf;
            eTask->outPAK.Bs = &pCurrentTask->mfxBS;

            inputTasks.push_back(eTask); //inputTasks in display order

            eTask = findFrameToEncode();
            if(eTask == NULL)
                continue; //not found frame to encode

            eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;
            initEncFrameParams(eTask);

            /* Load input Buffer for FEI ENCODE and FEI ENC */
            for (fieldId = 0; fieldId < numOfFields; fieldId++)
            {
                mfxExtFeiEncMVPredictors* pMvPredBuf = NULL;
                mfxExtFeiEncMBCtrl*       pMbEncCtrl = NULL;
                mfxExtFeiEncQP*           pMbQP      = NULL;
                for(int i=0; i<eTask->in.NumExtParam; i++){
                    if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV_PRED && feiEncMVPredictors && !m_encpakParams.bPREENC){

                        if (!(eTask->frameType & MFX_FRAMETYPE_I)){
                            if (m_encpakParams.bRepackPreencMV){
                                fread(tmpForReading, sizeof(*tmpForReading)*numMB, 1, pMvPred);
                                repackPreenc2Enc(tmpForReading, feiEncMVPredictors[fieldId].MB, numMB, tmpForMedian);
                            }
                            else {
                                pMvPredBuf = &((mfxExtFeiEncMVPredictors*)(eTask->in.ExtParam[i]))[fieldId];
                                fread(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred);
                            }
                        } else {
                            long shft = m_encpakParams.bRepackPreencMV ? sizeof(mfxExtFeiPreEncMV::mfxMB)*numMB
                                : sizeof(mfxExtFeiEncMVPredictors::mfxMB)*numMB;
                            fseek(pMvPred, shft, SEEK_CUR);
                        }
                    }
                    if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB && feiEncMBCtrl){
                        pMbEncCtrl = &((mfxExtFeiEncMBCtrl*)(eTask->in.ExtParam[i]))[fieldId];
                        fread(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs);
                    }
                    if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP && feiEncMbQp){
                        pMbQP = &((mfxExtFeiEncQP*)(eTask->in.ExtParam[i]))[fieldId];
                        fseek(pPerMbQP, (frameCount-1)*pMbQP->NumQPAlloc * numOfFields + pMbQP->NumQPAlloc * fieldId, SEEK_SET);
                        fread(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP);
                    }
                } //
            } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

            for (;;) {
                fprintf(stderr, "frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);

                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
                {
                    sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                    sts = MFX_ERR_NONE;
                    sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                    fprintf(stderr, "synced : %d\n", sts);
                }

                /* In case of PAK only we need to read data from file */
                if (m_encpakParams.bOnlyPAK)
                {
                    for (fieldId = 0; fieldId < numOfFields; fieldId++)
                    {
                        mfxExtFeiEncMV*     mvBuf     = NULL;
                        mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
                        for(int i=0; i<eTask->inPAK.NumExtParam; i++){
                            if(eTask->inPAK.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV && mvENCPAKout){
                                mvBuf = &((mfxExtFeiEncMV*) (eTask->inPAK.ExtParam[i]))[fieldId];
                                fread(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                            }
                            if(eTask->inPAK.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL && mbcodeout){
                                mbcodeBuf = &((mfxExtFeiPakMBCtrl*)(eTask->inPAK.ExtParam[i])) [fieldId];
                                fread(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
                            }
                        }
                    } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
                } // if (m_encpakParams.bOnlyPAK)

                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                {
                    sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
                    sts = MFX_ERR_NONE;
                    sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                    fprintf(stderr, "synced : %d\n", sts);
                }

                if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                } else if (MFX_ERR_NONE < sts && eTask->EncSyncP) {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    break;
                } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                    sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                } else {
                    // get next surface and new task for 2nd bitstream in ViewOutput mode
                    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                    break;
                }
            }

            eTask->encoded = 1;
            if ( (mfxU32)(this->m_refDist * (twoEncoders? 4: 2)) == inputTasks.size()) {
                inputTasks.front()->in.InSurface->Data.Locked--;
                if (twoEncoders)
                    inputTasks.front()->in.InSurface->Data.Locked--;

                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK) )
                    inputTasks.front()->outPAK.OutSurface->Data.Locked--;

                delete inputTasks.front();
                //remove prevTask
                inputTasks.pop_front();
                if (twoEncoders)
                    inputTasks.pop_front();
            }

            //drop output data to output file
            if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                pCurrentTask->WriteBitstream();

            if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
            {
                for (fieldId = 0; fieldId < numOfFields; fieldId++)
                {
                    mfxExtFeiEncMV*     mvBuf     = NULL;
                    mfxExtFeiEncMBStat* mbstatBuf = NULL;
                    mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
                    for (int i = 0; i < eTask->out.NumExtParam; i++){
                        if (eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV && mvENCPAKout){

                            if (!(eTask->frameType&MFX_FRAMETYPE_I)){
                                mvBuf = &((mfxExtFeiEncMV*)(eTask->out.ExtParam[i]))[fieldId];
                                fwrite(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                            }
                            else {
                                for (int k = 0; k < numMB; k++)
                                    fwrite(tmpMBenc, sizeof(*tmpMBenc), 1, mvENCPAKout);
                            }
                        }
                        if (eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT && mbstatout){
                            mbstatBuf = &((mfxExtFeiEncMBStat*)(eTask->out.ExtParam[i]))[fieldId];
                            fwrite(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, mbstatout);
                        }
                        if (eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL && mbcodeout){
                            mbcodeBuf = &((mfxExtFeiPakMBCtrl*)(eTask->out.ExtParam[i]))[fieldId];
                            fwrite(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
                        }
                    } // for(int i=0; i<eTask->out.NumExtParam; i++){
                } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
            } // if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
        } // if (m_encpakParams.bENCPAK)

        if (m_encpakParams.bENCODE) {
            for (;;) {
                // at this point surface for encoder contains either a frame from file or a frame processed by vpp

                int frame_type = GetFrameType(frameCount - 1);
                mfxFrameSurface1* encodeSurface = m_encpakParams.bPREENC ? eTask->in.InSurface : &m_pEncSurfaces[nEncSurfIdx];

                /* Load input Buffer for FEI ENCODE and FEI ENC */
                for (fieldId = 0; fieldId < numOfFields; fieldId++)
                {
                    mfxExtFeiEncMVPredictors* pMvPredBuf = NULL;
                    mfxExtFeiEncMBCtrl*       pMbEncCtrl = NULL;
                    mfxExtFeiEncQP*           pMbQP = NULL;
                    for (int i = 0; i<numExtInParams; i++){
                        if (inBufs[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV_PRED && feiEncMVPredictors && !m_encpakParams.bPREENC){

                            if (!(frame_type & MFX_FRAMETYPE_I)){
                                if (m_encpakParams.bRepackPreencMV){
                                    fread(tmpForReading, sizeof(*tmpForReading)*numMB, 1, pMvPred);
                                    repackPreenc2Enc(tmpForReading, feiEncMVPredictors[fieldId].MB, numMB, tmpForMedian);
                                }
                                else {
                                    pMvPredBuf = &((mfxExtFeiEncMVPredictors*)(inBufs[i]))[fieldId];
                                    fread(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred);
                                }
                            } else {
                                long shft = m_encpakParams.bRepackPreencMV ? sizeof(mfxExtFeiPreEncMV::mfxMB)*numMB
                                    : sizeof(mfxExtFeiEncMVPredictors::mfxMB)*numMB;
                                fseek(pMvPred, shft, SEEK_CUR);
                            }
                        }
                        if (inBufs[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB && feiEncMBCtrl){
                            pMbEncCtrl = &((mfxExtFeiEncMBCtrl*)(inBufs[i]))[fieldId];
                            fread(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs);
                        }
                        if (inBufs[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP && feiEncMbQp){
                            pMbQP = &((mfxExtFeiEncQP*)(inBufs[i]))[fieldId];
                            fseek(pPerMbQP, (frameCount - 1)*pMbQP->NumQPAlloc * numOfFields + pMbQP->NumQPAlloc * fieldId, SEEK_SET);
                            fread(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP);
                        }
                    } //
                } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

                // Add input buffers
                if (numExtInParams > 0) {
                    if (frame_type & MFX_FRAMETYPE_I){
                        encodeSurface->Data.NumExtParam = numExtInParamsI;
                        encodeSurface->Data.ExtParam = &inBufsI[0];
                        ctr->NumExtParam = numExtInParamsI;
                        ctr->ExtParam = &inBufsI[0];
                        feiEncCtrl->MVPredictor = 0;
                    }else{
                        encodeSurface->Data.NumExtParam = numExtInParams;
                        encodeSurface->Data.ExtParam = &inBufs[0];
                        ctr->NumExtParam = numExtInParams;
                        ctr->ExtParam = &inBufs[0];
                        feiEncCtrl->MVPredictor = numExtInParams != numExtInParamsI;
                    }
                }

                // Add output buffers
                if (numExtOutParams > 0) {
                    pCurrentTask->mfxBS.NumExtParam = numExtOutParams;
                    pCurrentTask->mfxBS.ExtParam = &outBufs[0];
                }

                sts = m_pmfxENCODE->EncodeFrameAsync(ctr, encodeSurface, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

                if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                } else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP) {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    break;
                } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                    sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                } else {
                    // get next surface and new task for 2nd bitstream in ViewOutput mode
                    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                    if (pCurrentTask->EncSyncP)
                    {
                        sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                        MSDK_BREAK_ON_ERROR(sts);
                        sts = SynchronizeFirstTask();
                        MSDK_BREAK_ON_ERROR(sts);
                    }
                    break;
                }
            }
        }
    } // while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)

    // means that the input file has ended, need to go to buffering loops
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    // loop to get buffered frames from encoder
    if (m_encpakParams.bPREENC) {
        mdprintf(stderr,"input_tasks_size=%u\n", (mfxU32)inputTasks.size());
        //encode last frames
        std::list<iTask*>::iterator it = inputTasks.begin();
        mfxI32 numUnencoded = 0;
        for(;it != inputTasks.end(); ++it){
            if((*it)->encoded == 0){
                numUnencoded++;
            }
        }

        //run processing on last frames
        if (numUnencoded > 0) {
            if (inputTasks.back()->frameType & MFX_FRAMETYPE_B) {
                inputTasks.back()->frameType = MFX_FRAMETYPE_P;
            }
            mdprintf(stderr, "run processing on last frames: %d\n", numUnencoded);
            while (numUnencoded != 0) {
                // get a pointer to a free task (bit stream and sync point for encoder)
                sts = GetFreeTask(&pCurrentTask);
                MSDK_BREAK_ON_ERROR(sts);

                iTask* eTask = findFrameToEncode();
                if (eTask == NULL) continue; //not found frame to encode

                initFrameParams(eTask);

                for (fieldId = 0; fieldId < numOfFields; fieldId++)
                {
                    mfxExtFeiPreEncMVPredictors* pMvPredBuf = NULL;
                    mfxExtFeiEncQP*              pMbQP      = NULL;
                    for(int i=0; i<eTask->in.NumExtParam; i++){
                        if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MV_PRED && feiEncMVPredictors){
                            if (!(eTask->frameType & MFX_FRAMETYPE_I)){
                                pMvPredBuf = &((mfxExtFeiPreEncMVPredictors*)(eTask->in.ExtParam[i]))[fieldId];
                                fread(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred);
                            } else {
                                fseek(pMvPred, sizeof(mfxExtFeiPreEncMVPredictors::mfxMB)*numMB, SEEK_CUR);
                            }
                        }
                        if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP && feiEncMbQp){
                            pMbQP = &((mfxExtFeiEncQP*)(eTask->in.ExtParam[i]))[fieldId];
                            fseek(pPerMbQP, (frameCount-1)*pMbQP->NumQPAlloc * numOfFields + pMbQP->NumQPAlloc * fieldId, SEEK_SET);
                            fread(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP);
                        }
                    } //
                } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

                for (;;) {
                    //Only synced operation supported for now
                    fprintf(stderr, "frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);
                    sts = m_pmfxPREENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                    /*PRE-ENC is running in separate session */
                    sts = m_preenc_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                    fprintf(stderr, "synced : %d\n", sts);

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

                for (fieldId = 0; fieldId < numOfFields; fieldId++)
                {
                    if (mbstatout)
                        fwrite(mbdata[fieldId].MB, sizeof (mbdata[fieldId].MB[0]) * mbdata[fieldId].NumMBAlloc, 1, mbstatout);
                    if (mvout){
                        if (!(eTask->frameType&MFX_FRAMETYPE_I)){
                            fwrite(mvs[fieldId].MB, sizeof(mvs[fieldId].MB[0]) * mvs[fieldId].NumMBAlloc, 1, mvout);
                        } else {
                            for (int k = 0; k < numMB; k++)
                                fwrite(tmpMBpreenc, sizeof(*tmpMBpreenc), 1, mvout);
                        }
                    }
                }

                //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                numUnencoded--;

                if (m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC) {
                    ctr->FrameType = eTask->frameType;
                    eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;

                    if (!(ctr->FrameType & MFX_FRAMETYPE_I)){
                        //init MV predictors
                        for (fieldId = 0; fieldId < numOfFields; fieldId++)
                        {
                            repackPreenc2Enc(mvs[fieldId].MB, feiEncMVPredictors[fieldId].MB, mvs[fieldId].NumMBAlloc, tmpForMedian);
                        }
                    }

                    // correct ref window size if search window is 0
                    if ((m_encpakParams.SearchWindow == 0) && (m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC || m_encpakParams.bENCODE)){
                        int frame_type = eTask->frameType;
                        for (fieldId = 0; fieldId < numOfFields; fieldId++)
                        {
                            feiEncCtrl[fieldId].RefHeight = m_encpakParams.RefHeight;
                            feiEncCtrl[fieldId].RefWidth  = m_encpakParams.RefWidth;
                            int sqr = feiEncCtrl[fieldId].RefHeight * feiEncCtrl[fieldId].RefWidth;
                            if ((frame_type & MFX_FRAMETYPE_B) && sqr > 1024){
                                feiEncCtrl[fieldId].RefHeight = 32;
                                feiEncCtrl[fieldId].RefWidth  = 32;
                            }
                            else if ((frame_type & MFX_FRAMETYPE_P) && sqr > 2048){
                                feiEncCtrl[fieldId].RefHeight = 32;
                                feiEncCtrl[fieldId].RefWidth  = 64;
                            }
                        }
                    }
                }

                if (m_encpakParams.bENCODE) { //if we have both
                    for (;;) {

                        /* Load input Buffer for FEI ENCODE and FEI ENC */
                        for (fieldId = 0; fieldId < numOfFields; fieldId++)
                        {
                            mfxExtFeiEncMBCtrl*       pMbEncCtrl = NULL;
                            mfxExtFeiEncQP*           pMbQP      = NULL;
                            for(int i=0; i<eTask->in.NumExtParam; i++){
                                if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB && feiEncMBCtrl){
                                    pMbEncCtrl = &((mfxExtFeiEncMBCtrl*)(eTask->in.ExtParam[i]))[fieldId];
                                    fread(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs);
                                }
                                if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP && feiEncMbQp){
                                    pMbQP = &((mfxExtFeiEncQP*)(eTask->in.ExtParam[i]))[fieldId];
                                    fseek(pPerMbQP, (frameCount-1)*pMbQP->NumQPAlloc * numOfFields + pMbQP->NumQPAlloc * fieldId, SEEK_SET);
                                    fread(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP);
                                }
                            } //
                        } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
                        //Add output buffers
                        if (numExtOutParams > 0) {
                            pCurrentTask->mfxBS.NumExtParam = numExtOutParams;
                            pCurrentTask->mfxBS.ExtParam    = &outBufs[0];
                        }
                        if (numExtInParams > 0) {
                            if (ctr->FrameType & MFX_FRAMETYPE_I){
                                eTask->in.InSurface->Data.NumExtParam = numExtInParamsI;
                                eTask->in.InSurface->Data.ExtParam    = &inBufsI[0];
                                ctr -> NumExtParam = numExtInParamsI;
                                ctr -> ExtParam    = &inBufsI[0];
                                feiEncCtrl -> MVPredictor = 0;
                            }else {
                                eTask->in.InSurface->Data.NumExtParam = numExtInParams;
                                eTask->in.InSurface->Data.ExtParam    = &inBufs[0];
                                ctr -> NumExtParam = numExtInParams;
                                ctr -> ExtParam    = &inBufs[0];
                                feiEncCtrl -> MVPredictor = numExtInParams != numExtInParamsI;
                            }
                        }
                        //no ctrl for buffered frames
                        //sts = m_pmfxENCODE->EncodeFrameAsync(ctr, NULL, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);
                        sts = m_pmfxENCODE->EncodeFrameAsync(ctr, eTask->in.InSurface, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

                        if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
                        {
                            if (MFX_WRN_DEVICE_BUSY == sts)
                                MSDK_SLEEP(1); // wait if device is busy
                        } else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP) {
                            sts = MFX_ERR_NONE; // ignore warnings if output is available
                            break;
                        } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                            sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                        } else {
                            // get new task for 2nd bitstream in ViewOutput mode
                            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                            if (pCurrentTask->EncSyncP)
                            {
                                sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                                MSDK_BREAK_ON_ERROR(sts);
                                //sts = SynchronizeFirstTask();
                                //MSDK_BREAK_ON_ERROR(sts);
                            }
                            break;
                        }
                    }
                    MSDK_BREAK_ON_ERROR(sts);
                /* FIXME ???*/
                //drop output data to output file
                //pCurrentTask->WriteBitstream();
                } // if (m_encpakParams.bENCODE) { //if we have both

                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK)){
                    // get a pointer to a free task (bit stream and sync point for encoder)
                    sts = GetFreeTask(&pCurrentTask);
                    MSDK_BREAK_ON_ERROR(sts);

                    eTask->encoded = 0; //because we not finished with it
                    eTask = findFrameToEncode();
                    if (eTask == NULL) continue; //not found frame to encode

                    eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;
                    initEncFrameParams(eTask);

                    /* Load input Buffer for FEI ENCODE and FEI ENC */
                    for (fieldId = 0; fieldId < numOfFields; fieldId++)
                    {
                        mfxExtFeiEncMBCtrl*       pMbEncCtrl = NULL;
                        mfxExtFeiEncQP*           pMbQP      = NULL;
                        for(int i=0; i<eTask->in.NumExtParam; i++){
                            if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB && feiEncMBCtrl){
                                pMbEncCtrl = &((mfxExtFeiEncMBCtrl*)(eTask->in.ExtParam[i]))[fieldId];
                                fread(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs);
                            }
                            if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP && feiEncMbQp){
                                pMbQP = &((mfxExtFeiEncQP*)(eTask->in.ExtParam[i]))[fieldId];
                                fseek(pPerMbQP, (frameCount-1)*pMbQP->NumQPAlloc * numOfFields + pMbQP->NumQPAlloc * fieldId, SEEK_SET);
                                fread(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP);
                            }
                        } //
                    } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

                    for (;;) {
                        //Only synced operation supported for now
                        if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
                        {
                            fprintf(stderr, "frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);
                            sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                            sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                            fprintf(stderr, "synced : %d\n", sts);
                            MSDK_BREAK_ON_ERROR(sts);
                        }

                        /* In case of PAK only we need to read data from file */
                        if (m_encpakParams.bOnlyPAK)
                        {
                            for (fieldId = 0; fieldId < numOfFields; fieldId++)
                            {
                                mfxExtFeiEncMV*     mvBuf     = NULL;
                                mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
                                for(int i=0; i<eTask->inPAK.NumExtParam; i++){
                                    if (eTask->inPAK.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV && mvENCPAKout){
                                        mvBuf = &((mfxExtFeiEncMV*)(eTask->inPAK.ExtParam[i]))[fieldId];
                                        fread(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                                    }
                                    if (eTask->inPAK.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL && mbcodeout){
                                        mbcodeBuf = &((mfxExtFeiPakMBCtrl*)(eTask->inPAK.ExtParam[i]))[fieldId];
                                        fread(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
                                    }
                                }
                            } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
                        } // if (m_encpakParams.bOnlyPAK)

                        if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                        {
                            sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
                            sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                            fprintf(stderr, "synced : %d\n", sts);
                            MSDK_BREAK_ON_ERROR(sts);
                        }

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
                    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                            pCurrentTask->WriteBitstream();

                    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
                    {
                        for (fieldId = 0; fieldId < numOfFields; fieldId++)
                        {
                            mfxExtFeiEncMV*     mvBuf     = NULL;
                            mfxExtFeiEncMBStat* mbstatBuf = NULL;
                            mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
                            for(int i=0; i<eTask->out.NumExtParam; i++){
                                if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV && mvENCPAKout){

                                    if (!(eTask->frameType&MFX_FRAMETYPE_I)){
                                        mvBuf = &((mfxExtFeiEncMV*)(eTask->out.ExtParam[i]))[fieldId];
                                        fwrite(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                                    }
                                    else {
                                        for (int k = 0; k < numMB; k++)
                                            fwrite(tmpMBenc, sizeof(*tmpMBenc), 1, mvENCPAKout);
                                    }
                                }
                                if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT && mbstatout){
                                    mbstatBuf = &((mfxExtFeiEncMBStat*)(eTask->out.ExtParam[i]))[fieldId];
                                    fwrite(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, mbstatout);
                                }
                                if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL && mbcodeout){
                                    mbcodeBuf = &((mfxExtFeiPakMBCtrl*)(eTask->out.ExtParam[i]))[fieldId];
                                    fwrite(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
                                }
                            } // for(int i=0; i<eTask->out.NumExtParam; i++){
                        } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
                    } //if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )

                    //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                    numUnencoded--;
                } // if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK))

            }  // while (numUnencoded != 0) {
        } //run processing on last frames  \n if (numUnencoded > 0) {

        //unlock last frames
        {
            std::list<iTask*>::iterator it = inputTasks.begin();
            for (; it != inputTasks.end(); ++it) {
                (*it)->in.InSurface->Data.Locked = 0;
                delete (*it);
                if (twoEncoders)
                    ++it;
            }
        }

        if (m_encpakParams.bENCODE) { //if we have both
            // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
            // indicates that there are no more buffered frames
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
            // exit in case of other errors
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // synchronize all tasks that are left in task pool
            while (MFX_ERR_NONE == sts) {
                sts = m_TaskPool.SynchronizeFirstTask();
            }
        }
    } // if (m_encpakParams.bPREENC)

    // loop to get buffered frames from encoder
    if (!m_encpakParams.bPREENC && ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK))){
        mdprintf(stderr,"input_tasks_size=%u\n", (mfxU32)inputTasks.size());
        //encode last frames
        std::list<iTask*>::iterator it = inputTasks.begin();
        mfxI32 numUnencoded = 0;
        for(;it != inputTasks.end(); ++it){
            if((*it)->encoded == 0){
                numUnencoded++;
            }
        }

        //run processing on last frames
        if (numUnencoded > 0) {
            if (inputTasks.back()->frameType & MFX_FRAMETYPE_B) {
                inputTasks.back()->frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            }
            mdprintf(stderr, "run processing on last frames: %d\n", numUnencoded);
            while (numUnencoded != 0) {
                // get a pointer to a free task (bit stream and sync point for encoder)
                sts = GetFreeTask(&pCurrentTask);
                MSDK_BREAK_ON_ERROR(sts);

                iTask* eTask = findFrameToEncode();
                if (eTask == NULL) continue; //not found frame to encode

                eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;
                initEncFrameParams(eTask);

                /* Load input Buffer for FEI ENCODE and FEI ENC */
                for (fieldId = 0; fieldId < numOfFields; fieldId++)
                {
                    mfxExtFeiEncMVPredictors* pMvPredBuf = NULL;
                    mfxExtFeiEncMBCtrl*       pMbEncCtrl = NULL;
                    mfxExtFeiEncQP*           pMbQP      = NULL;
                    for(int i=0; i<eTask->in.NumExtParam; i++){
                        if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV_PRED && feiEncMVPredictors){

                            if (!(eTask->frameType & MFX_FRAMETYPE_I)){
                                if (m_encpakParams.bRepackPreencMV){
                                    fread(tmpForReading, sizeof(*tmpForReading)*numMB, 1, pMvPred);
                                    repackPreenc2Enc(tmpForReading, feiEncMVPredictors[fieldId].MB, numMB, tmpForMedian);
                                }
                                else {
                                    pMvPredBuf = &((mfxExtFeiEncMVPredictors*)(eTask->in.ExtParam[i]))[fieldId];
                                    fread(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred);
                                }
                            }else {
                                long shft = m_encpakParams.bRepackPreencMV ? sizeof(mfxExtFeiPreEncMV::mfxMB)*numMB
                                    : sizeof(mfxExtFeiEncMVPredictors::mfxMB)*numMB;
                                fseek(pMvPred, shft, SEEK_CUR);
                            }
                        }
                        if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB && feiEncMBCtrl){
                            pMbEncCtrl = &((mfxExtFeiEncMBCtrl*)(eTask->in.ExtParam[i]))[fieldId];
                            fread(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs);
                        }
                        if(eTask->in.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP && feiEncMbQp){
                            pMbQP = &((mfxExtFeiEncQP*)(eTask->in.ExtParam[i]))[fieldId];
                            fseek(pPerMbQP, (frameCount-1)*pMbQP->NumQPAlloc * numOfFields + pMbQP->NumQPAlloc * fieldId, SEEK_SET);
                            fread(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP);
                        }
                    } //
                } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

                // correct ref window size if search window is 0
                if ((m_encpakParams.SearchWindow == 0) && (m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC)){
                    int frame_type = eTask->frameType;
                    for (fieldId = 0; fieldId < numOfFields; fieldId++)
                    {
                        feiEncCtrl[fieldId].RefHeight = m_encpakParams.RefHeight;
                        feiEncCtrl[fieldId].RefWidth  = m_encpakParams.RefWidth;
                        int sqr = feiEncCtrl[fieldId].RefHeight * feiEncCtrl[fieldId].RefWidth;
                        if ((frame_type & MFX_FRAMETYPE_B) && sqr > 1024){
                            feiEncCtrl[fieldId].RefHeight = 32;
                            feiEncCtrl[fieldId].RefWidth  = 32;
                        }
                        else if ((frame_type & MFX_FRAMETYPE_P) && sqr > 2048){
                            feiEncCtrl[fieldId].RefHeight = 32;
                            feiEncCtrl[fieldId].RefWidth  = 64;
                        }
                    }
                }

                for (;;) {
                    //Only synced operation supported for now
                    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
                    {
                        fprintf(stderr, "frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);
                        sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                        sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                        fprintf(stderr, "synced : %d\n", sts);
                        MSDK_BREAK_ON_ERROR(sts);
                    }

                    /* In case of PAK only we need to read data from file */
                    if (m_encpakParams.bOnlyPAK)
                    {
                        for (fieldId = 0; fieldId < numOfFields; fieldId++)
                        {
                            mfxExtFeiEncMV*     mvBuf     = NULL;
                            mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
                            for(int i=0; i<eTask->inPAK.NumExtParam; i++){
                                if(eTask->inPAK.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV && mvENCPAKout){
                                    mvBuf = &((mfxExtFeiEncMV*)(eTask->inPAK.ExtParam[i]))[fieldId];
                                    fread(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                                }
                                if(eTask->inPAK.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL && mbcodeout){
                                    mbcodeBuf = &((mfxExtFeiPakMBCtrl*)(eTask->inPAK.ExtParam[i]))[fieldId];
                                    fread(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
                                }
                            } //
                        } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
                    } // if (m_encpakParams.bOnlyPAK)

                    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                    {
                        sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
                        sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                        fprintf(stderr, "synced : %d\n", sts);
                        MSDK_BREAK_ON_ERROR(sts);
                    }

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
                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) )
                    pCurrentTask->WriteBitstream();

                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )
                {
                    for (fieldId = 0; fieldId < numOfFields; fieldId++)
                     {
                         mfxExtFeiEncMV*     mvBuf     = NULL;
                         mfxExtFeiEncMBStat* mbstatBuf = NULL;
                         mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
                         for(int i=0; i<eTask->out.NumExtParam; i++){
                             if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV && mvENCPAKout){
                                 if (!(eTask->frameType&MFX_FRAMETYPE_I)){
                                     mvBuf = &((mfxExtFeiEncMV*)(eTask->out.ExtParam[i]))[fieldId];
                                     fwrite(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                                 }
                                 else {
                                     for (int k = 0; k < numMB; k++)
                                         fwrite(tmpMBenc, sizeof(*tmpMBenc), 1, mvENCPAKout);
                                 }
                             }
                             if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT && mbstatout){
                                 mbstatBuf = &((mfxExtFeiEncMBStat*)(eTask->out.ExtParam[i]))[fieldId];
                                 fwrite(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, mbstatout);
                             }
                             if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL && mbcodeout){
                                 mbcodeBuf = &((mfxExtFeiPakMBCtrl*)(eTask->out.ExtParam[i]))[fieldId];
                                 fwrite(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
                             }
                         } // for(int i=0; i<eTask->out.NumExtParam; i++){
                     } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
                } //if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) )

                //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                numUnencoded--;
            }  // while (numUnencoded != 0) {
        } //run processing on last frames  \n if (numUnencoded > 0) {

        //unlock last frames
        {
            std::list<iTask*>::iterator it = inputTasks.begin();
            for (; it != inputTasks.end(); ++it) {
                (*it)->in.InSurface->Data.Locked = 0;
                if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK) )
                {
                    (*it)->outPAK.OutSurface->Data.Locked = 0;
                }
                delete (*it);
            }
        }
    } //     if (m_encpakParams.bENCPAK) {

    if (m_encpakParams.bENCODE && !m_encpakParams.bPREENC) {
        while (MFX_ERR_NONE <= sts) {
            // get a free task (bit stream and sync point for encoder)
            sts = GetFreeTask(&pCurrentTask);
            MSDK_BREAK_ON_ERROR(sts);

            for (;;) {
                //Add output buffers
                if (numExtOutParams > 0) {
                    pCurrentTask->mfxBS.NumExtParam = numExtOutParams;
                    pCurrentTask->mfxBS.ExtParam    = &outBufs[0];
                }
                //no ctrl for buffered frames
                sts = m_pmfxENCODE->EncodeFrameAsync(ctr, NULL, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

                if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                } else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP) {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    break;
                } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                    sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                } else {
                    // get new task for 2nd bitstream in ViewOutput mode
                    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                    if (pCurrentTask->EncSyncP)
                    {
                        sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
                        MSDK_BREAK_ON_ERROR(sts);
                    }
                    break;
                }
            }
            MSDK_BREAK_ON_ERROR(sts);
        }

        // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
        // indicates that there are no more buffered frames
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
        // exit in case of other errors
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // synchronize all tasks that are left in task pool
        while (MFX_ERR_NONE == sts) {
            sts = m_TaskPool.SynchronizeFirstTask();
        }
    }

    if ((m_encpakParams.bENCODE || m_encpakParams.bPREENC || m_encpakParams.bENCPAK) ||
            (m_encpakParams.bOnlyPAK) || (m_encpakParams.bOnlyENC) )
    {
        if(mvout != NULL)
            fclose(mvout);
        if(mbstatout != NULL)
            fclose(mbstatout);
        if(mbcodeout != NULL)
            fclose(mbcodeout);
        if(mvENCPAKout != NULL)
            fclose(mvENCPAKout);
        if(pMvPred != NULL)
            fclose(pMvPred);
        if(pEncMBs != NULL)
            fclose(pEncMBs);
        if (pPerMbQP != NULL)
            fclose(pPerMbQP);
    }

    if (m_encpakParams.bPREENC)
    {
        for (fieldId = 0; fieldId < numOfFields; fieldId++)
        {
            if ((preENCCtr[fieldId].MVPredictor) && (NULL != mvPreds[fieldId].MB))
            {
                delete (mvPreds[fieldId].MB);
                mvPreds[fieldId].MB = NULL;
            }

            if ((preENCCtr[fieldId].MBQp) && (NULL != qps[fieldId].QP))
            {
                delete (qps[fieldId].QP);
                qps[fieldId].QP = NULL;
            }

            if ((!preENCCtr[fieldId].DisableMVOutput) && (NULL != mvs[fieldId].MB))
            {
                delete (mvs[fieldId].MB);
                mvs[fieldId].MB = NULL;
            }

            if ((!preENCCtr[fieldId].DisableStatisticsOutput) && (NULL != mbdata[fieldId].MB))
            {
                delete (mbdata[fieldId].MB);
                mbdata[fieldId].MB = NULL;
            }
        }
        if (NULL != tmpMBpreenc){
            delete tmpMBpreenc;
            tmpMBpreenc = NULL;
        }
    } //if (m_encpakParams.bPREENC)


    if ((m_encpakParams.bENCODE)  || (m_encpakParams.bENCPAK)||
        (m_encpakParams.bOnlyPAK) || (m_encpakParams.bOnlyENC) )
    {
        for (fieldId = 0; fieldId < numOfFields; fieldId++)
        {
            if (NULL != feiEncMVPredictors[fieldId].MB)
            {
                delete (feiEncMVPredictors[fieldId].MB);
                feiEncMVPredictors[fieldId].MB =NULL;
            }
            if (NULL != feiEncMBCtrl[fieldId].MB)
            {
                delete(feiEncMBCtrl[fieldId].MB);
                feiEncMBCtrl[fieldId].MB = NULL;
            }

            if (NULL != feiEncMbQp[fieldId].QP)
            {
                delete (feiEncMbQp[fieldId].QP);
                feiEncMbQp[fieldId].QP = NULL;
            }

            if(NULL != feiEncMbStat[fieldId].MB)
            {
                delete (feiEncMbStat[fieldId].MB);
                feiEncMbStat[fieldId].MB = NULL;
            }
            if(NULL != feiEncMV[fieldId].MB)
            {
                delete (feiEncMV[fieldId].MB);
                feiEncMV[fieldId].MB = NULL;
            }

            if(NULL != feiEncMBCode[fieldId].MB)
            {
                delete(feiEncMBCode[fieldId].MB);
                feiEncMBCode[fieldId].MB = NULL;
            }

            if (NULL != m_feiSliceHeader[fieldId].Slice)
            {
                delete m_feiSliceHeader[fieldId].Slice;
                m_feiSliceHeader[fieldId].Slice = NULL;
            }
        } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
        if (NULL != tmpForMedian){
            delete [] tmpForMedian;
            tmpForMedian = NULL;
        }
        if (NULL != tmpForReading){
            delete [] tmpForReading;
            tmpForReading = NULL;
        }
        if (NULL != tmpMBenc){
            delete tmpMBenc;
            tmpMBenc = NULL;
        }
    }// if ((m_encpakParams.bENCODE)  || (m_encpakParams.bENCPAK)||


    // MFX_ERR_NOT_FOUND is the correct status to exit the loop with
    // EncodeFrameAsync and SyncOperation don't return this status
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND);
    // report any errors that occurred in asynchronous part
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

void repackPreenc2Enc(mfxExtFeiPreEncMV::mfxMB *preencMVoutMB, mfxExtFeiEncMVPredictors::mfxMB *EncMVPredMB, mfxU32 NumMB, mfxI16 *tmpBuf){

    memset(EncMVPredMB, 0, sizeof(*EncMVPredMB)*NumMB);
    for (mfxU32 i = 0; i<NumMB; i++){

        //only one ref is used for now
        for (int j = 0; j < 4; j++){
            EncMVPredMB[i].RefIdx[j].RefL0 = 0;
            EncMVPredMB[i].RefIdx[j].RefL1 = 0;
        }

        //use only first subblock component of MV
        for (int j = 0; j<2; j++){
            EncMVPredMB[i].MV[0][j].x = get16Median(preencMVoutMB+i, tmpBuf, 0, j);
            EncMVPredMB[i].MV[0][j].y = get16Median(preencMVoutMB+i, tmpBuf, 1, j);
        }
    } // for(mfxU32 i=0; i<NumMBAlloc; i++){
}

mfxI16 get16Median(mfxExtFeiPreEncMV::mfxMB* preencMB, mfxI16* tmpBuf, int xy, int L0L1){

    switch (xy){
    case 0:
        for (int k = 0; k < 16; k++){
            tmpBuf[k] = preencMB->MV[k][L0L1].x;
        }
        break;
    case 1:
        for (int k = 0; k < 16; k++){
            tmpBuf[k] = preencMB->MV[k][L0L1].y;
        }
        break;
    default:
        return 0;
    }

    std::sort(tmpBuf, tmpBuf + 16);
    return (tmpBuf[7] + tmpBuf[8]) / 2;
}

void CEncodingPipeline::PrintInfo()
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Encoding Sample Version %s\n"), MSDK_SAMPLE_VERSION);
    msdk_printf(MSDK_STRING("\nInput file format\t%s\n"), ColorFormatToStr(m_FileReader.m_ColorFormat));
    msdk_printf(MSDK_STRING("Output video\t\t%s\n"), CodecIdToStr(m_mfxEncParams.mfx.CodecId).c_str());

    mfxFrameInfo SrcPicInfo = m_mfxEncParams.vpp.In;
    mfxFrameInfo DstPicInfo = m_mfxEncParams.mfx.FrameInfo;

    msdk_printf(MSDK_STRING("Source picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), SrcPicInfo.Width, SrcPicInfo.Height);
    //msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), SrcPicInfo.CropX, SrcPicInfo.CropY, SrcPicInfo.CropW, SrcPicInfo.CropH);

    //msdk_printf(MSDK_STRING("Destination picture:\n"));
    //msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), DstPicInfo.Width, DstPicInfo.Height);
    //msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), DstPicInfo.CropX, DstPicInfo.CropY, DstPicInfo.CropW, DstPicInfo.CropH);

    msdk_printf(MSDK_STRING("Frame rate\t%.2f\n"), DstPicInfo.FrameRateExtN * 1.0 / DstPicInfo.FrameRateExtD);
    msdk_printf(MSDK_STRING("Bit rate(KBps)\t%d\n"), m_mfxEncParams.mfx.TargetKbps);
    msdk_printf(MSDK_STRING("Target usage\t%s\n"), TargetUsageToStr(m_mfxEncParams.mfx.TargetUsage));

    const msdk_char* sMemType = m_memType == D3D9_MEMORY  ? MSDK_STRING("d3d")
                       : (m_memType == D3D11_MEMORY ? MSDK_STRING("d3d11")
                                                    : MSDK_STRING("system"));
    msdk_printf(MSDK_STRING("Memory type\t%s\n"), sMemType);

    mfxIMPL impl;
    m_mfxSession.QueryIMPL(&impl);

    const msdk_char* sImpl = (MFX_IMPL_VIA_D3D11 == MFX_IMPL_VIA_MASK(impl)) ? MSDK_STRING("hw_d3d11")
                     : (MFX_IMPL_HARDWARE & impl)  ? MSDK_STRING("hw")
                                                   : MSDK_STRING("sw");
    msdk_printf(MSDK_STRING("Media SDK impl\t\t%s\n"), sImpl);

    mfxVersion ver;
    m_mfxSession.QueryVersion(&ver);
    msdk_printf(MSDK_STRING("Media SDK version\t%d.%d\n"), ver.Major, ver.Minor);

    msdk_printf(MSDK_STRING("\n"));
}

iTask* CEncodingPipeline::findFrameToEncode() {
    // at this point surface for encoder contains a frame from file
    std::list<iTask*>::iterator encTask = inputTasks.begin();

    iTask* prevTask = NULL;
    iTask* nextTask = NULL;
    //find task to encode
    mdprintf(stderr, "[");
    for (; encTask != inputTasks.end(); ++encTask) {
        mdprintf(stderr, "%d ", (*encTask)->frameType);
    }
    mdprintf(stderr, "] ");
    encTask = inputTasks.begin();
    for (; encTask != inputTasks.end(); ++encTask) {
        if ((*encTask)->encoded) continue;
        prevTask = NULL;
        nextTask = NULL;

        if ((*encTask)->frameType & MFX_FRAMETYPE_I) {
            break;
        } else if ((*encTask)->frameType & MFX_FRAMETYPE_P) { //find previous ref
            std::list<iTask*>::iterator it = encTask;
            while ((it--) != inputTasks.begin()) {
                iTask* curTask = *it;
                if (curTask->frameType & (MFX_FRAMETYPE_P | MFX_FRAMETYPE_I) &&
                        curTask->encoded) {
                    prevTask = curTask;
                    break;
                }
            }
            if (prevTask) break;
        } else if ((*encTask)->frameType & MFX_FRAMETYPE_B) {//find previous ref
            std::list<iTask*>::iterator it = encTask;
            while ((it--) != inputTasks.begin()) {
                iTask* curTask = *it;
                if (curTask->frameType & (MFX_FRAMETYPE_P | MFX_FRAMETYPE_I) &&
                        curTask->encoded) {
                    prevTask = curTask;
                    break;
                }
            }
            mdprintf(stderr, "pT=%p ", prevTask);

            //find next ref
            it = encTask;
            while ((++it) != inputTasks.end()) {
                iTask* curTask = *it;
                if (curTask->frameType & (MFX_FRAMETYPE_P | MFX_FRAMETYPE_I) &&
                        curTask->encoded) {
                    nextTask = curTask;
                    break;
                }
            }
            mdprintf(stderr, "nT=%p ", nextTask);
            if (prevTask && nextTask) break;
        }
    }

    mdprintf(stderr, "eT= %p in_size=%u prevTask=%p nextTask=%p ", encTask, (mfxU32)inputTasks.size(), prevTask, nextTask);

    if (encTask == inputTasks.end()) {
        mdprintf(stderr, "\n");
        return NULL; //skip B without refs
    }

    (*encTask)->prevTask = prevTask;
    (*encTask)->nextTask = nextTask;
    return *encTask;
}

void CEncodingPipeline::initFrameParams(iTask* eTask) {

    mfxU32 fieldId = 0, numOfFields = 1;
    preENCCtr[0].DisableMVOutput = disableMVoutPreENC;
    preENCCtr[1].DisableMVOutput = disableMVoutPreENC;
    preENCCtr[0].DisableStatisticsOutput = disableMBStatPreENC;
    preENCCtr[1].DisableStatisticsOutput = disableMBStatPreENC;

    if ( (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
         (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF) )
    {
        numOfFields = 2;
    }

    for (fieldId = 0; fieldId < numOfFields; fieldId++)
    {
        switch (eTask->frameType & MFX_FRAMETYPE_IPB) {
            case MFX_FRAMETYPE_I:
                eTask->in.NumFrameL0 = 0; //1;  //just workaround to support I frames
                eTask->in.NumFrameL1 = 0;
                eTask->in.L0Surface = eTask->in.L1Surface = NULL; //eTask->in.InSurface;
                //in data
                eTask->in.NumExtParam = numExtInParamsPreEnc;
                eTask->in.ExtParam = inBufsPreEnc;
                //out data
                //exclude MV output from output buffers
                if (preENCCtr[fieldId].DisableStatisticsOutput) {
                    eTask->out.NumExtParam = 0;
                    eTask->out.ExtParam    = NULL;
                } else {
                    eTask->out.NumExtParam = 1;
                    eTask->out.ExtParam    = outBufsPreEncI;
                }

                preENCCtr[fieldId].DisableMVOutput = 1;
                break;
            case MFX_FRAMETYPE_P:
                eTask->in.NumFrameL0 = 1;
                eTask->in.NumFrameL1 = 0;
                eTask->in.L0Surface = &eTask->prevTask->in.InSurface;
                eTask->in.L1Surface = NULL;
                //in data
                eTask->in.NumExtParam = numExtInParamsPreEnc;
                eTask->in.ExtParam    = inBufsPreEnc;
                //out data
                eTask->out.NumExtParam = numExtOutParamsPreEnc;
                eTask->out.ExtParam    = outBufsPreEnc;
                break;
            case MFX_FRAMETYPE_B:
                eTask->in.NumFrameL0 = 1;
                eTask->in.NumFrameL1 = 1;
                eTask->in.L0Surface = &eTask->prevTask->in.InSurface;
                eTask->in.L1Surface = &eTask->nextTask->in.InSurface;
                //in data
                eTask->in.NumExtParam = numExtInParamsPreEnc;
                eTask->in.ExtParam    = inBufsPreEnc;
                //out data
                eTask->out.NumExtParam = numExtOutParamsPreEnc;
                eTask->out.ExtParam    = outBufsPreEnc;
                break;
        }
    }
    mdprintf(stderr, "enc: %d t: %d\n", eTask->frameDisplayOrder, (eTask->frameType& MFX_FRAMETYPE_IPB));
}

void CEncodingPipeline::initEncFrameParams(iTask* eTask) {

    mfxU32 fieldId = 0, numOfFields = 1;

    if ( (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
         (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF) )
    {
        numOfFields = 2;
    }

    for (fieldId = 0; fieldId < numOfFields; fieldId++)
    {
        switch (eTask->frameType & MFX_FRAMETYPE_IPB) {
            case MFX_FRAMETYPE_I:
                /* ENC data */
                if (m_pmfxENC)
                {
                    eTask->in.NumFrameL0 = 0; //1;  //just workaround to support I frames
                    eTask->in.NumFrameL1 = 0;
                    eTask->in.L0Surface = eTask->in.L1Surface = NULL; //eTask->in.InSurface;
                    eTask->in.NumExtParam = numExtInParams;
                    eTask->in.ExtParam = inBufs;
                    eTask->out.NumExtParam = numExtOutParams;
                    eTask->out.ExtParam = outBufs;
                }
                /* PAK data */
                if (m_pmfxPAK)
                {
                    eTask->inPAK.NumFrameL0 = eTask->inPAK.NumFrameL1 = 0;
                    eTask->inPAK.L0Surface = eTask->inPAK.L1Surface = NULL;
                    /*insert SPS if we have IDR frame */
                    //if (eTask->frameType & MFX_FRAMETYPE_IDR)

                    eTask->inPAK.NumExtParam = numExtOutParams;
                    eTask->inPAK.ExtParam = outBufs;
                    eTask->outPAK.NumExtParam = numExtInParams;
                    eTask->outPAK.ExtParam = inBufs;
                }
                /* Process SPS, and PPS only*/
                if (m_encpakParams.bPassHeaders)
                {
                    m_feiPPS[fieldId].NumRefIdxL0Active = 0;
                    m_feiPPS[fieldId].NumRefIdxL1Active = 0;
                    /*I is always reference */
                    m_feiPPS[fieldId].ReferencePicFlag = 1;
                    if (eTask->frameType & MFX_FRAMETYPE_IDR)
                    {
                        m_feiPPS[fieldId].IDRPicFlag = 1;
                        m_feiPPS[fieldId].FrameNum = 0;
                        m_feiSPS.Pack = 1;
                        m_refFrameCounter = 1;
                    }
                    else
                    {
                        m_feiPPS[fieldId].IDRPicFlag = 0;
                        m_feiPPS[fieldId].FrameNum = m_refFrameCounter++;
                        m_feiSPS.Pack = 0;
                    }
                }
                break;
            case MFX_FRAMETYPE_P:
                if (m_pmfxENC)
                {
                    eTask->in.NumFrameL0 = 1;
                    eTask->in.NumFrameL1 = 0;
                    eTask->in.L0Surface = &eTask->prevTask->out.OutSurface;
                    eTask->in.L1Surface = NULL;
                    eTask->in.NumExtParam = numExtInParams;
                    eTask->in.ExtParam    = inBufs;
                    eTask->out.NumExtParam = numExtOutParams;
                    eTask->out.ExtParam    = outBufs;
                }

                if (m_pmfxPAK)
                {
                    eTask->inPAK.NumFrameL0 = 1;
                    eTask->inPAK.NumFrameL1 = 0;
                    eTask->inPAK.L0Surface = &eTask->prevTask->outPAK.OutSurface;
                    eTask->inPAK.L1Surface = NULL;
                    eTask->inPAK.NumExtParam = numExtOutParams;
                    eTask->inPAK.ExtParam    = outBufs;
                    eTask->outPAK.NumExtParam = numExtInParams;
                    eTask->outPAK.ExtParam    = inBufs;
                }

                /* PPS only */
                if (m_encpakParams.bPassHeaders)
                {
                    m_feiSPS.Pack = 0;
                    m_feiPPS[fieldId].IDRPicFlag = 0;
                    m_feiPPS[fieldId].NumRefIdxL0Active = eTask->in.NumFrameL0;
                    m_feiPPS[fieldId].NumRefIdxL1Active = 0;
                    m_feiPPS[fieldId].ReferencePicFlag = 1;
                    if (eTask->frameType & MFX_FRAMETYPE_REF)
                        m_feiPPS[fieldId].ReferencePicFlag = 1;
                    else
                        m_feiPPS[fieldId].ReferencePicFlag = 0;

                    if (eTask->prevTask->frameType & MFX_FRAMETYPE_REF)
                    {
                        m_feiPPS[fieldId].FrameNum = m_refFrameCounter++;
                    }
                }

                break;
            case MFX_FRAMETYPE_B:
                if (m_pmfxENC)
                {
                    eTask->in.NumFrameL0 = 1;
                    eTask->in.NumFrameL1 = 1;
                    eTask->in.L0Surface = &eTask->prevTask->out.OutSurface;
                    eTask->in.L1Surface = &eTask->nextTask->out.OutSurface;
                    eTask->in.NumExtParam = numExtInParams;
                    eTask->in.ExtParam    = inBufs;
                    eTask->out.NumExtParam = numExtOutParams;
                    eTask->out.ExtParam    = outBufs;
                }
                if (m_pmfxPAK)
                {
                    eTask->inPAK.NumFrameL0 = 1;
                    eTask->inPAK.NumFrameL1 = 1;
                    eTask->inPAK.L0Surface = &eTask->prevTask->outPAK.OutSurface;
                    eTask->inPAK.L1Surface = &eTask->nextTask->outPAK.OutSurface;
                    eTask->inPAK.NumExtParam = numExtOutParams;
                    eTask->inPAK.ExtParam    = outBufs;
                    eTask->outPAK.NumExtParam = numExtInParams;
                    eTask->outPAK.ExtParam    = inBufs;
                }

                /* PPS only */
                if (m_encpakParams.bPassHeaders)
                {
                    m_feiSPS.Pack = 0;
                    m_feiPPS[fieldId].IDRPicFlag = 0;
                    m_feiPPS[fieldId].NumRefIdxL0Active = eTask->in.NumFrameL0;
                    m_feiPPS[fieldId].NumRefIdxL1Active = eTask->in.NumFrameL1;
                    m_feiPPS[fieldId].ReferencePicFlag = 0;
                    m_feiPPS[fieldId].IDRPicFlag = 0;
                    m_feiPPS[fieldId].FrameNum = m_refFrameCounter;
                }

                break;
        }
    }
    mdprintf(stderr, "enc: %d t: %d\n", eTask->frameDisplayOrder, (eTask->frameType& MFX_FRAMETYPE_IPB));
}
