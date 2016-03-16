/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2015 Intel Corporation. All Rights Reserved.
//
*/


#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>
#define INTERDATA_SIZE_SMALL    8
#define INTERDATA_SIZE_BIG      64   // 32x32 and 64x64 blocks
#define MVDATA_SIZE     4 // mfxI16Pair
#define MBDIST_SIZE     64  // 16*mfxU32
#define DIST_SIZE       4

#if !defined(target_gen7_5) && !defined(target_gen8) && !defined(target_gen9) && !defined(CMRT_EMU)
#error One of macro should be defined: target_gen7_5, target_gen8, target_gen9
#endif

#ifdef target_gen8
typedef matrix<uchar, 4, 32> UniIn;
#else
typedef matrix<uchar, 3, 32> UniIn;
#endif

#define BIREFINEDATA_SIZE   8       // mv0 (mfxI16Pair) + mv1 (mfxI16Pair)

const uint2 g_0to8_init[9] = {1,2,3,4,0,5,6,7,8};
const int2 init_x_shift[9] = {0, -1,  0,  1, -1, 1, -1, 0, 1};
const int2 init_y_shift[9] = {0, -1, -1, -1,  0, 0,  1, 1, 1};
const int2 init_x64_shift[9] = {0, -2,  0,  2, -2, 2, -2, 0, 2};
const int2 init_y64_shift[9] = {0, -2, -2, -2,  0, 0,  2, 2, 2};

const uint1 g_0to24_init[24] =
{
            1,  2,
        3,  4,  5,  6,
    7,  8,  0,  9, 10, 11,
   12, 13, 14, 15, 16, 17,
       18, 19, 20, 21,
            22, 23
};

const int2 init_x24_shift[24] =
{
     0,
             0,  1,
        -1,  0,  1,  2,
    -2, -1,      1,  2,  3,
    -2, -1,  0,  1,  2,  3,
        -1,  0,  1,  2,
             0,  1
};
const int2 init_y24_shift[24] =
{
     0,
            -2, -2,
        -1, -1, -1, -1,
     0,  0,      0,  0,  0,
     1,  1,  1,  1,  1,  1,
         2,  2,  2,  2,
             3,  3
};

template <uint H, uint W> inline _GENX_ vector<uint2,W> Sad(matrix_ref<uint1,H,W> m1, matrix_ref<uint1,H,W> m2)
{
    vector<uint2,W> sad = cm_sad2<uint2>(m1.row(0), m2.row(0));
    #pragma unroll
    for (int i = 1; i < H; i++)
        sad = cm_sada2<uint2>(m1.row(i), m2.row(i), sad);
    return sad;
}

