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
//    Digesting message according to SHA256
//
// Contents:
//    ippsSHA256GetSize()
//    ippsSHA256Init()
//    ippsSHA256Duplicate()
//    ippsSHA256Update()
//    ippsSHA256GetTag()
//    ippsSHA256Final()
//    ippsSHA256MessageDigest()
//
//
//    Created: Sun 10-Mar-2002 12:47
//  Author(s): Sergey Kirillov
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpshs.h"
#include "pcpciphertool.h"


/*
// Init SHA256 digest
*/
void InitSHA256(IppsSHA256State* pState)
{
   DigestSHA256 H0_SHA256 = {
      0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
   };

   /* setup initial digest specified by proposal 256-384-512 */
   SHS_DGST(pState)[0] = H0_SHA256[0];
   SHS_DGST(pState)[1] = H0_SHA256[1];
   SHS_DGST(pState)[2] = H0_SHA256[2];
   SHS_DGST(pState)[3] = H0_SHA256[3];
   SHS_DGST(pState)[4] = H0_SHA256[4];
   SHS_DGST(pState)[5] = H0_SHA256[5];
   SHS_DGST(pState)[6] = H0_SHA256[6];
   SHS_DGST(pState)[7] = H0_SHA256[7];

   /* zeros message length */
   SHS_LENL(pState) = 0;

   /* message buffer is free */
   SHS_INDX(pState) = 0;
}


/*F*
//    Name: ippsSHA256GetSize
//
// Purpose: Returns size (bytes) of IppsSHA256State state.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSize == NULL
//    ippStsNoErr             no errors
//
// Parameters:
//    pSize       pointer to state size
//
*F*/
IPPFUN(IppStatus, ippsSHA256GetSize,(int* pSize))
{
   /* test pointer */
   IPP_BAD_PTR1_RET(pSize);

   *pSize = sizeof(IppsSHA256State)
           +(SHA256_ALIGNMENT-1);

   return ippStsNoErr;
}


/*F*
//    Name: ippsSHA256Init
//
// Purpose: Init SHA256
//
// Returns:                Reason:
//    ippStsNullPtrErr        pState == NULL
//    ippStsNoErr             no errors
//
// Parameters:
//    pState      pointer to the SHA512 state
//
*F*/
IPPFUN(IppStatus, ippsSHA256Init,(IppsSHA256State* pState))
{
   /* test state pointer */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned context */
   pState = (IppsSHA256State*)( IPP_ALIGNED_PTR(pState, SHA256_ALIGNMENT) );

   /* set state ID */
   SHS_ID(pState) = idCtxSHA256;

   InitSHA256(pState);

   return ippStsNoErr;
}


/*F*
//    Name: ippsSHA256Duplicate
//
// Purpose: Clone SHA256 state.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSrcState == NULL
//                            pDstState == NULL
//    ippStsContextMatchErr   pSrcState->idCtx != idCtxSHA256
//                            pDstState->idCtx != idCtxSHA256
//    ippStsNoErr             no errors
//
// Parameters:
//    pSrcState   pointer to the source SHA256 state
//    pDstState   pointer to the target SHA256 state
//
// Note:
//    pDstState may to be uninitialized by ippsSHA256Init()
//
*F*/
IPPFUN(IppStatus, ippsSHA256Duplicate,(const IppsSHA256State* pSrcState, IppsSHA256State* pDstState))
{
   /* test state pointers */
   IPP_BAD_PTR2_RET(pSrcState, pDstState);
   /* use aligned context */
   pSrcState = (IppsSHA256State*)( IPP_ALIGNED_PTR(pSrcState, SHA256_ALIGNMENT) );
   pDstState = (IppsSHA256State*)( IPP_ALIGNED_PTR(pDstState, SHA256_ALIGNMENT) );
   /* test states ID */
   IPP_BADARG_RET(idCtxSHA256 !=SHS_ID(pSrcState), ippStsContextMatchErr);
   //IPP_BADARG_RET(idCtxSHA256 !=SHS_ID(pDstState), ippStsContextMatchErr);

   /* copy state */
   CopyBlock(pSrcState, pDstState, sizeof(IppsSHA256State));

   return ippStsNoErr;
}


