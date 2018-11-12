// Copyright (c) 2015-2018 Intel Corporation
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

#if !defined(__MFX_SCREEN_CAPTURE_DIRTY_RECT_H__)
#define __MFX_SCREEN_CAPTURE_DIRTY_RECT_H__

#include <list>
#include "mfxstructures.h"
#include "mfxplugin.h"
#include "mfxsc.h"
#include "umc_mutex.h"
#include "mfx_screen_capture_ddi.h"

namespace MfxCapture
{

class CpuDirtyRectFilter : public DirtyRectFilter
{
public:
    CpuDirtyRectFilter(const mfxCoreInterface* _core);
    virtual ~CpuDirtyRectFilter();

    mfxStatus Init(const mfxVideoParam* par, bool isSysMem, bool isOpaque = false);
    mfxStatus Init(const mfxFrameInfo& in, const mfxFrameInfo& out);
    mfxStatus GetParam(mfxFrameInfo& in, mfxFrameInfo& out);
    mfxStatus RunFrameVPP(mfxFrameSurface1& in, mfxFrameSurface1& out);

protected:
    const mfxCoreInterface* m_pmfxCore;

    UMC::Mutex m_guard;
    std::list<mfxFrameSurface1> m_InSurfPool;
    std::list<mfxFrameSurface1> m_OutSurfPool;

    mfxFrameSurface1* AllocSurfs(const mfxVideoParam* par);
    void FreeSurfs();
    mfxFrameSurface1* GetFreeSurf(std::list<mfxFrameSurface1>& lSurfs);
    mfxFrameSurface1* GetSysSurface(const mfxFrameSurface1* surf, bool& release, bool& unlock, std::list<mfxFrameSurface1>& lSurfs);
    void ReleaseSurf(mfxFrameSurface1*& surf);

    bool m_bSysMem;
    bool m_bOpaqMem;

private:
    //prohobit copy constructor
    CpuDirtyRectFilter(const CpuDirtyRectFilter& that);
    //prohibit assignment operator
    CpuDirtyRectFilter& operator=(const CpuDirtyRectFilter&);
};

template<typename T>
T* GetExtendedBuffer(const mfxU32& BufferId, const mfxFrameSurface1* surf);

} //namespace MfxCapture
#endif //#if !defined(__MFX_SCREEN_CAPTURE_DIRTY_RECT_H__)