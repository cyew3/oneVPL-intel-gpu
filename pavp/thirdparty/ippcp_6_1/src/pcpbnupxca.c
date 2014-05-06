/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2002-2008 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//                  Cryptographic Primitives (ippcp)
//
//  File:       pcpinnerfuncpxca.c
//  Created:    2003/03/25    14:42
//
//  Last modified by Alexey Omeltchenko
*/

#include "precomp.h"
#include "owncp.h"
#include "pcpbnu.h"


/*memory management functions*/
void *cpMemset(void *dest, int c, int count)
{
    int i;
    for (i = 0; i < count; i++)
        ((char*)dest)[i] = (char)c;
    return dest;
}

void *cpMemcpy(void *dest, const void *src, int count)
{
    int i;
    for (i = 0; i < count; i++)
        ((char*)dest)[i] = ((char*)src)[i];
    return dest;
}

Ipp32u *cpMemset32u(Ipp32u *dest, Ipp32u c, int count)
{
    int i;
    for (i = 0; i < count; i++)
        dest[i] = c;
    return dest;
}

Ipp32u *cpMemcpy32u(Ipp32u *dest, const Ipp32u *src, unsigned int count)
{
    unsigned int i;
    for (i = 0; i < count; i++)
        dest[i] = src[i];
    return dest;
}

void cpEndianChange(Ipp32u *buffer, int size)
{
    int i;
    for (i = 0; i < size; i++)
        buffer[i] = ENDIANNESS(buffer[i]);
}

/*
// cpCmp_BNU
//
// Compare two BNU values
//
// Returns
//    -1, if A < B
//     0, if A = B
//    +1, if A > B
*/
int cpCmp_BNU(const Ipp32u* pA, const Ipp32u* pB, int ns)
{
   int sign;
   CMP_BNU(sign, pA, pB, ns);
   return sign;
}


/* Function cpAdd_BNU - addition of 2 BigNumbers  */
#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
void cpAdd_BNU(const Ipp32u *a, const Ipp32u *b, Ipp32u *r,int n, Ipp32u *carry)
{
    int i;
    Ipp32u temp=0;

    union
    {
        Ipp64u u_64;
        struct
        {
            #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
            Ipp32u hi;
            Ipp32u lo;
            #else
            Ipp32u lo;
            Ipp32u hi;
            #endif
        } u_32;
    } tmp;

   for(i=0; i<n; i++) {
        tmp.u_64 = (Ipp64u)( (Ipp64u)(a[i]) + b[i] + temp );
        r[i] = tmp.u_32.lo;
        temp = tmp.u_32.hi;
   }
   *carry = temp;
   return;

}
#endif


