//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>
#include "../include/utility_genx.h"


template<uint R, uint C>
inline _GENX_ void Average2x2(matrix_ref<uint1,R,C> in, matrix_ref<uint1,R/2,C/2> out)
{
    matrix<uint2,R,C/2> tmp = in.template select<R,1,C/2,2>(0,0) + in.template select<R,1,C/2,2>(0,1);
    matrix<uint2,R/2,C/2> tmp2 = tmp.template select<R/2,2,C/2,1>(0,0) + tmp.template select<R/2,2,C/2,1>(1,0);
    tmp2 += 2;
    out = tmp2 >> 2;
}

extern "C" _GENX_MAIN_ void
DownSampleMB4t(SurfaceIndex SRC, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X)
{
    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1,16,64> in;
    cmut::cmut_read_plane(SRC, GENX_SURFACE_Y_PLANE, 64*x, 16*y, in);

    matrix<uint1,8,32> out2x;
    Average2x2(in.select_all(), out2x.select_all());
    write_plane(DST_2X, GENX_SURFACE_Y_PLANE, 32*x, 8*y, out2x);

    matrix<uint1,4,16> out4x;
    Average2x2(out2x.select_all(), out4x.select_all());
    write_plane(DST_4X, GENX_SURFACE_Y_PLANE, 16*x, 4*y, out4x);

    matrix<uint1,2,8> out8x;
    Average2x2(out4x.select_all(), out8x.select_all());
    write_plane(DST_8X, GENX_SURFACE_Y_PLANE, 8*x, 2*y, out8x);

    matrix<uint1,1,4> out16x;
    Average2x2(out8x.select_all(), out16x.select_all());
    write_plane(DST_16X, GENX_SURFACE_Y_PLANE, 4*x, y, out16x);
}
