/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2015 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_dx11.h

\* ****************************************************************************** */
#pragma once

#include "mfx_camera_plugin_utils.h"

#define MFX_VA_WIN
#define MFX_ENABLE_VPP
#define MFX_D3D11_ENABLED

#include "d3d11_video_processor.h"

#define MFX_FOURCC_R16_BGGR MAKEFOURCC('I','R','W','0')
#define MFX_FOURCC_R16_RGGB MAKEFOURCC('I','R','W','1')
#define MFX_FOURCC_R16_GRBG MAKEFOURCC('I','R','W','2')
#define MFX_FOURCC_R16_GBRG MAKEFOURCC('I','R','W','3')

using namespace MfxCameraPlugin;
using namespace MfxHwVideoProcessing;


class D3D11FrameAllocResponse : public mfxFrameAllocResponse
{
public:
    D3D11FrameAllocResponse():
        m_numFrameActualReturnedByAllocFrames(0)
    {
        Zero((mfxFrameAllocResponse &)*this);
    }

    mfxStatus AllocateSurfaces(VideoCORE *core, mfxFrameAllocRequest & req)
    {
        if (! core)
            return MFX_ERR_MEMORY_ALLOC;

        req.NumFrameSuggested = req.NumFrameMin;
        mfxFrameAllocResponse response;
        m_mids.resize(req.NumFrameMin, 0);
        m_locked.resize(req.NumFrameMin, 0);
        core->AllocFrames(&req, &response, true);
        for (int i = 0; i < req.NumFrameMin; i++)
            m_mids[i] = response.mids[i];

        NumFrameActual = req.NumFrameMin;
        mids = &m_mids[0];

        return MFX_ERR_NONE;
    }

    mfxU32 Lock(mfxU32 idx)
    {
    if (idx >= m_locked.size())
        return 0;
    assert(m_locked[idx] < 0xffffffff);
    return ++m_locked[idx];
    }

    void Unlock()
    {
        std::fill(m_locked.begin(), m_locked.end(), 0);
    }

    mfxU32 Unlock(mfxU32 idx)
    {
        if (idx >= m_locked.size())
        return mfxU32(-1);
        assert(m_locked[idx] > 0);
        return --m_locked[idx];
    }

    mfxU32 Locked(mfxU32 idx) const
    {
        return (idx < m_locked.size()) ? m_locked[idx] : 1;
    }

    void Free(VideoCORE *core)
    {
        if (core)
        {
            if (MFX_HW_D3D11  == core->GetVAType())
            {
                for (size_t i = 0; i < m_responseQueue.size(); i++)
                    core->FreeFrames(&m_responseQueue[i]);
            }
            else
            {
                if (mids)
                {
                    NumFrameActual = m_numFrameActualReturnedByAllocFrames;
                    core->FreeFrames(this);
                }
            }
        }
    }

private:

    mfxU16      m_numFrameActualReturnedByAllocFrames;
    std::vector<mfxFrameAllocResponse> m_responseQueue;
    std::vector<mfxMemId>              m_mids;
    std::vector<mfxU32>                m_locked;

    mfxU16 m_width;
    mfxU16 m_height;
    mfxU32 m_fourCC;
};

class D3D11CameraProcessor: public CameraProcessor
{
public:
    D3D11CameraProcessor()
    {
        m_ddi.reset(0);
        m_executeParams = 0;
        m_executeSurf   = 0;
        m_last_index    = -1;
        m_paddedInput   = false;
        m_InSurfacePool  = new D3D11FrameAllocResponse();
        m_OutSurfacePool = new D3D11FrameAllocResponse();
    };

    ~D3D11CameraProcessor() {
        Close();
    };

    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        UNREFERENCED_PARAMETER(in);
        UNREFERENCED_PARAMETER(out);
        return MFX_ERR_NONE;
    }

    virtual mfxStatus Init(CameraParams *CameraParams);

    virtual mfxStatus Init(mfxVideoParam *par)
    {
        par;
        return MFX_ERR_NONE;
    }

    virtual mfxStatus Reset(mfxVideoParam *par, CameraParams * FrameParams)
    {
        par; FrameParams;
        return MFX_ERR_NONE;
    }

    virtual mfxStatus Close()
    {
        m_InSurfacePool->Free(m_core);
        m_OutSurfacePool->Free(m_core);
        if (m_executeParams )
            delete [] m_executeParams;
        if (m_executeSurf)
            delete [] m_executeSurf;

        m_executeParams = 0;
        m_executeSurf   = 0;
        return MFX_ERR_NONE;
    }

    virtual mfxStatus AsyncRoutine(AsyncParams *pParam);
    virtual mfxStatus CompleteRoutine(AsyncParams * pParam);

protected:
    mfxStatus CheckIOPattern(mfxU16  IOPattern);
    mfxStatus PreWorkOutSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex, MfxHwVideoProcessing::mfxExecuteParams *params);
    mfxStatus PreWorkInSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex, mfxDrvSurface *DrvSurf);
    mfxU32    BayerToFourCC(mfxU32);

private:

    // Do not allow copying of the object
    D3D11CameraProcessor(const D3D11CameraProcessor& from) {UNREFERENCED_PARAMETER(from);};
    D3D11CameraProcessor& operator=(const D3D11CameraProcessor&) {return *this; };
    mfxU32 m_last_index;
    mfxU32 FindFreeResourceIndex(
    D3D11FrameAllocResponse const & pool,
    mfxU32                        *index)
    {
        *index = NO_INDEX;

        for (mfxU32 i = m_last_index + 1; i < pool.NumFrameActual; i++)
            if (pool.Locked(i) == 0)
            {
                *index = i;
                m_last_index = i;
                return i;
            }

        if ( m_last_index == -1)
        {
            m_last_index = 0;
        }

        for (mfxU32 i = 0; i <= m_last_index; i++)
            if (pool.Locked(i) == 0)
            {
                *index = i;
                m_last_index = i;
                return i;
            }
        
        return NO_INDEX;
    }

    mfxMemId AcquireResource(
        D3D11FrameAllocResponse & pool,
        mfxU32                  index)
    {
        if (index > pool.NumFrameActual)
            return 0; /// MID_INVALID; ???
        pool.Lock(index);
        return pool.mids[index];
    }

    mfxMemId AcquireResource(
        D3D11FrameAllocResponse & pool,
        mfxU32 *index)
    {
        return AcquireResource(pool, FindFreeResourceIndex(pool, index));
    }


    std::auto_ptr<D3D11VideoProcessor>               m_ddi;
    MfxHwVideoProcessing::mfxExecuteParams          *m_executeParams;
    MfxHwVideoProcessing::mfxDrvSurface             *m_executeSurf;
    mfxU32                                           m_counter;
    D3D11FrameAllocResponse                          *m_InSurfacePool;
    D3D11FrameAllocResponse                          *m_OutSurfacePool;
    bool                                             m_systemMemOut;
    bool                                             m_paddedInput;
    CameraParams                                     m_CameraParams;
    UMC::Mutex                                       m_guard;
    UMC::Mutex                                       m_guard_exec;
    mfxU16                                           m_AsyncDepth;
    mfxU16                                           m_width;
    mfxU16                                           m_height;
};
