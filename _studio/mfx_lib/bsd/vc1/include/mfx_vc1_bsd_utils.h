/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2008 Intel Corporation. All Rights Reserved.
//          MFX VC-1 BSD utils
//
*/
#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_BSD)

#include "mfx_vc1_dec_common.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_job.h"

#ifndef _MFX_VC1_BSD_UTILS_H_
#define _MFX_VC1_BSD_UTILS_H_

namespace MfxVC1BSDPacking
{
    mfxStatus FillParamsForOneMB(VC1Context*     pContext,
                                 VC1MB*          pCurMB,
                                 VC1SingletonMB* pSingleMB,
                                 mfxMbCode*      pMbCode);

    mfxU8  ConvertMBTypeTo5bitMXF(VC1Context* pContext,
                                  VC1MB*     pCurMB);

    void  PackCodedBlockPattern(VC1MB*     pCurMB, mfxMbCode*      pMbCode);
}


// Slice Parameters description
class MfxVC1ThreadUnitParamsBSD
{
public:
    MfxVC1ThreadUnitParams   BaseSlice;
    Ipp32u*                  pBS;
    Ipp32s                   BitOffset;
    VC1PictureLayerHeader*   pPicLayerHeader;
    VC1VLCTables*            pVlcTbl;
};


#endif
#endif

