// Copyright (c) 2001-2018 Intel Corporation
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
#include "umc_structures.h"

#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#if defined(__GNUC__)
#if defined(__INTEL_COMPILER)
#pragma warning (disable:1478)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#include "decqtbl.h"

CJPEGDecoderQuantTable::CJPEGDecoderQuantTable(void)
{
  m_id          = 0;
  m_precision   = 0;
  m_initialized = 0;

  // align for max performance
  m_raw8u  = UMC::align_pointer<Ipp8u *>(m_rbf, CPU_CACHE_LINE);
  m_raw16u = UMC::align_pointer<Ipp16u *>(m_rbf,CPU_CACHE_LINE);
  memset(m_rbf, 0, sizeof(m_rbf));

#ifdef ALLOW_JPEG_SW_FALLBACK
  m_qnt16u = UMC::align_pointer<Ipp16u *>(m_qbf,CPU_CACHE_LINE);
  m_qnt32f = UMC::align_pointer<Ipp32f *>(m_qbf,CPU_CACHE_LINE);

  memset(m_qbf, 0, sizeof(m_qbf));
#endif
  return;
} // ctor


CJPEGDecoderQuantTable::~CJPEGDecoderQuantTable(void)
{
  m_id          = 0;
  m_precision   = 0;
  m_initialized = 0;

  memset(m_rbf, 0, sizeof(m_rbf));
#ifdef ALLOW_JPEG_SW_FALLBACK   
  memset(m_qbf, 0, sizeof(m_qbf));
#endif
  return;
} // dtor


JERRCODE CJPEGDecoderQuantTable::Init(int id,Ipp8u raw[64])
{
  m_id        = id & 0x0f;
  m_precision = 0; // 8-bit precision

  MFX_INTERNAL_CPY(m_raw8u,raw,DCTSIZE2);

#ifdef ALLOW_JPEG_SW_FALLBACK
  IppStatus status = ippiQuantInvTableInit_JPEG_8u16u(m_raw8u,m_qnt16u);
  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiQuantInvTableInit_JPEG_8u16u() failed - ",status);
    return JPEG_ERR_INTERNAL;
  }
#endif
  m_initialized = 1;

  return JPEG_OK;
} // CJPEGDecoderQuantTable::Init()

#ifdef ALLOW_JPEG_SW_FALLBACK
static
IppStatus ippiQuantInvTableInit_JPEG_16u32f(
  Ipp16u* raw,
  Ipp32f* qnt)
{
  Ipp16u    wb[DCTSIZE2];
  IppStatus status;

  status = ippiZigzagInv8x8_16s_C1((Ipp16s*)raw,(Ipp16s*)wb);
  if(ippStsNoErr != status)
  {
    return status;
  }

  for(int i = 0; i < DCTSIZE2; i++)
    ((Ipp32f*)qnt)[i] = (Ipp32f)((Ipp16u*)wb)[i];

  return ippStsNoErr;
} // ippiQuantInvTableInit_JPEG_16u32f()
#endif

JERRCODE CJPEGDecoderQuantTable::Init(int id,Ipp16u raw[64])
{
  m_id        = id & 0x0f;
  m_precision = 1; // 16-bit precision

  MFX_INTERNAL_CPY((Ipp16s*)m_raw16u, (Ipp16s*)raw, DCTSIZE2*sizeof(Ipp16s));
#ifdef ALLOW_JPEG_SW_FALLBACK
  IppStatus status = ippiQuantInvTableInit_JPEG_16u32f(m_raw16u,m_qnt32f);
  if(ippStsNoErr != status)
  {
    LOG1("IPP Error: ippiQuantInvTableInit_JPEG_16u32f() failed - ",status);
    return JPEG_ERR_INTERNAL;
  }
#endif

  m_initialized = 1;

  return JPEG_OK;
} // CJPEGDecoderQuantTable::Init()

#ifdef ALLOW_JPEG_SW_FALLBACK
JERRCODE CJPEGDecoderQuantTable::ConvertToLowPrecision(void)
{
  IppStatus status;

  status = ippiZigzagInv8x8_16s_C1((Ipp16s*)m_raw16u,(Ipp16s*)m_qnt16u);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  m_precision   = 0; // 8-bit precision
  m_initialized = 1;

  return JPEG_OK;
} // CJPEGDecoderQuantTable::ConvertToLowPrecision()


JERRCODE CJPEGDecoderQuantTable::ConvertToHighPrecision(void)
{
  int       step;
  IppiSize  roi = { DCTSIZE, DCTSIZE };
  Ipp16u    wb[DCTSIZE2];
  IppStatus status;

  step = DCTSIZE * sizeof(Ipp16s);

  status = ippiConvert_8u16u_C1R(m_raw8u,DCTSIZE*sizeof(Ipp8u),wb,step,roi);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  status = ippiCopy_16s_C1R((Ipp16s*)wb,step,(Ipp16s*)m_raw16u,step,roi);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  status = ippiQuantInvTableInit_JPEG_16u32f(m_raw16u,m_qnt32f);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  m_precision   = 1; // 16-bit precision
  m_initialized = 1;

  return JPEG_OK;
} // CJPEGDecoderQuantTable::ConvertToHighPrecision()
#endif

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