/*inline*/ _GENX_
void RefineMv7x7_0(matrix_ref<uint2,24,16> m_sad,
                   SurfaceIndex m_srcSurf,
                   SurfaceIndex m_fixSurf,
                   SurfaceIndex m_trySurf,
                   uint x, uint y,
                   vector<int2,2> mvtry,
                   vector<int2,2> mvfix)
{

    uint xinterp = x << 2;
    uint yinterp = y << 2;

    int xref_fix = xinterp + mvfix[0]; // qpels
    int yref_fix = yinterp + mvfix[1];

    vector<uint1,2> qpel_fix;
    qpel_fix[0] = xref_fix & 1;
    qpel_fix[1] = yref_fix & 1;

    xref_fix >>= 1; // hpels
    yref_fix >>= 1;

    int xref_try = xinterp + mvtry[0];
    int yref_try = yinterp + mvtry[1];

    xref_try >>= 1; // hpels
    yref_try >>= 1;
    xref_try -= 1;
    yref_try -= 1;

    vector<uint2,16> m_summ0_0, m_summ0_1, m_summ0_2;
    vector<uint2,16> m_summ1_0, m_summ1_1, m_summ1_2;
    vector<uint1,16> m_avgg;

    matrix<uint1,4,16> m_src;
    cmtl::ReadBlock(m_srcSurf, x, y, m_src.select_all());
    matrix<int2,4,16> m_src2;
    m_src2 = m_src << 1;

    matrix<uint1,4,16> m_ref0;
    matrix<uint1,8,32> tmp;

    matrix<uint2,4,16> m_sum00, m_sum01, m_sum02;
    matrix<uint2,4,16> m_sum10, m_sum11, m_sum12;

    cmtl::ReadBlock(m_fixSurf, xref_fix, yref_fix, tmp.select_all());
    if (qpel_fix.format<uint2>()[0] == 0x000) {
        m_ref0 = tmp.select<4,2,16,2>(0,0);
    } else if (qpel_fix.format<uint2>()[0] == 0x001) {  //bytes are shuffled
        m_sum00 = tmp.select<4,2,16,2>(0,0) + tmp.select<4,2,16,2>(0,1);
        m_sum00 += 1;
        m_ref0 = cm_asr<uint1>(m_sum00, 1, SAT);
    } else if (qpel_fix.format<uint2>()[0] == 0x100) {
        m_sum00 = tmp.select<4,2,16,2>(0,0) + tmp.select<4,2,16,2>(1,0);
        m_sum00 += 1;
        m_ref0 = cm_asr<uint1>(m_sum00, 1, SAT);
    } else if (qpel_fix.format<uint2>()[0] == 0x101) {
        m_sum00 = tmp.select<4,2,16,2>(0,0) + tmp.select<4,2,16,2>(0,1);
        m_sum01 = tmp.select<4,2,16,2>(1,0) + tmp.select<4,2,16,2>(1,1);
        m_sum00 += m_sum01;
        m_sum00 += 2;
        m_ref0 = cm_asr<uint1>(m_sum00, 2, SAT);
    }

    m_src2 -= m_ref0;

    matrix<uint1,4,16> m_src_new;
    m_src_new = matrix<uint1,4,16>(m_src2,SAT);

    matrix<uint1,7,32> m_ref00;
    matrix<uint1,7,32> m_ref01;
    matrix<uint1,7,32> m_ref10;
    matrix<uint1,7,32> m_ref11;

    cmtl::ReadBlock(m_trySurf, xref_try, yref_try, tmp.select_all());
    m_ref00.select<4,1,16,1>(0,0) = tmp.select<4,2,16,2>(0,0);
    m_ref01.select<4,1,16,1>(0,0) = tmp.select<4,2,16,2>(0,1);
    m_ref10.select<4,1,16,1>(0,0) = tmp.select<4,2,16,2>(1,0);
    m_ref11.select<4,1,16,1>(0,0) = tmp.select<4,2,16,2>(1,1);
    cmtl::ReadBlock(m_trySurf, xref_try + 32, yref_try, tmp.select_all());
    m_ref00.select<4,1,16,1>(0,16) = tmp.select<4,2,16,2>(0,0);
    m_ref01.select<4,1,16,1>(0,16) = tmp.select<4,2,16,2>(0,1);
    m_ref10.select<4,1,16,1>(0,16) = tmp.select<4,2,16,2>(1,0);
    m_ref11.select<4,1,16,1>(0,16) = tmp.select<4,2,16,2>(1,1);
    cmtl::ReadBlock(m_trySurf, xref_try, yref_try + 8, tmp.select<6,1,32,1>(0,0));
    m_ref00.select<3,1,16,1>(4,0) = tmp.select<3,2,16,2>(0,0);
    m_ref01.select<3,1,16,1>(4,0) = tmp.select<3,2,16,2>(0,1);
    m_ref10.select<3,1,16,1>(4,0) = tmp.select<3,2,16,2>(1,0);
    m_ref11.select<3,1,16,1>(4,0) = tmp.select<3,2,16,2>(1,1);
    cmtl::ReadBlock(m_trySurf, xref_try + 32, yref_try + 8, tmp.select<6,1,32,1>(0,0));
    m_ref00.select<3,1,16,1>(4,16) = tmp.select<3,2,16,2>(0,0);
    m_ref01.select<3,1,16,1>(4,16) = tmp.select<3,2,16,2>(0,1);
    m_ref10.select<3,1,16,1>(4,16) = tmp.select<3,2,16,2>(1,0);
    m_ref11.select<3,1,16,1>(4,16) = tmp.select<3,2,16,2>(1,1);

    matrix<uint1,4,16> m_avg;

    m_sad.row(0) = Sad(m_ref01.select<4,1,16,1>(0,0), m_src_new.select_all());

    m_sum00 = m_ref00.select<4,1,16,1>(0,0) + m_ref01.select<4,1,16,1>(0,0);
    m_sum00 += 1;
    m_sum01 = m_ref01.select<4,1,16,1>(0,0) + m_ref00.select<4,1,16,1>(0,1);
    m_sum01 += 1;
    m_avg = cm_asr<uint1>(m_sum01, 1, SAT);
    m_sad.row(1) = Sad(m_avg.select_all(), m_src_new.select_all());

    m_sad.row(6) = Sad(m_ref10.select<4,1,16,1>(0,0), m_src_new.select_all());
    m_sad.row(8) = Sad(m_ref11.select<4,1,16,1>(0,0), m_src_new.select_all());
    m_sad.row(10) = Sad(m_ref10.select<4,1,16,1>(0,1), m_src_new.select_all());

    m_sum10 = m_ref10.select<4,1,16,1>(0,0) + m_ref11.select<4,1,16,1>(0,0);
    m_sum10 += 1;
    m_avg = cm_asr<uint1>(m_sum10, 1, SAT);
    m_sad.row(7) = Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum11 = m_ref11.select<4,1,16,1>(0,0) + m_ref10.select<4,1,16,1>(0,1);
    m_sum11 += 1;
    m_avg = cm_asr<uint1>(m_sum11, 1, SAT);
    m_sad.row(9) = Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum12 = m_ref10.select<4,1,16,1>(0,1) + m_ref11.select<4,1,16,1>(0,1);
    m_sum12 += 1;
    m_avg = cm_asr<uint1>(m_sum12, 1, SAT);
    m_sad.row(11) = Sad(m_avg.select_all(), m_src_new.select_all());

    m_sum00 += m_sum10;
    m_avg = cm_asr<uint1>(m_sum00, 2, SAT);
    m_sad.row(2) = Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum01 += m_sum11;
    m_avg = cm_asr<uint1>(m_sum01, 2, SAT);
    m_sad.row(4) = Sad(m_avg.select_all(), m_src_new.select_all());

    m_sum01 = m_ref01.select<4,1,16,1>(0,0) + m_ref11.select<4,1,16,1>(0,0);
    m_sum01 += 1;
    m_avg = cm_asr<uint1>(m_sum01, 1, SAT);
    m_sad.row(3) = Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum00 = m_ref00.select<4,1,16,1>(0,1) + m_ref10.select<4,1,16,1>(0,1);
    m_sum00 += 1;
    m_avg = cm_asr<uint1>(m_sum00, 1, SAT);
    m_sad.row(5) = Sad(m_avg.select_all(), m_src_new.select_all());
    
    m_sad.row(19) = Sad(m_ref01.select<4,1,16,1>(1,0), m_src_new.select_all());
    m_sad.row(21) = Sad(m_ref00.select<4,1,16,1>(1,1), m_src_new.select_all());

    m_sum00 = m_ref00.select<4,1,16,1>(1,0) + m_ref01.select<4,1,16,1>(1,0);
    m_sum00 += 1;
    m_avg = cm_asr<uint1>(m_sum00, 1, SAT);
    m_sad.row(18) = Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum01 = m_ref01.select<4,1,16,1>(1,0) + m_ref00.select<4,1,16,1>(1,1);
    m_sum01 += 1;
    m_avg = cm_asr<uint1>(m_sum01, 1, SAT);
    m_sad.row(20) = Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum02 = m_ref00.select<4,1,16,1>(1,1) + m_ref01.select<4,1,16,1>(1,1);
    m_sum02 += 1;

    m_sum10 += m_sum00;
    m_avg = cm_asr<uint1>(m_sum10, 2, SAT);
    m_sad.row(13) = Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum11 += m_sum01;
    m_avg = cm_asr<uint1>(m_sum11, 2, SAT);
    m_sad.row(15) = Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum12 += m_sum02;
    m_avg = cm_asr<uint1>(m_sum12, 2, SAT);
    m_sad.row(17) = Sad(m_avg.select_all(), m_src_new.select_all());

    m_sum10 = m_ref00.select<4,1,16,1>(1,0) + m_ref10.select<4,1,16,1>(0,0);
    m_sum10 += 1;
    m_avg = cm_asr<uint1>(m_sum10, 1, SAT);
    m_sad.row(12) = Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum11 = m_ref01.select<4,1,16,1>(1,0) + m_ref11.select<4,1,16,1>(0,0);
    m_sum11 += 1;
    m_avg = cm_asr<uint1>(m_sum11, 1, SAT);
    m_sad.row(14) = Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum10 = m_ref00.select<4,1,16,1>(1,1) + m_ref10.select<4,1,16,1>(0,1);
    m_sum10 += 1;
    m_avg = cm_asr<uint1>(m_sum10, 1, SAT);
    m_sad.row(16) = Sad(m_avg.select_all(), m_src_new.select_all());

    m_sum11 = m_ref11.select<4,1,16,1>(1,0) + m_ref10.select<4,1,16,1>(1,1);
    m_sum11 += 1;

    m_sum01 += m_sum11;
    m_avg = cm_asr<uint1>(m_sum01, 2, SAT);
    m_sad.row(23) = Sad(m_avg.select_all(), m_src_new.select_all());

    m_sum01 = m_ref01.select<4,1,16,1>(1,0) + m_ref11.select<4,1,16,1>(1,0);
    m_sum01 += 1;
    m_avg = cm_asr<uint1>(m_sum01, 1, SAT);
    m_sad.row(22) = Sad(m_avg.select_all(), m_src_new.select_all());
}

