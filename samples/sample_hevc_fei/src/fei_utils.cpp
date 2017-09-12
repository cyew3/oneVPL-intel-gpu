/******************************************************************************\
Copyright (c) 2017, Intel Corporation
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

#include "fei_utils.h"

SurfacesPool::SurfacesPool(MFXFrameAllocator* allocator)
    : m_pAllocator(allocator)
{
    MSDK_ZERO_MEMORY(m_response);
}

SurfacesPool::~SurfacesPool()
{
    if (m_pAllocator)
    {
        m_pAllocator->Free(m_pAllocator->pthis, &m_response);
        m_pAllocator = NULL;
    }
}

void SurfacesPool::SetAllocator(MFXFrameAllocator* allocator)
{
    m_pAllocator = allocator;
}

mfxStatus SurfacesPool::AllocSurfaces(mfxFrameAllocRequest& request)
{
    MSDK_CHECK_POINTER(m_pAllocator, MFX_ERR_NULL_PTR);

    // alloc frames
    mfxStatus sts = m_pAllocator->Alloc(m_pAllocator->pthis, &request, &m_response);
    MSDK_CHECK_STATUS(sts, "m_pAllocator->Alloc failed");

    mfxFrameSurface1 surf;
    MSDK_ZERO_MEMORY(surf);

    MSDK_MEMCPY_VAR(surf.Info, &request.Info, sizeof(mfxFrameInfo));

    m_pool.reserve(m_response.NumFrameActual);
    for (mfxU16 i = 0; i < m_response.NumFrameActual; i++)
    {
        if (m_response.mids)
        {
            surf.Data.MemId = m_response.mids[i];
        }
        m_pool.push_back(surf);
    }

    return MFX_ERR_NONE;
}

mfxFrameSurface1* SurfacesPool::GetFreeSurface()
{
    if (m_pool.empty()) // seems AllocSurfaces wasn't called
        return NULL;

    mfxU16 idx = GetFreeSurfaceIndex(m_pool.data(), m_pool.size());

    return (idx != MSDK_INVALID_SURF_IDX) ? &m_pool[idx] : NULL;
}

mfxStatus SurfacesPool::LockSurface(mfxFrameSurface1* pSurf)
{
    mfxStatus sts = MFX_ERR_NOT_INITIALIZED;

    if (m_pAllocator)
    {
        sts = m_pAllocator->Lock(m_pAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
        MSDK_CHECK_STATUS(sts, "m_pAllocator->Lock failed");
    }

    return sts;
}

mfxStatus SurfacesPool::UnlockSurface(mfxFrameSurface1* pSurf)
{
    mfxStatus sts = MFX_ERR_NOT_INITIALIZED;

    if (m_pAllocator)
    {
        sts = m_pAllocator->Unlock(m_pAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
        MSDK_CHECK_STATUS(sts, "m_pAllocator->Unlock failed");
    }

    return sts;
}

/**********************************************************************************/

YUVReader::~YUVReader()
{
    Close();
}

void YUVReader::Close()
{
    m_FileReader.Close();
}

mfxStatus YUVReader::QueryIOSurf(mfxFrameAllocRequest* request)
{
    if (request)
    {
        MSDK_ZERO_MEMORY(*request);

        MSDK_MEMCPY_VAR(request->Info, &m_frameInfo, sizeof(mfxFrameInfo));
        request->NumFrameSuggested = request->NumFrameMin = 1;
    }

    return MFX_ERR_NONE;
}

// fill FrameInfo structure with user parameters taking into account that input stream sequence
// will be stored in MSDK surfaces, i.e. width/height should be aligned, FourCC within supported formats
mfxStatus YUVReader::FillInputFrameInfo(mfxFrameInfo& fi)
{
    MSDK_ZERO_MEMORY(fi);

    bool isProgressive = (MFX_PICSTRUCT_PROGRESSIVE == m_inPars.nPicStruct);
    fi.FourCC          = MFX_FOURCC_NV12;
    fi.ChromaFormat    = FourCCToChroma(fi.FourCC);
    fi.PicStruct       = m_inPars.nPicStruct;

    fi.CropX  = 0;
    fi.CropY  = 0;
    fi.CropW  = m_inPars.nWidth;
    fi.CropH  = m_inPars.nHeight;
    fi.Width  = MSDK_ALIGN16(fi.CropW);
    fi.Height = isProgressive ? MSDK_ALIGN16(fi.CropH) : MSDK_ALIGN32(fi.CropH);

    mfxStatus sts = ConvertFrameRate(m_inPars.dFrameRate, &fi.FrameRateExtN, &fi.FrameRateExtD);
    MSDK_CHECK_STATUS(sts, "ConvertFrameRate failed");

    return MFX_ERR_NONE;
}

mfxStatus YUVReader::PreInit()
{
    return FillInputFrameInfo(m_frameInfo);
}

mfxStatus YUVReader::GetActualFrameInfo(mfxFrameInfo & info)
{
    info = m_frameInfo;

    return MFX_ERR_NONE;
}

mfxStatus YUVReader::Init()
{
    std::list<msdk_string> in_file_names;
    in_file_names.push_back(msdk_string(m_inPars.strSrcFile));
    return m_FileReader.Init(in_file_names, m_srcColorFormat);
}

