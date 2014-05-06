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
//    Internal Definitions of Block Cipher Tools
//
//
//    Created: Sat 16-Feb-2002 14:48
//  Author(s): Sergey Kirillov
//
//   Modified: Fri 03-Feb-2006 17:31
//  Author(s): Vasiliy Buzoverya
//
*/
#if !defined(_PCP_CIPHERTOOL_H)
#define _PCP_CIPHERTOOL_H

void FillBlock8 (Ipp8u filler, const void* pSrc, void* pDst, int len);
void FillBlock16(Ipp8u filler, const void* pSrc, void* pDst, int len);
void FillBlock24(Ipp8u filler, const void* pSrc, void* pDst, int len);
void FillBlock32(Ipp8u filler, const void* pSrc, void* pDst, int len);

/* vb */
__INLINE void XorBlock8( const void* pSrc1, const void* pSrc2, void* pDst )
{
    int k;
    for( k = 0; k < 8; k++ )
        ( ( Ipp8u* )pDst )[k] = (Ipp8u)(( ( Ipp8u* )pSrc1 )[k] ^
            ( ( Ipp8u* )pSrc2 )[k]);
}

/* vb */
__INLINE void XorBlock16( const void* pSrc1, const void* pSrc2, void* pDst )
{
    int k;
    for( k = 0; k < 16; k++ )
        ( ( Ipp8u* )pDst )[k] = (Ipp8u)(( ( Ipp8u* )pSrc1 )[k] ^
            ( ( Ipp8u* )pSrc2 )[k]);
}

/* vb */
__INLINE void XorBlock24( const void* pSrc1, const void* pSrc2, void* pDst )
{
    int k;
    for( k = 0; k < 24; k++ )
        ( ( Ipp8u* )pDst )[k] = (Ipp8u)(( ( Ipp8u* )pSrc1 )[k] ^
            ( ( Ipp8u* )pSrc2 )[k]);
}

/* vb */
__INLINE void XorBlock32( const void* pSrc1, const void* pSrc2, void* pDst )
{
    int k;
    for( k = 0; k < 32; k++ )
        ( ( Ipp8u* )pDst )[k] = (Ipp8u)(( ( Ipp8u* )pSrc1 )[k] ^
            ( ( Ipp8u* )pSrc2 )[k]);
}

void XorBlock  (const void* pSrc1, const void* pSrc2, void* pDst, int len);

/* vb */
__INLINE void CopyBlock8( const void* pSrc, void* pDst )
{
    int k;
    for( k = 0; k < 8; k++ )
        ( ( Ipp8u* )pDst )[k] = ( ( Ipp8u* )pSrc )[k];
}

/* vb */
__INLINE void CopyBlock16( const void* pSrc, void* pDst )
{
    int k;
    for( k = 0; k < 16; k++ )
        ( ( Ipp8u* )pDst )[k] = ( ( Ipp8u* )pSrc )[k];
}

/* vb */
__INLINE void CopyBlock24( const void* pSrc, void* pDst )
{
    int k;
    for( k = 0; k < 24; k++ )
        ( ( Ipp8u* )pDst )[k] = ( ( Ipp8u* )pSrc )[k];
}

/* vb */
__INLINE void CopyBlock32( const void* pSrc, void* pDst )
{
    int k;
    for( k = 0; k < 32; k++ )
        ( ( Ipp8u* )pDst )[k] = ( ( Ipp8u* )pSrc )[k];
}

void CopyBlock(const void* pSrc, void* pDst, int len);

void PaddBlock(Ipp8u filler, void* pDst, int len);

int  TestPadding(Ipp8u filler, void* pSrc, int len);