/*inline*/ _GENX_
void RefineMv7x7(matrix_ref<uint2,24,16> m_sad,
                 SurfaceIndex m_srcSurf,
                 SurfaceIndex m_fixSurf,
                 SurfaceIndex m_trySurf,
                 uint x, uint y,
                 vector<int2,2> mvtry,
                 vector<int2,2> mvfix)
{

    uint xinterp = x << 2;
    uint yinterp = y << 2;

    int xref_fix = xinterp + mvfix[0]; // qpels
    int yref_fix = yinterp + mvfix[1];

    vector<uint1,2> qpel_fix;
    qpel_fix[0] = xref_fix & 1;
    qpel_fix[1] = yref_fix & 1;

    xref_fix >>= 1; // hpels
    yref_fix >>= 1;

    int xref_try = xinterp + mvtry[0];
    int yref_try = yinterp + mvtry[1];

    xref_try >>= 1; // hpels
    yref_try >>= 1;
    xref_try -= 1;
    yref_try -= 1;

    vector<uint2,16> m_summ0_0, m_summ0_1, m_summ0_2;
    vector<uint2,16> m_summ1_0, m_summ1_1, m_summ1_2;
    vector<uint1,16> m_avgg;

    matrix<uint1,4,16> m_src;
    cmtl::ReadBlock(m_srcSurf, x, y, m_src.select_all());
    matrix<int2,4,16> m_src2;
    m_src2 = m_src << 1;

    matrix<uint1,4,16> m_ref0;
    matrix<uint1,8,32> tmp;

    matrix<uint2,4,16> m_sum00, m_sum01, m_sum02;
    matrix<uint2,4,16> m_sum10, m_sum11, m_sum12;

    cmtl::ReadBlock(m_fixSurf, xref_fix, yref_fix, tmp.select_all());
//    read_plane(m_fixSurf, GENX_SURFACE_Y_PLANE, xref_fix, yref_fix, tmp.select_all());
    if (qpel_fix.format<uint2>()[0] == 0x000) {
        m_ref0 = tmp.select<4,2,16,2>(0,0);
    } else if (qpel_fix.format<uint2>()[0] == 0x001) {  //bytes are shuffled
        m_sum00 = tmp.select<4,2,16,2>(0,0) + tmp.select<4,2,16,2>(0,1);
        m_sum00 += 1;
        m_ref0 = cm_asr<uint1>(m_sum00, 1, SAT);
    } else if (qpel_fix.format<uint2>()[0] == 0x100) {
        m_sum00 = tmp.select<4,2,16,2>(0,0) + tmp.select<4,2,16,2>(1,0);
        m_sum00 += 1;
        m_ref0 = cm_asr<uint1>(m_sum00, 1, SAT);
    } else if (qpel_fix.format<uint2>()[0] == 0x101) {
        m_sum00 = tmp.select<4,2,16,2>(0,0) + tmp.select<4,2,16,2>(0,1);
        m_sum01 = tmp.select<4,2,16,2>(1,0) + tmp.select<4,2,16,2>(1,1);
        m_sum00 += m_sum01;
        m_sum00 += 2;
        m_ref0 = cm_asr<uint1>(m_sum00, 2, SAT);
    }

    m_src2 -= m_ref0;

    matrix<uint1,4,16> m_src_new;
    m_src_new = matrix<uint1,4,16>(m_src2,SAT);

    matrix<uint1,7,32> m_ref00;
    matrix<uint1,7,32> m_ref01;
    matrix<uint1,7,32> m_ref10;
    matrix<uint1,7,32> m_ref11;

    cmtl::ReadBlock(m_trySurf, xref_try, yref_try, tmp.select_all());
    m_ref00.select<4,1,16,1>(0,0) = tmp.select<4,2,16,2>(0,0);
    m_ref01.select<4,1,16,1>(0,0) = tmp.select<4,2,16,2>(0,1);
    m_ref10.select<4,1,16,1>(0,0) = tmp.select<4,2,16,2>(1,0);
    m_ref11.select<4,1,16,1>(0,0) = tmp.select<4,2,16,2>(1,1);
    cmtl::ReadBlock(m_trySurf, xref_try + 32, yref_try, tmp.select_all());
    m_ref00.select<4,1,16,1>(0,16) = tmp.select<4,2,16,2>(0,0);
    m_ref01.select<4,1,16,1>(0,16) = tmp.select<4,2,16,2>(0,1);
    m_ref10.select<4,1,16,1>(0,16) = tmp.select<4,2,16,2>(1,0);
    m_ref11.select<4,1,16,1>(0,16) = tmp.select<4,2,16,2>(1,1);
    cmtl::ReadBlock(m_trySurf, xref_try, yref_try + 8, tmp.select<6,1,32,1>(0,0));
    m_ref00.select<3,1,16,1>(4,0) = tmp.select<3,2,16,2>(0,0);
    m_ref01.select<3,1,16,1>(4,0) = tmp.select<3,2,16,2>(0,1);
    m_ref10.select<3,1,16,1>(4,0) = tmp.select<3,2,16,2>(1,0);
    m_ref11.select<3,1,16,1>(4,0) = tmp.select<3,2,16,2>(1,1);
    cmtl::ReadBlock(m_trySurf, xref_try + 32, yref_try + 8, tmp.select<6,1,32,1>(0,0));
    m_ref00.select<3,1,16,1>(4,16) = tmp.select<3,2,16,2>(0,0);
    m_ref01.select<3,1,16,1>(4,16) = tmp.select<3,2,16,2>(0,1);
    m_ref10.select<3,1,16,1>(4,16) = tmp.select<3,2,16,2>(1,0);
    m_ref11.select<3,1,16,1>(4,16) = tmp.select<3,2,16,2>(1,1);

    matrix<uint1,4,16> m_avg;

    m_sad.row(0) += Sad(m_ref01.select<4,1,16,1>(0,0), m_src_new.select_all());

    m_sum00 = m_ref00.select<4,1,16,1>(0,0) + m_ref01.select<4,1,16,1>(0,0);
    m_sum00 += 1;
    m_sum01 = m_ref01.select<4,1,16,1>(0,0) + m_ref00.select<4,1,16,1>(0,1);
    m_sum01 += 1;
    m_avg = cm_asr<uint1>(m_sum01, 1, SAT);
    m_sad.row(1) += Sad(m_avg.select_all(), m_src_new.select_all());

    m_sad.row(6) += Sad(m_ref10.select<4,1,16,1>(0,0), m_src_new.select_all());
    m_sad.row(8) += Sad(m_ref11.select<4,1,16,1>(0,0), m_src_new.select_all());
    m_sad.row(10) += Sad(m_ref10.select<4,1,16,1>(0,1), m_src_new.select_all());

    m_sum10 = m_ref10.select<4,1,16,1>(0,0) + m_ref11.select<4,1,16,1>(0,0);
    m_sum10 += 1;
    m_avg = cm_asr<uint1>(m_sum10, 1, SAT);
    m_sad.row(7) += Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum11 = m_ref11.select<4,1,16,1>(0,0) + m_ref10.select<4,1,16,1>(0,1);
    m_sum11 += 1;
    m_avg = cm_asr<uint1>(m_sum11, 1, SAT);
    m_sad.row(9) += Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum12 = m_ref10.select<4,1,16,1>(0,1) + m_ref11.select<4,1,16,1>(0,1);
    m_sum12 += 1;
    m_avg = cm_asr<uint1>(m_sum12, 1, SAT);
    m_sad.row(11) += Sad(m_avg.select_all(), m_src_new.select_all());

    m_sum00 += m_sum10;
    m_avg = cm_asr<uint1>(m_sum00, 2, SAT);
    m_sad.row(2) += Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum01 += m_sum11;
    m_avg = cm_asr<uint1>(m_sum01, 2, SAT);
    m_sad.row(4) += Sad(m_avg.select_all(), m_src_new.select_all());

    m_sum01 = m_ref01.select<4,1,16,1>(0,0) + m_ref11.select<4,1,16,1>(0,0);
    m_sum01 += 1;
    m_avg = cm_asr<uint1>(m_sum01, 1, SAT);
    m_sad.row(3) += Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum00 = m_ref00.select<4,1,16,1>(0,1) + m_ref10.select<4,1,16,1>(0,1);
    m_sum00 += 1;
    m_avg = cm_asr<uint1>(m_sum00, 1, SAT);
    m_sad.row(5) += Sad(m_avg.select_all(), m_src_new.select_all());
    
    m_sad.row(19) += Sad(m_ref01.select<4,1,16,1>(1,0), m_src_new.select_all());
    m_sad.row(21) += Sad(m_ref00.select<4,1,16,1>(1,1), m_src_new.select_all());

    m_sum00 = m_ref00.select<4,1,16,1>(1,0) + m_ref01.select<4,1,16,1>(1,0);
    m_sum00 += 1;
    m_avg = cm_asr<uint1>(m_sum00, 1, SAT);
    m_sad.row(18) += Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum01 = m_ref01.select<4,1,16,1>(1,0) + m_ref00.select<4,1,16,1>(1,1);
    m_sum01 += 1;
    m_avg = cm_asr<uint1>(m_sum01, 1, SAT);
    m_sad.row(20) += Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum02 = m_ref00.select<4,1,16,1>(1,1) + m_ref01.select<4,1,16,1>(1,1);
    m_sum02 += 1;

    m_sum10 += m_sum00;
    m_avg = cm_asr<uint1>(m_sum10, 2, SAT);
    m_sad.row(13) += Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum11 += m_sum01;
    m_avg = cm_asr<uint1>(m_sum11, 2, SAT);
    m_sad.row(15) += Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum12 += m_sum02;
    m_avg = cm_asr<uint1>(m_sum12, 2, SAT);
    m_sad.row(17) += Sad(m_avg.select_all(), m_src_new.select_all());

    m_sum10 = m_ref00.select<4,1,16,1>(1,0) + m_ref10.select<4,1,16,1>(0,0);
    m_sum10 += 1;
    m_avg = cm_asr<uint1>(m_sum10, 1, SAT);
    m_sad.row(12) += Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum11 = m_ref01.select<4,1,16,1>(1,0) + m_ref11.select<4,1,16,1>(0,0);
    m_sum11 += 1;
    m_avg = cm_asr<uint1>(m_sum11, 1, SAT);
    m_sad.row(14) += Sad(m_avg.select_all(), m_src_new.select_all());
    m_sum10 = m_ref00.select<4,1,16,1>(1,1) + m_ref10.select<4,1,16,1>(0,1);
    m_sum10 += 1;
    m_avg = cm_asr<uint1>(m_sum10, 1, SAT);
    m_sad.row(16) += Sad(m_avg.select_all(), m_src_new.select_all());

    m_sum11 = m_ref11.select<4,1,16,1>(1,0) + m_ref10.select<4,1,16,1>(1,1);
    m_sum11 += 1;

    m_sum01 += m_sum11;
    m_avg = cm_asr<uint1>(m_sum01, 2, SAT);
    m_sad.row(23) += Sad(m_avg.select_all(), m_src_new.select_all());

    m_sum01 = m_ref01.select<4,1,16,1>(1,0) + m_ref11.select<4,1,16,1>(1,0);
    m_sum01 += 1;
    m_avg = cm_asr<uint1>(m_sum01, 1, SAT);
    m_sad.row(22) += Sad(m_avg.select_all(), m_src_new.select_all());
}

