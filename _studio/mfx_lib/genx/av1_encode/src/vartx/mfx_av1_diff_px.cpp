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

namespace details {
    template <typename PixType>
    void diff_nxn_px(const PixType *src, int pitchSrc, const PixType *pred, int pitchPred, short *diff, int pitchDiff, int size)
    {
        for (int j = 0; j < size; j++)
            for (int i = 0; i < size; i++)
                diff[j*pitchDiff+i] = (short)src[j*pitchSrc+i] - pred[j*pitchPred+i];
    }

    template <typename PixType>
    void diff_nxm_px(const PixType *src, int pitchSrc, const PixType *pred, int pitchPred, short *diff, int pitchDiff, int width, int height)
    {
        for (int j = 0; j < height; j++)
            for (int i = 0; i < width; i++)
                diff[j*pitchDiff+i] = (short)src[j*pitchSrc+i] - pred[j*pitchPred+i];
    }

    template <class PixType>
    void diff_nv12_px(const PixType *src, int pitchSrc, const PixType *pred, int pitchPred, short *diff1, short *diff2, int pitchDiff, int width, int height)
    {
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                diff1[i] = (short)src[i*2+0] - pred[i*2+0];
                diff2[i] = (short)src[i*2+1] - pred[i*2+1];
            }
            diff1 += pitchDiff;
            diff2 += pitchDiff;
            src += pitchSrc;
            pred += pitchPred;
        }

    }
    template void diff_nv12_px<unsigned char>(const unsigned char*,int,const unsigned char*,int,short*,short*,int,int,int);
    template void diff_nv12_px<unsigned short>(const unsigned short*,int,const unsigned short*,int,short*,short*,int,int,int);
}; // namespace details

namespace VP9PP
{
    template <int w> void diff_nxn_px(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst) {
        details::diff_nxn_px(src1, pitchSrc1, src2, pitchSrc2, dst, pitchDst, w);
    }
    template void diff_nxn_px<4>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst);
    template void diff_nxn_px<8>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst);
    template void diff_nxn_px<16>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst);
    template void diff_nxn_px<32>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst);
    template void diff_nxn_px<64>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst);

    template <int w> void diff_nxn_p64_p64_pw_px(const unsigned char *src1, const unsigned char *src2, short *dst) {
        details::diff_nxn_px(src1, 64, src2, 64, dst, w, w);
    }
    template void diff_nxn_p64_p64_pw_px<4>(const unsigned char *src1, const unsigned char *src2, short *dst);
    template void diff_nxn_p64_p64_pw_px<8>(const unsigned char *src1, const unsigned char *src2, short *dst);
    template void diff_nxn_p64_p64_pw_px<16>(const unsigned char *src1, const unsigned char *src2, short *dst);
    template void diff_nxn_p64_p64_pw_px<32>(const unsigned char *src1, const unsigned char *src2, short *dst);
    template void diff_nxn_p64_p64_pw_px<64>(const unsigned char *src1, const unsigned char *src2, short *dst);

    template <int w> void diff_nxm_px(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst, int h) {
        details::diff_nxm_px(src1, pitchSrc1, src2, pitchSrc2, dst, pitchDst, w, h);
    }
    template void diff_nxm_px<4>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst, int h);
    template void diff_nxm_px<8>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst, int h);
    template void diff_nxm_px<16>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst, int h);
    template void diff_nxm_px<32>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst, int h);
    template void diff_nxm_px<64>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dst, int pitchDst, int h);

    template <int w> void diff_nxm_p64_p64_pw_px(const unsigned char *src1, const unsigned char *src2, short *dst, int h) {
        details::diff_nxm_px(src1, 64, src2, 64, dst, w, w, h);
    }
    template void diff_nxm_p64_p64_pw_px<4>(const unsigned char *src1, const unsigned char *src2, short *dst, int h);
    template void diff_nxm_p64_p64_pw_px<8>(const unsigned char *src1, const unsigned char *src2, short *dst, int h);
    template void diff_nxm_p64_p64_pw_px<16>(const unsigned char *src1, const unsigned char *src2, short *dst, int h);
    template void diff_nxm_p64_p64_pw_px<32>(const unsigned char *src1, const unsigned char *src2, short *dst, int h);
    template void diff_nxm_p64_p64_pw_px<64>(const unsigned char *src1, const unsigned char *src2, short *dst, int h);

    template <int w> void diff_nv12_px(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dstU, short *dstV, int pitchDst, int h) {
        details::diff_nv12_px(src1, pitchSrc1, src2, pitchSrc2, dstU, dstV, pitchDst, w, h);
    }
    template void diff_nv12_px<4>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dstU, short *dstV, int pitchDst, int h);
    template void diff_nv12_px<8>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dstU, short *dstV, int pitchDst, int h);
    template void diff_nv12_px<16>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dstU, short *dstV, int pitchDst, int h);
    template void diff_nv12_px<32>(const unsigned char *src1, int pitchSrc1, const unsigned char *src2, int pitchSrc2, short *dstU, short *dstV, int pitchDst, int h);

    template <int w> void diff_nv12_p64_p64_pw_px(const unsigned char *src1, const unsigned char *src2, short *dstU, short *dstV, int h) {
        details::diff_nv12_px(src1, 64, src2, 64, dstU, dstV, w, w, h);
    }
    template void diff_nv12_p64_p64_pw_px<4>(const unsigned char *src1, const unsigned char *src2, short *dstU, short *dstV, int h);
    template void diff_nv12_p64_p64_pw_px<8>(const unsigned char *src1, const unsigned char *src2, short *dstU, short *dstV, int h);
    template void diff_nv12_p64_p64_pw_px<16>(const unsigned char *src1, const unsigned char *src2, short *dstU, short *dstV, int h);
    template void diff_nv12_p64_p64_pw_px<32>(const unsigned char *src1, const unsigned char *src2, short *dstU, short *dstV, int h);

    void diff_histogram_px(unsigned char* pSrc, unsigned char* pRef, int pitch, int width, int height, int histogram[5], long long *pSrcDC, long long *pRefDC)
    {
        enum {HIST_THRESH_LO = 4, HIST_THRESH_HI = 12};
        long long srcDC = 0;
        long long refDC = 0;

        histogram[0] = 0;
        histogram[1] = 0;
        histogram[2] = 0;
        histogram[3] = 0;
        histogram[4] = 0;

        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                int s = pSrc[j];
                int r = pRef[j];
                int d = s - r;

                srcDC += s;
                refDC += r;

                if (d < -HIST_THRESH_HI)
                    histogram[0]++;
                else if (d < -HIST_THRESH_LO)
                    histogram[1]++;
                else if (d < HIST_THRESH_LO)
                    histogram[2]++;
                else if (d < HIST_THRESH_HI)
                    histogram[3]++;
                else
                    histogram[4]++;
            }
            pSrc += pitch;
            pRef += pitch;
        }
        *pSrcDC = srcDC;
        *pRefDC = refDC;
    }
}
