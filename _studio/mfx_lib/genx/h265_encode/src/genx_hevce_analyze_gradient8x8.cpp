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
#include "../include/utility_genx.h"
#include "../include/genx_hevce_analyze_gradient_common.h"

using namespace cmut;

extern "C" _GENX_MAIN_  void
AnalyzeGradient8x8(SurfaceIndex SURF_SRC,
SurfaceIndex SURF_GRADIENT_8x8,
uint4        width)
{
    enum {
        W = 8,
        H = 8,
        HISTSIZE = sizeof(uint2)* 40,
    };

    static_assert(W % 8 == 0 && H % 8 == 0, "Width and Height must be multiple of 8.");

    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1, 6, 8> data;
    matrix<int2, 8, 8> dx, dy;
    matrix<uint2, 8, 8> amp;
    matrix<uint2, 8, 8> ang;
    vector<uint2, 40> histogram4x4;
    vector<uint2, 40> histogram8x8 = 0;
    const int yBase = y * H;
    const int xBase = x * W;
    ReadGradient(SURF_SRC, xBase, yBase, dx, dy);
    Gradiant2AngAmp_MaskAtan2(dx, dy, ang, amp);
    Histogram_iselect(ang.select_all(), amp.select_all(), histogram8x8.select<histogram8x8.SZ, 1>(0));

    uint offset = ((width / 8) * y + x) * HISTSIZE;
    write(SURF_GRADIENT_8x8, offset + 0, cmut_slice<32>(histogram8x8, 0));
    write(SURF_GRADIENT_8x8, offset + 64, cmut_slice<8>(histogram8x8, 32));
}