extern "C" _GENX_MAIN_
void BiRefine32x32(SurfaceIndex SURF_DST, SurfaceIndex SURF_SRC,
                   vector<SurfaceIndex, 1> SURF_REFS0, vector<SurfaceIndex, 1> SURF_REFS1,
                   SurfaceIndex SURF_DATA0, SurfaceIndex SURF_DATA1)
{
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();

    vector<uint,10> data0, data1;
    vector<uint,2> outdata;  // mv0 + mv1
    vector<int2,4> mvtryfix;
    vector<SurfaceIndex, 1> refsTry;
    vector<SurfaceIndex, 1> refsFix;

    uint x = mbX << 5;  // TS is by 32x32
    uint y = mbY << 5;

    read(SURF_DATA0, mbX * INTERDATA_SIZE_BIG, mbY, data0.select<8,1>(0));
    read(SURF_DATA0, mbX * INTERDATA_SIZE_BIG + 32, mbY, data0.select<2,1>(8));
    read(SURF_DATA1, mbX * INTERDATA_SIZE_BIG, mbY, data1.select<8,1>(0));
    read(SURF_DATA1, mbX * INTERDATA_SIZE_BIG + 32, mbY, data1.select<2,1>(8));

    vector<uint2,9> g_0to8(g_0to8_init);
    vector<int2,9> xShift9(init_x_shift);
    vector<int2,9> yShift9(init_y_shift);

    uint ind0, ind1;
    vector<uint,2> bestsad;

    data0.select<9,1>(1) <<= 8;
    data0.select<9,1>(1) += g_0to8;
    bestsad(0) = cm_reduced_min<uint4>(data0.select<9,1>(1));
    ind0 = bestsad(0) & 0xFF;

    data1.select<9,1>(1) <<= 8;
    data1.select<9,1>(1) += g_0to8;
    bestsad(1) = cm_reduced_min<uint4>(data1.select<9,1>(1));
    ind1 = bestsad(1) & 0xFF;

    int ltry, lfix;
    if (bestsad(0) <= bestsad(1)) {
        mvtryfix.select<2,1>(0) = data1.format<int2>().select<2,1>(0);
        mvtryfix.select<2,1>(2) = data0.format<int2>().select<2,1>(0);
        mvtryfix[0] += xShift9[ind1];
        mvtryfix[1] += yShift9[ind1];
        mvtryfix[2] += xShift9[ind0];
        mvtryfix[3] += yShift9[ind0];
        ltry = 1;
        lfix = 0;
        refsTry = SURF_REFS1;
        refsFix = SURF_REFS0;
    } else {
        mvtryfix.select<2,1>(0) = data0.format<int2>().select<2,1>(0);
        mvtryfix.select<2,1>(2) = data1.format<int2>().select<2,1>(0);
        mvtryfix[0] += xShift9[ind0];
        mvtryfix[1] += yShift9[ind0];
        mvtryfix[2] += xShift9[ind1];
        mvtryfix[3] += yShift9[ind1];
        ltry = 0;
        lfix = 1; 
        refsTry = SURF_REFS0;
        refsFix = SURF_REFS1;
    }

    // nearest hpel for mv being refined
    mvtryfix[0] &= ~1;
    mvtryfix[1] &= ~1;
    
    matrix<uint,24,4> sad_mtx(0);
    for (uint1 yBlk = 0; yBlk < 32; yBlk += 8) {
        matrix<uint2,24,16> m_sad(0);
        for (uint1 xBlk = 0; xBlk < 32; xBlk += 16) {
            RefineMv7x7(m_sad, SURF_SRC, refsFix[0], refsTry[0], x + xBlk, y + yBlk,     mvtryfix.select<2,1>(0), mvtryfix.select<2,1>(2));
            RefineMv7x7(m_sad, SURF_SRC, refsFix[0], refsTry[0], x + xBlk, y + yBlk + 4, mvtryfix.select<2,1>(0), mvtryfix.select<2,1>(2));
        }
        sad_mtx += m_sad.select<24,1,4,2>(0,0) + m_sad.select<24,1,4,2>(0,8);
    }

    sad_mtx.select<24,1,2,1>(0,0) += sad_mtx.select<24,1,2,1>(0,2);

    vector<uint,24> sad;
    sad = sad_mtx.select<24,1,1,1>(0,0) + sad_mtx.select<24,1,1,1>(0,1);
    sad >>= 1;

    vector<uint1,24> g_0to24(g_0to24_init);
    vector<int1,24> xShift(init_x24_shift);
    vector<int1,24> yShift(init_y24_shift);

    sad <<= 8;
    sad += g_0to24;
    uint bestSad = cm_reduced_min<uint>(sad);
    bestSad &= 0xff;
    mvtryfix.select<2,1>(0)[0] += xShift[bestSad];
    mvtryfix.select<2,1>(0)[1] += yShift[bestSad];

    outdata[ltry] = mvtryfix.format<uint>()(0);

    // nearest hpel for mv being refined
    mvtryfix[2] &= ~1;
    mvtryfix[3] &= ~1;

    sad_mtx = 0;
    for (uint1 yBlk = 0; yBlk < 32; yBlk += 8) {
        matrix<uint2,24,16> m_sad(0);
        for (uint1 xBlk = 0; xBlk < 32; xBlk += 16) {
            RefineMv7x7(m_sad, SURF_SRC, refsTry[0], refsFix[0], x + xBlk, y + yBlk,     mvtryfix.select<2,1>(2), mvtryfix.select<2,1>(0));
            RefineMv7x7(m_sad, SURF_SRC, refsTry[0], refsFix[0], x + xBlk, y + yBlk + 4, mvtryfix.select<2,1>(2), mvtryfix.select<2,1>(0));
        }
        sad_mtx += m_sad.select<24,1,4,2>(0,0) + m_sad.select<24,1,4,2>(0,8);
    }

    sad_mtx.select<24,1,2,1>(0,0) += sad_mtx.select<24,1,2,1>(0,2);
    sad = sad_mtx.select<24,1,1,1>(0,0) + sad_mtx.select<24,1,1,1>(0,1);
    sad >>= 1;

    sad <<= 8;
    sad += g_0to24;
    bestSad = cm_reduced_min<uint>(sad);
    bestSad &= 0xff;
    mvtryfix.select<2,1>(2)[0] += xShift[bestSad];
    mvtryfix.select<2,1>(2)[1] += yShift[bestSad];

    outdata[lfix] = mvtryfix.format<uint>()(1);

    write(SURF_DST, mbX * BIREFINEDATA_SIZE, mbY, outdata);
}

