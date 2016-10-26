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

enum { W=32, H=8 };

extern "C" _GENX_MAIN_ void CopyVideoToSystem420(SurfaceIndex SRC, SurfaceIndex DST_LU, SurfaceIndex DST_CH, uint srcPaddingLuInBytes, uint srcPaddingChInBytes)
{
    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1,H,W> luma;
    read_plane(SRC, GENX_SURFACE_Y_PLANE, W*x, H*y, luma);
    write(DST_LU, srcPaddingLuInBytes+W*x, H*y, luma);
    matrix<uint1,H/2,W> chroma;
    read_plane(SRC, GENX_SURFACE_UV_PLANE, W*x, H/2*y, chroma);
    write(DST_CH, srcPaddingChInBytes+W*x, H/2*y, chroma);
}
