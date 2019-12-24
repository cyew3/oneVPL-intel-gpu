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

#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#include <cm/cm.h>
#include <cm/cmtl.h>

#define MEPU8 1
#define NEWMVPRED 0

typedef char  int1;
typedef short int2;
typedef int   int4;
typedef unsigned char  uint1;
typedef unsigned short uint2;
typedef unsigned int   uint4;

const int1 COEFS_AV1[4][4] = {
    { /*0, 0,*/  0, 64,  0,  0, /*0, 0*/ },
    { /*0, 1,*/ -7, 55, 19, -5, /*1, 0*/ },
    { /*0, 1,*/ -7, 38, 38, -7, /*1, 0*/ },
    { /*0, 1,*/ -5, 19, 55, -7, /*1, 0*/ },
};

static const uint1 MV_DELTA[8] = {
        1,  2,
    4,  5,  6,
    8,  9, 10
};

enum {
    SINGLE_REF,
    TWO_REFS,
    COMPOUND
};

_GENX_ matrix<uint1,8,8>   g_pred8;
_GENX_ matrix<uint1,16,16> g_pred16;

enum { X, Y };


_GENX_ inline vector<uint2,16> sad8x8(matrix_ref<uint1,8,8> src, matrix_ref<uint1,8,8> ref)
{
#if defined(target_gen12) || defined(target_gen12lp)
    vector<uint2, 16> sad = cm_abs<uint2>(src.select<2, 1, 8, 1>(0, 0) - ref.select<2, 1, 8, 1>(0, 0));
    sad = cm_abs<uint2>(src.select<2, 1, 8, 1>(2, 0) - ref.select<2, 1, 8, 1>(2, 0)) + sad;
    sad = cm_abs<uint2>(src.select<2, 1, 8, 1>(4, 0) - ref.select<2, 1, 8, 1>(4, 0)) + sad;
    sad = cm_abs<uint2>(src.select<2, 1, 8, 1>(6, 0) - ref.select<2, 1, 8, 1>(6, 0)) + sad;
    sad.select<8, 2>(0) += sad.select<8, 2>(1);
#else // target_gen12
    vector<uint2,16> sad = cm_sad2<uint2>(src.select<2,1,8,1>(0,0), ref.select<2,1,8,1>(0,0));
    sad = cm_sada2<uint2>(src.select<2,1,8,1>(2,0), ref.select<2,1,8,1>(2,0), sad);
    sad = cm_sada2<uint2>(src.select<2,1,8,1>(4,0), ref.select<2,1,8,1>(4,0), sad);
    sad = cm_sada2<uint2>(src.select<2,1,8,1>(6,0), ref.select<2,1,8,1>(6,0), sad);
#endif // target_gen12
    return sad;
}

