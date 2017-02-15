//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2017 Intel Corporation. All Rights Reserved.
//

#ifndef __COLORCOMP_H__
#define __COLORCOMP_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER) || defined (UMC_ENABLE_MJPEG_VIDEO_ENCODER)
#include "ippj.h"
#include "jpegbase.h"



class CJPEGColorComponent
{
public:
  int m_id;
  int m_comp_no;
  int m_hsampling;
  int m_vsampling;
  int m_scan_hsampling;
  int m_scan_vsampling;
  int m_h_factor;
  int m_v_factor;
  int m_nblocks;
  int m_q_selector;
  int m_dc_selector;
  int m_ac_selector;
  int m_ac_scan_completed;
  int m_cc_height;
  int m_cc_step;
  int m_cc_bufsize;
  int m_ss_height;
  int m_ss_step;
  int m_ss_bufsize;
  int m_need_upsampling;
  Ipp16s m_lastDC;

  CMemoryBuffer m_cc_buf;
  CMemoryBuffer m_ss_buf;
  CMemoryBuffer m_row1;
  CMemoryBuffer m_row2;

  CMemoryBuffer m_lnz_buf;
  int           m_lnz_bufsize;
  int           m_lnz_ds;

  Ipp16s* m_curr_row;
  Ipp16s* m_prev_row;

  CJPEGColorComponent(void);
  virtual ~CJPEGColorComponent(void);

  JERRCODE CreateBufferCC(int num_threads = 1);
  JERRCODE DeleteBufferCC(void);

  JERRCODE CreateBufferSS(int num_threads = 1);
  JERRCODE DeleteBufferSS(void);

  JERRCODE CreateBufferLNZ(int num_threads = 1);
  JERRCODE DeleteBufferLNZ(void);

  Ipp8u* GetCCBufferPtr(int thread_id = 0);
  // Get the buffer pointer
  template <class type_t> inline
  type_t *GetCCBufferPtr(const Ipp32u colMCU) const;

  Ipp8u* GetSSBufferPtr(int thread_id = 0);
  // Get the buffer pointer
  template <class type_t> inline
  type_t *GetSSBufferPtr(const Ipp32u colMCU) const;

  Ipp8u* GetLNZBufferPtr(int thread_id = 0);

};

template <class type_t> inline
type_t *CJPEGColorComponent::GetCCBufferPtr (const Ipp32u colMCU) const
{
  type_t *ptr = (type_t *) m_cc_buf.m_buffer;

  return (ptr + colMCU * 8 * m_hsampling);

} // type_t *CJPEGColorComponent::GetCCBufferPtr (const Ipp32u colMCU) const

template <class type_t> inline
type_t *CJPEGColorComponent::GetSSBufferPtr(const Ipp32u colMCU) const
{
  type_t *ptr = (type_t *) m_ss_buf.m_buffer;

  return (ptr + colMCU * 8 * m_hsampling);

} // type_t *CJPEGColorComponent::GetSSBufferPtr(const Ipp32u colMCU) const

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER || UMC_ENABLE_MJPEG_VIDEO_ENCODER
#endif // __COLORCOMP_H__
