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
//    Digesting message according to SHA1
//
// Contents:
//    ippsSHA1GetSize()
//    ippsSHA1Init()
//    ippsSHA1Duplicate()
//    ippsSHA1Update()
//    ippsSHA1GetTag()
//    ippsSHA1Final()
//    ippsSHA1MessageDigest()
//
//
//    Created: Thu 07-Mar-2002 18:52
//  Author(s): Sergey Kirillov
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpshs.h"
#include "pcpciphertool.h"


/*
// Init SHA1 digest
*/

void InitSHA1(IppsSHA1State* pState)
{
   DigestSHA1 H0_SHA1 = FIPS_180_SHA1_HASH0;

   /* setup initial digest specified by FIPS 180-1 */
   CopyDigestSHA1(H0_SHA1, SHS_DGST(pState));

   /* zeros message length */
   SHS_LENL(pState) = 0;

   /* message buffer is free */
   SHS_INDX(pState) = 0;
}


/*F*
//    Name: ippsSHA1GetSize
//
// Purpose: Returns size (bytes) of IppsSHA1State state.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSize == NULL
//    ippStsNoErr             no errors
//
// Parameters:
//    pSize       pointer to state size
//
*F*/
IPPFUN(IppStatus, ippsSHA1GetSize,(int* pSize))
{
   /* test pointer */
   IPP_BAD_PTR1_RET(pSize);

   *pSize = sizeof(IppsSHA1State)
           +(SHA1_ALIGNMENT-1);

   return ippStsNoErr;
}


/*F*
//    Name: ippsSHA1Init
//
// Purpose: Init SHA1 state.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pState == NULL
//    ippStsNoErr             no errors
//
// Parameters:
//    pState      pointer to the SHA1 state
//
*F*/
IPPFUN(IppStatus, ippsSHA1Init,(IppsSHA1State* pState))
{
   /* test state pointer */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned context */
   pState = (IppsSHA1State*)( IPP_ALIGNED_PTR(pState, SHA1_ALIGNMENT) );

   /* set state ID */
   SHS_ID(pState) = idCtxSHA1;

   InitSHA1(pState);

   return ippStsNoErr;
}


/*F*
//    Name: ippsSHA1Duplicate
//
// Purpose: Clone SHA1 state.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSrcState == NULL
//                            pDstState == NULL
//    ippStsContextMatchErr   pSrcState->idCtx != idCtxSHA1
//                            pDstState->idCtx != idCtxSHA1
//    ippStsNoErr             no errors
//
// Parameters:
//    pSrcState   pointer to the source SHA1 state
//    pDstState   pointer to the target SHA1 state
//
// Note:
//    pDstState may to be uninitialized by ippsSHA1Init()
//
*F*/
IPPFUN(IppStatus, ippsSHA1Duplicate,(const IppsSHA1State* pSrcState, IppsSHA1State* pDstState))
{
   /* test state pointers */
   IPP_BAD_PTR2_RET(pSrcState, pDstState);
   /* use aligned context */
   pSrcState = (IppsSHA1State*)( IPP_ALIGNED_PTR(pSrcState, SHA1_ALIGNMENT) );
   pDstState = (IppsSHA1State*)( IPP_ALIGNED_PTR(pDstState, SHA1_ALIGNMENT) );
   /* test states ID */
   IPP_BADARG_RET(idCtxSHA1 !=SHS_ID(pSrcState), ippStsContextMatchErr);
   //IPP_BADARG_RET(idCtxSHA1 !=SHS_ID(pDstState), ippStsContextMatchErr);

   /* copy state */
   CopyBlock(pSrcState, pDstState, sizeof(IppsSHA1State));

   return ippStsNoErr;
}