extern "C" _GENX_MAIN_
void BiRefine64x64(SurfaceIndex SURF_DST, SurfaceIndex SURF_SRC,
                   vector<SurfaceIndex, 1> SURF_REFS0, vector<SurfaceIndex, 1> SURF_REFS1,
                   SurfaceIndex SURF_DATA0, SurfaceIndex SURF_DATA1)
{
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();

    vector<uint,10> data0, data1;
    vector<uint,2> outdata;  // mv0 + mv1
    vector<int2,4> mvtryfix;
    vector<SurfaceIndex, 1> refsTry;
    vector<SurfaceIndex, 1> refsFix;

    uint x = mbX << 6;  // TS is by 64x64
    uint y = mbY << 6;

    read(SURF_DATA0, mbX * INTERDATA_SIZE_BIG, mbY, data0.select<8,1>(0));
    read(SURF_DATA0, mbX * INTERDATA_SIZE_BIG + 32, mbY, data0.select<2,1>(8));
    read(SURF_DATA1, mbX * INTERDATA_SIZE_BIG, mbY, data1.select<8,1>(0));
    read(SURF_DATA1, mbX * INTERDATA_SIZE_BIG + 32, mbY, data1.select<2,1>(8));

    vector<uint2,9> g_0to8(g_0to8_init);
    vector<int2,9> xShift9(init_x64_shift);
    vector<int2,9> yShift9(init_y64_shift);

    vector<uint,2> ind;
    vector<uint,2> bestsad;

    data0.select<9,1>(1) <<= 8;
    data0.select<9,1>(1) += g_0to8;
    bestsad(0) = cm_reduced_min<uint4>(data0.select<9,1>(1));

    data1.select<9,1>(1) <<= 8;
    data1.select<9,1>(1) += g_0to8;
    bestsad(1) = cm_reduced_min<uint4>(data1.select<9,1>(1));

    ind = bestsad & 0xFF;

    int ltry, lfix;
    if (bestsad(0) <= bestsad(1)) {
        mvtryfix.select<2,1>(0) = data1.format<int2>().select<2,1>(0);
        mvtryfix.select<2,1>(2) = data0.format<int2>().select<2,1>(0);
        mvtryfix[0] += xShift9[ind[1]];
        mvtryfix[1] += yShift9[ind[1]];
        mvtryfix[2] += xShift9[ind[0]];
        mvtryfix[3] += yShift9[ind[0]];
        ltry = 1;
        lfix = 0;
        refsTry = SURF_REFS1;
        refsFix = SURF_REFS0;
    } else {
        mvtryfix.select<2,1>(0) = data0.format<int2>().select<2,1>(0);
        mvtryfix.select<2,1>(2) = data1.format<int2>().select<2,1>(0);
        mvtryfix[0] += xShift9[ind[0]];
        mvtryfix[1] += yShift9[ind[0]];
        mvtryfix[2] += xShift9[ind[1]];
        mvtryfix[3] += yShift9[ind[1]];
        ltry = 0;
        lfix = 1; 
        refsTry = SURF_REFS0;
        refsFix = SURF_REFS1;
    }
    // nearest hpel for mv being refined
    //mvtryfix[0] &= ~1;
    //mvtryfix[1] &= ~1;
    
    matrix<uint,24,4> sad_mtx(0);
    matrix<uint2,24,16> m_sad;
    for (uint1 yBlk = 0; yBlk < 64; yBlk += 4) {
        RefineMv7x7_0(m_sad, SURF_SRC, refsFix[0], refsTry[0], x, y + yBlk, mvtryfix.select<2,1>(0), mvtryfix.select<2,1>(2));
        for (uint1 xBlk = 16; xBlk < 64; xBlk += 16) {
            RefineMv7x7(m_sad, SURF_SRC, refsFix[0], refsTry[0], x + xBlk, y + yBlk, mvtryfix.select<2,1>(0), mvtryfix.select<2,1>(2));
        }
        sad_mtx += m_sad.select<24,1,4,2>(0,0) + m_sad.select<24,1,4,2>(0,8);
    }

    sad_mtx.select<24,1,2,1>(0,0) += sad_mtx.select<24,1,2,1>(0,2);

    vector<uint,24> sad;
    sad = sad_mtx.select<24,1,1,1>(0,0) + sad_mtx.select<24,1,1,1>(0,1);
    sad >>= 1;

    vector<uint1,24> g_0to24(g_0to24_init);
    vector<int1,24> xShift24(init_x24_shift);
    vector<int1,24> yShift24(init_y24_shift);

    sad <<= 8;
    sad += g_0to24;
    uint bestSad = cm_reduced_min<uint>(sad);
    bestSad &= 0xff;
    mvtryfix.select<2,1>(0)[0] += xShift24[bestSad];
    mvtryfix.select<2,1>(0)[1] += yShift24[bestSad];

    outdata[ltry] = mvtryfix.format<uint>()(0);

    // nearest hpel for mv being refined
    mvtryfix[2] &= ~1;
    mvtryfix[3] &= ~1;

    sad_mtx = 0;
    for (uint1 yBlk = 0; yBlk < 64; yBlk += 4) {
        RefineMv7x7_0(m_sad, SURF_SRC, refsTry[0], refsFix[0], x, y + yBlk, mvtryfix.select<2,1>(2), mvtryfix.select<2,1>(0));
        for (uint1 xBlk = 16; xBlk < 64; xBlk += 16) {
            RefineMv7x7(m_sad, SURF_SRC, refsTry[0], refsFix[0], x + xBlk, y + yBlk, mvtryfix.select<2,1>(2), mvtryfix.select<2,1>(0));
        }
        sad_mtx += m_sad.select<24,1,4,2>(0,0) + m_sad.select<24,1,4,2>(0,8);
    }

    sad_mtx.select<24,1,2,1>(0,0) += sad_mtx.select<24,1,2,1>(0,2);
    sad = sad_mtx.select<24,1,1,1>(0,0) + sad_mtx.select<24,1,1,1>(0,1);
    sad >>= 1;

    sad <<= 8;
    sad += g_0to24;
    bestSad = cm_reduced_min<uint>(sad);
    bestSad &= 0xff;
    mvtryfix.select<2,1>(2)[0] += xShift24[bestSad];
    mvtryfix.select<2,1>(2)[1] += yShift24[bestSad];

    outdata[lfix] = mvtryfix.format<uint>()(1);

    write(SURF_DST, mbX * BIREFINEDATA_SIZE, mbY, outdata);
}