/*F*
//    Name: ippsSHA256Update
//
// Purpose: Updates intermadiate digest based on input stream.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSrc == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxSHA256
//    ippStsLengthErr         len <0
//    ippStsNoErr             no errors
//
// Parameters:
//    pSrc        pointer to the input stream
//    len         input stream length
//    pState      pointer to the SHA256 state
//
*F*/
IPPFUN(IppStatus, ippsSHA256Update,(const Ipp8u* pSrc, int len, IppsSHA256State* pState))
{
   int processedLen;

   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned context */
   pState = (IppsSHA256State*)( IPP_ALIGNED_PTR(pState, SHA256_ALIGNMENT) );

   /* test state ID */
   IPP_BADARG_RET(idCtxSHA256 !=SHS_ID(pState), ippStsContextMatchErr);
   /* test input length */
   IPP_BADARG_RET((len<0), ippStsLengthErr);
   /* test source pointer */
   IPP_BADARG_RET((len && !pSrc), ippStsNullPtrErr);

   /* handle empty message */
   if(!len)
      return ippStsNoErr;

   /* update message length */
   SHS_LENL(pState) += len*8;

   /*
   // test internal buffer filling
   */
   if(SHS_INDX(pState)) {
      /* copy from input stream to the internal buffer as match as possible */
      processedLen = IPP_MIN(len, (MBS_SHA256 - SHS_INDX(pState)));
      CopyBlock(pSrc, SHS_BUFF(pState)+SHS_INDX(pState), processedLen);

      /* update internal buffer filling */
      SHS_INDX(pState) += processedLen;

      /* update message pointer and length */
      pSrc += processedLen;
      len  -= processedLen;

      /* update digest if buffer full */
      if( MBS_SHA256 == SHS_INDX(pState) ) {
         UpdateSHA256(SHS_BUFF(pState), SHS_DGST(pState));
         SHS_INDX(pState) = 0;
      }
   }

   /*
   // main part
   */
   processedLen = len & ~(MBS_SHA256-1);
   if(processedLen) {
      ProcessSHA256(SHS_BUFF(pState), SHS_DGST(pState), pSrc, processedLen);
      /* update message pointer and length */
      pSrc += processedLen;
      len  -= processedLen;
   }

   /*
   // remaind
   */
   if(len) {
      CopyBlock(pSrc, SHS_BUFF(pState), len);
      /* update internal buffer filling */
      SHS_INDX(pState) += len;
   }

   return ippStsNoErr;
}


/*
// Compute digest
*/
void ComputeDigestSHA256(Ipp8u* pMD, const IppsSHA256State* pState)
{
   int padLen;

   DigestSHA256 aDigest;
   Ipp8u buffer[MBS_SHA256];

   /* make local copy of digest */
   CopyBlock(SHS_DGST(pState), aDigest, sizeof(DigestSHA256));
   /* make local copy of state's buffer */
   CopyBlock(SHS_BUFF(pState), buffer, MBS_SHA256);

   /* padding according to proposal 256-384-512 */
   padLen = MBS_SHA256 - SHS_INDX(pState);
   PaddBlock(0, buffer+SHS_INDX(pState), padLen);
   buffer[SHS_INDX(pState)] = 0x80;

   /* internal message block is too small to hold the message length too */
   if((MBS_SHA256-1-sizeof(Ipp64u)) < SHS_INDX(pState)) {
      UpdateSHA256(buffer, aDigest);

      /* continue padding into a next block */
      PaddBlock(0, buffer, MBS_SHA256);
      /* store the message length (remember about big endian) */
      #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
      ((Ipp32u*)buffer)[14] = HIDWORD(SHS_LENL(pState));
      ((Ipp32u*)buffer)[15] = LODWORD(SHS_LENL(pState));
      #else
      ((Ipp32u*)buffer)[14] = ENDIANNESS(HIDWORD(SHS_LENL(pState)));
      ((Ipp32u*)buffer)[15] = ENDIANNESS(LODWORD(SHS_LENL(pState)));
      #endif
      UpdateSHA256(buffer, aDigest);
   }

   /* internal message block is enough to hold the message length too */
   else {
      /* store the message length (remember about big endian) */
      #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
      ((Ipp32u*)buffer)[14] = HIDWORD(SHS_LENL(pState));
      ((Ipp32u*)buffer)[15] = LODWORD(SHS_LENL(pState));
      #else
      ((Ipp32u*)buffer)[14] = ENDIANNESS(HIDWORD(SHS_LENL(pState)));
      ((Ipp32u*)buffer)[15] = ENDIANNESS(LODWORD(SHS_LENL(pState)));
      #endif
      UpdateSHA256(buffer, aDigest);
   }

   /* store digest into the user buffer (remember digest in big endian) */
   {
      int i;
      for(i=0; i<(int)sizeof(DigestSHA256); i++)
         pMD[i] = (Ipp8u)( (aDigest[i>>2]) >> (24 - 8*(i&3)) );
   }
}


