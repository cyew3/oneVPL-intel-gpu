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
