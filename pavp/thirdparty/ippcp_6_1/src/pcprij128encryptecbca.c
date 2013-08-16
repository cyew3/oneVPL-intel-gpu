/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2002-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Encrypt byte data stream according to Rijndael (ECB mode)
//
// Contents:
//    ippsRijndael128EncryptECB()
//
//
//    Created: Sat 25-May-2002 18:14
//  Author(s): Sergey Kirillov
//
//   Modified: Tue 31-Jan-2006 14:08
//  Author(s): Vasiliy Buzoverya
//             The code to provide conditional compilation depending on
//             OpenMP API support by a compiler has been added.
*/
#include "precomp.h"

#ifndef _OPENMP /* vb */

#include "owncp.h"
#include "pcprij.h"
#include "pcpciphertool.h"

#if defined(_IPP_TRS)
#  include "pcprijtables.h"
#endif


/*F*
//    Name: ippsRijndael128EncryptECB
//
// Purpose: Encrypt byte data stream according to Rijndael in EBC mode.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pCtx == NULL
//                            pSrc == NULL
//                            pDst == NULL
//    ippStsContextMatchErr   pCtx->idCtx != idCtxRijndael
//    ippStsLengthErr         len <1
//                            pCtx->nb mismatch function suffix 128
//    ippStsPaddingSchemeErr  padding != IppsCPPaddingPKCS7
//                            padding != IppsCPPaddingZEROS
//                            padding != IppsCPPaddingNONE
//    ippStsUnderRunErr       (padding==IppsCPPaddingNONE) && (len%MBS_RIJ128)
//    ippStsNoErr             no errors
//
// Parameters:
//    pSrc        pointer to the source data stream
//    pDst        pointer to the target data stream
//    len         plaintext stream length (bytes)
//    pCtx        RIJ context
//    padding     the padding scheme indicator
//
*F*/
#if 0
IPPFUN(IppStatus, ippsRijndael128EncryptECB,(const Ipp8u* pSrc, Ipp8u* pDst, int len,
                                             const IppsRijndael128Spec* pCtx,
                                             IppsCPPadding padding))
{
   Ipp32u tmpInp[NB(128)];
   Ipp32u tmpOut[NB(128)];

   /* test context */
   IPP_BAD_PTR1_RET(pCtx);
   /* use aligned Rijndael context */
   pCtx = (IppsRijndael128Spec*)( IPP_ALIGNED_PTR(pCtx, RIJ_ALIGNMENT) );

   IPP_BADARG_RET(!RIJ_ID_TEST(pCtx), ippStsContextMatchErr);
   /* test source and target pointers */
   IPP_BAD_PTR2_RET(pSrc, pDst);
   /* test stream length */
   IPP_BADARG_RET((len<1), ippStsLengthErr);
   /* test data block size */
   IPP_BADARG_RET(NB(128)!=RIJ_NB(pCtx), ippStsLengthErr);
   /* test padding */
   IPP_BADARG_RET(((IppsCPPaddingPKCS7!=padding)&&
                   (IppsCPPaddingZEROS!=padding)&&
                   (IppsCPPaddingNONE !=padding)), ippStsPaddingSchemeErr);
   /* test stream integrity */
   IPP_BADARG_RET(((len&(MBS_RIJ128-1)) && (IppsCPPaddingNONE==padding)), ippStsUnderRunErr);

   /*
   // encrypt block-by-block aligned streams
   */
   if( !(IPP_UINT_PTR(pSrc) & 0x3) && !(IPP_UINT_PTR(pDst) & 0x3)) {
      while(len >= MBS_RIJ128) {
         Encrypt_RIJ128((const Ipp32u*)pSrc, (Ipp32u*)pDst, RIJ_NR(pCtx), RIJ_EKEYS(pCtx));
         pSrc += MBS_RIJ128;
         pDst += MBS_RIJ128;
         len  -= MBS_RIJ128;
      }
   }

   /*
   // encrypt block-by-block misaligned streams
   */
   else {
      while(len >= MBS_RIJ128) {
         CopyBlock16(pSrc, tmpInp);
         Encrypt_RIJ128(tmpInp, tmpOut, RIJ_NR(pCtx), RIJ_EKEYS(pCtx));
         CopyBlock16(tmpOut, pDst);

         pSrc += MBS_RIJ128;
         pDst += MBS_RIJ128;
         len  -= MBS_RIJ128;
      }
   }

   /*
   // encrypt last data block
   */
   if(len) {
      Ipp8u filler = (Ipp8u)( (padding==IppsCPPaddingPKCS7)? (MBS_RIJ128-len) : 0);
      FillBlock16(filler, pSrc, tmpInp, len);
      Encrypt_RIJ128(tmpInp, tmpOut, RIJ_NR(pCtx), RIJ_EKEYS(pCtx));
      CopyBlock16(tmpOut, pDst);
   }

   return ippStsNoErr;
}
#endif

