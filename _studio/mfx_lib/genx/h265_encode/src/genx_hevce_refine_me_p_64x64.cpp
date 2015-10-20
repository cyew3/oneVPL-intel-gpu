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
#include "../include/genx_hevce_me_common.h"

template <uint H, uint W> inline _GENX_ vector<uint2,W> Sad(matrix_ref<uint1,H,W> m1, matrix_ref<uint1,H,W> m2)
{
    vector<uint2,W> sad = cm_sad2<uint2>(m1.row(0), m2.row(0));
    #pragma unroll
    for (int i = 1; i < H; i++)
        sad = cm_sada2<uint2>(m1.row(i), m2.row(i), sad);
    return sad;
}

static const uint1 INIT_DELTA_MV[9] = {0x00, 0x10, 0x20, 0x01, 0x11, 0x21, 0x02, 0x12, 0x22};

extern "C" _GENX_MAIN_
void RefineMeP64x64(SurfaceIndex DATA, SurfaceIndex SRC, SurfaceIndex REF)
{
    enum { W=16, H=16 };

    uint ox = get_thread_origin_x();
    uint oy = get_thread_origin_y();

    vector<int2,2> mv;
    read(DATA, INTERDATA_SIZE_BIG * ox, oy, mv);
    mv >>= 2;

    int srcx64 = 64 * ox;
    int srcy64 = 64 * oy;
    int refx64 = srcx64 + mv[0];
    int refy64 = srcy64 + mv[1];

    matrix<uint2,9,16> intsad = 0;
    uint4 bestIntSad;
    for (int y = 0; y < 64; y += 16) {
        int refy = refy64 + y;
        int srcy = srcy64 + y;
        for (int x = 0; x < 64; x += 16) {
            int refx = refx64 + x;
            int srcx = srcx64 + x;
            matrix<uint1,H,W> src;
            matrix<uint1,H+2,32> ref;
            read_plane(SRC, GENX_SURFACE_Y_PLANE, srcx,   srcy, src);
            read_plane(REF, GENX_SURFACE_Y_PLANE, refx-1, refy-1,    ref.select<8,1,32,1>(0,0));
            read_plane(REF, GENX_SURFACE_Y_PLANE, refx-1, refy-1+8,  ref.select<8,1,32,1>(8,0));
            read_plane(REF, GENX_SURFACE_Y_PLANE, refx-1, refy-1+16, ref.select<2,1,32,1>(16,0));

            intsad.row(0) += Sad(ref.select<H,1,W,1>(0,0), src.select_all());
            intsad.row(1) += Sad(ref.select<H,1,W,1>(0,1), src.select_all());
            intsad.row(2) += Sad(ref.select<H,1,W,1>(0,2), src.select_all());
            intsad.row(3) += Sad(ref.select<H,1,W,1>(1,0), src.select_all());
            intsad.row(4) += Sad(ref.select<H,1,W,1>(1,1), src.select_all());
            intsad.row(5) += Sad(ref.select<H,1,W,1>(1,2), src.select_all());
            intsad.row(6) += Sad(ref.select<H,1,W,1>(2,0), src.select_all());
            intsad.row(7) += Sad(ref.select<H,1,W,1>(2,1), src.select_all());
            intsad.row(8) += Sad(ref.select<H,1,W,1>(2,2), src.select_all());
        }
    }
    {
    matrix<uint4,9,W/4> tmp  = intsad.select<9,1,W/4,4>(0,0) + intsad.select<9,1,W/4,4>(0,2);
    matrix<uint4,9,W/8> tmp2 = tmp.select<9,1,W/8,2>(0,0) + tmp.select<9,1,W/8,2>(0,1);
    vector<uint4,9> dist = tmp2.select<9,1,1,1>(0,0) + tmp2.select<9,1,1,1>(0,1);
    vector<uint1,9> deltaMv(INIT_DELTA_MV);
    dist <<= 8;
    dist += deltaMv;
    bestIntSad = cm_reduced_min<uint4>(dist);
    mv[0] += ((bestIntSad >> 4) & 0xf) - 1;
    mv[1] += ((bestIntSad     ) & 0xf) - 1;
    bestIntSad >>= 8;
    refx64 = srcx64 + mv[0];
    refy64 = srcy64 + mv[1];
    mv <<= 2;
    write(DATA, INTERDATA_SIZE_BIG * ox, oy, mv);
    }


    matrix<uint2,9,W> sad = 0;

    for (int y16 = 0; y16 < 64; y16 += H) {
        int refy = refy64 + y16;
        int srcy = srcy64 + y16;
        for (int x16 = 0; x16 < 64; x16 += W) {
            matrix<uint1,H,W> src;
            matrix<uint1,H+4,32> ref;
            int refx = refx64 + x16;
            int srcx = srcx64 + x16;
            read_plane(SRC, GENX_SURFACE_Y_PLANE, srcx,   srcy, src);
            read_plane(REF, GENX_SURFACE_Y_PLANE, refx-2, refy-2,    ref.select<8,1,32,1>(0,0));
            read_plane(REF, GENX_SURFACE_Y_PLANE, refx-2, refy-2+8,  ref.select<8,1,32,1>(8,0));
            read_plane(REF, GENX_SURFACE_Y_PLANE, refx-2, refy-2+16, ref.select<4,1,32,1>(16,0));

            matrix<int2,H+4,W> tmpH;
            matrix<int2,H+1,W> tmpD;
            matrix<int2,H+1,W> tmpV;
            matrix<uint1,H+1,W> dst;

            tmpV = 4 - ref.select<H+1,1,W,1>(0,2);
            tmpV -= ref.select<H+1,1,W,1>(3,2);
            tmpV += 5 * cm_add<int2>(ref.select<H+1,1,W,1>(1,2), ref.select<H+1,1,W,1>(2,2));
            dst.select<H+1,1,W,1>(0,0) = cm_asr<uint1>(tmpV, 3, SAT);

            sad.row(1) += Sad(dst.select<H,1,W,1>(0,0), src.select_all());
            sad.row(7) += Sad(dst.select<H,1,W,1>(1,0), src.select_all());

            tmpH = ref.select<H+4,1,W,1>(0,1) + ref.select<H+4,1,W,1>(0,2);
            tmpH *= 5;
            tmpH -= ref.select<H+4,1,W,1>(0,0);
            tmpH -= ref.select<H+4,1,W,1>(0,3);

            tmpD = 32 - tmpH.select<H+1,1,W,1>(0,0);
            tmpD -= tmpH.select<H+1,1,W,1>(3,0);
            tmpD += 5 * cm_add<int2>(tmpH.select<H+1,1,W,1>(1,0), tmpH.select<H+1,1,W,1>(2,0));
            dst.select<H+1,1,W,1>(0,0) = cm_asr<uint1>(tmpD, 6, SAT);
            sad.row(0) += Sad(dst.select<H,1,W,1>(0,0), src.select_all());
            sad.row(6) += Sad(dst.select<H,1,W,1>(1,0), src.select_all());

            tmpH.select<H,1,W,1>(2,0) += 4;
            dst.select<H,1,W,1>(0,0) = cm_asr<uint1>(tmpH.select<H,1,W,1>(2,0), 3, SAT);
            sad.row(3) += Sad(dst.select<H,1,W,1>(0,0), src.select_all());

            tmpH = ref.select<H+4,1,W,1>(0,2) + ref.select<H+4,1,W,1>(0,3);
            tmpH *= 5;
            tmpH -= ref.select<H+4,1,W,1>(0,1);
            tmpH -= ref.select<H+4,1,W,1>(0,4);

            tmpD = 32 - tmpH.select<H+1,1,W,1>(0,0);
            tmpD -= tmpH.select<H+1,1,W,1>(3,0);
            tmpD += 5 * cm_add<int2>(tmpH.select<H+1,1,W,1>(1,0), tmpH.select<H+1,1,W,1>(2,0));
            dst.select<H+1,1,W,1>(0,0) = cm_asr<uint1>(tmpD, 6, SAT);
            sad.row(2) += Sad(dst.select<H,1,W,1>(0,0), src.select_all());
            sad.row(8) += Sad(dst.select<H,1,W,1>(1,0), src.select_all());

            tmpH.select<H,1,W,1>(2,0) += 4;
            dst.select<H,1,W,1>(0,0) = cm_asr<uint1>(tmpH.select<H,1,W,1>(2,0), 3, SAT);
            sad.row(5) += Sad(dst.select<H,1,W,1>(0,0), src.select_all());
        }
    }

    // final sads (switch to uint4, uint2 is not enough)
    matrix<uint4,9,W/4> tmp  = sad.select<9,1,W/4,4>(0,0) + sad.select<9,1,W/4,4>(0,2);
    matrix<uint4,9,W/8> tmp2 = tmp.select<9,1,W/8,2>(0,0) + tmp.select<9,1,W/8,2>(0,1);
    vector<uint4,9> dist = tmp2.select<9,1,1,1>(0,0) + tmp2.select<9,1,1,1>(0,1);
    dist[4] = bestIntSad;

    write(DATA, INTERDATA_SIZE_BIG * ox + 4,  oy, dist.select<8,1>(0));
    write(DATA, INTERDATA_SIZE_BIG * ox + 36, oy, dist.select<1,1>(8));
}
