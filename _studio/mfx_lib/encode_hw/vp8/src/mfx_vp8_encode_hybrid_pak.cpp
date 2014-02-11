/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#include "mfx_vp8_encode_hybrid_bse.h"
#include "mfx_vp8_encode_utils_hw.h"

#if defined VP8_HYBRID_CPUPAK

#include "ippcc.h"

#pragma warning(disable:4244)

namespace MFX_VP8ENC
{
    const unsigned char vp8_ClampTbl[768] =
    {
        0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
        ,0x00 ,0x01 ,0x02 ,0x03 ,0x04 ,0x05 ,0x06 ,0x07
        ,0x08 ,0x09 ,0x0a ,0x0b ,0x0c ,0x0d ,0x0e ,0x0f
        ,0x10 ,0x11 ,0x12 ,0x13 ,0x14 ,0x15 ,0x16 ,0x17
        ,0x18 ,0x19 ,0x1a ,0x1b ,0x1c ,0x1d ,0x1e ,0x1f
        ,0x20 ,0x21 ,0x22 ,0x23 ,0x24 ,0x25 ,0x26 ,0x27
        ,0x28 ,0x29 ,0x2a ,0x2b ,0x2c ,0x2d ,0x2e ,0x2f
        ,0x30 ,0x31 ,0x32 ,0x33 ,0x34 ,0x35 ,0x36 ,0x37
        ,0x38 ,0x39 ,0x3a ,0x3b ,0x3c ,0x3d ,0x3e ,0x3f
        ,0x40 ,0x41 ,0x42 ,0x43 ,0x44 ,0x45 ,0x46 ,0x47
        ,0x48 ,0x49 ,0x4a ,0x4b ,0x4c ,0x4d ,0x4e ,0x4f
        ,0x50 ,0x51 ,0x52 ,0x53 ,0x54 ,0x55 ,0x56 ,0x57
        ,0x58 ,0x59 ,0x5a ,0x5b ,0x5c ,0x5d ,0x5e ,0x5f
        ,0x60 ,0x61 ,0x62 ,0x63 ,0x64 ,0x65 ,0x66 ,0x67
        ,0x68 ,0x69 ,0x6a ,0x6b ,0x6c ,0x6d ,0x6e ,0x6f
        ,0x70 ,0x71 ,0x72 ,0x73 ,0x74 ,0x75 ,0x76 ,0x77
        ,0x78 ,0x79 ,0x7a ,0x7b ,0x7c ,0x7d ,0x7e ,0x7f
        ,0x80 ,0x81 ,0x82 ,0x83 ,0x84 ,0x85 ,0x86 ,0x87
        ,0x88 ,0x89 ,0x8a ,0x8b ,0x8c ,0x8d ,0x8e ,0x8f
        ,0x90 ,0x91 ,0x92 ,0x93 ,0x94 ,0x95 ,0x96 ,0x97
        ,0x98 ,0x99 ,0x9a ,0x9b ,0x9c ,0x9d ,0x9e ,0x9f
        ,0xa0 ,0xa1 ,0xa2 ,0xa3 ,0xa4 ,0xa5 ,0xa6 ,0xa7
        ,0xa8 ,0xa9 ,0xaa ,0xab ,0xac ,0xad ,0xae ,0xaf
        ,0xb0 ,0xb1 ,0xb2 ,0xb3 ,0xb4 ,0xb5 ,0xb6 ,0xb7
        ,0xb8 ,0xb9 ,0xba ,0xbb ,0xbc ,0xbd ,0xbe ,0xbf
        ,0xc0 ,0xc1 ,0xc2 ,0xc3 ,0xc4 ,0xc5 ,0xc6 ,0xc7
        ,0xc8 ,0xc9 ,0xca ,0xcb ,0xcc ,0xcd ,0xce ,0xcf
        ,0xd0 ,0xd1 ,0xd2 ,0xd3 ,0xd4 ,0xd5 ,0xd6 ,0xd7
        ,0xd8 ,0xd9 ,0xda ,0xdb ,0xdc ,0xdd ,0xde ,0xdf
        ,0xe0 ,0xe1 ,0xe2 ,0xe3 ,0xe4 ,0xe5 ,0xe6 ,0xe7
        ,0xe8 ,0xe9 ,0xea ,0xeb ,0xec ,0xed ,0xee ,0xef
        ,0xf0 ,0xf1 ,0xf2 ,0xf3 ,0xf4 ,0xf5 ,0xf6 ,0xf7
        ,0xf8 ,0xf9 ,0xfa ,0xfb ,0xfc ,0xfd ,0xfe ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
        ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    };

    Vp8PakMfd::Vp8PakMfd()
    {
        m_aRow[0] = (PixType*) malloc(4*4096); 
        m_aRow[1] = m_aRow[0] + 4096;
        m_aRow[2] = m_aRow[1] + 2048;

        m_lRow[0] = m_aRow[0] + 8192;
        m_lRow[1] = m_lRow[0] + 4096;
        m_lRow[2] = m_lRow[1] + 2048;
    }

    Vp8PakMfd::~Vp8PakMfd()
    {
        free(m_aRow[0]);
    }

    Vp8EncoderFrame::Vp8EncoderFrame()
    {
        m_buffer_ptr = data_y = data_u = data_v = NULL;

        frameSize.width = frameSize.height = 0;
        mbPerCol = mbPerRow = mbStep = 0;
        uNumMBs = 0;
        m_buffer_size = 0;
    }

    Vp8EncoderFrame::~Vp8EncoderFrame()
    {
        if(m_buffer_ptr) free(m_buffer_ptr);
    }

    enum {
        DATA_ALIGN          = 64,
        DEFAULT_ALIGN_VALUE = 16
    };

    template<class T> inline   T align_value(U32 nValue, U32 lAlignValue = DEFAULT_ALIGN_VALUE)
    {
        return static_cast<T> ((nValue + (lAlignValue - 1)) & ~(lAlignValue - 1));
    }

    template<class T> inline    T align_pointer(void *pv, size_t lAlignValue = DEFAULT_ALIGN_VALUE)
    {
        // some compilers complain to conversion to/from
        // pointer types from/to integral types.
        return (T) ((((size_t) (pv)) + (lAlignValue - 1)) & ~(lAlignValue - 1));
    }

    void Vp8EncoderFrame::InitAlloc(U32 width, U32 height)
    {
        U32     framesize;

        if((width | height) & ~0x3fff)
            return ;

        frameSize.width = width; frameSize.height = height;

        mbPerCol = (frameSize.height + 15) >> 4;
        mbPerRow = (frameSize.width + 15) >> 4;
        uNumMBs  = mbPerCol * mbPerRow;

        pitch = align_value<U32> (((mbPerRow << 4) + 2*VP8_PADDING), DATA_ALIGN);

        framesize  = pitch * ((mbPerCol << 4) + 2*VP8_PADDING);
        framesize += pitch * ((mbPerCol << 3) + VP8_PADDING);
        framesize += DEFAULT_ALIGN_VALUE;

        if(m_buffer_size < framesize) {
            if(m_buffer_ptr) free(m_buffer_ptr);
            m_buffer_ptr = (U8*)malloc(framesize);
            if(!m_buffer_ptr) 
                return;
            m_buffer_size = framesize;
        }

        data_y = align_pointer<U8*> (m_buffer_ptr);                 /* aligned by 16 */
        data_u = data_y + pitch *((mbPerCol << 4) + 2*VP8_PADDING); /* aligned by 16 */
        data_v = data_u + (pitch >> 1);                             /* aligned by 16 if mbPerRow is even */

        data_y += (pitch + 1) * VP8_PADDING;
        data_u += (pitch + 1) * (VP8_PADDING>>1);
        data_v += (pitch + 1) * (VP8_PADDING>>1);
    }

    void vp8_fdct4x4(CoeffsType *pSrc, CoeffsType *pDst)
    {
        I32         i, a, b, c, d;
        CoeffsType *pD = pDst;

        for (pD = pDst, i = 0; i < 4; i++, pSrc+=4, pD+=4)
        {
            a = ((pSrc[0] + pSrc[3])<<3);
            b = ((pSrc[1] + pSrc[2])<<3);
            c = ((pSrc[1] - pSrc[2])<<3);
            d = ((pSrc[0] - pSrc[3])<<3);

            pD[0] = a + b;
            pD[1] = (c * 2217 + d * 5352 +  14500)>>12;
            pD[2] = a - b;
            pD[3] = (d * 2217 - c * 5352 +   7500)>>12;
        }
        for (pD = pDst, i = 0; i < 4; i++, pD++)
        {
            a = pD[0] + pD[12];
            b = pD[4] + pD[8];
            c = pD[4] - pD[8];
            d = pD[0] - pD[12];

            pD[0]  = ( a + b + 7)>>4;
            pD[8]  = ( a - b + 7)>>4;
            pD[4]  =((c * 2217 + d * 5352 +  12000)>>16) + (d!=0);
            pD[12] = (d * 2217 - c * 5352 +  51000)>>16;
        }
    }

    void vp8_fwht4x4(CoeffsType *pSrc, CoeffsType *pDst)
    {
        U32 i;
        I32 a1, b1, c1, d1;
        I32 a2, b2, c2, d2;
        CoeffsType *pD = pDst;

        for (i = 0; i < 4; i++,pSrc += 4,pD += 4)
        {
            a1 = ((pSrc[0] + pSrc[2])<<2);
            d1 = ((pSrc[1] + pSrc[3])<<2);
            c1 = ((pSrc[1] - pSrc[3])<<2);
            b1 = ((pSrc[0] - pSrc[2])<<2);

            pD[0] = a1 + d1 + (a1!=0);
            pD[1] = b1 + c1;
            pD[2] = b1 - c1;
            pD[3] = a1 - d1;
        }
        for (i = 0,pD = pDst; i < 4; i++, pD++)
        {
            a1 = pD[0] + pD[8];
            d1 = pD[4] + pD[12];
            c1 = pD[4] - pD[12];
            b1 = pD[0] - pD[8];

            a2 = a1 + d1;
            b2 = b1 + c1;
            c2 = b1 - c1;
            d2 = a1 - d1;

            a2 += a2<0;
            b2 += b2<0;
            c2 += c2<0;
            d2 += d2<0;

            pD[0] = (a2+3) >> 3;
            pD[4] = (b2+3) >> 3;
            pD[8] = (c2+3) >> 3;
            pD[12]= (d2+3) >> 3;
        }
    }

    void Vp8PakMfdVft::doProcess(FwdTransIf &mB)
    {
        U32 i, j, k, offsetSrc, offsetRec;
        I16 tCoeff[16];

        if((mB.mbType==VP8_MB_B_PRED)&&(mB.mbSubId>=0)) {
            k = mB.mbSubId;
            offsetSrc = ((k&0x3) + (k>>2)*mB.pitchSrc) << 2;
            offsetRec = ((k&0x3) + (k>>2)*mB.pitchRec) << 2;
            for(j=0;j<4;j++) {
                for(i=0;i<4;i++) {
                    tCoeff[(j<<2)+i] = mB.pYSrc[i+offsetSrc+j*mB.pitchSrc] - mB.pYRec[i+offsetRec+j*mB.pitchRec];
                }
            }
            vp8_fdct4x4(tCoeff, mB.pCoeffs+16*k);
            if(mB.mbSubId!=15) return;
        } else {
            for(k=0;k<16;k++){
                offsetSrc = ((k&0x3) + (k>>2)*mB.pitchSrc) << 2;
                offsetRec = ((k&0x3) + (k>>2)*mB.pitchRec) << 2;
                for(j=0;j<4;j++) {
                    for(i=0;i<4;i++) {
                        tCoeff[(j<<2)+i] = mB.pYSrc[i+offsetSrc+j*mB.pitchSrc] - mB.pYRec[i+offsetRec+j*mB.pitchRec];
                    }
                }
                vp8_fdct4x4(tCoeff, mB.pCoeffs+16*k);
            }
        }
        for(k=0;k<4;k++){
            offsetSrc = ((k&0x1) + (k>>1)*mB.pitchSrc) << 2;
            offsetRec = ((k&0x1) + (k>>1)*mB.pitchRec) << 2;
            for(j=0;j<4;j++) {
                for(i=0;i<4;i++) {
                    tCoeff[(j<<2)+i] = mB.pUSrc[i+offsetSrc+j*mB.pitchSrc] - mB.pURec[i+offsetRec+j*mB.pitchRec];
                }
            }
            vp8_fdct4x4(tCoeff, mB.pCoeffs+16*(k+16));
        }

        for(k=0;k<4;k++){
            offsetSrc = ((k&0x1) + (k>>1)*mB.pitchSrc) << 2;
            offsetRec = ((k&0x1) + (k>>1)*mB.pitchRec) << 2;
            for(j=0;j<4;j++) {
                for(i=0;i<4;i++) {
                    tCoeff[(j<<2)+i] = mB.pVSrc[i+offsetSrc+j*mB.pitchSrc] - mB.pVRec[i+offsetRec+j*mB.pitchRec];
                }
            }
            vp8_fdct4x4(tCoeff, mB.pCoeffs+16*(k+20));
        }

        if(mB.mbType!=VP8_MB_B_PRED)
        {
            for(j=0;j<16;j++) { tCoeff[j] = mB.pCoeffs[16*j]; mB.pCoeffs[16*j] = 0; };
            vp8_fwht4x4(tCoeff, mB.pCoeffs+16*24);
        }
    }

    void Vp8PakMfdVft::doProcessSkip(FwdTransIf &mB)
    {
        U32 k, offsetSrc, offsetRec;

        if((mB.mbType==VP8_MB_B_PRED)&&(mB.mbSubId>=0)) {
            k = mB.mbSubId;
            offsetSrc = ((k&0x3) + (k>>2)*mB.pitchSrc) << 2;
            offsetRec = ((k&0x3) + (k>>2)*mB.pitchRec) << 2;
            if(mB.mbSubId!=15) return;
        } else {
            for(k=0;k<16;k++){
                offsetSrc = ((k&0x3) + (k>>2)*mB.pitchSrc) << 2;
                offsetRec = ((k&0x3) + (k>>2)*mB.pitchRec) << 2;
            }
        }
        for(k=0;k<4;k++){
            offsetSrc = ((k&0x1) + (k>>1)*mB.pitchSrc) << 2;
            offsetRec = ((k&0x1) + (k>>1)*mB.pitchRec) << 2;
        }

        for(k=0;k<4;k++){
            offsetSrc = ((k&0x1) + (k>>1)*mB.pitchSrc) << 2;
            offsetRec = ((k&0x1) + (k>>1)*mB.pitchRec) << 2;
        }
    }

#define IDCT_20091 20091
#define IDCT_35468 35468

    void vp8_idct_4x4(I16* coefs, I16* data, I32 step)
    {
        I32  i   = 0;
        I16 *src = coefs;
        I16 *dst = data;
        I16  temp[16];
        I16  a1, a2, a3, a4;

        for(i = 0 ; i < 4; i++)
        {
            a1 = src[i] + src[8+i];
            a2 = src[i] - src[8+i];
            a3 = ((IDCT_35468*src[4+i])>>16) - (((IDCT_20091*src[12+i])>>16) + src[12+i]);
            a4 = (src[4+i] + ((IDCT_20091*src[4+i])>>16)) + ((src[12+i]*IDCT_35468)>>16);

            temp[i + 0]  = (I16)(a1 + a4);
            temp[i + 4]  = (I16)(a2 + a3);
            temp[i + 8]  = (I16)(a2 - a3);
            temp[i + 12] = (I16)(a1 - a4);
        }

        for(i = 0; i < 4; i++)
        {
            a1 = temp[i*4 + 0] + temp[i*4 + 2];
            a2 = temp[i*4 + 0] - temp[i*4 + 2];

            a3 = ((temp[i*4+1]*IDCT_35468)>>16) - (temp[i*4+3] + ((temp[i*4+3]*IDCT_20091)>>16));
            a4 = (temp[i*4+1] + ((temp[i*4+1]*IDCT_20091)>>16)) + ((temp[i*4+3]*IDCT_35468)>>16);

            dst[0 + i*step] = (I16)((a1 + a4 + 4) >>3);
            dst[1 + i*step] = (I16)((a2 + a3 + 4) >>3);
            dst[2 + i*step] = (I16)((a2 - a3 + 4) >>3);
            dst[3 + i*step] = (I16)((a1 - a4 + 4) >>3);
        }
    }

    void vp8_iwht_4x4(I16* dcCoefs, I16* coefs)
    {
        I32 i = 0;
        I32 a1, b1, c1, d1;

        I16* ptr = dcCoefs;

        for(i = 0; i < 4; i++)
        {
            a1 = ptr[i+0] + ptr[i+12];
            b1 = ptr[i+4] + ptr[i+8];
            c1 = ptr[i+4] - ptr[i+8];
            d1 = ptr[i+0] - ptr[i+12];

            ptr[i+0]  = (I16)(a1 + b1);
            ptr[i+4]  = (I16)(c1 + d1);
            ptr[i+8]  = (I16)(a1 - b1);
            ptr[i+12] = (I16)(d1 - c1);
        }

        for(i = 0; i < 4; i++)
        {
            a1 = ptr[i*4+0] + ptr[i*4+3];
            b1 = ptr[i*4+1] + ptr[i*4+2];
            c1 = ptr[i*4+1] - ptr[i*4+2];
            d1 = ptr[i*4+0] - ptr[i*4+3];


            coefs[i*4 + 0] = (I16)((a1 + b1 + 3) >> 3);
            coefs[i*4 + 1] = (I16)((c1 + d1 + 3) >> 3);
            coefs[i*4 + 2] = (I16)((a1 - b1 + 3) >> 3);
            coefs[i*4 + 3] = (I16)((d1 - c1 + 3) >> 3);
        }

        return;
    }

