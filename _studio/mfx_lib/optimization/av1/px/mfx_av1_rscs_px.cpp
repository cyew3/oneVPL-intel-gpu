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

namespace AV1PP
{
    void compute_rscs_px(const unsigned char *ySrc, int pitchSrc, int *lcuRs, int *lcuCs, int pitchRsCs, int width, int height)
    {
        for(int i=0; i<height; i+=4)
        {
            for(int j=0; j<width; j+=4)
            {
                int Rs=0;
                int Cs=0;
                for(int k=0; k<4; k++)
                {
                    for(int l=0; l<4; l++)
                    {
                        int temp = ySrc[(i+k)*pitchSrc+(j+l)]-ySrc[(i+k)*pitchSrc+(j+l-1)];
                        Cs += temp*temp;

                        temp = ySrc[(i+k)*pitchSrc+(j+l)]-ySrc[(i+k-1)*pitchSrc+(j+l)];
                        Rs += temp*temp;
                    }
                }
                lcuCs[(i>>2)*pitchRsCs+(j>>2)] = Cs>>4;
                lcuRs[(i>>2)*pitchRsCs+(j>>2)] = Rs>>4;
            }
        }
    }

    void compute_rscs_diff_px(float* pRs0, float* pCs0, float* pRs1, float* pCs1, int len, float* pRsDiff, float* pCsDiff)
    {
        //int len = wblocks * hblocks;
        double accRs = 0.0;
        double accCs = 0.0;

        for (int i = 0; i < len; i++) {
            accRs += (pRs0[i] - pRs1[i]) * (pRs0[i] - pRs1[i]);
            accCs += (pCs0[i] - pCs1[i]) * (pCs0[i] - pCs1[i]);
        }
        *pRsDiff = (float)accRs;
        *pCsDiff = (float)accCs;
    }


    void compute_rscs_4x4_px(const unsigned char* pSrc, int srcPitch, int wblocks, int hblocks, float* pRs, float* pCs)
    {
        for (int i = 0; i < hblocks; i++) {
            for (int j = 0; j < wblocks; j++) {
                int accRs = 0;
                int accCs = 0;

                for (int k = 0; k < 4; k++) {
                    for (int l = 0; l < 4; l++) {
                        int dRs = pSrc[l] - pSrc[l - srcPitch];
                        int dCs = pSrc[l] - pSrc[l - 1];
                        accRs += dRs * dRs;
                        accCs += dCs * dCs;
                    }
                    pSrc += srcPitch;
                }
                pRs[i * wblocks + j] = accRs * (1.0f / 16);
                pCs[i * wblocks + j] = accCs * (1.0f / 16);

                pSrc -= 4 * srcPitch;
                pSrc += 4;
            }
            pSrc -= 4 * wblocks;
            pSrc += 4 * srcPitch;
        }
    }
}

