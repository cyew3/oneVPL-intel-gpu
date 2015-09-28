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

#define BORDER 4
#define MVDATA_SIZE     4 // mfxI16Pair
#define MBDIST_SIZE     64  // 16*mfxU32
#define MODE_SATD4 4
#define MODE_SATD8 8
#define SATD_BLOCKH 16
#define SATD_BLOCKW 16

#define SLICE(VEC, FROM, HOWMANY, STEP) ((VEC).select<HOWMANY, STEP>(FROM))
#define SLICE1(VEC, FROM, HOWMANY) SLICE(VEC, FROM, HOWMANY, 1)

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void read_block(SurfaceIndex SURF_IN, uint x, uint y, matrix<uint1, BLOCKH, BLOCKW>& m_in)
{
    enum
    {
        MAX_SIZE = 256
    };

    if (BLOCKH*BLOCKW <= MAX_SIZE)
    {
        read(SURF_IN, x, y, m_in);
    }
    else
    {
        const uint count = (BLOCKH*BLOCKW + MAX_SIZE - 1)/MAX_SIZE;
        const uint lines = MAX_SIZE/BLOCKW; 
        const uint done = lines*(count-1);
        const uint rest = BLOCKH - done;
        
        #pragma unroll
        for (int i=0; i<count-1; i++)
        {
            read(SURF_IN, x, y+lines*i, m_in.template select<lines, 1, BLOCKW, 1>(lines*i, 0));
        }
        read(SURF_IN, x, y+done, m_in.template select<rest, 1, BLOCKW, 1>(done, 0));
    }
}


template<uint R, uint C, uint WIDTH>
void _GENX_ inline sad_fixWidth(matrix_ref<uchar, R, C> a, matrix_ref<uchar, R, C> b, vector<ushort, WIDTH> &sum) {
    
    sum = 0;
    matrix_ref<uchar, R*C/WIDTH, WIDTH> a_ref = a.template format<uchar, R*C/WIDTH, WIDTH>();
    matrix_ref<uchar, R*C/WIDTH, WIDTH> b_ref = b.template format<uchar, R*C/WIDTH, WIDTH>();

#pragma unroll
    for (int i = 0; i<R*C/WIDTH; i++) {
        sum = cm_sada2<ushort>(a_ref.row(i), b_ref.row(i), sum);
    }
}


template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSADFromIntPel(matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             matrix<uint1, BLOCKH, BLOCKW> m_src,
                             vector< uint,9 >& sad)
{
    enum
    {
        SAD_WIDTH = 16,
    };

    matrix<uint1, BLOCKH, BLOCKW> m_avg;
    vector<ushort, SAD_WIDTH> m_sad;

    m_avg = cm_avg<uint1>(m_ref_top, m_ref_left);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(0) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));    

    m_avg = cm_avg<uint1>(m_ref_top, m_ref);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(1) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref_top, m_ref_right);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(2) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(3) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   

    sad_fixWidth(m_ref.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(4) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0)); 

    m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(5) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   
 
    m_avg = cm_avg<uint1>(m_ref_bot, m_ref_left);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(6) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref_bot, m_ref);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(7) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));    

    m_avg = cm_avg<uint1>(m_ref_bot, m_ref_right);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(8) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   
}


template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSADFromHalfPel( matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             matrix<uint1, BLOCKH, BLOCKW> m_src,
                             vector< uint,9 >& sad)
{
    enum
    {
        SAD_WIDTH = 16,
    };

    matrix<uint1, BLOCKH, BLOCKW> m_avg;
    vector<ushort, SAD_WIDTH> m_sad;
 
    m_avg = cm_avg<uint1>(m_ref, m_ref_topleft);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH,1,BLOCKW,1>(0,0), m_sad);
    sad(0) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_top);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(1) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_topright);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(2) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(3) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   

    sad_fixWidth(m_ref.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(4) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0)); 

    m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(5) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   
 
    m_avg = cm_avg<uint1>(m_ref, m_ref_botleft);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(6) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_bot);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(7) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   

    m_avg = cm_avg<uint1>(m_ref, m_ref_botright);
    sad_fixWidth(m_avg.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_src.template select<BLOCKH, 1, BLOCKW, 1>(0,0), m_sad);
    sad(8) = cm_sum<uint>(m_sad.template select<SAD_WIDTH/2, 2>(0));   
}


