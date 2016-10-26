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
#include "../include/genx_hevce_refine_me_p_common.h"


extern "C" _GENX_MAIN_
void RefineMeP16x32Sad(SurfaceIndex SURF_MBDIST_16x32, SurfaceIndex SURF_MBDATA_2X,
                       SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF_F, SurfaceIndex SURF_REF_H,
                       SurfaceIndex SURF_REF_V, SurfaceIndex SURF_REF_D)
{
    enum
    {
        CHUNKW = 16,
        CHUNKH = 32,
        BLOCKW = 16,
        BLOCKH = 16
    };

    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();

    vector< uint,9 > sad;
    vector< int2,2 > mv;
    read(SURF_MBDATA_2X, mbX * MVDATA_SIZE, mbY, mv);

    uint x = mbX * CHUNKW;
    uint y = mbY * CHUNKH;

    sad = QpelRefinement<CHUNKW, CHUNKH, BLOCKW, BLOCKH>(SURF_SRC, SURF_REF_F, SURF_REF_H, SURF_REF_V, SURF_REF_D, x, y, mv[0], mv[1]);

    write(SURF_MBDIST_16x32, mbX * MBDIST_SIZE, mbY, SLICE1(sad, 0, 8));   // 32bytes is max until BDW
    write(SURF_MBDIST_16x32, mbX * MBDIST_SIZE + 32, mbY, SLICE1(sad, 8, 1));
}
