/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

File Name: dirty_rect_genx.cpp

\* ****************************************************************************** */

#include <cm.h>

template<uint PLANE_WIDTH, uint PLANE_HEIGHT>
void _GENX_MAIN_ cmkDirtyRectNV12(short frame_width, short frame_height, SurfaceIndex roi_map, SurfaceIndex surf_in, SurfaceIndex surf_out)
{
    uint x = get_thread_origin_x() * PLANE_WIDTH;
    uint y = get_thread_origin_y() * PLANE_HEIGHT;

    if(x >= frame_width)
        return;
    if(y >= frame_height)
        return;

    matrix<uchar, 4, 4> dirty = 0;
    //Y, NV12
    {
        matrix<uchar, PLANE_HEIGHT, PLANE_WIDTH> in_block_y, out_block_y;
        read_plane(surf_in,  GENX_SURFACE_Y_PLANE, x, y,  in_block_y);
        read_plane(surf_out, GENX_SURFACE_Y_PLANE, x, y, out_block_y);

        out_block_y = out_block_y ^ in_block_y;

        uint sum = cm_sum<uint>(out_block_y);
        if(sum)
        {
            dirty[0][0] = 1;
            write(roi_map, get_thread_origin_x() * 4, get_thread_origin_y() * 4, dirty);
            //printf("cm_sum = %d\n", sum);
            //printf("block = %d x %d\n", x_block, y_block);
            //printf("writing to %d x %d\n", x_block * sizeof(int), y_block * 4);
            return;
        }
    }

    //UV, NV12
    {
        matrix<uchar, PLANE_HEIGHT / 2, PLANE_WIDTH> in_block_uv, out_block_uv;

        y = y / 2;
        read_plane(surf_in,  GENX_SURFACE_UV_PLANE, x, y,  in_block_uv);
        read_plane(surf_out, GENX_SURFACE_UV_PLANE, x, y, out_block_uv);

        out_block_uv = out_block_uv ^ in_block_uv;

        uint sum = cm_sum<uint>(out_block_uv);
        if(sum)
        {
            dirty[0][0] = 1;
            write(roi_map, get_thread_origin_x() * 4, get_thread_origin_y() * 4, dirty);
            //printf("cm_sum = %d\n", sum);
            //printf("block = %d x %d\n", x_block, y_block);
            //printf("writing to %d x %d\n", x_block * sizeof(int), y_block * 4);
            return;
        }
    }
}

template<uint PLANE_WIDTH, uint PLANE_HEIGHT>
void _GENX_MAIN_ cmkDirtyRectRGB4(short frame_width, short frame_height, SurfaceIndex roi_map, SurfaceIndex surf_in, SurfaceIndex surf_out)
{
    const uint x = get_thread_origin_x() * PLANE_WIDTH;
    const uint y = get_thread_origin_y() * PLANE_HEIGHT;

    if(x >= frame_width)
        return;
    if(y >= frame_height)
        return;

    matrix<uchar, 4, 4> dirty = 0;

    //ARGB, RGB4
    {
        uint sum = 0;
        matrix<uchar, PLANE_HEIGHT, 4 * PLANE_WIDTH> in_block,
                                                     out_block;
        //Currently media block read only supports objects that fit into a single dataport transaction:
        //   width <= 32 bytes (64 bytes for linear/Xtile surfaces on BDW+), size <= 256 bytes.
        //Workaround with several reads
#if 0
        static_assert (32 == PLANE_WIDTH, "This code block is designed for 32 block width");
        matrix_ref<uchar, PLANE_HEIGHT, PLANE_WIDTH> in_block_ref1  =  in_block.template select<PLANE_HEIGHT, 1,PLANE_WIDTH, 1>(0,PLANE_WIDTH * 0),
                                                     in_block_ref2  =  in_block.template select<PLANE_HEIGHT, 1,PLANE_WIDTH, 1>(0,PLANE_WIDTH * 1),
                                                     in_block_ref3  =  in_block.template select<PLANE_HEIGHT, 1,PLANE_WIDTH, 1>(0,PLANE_WIDTH * 2),
                                                     in_block_ref4  =  in_block.template select<PLANE_HEIGHT, 1,PLANE_WIDTH, 1>(0,PLANE_WIDTH * 3),

                                                     out_block_ref1 = out_block.template select<PLANE_HEIGHT, 1,PLANE_WIDTH, 1>(0,PLANE_WIDTH * 0),
                                                     out_block_ref2 = out_block.template select<PLANE_HEIGHT, 1,PLANE_WIDTH, 1>(0,PLANE_WIDTH * 1),
                                                     out_block_ref3 = out_block.template select<PLANE_HEIGHT, 1,PLANE_WIDTH, 1>(0,PLANE_WIDTH * 2),
                                                     out_block_ref4 = out_block.template select<PLANE_HEIGHT, 1,PLANE_WIDTH, 1>(0,PLANE_WIDTH * 3);

        read(surf_in , x * 4 + 0*PLANE_WIDTH, y, in_block_ref1);
        read(surf_in , x * 4 + 1*PLANE_WIDTH, y, in_block_ref2);
        read(surf_in , x * 4 + 2*PLANE_WIDTH, y, in_block_ref3);
        read(surf_in , x * 4 + 3*PLANE_WIDTH, y, in_block_ref4);

        read(surf_out, x * 4 + 0*PLANE_WIDTH, y, out_block_ref1);
        read(surf_out, x * 4 + 1*PLANE_WIDTH, y, out_block_ref2);
        read(surf_out, x * 4 + 2*PLANE_WIDTH, y, out_block_ref3);
        read(surf_out, x * 4 + 3*PLANE_WIDTH, y, out_block_ref4);

#else

        static_assert((PLANE_WIDTH == 16), "PLANE_WIDTH is not compitable");
#pragma unroll
        for(uint i = 0; i < 4; ++i)
        {
            read(surf_in , x * 4 + i*PLANE_WIDTH, y, in_block.template select<PLANE_HEIGHT,  1, PLANE_WIDTH, 1>(0,PLANE_WIDTH * i) );
            read(surf_out, x * 4 + i*PLANE_WIDTH, y, out_block.template select<PLANE_HEIGHT, 1, PLANE_WIDTH, 1>(0,PLANE_WIDTH * i) );
        }
#endif

        //actual processing
        {
            out_block = out_block ^ in_block;
            sum = cm_sum<uint>(out_block);
        }

        if(sum)
        {
            dirty[0][0] = 1;
            write(roi_map, get_thread_origin_x() * 4, get_thread_origin_y() * 4, dirty);
            //printf("cm_sum = %d\n", sum);
            //printf("block = %d x %d\n", x_block, y_block);
            //printf("writing to %d x %d\n", x_block * sizeof(int), y_block * 4);
            return;
        }
    }
}

template void cmkDirtyRectNV12 <16, 16>(short frame_width, short frame_height, SurfaceIndex roi_map, SurfaceIndex surf_in, SurfaceIndex surf_out);
template void cmkDirtyRectRGB4 <16, 16>(short frame_width, short frame_height, SurfaceIndex roi_map, SurfaceIndex surf_in, SurfaceIndex surf_out);