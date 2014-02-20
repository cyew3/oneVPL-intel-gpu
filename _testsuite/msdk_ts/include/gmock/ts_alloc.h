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
    std::vector<mfxFrameSurface1> m_pool;

    void Close();

public:
    tsSurfacePool(frame_allocator* allocator = 0);
    ~tsSurfacePool();

    inline frame_allocator* GetAllocator() { return m_allocator; }
    inline mfxU32           PoolSize()     { return (mfxU32)m_pool.size(); }

    void SetAllocator       (frame_allocator* allocator, bool external);
    void UseDefaultAllocator(bool isSW = false);

    mfxStatus AllocSurfaces (mfxFrameAllocRequest request, bool direct_pointers = true);
    mfxStatus LockSurface   (mfxFrameSurface1& s);
    mfxStatus UnlockSurface (mfxFrameSurface1& s);

    mfxFrameSurface1* GetSurface();

};