template <uint W, uint H, uint BLOCKW, uint BLOCKH>
inline _GENX_
void InterpolateQpelAtIntPel(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW> m_src;
    matrix<uint1, BLOCKH, BLOCKW> m_ref;
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref_hori;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref_vert;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;
    //#pragma unroll
    for (int i=0; i<H/BLOCKH; i++)
        //#pragma unroll
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;
            
            read_block(SURF_SRC, x, y, m_src);
            read_block(SURF_REF, xref, yref, m_ref);
            read_block(SURF_HPEL_HORZ, xref-1+BORDER, yref+BORDER,  m_ref_hori);
            read_block(SURF_HPEL_VERT, xref+BORDER, yref-1+BORDER,  m_ref_vert);
            
            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref_hori.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref_hori.template select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            CalcQpelSADFromIntPel<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, m_ref, m_src, sad_inter);
            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH>
inline _GENX_
void InterpolateQpelAtHalfPelDiag(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW> m_src;
    matrix<uint1, BLOCKH, BLOCKW> m_ref;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref_hori;
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref_vert;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;
    //#pragma unroll
    for (int i=0; i<H/BLOCKH; i++)
        //#pragma unroll
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;

            read_block(SURF_SRC, x, y, m_src);
            read_block(SURF_HPEL_DIAG, xref+BORDER, yref+BORDER, m_ref);
            read_block(SURF_HPEL_HORZ, xref+BORDER, yref+BORDER,  m_ref_hori);
            read_block(SURF_HPEL_VERT, xref+BORDER, yref+BORDER,  m_ref_vert);
            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref_hori.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref_hori.template select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            CalcQpelSADFromIntPel<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, m_ref, m_src, sad_inter);
            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH>
inline _GENX_
void InterpolateQpelAtHalfPelHorz(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW> m_src;
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref;
    matrix<uint1, BLOCKH, BLOCKW> m_ref_hori;
    matrix<uint1, BLOCKH+1, BLOCKW*2> m_ref_vert;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref_diag;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;
    //#pragma unroll
    for (int i=0; i<H/BLOCKH; i++)
        //#pragma unroll
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;

            read_block(SURF_SRC, x, y, m_src);
            read_block(SURF_REF, xref, yref, m_ref);
            read_block(SURF_HPEL_DIAG, xref+BORDER, yref-1+BORDER, m_ref_diag);
            read_block(SURF_HPEL_HORZ, xref+BORDER, yref+BORDER,  m_ref_hori);
            read_block(SURF_HPEL_VERT, xref+BORDER, yref-1+BORDER,  m_ref_vert);
            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref.template select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref_diag.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref_diag.template select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_topright = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(1, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botright = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(1, 1);

            CalcQpelSADFromHalfPel<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, 
                m_ref_topleft, m_ref_topright, m_ref_botleft, m_ref_botright, m_ref_hori, m_src, sad_inter);
            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH>
inline _GENX_
void InterpolateQpelAtHalfPelVert(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW> m_src;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref;
    matrix<uint1, BLOCKH+1, BLOCKW*2> m_ref_hori;
    matrix<uint1, BLOCKH, BLOCKW> m_ref_vert;
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref_diag;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;
    //#pragma unroll
    for (int i=0; i<H/BLOCKH; i++)
        //#pragma unroll
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;

            read_block(SURF_SRC, x, y, m_src);
            read_block(SURF_REF, xref, yref, m_ref);
            read_block(SURF_HPEL_DIAG, xref-1+BORDER, yref+BORDER, m_ref_diag);
            read_block(SURF_HPEL_HORZ, xref-1+BORDER, yref+BORDER,  m_ref_hori);
            read_block(SURF_HPEL_VERT, xref+BORDER, yref+BORDER,  m_ref_vert);
            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref_diag.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref_diag.template select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref.template select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft = m_ref_hori.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_topright = m_ref_hori.template select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft = m_ref_hori.template select<BLOCKH, 1, BLOCKW, 1>(1, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botright = m_ref_hori.template select<BLOCKH, 1, BLOCKW, 1>(1, 1);

            CalcQpelSADFromHalfPel<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, 
                m_ref_topleft, m_ref_topright, m_ref_botleft, m_ref_botright, m_ref_vert, m_src, sad_inter);
            sad += sad_inter;
        }

}


