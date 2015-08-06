/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: dirty_rect_genx.cpp

\* ****************************************************************************** */

#include <cm.h>

extern "C" _GENX_MAIN_
void DirtyRectNV12(SurfaceIndex surf_in, SurfaceIndex surf_out, unsigned int w_size, unsigned int h_size, SurfaceIndex roi_map, short frame_width, short frame_height)
{
    unsigned int x_pos  = 4 * (get_thread_origin_x()/* * w_size */);
    unsigned int y_pos  = 4 * (get_thread_origin_y()/* * h_size */);
    if(y_pos >= frame_height)
        return;
    if(x_pos >= frame_width)
        return;

    matrix<uchar, 4, 4> dirty;
    unsigned int x_block  = x_pos / w_size;
    unsigned int y_block  = y_pos / h_size;

    read(roi_map, x_block * sizeof(int), y_block * 4, dirty);
    {
        //already dirty?
        if(dirty[0][0])
            return;
    }

    //Y, NV12
    {
        matrix<uchar, 4, 4> in_block_y, out_block_y;
        read_plane(surf_in,  GENX_SURFACE_Y_PLANE, x_pos, y_pos,  in_block_y);
        read_plane(surf_out, GENX_SURFACE_Y_PLANE, x_pos, y_pos, out_block_y);

        out_block_y = out_block_y ^ in_block_y;

        short sum = cm_sum<uchar>(out_block_y);
        if(sum)
        {
            dirty = 1;
            write(roi_map, x_block * sizeof(int), y_block * 4, dirty);
            //printf("cm_sum = %d\n", sum);
            //printf("block = %d x %d\n", x_block, y_block);
            //printf("writing to %d x %d\n", x_block * sizeof(int), y_block * 4);
            return;
        }
    }

    //UV, NV12
    {
        matrix<uchar, 2, 4> in_block_uv, out_block_uv;

        y_pos = y_pos / 2;
        read_plane(surf_in,  GENX_SURFACE_UV_PLANE, x_pos, y_pos,  in_block_uv);
        read_plane(surf_out, GENX_SURFACE_UV_PLANE, x_pos, y_pos, out_block_uv);

        out_block_uv = out_block_uv ^ in_block_uv;

        short sum = cm_sum<uchar>(out_block_uv);
        if(sum)
        {
            dirty = 1;
            write(roi_map, x_block * sizeof(int), y_block * 4, dirty);
            //printf("cm_sum = %d\n", sum);
            //printf("block = %d x %d\n", x_block, y_block);
            //printf("writing to %d x %d\n", x_block * sizeof(int), y_block * 4);
            return;
        }
    }
}

extern "C" _GENX_MAIN_
void DirtyRectRGB4(SurfaceIndex surf_in, SurfaceIndex surf_out, unsigned int w_size, unsigned int h_size, SurfaceIndex roi_map, short frame_width, short frame_height)
{
    unsigned int x_pos  = 4 * (get_thread_origin_x()/* * w_size */);
    unsigned int y_pos  = 4 * (get_thread_origin_y()/* * h_size */);
    if(y_pos >= frame_height)
        return;
    if(x_pos >= frame_width)
        return;

    matrix<uchar, 4, 4> dirty;
    unsigned int x_block  = x_pos / w_size;
    unsigned int y_block  = y_pos / h_size;

    read(roi_map, x_block * sizeof(int), y_block * 4, dirty);
    {
        //already dirty?
        if(dirty[0][0])
            return;
    }

    //ARGB, RGB4
    {
        matrix<uchar, 4, 4*4> in_block, out_block;
        //read_plane(surf_in,  GENX_SURFACE_Y_PLANE, x_pos, y_pos,  in_block);
        //read_plane(surf_out, GENX_SURFACE_Y_PLANE, x_pos, y_pos, out_block);

        read(surf_in , x_pos * 4, y_pos,  in_block);
        read(surf_out, x_pos * 4, y_pos, out_block);


        out_block = out_block ^ in_block;

        short sum = cm_sum<uchar>(out_block);
        if(sum)
        {
            dirty = 1;
            write(roi_map, x_block * sizeof(int), y_block * 4, dirty);
            //printf("cm_sum = %d\n", sum);
            //printf("block = %d x %d\n", x_block, y_block);
            //printf("writing to %d x %d\n", x_block * sizeof(int), y_block * 4);
            return;
        }
    }
}