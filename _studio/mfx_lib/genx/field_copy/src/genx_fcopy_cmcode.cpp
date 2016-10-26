//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include <stdio.h>
#include <cm.h>
/*
#include <genx_vme.h>
#include "..\include\genx_fcopy_cmcode.h"
*/

extern "C" _GENX_MAIN_  void
MbCopyFieLd(SurfaceIndex InSurfIndex, SurfaceIndex OutSurfIndex, int fieldMask)
{
    // Luma only
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint ix = mbX << 4; // 16x16
    uint iy = mbY << 4;

    if (fieldMask > 5) {
        matrix<uint1,16,16> inMbY0;
        matrix<uint1,16,16> inMbY1;
        matrix<uint1,16,16> outMbY;
        matrix<uint1,16,16> inMbUV;
        matrix<uint1,8,16> outMbUV;

        read_plane(InSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy * 2, inMbY0);
        read_plane(InSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy * 2 + 16, inMbY1);
        read_plane(InSurfIndex, GENX_SURFACE_UV_PLANE, ix, iy * 2, inMbUV);

        switch (fieldMask)
        {
            case 0x6:  // TFF2FIELD
                outMbY.select<8,1,16,1>(0,0) = inMbY0.select<8,2,16,1>(0,0);
                outMbY.select<8,1,16,1>(8,0) = inMbY1.select<8,2,16,1>(0,0);
                outMbUV.select<8,1,16,1>(0,0) = inMbUV.select<8,2,16,1>(0,0);
                break;
            case 0x7:  // BFF2FIELD
                outMbY.select<8,1,16,1>(0,0) = inMbY0.select<8,2,16,1>(1,0);
                outMbY.select<8,1,16,1>(8,0) = inMbY1.select<8,2,16,1>(1,0);
                outMbUV.select<8,1,16,1>(0,0) = inMbUV.select<8,2,16,1>(1,0);
                break;
            default:
                return;
        }

        write_plane(OutSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy, outMbY);
        write_plane(OutSurfIndex, GENX_SURFACE_UV_PLANE, ix, iy / 2, outMbUV);
    } else if (fieldMask > 3) {
        matrix<uint1,16,16> inMbY;
        matrix<uint1,16,16> outMbY0;
        matrix<uint1,16,16> outMbY1;
        matrix<uint1,8,16> inMbUV;
        matrix<uint1,16,16> outMbUV;

        read_plane(InSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy, inMbY);
        read_plane(OutSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy * 2, outMbY0);
        read_plane(OutSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy * 2 + 16, outMbY1);
        read_plane(InSurfIndex, GENX_SURFACE_UV_PLANE, ix, iy / 2, inMbUV);
        read_plane(OutSurfIndex, GENX_SURFACE_UV_PLANE, ix, iy, outMbUV);

        switch (fieldMask)
        {
            case 0x4:  // FIELD2TFF
                outMbY0.select<8,2,16,1>(0,0) = inMbY.select<8,1,16,1>(0,0);
                outMbY1.select<8,2,16,1>(0,0) = inMbY.select<8,1,16,1>(8,0);
                outMbUV.select<8,2,16,1>(0,0) = inMbUV.select<8,1,16,1>(0,0);
                break;
            case 0x5:  // FIELD2BFF
                outMbY0.select<8,2,16,1>(1,0) = inMbY.select<8,1,16,1>(0,0);
                outMbY1.select<8,2,16,1>(1,0) = inMbY.select<8,1,16,1>(8,0);
                outMbUV.select<8,2,16,1>(1,0) = inMbUV.select<8,1,16,1>(0,0);
                break;
        }

        write_plane(OutSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy * 2, outMbY0);
        write_plane(OutSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy * 2 + 16, outMbY1);
        write_plane(OutSurfIndex, GENX_SURFACE_UV_PLANE, ix, iy, outMbUV);
    } else {
        matrix<uint1,16,16> inMbY;
        matrix<uint1,16,16> outMbY;
        matrix<uint1,8,16> inMbUV;
        matrix<uint1,8,16> outMbUV;

        read_plane(InSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy, inMbY);
        read_plane(OutSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy, outMbY);
        read_plane(InSurfIndex, GENX_SURFACE_UV_PLANE, ix, iy / 2, inMbUV);
        read_plane(OutSurfIndex, GENX_SURFACE_UV_PLANE, ix, iy / 2, outMbUV);

        switch (fieldMask)
        {
            case 0x0:  // TFF2TFF
                outMbY.select<8,2,16,1>(0,0) = inMbY.select<8,2,16,1>(0,0);
                outMbUV.select<4,2,16,1>(0,0) = inMbUV.select<4,2,16,1>(0,0);
                break;
            case 0x1:  // TFF2BFF
                outMbY.select<8,2,16,1>(1,0) = inMbY.select<8,2,16,1>(0,0);
                outMbUV.select<4,2,16,1>(1,0) = inMbUV.select<4,2,16,1>(0,0);
                break;
            case 0x2:  // BFF2TFF
                outMbY.select<8,2,16,1>(0,0) = inMbY.select<8,2,16,1>(1,0);
                outMbUV.select<4,2,16,1>(0,0) = inMbUV.select<4,2,16,1>(1,0);
                break;
            case 0x3:  // BTFF2BFF
                outMbY.select<8,2,16,1>(1,0) = inMbY.select<8,2,16,1>(1,0);
                outMbUV.select<4,2,16,1>(1,0) = inMbUV.select<4,2,16,1>(1,0);
                break;
            default:
                return;
        }

        write_plane(OutSurfIndex, GENX_SURFACE_Y_PLANE, ix, iy, outMbY);
        write_plane(OutSurfIndex, GENX_SURFACE_UV_PLANE, ix, iy / 2, outMbUV);
    }
}