template <uint W, uint H, uint BLOCKW, uint BLOCKH>
_GENX_
vector< uint,9 > QpelRefinement(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                    SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                    uint x, uint y, int2 hpelMvx, int2 hpelMvy) 
{                
    static_assert(((BLOCKW & (BLOCKW - 1)) == 0), "Block width has to be power of 2");

    int2 xref = x + (hpelMvx >> 2);
    int2 yref = y + (hpelMvy >> 2);
    hpelMvx &= 3;
    hpelMvy &= 3;
    vector< uint,9 > sad;

    if (hpelMvx == 0 && hpelMvy == 0)
        InterpolateQpelAtIntPel< W, H, BLOCKW, BLOCKH >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, sad );
    else if (hpelMvx == 2 && hpelMvy == 0)
        InterpolateQpelAtHalfPelHorz< W, H, BLOCKW, BLOCKH >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, sad );
    else if (hpelMvx == 0 && hpelMvy == 2)
        InterpolateQpelAtHalfPelVert< W, H, BLOCKW, BLOCKH >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, sad );
    else if (hpelMvx == 2 && hpelMvy == 2)
        InterpolateQpelAtHalfPelDiag< W, H, BLOCKW, BLOCKH >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, sad );
    
    return sad;
}

template<uint W, uint H>
void _GENX_ inline transpose4Lines(matrix<short, H, W>& a) {
   
    matrix_ref<int, H, W/2> a_int = a.template format<int, H, W/2>();
    matrix<short, 2, W/2> b;
    matrix_ref<int, 2, W/4> b_int = b.template format<int, 2, W/4>();

#pragma unroll
    for (int i=0; i<H/4; i++)
    {
        b_int = a_int.template select<2, 1, W/4, 2>(i*4, 1);
        a_int.template select<2, 1, W/4, 2>(i*4, 1) = a_int.template select<2, 1, W/4, 2>(i*4+2, 0);
        a_int.template select<2, 1, W/4, 2>(i*4+2, 0) = b_int;
    }

#pragma unroll
    for (int i=0; i<H/4; i++)
    {
        b = a.template select<2, 2, W/2, 2>(i*4, 1);
        a.template select<2, 2, W/2, 2>(i*4, 1) = a.template select<2, 2, W/2, 2>(i*4+1, 0);
        a.template select<2, 2, W/2, 2>(i*4+1, 0) = b;
    }
    
}

template<uint W, uint H>
void _GENX_ inline transpose8Lines(matrix<short, H, W>& a) {
   
    matrix<short, 4, 4> b;
#pragma unroll
    for (int i=0; i<H/8; i++)
    {
#pragma unroll
        for (int j=0; j<W/8; j++)
        {
            b = a.template select<4, 1, 4, 1>(i*8, j*8+4);
            a.template select<4, 1, 4, 1>(i*8, j*8+4) = a.template select<4, 1, 4, 1>(i*8+4, j*8);
            a.template select<4, 1, 4, 1>(i*8+4, j*8) = b;
        }
    }

    matrix<short, 2, W/2> c;
    matrix_ref<int, 2, W/4> c_int = c.template format<int, 2, W/4>();
    matrix_ref<int, H, W/2> a_int = a.template format<int, H, W/2>();

#pragma unroll
    for (int i=0; i<H/4; i++)
    {
        c_int = a_int.template select<2, 1, W/4, 2>(i*4, 1);
        a_int.template select<2, 1, W/4, 2>(i*4, 1) = a_int.template select<2, 1, W/4, 2>(i*4+2, 0);
        a_int.template select<2, 1, W/4, 2>(i*4+2, 0) = c_int;
    }

#pragma unroll
    for (int i=0; i<H/4; i++)
    {
        c = a.template select<2, 2, W/2, 2>(i*4, 1);
        a.template select<2, 2, W/2, 2>(i*4, 1) = a.template select<2, 2, W/2, 2>(i*4+1, 0);
        a.template select<2, 2, W/2, 2>(i*4+1, 0) = c;
    }
}

