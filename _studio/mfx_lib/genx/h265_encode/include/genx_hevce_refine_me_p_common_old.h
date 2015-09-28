/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
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


#define MVDATA_SIZE     4 // mfxI16Pair
#define MBDIST_SIZE     64  // 16*mfxU32

#define SELECT_N_ROWS(m, from, nrows) m.select<nrows, 1, m.COLS, 1>(from)
#define SELECT_N_COLS(m, from, ncols) m.select<m.ROWS, 1, ncols, 1>(0, from)
#define SELECT_N_ROWS_TEMPL(m, from, nrows) m.template select<nrows, 1, m.COLS, 1>(from)
#define SELECT_N_COLS_TEMPL(m, from, ncols) m.template select<m.ROWS, 1, ncols, 1>(0, from)
#define SLICE(VEC, FROM, HOWMANY, STEP) ((VEC).select<HOWMANY, STEP>(FROM))
#define SLICE1(VEC, FROM, HOWMANY) SLICE(VEC, FROM, HOWMANY, 1)
#define NROWS(m, from, nrows) SELECT_N_ROWS_TEMPL(m, from, nrows)
#define NCOLS(m, from, ncols) SELECT_N_COLS_TEMPL(m, from, ncols)

#define CALC_SAD_ACC(sad, ref, src) sad += cm_sad2<ushort>(ref, src).template select<W/2,2>(0)
#define CALC_SAD_QPEL_ACC(sad, hpel1, hpel2, src) \
    tmp = cm_avg<short>(hpel1, hpel2); \
    sad += cm_sad2<ushort>(tmp.template format<uchar>().template select<W, 2>(0), src).template select<W/2,2>(0)
#define INTERPOLATE(dst, src0, src1, src2, src3) dst = src1 + src2; dst *= 5; dst -= src0; dst -= src3
#define SHIFT_SATUR(dst, tmp, src, shift) tmp = src + (1 << (shift - 1)); dst = cm_shr<uchar>(tmp, shift, SAT);


template<int W>
_GENX_ inline
void SumSad(matrix_ref<ushort, 9, W/2> partSad, vector_ref<uint, 9> sad);

template<>
_GENX_ inline
void SumSad<32>(matrix_ref<ushort, 9, 16> partSad, vector_ref<uint, 9> sad)
{
    matrix<ushort, 9, 8> t1 = SELECT_N_COLS(partSad, 0, 8) + SELECT_N_COLS(partSad, 8, 8);
    matrix<ushort, 9, 4> t2 = SELECT_N_COLS(t1, 0, 4) + SELECT_N_COLS(t1, 4, 4);
    matrix<ushort, 9, 2> t3 = SELECT_N_COLS(t2, 0, 2) + SELECT_N_COLS(t2, 2, 2);
    sad = SELECT_N_COLS(t3, 0, 1) + SELECT_N_COLS(t3, 1, 1);
}

template<>
_GENX_ inline
void SumSad<16>(matrix_ref<ushort, 9, 8> partSad, vector_ref<uint, 9> sad)
{
    matrix<ushort, 9, 4> t1 = SELECT_N_COLS(partSad, 0, 4) + SELECT_N_COLS(partSad, 4, 4);
    matrix<ushort, 9, 2> t2 = SELECT_N_COLS(t1, 0, 2) + SELECT_N_COLS(t1, 2, 2);
    sad = SELECT_N_COLS(t2, 0, 1) + SELECT_N_COLS(t2, 1, 1);
}



template<int W, int H>
_GENX_
void InterpolateQpelFromIntPel(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 9> sad)
{
    matrix<uchar, 8, W> src;
    matrix<uchar, 8, W> fpel0, fpel1, fpel2, fpel3, fpel4;
    vector<short,    W> tmp;
    vector<uchar,    W> horzL, horzR;
    vector<uchar,    W> vert;

    matrix<ushort, 9, W/2> partSad = 0;

    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref - 2, fpel2);

    INTERPOLATE(tmp, fpel2.row(0), fpel2.row(1), fpel2.row(2), fpel2.row(3));
    SHIFT_SATUR(vert, tmp, tmp, 3);

