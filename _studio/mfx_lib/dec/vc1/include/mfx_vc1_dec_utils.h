/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2011 Intel Corporation. All Rights Reserved.
//          MFX VC-1 DEC utils
//
*/
#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_DEC)

#include "mfx_vc1_dec.h"
#include "umc_vc1_common_defs.h"

#ifndef _MFX_VC1_DEC_UTILS_H_
#define _MFX_VC1_DEC_UTILS_H_


namespace MfxVC1DECUnpacking
{
     void      TranslateMfxToUmcIQT(mfxMbCode*      pMbCode,
                                    VC1MB*          pCurMB,
                                    VC1DCMBParam*   pCurrDC);

     void      BiasDetermination(VC1Context* pContext);


     void      TranslateMfxToUmcPredDec(mfxMbCode*      pMbCode,
                                      VC1MB*          pCurMB);

     mfxStatus RunMbGroupGetRecon(Ipp16u                   MBStartRow,
                                  Ipp16u                   MBEndRow,
                                  VC1Context*              pContext,
                                  mfxFrameCUC*             pCUC);

     mfxStatus RunMbGroupPredILDB(Ipp16u                   MBStartRow,
                                  Ipp16u                   MBEndRow,
                                  VC1Context*              pContext,
                                  mfxFrameCUC*             pCUC);

    inline void SetMBPlanesInternal(Ipp8u* pY,
                                    Ipp8u* pU,
                                    Ipp8u* pV,
                                    Ipp32u pitch,
                                    VC1MB* pCurMB,
                                    Ipp32s mbXPos,
                                    Ipp32s mbYPos)
    {
        pCurMB->currYPitch = pitch;
        pCurMB->currYPlane = pY + pitch * (mbYPos << 4) + (mbXPos << 4);

        //U
        pCurMB->currUPitch = pitch/2;
        pCurMB->currUPlane = pU + pCurMB->currUPitch*(mbYPos << 3) + (mbXPos << 3);

        //V
        pCurMB->currVPitch = pitch/2;
        pCurMB->currVPlane =  pV + pCurMB->currUPitch*(mbYPos << 3) + (mbXPos << 3);
    }


     // recon functionality. TBD
     void GetReconPMbNonInterlace(VC1Context*     pContext,
                                  VC1MB*          pCurMB,
                                  Ipp16s*         pResid);

     void GetReconPMbInterlace(VC1Context*     pContext,
                               VC1MB*          pCurMB,
                               Ipp16s*         pResid);

     void  UnPackCodedBlockPattern(VC1MB*          pCurMB, mfxMbCode*      pMbCode);

     // we sure about our types
     template <class T>
     T bit_get(T value, Ipp32u offset, Ipp32u count)
     {
         return (T)( (value & (((1<<count)-1) << offset)) >> offset);
     };

     Ipp32u CalculateSkipDirectFlag(mfxU8 mbType, mfxU8 Skip8x8Flag);

};

#endif
#endif