/* Function cpSub_BNU - subtraction of 2 BigNumbers  */
#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
void cpSub_BNU(const Ipp32u *a, const Ipp32u *b, Ipp32u *r,int n, Ipp32u *carry)
{
    const Ipp32u *pa, *pb;
    Ipp32u *pr;
    int c=0, c1=0;
    int i, n4, n3;
    n4=n/4, n3=n%4;
    pa = a;
    pb = b;
    pr = r;

    for (i=0; i<n4; i++)
    {
        if (pa[0] != pb[0])
            c1=(pa[0] < pb[0]);
        pr[0]=(pa[0] - pb[0] - c)&IPP_MAX_32U;
        c = c1;

        if (pa[1] != pb[1])
            c1=(pa[1] < pb[1]);
        pr[1]=(pa[1] - pb[1] - c)&IPP_MAX_32U;
        c = c1;

        if (pa[2] != pb[2])
            c1=(pa[2] < pb[2]);
        pr[2]=(pa[2] - pb[2] - c)&IPP_MAX_32U;
        c = c1;

        if (pa[3] != pb[3])
            c1=(pa[3] < pb[3]);
        pr[3]=(pa[3] - pb[3] - c)&IPP_MAX_32U;
        c = c1;

        pa+=4;
        pb+=4;
        pr+=4;
    }
    if (n3!=0)
    {
        if (pa[0] != pb[0])
            c1=(pa[0] < pb[0]);
        pr[0]=(pa[0] - pb[0] - c)&IPP_MAX_32U;
        c = c1;
        if (n3!=1)
        {
            if (pa[1] != pb[1])
                c1=(pa[1] < pb[1]);
            pr[1]=(pa[1] - pb[1] - c)&IPP_MAX_32U;
            c = c1;
            if (n3!=2)
            {
                if (pa[2] != pb[2])
                    c1=(pa[2] < pb[2]);
                pr[2]=(pa[2] - pb[2] - c)&IPP_MAX_32U;
                c = c1;
            }
        }
    }

    *carry = c;
}
#endif


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
void cpMulDgt_BNU(const Ipp32u *a, Ipp32u *r, int n, Ipp32u w, Ipp32u *carry)
{
    Ipp32u c;
    int j;
    union
    {
        Ipp64u u_64;
        struct
        {
            #if (IPP_ENDIAN == IPP_BIG_ENDIAN)
            Ipp32u hi;
            Ipp32u lo;
            #else
            Ipp32u lo;
            Ipp32u hi;
            #endif
        } u_32;
    } tmp;

    /* Multiplication */
    c = 0;
    for(j = 0; j < n; j++) {
        tmp.u_64 = (Ipp64u)w * (Ipp64u)a[j] + c;
        r[j] = tmp.u_32.lo;
        c = tmp.u_32.hi;
    }

    *carry = c;
}
#endif


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
void cpAddMulDgt_BNU(const Ipp32u *a, Ipp32u *r, int num, Ipp32u w, Ipp32u *carry)
{
    Ipp32u temp_carry=0;
    Ipp64u temp_a;
    int n4, n3;
    n4 = num/4;
    n3=num-n4*4;

    while (n4--)
    {
        temp_a= (Ipp64u)a[0] * w + r[0] + temp_carry; r[0] = (Ipp32u)temp_a; temp_carry = (Ipp32u)(temp_a>>32);
        temp_a= (Ipp64u)a[1] * w + r[1] + temp_carry; r[1] = (Ipp32u)temp_a; temp_carry = (Ipp32u)(temp_a>>32);
        temp_a= (Ipp64u)a[2] * w + r[2] + temp_carry; r[2] = (Ipp32u)temp_a; temp_carry = (Ipp32u)(temp_a>>32);
        temp_a= (Ipp64u)a[3] * w + r[3] + temp_carry; r[3] = (Ipp32u)temp_a; temp_carry = (Ipp32u)(temp_a>>32);
        a+=4; r+=4;
    }
    if (n3)
    {   /*1*/
        temp_a= (Ipp64u)a[0] * w + r[0] + temp_carry; r[0] = (Ipp32u)temp_a; temp_carry = (Ipp32u)(temp_a>>32);
        if (--n3!=0)
        {   /*2*/
            temp_a= (Ipp64u)a[1] * w + r[1] + temp_carry; r[1] = (Ipp32u)temp_a; temp_carry = (Ipp32u)(temp_a>>32);
            if (--n3!=0)
            {   /*3*/
                temp_a= (Ipp64u)a[2] * w + r[2] + temp_carry; r[2] = (Ipp32u)temp_a; temp_carry = (Ipp32u)(temp_a>>32);
            }
        }
    }

    *carry = temp_carry;

}
#endif


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
void cpSubMulDgt_BNU(const Ipp32u* pA, Ipp32u* pR, int ns, Ipp32u w, Ipp32u *pCarry)
{
   Ipp32u carry = 0;
   for(; ns>0; ns--) {
      Ipp64u r = (Ipp64u)*pR - (Ipp64u)(*pA++) * w - (Ipp64u)carry;
      *pR++  = LODWORD(r);
      carry  =-HIDWORD(r);
   }
   *pCarry = carry;
}
#endif


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
void cpSqr_BNU8(const Ipp32u *a, Ipp32u *r)
{
    Ipp64u temp64;
    Ipp32u temp[16];
    int i,j;
    const Ipp32u *pa;
    Ipp32u *pr;
    Ipp32u carry;

    pa=a;
    pr=r;
    pr[0]=pr[15]=0;
    pr++;
    j=8;

    if(--j > 0)
    {
        pa++;
        cpMulDgt_BNU((Ipp32u*)pa, (Ipp32u *)pr, j, pa[-1], pr+j);
        pr+=2;
    }

    for (i=6; i>0; i--)
    {
        j--;
        pa++;
        cpAddMulDgt_BNU((Ipp32u*)pa, (Ipp32u*)pr, j, pa[-1], pr+j);
        pr+=2;
    }

    cpAdd_BNU((Ipp32u *)r, (Ipp32u *)r, (Ipp32u *)r, 16, &carry);

    /* There will not be a carry */
    temp64 = (Ipp64u)a[0]*a[0]; temp[0]=(Ipp32u)temp64; temp[1]=(Ipp32u)(temp64>>32);
    temp64 = (Ipp64u)a[1]*a[1]; temp[2]=(Ipp32u)temp64; temp[3]=(Ipp32u)(temp64>>32);
    temp64 = (Ipp64u)a[2]*a[2]; temp[4]=(Ipp32u)temp64; temp[5]=(Ipp32u)(temp64>>32);
    temp64 = (Ipp64u)a[3]*a[3]; temp[6]=(Ipp32u)temp64; temp[7]=(Ipp32u)(temp64>>32);

    temp64 = (Ipp64u)a[4]*a[4]; temp[8] =(Ipp32u)temp64; temp[9] =(Ipp32u)(temp64>>32);
    temp64 = (Ipp64u)a[5]*a[5]; temp[10]=(Ipp32u)temp64; temp[11]=(Ipp32u)(temp64>>32);
    temp64 = (Ipp64u)a[6]*a[6]; temp[12]=(Ipp32u)temp64; temp[13]=(Ipp32u)(temp64>>32);
    temp64 = (Ipp64u)a[7]*a[7]; temp[14]=(Ipp32u)temp64; temp[15]=(Ipp32u)(temp64>>32);

    cpAdd_BNU((Ipp32u *)r, (Ipp32u *)temp, (Ipp32u *)r, 16, &carry);


}
#endif


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
void cpSqr_BNU4(const Ipp32u *a, Ipp32u *r)
{
    Ipp64u uv;
    int i,j;
    Ipp32u* pDst;
    pDst = (Ipp32u*)r;
    cpMemset32u((void *)r, 0, (2 * 4));

    for(i=0; i<4; i++) {
        uv = 0;
        for(j=0; j<4; j++){
            uv += (Ipp64u)(a[i])*(Ipp64u)(a[j])+(Ipp64u)(pDst[i+j]);
            pDst[i+j]= (Ipp32u)(uv);
            /*uv = ((Ipp32u*)(&uv))[1];*/  uv = (uv>>32);
        }
        pDst[i+j] = (Ipp32u)(uv);

    }
}
#endif


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP==_IPP_W7) || (_IPP==_IPP_T7) || \
      (_IPP==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7))
