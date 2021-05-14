// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "assert.h"
#include "string.h"
#include "stdlib.h"

#define ABS( a ) ( ((a) < 0) ? (-(a)) : (a) )

namespace AV1PP {
    int satd_4x4_px(const unsigned char* src1, int pitch1, const unsigned char* src2, int pitch2)
    {
        short tmpBuff[4][4];
        short diffBuff[4][4];
        unsigned int satd = 0;
        int b;

        int i,j;

        //ippiSub4x4_8u16s_C1R(src1, pitch1, src2, pitch2, &diffBuff[0][0], 8);
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++)
                diffBuff[i][j] = (short)(src1[j] - src2[j]);
            src1 += pitch1;
            src2 += pitch2;
        }

        for (b = 0; b < 4; b ++) {
            int a01, a23, b01, b23;

            a01 = diffBuff[b][0] + diffBuff[b][1];
            a23 = diffBuff[b][2] + diffBuff[b][3];
            b01 = diffBuff[b][0] - diffBuff[b][1];
            b23 = diffBuff[b][2] - diffBuff[b][3];
            tmpBuff[b][0] = a01 + a23;
            tmpBuff[b][1] = a01 - a23;
            tmpBuff[b][2] = b01 - b23;
            tmpBuff[b][3] = b01 + b23;
        }
        for (b = 0; b < 4; b ++) {
            int a01, a23, b01, b23;

            a01 = tmpBuff[0][b] + tmpBuff[1][b];
            a23 = tmpBuff[2][b] + tmpBuff[3][b];
            b01 = tmpBuff[0][b] - tmpBuff[1][b];
            b23 = tmpBuff[2][b] - tmpBuff[3][b];
            satd += ABS(a01 + a23) + ABS(a01 - a23) + ABS(b01 - b23) + ABS(b01 + b23);
        }

        return satd;
    }

    int satd_4x4_pitch64_px(const unsigned char* src1, const unsigned char* src2, int pitch2) {
        return satd_4x4_px(src1, 64, src2, pitch2);
    }

    int satd_4x4_pitch64_both_px(const unsigned char* src1, const unsigned char* src2) {
        return satd_4x4_px(src1, 64, src2, 64);
    }

    int satd_4x4_px(const unsigned short* src1, int pitch1, const unsigned short* src2, int pitch2)
    {
      int k, satd = 0, diff[16], m[16], d[16];

      for( k = 0; k < 16; k+=4 )
      {
        diff[k+0] = src1[0] - src2[0];
        diff[k+1] = src1[1] - src2[1];
        diff[k+2] = src1[2] - src2[2];
        diff[k+3] = src1[3] - src2[3];

        src1 += pitch1;
        src2 += pitch2;
      }

      m[ 0] = diff[ 0] + diff[12];
      m[ 1] = diff[ 1] + diff[13];
      m[ 2] = diff[ 2] + diff[14];
      m[ 3] = diff[ 3] + diff[15];
      m[ 4] = diff[ 4] + diff[ 8];
      m[ 5] = diff[ 5] + diff[ 9];
      m[ 6] = diff[ 6] + diff[10];
      m[ 7] = diff[ 7] + diff[11];
      m[ 8] = diff[ 4] - diff[ 8];
      m[ 9] = diff[ 5] - diff[ 9];
      m[10] = diff[ 6] - diff[10];
      m[11] = diff[ 7] - diff[11];
      m[12] = diff[ 0] - diff[12];
      m[13] = diff[ 1] - diff[13];
      m[14] = diff[ 2] - diff[14];
      m[15] = diff[ 3] - diff[15];

      d[ 0] = m[ 0] + m[ 4];
      d[ 1] = m[ 1] + m[ 5];
      d[ 2] = m[ 2] + m[ 6];
      d[ 3] = m[ 3] + m[ 7];
      d[ 4] = m[ 8] + m[12];
      d[ 5] = m[ 9] + m[13];
      d[ 6] = m[10] + m[14];
      d[ 7] = m[11] + m[15];
      d[ 8] = m[ 0] - m[ 4];
      d[ 9] = m[ 1] - m[ 5];
      d[10] = m[ 2] - m[ 6];
      d[11] = m[ 3] - m[ 7];
      d[12] = m[12] - m[ 8];
      d[13] = m[13] - m[ 9];
      d[14] = m[14] - m[10];
      d[15] = m[15] - m[11];

      m[ 0] = d[ 0] + d[ 3];
      m[ 1] = d[ 1] + d[ 2];
      m[ 2] = d[ 1] - d[ 2];
      m[ 3] = d[ 0] - d[ 3];
      m[ 4] = d[ 4] + d[ 7];
      m[ 5] = d[ 5] + d[ 6];
      m[ 6] = d[ 5] - d[ 6];
      m[ 7] = d[ 4] - d[ 7];
      m[ 8] = d[ 8] + d[11];
      m[ 9] = d[ 9] + d[10];
      m[10] = d[ 9] - d[10];
      m[11] = d[ 8] - d[11];
      m[12] = d[12] + d[15];
      m[13] = d[13] + d[14];
      m[14] = d[13] - d[14];
      m[15] = d[12] - d[15];

      d[ 0] = m[ 0] + m[ 1];
      d[ 1] = m[ 0] - m[ 1];
      d[ 2] = m[ 2] + m[ 3];
      d[ 3] = m[ 3] - m[ 2];
      d[ 4] = m[ 4] + m[ 5];
      d[ 5] = m[ 4] - m[ 5];
      d[ 6] = m[ 6] + m[ 7];
      d[ 7] = m[ 7] - m[ 6];
      d[ 8] = m[ 8] + m[ 9];
      d[ 9] = m[ 8] - m[ 9];
      d[10] = m[10] + m[11];
      d[11] = m[11] - m[10];
      d[12] = m[12] + m[13];
      d[13] = m[12] - m[13];
      d[14] = m[14] + m[15];
      d[15] = m[15] - m[14];

      for (k=0; k<16; ++k)
      {
        satd += abs(d[k]);
      }

      // NOTE - assume this scaling is done by caller (be consistent with 8-bit version)
      //satd = ((satd+1)>>1);

      return satd;
    }

    void satd_4x4_pair_px(const unsigned char* src1, int pitch1, const unsigned char* src2, int pitch2, int *satdPair)
    {
        satdPair[0] = satd_4x4_px(src1 + 0, pitch1, src2 + 0, pitch2);
        satdPair[1] = satd_4x4_px(src1 + 4, pitch1, src2 + 4, pitch2);
    }

    void satd_4x4_pair_pitch64_px(const unsigned char* src1, const unsigned char* src2, int pitch2, int *satdPair) {
        return satd_4x4_pair_px(src1, 64, src2, pitch2, satdPair);
    }

    void satd_4x4_pair_pitch64_both_px(const unsigned char* src1, const unsigned char* src2, int *satdPair) {
        return satd_4x4_pair_px(src1, 64, src2, 64, satdPair);
    }

    void satd_4x4_pair_px(const unsigned short* src1, int pitch1, const unsigned short* src2, int pitch2, int* satdPair)
    {
        satdPair[0] = satd_4x4_px(src1 + 0, pitch1, src2 + 0, pitch2);
        satdPair[1] = satd_4x4_px(src1 + 4, pitch1, src2 + 4, pitch2);
    }

    int satd_8x8_px(const unsigned char* src1, int pitch1, const unsigned char* src2, int pitch2)
    {
        int i, j;
        unsigned int satd = 0;
        short diff[8][8];

        //ippiSub8x8_8u16s_C1R(src1, pitch1, src2, pitch2, &diff[0][0], 16);
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++)
                diff[i][j] = (short)(src1[j] - src2[j]);
            src1 += pitch1;
            src2 += pitch2;
        }

        for (i = 0; i < 8; i++) {
            int t0 = diff[i][0] + diff[i][4];
            int t4 = diff[i][0] - diff[i][4];
            int t1 = diff[i][1] + diff[i][5];
            int t5 = diff[i][1] - diff[i][5];
            int t2 = diff[i][2] + diff[i][6];
            int t6 = diff[i][2] - diff[i][6];
            int t3 = diff[i][3] + diff[i][7];
            int t7 = diff[i][3] - diff[i][7];
            int s0 = t0 + t2;
            int s2 = t0 - t2;
            int s1 = t1 + t3;
            int s3 = t1 - t3;
            int s4 = t4 + t6;
            int s6 = t4 - t6;
            int s5 = t5 + t7;
            int s7 = t5 - t7;
            diff[i][0] = s0 + s1;
            diff[i][1] = s0 - s1;
            diff[i][2] = s2 + s3;
            diff[i][3] = s2 - s3;
            diff[i][4] = s4 + s5;
            diff[i][5] = s4 - s5;
            diff[i][6] = s6 + s7;
            diff[i][7] = s6 - s7;
        }
        for (i = 0; i < 8; i++) {
            int t0 = diff[0][i] + diff[4][i];
            int t4 = diff[0][i] - diff[4][i];
            int t1 = diff[1][i] + diff[5][i];
            int t5 = diff[1][i] - diff[5][i];
            int t2 = diff[2][i] + diff[6][i];
            int t6 = diff[2][i] - diff[6][i];
            int t3 = diff[3][i] + diff[7][i];
            int t7 = diff[3][i] - diff[7][i];
            int s0 = t0 + t2;
            int s2 = t0 - t2;
            int s1 = t1 + t3;
            int s3 = t1 - t3;
            int s4 = t4 + t6;
            int s6 = t4 - t6;
            int s5 = t5 + t7;
            int s7 = t5 - t7;
            satd += ABS(s0 + s1);
            satd += ABS(s0 - s1);
            satd += ABS(s2 + s3);
            satd += ABS(s2 - s3);
            satd += ABS(s4 + s5);
            satd += ABS(s4 - s5);
            satd += ABS(s6 + s7);
            satd += ABS(s6 - s7);
        }

        return satd;
    }

    int satd_8x8_pitch64_px(const unsigned char* src1, const unsigned char* src2, int pitch2) {
        return satd_8x8_px(src1, 64, src2, pitch2);
    }

    int satd_8x8_pitch64_both_px(const unsigned char* src1, const unsigned char* src2) {
        return satd_8x8_px(src1, 64, src2, 64);
    }

    int satd_8x8_px(const unsigned short* src1, int pitch1, const unsigned short* src2, int pitch2)
    {
      int k, i, j, jj, sad=0;
      int diff[64], m1[8][8], m2[8][8], m3[8][8];

      for( k = 0; k < 64; k += 8 )
      {
        diff[k+0] = src1[0] - src2[0];
        diff[k+1] = src1[1] - src2[1];
        diff[k+2] = src1[2] - src2[2];
        diff[k+3] = src1[3] - src2[3];
        diff[k+4] = src1[4] - src2[4];
        diff[k+5] = src1[5] - src2[5];
        diff[k+6] = src1[6] - src2[6];
        diff[k+7] = src1[7] - src2[7];

        src1 += pitch1;
        src2 += pitch2;
      }

      for (j=0; j < 8; j++)
      {
        jj = j << 3;
        m2[j][0] = diff[jj  ] + diff[jj+4];
        m2[j][1] = diff[jj+1] + diff[jj+5];
        m2[j][2] = diff[jj+2] + diff[jj+6];
        m2[j][3] = diff[jj+3] + diff[jj+7];
        m2[j][4] = diff[jj  ] - diff[jj+4];
        m2[j][5] = diff[jj+1] - diff[jj+5];
        m2[j][6] = diff[jj+2] - diff[jj+6];
        m2[j][7] = diff[jj+3] - diff[jj+7];

        m1[j][0] = m2[j][0] + m2[j][2];
        m1[j][1] = m2[j][1] + m2[j][3];
        m1[j][2] = m2[j][0] - m2[j][2];
        m1[j][3] = m2[j][1] - m2[j][3];
        m1[j][4] = m2[j][4] + m2[j][6];
        m1[j][5] = m2[j][5] + m2[j][7];
        m1[j][6] = m2[j][4] - m2[j][6];
        m1[j][7] = m2[j][5] - m2[j][7];

        m2[j][0] = m1[j][0] + m1[j][1];
        m2[j][1] = m1[j][0] - m1[j][1];
        m2[j][2] = m1[j][2] + m1[j][3];
        m2[j][3] = m1[j][2] - m1[j][3];
        m2[j][4] = m1[j][4] + m1[j][5];
        m2[j][5] = m1[j][4] - m1[j][5];
        m2[j][6] = m1[j][6] + m1[j][7];
        m2[j][7] = m1[j][6] - m1[j][7];
      }

      for (i=0; i < 8; i++)
      {
        m3[0][i] = m2[0][i] + m2[4][i];
        m3[1][i] = m2[1][i] + m2[5][i];
        m3[2][i] = m2[2][i] + m2[6][i];
        m3[3][i] = m2[3][i] + m2[7][i];
        m3[4][i] = m2[0][i] - m2[4][i];
        m3[5][i] = m2[1][i] - m2[5][i];
        m3[6][i] = m2[2][i] - m2[6][i];
        m3[7][i] = m2[3][i] - m2[7][i];

        m1[0][i] = m3[0][i] + m3[2][i];
        m1[1][i] = m3[1][i] + m3[3][i];
        m1[2][i] = m3[0][i] - m3[2][i];
        m1[3][i] = m3[1][i] - m3[3][i];
        m1[4][i] = m3[4][i] + m3[6][i];
        m1[5][i] = m3[5][i] + m3[7][i];
        m1[6][i] = m3[4][i] - m3[6][i];
        m1[7][i] = m3[5][i] - m3[7][i];

        m2[0][i] = m1[0][i] + m1[1][i];
        m2[1][i] = m1[0][i] - m1[1][i];
        m2[2][i] = m1[2][i] + m1[3][i];
        m2[3][i] = m1[2][i] - m1[3][i];
        m2[4][i] = m1[4][i] + m1[5][i];
        m2[5][i] = m1[4][i] - m1[5][i];
        m2[6][i] = m1[6][i] + m1[7][i];
        m2[7][i] = m1[6][i] - m1[7][i];
      }

      for (i = 0; i < 8; i++)
      {
        for (j = 0; j < 8; j++)
        {
          sad += abs(m2[i][j]);
        }
      }

      // NOTE - assume this scaling is done by caller (be consistent with 8-bit version)
      //sad=((sad+2)>>2);

      return sad;
    }

    void satd_8x8_pair_px(const unsigned char* src1, int pitch1, const unsigned char* src2, int pitch2, int* satdPair)
    {
        satdPair[0] = satd_8x8_px(src1 + 0, pitch1, src2 + 0, pitch2);
        satdPair[1] = satd_8x8_px(src1 + 8, pitch1, src2 + 8, pitch2);
    }

    void satd_8x8_pair_pitch64_px(const unsigned char* src1, const unsigned char* src2, int pitch2, int *satdPair) {
        return satd_8x8_pair_px(src1, 64, src2, pitch2, satdPair);
    }

    void satd_8x8_pair_pitch64_both_px(const unsigned char* src1, const unsigned char* src2, int *satdPair) {
        return satd_8x8_pair_px(src1, 64, src2, 64, satdPair);
    }

    void satd_8x8_pair_px(const unsigned short* src1, int pitch1, const unsigned short* src2, int pitch2, int* satdPair)
    {
        satdPair[0] = satd_8x8_px(src1 + 0, pitch1, src2 + 0, pitch2);
        satdPair[1] = satd_8x8_px(src1 + 8, pitch1, src2 + 8, pitch2);
    }

    void satd_8x8x2_px(const unsigned char* src1, int pitch1, const unsigned char* src2, int pitch2, int* satdPair)
    {
        satdPair[0] = satd_8x8_px(src1, pitch1, src2 + 0, pitch2);
        satdPair[1] = satd_8x8_px(src1, pitch1, src2 + 8, pitch2);
    }

    void satd_8x8x2_pitch64_px(const unsigned char* src1, const unsigned char* src2, int pitch2, int* satdPair) {
        return satd_8x8x2_px(src1, 64, src2, pitch2, satdPair);
    }


    template <int w, int h> int satd_px(const unsigned char* src1, int pitch1, const unsigned char* src2, int pitch2) {
        // assume height and width are multiple of 4
        assert(!(w & 0x03));
        assert(!(h & 0x03));

        int satdTotal = 0;
        int satd[2] = {0, 0};

        if (w == 4 && h == 4) {
            return (satd_4x4_px(src1, pitch1, src2, pitch2) + 1) >> 1;
        } else if ( (h | w) & 0x07 ) {
            // multiple 4x4 blocks - do as many pairs as possible
            int widthPair = w & ~0x07;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 4, src1 += pitch1 * 4, src2 += pitch2 * 4) {
                int i = 0;
                for (; i < widthPair; i += 4*2) {
                    satd_4x4_pair_px(src1 + i, pitch1, src2 + i, pitch2, satd);
                    satdTotal += ( (satd[0] + 1) >> 1) + ( (satd[1] + 1) >> 1 );
                }

                if (widthRem) {
                    satd[0] = satd_4x4_px(src1 + i, pitch1, src2 + i, pitch2);
                    satdTotal += (satd[0] + 1) >> 1;
                }
            }
        }
        else if (w == 8 && h == 8) {
            /* single 8x8 block */
            satd[0] = satd_8x8_px(src1, pitch1, src2, pitch2);
            satdTotal += (satd[0] + 2) >> 2;
        } else {
            /* multiple 8x8 blocks - do as many pairs as possible */
            int widthPair = w & ~0x0f;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 8, src1 += pitch1 * 8, src2 += pitch2 * 8) {
                int i = 0;
                for (; i < widthPair; i += 8*2) {
                    satd_8x8_pair_px(src1 + i, pitch1, src2 + i, pitch2, satd);
                    satdTotal += ( (satd[0] + 2) >> 2) + ( (satd[1] + 2) >> 2 );
                }
                if (widthRem) {
                    satd[0] = satd_8x8_px(src1 + i, pitch1, src2 + i, pitch2);
                    satdTotal += (satd[0] + 2) >> 2;
                }
            }
        }

        return satdTotal;
    }
    template int satd_px< 4, 4>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px< 4, 8>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px< 8, 4>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px< 8, 8>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px< 8,16>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px<16, 8>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px<16,16>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px<16,32>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px<32,16>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px<32,32>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px<32,64>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px<64,32>(const unsigned char*,int,const unsigned char*,int);
    template int satd_px<64,64>(const unsigned char*,int,const unsigned char*,int);

    template <int w, int h> int satd_pitch64_px(const unsigned char* src1, const unsigned char* src2, int pitch2) {
        // assume height and width are multiple of 4
        assert(!(w & 0x03));
        assert(!(h & 0x03));

        int satdTotal = 0;
        int satd[2] = {0, 0};

        if (w == 4 && h == 4) {
            return (satd_4x4_pitch64_px(src1, src2, pitch2) + 1) >> 1;
        } else if ( (h | w) & 0x07 ) {
            // multiple 4x4 blocks - do as many pairs as possible
            int widthPair = w & ~0x07;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 4, src1 += 64 * 4, src2 += pitch2 * 4) {
                int i = 0;
                for (; i < widthPair; i += 4*2) {
                    satd_4x4_pair_pitch64_px(src1 + i, src2 + i, pitch2, satd);
                    satdTotal += ( (satd[0] + 1) >> 1) + ( (satd[1] + 1) >> 1 );
                }

                if (widthRem) {
                    satd[0] = satd_4x4_pitch64_px(src1 + i, src2 + i, pitch2);
                    satdTotal += (satd[0] + 1) >> 1;
                }
            }
        }
        else if (w == 8 && h == 8) {
            /* single 8x8 block */
            satd[0] = satd_8x8_pitch64_px(src1, src2, pitch2);
            satdTotal += (satd[0] + 2) >> 2;
        } else {
            /* multiple 8x8 blocks - do as many pairs as possible */
            int widthPair = w & ~0x0f;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 8, src1 += 64 * 8, src2 += pitch2 * 8) {
                int i = 0;
                for (; i < widthPair; i += 8*2) {
                    satd_8x8_pair_pitch64_px(src1 + i, src2 + i, pitch2, satd);
                    satdTotal += ( (satd[0] + 2) >> 2) + ( (satd[1] + 2) >> 2 );
                }
                if (widthRem) {
                    satd[0] = satd_8x8_pitch64_px(src1 + i, src2 + i, pitch2);
                    satdTotal += (satd[0] + 2) >> 2;
                }
            }
        }

        return satdTotal;
    }
    template int satd_pitch64_px< 4, 4>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px< 4, 8>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px< 8, 4>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px< 8, 8>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px< 8,16>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px<16, 8>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px<16,16>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px<16,32>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px<32,16>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px<32,32>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px<32,64>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px<64,32>(const unsigned char*,const unsigned char*,int);
    template int satd_pitch64_px<64,64>(const unsigned char*,const unsigned char*,int);

    int satd_with_const_8x8_px(const unsigned char* src1, int pitch1, const unsigned char src2)
    {
        int i, j;
        unsigned int satd = 0;
        short diff[8][8];

        //ippiSub8x8_8u16s_C1R(src1, pitch1, src2, pitch2, &diff[0][0], 16);
        for (i = 0; i < 8; i++) {
            for (j = 0; j < 8; j++)
                diff[i][j] = (short)(src1[j] - src2);
            src1 += pitch1;
        }

        for (i = 0; i < 8; i++) {
            int t0 = diff[i][0] + diff[i][4];
            int t4 = diff[i][0] - diff[i][4];
            int t1 = diff[i][1] + diff[i][5];
            int t5 = diff[i][1] - diff[i][5];
            int t2 = diff[i][2] + diff[i][6];
            int t6 = diff[i][2] - diff[i][6];
            int t3 = diff[i][3] + diff[i][7];
            int t7 = diff[i][3] - diff[i][7];
            int s0 = t0 + t2;
            int s2 = t0 - t2;
            int s1 = t1 + t3;
            int s3 = t1 - t3;
            int s4 = t4 + t6;
            int s6 = t4 - t6;
            int s5 = t5 + t7;
            int s7 = t5 - t7;
            diff[i][0] = s0 + s1;
            diff[i][1] = s0 - s1;
            diff[i][2] = s2 + s3;
            diff[i][3] = s2 - s3;
            diff[i][4] = s4 + s5;
            diff[i][5] = s4 - s5;
            diff[i][6] = s6 + s7;
            diff[i][7] = s6 - s7;
        }
        for (i = 0; i < 8; i++) {
            int t0 = diff[0][i] + diff[4][i];
            int t4 = diff[0][i] - diff[4][i];
            int t1 = diff[1][i] + diff[5][i];
            int t5 = diff[1][i] - diff[5][i];
            int t2 = diff[2][i] + diff[6][i];
            int t6 = diff[2][i] - diff[6][i];
            int t3 = diff[3][i] + diff[7][i];
            int t7 = diff[3][i] - diff[7][i];
            int s0 = t0 + t2;
            int s2 = t0 - t2;
            int s1 = t1 + t3;
            int s3 = t1 - t3;
            int s4 = t4 + t6;
            int s6 = t4 - t6;
            int s5 = t5 + t7;
            int s7 = t5 - t7;
            satd += ABS(s0 + s1);
            satd += ABS(s0 - s1);
            satd += ABS(s2 + s3);
            satd += ABS(s2 - s3);
            satd += ABS(s4 + s5);
            satd += ABS(s4 - s5);
            satd += ABS(s6 + s7);
            satd += ABS(s6 - s7);
        }

        return satd;
    }

    void satd_with_const_8x8_pair_px(const unsigned char* src1, int pitch1, const unsigned char src2, int* satdPair)
    {
        satdPair[0] = satd_with_const_8x8_px(src1 + 0, pitch1, src2);
        satdPair[1] = satd_with_const_8x8_px(src1 + 8, pitch1, src2);
    }

    void satd_with_const_8x8_pair_pitch64_px(const unsigned char* src1, const unsigned char src2, int *satdPair) {
        return satd_with_const_8x8_pair_px(src1, 64, src2, satdPair);
    }

    template <int w, int h> int satd_with_const_pitch64_px(const unsigned char* src1, const unsigned char src2) {
        // assume height and width are multiple of 4
        assert(!(w & 0x03));
        assert(!(h & 0x03));

        int satdTotal = 0;
        int satd[2] = { 0, 0 };

        if (w == 4 && h == 4) {
            assert(0);
            return 0;
            //return (satd_4x4_pitch64_px(src1, src2, pitch2) + 1) >> 1;
        }
        else if ((h | w) & 0x07) {
            // multiple 4x4 blocks - do as many pairs as possible
            assert(0);
            return 0;
            //int widthPair = w & ~0x07;
            //int widthRem = w - widthPair;
            //for (int j = 0; j < h; j += 4, src1 += 64 * 4, src2 += pitch2 * 4) {
            //    int i = 0;
            //    for (; i < widthPair; i += 4 * 2) {
            //        satd_4x4_pair_pitch64_px(src1 + i, src2 + i, pitch2, satd);
            //        satdTotal += ((satd[0] + 1) >> 1) + ((satd[1] + 1) >> 1);
            //    }

            //    if (widthRem) {
            //        satd[0] = satd_4x4_pitch64_px(src1 + i, src2 + i, pitch2);
            //        satdTotal += (satd[0] + 1) >> 1;
            //    }
            //}
        }
        else if (w == 8 && h == 8) {
            /* single 8x8 block */
            assert(0);
            return 0;
            //satd[0] = satd_8x8_pitch64_px(src1, src2, pitch2);
            //satdTotal += (satd[0] + 2) >> 2;
        }
        else {
            /* multiple 8x8 blocks - do as many pairs as possible */
            int widthPair = w & ~0x0f;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 8, src1 += 64 * 8) {
                int i = 0;
                for (; i < widthPair; i += 8 * 2) {
                    satd_with_const_8x8_pair_pitch64_px(src1 + i, src2, satd);
                    satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
                }
                if (widthRem) {
                    assert(0);
                    //satd[0] = satd_8x8_pitch64_px(src1 + i, src2 + i, pitch2);
                    //satdTotal += (satd[0] + 2) >> 2;
                }
            }
        }

        return satdTotal;
    }
    template int satd_with_const_pitch64_px< 4, 4>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px< 4, 8>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px< 8, 4>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px< 8, 8>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px< 8, 16>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px<16, 8>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px<16, 16>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px<16, 32>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px<32, 16>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px<32, 32>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px<32, 64>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px<64, 32>(const unsigned char*, const unsigned char);
    template int satd_with_const_pitch64_px<64, 64>(const unsigned char*, const unsigned char);

    template <int w, int h> int satd_pitch64_both_px(const unsigned char* src1, const unsigned char* src2) {
        // assume height and width are multiple of 4
        assert(!(w & 0x03));
        assert(!(h & 0x03));

        int satdTotal = 0;
        int satd[2] = {0, 0};

        if (w == 4 && h == 4) {
            return (satd_4x4_pitch64_both_px(src1, src2) + 1) >> 1;
        } else if ( (h | w) & 0x07 ) {
            // multiple 4x4 blocks - do as many pairs as possible
            int widthPair = w & ~0x07;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 4, src1 += 64 * 4, src2 += 64 * 4) {
                int i = 0;
                for (; i < widthPair; i += 4*2) {
                    satd_4x4_pair_pitch64_both_px(src1 + i, src2 + i, satd);
                    satdTotal += ( (satd[0] + 1) >> 1) + ( (satd[1] + 1) >> 1 );
                }

                if (widthRem) {
                    satd[0] = satd_4x4_pitch64_both_px(src1 + i, src2 + i);
                    satdTotal += (satd[0] + 1) >> 1;
                }
            }
        }
        else if (w == 8 && h == 8) {
            /* single 8x8 block */
            satd[0] = satd_8x8_pitch64_both_px(src1, src2);
            satdTotal += (satd[0] + 2) >> 2;
        } else {
            /* multiple 8x8 blocks - do as many pairs as possible */
            int widthPair = w & ~0x0f;
            int widthRem = w - widthPair;
            for (int j = 0; j < h; j += 8, src1 += 64 * 8, src2 += 64 * 8) {
                int i = 0;
                for (; i < widthPair; i += 8*2) {
                    satd_8x8_pair_pitch64_both_px(src1 + i, src2 + i, satd);
                    satdTotal += ( (satd[0] + 2) >> 2) + ( (satd[1] + 2) >> 2 );
                }
                if (widthRem) {
                    satd[0] = satd_8x8_pitch64_both_px(src1 + i, src2 + i);
                    satdTotal += (satd[0] + 2) >> 2;
                }
            }
        }

        return satdTotal;
    }
    template int satd_pitch64_both_px< 4, 4>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px< 4, 8>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px< 8, 4>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px< 8, 8>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px< 8,16>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px<16, 8>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px<16,16>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px<16,32>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px<32,16>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px<32,32>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px<32,64>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px<64,32>(const unsigned char*,const unsigned char*);
    template int satd_pitch64_both_px<64,64>(const unsigned char*,const unsigned char*);
};  // namespace AV1PP
