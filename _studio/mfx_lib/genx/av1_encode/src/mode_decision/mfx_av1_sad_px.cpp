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

#define ABS(v) ((v) < 0 ? -(v) : (v))

namespace VP9PP
{
    namespace details {
        template <typename PixType, int w, int h>
        int sad_general_px(const PixType *p1, int pitch1, const PixType *p2, int pitch2) {
            int sum = 0;
            for (int i = 0; i < h; i++, p1+=pitch1, p2+=pitch2)
                for (int j = 0; j < w; j++)
                    sum += ABS(p1[j] - p2[j]);
            return sum;
        }

        template <typename PixType, int w, int h>
        int sad_special_px(const PixType *p1, int pitch1, const PixType *p2) {
            return sad_general_px<PixType, w, h>(p1, pitch1, p2, 64);
        }
    }; // namespace details

    template <int w, int h> int sad_general_px(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2) {
        return details::sad_general_px<unsigned char, w, h>(p1, pitch1, p2, pitch2);
    }
    template int sad_general_px< 4, 4>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px< 4, 8>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px< 8, 4>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px< 8, 8>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px< 8,16>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px<16, 8>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px<16,16>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px<16,32>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px<32,16>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px<32,32>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px<32,64>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px<64,32>(const unsigned char*,int,const unsigned char*,int);
    template int sad_general_px<64,64>(const unsigned char*,int,const unsigned char*,int);

    template <int w, int h> int sad_special_px(const unsigned char *p1, int pitch1, const unsigned char *p2) {
        return details::sad_special_px<unsigned char, w, h>(p1, pitch1, p2);
    }
    template int sad_special_px< 4, 4>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px< 4, 8>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px< 8, 4>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px< 8, 8>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px< 8,16>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px<16, 8>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px<16,16>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px<16,32>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px<32,16>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px<32,32>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px<32,64>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px<64,32>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px<64,48>(const unsigned char*,int,const unsigned char*);
    template int sad_special_px<64,64>(const unsigned char*,int,const unsigned char*);

    template <int size> int sad_store8x8_px(const unsigned char *p1, const unsigned char *p2, int *sads8x8) {
        int sad = 0;
        for (int y = 0; y < size; y += 8) {
            for (int x = 0; x < size; x += 8) {
                const int sad8x8 = sad_general_px<8, 8>(p1 + 64 * y + x, 64, p2 + 64 * y + x, 64);
                sads8x8[8 * (y>>3) + (x>>3)] = sad8x8;
                sad += sad8x8;
            }
        }
        return sad;
    }
    template int sad_store8x8_px<8> (const unsigned char *p1, const unsigned char *p2, int *sads8x8);
    template int sad_store8x8_px<16>(const unsigned char *p1, const unsigned char *p2, int *sads8x8);
    template int sad_store8x8_px<32>(const unsigned char *p1, const unsigned char *p2, int *sads8x8);
    template int sad_store8x8_px<64>(const unsigned char *p1, const unsigned char *p2, int *sads8x8);


    void search_best_block8x8_px(unsigned char *pSrc, unsigned char *pRef, int pitch, int xrange, int yrange, unsigned int *bestSAD, int *bestX, int *bestY) {
        enum {SAD_SEARCH_VSTEP  = 2};  // 1=FS 2=FHS
        for (int y = 0; y < yrange; y += SAD_SEARCH_VSTEP) {
            for (int x = 0; x < xrange; x++) {
                unsigned char* pr = pRef + (y * pitch) + x;
                unsigned char* ps = pSrc;
                unsigned int sad = 0;
                for (int i = 0; i < 8; i++) {
                    sad += ABS(pr[0] - ps[0]);
                    sad += ABS(pr[1] - ps[1]);
                    sad += ABS(pr[2] - ps[2]);
                    sad += ABS(pr[3] - ps[3]);
                    sad += ABS(pr[4] - ps[4]);
                    sad += ABS(pr[5] - ps[5]);
                    sad += ABS(pr[6] - ps[6]);
                    sad += ABS(pr[7] - ps[7]);
                    pr += pitch;
                    ps += pitch;
                }
                if (sad < *bestSAD) {
                    *bestSAD = sad;
                    *bestX = x;
                    *bestY = y;
                }
            }
        }
    }
}
