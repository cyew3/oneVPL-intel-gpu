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
//    Decrypt byte data stream according to Rijndael (ECB mode)
//
// Contents:
//    ippsRijndael128DecryptECB()
//
//
//    Created: Sat 25-May-2002 18:14
//  Author(s): Sergey Kirillov
//
//   Modified: Mon 07-Dec-2005 09:57
//  Author(s): Vasiliy Buzoverya
//             The code to provide conditional compilation depending on
//             OpenMP API support by compiler has been added.
*/
#include "precomp.h"

#ifndef _OPENMP

#include "owncp.h"
#include "pcprij.h"
#include "pcpciphertool.h"

#if defined(_IPP_TRS)
#  include "pcprijtables.h"
#endif


/*F*
//    Name: ippsRijndael128DecryptECB
//
// Purpose: Decrypt byte data stream according to Rijndael in EBC mode.
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
//    ippStsPaddingErr        last decrypted block filler mismatch padding
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
IPPFUN(IppStatus, ippsRijndael128DecryptECB,(const Ipp8u* pSrc, Ipp8u* pDst, int len,
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
   // decrypt block-by-block aligned streams
   */
   if( !(IPP_UINT_PTR(pSrc) & 0x3) && !(IPP_UINT_PTR(pDst) & 0x3)) {
      while(len >= MBS_RIJ128) {
         Decrypt_RIJ128((const Ipp32u*)pSrc, (Ipp32u*)pDst, RIJ_NR(pCtx), RIJ_DKEYS(pCtx));
         pSrc += MBS_RIJ128;
         pDst += MBS_RIJ128;
         len  -= MBS_RIJ128;
      }
   }

   /*
   // decrypt block-by-block misaligned streams
   */
   else {
      while(len >= MBS_RIJ128) {
         CopyBlock16(pSrc, tmpInp);
         Decrypt_RIJ128(tmpInp, tmpOut, RIJ_NR(pCtx), RIJ_DKEYS(pCtx));
         CopyBlock16(tmpOut, pDst);

         pSrc += MBS_RIJ128;
         pDst += MBS_RIJ128;
         len  -= MBS_RIJ128;
      }
   }

   /*
   // decrypt last data block
   */
   if(len) {
      Ipp8u filler = (Ipp8u)( (padding==IppsCPPaddingPKCS7)? (MBS_RIJ128-len) : 0);
      CopyBlock16(pSrc, tmpInp);
      Decrypt_RIJ128(tmpInp, tmpOut, RIJ_NR(pCtx), RIJ_DKEYS(pCtx));
      CopyBlock(tmpOut, pDst, len);
      if( !TestPadding(filler, ((Ipp8u*)&tmpOut)+len, MBS_RIJ128-len) )
         IPP_ERROR_RET(ippStsPaddingErr);
   }

   return ippStsNoErr;
}
#endif

IPPFUN(IppStatus, ippsRijndael128DecryptECB,(const Ipp8u* pSrc, Ipp8u* pDst, int len,
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
      /* setup decoder method */
      Rijn128Cipher decoder = SETUP_DECODER_RIJ128(pCtx);

      #if defined(_IPP_TRS)
      Ipp32u RijDecTbl[4][256] = {
         { DEC_SBOX(inv_t0) },
         { DEC_SBOX(inv_t1) },
         { DEC_SBOX(inv_t2) },
         { DEC_SBOX(inv_t3) }
      };
      Ipp8u RijDecSbox[256] = {
         DEC_SBOX(none_t)
      };
      #endif

      /*
      // decrypt block-by-block aligned streams
      */
      if( !(IPP_UINT_PTR(pSrc) & 0x3) && !(IPP_UINT_PTR(pDst) & 0x3)) {
         while(len >= MBS_RIJ128) {
            //(RIJ_DECODER(pCtx))(
            decoder(
                           #if defined(_IPP_TRS)
                           RijDecTbl, RijDecSbox,
                           #endif
                           (const Ipp32u*)pSrc, (Ipp32u*)pDst, RIJ_NR(pCtx), RIJ_DKEYS(pCtx));
            pSrc += MBS_RIJ128;
            pDst += MBS_RIJ128;
            len  -= MBS_RIJ128;
         }
      }

      /*
      // decrypt block-by-block misaligned streams
      */
      else {
         Ipp32u tmpInp[NB(128)];
         Ipp32u tmpOut[NB(128)];

         while(len >= MBS_RIJ128) {
            CopyBlock16(pSrc, tmpInp);
            //(RIJ_DECODER(pCtx))(
            decoder(
                           #if defined(_IPP_TRS)
                           RijDecTbl, RijDecSbox,
                           #endif
                           tmpInp, tmpOut, RIJ_NR(pCtx), RIJ_DKEYS(pCtx));
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
