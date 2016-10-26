//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 1985-2014 Intel Corporation. All Rights Reserved.
//

#pragma once

template<int size, typename ty>
struct bytes_of
{
  enum
  {
    value = size * sizeof(ty),
  };
};

namespace cmut
{
  template<typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ matrix<T, HEIGHT / 2, WIDTH> top_field(matrix<T, HEIGHT, WIDTH> & m)
  {
      return m.template select<HEIGHT / 2, 2, WIDTH, 1>(0, 0);
  }

  template<typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ matrix<T, HEIGHT / 2, WIDTH> bot_field(matrix<T, HEIGHT, WIDTH> & m)
  {
      return m.template select<HEIGHT / 2, 2, WIDTH, 1>(1, 0);
  }

  template<typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ matrix_ref<T, HEIGHT / 2, WIDTH> top_field_ref(matrix<T, HEIGHT, WIDTH> & m)
  {
      return m.template select<HEIGHT / 2, 2, WIDTH, 1>(0, 0);
  }

  template<typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ matrix_ref<T, HEIGHT / 2, WIDTH> bot_field_ref(matrix<T, HEIGHT, WIDTH> & m)
  {
      return m.template select<HEIGHT / 2, 2, WIDTH, 1>(1, 0);
  }

#if 0
  template<uint R, uint C>
  inline _GENX_ matrix<uchar, R, C> cm_median(const matrix<uchar, R, C> & v0, const matrix<uchar, R, C> & v1, const matrix<uchar, R, C> & v2)
  {
      matrix<ushort, R, C> v1v2min = cm_min<ushort>(v1, v2);
      matrix<ushort, R, C> v1v2max = cm_max<ushort>(v1, v2);

      return cm_min<ushort>(cm_max<ushort>(v0, v1v2min), v1v2max);
#else
  template<uint R, uint C>
  inline _GENX_ matrix<uchar, R, C> cm_median(const matrix<uchar, R, C> & v0, const matrix<uchar, R, C> & v1, const matrix<uchar, R, C> & v2)
  {
      matrix<ushort, R, C> v1v2min = cm_min<ushort>(v1, v2);
      matrix<ushort, R, C> v1v2max = cm_max<ushort>(v1, v2);
      matrix<ushort, R, C> temp = cm_max<ushort>(v0, v1v2min);
      matrix<uchar, R, C> out; // optimization to avoid uw -> ub conversion at end of function
      out.template select<R, 1, C, 2>(0, 0) = cm_min<uchar>(temp.template select<R, 1, C, 2>(0, 0), v1v2max.template select<R, 1, C, 2>(0, 0));
      out.template select<R, 1, C, 2>(0, 1) = cm_min<uchar>(temp.template select<R, 1, C, 2>(0, 1), v1v2max.template select<R, 1, C, 2>(0, 1));
      return out;
#endif
  }

  template<uint START_ROW, uint ROWS, uint COLS, CmSurfacePlaneIndex PLANE>
  inline _GENX_ void cm_copy_plane(SurfaceIndex dstSurfaceId, SurfaceIndex srcSurfaceId, uint startCol)
  {
      matrix<unsigned char, ROWS, COLS> m;
      cm_read_plane<PLANE>(srcSurfaceId, startCol, START_ROW, m);
      cm_write_plane<PLANE>(dstSurfaceId, startCol, START_ROW, m);
  }

  //Block Width (bytes)   Maximum Block Height (rows)
  //  1 - 4                       64
  //  5 - 8                       32
  //  9 - 16                      16
  //  17 - 32                     8
  //  33 - 64 {BDW + }            4

  //CM compiler doesn't treat below as const or correctly, thus use macro as workaround
  //constexpr uint max_media_block_rw_height(uint width)
  //{
  //  return width >= 1 && width <= 4 ? 64 :
  //    (width >= 5 && width <= 8 ? 32 :
  //    (width >= 9 && width <= 16 ? 16 :
  //    (width >= 17 && width <= 32 ? 8 :
  //    (width >= 33 && width <= 64 ? 4 : 0))));
  //}

#define max_media_block_rw_height(width)  \
  (width >= 1 && width <= 4 ? 64 :  \
  (width >= 5 && width <= 8 ? 32 :  \
  (width >= 9 && width <= 16 ? 16 : \
  (width >= 17 && width <= 32 ? 8 : \
  (width >= 33 && width <= 64 ? 4 : 0)))))

  //FIXME remove magic number 32 and consider platform constraint on each dword IO
  //width is united in bytes
  //constexpr uint max_media_block_rw_width
  //{
  //  return 32;
  //}

  //template<typename T>
  //constexpr const T min(const T a, const T b)
  //{
  //  return a < b ? (a) : (b);
  //}
#define max_media_block_rw_width 32
#define min(a, b) (((a) < (b)) ? (a) : (b))

  template<typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ void cm_read(SurfaceIndex surfaceId, uint x, uint y, matrix<T, HEIGHT, WIDTH> & m)
  {
    cm_read(surfaceId, x, y, m.select_all());
  }

  template<typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ void cm_read(SurfaceIndex surfaceId, uint x, uint y, matrix_ref<T, HEIGHT, WIDTH> m)
  {
    //Fill as wide width as possible, then to decide height, but will xtile or ytile layout impact?
    enum
    {
      EACH_IO_WIDTH = min(WIDTH * sizeof(T), max_media_block_rw_width),
      EACH_IO_HEIGHT = min(HEIGHT, max_media_block_rw_height(EACH_IO_WIDTH)),
    };

    static_assert(((WIDTH * sizeof(T)) % EACH_IO_WIDTH) == 0, "WIDTH is not compitable to EACH_IO_WIDTH");
    static_assert((HEIGHT % EACH_IO_HEIGHT) == 0, "HEIGHT is not compitable to EACH_IO_HEIGHT");

    uint h = 0;
    uint w = 0;
#pragma unroll
    for (h = 0; h < HEIGHT; h += EACH_IO_HEIGHT) {
#pragma unroll
      for (w = 0; w < WIDTH * sizeof(T); w += EACH_IO_WIDTH) {
        read(surfaceId, x + w, y + h, m.template select<EACH_IO_HEIGHT, 1, EACH_IO_WIDTH / sizeof(T), 1>(h, w / sizeof(T)));
      }
    }
  }

  //FIXME support arbitrary size
  template<typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ void cm_write(SurfaceIndex surfaceId, uint x, uint y, matrix_ref<T, HEIGHT, WIDTH> m)
  {
    //Fill as wide width as possible, then to decide height, but will xtile or ytile layout impact?
    enum
    {
      EACH_IO_WIDTH = min(WIDTH * sizeof(T), max_media_block_rw_width),
      EACH_IO_HEIGHT = min(HEIGHT, max_media_block_rw_height(EACH_IO_WIDTH)),
    };

    static_assert(((WIDTH * sizeof(T)) % EACH_IO_WIDTH) == 0, "WIDTH is not compitable to EACH_IO_WIDTH");
    static_assert((HEIGHT % EACH_IO_HEIGHT) == 0, "HEIGHT is not compitable to EACH_IO_HEIGHT");

    uint h = 0;
    uint w = 0;
#pragma unroll
    for (h = 0; h < HEIGHT; h += EACH_IO_HEIGHT) {
#pragma unroll
      for (w = 0; w < WIDTH * sizeof(T); w += EACH_IO_WIDTH) {
        write(surfaceId, x + w, y + h, m.template select<EACH_IO_HEIGHT, 1, EACH_IO_WIDTH / sizeof(T), 1>(h, w / sizeof(T)));
      }
    }
  }

  template<typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ void cm_write(SurfaceIndex surfaceId, uint x, uint y, matrix<T, HEIGHT, WIDTH> & m)
  {
    cm_write(surfaceId, x, y, m.select_all());
  }

  template<CmSurfacePlaneIndex PLANE, typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ void cm_read_plane(SurfaceIndex surfaceId, uint x, uint y, matrix_ref<T, HEIGHT, WIDTH> m)
  {
    //Fill as wide width as possible, then to decide height, but will xtile or ytile layout impact?
    enum
    {
      EACH_IO_WIDTH = min(WIDTH * sizeof(T), max_media_block_rw_width),
      EACH_IO_HEIGHT = min(HEIGHT, max_media_block_rw_height(EACH_IO_WIDTH)),
    };

    static_assert(((WIDTH * sizeof(T)) % EACH_IO_WIDTH) == 0, "WIDTH is not compitable to EACH_IO_WIDTH");
    static_assert((HEIGHT % EACH_IO_HEIGHT) == 0, "HEIGHT is not compitable to EACH_IO_HEIGHT");

    uint h = 0;
    uint w = 0;
#pragma unroll
    for (h = 0; h < HEIGHT; h += EACH_IO_HEIGHT) {
#pragma unroll
      for (w = 0; w < WIDTH * sizeof(T); w += EACH_IO_WIDTH) {
        read_plane(surfaceId, PLANE, x + w, y + h, m.template select<EACH_IO_HEIGHT, 1, EACH_IO_WIDTH / sizeof(T), 1>(h, w / sizeof(T)));
      }
    }
  }

  template<CmSurfacePlaneIndex PLANE, uint IS_TOP_FIELD, typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ void cm_read_plane_field(SurfaceIndex surfaceId, uint x, uint y, matrix_ref<T, HEIGHT, WIDTH> m)
  {
      //Fill as wide width as possible, then to decide height, but will xtile or ytile layout impact?
      enum
      {
          EACH_IO_WIDTH = min(WIDTH * sizeof(T), max_media_block_rw_width),
          EACH_IO_HEIGHT = min(HEIGHT, max_media_block_rw_height(EACH_IO_WIDTH)),
      };

      static_assert(((WIDTH * sizeof(T)) % EACH_IO_WIDTH) == 0, "WIDTH is not compitable to EACH_IO_WIDTH");
      static_assert((HEIGHT % EACH_IO_HEIGHT) == 0, "HEIGHT is not compitable to EACH_IO_HEIGHT");

      uint h = 0;
      uint w = 0;
#pragma unroll
      for (h = 0; h < HEIGHT; h += EACH_IO_HEIGHT) {
#pragma unroll
          for (w = 0; w < WIDTH * sizeof(T); w += EACH_IO_WIDTH) {
#ifndef CMRT_EMU
              if (IS_TOP_FIELD)
                read_plane(TOP_FIELD(surfaceId), PLANE, x + w, y + h, m.template select<EACH_IO_HEIGHT, 1, EACH_IO_WIDTH / sizeof(T), 1>(h, w / sizeof(T)));
              else
                read_plane(BOTTOM_FIELD(surfaceId), PLANE, x + w, y + h, m.template select<EACH_IO_HEIGHT, 1, EACH_IO_WIDTH / sizeof(T), 1>(h, w / sizeof(T)));
#else
              if (IS_TOP_FIELD)
                  read_plane(TOP_FIELD(surfaceId), PLANE, x + w, (y + h)<<1, m.template select<EACH_IO_HEIGHT, 1, EACH_IO_WIDTH / sizeof(T), 1>(h, w / sizeof(T)));
              else
                  read_plane(BOTTOM_FIELD(surfaceId), PLANE, x + w, (y + h)<<1, m.template select<EACH_IO_HEIGHT, 1, EACH_IO_WIDTH / sizeof(T), 1>(h, w / sizeof(T)));
#endif
          }
      }
  }

  template<CmSurfacePlaneIndex PLANE, typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ void cm_read_plane(SurfaceIndex surfaceId, uint x, uint y, matrix<T, HEIGHT, WIDTH> & m)
  {
    cm_read_plane<PLANE>(surfaceId, x, y, m.select_all());
  }

  template<CmSurfacePlaneIndex PLANE, uint IS_TOP_FIELD, typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ void cm_read_plane_field(SurfaceIndex surfaceId, uint x, uint y, matrix<T, HEIGHT, WIDTH> & m)
  {
      cm_read_plane_field<PLANE, IS_TOP_FIELD>(surfaceId, x, y, m.select_all());
  }

  template<CmSurfacePlaneIndex PLANE, typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ void cm_write_plane(SurfaceIndex surfaceId, uint x, uint y, matrix_ref<T, HEIGHT, WIDTH> m)
  {
    //Fill as wide width as possible, then to decide height, but will xtile or ytile layout impact?
    enum
    {
      EACH_IO_WIDTH = min(WIDTH * sizeof(T), max_media_block_rw_width),
      EACH_IO_HEIGHT = min(HEIGHT, max_media_block_rw_height(EACH_IO_WIDTH)),
    };

    static_assert(((WIDTH * sizeof(T)) % EACH_IO_WIDTH) == 0, "WIDTH is not compitable to EACH_IO_WIDTH");
    static_assert((HEIGHT % EACH_IO_HEIGHT) == 0, "HEIGHT is not compitable to EACH_IO_HEIGHT");

    uint h = 0;
    uint w = 0;
#pragma unroll
    for (h = 0; h < HEIGHT; h += EACH_IO_HEIGHT) {
#pragma unroll
      for (w = 0; w < WIDTH * sizeof(T); w += EACH_IO_WIDTH) {
        write_plane(surfaceId, PLANE, x + w, y + h, m.template select<EACH_IO_HEIGHT, 1, EACH_IO_WIDTH / sizeof(T), 1>(h, w / sizeof(T)));
      }
    }
  }

  template<CmSurfacePlaneIndex PLANE, typename T, uint HEIGHT, uint WIDTH>
  inline _GENX_ void cm_write_plane(SurfaceIndex surfaceId, uint x, uint y, matrix<T, HEIGHT, WIDTH> & m)
  {
    cm_write_plane<PLANE>(surfaceId, x, y, m.select_all());
  }

  template<typename ty, uint size>
  inline _GENX_ void cm_write(SurfaceIndex index, uint offset, vector_ref<ty, size> v)
  {
  #pragma unroll
    for (uint i = 0; i < size; i += 32) {
      write(index, offset + i * sizeof(ty), v.template select<32, 1>(i));
    }
  }

  template<typename dataTy, uint size>
  inline _GENX_ void cm_read(SurfaceIndex id, uint globalOffset, 
                              vector_ref<uint, 8> elementOffset, vector_ref<dataTy, 8> v) 
  {
    read (id, globalOffset, elementOffset, v);
  }

  template<typename dataTy, uint size>
  inline _GENX_ void cm_read (SurfaceIndex id, uint globalOffset, 
                               vector_ref<uint, 16> elementOffset, vector_ref<dataTy, 16> v) 
  {
    read (id, globalOffset, elementOffset, v);
  }

  template<typename dataTy, uint size>
  inline _GENX_ void cm_read(SurfaceIndex id, uint globalOffset, 
                              vector_ref<uint, size> elementOffset, vector_ref<dataTy, size> v) 
  {
    read (id, globalOffset, elementOffset.template select<16, 1> (0), v.template select<16, 1> (0));
    cm_read<dataTy, size - 16> (id, globalOffset, elementOffset.template select<size - 16, 1> (16), v.template select<size - 16, 1> (16));
  }

  template<typename dataTy, uint rows, uint cols>
  inline _GENX_ void cm_read(SurfaceIndex id, uint globalOffset, 
                              matrix_ref<uint, 1, cols> elementOffset, matrix_ref<dataTy, 1, cols> m)
  {
    cm_read<dataTy, cols> (id, globalOffset, elementOffset.row (0), m.row (0));
  }

  template<typename dataTy, uint rows, uint cols>
  inline _GENX_ void cm_read (SurfaceIndex id, uint globalOffset, 
                               matrix_ref<uint, rows, cols> elementOffset, matrix_ref<dataTy, rows, cols> m)
  {
    cm_read<dataTy, cols> (id, globalOffset, elementOffset.row (0), m.row (0));
    cm_read<dataTy, rows - 1, cols>(id, globalOffset, elementOffset.template select<rows - 1, 1, cols, 1> (1, 0), m.template select<rows - 1, 1, cols, 1> (1, 0));
  }

  template<typename dataTy, uint rows, uint cols>
  inline _GENX_ void cmk_row_increment(matrix_ref<dataTy, 2, cols> m, dataTy increment) 
  {
    m.row (1) = m.row (0) + increment;
  }

  template<typename dataTy, uint rows, uint cols>
  inline _GENX_ void cmk_row_increment(matrix_ref<dataTy, rows, cols> m, dataTy increment) 
  {
    m.row (1) = m.row (0) + increment;
    cmk_row_increment<dataTy, rows - 1, cols>(m.template select<rows - 1, 1, cols, 1> (1, 0), increment);
  }

  template<bool>
  struct cmk_stop
  {
  };

  template<int>
  struct cmk_int2type
  {
  };
  // cmk_increase_by_cols, FIXME check whether size is power of 2
  template<typename dataTy, uint size, uint selects>
  inline _GENX_ void cmk_increase_by_cols (vector_ref<dataTy, size> v, dataTy increment, cmk_stop<false>)
  {
    v.template select<selects, 1> (selects) = v.template select<selects, 1> (0) + increment;
    enum { stop = selects * 2 * 2 > size };
    cmk_increase_by_cols<dataTy, size, selects * 2>(v, increment * 2, cmk_stop<stop> ());
  }

  template<typename dataTy, uint size, uint selects>
  inline _GENX_ void cmk_increase_by_cols(vector_ref<dataTy, size> v, dataTy increment, cmk_stop<true>)
  {
  }

  template<typename dataTy, uint size, uint selects>
  inline _GENX_ void cmk_increase_by_cols(vector_ref<dataTy, size> v, dataTy increment)
  {
    enum { stop = selects * 2 > size };
    cmk_increase_by_cols<dataTy, size, selects>(v, increment, cmk_stop<stop> ());
  }

  // cmk_increase_by_cols, FIXME check whether rows is power of 2
  template<typename dataTy, uint rows, uint cols, uint selects>
  inline _GENX_ void cmk_increase_by_rows(matrix_ref<dataTy, rows, cols> m, dataTy increment, cmk_stop<false>)
  {
    m.template select<selects, 1, cols, 1>(selects, 0) = m.template select<selects, 1, cols, 1>(0, 0) + increment;
    enum { stop = selects * 2 * 2 > rows };
    cmk_increase_by_rows<dataTy, rows, cols, selects * 2>(m, increment * 2, cmk_stop<stop> ());
  }

  template<typename dataTy, uint rows, uint cols, uint selects>
  inline _GENX_ void cmk_increase_by_rows(matrix_ref<dataTy, rows, cols> m, dataTy increment, cmk_stop<true>)
  {
  }

  template<typename dataTy, uint rows, uint cols, uint selects>
  inline _GENX_ void cmk_increase_by_rows (matrix_ref<dataTy, rows, cols> m, dataTy increment)
  {
    enum { stop = selects * 2 > rows };
    cmk_increase_by_rows<dataTy, rows, cols, selects>(m, increment, cmk_stop<stop> ());
  }


  //template<typename ty, uint rows, uint cols>
  //inline _GENX_ void cm_read (SurfaceIndex index, uint x, uint y, matrix_ref<ty, rows, cols> m) 
  //{
  //  enum { colInterval = 32 / sizeof(ty) };
  //
  //#pragma unroll
  //  for (uint col = 0; col < cols; col += colInterval) {
  //    read (index, x + col * sizeof(ty), y, m.template select<rows, 1, colInterval, 1> (0, col));
  //  }
  //}

  template<typename ty, uint size>
  inline _GENX_ void cm_read(SurfaceIndex index, uint offset, vector_ref<ty, size> v) 
  {
  #pragma unroll
    for (uint i = 0; i < size; i += 32) {
      read(index, offset + i * sizeof(ty), v.template select<32, 1> (i));
    }
  }

  template<typename ty, uint size>
  inline _GENX_ void cm_read_4(SurfaceIndex index, uint offset, vector_ref<ty, size> v) 
  {
  #pragma unroll
    for (uint i = 0; i < size; i += 4) {
      read(index, offset + i * sizeof(ty), v.template select<4, 1> (i));
    }
  }

  template<typename valueTy, uint size>
  inline _GENX_ void cmk_transpose_by_row_to_column(matrix_ref<valueTy, 8, size> tm, matrix_ref<valueTy, size, 8> m) 
  {
  #pragma unroll
    for (uint i = 0; i < size; ++i) {
      tm.column(i) = m.row(i);
    }
  }

  #define CM_CONST_E    2.718282f
  #define CM_CONST_PI   3.141593f
  #define CM_CONST_2PI  6.283185f

  //#define DBL_EPSILON     0.00001f /* smallest such that 1.0+DBL_EPSILON != 1.0 */

  template<uint rows, uint cols>
  inline _GENX_ matrix<float, rows, cols> cmk_atan2(matrix_ref<short, rows, cols> y, matrix_ref<short, rows, cols> x) 
  {
    matrix<float, rows, cols> a0;
    matrix<float, rows, cols> a1;

    matrix<unsigned short, rows, cols> mask = y >= 0;
    a0.merge(CM_CONST_PI * 0.5, CM_CONST_PI * 1.5, mask);
    a1.merge(0, CM_CONST_PI * 2, mask);

    a1.merge(CM_CONST_PI, x < 0);

    matrix<float, rows, cols> xy = x * y;
    matrix<float, rows, cols> x2 = x * x;
    matrix<float, rows, cols> y2 = y * y;

    a0 -= (xy / (y2 + 0.28f * x2 + DBL_EPSILON));
    a1 += (xy / (x2 + 0.28f * y2 + DBL_EPSILON));

    matrix<float, rows, cols> atan2;
    atan2.merge(a1, a0, y2 <= x2);

    return atan2;
  }

  //template<uint rows, uint cols>
  //_GENX_ void cmk_atan2 (matrix_ref<float, rows, cols> atan2, 
  //                              matrix_ref<short, rows, cols> y, 
  //                              matrix_ref<short, rows, cols> x) 
  //{
  //  matrix<float, rows, cols> x2 = x * x;
  //  matrix<float, rows, cols> y2 = y * y;
  //
  //  matrix<float, rows, cols> pi;
  //  pi.merge (PI * 0.5, PI * 1.5, y >= 0);
  //  matrix<float, rows, cols> a0 = pi - x * y / (y2 + 0.28f * x2 + DBL_EPSILON);
  //
  //  pi.merge (0, PI * 2, y >= 0);
  //  pi.merge (PI, x < 0);
  //  matrix<float, rows, cols> a1 = pi + x * y / (x2 + 0.28f * y2 + DBL_EPSILON);
  //
  //  atan2.merge (a1, a0, y2 <= x2);
  //}

  #define cm_floor cm_rndd
  #define cm_ceil cm_rndu

//template<uint rows, uint cols>
//inline _GENX_ void cmk_floor (matrix_ref<short, rows, cols> floor, matrix_ref<float, rows, cols> x) 
//{
//  floor = x;
//  floor -= (x < 0);
//}
};