    void Vp8PakMfdVit::doProcess(InvTransIf &mB)
    {
        U32 i,j,k, offsetDst, offsetRec;
        I16 tCoeff[16];

        if(mB.mbType!=VP8_MB_B_PRED)
        {
            vp8_iwht_4x4(mB.pCoeffs+16*24,tCoeff);
            for(j=0;j<16;j++) mB.pCoeffs[16*j] = tCoeff[j];
        }

        if((mB.mbType==VP8_MB_B_PRED)&&(mB.mbSubId>=0))
        {
            k = mB.mbSubId;
            offsetDst = ((k&0x3) + (k>>2)*mB.pitchDst) << 2;
            offsetRec = ((k&0x3) + (k>>2)*mB.pitchRec) << 2;
            vp8_idct_4x4(mB.pCoeffs+16*k, tCoeff, 4);
            for(j=0;j<4;j++){
                for(i=0;i<4;i++) {
                    mB.pYDst[i+j*mB.pitchDst+offsetDst] = vp8_clamp8u(mB.pYRec[i+j*mB.pitchRec+offsetRec] + tCoeff[(j<<2)+i]);
                }
            }
            if(mB.mbSubId!=15) return;
        } else {
            for(k=0;k<16;k++){
                offsetDst = ((k&0x3) + (k>>2)*mB.pitchDst) << 2;
                offsetRec = ((k&0x3) + (k>>2)*mB.pitchRec) << 2;
                vp8_idct_4x4(mB.pCoeffs+16*k, tCoeff, 4);
                for(j=0;j<4;j++){
                    for(i=0;i<4;i++) {
                        mB.pYDst[i+j*mB.pitchDst+offsetDst] = vp8_clamp8u(mB.pYRec[i+j*mB.pitchRec+offsetRec] + tCoeff[(j<<2)+i]);
                    }
                }
            }
        }
        for(k=0;k<4;k++){
            offsetDst = ((k&0x1) + (k>>1)*mB.pitchDst) << 2;
            offsetRec = ((k&0x1) + (k>>1)*mB.pitchRec) << 2;
            vp8_idct_4x4(mB.pCoeffs+16*(k+16), tCoeff, 4);
            for(j=0;j<4;j++){
                for(i=0;i<4;i++) {
                    mB.pUDst[i+j*mB.pitchDst+offsetDst] = vp8_clamp8u(mB.pURec[i+j*mB.pitchRec+offsetRec] + tCoeff[(j<<2)+i]);
                }
            }
            vp8_idct_4x4(mB.pCoeffs+16*(k+20), tCoeff, 4);
            for(j=0;j<4;j++){
                for(i=0;i<4;i++) {
                    mB.pVDst[i+j*mB.pitchDst+offsetDst] = vp8_clamp8u(mB.pVRec[i+j*mB.pitchRec+offsetRec] + tCoeff[(j<<2)+i]);
                }
            }
        }
    }

    void Vp8PakMfdVit::doProcessSkip(InvTransIf &mB)
    {
        U32 i,j,k, offsetDst, offsetRec;

        if((mB.mbType==VP8_MB_B_PRED)&&(mB.mbSubId>=0))
        {
            k = mB.mbSubId;
            offsetDst = ((k&0x3) + (k>>2)*mB.pitchDst) << 2;
            offsetRec = ((k&0x3) + (k>>2)*mB.pitchRec) << 2;
            for(j=0;j<4;j++){
                for(i=0;i<4;i++) {
                    mB.pYDst[i+j*mB.pitchDst+offsetDst] = mB.pYRec[i+j*mB.pitchRec+offsetRec];
                }
            }
            if(mB.mbSubId!=15) return;
        } else {
            for(k=0;k<16;k++){
                offsetDst = ((k&0x3) + (k>>2)*mB.pitchDst) << 2;
                offsetRec = ((k&0x3) + (k>>2)*mB.pitchRec) << 2;
                for(j=0;j<4;j++){
                    for(i=0;i<4;i++) {
                        mB.pYDst[i+j*mB.pitchDst+offsetDst] = mB.pYRec[i+j*mB.pitchRec+offsetRec];
                    }
                }
            }
        }
        for(k=0;k<4;k++){
            offsetDst = ((k&0x1) + (k>>1)*mB.pitchDst) << 2;
            offsetRec = ((k&0x1) + (k>>1)*mB.pitchRec) << 2;
            for(j=0;j<4;j++){
                for(i=0;i<4;i++) {
                    mB.pUDst[i+j*mB.pitchDst+offsetDst] = mB.pURec[i+j*mB.pitchRec+offsetRec];
                }
            }
            for(j=0;j<4;j++){
                for(i=0;i<4;i++) {
                    mB.pVDst[i+j*mB.pitchDst+offsetDst] = mB.pVRec[i+j*mB.pitchRec+offsetRec];
                }
            }
        }
    }

#define ABS(x) ((x < 0) ? -(x) : (x))

