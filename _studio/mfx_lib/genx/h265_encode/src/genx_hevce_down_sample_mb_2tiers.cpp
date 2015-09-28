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


template<uint R, uint C>
inline _GENX_ void Average2x2(matrix_ref<uint1,R,C> in, matrix_ref<uint1,R/2,C/2> out)
{
    matrix<uint2,R,C/2> tmp = in.template select<R,1,C/2,2>(0,0) + in.template select<R,1,C/2,2>(0,1);
    matrix<uint2,R/2,C/2> tmp2 = tmp.template select<R/2,2,C/2,1>(0,0) + tmp.template select<R/2,2,C/2,1>(1,0);
    tmp2 += 2;
    out = tmp2 >> 2;
}

extern "C" _GENX_MAIN_  void
DownSampleMB2t(SurfaceIndex SurfIndex,  // 2 tiers
               SurfaceIndex Surf2XIndex,
               SurfaceIndex Surf4XIndex)
{
    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1,16,32> in;
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, 32*x, 16*y,   in.select<8,1,32,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, 32*x, 16*y+8, in.select<8,1,32,1>(8,0));

    matrix<uint1,8,16> out2x;
    Average2x2(in.select_all(), out2x.select_all());
    write_plane(Surf2XIndex, GENX_SURFACE_Y_PLANE, 16*x, 8*y, out2x);

    matrix<uint1,4,8> out4x;
    Average2x2(out2x.select_all(), out4x.select_all());
    write_plane(Surf4XIndex, GENX_SURFACE_Y_PLANE, 8*x, 4*y, out4x);
}
