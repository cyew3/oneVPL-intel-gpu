__kernel void rotate_Y(__read_only image2d_t YIn, __write_only image2d_t YOut)
{
    int2 coord_src = (int2)(get_global_id(0), get_global_id(1));
    int2 dim = get_image_dim(YOut);
    int2 coord_dst = dim  - (int2)(1, 1) - coord_src;
    const sampler_t smp = CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

    // rotate Y plane
    uint4 pixel = read_imageui(YIn, smp, coord_src);    
    write_imageui(YOut, coord_dst, pixel);
}

__kernel void rotate_UV(__read_only image2d_t UVIn, __write_only image2d_t UVOut)
{
    int2 coord_src = (int2)(get_global_id(0), get_global_id(1));
    int2 dim = get_image_dim(UVOut);
    int2 coord_dst = dim  - (int2)(1, 1) - coord_src;
    const sampler_t smp = CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

    // rotate UV plane
    uint4 pixel = read_imageui(UVIn, smp, coord_src);
    write_imageui(UVOut, coord_dst, pixel);
}

__kernel void rotate_Y_packed(__read_only image2d_t YIn, __write_only image2d_t YOut)
{
    int2 coord_src = (int2)(get_global_id(0), get_global_id(1));
    int2 dim = get_image_dim(YOut);
    int2 coord_dst = dim  - (int2)(1, 1) - coord_src;
    const sampler_t smp = CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE; 

    // rotate Y plane
    uint4 pixel = read_imageui(YIn, smp, coord_src);    
    write_imageui(YOut, coord_dst, pixel.wzyx); // rotate samples within pixel
}

__kernel void rotate_UV_packed(__read_only image2d_t UVIn, __write_only image2d_t UVOut)
{
    int2 coord_src = (int2)(get_global_id(0), get_global_id(1));
    int2 dim = get_image_dim(UVOut);
    int2 coord_dst = dim  - (int2)(1, 1) - coord_src;
    const sampler_t smp = CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE;

    // rotate UV plane
    uint4 pixel = read_imageui(UVIn, smp, coord_src);
    write_imageui(UVOut, coord_dst, pixel.zwxy); // rotate samples within pixel
}