#define ONE_STEP(j)                                                                             \
    INTERPOLATE(tmp, fpel0.row(j), fpel1.row(j), fpel2.row(j+2&7), fpel3.row(j));               \
    SHIFT_SATUR(horzL, tmp, tmp, 3);                                                            \
    INTERPOLATE(tmp, fpel1.row(j), fpel2.row(j+2&7), fpel3.row(j), fpel4.row(j));               \
    SHIFT_SATUR(horzR, tmp, tmp, 3);                                                            \
    CALC_SAD_ACC(partSad.row(4), fpel2.row(j+2&7), src.row(j));                                 \
    CALC_SAD_QPEL_ACC(partSad.row(3), horzL, fpel2.row(j+2&7), src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(0), vert,  horzL,            src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(1), vert,  fpel2.row(j+2&7), src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(2), vert,  horzR,            src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(5), horzR, fpel2.row(j+2&7), src.row(j));                     \
    INTERPOLATE(tmp, fpel2.row(j+1&7), fpel2.row(j+2&7), fpel2.row(j+3&7), fpel2.row(j+4&7));   \
    SHIFT_SATUR(vert, tmp, tmp, 3);                                                             \
    CALC_SAD_QPEL_ACC(partSad.row(6), vert,  horzL,            src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(7), vert,  fpel2.row(j+2&7), src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(8), vert,  horzR,            src.row(j))

    for (int i = 0; i < H; i += 8)
    {
        read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc,     ysrc + i, src);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref + i, fpel0);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i, fpel1);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i, fpel3);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i, fpel4);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 2, SELECT_N_ROWS_TEMPL(fpel2, 4, 4));

        ONE_STEP(0);
        ONE_STEP(1);
        ONE_STEP(2);
        ONE_STEP(3);

        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 6, SELECT_N_ROWS_TEMPL(fpel2, 0, 4));

        ONE_STEP(4);
        ONE_STEP(5);
        ONE_STEP(6);
        ONE_STEP(7);
    }

#undef ONE_STEP

    SumSad<W>(partSad, sad);
}