/* addition functions for CTR mode of diffenent block ciphers */
#if ( _IPP_ARCH == _IPP_ARCH_XSC )
void StdIncrement(Ipp8u* pCounter, int blkSize, int numSize);
#else
__INLINE void StdIncrement(Ipp8u* pCounter, int blkSize, int numSize)
{
   int maskPosition = (blkSize-numSize)/8;
   Ipp8u mask = (Ipp8u)( 0xFF >> (blkSize-numSize)%8 );

   /* save crytical byte */
   Ipp8u save  = (Ipp8u)( pCounter[maskPosition] & ~mask );

   int len = BITS2WORD8_SIZE(blkSize);
   Ipp32u carry = 1;
   for(; (len>maskPosition) && carry; len--) {
      Ipp32u x = pCounter[len-1] + carry;
      pCounter[len-1] = (Ipp8u)x;
      carry = (x>>8) & 0xFF;
   }

   /* update crytical byte */
   pCounter[maskPosition] &= mask;
   pCounter[maskPosition] |= save;
}
#endif

/* vb */
__INLINE void ompStdIncrement64( void* pInitCtrVal, void* pCurrCtrVal,
                                int ctrNumBitSize, int n )
{
    int    k;
    Ipp64u cntr;
    Ipp64u temp;
    Ipp64s item;

  #if( IPP_ENDIAN == IPP_LITTLE_ENDIAN )
    for( k = 0; k < 8; k++ )
        ( ( Ipp8u* )&cntr )[k] = ( ( Ipp8u* )pInitCtrVal )[7 - k];
  #else
    for( k = 0; k < 8; k++ )
        ( ( Ipp8u* )&cntr )[k] = ( ( Ipp8u* )pInitCtrVal )[k];
  #endif

    if( ctrNumBitSize == 64 )
    {
        cntr += ( Ipp64u )n;
    }
    else
    {
        /* gres: Ipp64u mask = ( Ipp64u )0xFFFFFFFFFFFFFFFF >> ( 64 - ctrNumBitSize ); */
        Ipp64u mask = CONST_64(0xFFFFFFFFFFFFFFFF) >> ( 64 - ctrNumBitSize );
        Ipp64u save = cntr & ( ~mask );
        Ipp64u bndr = ( Ipp64u )1 << ctrNumBitSize;

        temp = cntr & mask;
        cntr = temp + ( Ipp64u )n;

        if( cntr > bndr )
        {
            item = ( Ipp64s )n - ( Ipp64s )( bndr - temp );

            while( item > 0 )
            {
                cntr  = ( Ipp64u )item;
                item -= ( Ipp64s )bndr;
            }
        }

        cntr = save | ( cntr & mask );
    }

  #if( IPP_ENDIAN == IPP_LITTLE_ENDIAN )
    for( k = 0; k < 8; k++ )
        ( ( Ipp8u* )pCurrCtrVal )[7 - k] = ( ( Ipp8u* )&cntr )[k];
  #else
    for( k = 0; k < 8; k++ )
        ( ( Ipp8u* )pCurrCtrVal )[k] = ( ( Ipp8u* )&cntr )[k];
  #endif
}