    const U8 vp8_hev_thresh_lut[2][64] = 
    {
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
        },
        {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
                3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
            }
    };

    I32 hev(U8 threshold, U8 *ptr, I32 step)
    {
        I32 p0, p1;
        I32 q0, q1;

        p0 = ptr[-1*step]; p1 = ptr[-2*step];
        q0 = ptr[0*step];  q1 = ptr[1*step];

        return ABS(p1 - p0) > threshold || ABS(q1 - q0) > threshold;
    }


    I32 do_filter_simple(U8 E, U8 *ptr, I32 step)
    {
        I32 p0, p1;
        I32 q0, q1;

        p0 = ptr[-1*step]; p1 = ptr[-2*step];
        q0 = ptr[0*step];  q1 = ptr[1*step];

        return (ABS(p0 - q0)*2 + (ABS(p1 - q1) >> 1)) <= E;
    }

    I32 do_filter_normal(U8 I, U8 E, U8 *ptr, I32 step)
    {
        I32 p0, p1, p2, p3;
        I32 q0, q1, q2 ,q3;

        p0 = ptr[-1*step]; p1 = ptr[-2*step]; p2 = ptr[-3*step]; p3 = ptr[-4*step];
        q0 = ptr[0*step];  q1 = ptr[1*step];  q2 = ptr[2*step];  q3 = ptr[3*step];

        return (ABS(p0 - q0)*2 + (ABS(p1 - q1) >> 1)) <= E &&
            ABS(p3 - p2) <= I &&
            ABS(p2 - p1) <= I &&
            ABS(p1 - p0) <= I &&
            ABS(q3 - q2) <= I &&
            ABS(q2 - q1) <= I &&
            ABS(q1 - q0) <= I;
    }

    void common_adjust(U8 use_outer_taps, U8 *ptr, I32 step)
    {
        I32 a = 0, t = 0, F0 = 0, F1 = 0;
        I32 p0, p1, q0, q1;

        p0 = ptr[-1*step]; p1 = ptr[-2*step];
        q0 = ptr[0*step];  q1 = ptr[1*step];

        a = 3*(q0 - p0);

        if(use_outer_taps)
            t = vp8_clamp8s(p1 - q1);

        a = vp8_clamp8s(a + t);

        F0 = a + 4;
        F1 = a + 3;
        F0 = (F0 < -128)? -128 : (F0 > 127) ? 127 : F0;
        F1 = (F1 < -128)? -128 : (F1 > 127) ? 127 : F1;
        F0 >>= 3;
        F1 >>= 3;

        ptr[ 0]= vp8_clamp8u(q0 - F0);
        ptr[-1*step]= vp8_clamp8u(p0 + F1);

        if(!use_outer_taps)
        {
            a = (F0 + 1) >> 1;
            ptr[ 1*step]= vp8_clamp8u(q1 - a);
            ptr[-2*step]= vp8_clamp8u(p1 + a);
        }
    }

    void vp8_lf_mb_edges(U8 *ptr, I32 step)
    {
        I32 w = 0;
        I32 F0 = 0, F1 = 0, F2 = 0;
        I32 p0, p1, p2, p3;
        I32 q0, q1, q2 ,q3;

        p0 = ptr[-1*step]; p1 = ptr[-2*step]; p2 = ptr[-3*step]; p3 = ptr[-4*step];
        q0 = ptr[0*step];  q1 = ptr[1*step];  q2 = ptr[2*step];  q3 = ptr[3*step];

        w = vp8_clamp8s(vp8_clamp8s(p1 - q1) + 3*(q0 - p0));

        F0 = vp8_clamp8s((27*w + 63) >> 7);
        F1 = vp8_clamp8s((18*w + 63) >> 7);
        F2 = vp8_clamp8s((9*w + 63) >> 7);

        ptr[0*step]  = vp8_clamp8u(q0 - F0);
        ptr[-1*step] = vp8_clamp8u(p0 + F0);
        ptr[1*step]  = vp8_clamp8u(q1 - F1);
        ptr[-2*step] = vp8_clamp8u(p1 + F1);
        ptr[2*step]  = vp8_clamp8u(q2 - F2);
        ptr[-3*step] = vp8_clamp8u(p2 + F2);
    }

    void vp8_lf_normal_mb(U8 *dst, I32 step_x, I32 step_y, U8 hev_threshold, U8 interior_limit, U8 edge_limit, U8 loop_size)
    {
        I32 i = 0;

        for(i = 0; i < loop_size; i++)
            if(do_filter_normal(interior_limit, edge_limit, dst + i*step_y, step_x))
                if(hev(hev_threshold,  dst + i*step_y, step_x)) {
                    common_adjust(1, dst + i*step_y, step_x);
                } else {
                    vp8_lf_mb_edges(dst + i*step_y, step_x);
                }
    }

    void vp8_lf_normal_sb(U8 *dst, I32 step_x, I32 step_y, U8 hev_threshold, U8 interior_limit, U8 edge_limit, U8 loop_size)
    {
        I32 i = 0, hv = 0;

        for(i = 0; i < loop_size; i++)
            if(do_filter_normal(interior_limit, edge_limit, dst + i*step_y, step_x))
            {
                hv = hev(hev_threshold,  dst + i*step_y, step_x);
                if(!hv)
                    common_adjust(0, dst + i*step_y, step_x);
                else
                    common_adjust(1, dst + i*step_y, step_x);
            }
    }

    void vp8_loop_filter_normal_mb(U8 *dst_y, U8 *dst_u, U8 *dst_v, I32 step_x, I32 step_y, U8 filter_level, U8 interior_limit, U8 hev_threshold)
    {
        U8  mb_edge_limit = ((filter_level + 2) << 1) + interior_limit;

        vp8_lf_normal_mb(dst_y, step_x, step_y, hev_threshold, interior_limit, mb_edge_limit, 16);
        vp8_lf_normal_mb(dst_u, step_x, step_y, hev_threshold, interior_limit, mb_edge_limit, 8);
        vp8_lf_normal_mb(dst_v, step_x, step_y, hev_threshold, interior_limit, mb_edge_limit, 8);
    }

    void vp8_loop_filter_normal_sb(U8 *dst_y, U8 *dst_u, U8 *dst_v, I32 step_x, I32 step_y, U8 filter_level, U8 interior_limit, U8 hev_threshold)
    {
        U8 sb_edge_limit = (filter_level << 1) + interior_limit;

        vp8_lf_normal_sb(dst_y + 4*step_x,  step_x, step_y, hev_threshold, interior_limit, sb_edge_limit, 16);
        vp8_lf_normal_sb(dst_y + 8*step_x,  step_x, step_y, hev_threshold, interior_limit, sb_edge_limit, 16);
        vp8_lf_normal_sb(dst_y + 12*step_x, step_x, step_y, hev_threshold, interior_limit, sb_edge_limit, 16);
        vp8_lf_normal_sb(dst_u + 4*step_x,  step_x, step_y, hev_threshold, interior_limit, sb_edge_limit, 8);
        vp8_lf_normal_sb(dst_v + 4*step_x,  step_x, step_y, hev_threshold, interior_limit, sb_edge_limit, 8);
    }

    void vp8_loop_filter_simple(U8 *dst_y, I32 step_x, I32 step_y, U8 edge_limit)
    {
        I32 i = 0;

        for(i = 0; i < 16; i++)
            if(do_filter_simple(edge_limit, dst_y + i*step_y, step_x))
                common_adjust(1, dst_y + i*step_y, step_x);
    }

    void Vp8PakMfdLf::doProcess(LoopFilterIf &mB)
    {
        I32  pitch = mB.pitch;

        if(mB.LfType) {
            U8 mb_edge_limit = ((mB.LfLevel + 2) << 1) + mB.LfLimit;
            U8 sb_edge_limit = (mB.LfLevel << 1) + mB.LfLimit;

            if(mB.MbXcnt) // inter Mb VertEdges
                vp8_loop_filter_simple(mB.pYRec, 1, pitch, mb_edge_limit);

            if(mB.notSkipBlockFilter) // inter subBlock VertEdges
            {
                vp8_loop_filter_simple(mB.pYRec + 4,  1, pitch, sb_edge_limit);
                vp8_loop_filter_simple(mB.pYRec + 8,  1, pitch, sb_edge_limit);
                vp8_loop_filter_simple(mB.pYRec + 12, 1, pitch, sb_edge_limit);
            }

            if(mB.MbYcnt) // inter Mb HorEdges
                vp8_loop_filter_simple(mB.pYRec, pitch, 1, mb_edge_limit);

            if(mB.notSkipBlockFilter) // inter subBlock HorEdges
            {
                vp8_loop_filter_simple(mB.pYRec + 4*pitch,  pitch, 1, sb_edge_limit);
                vp8_loop_filter_simple(mB.pYRec + 8*pitch,  pitch, 1, sb_edge_limit);
                vp8_loop_filter_simple(mB.pYRec + 12*pitch, pitch, 1, sb_edge_limit);
            }
        } else {
            if(mB.MbXcnt) // inter Mb VertEdges
                vp8_loop_filter_normal_mb(mB.pYRec, mB.pURec, mB.pVRec, 1, pitch, mB.LfLevel, mB.LfLimit,
                vp8_hev_thresh_lut[mB.fType][mB.LfLevel]);

            if(mB.notSkipBlockFilter) // inter subBlock VertEdges
                vp8_loop_filter_normal_sb(mB.pYRec, mB.pURec, mB.pVRec, 1, pitch, mB.LfLevel, mB.LfLimit,
                vp8_hev_thresh_lut[mB.fType][mB.LfLevel]);

            if(mB.MbYcnt) // inter Mb HorEdges
                vp8_loop_filter_normal_mb(mB.pYRec, mB.pURec, mB.pVRec, pitch, 1, mB.LfLevel, mB.LfLimit,
                vp8_hev_thresh_lut[mB.fType][mB.LfLevel]);

            if(mB.notSkipBlockFilter) // inter subBlock VertEdges
                vp8_loop_filter_normal_sb(mB.pYRec, mB.pURec, mB.pVRec, pitch, 1, mB.LfLevel, mB.LfLimit,
                vp8_hev_thresh_lut[mB.fType][mB.LfLevel]);
        }
    }

    void vp8_intra_predict16x16(PixType* pSrcDst, U32 Step, U8 bMode, U8* above, U8* left, U8 ltp, U8 haveUp, U8 haveLeft)
    {
        U32 i  = 0, j  = 0, sh = 3;

        switch(bMode)
        {
        case VP8_MB_DC_PRED:
            {
                U16 average = 0;
                U16 dcVal;

                if(haveUp)
                    for(i = 0; i < 16; i++) average += above[i];
                if(haveLeft)
                    for(i = 0; i < 16; i++) average += left[i];
                if(!haveLeft && !haveUp) {
                    dcVal = 128;
                } else {
                    sh   += (haveLeft + haveUp);
                    dcVal = (average + (1 << (sh - 1))) >> sh;
                }

                for(i = 0; i < 16; i++, pSrcDst+=Step)
                    for(j = 0; j < 16; j++)
                        pSrcDst[j] = vp8_clamp8u(dcVal);
            }
            break;

        case VP8_MB_V_PRED:
            {
                for(i = 0; i < 16; i++, pSrcDst+=Step)
                    for(j = 0; j < 16; j++)
                        pSrcDst[j] = vp8_clamp8u(above[j]);
            }
            break;

        case VP8_MB_H_PRED:
            {
                for(i = 0; i < 16; i++, pSrcDst+=Step)
                    for(j = 0; j < 16; j++)
                        pSrcDst[j] = vp8_clamp8u(left[i]);
            }
            break;

        default:
        case VP8_MB_TM_PRED:
            {
                for(i = 0; i < 16; i++)
                    for(j = 0; j < 16; j++)
                        pSrcDst[i*Step + j] = vp8_clamp8u(above[j] + left[i] - ltp);
            }
            break;
        }
    }

    void vp8_intra_predict8x8(PixType* pSrcDst, U32 Step, U8 bMode, U8* above, U8* left, U8 ltp, U8 haveUp, U8 haveLeft)
    {
        U32   i = 0, j = 0, sh = 2;

        switch(bMode)
        {
        case VP8_MB_DC_PRED:
            {
                U16 average = 0;
                U16 dcVal;

                if(haveUp)
                    for(i = 0; i < 8; i++) average += above[i];
                if(haveLeft)
                    for(i = 0; i < 8; i++) average += left[i];
                if(!haveLeft && !haveUp) {
                    dcVal = 128;
                } else {
                    sh   += (haveLeft + haveUp);
                    dcVal = (average + (1 << (sh - 1))) >> sh;
                }

                for(i = 0; i < 8; i++, pSrcDst+=Step)
                    for(j = 0; j < 8; j++)
                        pSrcDst[j] = vp8_clamp8u(dcVal);
            }
            break;

        case VP8_MB_V_PRED:
            {
                for(i = 0; i < 8; i++, pSrcDst+=Step)
                    for(j = 0; j < 8; j++)
                        pSrcDst[j] = vp8_clamp8u(above[j]);
            }
            break;

        case VP8_MB_H_PRED:
            {
                for(i = 0; i < 8; i++, pSrcDst+=Step)
                    for(j = 0; j < 8; j++)
                        pSrcDst[j] = vp8_clamp8u(left[i]);
            }
            break;

        default:
        case VP8_MB_TM_PRED:
            {
                for(i = 0; i < 8; i++)
                    for(j = 0; j < 8; j++)
                        pSrcDst[i*Step + j] = vp8_clamp8u(above[j] + left[i] - ltp);
            }
            break;
        }
    }

    void vp8_intra_predict4x4(PixType* pSrcDst, U32 Step, U8 bMode, U8* above, U8* left, U8 ltp)
    {
        U32 i = 0, j = 0;
        U8  *a = above, *l = left;
        I16 v1, v2, v3, v4, v5, v6 ,v7, v8, v9, v10;

        switch(bMode)
        {
        case VP8_B_DC_PRED:
            {
                U32 v = 4;

                for(i = 0; i < 4; i++)
                    v += l[i] + a[i];
                v = vp8_clamp8u(v >> 3);

                for(i = 0; i < 4; i++, pSrcDst+=Step)
                    pSrcDst[0] = pSrcDst[1] = pSrcDst[2] = pSrcDst[3] = v;
            }
            break;

        case VP8_B_VE_PRED:
            {
                v1 = vp8_clamp8u((ltp  + 2*a[0] + a[1] + 2) >> 2);
                v2 = vp8_clamp8u((a[0] + 2*a[1] + a[2] + 2) >> 2);
                v3 = vp8_clamp8u((a[1] + 2*a[2] + a[3] + 2) >> 2);
                v4 = vp8_clamp8u((a[2] + 2*a[3] + a[4] + 2) >> 2);

                for(i = 0; i < 4; i++) {
                    pSrcDst[0 + i*Step] = (U8)v1;
                    pSrcDst[1 + i*Step] = (U8)v2;
                    pSrcDst[2 + i*Step] = (U8)v3;
                    pSrcDst[3 + i*Step] = (U8)v4;
                }
            }
            break;

        case VP8_B_HE_PRED:
            {
                v1 = vp8_clamp8u((ltp  + 2*l[0] + l[1] + 2) >> 2);
                v2 = vp8_clamp8u((l[0] + 2*l[1] + l[2] + 2) >> 2);
                v3 = vp8_clamp8u((l[1] + 2*l[2] + l[3] + 2) >> 2);
                v4 = vp8_clamp8u((l[2] + 2*l[3] + l[3] + 2) >> 2);

                for(i = 0; i < 4; i++) {
                    pSrcDst[i + 0*Step] = (U8)v1;
                    pSrcDst[i + 1*Step] = (U8)v2;
                    pSrcDst[i + 2*Step] = (U8)v3;
                    pSrcDst[i + 3*Step] = (U8)v4;
                }
            }
            break;

        case VP8_B_TM_PRED:
            {
                for(i = 0; i < 4; i++)
                    for(j = 0; j < 4; j++)
                        pSrcDst[i*Step + j] = vp8_clamp8u(l[i] + a[j] - ltp);
            }
            break;

        case VP8_B_LD_PRED:
            {
                v1 = (a[0] + 2*a[1] + a[2] + 2) >> 2;
                v2 = (a[1] + 2*a[2] + a[3] + 2) >> 2;
                v3 = (a[2] + 2*a[3] + a[4] + 2) >> 2;
                v4 = (a[3] + 2*a[4] + a[5] + 2) >> 2;
                v5 = (a[4] + 2*a[5] + a[6] + 2) >> 2;
                v6 = (a[5] + 2*a[6] + a[7] + 2) >> 2;
                v7 = (a[6] + 2*a[7] + a[7] + 2) >> 2;

                pSrcDst[0 + 0*Step] = vp8_clamp8u(v1);

                pSrcDst[1 + 0*Step] = vp8_clamp8u(v2);
                pSrcDst[0 + 1*Step] = vp8_clamp8u(v2);

                pSrcDst[2 + 0*Step] = vp8_clamp8u(v3);
                pSrcDst[1 + 1*Step] = vp8_clamp8u(v3);
                pSrcDst[0 + 2*Step] = vp8_clamp8u(v3);

                pSrcDst[3 + 0*Step] = vp8_clamp8u(v4);
                pSrcDst[2 + 1*Step] = vp8_clamp8u(v4);
                pSrcDst[1 + 2*Step] = vp8_clamp8u(v4);
                pSrcDst[0 + 3*Step] = vp8_clamp8u(v4);

                pSrcDst[3 + 1*Step] = vp8_clamp8u(v5);
                pSrcDst[2 + 2*Step] = vp8_clamp8u(v5);
                pSrcDst[1 + 3*Step] = vp8_clamp8u(v5);

                pSrcDst[3 + 2*Step] = vp8_clamp8u(v6);
                pSrcDst[2 + 3*Step] = vp8_clamp8u(v6);

                pSrcDst[3 + 3*Step] = vp8_clamp8u(v7);
            }
            break;

        case VP8_B_RD_PRED:
            {
                v1 = (l[3] + 2*l[2] + l[1] + 2) >> 2;
                v2 = (l[2] + 2*l[1] + l[0] + 2) >> 2;
                v3 = (l[1] + 2*l[0] + ltp  + 2) >> 2;
                v4 = (l[0] + 2*ltp  + a[0] + 2) >> 2;
                v5 = (ltp  + 2*a[0] + a[1] + 2) >> 2;
                v6 = (a[0] + 2*a[1] + a[2] + 2) >> 2;
                v7 = (a[1] + 2*a[2] + a[3] + 2) >> 2;

                pSrcDst[0 + 0*Step] = vp8_clamp8u(v4);
                pSrcDst[1 + 1*Step] = vp8_clamp8u(v4);
                pSrcDst[2 + 2*Step] = vp8_clamp8u(v4);
                pSrcDst[3 + 3*Step] = vp8_clamp8u(v4);

                pSrcDst[1 + 0*Step] = vp8_clamp8u(v5);
                pSrcDst[2 + 1*Step] = vp8_clamp8u(v5);
                pSrcDst[3 + 2*Step] = vp8_clamp8u(v5);

                pSrcDst[2 + 0*Step] = vp8_clamp8u(v6);
                pSrcDst[3 + 1*Step] = vp8_clamp8u(v6);

                pSrcDst[3 + 0*Step] = vp8_clamp8u(v7);

                pSrcDst[0 + 1*Step] = vp8_clamp8u(v3);
                pSrcDst[1 + 2*Step] = vp8_clamp8u(v3);
                pSrcDst[2 + 3*Step] = vp8_clamp8u(v3);

                pSrcDst[0 + 2*Step] = vp8_clamp8u(v2);
                pSrcDst[1 + 3*Step] = vp8_clamp8u(v2);

                pSrcDst[0 + 3*Step] = vp8_clamp8u(v1);
            }
            break;

        case VP8_B_VR_PRED:
            {
                v1 = (ltp  + a[0] + 1) >> 1;
                v2 = (a[0] + a[1] + 1) >> 1;
                v3 = (a[1] + a[2] + 1) >> 1;
                v4 = (a[2] + a[3] + 1) >> 1;

                v5  =(l[0] + 2*ltp  + a[0] + 2) >> 2;
                v6  =(ltp  + 2*a[0] + a[1] + 2) >> 2;
                v7  =(a[0] + 2*a[1] + a[2] + 2) >> 2;
                v8  =(a[1] + 2*a[2] + a[3] + 2) >> 2;
                v9  =(ltp  + 2*l[0] + l[1] + 2) >> 2;
                v10 =(l[0] + 2*l[1] + l[2] + 2) >> 2;

                pSrcDst[0 + 0*Step] = vp8_clamp8u(v1);
                pSrcDst[1 + 2*Step] = vp8_clamp8u(v1);

                pSrcDst[1 + 0*Step] = vp8_clamp8u(v2);
                pSrcDst[2 + 2*Step] = vp8_clamp8u(v2);

                pSrcDst[2 + 0*Step] = vp8_clamp8u(v3);
                pSrcDst[3 + 2*Step] = vp8_clamp8u(v3);

                pSrcDst[3 + 0*Step] = vp8_clamp8u(v4);

                pSrcDst[0 + 1*Step] = vp8_clamp8u(v5);
                pSrcDst[1 + 3*Step] = vp8_clamp8u(v5);

                pSrcDst[1 + 1*Step] = vp8_clamp8u(v6);
                pSrcDst[2 + 3*Step] = vp8_clamp8u(v6);

                pSrcDst[2 + 1*Step] = vp8_clamp8u(v7);
                pSrcDst[3 + 3*Step] = vp8_clamp8u(v7);

                pSrcDst[3 + 1*Step] = vp8_clamp8u(v8);

                pSrcDst[0 + 2*Step] = vp8_clamp8u(v9);

                pSrcDst[0 + 3*Step] = vp8_clamp8u(v10);
            }
            break;

        case VP8_B_VL_PRED:
            {
                v1 = (a[0]  + a[1] + 1) >> 1;
                v2 = (a[1]  + a[2] + 1) >> 1;

                v3 = (a[2]  + a[3] + 1) >> 1;

                v4 = (a[3]  + a[4] + 1) >> 1;
                v5 = (a[4]  + 2*a[5] + a[6] + 2) >> 2;

                v6   = (a[0] + 2*a[1]  + a[2] + 2) >> 2;
                v7   = (a[1] + 2*a[2]  + a[3] + 2) >> 2;
                v8   = (a[2] + 2*a[3]  + a[4] + 2) >> 2;
                v9   = (a[3] + 2*a[4]  + a[5] + 2) >> 2;
                v10  = (a[5] + 2*a[6]  + a[7] + 2) >> 2;

                pSrcDst[0 + 0*Step] = vp8_clamp8u(v1);

                pSrcDst[1 + 0*Step] = vp8_clamp8u(v2);
                pSrcDst[0 + 2*Step] = vp8_clamp8u(v2);

                pSrcDst[2 + 0*Step] = vp8_clamp8u(v3);
                pSrcDst[1 + 2*Step] = vp8_clamp8u(v3);

                pSrcDst[3 + 0*Step] = vp8_clamp8u(v4);
                pSrcDst[2 + 2*Step] = vp8_clamp8u(v4);

                pSrcDst[0 + 1*Step] = vp8_clamp8u(v6);

                pSrcDst[1 + 1*Step] = vp8_clamp8u(v7);
                pSrcDst[0 + 3*Step] = vp8_clamp8u(v7);

                pSrcDst[2 + 1*Step] = vp8_clamp8u(v8);
                pSrcDst[1 + 3*Step] = vp8_clamp8u(v8);


                pSrcDst[3 + 1*Step] = vp8_clamp8u(v9);
                pSrcDst[2 + 3*Step] = vp8_clamp8u(v9);

                pSrcDst[3 + 2*Step] = vp8_clamp8u(v5);

                pSrcDst[3 + 3*Step] = vp8_clamp8u(v10);
            }
            break;

        case VP8_B_HD_PRED:
            {
                v1 = (ltp  + l[0] + 1) >> 1;
                v5 = (l[0] + l[1] + 1) >> 1;
                v7 = (l[1] + l[2] + 1) >> 1;
                v9 = (l[2] + l[3] + 1) >> 1;

                v2  = (l[0] + 2*ltp  + a[0] + 2) >> 2;
                v3  = (ltp  + 2*a[0] + a[1] + 2) >> 2;
                v4  = (a[0] + 2*a[1] + a[2] + 2) >> 2;
                v6  = (ltp  + 2*l[0] + l[1] + 2) >> 2;
                v8  = (l[0] + 2*l[1] + l[2] + 2) >> 2;
                v10 = (l[1] + 2*l[2] + l[3] + 2) >> 2;

                pSrcDst[0 + 0*Step] = vp8_clamp8u(v1);
                pSrcDst[2 + 1*Step] = vp8_clamp8u(v1);

                pSrcDst[1 + 0*Step] = vp8_clamp8u(v2);
                pSrcDst[3 + 1*Step] = vp8_clamp8u(v2);

                pSrcDst[2 + 0*Step] = vp8_clamp8u(v3);

                pSrcDst[3 + 0*Step] = vp8_clamp8u(v4);

                pSrcDst[0 + 1*Step] = vp8_clamp8u(v5);
                pSrcDst[2 + 2*Step] = vp8_clamp8u(v5);

                pSrcDst[1 + 1*Step] = vp8_clamp8u(v6);
                pSrcDst[3 + 2*Step] = vp8_clamp8u(v6);

                pSrcDst[0 + 2*Step] = vp8_clamp8u(v7);
                pSrcDst[2 + 3*Step] = vp8_clamp8u(v7);

                pSrcDst[1 + 2*Step] = vp8_clamp8u(v8);
                pSrcDst[3 + 3*Step] = vp8_clamp8u(v8);

                pSrcDst[0 + 3*Step] = vp8_clamp8u(v9);

                pSrcDst[1 + 3*Step] = vp8_clamp8u(v10);
            }
            break;

        default:
        case VP8_B_HU_PRED:
            {
                v1 = (l[0]  + l[1] + 1) >> 1;
                v3 = (l[1]  + l[2] + 1) >> 1;
                v5 = (l[2]  + l[3] + 1) >> 1;

                v2  = (l[0] + 2*l[1]  + l[2] + 2) >> 2;
                v4  = (l[1] + 2*l[2]  + l[3] + 2) >> 2;
                v6  = (l[2] + 3*l[3]         + 2) >> 2;

                v7 = l[3];

                pSrcDst[0 + 0*Step] = vp8_clamp8u(v1);

                pSrcDst[1 + 0*Step] = vp8_clamp8u(v2);

                pSrcDst[2 + 0*Step] = vp8_clamp8u(v3);
                pSrcDst[0 + 1*Step] = vp8_clamp8u(v3);

                pSrcDst[3 + 0*Step] = vp8_clamp8u(v4);
                pSrcDst[1 + 1*Step] = vp8_clamp8u(v4);

                pSrcDst[2 + 1*Step] = vp8_clamp8u(v5);
                pSrcDst[0 + 2*Step] = vp8_clamp8u(v5);

                pSrcDst[3 + 1*Step] = vp8_clamp8u(v6);
                pSrcDst[1 + 2*Step] = vp8_clamp8u(v6);

                pSrcDst[2 + 2*Step] = vp8_clamp8u(v7);
                pSrcDst[3 + 2*Step] = vp8_clamp8u(v7);
                pSrcDst[0 + 3*Step] = vp8_clamp8u(v7);
                pSrcDst[1 + 3*Step] = vp8_clamp8u(v7);
                pSrcDst[2 + 3*Step] = vp8_clamp8u(v7);
                pSrcDst[3 + 3*Step] = vp8_clamp8u(v7);
            }
            break;
        }
    }

    void PredictIntraMB(PredictionIf &mB)
    {
        I32      i, j;

        PixType  ltp, *aptr, *ptr, left[16], above[16];

        U8       haveLeft = (mB.MbXcnt == 0) ? 0 : 1;
        U8       haveUp   = (mB.MbYcnt == 0) ? 0 : 1;

        switch(mB.PredictionCtrl.MbYIntraMode)
        {
        case VP8_MB_DC_PRED:
        case VP8_MB_V_PRED:
        case VP8_MB_H_PRED:
        case VP8_MB_TM_PRED:
            memcpy(left, mB.lRow[0], 16*sizeof(PixType));
            ltp = left[15]; left[15] = (haveLeft) ? mB.aRow[0][-1] : 129;
            vp8_intra_predict16x16(mB.pYRec, mB.pitchRec, mB.PredictionCtrl.MbYIntraMode, mB.aRow[0], left, ltp, haveUp, haveLeft);
            break;
        case VP8_MB_B_PRED:
            j = mB.PredictionCtrl.subBlockId >> 2;
            i = mB.PredictionCtrl.subBlockId & 0x3;
            ptr = mB.pYRec + ((mB.pitchRec*j+i) << 2);

            aptr = mB.aRow[0]+(i<<2);
            if(!mB.PredictionCtrl.notLastInRow && i==3){
                above[0] = aptr[0]; above[1] = aptr[1]; above[2] = aptr[2]; above[3] = aptr[3];
                above[4] = above[5] = above[6] = above[7] = (j) ? mB.lRow[0][15] : mB.aRow[0][15];
                aptr = above;
            }

            left[0] = mB.lRow[0][(j<<2)+0];
            left[1] = mB.lRow[0][(j<<2)+1];
            left[2] = mB.lRow[0][(j<<2)+2];
            left[3] = (i) ? mB.aRow[0][(i<<2)-1] : (haveLeft) ? (j==3) ? mB.aRow[0][(i<<2)-1] : mB.lRow[0][(j<<2)+3] :129;

            ltp = (j) ? mB.lRow[0][(j<<2)-1] : mB.lRow[0][15];

            vp8_intra_predict4x4(ptr, mB.pitchRec, mB.PredictionCtrl.Mb4x4YIntraModes[mB.PredictionCtrl.subBlockId], aptr, left, ltp);
            break;
        }

        if(mB.PredictionCtrl.MbYIntraMode!=VP8_MB_B_PRED || mB.PredictionCtrl.subBlockId==15)
        {
            memcpy(left, mB.lRow[1], 8*sizeof(PixType));
            ltp = left[7]; left[7] = (haveLeft) ? mB.aRow[1][-1] : 129;
            vp8_intra_predict8x8(mB.pURec, mB.pitchRec, mB.PredictionCtrl.MbUVIntraMode, mB.aRow[1], left, ltp, haveUp, haveLeft);
            memcpy(left, mB.lRow[2], 8*sizeof(PixType));
            ltp = left[7]; left[7] = (haveLeft) ? mB.aRow[2][-1] : 129;
            vp8_intra_predict8x8(mB.pVRec, mB.pitchRec, mB.PredictionCtrl.MbUVIntraMode, mB.aRow[2], left, ltp, haveUp, haveLeft);
        }
    }

    const I16 vp8_6tap_filters[8][6] =
    {
        { 0,  0,  128,    0,   0,  0 },
        { 0, -6,  123,   12,  -1,  0 },
        { 2, -11, 108,   36,  -8,  1 },
        { 0, -9,   93,   50,  -6,  0 },
        { 3, -16,  77,   77, -16,  3 },
        { 0, -6,   50,   93,  -9,  0 },
        { 1, -8,   36,  108, -11,  2 },
        { 0, -1,   12,  123,  -6,  0 },
    };

    const I16 vp8_bilinear_filters[8][2] =
    {
        { 128,   0 },
        { 112,  16 },
        {  96,  32 },
        {  80,  48 },
        {  64,  64 },
        {  48,  80 },
        {  32,  96 },
        {  16, 112 }
    };

    const U8 vp8_filter_hv[8] = {0, 1, 1, 1, 1, 1, 1, 1};

    static void vp8_inter_predict_6tap_h(PixType *pSrc, I32 srcStep, PixType *pDst, I32 dstStep, U8 size_h, U8 size_v, U8 hf_indx, U8 /*vf_indx*/)
    {
        I32 i, j;
        I16 val;

        const I16 *filter = vp8_6tap_filters[hf_indx];

        for(i = 0; i < size_v; i++, pSrc+=srcStep, pDst+=dstStep)
            for(j = 0; j < size_h; j++)
            {
                val = (I16)((pSrc[j-2] * filter[0] +
                    pSrc[j-1] * filter[1] +
                    pSrc[j+0] * filter[2] +
                    pSrc[j+1] * filter[3] +
                    pSrc[j+2] * filter[4] +
                    pSrc[j+3] * filter[5] + 64) >> 7);
                pDst[j] = vp8_clamp8u(val);
            }
    }

    static void vp8_inter_predict_6tap_v(PixType *pSrc, I32 srcStep, PixType *pDst, I32 dstStep, U8 size_h, U8 size_v, U8 /*hf_indx*/, U8 vf_indx)
    {
        I32 i, j;
        I16 val;
        const I16* filter = vp8_6tap_filters[vf_indx];

        for(i = 0; i < size_v; i++, pSrc+=srcStep, pDst+=dstStep)
            for(j = 0; j < size_h; j++)
            {
                val = (I16)((pSrc[j-2*srcStep] * filter[0] +
                    pSrc[j-1*srcStep] * filter[1] +
                    pSrc[j+0*srcStep] * filter[2] +
                    pSrc[j+1*srcStep] * filter[3] +
                    pSrc[j+2*srcStep] * filter[4] +
                    pSrc[j+3*srcStep] * filter[5] + 64) >> 7);
                pDst[j] = vp8_clamp8u(val);
            }
    }

    static void vp8_inter_predict_6tap_hv(PixType *pSrc, I32 srcStep, PixType *pDst, I32 dstStep, U8 size_h, U8 size_v, U8 hf_indx, U8 vf_indx)
    {
        I32  i, j;
        I16  val;
        const I16* filter_h = vp8_6tap_filters[hf_indx];
        const I16* filter_v = vp8_6tap_filters[vf_indx];

        PixType tmp[21*24];
        PixType *tmp_ptr = tmp;

        pSrc -= 2*srcStep;

        for(i = 0; i < size_v+5; i++)
        {
            for(j = 0; j < size_h; j++)
            {
                val = (I16)((pSrc[j-2] * filter_h[0] +
                    pSrc[j-1] * filter_h[1] +
                    pSrc[j+0] * filter_h[2] +
                    pSrc[j+1] * filter_h[3] +
                    pSrc[j+2] * filter_h[4] +
                    pSrc[j+3] * filter_h[5] + 64) >> 7);
                tmp_ptr[j] = vp8_clamp8u(val);
            }
            pSrc  += srcStep;
            tmp_ptr += size_h;
        }

        tmp_ptr = tmp + 2*size_h;

        for(i = 0; i < size_v; i++)
        {
            for(j = 0; j < size_h; j++)
            {
                val = (I16)((tmp_ptr[j-2*size_h] * filter_v[0] +
                    tmp_ptr[j-1*size_h] * filter_v[1] +
                    tmp_ptr[j+0*size_h] * filter_v[2] +
                    tmp_ptr[j+1*size_h] * filter_v[3] +
                    tmp_ptr[j+2*size_h] * filter_v[4] +
                    tmp_ptr[j+3*size_h] * filter_v[5] + 64) >> 7);
                pDst[j] = vp8_clamp8u(val);
            }
            tmp_ptr += size_h;
            pDst     += dstStep;
        }
    }

    static void vp8_inter_predict_bilinear_h(PixType *pSrc, I32 srcStep, PixType *pDst, I32 dstStep, U8 size_h, U8 size_v, U8 hf_indx, U8 /*vf_indx*/)
    {
        I32 i, j;
        I16 val;
        const I16* filter = vp8_bilinear_filters[hf_indx];

        for(i = 0; i < size_v; i++, pSrc+=srcStep, pDst+=dstStep)
            for(j = 0; j < size_h; j++)
            {
                val = (I16)((pSrc[j+0] * filter[0] + pSrc[j+1] * filter[1] + 64) >> 7);
                pDst[j] = vp8_clamp8u(val);
            }
    }

    static void vp8_inter_predict_bilinear_v(PixType *pSrc, I32 srcStep, PixType *pDst, I32 dstStep, U8 size_h, U8 size_v, U8 /*hf_indx*/, U8 vf_indx)
    {
        I32 i, j;
        I16 val;
        const I16* filter = vp8_bilinear_filters[vf_indx];

        for(i = 0; i < size_v; i++, pSrc+=srcStep, pDst+=dstStep)
            for(j = 0; j < size_h; j++)
            {
                val = (I16)((pSrc[j+0*srcStep] * filter[0] + pSrc[j+1*srcStep] * filter[1] + 64) >> 7);
                pDst[j] = vp8_clamp8u(val);
            }
    }

    static void vp8_inter_predict_bilinear_hv(PixType *pSrc, I32 srcStep, PixType *pDst, I32 dstStep, U8 size_h, U8 size_v, U8 hf_indx, U8 vf_indx)
    {
        I32 i, j;
        I16 val;
        const I16* filter_h = vp8_bilinear_filters[hf_indx];
        const I16* filter_v = vp8_bilinear_filters[vf_indx];

        PixType  tmp[17*16];
        PixType *tmp_ptr = tmp;

        for(i = 0; i < size_v+1; i++)
        {
            for(j = 0; j < size_h; j++)
            {
                val = (I16)((pSrc[j+0] * filter_h[0] + pSrc[j+1] * filter_h[1] + 64) >> 7);
                tmp_ptr[j] = vp8_clamp8u(val);
            }
            pSrc  += srcStep;
            tmp_ptr += size_h;
        }

        tmp_ptr = tmp;

        for(i = 0; i < size_v; i++)
        {
            for(j = 0; j < size_h; j++)
            {
                val = (I16)((tmp_ptr[j+0*size_h] * filter_v[0] + tmp_ptr[j+1*size_h] * filter_v[1] + 64) >> 7);
                pDst[j] = vp8_clamp8u(val);
            }
            tmp_ptr += size_h;
            pDst    += dstStep;
        }
    }

    static void vp8_inter_copy(PixType *pSrc, I32 srcStep, PixType *pDst, I32 dstStep, U8 size_h, U8 size_v, U8 /*hf_indx*/, U8 /*vf_indx*/)
    {
        I32 i, j;

        for(i = 0; i < size_v; i++, pSrc+=srcStep, pDst+=dstStep)
            for(j = 0; j < size_h; j += 4)
            {
                pDst[j + 0] = pSrc[j + 0];
                pDst[j + 1] = pSrc[j + 1];
                pDst[j + 2] = pSrc[j + 2];
                pDst[j + 3] = pSrc[j + 3];
            }
    }

    typedef void (*vp8_inter_predict_fptr)(PixType *pSrc, I32 srcStep, PixType *pDst, I32 dstStep, U8 size_h, U8 size_v, U8 hf_indx, U8 vf_indx);

    vp8_inter_predict_fptr vp8_inter_predict_fun[2][2][2] = /*6T/B | h |v */
    {
        {// 6TAP functions
            { vp8_inter_copy,           vp8_inter_predict_6tap_v  },
            { vp8_inter_predict_6tap_h, vp8_inter_predict_6tap_hv }
        },
        {// bilinear func`s
            { vp8_inter_copy,               vp8_inter_predict_bilinear_v  },
            { vp8_inter_predict_bilinear_h, vp8_inter_predict_bilinear_hv }
        }
    };

    inline void evalSubMbUVMv(PredictionIf &mB, U8 block, I16 &vx, I16 &vy)
    {
        I32 tvx = mB.PredictionCtrl.Mb4x4MVs[block+0].MV.mvx +
            mB.PredictionCtrl.Mb4x4MVs[block+1].MV.mvx +
            mB.PredictionCtrl.Mb4x4MVs[block+4].MV.mvx +
            mB.PredictionCtrl.Mb4x4MVs[block+5].MV.mvx;
        I32 tvy = mB.PredictionCtrl.Mb4x4MVs[block+0].MV.mvy +
            mB.PredictionCtrl.Mb4x4MVs[block+1].MV.mvy +
            mB.PredictionCtrl.Mb4x4MVs[block+4].MV.mvy +
            mB.PredictionCtrl.Mb4x4MVs[block+5].MV.mvy;
        tvx = (tvx<0) ? tvx-4 : tvx+4;
        tvy = (tvy<0) ? tvy-4 : tvy+4;
        vx = tvx / 8;
        vy = tvy / 8;
    }

    void PredictInterMB(PredictionIf &mB)
    {
        U8  ft = mB.PredictionCtrl.filterType & 1; //0 - 6TAP, 1 - bilinear
        vp8_inter_predict_fptr  pred_fn;
        I16 mv_x, mv_y;
        I32 i, j, offset, delta;

        if(mB.PredictionCtrl.MbInterMode != VP8_MV_SPLIT) {
            mv_x = mB.PredictionCtrl.MbMv.MV.mvx,
            mv_y = mB.PredictionCtrl.MbMv.MV.mvy;
            mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1],mB.PredictionCtrl.MvLimits[3]);
            mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0],mB.PredictionCtrl.MvLimits[2]);
            pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
            offset = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
            pred_fn(mB.PredictionCtrl.pYRef+offset, mB.PredictionCtrl.pitchRef, mB.pYRec, mB.pitchRec, 16, 16, mv_x&7, mv_y&7);

            mv_x = mB.PredictionCtrl.MbMv.MV.mvx,
            mv_y = mB.PredictionCtrl.MbMv.MV.mvy;
            mv_x = ((mv_x < 0) ? mv_x-1 : mv_x+1) / 2;
            mv_y = ((mv_y < 0) ? mv_y-1 : mv_y+1) / 2;
            if(mB.PredictionCtrl.filterType & 2) {
                mv_x = mv_x & (~7);
                mv_y = mv_y & (~7);
            }
            mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1]>>1,mB.PredictionCtrl.MvLimits[3]>>1);
            mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0]>>1,mB.PredictionCtrl.MvLimits[2]>>1);
            pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
            offset  = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
            pred_fn(mB.PredictionCtrl.pURef+offset, mB.PredictionCtrl.pitchRef, mB.pURec, mB.pitchRec, 8, 8, mv_x&7, mv_y&7);
            pred_fn(mB.PredictionCtrl.pVRef+offset, mB.PredictionCtrl.pitchRef, mB.pVRec, mB.pitchRec, 8, 8, mv_x&7, mv_y&7);
        } else {
            switch(mB.PredictionCtrl.MbSplitMode)
            {
            case VP8_MV_TOP_BOTTOM:
                {
                    mv_x = mB.PredictionCtrl.Mb4x4MVs[0].MV.mvx,
                    mv_y = mB.PredictionCtrl.Mb4x4MVs[0].MV.mvy;
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1],mB.PredictionCtrl.MvLimits[3]);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0],mB.PredictionCtrl.MvLimits[2]);
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    offset = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pYRef+offset, mB.PredictionCtrl.pitchRef, mB.pYRec, mB.pitchRec, 16, 8, mv_x&7, mv_y&7);

                    mv_x = mB.PredictionCtrl.Mb4x4MVs[8].MV.mvx,
                    mv_y = mB.PredictionCtrl.Mb4x4MVs[8].MV.mvy;
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1],mB.PredictionCtrl.MvLimits[3]);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0],mB.PredictionCtrl.MvLimits[2]);
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    offset = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pYRef+offset + (mB.PredictionCtrl.pitchRef<<3), mB.PredictionCtrl.pitchRef, mB.pYRec + (mB.pitchRec<<3), mB.pitchRec, 16, 8, mv_x&7, mv_y&7);

                    evalSubMbUVMv(mB, 0, mv_x, mv_y);
                    if(mB.PredictionCtrl.filterType & 2) {
                        mv_x = mv_x & (~7);
                        mv_y = mv_y & (~7);
                    }
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1]>>1,mB.PredictionCtrl.MvLimits[3]>>1);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0]>>1,mB.PredictionCtrl.MvLimits[2]>>1);
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    offset  = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pURef+offset, mB.PredictionCtrl.pitchRef, mB.pURec, mB.pitchRec, 8, 4, mv_x&7, mv_y&7);
                    pred_fn(mB.PredictionCtrl.pVRef+offset, mB.PredictionCtrl.pitchRef, mB.pVRec, mB.pitchRec, 8, 4, mv_x&7, mv_y&7);

                    evalSubMbUVMv(mB, 8, mv_x, mv_y);
                    if(mB.PredictionCtrl.filterType & 2) {
                        mv_x = mv_x & (~7);
                        mv_y = mv_y & (~7);
                    }
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1]>>1,mB.PredictionCtrl.MvLimits[3]>>1);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0]>>1,mB.PredictionCtrl.MvLimits[2]>>1);
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    offset  = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pURef+offset + (mB.PredictionCtrl.pitchRef<<2), mB.PredictionCtrl.pitchRef, mB.pURec + (mB.pitchRec<<2), mB.pitchRec, 8, 4, mv_x&7, mv_y&7);
                    pred_fn(mB.PredictionCtrl.pVRef+offset + (mB.PredictionCtrl.pitchRef<<2), mB.PredictionCtrl.pitchRef, mB.pVRec + (mB.pitchRec<<2), mB.pitchRec, 8, 4, mv_x&7, mv_y&7);
                }
                break;
            case VP8_MV_LEFT_RIGHT:
                {
                    mv_x = mB.PredictionCtrl.Mb4x4MVs[0].MV.mvx,
                    mv_y = mB.PredictionCtrl.Mb4x4MVs[0].MV.mvy;
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1],mB.PredictionCtrl.MvLimits[3]);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0],mB.PredictionCtrl.MvLimits[2]);
                    offset = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pYRef+offset, mB.PredictionCtrl.pitchRef, mB.pYRec, mB.pitchRec, 8, 16, mv_x&7, mv_y&7);

                    mv_x = mB.PredictionCtrl.Mb4x4MVs[2].MV.mvx,
                    mv_y = mB.PredictionCtrl.Mb4x4MVs[2].MV.mvy;
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1],mB.PredictionCtrl.MvLimits[3]);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0],mB.PredictionCtrl.MvLimits[2]);
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    offset = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pYRef+offset + 8, mB.PredictionCtrl.pitchRef, mB.pYRec + 8, mB.pitchRec, 8, 16, mv_x&7, mv_y&7);

                    evalSubMbUVMv(mB, 0, mv_x, mv_y);
                    if(mB.PredictionCtrl.filterType & 2) {
                        mv_x = mv_x & (~7);
                        mv_y = mv_y & (~7);
                    }
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1]>>1,mB.PredictionCtrl.MvLimits[3]>>1);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0]>>1,mB.PredictionCtrl.MvLimits[2]>>1);
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    offset  = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pURef+offset, mB.PredictionCtrl.pitchRef, mB.pURec, mB.pitchRec, 4, 8, mv_x&7, mv_y&7);
                    pred_fn(mB.PredictionCtrl.pVRef+offset, mB.PredictionCtrl.pitchRef, mB.pVRec, mB.pitchRec, 4, 8, mv_x&7, mv_y&7);

                    evalSubMbUVMv(mB, 2, mv_x, mv_y);
                    if(mB.PredictionCtrl.filterType & 2) {
                        mv_x = mv_x & (~7);
                        mv_y = mv_y & (~7);
                    }
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1]>>1,mB.PredictionCtrl.MvLimits[3]>>1);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0]>>1,mB.PredictionCtrl.MvLimits[2]>>1);
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    offset  = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pURef+offset + 4, mB.PredictionCtrl.pitchRef, mB.pURec + 4, mB.pitchRec, 4, 8, mv_x&7, mv_y&7);
                    pred_fn(mB.PredictionCtrl.pVRef+offset + 4, mB.PredictionCtrl.pitchRef, mB.pVRec + 4, mB.pitchRec, 4, 8, mv_x&7, mv_y&7);
                }
                break;
            case VP8_MV_QUARTERS:
                {
                    mv_x = mB.PredictionCtrl.Mb4x4MVs[0].MV.mvx,
                    mv_y = mB.PredictionCtrl.Mb4x4MVs[0].MV.mvy;
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1],mB.PredictionCtrl.MvLimits[3]);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0],mB.PredictionCtrl.MvLimits[2]);
                    offset = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pYRef+offset, mB.PredictionCtrl.pitchRef, mB.pYRec, mB.pitchRec, 8, 8, mv_x&7, mv_y&7);

                    mv_x = mB.PredictionCtrl.Mb4x4MVs[2].MV.mvx,
                    mv_y = mB.PredictionCtrl.Mb4x4MVs[2].MV.mvy;
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1],mB.PredictionCtrl.MvLimits[3]);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0],mB.PredictionCtrl.MvLimits[2]);
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    offset = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pYRef+offset + 8, mB.PredictionCtrl.pitchRef, mB.pYRec + 8, mB.pitchRec, 8, 8, mv_x&7, mv_y&7);

                    mv_x = mB.PredictionCtrl.Mb4x4MVs[8].MV.mvx,
                    mv_y = mB.PredictionCtrl.Mb4x4MVs[8].MV.mvy;
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1],mB.PredictionCtrl.MvLimits[3]);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0],mB.PredictionCtrl.MvLimits[2]);
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    offset = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pYRef+offset + (mB.PredictionCtrl.pitchRef<<3), mB.PredictionCtrl.pitchRef, mB.pYRec + (mB.pitchRec<<3), mB.pitchRec, 8, 8, mv_x&7, mv_y&7);

                    mv_x = mB.PredictionCtrl.Mb4x4MVs[10].MV.mvx,
                    mv_y = mB.PredictionCtrl.Mb4x4MVs[10].MV.mvy;
                    mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1],mB.PredictionCtrl.MvLimits[3]);
                    mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0],mB.PredictionCtrl.MvLimits[2]);
                    pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                    offset = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                    pred_fn(mB.PredictionCtrl.pYRef+offset + (mB.PredictionCtrl.pitchRef<<3) + 8, mB.PredictionCtrl.pitchRef, mB.pYRec + (mB.pitchRec<<3) + 8, mB.pitchRec, 8, 8, mv_x&7, mv_y&7);

                    for(j=0; j<2; j++)
                        for(i=0; i<2; i++) {
                            evalSubMbUVMv(mB, ((j<<2)+i)<<1, mv_x, mv_y);
                            if(mB.PredictionCtrl.filterType & 2) {
                                mv_x = mv_x & (~7);
                                mv_y = mv_y & (~7);
                            }
                            mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1]>>1,mB.PredictionCtrl.MvLimits[3]>>1);
                            mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0]>>1,mB.PredictionCtrl.MvLimits[2]>>1);
                            pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                            offset  = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                            delta = (j * mB.PredictionCtrl.pitchRef + i)<<2;
                            pred_fn(mB.PredictionCtrl.pURef+offset+delta, mB.PredictionCtrl.pitchRef, mB.pURec+((j * mB.pitchRec + i)<<2), mB.pitchRec, 4, 4, mv_x&7, mv_y&7);
                            pred_fn(mB.PredictionCtrl.pVRef+offset+delta, mB.PredictionCtrl.pitchRef, mB.pVRec+((j * mB.pitchRec + i)<<2), mB.pitchRec, 4, 4, mv_x&7, mv_y&7);
                        }
                }
                break;
            default:
            case VP8_MV_16:
                {
                    for(j=0; j<4; j++)
                        for(i=0; i<4; i++) {
                            mv_x = mB.PredictionCtrl.Mb4x4MVs[(j<<2)+i].MV.mvx,
                            mv_y = mB.PredictionCtrl.Mb4x4MVs[(j<<2)+i].MV.mvy;
                            mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1],mB.PredictionCtrl.MvLimits[3]);
                            mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0],mB.PredictionCtrl.MvLimits[2]);
                            pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                            offset = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                            delta = (j * mB.PredictionCtrl.pitchRef + i)<<2;
                            pred_fn(mB.PredictionCtrl.pYRef+offset+delta, mB.PredictionCtrl.pitchRef, mB.pYRec + ((j * mB.pitchRec + i)<<2), mB.pitchRec, 4, 4, mv_x&7, mv_y&7);
                        }
                    for(j=0; j<2; j++)
                        for(i=0; i<2; i++) {
                            evalSubMbUVMv(mB, ((j<<2)+i)<<1, mv_x, mv_y);
                            if(mB.PredictionCtrl.filterType & 2) {
                                mv_x = mv_x & (~7);
                                mv_y = mv_y & (~7);
                            }
                            mv_x = VP8_LIMIT(mv_x,mB.PredictionCtrl.MvLimits[1]>>1,mB.PredictionCtrl.MvLimits[3]>>1);
                            mv_y = VP8_LIMIT(mv_y,mB.PredictionCtrl.MvLimits[0]>>1,mB.PredictionCtrl.MvLimits[2]>>1);
                            pred_fn = vp8_inter_predict_fun[ft][vp8_filter_hv[mv_x&7]][vp8_filter_hv[mv_y&7]];
                            offset  = (mv_y>>3)* mB.PredictionCtrl.pitchRef + (mv_x>>3);
                            delta = (j * mB.PredictionCtrl.pitchRef + i)<<2;
                            pred_fn(mB.PredictionCtrl.pURef+offset+delta, mB.PredictionCtrl.pitchRef, mB.pURec+((j * mB.pitchRec + i)<<2), mB.pitchRec, 4, 4, mv_x&7, mv_y&7);
                            pred_fn(mB.PredictionCtrl.pVRef+offset+delta, mB.PredictionCtrl.pitchRef, mB.pVRec+((j * mB.pitchRec + i)<<2), mB.pitchRec, 4, 4, mv_x&7, mv_y&7);
                        }
                }
                break;
            }
        }
    }

    void Vp8PakMfdVp::doProcess(PredictionIf &mB)
    {
        if(mB.RefFrameMode!=VP8_INTRA_FRAME)
            PredictInterMB(mB);
        else
            PredictIntraMB(mB);
    }

    inline void quantize(I16 in, U16 shift, U16 qmul, U16 iqmul, U16 add, I16 *out, I16 *iout)
    {
        I32 val, sign, qval;

        val   = in;
        sign  = val >> 31;
        val   = (val^sign)-sign;
        qval  = ((val+add)*qmul) >> shift;
        qval  = (qval^sign)-sign;
        *out  = qval;
        *iout = qval*iqmul;
    }

    void Vp8PakMfdVq::doProcess(QrcIf &mB)
    {
        CoeffsType *ptr, *optr;

        U32     i, j, qplane;
        I32     add;
        U16     q_dc, q_ac, s_dc, s_ac, iq_dc, iq_ac;

        ptr   = mB.pInvTransformResult;
        optr  = mB.pmbCoeffs;

        qplane = VP8_Y_DC;

        q_dc  = m_VqState.fQuants[mB.segId].fwd_mul[VP8_Y_DC];//xzhang
        q_ac  = m_VqState.fQuants[mB.segId].fwd_mul[VP8_Y_AC];
        iq_dc = m_VqState.fQuants[mB.segId].inv_mul[VP8_Y_DC];
        iq_ac = m_VqState.fQuants[mB.segId].inv_mul[VP8_Y_AC];
        s_dc  = m_VqState.fQuants[mB.segId].rshift[VP8_Y_DC];
        s_ac  = m_VqState.fQuants[mB.segId].rshift[VP8_Y_AC];
        if((mB.mbType==VP8_MB_B_PRED)&&(mB.mbSubId>=0))
        {
            optr += 16*mB.mbSubId;
            ptr += 16*mB.mbSubId;
            quantize(*ptr, s_dc, q_dc, iq_dc, ((iq_dc*48)>>7)/*(iq_dc>>1)*/, optr, ptr);
            optr++; ptr++;
            add = ((48*iq_ac)>>7);
            for(i=1; i<16; i++) {
                quantize(*ptr, s_ac, q_ac, iq_ac, add, optr, ptr);
                optr++; ptr++;
            }
            if(mB.mbSubId!=15) return;
        } else {
            for(j=0; j<16; j++)
            {
                quantize(*ptr, s_dc, q_dc, iq_dc, ((iq_dc*48)>>7)/*(iq_dc>>1)*/, optr, ptr);
                optr++; ptr++;
                add = ((48*iq_ac)>>7);
                for(i=1; i<16; i++) {
                    quantize(*ptr, s_ac, q_ac, iq_ac, add, optr, ptr);
                    optr++; ptr++;
                }
            }
        }

        q_dc  = m_VqState.fQuants[mB.segId].fwd_mul[VP8_UV_DC];
        q_ac  = m_VqState.fQuants[mB.segId].fwd_mul[VP8_UV_AC];
        iq_dc = m_VqState.fQuants[mB.segId].inv_mul[VP8_UV_DC];
        iq_ac = m_VqState.fQuants[mB.segId].inv_mul[VP8_UV_AC];
        s_dc  = m_VqState.fQuants[mB.segId].rshift[VP8_UV_DC];
        s_ac  = m_VqState.fQuants[mB.segId].rshift[VP8_UV_AC];
        for(j=0; j<8; j++)
        {
            quantize(*ptr, s_dc, q_dc, iq_dc, ((48*iq_dc)>>7)/*(iq_dc>>1)*/, optr, ptr);
            optr++; ptr++;
            add = ((48*iq_ac)>>7);
            for(i=1; i<16; i++) {
                quantize(*ptr, s_ac, q_ac, iq_ac, add, optr, ptr);
                optr++; ptr++;
            }
        }

        if(mB.mbType!=VP8_MB_B_PRED)
        {
            optr  = mB.pY2mbCoeffs;
            qplane = VP8_Y2_DC;

            q_dc  = m_VqState.fQuants[mB.segId].fwd_mul[qplane];
            q_ac  = m_VqState.fQuants[mB.segId].fwd_mul[qplane-1];
            iq_dc = m_VqState.fQuants[mB.segId].inv_mul[qplane];
            iq_ac = m_VqState.fQuants[mB.segId].inv_mul[qplane-1];
            s_dc  = m_VqState.fQuants[mB.segId].rshift[qplane];
            s_ac  = m_VqState.fQuants[mB.segId].rshift[qplane-1];

            quantize(*ptr, s_dc, q_dc, iq_dc, ((48*iq_dc)>>7) /*(iq_dc>>1)*/, optr, ptr);
            optr++; ptr++;
            add = ((48*iq_ac)>>7);
            for(i=1; i<16; i++) {
                quantize(*ptr, s_ac, q_ac, iq_ac, add, optr, ptr);
                optr++; ptr++;
            }
        }
    }

    const I16 vp8_quant_dc[VP8_MAX_QP + 1 + 32] =
    {
        4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,
        4,   5,   6,   7,   8,   9,  10,  10,  11,  12,  13,  14,  15,  16,  17,  17,
        18,  19,  20,  20,  21,  21,  22,  22,  23,  23,  24,  25,  25,  26,  27,  28,
        29,  30,  31,  32,  33,  34,  35,  36,  37,  37,  38,  39,  40,  41,  42,  43,
        44,  45,  46,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,
        59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
        75,  76,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
        91,  93,  95,  96,  98, 100, 101, 102, 104, 106, 108, 110, 112, 114, 116, 118,
        122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 143, 145, 148, 151, 154, 157,
        157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157
    };

    const I16 vp8_quant_ac[VP8_MAX_QP + 1 + 32] =
    {
        4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,
        4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
        20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,
        36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,
        52,  53,  54,  55,  56,  57,  58,  60,  62,  64,  66,  68,  70,  72,  74,  76,
        78,  80,  82,  84,  86,  88,  90,  92,  94,  96,  98, 100, 102, 104, 106, 108,
        110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 146, 149, 152,
        155, 158, 161, 164, 167, 170, 173, 177, 181, 185, 189, 193, 197, 201, 205, 209,
        213, 217, 221, 225, 229, 234, 239, 245, 249, 254, 259, 264, 269, 274, 279, 284,
        284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284
    };

    const I16 vp8_quant_dc2[VP8_MAX_QP + 1 + 32] =
    {
        8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,
        8,  10,  12,  14,  16,  18,  20,  20,  22,  24,  26,  28,  30,  32,  34,  34,
        36,  38,  40,  40,  42,  42,  44,  44,  46,  46,  48,  50,  50,  52,  54,  56,
        58,  60,  62,  64,  66,  68,  70,  72,  74,  74,  76,  78,  80,  82,  84,  86,
        88,  90,  92,  92,  94,  96,  98, 100, 102, 104, 106, 108, 110, 112, 114, 116,
        118, 120, 122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 142, 144, 146, 148,
        150, 152, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178,
        182, 186, 190, 192, 196, 200, 202, 204, 208, 212, 216, 220, 224, 228, 232, 236,
        244, 248, 252, 256, 260, 264, 268, 272, 276, 280, 286, 290, 296, 302, 308, 314,
        314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314, 314
    };

    const I16 vp8_quant_ac2[VP8_MAX_QP + 1 + 32] =
    {
        8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   8,
        8,   8,   9,  10,  12,  13,  15,  17,  18,  20,  21,  23,  24,  26,  27,  29,
        31,  32,  34,  35,  37,  38,  40,  41,  43,  44,  46,  48,  49,  51,  52,  54,
        55,  57,  58,  60,  62,  63,  65,  66,  68,  69,  71,  72,  74,  75,  77,  79,
        80,  82,  83,  85,  86,  88,  89,  93,  96,  99, 102, 105, 108, 111, 114, 117,
        120, 124, 127, 130, 133, 136, 139, 142, 145, 148, 151, 155, 158, 161, 164, 167,
        170, 173, 176, 179, 184, 189, 193, 198, 203, 207, 212, 217, 221, 226, 230, 235,
        240, 244, 249, 254, 258, 263, 268, 274, 280, 286, 292, 299, 305, 311, 317, 323,
        330, 336, 342, 348, 354, 362, 370, 379, 385, 393, 401, 409, 416, 424, 432, 440,
        440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440, 440
    };

    const I16 vp8_quant_dc_uv[VP8_MAX_QP + 1 + 32] =
    {
        4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,   4,
        4,   5,   6,   7,   8,   9,  10,  10,  11,  12,  13,  14,  15,  16,  17,  17,
        18,  19,  20,  20,  21,  21,  22,  22,  23,  23,  24,  25,  25,  26,  27,  28,
        29,  30,  31,  32,  33,  34,  35,  36,  37,  37,  38,  39,  40,  41,  42,  43,
        44,  45,  46,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,
        59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
        75,  76,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
        91,  93,  95,  96,  98, 100, 101, 102, 104, 106, 108, 110, 112, 114, 116, 118,
        122, 124, 126, 128, 130, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,
        132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132
    };

    const I16 *vp8_quant_tables[6] = { vp8_quant_ac, vp8_quant_dc, vp8_quant_ac2, vp8_quant_dc2, vp8_quant_ac, vp8_quant_dc_uv };

    inline void eval_quant_params(I16 qi, U16 *fmul, U16 *imul, U16 *shift, const I16* table)
    {
        U32     t = 1<<16;

        t /= table[qi];

        *fmul = (U16)t; *imul = table[qi]; *shift = 16;
    }

    void InitializeQuantTable(Vp8FrameParamsEnc &m_ctrl, PakFrameInfVp8 &m_pakFrameState)
    {
        U32 i, j;
        I16 qidx, indexDelta[VP8_NUM_COEFF_QUANTIZERS];
        U16 fmul, imul, shift;

        indexDelta[0] = 0;
        for(i=1;i<VP8_NUM_COEFF_QUANTIZERS;i++)
            indexDelta[i] = m_ctrl.qDelta[i-1];

        for(i=0;i<VP8_MAX_NUM_OF_SEGMENTS;i++)
        {
            qidx = m_ctrl.qIndex;
            if(m_ctrl.SegOn) {
                if(m_ctrl.SegFeatMode)
                    qidx = m_ctrl.segFeatureData[0][i];//xzhang: check its interaction with MBRC
                else
                    qidx += m_ctrl.segFeatureData[0][i];
                qidx = (qidx<0)?0:(qidx>VP8_MAX_QP)?VP8_MAX_QP:qidx;
            }
            for(j=0;j<VP8_NUM_COEFF_QUANTIZERS;j++)
            {
                eval_quant_params(16+qidx+indexDelta[j], &fmul, &imul, &shift, vp8_quant_tables[j]);
                m_pakFrameState.fQuants[i].fwd_mul[j] = fmul;
                m_pakFrameState.fQuants[i].inv_mul[j] = imul;
                m_pakFrameState.fQuants[i].rshift[j] = shift;
            }
        }
    }

    const I8 vp8_mode_lf_deltas_lut[VP8_NUM_MV_MODES + 2] = { 2, 2, 1, 2, 3, 0, -1 };

    void InitializeLfTable(Vp8FrameParamsEnc &m_ctrl, PakFrameInfVp8 &m_pakFrameState)
    {
        U32 i, j, k;
        I32 filter_level   = 0;
        U8  interior_limit = 0;

        for(i=0; i<VP8_NUM_OF_REF_FRAMES; i++)
            for(j=0; j<VP8_NUM_MV_MODES+2; j++)
                for(k=0; k<VP8_MAX_NUM_OF_SEGMENTS; k++)
                {
                    filter_level = m_ctrl.LoopFilterLevel;
                    if(m_ctrl.SegOn) {
                        if(m_ctrl.SegFeatMode)
                            filter_level  = m_ctrl.segFeatureData[1][k];
                        else
                            filter_level += m_ctrl.segFeatureData[1][k];
                        filter_level = (filter_level<0)?0:(filter_level>VP8_MAX_LF_LEVEL)?VP8_MAX_LF_LEVEL:filter_level;
                    }

                    if(m_ctrl.LoopFilterAdjOn) {
                        filter_level += m_ctrl.refLFDeltas[i];
                        if(vp8_mode_lf_deltas_lut[j] >= 0)
                            filter_level += m_ctrl.modeLFDeltas[vp8_mode_lf_deltas_lut[j]];
                    }

                    filter_level = (filter_level<0)?0:(filter_level>VP8_MAX_LF_LEVEL)?VP8_MAX_LF_LEVEL:filter_level;

                    interior_limit = (U8)filter_level;
                    if(m_ctrl.SharpnessLevel)
                    {
                        interior_limit = (U8)(filter_level >> ((m_ctrl.SharpnessLevel > 4) ? 2 : 1));
                        if(interior_limit>(9 - m_ctrl.SharpnessLevel))
                            interior_limit = 9 - (U8)m_ctrl.SharpnessLevel;
                    }

                    if(!interior_limit)
                        interior_limit = 1;

                    m_pakFrameState.fLfLevel[i][j][k] = (U8)filter_level;
                    m_pakFrameState.fLfLimit[i][j][k] = interior_limit;
                }
    }

