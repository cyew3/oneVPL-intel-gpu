//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include <cstring>
#include "scd_tools.h"

namespace SCDTools
{

mfxStatus InternalSurfaces::Alloc(mfxFrameAllocator& allocator, mfxFrameAllocRequest& req)
{
    ULGuard guard(m_mtx);
    mfxStatus sts;

    Sync(guard, false);

    if (!m_free.empty() || !m_busy.empty())
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    sts = allocator.Alloc(allocator.pthis, &req, &m_response);
    if (sts)
        return sts;

    for (mfxU16 i = 0; i < m_response.NumFrameActual; i++)
        m_free.push_back(m_response.mids[i]);

    m_pAlloc = &allocator;
    m_info = req.Info;

    return sts;
}

void InternalSurfaces::Free()
{
    ULGuard guard(m_mtx);

    if (m_pAlloc && m_response.NumFrameActual)
    {
        Sync(guard, true);

        m_pAlloc->Free(m_pAlloc->pthis, &m_response);
        m_response.NumFrameActual = 0;
        m_pAlloc = 0;
        m_free.resize(0);
    }
}

bool InternalSurfaces::Pop(mfxFrameSurface1& surf)
{
    surf.Info = m_info;
    memset(&surf.Data, 0, sizeof(surf.Data));
    surf.Data.MemType = m_response.MemType;
    surf.Data.MemId = AsyncPool<mfxMemId>::Pop();

    if (surf.Data.MemId)
    {
        surf.Data.MemId = *(mfxMemId*)surf.Data.MemId;
        return true;
    }

    surf.Data.MemId = 0;

    return false;
}

bool InternalSurfaces::Push(mfxFrameSurface1& surf)
{
    if (surf.Data.Locked)
        return false;

    return AsyncPool<mfxMemId>::Push(surf.Data.MemId);
}

}
