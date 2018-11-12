// Copyright (c) 2012-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cm/cm.h>
#include <cm/cmtl.h>

extern "C" _GENX_MAIN_  void cubes_copy_model(
SurfaceIndex output,
SurfaceIndex input,
SamplerIndex sampler,
uint input_width,
uint input_height,
uint output_width,
uint output_height)
{
	uint i, n, j;
	vector<float, 16> u;
	vector<float, 16> v;
	vector<float, 16> x;
	vector<float, 16> y;

	matrix<float, 4, 16> projected_row;
	matrix<uchar, 16, 16> projected_y_data;
	matrix<uchar, 8, 16> projected_uv_data;

	uint x_id = get_thread_origin_x() * 16;
	uint y_id = get_thread_origin_y() * 16;

	cm_vector(offset, float, 16, 0, 1);

	if (y_id >= input_height)
	{
		if (x_id < 1024)
		{
			x = ((output_width + x_id) + offset);
			y = ((y_id - input_height) + offset);
		}
		else if (x_id >= 1024 && x_id < 2048)
		{
			x = ((output_width + x_id - 1024) + offset);
			y = ((1024 + y_id - input_height) + offset);
		}
		else if (x_id >= 2048 && x_id < 3072)
		{
			x = ((output_width + x_id - 2048) + offset);
			y = ((2048 + y_id - input_height) + offset);
		}
		else
		{
			return;
		}
	}
	else
	{
		x = (x_id + offset);
		y = (y_id + offset);
	}

#pragma unroll(8)
	for (i = 0; i < 8; ++i)
	{
#pragma unroll(2)
		for (j = 0; j < 2; ++j)
		{
			n = (i << 1) + j; // i*2 + j
			u = x;
			v = y(n);
			u = u / input_width;
			v = v / input_height;

			sample16(projected_row, CM_BGR_ENABLE, input, sampler, u, v);
			projected_row = projected_row * 255;
			projected_y_data.select<1, 1, 16, 1>(n, 0) = projected_row.row(1);
			if (j % 2 == 0)
			{
				projected_uv_data.select<1, 1, 8, 2>(i, 0) = projected_row.select<1, 1, 8, 2>(2, 0);
				projected_uv_data.select<1, 1, 8, 2>(i, 1) = projected_row.select<1, 1, 8, 2>(0, 0);
			}
		}
	}
	write_plane(output, GENX_SURFACE_Y_PLANE, x_id, y_id, projected_y_data);
	write_plane(output, GENX_SURFACE_UV_PLANE, x_id, y_id / 2, projected_uv_data);
}