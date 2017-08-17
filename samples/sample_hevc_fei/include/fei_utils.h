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

#ifndef __FEI_UTILS_H__
#define __FEI_UTILS_H__

#include "sample_defs.h"
#include "sample_utils.h"
#include "base_allocator.h"
#include "sample_hevc_fei_defs.h"

class SurfacesPool
{
public:
    SurfacesPool(MFXFrameAllocator* allocator = NULL);
    ~SurfacesPool();

    mfxStatus AllocSurfaces(mfxFrameAllocRequest& request);
    void SetAllocator(MFXFrameAllocator* allocator);
    mfxFrameSurface1* GetFreeSurface();
    mfxStatus LockSurface(mfxFrameSurface1* pSurf);
    mfxStatus UnlockSurface(mfxFrameSurface1* pSurf);

private:
    MFXFrameAllocator* m_pAllocator;
    std::vector<mfxFrameSurface1> m_pool;
    mfxFrameAllocResponse m_response;

private:
    DISALLOW_COPY_AND_ASSIGN(SurfacesPool);
};

/**********************************************************************************/

class IYUVSource
{
public:
    IYUVSource(const SourceFrameInfo& inPars, SurfacesPool* sp)
        : m_inPars(inPars)
        , m_pOutSurfPool(sp)
    {
        MSDK_ZERO_MEMORY(m_frameInfo);
    }

    virtual ~IYUVSource() {}

    virtual mfxStatus QueryIOSurf(mfxFrameAllocRequest* request) = 0;
    virtual mfxStatus PreInit() = 0;
    virtual mfxStatus Init()    = 0;
    virtual void      Close()   = 0;

    virtual mfxStatus GetActualFrameInfo(mfxFrameInfo & info) = 0;
    virtual mfxStatus GetFrame(mfxFrameSurface1* & pSurf) = 0;

protected:
    SourceFrameInfo m_inPars;
    mfxFrameInfo    m_frameInfo;
    SurfacesPool*   m_pOutSurfPool;

private:
    DISALLOW_COPY_AND_ASSIGN(IYUVSource);
};

// reader of raw frames
class YUVReader : public IYUVSource
{
public:
    YUVReader(const SourceFrameInfo& inPars, SurfacesPool* sp)
        : IYUVSource(inPars, sp)
        , m_srcColorFormat(inPars.ColorFormat)
    {
    }

    virtual ~YUVReader();

    virtual mfxStatus QueryIOSurf(mfxFrameAllocRequest* request);
    virtual mfxStatus PreInit();
    virtual mfxStatus Init();
    virtual void      Close();

    virtual mfxStatus GetActualFrameInfo(mfxFrameInfo & info);
    virtual mfxStatus GetFrame(mfxFrameSurface1* & pSurf);

protected:
    mfxStatus FillInputFrameInfo(mfxFrameInfo& fi);

protected:
    CSmplYUVReader   m_FileReader;
    mfxU32           m_srcColorFormat;

private:
    DISALLOW_COPY_AND_ASSIGN(YUVReader);
};

class Decoder : public IYUVSource
{
public:
    Decoder(const SourceFrameInfo& inPars, SurfacesPool* sp, MFXVideoSession* session)
        : IYUVSource(inPars, sp)
        , m_session(session)
    {
        MSDK_ZERO_MEMORY(m_Bitstream);
        m_Bitstream.TimeStamp=(mfxU64)-1;
    }

    virtual ~Decoder()
    {
        WipeMfxBitstream(&m_Bitstream);
    }

    virtual mfxStatus QueryIOSurf(mfxFrameAllocRequest* request);
    virtual mfxStatus PreInit();
    virtual mfxStatus Init();
    virtual void      Close();

    virtual mfxStatus GetActualFrameInfo(mfxFrameInfo & info);
    virtual mfxStatus GetFrame(mfxFrameSurface1* & pSurf);

protected:
    mfxStatus InitDecParams(MfxVideoParamsWrapper & par);

protected:
    CSmplBitstreamReader           m_FileReader;
    mfxBitstream                   m_Bitstream;

    MFXVideoSession*               m_session;
    std::auto_ptr<MFXVideoDECODE>  m_DEC;
    MfxVideoParamsWrapper          m_par;

    mfxSyncPoint                   m_LastSyncp;

private:
    DISALLOW_COPY_AND_ASSIGN(Decoder);
};

/**********************************************************************************/

class FileHandler
{
public:
    FileHandler(const msdk_char* _filename, const msdk_char* _mode);
    ~FileHandler();

    template <typename T>
    mfxStatus Read(T* ptr, size_t size, size_t count)
    {
        if (m_file && (fread(ptr, size, count, m_file) != count))
            return MFX_ERR_DEVICE_FAILED;

        return MFX_ERR_NONE;
    }

    template <typename T>
    mfxStatus Write(T* ptr, size_t size, size_t count)
    {
        if (m_file && (fwrite(ptr, size, count, m_file) != count))
            return MFX_ERR_DEVICE_FAILED;

        return MFX_ERR_NONE;
    }

private:
    FILE* m_file;

private:
    DISALLOW_COPY_AND_ASSIGN(FileHandler);
};

/**********************************************************************************/

template<typename T>
T* AcquireResource(std::vector<T> & pool)
{
    T * freeBuffer = NULL;
    for (size_t i = 0; i < pool.size(); i++)
    {
        if (pool[i].m_locked == 0)
        {
            freeBuffer = &pool[i];
            msdk_atomic_inc16((volatile mfxU16*)&pool[i].m_locked);
            break;
        }
    }
    return freeBuffer;
}

class MFX_VPP
{
public:
    MFX_VPP(MFXVideoSession* session, MFXFrameAllocator* allocator, MfxVideoParamsWrapper& vpp_pars);
    ~MFX_VPP();

    mfxStatus Init();
    mfxStatus Reset(mfxVideoParam& par);

    mfxStatus QueryIOSurf(mfxFrameAllocRequest* request);
    // component manages its output surface pool taking into account external request for surfaces
    // which can be passed from another component
    mfxStatus AllocOutFrames(mfxFrameAllocRequest* ext_request);

    mfxStatus PreInit();
    const mfxFrameInfo& GetOutFrameInfo();

    mfxStatus ProcessFrame(mfxFrameSurface1* pInSurf, mfxFrameSurface1* & pOutSurf);

private:
    mfxStatus Query();

    MFXVideoSession*    m_pmfxSession; // pointer to MFX session shared by external interface

    MFXVideoVPP             m_mfxVPP;
    SurfacesPool            m_outSurfPool;
    MfxVideoParamsWrapper   m_videoParams; // reflects current state VPP parameters

private:
    DISALLOW_COPY_AND_ASSIGN(MFX_VPP);
};

#endif // #define __FEI_UTILS_H__
