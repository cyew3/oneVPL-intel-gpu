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
void RefineMeP16x32(SurfaceIndex SURF_MBDIST_16x32, SurfaceIndex SURF_MV16X32,
                    SurfaceIndex SURF_SRC_1X, SurfaceIndex SURF_REF_1X)
{
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();

    vector< uint,9 > sad16x32;
    vector< int2,2 > mv16x32;
    read(SURF_MV16X32, mbX * MVDATA_SIZE, mbY, mv16x32);

    uint x = mbX * 16;
    uint y = mbY * 32;

    QpelRefine(16, 32, SURF_SRC_1X, SURF_REF_1X, x, y, mv16x32[0], mv16x32[1], sad16x32);

    write(SURF_MBDIST_16x32, mbX * MBDIST_SIZE, mbY, SLICE1(sad16x32, 0, 8));   // 32bytes is max until BDW
    write(SURF_MBDIST_16x32, mbX * MBDIST_SIZE + 32, mbY, SLICE1(sad16x32, 8, 1));
}