/* vb */
__INLINE void ompStdIncrement128( void* pInitCtrVal, void* pCurrCtrVal,
                                 int ctrNumBitSize, int n )
{
    int    k;
    Ipp64u low;
    Ipp64u hgh;
    Ipp64u flag;
    Ipp64u mask = CONST_64(0xFFFFFFFFFFFFFFFF);
    Ipp64u save;

  #if( IPP_ENDIAN == IPP_LITTLE_ENDIAN )
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )&low )[k] = ( ( Ipp8u* )pInitCtrVal )[15 - k];
        ( ( Ipp8u* )&hgh )[k] = ( ( Ipp8u* )pInitCtrVal )[7 - k];
    }
  #else
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )&low )[k] = ( ( Ipp8u* )pInitCtrVal )[8 + k];
        ( ( Ipp8u* )&hgh )[k] = ( ( Ipp8u* )pInitCtrVal )[k];
    }
  #endif

    if( ctrNumBitSize == 64 )
    {
        low += ( Ipp64u )n;
    }
    else if( ctrNumBitSize < 64 )
    {
        Ipp64u bndr;
        Ipp64u cntr;
        Ipp64s item;

        mask >>= ( 64 - ctrNumBitSize );
        save   = low & ( ~mask );
        cntr   = ( low & mask ) + ( Ipp64u )n;

        if( ctrNumBitSize < 31 )
        {
            bndr = ( Ipp64u )1 << ctrNumBitSize;

            if( cntr > bndr )
            {
                item = ( Ipp64s )( ( Ipp64s )n - ( ( Ipp64s )bndr -
                ( Ipp64s )( low & mask ) ) );

                while( item > 0 )
                {
                    cntr  = ( Ipp64u )item;
                    item -= ( Ipp64s )bndr;
                }
            }
        }

        low = save | ( cntr & mask );
    }
    else
    {
        flag = ( low >> 63 );

        if( ctrNumBitSize != 128 )
        {
            mask >>= ( 128 - ctrNumBitSize );
            save   = hgh & ( ~mask );
            hgh   &= mask;
        }

        low += ( Ipp64u )n;

        if( flag != ( low >> 63 ) ) hgh++;

        if( ctrNumBitSize != 128 )
        {
            hgh = save | ( hgh & mask );
        }
    }

  #if( IPP_ENDIAN == IPP_LITTLE_ENDIAN )
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )pCurrCtrVal )[15 - k] = ( ( Ipp8u* )&low )[k];
        ( ( Ipp8u* )pCurrCtrVal )[7 - k]  = ( ( Ipp8u* )&hgh )[k];
    }
  #else
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )pCurrCtrVal )[8 + k] = ( ( Ipp8u* )&low )[k];
        ( ( Ipp8u* )pCurrCtrVal )[k]     = ( ( Ipp8u* )&hgh )[k];
    }
  #endif
}


/* vb */
__INLINE void ompStdIncrement192( void* pInitCtrVal, void* pCurrCtrVal,
                                int ctrNumBitSize, int n )
{
    int    k;
    Ipp64u low;
    Ipp64u mdl;
    Ipp64u hgh;
    Ipp64u flag;
    Ipp64u mask = CONST_64(0xFFFFFFFFFFFFFFFF);
    Ipp64u save;

  #if( IPP_ENDIAN == IPP_LITTLE_ENDIAN )
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )&low )[k] = ( ( Ipp8u* )pInitCtrVal )[23 - k];
        ( ( Ipp8u* )&mdl )[k] = ( ( Ipp8u* )pInitCtrVal )[15 - k];
        ( ( Ipp8u* )&hgh )[k] = ( ( Ipp8u* )pInitCtrVal )[7  - k];
    }
  #else
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )&low )[k] = ( ( Ipp8u* )pInitCtrVal )[16 + k];
        ( ( Ipp8u* )&mdl )[k] = ( ( Ipp8u* )pInitCtrVal )[8  + k];
        ( ( Ipp8u* )&hgh )[k] = ( ( Ipp8u* )pInitCtrVal )[k];
    }
  #endif

    if( ctrNumBitSize == 64 )
    {
        low += ( Ipp64u )n;
    }
    else if( ctrNumBitSize == 128 )
    {
        flag = ( low >> 63 );
        low += ( Ipp64u )n;
        if( flag != ( low >> 63 ) ) mdl++;
    }
    else if( ctrNumBitSize == 192 )
    {
        flag = ( low >> 63 );
        low += ( Ipp64u )n;

        if( flag != ( low >> 63 ) )
        {
            flag = ( mdl >> 63 );
            mdl++;
            if( flag != ( mdl >> 63 ) ) hgh++;
        }
    }
    else if( ctrNumBitSize < 64 )
    {
        Ipp64u bndr;
        Ipp64u cntr;
        Ipp64s item;

        mask >>= ( 64 - ctrNumBitSize );
        save   = low & ( ~mask );
        cntr   = ( low & mask ) + ( Ipp64u )n;

        if( ctrNumBitSize < 31 )
        {
            bndr = ( Ipp64u )1 << ctrNumBitSize;

            if( cntr > bndr )
            {
                item = ( Ipp64s )( ( Ipp64s )n - ( ( Ipp64s )bndr -
                    ( Ipp64s )( low & mask ) ) );

                while( item > 0 )
                {
                    cntr  = ( Ipp64u )item;
                    item -= ( Ipp64s )bndr;
                }
            }
        }

        low = save | ( cntr & mask );
    }
    else if( ctrNumBitSize < 128 )
    {
        flag   = ( low >> 63 );
        mask >>= ( 128 - ctrNumBitSize );
        save   = mdl & ( ~mask );
        mdl   &= mask;
        low   += ( Ipp64u )n;
        if( flag != ( low >> 63 ) ) mdl++;
        mdl    = save | ( mdl & mask );
    }
    else
    {
        flag   = ( low >> 63 );
        mask >>= ( 192 - ctrNumBitSize );
        save   = hgh & ( ~mask );
        hgh   &= mask;
        low   += ( Ipp64u )n;

        if( flag != ( low >> 63 ) )
        {
            flag = ( mdl >> 63 );
            mdl++;
            if( flag != ( mdl >> 63 ) ) hgh++;
        }

        hgh    = save | ( hgh & mask );
    }

  #if( IPP_ENDIAN == IPP_LITTLE_ENDIAN )
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )pCurrCtrVal )[23 - k] = ( ( Ipp8u* )&low )[k];
        ( ( Ipp8u* )pCurrCtrVal )[15 - k] = ( ( Ipp8u* )&mdl )[k];
        ( ( Ipp8u* )pCurrCtrVal )[7  - k] = ( ( Ipp8u* )&hgh )[k];
    }
  #else
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )pCurrCtrVal )[16 + k] = ( ( Ipp8u* )&low )[k];
        ( ( Ipp8u* )pCurrCtrVal )[8  + k] = ( ( Ipp8u* )&mdl )[k];
        ( ( Ipp8u* )pCurrCtrVal )[k]      = ( ( Ipp8u* )&hgh )[k];
    }
  #endif
}


