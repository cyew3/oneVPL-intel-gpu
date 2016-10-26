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
#include "..\include\genx_hevce_refine_me_p_common_old.h"


extern "C" _GENX_MAIN_
void RefineMeP32x16(SurfaceIndex SURF_MBDIST_32x16, SurfaceIndex SURF_MV32X16,
                    SurfaceIndex SURF_SRC_1X, SurfaceIndex SURF_REF_1X)
{
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();

    vector< uint,9 > sad32x16;
    vector< int2,2 > mv32x16;
    read(SURF_MV32X16, mbX * MVDATA_SIZE, mbY, mv32x16);

    uint x = mbX * 32;
    uint y = mbY * 16;

    QpelRefine(32, 16, SURF_SRC_1X, SURF_REF_1X, x, y, mv32x16[0], mv32x16[1], sad32x16);

    write(SURF_MBDIST_32x16, mbX * MBDIST_SIZE, mbY, SLICE1(sad32x16, 0, 8));   // 32bytes is max until BDW
    write(SURF_MBDIST_32x16, mbX * MBDIST_SIZE + 32, mbY, SLICE1(sad32x16, 8, 1));
}
