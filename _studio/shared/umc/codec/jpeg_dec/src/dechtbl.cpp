//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)

#include "jpegbase.h"
#include "dechtbl.h"
#include <cstdlib>

CJPEGDecoderHuffmanTable::CJPEGDecoderHuffmanTable(void)
{
  m_id     = 0;
  m_hclass = 0;
#ifdef ALLOW_JPEG_SW_FALLBACK
  m_table  = 0;
#endif
  m_bEmpty = 1;
  m_bValid = 0;

  memset(m_bits, 0, sizeof(m_bits));
  memset(m_vals, 0, sizeof(m_vals));

  return;
} // ctor


CJPEGDecoderHuffmanTable::~CJPEGDecoderHuffmanTable(void)
{
  Destroy();
  return;
} // dtor


JERRCODE CJPEGDecoderHuffmanTable::Create(void)
{
#ifdef ALLOW_JPEG_SW_FALLBACK
  int       size;
  IppStatus status;

  status = ippiDecodeHuffmanSpecGetBufSize_JPEG_8u(&size);
  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiDecodeHuffmanSpecGetBufSize_JPEG_8u() failed - ",status);
    return JPEG_ERR_INTERNAL;
  }

  if(0 != m_table)
  {
    free(m_table);
    m_table = 0;
  }

  m_table = (IppiDecodeHuffmanSpec*)malloc(size);
  if(0 == m_table)
  {
    LOG0("IPP Error: malloc() failed");
    return JPEG_ERR_ALLOC;
  }
#endif
  m_bEmpty = 0;
  m_bValid = 0;

  return JPEG_OK;
} // CJPEGDecoderHuffmanTable::Create()


JERRCODE CJPEGDecoderHuffmanTable::Destroy(void)
{
  m_id     = 0;
  m_hclass = 0;

  memset(m_bits, 0, sizeof(m_bits));
  memset(m_vals, 0, sizeof(m_vals));

#ifdef ALLOW_JPEG_SW_FALLBACK
  if(0 != m_table)
  {
    free(m_table);
    m_table = 0;
  }
#endif
  m_bValid = 0;
  m_bEmpty = 1;

  return JPEG_OK;
} // CJPEGDecoderHuffmanTable::Destroy()


JERRCODE CJPEGDecoderHuffmanTable::Init(int id,int hclass,Ipp8u* bits,Ipp8u* vals)
{
  IppStatus status;

  m_id     = id     & 0x0f;
  m_hclass = hclass & 0x0f;

  MFX_INTERNAL_CPY(m_bits,bits,16);
  MFX_INTERNAL_CPY(m_vals,vals,256);

#ifdef ALLOW_JPEG_SW_FALLBACK
  status = ippiDecodeHuffmanSpecInit_JPEG_8u(m_bits,m_vals,m_table);
  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiDecodeHuffmanSpecInit_JPEG_8u() failed - ",status);
    return JPEG_ERR_DHT_DATA;
  }
#endif
  m_bValid = 1;
  m_bEmpty = 0;

  return JPEG_OK;
} // CJPEGDecoderHuffmanTable::Init()



#ifdef ALLOW_JPEG_SW_FALLBACK
CJPEGDecoderHuffmanState::CJPEGDecoderHuffmanState(void)
{
  m_state = 0;
  return;
} // ctor


CJPEGDecoderHuffmanState::~CJPEGDecoderHuffmanState(void)
{
  Destroy();
  return;
} // dtor


JERRCODE CJPEGDecoderHuffmanState::Create(void)
{
  int       size;
  IppStatus status;

  status = ippiDecodeHuffmanStateGetBufSize_JPEG_8u(&size);
  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiDecodeHuffmanStateGetBufSize_JPEG_8u() failed - ",status);
    return JPEG_ERR_INTERNAL;
  }

  if(0 != m_state)
  {
    free(m_state);
    m_state = 0;
  }

  m_state = (IppiDecodeHuffmanState*)malloc(size);
  if(0 == m_state)
  {
    LOG0("IPP Error: malloc() failed");
    return JPEG_ERR_ALLOC;
  }

  return JPEG_OK;
} // CJPEGDecoderHuffmanState::Create()


JERRCODE CJPEGDecoderHuffmanState::Destroy(void)
{
  if(0 != m_state)
  {
    free(m_state);
    m_state = 0;
  }

  return JPEG_OK;
} // CJPEGDecoderHuffmanState::Destroy()


JERRCODE CJPEGDecoderHuffmanState::Init(void)
{
  IppStatus status;

  status = ippiDecodeHuffmanStateInit_JPEG_8u(m_state);
  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiDecodeHuffmanStateInit_JPEG_8u() failed - ",status);
    return JPEG_ERR_INTERNAL;
  }

  return JPEG_OK;
} // CJPEGDecoderHuffmanState::Init()
#endif // #ifdef ALLOW_JPEG_SW_FALLBACK

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
