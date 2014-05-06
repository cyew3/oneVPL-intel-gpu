/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Encrypt/Decrypt byte data stream according to Rijndael128 (CTR mode)
//
// Contents:
//    ippsRijndael128EncryptCTR()
//    ippsRijndael128DecryptCTR()
//
//
//    Created: Sat 12-Jun-2004 13:04
//  Author(s): Sergey Kirillov
//
//   Modified: Tue 31-Jan-2006 14:07
//  Author(s): Vasiliy Buzoverya
//             The code to provide conditional compilation depending on
//             OpenMP API support by a compiler has been added.
*/
#if defined(_IPP_v50_)

#include "precomp.h"

#ifndef _OPENMP /* vb */

#include "owncp.h"
#include "pcprij.h"
#include "pcpciphertool.h"

#if defined(_IPP_TRS)
#  include "pcprijtables.h"
#endif


/*F*
//    Name: ippsRijndael128EncryptCTR
//          ippsRijndael128DecryptCTR
//
// Purpose: Encrypt/Decrypt byte data stream according to Rijndael in CTR mode.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pCtx == NULL
//                            pSrc == NULL
//                            pDst == NULL
//                            pCtrValue ==NULL
//    ippStsContextMatchErr   pCtx->idCtx != idCtxRijndael
//    ippStsLengthErr         len <1
//    ippStsCTRSizeErr        128 < ctrNumBitSize < 1
//                            pCtx->nb mismatch function suffix 128
//    ippStsNoErr             no errors
//
// Parameters:
//    pSrc           pointer to the source data stream
//    pDst           pointer to the target data stream
//    len            plaintext stream length (bytes)
//    pCtx           RIJ context
//    pCtrValue      pointer to the counter block
//    ctrNumBitSize  counter block size (bits)
//
// Note:
//    counter will updated on return
//
*F*/
static
IppStatus Rij128CTR(const Ipp8u* pSrc, Ipp8u* pDst, int len,
                    const IppsRijndael128Spec* pCtx,
                    Ipp8u* pCtrValue, int ctrNumBitSize)
{
   Ipp32u counter[NB(128)];
   Ipp32u  output[NB(128)];

   /* test context */
   IPP_BAD_PTR1_RET(pCtx);
   /* use aligned Rijndael context */
   pCtx = (IppsRijndael128Spec*)( IPP_ALIGNED_PTR(pCtx, RIJ_ALIGNMENT) );

   IPP_BADARG_RET(!RIJ_ID_TEST(pCtx), ippStsContextMatchErr);
   /* test source, target and counter block pointers */
   IPP_BAD_PTR3_RET(pSrc, pDst, pCtrValue);
   /* test stream length */
   IPP_BADARG_RET((len<1), ippStsLengthErr);
   /* test data block size */
   IPP_BADARG_RET(NB(128)!=RIJ_NB(pCtx), ippStsLengthErr);
   /* test counter block size */
   IPP_BADARG_RET(((MBS_RIJ128*8)<ctrNumBitSize)||(ctrNumBitSize<1), ippStsCTRSizeErr);

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

      /* copy counter */
      CopyBlock16(pCtrValue, counter);

      /*
      // encrypt block-by-block aligned streams
      */
      while(len >= MBS_RIJ128) {
         /* encrypt counter block */
         //(RIJ_ENCODER(pCtx))(
         encoder(
                        #if defined(_IPP_TRS)
                        RijEncTbl, RijEncSbox,
                        #endif
                        counter, output, RIJ_NR(pCtx), RIJ_EKEYS(pCtx));
         /* compute ciphertext block */
         if( !(IPP_UINT_PTR(pSrc) & 0x3) && !(IPP_UINT_PTR(pDst) & 0x3)) {
            ((Ipp32u*)pDst)[0] = output[0]^((Ipp32u*)pSrc)[0];
            ((Ipp32u*)pDst)[1] = output[1]^((Ipp32u*)pSrc)[1];
            ((Ipp32u*)pDst)[2] = output[2]^((Ipp32u*)pSrc)[2];
            ((Ipp32u*)pDst)[3] = output[3]^((Ipp32u*)pSrc)[3];
         }
         else
            XorBlock16(pSrc, output, pDst);
         /* encrement counter block */
         StdIncrement((Ipp8u*)counter,MBS_RIJ128*8, ctrNumBitSize);

         pSrc += MBS_RIJ128;
         pDst += MBS_RIJ128;
         len  -= MBS_RIJ128;
      }
      /*
      // encrypt last data block
      */
      if(len) {
         /* encrypt counter block */
         //(RIJ_ENCODER(pCtx))(
         encoder(
                        #if defined(_IPP_TRS)
                        RijEncTbl, RijEncSbox,
                        #endif
                        counter, output, RIJ_NR(pCtx), RIJ_EKEYS(pCtx));
         /* compute ciphertext block */
         XorBlock(pSrc, output, pDst,len);
         /* encrement counter block */
         StdIncrement((Ipp8u*)counter,MBS_RIJ128*8, ctrNumBitSize);
      }

      /* update counter */
      CopyBlock16(counter, pCtrValue);

      return ippStsNoErr;
   }
}

IPPFUN(IppStatus, ippsRijndael128EncryptCTR,(const Ipp8u* pSrc, Ipp8u* pDst, int len,
                                      const IppsRijndael128Spec* pCtx,
                                      Ipp8u* pCtrValue, int ctrNumBitSize))
{
   return Rij128CTR(pSrc,pDst,len, pCtx, pCtrValue,ctrNumBitSize);
}

IPPFUN(IppStatus, ippsRijndael128DecryptCTR,(const Ipp8u* pSrc, Ipp8u* pDst, int len,
                                      const IppsRijndael128Spec* pCtx,
                                      Ipp8u* pCtrValue, int ctrNumBitSize))
{
   return Rij128CTR(pSrc,pDst,len, pCtx, pCtrValue,ctrNumBitSize);
}

#endif /* #ifndef _OPENMP */

#endif /* _IPP_v50_ */