template<uint BLOCKH, uint BLOCKW>
_GENX_
uint CalcBlockSATD4x4Combine() {
    
    uint satd = 0;
    const uint WIDTH = 4;

    matrix<short, BLOCKH, BLOCKW> diff = cm_add<short>(g_m_avg, -g_m_src);;
    vector<short, BLOCKW> a01, a23, b01, b23;
    vector<uint, BLOCKW> abs = 0;

#pragma unroll
    for (int i=0; i<BLOCKH/WIDTH; i++)
    {
        a01 = diff.row(0 + i*WIDTH) + diff.row(1 + i*WIDTH);
        a23 = diff.row(2 + i*WIDTH) + diff.row(3 + i*WIDTH);
        b01 = diff.row(0 + i*WIDTH) - diff.row(1 + i*WIDTH);
        b23 = diff.row(2 + i*WIDTH) - diff.row(3 + i*WIDTH);

        diff.row(0 + i*WIDTH) = a01 + a23;
        diff.row(1 + i*WIDTH) = a01 - a23;
        diff.row(2 + i*WIDTH) = b01 - b23;
        diff.row(3 + i*WIDTH) = b01 + b23;
    }

    transpose4Lines(diff);
#pragma unroll
    for (int i=0; i<BLOCKH/WIDTH; i++)
    {
        a01 = diff.row(0 + i*WIDTH) + diff.row(1 + i*WIDTH);
        a23 = diff.row(2 + i*WIDTH) + diff.row(3 + i*WIDTH);
        b01 = diff.row(0 + i*WIDTH) - diff.row(1 + i*WIDTH);
        b23 = diff.row(2 + i*WIDTH) - diff.row(3 + i*WIDTH);

        abs += cm_abs<uint>(cm_add<int2>(a01, a23));
        abs += cm_abs<uint>(cm_add<int2>(a01, -a23));
        abs += cm_abs<uint>(cm_add<int2>(b01, b23));
        abs += cm_abs<uint>(cm_add<int2>(b01, -b23));
    }

    satd = cm_sum<uint>(abs);

    return satd;
}

template<uint BLOCKH, uint BLOCKW>
_GENX_ 
uint CalcBlockSATD8x8Combine() {
    
    uint satd = 0;
    const uint WIDTH = 8;

    matrix<short, BLOCKH, BLOCKW> diff = cm_add<short>(g_m_avg, -g_m_src);
    vector<short, BLOCKW> t0, t1, t2, t3, t4, t5, t6, t7;
    vector<short, BLOCKW> s0, s1, s2, s3, s4, s5, s6, s7;
    vector<uint, BLOCKW> abs = 0;

#pragma unroll
    for (int i=0; i<BLOCKH/WIDTH; i++)
    {
        t0 = diff.row(0 + i*WIDTH) + diff.row(4 + i*WIDTH);
        t4 = diff.row(0 + i*WIDTH) - diff.row(4 + i*WIDTH);
        t1 = diff.row(1 + i*WIDTH) + diff.row(5 + i*WIDTH);
        t5 = diff.row(1 + i*WIDTH) - diff.row(5 + i*WIDTH);
        t2 = diff.row(2 + i*WIDTH) + diff.row(6 + i*WIDTH);
        t6 = diff.row(2 + i*WIDTH) - diff.row(6 + i*WIDTH);
        t3 = diff.row(3 + i*WIDTH) + diff.row(7 + i*WIDTH);
        t7 = diff.row(3 + i*WIDTH) - diff.row(7 + i*WIDTH);

        s0 = t0 + t2;
        s2 = t0 - t2;
        s1 = t1 + t3;
        s3 = t1 - t3;
        s4 = t4 + t6;
        s6 = t4 - t6;
        s5 = t5 + t7;
        s7 = t5 - t7;

        diff.row(0+ i*WIDTH) = s0 + s1;
        diff.row(1+ i*WIDTH) = s0 - s1;
        diff.row(2+ i*WIDTH) = s2 + s3;
        diff.row(3+ i*WIDTH) = s2 - s3;
        diff.row(4+ i*WIDTH) = s4 + s5;
        diff.row(5+ i*WIDTH) = s4 - s5;
        diff.row(6+ i*WIDTH) = s6 + s7;
        diff.row(7+ i*WIDTH) = s6 - s7;
    }

    transpose8Lines(diff);
#pragma unroll
    for (int i=0; i<BLOCKH/WIDTH; i++)
    {
        t0 = diff.row(0 + i*WIDTH) + diff.row(4 + i*WIDTH);
        t4 = diff.row(0 + i*WIDTH) - diff.row(4 + i*WIDTH);
        t1 = diff.row(1 + i*WIDTH) + diff.row(5 + i*WIDTH);
        t5 = diff.row(1 + i*WIDTH) - diff.row(5 + i*WIDTH);
        t2 = diff.row(2 + i*WIDTH) + diff.row(6 + i*WIDTH);
        t6 = diff.row(2 + i*WIDTH) - diff.row(6 + i*WIDTH);
        t3 = diff.row(3 + i*WIDTH) + diff.row(7 + i*WIDTH);
        t7 = diff.row(3 + i*WIDTH) - diff.row(7 + i*WIDTH);

        s0 = t0 + t2;
        s2 = t0 - t2;
        s1 = t1 + t3;
        s3 = t1 - t3;
        s4 = t4 + t6;
        s6 = t4 - t6;
        s5 = t5 + t7;
        s7 = t5 - t7;

        abs += cm_abs<uint>(cm_add<int2>(s0, s1));
        abs += cm_abs<uint>(cm_add<int2>(s0, -s1));
        abs += cm_abs<uint>(cm_add<int2>(s2, s3));
        abs += cm_abs<uint>(cm_add<int2>(s2, -s3));
        abs += cm_abs<uint>(cm_add<int2>(s4, s5));
        abs += cm_abs<uint>(cm_add<int2>(s4, -s5));
        abs += cm_abs<uint>(cm_add<int2>(s6, s7));
        abs += cm_abs<uint>(cm_add<int2>(s6, -s7));
    }

    satd = cm_sum<uint>(abs);

    return satd;
}

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSATDFromIntDiagPel4x4(matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             vector< uint,9 >& sad)
{
    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref_left);
    sad(0) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref);
    sad(1) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref_right);
    sad(2) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad(3) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = m_ref;
    sad(4) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad(5) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();
 
    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref_left);
    sad(6) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref);
    sad(7) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref_right);
    sad(8) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();
   
}

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSATDFromIntDiagPel8x8(matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             vector< uint,9 >& sad)
{
    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref_left);
    sad(0) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref);
    sad(1) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_top, m_ref_right);
    sad(2) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad(3) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = m_ref;
    sad(4) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad(5) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();
 
    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref_left);
    sad(6) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref);
    sad(7) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref_bot, m_ref_right);
    sad(8) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();
   
}

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSATDFromHalfPel4x4( matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             vector< uint,9 >& sad)
{
    g_m_avg = cm_avg<uint1>(m_ref, m_ref_topleft);
    sad(0) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_top);
    sad(1) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_topright);
    sad(2) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad(3) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = m_ref;
    sad(4) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad(5) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();
 
    g_m_avg = cm_avg<uint1>(m_ref, m_ref_botleft);
    sad(6) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_bot);
    sad(7) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_botright);
    sad(8) = CalcBlockSATD4x4Combine<BLOCKH, BLOCKW>();
    
}