_GENX_ inline vector<uint2,16> sad16x16(matrix_ref<uint1,16,16> m1, matrix_ref<uint1,16,16> m2)
{
#if defined(target_gen12) || defined(target_gen12lp)
    vector<uint2, 16> sad = cm_abs<uint2>(m1.row(0) - m2.row(0));
    sad = cm_abs<uint2>(m1.row(1) - m2.row(1)) + sad;
    sad = cm_abs<uint2>(m1.row(2) - m2.row(2)) + sad;
    sad = cm_abs<uint2>(m1.row(3) - m2.row(3)) + sad;
    sad = cm_abs<uint2>(m1.row(4) - m2.row(4)) + sad;
    sad = cm_abs<uint2>(m1.row(5) - m2.row(5)) + sad;
    sad = cm_abs<uint2>(m1.row(6) - m2.row(6)) + sad;
    sad = cm_abs<uint2>(m1.row(7) - m2.row(7)) + sad;
    sad = cm_abs<uint2>(m1.row(8) - m2.row(8)) + sad;
    sad = cm_abs<uint2>(m1.row(9) - m2.row(9)) + sad;
    sad = cm_abs<uint2>(m1.row(10) - m2.row(10)) + sad;
    sad = cm_abs<uint2>(m1.row(11) - m2.row(11)) + sad;
    sad = cm_abs<uint2>(m1.row(12) - m2.row(12)) + sad;
    sad = cm_abs<uint2>(m1.row(13) - m2.row(13)) + sad;
    sad = cm_abs<uint2>(m1.row(14) - m2.row(14)) + sad;
    sad = cm_abs<uint2>(m1.row(15) - m2.row(15)) + sad;
    sad.select<8, 2>(0) += sad.select<8, 2>(1);
#else // target_gen12
    vector<uint2,16> sad = cm_sad2<uint2>(m1.row(0), m2.row(0));
    sad = cm_sada2<uint2>(m1.row(1),  m2.row(1),  sad);
    sad = cm_sada2<uint2>(m1.row(2),  m2.row(2),  sad);
    sad = cm_sada2<uint2>(m1.row(3),  m2.row(3),  sad);
    sad = cm_sada2<uint2>(m1.row(4),  m2.row(4),  sad);
    sad = cm_sada2<uint2>(m1.row(5),  m2.row(5),  sad);
    sad = cm_sada2<uint2>(m1.row(6),  m2.row(6),  sad);
    sad = cm_sada2<uint2>(m1.row(7),  m2.row(7),  sad);
    sad = cm_sada2<uint2>(m1.row(8),  m2.row(8),  sad);
    sad = cm_sada2<uint2>(m1.row(9),  m2.row(9),  sad);
    sad = cm_sada2<uint2>(m1.row(10), m2.row(10), sad);
    sad = cm_sada2<uint2>(m1.row(11), m2.row(11), sad);
    sad = cm_sada2<uint2>(m1.row(12), m2.row(12), sad);
    sad = cm_sada2<uint2>(m1.row(13), m2.row(13), sad);
    sad = cm_sada2<uint2>(m1.row(14), m2.row(14), sad);
    sad = cm_sada2<uint2>(m1.row(15), m2.row(15), sad);
#endif // target_gen12
    return sad;
}

