/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "ts_common.h"

class tsBufferAllocator : public buffer_allocator
{
public: 
    tsBufferAllocator();
    ~tsBufferAllocator();
};

class tsSurfacePool
{
private:
    frame_allocator* m_allocator;
    bool             m_external;
    bool             m_isd3d11;
    std::vector<mfxFrameSurface1> m_pool;
    std::vector<mfxFrameSurface1*> m_opaque_pool;
    mfxFrameAllocResponse m_response;

    void Close();

public:
    tsSurfacePool(frame_allocator* allocator = 0, bool d3d11 = !!((g_tsImpl) & 0xF00));
    ~tsSurfacePool();

    inline frame_allocator* GetAllocator() { return m_allocator; }
    inline mfxU32           PoolSize()     { return (mfxU32)m_pool.size(); }

    void SetAllocator       (frame_allocator* allocator, bool external);
    void UseDefaultAllocator(bool isSW = false);
    void AllocOpaque        (mfxFrameAllocRequest request, mfxExtOpaqueSurfaceAlloc& osa);

    mfxStatus AllocSurfaces (mfxFrameAllocRequest request, bool direct_pointers = true);
    mfxStatus FreeSurfaces  ();
    mfxStatus LockSurface   (mfxFrameSurface1& s);
    mfxStatus UnlockSurface (mfxFrameSurface1& s);

    mfxFrameSurface1* GetSurface();

    mfxFrameSurface1* GetSurface(mfxU32 ind);
};
