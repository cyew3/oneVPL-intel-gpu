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

#define MFX_FRAMETYPE_IPB (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B)

static FILE* mbstatout=NULL;
static FILE* mvout = NULL;
static FILE* mvENCPAKout = NULL;
static FILE* mbcodeout = NULL;

CEncTaskPool::CEncTaskPool()
{
    m_pTasks  = NULL;
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
            //Write output for enc+pak
            mfxBitstream& bs = m_pTasks[m_nTaskBufferStart].mfxBS;
            mfxExtFeiEncMV* mvBuf=NULL;
            mfxExtFeiEncMBStat* mbstatBuf = NULL;
            mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
            for(int i=0; i<bs.NumExtParam; i++){
                if(bs.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV && mvENCPAKout){
                    mvBuf = (mfxExtFeiEncMV*)bs.ExtParam[i];
                    fwrite(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
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
        else if (MFX_ERR_ABORTED == sts)
        {
            while (!m_pTasks[m_nTaskBufferStart].DependentVppTasks.empty())
            {
                // find out if the error occurred in a VPP task to perform recovery procedure if applicable
                sts = m_pmfxSession->SyncOperation(*m_pTasks[m_nTaskBufferStart].DependentVppTasks.begin(), 0);

                if (MFX_ERR_NONE == sts)
                {
                    m_pTasks[m_nTaskBufferStart].DependentVppTasks.pop_front();
                    sts = MFX_ERR_ABORTED; // save the status of the encode task
                    continue; // go to next vpp task
                }
                else
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
    DependentVppTasks.clear();

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

    DependentVppTasks.clear();

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocAndInitVppDoNotUse()
{
    m_VppDoNotUse.NumAlg = 4;

    m_VppDoNotUse.AlgList = new mfxU32 [m_VppDoNotUse.NumAlg];
    MSDK_CHECK_POINTER(m_VppDoNotUse.AlgList,  MFX_ERR_MEMORY_ALLOC);

    m_VppDoNotUse.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE; // turn off denoising (on by default)
    m_VppDoNotUse.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS; // turn off scene analysis (on by default)
    m_VppDoNotUse.AlgList[2] = MFX_EXTBUFF_VPP_DETAIL; // turn off detail enhancement (on by default)
    m_VppDoNotUse.AlgList[3] = MFX_EXTBUFF_VPP_PROCAMP; // turn off processing amplified (on by default)

    return MFX_ERR_NONE;

} // CEncodingPipeline::AllocAndInitVppDoNotUse()

void CEncodingPipeline::FreeVppDoNotUse()
{
    MSDK_SAFE_DELETE_ARRAY(m_VppDoNotUse.AlgList);
}

mfxStatus CEncodingPipeline::InitMfxEncParams(sInputParams *pInParams)
{
    m_encpakParams = *pInParams;

    m_mfxEncParams.mfx.CodecId                 = pInParams->CodecId;
    m_mfxEncParams.mfx.TargetUsage             = pInParams->nTargetUsage; // trade-off between quality and speed
    m_mfxEncParams.mfx.TargetKbps              = pInParams->nBitRate; // in Kbps
    m_mfxEncParams.mfx.RateControlMethod       = (pInParams->bLABRC || pInParams->nLADepth) ?
        (mfxU16)MFX_RATECONTROL_LA : (mfxU16)MFX_RATECONTROL_CBR;
    ConvertFrameRate(pInParams->dFrameRate, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtN, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);
    m_mfxEncParams.mfx.EncodedOrder            = 0; // binary flag, 0 signals encoder to take frames in display order


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
    if (pInParams->nLADepth)
    {
        m_CodingOption2.LookAheadDepth = pInParams->nLADepth;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_CodingOption2);
    }

    if(pInParams->bENCPAK)
    {
        //force rc to cqp only
        m_mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
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

    // JPEG encoder settings overlap with other encoders settings in mfxInfoMFX structure
    if (MFX_CODEC_JPEG == pInParams->CodecId)
    {
        m_mfxEncParams.mfx.Interleaved = 1;
        m_mfxEncParams.mfx.Quality = pInParams->nQuality;
        m_mfxEncParams.mfx.RestartInterval = 0;
        MSDK_ZERO_MEMORY(m_mfxEncParams.mfx.reserved5);
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitMfxVppParams(sInputParams *pInParams)
{
    MSDK_CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR);

    // specify memory type
    if (D3D9_MEMORY == pInParams->memType || D3D11_MEMORY == pInParams->memType)
    {
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    else
    {
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }

    // input frame info
    m_mfxVppParams.vpp.In.FourCC    = MFX_FOURCC_NV12;
    m_mfxVppParams.vpp.In.PicStruct = pInParams->nPicStruct;;
    ConvertFrameRate(pInParams->dFrameRate, &m_mfxVppParams.vpp.In.FrameRateExtN, &m_mfxVppParams.vpp.In.FrameRateExtD);

    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxVppParams.vpp.In.Width     = MSDK_ALIGN16(pInParams->nWidth);
    m_mfxVppParams.vpp.In.Height    = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxVppParams.vpp.In.PicStruct)?
        MSDK_ALIGN16(pInParams->nHeight) : MSDK_ALIGN32(pInParams->nHeight);

    // set crops in input mfxFrameInfo for correct work of file reader
    // VPP itself ignores crops at initialization
    m_mfxVppParams.vpp.In.CropW = pInParams->nWidth;
    m_mfxVppParams.vpp.In.CropH = pInParams->nHeight;

    // fill output frame info
    MSDK_MEMCPY_VAR(m_mfxVppParams.vpp.Out,&m_mfxVppParams.vpp.In, sizeof(mfxFrameInfo));

    // only resizing is supported
    m_mfxVppParams.vpp.Out.Width = MSDK_ALIGN16(pInParams->nDstWidth);
    m_mfxVppParams.vpp.Out.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxVppParams.vpp.Out.PicStruct)?
        MSDK_ALIGN16(pInParams->nDstHeight) : MSDK_ALIGN32(pInParams->nDstHeight);

    // configure and attach external parameters
    AllocAndInitVppDoNotUse();
    m_VppExtParams.push_back((mfxExtBuffer *)&m_VppDoNotUse);

    m_mfxVppParams.ExtParam = &m_VppExtParams[0]; // vector is stored linearly in memory
    m_mfxVppParams.NumExtParam = (mfxU16)m_VppExtParams.size();

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
    MSDK_CHECK_POINTER(m_pmfxENCPAK, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest EncRequest;
    mfxFrameAllocRequest ReconRequest;
    mfxFrameAllocRequest VppRequest[2];

    mfxU16 nEncSurfNum = 0; // number of surfaces for encoder
    mfxU16 nVppSurfNum = 0; // number of surfaces for vpp

    MSDK_ZERO_MEMORY(EncRequest);
    MSDK_ZERO_MEMORY(VppRequest[0]);
    MSDK_ZERO_MEMORY(VppRequest[1]);

    // Calculate the number of surfaces for components.
    // QueryIOSurf functions tell how many surfaces are required to produce at least 1 output.
    // To achieve better performance we provide extra surfaces.
    // 1 extra surface at input allows to get 1 extra output.

    sts = m_pmfxENCPAK->QueryIOSurf(&m_mfxEncParams, &EncRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (m_pmfxVPP)
    {
        // VppRequest[0] for input frames request, VppRequest[1] for output frames request
        sts = m_pmfxVPP->QueryIOSurf(&m_mfxVppParams, VppRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // The number of surfaces shared by vpp output and encode input.
    // When surfaces are shared 1 surface at first component output contains output frame that goes to next component input
    nEncSurfNum = EncRequest.NumFrameSuggested + MSDK_MAX(VppRequest[1].NumFrameSuggested, 1) - 1 + (m_nAsyncDepth - 1);
    if ( (m_encpakParams.bPREENC) || (m_encpakParams.bENCoPAKo))
    {
        nEncSurfNum = m_refDist * 2;
    }

    // The number of surfaces for vpp input - so that vpp can work at async depth = m_nAsyncDepth
    nVppSurfNum = VppRequest[0].NumFrameSuggested + (m_nAsyncDepth - 1);

    // prepare allocation requests
    EncRequest.NumFrameMin = nEncSurfNum;
    EncRequest.NumFrameSuggested = nEncSurfNum;
    MSDK_MEMCPY_VAR(EncRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
    if (m_pmfxVPP)
    {
        EncRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT; // surfaces are shared between vpp output and encode input
    }

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
    if (m_pmfxPAK)
    {
        MSDK_MEMCPY_VAR(ReconRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
        ReconRequest.NumFrameMin = m_mfxEncParams.mfx.GopRefDist *2 + m_mfxEncParams.AsyncDepth;
        ReconRequest.NumFrameSuggested = m_mfxEncParams.mfx.GopRefDist *2 + m_mfxEncParams.AsyncDepth;
        /* type of reconstructed surfaces for PAK should be same as for Media SDK's decoders!!!
         * Because libVA required reconstructed surfaces for vaCreateContext */
        ReconRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &ReconRequest, &m_ReconResponse);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // alloc frames for vpp if vpp is enabled
    if (m_pmfxVPP)
    {
        VppRequest[0].NumFrameMin = nVppSurfNum;
        VppRequest[0].NumFrameSuggested = nVppSurfNum;
        MSDK_MEMCPY_VAR(VppRequest[0].Info, &(m_mfxVppParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &(VppRequest[0]), &m_VppResponse);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // prepare mfxFrameSurface1 array for encoder
    m_pEncSurfaces = new mfxFrameSurface1 [m_EncResponse.NumFrameActual];
    MSDK_CHECK_POINTER(m_pEncSurfaces, MFX_ERR_MEMORY_ALLOC);
    m_pReconSurfaces = new mfxFrameSurface1 [m_ReconResponse.NumFrameActual];
    MSDK_CHECK_POINTER(m_pReconSurfaces, MFX_ERR_MEMORY_ALLOC);

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

    // prepare mfxFrameSurface1 array for vpp if vpp is enabled
    if (m_pmfxVPP)
    {
        m_pVppSurfaces = new mfxFrameSurface1 [m_VppResponse.NumFrameActual];
        MSDK_CHECK_POINTER(m_pVppSurfaces, MFX_ERR_MEMORY_ALLOC);

        for (int i = 0; i < m_VppResponse.NumFrameActual; i++)
        {
            MSDK_ZERO_MEMORY(m_pVppSurfaces[i]);
            MSDK_MEMCPY_VAR(m_pVppSurfaces[i].Info, &(m_mfxVppParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

            if (m_bExternalAlloc)
            {
                m_pVppSurfaces[i].Data.MemId = m_VppResponse.mids[i];
            }
            else
            {
                sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_VppResponse.mids[i], &(m_pVppSurfaces[i].Data));
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
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

        p_vaapiAllocParams->m_dpy = (VADisplay)hdl;
        m_pmfxAllocatorParams = p_vaapiAllocParams;

        /* In case of video memory we must provide MediaSDK with external allocator
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to mediasdk */
        sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

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
    MSDK_SAFE_DELETE_ARRAY(m_pVppSurfaces);

    // delete frames
    if (m_pMFXAllocator)
    {
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_EncResponse);
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_VppResponse);
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
    m_pmfxENCPAK = NULL;
    m_pmfxVPP = NULL;
    m_pMFXAllocator = NULL;
    m_pmfxAllocatorParams = NULL;
    m_memType = D3D9_MEMORY; //only hw memory is supported
    m_bExternalAlloc = false;
    m_pEncSurfaces = NULL;
    m_pVppSurfaces = NULL;
    m_nAsyncDepth = 0;

    m_nNumView = 0;

    m_FileWriters.first = m_FileWriters.second = NULL;

    MSDK_ZERO_MEMORY(m_VppDoNotUse);
    m_VppDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
    m_VppDoNotUse.Header.BufferSz = sizeof(m_VppDoNotUse);

    MSDK_ZERO_MEMORY(m_CodingOption);
    m_CodingOption.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    m_CodingOption.Header.BufferSz = sizeof(m_CodingOption);

    MSDK_ZERO_MEMORY(m_CodingOption2);
    m_CodingOption2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    m_CodingOption2.Header.BufferSz = sizeof(m_CodingOption2);

#if D3D_SURFACES_SUPPORT
    m_hwdev = NULL;
#endif

    MSDK_ZERO_MEMORY(m_mfxEncParams);
    MSDK_ZERO_MEMORY(m_mfxVppParams);

    MSDK_ZERO_MEMORY(m_EncResponse);
    MSDK_ZERO_MEMORY(m_VppResponse);
}

CEncodingPipeline::~CEncodingPipeline()
{
    Close();
}

void CEncodingPipeline::SetMultiView()
{
    m_FileReader.SetMultiView();
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
    if((pParams->bENCPAK) || (pParams->bENCoPAKo)){ //need only for ENC+PAK
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
                            pParams->numViews,
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
    }
    else
        sts = m_mfxSession.Init(MFX_IMPL_SOFTWARE, NULL);


    // we check if codec is distributed as a mediasdk plugin and load it if yes
    // else if codec is not in the list of mediasdk plugins, we assume, that it is supported inside mediasdk library
//    if (pParams->bUseHWLib){
//             m_pUID = msdkGetPluginUID(MSDK_VENC | MSDK_IMPL_HW, pParams->CodecId);
//          } else {
//             m_pUID = msdkGetPluginUID(MSDK_VENC | MSDK_IMPL_SW, pParams->CodecId);
//          }
//
//    if (m_pUID)
//      {
//          sts = MFXVideoUSER_Load(m_mfxSession, &(m_pUID->mfx), 1);
//          if (MFX_ERR_NONE != sts)
//           {
//                printf("error: failed to load Media SDK plugin:\n");
//                printf("error:   GUID = { 0x%08x, 0x%04x, 0x%04x, { 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x } }\n",
//                       m_pUID->guid.data1, m_pUID->guid.data2, m_pUID->guid.data3,
//                       m_pUID->guid.data4[0], m_pUID->guid.data4[1], m_pUID->guid.data4[2], m_pUID->guid.data4[3],
//                       m_pUID->guid.data4[4], m_pUID->guid.data4[5], m_pUID->guid.data4[6], m_pUID->guid.data4[7]);
//                printf("error:   UID (mfx raw) = %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x\n",
//                       m_pUID->raw[0],  m_pUID->raw[1],  m_pUID->raw[2],  m_pUID->raw[3],
//                       m_pUID->raw[4],  m_pUID->raw[5],  m_pUID->raw[6],  m_pUID->raw[7],
//                       m_pUID->raw[8],  m_pUID->raw[9],  m_pUID->raw[10], m_pUID->raw[11],
//                       m_pUID->raw[12], m_pUID->raw[13], m_pUID->raw[14], m_pUID->raw[15]);
//                const msdk_char* str = msdkGetPluginName(MSDK_VENC | MSDK_IMPL_SW, pParams->CodecId);
//                msdk_printf(MSDK_STRING("error:   name = %s\n"), str);
//                msdk_printf(MSDK_STRING("error:   You may need to install this plugin separately!\n"));
//            }
//            else
//            {
//               msdk_printf(MSDK_STRING("Plugin '%s' loaded successfully\n"), msdkGetPluginName(MSDK_VENC | MSDK_IMPL_SW, pParams->CodecId));
//            }
//        }
//
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // set memory type
    m_memType = pParams->memType;

    // create and init frame allocator
    sts = CreateAllocator();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitMfxEncParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    if (pParams->nWidth  != pParams->nDstWidth ||
        pParams->nHeight != pParams->nDstHeight ||
        m_mfxVppParams.vpp.In.FourCC != m_mfxVppParams.vpp.Out.FourCC)
    {
        sts = InitMfxVppParams(pParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    //if(pParams->bENCPAK){
        // create encoder
        m_pmfxENCPAK = new MFXVideoENCODE(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENCPAK, MFX_ERR_MEMORY_ALLOC);
    //}

    if(pParams->bPREENC){
        // create encoder
        m_pmfxPREENC = new MFXVideoENC(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPREENC, MFX_ERR_MEMORY_ALLOC);
    }

    if(pParams->bENCoPAKo){
        // create encoder
        m_pmfxENC = new MFXVideoENC(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);

        m_pmfxPAK = new MFXVideoPAK(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPAK, MFX_ERR_MEMORY_ALLOC);
    }

    // create preprocessor if resizing was requested from command line
    // or if different FourCC is set in InitMfxVppParams
    if (pParams->nWidth  != pParams->nDstWidth ||
        pParams->nHeight != pParams->nDstHeight ||
        m_mfxVppParams.vpp.In.FourCC != m_mfxVppParams.vpp.Out.FourCC)
    {
        m_pmfxVPP = new MFXVideoVPP(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxVPP, MFX_ERR_MEMORY_ALLOC);
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

    MSDK_SAFE_DELETE(m_pmfxENCPAK);
    MSDK_SAFE_DELETE(m_pmfxENC);
    MSDK_SAFE_DELETE(m_pmfxPAK);
    MSDK_SAFE_DELETE(m_pmfxPREENC);
    MSDK_SAFE_DELETE(m_pmfxVPP);

    //if (m_pUID) MFXVideoUSER_UnLoad(m_mfxSession, &(m_pUID->mfx));

    m_pHEVC_plugin.reset();

    FreeVppDoNotUse();

    DeleteFrames();

    m_TaskPool.Close();
    m_mfxSession.Close();

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

    if(m_pmfxENCPAK)
    {
        sts = m_pmfxENCPAK->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if (m_pmfxVPP)
    {
        sts = m_pmfxVPP->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // free allocated frames
    DeleteFrames();

    m_TaskPool.Close();

    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //use encoded order if both is used
    //TODO: check for ENC_PAK
    if(m_encpakParams.bENCPAK && m_encpakParams.bPREENC){
        m_mfxEncParams.mfx.EncodedOrder = 1;
    }

    if ((m_encpakParams.bENCPAK) && (m_pmfxENCPAK) )
    {
        sts = m_pmfxENCPAK->Init(&m_mfxEncParams);
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
        preEncParams.ExtParam = buf;

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
        encParams.ExtParam = buf;

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
        pakParams.ExtParam = buf;

        sts = m_pmfxPAK->Init(&pakParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if (m_pmfxVPP)
    {
        sts = m_pmfxVPP->Init(&m_mfxVppParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    mfxU32 nEncodedDataBufferSize = m_mfxEncParams.mfx.FrameInfo.Width * m_mfxEncParams.mfx.FrameInfo.Height * 4;
    sts = m_TaskPool.Init(&m_mfxSession, m_FileWriters.first, m_nAsyncDepth * 2, nEncodedDataBufferSize, m_FileWriters.second);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocateSufficientBuffer(mfxBitstream* pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENCPAK, MFX_ERR_NOT_INITIALIZED);

    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    mfxStatus sts = m_pmfxENCPAK->GetVideoParam(&par);
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
    MSDK_CHECK_POINTER(m_pmfxENCPAK, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    mfxFrameSurface1* pSurf = NULL; // dispatching pointer
    mfxFrameSurface1* pReconSurf = NULL;

    sTask *pCurrentTask = NULL; // a pointer to the current task
    mfxU16 nEncSurfIdx = 0;     // index of free surface for encoder input (vpp output)
    mfxU16 nReconSurfIdx = 0;     // index of free surface for reconstruct pool
    mfxU16 nVppSurfIdx = 0;     // index of free surface for vpp input

    mfxU32 fieldId = 0, numOfFields = 1; // default is progressive case
    mfxI32 heightMB = ((m_encpakParams.nHeight + 15) & ~15);
    mfxI32 widthMB = ((m_encpakParams.nWidth + 15) & ~15);
    mfxI32 numMB = heightMB * widthMB / 256;

    mfxSyncPoint VppSyncPoint = NULL; // a sync point associated with an asynchronous vpp call
    bool bVppMultipleOutput = false;  // this flag is true if VPP produces more frames at output
                                      // than consumes at input. E.g. framerate conversion 30 fps -> 60 fps

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

    numExtInParams = 0;
    numExtOutParams = 0;

    numExtOutParamsPreEnc = 0;
    numExtInParamsPreEnc = 0;

    /* if TFF or BFF*/
    if ( (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
         (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF) )
    {
        numOfFields = 2;
        numMB = numMB/2;
    }

    if (m_encpakParams.bPREENC) {
        inputTasks.clear();  //for reordering

        //setup control structures
        bool disableMVoutput = m_encpakParams.mvoutFile == NULL && !m_encpakParams.bENCPAK;
        bool disableMBoutput = (m_encpakParams.mbstatoutFile == NULL) || m_encpakParams.bENCPAK; //couple with ENC+PAK
        bool enableMVpredictor = m_encpakParams.mvinFile != NULL;
        bool enableMBQP = m_encpakParams.mbQpFile != NULL;

        disableMVoutPreENC = disableMVoutput;
        disableMBStatPreENC = disableMBoutput;

        for (fieldId = 0; fieldId < numOfFields; fieldId++)
        {
            memset(&preENCCtr[fieldId], 0, sizeof (mfxExtFeiPreEncCtrl));
            preENCCtr[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_CTRL;
            preENCCtr[fieldId].Header.BufferSz = sizeof (mfxExtFeiPreEncCtrl);
            preENCCtr[fieldId].DisableMVOutput = disableMVoutput;
            preENCCtr[fieldId].DisableStatisticsOutput = disableMBoutput;
            preENCCtr[fieldId].FTEnable = 1;//0;
            preENCCtr[fieldId].AdaptiveSearch = 1;
            preENCCtr[fieldId].LenSP = 57;
            preENCCtr[fieldId].MBQp = enableMBQP;
            preENCCtr[fieldId].MVPredictor = enableMVpredictor;
            preENCCtr[fieldId].RefHeight = 40;
            preENCCtr[fieldId].RefWidth = 48;
            preENCCtr[fieldId].SubPelMode = 3;
            preENCCtr[fieldId].SearchWindow = 0;
            preENCCtr[fieldId].MaxLenSP = 57;
            preENCCtr[fieldId].Qp = m_encpakParams.QP;
            preENCCtr[fieldId].InterSAD = 2;
            preENCCtr[fieldId].IntraSAD = 2;
            preENCCtr[fieldId].SubMBPartMask = 0;//0x77;

            //inBufsPreEnc[numExtInParamsPreEnc++] = (mfxExtBuffer*) & preENCCtr;

            if (preENCCtr[fieldId].MVPredictor) {
                preENCCtr[fieldId].MVPredictor = 0;
                mvPreds[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV_PRED;
                mvPreds[fieldId].Header.BufferSz = sizeof (mfxExtFeiPreEncMVPredictors);
                mvPreds[fieldId].NumMBAlloc = numMB;
                mvPreds[fieldId].MB = new mfxExtFeiPreEncMVPredictors::mfxMB [numMB];
                //inBufsPreEnc[numExtInParamsPreEnc++] = (mfxExtBuffer*) & mvPreds;
                //read mvs from file
                printf("Using MV input file: %s\n", m_encpakParams.mvinFile);

                FILE* fmv=NULL;
                MSDK_FOPEN(fmv,m_encpakParams.mvinFile, MSDK_CHAR("rb"));
                if (fmv == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mvinFile);
                    exit(-1);
                }

                fclose(fmv);
            }

            if (preENCCtr[fieldId].MBQp) {
                qps[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
                qps[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncQP);
                qps[fieldId].NumQPAlloc = numMB;
                qps[fieldId].QP = new mfxU8[numMB];
                //inBufsPreEnc[numExtInParamsPreEnc++] = (mfxExtBuffer*) & qps;
                //read mvs from file
                printf("Using QP input file: %s\n", m_encpakParams.mbQpFile);
                FILE* fqp=NULL;
                MSDK_FOPEN(fqp,m_encpakParams.mbQpFile, MSDK_CHAR("rb"));
                if (fqp == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbQpFile);
                    exit(-1);
                }

                //size_t fsize = fseek(fqp, 0, SEEK_END);
                fseek(fqp, 0, SEEK_SET);
                fread(qps[fieldId].QP, sizeof(qps[fieldId].QP[0])*numMB, 1, fqp);
                fclose(fqp);
            }

            if (!preENCCtr[fieldId].DisableMVOutput) {
                mvs[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV;
                mvs[fieldId].Header.BufferSz = sizeof (mfxExtFeiPreEncMV);
                mvs[fieldId].NumMBAlloc = numMB;
                mvs[fieldId].MB = new mfxExtFeiPreEncMV::mfxMB [numMB];
                //outBufsPreEnc[numExtOutParamsPreEnc++] = (mfxExtBuffer*) & mvs;

                if (m_encpakParams.mvoutFile != NULL &&
                        !m_encpakParams.bENCPAK) {
                    printf("Using MV output file: %s\n", m_encpakParams.mvoutFile);
                    MSDK_FOPEN(mvout,m_encpakParams.mvoutFile, MSDK_CHAR("wb"));
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
                MSDK_FOPEN(mbstatout,m_encpakParams.mbstatoutFile, MSDK_CHAR("wb"));
                if (mbstatout == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbstatoutFile);
                    exit(-1);
                }
            } // if (!preENCCtr.DisableStatisticsOutput)
        } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
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
    } // if (m_encpakParams.bPREENC) {

    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bENCoPAKo)){
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

        for (fieldId = 0; fieldId < numOfFields; fieldId++)
        {
            memset(&feiEncCtrl[fieldId], 0, sizeof (mfxExtFeiEncFrameCtrl));
            feiEncCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
            feiEncCtrl[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncFrameCtrl);
            feiEncCtrl[fieldId].MaxLenSP = 57;
            feiEncCtrl[fieldId].LenSP = 57;
            feiEncCtrl[fieldId].SubMBPartMask = 0x77;
            feiEncCtrl[fieldId].MultiPredL0 = 0;
            feiEncCtrl[fieldId].MultiPredL1 = 0;
            feiEncCtrl[fieldId].SubPelMode = 3;
            feiEncCtrl[fieldId].InterSAD = 2;
            feiEncCtrl[fieldId].IntraSAD = 2;
            feiEncCtrl[fieldId].DistortionType = 2;
            feiEncCtrl[fieldId].RepartitionCheckEnable = 0;
            feiEncCtrl[fieldId].AdaptiveSearch = 1;
            feiEncCtrl[fieldId].MVPredictor = MVPredictors;
            feiEncCtrl[fieldId].NumMVPredictors = 1; //always 4 predictors
            feiEncCtrl[fieldId].PerMBQp = MBQP;
            feiEncCtrl[fieldId].PerMBInput = MBCtrl;
            feiEncCtrl[fieldId].MBSizeCtrl = m_encpakParams.bMBSize;
            feiEncCtrl[fieldId].RefHeight = 40;
            feiEncCtrl[fieldId].RefWidth = 48;
            feiEncCtrl[fieldId].SearchWindow = 1;

            //inBufs[numExtInParams++] = (mfxExtBuffer*) & feiEncCtrl;

            //feiEncMVPredictors;
            if (MVPredictors) {
                feiEncMVPredictors[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV_PRED;
                feiEncMVPredictors[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncMVPredictors);
                feiEncMVPredictors[fieldId].NumMBAlloc = numMB;
                feiEncMVPredictors[fieldId].MB = new mfxExtFeiEncMVPredictors::mfxMB [numMB];
                //inBufs[numExtInParams++] = (mfxExtBuffer*) &feiEncMVPredictors;
                //read mvs from file
                if (!m_encpakParams.bPREENC) { //not load if we couple with PREENC
                    printf("Using MV input file: %s\n", m_encpakParams.mvinFile);
                    FILE* fmv=NULL;
                    MSDK_FOPEN(fmv,m_encpakParams.mvinFile, MSDK_CHAR("rb"));
                    if (fmv == NULL) {
                        fprintf(stderr, "Can't open file %s\n", m_encpakParams.mvinFile);
                        exit(-1);
                    }

                    fseek(fmv, 0, SEEK_SET);
                    fread(feiEncMVPredictors[fieldId].MB, sizeof (mfxExtFeiEncMVPredictors::mfxMB) * numMB, 1, fmv);
                    fclose(fmv);
                }
            }

            //feiEncMBCtrl;
            if (MBCtrl) {
                feiEncMBCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB;
                feiEncMBCtrl[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncMBCtrl);
                feiEncMBCtrl[fieldId].NumMBAlloc = numMB;
                feiEncMBCtrl[fieldId].MB = new mfxExtFeiEncMBCtrl::mfxMB [numMB];
                //inBufs[numExtInParams++] = (mfxExtBuffer*) &feiEncMBCtrl;
                //read mvs from file
                printf("Using MB control input file: %s\n", m_encpakParams.mbctrinFile);

                FILE* mbs=NULL;
                MSDK_FOPEN(mbs,m_encpakParams.mbctrinFile, MSDK_CHAR("rb"));

                if ( mbs == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbctrinFile);
                    exit(-1);
                }

                fseek(mbs, 0, SEEK_SET);
                fread(feiEncMBCtrl[fieldId].MB, sizeof(mfxExtFeiEncMBCtrl::mfxMB)*numMB, 1, mbs);
                fclose(mbs);
            }

            if (MBQP) {
                feiEncMbQp[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
                feiEncMbQp[fieldId].Header.BufferSz = sizeof (mfxExtFeiEncQP);
                feiEncMbQp[fieldId].NumQPAlloc = numMB;
                feiEncMbQp[fieldId].QP = new mfxU8 [numMB];
                //inBufs[numExtInParams++] = (mfxExtBuffer*) &feiEncMbQp;
                //read mvs from file
                printf("Use MB QP input file: %s\n", m_encpakParams.mbQpFile);

                FILE* mbs=NULL;
                MSDK_FOPEN(mbs,m_encpakParams.mbQpFile, MSDK_CHAR("rb"));

                if ( mbs == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbQpFile);
                    exit(-1);
                }

                fseek(mbs, 0, SEEK_SET);
                fread(feiEncMbQp[fieldId].QP, sizeof(feiEncMbQp[fieldId].QP[0])*numMB, 1, mbs);
                fclose(mbs);
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
                MSDK_FOPEN(mbstatout,m_encpakParams.mbstatoutFile, MSDK_CHAR("wb"));
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
                MSDK_FOPEN(mvENCPAKout,m_encpakParams.mvoutFile, MSDK_CHAR("wb"));
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
                MSDK_FOPEN(mbcodeout,m_encpakParams.mbcodeoutFile, MSDK_CHAR("wb"));
                if ( mbcodeout == NULL) {
                    fprintf(stderr, "Can't open file %s\n", m_encpakParams.mbcodeoutFile);
                    exit(-1);
                }
            } // if(MBCodeOut){
        } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

        // lets count all control and Ext buffers...
        inBufs[numExtInParams++] = (mfxExtBuffer*) & feiEncCtrl;
        if (MVPredictors)
            inBufs[numExtInParams++] = (mfxExtBuffer*) &feiEncMVPredictors;
        if (MBCtrl)
            inBufs[numExtInParams++] = (mfxExtBuffer*) &feiEncMBCtrl;
        if (MBQP)
            inBufs[numExtInParams++] = (mfxExtBuffer*) &feiEncMbQp;

        ctr = &feiCtrl;
        feiCtrl.NumExtParam = numExtInParams;
        feiCtrl.ExtParam = &inBufs[0];

        if(MBStatOut)
            outBufs[numExtOutParams++] = (mfxExtBuffer*) &feiEncMbStat;
        if(MVOut)
            outBufs[numExtOutParams++] = (mfxExtBuffer*) &feiEncMV;
        if(MBCodeOut)
            outBufs[numExtOutParams++] = (mfxExtBuffer*) &feiEncMBCode;
    } // if (m_encpakParams.bENCPAK) {

    // Since in sample we support just 2 views
    // we will change this value between 0 and 1 in case of MVC
    mfxU16 currViewNum = 0;

    sts = MFX_ERR_NONE;

    // main loop, preprocessing and encoding
    mfxU32 frameCount = 0;
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
    {
        if(m_encpakParams.nNumFrames && frameCount >= m_encpakParams.nNumFrames)
            break;

        // get a pointer to a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);

        iTask* eTask=NULL;
        if ((m_encpakParams.bPREENC) || (m_encpakParams.bENCoPAKo)){
            eTask = new iTask;
            eTask->frameType = GetFrameType(frameCount);
            eTask->frameDisplayOrder = frameCount;
            eTask->encoded = 0;
            mdprintf(stderr,"frame: %d  t:%d pt=%p ", frameCount, eTask->frameType, eTask);
        }

        // find free surface for encoder input
        nEncSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
        MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
        if(m_pmfxPAK)
        {
            nReconSurfIdx = GetFreeSurface(m_pReconSurfaces, m_ReconResponse.NumFrameActual);
            MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
        }

        frameCount++;

        // point pSurf to encoder surface
        pSurf = &m_pEncSurfaces[nEncSurfIdx];
        pReconSurf = &m_pReconSurfaces[nReconSurfIdx];
        if (!bVppMultipleOutput)
        {
            // if vpp is enabled find free surface for vpp input and point pSurf to vpp surface
            if (m_pmfxVPP)
            {
                nVppSurfIdx = GetFreeSurface(m_pVppSurfaces, m_VppResponse.NumFrameActual);
                MSDK_CHECK_ERROR(nVppSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);

                pSurf = &m_pVppSurfaces[nVppSurfIdx];
            }

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
        }

        // perform preprocessing if required
        if (m_pmfxVPP)
        {
            bVppMultipleOutput = false; // reset the flag before a call to VPP
            for (;;)
            {
                sts = m_pmfxVPP->RunFrameVPPAsync(&m_pVppSurfaces[nVppSurfIdx], &m_pEncSurfaces[nEncSurfIdx],
                    NULL, &VppSyncPoint);

                if (MFX_ERR_NONE < sts && !VppSyncPoint) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                }
                else if (MFX_ERR_NONE < sts && VppSyncPoint)
                {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    break;
                }
                else
                    break; // not a warning
            }

            // process errors
            if (MFX_ERR_MORE_DATA == sts)
            {
                continue;
            }
            else if (MFX_ERR_MORE_SURFACE == sts)
            {
                bVppMultipleOutput = true;
            }
            else
            {
                MSDK_BREAK_ON_ERROR(sts);
            }
        }

        // save the id of preceding vpp task which will produce input data for the encode task
        if (VppSyncPoint)
        {
            pCurrentTask->DependentVppTasks.push_back(VppSyncPoint);
            VppSyncPoint = NULL;
        }

        if (m_encpakParams.bPREENC) {
            pSurf->Data.Locked = 1;
            eTask->in.InSurface = pSurf;

            inputTasks.push_back(eTask); //inputTasks in display order

            eTask = findFrameToEncode();
            if(eTask == NULL) continue; //not found frame to encode

            eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;
            initFrameParams(eTask);

            for (;;) {
                fprintf(stderr, "frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);

                sts = m_pmfxPREENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                sts = MFX_ERR_NONE;

                sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
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
            if ( (mfxU32)(this->m_refDist * 2) == inputTasks.size()) {
                inputTasks.front()->in.InSurface->Data.Locked = 0;
                delete inputTasks.front();
                //remove prevTask
                inputTasks.pop_front();
            }

            //drop output data to output file
            //pCurrentTask->WriteBitstream();

            for (fieldId = 0; fieldId < numOfFields; fieldId++)
            {
                if (mbstatout)
                    fwrite(mbdata[fieldId].MB, sizeof (mbdata[fieldId].MB[0]) * mbdata[fieldId].NumMBAlloc, 1, mbstatout);
                //if (mvout && !(eTask->frameType&MFX_FRAMETYPE_I))
                if (mvout)
                    fwrite(mvs[fieldId].MB, sizeof (mvs[fieldId].MB[0]) * mvs[fieldId].NumMBAlloc, 1, mvout);
                //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }

            if(m_encpakParams.bENCPAK) {
                ctr->FrameType = eTask->frameType;
                //m_pEncSurfaces[nEncSurfIdx].Data.FrameOrder = eTask->frameDisplayOrder;
                eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;

                //init MV predictors
                for (fieldId = 0; fieldId < numOfFields; fieldId++)
                {
                    memset(feiEncMVPredictors[fieldId].MB, 0,
                            sizeof(feiEncMVPredictors[fieldId].MB[0])*feiEncMVPredictors[fieldId].NumMBAlloc);
                    for(mfxU32 i=0; i<mvs[fieldId].NumMBAlloc; i++){
                        //only one ref is used for now
                        /*
                        feiEncMVPredictors.MB[i].RefIdx[0].RefL0 = 0;
                        feiEncMVPredictors.MB[i].RefIdx[0].RefL1 = 0;
                        feiEncMVPredictors.MB[i].RefIdx[1].RefL0 = 0;
                        feiEncMVPredictors.MB[i].RefIdx[1].RefL1 = 0;
                        feiEncMVPredictors.MB[i].RefIdx[2].RefL0 = 0;
                        feiEncMVPredictors.MB[i].RefIdx[2].RefL1 = 0;
                        feiEncMVPredictors.MB[i].RefIdx[3].RefL0 = 0;
                        feiEncMVPredictors.MB[i].RefIdx[3].RefL1 = 0;
                         */

                        //use only first subblock component of MV
                        feiEncMVPredictors[fieldId].MB[i].MV[0][0].x = mvs[fieldId].MB[i].MV[0][0].x; //x L0
                        feiEncMVPredictors[fieldId].MB[i].MV[0][0].y = mvs[fieldId].MB[i].MV[0][0].y; //y L0
                        feiEncMVPredictors[fieldId].MB[i].MV[0][1].x = mvs[fieldId].MB[i].MV[0][1].x; //x L1
                        feiEncMVPredictors[fieldId].MB[i].MV[0][1].y = mvs[fieldId].MB[i].MV[0][1].y; //y L1
                    } // for(mfxI32 i=0; i<mvs.NumMBAlloc; i++){
                }// for (fieldId = 0; fieldId < numOfFields; fieldId++)
            }
        }

        /* ENC call */
        if (m_encpakParams.bENCoPAKo) {
            pSurf->Data.Locked++;
            eTask->in.InSurface = pSurf;
            eTask->inPAK.InSurface = pSurf;
            /* this is Recon surface which will be generated by FEI PAK*/
            pReconSurf->Data.Locked++;
            eTask->outPAK.OutSurface = pReconSurf;
            eTask->outPAK.Bs = &pCurrentTask->mfxBS;

            inputTasks.push_back(eTask); //inputTasks in display order

            eTask = findFrameToEncode();
            if(eTask == NULL) continue; //not found frame to encode

            eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;
            initEncFrameParams(eTask);

            for (;;) {
                fprintf(stderr, "frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);

                sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                sts = MFX_ERR_NONE;

                sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                fprintf(stderr, "synced : %d\n", sts);

                sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
                sts = MFX_ERR_NONE;

                sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                fprintf(stderr, "synced : %d\n", sts);

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
            if ( (mfxU32)(this->m_refDist * 2) == inputTasks.size()) {
                inputTasks.front()->in.InSurface->Data.Locked--;
                inputTasks.front()->outPAK.OutSurface->Data.Locked--;
                delete inputTasks.front();
                //remove prevTask
                inputTasks.pop_front();
            }

            //drop output data to output file
            pCurrentTask->WriteBitstream();

            for (fieldId = 0; fieldId < numOfFields; fieldId++)
            {
                mfxExtFeiEncMV* mvBuf=NULL;
                mfxExtFeiEncMBStat* mbstatBuf = NULL;
                mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
                for(int i=0; i<eTask->out.NumExtParam; i++){
                    if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV && mvENCPAKout){
                        mvBuf = (mfxExtFeiEncMV*)eTask->out.ExtParam[i];
                        fwrite(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                    }
                    if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT && mbstatout){
                        mbstatBuf = (mfxExtFeiEncMBStat*)eTask->out.ExtParam[i];
                        fwrite(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, mbstatout);
                    }
                    if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL && mbcodeout){
                        mbcodeBuf = (mfxExtFeiPakMBCtrl*)eTask->out.ExtParam[i];
                        fwrite(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
                    }
                } // for(int i=0; i<eTask->out.NumExtParam; i++){
            } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
        } // if (m_encpakParams.bENCoPAKo)


        if (m_encpakParams.bENCPAK) {
            for (;;) {
                // at this point surface for encoder contains either a frame from file or a frame processed by vpp
                //Add output buffers
                if (numExtOutParams > 0) {
                    pCurrentTask->mfxBS.NumExtParam = numExtOutParams;
                    pCurrentTask->mfxBS.ExtParam = &outBufs[0];
                }
                if(m_encpakParams.bPREENC)
                    sts = m_pmfxENCPAK->EncodeFrameAsync(ctr, eTask->in.InSurface, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);
                else
                    sts = m_pmfxENCPAK->EncodeFrameAsync(ctr, &m_pEncSurfaces[nEncSurfIdx], &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

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
                    break;
                }
            }
        }
    }

    // means that the input file has ended, need to go to buffering loops
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (m_pmfxVPP)
    {
        // loop to get buffered frames from vpp
        while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts)
            // MFX_ERR_MORE_SURFACE can be returned only by RunFrameVPPAsync
            // MFX_ERR_MORE_DATA is accepted only from EncodeFrameAsync
        {
            // find free surface for encoder input (vpp output)
            nEncSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
            MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);

            for (;;)
            {
                sts = m_pmfxVPP->RunFrameVPPAsync(NULL, &m_pEncSurfaces[nEncSurfIdx], NULL, &VppSyncPoint);

                if (MFX_ERR_NONE < sts && !VppSyncPoint) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                }
                else if (MFX_ERR_NONE < sts && VppSyncPoint)
                {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    break;
                }
                else
                    break; // not a warning
            }

            if (MFX_ERR_MORE_SURFACE == sts)
            {
                continue;
            }
            else
            {
                MSDK_BREAK_ON_ERROR(sts);
            }

            // get a free task (bit stream and sync point for encoder)
            sts = GetFreeTask(&pCurrentTask);
            MSDK_BREAK_ON_ERROR(sts);

            // save the id of preceding vpp task which will produce input data for the encode task
            if (VppSyncPoint)
            {
                pCurrentTask->DependentVppTasks.push_back(VppSyncPoint);
                VppSyncPoint = NULL;
            }

            for (;;)
{
                //Add output buffers
                if (numExtOutParams > 0) {
                    pCurrentTask->mfxBS.NumExtParam = numExtOutParams;
                    pCurrentTask->mfxBS.ExtParam = &outBufs[0];
                }
                sts = m_pmfxENCPAK->EncodeFrameAsync(ctr, &m_pEncSurfaces[nEncSurfIdx], &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

                if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts)
                        MSDK_SLEEP(1); // wait if device is busy
                }
                else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP)
                {
                    sts = MFX_ERR_NONE; // ignore warnings if output is available
                    break;
                }
                else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
                {
                    sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                }
                else
                {
                    // get next surface and new task for 2nd bitstream in ViewOutput mode
                    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                    break;
                }
            }
        }

        // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
        // indicates that there are no more buffered frames
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
        // exit in case of other errors
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

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


                for (;;) {
                    //Only synced operation supported for now
                    fprintf(stderr, "frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);
                    sts = m_pmfxPREENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                    sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
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
                    if (mvout && !(eTask->frameType&MFX_FRAMETYPE_I))
                        fwrite(mvs[fieldId].MB, sizeof (mvs[fieldId].MB[0]) * mvs[fieldId].NumMBAlloc, 1, mvout);
                }

                //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                numUnencoded--;

                if (m_encpakParams.bENCPAK) {
                    ctr->FrameType = eTask->frameType;
                    eTask->in.InSurface->Data.FrameOrder = eTask->frameDisplayOrder;

                    //init MV predictors
                    for (fieldId = 0; fieldId < numOfFields; fieldId++)
                    {
                        memset(feiEncMVPredictors[fieldId].MB, 0,
                                sizeof (feiEncMVPredictors[fieldId].MB[0]) * feiEncMVPredictors[fieldId].NumMBAlloc);
                        for (mfxU32 i = 0; i < mvs[fieldId].NumMBAlloc; i++) {
                            //only one ref is used for now
                            /*
                            feiEncMVPredictors.MB[i].RefIdx[0].RefL0 = 0;
                            feiEncMVPredictors.MB[i].RefIdx[0].RefL1 = 0;
                            feiEncMVPredictors.MB[i].RefIdx[1].RefL0 = 0;
                            feiEncMVPredictors.MB[i].RefIdx[1].RefL1 = 0;
                            feiEncMVPredictors.MB[i].RefIdx[2].RefL0 = 0;
                            feiEncMVPredictors.MB[i].RefIdx[2].RefL1 = 0;
                            feiEncMVPredictors.MB[i].RefIdx[3].RefL0 = 0;
                            feiEncMVPredictors.MB[i].RefIdx[3].RefL1 = 0;
                             */

                            //use only first subblock component of MV
                            feiEncMVPredictors[fieldId].MB[i].MV[0][0].x = mvs[fieldId].MB[i].MV[0][0].x; //x L0
                            feiEncMVPredictors[fieldId].MB[i].MV[0][0].y = mvs[fieldId].MB[i].MV[0][0].y; //y L0
                            feiEncMVPredictors[fieldId].MB[i].MV[0][1].x = mvs[fieldId].MB[i].MV[0][1].x; //x L1
                            feiEncMVPredictors[fieldId].MB[i].MV[0][1].y = mvs[fieldId].MB[i].MV[0][1].y; //y L1
                        } // for (mfxI32 i = 0; i < mvs.NumMBAlloc; i++) {
                    } // for (fieldId = 0; fieldId < numOfFields; fieldId++)
                }

                if (m_encpakParams.bENCPAK) { //if we have both
                    for (;;) {
                        //Add output buffers
                        if (numExtOutParams > 0) {
                            pCurrentTask->mfxBS.NumExtParam = numExtOutParams;
                            pCurrentTask->mfxBS.ExtParam = &outBufs[0];
                        }
                        //no ctrl for buffered frames
                        //sts = m_pmfxENCPAK->EncodeFrameAsync(ctr, NULL, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);
                        sts = m_pmfxENCPAK->EncodeFrameAsync(ctr, eTask->in.InSurface, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

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
                            break;
                        }
                    }
                    MSDK_BREAK_ON_ERROR(sts);
                /* FIXME ???*/
                //drop output data to output file
                //pCurrentTask->WriteBitstream();
                } // if (m_encpakParams.bENCPAK) { //if we have both



            }  // while (numUnencoded != 0) {
        } //run processing on last frames  \n if (numUnencoded > 0) {

        //unlock last frames
        {
            std::list<iTask*>::iterator it = inputTasks.begin();
            for (; it != inputTasks.end(); ++it) {
                (*it)->in.InSurface->Data.Locked = 0;
            }
        }

        if (m_encpakParams.bENCPAK) { //if we have both
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
    }

    // loop to get buffered frames from encoder
    if (m_encpakParams.bENCoPAKo) {
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

                for (;;) {
                    //Only synced operation supported for now
                    fprintf(stderr, "frame: %d  t:%d : submit ", eTask->frameDisplayOrder, eTask->frameType);
                    sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                    sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                    fprintf(stderr, "synced : %d\n", sts);
                    MSDK_BREAK_ON_ERROR(sts);

                    sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
                    sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                    fprintf(stderr, "synced : %d\n", sts);
                    MSDK_BREAK_ON_ERROR(sts);

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
                pCurrentTask->WriteBitstream();

                for (fieldId = 0; fieldId < numOfFields; fieldId++)
                 {
                     mfxExtFeiEncMV* mvBuf=NULL;
                     mfxExtFeiEncMBStat* mbstatBuf = NULL;
                     mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
                     for(int i=0; i<eTask->out.NumExtParam; i++){
                         if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV && mvENCPAKout){
                             mvBuf = (mfxExtFeiEncMV*)eTask->out.ExtParam[i];
                             fwrite(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout);
                         }
                         if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT && mbstatout){
                             mbstatBuf = (mfxExtFeiEncMBStat*)eTask->out.ExtParam[i];
                             fwrite(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, mbstatout);
                         }
                         if(eTask->out.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL && mbcodeout){
                             mbcodeBuf = (mfxExtFeiPakMBCtrl*)eTask->out.ExtParam[i];
                             fwrite(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout);
                         }
                     } // for(int i=0; i<eTask->out.NumExtParam; i++){
                 } // for (fieldId = 0; fieldId < numOfFields; fieldId++)

                //MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                numUnencoded--;
            }  // while (numUnencoded != 0) {
        } //run processing on last frames  \n if (numUnencoded > 0) {

        //unlock last frames
        {
            std::list<iTask*>::iterator it = inputTasks.begin();
            for (; it != inputTasks.end(); ++it) {
                (*it)->in.InSurface->Data.Locked = 0;
                (*it)->outPAK.OutSurface->Data.Locked = 0;
            }
        }
    } //     if (m_encpakParams.bENCoPAKo) {

    if (m_encpakParams.bENCPAK && !m_encpakParams.bPREENC) {
        while (MFX_ERR_NONE <= sts) {
            // get a free task (bit stream and sync point for encoder)
            sts = GetFreeTask(&pCurrentTask);
            MSDK_BREAK_ON_ERROR(sts);

            for (;;) {
                //Add output buffers
                if (numExtOutParams > 0) {
                    pCurrentTask->mfxBS.NumExtParam = numExtOutParams;
                    pCurrentTask->mfxBS.ExtParam = &outBufs[0];
                }
                //no ctrl for buffered frames
                sts = m_pmfxENCPAK->EncodeFrameAsync(ctr, NULL, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

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

    if (m_encpakParams.bENCPAK || m_encpakParams.bPREENC || m_encpakParams.bENCoPAKo){
        if(mvout != NULL) fclose(mvout);
        if(mbstatout != NULL) fclose(mbstatout);
        if(mbcodeout != NULL) fclose(mbcodeout);
        if(mvENCPAKout != NULL) fclose(mvENCPAKout);
    }

    // MFX_ERR_NOT_FOUND is the correct status to exit the loop with
    // EncodeFrameAsync and SyncOperation don't return this status
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND);
    // report any errors that occurred in asynchronous part
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

void CEncodingPipeline::PrintInfo()
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Encoding Sample Version %s\n"), MSDK_SAMPLE_VERSION);
    msdk_printf(MSDK_STRING("\nInput file format\t%s\n"), ColorFormatToStr(m_FileReader.m_ColorFormat));
    msdk_printf(MSDK_STRING("Output video\t\t%s\n"), CodecIdToStr(m_mfxEncParams.mfx.CodecId).c_str());

    mfxFrameInfo SrcPicInfo = m_mfxVppParams.vpp.In;
    mfxFrameInfo DstPicInfo = m_mfxEncParams.mfx.FrameInfo;

    msdk_printf(MSDK_STRING("Source picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), SrcPicInfo.Width, SrcPicInfo.Height);
    msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), SrcPicInfo.CropX, SrcPicInfo.CropY, SrcPicInfo.CropW, SrcPicInfo.CropH);

    msdk_printf(MSDK_STRING("Destination picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), DstPicInfo.Width, DstPicInfo.Height);
    msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), DstPicInfo.CropX, DstPicInfo.CropY, DstPicInfo.CropW, DstPicInfo.CropH);

    msdk_printf(MSDK_STRING("Frame rate\t%.2f\n"), DstPicInfo.FrameRateExtN * 1.0 / DstPicInfo.FrameRateExtD);
    msdk_printf(MSDK_STRING("Bit rate(Kbps)\t%d\n"), m_mfxEncParams.mfx.TargetKbps);
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
                eTask->in.L0Surface = NULL; //eTask->in.InSurface;
                //in data
                eTask->in.NumExtParam = numExtInParamsPreEnc;
                eTask->in.ExtParam = inBufsPreEnc;
                //out data
                //exclude MV output from output buffers
                if (preENCCtr[fieldId].DisableStatisticsOutput) {
                    eTask->out.NumExtParam = 0;
                    eTask->out.ExtParam = NULL;
                } else {
                    eTask->out.NumExtParam = 1;
                    eTask->out.ExtParam = outBufsPreEncI;
                }

                preENCCtr[fieldId].DisableMVOutput = 1;
                break;
            case MFX_FRAMETYPE_P:
                eTask->in.NumFrameL0 = 1;
                eTask->in.NumFrameL1 = 0;
                eTask->in.L0Surface = &eTask->prevTask->in.InSurface;
                //in data
                eTask->in.NumExtParam = numExtInParamsPreEnc;
                eTask->in.ExtParam = inBufsPreEnc;
                //out data
                eTask->out.NumExtParam = numExtOutParamsPreEnc;
                eTask->out.ExtParam = outBufsPreEnc;
                break;
            case MFX_FRAMETYPE_B:
                eTask->in.NumFrameL0 = 1;
                eTask->in.NumFrameL1 = 1;
                eTask->in.L0Surface = &eTask->prevTask->in.InSurface;
                eTask->in.L1Surface = &eTask->nextTask->in.InSurface;
                //in data
                eTask->in.NumExtParam = numExtInParamsPreEnc;
                eTask->in.ExtParam = inBufsPreEnc;
                //out data
                eTask->out.NumExtParam = numExtOutParamsPreEnc;
                eTask->out.ExtParam = outBufsPreEnc;
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
                eTask->in.NumFrameL0 = eTask->inPAK.NumFrameL0 = 0; //1;  //just workaround to support I frames
                eTask->in.NumFrameL1 = eTask->inPAK.NumFrameL1 = 0;
                eTask->in.L0Surface = eTask->inPAK.L0Surface = NULL; //eTask->in.InSurface;
                //ENC data
                eTask->in.NumExtParam = numExtInParams;
                eTask->in.ExtParam = inBufs;
                eTask->out.NumExtParam = numExtOutParams;
                eTask->out.ExtParam = outBufs;
                /* PAK data */
                eTask->inPAK.NumExtParam = numExtOutParams;
                eTask->inPAK.ExtParam = outBufs;
                eTask->outPAK.NumExtParam = numExtInParams;
                eTask->outPAK.ExtParam = inBufs;
                break;
            case MFX_FRAMETYPE_P:
                eTask->in.NumFrameL0 = eTask->inPAK.NumFrameL0 = 1;
                eTask->in.NumFrameL1 = eTask->inPAK.NumFrameL1 = 0;
                eTask->in.L0Surface = eTask->inPAK.L0Surface = &eTask->prevTask->outPAK.OutSurface;
                //in data
                eTask->in.NumExtParam = numExtInParams;
                eTask->in.ExtParam = inBufs;
                //out data
                eTask->out.NumExtParam = numExtOutParams;
                eTask->out.ExtParam = outBufs;
                eTask->inPAK.NumExtParam = numExtOutParams;
                eTask->inPAK.ExtParam = outBufs;
                eTask->outPAK.NumExtParam = numExtInParams;
                eTask->outPAK.ExtParam = inBufs;
                break;
            case MFX_FRAMETYPE_B:
                eTask->in.NumFrameL0 = eTask->inPAK.NumFrameL0 = 1;
                eTask->in.NumFrameL1 = eTask->inPAK.NumFrameL1 = 1;
                eTask->in.L0Surface = eTask->inPAK.L0Surface = &eTask->prevTask->outPAK.OutSurface;
                eTask->in.L1Surface = eTask->inPAK.L1Surface = &eTask->nextTask->outPAK.OutSurface;
                //in data
                eTask->in.NumExtParam = numExtInParams;
                eTask->in.ExtParam = inBufs;
                //out data
                eTask->out.NumExtParam = numExtOutParams;
                eTask->out.ExtParam = outBufs;
                eTask->inPAK.NumExtParam = numExtOutParams;
                eTask->inPAK.ExtParam = outBufs;
                eTask->outPAK.NumExtParam = numExtInParams;
                eTask->outPAK.ExtParam = inBufs;
                break;
        }
    }
    mdprintf(stderr, "enc: %d t: %d\n", eTask->frameDisplayOrder, (eTask->frameType& MFX_FRAMETYPE_IPB));
}