/* vb */
__INLINE void ompStdIncrement256( void* pInitCtrVal, void* pCurrCtrVal,
                                 int ctrNumBitSize, int n )
{
    int    k;
    Ipp64u low;
    Ipp64u mdl;
    Ipp64u mdm;
    Ipp64u hgh;
    Ipp64u flag;
    Ipp64u mask = CONST_64(0xFFFFFFFFFFFFFFFF);
    Ipp64u save;

  #if( IPP_ENDIAN == IPP_LITTLE_ENDIAN )
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )&low )[k] = ( ( Ipp8u* )pInitCtrVal )[31 - k];
        ( ( Ipp8u* )&mdl )[k] = ( ( Ipp8u* )pInitCtrVal )[23 - k];
        ( ( Ipp8u* )&mdm )[k] = ( ( Ipp8u* )pInitCtrVal )[15 - k];
        ( ( Ipp8u* )&hgh )[k] = ( ( Ipp8u* )pInitCtrVal )[7  - k];
    }
  #else
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )&low )[k] = ( ( Ipp8u* )pInitCtrVal )[24 + k];
        ( ( Ipp8u* )&mdl )[k] = ( ( Ipp8u* )pInitCtrVal )[16 + k];
        ( ( Ipp8u* )&mdm )[k] = ( ( Ipp8u* )pInitCtrVal )[8  + k];
        ( ( Ipp8u* )&hgh )[k] = ( ( Ipp8u* )pInitCtrVal )[k];
    }
  #endif

    if( ctrNumBitSize == 64 )
    {
        low += ( Ipp64u )n;
    }
    else if( ctrNumBitSize == 128 )
    {
        flag = ( low >> 63 );
        low += ( Ipp64u )n;
        if( flag != ( low >> 63 ) ) mdl++;
    }
    else if( ctrNumBitSize == 192 )
    {
        flag = ( low >> 63 );
        low += ( Ipp64u )n;

        if( flag != ( low >> 63 ) )
        {
            flag = ( mdl >> 63 );
            mdl++;
            if( flag != ( mdl >> 63 ) ) hgh++;
        }
    }
    else if( ctrNumBitSize == 256 )
    {
        flag = ( low >> 63 );
        low += ( Ipp64u )n;

        if( flag != ( low >> 63 ) )
        {
            flag = ( mdl >> 63 );
            mdl++;

            if( flag != ( mdl >> 63 ) )
            {
                flag = ( mdm >> 63 );
                mdm++;
                if( flag != ( mdm >> 63 ) ) hgh++;
            }
        }
    }
    else if( ctrNumBitSize < 64 )
    {
        Ipp64u bndr;
        Ipp64u cntr;
        Ipp64s item;

        mask >>= ( 64 - ctrNumBitSize );
        save   = low & ( ~mask );
        cntr   = ( low & mask ) + ( Ipp64u )n;

        if( ctrNumBitSize < 31 )
        {
            bndr = ( Ipp64u )1 << ctrNumBitSize;

            if( cntr > bndr )
            {
                item = ( Ipp64s )( ( Ipp64s )n - ( ( Ipp64s )bndr -
                    ( Ipp64s )( low & mask ) ) );

                while( item > 0 )
                {
                    cntr  = ( Ipp64u )item;
                    item -= ( Ipp64s )bndr;
                }
            }
        }

        low = save | ( cntr & mask );
    }
    else if( ctrNumBitSize < 128 )
    {
        flag   = ( low >> 63 );
        mask >>= ( 128 - ctrNumBitSize );
        save   = mdl & ( ~mask );
        mdl   &= mask;
        low   += ( Ipp64u )n;
        if( flag != ( low >> 63 ) ) mdl++;
        mdl    = save | ( mdl & mask );
    }
    else if( ctrNumBitSize < 192 )
    {
        flag   = ( low >> 63 );
        mask >>= ( 192 - ctrNumBitSize );
        save   = mdm & ( ~mask );
        mdm   &= mask;
        low   += ( Ipp64u )n;

        if( flag != ( low >> 63 ) )
        {
            flag = ( mdl >> 63 );
            mdl++;
            if( flag != ( mdl >> 63 ) ) mdm++;
        }

        mdm    = save | ( mdm & mask );
    }
    else
    {
        flag   = ( low >> 63 );
        mask >>= ( 256 - ctrNumBitSize );
        save   = hgh & ( ~mask );
        hgh   &= mask;
        low   += ( Ipp64u )n;

        if( flag != ( low >> 63 ) )
        {
            flag = ( mdl >> 63 );
            mdl++;

            if( flag != ( mdl >> 63 ) )
            {
                flag = ( mdm >> 63 );
                mdm++;
                if( flag != ( mdm >> 63 ) ) hgh++;
            }
        }

        hgh    = save | ( hgh & mask );
    }

  #if( IPP_ENDIAN == IPP_LITTLE_ENDIAN )
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )pCurrCtrVal )[31 - k] = ( ( Ipp8u* )&low )[k];
        ( ( Ipp8u* )pCurrCtrVal )[23 - k] = ( ( Ipp8u* )&mdl )[k];
        ( ( Ipp8u* )pCurrCtrVal )[15 - k] = ( ( Ipp8u* )&mdm )[k];
        ( ( Ipp8u* )pCurrCtrVal )[7  - k] = ( ( Ipp8u* )&hgh )[k];
    }
  #else
    for( k = 0; k < 8; k++ )
    {
        ( ( Ipp8u* )pCurrCtrVal )[24 + k] = ( ( Ipp8u* )&low )[k];
        ( ( Ipp8u* )pCurrCtrVal )[16 + k] = ( ( Ipp8u* )&mdl )[k];
        ( ( Ipp8u* )pCurrCtrVal )[8  + k] = ( ( Ipp8u* )&mdm )[k];
        ( ( Ipp8u* )pCurrCtrVal )[k]      = ( ( Ipp8u* )&hgh )[k];
    }
  #endif
}

/* compare (equivalence) */
int EquBlock(const void* pSrc1, const void* pSrc2, int len);

#endif /* _PCP_CIPHERTOOL_H */