/*F*
//    Name: ippsSHA256GetTag
//
// Purpose: Compute digest based on current state.
//          Note, that futher digest update is possible
//
// Returns:                Reason:
//    ippStsNullPtrErr        pTag == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxSHA256
//    ippStsLengthErr         max_SHA_digestLen < tagLen <1
//    ippStsNoErr             no errors
//
// Parameters:
//    pTag        address of the output digest
//    tagLen      length of digest
//    pState      pointer to the SHS state
//
*F*/
IPPFUN(IppStatus, ippsSHA256GetTag,(Ipp8u* pTag, Ipp32u tagLen, const IppsSHA256State* pState))
{
   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned context */
   pState = (IppsSHA256State*)( IPP_ALIGNED_PTR(pState, SHA256_ALIGNMENT) );

   /* test state ID */
   IPP_BADARG_RET(idCtxSHA256 !=SHS_ID(pState), ippStsContextMatchErr);
   /* test digest pointer */
   IPP_BAD_PTR1_RET(pTag);
   IPP_BADARG_RET((tagLen<1)||(sizeof(DigestSHA256)<tagLen), ippStsLengthErr);

   {
      Ipp8u digest[sizeof(DigestSHA256)];

      ComputeDigestSHA256(digest, pState);
      CopyBlock(digest, pTag, tagLen);

      return ippStsNoErr;
   }
}


/*F*
//    Name: ippsSHA256Final
//
// Purpose: Stop message digesting and return digest.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pDigest == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxSHA256
//    ippStsNoErr             no errors
//
// Parameters:
//    pMD         address of the output digest
//    pState      pointer to the SHA256 state
//
*F*/
IPPFUN(IppStatus, ippsSHA256Final,(Ipp8u* pMD, IppsSHA256State* pState))
{
   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned context */
   pState = (IppsSHA256State*)( IPP_ALIGNED_PTR(pState, SHA256_ALIGNMENT) );

   /* test state ID */
   IPP_BADARG_RET(idCtxSHA256 !=SHS_ID(pState), ippStsContextMatchErr);
   /* test digest pointer */
   IPP_BAD_PTR1_RET(pMD);

   {
      Ipp8u digest[sizeof(DigestSHA256)];

      ComputeDigestSHA256(digest, pState);
      CopyBlock(digest, pMD, sizeof(DigestSHA256));
      InitSHA256(pState);

      return ippStsNoErr;
   }
}