void cpMul_BNU8(const Ipp32u *a, const Ipp32u *b, Ipp32u *r)
{
    Ipp32u c;
    Ipp64u uv;
    int i, j;
        /* Multiplication */
    cpMemset32u(r, 0, 16);
    for (i = 0; i < 8; i++) {
        c = 0;
        for(j = 0; j < 8; j++) {
            uv = (Ipp64u)(r[i + j]) + (Ipp64u)b[i] * (Ipp64u)a[j] + c;
            r[i + j] = (Ipp32u)uv;
            c = (Ipp32u)(uv >> 32);
        }
        r[i + 8] = c;
    }
}
#endif


#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7) )
void cpMul_BNU4(const Ipp32u *a, const Ipp32u *b, Ipp32u *r)
{
    Ipp32u c;
    Ipp64u uv;
    int i, j;
        /* Multiplication */
    cpMemset32u(r, 0, 8);
    for (i = 0; i < 4; i++) {
        c = 0;
        for(j = 0; j < 4; j++) {
            uv = (Ipp64u)(r[i + j]) + (Ipp64u)b[i] * (Ipp64u)a[j] + c;
            r[i + j] = (Ipp32u)uv;
            c = (Ipp32u)(uv >> 32);
        }
        r[i + 4] = c;
    }
}
#endif


/*
// Div64u32u
//
// 64-bits by 32-bits division
//
*/
#if !((_IPPXSC==_IPPXSC_S1) || (_IPPXSC==_IPPXSC_S2) || (_IPPXSC==_IPPXSC_C2) || \
      (_IPP32E==_IPP32E_M7) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9) || \
      (_IPP64==_IPP64_I7) )
Ipp64u cpDiv64u32u(Ipp64u a, Ipp32u b)
{
   Ipp64u tmp = a/b;
   Ipp32u q = LODWORD(tmp);
   Ipp32u r = LODWORD( a - q*b );
   return MAKEDWORD(r,q);
}
#endif
