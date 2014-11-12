/**********************************************************************************

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.

***********************************************************************************/

__kernel void rotate_Y(__read_only image2d_t YIn, __write_only image2d_t YOut)
{
    int2 coord_src = (int2)(get_global_id(0), get_global_id(1));
    int2 dim = (int2)(get_global_size(0), get_global_size(1));
    int2 coord_dst = dim  - (int2)(1, 1) - coord_src;
    const sampler_t smp = CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

    // rotate Y plane
    float4 pixel = read_imagef(YIn, smp, coord_src);
    write_imagef(YOut, coord_dst, pixel);
}

__kernel void rotate_UV(__read_only image2d_t UVIn, __write_only image2d_t UVOut)
{
    int2 coord_src = (int2)(get_global_id(0), get_global_id(1));
    int2 dim = (int2)(get_global_size(0), get_global_size(1));
    int2 coord_dst = dim  - (int2)(1, 1) - coord_src;
    const sampler_t smp = CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

    // rotate UV plane
    float4 pixel = read_imagef(UVIn, smp, coord_src);
    write_imagef(UVOut, coord_dst, pixel);
}

__kernel void rotate_Y_packed(__read_only image2d_t YIn, __write_only image2d_t YOut)
 {
    int2 coord_src = (int2)(get_global_id(0), get_global_id(1));
    int2 dim = (int2)(get_global_size(0), get_global_size(1));
    int2 coord_dst = dim  - (int2)(1, 1) - coord_src;
    const sampler_t smp = CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

    // rotate Y plane
    uint4 pixel = read_imageui(YIn, smp, coord_src);
    write_imageui(YOut, coord_dst, pixel.wzyx); // rotate samples within pixel
}

__kernel void rotate_UV_packed(__read_only image2d_t UVIn, __write_only image2d_t UVOut)
{
    int2 coord_src = (int2)(get_global_id(0), get_global_id(1));
    int2 dim = (int2)(get_global_size(0), get_global_size(1));
    int2 coord_dst = dim  - (int2)(1, 1) - coord_src;
    const sampler_t smp = CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

    // rotate UV plane
    uint4 pixel = read_imageui(UVIn, smp, coord_src);
    write_imageui(UVOut, coord_dst, pixel.zwxy); // rotate samples within pixel
}