template<int W, int H>
_GENX_
void InterpolateQpelFromHalfPelVert(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 9> sad)
{
    matrix<uchar, 8, W> src;
    matrix<uchar, 4, W> fpel0, fpel1, fpel3, fpel4;
    matrix<uchar, 8, W> fpel2;
    matrix<short, 4, W> tmpL, tmpR;
    vector<short,    W> tmp;
    vector<uchar,    W> horzL, horzR;
    vector<uchar,    W> diagL, diagR;
    vector<uchar,    W> vert;
    matrix<ushort, 9, W/2> partSad = 0;

    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref - 1, NROWS(fpel0,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref - 1, NROWS(fpel1,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref - 1, NROWS(fpel2,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref - 1, NROWS(fpel3,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref - 1, NROWS(fpel4,1,3) );

    INTERPOLATE( NROWS(tmpL,0,3), NROWS(fpel0,1,3), NROWS(fpel1,1,3), NROWS(fpel2,1,3), NROWS(fpel3,1,3) );
    INTERPOLATE( NROWS(tmpR,0,3), NROWS(fpel1,1,3), NROWS(fpel2,1,3), NROWS(fpel3,1,3), NROWS(fpel4,1,3) );
    SHIFT_SATUR( horzL, tmp, tmpL.row(1), 3 );
    SHIFT_SATUR( horzR, tmp, tmpR.row(1), 3 );

#define ONE_STEP(j)                                                                                         \
    INTERPOLATE( tmpL.row(j+3&3), fpel0.row(j+4&3), fpel1.row(j+4&3), fpel2.row(j+4&7), fpel3.row(j+4&3) ); \
    INTERPOLATE( tmpR.row(j+3&3), fpel1.row(j+4&3), fpel2.row(j+4&7), fpel3.row(j+4&3), fpel4.row(j+4&3) ); \
    INTERPOLATE( tmp, tmpL.row(j+0&3), tmpL.row(j+1&3), tmpL.row(j+2&3), tmpL.row(j+3&3) );                 \
    SHIFT_SATUR( diagL, tmp, tmp, 6 );                                                                      \
    INTERPOLATE( tmp, tmpR.row(j+0&3), tmpR.row(j+1&3), tmpR.row(j+2&3), tmpR.row(j+3&3) );                 \
    SHIFT_SATUR( diagR, tmp, tmp, 6 );                                                                      \
    INTERPOLATE( tmp, fpel2.row(j+1&7), fpel2.row(j+2&7), fpel2.row(j+3&7), fpel2.row(j+4&7) );             \
    SHIFT_SATUR( vert, tmp, tmp, 3 );                                                                       \
    CALC_SAD_ACC( partSad.row(4), vert, src.row(j) );                                                       \
    CALC_SAD_QPEL_ACC( partSad.row(0), vert, horzL,            src.row(j) );                                \
    CALC_SAD_QPEL_ACC( partSad.row(1), vert, fpel2.row(j+2&7), src.row(j) );                                \
    CALC_SAD_QPEL_ACC( partSad.row(2), vert, horzR,            src.row(j) );                                \
    CALC_SAD_QPEL_ACC( partSad.row(3), vert, diagL,            src.row(j) );                                \
    CALC_SAD_QPEL_ACC( partSad.row(5), vert, diagR,            src.row(j) );                                \
    SHIFT_SATUR( horzL, tmp, tmpL.row(j+2&3), 3 );                                                          \
    SHIFT_SATUR( horzR, tmp, tmpR.row(j+2&3), 3 );                                                          \
    CALC_SAD_QPEL_ACC( partSad.row(6), vert,  horzL,            src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(7), vert,  fpel2.row(j+3&7), src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(8), vert,  horzR,            src.row(j) )

    for (int i = 0; i < H; i += 8)
    {
        read_plane( SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc, ysrc + i, src);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref + i + 2, NROWS(fpel0,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 2, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 2, NROWS(fpel2,4,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 2, NROWS(fpel3,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 2, NROWS(fpel4,0,4) );

        ONE_STEP(0);
        ONE_STEP(1);
        ONE_STEP(2);
        ONE_STEP(3);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref + i + 6, NROWS(fpel0,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 6, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 6, NROWS(fpel2,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 6, NROWS(fpel3,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 6, NROWS(fpel4,0,4) );

        ONE_STEP(4);
        ONE_STEP(5);
        ONE_STEP(6);
        ONE_STEP(7);
    }

#undef ONE_STEP

    SumSad<W>(partSad, sad);
}


template<int W, int H>
_GENX_
void InterpolateQpelFromHalfPelHorz(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 9> sad)
{
    matrix<uchar, 8, W> src;
    matrix<uchar, 4, W> fpel1, fpel4;
    matrix<uchar, 8, W> fpel2, fpel3;
    matrix<short, 4, W> tmpR;
    vector<short,    W> tmp;
    vector<uchar,    W> horzR;
    vector<uchar,    W> diagR;
    vector<uchar,    W> vert;
    vector<uchar,    W> vertR;
    matrix<ushort, 9, W/2> partSad = 0;

    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref - 2, NROWS(fpel1,0,4) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref - 2, NROWS(fpel2,0,4) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref - 2, NROWS(fpel3,0,4) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref - 2, NROWS(fpel4,0,4) );

    INTERPOLATE( tmpR, fpel1, NROWS(fpel2,0,4), NROWS(fpel3,0,4), fpel4 );
    INTERPOLATE( tmp, tmpR.row(0), tmpR.row(1), tmpR.row(2), tmpR.row(3) );
    SHIFT_SATUR( diagR, tmp, tmp, 6 );
    INTERPOLATE( tmp, fpel2.row(0), fpel2.row(1), fpel2.row(2), fpel2.row(3) );
    SHIFT_SATUR( vert, tmp, tmp, 3 );
    INTERPOLATE( tmp, fpel3.row(0), fpel3.row(1), fpel3.row(2), fpel3.row(3) );
    SHIFT_SATUR( vertR, tmp, tmp, 3 );

#define ONE_STEP(j)                                                                                         \
    SHIFT_SATUR( horzR, tmp, tmpR.row(j+2&3), 3 );                                                          \
    CALC_SAD_ACC( partSad.row(4), horzR, src.row(j) );                                                      \
    CALC_SAD_QPEL_ACC( partSad.row(0), horzR, vert,             src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(1), horzR, diagR,            src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(2), horzR, vertR,            src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(3), horzR, fpel2.row(j+2&7), src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(5), horzR, fpel3.row(j+2&7), src.row(j) );                               \
    INTERPOLATE( tmpR.row(j+4&3), fpel1.row(j+4&3), fpel2.row(j+4&7), fpel3.row(j+4&7), fpel4.row(j+4&3) ); \
    INTERPOLATE( tmp, tmpR.row(j+1&3), tmpR.row(j+2&3), tmpR.row(j+3&3), tmpR.row(j+4&3) );                 \
    SHIFT_SATUR( diagR, tmp, tmp, 6 );                                                                      \
    INTERPOLATE( tmp, fpel2.row(j+1&7), fpel2.row(j+2&7), fpel2.row(j+3&7), fpel2.row(j+4&7) );             \
    SHIFT_SATUR( vert, tmp, tmp, 3 );                                                                       \
    INTERPOLATE( tmp, fpel3.row(j+1&7), fpel3.row(j+2&7), fpel3.row(j+3&7), fpel3.row(j+4&7) );             \
    SHIFT_SATUR( vertR, tmp, tmp, 3 );                                                                      \
    CALC_SAD_QPEL_ACC( partSad.row(6), horzR, vert,  src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(7), horzR, diagR, src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(8), horzR, vertR, src.row(j) )

    for (int i = 0; i < H; i += 8)
    {
        read_plane( SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc, ysrc + i, src);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 2, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 2, NROWS(fpel2,4,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 2, NROWS(fpel3,4,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 2, NROWS(fpel4,0,4) );

        ONE_STEP(0);
        ONE_STEP(1);
        ONE_STEP(2);
        ONE_STEP(3);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 6, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 6, NROWS(fpel2,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 6, NROWS(fpel3,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 6, NROWS(fpel4,0,4) );

        ONE_STEP(4);
        ONE_STEP(5);
        ONE_STEP(6);
        ONE_STEP(7);
    }

#undef ONE_STEP

    SumSad<W>(partSad, sad);
}


template<int W, int H>
_GENX_
void InterpolateQpelFromHalfPelDiag(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 9> sad)
{
    matrix<uchar, 8, W> src;
    matrix<uchar, 4, W> fpel1, fpel4;
    matrix<uchar, 8, W> fpel2, fpel3;
    matrix<short, 4, W> tmpR;
    vector<short,    W> tmp;
    vector<uchar,    W> horzR;
    vector<uchar,    W> diagR;
    vector<uchar,    W> vert;
    vector<uchar,    W> vertR;
    matrix<ushort, 9, W/2> partSad = 0;

    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref - 1, NROWS(fpel1,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref - 1, NROWS(fpel2,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref - 1, NROWS(fpel3,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref - 1, NROWS(fpel4,1,3) );

    INTERPOLATE( NROWS(tmpR,0,3), NROWS(fpel1,1,3), NROWS(fpel2,1,3), NROWS(fpel3,1,3), NROWS(fpel4,1,3) );
    SHIFT_SATUR( horzR, tmp, tmpR.row(1), 3 );

#define ONE_STEP(j)                                                                                         \
    INTERPOLATE( tmp, fpel2.row(j+1&7), fpel2.row(j+2&7), fpel2.row(j+3&7), fpel2.row(j+4&7) );             \
    SHIFT_SATUR( vert, tmp, tmp, 3 );                                                                       \
    INTERPOLATE( tmp, fpel3.row(j+1&7), fpel3.row(j+2&7), fpel3.row(j+3&7), fpel3.row(j+4&7) );             \
    SHIFT_SATUR( vertR, tmp, tmp, 3 );                                                                      \
    INTERPOLATE( tmpR.row(j+3&3), fpel1.row(j+4&3), fpel2.row(j+4&7), fpel3.row(j+4&7), fpel4.row(j+4&3) ); \
    INTERPOLATE( tmp, tmpR.row(j+0&3), tmpR.row(j+1&3), tmpR.row(j+2&3), tmpR.row(j+3&3) );                 \
    SHIFT_SATUR( diagR, tmp, tmp, 6 );                                                                      \
    CALC_SAD_ACC( partSad.row(4), diagR, src.row(j) );                                                      \
    CALC_SAD_QPEL_ACC( partSad.row(0), horzR, vert,  src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(1), horzR, diagR, src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(2), horzR, vertR, src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(3), diagR, vert,  src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(5), diagR, vertR, src.row(j) );                                          \
    SHIFT_SATUR( horzR, tmp, tmpR.row(j+2&3), 3 );                                                          \
    CALC_SAD_QPEL_ACC( partSad.row(6), horzR, vert,  src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(7), horzR, diagR, src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(8), horzR, vertR, src.row(j) )

    for (int i = 0; i < H; i += 8)
    {
        read_plane( SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc, ysrc + i, src);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 2, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 2, NROWS(fpel2,4,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 2, NROWS(fpel3,4,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 2, NROWS(fpel4,0,4) );

        ONE_STEP(0);
        ONE_STEP(1);
        ONE_STEP(2);
        ONE_STEP(3);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 6, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 6, NROWS(fpel2,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 6, NROWS(fpel3,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 6, NROWS(fpel4,0,4) );

        ONE_STEP(4);
        ONE_STEP(5);
        ONE_STEP(6);
        ONE_STEP(7);
    }

#undef ONE_STEP

    SumSad<W>(partSad, sad);
}


#define QpelRefine(W, H, SURF_SRC, SURF_REF, x, y, hpelMvx, hpelMvy, sad) { \
    int2 xref = x + (hpelMvx >> 2); \
    int2 yref = y + (hpelMvy >> 2); \
    hpelMvx &= 3; \
    hpelMvy &= 3; \
    if (hpelMvx == 0 && hpelMvy == 0) \
        InterpolateQpelFromIntPel< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad ); \
    else if (hpelMvx == 2 && hpelMvy == 0) \
        InterpolateQpelFromHalfPelHorz< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad ); \
    else if (hpelMvx == 0 && hpelMvy == 2) \
        InterpolateQpelFromHalfPelVert< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad ); \
    else if (hpelMvx == 2 && hpelMvy == 2) \
        InterpolateQpelFromHalfPelDiag< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad ); \
}
