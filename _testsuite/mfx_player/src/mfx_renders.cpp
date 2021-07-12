/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2020 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_pipeline_utils.h"
#include "mfx_renders.h"
#include "mfx_frame_locker_utils.h"
#include "ippdc.h"
#include "ippcc.h"
#include "ippi.h"
#include "vm_file.h"
#include "mfx_serializer.h"
#include "mfx_extended_buffer.h"
#include "mfx_lucas_yuv_reader.h"
#include "lucas.h"
#include "shared_utils.h"

#include "mfx_sysmem_allocator.h"

//////////////////////////////////////////////////////////////////////////

MFXVideoRender::MFXVideoRender( IVideoSession * core
                              , mfxStatus *status)
    : m_nFourCC()
    , m_nFrames()
    , m_bFrameLocked()
    , m_VideoParams()
    , m_bIsViewRender(false)//until we call to init we don't know whether the render is a view render
    , m_bAutoView(false)
    , m_nViewId()
    , m_nMemoryModel(GENERAL_ALLOC)
    , m_bDecodeD3D11(false)
#if defined(_WIN32) || defined(_WIN64)
    , m_pLock(nullptr)
#endif
{
    if (NULL != status)
        *status = MFX_ERR_NONE;

    m_pSessionWrapper = core;
}

MFXVideoRender::~MFXVideoRender()
{
    Close();
}

