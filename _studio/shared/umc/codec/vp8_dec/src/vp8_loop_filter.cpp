// Copyright (c) 2014-2018 Intel Corporation
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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "vp8_dec_defs.h"
#include "umc_vp8_decoder.h"

using namespace UMC;

namespace UMC
{

#define ABS(x) ((x < 0) ? -(x) : (x))


//????
static const int8_t vp8_mode_lf_deltas_lut[VP8_NUM_MV_MODES + VP8_NUM_MB_MODES_Y] = 
{
  -1, -1, -1, -1, 0, 2, 2, 1, 2, 3
};


static const uint8_t vp8_hev_thresh_lut[2][64] = 
{
  {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
  },
  {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
  }
};



static int hev(uint8_t threshold, uint8_t* ptr, int32_t step) // if(HorEdge) step = 1; 
{                                                        // else step = dataStep
  int32_t p0, p1;
  int32_t q0, q1;

  p0 = ptr[-1*step]; p1 = ptr[-2*step];
  q0 = ptr[0*step];  q1 = ptr[1*step];

  return ABS(p1 - p0) > threshold || ABS(q1 - q0) > threshold;
} // hev()


static int do_filter_simple(uint8_t E, uint8_t* ptr, int32_t step)
{
  int32_t p0, p1;
  int32_t q0, q1;

  p0 = ptr[-1*step]; p1 = ptr[-2*step];
  q0 = ptr[0*step];  q1 = ptr[1*step];

  return (ABS(p0 - q0)*2 + (ABS(p1 - q1) >> 1)) <= E;
} // do_filter_simple()


static int do_filter_normal(uint8_t I, uint8_t E, uint8_t* ptr, int32_t step)
{
  int32_t p0, p1, p2, p3;
  int32_t q0, q1, q2 ,q3;

  p0 = ptr[-1*step]; p1 = ptr[-2*step]; p2 = ptr[-3*step]; p3 = ptr[-4*step];
  q0 = ptr[0*step];  q1 = ptr[1*step];  q2 = ptr[2*step];  q3 = ptr[3*step];

  return (ABS(p0 - q0)*2 + (ABS(p1 - q1) >> 1)) <= E &&
          ABS(p3 - p2) <= I &&
          ABS(p2 - p1) <= I &&
          ABS(p1 - p0) <= I &&
          ABS(q3 - q2) <= I &&
          ABS(q2 - q1) <= I &&
          ABS(q1 - q0) <= I;

  //return (ABS(p0 - q0)*2 + (ABS(p1 - q1) >> 1)) <= E +
  //        ABS(p3 - p2) <= I +
  //        ABS(p2 - p1) <= I +
  //        ABS(p1 - p0) <= I +
  //        ABS(q3 - q2) <= I +
  //        ABS(q2 - q1) <= I +
  //        ABS(q1 - q0) <= I;

} // do_filter_normal()


//may be rename to simple_filter()????
static void common_adjust(uint8_t use_outer_taps, uint8_t* ptr, int32_t step)
{
  int32_t a = 0;
  int32_t t = 0;

  int32_t F0 = 0;
  int32_t F1 = 0;

  int32_t p0, p1;
  int32_t q0, q1;

  p0 = ptr[-1*step]; p1 = ptr[-2*step];
  q0 = ptr[0*step];  q1 = ptr[1*step];

  a = 3*(q0 - p0);

  if(use_outer_taps)
    t = vp8_clamp8s(p1 - q1);

  a = vp8_clamp8s(a + t);

  F0 = a + 4;
  F1 = a + 3;

  vp8_CLIP( F0, -128, 127);
  vp8_CLIP( F1, -128, 127);

  F0 >>= 3;
  F1 >>= 3;

  /*q0 */ptr[ 0]= vp8_clamp8u(q0 - F0);
  /*p0*/ ptr[-1*step]= vp8_clamp8u(p0 + F1);

  if(!use_outer_taps)
  {
    a = (F0 + 1) >> 1;
  /*q1 */ptr[ 1*step]= vp8_clamp8u(q1 - a);
  /*p1*/ ptr[-2*step]= vp8_clamp8u(p1 + a);
  }

  return;
} // common_adjust()



static void vp8_lf_mb_edges(uint8_t* ptr, int32_t step)
{
  int32_t w = 0;

  int32_t F0 = 0;
  int32_t F1 = 0;
  int32_t F2 = 0;

  int32_t p0, p1, p2, p3;
  int32_t q0, q1, q2 ,q3;

  p0 = ptr[-1*step]; p1 = ptr[-2*step]; p2 = ptr[-3*step]; p3 = ptr[-4*step];
  q0 = ptr[0*step];  q1 = ptr[1*step];  q2 = ptr[2*step];  q3 = ptr[3*step];

  w = vp8_clamp8s(vp8_clamp8s(p1 - q1) + 3*(q0 - p0));

  F0 = vp8_clamp8s((27*w + 63) >> 7);

  F1 = vp8_clamp8s((18*w + 63) >> 7);
  F2 = vp8_clamp8s((9*w + 63) >> 7);


  /*q0*/ptr[0*step]  = vp8_clamp8u(q0 - F0);
  /*p0*/ptr[-1*step] = vp8_clamp8u(p0 + F0);

  /*q1*/ptr[1*step]  = vp8_clamp8u(q1 - F1);
  /*p1*/ptr[-2*step] = vp8_clamp8u(p1 + F1);

  /*q2*/ptr[2*step]  = vp8_clamp8u(q2 - F2);
  /*p2*/ptr[-3*step] = vp8_clamp8u(p2 + F2);


  return;
}


//
// Normal filters
//
static void vp8_lf_normal_mb_ve(uint8_t* dst, int32_t step,
                                uint8_t hev_threshold,
                                uint8_t interior_limit,
                                uint8_t edge_limit,
                                uint8_t loop_size) // 16 for Y, 8 for U/V
{
  int32_t i = 0;

  for(i = 0; i < loop_size; i++)
  {
    if(do_filter_normal(interior_limit, edge_limit, dst + i*step, 1))
    {
      if(hev(hev_threshold,  dst + i*step, 1))
      {
        common_adjust(1, dst + i*step, 1);
      }
      else
      {
        vp8_lf_mb_edges(dst + i*step, 1);
      }
    }
  }

  return;
} // vp8_lf_normal_mb_ve()


static void vp8_lf_normal_mb_he(uint8_t* dst, int32_t step,
                                uint8_t hev_threshold,
                                uint8_t interior_limit,
                                uint8_t edge_limit,
                                uint8_t loop_size) // 16 for Y, 8 for U/V
{
  int32_t i = 0;

  for(i = 0; i < loop_size; i++)
  {
    if(do_filter_normal(interior_limit, edge_limit, dst + i, step))
    {
      if(hev(hev_threshold,  dst + i, step))
      {
        common_adjust(1, dst + i, step);
      }
      else
      {
        vp8_lf_mb_edges(dst + i, step);
      }
    }
  }

  return;
} // vp8_lf_normal_mb_he()



static void vp8_lf_normal_sb_ve(uint8_t* dst, int32_t step,
                                uint8_t hev_threshold,
                                uint8_t interior_limit,
                                uint8_t edge_limit,
                                uint8_t loop_size) // 16 for Y, 8 for U/V
{
  int32_t i = 0;
  int32_t hv = 0;

  for(i = 0; i < loop_size; i++)
  {
    if(do_filter_normal(interior_limit, edge_limit, dst + i*step, 1))
    {
      hv = hev(hev_threshold,  dst + i*step, 1);
      if(!hv)
        common_adjust(0, dst + i*step, 1);
      else
        common_adjust(1, dst + i*step, 1);
    }
  }

  return;
} // vp8_lf_normal_sb_ve()



static void vp8_lf_normal_sb_he(uint8_t* dst, int32_t step,
                                uint8_t hev_threshold,
                                uint8_t interior_limit,
                                uint8_t edge_limit,
                                uint8_t loop_size) // 16 for Y, 8 for U/V
{
  int32_t i = 0;
  int32_t hv = 0;

  for(i = 0; i < loop_size; i++)
  {
    if(do_filter_normal(interior_limit, edge_limit, dst + i, step))
    {
      hv = hev(hev_threshold,  dst + i, step);
      if(!hv)
        common_adjust(0, dst + i, step);
      else
        common_adjust(1, dst + i, step);
    }
  }

  return;
} // vp8_lf_normal_sb_he()


static void vp8_loop_filter_normal_mb_ve(uint8_t* dst_y, int32_t step_y,
                                         uint8_t* dst_u, uint8_t* dst_v,
                                         int32_t step_uv,
                                         uint8_t filter_level,
                                         uint8_t interior_limit,
                                         uint8_t hev_threshold)
{
  uint8_t mb_edge_limit;

  mb_edge_limit = ((filter_level + 2) << 1) + interior_limit;

  vp8_lf_normal_mb_ve(dst_y,  step_y,  hev_threshold, interior_limit, mb_edge_limit, 16);

  vp8_lf_normal_mb_ve(dst_u,  step_uv, hev_threshold, interior_limit, mb_edge_limit, 8);
  vp8_lf_normal_mb_ve(dst_v,  step_uv, hev_threshold, interior_limit, mb_edge_limit, 8);

  return;
} // vp8_loop_filter_normal_mb_ve()


static void vp8_loop_filter_normal_mb_he(uint8_t* dst_y, int32_t step_y,
                                         uint8_t* dst_u, uint8_t* dst_v,
                                         int32_t step_uv,
                                         uint8_t  filter_level,
                                         uint8_t  interior_limit,
                                         uint8_t  hev_threshold)
{
  uint8_t mb_edge_limit;

  mb_edge_limit = ((filter_level + 2) << 1) + interior_limit;

  vp8_lf_normal_mb_he(dst_y,  step_y,  hev_threshold, interior_limit, mb_edge_limit, 16);

  vp8_lf_normal_mb_he(dst_u,  step_uv, hev_threshold, interior_limit, mb_edge_limit, 8);
  vp8_lf_normal_mb_he(dst_v,  step_uv, hev_threshold, interior_limit, mb_edge_limit, 8);

  return;
} // vp8_loop_filter_normal_mb_he()



static void vp8_loop_filter_normal_sb_ve(uint8_t* dst_y, int32_t step_y,
                                         uint8_t* dst_u, uint8_t* dst_v,
                                         int32_t step_uv,
                                         uint8_t  filter_level,
                                         uint8_t  interior_limit,
                                         uint8_t  hev_threshold)
{
  uint8_t sb_edge_limit;

  sb_edge_limit = (filter_level << 1) + interior_limit;

  vp8_lf_normal_sb_ve(dst_y + 4,  step_y,  hev_threshold, interior_limit, sb_edge_limit, 16);
  vp8_lf_normal_sb_ve(dst_y + 8,  step_y,  hev_threshold, interior_limit, sb_edge_limit, 16);
  vp8_lf_normal_sb_ve(dst_y + 12, step_y,  hev_threshold, interior_limit, sb_edge_limit, 16);

  vp8_lf_normal_sb_ve(dst_u + 4,  step_uv, hev_threshold, interior_limit, sb_edge_limit, 8);
  vp8_lf_normal_sb_ve(dst_v + 4,  step_uv, hev_threshold, interior_limit, sb_edge_limit, 8);

  return;
} // vp8_loop_filter_normal_sb_ve()



static void vp8_loop_filter_normal_sb_he(uint8_t* dst_y, int32_t step_y,
                                         uint8_t* dst_u, uint8_t* dst_v,
                                         int32_t step_uv,
                                         uint8_t  filter_level,
                                         uint8_t  interior_limit,
                                         uint8_t  hev_threshold)
{
  uint8_t sb_edge_limit;

  sb_edge_limit = (filter_level << 1) + interior_limit;

  vp8_lf_normal_sb_he(dst_y + 4*step_y,  step_y,  hev_threshold, interior_limit, sb_edge_limit, 16);
  vp8_lf_normal_sb_he(dst_y + 8*step_y,  step_y,  hev_threshold, interior_limit, sb_edge_limit, 16);
  vp8_lf_normal_sb_he(dst_y + 12*step_y, step_y,  hev_threshold, interior_limit, sb_edge_limit, 16);

  vp8_lf_normal_sb_he(dst_u + 4*step_uv,  step_uv, hev_threshold, interior_limit, sb_edge_limit, 8);
  vp8_lf_normal_sb_he(dst_v + 4*step_uv,  step_uv, hev_threshold, interior_limit, sb_edge_limit, 8);

  return;
} // vp8_loop_filter_normal_sb_he()


//
// simple filters
//
static void vp8_loop_filter_simple_ve(uint8_t* dst_y, int32_t step_y, uint8_t edge_limit)
{
  int32_t i = 0;

  for(i = 0; i < 16; i++)
  {
    if(do_filter_simple(edge_limit, dst_y + i*step_y, 1))
      common_adjust(1, dst_y + i*step_y, 1);
  }

  return;
} // vp8_loop_filter_simple_ve()


static void vp8_loop_filter_simple_he(uint8_t* dst_y, int32_t step_y, uint8_t edge_limit)
{
  int32_t i = 0;

  for(i = 0; i < 16; i++)
  {
    if(do_filter_simple(edge_limit, dst_y + i, step_y))
      common_adjust(1, dst_y + i, step_y);
  }

  return;
} // vp8_loop_filter_simple_he()


void VP8VideoDecoder::LoopFilterInit(void)
{
  uint32_t mb_row = 0;
  uint32_t mb_col = 0;

  int32_t filter_level   = 0;
  uint8_t  interior_limit = 0;

  vp8_MbInfo* currMb  = 0;

  // prepare all needed filter params for each Mb in currFrame
  for(mb_row = 0; mb_row < m_FrameInfo.mbPerCol; mb_row++)
  {
    for(mb_col = 0; mb_col < m_FrameInfo.mbPerRow; mb_col++)
    {
      filter_level = m_FrameInfo.loopFilterLevel;

      currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

      if(m_FrameInfo.segmentationEnabled)
      {
        if(m_FrameInfo.segmentAbsMode)
          filter_level = m_FrameInfo.segmentFeatureData[VP8_ALT_LOOP_FILTER][currMb->segmentID];
        else
          filter_level += m_FrameInfo.segmentFeatureData[VP8_ALT_LOOP_FILTER][currMb->segmentID];
      }

      if(m_FrameInfo.mbLoopFilterAdjust)
      {
        filter_level += m_FrameInfo.refLoopFilterDeltas[currMb->refFrame];

        if(vp8_mode_lf_deltas_lut[currMb->mode] >= 0)
  /*??*/  filter_level += m_FrameInfo.modeLoopFilterDeltas[vp8_mode_lf_deltas_lut[currMb->mode]]; //?????
      }

      vp8_CLIP(filter_level, 0, VP8_MAX_LF_LEVEL);
      currMb->lfInfo.filterLevel = (uint8_t)filter_level;

      interior_limit = (uint8_t)filter_level;

      if(m_FrameInfo.sharpnessLevel)
      {
        interior_limit = (uint8_t)(filter_level >> ((m_FrameInfo.sharpnessLevel > 4) ? 2 : 1));
        interior_limit = MFX_MIN(interior_limit, 9 - m_FrameInfo.sharpnessLevel);
      }

      if(!interior_limit)
        interior_limit = 1;

      currMb->lfInfo.interiorLimit = interior_limit;

      currMb->lfInfo.skipBlockFilter = (currMb->mode != VP8_MB_B_PRED) &&
                                       (currMb->mode != VP8_MV_SPLIT) && currMb->skipCoeff;

    } // mb_col
  } // mb_row

  return;
} // VP8VideoDecoder::LoopFilterInit()


void VP8VideoDecoder::LoopFilterNormal(void)
{
  uint32_t mb_row = 0;
  uint32_t mb_col = 0;

  // Calculate all filters params limits
  LoopFilterInit();


  uint8_t* ptr_y = 0;
  uint8_t* ptr_u = 0;
  uint8_t* ptr_v = 0;

  int32_t step_y  = m_CurrFrame->step_y;
  int32_t step_uv = m_CurrFrame->step_uv;

  vp8_MbInfo* currMb  = 0;

  for(mb_row = 0; mb_row < m_FrameInfo.mbPerCol; mb_row++)
  {
    ptr_y = m_CurrFrame->data_y + mb_row*16*step_y;
    ptr_u = m_CurrFrame->data_u + mb_row*8*step_uv;
    ptr_v = m_CurrFrame->data_v + mb_row*8*step_uv;

    for(mb_col = 0; mb_col < m_FrameInfo.mbPerRow; mb_col++)
    {
      currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

      if(currMb->lfInfo.filterLevel)
      {
        if(mb_col) // inter Mb VertEdges
        {
          if(m_FrameInfo.frameType < 3)
            vp8_loop_filter_normal_mb_ve(ptr_y, step_y,
                                           ptr_u, ptr_v, 
                                           step_uv,
                                           currMb->lfInfo.filterLevel,
                                           currMb->lfInfo.interiorLimit,
                                           vp8_hev_thresh_lut[m_FrameInfo.frameType - 1][currMb->lfInfo.filterLevel]);
        }

        if(!currMb->lfInfo.skipBlockFilter) // inter subBlock VertEdges
        {
          if(m_FrameInfo.frameType < 3)
              vp8_loop_filter_normal_sb_ve(ptr_y, step_y,
                                           ptr_u, ptr_v, 
                                           step_uv,
                                           currMb->lfInfo.filterLevel,
                                           currMb->lfInfo.interiorLimit,
                                           vp8_hev_thresh_lut[m_FrameInfo.frameType - 1][currMb->lfInfo.filterLevel]);
        }

        if(mb_row) // inter Mb HorEdges
        {
          if(m_FrameInfo.frameType < 3)
            vp8_loop_filter_normal_mb_he(ptr_y, step_y,
                                       ptr_u, ptr_v, 
                                       step_uv,
                                       currMb->lfInfo.filterLevel,
                                       currMb->lfInfo.interiorLimit,
                                       vp8_hev_thresh_lut[m_FrameInfo.frameType - 1][currMb->lfInfo.filterLevel]);
        }

        if(!currMb->lfInfo.skipBlockFilter) // inter subBlock HorEdges
        {
          if(m_FrameInfo.frameType < 3)
              vp8_loop_filter_normal_sb_he(ptr_y, step_y,
                                           ptr_u, ptr_v, 
                                           step_uv,
                                           currMb->lfInfo.filterLevel,
                                           currMb->lfInfo.interiorLimit,
                                           vp8_hev_thresh_lut[m_FrameInfo.frameType - 1][currMb->lfInfo.filterLevel]);
        }
      }

      ptr_y += 16;
      ptr_u += 8;
      ptr_v += 8;

    } // mb_col
  } // mb_row


  return;
}


void VP8VideoDecoder::LoopFilterSimple(void)
{
  uint32_t mb_row = 0;
  uint32_t mb_col = 0;

  // Calculate all filters params limits
  LoopFilterInit();

  uint8_t* ptr_y = 0;

  int32_t step_y  = m_CurrFrame->step_y;

  vp8_MbInfo* currMb  = 0;


  uint8_t  mb_edge_limit;
  uint8_t  sb_edge_limit;

  for(mb_row = 0; mb_row < m_FrameInfo.mbPerCol; mb_row++)
  {
    ptr_y = m_CurrFrame->data_y + mb_row*16*step_y;

    for(mb_col = 0; mb_col < m_FrameInfo.mbPerRow; mb_col++)
    {
      currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

      if(currMb->lfInfo.filterLevel)
      {
        mb_edge_limit = ((currMb->lfInfo.filterLevel + 2) << 1) +
                        currMb->lfInfo.interiorLimit;

        sb_edge_limit = (currMb->lfInfo.filterLevel << 1) + currMb->lfInfo.interiorLimit;

        if(mb_col) // inter Mb VertEdges
        {
          vp8_loop_filter_simple_ve(ptr_y, step_y, mb_edge_limit);
        }

        if(!currMb->lfInfo.skipBlockFilter) // inter subBlock VertEdges
        {
          vp8_loop_filter_simple_ve(ptr_y + 4,  step_y, sb_edge_limit);
          vp8_loop_filter_simple_ve(ptr_y + 8,  step_y, sb_edge_limit);
          vp8_loop_filter_simple_ve(ptr_y + 12, step_y, sb_edge_limit);
        }

        if(mb_row) // inter Mb HorEdges
        {
          vp8_loop_filter_simple_he(ptr_y, step_y,mb_edge_limit);
        }

        if(!currMb->lfInfo.skipBlockFilter) // inter subBlock HorEdges
        {
          vp8_loop_filter_simple_he(ptr_y + 4*step_y,  step_y, sb_edge_limit);
          vp8_loop_filter_simple_he(ptr_y + 8*step_y,  step_y, sb_edge_limit);
          vp8_loop_filter_simple_he(ptr_y + 12*step_y, step_y, sb_edge_limit);
        }
      }

      ptr_y += 16;

    } // mb_col
  } // mb_row

  return;
} // VP8VideoDecoder::LoopFilterSimple()


} // namespace UMC

#endif // UMC_ENABLE_VP8_VIDEO_DECODER