mfxStatus YUVReader::GetFrame(mfxFrameSurface1* & pSurf)
{
    mfxStatus sts = MFX_ERR_NONE;

    // point pSurf to surface from shared surface pool
    pSurf = m_pOutSurfPool->GetFreeSurface();
    MSDK_CHECK_POINTER(pSurf, MFX_ERR_MEMORY_ALLOC);

    // need to call Lock to access surface data and...
    sts = m_pOutSurfPool->LockSurface(pSurf);
    MSDK_CHECK_STATUS(sts, "LockSurface failed");

    // load frame from file to surface data
    sts = m_FileReader.LoadNextFrame(pSurf);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, sts);

    // ... after we're done call Unlock
    sts = m_pOutSurfPool->UnlockSurface(pSurf);
    MSDK_CHECK_STATUS(sts, "UnlockSurface failed");

    return sts;
}

/**********************************************************************************/

mfxStatus Decoder::QueryIOSurf(mfxFrameAllocRequest* request)
{
    return m_DEC->QueryIOSurf(&m_par, request);
}

mfxStatus Decoder::PreInit()
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_FileReader.Init(m_inPars.strSrcFile);
    MSDK_CHECK_STATUS(sts, "Can't open input file");

    sts = InitMfxBitstream(&m_Bitstream, 1024 * 1024);
    MSDK_CHECK_STATUS(sts, "InitMfxBitstream failed");

    m_DEC.reset(new MFXVideoDECODE(*m_session));

    m_par.AsyncDepth  = 1;
    m_par.mfx.CodecId = m_inPars.DecodeId;
    m_par.IOPattern   = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    sts = InitDecParams(m_par);
    MSDK_CHECK_STATUS(sts, "Can't initialize decoder params");

    return sts;
}

mfxStatus Decoder::GetActualFrameInfo(mfxFrameInfo & info)
{
    info = m_par.mfx.FrameInfo;

    return MFX_ERR_NONE;
}

mfxStatus Decoder::Init()
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_DEC->Init(&m_par);
    MSDK_CHECK_STATUS(sts, "Decoder initialization failed");

    return sts;
}

mfxStatus Decoder::GetFrame(mfxFrameSurface1* & outSrf)
{
    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    mfxFrameSurface1 * workSrf = NULL;
    mfxSyncPoint syncp = NULL;
    bool bEOS = false;

    while (MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE < sts)
    {
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            WaitForDeviceToBecomeFree(*m_session, m_LastSyncp, sts);
        }
        else if (MFX_ERR_MORE_DATA == sts)
        {
            sts = m_FileReader.ReadNextFrame(&m_Bitstream);
            if (MFX_ERR_MORE_DATA == sts)
            {
                bEOS = true;
                sts = MFX_ERR_NONE;
            }
            MSDK_BREAK_ON_ERROR(sts);
        }
        else if (MFX_ERR_MORE_SURFACE == sts)
        {
            workSrf = m_pOutSurfPool->GetFreeSurface();
            MSDK_CHECK_POINTER(workSrf, MFX_ERR_MEMORY_ALLOC);
        }

        sts = m_DEC->DecodeFrameAsync(bEOS ? NULL : &m_Bitstream, workSrf, &outSrf, &syncp);

        if (bEOS && MFX_ERR_MORE_DATA == sts)
            break;

        if (MFX_ERR_NONE == sts)
        {
            m_LastSyncp = syncp;
        }

        if (syncp && MFX_ERR_NONE < sts)
        {
            sts = MFX_ERR_NONE;
        }

    }
    if (MFX_ERR_NONE == sts && syncp)
    {
        sts = m_session->SyncOperation(syncp, MSDK_WAIT_INTERVAL);
        MSDK_CHECK_STATUS(sts, "Decoder SyncOperation failed");
    }

    return sts;
}

void Decoder::Close()
{
    return;
}

mfxStatus Decoder::InitDecParams(MfxVideoParamsWrapper & par)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_FileReader.ReadNextFrame(&m_Bitstream);
    if (MFX_ERR_MORE_DATA == sts)
        return sts;

    MSDK_CHECK_STATUS(sts, "ReadNextFrame failed");

    for (;;)
    {
        sts = m_DEC->DecodeHeader(&m_Bitstream, &par);

        if (MFX_ERR_MORE_DATA == sts)
        {
            if (m_Bitstream.MaxLength == m_Bitstream.DataLength)
            {
                sts = ExtendMfxBitstream(&m_Bitstream, m_Bitstream.MaxLength * 2);
                MSDK_CHECK_STATUS(sts, "ExtendMfxBitstream failed");
            }

            sts = m_FileReader.ReadNextFrame(&m_Bitstream);
            if (MFX_ERR_MORE_DATA == sts)
                return sts;
        }
        else
            break;
    }

    return sts;
}

/**********************************************************************************/

FileHandler::FileHandler(const msdk_char* _filename, const msdk_char* _mode)
    : m_file(NULL)
{
    MSDK_FOPEN(m_file, _filename, _mode);
    if (m_file == NULL)
    {
        msdk_printf(MSDK_STRING("Can't open file %s in mode %s\n"), _filename, _mode);
        throw mfxError(MFX_ERR_NOT_INITIALIZED, "Opening file failed");
    }
}

FileHandler::~FileHandler()
{
    if (m_file)
    {
        fclose(m_file);
    }
}