/*F*
//    Name: ippsSHA1Update
//
// Purpose: Updates intermadiate digest based on input stream.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pSrc == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxSHA1
//    ippStsLengthErr         len <0
//    ippStsNoErr             no errors
//
// Parameters:
//    pSrc        pointer to the input stream
//    len         input stream length
//    pState      pointer to the SHA1 state
//
*F*/
IPPFUN(IppStatus, ippsSHA1Update,(const Ipp8u* pSrc, int len, IppsSHA1State* pState))
{
   int processedLen;

   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned context */
   pState = (IppsSHA1State*)( IPP_ALIGNED_PTR(pState, SHA1_ALIGNMENT) );

   /* test state ID */
   IPP_BADARG_RET(idCtxSHA1 !=SHS_ID(pState), ippStsContextMatchErr);
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
      processedLen = IPP_MIN(len, (MBS_SHA1 - SHS_INDX(pState)));
      CopyBlock(pSrc, SHS_BUFF(pState)+SHS_INDX(pState), processedLen);

      /* update internal buffer filling */
      SHS_INDX(pState) += processedLen;

      /* update message pointer and length */
      pSrc += processedLen;
      len  -= processedLen;

      /* update digest if buffer full */
      if( MBS_SHA1 == SHS_INDX(pState) ) {
         UpdateSHA1(SHS_BUFF(pState), SHS_DGST(pState));
         SHS_INDX(pState) = 0;
      }
   }

   /*
   // main part
   */
   processedLen = len & ~(MBS_SHA1-1);
   if(processedLen) {
      ProcessSHA1(SHS_BUFF(pState), SHS_DGST(pState), pSrc, processedLen);
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
void ComputeDigestSHA1(Ipp8u* pMD, const IppsSHA1State* pState)
{
   int padLen;

   DigestSHA1 aDigest;
   Ipp8u buffer[MBS_SHA1];

   /* make local copy of digest */
   CopyBlock(SHS_DGST(pState), aDigest, sizeof(DigestSHA1));
   /* make local copy of state's buffer */
   CopyBlock(SHS_BUFF(pState), buffer, MBS_SHA1);

   /* padding according to FIPS 180-1 */
   padLen = MBS_SHA1 - SHS_INDX(pState);
   PaddBlock(0, buffer+SHS_INDX(pState), padLen);
   buffer[SHS_INDX(pState)] = 0x80;

   /* internal message block is too small to hold the message length too */
   if((MBS_SHA1-1-sizeof(Ipp64u)) < SHS_INDX(pState)) {
      UpdateSHA1(buffer, aDigest);

      /* continue padding into a next block */
      PaddBlock(0, buffer, MBS_SHA1);
      /* store the message length (remember about big endian) */
      #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
      ((Ipp32u*)buffer)[14] = HIDWORD(SHS_LENL(pState));
      ((Ipp32u*)buffer)[15] = LODWORD(SHS_LENL(pState));
      #else
      ((Ipp32u*)buffer)[14] = ENDIANNESS(HIDWORD(SHS_LENL(pState)));
      ((Ipp32u*)buffer)[15] = ENDIANNESS(LODWORD(SHS_LENL(pState)));
      #endif
      UpdateSHA1(buffer, aDigest);
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
      UpdateSHA1(buffer, aDigest);
   }

   /* store digest into the user buffer (remember digest in big endian) */
   {
      int i;
      for(i=0; i<(int)sizeof(DigestSHA1); i++)
         pMD[i] = (Ipp8u)( (aDigest[i>>2]) >> (24 - 8*(i&3)) );
   }
}

/*F*
//    Name: ippsSHA1GetTag
//
// Purpose: Compute digest based on current state.
//          Note, that futher digest update is possible
//
// Returns:                Reason:
//    ippStsNullPtrErr        pTag == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxSHA1
//    ippStsLengthErr         max_SHA_digestLen < tagLen <1
//    ippStsNoErr             no errors
//
// Parameters:
//    pTag        address of the output digest
//    tagLen      length of digest
//    pState      pointer to the SHS state
//
*F*/
IPPFUN(IppStatus, ippsSHA1GetTag,(Ipp8u* pTag, Ipp32u tagLen, const IppsSHA1State* pState))
{
   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned context */
   pState = (IppsSHA1State*)( IPP_ALIGNED_PTR(pState, SHA1_ALIGNMENT) );

   /* test state ID */
   IPP_BADARG_RET(idCtxSHA1 !=SHS_ID(pState), ippStsContextMatchErr);
   /* test digest pointer */
   IPP_BAD_PTR1_RET(pTag);
   IPP_BADARG_RET((tagLen<1)||(sizeof(DigestSHA1)<tagLen), ippStsLengthErr);

   {
      Ipp8u digest[sizeof(DigestSHA1)];

      ComputeDigestSHA1(digest, pState);
      CopyBlock(digest, pTag, tagLen);

      return ippStsNoErr;
   }
}


/*F*
//    Name: ippsSHA1Final
//
// Purpose: Stop message digesting and return digest.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pMD == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxSHA1
//    ippStsNoErr             no errors
//
// Parameters:
//    pMD         address of the output digest
//    pState      pointer to the SHS state
//
*F*/
IPPFUN(IppStatus, ippsSHA1Final,(Ipp8u* pMD, IppsSHA1State* pState))
{
   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned context */
   pState = (IppsSHA1State*)( IPP_ALIGNED_PTR(pState, SHA1_ALIGNMENT) );

   /* test state ID */
   IPP_BADARG_RET(idCtxSHA1 !=SHS_ID(pState), ippStsContextMatchErr);
   /* test digest pointer */
   IPP_BAD_PTR1_RET(pMD);
   {
      Ipp8u digest[sizeof(DigestSHA1)];

      ComputeDigestSHA1(digest, pState);
      CopyBlock(digest, pMD, sizeof(DigestSHA1));
      InitSHA1(pState);

      return ippStsNoErr;
   }
}

#if 0
/*F*
//    Name: ippsSHA1Final
//
// Purpose: Stop message digesting and return digest.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pMD == NULL
//                            pState == NULL
//    ippStsContextMatchErr   pState->idCtx != idCtxSHA1
//    ippStsNoErr             no errors
//
// Parameters:
//    pMD         address of the output digest
//    pState      pointer to the SHS state
//
*F*/
IPPFUN(IppStatus, ippsSHA1Final,(Ipp8u* pMD, IppsSHA1State* pState))
{
   int padLen;
   int i;

   /* test state pointer and ID */
   IPP_BAD_PTR1_RET(pState);
   /* use aligned context */
   pState = (IppsSHA1State*)( IPP_ALIGNED_PTR(pState, SHA1_ALIGNMENT) );

   /* test state ID */
   IPP_BADARG_RET(idCtxSHA1 !=SHS_ID(pState), ippStsContextMatchErr);
   /* test digest pointer */
   IPP_BAD_PTR1_RET(pMD);

   /* padding according to FIPS 180-1 */
   padLen = MBS_SHA1 - SHS_INDX(pState);
   PaddBlock(0, SHS_BUFF(pState)+SHS_INDX(pState), padLen);
   SHS_BUFF(pState)[SHS_INDX(pState)] = 0x80;

   /* internal message block is too small to hold the message length too */
   if((MBS_SHA1-1-sizeof(Ipp64u)) < SHS_INDX(pState)) {
      UpdateSHA1(SHS_BUFF(pState), SHS_DGST(pState));

      /* continue padding into a next block */
      PaddBlock(0, SHS_BUFF(pState), MBS_SHA1);
      /* store the message length (remember about big endian) */
      #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
      ((Ipp32u*)SHS_BUFF(pState))[14] = HIDWORD(SHS_LENL(pState));
      ((Ipp32u*)SHS_BUFF(pState))[15] = LODWORD(SHS_LENL(pState));
      #else
      ((Ipp32u*)SHS_BUFF(pState))[14] = ENDIANNESS(HIDWORD(SHS_LENL(pState)));
      ((Ipp32u*)SHS_BUFF(pState))[15] = ENDIANNESS(LODWORD(SHS_LENL(pState)));
      #endif
      UpdateSHA1(SHS_BUFF(pState), SHS_DGST(pState));
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
      UpdateSHA1(SHS_BUFF(pState), SHS_DGST(pState));
   }

   /* store digest into the user buffer (remember digest in big endian) */
   for(i=0; i<(int)sizeof(DigestSHA1); i++)
      pMD[i] = (Ipp8u)( (SHS_DGST(pState)[i>>2]) >> (24 - 8*(i&3)) );

   /* reinit state */
   InitSHA1(pState);

   return ippStsNoErr;
}
#endif


/*F*
//    Name: ippsSHA1MessageDigest
//
// Purpose: Digest of the whole message.
//
// Returns:                Reason:
//    ippStsNullPtrErr        pMsg == NULL
//                            pMD == NULL
//    ippStsLengthErr         len <0
//    ippStsNoErr             no errors
//
// Parameters:
//    pMsg        pointer to the input message
//    len         input message length
//    pMD         address of the output digest
//
*F*/
IPPFUN(IppStatus, ippsSHA1MessageDigest,(const Ipp8u* pMsg, int len, Ipp8u* pMD))
{
   /* test digest pointer */
   IPP_BAD_PTR1_RET(pMD);
   /* test message length */
   IPP_BADARG_RET((len<0), ippStsLengthErr);
   /* test message pointer */
   IPP_BADARG_RET((len && !pMsg), ippStsNullPtrErr);

   {
      Ipp8u blob[sizeof(IppsSHA1State)+SHA1_ALIGNMENT-1];
      IppsSHA1State* pState = (IppsSHA1State*)( IPP_ALIGNED_PTR(&blob, SHA1_ALIGNMENT) );

      ippsSHA1Init  (pState);
      if(len) ippsSHA1Update(pMsg, len, pState);
      ippsSHA1Final (pMD, pState);

      return ippStsNoErr;
   }
}