template <uint BLOCKW, uint BLOCKH>
inline _GENX_
void CalcQpelSATDFromHalfPel8x8( matrix<uint1, BLOCKH, BLOCKW> m_ref_left,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_right,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_top,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_bot,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_topright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref_botright,
                             matrix<uint1, BLOCKH, BLOCKW> m_ref,
                             vector< uint,9 >& sad)
{
    g_m_avg = cm_avg<uint1>(m_ref, m_ref_topleft);
    sad(0) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_top);
    sad(1) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_topright);
    sad(2) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_left);
    sad(3) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = m_ref;
    sad(4) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_right);
    sad(5) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();
 
    g_m_avg = cm_avg<uint1>(m_ref, m_ref_botleft);
    sad(6) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_bot);
    sad(7) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();

    g_m_avg = cm_avg<uint1>(m_ref, m_ref_botright);
    sad(8) = CalcBlockSATD8x8Combine<BLOCKH, BLOCKW>();
    
}

template <uint W, uint H, uint BLOCKW, uint BLOCKH, uint SADMODE>
inline _GENX_
void InterpolateQpelAtIntDiagPelSATD(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, int2 hpelMvx, int2 hpelMvy, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW> m_ref;
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref_hori;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref_vert;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;

    for (int i=0; i<H/BLOCKH; i++)
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;
            
            read_block(SURF_SRC, x, y, g_m_src);

            if (hpelMvx == 0 && hpelMvy == 0){
                read_block(SURF_REF, xref, yref, m_ref);
                read_block(SURF_HPEL_HORZ, xref-1+BORDER, yref+BORDER,  m_ref_hori);
                read_block(SURF_HPEL_VERT, xref+BORDER, yref-1+BORDER,  m_ref_vert);
            } 
            else {
                read_block(SURF_HPEL_DIAG, xref+BORDER, yref+BORDER, m_ref);
                read_block(SURF_HPEL_HORZ, xref+BORDER, yref+BORDER,  m_ref_vert);
                read_block(SURF_HPEL_VERT, xref+BORDER, yref+BORDER,  m_ref_hori);
            }
            
            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref_hori.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref_hori.template select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            if (MODE_SATD4 == SADMODE)
            {
                CalcQpelSATDFromIntDiagPel4x4<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, m_ref, sad_inter);
            }
            else
            {
                CalcQpelSATDFromIntDiagPel8x8<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, m_ref, sad_inter);
            }

            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH, uint SADMODE>