IPPFUN(IppStatus, ippsRijndael128EncryptECB,(const Ipp8u* pSrc, Ipp8u* pDst, int len,
                                             const IppsRijndael128Spec* pCtx,
                                             IppsCPPadding padding))
{
   /* test context */
   IPP_BAD_PTR1_RET(pCtx);
   /* use aligned Rijndael context */
   pCtx = (IppsRijndael128Spec*)( IPP_ALIGNED_PTR(pCtx, RIJ_ALIGNMENT) );

   IPP_BADARG_RET(!RIJ_ID_TEST(pCtx), ippStsContextMatchErr);
   /* test source and target pointers */
   IPP_BAD_PTR2_RET(pSrc, pDst);
   /* test stream length */
   IPP_BADARG_RET((len<1), ippStsLengthErr);
   /* test data block size */
   IPP_BADARG_RET(NB(128)!=RIJ_NB(pCtx), ippStsLengthErr);

   /* force IppsCPPaddingNONE padding */
   if(IppsCPPaddingNONE!=padding)
      padding = IppsCPPaddingNONE;
   /* test stream integrity */
   IPP_BADARG_RET(((len&(MBS_RIJ128-1)) && (IppsCPPaddingNONE==padding)), ippStsUnderRunErr);

   {
      /* setup encoder method */
      Rijn128Cipher encoder = SETUP_ENCODER_RIJ128(pCtx);

      #if defined(_IPP_TRS)
      Ipp32u RijEncTbl[4][256] = {
         { ENC_SBOX(fwd_t0) },
         { ENC_SBOX(fwd_t1) },
         { ENC_SBOX(fwd_t2) },
         { ENC_SBOX(fwd_t3) }
      };
      Ipp8u RijEncSbox[256] = {
         ENC_SBOX(none_t)
      };
      #endif

      /*
      // encrypt block-by-block aligned streams
      */
      if( !(IPP_UINT_PTR(pSrc) & 0x3) && !(IPP_UINT_PTR(pDst) & 0x3)) {
         while(len >= MBS_RIJ128) {
            //(RIJ_ENCODER(pCtx))(
            encoder(
                           #if defined(_IPP_TRS)
                           RijEncTbl, RijEncSbox,
                           #endif
                           (const Ipp32u*)pSrc, (Ipp32u*)pDst, RIJ_NR(pCtx), RIJ_EKEYS(pCtx));
            pSrc += MBS_RIJ128;
            pDst += MBS_RIJ128;
            len  -= MBS_RIJ128;
         }
      }

      /*
      // encrypt block-by-block misaligned streams
      */
      else {
         Ipp32u tmpInp[NB(128)];
         Ipp32u tmpOut[NB(128)];

         while(len >= MBS_RIJ128) {
            CopyBlock16(pSrc, tmpInp);
            //(RIJ_ENCODER(pCtx))(
            encoder(
                           #if defined(_IPP_TRS)
                           RijEncTbl, RijEncSbox,
                           #endif
                           tmpInp, tmpOut, RIJ_NR(pCtx), RIJ_EKEYS(pCtx));
            CopyBlock16(tmpOut, pDst);

            pSrc += MBS_RIJ128;
            pDst += MBS_RIJ128;
            len  -= MBS_RIJ128;
         }
      }

      return ippStsNoErr;
   }
}

#endif /* #ifndef _OPENMP */
