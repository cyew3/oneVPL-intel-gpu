/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_dirty_rect.h

\* ****************************************************************************** */

#if !defined(__MFX_SCREEN_CAPTURE_CM_DIRTY_RECT_H__)
#define __MFX_SCREEN_CAPTURE_CM_DIRTY_RECT_H__

#include <list>
#include <vector>
#include "mfxstructures.h"
#include "mfxplugin.h"
#include "mfxsc.h"
#include "umc_mutex.h"

#include "mfx_screen_capture_ddi.h"

#include "cmrt_cross_platform.h"

namespace MfxCapture
{

struct TaskContext
{
    mfxU8 busy;
    CmThreadSpace* pThreadSpace;
    CmTask*        pTask;
    struct {
        CmSurface2DUP* cmSurf;
        mfxU8*         physBuffer;
        mfxU8*         map;
    } roiMap;
    CmSurface2D*  inSurf;
    CmSurface2D*  outSurf;
};

class CmDirtyRectFilter : public DirtyRectFilter
{
public:
    CmDirtyRectFilter(const mfxCoreInterface* _core);
    virtual ~CmDirtyRectFilter();

    mfxStatus Init(const mfxVideoParam* par, bool isSysMem);
    mfxStatus Init(const mfxFrameInfo& in, const mfxFrameInfo& out);
    mfxStatus GetParam(mfxFrameInfo& in, mfxFrameInfo& out);
    mfxStatus RunFrameVPP(mfxFrameSurface1& in, mfxFrameSurface1& out);

protected:
    const mfxCoreInterface* m_pmfxCore;


    CmDevice * m_pCMDevice;
    std::vector<CmProgram *> m_vPrograms;
    CmQueue *   m_pQueue;
    CmKernel*   m_pKernel;
    CmKernel*   m_pKernelNV12;
    CmKernel*   m_pKernelRGB4;

    mfxHandleType m_mfxDeviceType;
    mfxHDL        m_mfxDeviceHdl;

    UMC::Mutex m_guard;
    std::list<mfxFrameSurface1> m_InSurfPool;
    std::list<mfxFrameSurface1> m_OutSurfPool;

    mfxFrameSurface1* AllocSurfs(const mfxVideoParam* par);
    void FreeSurfs();
    mfxFrameSurface1* GetFreeSurf(std::list<mfxFrameSurface1>& lSurfs);
    void ReleaseSurf(mfxFrameSurface1& surf);

    mfxU16 m_width ;
    mfxU16 m_height;

private:
    //prohobit copy constructor
    CmDirtyRectFilter(const CmDirtyRectFilter& that);
    //prohibit assignment operator
    CmDirtyRectFilter& operator=(const CmDirtyRectFilter&);
};

template<typename T>
T* GetExtendedBuffer(const mfxU32& BufferId, const mfxFrameSurface1* surf);

} //namespace MfxCapture
#endif //#if !defined(__MFX_SCREEN_CAPTURE_CM_DIRTY_RECT_H__)