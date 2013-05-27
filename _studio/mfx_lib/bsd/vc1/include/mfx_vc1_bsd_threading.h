/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2008 Intel Corporation. All Rights Reserved.
//          MFX VC-1 DEC threading support
//
*/
#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_BSD)

#include "mfx_vc1_bsd_utils.h"
#include "umc_vc1_dec_job.h"
#include "mfx_vc1_dec_common.h"

#ifndef _MFX_VC1_BSD_THREADING_H_
#define _MFX_VC1_BSD_THREADING_H_

//IRun BSD functionality
class VC1BSDRunBSD : public VC1TaskMfxBase
{
public:
    VC1BSDRunBSD() {};
    virtual ~VC1BSDRunBSD(){};
    virtual   mfxStatus ProcessJob();
    mfxStatus InitSpecific(MfxVC1ThreadUnitParamsBSD* pInitParams);
    void      SetDependenceParams(VC1BSDRunBSD* pPrev);

private:

    Ipp32u*                  m_pBS;
    Ipp32s                   m_bitOffset;
    IppiEscInfo_VC1          m_EscInfo;
    VC1PictureLayerHeader*   m_picLayerHeader;
    VC1VLCTables*            m_vlcTbl;

    Ipp16s*                  m_pResDBuffer;
};
// Queue of Run BSD tasks
class VC1MfxTQueueBsd : public VC1MfxTQueueBase
{
public:
    virtual         mfxStatus Init();
    virtual         VC1TaskMfxBase* GetNextStaticTask(Ipp32s threadID);
    virtual         VC1TaskMfxBase* GetNextDynamicTask(Ipp32s threadID);
    virtual         bool            IsFuncComplte();
    mfxStatus       FormalizeSliceTaskGroup(MfxVC1ThreadUnitParamsBSD* pSliceParams);
private:
    VC1BSDRunBSD  m_pRunBsdQueue[512]; // Allocate memory for max number of tasks
};

#endif
#endif