mfxStatus MFXVideoRender::Query(mfxVideoParam *in, mfxVideoParam *out) 
{
    MFX_CHECK_POINTER(out);

    mfxExtBuffer** ptr = out->ExtParam;
    mfxU16 num = out->NumExtParam;

    if (in == 0)
        memset(out, 0, sizeof(*out));
    else
        memcpy(out, in, sizeof(*out));

    out->ExtParam = ptr;
    out->NumExtParam = num;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::QueryIOSurf(mfxVideoParam * /*par*/, mfxFrameAllocRequest *request)
{
    //MFX_CHECK_POINTER(par);
    MFX_CHECK_POINTER(request);

    request->NumFrameMin = request->NumFrameSuggested = 0;
    
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::Init(mfxVideoParam *par, const vm_char * /*pFilename*/)
{ 
    if (par != NULL)
    {
        m_VideoParams = *par;

        if (m_bAutoView)
        {
            MFXExtBufferPtr<mfxExtMVCSeqDesc> mvc_seq_dsc(m_VideoParams);

            if (mvc_seq_dsc.get())
            {
                m_bIsViewRender = mvc_seq_dsc->NumView != 0;
            }
        }
    }
    
    return MFX_ERR_NONE; 
}

mfxStatus MFXVideoRender::Close()
{
#if defined(_WIN32) || defined(_WIN64)
    if (m_pLock)
    {
        delete m_pLock;
        m_pLock = nullptr;
    }
#endif
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::Reset(mfxVideoParam *par) 
{ 
    UNREFERENCED_PARAMETER(par);
    return MFX_ERR_NONE; 
}

mfxStatus MFXVideoRender::GetVideoParam(mfxVideoParam *par) 
{ 
    UNREFERENCED_PARAMETER(par);
    return MFX_ERR_NONE; 
}

mfxStatus MFXVideoRender::GetEncodeStat(mfxEncodeStat *stat)
{
    MFX_CHECK_POINTER(stat);
    MFX_ZERO_MEM(*stat);
    stat->NumFrame = m_nFrames;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::LockFrame(mfxFrameSurface1 *pSurface)
{
    if (m_nMemoryModel == GENERAL_ALLOC)
    {
        if (NULL != pSurface)
        {
            if (NULL == pSurface->Data.Y && 
                NULL == pSurface->Data.U &&
                NULL == pSurface->Data.V &&
                NULL != pSurface->Data.MemId)
            {
                //external allocator is used
                MFX_CHECK_POINTER(m_pSessionWrapper);
                mfxFrameAllocator *pAlloc = m_pSessionWrapper->GetFrameAllocator();
                MFX_CHECK_POINTER(pAlloc);
                MFX_CHECK_STS(pAlloc->Lock(pAlloc->pthis, pSurface->Data.MemId, &pSurface->Data));
                m_bFrameLocked = true; //to prevent double unlock case
            }
        }
    }
    else if ((m_nMemoryModel == VISIBLE_INT_ALLOC || m_nMemoryModel == HIDDEN_INT_ALLOC) && pSurface->FrameInterface)
    {
        MFX_CHECK_STS(pSurface->FrameInterface->Map(pSurface, MFX_MAP_READ));
    }
    
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::UnlockFrame(mfxFrameSurface1 *pSurface)
{
    if (m_nMemoryModel == GENERAL_ALLOC)
    {
        if (NULL != pSurface && NULL != pSurface->Data.MemId && m_bFrameLocked)
        {
            //external allocator is used
            MFX_CHECK_POINTER(m_pSessionWrapper);
            mfxFrameAllocator *pAlloc = m_pSessionWrapper->GetFrameAllocator();
            MFX_CHECK_POINTER(pAlloc);
            MFX_CHECK_STS(pAlloc->Unlock(pAlloc->pthis, pSurface->Data.MemId, &pSurface->Data));
            m_bFrameLocked = false;
        }
    }
    else if ((m_nMemoryModel == VISIBLE_INT_ALLOC || m_nMemoryModel == HIDDEN_INT_ALLOC) && pSurface->FrameInterface)
    {
        MFX_CHECK_STS(pSurface->FrameInterface->Unmap(pSurface));
    }
    
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::GetFrameHDL(mfxMemId mid, mfxHDL* handle)
{
    if (NULL != mid)
    {
        MFX_CHECK_POINTER(m_pSessionWrapper);
        mfxFrameAllocator* pAlloc = m_pSessionWrapper->GetFrameAllocator();
        MFX_CHECK_POINTER(pAlloc);
        MFX_CHECK_STS(pAlloc->GetHDL(pAlloc->pthis, mid, handle));
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl) 
{ 
    UNREFERENCED_PARAMETER(surface);
    UNREFERENCED_PARAMETER(pCtrl);
    if (NULL != surface)
    {
        m_nViewId = surface->Info.FrameId.ViewId;
    }
    m_nFrames ++;
    return MFX_ERR_NONE; 
}

mfxStatus MFXVideoRender::WaitTasks(mfxU32 nMilisecconds)
{
    UNREFERENCED_PARAMETER(nMilisecconds);
    return MFX_WRN_IN_EXECUTION; 
}

mfxStatus MFXVideoRender::SetOutputFourcc(mfxU32 nFourCC)
{
    m_nFourCC = nFourCC;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::SetAutoView(bool bIsAutoViewRender)
{
    m_bAutoView = bIsAutoViewRender;
    return MFX_ERR_NONE;
}

void MFXVideoRender::SetMemoryModel(MemoryModel memoryModel)
{
    m_nMemoryModel = memoryModel;
}

mfxStatus MFXVideoRender::GetDownStream(IFile **ppFile)
{
    MFX_CHECK_POINTER(ppFile);
    *ppFile = m_pFile.get();
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::SetDownStream(IFile *pFile)
{
    //do not destroy object
    m_pFile.release();
    m_pFile.reset(pFile);
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::GetDevice(IHWDevice **ppDevice)
{
    MFX_CHECK_POINTER(ppDevice);
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::SetDevice(IHWDevice* pDevice)
{
    MFX_CHECK_POINTER(pDevice);
    m_pHWDevice = pDevice;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::SetDecodeD3D11(bool bDecodeD3D11)
{
    m_bDecodeD3D11 = bDecodeD3D11;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoRender::SetVDSFCFormat(bool bVDSFCFormatSetting)
{
    m_bVDSFCFormatSetting = bVDSFCFormatSetting;
    return MFX_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
MFXFileWriteRender::MFXFileWriteRender(const FileWriterRenderInputParams &params, IVideoSession * core, mfxStatus *status)
    : MFXVideoRender(core, status)
    , m_params(params)
    , m_nFourCC(params.info.FourCC)
    , m_pOpenMode(VM_STRING("wb"))
    , m_auxSurface()
    , m_Current()
{
#if defined(_WIN32) || defined(_WIN64)
    if (m_nFourCC == DXGI_FORMAT_AYUV)
        m_nFourCC = MFX_FOURCC_AYUV;
#endif

    //m_fDest = NULL;
    m_nTimesClosed = 0;
    m_bCreateNewFileOnClose = false;

    MFX_ZERO_MEM(m_surfaceForCopy);
}

MFXFileWriteRender::~MFXFileWriteRender()
{
    Close();

#ifdef LUCAS_DLL
    lucas::CLucasCtx &lucasCtx = lucas::CLucasCtx::Instance();
    if(lucasCtx->output ) {
        // release last buffer
        for (std::map<int, char*>::iterator i = m_lucas_buffer.begin(); i != m_lucas_buffer.end(); i++)
        {
            lucasCtx->output(lucasCtx->fmWrapperPtr, i->first, i->second, 0, -1);
        }
    }
#endif
}

MFXFileWriteRender * MFXFileWriteRender::Clone()
{
    mfxStatus sts = MFX_ERR_NONE;

    std::auto_ptr<MFXFileWriteRender> pNew (new MFXFileWriteRender(m_params, m_pSessionWrapper, &sts));
    MFX_CHECK_WITH_ERR(sts == MFX_ERR_NONE, NULL);
    MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pNew->SetAutoView(m_bAutoView), NULL);
    if (m_pFile.get())
    {
        MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pNew->SetDownStream(m_pFile->Clone()), NULL);
    }
    MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pNew->Init(&m_VideoParams, m_OutputFileName.c_str()), NULL);

    return pNew.release();
}

mfxStatus MFXFileWriteRender::Init(mfxVideoParam *pInit, const vm_char *pFilename)
{
    MFX_CHECK_STS(MFXVideoRender::Init(pInit, pFilename));
    MFX_CHECK_POINTER(pInit);
    MFX_CHECK_POINTER(m_pFile.get());
    
    MFX_CHECK(0 != pInit->mfx.FrameInfo.Width);
    MFX_CHECK(0 != pInit->mfx.FrameInfo.Height);

#ifdef LUCAS_DLL
    if(!lucas::CLucasCtx::Instance()->output) {
#endif
    //if render is view render we dont know exat file name at this point, 
    //we receive viewid only at rendering
    if (m_bIsViewRender)
    {
        if (!m_pFile->isOpen() && vm_string_strlen(pFilename))
        {
            m_nFrames = 0;
            m_OutputFileName = pFilename;
        }
    }
    else
    {
        if (!m_pFile->isOpen() && 0 != vm_string_strlen(pFilename))
        {
            MFX_CHECK_STS(m_pFile->Open(pFilename, m_pOpenMode));
            //MFX_CHECK_FOPEN(m_fDest, pFilename, m_pOpenMode);
            m_nFrames = 0;
            m_OutputFileName = pFilename;
        }else if (m_pFile->isOpen() && m_bCreateNewFileOnClose)
        {
            m_pFile->Close();
            //fclose(m_fDest);
            tstring targetname = FileNameWithId(m_OutputFileName.c_str(), m_nTimesClosed);
            MFX_CHECK_STS(m_pFile->Open(targetname.c_str(), m_pOpenMode));
            PrintInfo(VM_STRING("Output File"), VM_STRING("%s"), targetname.c_str());
        }
    }
#ifdef LUCAS_DLL
    }
#endif

    MFX_CHECK_STS(AllocAuxSurface());

    return MFX_ERR_NONE;
}

mfxStatus MFXFileWriteRender::Close()
{
    MFX_CHECK_STS(ReleaseSurface(m_auxSurface));
    MFX_CHECK_STS(ReleaseSurface(m_surfaceForCopy));

    m_nTimesClosed++;

    return MFX_ERR_NONE;
}

mfxStatus MFXFileWriteRender::AllocAuxSurface()
{
    if (m_nFourCC == 0 || m_nFourCC == MFX_FOURCC_YV12 || m_nFourCC == MFX_FOURCC_YV16 ||
        m_nFourCC == MFX_FOURCC_YUV420_16 || m_nFourCC == MFX_FOURCC_YUV422_16 || m_nFourCC == MFX_FOURCC_YUV444_16 ||
        m_nFourCC == MFX_FOURCC_YUV444_8)
    {
        mfxFrameInfo targetInfo = m_VideoParams.mfx.FrameInfo;
        if (!m_params.useHMstyle)
        {
            VM_ASSERT(m_nFourCC != MFX_FOURCC_YUV422_16);
            bool useP010 = m_nFourCC == MFX_FOURCC_YUV420_16 || m_params.use10bitOutput;
            targetInfo.FourCC = useP010 ? MFX_FOURCC_YUV420_16 : static_cast<int>(MFX_FOURCC_YV12);
            if (m_VideoParams.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422)
                targetInfo.FourCC = MFX_FOURCC_YV16;
        }
        else
        {
            targetInfo.FourCC = m_nFourCC;
        }
        MFX_CHECK_STS(AllocSurface(&targetInfo, &m_auxSurface));
    }
  return MFX_ERR_NONE;
}

mfxStatus MFXFileWriteRender::SetOutputFourcc(mfxU32 nFourCC)
{
    if (m_nFourCC != nFourCC)
    {
        m_nFourCC = nFourCC;
        //Clean up previos surface data
        MFX_CHECK_STS(ReleaseSurface(m_auxSurface));
        //Reallocate surface with new fcc
        MFX_CHECK_STS(AllocAuxSurface());
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXFileWriteRender::ReleaseSurface(mfxFrameSurface1& surf)
{
    mfxU8* ptr = MFX_MIN_POINTER(MFX_MIN_POINTER(surf.Data.Y, surf.Data.U), MFX_MIN_POINTER(surf.Data.V, surf.Data.A));
    if (ptr)
        delete[] ptr;
    MFX_ZERO_MEM(surf);
    return MFX_ERR_NONE;
}

mfxStatus MFXFileWriteRender::RenderFrame(mfxFrameSurface1 * pSurface, mfxEncodeCtrl * pCtrl)
{
    // NULL means flushing (end of stream)
    if(NULL == pSurface)
        return MFX_ERR_NONE;


#ifndef LUCAS_DLL
    //need to open file if it is a view render
    if (m_bIsViewRender && !m_pFile->isOpen())
    {
        MFX_CHECK_STS(m_pFile->Open(FileNameWithId(m_OutputFileName.c_str(), pSurface->Info.FrameId.ViewId).c_str(), m_pOpenMode));
    }
#endif

#if defined(_WIN32) || defined(_WIN64)

    bool bFrameLocked = false;
    if (m_bDecodeD3D11)
    {
        mfxHDLPair handle;
        if (NULL != pSurface->Data.MemId)
        {
            MFX_CHECK_STS(GetFrameHDL(pSurface->Data.MemId, (mfxHDL*)(&handle)));
        }

        if (m_pLock == nullptr)
            m_pLock = new MFXFrameLocker(m_pHWDevice);
        if (NULL != pSurface)
        {
            if (NULL == pSurface->Data.Y &&
                NULL == pSurface->Data.U &&
                NULL == pSurface->Data.V)
            {
                MFX_CHECK_STS(m_pLock->MapFrame(&pSurface->Data, (mfxHDL*)(&handle)));
                bFrameLocked = true;
            }  
        }
    }
    else
#endif
    {
        MFX_CHECK_STS(LockFrame(pSurface));
    }

    mfxFrameSurface1 * pConvertedSurface = pSurface;

    //so target fourcc is different than input
    if ( (m_nFourCC != pSurface->Info.FourCC) && (m_bVDSFCFormatSetting == false) )
    {
        if (pSurface->Info.Width != m_auxSurface.Info.Width || pSurface->Info.Height != m_auxSurface.Info.Height)
        {
            // reallocate
            MFX_CHECK_STS(ReleaseSurface(m_auxSurface));
            mfxU32 nOldCC = m_VideoParams.mfx.FrameInfo.FourCC;
            if (!m_params.useHMstyle)
            {
                bool useP010 = m_nFourCC == MFX_FOURCC_YUV420_16 || m_params.use10bitOutput;
                m_VideoParams.mfx.FrameInfo.FourCC = useP010 ? MFX_FOURCC_YUV420_16 : static_cast<int>(MFX_FOURCC_YV12);
                if (m_VideoParams.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422)
                    m_VideoParams.mfx.FrameInfo.FourCC = MFX_FOURCC_YV16;
            }
            else
            {
                m_VideoParams.mfx.FrameInfo.FourCC = m_nFourCC;
            }
            m_VideoParams.mfx.FrameInfo.Width = pSurface->Info.Width;
            m_VideoParams.mfx.FrameInfo.CropW = pSurface->Info.CropW;
            m_VideoParams.mfx.FrameInfo.Height = pSurface->Info.Height;
            m_VideoParams.mfx.FrameInfo.CropH = pSurface->Info.CropH;
            MFX_CHECK_STS(AllocSurface(&m_VideoParams.mfx.FrameInfo, &m_auxSurface));
            m_VideoParams.mfx.FrameInfo.FourCC = nOldCC;
        }
        //selecting converter
        pConvertedSurface = ConvertSurface(pSurface, &m_auxSurface, &m_params);
        if (!pConvertedSurface)
            return MFX_ERR_UNKNOWN;
    }

    MFX_CHECK_STS(WriteSurface(pConvertedSurface));

    //MFXTimingDump_StopTimer("CopyFrame", "app", MFX_ERR_NONE);

#if defined(_WIN32) || defined(_WIN64)
    if (m_bDecodeD3D11)
    {
        if (NULL != pSurface && bFrameLocked)
            MFX_CHECK_STS(m_pLock->UnmapFrame(&pSurface->Data));
    }
    else
#endif
    {
        MFX_CHECK_STS(UnlockFrame(pSurface));
    }

    return MFXVideoRender::RenderFrame(pSurface, pCtrl);
}

#ifdef LUCAS_DLL
#define WRITE(buff, count) \
    if(!lucasCtx->output) { \
    MFX_CHECK_STS(PutData(buff, count)); \
    } \
  else { \
  memcpy(lucas_ptr, buff, count); \
  lucas_ptr += count; \
}
#else
#define WRITE(buff, count) MFX_CHECK_STS(PutData(buff, count));
#endif

void XOR31(mfxU16 *data, size_t size);

mfxStatus MFXFileWriteRender::WriteSurface(mfxFrameSurface1 * pConvertedSurface)
{
    unsigned int i;
    mfxFrameInfo * pInfo = &pConvertedSurface->Info;
    mfxFrameData * pData = &pConvertedSurface->Data;

#ifdef LUCAS_DLL
    lucas::CLucasCtx &lucasCtx = lucas::CLucasCtx::Instance();
    int lucas_bufferSize = pInfo->CropW * pInfo->CropH * 3 / 2;
    if(lucasCtx->output && NULL == m_lucas_buffer[pInfo->FrameId.ViewId]){
          // get buffer for corresponding MVC view ID 
          m_lucas_buffer[pInfo->FrameId.ViewId] = lucasCtx->output(lucasCtx->fmWrapperPtr, pInfo->FrameId.ViewId, 0, 0, lucas_bufferSize);
    }
    char *lucas_ptr = m_lucas_buffer[pInfo->FrameId.ViewId];
#endif
    
    Ipp32s crop_x = pInfo->CropX;
    Ipp32s crop_y = pInfo->CropY;
    mfxU32 pitch = pData->PitchLow + ((mfxU32)pData->PitchHigh << 16);

    bool skipChroma = !m_params.alwaysWriteChroma && pConvertedSurface->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV400;

    switch (pInfo->FourCC)
    {
        case MFX_FOURCC_YV12: //actually it is i420 format not YV12
        case MFX_FOURCC_YV16: //actually it is i422 format not YV16
        case MFX_FOURCC_YUV444_8:
        {
            mfxU8 isHalfHeight = pInfo->FourCC == MFX_FOURCC_YV12 ? 1 : 0;
            mfxU8 isHalfWidth  = pInfo->FourCC == MFX_FOURCC_YUV444_8 ? 0 : 1;

            m_Current.m_comp = VM_STRING('Y');
            m_Current.m_pixX = 0;

            for (i = 0; i < pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                WRITE(pData->Y + (crop_y * pitch + crop_x) + i*pitch, pInfo->CropW);
            }

            if (!skipChroma)
            {
                crop_x >>= isHalfWidth;
                crop_y >>= isHalfHeight;

                mfxU32 height = isHalfHeight ? ((pInfo->CropH + 1) >> 1) : pInfo->CropH;
                mfxU32 width = isHalfWidth ? ((pInfo->CropW + 1) >> 1) : pInfo->CropW;
                if(isHalfWidth)
                    pitch /= 2;

                m_Current.m_comp = VM_STRING('U');
                for (i = 0; i < height; i++)
                {
                    m_Current.m_pixY = i;
                    WRITE(pData->U + (crop_y * pitch + crop_x) + i*pitch, width);
                }
                m_Current.m_comp = VM_STRING('V');
                for (i = 0; i < height; i++)
                {
                    m_Current.m_pixY = i;
                    WRITE(pData->V + (crop_y * pitch + crop_x) + i*pitch, width);
                }
            }
            break;
        }
        case MFX_FOURCC_YUV444_16:
        case MFX_FOURCC_YUV422_16:
        case MFX_FOURCC_YUV420_16:
        {
            m_Current.m_comp = VM_STRING('Y');
            m_Current.m_pixX = 0;

            crop_x <<= 1; // because of sample equals two bytes 

            for (i = 0; i < pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                if (m_params.VpxDec16bFormat)
                    XOR31((mfxU16 *)(pData->Y + (crop_y * pitch + crop_x) + i*pitch), pInfo->CropW);

                WRITE(pData->Y + (crop_y * pitch + crop_x) + i*pitch, pInfo->CropW*2);
            }

            mfxU8 isHalfHeight = pInfo->FourCC == MFX_FOURCC_YUV420_16 ? 1 : 0;
            mfxU8 isHalfWidth  = pInfo->FourCC == MFX_FOURCC_YUV444_16 ? 0 : 1;
            if (isHalfWidth)
                pitch /= 2;

            if (!skipChroma)
            {
                crop_x >>= isHalfWidth;
                crop_y >>= isHalfHeight;

                mfxU32 height = isHalfHeight ? ((pInfo->CropH + 1) / 2) : pInfo->CropH;
                mfxU32 width = isHalfWidth ? ((pInfo->CropW + 1) / 2) : pInfo->CropW;

                m_Current.m_comp = VM_STRING('U');
                for (i = 0; i < height; i++)
                {
                    m_Current.m_pixY = i;
                    if (m_params.VpxDec16bFormat)
                        XOR31((mfxU16 *)(pData->U + (crop_y * pitch + crop_x) + i*pitch), width);

                    WRITE(pData->U + (crop_y * pitch + crop_x) + i*pitch, width*2);
                }
                m_Current.m_comp = VM_STRING('V');
                for (i = 0; i < height; i++)
                {
                    m_Current.m_pixY = i;
                    if (m_params.VpxDec16bFormat)
                        XOR31((mfxU16 *)(pData->V + (crop_y * pitch + crop_x) + i*pitch), width);

                    WRITE(pData->V + (crop_y * pitch + crop_x) + i*pitch, width*2);
                }
            }
            break;
        }
        case MFX_FOURCC_NV12:
        {
            m_Current.m_comp = VM_STRING('Y');
            m_Current.m_pixX = 0;
            for (i = 0; i < pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                WRITE(pData->Y + (pInfo->CropY * pitch + pInfo->CropX)+ i * pitch, pInfo->CropW);
            }

            if (!skipChroma)
            {
                m_Current.m_comp = VM_STRING('U');
                for (i = 0; i < (mfxU16) ((pInfo->CropH + 1) / 2); i++)
                {
                    m_Current.m_pixY = i;
                    WRITE(pData->UV + (pInfo->CropY * pitch / 2 + pInfo->CropX) + i * pitch, pInfo->CropW);

                }
            }
            break;
        }
        case MFX_FOURCC_P010:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        case MFX_FOURCC_P016:
#endif
        {
            m_Current.m_comp = VM_STRING('Y');
            m_Current.m_pixX = 0;

            crop_x <<= 1; // sample - two bytes

            for (i = 0; i < pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                WRITE(pData->Y + (pInfo->CropY * pitch + crop_x)+ i * pitch, pInfo->CropW * 2);
            }

            if (!skipChroma)
            {
                m_Current.m_comp = VM_STRING('U');

                crop_y >>= 1;
                for (i = 0; i < (mfxU16) ((pInfo->CropH + 1) / 2); i++)
                {
                    m_Current.m_pixY = i;
                    WRITE(pData->UV + (crop_y*pitch + crop_x) + i * pitch, pInfo->CropW * 2);
                }
            }
            break;
        }
        case MFX_FOURCC_P210:
        {
            m_Current.m_comp = VM_STRING('Y');
            m_Current.m_pixX = 0;
            for (i = 0; i < pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                WRITE(pData->Y + (pInfo->CropY * pitch + pInfo->CropX)+ i * pitch, pInfo->CropW * 2);
            }

            if (!skipChroma)
            {
                m_Current.m_comp = VM_STRING('U');
                for (i = 0; i < pInfo->CropH; i++)
                {
                    m_Current.m_pixY = i;
                    WRITE(pData->UV + (pInfo->CropY * pitch / 2 + pInfo->CropX) + i * pitch, pInfo->CropW * 2);
                }
            }
            break;
        }
        case MFX_FOURCC_ARGB16:
        {
            mfxU8 *plane = pData->B + pInfo->CropX * 4 + pInfo->CropY * pitch;
            for (i = 0; i < pInfo->CropH; i++)
            {
                MFX_CHECK_STS(PutData(plane, pInfo->CropW * 8));
                plane += pitch;
            }
            break;
        }
        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_AYUV:
        {
            m_Current.m_comp = VM_STRING('R');
            m_Current.m_pixX = 0;
            mfxU8* plane = pData->B + pInfo->CropX * 4;

            for (i = 0; i <pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                MFX_CHECK_STS(PutData(plane, pInfo->CropW * 4));
                plane += pitch;
            }
            break;
        }
        case MFX_FOURCC_BGR4:
        {
            m_Current.m_comp = VM_STRING('R');
            m_Current.m_pixX = 0;
            mfxU8* plane = pData->R + pInfo->CropX * 4;

            for (i = 0; i <pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                MFX_CHECK_STS(PutData(plane, pInfo->CropW * 4));
                plane += pitch;
            }
            break;
        }
        case MFX_FOURCC_A2RGB10:
        {
            m_Current.m_comp = VM_STRING('R');
            m_Current.m_pixX = 0;
            mfxU8* plane = pData->B + pInfo->CropX * 4;

            for (i = 0; i <pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                MFX_CHECK_STS(PutData(plane, pInfo->CropW * 4));
                plane += pitch;
            }
            break;
        }
#if defined(_WIN32) || defined(_WIN64)
        case DXGI_FORMAT_AYUV :
        {
            m_Current.m_comp = VM_STRING('R');
            m_Current.m_pixX = 0;
            mfxU8* plane = pData->B + pInfo->CropX * 4;

            for (i = 0; i <pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                MFX_CHECK_STS(PutData(plane, pInfo->CropW * 4));
                plane += pitch;
            }
            break;
        }
#endif
        case MFX_FOURCC_YUY2 :
        {
            m_Current.m_comp = VM_STRING('Y');
            m_Current.m_pixX = 0;
            mfxU8* plane = pData->Y + pInfo->CropY * pitch + pInfo->CropX * 2;


            for (i = 0; i <pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                MFX_CHECK_STS(PutData(plane, pInfo->CropW * 2));
                plane += pitch;
            }
            break;
        }
        case MFX_FOURCC_NV16:
        {
            m_Current.m_comp = VM_STRING('Y');
            m_Current.m_pixX = 0;
            for (i = 0; i < pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                WRITE(pData->Y + (pInfo->CropY * pitch + pInfo->CropX)+ i * pitch, pInfo->CropW);
            }
            m_Current.m_comp = VM_STRING('U');
            for (i = 0; i < pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                WRITE(pData->UV + (pInfo->CropY * pitch / 2 + pInfo->CropX) + i * pitch, pInfo->CropW);
            }
            break;
        }
        case MFX_FOURCC_Y210:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        case MFX_FOURCC_Y216:
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
        {
            m_Current.m_comp = VM_STRING('Y');
            m_Current.m_pixX = 0;
            for (i = 0; i < pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                WRITE(pData->Y + (4*pInfo->CropY * pitch + 4*pInfo->CropX) + i * pitch, 4 * pInfo->CropW);
            }
            break;
        }
        case MFX_FOURCC_Y410:
        {
            m_Current.m_comp = VM_STRING('U');
            m_Current.m_pixX = 0;
            mfxU8* plane = pData->U + pInfo->CropX * 4;

            for (i = 0; i <pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                MFX_CHECK_STS(PutData(plane, pInfo->CropW * 4));
                plane += pitch;
            }
            break;
        }
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        case MFX_FOURCC_Y416:
        {
            m_Current.m_comp = VM_STRING('U');
            m_Current.m_pixX = 0;
            mfxU8* plane = pData->U + pInfo->CropX * 8;

            for (i = 0; i <pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                MFX_CHECK_STS(PutData(plane, pInfo->CropW * 8));
                plane += pitch;
            }
            break;
        }
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
        case MFX_FOURCC_RGBP:
        {
            m_Current.m_comp = VM_STRING('R');
            m_Current.m_pixX = 0;
            mfxU8* plane = pData->R + pInfo->CropX;
            for (i = 0; i <pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                MFX_CHECK_STS(PutData(plane, pInfo->CropW));
                plane += pitch;
            }
            m_Current.m_comp = VM_STRING('G');
            plane = pData->G + pInfo->CropX;
            for (i = 0; i <pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                MFX_CHECK_STS(PutData(plane, pInfo->CropW));
                plane += pitch;
            }
            m_Current.m_comp = VM_STRING('B');
            plane = pData->B + pInfo->CropX;
            for (i = 0; i <pInfo->CropH; i++)
            {
                m_Current.m_pixY = i;
                MFX_CHECK_STS(PutData(plane, pInfo->CropW));
                plane += pitch;
            }
            break;
        }
        default:
        {
            MFX_TRACE_AND_EXIT(MFX_ERR_UNSUPPORTED, (VM_STRING("[MFXFileWriteRender] file writing in %s colorformat not supported\n"), 
                GetMFXFourccString(pInfo->FourCC).c_str()));
        }
    }

    m_pFile->FrameBoundary();

#ifdef LUCAS_DLL
    if(lucasCtx->output) {
      // send current buffer and get a new one
      m_lucas_buffer[pInfo->FrameId.ViewId] = lucasCtx->output(lucasCtx->fmWrapperPtr, pInfo->FrameId.ViewId, m_lucas_buffer[pInfo->FrameId.ViewId], lucas_bufferSize, lucas_bufferSize);
    }
#endif

    
    return MFX_ERR_NONE;
}

mfxStatus MFXFileWriteRender::PutData(mfxU8* pData, mfxU32 nSize)
{
    if (NULL == pData)
    {
        return PutBsData(NULL);
    }
    mfxBitstream bs ={0};
    bs.Data = pData;
    bs.DataLength = nSize;
    return PutBsData(&bs);
}

mfxStatus MFXFileWriteRender::PutBsData(mfxBitstream *pBs)
{
#ifdef LUCAS_DLL
    lucas::CLucasCtx &lucasCtx = lucas::CLucasCtx::Instance();
    if(!lucasCtx->output)
#endif
    //if (!m_fDest) return MFX_ERR_NONE;
    if (!m_pFile->isOpen()) return MFX_ERR_NONE;

#ifdef LUCAS_DLL
    if(!lucasCtx->output) {
#endif
    //encoder often returns zero length frames
    //MFX_CHECK(nSize != 0);
    MFX_CHECK_STS(m_pFile->Write(pBs));
    //MFX_CHECK_STS(m_pFile->WritenSize == fwrite(pData, 1, nSize, m_fDest));
#ifdef LUCAS_DLL
    }
    else {
        char *ptr = pBs ? (char *)(pBs->Data + pBs->DataOffset) : NULL;
        int bufLen  = pBs ? pBs->DataLength : 0;
        lucasCtx->output(lucasCtx->fmWrapperPtr, 0, ptr, bufLen, 0);
    }
#endif
    return MFX_ERR_NONE;
}
//////////////////////////////////////////////////////////////////////////
MFXBMPRender::MFXBMPRender(IVideoSession *core, mfxStatus *status)
    : MFXFileWriteRender(FileWriterRenderInputParams(MFX_FOURCC_RGB4), core, status)
{
}

MFXBMPRender::~MFXBMPRender()
{
    Close();
}

mfxStatus MFXBMPRender::RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl)
{
    if (NULL == surface)
        return MFXFileWriteRender::RenderFrame(surface, pCtrl);

    bmpfile_DIBheader dibheader;
    
    dibheader.header_sz = sizeof(bmpfile_DIBheader);
    dibheader.width  = m_VideoParams.mfx.FrameInfo.CropW;
    dibheader.height = m_VideoParams.mfx.FrameInfo.CropH;
    dibheader.nplanes =1;
    dibheader.bitspp=32;
    dibheader.compress_type = 0L;
    dibheader.bmp_bytesz = m_VideoParams.mfx.FrameInfo.CropW * m_VideoParams.mfx.FrameInfo.CropH * 4;
    dibheader.hres = 100;
    dibheader.vres = 100;
    dibheader.ncolors = 0;
    dibheader.nimpcolors = 0;

    bmpfile_header h = {0};
    h.bmp_offset = sizeof(bmpfile_DIBheader) + sizeof(bmpfile_magic) + sizeof(bmpfile_header);
    h.filesz = dibheader.bmp_bytesz + h.bmp_offset;

    bmpfile_magic magic = {'B','M'};


    MFX_CHECK_STS(PutData((mfxU8*)&magic, sizeof(magic)));
    MFX_CHECK_STS(PutData((mfxU8*)&h, sizeof(h)));
    MFX_CHECK_STS(PutData((mfxU8*)&dibheader, sizeof(dibheader)));

    return MFXFileWriteRender::RenderFrame(surface, pCtrl);
}

mfxStatus MFXBMPRender::WriteSurface(mfxFrameSurface1 * pConvertedSurface)
{
    mfxU32 i;
    mfxFrameInfo     * pInfo = &pConvertedSurface->Info;
    mfxFrameData     * pData = &pConvertedSurface->Data;

    MFX_CHECK(pInfo->FourCC == MFX_FOURCC_RGB4);

    m_Current.m_comp = VM_STRING('R');
    m_Current.m_pixX = 0;
    mfxU32 pitch = pData->PitchLow + ((mfxU32)pData->PitchHigh << 16);
    mfxU8* plane = pData->B + (pInfo->CropY * pitch + pInfo->CropX * 4) + (pInfo->CropH - 1) * pitch;

    for (i = pInfo->CropH; i > 0; i--)
    {
        m_Current.m_pixY = i;
        MFX_CHECK_STS(PutData(plane, pInfo->CropW * 4));
        plane -= pitch;
    }
    return MFX_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
MFXMetricComparatorRender::MFXMetricComparatorRender(const FileWriterRenderInputParams & params, IVideoSession *core, mfxStatus *status)
    : MFXFileWriteRender(params, core, status)
#ifdef LUCAS_DLL
    , m_reader(new LucasYUVReader)
#else
    , m_reader(new CYUVReader)
#endif
    , m_refsurface()
    , m_pRefsurface(&m_refsurface)
    , m_bAllocated()
    , m_nInFileLen(0)
    , m_nNewFileOffset(0)
    , m_nOldFileOffset()
    , m_pRefArray(0)
    , m_nRefArray()
    , m_uiDeltaVal()
    , m_metricType(0)
    , m_bFileSourceMode()
    , m_bVerbose()
    , m_nFrameToProcess()
    , m_pMetricsOutFileName()
    , m_bFirstFrame()
    , m_bDelaySetSurface()
    , m_bDelaySetOutputPerfFile()
{
}

MFXMetricComparatorRender * MFXMetricComparatorRender::Clone()
{
    mfxStatus sts = MFX_ERR_NONE;
    
    std::auto_ptr<MFXMetricComparatorRender> pNew (new MFXMetricComparatorRender(m_params, m_pSessionWrapper, &sts));
    MFX_CHECK_WITH_ERR(sts == MFX_ERR_NONE, NULL);

    MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pNew->SetAutoView(m_bAutoView), NULL);
    
    if (m_pFile.get())
    {
        MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pNew->SetDownStream(m_pFile->Clone()), NULL);
    }


    MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pNew->Init(&m_VideoParams, m_OutputFileName.c_str()), NULL);
    if (!m_pMetricsOutFileName.empty())
    {
        MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pNew->SetOutputPerfFile(
            m_pMetricsOutFileName.c_str()), NULL);
    }
    if (m_bFileSourceMode)
    {
        MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pNew->SetSurfacesSource(
            m_SurfacesSourceFileName.c_str(), NULL, m_bVerbose, m_nFrameToProcess), NULL);
    }else
    {
        //TODO: how we can implement this
    }
    
    //need to populate metrics
    for(size_t i=0; i < m_metrics_input.size(); i++)
    {
        MFX_CHECK_WITH_ERR(MFX_ERR_NONE == pNew->AddMetric(m_metrics_input[i].first, m_metrics_input[i].second), NULL);
    }

    return pNew.release();
}

MFXMetricComparatorRender::~MFXMetricComparatorRender()
{
    Close();

    vm_char  planes[] = VM_STRING("YUV");    
    mfxU32 const nMetricNumberSize = 4;
    vm_char  pMetricNumber[nMetricNumberSize + 1] = VM_STRING("");
    if (m_bIsViewRender)
    {
          vm_string_snprintf(pMetricNumber, nMetricNumberSize, VM_STRING("_%d"), m_nViewId);
    }

    for (size_t i = 0; i < m_pComparators.size(); i++)
    {
        mfxF64 metricVal[3];
        vm_char *pMetricName = m_pComparators[i].first->GetMetricName();
        m_pComparators[i].first->GetMaxResult(metricVal);
        vm_string_printf(VM_STRING("\n----BEST: %s%s Y = %.3f U = %.3f V = %.3f\n"), pMetricName, pMetricNumber, metricVal[0], metricVal[1], metricVal[2]);
        m_pComparators[i].first->GetMinResult(metricVal);
        vm_string_printf(VM_STRING("---WORST: %s%s Y = %.3f U = %.3f V = %.3f\n"), pMetricName, pMetricNumber, metricVal[0], metricVal[1], metricVal[2]);
        m_pComparators[i].first->GetOveralResult(metricVal, m_refsurface.Info.FourCC);
        vm_string_printf(VM_STRING("---TOTAL: %s%s Y = %.3f U = %.3f V = %.3f\n"), pMetricName, pMetricNumber, metricVal[0], metricVal[1], metricVal[2]);
        for (int j = 0; j < 3; j++){
            vm_string_printf(VM_STRING("<avg_metric=%c-%s%s> %.5f</avg_metric>\n"), planes[j], pMetricName, pMetricNumber, metricVal[j]);
        }

        m_pComparators[i].first->GetAverageResult(metricVal);
        for (int j = 0; j < 3; j++){
            vm_string_printf(VM_STRING("<avg_metric=%c-A%s%s> %.5f</avg_metric>\n"), planes[j], pMetricName, pMetricNumber, metricVal[j]);
        }

        //printing to file if specified
        vm_file * fMetrics = NULL;
        tstring targetFile = m_bIsViewRender ? 
            FileNameWithId(m_pMetricsOutFileName.c_str(), m_nViewId)
            : m_pMetricsOutFileName.c_str();
        if (NULL != (fMetrics = vm_file_fopen(targetFile.c_str(), VM_STRING("r+"))))
        {
            vm_file_fseek(fMetrics, -2, VM_FILE_SEEK_END);
            vm_string_fprintf(fMetrics, VM_STRING(", %.4lf\n"), (4. * metricVal[0] + metricVal[1] + metricVal[2]) / 6.);
            vm_file_fclose(fMetrics);
        }
    }

    for (size_t i = 0; i < m_pComparators.size(); i++)
    {
        delete m_pComparators[i].first;
    }
    m_pComparators.clear();
    m_metrics_input.clear();
}

mfxStatus MFXMetricComparatorRender::Init(mfxVideoParam *pInit, const vm_char *pFilename)
{
    MFX_CHECK_STS(MFXFileWriteRender::Init(pInit, pFilename));
    
     //yuv file generating enabled according to requirements bug: VCSD100006854
    /*if (NULL != m_fDest)
    {
        fclose(m_fDest);
        m_fDest = NULL;
    }*/

    return MFX_ERR_NONE;
}
mfxStatus MFXMetricComparatorRender::SetOutputPerfFile(const vm_char * pOutFile)
{
    if (m_bIsViewRender)
    {
        m_bDelaySetOutputPerfFile = true;
    }
    
    if (pOutFile != NULL && !m_bFirstFrame)
    {
        m_pMetricsOutFileName = pOutFile;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXMetricComparatorRender::Close()
{
    MFX_DELETE_ARRAY(m_refsurface.Data.Y);
    MFX_DELETE_ARRAY(m_pRefArray);
    m_bAllocated = false;
    
    return MFXFileWriteRender::Close();
}

mfxStatus MFXMetricComparatorRender::SetSurfacesSource(const vm_char      *pFile,
                                                       mfxFrameSurface1 *pRefSurface,
                                                       bool              bVerbose,
                                                       mfxU32            nLimitFrames)
{
    //if (m_bIsViewRender)
    {
        m_bDelaySetSurface = true;
    }

    m_bVerbose = bVerbose;

    if (!m_bFirstFrame)
    {
        if (NULL != pFile)
        {
            m_bFileSourceMode = true;
            m_nFrameToProcess = nLimitFrames;
            m_SurfacesSourceFileName = pFile;
        }else
        {
            m_pRefsurface = pRefSurface;
        }
    }
    
    if (m_bFirstFrame || !m_bDelaySetSurface)//either before render frame, or in 
    {
        if (NULL != pFile)
        {
            mfxU32 file_attr;
            MFX_CHECK_LASTERR(vm_file_getinfo(pFile, &m_nInFileLen, &file_attr));
            
            m_reader->Close();
            MFX_CHECK_STS(m_reader->Init(pFile));
        }else
        {
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXMetricComparatorRender::ReallocArray(mfxU32 nSize)
{
    if (NULL == m_pRefArray || m_nRefArray < nSize)
    {
        m_nRefArray = (nSize / 1024 + 100) * 1024;
        m_pRefArray = new mfxU8[m_nRefArray];
        MFX_CHECK_WITH_ERR(NULL != m_pRefArray, MFX_ERR_MEMORY_ALLOC);
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXMetricComparatorRender::PutData(mfxU8* pData, mfxU32 nSize)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_metricType & METRIC_SSIM)
    {
        //this metric couldnot be calculated using part of picture
    }
    if (m_metricType & METRIC_PSNR)
    {
        //this metric couldnot be calculated using part of picture
    }

    //mpeg2 suitable delta comparison
    //TODO:hope delta will not be used for encode->decode comparison
    if (m_metricType & METRIC_DELTA)
    {
        if (!m_bFileSourceMode)
        {
            return MFX_ERR_UNSUPPORTED;
        }
        MFX_CHECK_STS(ReallocArray(nSize));
        MFX_CHECK_STS(m_reader->LoadBlock(m_pRefArray, nSize, 1, nSize));
        for (mfxU32 i = 0; i < nSize; i++)
        {
            bool bFailed ;
            if (m_uiDeltaVal == 0)
                bFailed = pData[i] != m_pRefArray[i];
            else
                bFailed  = abs(pData[i] - m_pRefArray[i]) > m_uiDeltaVal;

            if (bFailed)
            {
                sts = MFX_ERR_UNKNOWN;
                m_Current.m_pixX  += i;
                m_Current.m_valRef = m_pRefArray[i];
                m_Current.m_valTst = pData[i];

                ReportDifference(VM_STRING("DELTA"));
                break;
            }
        }
    }

    //bit exactly cmp
    //TODO:hope bitexact will not be used for encode->decode comparison
    if (m_metricType & METRIC_CRC)
    {
        if (!m_bFileSourceMode)
        {
            return MFX_ERR_UNSUPPORTED;
        }
        MFX_CHECK_STS(ReallocArray(nSize));
        MFX_CHECK_STS(m_reader->LoadBlock(m_pRefArray, nSize, 1, nSize));
        if (0 != memcmp(m_pRefArray, pData, nSize))
        {
            sts = MFX_ERR_UNKNOWN;
            for (mfxU32 i = 0; i < nSize; i++)
            {
                if (pData[i] != m_pRefArray[i])
                {
                    m_Current.m_pixX  += i;
                    m_Current.m_valRef = m_pRefArray[i];
                    m_Current.m_valTst = pData[i];

                    ReportDifference(VM_STRING("BIT to BIT"));
                    break;
                }
            }
        }
    }
    
    MFX_CHECK_STS(sts);

    //yuv file generating enabled according to requirements bug: VCSD100006854
    return MFXFileWriteRender::PutData(pData, nSize);
}

mfxStatus MFXMetricComparatorRender::RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl)
{
    if (NULL != surface && !m_bFirstFrame)
    {
        m_bFirstFrame = true;
        //need to specify output perf file
        if (m_bDelaySetOutputPerfFile)
        {
            MFX_CHECK_STS(SetOutputPerfFile(m_pMetricsOutFileName.c_str()));
        }
        if (m_bDelaySetSurface)
        {
            tstring fileName = m_bIsViewRender ? FileNameWithId(m_SurfacesSourceFileName.c_str(), surface->Info.FrameId.ViewId) : m_SurfacesSourceFileName;
            MFX_CHECK_STS(SetSurfacesSource(
                fileName.c_str(), NULL, m_bVerbose, m_nFrameToProcess));
        }
    }

    MFX_CHECK_STS(MFXFileWriteRender::RenderFrame(surface, pCtrl));
    mfxStatus res = MFX_ERR_NONE;
    
    if (NULL == surface)
    {
        if (m_nNewFileOffset != m_nInFileLen && ( 0 == m_nFrameToProcess || m_nFrames != m_nFrameToProcess))
        {
            vm_string_printf(VM_STRING("FAILED: Wrong number of decoded frames\n"));
            for(size_t i = 0; i < m_pComparators.size() && res == MFX_ERR_NONE; i++)
            {
                m_pComparators[i].first->Reset();
            }
            res = PIPELINE_ERR_FILES_SIZES_MISMATCH;
        }
        MFX_CHECK_STS(res);
        return MFX_ERR_NONE;
    }

    bool skipChroma = !m_params.alwaysWriteChroma && surface->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV400;

    mfxU64 nFrameSize = 0;
    switch (m_nFourCC)
    {
        case MFX_FOURCC_NV12 :
        case MFX_FOURCC_YV12 :
            nFrameSize = (mfxU64)(surface->Info.CropW * surface->Info.CropH);
            if (!skipChroma)
                nFrameSize += (mfxU64)(((surface->Info.CropW + 1) >> 1) * ((surface->Info.CropH + 1) >> 1)) * 2;
            break;
        case MFX_FOURCC_NV16 :
        case MFX_FOURCC_YV16 :
            nFrameSize = (mfxU64)(surface->Info.CropW * surface->Info.CropH);
            if (!skipChroma)
                nFrameSize += (mfxU64)(((surface->Info.CropW + 1) >> 1) * surface->Info.CropH) * 2;
            break;
        case MFX_FOURCC_RGB3 :
            nFrameSize = (mfxU64)(surface->Info.CropW * surface->Info.CropH) * 3 ;
            break;
        case MFX_FOURCC_RGB4 :
            nFrameSize = (mfxU64)(surface->Info.CropW * surface->Info.CropH) * 4 ;
            break;
#if defined(_WIN32) || defined(_WIN64)
        case DXGI_FORMAT_AYUV :
#endif
        case MFX_FOURCC_YUV444_8:
            nFrameSize = (mfxU64)(surface->Info.CropW * surface->Info.CropH) * 3;
            break;
        case MFX_FOURCC_AYUV :
            nFrameSize = (mfxU64)(surface->Info.CropW * surface->Info.CropH) * 4 ;
            break;
        case MFX_FOURCC_YUY2 :
            nFrameSize = (mfxU64)(surface->Info.CropW * surface->Info.CropH) * 2 ;
            break;
        case MFX_FOURCC_P010 :
        case MFX_FOURCC_YUV420_16 :
            nFrameSize = (mfxU64)(surface->Info.CropW * surface->Info.CropH) * 2;
            if (!skipChroma)
                nFrameSize += (mfxU64)(((surface->Info.CropW + 1) >> 1) * ((surface->Info.CropH + 1) >> 1)) * 4;
            break;
        case MFX_FOURCC_P210 :
        case MFX_FOURCC_YUV422_16 :
            nFrameSize = (mfxU64)(surface->Info.CropW * surface->Info.CropH) * 2;
            if (!skipChroma)
                nFrameSize += (mfxU64)(((surface->Info.CropW + 1) >> 1) * surface->Info.CropH) * 4;
            break;
        case MFX_FOURCC_YUV444_16 :
            nFrameSize = (mfxU64)(surface->Info.CropW * surface->Info.CropH) * 2;
            if (!skipChroma)
                nFrameSize += (mfxU64)(surface->Info.CropW * surface->Info.CropH) * 4;
            break;
        default:
            vm_char ar[] = {*(0 + (char*)&surface->Info.FourCC),
                          *(1 + (char*)&surface->Info.FourCC),
                          *(2 + (char*)&surface->Info.FourCC),
                          *(3 + (char*)&surface->Info.FourCC),
                          0};

            MFX_TRACE_AND_EXIT(MFX_ERR_UNSUPPORTED, (VM_STRING("ERROR: unsupported surface->Info.FourCC = %s\n"), ar));
    }

    m_nNewFileOffset = m_nOldFileOffset + nFrameSize;
    m_nOldFileOffset = m_nNewFileOffset;

    if (METRIC_DELTA == m_metricType || METRIC_CRC == m_metricType)
    {
        return MFX_ERR_NONE;
    }

    MFX_CHECK(!m_pComparators.empty());
    MFX_CHECK_STS(LockFrame(surface));
    MFX_CHECK_STS(LockFrame(m_pRefsurface));

    //loading next block

    for(;;)
    {
        mfxFrameSurface1 tmpSurface = m_refsurface;

        if(m_bFileSourceMode)
        {
            if (!m_bAllocated)
            {
                if (MFX_ERR_NONE != (res = AllocSurface(&surface->Info, &m_refsurface)))
                {
                    break;
                }
                m_bAllocated = true;
            }            
            //if surface info changed during runtime, lets stick with it updated crops
            MFX_CHECK(m_pRefsurface->Info.Width  >= surface->Info.CropW + surface->Info.CropX);
            MFX_CHECK(m_pRefsurface->Info.Height >= surface->Info.CropH + surface->Info.CropX);
            //load surface with update info
            tmpSurface.Data = m_refsurface.Data;
            tmpSurface.Info = surface->Info;

            if (MFX_ERR_NONE != (res = m_reader->LoadNextFrame(&tmpSurface.Data, &tmpSurface.Info)))
            {
                //cleaning UP collected mectircs results
                for(size_t i = 0; i < m_pComparators.size(); i++)
                {
                    m_pComparators[i].first->Reset();
                }
                break;
            }
        }
        
        for(size_t i = 0; i < m_pComparators.size() && res == MFX_ERR_NONE; i++)
        {
            double metricVal[3] = {0.};

            res = m_pComparators[i].first->Compare(surface, &tmpSurface);

            if (MFX_ERR_NONE == res)
            {
                m_pComparators[i].first->GetLastCmpResult(metricVal);
            }

            if (m_bVerbose)
            {
                vm_string_printf(VM_STRING("Frame%2d  %s Y = %.3f U = %.3f V = %.3f\n"), m_nFrames, m_pComparators[i].first->GetMetricName(), metricVal[0], metricVal[1], metricVal[2]);
            }

            if (MFX_ERR_NONE == res && m_pComparators[i].second != -1.0)
            {
                vm_char planes[] = { VM_STRING("YUV") };

                for (int c = 0; c < 3; c++)
                {
                    if (metricVal[c] < m_pComparators[i].second)
                    {
                        m_Current.m_valRef = m_pComparators[i].second;
                        m_Current.m_valTst = metricVal[c];
                        m_Current.m_pixX   = (mfxU32)-1;
                        m_Current.m_pixY   = (mfxU32)-1;
                        m_Current.m_comp   = planes[c];
                        ReportDifference(m_pComparators[i].first->GetMetricName());
                        res = PIPELINE_ERR_THRESHOLD;
                        break;
                    }
                }
            } else
            {
                if (res == MFX_ERR_UNSUPPORTED)
                {
                    res = MFX_ERR_NONE;
                }
            }
        }
        break;
    }
    UnlockFrame(m_pRefsurface);
    UnlockFrame(surface);
    MFX_CHECK_STS(res);

    return res;
}
mfxStatus MFXMetricComparatorRender::AddMetric(MetricType type, mfxF64 fVal)
{
    std::auto_ptr<IMFXSurfacesComparator>  pCmp;

    mfxStatus sts = ComparatorFactory::Instance().CreateComparatorByType(type, pCmp);
    if (sts != MFX_ERR_NONE)
    {
        MFX_CHECK_STS_SKIP(sts, MFX_ERR_UNSUPPORTED);
        switch (type)
        {
            case METRIC_DELTA:
            {
                m_uiDeltaVal = (mfxU8)fVal;
                break;
            }
            
            case METRIC_CRC:
                break;
            
            default :
                return MFX_ERR_UNSUPPORTED;
        }
    }else
    {
        m_pComparators.push_back(std::pair<IMFXSurfacesComparator*, mfxF64>(pCmp.release(), fVal));
    }

    m_metrics_input.push_back(std::pair<MetricType, mfxF64>(type, fVal));
    
    m_metricType |= type;
    
    return MFX_ERR_NONE;
}


void MFXMetricComparatorRender::ReportDifference(const vm_char * metricName)
{
    mfxU32 const nMetricNumberSize = 4;
    vm_char  pMetricNumber[nMetricNumberSize + 1] = VM_STRING("");
    if (m_bIsViewRender)
    {
        vm_string_snprintf(pMetricNumber, nMetricNumberSize, VM_STRING("_%d"), m_nViewId);
    }

    vm_string_printf(VM_STRING("\nFAILED: %s%s comparison at\n"), metricName, pMetricNumber);
    vm_string_printf(VM_STRING("    frame  : %d\n"), m_nFrames);
    vm_string_printf(VM_STRING("    comp   : %c\n"), m_Current.m_comp);
    if (static_cast<int>(m_Current.m_pixX) != -1 && static_cast<int>(m_Current.m_pixY) != -1)
    {
        vm_string_printf(VM_STRING("    x, y   : %d, %d\n"), m_Current.m_pixX, m_Current.m_pixY);
    }
    //vm_string_printf(VM_STRING("    mb     : %d, %d\n"), report.m_mbX, report.m_mbY);
    vm_string_printf(VM_STRING("    refVal : %.2lf\n"), m_Current.m_valRef);
    vm_string_printf(VM_STRING("    tstVal : %.2lf\n"), m_Current.m_valTst);
}

// Special "format" used in vpxdec, 6 LSBs == 31
void XOR31(mfxU16 *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        data[i] = data[i] | 31;
}

template<typename T_In, typename T_Out>
void copyPlane(mfxU8 * inparam, size_t pitchIn, mfxU8 * outparam, size_t pitchOut, mfxU32 height, mfxU32 width, int shift)
{
    T_In * in = (T_In *)inparam;
    T_Out * out = (T_Out *)outparam;

    pitchIn >>= sizeof(T_In) == 2 ? 1 : 0;  // from byte pitch to sample
    pitchOut >>= sizeof(T_Out) == 2 ? 1 : 0;

    for (mfxU16 i = 0; i < height; i++)
    {
        if (shift || sizeof(T_In) != sizeof(T_Out))
        {
            if (shift < 0)
            {
                for (mfxU16 j = 0; j < width; j++)
                {
                    out[j] = in[j] << -shift;
                }
            }
            else
            {
                for (mfxU16 j = 0; j < width; j++)
                {
                    out[j] = in[j] >> shift;
                }
            }
        }
        else
            memcpy(out, in, sizeof(T_Out)*width);

        out += pitchOut;
        in += pitchIn;
    }
}

template<typename T_In, typename T_Out>
void copyChromaPlane(mfxU8 * inparam, size_t pitchIn, mfxU8 * outUparam, mfxU8 * outVparam, size_t pitchOut, mfxU32 height, mfxU32 width, int shift)
{
    T_In * in = (T_In *)inparam;
    T_Out * outU = (T_Out *)outUparam;
    T_Out * outV = (T_Out *)outVparam;

    pitchIn >>= sizeof(T_In) == 2 ? 1 : 0; // from byte pitch to sample
    pitchOut >>= sizeof(T_Out) == 2 ? 1 : 0;

    for (mfxU16 i = 0; i < height; i++)
    {
        if (shift || sizeof(T_In) != sizeof(T_Out))
        {
            if (shift < 0)
            {
                for (mfxU16 j = 0; j < width; j++)
                {
                    outU[j] = in[2*j] << -shift;
                    outV[j] = in[2*j + 1] << -shift;
                }
            }
            else
            {
                for (mfxU16 j = 0; j < width; j++)
                {
                    outU[j] = in[2*j] >> shift;
                    outV[j] = in[2*j + 1] >> shift;
                }
            }
        }
        else
        {
            for (mfxU16 j = 0; j < width; j++)
            {
                outU[j] = in[2*j];
                outV[j] = in[2*j + 1];
            }
        }

        outU += pitchOut;
        outV += pitchOut;
        in += pitchIn;
    }
}

mfxFrameSurface1* ConvertSurface(mfxFrameSurface1* pSurfaceIn, mfxFrameSurface1* pSurfaceOut, FileWriterRenderInputParams * params)
{
    //nothing to convert
    if (!pSurfaceIn || !pSurfaceOut || !params)
        return 0;
    
    mfxU32 pitchIn = pSurfaceIn->Data.PitchLow + ((mfxU32)pSurfaceIn->Data.PitchHigh << 16);
    mfxU32 pitchOut = pSurfaceOut->Data.PitchLow + ((mfxU32)pSurfaceOut->Data.PitchHigh << 16);
    bool useSameBitDepthForComponent = params->useSameBitDepthForComponents || params->useForceDecodeDumpFmt;

    if (pSurfaceIn->Info.FourCC == MFX_FOURCC_P010)
    {
        pitchIn >>= 1;
        pitchOut >>= 1;
    }

    // fill absent field
    if ((pSurfaceIn->Data.Corrupted & MFX_CORRUPTION_ABSENT_TOP_FIELD) || (pSurfaceIn->Data.Corrupted & MFX_CORRUPTION_ABSENT_BOTTOM_FIELD))
    {
        IppiSize srcSize = 
        {
            pSurfaceIn->Info.Width, 
            pSurfaceIn->Info.Height >> 1
        };

        mfxU32 isBottom = (pSurfaceIn->Data.Corrupted & MFX_CORRUPTION_ABSENT_BOTTOM_FIELD) ? 1 : 0;

        if (pSurfaceIn->Info.FourCC == MFX_FOURCC_P010)
        {
            mfxU16 defaultValue = 1 << (pSurfaceIn->Info.BitDepthLuma - 1);
            ippiSet_16u_C1R(defaultValue, pSurfaceIn->Data.Y16 + isBottom*pitchIn, (pitchIn << 1), srcSize);
            srcSize.height >>= 1;
            defaultValue = 1 << (pSurfaceIn->Info.BitDepthChroma - 1);
            ippiSet_16u_C1R(defaultValue, pSurfaceIn->Data.U16 + isBottom*pitchIn, (pitchIn << 1), srcSize);
        }
        else
        {
            ippiSet_8u_C1R(128, pSurfaceIn->Data.Y + isBottom*pitchIn, (pitchIn << 1), srcSize);

            if (pSurfaceIn->Info.FourCC == MFX_FOURCC_NV12)
            {
                srcSize.height >>= 1;
                ippiSet_8u_C1R(128, pSurfaceIn->Data.U + isBottom*pitchIn, (pitchIn << 1), srcSize);
            }
            else
            {
                srcSize.width >>= 1;
                srcSize.height >>= 1;

                ippiSet_8u_C1R(128, pSurfaceIn->Data.U + isBottom*(pitchIn >> 1), pitchIn, srcSize);
                ippiSet_8u_C1R(128, pSurfaceIn->Data.V + isBottom*(pitchIn >> 1), pitchIn, srcSize);
            }
        }
    }

    // fill chroma values for monochrome surface
    if (pSurfaceIn->Info.ChromaFormat == MFX_CHROMAFORMAT_YUV400)
    {
        IppiSize srcSize = {pSurfaceIn->Info.Width, pSurfaceIn->Info.Height};

        srcSize.height >>= 1;
        if (pSurfaceIn->Info.FourCC == MFX_FOURCC_P010)
        {
            mfxU16 defaultValue = 1 << (pSurfaceIn->Info.BitDepthChroma - 1);
            ippiSet_16u_C1R(defaultValue, pSurfaceIn->Data.U16, pitchIn, srcSize);
        }
        else
        {
            if (pSurfaceIn->Info.FourCC == MFX_FOURCC_NV12)
            {
                ippiSet_8u_C1R(128, pSurfaceIn->Data.UV, pitchIn, srcSize);
            }
            else
            {
                srcSize.width >>= 1;

                ippiSet_8u_C1R(128, pSurfaceIn->Data.U, pitchIn, srcSize);
                ippiSet_8u_C1R(128, pSurfaceIn->Data.V, pitchIn, srcSize);
            }
        }
    }

    // color conversion
    if (pSurfaceIn->Info.FourCC != MFX_FOURCC_NV12 &&
        pSurfaceIn->Info.FourCC != MFX_FOURCC_P010 &&
        pSurfaceIn->Info.FourCC != MFX_FOURCC_NV16 &&
        pSurfaceIn->Info.FourCC != MFX_FOURCC_P210 &&
        pSurfaceIn->Info.FourCC != MFX_FOURCC_Y210 &&
        pSurfaceIn->Info.FourCC != MFX_FOURCC_Y410 &&
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        pSurfaceIn->Info.FourCC != MFX_FOURCC_P016 &&
        pSurfaceIn->Info.FourCC != MFX_FOURCC_Y216 &&
        pSurfaceIn->Info.FourCC != MFX_FOURCC_Y416 &&
#endif
        pSurfaceIn->Info.FourCC != MFX_FOURCC_AYUV &&
        pSurfaceIn->Info.FourCC != MFX_FOURCC_YUY2)
    {
        return pSurfaceIn;
    }

    if (pSurfaceOut->Info.FourCC != MFX_FOURCC_YV12      &&
        pSurfaceOut->Info.FourCC != MFX_FOURCC_YV16      &&
        pSurfaceOut->Info.FourCC != MFX_FOURCC_YUV444_8  &&
        pSurfaceOut->Info.FourCC != MFX_FOURCC_YUV420_16 &&
        pSurfaceOut->Info.FourCC != MFX_FOURCC_YUV422_16 &&
        pSurfaceOut->Info.FourCC != MFX_FOURCC_YUV444_16)
    {
        return pSurfaceIn;
    }

    pSurfaceOut->Data.FrameOrder = pSurfaceIn->Data.FrameOrder;
    pSurfaceOut->Data.TimeStamp  = pSurfaceIn->Data.TimeStamp;
    pSurfaceOut->Data.Corrupted  = pSurfaceIn->Data.Corrupted;

    pSurfaceOut->Info.FrameRateExtD = pSurfaceIn->Info.FrameRateExtD;
    pSurfaceOut->Info.FrameRateExtN = pSurfaceIn->Info.FrameRateExtN;

    pSurfaceOut->Info.FrameId = pSurfaceIn->Info.FrameId;

    pSurfaceOut->Info.CropH = pSurfaceIn->Info.CropH;
    pSurfaceOut->Info.CropW = pSurfaceIn->Info.CropW;
    pSurfaceOut->Info.CropX = pSurfaceIn->Info.CropX;
    pSurfaceOut->Info.CropY = pSurfaceIn->Info.CropY;

    pSurfaceOut->Info.AspectRatioH = pSurfaceIn->Info.AspectRatioH;
    pSurfaceOut->Info.AspectRatioW = pSurfaceIn->Info.AspectRatioW;

    pSurfaceOut->Info.PicStruct = pSurfaceIn->Info.PicStruct;
    pSurfaceOut->Info.ChromaFormat = pSurfaceIn->Info.ChromaFormat;

    Ipp8u *(pDst[3]) = 
    {
        pSurfaceOut->Data.Y,
        pSurfaceOut->Data.U,
        pSurfaceOut->Data.V
    };

    Ipp32s pDstStep[3] =
    {
        static_cast<Ipp32s>(pitchOut),
        static_cast<Ipp32s>(pitchOut >> 1),
        static_cast<Ipp32s>(pitchOut >> 1)
    };

    mfxU32 const originalBDLuma = pSurfaceIn->Info.BitDepthLuma ? pSurfaceIn->Info.BitDepthLuma : 8;
    mfxU32 const originalBDChroma = pSurfaceIn->Info.BitDepthChroma ? pSurfaceIn->Info.BitDepthChroma : 8;
    mfxU32 finalBitDepth = params->use10bitOutput ? 10 : IPP_MAX(originalBDLuma, originalBDChroma);

    // align luma and chroma bitdepth to 10/12 bit
    if (finalBitDepth > 8 && finalBitDepth <= 10)
    {
        finalBitDepth = 10;
    }
    else if (finalBitDepth > 10 && finalBitDepth <= 12)
    {
        finalBitDepth = 12;
    }

    //set dumpFileFmt
    if(params->useForceDecodeDumpFmt)
    {
        finalBitDepth = params->info.BitDepthLuma;
    }

    mfxU32 const finalBitDepthLuma =
        useSameBitDepthForComponent ? finalBitDepth : originalBDLuma;
    mfxU32 const finalBitDepthChroma =
        useSameBitDepthForComponent ? finalBitDepth : originalBDChroma;

    mfxI32 const l_shift = params->VpxDec16bFormat ? 0 : (pSurfaceIn->Info.Shift ? (16 - finalBitDepth) : originalBDLuma   - finalBitDepthLuma);
    mfxI32 const c_shift = params->VpxDec16bFormat ? 0 : (pSurfaceIn->Info.Shift ? (16 - finalBitDepth) : originalBDChroma - finalBitDepthChroma);

    if (pSurfaceOut->Info.FourCC == MFX_FOURCC_YUV420_16)
    {
        if (pSurfaceIn->Info.FourCC == MFX_FOURCC_NV12)
            copyPlane<mfxU8, mfxU16>(pSurfaceIn->Data.Y, pSurfaceIn->Data.Pitch, pSurfaceOut->Data.Y, pSurfaceOut->Data.Pitch, pSurfaceIn->Info.Height, pSurfaceIn->Info.Width, l_shift);
        else
            copyPlane<mfxU16, mfxU16>(pSurfaceIn->Data.Y, pSurfaceIn->Data.Pitch, pSurfaceOut->Data.Y, pSurfaceOut->Data.Pitch, pSurfaceIn->Info.Height, pSurfaceIn->Info.Width, l_shift);

        if (pSurfaceIn->Info.FourCC == MFX_FOURCC_NV12)
            copyChromaPlane<mfxU8, mfxU16>(pSurfaceIn->Data.UV, pSurfaceIn->Data.Pitch, pSurfaceOut->Data.U, pSurfaceOut->Data.V, pSurfaceOut->Data.Pitch >> 1, pSurfaceIn->Info.Height / 2, pSurfaceIn->Info.Width / 2, c_shift);
        else
            copyChromaPlane<mfxU16, mfxU16>(pSurfaceIn->Data.UV, pSurfaceIn->Data.Pitch, pSurfaceOut->Data.U, pSurfaceOut->Data.V, pSurfaceOut->Data.Pitch >> 1, pSurfaceIn->Info.Height / 2, pSurfaceIn->Info.Width / 2, c_shift);

        return pSurfaceOut;
    }
    else if (pSurfaceOut->Info.FourCC == MFX_FOURCC_YUV422_16)
    {
        if (   pSurfaceIn->Info.FourCC == MFX_FOURCC_Y210
#if (MFX_VERSION >= MFX_VERSION_NEXT)
            || pSurfaceIn->Info.FourCC == MFX_FOURCC_Y216
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
           )
        {
#pragma pack(push, 1)
            struct Y216Pixel
            {
                mfxU16 y0, u, y1, v;
            };
#pragma pack(pop)

            mfxU16* pDstY = pSurfaceOut->Data.Y16;
            mfxU16* pDstU = pSurfaceOut->Data.U16;
            mfxU16* pDstV = pSurfaceOut->Data.V16;

            pitchOut /= 2;
            pitchIn /= 2;

            for (size_t i = 0; i < pSurfaceIn->Info.Height; i++)
            {
                Y216Pixel const* pSrc
                    = (Y216Pixel*)(pSurfaceIn->Data.Y16 + pitchIn * i);

                for (size_t j = 0; j < pSurfaceIn->Info.Width; j += 2)
                {
                    pDstY[i * pitchOut + j + 0] = pSrc->y0 >> l_shift;
                    pDstY[i * pitchOut + j + 1] = pSrc->y1 >> l_shift;

                    pDstU[i * pitchOut / 2 + j / 2] = pSrc->u >> c_shift;
                    pDstV[i * pitchOut / 2 + j / 2] = pSrc->v >> c_shift;

                    ++pSrc;
                }
            }
        }
        else if(pSurfaceIn->Info.FourCC == MFX_FOURCC_YUY2)
        {
            #pragma pack(push, 1)
                struct YUY2Pixel
                {
                    mfxU8 y0, u, y1, v;
                };
            #pragma pack(pop)

            mfxU16* pDstY = pSurfaceOut->Data.Y16;
            mfxU16* pDstU = pSurfaceOut->Data.U16;
            mfxU16* pDstV = pSurfaceOut->Data.V16;

            pitchOut /= sizeof(mfxU16);

            for (size_t i = 0; i < pSurfaceIn->Info.Height; i++)
            {
                YUY2Pixel const* pSrc
                    = reinterpret_cast<YUY2Pixel const*>(pSurfaceIn->Data.Y + pitchIn * i);

                for (size_t j = 0; j < pSurfaceIn->Info.Width; j += 2)
                {
                    pDstY[i * pitchOut + j + 0] = pSrc->y0 << (finalBitDepth - 8);
                    pDstY[i * pitchOut + j + 1] = pSrc->y1 << (finalBitDepth - 8);

                    pDstU[i * pitchOut / 2 + j / 2] = pSrc->u << (finalBitDepth - 8);
                    pDstV[i * pitchOut / 2 + j / 2] = pSrc->v << (finalBitDepth - 8);

                    ++pSrc;
                }
            }
        }
        else
        {
            copyPlane<mfxU16, mfxU16>(pSurfaceIn->Data.Y, pSurfaceIn->Data.Pitch, pSurfaceOut->Data.Y, pSurfaceOut->Data.Pitch, pSurfaceIn->Info.Height, pSurfaceIn->Info.Width, l_shift);
            copyChromaPlane<mfxU16, mfxU16>(pSurfaceIn->Data.UV, pSurfaceIn->Data.Pitch, pSurfaceOut->Data.U, pSurfaceOut->Data.V, pSurfaceOut->Data.Pitch >> 1, pSurfaceIn->Info.Height, pSurfaceIn->Info.Width / 2, c_shift);
        }

        return pSurfaceOut;
    }
    else if (pSurfaceOut->Info.FourCC == MFX_FOURCC_YUV444_16)
    {
        if (pSurfaceIn->Info.FourCC == MFX_FOURCC_Y410)
        {
            //full unpack Y410 to YUV444_10b not using alpha
            mfxU16* pDstY = pSurfaceOut->Data.Y16;
            mfxU16* pDstU = pSurfaceOut->Data.U16;
            mfxU16* pDstV = pSurfaceOut->Data.V16;

            mfxY410 const* pSrc = pSurfaceIn->Data.Y410;

            pitchOut /= sizeof(mfxU16);
            pitchIn  /= sizeof(mfxY410);

            for (size_t i = 0; i < pSurfaceIn->Info.Height; i++)
            {
                for (size_t j = 0; j < pSurfaceIn->Info.Width; j++)
                {
                    pDstY[i*pitchOut + j] = pSrc[i*pitchIn + j].Y << (finalBitDepth - 10);
                    pDstU[i*pitchOut + j] = pSrc[i*pitchIn + j].U << (finalBitDepth - 10);
                    pDstV[i*pitchOut + j] = pSrc[i*pitchIn + j].V << (finalBitDepth - 10);
                }
            }
        }else if(pSurfaceIn->Info.FourCC == MFX_FOURCC_AYUV)
        {
            mfxI32 l_shift_AYUV = finalBitDepth - 8;
            mfxI32 c_shift_AYUV = finalBitDepth - 8;

            //full unpack AYUV to YUV444_10b not using alpha
            mfxU16* pDstY = pSurfaceOut->Data.Y16;
            mfxU16* pDstU = pSurfaceOut->Data.U16;
            mfxU16* pDstV = pSurfaceOut->Data.V16;

            pitchOut /= sizeof(mfxU16);

            for (size_t i = 0; i < pSurfaceIn->Info.Height; i++)
            {
                for (size_t j = 0; j < pSurfaceIn->Info.Width; j++)
                {
                    pDstY[i*pitchOut + j] = pSurfaceIn->Data.Y[i * pitchIn + 4 * j] << l_shift_AYUV;
                    pDstU[i*pitchOut + j] = pSurfaceIn->Data.U[i * pitchIn + 4 * j] << l_shift_AYUV;
                    pDstV[i*pitchOut + j] = pSurfaceIn->Data.V[i * pitchIn + 4 * j] << l_shift_AYUV;
                }
            }
        }
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        else if (pSurfaceIn->Info.FourCC == MFX_FOURCC_Y416)
        {
            mfxU16* pDstY = pSurfaceOut->Data.Y16;
            mfxU16* pDstU = pSurfaceOut->Data.U16;
            mfxU16* pDstV = pSurfaceOut->Data.V16;

            pitchOut /= sizeof(mfxU16);
            pitchIn  /= sizeof(mfxU16);

            for (size_t i = 0; i < pSurfaceIn->Info.Height; i++)
            {
                for (size_t j = 0; j < pSurfaceIn->Info.Width; j++)
                {
                    pDstU[i * pitchOut + j] = pSurfaceIn->Data.U16[i * pitchIn + 4 * j] >> l_shift;
                    pDstY[i * pitchOut + j] = pSurfaceIn->Data.Y16[i * pitchIn + 4 * j] >> c_shift;
                    pDstV[i * pitchOut + j] = pSurfaceIn->Data.V16[i * pitchIn + 4 * j] >> c_shift;
                }
            }
        }
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
    }
    else if (pSurfaceOut->Info.FourCC == MFX_FOURCC_YV12)
    {
        VM_ASSERT(pSurfaceIn->Info.FourCC == MFX_FOURCC_NV12);
        const Ipp8u *(pSrc[2]) = {pSurfaceIn->Data.Y, pSurfaceIn->Data.U };
        Ipp32s pSrcStep[2] = {static_cast<Ipp32s>(pitchIn), static_cast<Ipp32s>(pitchIn)};
        IppiSize srcSize = { pSurfaceIn->Info.Width, pSurfaceIn->Info.Height};
        IppStatus sts = ippiYCbCr420_8u_P2P3R(pSrc[0], pSrcStep[0], pSrc[1], pSrcStep[1], pDst, pDstStep, srcSize);

        if (sts != ippStsNoErr)
        {
            return 0;
        }
    }
    else if (pSurfaceOut->Info.FourCC == MFX_FOURCC_YV16)
    {
        if (pSurfaceIn->Info.FourCC != MFX_FOURCC_YUY2)
        {
            VM_ASSERT(pSurfaceIn->Info.FourCC == MFX_FOURCC_NV16);

            copyPlane<mfxU8, mfxU8>(pSurfaceIn->Data.Y, pSurfaceIn->Data.Pitch, pSurfaceOut->Data.Y, pSurfaceOut->Data.Pitch, pSurfaceIn->Info.Height, pSurfaceIn->Info.Width, 0);
            copyChromaPlane<mfxU8, mfxU8>(pSurfaceIn->Data.UV, pSurfaceIn->Data.Pitch, pSurfaceOut->Data.U, pSurfaceOut->Data.V, pSurfaceOut->Data.Pitch >> 1, pSurfaceIn->Info.Height, pSurfaceIn->Info.Width / 2, 0);
        }
        else
        {
#pragma pack(push, 1)
            struct YUY2Pixel
            {
                mfxU8 y0, u, y1, v;
            };
#pragma pack(pop)

            mfxU8* pDstY = pSurfaceOut->Data.Y;
            mfxU8* pDstU = pSurfaceOut->Data.U;
            mfxU8* pDstV = pSurfaceOut->Data.V;

            for (size_t i = 0; i < pSurfaceIn->Info.Height; i++)
            {
                YUY2Pixel const* pSrc
                    = reinterpret_cast<YUY2Pixel const*>(pSurfaceIn->Data.Y + pitchIn * i);

                for (size_t j = 0; j < pSurfaceIn->Info.Width; j += 2)
                {
                    pDstY[i * pitchOut + j + 0] = pSrc->y0;
                    pDstY[i * pitchOut + j + 1] = pSrc->y1;

                    pDstU[i * pitchOut / 2 + j / 2] = pSrc->u;
                    pDstV[i * pitchOut / 2 + j / 2] = pSrc->v;

                    ++pSrc;
                }
            }
        }
    }
    else if (pSurfaceOut->Info.FourCC == MFX_FOURCC_YUV444_8)
    {
        VM_ASSERT(pSurfaceIn->Info.FourCC == MFX_FOURCC_AYUV || pSurfaceIn->Info.FourCC == MFX_FOURCC_Y410);

        switch (pSurfaceIn->Info.FourCC)
        {
            case MFX_FOURCC_Y410:
                {
                    mfxY410 const* pSrc = pSurfaceIn->Data.Y410;
                    pitchIn  /= sizeof(mfxY410);
                    for (size_t i = 0; i < pSurfaceIn->Info.Height; i++)
                    {
                        for (size_t j = 0; j < pSurfaceIn->Info.Width; j++)
                        {
                            pDst[0][i*pitchOut + j] = pSrc[i*pitchIn + j].Y;
                            pDst[1][i*pitchOut + j] = pSrc[i*pitchIn + j].U;
                            pDst[2][i*pitchOut + j] = pSrc[i*pitchIn + j].V;
                        }
                    }
                }
                break;
            case MFX_FOURCC_AYUV:
                for (size_t i = 0; i < pSurfaceIn->Info.Height; i++)
                {
                    for (size_t j = 0; j < pSurfaceIn->Info.Width; j++)
                    {
                        pDst[0][i*pitchOut + j] = pSurfaceIn->Data.Y[i*pitchIn + 4*j];
                        pDst[1][i*pitchOut + j] = pSurfaceIn->Data.U[i*pitchIn + 4*j];
                        pDst[2][i*pitchOut + j] = pSurfaceIn->Data.V[i*pitchIn + 4*j];
                    }
                }
                break;
        default:
            return 0;
        }
    }
    else // IMC3
    {
        const Ipp8u *(pSrc[3]) = { pSurfaceIn->Data.Y, pSurfaceIn->Data.U, pSurfaceIn->Data.V, };

        Ipp32s pSrcStep[3] = { static_cast<Ipp32s>(pitchIn), static_cast<Ipp32s>(pitchIn), static_cast<Ipp32s>(pitchIn) };

        IppiSize srcSize[3] = 
        {
            {pSurfaceIn->Info.Width, pSurfaceIn->Info.Height}, {pSurfaceIn->Info.Width >> 1, pSurfaceIn->Info.Height >> 1},
            {pSurfaceIn->Info.Width >> 1, pSurfaceIn->Info.Height >> 1}
        };

        for (Ipp32s c = 0; c < 3; c++)
        {
            IppStatus sts = ippiCopy_8u_C1R(
                pSrc[c], pSrcStep[c],
                pDst[c], pDstStep[c],
                srcSize[c]);

            if (sts != ippStsNoErr)
            {
                return 0;
            }
        }
    }

    return pSurfaceOut;
}

mfxStatus       AllocSurface(const mfxFrameInfo *pTargetInfo, mfxFrameSurface1* pSurfaceOut)
{
    if (!pTargetInfo->FourCC)
        return MFX_ERR_NONE;

    mfxU32 frameSize;

    MFX_CHECK_STS(InitMfxFrameSurface(pSurfaceOut, pTargetInfo, &frameSize, false));
    
    pSurfaceOut->Data.PitchLow = pTargetInfo->Width;

    mfxU8 halfHeight = pTargetInfo->ChromaFormat != MFX_CHROMAFORMAT_YUV422 &&
                       pTargetInfo->ChromaFormat != MFX_CHROMAFORMAT_YUV444
                            ? 1 : 0;
    mfxU32 bufSize = 0;
    if(pTargetInfo->ChromaFormat == MFX_CHROMAFORMAT_YUV444)
        bufSize = pSurfaceOut->Info.Height * pSurfaceOut->Data.PitchLow * 3;
    else
        bufSize = halfHeight ? (pSurfaceOut->Info.Height * pSurfaceOut->Data.PitchLow * 3 / 2) : (pSurfaceOut->Info.Height * pSurfaceOut->Data.PitchLow * 2);

    if (pSurfaceOut->Info.FourCC != MFX_FOURCC_P010      &&
        pSurfaceOut->Info.FourCC != MFX_FOURCC_P210      &&
        pSurfaceOut->Info.FourCC != MFX_FOURCC_YUV420_16 &&
        pSurfaceOut->Info.FourCC != MFX_FOURCC_YUV422_16 &&
        pSurfaceOut->Info.FourCC != MFX_FOURCC_YUV444_16
        && pSurfaceOut->Info.FourCC != MFX_FOURCC_Y410
    )
    {
        mfxU8 * pBuffer = new mfxU8[bufSize];
        MFX_CHECK_WITH_ERR(NULL != pBuffer, MFX_ERR_MEMORY_ALLOC);

        pSurfaceOut->Data.Y = pBuffer;

        switch (pSurfaceOut->Info.FourCC)
        {
            case MFX_FOURCC_NV12: 
            case MFX_FOURCC_NV16:
            {
                pSurfaceOut->Data.U = pBuffer + pSurfaceOut->Data.PitchLow * pSurfaceOut->Info.Height;
                pSurfaceOut->Data.V = pSurfaceOut->Data.U + 1;
                break;
            }
            case MFX_FOURCC_YV12: 
            case MFX_FOURCC_YV16: 
            {
                pSurfaceOut->Data.U = pBuffer + pSurfaceOut->Data.PitchLow * pSurfaceOut->Info.Height;
                pSurfaceOut->Data.V = pSurfaceOut->Data.U + ((pSurfaceOut->Data.PitchLow * pSurfaceOut->Info.Height) >> (1 + halfHeight));
                break;
            }
            case MFX_FOURCC_YUV444_8: 
            {
                pSurfaceOut->Data.U = pBuffer + pSurfaceOut->Data.PitchLow * pSurfaceOut->Info.Height;
                pSurfaceOut->Data.V = pSurfaceOut->Data.U + pSurfaceOut->Data.PitchLow * pSurfaceOut->Info.Height;
                break;
            }
        }
    }
    else
    {
        mfxU16 * pBuffer = new mfxU16[bufSize];
        pSurfaceOut->Data.Y16 = pBuffer;

        switch (pSurfaceOut->Info.FourCC)
        {
            case MFX_FOURCC_P210:
            case MFX_FOURCC_P010:
                pSurfaceOut->Data.U16 = pBuffer + pSurfaceOut->Data.PitchLow * pSurfaceOut->Info.Height;
                pSurfaceOut->Data.V = (mfxU8*)(pSurfaceOut->Data.U16 + 1);
                break;
            case MFX_FOURCC_YUV422_16:
            case MFX_FOURCC_YUV420_16:
                pSurfaceOut->Data.U16 = pBuffer + pSurfaceOut->Data.PitchLow * pSurfaceOut->Info.Height;
                pSurfaceOut->Data.V = (mfxU8*)(pSurfaceOut->Data.U16 + ((pSurfaceOut->Data.PitchLow * pSurfaceOut->Info.Height) >> (1 + halfHeight)));
                break;
            case MFX_FOURCC_YUV444_16:
                pSurfaceOut->Data.U16 = pBuffer + pSurfaceOut->Data.PitchLow * pSurfaceOut->Info.Height;
                pSurfaceOut->Data.V16 = pSurfaceOut->Data.U16 + pSurfaceOut->Data.PitchLow * pSurfaceOut->Info.Height;
                break;
        }
        pSurfaceOut->Data.PitchLow <<= 1;
    }
    
    return MFX_ERR_NONE;
}