inline _GENX_
void InterpolateQpelAtHoriVertPelSATD(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                             SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                             uint x, uint y, int2 xref, int2 yref, int2 hpelMvx, int2 hpelMvy, vector< uint,9 >& sad)
{
    matrix<uint1, BLOCKH, BLOCKW*2> m_ref;
    matrix<uint1, BLOCKH, BLOCKW> m_ref_hori;
    matrix<uint1, BLOCKH+1, BLOCKW*2> m_ref_vert;
    matrix<uint1, BLOCKH+1, BLOCKW> m_ref_diag;

    sad = 0;
    int x0=x, xref0=xref;
    int y0=y, yref0=yref;

    for (int i=0; i<H/BLOCKH; i++)
        for (int j=0; j<W/BLOCKW; j++)
        {
            x = x0 + BLOCKW*j;
            y = y0 + BLOCKH*i;
            xref = xref0 + BLOCKW*j;
            yref = yref0 + BLOCKH*i;

            read_block(SURF_SRC, x, y, g_m_src);

            if (hpelMvx == 2 && hpelMvy == 0)
            {
                read_block(SURF_REF, xref, yref, m_ref);
                read_block(SURF_HPEL_DIAG, xref+BORDER, yref-1+BORDER, m_ref_diag);
                read_block(SURF_HPEL_HORZ, xref+BORDER, yref+BORDER,  m_ref_hori);
                read_block(SURF_HPEL_VERT, xref+BORDER, yref-1+BORDER,  m_ref_vert);
            }
            else
            {
                read_block(SURF_REF, xref, yref, m_ref_diag);
                read_block(SURF_HPEL_DIAG, xref-1+BORDER, yref+BORDER, m_ref);
                read_block(SURF_HPEL_HORZ, xref-1+BORDER, yref+BORDER,  m_ref_vert);
                read_block(SURF_HPEL_VERT, xref+BORDER, yref+BORDER,  m_ref_hori);        
            }

            vector< uint,9 > sad_inter;

            matrix<uint1, BLOCKH, BLOCKW> m_ref_left = m_ref.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_right = m_ref.template select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_top = m_ref_diag.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_bot = m_ref_diag.template select<BLOCKH, 1, BLOCKW, 1>(1, 0);

            matrix<uint1, BLOCKH, BLOCKW> m_ref_topleft = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(0, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_topright = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(0, 1);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botleft = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(1, 0);
            matrix<uint1, BLOCKH, BLOCKW> m_ref_botright = m_ref_vert.template select<BLOCKH, 1, BLOCKW, 1>(1, 1);

            if (MODE_SATD4 == SADMODE) 
            {
                 CalcQpelSATDFromHalfPel4x4<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, 
                    m_ref_topleft, m_ref_topright, m_ref_botleft, m_ref_botright, m_ref_hori, sad_inter);
            }
            else
            {
                 CalcQpelSATDFromHalfPel8x8<BLOCKW, BLOCKH>(m_ref_left, m_ref_right, m_ref_top, m_ref_bot, 
                    m_ref_topleft, m_ref_topright, m_ref_botleft, m_ref_botright, m_ref_hori, sad_inter);
           
            }
            sad += sad_inter;
        }

}

template <uint W, uint H, uint BLOCKW, uint BLOCKH, uint SADMODE>
_GENX_
vector< uint,9 > QpelRefinementSATD(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                    SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG, 
                    uint x, uint y, int2 hpelMvx, int2 hpelMvy) 
{                
    static_assert(((BLOCKW & (BLOCKW - 1)) == 0), "Block width has to be power of 2");

    int2 xref = x + (hpelMvx >> 2);
    int2 yref = y + (hpelMvy >> 2);
    hpelMvx &= 3;
    hpelMvy &= 3;
    vector< uint,9 > sad;

    if ((hpelMvx == 0 && hpelMvy == 0) || (hpelMvx == 2 && hpelMvy == 2))
        InterpolateQpelAtIntDiagPelSATD< W, H, BLOCKW, BLOCKH, SADMODE >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, hpelMvx, hpelMvy, sad );
    else if ((hpelMvx == 2 && hpelMvy == 0) || (hpelMvx == 0 && hpelMvy == 2))
        InterpolateQpelAtHoriVertPelSATD< W, H, BLOCKW, BLOCKH, SADMODE >( SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, xref, yref, hpelMvx, hpelMvy, sad );
    return sad;
}