#define CHECK(cond) if (!(cond)) return MFX_ERR_UNDEFINED_BEHAVIOR;
#define CHECK_STS(sts) if ((sts) != UMC_OK) return MFX_ERR_UNDEFINED_BEHAVIOR;

    inline void AddSeqHeader(unsigned int width,
        unsigned int   height,
        unsigned int   FrameRateN,
        unsigned int   FrameRateD,
        unsigned char *pBitstream)
    {
        U32   ivf_file_header[8]  = {0x46494B44, 0x00200000, 0x30385056, width + (height << 16), FrameRateD<<1, FrameRateN, 0x00000000, 0x00000000};

        memcpy(pBitstream, ivf_file_header, sizeof (ivf_file_header));
    }

    inline void AddPictureHeader(unsigned char *pBitstream)
    {
        U32 ivf_frame_header[3] = {0x00000000, 0x00000000, 0x00000000};

        memcpy(pBitstream, ivf_frame_header, sizeof (ivf_frame_header));
    }

    inline void UpdatePictureHeader(unsigned int frameLen,
        unsigned int frameNum,
        unsigned char *pPictureHeader)
    {
        U32 ivf_frame_header[3] = {frameLen, frameNum<1, 0x00000000};

        memcpy(pPictureHeader, ivf_frame_header, sizeof (ivf_frame_header));
    }

    void Vp8CoreBSE::CopyInputFrame(mfxFrameData *iFrame)
    {
        U8   *pDst[3];
        I32   pitchDst[3];
        I32   pitch = iFrame->PitchLow + ((mfxU32)iFrame->PitchHigh << 16);
        IppiSize sz;

        sz.width = (m_Params.SrcWidth+1)&~1;
        sz.height = (m_Params.SrcHeight+1)&~1;

        pDst[0] = m_frames[m_frame_idx[VP8_ORIGINAL_FRAME]].data_y;
        pDst[1] = m_frames[m_frame_idx[VP8_ORIGINAL_FRAME]].data_u;
        pDst[2] = m_frames[m_frame_idx[VP8_ORIGINAL_FRAME]].data_v;
        pitchDst[0] = pitchDst[1] = pitchDst[2] = m_frames[m_frame_idx[VP8_ORIGINAL_FRAME]].pitch;

        ippiYCbCr420ToYCrCb420_8u_P2P3R(iFrame->Y, pitch, iFrame->UV, pitch, pDst, pitchDst, sz);
    }

    mfxStatus Vp8CoreBSE::RunPAK(bool bSeqHeader ,mfxBitstream * pBitstream, TaskHybridDDI *pTask, MBDATA_LAYOUT const & layout,VideoCORE * pCore
#if defined (VP8_HYBRID_DUMP_READ) || defined (VP8_HYBRID_DUMP_WRITE)
        , FILE* m_bse_dump, mfxU32 m_bse_dump_size
#endif
        )
    {
        mfxStatus   sts = MFX_ERR_NONE;
        mfxU8      *pPictureHeader = 0;
        mfxU32      hOffset = 12;

        int i;

        /*Find unused frame placeholder for current frame */
        for(i=0;i<VP8_NUM_OF_REF_FRAMES;i++) 
        {
            if(i==m_frame_idx[VP8_LAST_FRAME]) continue;
            if(i==m_frame_idx[VP8_GOLDEN_FRAME]) continue;
            if(i==m_frame_idx[VP8_ALTREF_FRAME]) continue;
            break;
        }
        m_frame_idx[VP8_INTRA_FRAME] = i;

        m_pakSurfaceState.CurSurfState.pYPlane              = m_frames[m_frame_idx[VP8_ORIGINAL_FRAME]].data_y;
        m_pakSurfaceState.CurSurfState.pUPlane              = m_frames[m_frame_idx[VP8_ORIGINAL_FRAME]].data_u;
        m_pakSurfaceState.CurSurfState.pVPlane              = m_frames[m_frame_idx[VP8_ORIGINAL_FRAME]].data_v;
        m_pakSurfaceState.CurSurfState.pitchPixels          = m_frames[m_frame_idx[VP8_ORIGINAL_FRAME]].pitch;
        m_pakSurfaceState.ReconSurfState.pYPlane            = m_frames[m_frame_idx[VP8_INTRA_FRAME]].data_y;
        m_pakSurfaceState.ReconSurfState.pUPlane            = m_frames[m_frame_idx[VP8_INTRA_FRAME]].data_u;
        m_pakSurfaceState.ReconSurfState.pVPlane            = m_frames[m_frame_idx[VP8_INTRA_FRAME]].data_v;
        m_pakSurfaceState.ReconSurfState.pitchPixels        = m_frames[m_frame_idx[VP8_INTRA_FRAME]].pitch;
        m_pakSurfaceState.LrfSurfState.pYPlane              = m_frames[m_frame_idx[VP8_LAST_FRAME]].data_y;
        m_pakSurfaceState.LrfSurfState.pUPlane              = m_frames[m_frame_idx[VP8_LAST_FRAME]].data_u;
        m_pakSurfaceState.LrfSurfState.pVPlane              = m_frames[m_frame_idx[VP8_LAST_FRAME]].data_v;
        m_pakSurfaceState.LrfSurfState.pitchPixels          = m_frames[m_frame_idx[VP8_LAST_FRAME]].pitch;
        m_pakSurfaceState.GrfSurfState.pYPlane              = m_frames[m_frame_idx[VP8_GOLDEN_FRAME]].data_y;
        m_pakSurfaceState.GrfSurfState.pUPlane              = m_frames[m_frame_idx[VP8_GOLDEN_FRAME]].data_u;
        m_pakSurfaceState.GrfSurfState.pVPlane              = m_frames[m_frame_idx[VP8_GOLDEN_FRAME]].data_v;
        m_pakSurfaceState.GrfSurfState.pitchPixels          = m_frames[m_frame_idx[VP8_GOLDEN_FRAME]].pitch;
        m_pakSurfaceState.ArfSurfState.pYPlane              = m_frames[m_frame_idx[VP8_ALTREF_FRAME]].data_y;
        m_pakSurfaceState.ArfSurfState.pUPlane              = m_frames[m_frame_idx[VP8_ALTREF_FRAME]].data_u;
        m_pakSurfaceState.ArfSurfState.pVPlane              = m_frames[m_frame_idx[VP8_ALTREF_FRAME]].data_v;
        m_pakSurfaceState.ArfSurfState.pitchPixels          = m_frames[m_frame_idx[VP8_ALTREF_FRAME]].pitch;

        m_pakFrameInfState.fWidth      = m_frames[m_frame_idx[VP8_ORIGINAL_FRAME]].frameSize.width;
        m_pakFrameInfState.fHeight     = m_frames[m_frame_idx[VP8_ORIGINAL_FRAME]].frameSize.height;
        m_pakFrameInfState.fType       = m_ctrl.FrameType;
        m_pakFrameInfState.fSegCtrl    = m_ctrl.SegOn | (m_ctrl.MBSegUpdate<<1) | (m_ctrl.SegFeatUpdate<<2);
        m_pakFrameInfState.fLfType     = m_ctrl.LoopFilterType;
        m_pakFrameInfState.fLfEnabled  = (m_ctrl.LoopFilterLevel!=0);
        switch (m_ctrl.Version) 
        {
        case 1:
        case 2:
            m_pakFrameInfState.fInterCtrl = 1 /*VP8_BILINEAR_INTERP*/;
            break;
        case 3:
            m_pakFrameInfState.fInterCtrl = 3 /*VP8_BILINEAR_INTERP | VP8_CHROMA_FULL_PEL*/;
            break;
        case 0:
        default:
            m_pakFrameInfState.fInterCtrl = 0;
            break;
        }

        InitializeQuantTable(m_ctrl, m_pakFrameInfState);
        InitializeLfTable(m_ctrl, m_pakFrameInfState);

        CHECK(pBitstream->DataLength + pBitstream->DataOffset + 3 + 8  < pBitstream->MaxLength);

        m_pBitstream = pBitstream;
        pPictureHeader = pBitstream->Data + pBitstream->DataLength + pBitstream->DataOffset;
        if (bSeqHeader) {
            AddSeqHeader(m_Params.SrcWidth , m_Params.SrcHeight, m_Params.FrameRateNom, m_Params.FrameRateDeNom, pPictureHeader);
            pBitstream->DataLength += 32; hOffset += 32;
        }
        pPictureHeader = pBitstream->Data + pBitstream->DataLength + pBitstream->DataOffset;
        AddPictureHeader(pPictureHeader);
        pBitstream->DataLength += 12;

        memset(&m_cnt1, 0, sizeof(m_cnt1));

        PakMfdStateVp8  mfdState;
        mfdState.pakFrameInfState = m_pakFrameInfState;
        mfdState.pakSurfaceState = m_pakSurfaceState;
        m_pakMfd.LoadState(&mfdState);

        PAKTokenizeAndCnt(pTask, layout, pCore
#if defined (VP8_HYBRID_DUMP_READ) || defined (VP8_HYBRID_DUMP_WRITE)
            , m_bse_dump, m_bse_dump_size
#endif
            );
        UpdateEntropyModel();
        EncodeTokenPartitions();
        EncodeFirstPartition();
        sts = OutputBitstream();

        if(sts == MFX_ERR_NONE)
            UpdatePictureHeader(pBitstream->DataLength-hOffset, m_encoded_frame_num, pPictureHeader);

        RefreshFrames();

        return sts;   
    }

    void Vp8CoreBSE::RefreshFrames(void)
    {
        if(m_ctrl.FrameType)
        {
            switch(m_ctrl.AltRefCopyFlag) {
            case 1:
                m_frame_idx[VP8_ALTREF_FRAME] = m_frame_idx[VP8_LAST_FRAME];
                //                m_frame_tid[VP8_ALTREF_FRAME] = m_frame_tid[VP8_LAST_FRAME];
                //                m_frame_poc[VP8_ALTREF_FRAME] = m_frame_poc[VP8_LAST_FRAME];
                break;
            case 2:
                m_frame_idx[VP8_ALTREF_FRAME] = m_frame_idx[VP8_GOLDEN_FRAME];
                //                m_frame_tid[VP8_ALTREF_FRAME] = m_frame_tid[VP8_GOLDEN_FRAME];
                //                m_frame_poc[VP8_ALTREF_FRAME] = m_frame_poc[VP8_GOLDEN_FRAME];
                break;
            case 0:
            default:
                break;
            }
            switch(m_ctrl.GoldenCopyFlag) {
            case 1:
                m_frame_idx[VP8_GOLDEN_FRAME] = m_frame_idx[VP8_LAST_FRAME];
                //                m_frame_tid[VP8_GOLDEN_FRAME] = m_frame_tid[VP8_LAST_FRAME];
                //                m_frame_poc[VP8_GOLDEN_FRAME] = m_frame_poc[VP8_LAST_FRAME];
                break;
            case 2:
                m_frame_idx[VP8_GOLDEN_FRAME] = m_frame_idx[VP8_ALTREF_FRAME];
                //                m_frame_tid[VP8_GOLDEN_FRAME] = m_frame_tid[VP8_ALTREF_FRAME];
                //                m_frame_poc[VP8_GOLDEN_FRAME] = m_frame_poc[VP8_ALTREF_FRAME];
                break;
            case 0:
            default:
                break;
            }
            if(m_ctrl.GoldenCopyFlag==3) {
                m_frame_idx[VP8_GOLDEN_FRAME] = m_frame_idx[VP8_INTRA_FRAME];
                //                m_frame_tid[VP8_GOLDEN_FRAME] = m_frame_tid[VP8_INTRA_FRAME];
                //                m_frame_poc[VP8_GOLDEN_FRAME] = m_frame_poc[VP8_INTRA_FRAME];
            }
            if(m_ctrl.AltRefCopyFlag==3) {
                m_frame_idx[VP8_ALTREF_FRAME] = m_frame_idx[VP8_INTRA_FRAME];
                //                m_frame_tid[VP8_ALTREF_FRAME] = m_frame_tid[VP8_INTRA_FRAME];
                //                m_frame_poc[VP8_ALTREF_FRAME] = m_frame_poc[VP8_INTRA_FRAME];
            }
            if(m_ctrl.LastFrameUpdate) {
                m_frame_idx[VP8_LAST_FRAME] = m_frame_idx[VP8_INTRA_FRAME];
                //                m_frame_tid[VP8_LAST_FRAME] = m_frame_tid[VP8_INTRA_FRAME];
                //                m_frame_poc[VP8_LAST_FRAME] = m_frame_poc[VP8_INTRA_FRAME];
            }
        } else
        {
            m_frame_idx[VP8_LAST_FRAME] = m_frame_idx[VP8_INTRA_FRAME];
            m_frame_idx[VP8_GOLDEN_FRAME] = m_frame_idx[VP8_INTRA_FRAME];
            m_frame_idx[VP8_ALTREF_FRAME] = m_frame_idx[VP8_INTRA_FRAME];
            //            m_frame_tid[VP8_ALTREF_FRAME] = m_frame_tid[VP8_GOLDEN_FRAME] = m_frame_tid[VP8_LAST_FRAME] = m_frame_tid[VP8_INTRA_FRAME];
            //            m_frame_poc[VP8_ALTREF_FRAME] = m_frame_poc[VP8_GOLDEN_FRAME] = m_frame_poc[VP8_LAST_FRAME] = m_frame_poc[VP8_INTRA_FRAME];
        }
    }


    enum { CNT_ZERO, CNT_NEAREST, CNT_NEAR, CNT_SPLITMV };

    VPX_FORCEINLINE void Vp8GetNearMVs(TrMbCodeVp8 *pMb, I32 fWidthInMBs, MvTypeVp8 *nearMVs, I32 *mvCnt, U8 *refBias)
    {
        TrMbCodeVp8 *aMb, *lMb, *alMb;
        MvTypeVp8    tMv;
        I32          idx;

        mvCnt[0] = mvCnt[1] = mvCnt[2] = mvCnt[3] = 0;
        nearMVs[0].s32 = nearMVs[1].s32 = nearMVs[2].s32 = nearMVs[3].s32 = 0;

        if(!pMb->MbYcnt) {
            if(pMb->MbXcnt) {
                /* Only left neighbour present */
                lMb = pMb-1;
                if (lMb->RefFrameMode != VP8_INTRA_FRAME)
                {
                    tMv = (lMb->MbMode==VP8_MV_SPLIT) ? lMb->NewMV4x4[15] : lMb->NewMV16;
                    if (tMv.s32)
                    {
                        if(refBias[lMb->RefFrameMode^pMb->RefFrameMode]) {
                            nearMVs[CNT_NEAREST].MV.mvx = -tMv.MV.mvx;
                            nearMVs[CNT_NEAREST].MV.mvy = -tMv.MV.mvy;
                        } else {
                            nearMVs[CNT_NEAREST].s32 = tMv.s32;
                        }
                        nearMVs[CNT_ZERO].s32 = nearMVs[CNT_NEAREST].s32;
                        mvCnt[CNT_NEAREST] = 2;
                    }
                    else {
                        mvCnt[CNT_ZERO] = 2;
                    }
                    mvCnt[CNT_SPLITMV] = (lMb->MbMode == VP8_MV_SPLIT) ? 2 : 0;
                }
            }
        } else {
            if(!pMb->MbXcnt) {
                /* Only top neighbour present */
                aMb = pMb - fWidthInMBs;
                if (aMb->RefFrameMode != VP8_INTRA_FRAME)
                {
                    tMv = (aMb->MbMode==VP8_MV_SPLIT) ? aMb->NewMV4x4[15] : aMb->NewMV16;
                    if (tMv.s32)
                    {
                        if(refBias[aMb->RefFrameMode^pMb->RefFrameMode]) {
                            nearMVs[CNT_NEAREST].MV.mvx = -tMv.MV.mvx;
                            nearMVs[CNT_NEAREST].MV.mvy = -tMv.MV.mvy;
                        } else {
                            nearMVs[CNT_NEAREST].s32 = tMv.s32;
                        }
                        nearMVs[CNT_ZERO].s32 = nearMVs[CNT_NEAREST].s32;
                        mvCnt[CNT_NEAREST] = 2;
                    }
                    else {
                        mvCnt[CNT_ZERO] = 2;
                    }
                    mvCnt[CNT_SPLITMV] = (aMb->MbMode == VP8_MV_SPLIT) ? 2 : 0;
                }
            } else {
                /* All three neighbours present */
                idx  = 0;
                lMb  = pMb - 1;
                aMb  = pMb - fWidthInMBs;
                alMb = aMb - 1;

                if (aMb->RefFrameMode != VP8_INTRA_FRAME)
                {
                    tMv = (aMb->MbMode==VP8_MV_SPLIT) ? aMb->NewMV4x4[15] : aMb->NewMV16;
                    if (tMv.s32)
                    {
                        if(refBias[aMb->RefFrameMode^pMb->RefFrameMode]) {
                            nearMVs[CNT_NEAREST].MV.mvx = -tMv.MV.mvx;
                            nearMVs[CNT_NEAREST].MV.mvy = -tMv.MV.mvy;
                        } else {
                            nearMVs[CNT_NEAREST].s32 = tMv.s32;
                        }

                        mvCnt[CNT_NEAREST] = 2;
                        idx = 1;
                    }
                    else {
                        mvCnt[CNT_ZERO] = 2;
                    }
                }

                if (lMb->RefFrameMode != VP8_INTRA_FRAME)
                {
                    tMv = (lMb->MbMode==VP8_MV_SPLIT) ? lMb->NewMV4x4[15] : lMb->NewMV16;
                    if (tMv.s32)
                    {
                        if(refBias[lMb->RefFrameMode^pMb->RefFrameMode]) {
                            tMv.MV.mvx = -tMv.MV.mvx;
                            tMv.MV.mvy = -tMv.MV.mvy;
                        }
                        if(tMv.s32 != nearMVs[CNT_NEAREST].s32) 
                            nearMVs[++idx].s32 = tMv.s32;
                        mvCnt[idx] += 2;
                    }
                    else {
                        mvCnt[CNT_ZERO] += 2;
                    }
                }

                if(alMb->RefFrameMode != VP8_INTRA_FRAME)
                {
                    tMv = (alMb->MbMode==VP8_MV_SPLIT) ? alMb->NewMV4x4[15] : alMb->NewMV16;
                    if (tMv.s32)
                    {
                        if(refBias[alMb->RefFrameMode^pMb->RefFrameMode]) {
                            tMv.MV.mvx = -tMv.MV.mvx;
                            tMv.MV.mvy = -tMv.MV.mvy;
                        }
                        if(tMv.s32 != nearMVs[idx].s32) 
                            nearMVs[++idx].s32 = tMv.s32;
                        mvCnt[idx]++;
                    }
                    else {
                        mvCnt[CNT_ZERO]++;
                    }
                }

                if(mvCnt[CNT_SPLITMV] && (nearMVs[CNT_NEAREST].s32 == nearMVs[CNT_SPLITMV].s32))
                    mvCnt[CNT_NEAREST]++;

                if(mvCnt[CNT_NEAR] > mvCnt[CNT_NEAREST])
                {
                    I32 tmp;

                    tmp = nearMVs[CNT_NEAR].s32;
                    nearMVs[CNT_NEAR].s32    = nearMVs[CNT_NEAREST].s32;
                    nearMVs[CNT_NEAREST].s32 = tmp;

                    tmp = mvCnt[CNT_NEAR];
                    mvCnt[CNT_NEAR]    = mvCnt[CNT_NEAREST];
                    mvCnt[CNT_NEAREST] = tmp;
                }

                if (mvCnt[CNT_NEAREST] >= mvCnt[CNT_ZERO])
                    nearMVs[CNT_ZERO].s32 = nearMVs[CNT_NEAREST].s32;

                mvCnt[CNT_SPLITMV] = (((lMb->RefFrameMode != VP8_INTRA_FRAME)&&(lMb->MbMode == VP8_MV_SPLIT)) ? 2 : 0) +
                    (((aMb->RefFrameMode != VP8_INTRA_FRAME)&&(aMb->MbMode == VP8_MV_SPLIT)) ? 2 : 0) +
                    (((alMb->RefFrameMode != VP8_INTRA_FRAME)&&(alMb->MbMode == VP8_MV_SPLIT)) ? 1 : 0);
            }
        }
    }

    VPX_FORCEINLINE void vp8UpdateMVCounters(Vp8MvCounters *cnt, I16 v)
    {
        U16     absv = (v<0)?-v:v;
        I32     i;

        if(absv<VP8_MV_LONG-1) {
            cnt->evnCnt[0][VP8_MV_IS_SHORT]++;
            cnt->absValHist[absv]++;
        } else {
            cnt->evnCnt[1][VP8_MV_IS_SHORT]++;
            for(i=0;i<3;i++)
                cnt->evnCnt[(absv >> i) & 1][VP8_MV_LONG+i]++;
            for (i = VP8_MV_LONG_BITS - 1; i > 3; i--)
                cnt->evnCnt[(absv >> i) & 1][VP8_MV_LONG+i]++;
            if (absv & 0xFFF0)
                cnt->evnCnt[(absv >> 3) & 1][VP8_MV_LONG+3]++;
        }
        if(absv!=0) 
            cnt->evnCnt[(absv!=v)][VP8_MV_SIGN]++;
    }

    VPX_FORCEINLINE void vp8UpdateEobCounters(Vp8Counters *cnt, U8 ctx1, U8 ctx2, U8 ctx3)
    {
        cnt->mbTokenHist[ctx1][ctx2][ctx3][VP8_DCTEOB]++;
    }

    VPX_FORCEINLINE U16 U162bMode(U16 mode) {
        switch(mode) {
        case 0: return VP8_B_DC_PRED;
        case 2: return VP8_B_VE_PRED;
        case 3: return VP8_B_HE_PRED;
        case 1: return VP8_B_TM_PRED;
        case 4: return VP8_B_LD_PRED;
        case 5: return VP8_B_RD_PRED;
        case 6: return VP8_B_VR_PRED;
        case 7: return VP8_B_VL_PRED;
        case 8: return VP8_B_HD_PRED;
        default:
        case 9: return VP8_B_HU_PRED;
        }
    }

    VPX_FORCEINLINE void MbHLD2trMbCode(HLDMbDataVp8 *pS, HLDMvDataVp8 *pSm, TrMbCodeVp8* pMb)
    {
        mfxU8   sbModes[16];
        sbModes[ 0] = (pS->data[1] >>  0) & 0xF;
        sbModes[ 1] = (pS->data[1] >>  4) & 0xF;
        sbModes[ 2] = (pS->data[1] >>  8) & 0xF;
        sbModes[ 3] = (pS->data[1] >> 12) & 0xF;
        sbModes[ 4] = (pS->data[1] >> 16) & 0xF;
        sbModes[ 5] = (pS->data[1] >> 20) & 0xF;
        sbModes[ 6] = (pS->data[1] >> 24) & 0xF;
        sbModes[ 7] = (pS->data[1] >> 28) & 0xF;

        sbModes[ 8] = (pS->data[2] >>  0) & 0xF;
        sbModes[ 9] = (pS->data[2] >>  4) & 0xF;
        sbModes[10] = (pS->data[2] >>  8) & 0xF;
        sbModes[11] = (pS->data[2] >> 12) & 0xF;
        sbModes[12] = (pS->data[2] >> 16) & 0xF;
        sbModes[13] = (pS->data[2] >> 20) & 0xF;
        sbModes[14] = (pS->data[2] >> 24) & 0xF;
        sbModes[15] = (pS->data[2] >> 28) & 0xF;

        pMb->DWord0       = 0;
        pMb->nonZeroCoeff = 0;
        pMb->MbXcnt       = (pS->data[0]) & 0x3FF;
        pMb->MbYcnt       = (pS->data[0] >> 10) & 0x3FF;
        pMb->CoeffSkip    = (pS->data[0] >> 20) & 0x1;
        pMb->RefFrameMode = ((pS->data[0] >> 21) & 0x1) ? ((pS->data[0] >> 22) & 0x3) : VP8_INTRA_FRAME;

        if(pMb->RefFrameMode) {
            pMb->MbMode = (pS->data[0] >> 24) & 0x07;
            if(pMb->MbMode==VP8_MV_SPLIT) {
                pMb->MbSubmode = ((pS->data[0] >> 27) & 0x7) - 1;
                switch(pMb->MbSubmode) {
                case VP8_MV_TOP_BOTTOM:
                    sbModes[ 7] = sbModes[ 6] = sbModes[ 5] = sbModes[ 4] = sbModes[ 3] = sbModes[ 2] = sbModes[ 1] = sbModes[ 0];
                    sbModes[15] = sbModes[14] = sbModes[13] = sbModes[12] = sbModes[11] = sbModes[10] = sbModes[ 9] = sbModes[ 8];
                    break;
                case VP8_MV_LEFT_RIGHT:
                    sbModes[13] = sbModes[12] = sbModes[ 9] = sbModes[ 8] = sbModes[ 5] = sbModes[ 4] = sbModes[ 1] = sbModes[ 0];
                    sbModes[15] = sbModes[14] = sbModes[11] = sbModes[10] = sbModes[ 7] = sbModes[ 6] = sbModes[ 3] = sbModes[ 2];
                    break;
                case VP8_MV_QUARTERS:
                    sbModes[13] = sbModes[12] = sbModes[ 9] = sbModes[ 8];  sbModes[ 5] = sbModes[ 4] = sbModes[ 1] = sbModes[ 0];
                    sbModes[15] = sbModes[14] = sbModes[11] = sbModes[10];  sbModes[ 7] = sbModes[ 6] = sbModes[ 3] = sbModes[ 2];
                    break;
                default:
                    break;
                }
                pMb->InterSplitModes = 0;
                for(I32 k=15;k>=0;k--) {
                    pMb->InterSplitModes <<= 2;
                    pMb->InterSplitModes |= sbModes[k];
                    pMb->NewMV4x4[k].s32 = pSm->MV[k].s32;
                }
            } else {
                pMb->NewMV16.s32 = pSm->MV[15].s32;            /* VP8_MV_NEW vector placeholder */
            }
        } else {
            pMb->MbMode = (pS->data[0] >> 27) & 0x07;
            if(pMb->MbMode == VP8_MB_B_PRED) {
                pMb->Y4x4Modes0 = (U162bMode(sbModes[ 3])<<12)|(U162bMode(sbModes[ 2])<<8)|(U162bMode(sbModes[ 1])<<4)|(U162bMode(sbModes[ 0]));
                pMb->Y4x4Modes1 = (U162bMode(sbModes[ 7])<<12)|(U162bMode(sbModes[ 6])<<8)|(U162bMode(sbModes[ 5])<<4)|(U162bMode(sbModes[ 4]));
                pMb->Y4x4Modes2 = (U162bMode(sbModes[11])<<12)|(U162bMode(sbModes[10])<<8)|(U162bMode(sbModes[ 9])<<4)|(U162bMode(sbModes[ 8]));
                pMb->Y4x4Modes3 = (U162bMode(sbModes[15])<<12)|(U162bMode(sbModes[14])<<8)|(U162bMode(sbModes[13])<<4)|(U162bMode(sbModes[12]));
            } else {
                pMb->Y4x4Modes0 = (U16)((pMb->MbMode<<12)|(pMb->MbMode<<8)|(pMb->MbMode<<4)|(pMb->MbMode));
                pMb->Y4x4Modes1 = (U16)((pMb->MbMode<<12)|(pMb->MbMode<<8)|(pMb->MbMode<<4)|(pMb->MbMode));
                pMb->Y4x4Modes2 = (U16)((pMb->MbMode<<12)|(pMb->MbMode<<8)|(pMb->MbMode<<4)|(pMb->MbMode));
                pMb->Y4x4Modes3 = (U16)((pMb->MbMode<<12)|(pMb->MbMode<<8)|(pMb->MbMode<<4)|(pMb->MbMode));
            }
            pMb->MbSubmode =(pS->data[0] >> 30) & 0x3;
        }
    }

    VPX_FORCEINLINE U32 un_zigzag_and_last_nonzero_coeff_sse_pak( const I16* src, I16* dst )
    {
#define pshufb_word_lower(a) (a)*2, (a)*2+1
#define pshufb_word_upper(a) (a-8)*2, (a-8)*2+1
        //        const U8 vp8_zigzag_scan[16] = { 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 };
        __m128i v_shuf0_7 = _mm_setr_epi8(
            pshufb_word_lower(0),
            pshufb_word_lower(1),
            pshufb_word_lower(4),
            -1, -1, // pshufb_word_lower(8),
            pshufb_word_lower(5),
            pshufb_word_lower(2),
            pshufb_word_lower(3),
            pshufb_word_lower(6) );

        __m128i v_shuf8_15 = _mm_setr_epi8(
            pshufb_word_upper(9),
            pshufb_word_upper(12),
            pshufb_word_upper(13),
            pshufb_word_upper(10),
            -1, -1, // pshufb_word_upper(7),
            pshufb_word_upper(11),
            pshufb_word_upper(14),
            pshufb_word_upper(15) );
#undef pshufb_word_lower
#undef pshufb_word_upper

        __m128i v_coeffs0_7  = _mm_loadu_si128( (const __m128i*)src );
        __m128i v_coeffs8_15 = _mm_loadu_si128( (const __m128i*)(src+8) );

        v_coeffs0_7  = _mm_shuffle_epi8( v_coeffs0_7, v_shuf0_7  );
        v_coeffs8_15 = _mm_shuffle_epi8( v_coeffs8_15, v_shuf8_15);

        v_coeffs0_7  = _mm_insert_epi16( v_coeffs0_7,  *(const int*)(src+8), 3 ); // *(const int*) required for CL to generate a LD+OP form
        v_coeffs8_15 = _mm_insert_epi16( v_coeffs8_15, *(const int*)(src+7), 4 );

        _mm_storeu_si128( (__m128i*)dst,       v_coeffs0_7  );
        _mm_storeu_si128( (__m128i*)(dst + 8), v_coeffs8_15 );

        v_coeffs0_7  = _mm_cmpeq_epi16( v_coeffs0_7,  _mm_setzero_si128() );
        v_coeffs8_15 = _mm_cmpeq_epi16( v_coeffs8_15, _mm_setzero_si128() );

        U32 mask = _mm_movemask_epi8( v_coeffs0_7 );
        mask |= _mm_movemask_epi8( v_coeffs8_15 ) << 16;

        return mask;
    }

    void Vp8CoreBSE::PAKTokenizeAndCnt(TaskHybridDDI *pTask, MBDATA_LAYOUT const & layout, VideoCORE * pCore
#if defined (VP8_HYBRID_DUMP_READ) || defined (VP8_HYBRID_DUMP_WRITE)
        , FILE* m_bse_dump, mfxU32 m_bse_dump_size
#endif
        )
    {
        I32          i, j, partId, mPitch, maxPartId, mRowCnt, cnzMsk;
        U32         *pCoeff32, isZeroY2;
        U8           cntx3;
        TrMbCodeVp8 *cMb,*aMb,*lMb;
        MvTypeVp8    nearMvs[4];
        I32          nearCnt[4];
        I16          limT, limL, limB, limR;
        U32          aCntx3, lCntx3;
        CoeffsType   c_buffer[25*16];

        FrameLocker lockSrc0(pCore, pTask->ddi_frames.m_pMB_hw->pSurface->Data, false);        
        FrameLocker lockDst0(pCore, pTask->ddi_frames.m_pMB_sw->pSurface->Data, false);

#if defined (VP8_HYBRID_DUMP_READ)
        fread(pTask->ddi_frames.m_pMB_hw->pSurface->Data.Y, 1, m_bse_dump_size, m_bse_dump);
#endif

#if defined (MFX_VA_WIN)
        pCore->DoFastCopy(pTask->ddi_frames.m_pMB_sw->pSurface, pTask->ddi_frames.m_pMB_hw->pSurface);
#if defined (VP8_HYBRID_DUMP_WRITE)
        fwrite(pTask->ddi_frames.m_pMB_sw->pSurface->Data.Y, 1, m_bse_dump_size, m_bse_dump);
#endif

        HLDMbDataVp8 *pHLDMbData = (HLDMbDataVp8 *)(pTask->ddi_frames.m_pMB_sw->pSurface->Data.Y + layout.MB_CODE_offset);
        HLDMvDataVp8 *pHLDMvData = (HLDMvDataVp8 *)(pTask->ddi_frames.m_pMB_sw->pSurface->Data.Y + layout.MV_offset);
#elif defined (MFX_VA_LINUX)
#if defined (VP8_HYBRID_DUMP_WRITE)
        fwrite(pTask->ddi_frames.m_pMB_hw->pSurface->Data.Y, 1, m_bse_dump_size, m_bse_dump);
#endif
        HLDMbDataVp8 *pHLDMbData = (HLDMbDataVp8 *)(pTask->ddi_frames.m_pMB_hw->pSurface->Data.Y + layout.MB_CODE_offset);
        HLDMvDataVp8 *pHLDMvData = (HLDMvDataVp8 *)(pTask->ddi_frames.m_pMB_hw->pSurface->Data.Y + layout.MV_offset);
#endif
        mPitch      = ((pHLDMbData[m_Params.fSizeInMBs-1].data[0]) & 0x3FF) + 1;
        mRowCnt     = ((pHLDMbData[m_Params.fSizeInMBs-1].data[0]>>10) & 0x3FF) + 1;
        cMb         = m_Mbs1;
        partId      = 0;
        maxPartId   = (1<<m_ctrl.PartitionNumL2);

        for(i=0; i<maxPartId; i++) m_TokensE[i] = m_TokensB[i];

        for(i=0; i<(I32)m_Params.fSizeInMBs; i++)
        {
            // Initialized truncated MB data
            MbHLD2trMbCode(pHLDMbData, pHLDMvData, cMb);
            partId = (cMb->MbYcnt&(~(0xFFFFFFFF<<m_ctrl.PartitionNumL2)));

            m_pakMfd.EncAndRecMB(*cMb, c_buffer);

            // Gather counters
            m_cnt1.mbRefHist[cMb->RefFrameMode]++;
            if(m_ctrl.SegOn && m_ctrl.MBSegUpdate)
                m_cnt1.mbSegHist[cMb->SegmentId]++;
            if(cMb->RefFrameMode) {
                limT = -(((I16)cMb->MbYcnt + 1) << 7);
                limL = -(((I16)cMb->MbXcnt + 1) << 7);
                limB = (I16)(mRowCnt - cMb->MbYcnt) << 7;
                limR = (I16)(mPitch - cMb->MbXcnt) << 7;
                Vp8GetNearMVs(cMb, mPitch, nearMvs, nearCnt, m_fRefBias);
                clampMvComponent(nearMvs[0], limT, limB, limL, limR);

                switch(cMb->MbMode) {
                case VP8_MV_NEW:
                    vp8UpdateMVCounters(m_cnt1.mbMVs,  (cMb->NewMV16.MV.mvy - nearMvs[0].MV.mvy)>>1);
                    vp8UpdateMVCounters(m_cnt1.mbMVs+1,(cMb->NewMV16.MV.mvx - nearMvs[0].MV.mvx)>>1);
                    break;
                case VP8_MV_SPLIT:
                    {
                        I8 j = 0, bl = 0, offset, num_blocks, smode;

                        switch(cMb->MbSubmode) {
                        case VP8_MV_TOP_BOTTOM:
                            num_blocks = 2; offset = 8; break;
                        case VP8_MV_LEFT_RIGHT:
                            num_blocks = 2; offset = 2; break;
                        case VP8_MV_QUARTERS:
                            num_blocks = 4; offset = 2; break;
                        case VP8_MV_16:
                        default:
                            num_blocks = 16; offset = 1; break;
                        };
                        for (bl = 0, j = 0; j < num_blocks; j++)
                        {
                            smode = (cMb->InterSplitModes >> (bl*2)) & 0x03;
                            if(smode==VP8_B_MV_NEW) {
                                vp8UpdateMVCounters(m_cnt1.mbMVs,  (cMb->NewMV4x4[bl].MV.mvy - nearMvs[0].MV.mvy)>>1);
                                vp8UpdateMVCounters(m_cnt1.mbMVs+1,(cMb->NewMV4x4[bl].MV.mvx - nearMvs[0].MV.mvx)>>1);
                            }
                            if (cMb->MbSubmode == VP8_MV_QUARTERS && j==1) bl+=4;
                            bl += offset;
                        }
                    }
                    break;
                default:
                    break;
                }
            } else {
                m_cnt1.mbModeYHist[cMb->MbMode]++;
                m_cnt1.mbModeUVHist[cMb->MbSubmode]++;
            }
            if(cMb->CoeffSkip) m_cnt1.mbSkipFalse++;

            // Check for skip and set Y2 contexts
            lMb = (cMb->MbXcnt)? cMb - 1 : &m_externalMb1;
            aMb = (cMb->MbYcnt)? cMb - mPitch : &m_externalMb1;
            isZeroY2 = 0;
            if(cMb->MbMode!=VP8_MB_B_PRED) {
                if(!cMb->CoeffSkip) {
                    pCoeff32 = (U32*)(c_buffer+24*16);
                    for(j=0; j<8; j++)
                        isZeroY2 |= pCoeff32[j];
                }
                cMb->Y2AboveNotZero = cMb->Y2LeftNotZero = (isZeroY2!=0);
            } else {
                cMb->Y2AboveNotZero = aMb->Y2AboveNotZero;
                cMb->Y2LeftNotZero  = lMb->Y2LeftNotZero;
            }

            // Forced skip
            if(cMb->CoeffSkip && m_ctrl.MbNoCoeffSkip) {
                cMb->nonZeroCoeff = 0;
                cMb++; pHLDMbData++; pHLDMvData++;
                continue;
            }

            CoeffsType  un_zigzag_coeffs[16];
            unsigned long mask, index;

            aCntx3 = ((aMb->nonZeroCoeff>>12) & 0x0000000F) | ((aMb->nonZeroCoeff>>2) & 0x00330000);
            lCntx3 = ((lMb->nonZeroCoeff>> 3) & 0x00001111) | ((lMb->nonZeroCoeff>>1) & 0x00550000);
            cnzMsk = 0;

            if(cMb->MbMode!=VP8_MB_B_PRED) {
                cntx3 = (U8)(aMb->Y2AboveNotZero + lMb->Y2LeftNotZero);
                mask = un_zigzag_and_last_nonzero_coeff_sse_pak( (c_buffer+24*16), un_zigzag_coeffs);
                //fflush(stdout);
                //printf("\nLCM0 %d, %d, %d, %d, %d, %d, %d, %d ", un_zigzag_coeffs[0], un_zigzag_coeffs[1], un_zigzag_coeffs[2], un_zigzag_coeffs[3], un_zigzag_coeffs[4], un_zigzag_coeffs[5], un_zigzag_coeffs[6], un_zigzag_coeffs[7]);
                //printf("%d, %d, %d, %d, %d, %d, %d, %d\n", un_zigzag_coeffs[8], un_zigzag_coeffs[9], un_zigzag_coeffs[10], un_zigzag_coeffs[11], un_zigzag_coeffs[12], un_zigzag_coeffs[13], un_zigzag_coeffs[14], un_zigzag_coeffs[15]);
                //fflush(stdout);
                if ( ! _BitScanReverse( &index, (mask ^ -1) ) ) {
                    *m_TokensE[partId]++ = (VP8_NUM_COEFF_BANDS*VP8_NUM_LOCAL_COMPLEXITIES+cntx3) + 256;
                    vp8UpdateEobCounters(&m_cnt1, 1, 0, cntx3);
                } else {
                    EncodeTokens( un_zigzag_coeffs, 0, (I32)index >> 1, 1, cntx3, (U8)partId );
                }

                for(j=0;j<16;j++) {
                    cntx3 = ((aCntx3 >> j)&0x1) + ((lCntx3 >> j)&0x1);
                    mask = un_zigzag_and_last_nonzero_coeff_sse_pak( (c_buffer+j*16), un_zigzag_coeffs) | 0x3;
                    //fflush(stdout);
                    //printf("\nLCM1 %d, %d, %d, %d, %d, %d, %d, %d ", 0, un_zigzag_coeffs[1], un_zigzag_coeffs[2], un_zigzag_coeffs[3], un_zigzag_coeffs[4], un_zigzag_coeffs[5], un_zigzag_coeffs[6], un_zigzag_coeffs[7]);
                    //printf("%d, %d, %d, %d, %d, %d, %d, %d\n", un_zigzag_coeffs[8], un_zigzag_coeffs[9], un_zigzag_coeffs[10], un_zigzag_coeffs[11], un_zigzag_coeffs[12], un_zigzag_coeffs[13], un_zigzag_coeffs[14], un_zigzag_coeffs[15]);
                    //fflush(stdout);
                    if ( ! _BitScanReverse( &index, (mask ^ -1) ) ) {
                        *m_TokensE[partId]++ = (VP8_NUM_LOCAL_COMPLEXITIES+cntx3) + 256;
                        vp8UpdateEobCounters(&m_cnt1, 0, 1, cntx3);
                    } else {
                        EncodeTokens( un_zigzag_coeffs, 1, (I32)index >> 1, 0, cntx3, (U8)partId );
                        cnzMsk |= (0x1 << j);
                        aCntx3 |= ((0x1<<(j+4)) & 0x0000FFF0);
                        lCntx3 |= ((0x1<<(j+1)) & 0x0000EEEE);
                    }
                }
            } else {
                for(j=0;j<16;j++) {
                    cntx3 = ((aCntx3 >> j)&0x1) + ((lCntx3 >> j)&0x1);
                    mask = un_zigzag_and_last_nonzero_coeff_sse_pak( (c_buffer+j*16), un_zigzag_coeffs);
                    //fflush(stdout);
                    //printf("\nLCM2 %d, %d, %d, %d, %d, %d, %d, %d ", un_zigzag_coeffs[0], un_zigzag_coeffs[1], un_zigzag_coeffs[2], un_zigzag_coeffs[3], un_zigzag_coeffs[4], un_zigzag_coeffs[5], un_zigzag_coeffs[6], un_zigzag_coeffs[7]);
                    //printf("%d, %d, %d, %d, %d, %d, %d, %d\n", un_zigzag_coeffs[8], un_zigzag_coeffs[9], un_zigzag_coeffs[10], un_zigzag_coeffs[11], un_zigzag_coeffs[12], un_zigzag_coeffs[13], un_zigzag_coeffs[14], un_zigzag_coeffs[15]);
                    //fflush(stdout);
                    if ( ! _BitScanReverse( &index, (mask ^ -1) ) ) {
                        *m_TokensE[partId]++ = ((3*VP8_NUM_COEFF_BANDS*VP8_NUM_LOCAL_COMPLEXITIES+cntx3) ) + 256;
                        vp8UpdateEobCounters(&m_cnt1, 3, vp8_coefBands[0], cntx3);
                    } else {
                        EncodeTokens( un_zigzag_coeffs, 0, (I32)index >> 1, 3, cntx3, (U8)partId );
                        cnzMsk |= (0x1 << j);
                        aCntx3 |= ((0x1<<(j+4)) & 0x0000FFF0);
                        lCntx3 |= ((0x1<<(j+1)) & 0x0000EEEE);
                    }
                }
            }

            for(j=16;j<24;j++) {
                cntx3 = ((aCntx3 >> j)&0x1) + ((lCntx3 >> j)&0x1);
                mask = un_zigzag_and_last_nonzero_coeff_sse_pak( (c_buffer+j*16), un_zigzag_coeffs);
                //fflush(stdout);
                //printf("\nCCMP %d, %d, %d, %d, %d, %d, %d, %d ", un_zigzag_coeffs[0], un_zigzag_coeffs[1], un_zigzag_coeffs[2], un_zigzag_coeffs[3], un_zigzag_coeffs[4], un_zigzag_coeffs[5], un_zigzag_coeffs[6], un_zigzag_coeffs[7]);
                //printf("%d, %d, %d, %d, %d, %d, %d, %d\n", un_zigzag_coeffs[8], un_zigzag_coeffs[9], un_zigzag_coeffs[10], un_zigzag_coeffs[11], un_zigzag_coeffs[12], un_zigzag_coeffs[13], un_zigzag_coeffs[14], un_zigzag_coeffs[15]);
                //fflush(stdout);
                if ( ! _BitScanReverse( &index, (mask ^ -1) ) ) {
                    *m_TokensE[partId]++ = ((2*VP8_NUM_COEFF_BANDS*VP8_NUM_LOCAL_COMPLEXITIES+cntx3)) + 256;
                    vp8UpdateEobCounters(&m_cnt1, 2, vp8_coefBands[0], cntx3);
                } else {
                    EncodeTokens( un_zigzag_coeffs, 0, (I32)index >> 1, 2, cntx3, (U8)partId );
                    cnzMsk |= (0x1 << j);
                    aCntx3 |= ((0x1<<(j+2)) & 0x00CC0000);
                    lCntx3 |= ((0x1<<(j+1)) & 0x00AA0000);
                }
            }

            cMb->nonZeroCoeff = cnzMsk;
            cMb++; pHLDMbData++; pHLDMvData++;
        }

        ReplicateBorders(&m_pakSurfaceState.ReconSurfState);
    }

    void Vp8PakMfd::LoadState(PakMfdStateVp8 *in_hState)
    {
        PakMfdVqStateVp8    vqState;

        m_mfdState = *in_hState;
        memmove(vqState.fQuants,m_mfdState.pakFrameInfState.fQuants,sizeof(vqState.fQuants));

        m_Qnt.LoadState(&vqState);

        memset(m_aRow[0], 127, 8192*sizeof(PixType));
        memset(m_lRow[0], 129, 8192*sizeof(PixType));
        m_lRow[0][15] = m_lRow[1][7] = m_lRow[2][7] = 127;
    }

    void Vp8PakMfd::EncAndRecMB(TrMbCodeVp8 &mB, CoeffsType* coeffs)
    {
        I32     i, j, x, y, y_offset, uv_offset, pitch;
        U32    *pCoeff32, isNotSkip=0;
        U8      is4x4 = 0;
        PixType tl;

        FwdTransIf      block_fwdt;
        QrcIf           block_qtz;
        InvTransIf      block_invt;
        PredictionIf    block_pred;
        LoopFilterIf    block_loopf;

        pitch     = m_mfdState.pakSurfaceState.ReconSurfState.pitchPixels; /* All surfaces expected to have the same pitch */
        y_offset  = (mB.MbYcnt*pitch + mB.MbXcnt)<<4;
        uv_offset = (mB.MbYcnt*pitch + mB.MbXcnt)<<3;

        /* Recon */
        block_pred.RefFrameMode = mB.RefFrameMode;
        block_pred.MbXcnt       = mB.MbXcnt;
        block_pred.MbYcnt       = mB.MbYcnt;
        block_pred.pitchRec     = 16;
        block_pred.pYRec        = m_reconBuf;
        block_pred.pURec        = m_reconBuf + 16*16;
        block_pred.pVRec        = block_pred.pURec + 8;
        block_pred.aRow[0]      = m_aRow[0] + (mB.MbXcnt<<4);
        block_pred.aRow[1]      = m_aRow[1] + (mB.MbXcnt<<3);
        block_pred.aRow[2]      = m_aRow[2] + (mB.MbXcnt<<3);
        block_pred.lRow[0]      = m_lRow[0] + (mB.MbYcnt<<4);
        block_pred.lRow[1]      = m_lRow[1] + (mB.MbYcnt<<3);
        block_pred.lRow[2]      = m_lRow[2] + (mB.MbYcnt<<3);
        if(block_pred.RefFrameMode==VP8_INTRA_FRAME) {
            block_pred.PredictionCtrl.MbYIntraMode = mB.MbMode;
            block_pred.PredictionCtrl.MbUVIntraMode = mB.MbSubmode;
            block_pred.PredictionCtrl.notLastInRow = (mB.MbXcnt+1!=(U32)(m_mfdState.pakFrameInfState.fWidth+15)>>4);
            block_pred.PredictionCtrl.subBlockId = -1;
            if(mB.MbMode==VP8_MB_B_PRED)
            {
                is4x4 = 1;
                for(i=0;i<4;i++)
                    block_pred.PredictionCtrl.Mb4x4YIntraModes[i]    = (mB.Y4x4Modes0>>4*i)&0xF;
                for(i=0;i<4;i++)
                    block_pred.PredictionCtrl.Mb4x4YIntraModes[i+4]  = (mB.Y4x4Modes1>>4*i)&0xF;
                for(i=0;i<4;i++)
                    block_pred.PredictionCtrl.Mb4x4YIntraModes[i+8]  = (mB.Y4x4Modes2>>4*i)&0xF;
                for(i=0;i<4;i++)
                    block_pred.PredictionCtrl.Mb4x4YIntraModes[i+12] = (mB.Y4x4Modes3>>4*i)&0xF;
            }
        } else {
            block_pred.PredictionCtrl.MvLimits[0] = -(((I16)mB.MbYcnt + 1) << 7) - (8<<3);
            block_pred.PredictionCtrl.MvLimits[1] = -(((I16)mB.MbXcnt + 1) << 7) - (8<<3);
            block_pred.PredictionCtrl.MvLimits[2] = ((((m_mfdState.pakFrameInfState.fHeight+15)&~0x0F) - (mB.MbYcnt<<4)) << 3) + (8<<3);
            block_pred.PredictionCtrl.MvLimits[3] = ((((m_mfdState.pakFrameInfState.fWidth+15)&~0x0F) - (mB.MbXcnt<<4)) << 3) + (8<<3);

            block_pred.PredictionCtrl.MbInterMode = mB.MbMode;
            block_pred.PredictionCtrl.filterType = m_mfdState.pakFrameInfState.fInterCtrl;
            if(block_pred.PredictionCtrl.MbInterMode != VP8_MV_SPLIT) {
                block_pred.PredictionCtrl.MbMv.s32 = mB.NewMV16.s32;
            } else {
                block_pred.PredictionCtrl.MbSplitMode = mB.MbSubmode;
                for(i=0;i<16;i++) {
                    block_pred.PredictionCtrl.Mb4x4MVs[i].s32 = mB.NewMV4x4[i].s32;
                }
            }
            block_pred.PredictionCtrl.pitchRef = pitch;
            if(block_pred.RefFrameMode==VP8_LAST_FRAME) {
                block_pred.PredictionCtrl.pYRef = m_mfdState.pakSurfaceState.LrfSurfState.pYPlane + y_offset;
                block_pred.PredictionCtrl.pURef = m_mfdState.pakSurfaceState.LrfSurfState.pUPlane + uv_offset;
                block_pred.PredictionCtrl.pVRef = m_mfdState.pakSurfaceState.LrfSurfState.pVPlane + uv_offset;
            } else if(block_pred.RefFrameMode==VP8_GOLDEN_FRAME) {
                block_pred.PredictionCtrl.pYRef = m_mfdState.pakSurfaceState.GrfSurfState.pYPlane + y_offset;
                block_pred.PredictionCtrl.pURef = m_mfdState.pakSurfaceState.GrfSurfState.pUPlane + uv_offset;
                block_pred.PredictionCtrl.pVRef = m_mfdState.pakSurfaceState.GrfSurfState.pVPlane + uv_offset;
            } else {
                block_pred.PredictionCtrl.pYRef = m_mfdState.pakSurfaceState.ArfSurfState.pYPlane + y_offset;
                block_pred.PredictionCtrl.pURef = m_mfdState.pakSurfaceState.ArfSurfState.pUPlane + uv_offset;
                block_pred.PredictionCtrl.pVRef = m_mfdState.pakSurfaceState.ArfSurfState.pVPlane + uv_offset;
            }
        }

        block_fwdt.mbType   = mB.MbMode;
        block_fwdt.mbSubId  = -1;
        block_fwdt.pCoeffs  = m_buffer;
        block_fwdt.pitchSrc = pitch;
        block_fwdt.pYSrc    = m_mfdState.pakSurfaceState.CurSurfState.pYPlane + y_offset;
        block_fwdt.pUSrc    = m_mfdState.pakSurfaceState.CurSurfState.pUPlane + uv_offset;
        block_fwdt.pVSrc    = m_mfdState.pakSurfaceState.CurSurfState.pVPlane + uv_offset;
        block_fwdt.pitchRec = block_pred.pitchRec;
        block_fwdt.pYRec    = block_pred.pYRec;
        block_fwdt.pURec    = block_pred.pURec;
        block_fwdt.pVRec    = block_pred.pVRec;

        block_qtz.mbType                = mB.MbMode;
        block_qtz.mbSubId               = -1;
        block_qtz.segId                 = mB.SegmentId;
        block_qtz.pmbCoeffs             = coeffs;
        block_qtz.pY2mbCoeffs           = coeffs+24*16;
        block_qtz.pInvTransformResult   = m_buffer;

        block_invt.mbType   = mB.MbMode;
        block_invt.mbSubId  = -1;
        block_invt.pCoeffs  = m_buffer;
        block_invt.pitchDst = pitch;
        block_invt.pYDst    = m_mfdState.pakSurfaceState.ReconSurfState.pYPlane + y_offset;
        block_invt.pUDst    = m_mfdState.pakSurfaceState.ReconSurfState.pUPlane + uv_offset;
        block_invt.pVDst    = m_mfdState.pakSurfaceState.ReconSurfState.pVPlane + uv_offset;
        block_invt.pitchRec = block_pred.pitchRec;
        block_invt.pYRec    = block_pred.pYRec;
        block_invt.pURec    = block_pred.pURec;
        block_invt.pVRec    = block_pred.pVRec;

        if(is4x4) {
            for(i=0;i<16;i++) {
                block_pred.PredictionCtrl.subBlockId = block_fwdt.mbSubId = block_qtz.mbSubId = block_invt.mbSubId = i;
                m_Pred.doProcess(block_pred);
                m_FwdT.doProcess(block_fwdt);
                m_Qnt.doProcess(block_qtz);
                m_InvT.doProcess(block_invt);

                x = (i&0x03)<<2; y = i&~0x03;
                if(y)
                    block_pred.lRow[0][y-1] = block_pred.aRow[0][x+3];
                else
                    tl = block_pred.aRow[0][x+3];
                memcpy(block_pred.aRow[0]+x, block_invt.pYDst + (y+3)*block_invt.pitchDst + x, 4*sizeof(PixType));
                for(j=0;j<3;j++)
                    block_pred.lRow[0][y+j] = *(block_invt.pYDst + (y+j)*block_invt.pitchDst + x + 3);
                if(!y)
                    block_pred.lRow[0][15] = tl;
            }
        } else {
            m_Pred.doProcess(block_pred);
            m_FwdT.doProcess(block_fwdt);
            m_Qnt.doProcess(block_qtz);
            m_InvT.doProcess(block_invt);

            tl = block_pred.aRow[0][15];
            memcpy(block_pred.aRow[0],block_invt.pYDst + 15*block_invt.pitchDst,16*sizeof(PixType));
            for(i=0;i<15;i++)
                block_pred.lRow[0][i] = *(block_invt.pYDst + i*block_invt.pitchDst + 15);
            block_pred.lRow[0][15] = tl;
        }

        for(pCoeff32 = (U32*)(coeffs),i=0; i<24*8; i++) isNotSkip |= pCoeff32[i];
        if(mB.MbMode!=VP8_MB_B_PRED)
            for(pCoeff32 = (U32*)(coeffs+24*16),i=0; i<8; i++) isNotSkip |= pCoeff32[i];
        mB.CoeffSkip = (isNotSkip==0);

        {
            tl = block_pred.aRow[1][7];
            memcpy(block_pred.aRow[1],block_invt.pUDst + 7*block_invt.pitchDst,8*sizeof(PixType));
            for(i=0;i<7;i++)
                block_pred.lRow[1][i] = *(block_invt.pUDst + i*block_invt.pitchDst + 7);
            block_pred.lRow[1][7] = tl;

            tl = block_pred.aRow[2][7];
            memcpy(block_pred.aRow[2],block_invt.pVDst + 7*block_invt.pitchDst,8*sizeof(PixType));
            for(i=0;i<7;i++)
                block_pred.lRow[2][i] = *(block_invt.pVDst + i*block_invt.pitchDst + 7);
            block_pred.lRow[2][7] = tl;
        }

        i = (mB.RefFrameMode != VP8_INTRA_FRAME) ? mB.MbMode : ((mB.MbMode==VP8_MB_B_PRED)? 5: 6);
        block_loopf.LfLevel = m_mfdState.pakFrameInfState.fLfLimit[mB.RefFrameMode][i][mB.SegmentId];

        if(m_mfdState.pakFrameInfState.fLfEnabled && block_loopf.LfLevel)
        {
            block_loopf.fType   = m_mfdState.pakFrameInfState.fType;
            block_loopf.LfType  = m_mfdState.pakFrameInfState.fLfType;
            block_loopf.MbXcnt  = mB.MbXcnt;
            block_loopf.MbYcnt  = mB.MbYcnt;
            block_loopf.LfLimit = m_mfdState.pakFrameInfState.fLfLimit[mB.RefFrameMode][i][mB.SegmentId];
            block_loopf.notSkipBlockFilter = !mB.CoeffSkip || (mB.MbMode==VP8_MB_B_PRED);
            block_loopf.pitch   = m_mfdState.pakSurfaceState.ReconSurfState.pitchPixels;
            block_loopf.pYRec   = m_mfdState.pakSurfaceState.ReconSurfState.pYPlane + y_offset;
            block_loopf.pURec   = m_mfdState.pakSurfaceState.ReconSurfState.pUPlane + uv_offset;
            block_loopf.pVRec   = m_mfdState.pakSurfaceState.ReconSurfState.pVPlane + uv_offset;
            m_LF.doProcess(block_loopf);
        }
    }

    void Vp8CoreBSE::ReplicateBorders(VidSurfaceState *SurfState)
    {
        I32      i, w, h, pitch, padd;
        U8      *ptr1, *ptr2;

        U16 m_fWidthInMBs   = (m_pakFrameInfState.fWidth+15) >> 4;
        U16 m_fHeightInMBs  = (m_pakFrameInfState.fHeight+15) >> 4;

        w     = m_fWidthInMBs<<4;
        h     = m_fHeightInMBs<<4;
        pitch = SurfState->pitchPixels;
        padd  = VP8_PADDING;

        ptr1  = SurfState->pYPlane;
        for(i = 0; i < h; i++, ptr1 += pitch)
        {
            memset(ptr1 - padd, ptr1[0], padd);
            memset(ptr1 + w, ptr1[w - 1], padd);
        }
        ptr1 = SurfState->pYPlane - padd;
        ptr2 = SurfState->pYPlane - padd - pitch*padd;
        for(i = 0; i < padd; i++, ptr2+= pitch)
        {
            memmove(ptr2, ptr1, pitch);
        }
        ptr1 = SurfState->pYPlane + pitch*(h-1) - padd;
        ptr2 = ptr1 + pitch;
        for(i = 0; i < padd; i++, ptr2+= pitch)
        {
            memmove(ptr2, ptr1, pitch);
        }

        w = (w+1)>>1; h = (h+1)>>1; padd >>= 1;

        ptr1  = SurfState->pUPlane;
        for(i = 0; i < h; i++, ptr1 += pitch)
        {
            memset(ptr1 - padd, ptr1[0], padd);
            memset(ptr1 + w, ptr1[w - 1], padd);
        }

        ptr1 = SurfState->pUPlane - padd;
        ptr2 = SurfState->pUPlane - padd - pitch*padd;
        for(i = 0; i < padd; i++, ptr2+= pitch)
        {
            memmove(ptr2, ptr1, pitch>>1);
        }

        ptr1 = SurfState->pUPlane - padd + pitch*(h-1);
        ptr2 = ptr1 + pitch;
        for(i = 0; i < padd; i++, ptr2+= pitch)
        {
            memmove(ptr2, ptr1, pitch>>1);
        }

        ptr1  = SurfState->pVPlane;
        for(i = 0; i < h; i++, ptr1 += pitch)
        {
            memset(ptr1 - padd, ptr1[0], padd);
            memset(ptr1 + w, ptr1[w - 1], padd);
        }

        ptr1 = SurfState->pVPlane - padd;
        ptr2 = SurfState->pVPlane - padd - pitch*padd;
        for(i = 0; i < padd; i++, ptr2+= pitch)
        {
            memmove(ptr2, ptr1, pitch>>1);
        }

        ptr1 = SurfState->pVPlane + pitch*(h-1) - padd;
        ptr2 = ptr1 + pitch;
        for(i = 0; i < padd; i++, ptr2+= pitch)
        {
            memmove(ptr2, ptr1, pitch>>1);
        }
    }
}
#endif