#if 0
/*F*
//    Name: ippsSHA256Final
//
// Purpose: Stop message digesting and return digest.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pDigest == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxSHA256
//    ippStsNoErr             no errors
//
// Parameters:
//    pMD         address of the output digest
//    pState      pointer to the SHA256 state
//
*F*/
IPPFUN(IppStatus, ippsSHA256Final,(Ipp8u* pMD, IppsSHA256State* pState))
{
   int padLen;
   int i;

   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned context */
   pState = (IppsSHA256State*)( IPP_ALIGNED_PTR(pState, SHA256_ALIGNMENT) );

   /* test state ID */
   IPP_BADARG_RET(idCtxSHA256 !=SHS_ID(pState), ippStsContextMatchErr);
   /* test digest pointer */
   IPP_BAD_PTR1_RET(pMD);

   /* padding according to proposal 256-384-512 */
   padLen = MBS_SHA256 - SHS_INDX(pState);
   PaddBlock(0, SHS_BUFF(pState)+SHS_INDX(pState), padLen);
   SHS_BUFF(pState)[SHS_INDX(pState)] = 0x80;

   /* internal message block is too small to hold the message length too */
   if((MBS_SHA256-1-sizeof(Ipp64u)) < SHS_INDX(pState)) {
      UpdateSHA256(SHS_BUFF(pState), SHS_DGST(pState));

      /* continue padding into a next block */
      PaddBlock(0, SHS_BUFF(pState), MBS_SHA256);
      /* store the message length (remember about big endian) */
      #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
      ((Ipp32u*)SHS_BUFF(pState))[14] = HIDWORD(SHS_LENL(pState));
      ((Ipp32u*)SHS_BUFF(pState))[15] = LODWORD(SHS_LENL(pState));
      #else
      ((Ipp32u*)SHS_BUFF(pState))[14] = ENDIANNESS(HIDWORD(SHS_LENL(pState)));
      ((Ipp32u*)SHS_BUFF(pState))[15] = ENDIANNESS(LODWORD(SHS_LENL(pState)));
      #endif
      UpdateSHA256(SHS_BUFF(pState), SHS_DGST(pState));
   }

   /* internal message block is enough to hold the message length too */
   else {
      /* store the message length (remember about big endian) */
      #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
      ((Ipp32u*)SHS_BUFF(pState))[14] = HIDWORD(SHS_LENL(pState));
      ((Ipp32u*)SHS_BUFF(pState))[15] = LODWORD(SHS_LENL(pState));
      #else
      ((Ipp32u*)SHS_BUFF(pState))[14] = ENDIANNESS(HIDWORD(SHS_LENL(pState)));
      ((Ipp32u*)SHS_BUFF(pState))[15] = ENDIANNESS(LODWORD(SHS_LENL(pState)));
      #endif
      UpdateSHA256(SHS_BUFF(pState), SHS_DGST(pState));
   }

   /* store digest into the user buffer (remember digest in big endian) */
   for(i=0; i<(int)sizeof(DigestSHA256); i++)
      pMD[i] = (Ipp8u)( (SHS_DGST(pState)[i>>2]) >> (24 - 8*(i&3)) );

   /* reinit state */
   InitSHA256(pState);

   return ippStsNoErr;
}
#endif


/*F*
//    Name: ippsSHA256MessageDigest
//
// Purpose: Digest of the whole message.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pMsg == NULL
//                            pDigest == NULL
//    ippStsLengthErr         len <0
//    ippStsNoErr             no errors
//
// Parameters:
//    pMsg        pointer to the input message
//    len         input message length
//    pMD         address of the output digest
//
*F*/
IPPFUN(IppStatus, ippsSHA256MessageDigest,(const Ipp8u* pMsg, int len, Ipp8u* pMD))
{
   /* test digest pointer */
   IPP_BAD_PTR1_RET(pMD);
   /* test message length */
   IPP_BADARG_RET((len<0), ippStsLengthErr);
   /* test message pointer */
   IPP_BADARG_RET((len && !pMsg), ippStsNullPtrErr);

   {
      Ipp8u blob[sizeof(IppsSHA256State)+SHA256_ALIGNMENT-1];
      IppsSHA256State* pState = (IppsSHA256State*)( IPP_ALIGNED_PTR(&blob, SHA256_ALIGNMENT) );

      ippsSHA256Init  (pState);
      if(len) ippsSHA256Update(pMsg, len, pState);
      ippsSHA256Final (pMD, pState);

      return ippStsNoErr;
   }
}
