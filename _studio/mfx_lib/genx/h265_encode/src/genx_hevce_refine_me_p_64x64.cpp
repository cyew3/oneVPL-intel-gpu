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

template <uint H, uint W> inline _GENX_ vector<uint2,W> Sad(matrix_ref<uint1,H,W> m1, matrix_ref<uint1,H,W> m2)
{
    vector<uint2,W> sad = cm_sad2<uint2>(m1.row(0), m2.row(0));
    #pragma unroll
    for (int i = 1; i < H; i++)
        sad = cm_sada2<uint2>(m1.row(i), m2.row(i), sad);
    return sad;
}

extern "C" _GENX_MAIN_
void RefineMeP64x64(SurfaceIndex DIST, SurfaceIndex MV, SurfaceIndex SRC, SurfaceIndex REF)
{
    enum { W=16, H=16 };

    uint blkx = get_thread_origin_x();
    uint blky = get_thread_origin_y();

    vector<int2,2> mv;
    read(MV, 4 * blkx, blky, mv);

    int x = W * blkx + (mv[0] >> 2);
    int y = H * blky + (mv[1] >> 2);

    matrix<uint1,H,W> src;
    matrix<uint1,H+4,32> ref;
    read_plane(SRC, GENX_SURFACE_Y_PLANE, W*blkx, H*blky, src);
    read_plane(REF, GENX_SURFACE_Y_PLANE, x-2, y-2,    ref.select<8,1,32,1>(0,0));
    read_plane(REF, GENX_SURFACE_Y_PLANE, x-2, y-2+8,  ref.select<8,1,32,1>(8,0));
    read_plane(REF, GENX_SURFACE_Y_PLANE, x-2, y-2+16, ref.select<4,1,32,1>(16,0));

    matrix<int2,H+4,W> tmpH;
    matrix<int2,H+1,W> tmpD;
    matrix<int2,H+1,W> tmpV;
    matrix<uint1,H+1,W> dst;

    tmpV = 4 - ref.select<H+1,1,W,1>(0,2);
    tmpV -= ref.select<H+1,1,W,1>(3,2);
    tmpV += 5 * cm_add<int2>(ref.select<H+1,1,W,1>(1,2), ref.select<H+1,1,W,1>(2,2));
    dst.select<H+1,1,W,1>(0,0) = cm_asr<uint1>(tmpV, 3, SAT);

    matrix<uint2,9,W> sad;
    sad.row(4) = Sad(ref.select<H,1,W,1>(2,2), src.select_all());
    sad.row(1) = Sad(dst.select<H,1,W,1>(0,0), src.select_all());
    sad.row(7) = Sad(dst.select<H,1,W,1>(1,0), src.select_all());

    tmpH = ref.select<H+4,1,W,1>(0,1) + ref.select<H+4,1,W,1>(0,2);
    tmpH *= 5;
    tmpH -= ref.select<H+4,1,W,1>(0,0);
    tmpH -= ref.select<H+4,1,W,1>(0,3);

    tmpD = 32 - tmpH.select<H+1,1,W,1>(0,0);
    tmpD -= tmpH.select<H+1,1,W,1>(3,0);
    tmpD += 5 * cm_add<int2>(tmpH.select<H+1,1,W,1>(1,0), tmpH.select<H+1,1,W,1>(2,0));
    dst.select<H+1,1,W,1>(0,0) = cm_asr<uint1>(tmpD, 6, SAT);
    sad.row(0) = Sad(dst.select<H,1,W,1>(0,0), src.select_all());
    sad.row(6) = Sad(dst.select<H,1,W,1>(1,0), src.select_all());

    tmpH.select<H,1,W,1>(2,0) += 4;
    dst.select<H,1,W,1>(0,0) = cm_asr<uint1>(tmpH.select<H,1,W,1>(2,0), 3, SAT);
    sad.row(3) = Sad(dst.select<H,1,W,1>(0,0), src.select_all());

    tmpH = ref.select<H+4,1,W,1>(0,2) + ref.select<H+4,1,W,1>(0,3);
    tmpH *= 5;
    tmpH -= ref.select<H+4,1,W,1>(0,1);
    tmpH -= ref.select<H+4,1,W,1>(0,4);

    tmpD = 32 - tmpH.select<H+1,1,W,1>(0,0);
    tmpD -= tmpH.select<H+1,1,W,1>(3,0);
    tmpD += 5 * cm_add<int2>(tmpH.select<H+1,1,W,1>(1,0), tmpH.select<H+1,1,W,1>(2,0));
    dst.select<H+1,1,W,1>(0,0) = cm_asr<uint1>(tmpD, 6, SAT);
    sad.row(2) = Sad(dst.select<H,1,W,1>(0,0), src.select_all());
    sad.row(8) = Sad(dst.select<H,1,W,1>(1,0), src.select_all());

    tmpH.select<H,1,W,1>(2,0) += 4;
    dst.select<H,1,W,1>(0,0) = cm_asr<uint1>(tmpH.select<H,1,W,1>(2,0), 3, SAT);
    sad.row(5) = Sad(dst.select<H,1,W,1>(0,0), src.select_all());

    matrix<uint4,9,W/4> tmp;
    tmp = sad.select<9,1,W/4,4>(0,0) + sad.select<9,1,W/4,4>(0,2);
    matrix<uint4,9,W/8> tmp2;
    tmp2 = tmp.select<9,1,W/8,2>(0,0) + tmp.select<9,1,W/8,2>(0,1);
    vector<uint4,9> tmp3;
    tmp3 = tmp2.select<9,1,1,1>(0,0) + tmp2.select<9,1,1,1>(0,1);

    write(DIST, 64*blkx, blky, tmp3.select<8,1>(0));
    write(DIST, 64*blkx+32, blky, tmp3.select<1,1>(8));
}

