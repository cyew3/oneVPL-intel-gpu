/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_dirty_rect.h

\* ****************************************************************************** */

#if !defined(__MFX_SCREEN_CAPTURE_DIRTY_RECT_H__)
#define __MFX_SCREEN_CAPTURE_DIRTY_RECT_H__

#include <list>
#include "mfxstructures.h"
#include "mfxplugin.h"
#include "mfxsc.h"
#include "umc_mutex.h"

namespace MfxCapture
{

class CpuDirtyRectFilter
{
public:
    CpuDirtyRectFilter(const mfxCoreInterface* _core);
    virtual ~CpuDirtyRectFilter();

    mfxStatus Init(const mfxVideoParam* par, bool isSysMem);
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
    void ReleaseSurf(mfxFrameSurface1& surf);

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