inline _GENX_ void Interpolate8(SurfaceIndex REF, vector_ref<int2,2> pos, matrix_ref<int1,2,4> filt)
{
    matrix<uint1,14,16> in;
    matrix<int2,4,8> acc;
    matrix<uint1,14,8> tmpHor;

    if ((filt(X,1) & filt(Y,1)) == 64) {
        read(REF, pos(X) + 2, pos(Y) + 2, g_pred8);
        return;
    }

    // hor interpolation
    if (filt(X,1) == 64) {
        read(REF, pos(X) + 2, pos(Y), tmpHor);
    } else {
        read(REF, pos(X), pos(Y), in);
        #pragma unroll
        for (int i = 0; i < 12; i += 4) {
            #pragma unroll
            for (int j = 0; j < 4; j += 2) {
                acc.select<2,1,8,1>(j)  = in.select<2,1,8,1>(i+j,0) + in.select<2,1,8,1>(i+j,5);
                acc.select<2,1,8,1>(j) += in.select<2,1,8,1>(i+j,1) * filt(X,2-2);
                acc.select<2,1,8,1>(j) += in.select<2,1,8,1>(i+j,2) * filt(X,3-2);
                acc.select<2,1,8,1>(j) += in.select<2,1,8,1>(i+j,3) * filt(X,4-2);
                acc.select<2,1,8,1>(j) += in.select<2,1,8,1>(i+j,4) * filt(X,5-2);
                acc.select<2,1,8,1>(j) += 32;
            }
            tmpHor.select<4,1,8,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
        acc.select<2,1,8,1>(0)  = in.select<2,1,8,1>(12,0) + in.select<2,1,8,1>(12,5);
        acc.select<2,1,8,1>(0) += in.select<2,1,8,1>(12,1) * filt(X,2-2);
        acc.select<2,1,8,1>(0) += in.select<2,1,8,1>(12,2) * filt(X,3-2);
        acc.select<2,1,8,1>(0) += in.select<2,1,8,1>(12,3) * filt(X,4-2);
        acc.select<2,1,8,1>(0) += in.select<2,1,8,1>(12,4) * filt(X,5-2);
        acc.select<2,1,8,1>(0) += 32;
        tmpHor.select<2,1,8,1>(12) = cm_shr<uint1>(acc.select<2,1,8,1>(0), 6, SAT);
    }

    // vert interpolation
    if (filt(Y,1) == 64) {
        g_pred8 = tmpHor.select<8,1,8,1>(2,0);
    } else {
        #pragma unroll
        for (int i = 0; i < 8; i += 4) {
            #pragma unroll
            for (int j = 0; j < 4; j += 2) {
                acc.select<2,1,8,1>(j)  = tmpHor.select<2,1,8,1>(i+j+0) + tmpHor.select<2,1,8,1>(i+j+5);
                acc.select<2,1,8,1>(j) += tmpHor.select<2,1,8,1>(i+j+1) * filt(Y,2-2);
                acc.select<2,1,8,1>(j) += tmpHor.select<2,1,8,1>(i+j+2) * filt(Y,3-2);
                acc.select<2,1,8,1>(j) += tmpHor.select<2,1,8,1>(i+j+3) * filt(Y,4-2);
                acc.select<2,1,8,1>(j) += tmpHor.select<2,1,8,1>(i+j+4) * filt(Y,5-2);
                acc.select<2,1,8,1>(j) += 32;
            }
            g_pred8.select<4,1,8,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
    }
}

inline _GENX_ void Interpolate16(SurfaceIndex REF, vector_ref<int2,2> pos, matrix_ref<int1,2,4> filt)
{
    matrix<uint1,22,32> in;
    matrix<uint1,22,16> tmpHor;
    matrix<int2,2,16> acc;

    if ((filt(X,1) & filt(Y,1)) == 64) {
        read(REF, pos(X) + 2, pos(Y) + 2, g_pred16);
        return;
    }

    if (filt(X,1) == 64) {
        read(REF, pos(X) + 2, pos(Y) + 0,  tmpHor.select<16,1,16,1>(0));
        read(REF, pos(X) + 2, pos(Y) + 16, tmpHor.select<5,1,16,1>(16));
    } else {
        read(REF, pos(X), pos(Y) + 0,  in.select<8,1,32,1>(0));
        read(REF, pos(X), pos(Y) + 8,  in.select<8,1,32,1>(8));
        read(REF, pos(X), pos(Y) + 16, in.select<5,1,32,1>(16));
        #pragma unroll
        for (int i = 0; i < 22; i += 2) {
            #pragma unroll
            for (int j = 0; j < 2; j++) {
                acc.row(j)  = in.select<1,1,16,1>(i+j,1-1) + in.select<1,1,16,1>(i+j,6-1);
                acc.row(j) += in.select<1,1,16,1>(i+j,2-1) * filt(X,2-2);
                acc.row(j) += in.select<1,1,16,1>(i+j,3-1) * filt(X,3-2);
                acc.row(j) += in.select<1,1,16,1>(i+j,4-1) * filt(X,4-2);
                acc.row(j) += in.select<1,1,16,1>(i+j,5-1) * filt(X,5-2);
                acc.row(j) += 32;
            }
            tmpHor.select<2,1,16,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
    }

    // ref0 vert interpolation
    if (filt(Y,1) == 64) {
        g_pred16 = tmpHor.select<16,1,16,1>(2,0);
    } else {
        #pragma unroll
        for (int i = 0; i < 16; i+=2) {
            #pragma unroll
            for (int j = 0; j < 2; j++) {
                acc.row(j)  = tmpHor.row(i+j+1-1) + tmpHor.row(i+j+6-1);
                acc.row(j) += tmpHor.row(i+j+2-1) * filt(Y,2-2);
                acc.row(j) += tmpHor.row(i+j+3-1) * filt(Y,3-2);
                acc.row(j) += tmpHor.row(i+j+4-1) * filt(Y,4-2);
                acc.row(j) += tmpHor.row(i+j+5-1) * filt(Y,5-2);
                acc.row(j) += 32;
            }
            g_pred16.select<2,1,16,1>(i) = cm_shr<uint1>(acc, 6, SAT);
        }
    }
}


extern "C" _GENX_MAIN_
void MePuGacc8x8And16x16(SurfaceIndex SRC, SurfaceIndex REF0, SurfaceIndex REF1,
                         SurfaceIndex MEDATA8_0, SurfaceIndex MEDATA8_1, SurfaceIndex MEDATA16_0, SurfaceIndex MEDATA16_1,
                         SurfaceIndex OUT_PRED8, SurfaceIndex OUT_PRED16, uint4 refConfig)
{
    vector<int2,2> origin;
    origin(X) = get_thread_origin_x();
    origin(Y) = get_thread_origin_y();

    vector<int4,2> origxy = origin;
    origxy(X) *= 8;

    vector<int4,2> pel = origin * 16;

    vector<int4,2> pel_minus2 = pel - 2;

    // make coefs
    matrix<int1,4,4> coefs(COEFS_AV1);

    // read source
    matrix<uint1,16,16> src16x16;
    read(SRC, pel(X), pel(Y), src16x16);

    const uint4 BLK_SIZE = 16;
    matrix<uint2,4,16> sads;

    // read motion vectors and prepare filters
    matrix<int2,2,4> medata;
    read(MEDATA16_0, origxy(X), origxy(Y), medata.select<1,1,4,1>(0));
    read(MEDATA16_1, origxy(X), origxy(Y), medata.select<1,1,4,1>(1));
    matrix_ref<int2,2,2> mv = medata.select<2,1,2,1>();
    vector<uint2,4> dmv = mv & 3;
    matrix<int1,4,4> filt;
    filt.format<uint4>() = coefs.format<uint4>().iselect(dmv);

    // load reference blocks 16x16
    vector<int2,4> pos = pel_minus2.replicate<2>() + cm_shr<int2>(mv, 2);

    // ref0
    matrix<uint1,16,16> pred0;
    Interpolate16(REF0, pos.select<2,1>(0), filt.select<2,1,4,1>(0));
    sads.row(0) = sad16x16(src16x16, g_pred16);
    pred0 = g_pred16;

    matrix<uint1,16,16> pred1, pred2;
    if (refConfig != SINGLE_REF) {
        // ref1
        Interpolate16(REF1, pos.select<2, 1>(2), filt.select<2, 1, 4, 1>(2));
        sads.row(1) = sad16x16(src16x16, g_pred16);
        pred1 = g_pred16;

        // Compound
        if (refConfig == COMPOUND) {
            pred2 = cm_avg<uint1>(pred0, pred1);
            sads.row(2) = sad16x16(src16x16, pred2);
        }
    }

    // accumulate SADs
    vector<uint2,16> tmp1  = sads.select<4,1,4,4>(0,0) + sads.select<4,1,4,4>(0,2);
    vector<uint2,8>  tmp2  = tmp1.select<8,2>(0) + tmp1.select<8,2>(1);
    vector<uint4, 4> costs = tmp2.select<4,2>(0) + tmp2.select<4,2>(1);

    // get min SAD
    costs <<= 2;
    costs(1) += 1;
    costs(2) += 2;
    if (refConfig != SINGLE_REF) {
        costs(0) = cm_min<uint4>(costs(0), costs(1));
        if (refConfig == COMPOUND)
            costs(0) = cm_min<uint4>(costs(0), costs(2));
    }

    uint4 idx = costs(0) & 3;

    // best candidate report (refIdx & predictor)
    if (idx == 1)
        pred0 = pred1;
    else if (idx == 2)
        pred0 = pred2;
#if NEWMVPRED
    write(OUT_PRED16, pel(X), pel(Y), pred0);
#endif
    medata.row(0).format<int4>()[1] = costs(0);
    write(MEDATA16_0, origxy(X), origxy(Y), medata.select<1,1,4,1>());

#if MEPU8
    // 8x8 4 times
    origxy *= 2;
    matrix<uint1,4,16> medata8;
    read(MEDATA8_0, origxy(X), origxy(Y), medata8.select<2,1,16,1>(0));
    read(MEDATA8_1, origxy(X), origxy(Y), medata8.select<2,1,16,1>(2));
    matrix<int2,4,4> mvs = medata8.format<int2,8,4>().select<8,1,2,1>();

    matrix<int2,4,4> intmvs = mvs >> 2;
    matrix<uint2,4,4> dmvs = mvs & 3;

    matrix<int1,8,8> filts;
    filts.format<uint4>() = coefs.format<uint4>().iselect(dmvs.format<uint2>());

    matrix<int4,2,4> pel8 = pel.replicate<4>();
    pel8.select<2,1,1,1>(0,2) += 8;
    pel8.select<1,1,2,2>(1,1) += 8;

    matrix<int2,4,4> pos8 = pel8.replicate<2>() + intmvs;
    pos8 -= 2;

    #pragma unroll
    for (int i = 0; i < 2; i++) {
        #pragma unroll
        for (int j = 0; j < 2; j++) {
            matrix_ref<uint1,8,8> src = src16x16.select<8,1,8,1>(8*i, 8*j);

            matrix<int1,2,4> filt0 = filts.select<1,1,8,1>(2*i+j+0);
            matrix<int1,2,4> filt1 = filts.select<1,1,8,1>(2*i+j+4);

            vector<int2,2> pos0 = pos8.select<1,1,2,1>(i+0,2*j);
            vector<int2,2> pos1 = pos8.select<1,1,2,1>(i+2,2*j);

            // ref0 interpolation
            matrix<uint1,8,8> pred0;
            Interpolate8(REF0, pos0, filt0);
            sads.row(0) = sad8x8(src, g_pred8);
            pred0 = g_pred8;

            matrix<uint1,8,8> pred1, pred2;
            if (refConfig != SINGLE_REF) {
                // ref1 interpolation
                Interpolate8(REF1, pos1, filt1);
                sads.row(1) = sad8x8(src, g_pred8);
                pred1 = g_pred8;

                // Compound
                if (refConfig == COMPOUND) {
                    pred2 = cm_avg<uint1>(pred0, pred1);
                    sads.row(2) = sad8x8(src, pred2);
                }
            }

            // accumulate SADs
            vector<uint2,16> tmp1  = sads.select<4,1,4,4>(0,0) + sads.select<4,1,4,4>(0,2);
            vector<uint2,8>  tmp2  = tmp1.select<8,2>(0) + tmp1.select<8,2>(1);
            vector<uint2,4>  costs = tmp2.select<4,2>(0) + tmp2.select<4,2>(1);

            // get min SAD
            costs <<= 2;
            costs(1) += 1;
            costs(2) += 2;
            if (refConfig != SINGLE_REF) {
                costs(0) = cm_min<uint2>(costs(0), costs(1));
                if (refConfig == COMPOUND)
                    costs(0) = cm_min<uint2>(costs(0), costs(2));
            }
#if NEWMVPRED
            uint2 idx = costs(0) & 3;

            if (idx == 1)
                pred0 = pred1;
            else if (idx == 2)
                pred0 = pred2;

            write(OUT_PRED8, pel8(i,2*j+X), pel8(i,2*j+Y), pred0);
#endif
            medata8.format<uint4>()[1 + 4 * i + 2 * j] = costs(0);
        }
    }
    write(MEDATA8_0, origxy(X), origxy(Y), medata8.select<2,1,16,1>());
#endif
}

extern "C" _GENX_MAIN_
void MePuGacc32x32(SurfaceIndex SRC, SurfaceIndex REF0, SurfaceIndex REF1, SurfaceIndex MEDATA0, SurfaceIndex MEDATA1,
                   SurfaceIndex OUT_PRED, uint4 refConfig)
{
    vector<int2,2> origin;
    origin(X) = get_thread_origin_x();
    origin(Y) = get_thread_origin_y();

    vector<int4,2> origxy = origin;
    origxy(X) *= 64;

    vector<int4,2> pel = origin * 32;

    vector<int4,2> pel_minus2 = pel - 2;

    matrix<uint1,2,8> medata;        // mv & dist0
    vector<uint4,16>  medataDist8;  // 8 more dists

    // read motion vectors and prepare filters
    read(MEDATA0, origxy(X) + 0, origxy(Y), medata.row(0));
    read(MEDATA1, origxy(X) + 0, origxy(Y), medata.row(1));
    read(MEDATA0, origxy(X) + 8, origxy(Y), medataDist8.select<8,1>(0));
    read(MEDATA1, origxy(X) + 8, origxy(Y), medataDist8.select<8,1>(8));
    vector<uint4,2> medataDist0 = medata.format<uint4>().select<2,2>(1) << 4;
    medataDist8 <<= 4;
    vector<uint2,8> mv_delta(MV_DELTA);
    medataDist8 += mv_delta.replicate<2>();

    vector<uint4,8> tmp8 = cm_min<uint4>(medataDist8.select<8,2>(0), medataDist8.select<8,2>(1));
    vector<uint4,4> tmp4 = cm_min<uint4>(tmp8.select<4,2>(0), tmp8.select<4,2>(1));
    vector<uint4,2> minCosts = cm_min<uint4>(tmp4.select<2,2>(0), tmp4.select<2,2>(1));
    minCosts = cm_min<uint4>(minCosts, medataDist0);

    vector<uint4,2> bestIdx = minCosts & 15;
    vector<int2,4> dxdy;
    dxdy.select<2,2>(X) = bestIdx  & 3;
    dxdy.select<2,2>(Y) = bestIdx >> 2;
    dxdy -= 1;

    vector<int2,4> mv = medata.format<int2,2,4>().select<2,1,2,1>() + dxdy;
    vector<uint2,4> dmv = mv & 3;

    matrix<int1,4,4> filt;
    matrix<int1,4,4> coefs_av1(COEFS_AV1);
    filt.format<uint4>() = coefs_av1.format<uint4>().iselect(dmv);

    vector<int2,4> pos = pel_minus2.replicate<2>() + cm_shr<int2>(mv, 2);

    matrix<uint4,4,64> second;
    matrix<uint4,4,64> comp;
    matrix<uint2,4,16> sadAcc = 0;

    vector<int2,2> blk;

    uint2 i = 0;
    for (blk(Y) = 0; blk(Y) < 32; blk(Y) += 16) {
        for (blk(X) = 0; blk(X) < 32; blk(X) += 16, i++) {
            vector<int2,4> pos_blk = pos + blk.replicate<2>();
            vector<int4,2> pel_blk = pel + blk;

            matrix<uint1,16,16> first;
            matrix<uint1,16,16> src;
            read(SRC, pel_blk(X), pel_blk(Y), src);

            Interpolate16(REF0, pos_blk.select<2,1>(0), filt.select<2,1,4,1>(0));
            sadAcc.row(0) += sad16x16(src, g_pred16);
            first = g_pred16;
#if NEWMVPRED
            write(OUT_PRED, pel_blk(X), pel_blk(Y), first);
#endif
            if (refConfig != SINGLE_REF) {
                Interpolate16(REF1, pos_blk.select<2, 1>(2), filt.select<2, 1, 4, 1>(2));
                sadAcc.row(1) += sad16x16(src, g_pred16);
                second.row(i) = g_pred16.format<uint4>();

                if (refConfig == COMPOUND) {
                    g_pred16 = cm_avg<uint1>(first, g_pred16);
                    sadAcc.row(2) += sad16x16(src, g_pred16);
                    comp.row(i) = g_pred16.format<uint4>();
                }
            }
        }
    }

    vector<uint4,16> tmp1 = sadAcc.format<uint4>().select<16,2>(0) + sadAcc.format<uint4>().select<16,2>(1);
    vector<uint4,8>  tmp2 = tmp1.select<8,2>(0) + tmp1.select<8,2>(1);
    vector<uint4,4>  cost = tmp2.select<4,2>(0) + tmp2.select<4,2>(1);

    // get min SAD
    cost <<= 2;
    cost(1) += 1;
    cost(2) += 2;
    if (refConfig != SINGLE_REF) {
        cost(0) = cm_min<uint4>(cost(0), cost(1));
        if (refConfig == COMPOUND)
            cost(0) = cm_min<uint4>(cost(0), cost(2));
    }
#if NEWMVPRED
    uint4 idx = cost(0) & 3;

    // best candidate report (refIdx & predictor)
    if (idx > 0) {
        if (idx == 2) // BiPred - rewrite
            second = comp;

        write(OUT_PRED, pel(X) +  0, pel(Y) +  0, second.row(0).format<uint1,16,16>());
        write(OUT_PRED, pel(X) + 16, pel(Y) +  0, second.row(1).format<uint1,16,16>());
        write(OUT_PRED, pel(X) +  0, pel(Y) + 16, second.row(2).format<uint1,16,16>());
        write(OUT_PRED, pel(X) + 16, pel(Y) + 16, second.row(3).format<uint1,16,16>());
    }
#endif

    medata.format<int2,2,4>().select<2,1,2,1>() = mv;
    medata.format<int4>()(1) = cost(0);
    write(MEDATA1, origxy(X), origxy(Y), medata.row(1));
    write(MEDATA0, origxy(X), origxy(Y), medata.row(0));
}


extern "C" _GENX_MAIN_
    void MePuGacc64x64(SurfaceIndex SRC, SurfaceIndex REF0, SurfaceIndex REF1, SurfaceIndex MEDATA0,
                       SurfaceIndex MEDATA1, SurfaceIndex OUT_PRED, uint4 refConfig, SurfaceIndex SCRATCHPAD)
{
    vector<int2,2> origin;
    origin(X) = get_thread_origin_x();
    origin(Y) = get_thread_origin_y();

    vector<int4,2> origxy = origin;
    origxy(X) *= 64;

    vector<int4,2> pel = origin * 64;

    vector<int4,2> pel_minus2 = pel - 2;


    matrix<uint1,2,8> medata;       // mv & dist0
    vector<uint4,16>  medataDist8;  // 8 more dists

    // read motion vectors and prepare filters
    read(MEDATA0, origxy(X) + 0, origxy(Y), medata.row(0));
    read(MEDATA1, origxy(X) + 0, origxy(Y), medata.row(1));
    read(MEDATA0, origxy(X) + 8, origxy(Y), medataDist8.select<8,1>(0));
    read(MEDATA1, origxy(X) + 8, origxy(Y), medataDist8.select<8,1>(8));

    vector<uint4,2> medataDist0 = medata.format<uint4>().select<2,2>(1) << 4;
    medataDist8 <<= 4;
    vector<uint2,8> mv_delta(MV_DELTA);
    medataDist8 += mv_delta.replicate<2>();

    vector<uint4,8> tmp8 = cm_min<uint4>(medataDist8.select<8,2>(0), medataDist8.select<8,2>(1));
    vector<uint4,4> tmp4 = cm_min<uint4>(tmp8.select<4,2>(0), tmp8.select<4,2>(1));
    vector<uint4,2> minCosts = cm_min<uint4>(tmp4.select<2,2>(0), tmp4.select<2,2>(1));
    minCosts = cm_min<uint4>(minCosts, medataDist0);

    vector<uint4,2> bestIdx = minCosts & 15;
    vector<int2,4> dxdy;
    dxdy.select<2,2>(X) = bestIdx  & 3;
    dxdy.select<2,2>(Y) = bestIdx >> 2;
    dxdy -= 1;
    dxdy *= 2;

    vector<int2,4> mv = medata.format<int2,2,4>().select<2,1,2,1>() + dxdy;
    vector<uint2,4> dmv = mv & 3;
    vector<int2,4> pos = pel_minus2.replicate<2>() + cm_shr<int2>(mv, 2);

    matrix<int1,4,4> filt;
    matrix<int1,4,4> coefs_av1(COEFS_AV1);
    filt.format<uint4>() = coefs_av1.format<uint4>().iselect(dmv);

    matrix<uint2,4,16> sads(0);

    vector<int4,2> scratch_loc = pel;
    scratch_loc(Y) *= 2;

    vector<int2,2> blk;
    for (blk(Y) = 0; blk(Y) < 64; blk(Y) += 16) {
        for (blk(X) = 0; blk(X) < 64; blk(X) += 16) {
            matrix<uint1,16,16> src;
            matrix<uint1,16,16> pred0;
            matrix<uint1,16,16> pred1;

            vector<int2,4> pos_ = pos + blk.replicate<2>();
            vector<int4,2> pel_ = pel + blk;
            vector<int4,2> write_loc_ = scratch_loc + blk;

            read(SRC, pel_(X), pel_(Y), src);

            Interpolate16(REF0, pos_.select<2,1>(0), filt.select<2,1,4,1>(0));
            sads.row(0) += sad16x16(src, g_pred16);
            write(SCRATCHPAD, write_loc_(X), write_loc_(Y) + 0, g_pred16);
            pred0 = g_pred16;

            if (refConfig != SINGLE_REF) {
                Interpolate16(REF1, pos_.select<2, 1>(2), filt.select<2, 1, 4, 1>(2));
                sads.row(1) += sad16x16(src, g_pred16);
                write(SCRATCHPAD, write_loc_(X), write_loc_(Y) + 64, g_pred16);

                if (refConfig == COMPOUND) {
                    g_pred16 = cm_avg<uint1>(pred0, g_pred16);
                    sads.row(2) += sad16x16(src, g_pred16);
#if NEWMVPRED
                    write(OUT_PRED, pel_(X), pel_(Y), g_pred16);
#endif
                }
            }
        }
    }

    // accumulate SADs
    vector<uint4,16> tmp0 = sads.format<uint4>().select<16,2>(0) + sads.format<uint4>().select<16,2>(1);
    vector<uint4,8>  tmp1 = tmp0.select<8,2>(0) + tmp0.select<8,2>(1);
    vector<uint4,4> costs = tmp1.select<4,2>(0) + tmp1.select<4,2>(1);

    costs <<= 2;
    costs(1) += 1;
    costs(2) += 2;
    if (refConfig != SINGLE_REF) {
        costs(0) = cm_min<uint4>(costs(0), costs(1));
        if (refConfig == COMPOUND)
            costs(0) = cm_min<uint4>(costs(0), costs(2));
    }

    uint4 idx = costs(0) & 3;

    // best candidate report (refIdx & predictor)
    if (idx < 2) {
        scratch_loc(Y) += idx * 64;
        for (uint row = 0; row < 64; row += 8) {
            matrix<uint1,8,32> src;
            matrix<uint1,8,32> pred;

            read(MODIFIED(SCRATCHPAD), scratch_loc(X) + 0,  scratch_loc(Y) + row, pred);
#if NEWMVPRED
            write(OUT_PRED, pel(X) + 0,  pel(Y) + row, pred);
#endif
            read(MODIFIED(SCRATCHPAD), scratch_loc(X) + 32, scratch_loc(Y) + row, pred);
#if NEWMVPRED
            write(OUT_PRED, pel(X) + 32, pel(Y) + row, pred);
#endif
        }
    }

    medata.row(0).format<int2>().select<2,1>() = mv.select<2,1>(0);
    medata.row(1).format<int2>().select<2,1>() = mv.select<2,1>(2);
    medata.row(0).format<int4>()(1) = costs(0);

    write(MEDATA1, origxy(X), origxy(Y), medata.row(1));
    write(MEDATA0, origxy(X), origxy(Y), medata.row(0));
}
