// Copyright (c) 2017-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
