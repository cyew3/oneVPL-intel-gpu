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
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <stdio.h>
#include <string.h>

#include "ippi.h"
#include "ippj.h"
#include "jpegbase.h"
#include "jpegdec.h"
#include <cstdlib>

#include "vm_debug.h"

extern void ConvertFrom_YUV444_To_YV12(const Ipp8u *src[3], Ipp32u srcPitch, Ipp8u * dst[2], Ipp32u dstPitch, IppiSize size);
extern void ConvertFrom_YUV422V_To_YV12(Ipp8u *src[3], Ipp32u srcPitch, Ipp8u * dst[2], Ipp32u dstPitch, IppiSize size);
extern void ConvertFrom_YUV422H_4Y_To_NV12(const Ipp8u *src[3], Ipp32u srcPitch, Ipp8u * dst[2], Ipp32u dstPitch, IppiSize size);
extern void ConvertFrom_YUV422V_4Y_To_NV12(const Ipp8u *src[3], Ipp32u srcPitch, Ipp8u * dst[2], Ipp32u dstPitch, IppiSize size);

#define HUFF_ROW_API // enable new huffman functions, to improve performance

#define DCT_QUANT_INV8x8To1x1LS(pMCUBuf, dst, qtbl)\
{\
  int val = (((pMCUBuf[0] * qtbl[0]) >> 3) + 128);\
  dst[0] = (Ipp8u)(val > 255 ? 255 : (val < 0 ? 0 : val));\
}

CJPEGDecoder::CJPEGDecoder(void)
    : m_dst()
{
  Reset();
  return;
} // ctor


CJPEGDecoder::~CJPEGDecoder(void)
{
  Clean();
  return;
} // dtor


void CJPEGDecoder::Reset(void)
{
  m_jpeg_width             = 0;
  m_jpeg_height            = 0;
  m_jpeg_ncomp             = 0;
  m_jpeg_precision         = 8;
  m_jpeg_sampling          = JS_OTHER;
  m_jpeg_color             = JC_UNKNOWN;
  m_jpeg_quality           = 100;
  
  m_jpeg_mode              = JPEG_UNKNOWN;
  m_jpeg_dct_scale         = JD_1_1;
  m_dd_factor              = 1;

  m_jpeg_comment_detected  = 0;
  m_jpeg_comment           = 0;
  m_jpeg_comment_size      = 0;

  m_jfif_app0_detected     = 0;
  m_jfif_app0_major        = 0;
  m_jfif_app0_minor        = 0;
  m_jfif_app0_units        = 0;
  m_jfif_app0_xDensity     = 0;
  m_jfif_app0_yDensity     = 0;
  m_jfif_app0_thumb_width  = 0;
  m_jfif_app0_thumb_height = 0;

  m_jfxx_app0_detected     = 0;
  m_jfxx_thumbnails_type   = 0;

  m_avi1_app0_detected     = 0;
  m_avi1_app0_polarity     = 0;
  m_avi1_app0_reserved     = 0;
  m_avi1_app0_field_size   = 0;
  m_avi1_app0_field_size2  = 0;

  m_exif_app1_detected     = 0;
  m_exif_app1_data_size    = 0;
  m_exif_app1_data         = 0;

  m_adobe_app14_detected   = 0;
  m_adobe_app14_version    = 0;
  m_adobe_app14_flags0     = 0;
  m_adobe_app14_flags1     = 0;
  m_adobe_app14_transform  = 0;

  m_precision              = 0;
  m_max_hsampling          = 0;
  m_max_vsampling          = 0;
  m_numxMCU                = 0;
  m_numyMCU                = 0;
  m_mcuWidth               = 0;
  m_mcuHeight              = 0;
  m_ccWidth                = 0;
  m_ccHeight               = 0;
  m_xPadding               = 0;
  m_yPadding               = 0;
  m_rst_go                 = 0;
  m_mcu_decoded            = 0;
  m_mcu_to_decode          = 0;
  m_restarts_to_go         = 0;
  m_next_restart_num       = 0;
  m_sos_len                = 0;
  m_curr_comp_no           = 0;
  m_num_scans              = 0;
  for(int i = 0; i < MAX_SCANS_PER_FRAME; i++)
  {
    m_scans[i].scan_no = i;
    m_scans[i].jpeg_restart_interval = 0;
    m_scans[i].min_h_factor = 0;
    m_scans[i].min_v_factor = 0;
    m_scans[i].numxMCU = 0;
    m_scans[i].numyMCU = 0;
    m_scans[i].mcuWidth = 0;
    m_scans[i].mcuHeight = 0;
    m_scans[i].xPadding = 0;
    m_scans[i].yPadding = 0;
    m_scans[i].ncomps = 0;
    m_scans[i].first_comp = 0;
  }
  m_curr_scan = &m_scans[0];
  m_ss                     = 0;
  m_se                     = 0;
  m_al                     = 0;
  m_ah                     = 0;
  m_dc_scan_completed      = 0;
  m_ac_scans_completed     = 0;
  m_init_done              = 0;
  m_marker                 = JM_NONE;

  m_block_buffer           = 0;
  m_num_threads            = 0;
  m_nblock                 = 0;

  m_use_qdct               = 0;
  m_sof_find               = 0;
#ifdef __TIMING__
  m_clk_dct                = 0;

  m_clk_dct1x1             = 0;
  m_clk_dct2x2             = 0;
  m_clk_dct4x4             = 0;
  m_clk_dct8x8             = 0;

  m_clk_ss                 = 0;
  m_clk_cc                 = 0;
  m_clk_diff               = 0;
  m_clk_huff               = 0;
#endif

  return;
} // CJPEGDecoder::Reset(void)


JERRCODE CJPEGDecoder::Clean(void)
{
  int i;

  m_jpeg_comment_detected = 0;

  if(0 != m_jpeg_comment)
  {
    free(m_jpeg_comment);
    m_jpeg_comment = 0;
    m_jpeg_comment_size = 0;
  }

  m_avi1_app0_detected    = 0;
  m_avi1_app0_polarity    = 0;
  m_avi1_app0_reserved    = 0;
  m_avi1_app0_field_size  = 0;
  m_avi1_app0_field_size2 = 0;

  m_jfif_app0_detected    = 0;
  m_jfxx_app0_detected    = 0;

  m_exif_app1_detected    = 0;

  if(0 != m_exif_app1_data)
  {
    free(m_exif_app1_data);
    m_exif_app1_data = 0;
  }

  m_adobe_app14_detected = 0;

  m_curr_scan->ncomps = 0;
  m_init_done   = 0;

  for(i = 0; i < MAX_COMPS_PER_SCAN; i++)
  {
    if(0 != m_ccomp[i].m_curr_row)
    {
      free(m_ccomp[i].m_curr_row);
      m_ccomp[i].m_curr_row = 0;
    }
    if(0 != m_ccomp[i].m_prev_row)
    {
      free(m_ccomp[i].m_prev_row);
      m_ccomp[i].m_prev_row = 0;
    }
  }

  for(i = 0; i < MAX_HUFF_TABLES; i++)
  {
    m_dctbl[i].Destroy();
    m_actbl[i].Destroy();
  }

  if(0 != m_block_buffer)
  {
    free(m_block_buffer);
    m_block_buffer = 0;
  }

  m_state.Destroy();

  return JPEG_OK;
} // CJPEGDecoder::Clean()

JERRCODE CJPEGDecoder::SetDestination(
  Ipp8u*   pDst,
  int      dstStep,
  IppiSize dstSize,
  int      dstChannels,
  JCOLOR   dstColor,
  JSS      dstSampling,
  int      dstPrecision,
  JDD      dstDctScale)
{
  if(0 == pDst)
    return JPEG_ERR_PARAMS;

  if(0 > dstStep)
    return JPEG_ERR_PARAMS;

  if(dstChannels <= 0 || dstChannels > 4)
    return JPEG_ERR_PARAMS;

  if(dstPrecision <= 0 || dstPrecision != m_jpeg_precision)
    return JPEG_ERR_PARAMS;

  m_dst.p.Data8u[0] = pDst;
  m_dst.lineStep[0] = dstStep;
  m_dst.width       = dstSize.width;
  m_dst.height      = dstSize.height;
  m_dst.nChannels   = dstChannels;
  m_dst.color       = dstColor;
  m_dst.sampling    = dstSampling;
  m_dst.precision   = dstPrecision;

  m_jpeg_dct_scale  = dstDctScale;

  m_dst.order = JD_PIXEL;

  return JPEG_OK;
} // CJPEGDecoder::SetDestination()


 JERRCODE CJPEGDecoder::SetDestination(
  Ipp16s*  pDst,
  int      dstStep,
  IppiSize dstSize,
  int      dstChannels,
  JCOLOR   dstColor,
  JSS      dstSampling,
  int      dstPrecision)
{
  m_dst.p.Data16s[0] = pDst;
  m_dst.lineStep[0]  = dstStep;
  m_dst.width        = dstSize.width;
  m_dst.height       = dstSize.height;
  m_dst.nChannels    = dstChannels;
  m_dst.color        = dstColor;
  m_dst.sampling     = dstSampling;
  m_dst.precision    = dstPrecision;

  m_dst.order = JD_PIXEL;

  return JPEG_OK;
} // CJPEGDecoder::SetDestination()


 JERRCODE CJPEGDecoder::SetDestination(
  Ipp8u*   pDst[],
  int      dstStep[],
  IppiSize dstSize,
  int      dstChannels,
  JCOLOR   dstColor,
  JSS      dstSampling,
  int      dstPrecision,
  JDD      dstDctScale)
{
  m_dst.p.Data8u[0] = pDst[0];
  m_dst.p.Data8u[1] = pDst[1];
  m_dst.p.Data8u[2] = pDst[2];
  m_dst.p.Data8u[3] = pDst[3];
  m_dst.lineStep[0] = dstStep[0];
  m_dst.lineStep[1] = dstStep[1];
  m_dst.lineStep[2] = dstStep[2];
  m_dst.lineStep[3] = dstStep[3];

  m_dst.order = JD_PLANE;

  m_dst.width       = dstSize.width;
  m_dst.height      = dstSize.height;
  m_dst.nChannels   = dstChannels;
  m_dst.color       = dstColor;
  m_dst.sampling    = dstSampling;
  m_dst.precision   = dstPrecision;

  m_jpeg_dct_scale  = dstDctScale;

  return JPEG_OK;
} // CJPEGDecoder::SetDestination()


  JERRCODE CJPEGDecoder::SetDestination(
  Ipp16s*  pDst[],
  int      dstStep[],
  IppiSize dstSize,
  int      dstChannels,
  JCOLOR   dstColor,
  JSS      dstSampling,
  int      dstPrecision)
{
  m_dst.p.Data16s[0] = pDst[0];
  m_dst.p.Data16s[1] = pDst[1];
  m_dst.p.Data16s[2] = pDst[2];
  m_dst.p.Data16s[3] = pDst[3];
  m_dst.lineStep[0]  = dstStep[0];
  m_dst.lineStep[1]  = dstStep[1];
  m_dst.lineStep[2]  = dstStep[2];
  m_dst.lineStep[3]  = dstStep[3];

  m_dst.order = JD_PLANE;

  m_dst.width        = dstSize.width;
  m_dst.height       = dstSize.height;
  m_dst.nChannels    = dstChannels;
  m_dst.color        = dstColor;
  m_dst.sampling     = dstSampling;
  m_dst.precision    = dstPrecision;

  return JPEG_OK;
} // CJPEGDecoder::SetDestination()

JERRCODE CJPEGDecoder::ProcessRestart(void)
{
  JERRCODE  jerr;
  IppStatus status;

  status = ippiDecodeHuffmanStateInit_JPEG_8u(m_state);
  if(ippStsNoErr != status)
  {
    LOG0("Error: ippiDecodeHuffmanStateInit_JPEG_8u() failed");
    return JPEG_ERR_INTERNAL;
  }

  for(int n = 0; n < m_jpeg_ncomp; n++)
  {
    m_ccomp[n].m_lastDC = 0;
  }

  jerr = ParseRST();
  if(JPEG_OK != jerr)
  {
    LOG0("Error: ParseRST() failed");
    return jerr;
  }

  m_rst_go = 1;
  m_restarts_to_go = m_curr_scan->jpeg_restart_interval;

  return JPEG_OK;
} // CJPEGDecoder::ProcessRestart()


JERRCODE CJPEGDecoder::ParseAPP1(void)
{
  int i;
  int b0, b1, b2, b3, b4;
  int len;
  JERRCODE jerr;

  TRC0("-> APP0");

  jerr = m_BitStreamIn.ReadWord(&len);
  if(JPEG_OK != jerr)
    return jerr;

  len -= 2;

  jerr = m_BitStreamIn.CheckByte(0,&b0);
  if(JPEG_OK != jerr)
    return jerr;

  jerr = m_BitStreamIn.CheckByte(1,&b1);
  if(JPEG_OK != jerr)
    return jerr;

  jerr = m_BitStreamIn.CheckByte(2,&b2);
  if(JPEG_OK != jerr)
    return jerr;

  jerr = m_BitStreamIn.CheckByte(3,&b3);
  if(JPEG_OK != jerr)
    return jerr;

  jerr = m_BitStreamIn.CheckByte(4,&b4);
  if(JPEG_OK != jerr)
    return jerr;

  if(b0 == 0x45 && // E
     b1 == 0x78 && // x
     b2 == 0x69 && // i
     b3 == 0x66 && // f
     b4 == 0)
  {
    m_exif_app1_detected  = 1;
    m_exif_app1_data_size = len;

    jerr = m_BitStreamIn.Seek(6);
    if(JPEG_OK != jerr)
      return jerr;

    len -= 6;

    if(m_exif_app1_data != 0)
    {
      free(m_exif_app1_data);
      m_exif_app1_data = 0;
    }

    m_exif_app1_data = (Ipp8u*)malloc(len);
    if(0 == m_exif_app1_data)
      return JPEG_ERR_ALLOC;

    for(i = 0; i < len; i++)
    {
      jerr = m_BitStreamIn.ReadByte(&b0);
      if(JPEG_OK != jerr)
        return jerr;

      m_exif_app1_data[i] = (Ipp8u)b0;
    }
  }
  else
  {
    jerr = m_BitStreamIn.Seek(len);
    if(JPEG_OK != jerr)
      return jerr;
  }

  m_marker = JM_NONE;

  return JPEG_OK;
} // CJPEGDecoder::ParseAPP1()

JERRCODE CJPEGDecoder::ParseCOM(void)
{
  int i;
  int c;
  int len;
  JERRCODE jerr;

  TRC0("-> COM");

  jerr = m_BitStreamIn.ReadWord(&len);
  if(JPEG_OK != jerr)
    return jerr;

  len -= 2;

  TRC1("  bytes for comment - ",len);

  m_jpeg_comment_detected = 1;
  m_jpeg_comment_size     = len;

  if(m_jpeg_comment != 0)
  {
    free(m_jpeg_comment);
  }

  m_jpeg_comment = (Ipp8u*)malloc(len+1);
  if(0 == m_jpeg_comment)
    return JPEG_ERR_ALLOC;

  for(i = 0; i < len; i++)
  {
    jerr = m_BitStreamIn.ReadByte(&c);
    if(JPEG_OK != jerr)
      return jerr;
    m_jpeg_comment[i] = (Ipp8u)c;
  }

  m_jpeg_comment[len] = 0;

  m_marker = JM_NONE;

  return JPEG_OK;
} // CJPEGDecoder::ParseCOM()

JERRCODE CJPEGDecoder::ParseSOF2(void)
{
  int i;
  int len;
  CJPEGColorComponent* curr_comp;
  JERRCODE jerr;

  TRC0("-> SOF2");

  jerr = m_BitStreamIn.ReadWord(&len);
  if(JPEG_OK != jerr)
    return jerr;

  len -= 2;

  jerr = m_BitStreamIn.ReadByte(&m_jpeg_precision);
  if(JPEG_OK != jerr)
    return jerr;

  if(m_jpeg_precision != 8)
  {
    return JPEG_NOT_IMPLEMENTED;
  }

  jerr = m_BitStreamIn.ReadWord(&m_jpeg_height);
  if(JPEG_OK != jerr)
    return jerr;

  jerr = m_BitStreamIn.ReadWord(&m_jpeg_width);
  if(JPEG_OK != jerr)
    return jerr;

  jerr = m_BitStreamIn.ReadByte(&m_jpeg_ncomp);
  if(JPEG_OK != jerr)
    return jerr;

  TRC1("  height    - ",m_jpeg_height);
  TRC1("  width     - ",m_jpeg_width);
  TRC1("  nchannels - ",m_jpeg_ncomp);

  if(m_jpeg_ncomp < 0 || m_jpeg_ncomp > MAX_COMPS_PER_SCAN)
  {
    return JPEG_ERR_SOF_DATA;
  }

  len -= 6;

  if(len != m_jpeg_ncomp * 3)
  {
    return JPEG_ERR_SOF_DATA;
  }

  for(i = 0; i < m_jpeg_ncomp; i++)
  {
    curr_comp = &m_ccomp[i];

    jerr = m_BitStreamIn.ReadByte(&curr_comp->m_id);
    if(JPEG_OK != jerr)
      return jerr;

    curr_comp->m_comp_no = i;

    int ss;

    jerr = m_BitStreamIn.ReadByte(&ss);
    if(JPEG_OK != jerr)
      return jerr;

    curr_comp->m_hsampling  = (ss >> 4) & 0x0f;
    curr_comp->m_vsampling  = (ss     ) & 0x0f;

    if(m_jpeg_ncomp == 1)
    {
      curr_comp->m_hsampling  = 1;
      curr_comp->m_vsampling  = 1;
    }

    jerr = m_BitStreamIn.ReadByte(&curr_comp->m_q_selector);
    if(JPEG_OK != jerr)
      return jerr;

    if(curr_comp->m_hsampling <= 0 || curr_comp->m_vsampling <= 0)
    {
      return JPEG_ERR_SOF_DATA;
    }

    // num of DU block per component
    curr_comp->m_nblocks = curr_comp->m_hsampling * curr_comp->m_vsampling;

    // num of DU blocks per frame
    m_nblock += curr_comp->m_nblocks;

    TRC1("    id ",curr_comp->m_id);
    TRC1("      hsampling - ",curr_comp->m_hsampling);
    TRC1("      vsampling - ",curr_comp->m_vsampling);
    TRC1("      qselector - ",curr_comp->m_q_selector);
  }

  jerr = DetectSampling();
  if(JPEG_OK != jerr)
  {
    return jerr;
  }

  m_max_hsampling = m_ccomp[0].m_hsampling;
  m_max_vsampling = m_ccomp[0].m_vsampling;

  for(i = 0; i < m_jpeg_ncomp; i++)
  {
    curr_comp = &m_ccomp[i];

    if(m_max_hsampling < curr_comp->m_hsampling)
      m_max_hsampling = curr_comp->m_hsampling;

    if(m_max_vsampling < curr_comp->m_vsampling)
      m_max_vsampling = curr_comp->m_vsampling;
  }

  for(i = 0; i < m_jpeg_ncomp; i++)
  {
    curr_comp = &m_ccomp[i];

    curr_comp->m_h_factor = m_max_hsampling / curr_comp->m_hsampling;
    curr_comp->m_v_factor = m_max_vsampling / curr_comp->m_vsampling;
  }

  m_jpeg_mode = JPEG_PROGRESSIVE;

  m_marker = JM_NONE;

  return JPEG_OK;
} // CJPEGDecoder::ParseSOF2()


JERRCODE CJPEGDecoder::ParseSOF3(void)
{
  int i;
  int len;
  CJPEGColorComponent* curr_comp;
  JERRCODE jerr;

  TRC0("-> SOF3");

  jerr = m_BitStreamIn.ReadWord(&len);
  if(JPEG_OK != jerr)
    return jerr;

  len -= 2;

  jerr = m_BitStreamIn.ReadByte(&m_jpeg_precision);
  if(JPEG_OK != jerr)
    return jerr;

  if(m_jpeg_precision < 2 || m_jpeg_precision > 16)
  {
    return JPEG_ERR_SOF_DATA;
  }

  jerr = m_BitStreamIn.ReadWord(&m_jpeg_height);
  if(JPEG_OK != jerr)
    return jerr;

  jerr = m_BitStreamIn.ReadWord(&m_jpeg_width);
  if(JPEG_OK != jerr)
    return jerr;

  jerr = m_BitStreamIn.ReadByte(&m_jpeg_ncomp);
  if(JPEG_OK != jerr)
    return jerr;

  TRC1("  height    - ",m_jpeg_height);
  TRC1("  width     - ",m_jpeg_width);
  TRC1("  nchannels - ",m_jpeg_ncomp);

  len -= 6;

  if(len != m_jpeg_ncomp * 3)
  {
    // too short frame segment
    // need to have 3 bytes per component for parameters
    return JPEG_ERR_SOF_DATA;
  }

  for(i = 0; i < m_jpeg_ncomp; i++)
  {
    curr_comp = &m_ccomp[i];

    jerr = m_BitStreamIn.ReadByte(&curr_comp->m_id);
    if(JPEG_OK != jerr)
      return jerr;

    int ss;

    jerr = m_BitStreamIn.ReadByte(&ss);
    if(JPEG_OK != jerr)
      return jerr;

    curr_comp->m_hsampling  = (ss >> 4) & 0x0f;
    curr_comp->m_vsampling  = (ss     ) & 0x0f;

    if(m_jpeg_ncomp == 1)
    {
      curr_comp->m_hsampling  = 1;
      curr_comp->m_vsampling  = 1;
    }

    jerr = m_BitStreamIn.ReadByte(&curr_comp->m_q_selector);
    if(JPEG_OK != jerr)
      return jerr;

    if(curr_comp->m_hsampling <= 0 || curr_comp->m_vsampling <= 0)
    {
      return JPEG_ERR_SOF_DATA;
    }

    // num of DU block per component
    curr_comp->m_nblocks = curr_comp->m_hsampling * curr_comp->m_vsampling;

    // num of DU blocks per frame
    m_nblock += curr_comp->m_nblocks;

    TRC1("    id ",curr_comp->m_id);
    TRC1("      hsampling - ",curr_comp->m_hsampling);
    TRC1("      vsampling - ",curr_comp->m_vsampling);
    TRC1("      qselector - ",curr_comp->m_q_selector);
  }

  jerr = DetectSampling();
  if(JPEG_OK != jerr)
  {
    return jerr;
  }

  m_max_hsampling = m_ccomp[0].m_hsampling;
  m_max_vsampling = m_ccomp[0].m_vsampling;

  for(i = 0; i < m_jpeg_ncomp; i++)
  {
    curr_comp = &m_ccomp[i];

    if(m_max_hsampling < curr_comp->m_hsampling)
      m_max_hsampling = curr_comp->m_hsampling;

    if(m_max_vsampling < curr_comp->m_vsampling)
      m_max_vsampling = curr_comp->m_vsampling;
  }

  for(i = 0; i < m_jpeg_ncomp; i++)
  {
    curr_comp = &m_ccomp[i];

    curr_comp->m_h_factor = m_max_hsampling / curr_comp->m_hsampling;
    curr_comp->m_v_factor = m_max_vsampling / curr_comp->m_vsampling;
  }

  m_jpeg_mode = JPEG_LOSSLESS;

  m_marker = JM_NONE;

  return JPEG_OK;
} // CJPEGDecoder::ParseSOF3()

JERRCODE CJPEGDecoder::ParseRST(void)
{
  JERRCODE jerr;

  TRC0("-> RST");

  if(m_marker == 0xff)
  {
    jerr = m_BitStreamIn.Seek(-1);
    if(JPEG_OK != jerr)
      return jerr;

    m_marker = JM_NONE;
  }

  if(m_marker == JM_NONE)
  {
    jerr = NextMarker(&m_marker);
    if(JPEG_OK != jerr)
    {
      LOG0("Error: NextMarker() failed");
      return jerr;
    }
  }

  TRC1("restart interval ",m_next_restart_num);
  if(m_marker == ((int)JM_RST0 + m_next_restart_num))
  {
    m_marker = JM_NONE;
  }
  else
  {
    LOG1("  - got marker   - ",m_marker);
    LOG1("  - but expected - ",(int)JM_RST0 + m_next_restart_num);
    m_marker = JM_NONE;
//    return JPEG_ERR_RST_DATA;
  }

  // Update next-restart state
  m_next_restart_num = (m_next_restart_num + 1) & 7;

  return JPEG_OK;

} // CJPEGDecoder::ParseRST()

JERRCODE CJPEGDecoder::ParseData()
{
    Ipp32s i;
    JERRCODE jerr = Init();
    if(JPEG_OK != jerr)
    {
      return jerr;
    }

    switch(m_jpeg_mode)
    {
    case JPEG_BASELINE:
    case JPEG_EXTENDED:
      {
        jerr = DecodeScanBaseline();
        if(JPEG_OK != jerr)
          return jerr;

      }
      break;

    case JPEG_PROGRESSIVE:
      {
        jerr = DecodeScanProgressive();

        m_ac_scans_completed = 0;
        for(i = 0; i < m_jpeg_ncomp; i++)
        {
          m_ac_scans_completed += m_ccomp[i].m_ac_scan_completed;
        }

        if(JPEG_OK != jerr ||
          (m_dc_scan_completed != 0 && m_ac_scans_completed == m_jpeg_ncomp))
        {
          Ipp16s* pMCUBuf;
          for(i = 0; i < (int) m_numyMCU; i++)
          {
            pMCUBuf = m_block_buffer + (i* m_numxMCU * DCTSIZE2* m_nblock);

#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            switch (m_jpeg_dct_scale)
            {
              case JD_1_1:
              {
                  jerr = ReconstructMCURowBL8x8(pMCUBuf, 0, m_numxMCU);
              }
              break;

              case JD_1_2:
              {
                jerr = ReconstructMCURowBL8x8To4x4(pMCUBuf, 0, m_numxMCU);
              }
              break;

              case JD_1_4:
              {
                jerr = ReconstructMCURowBL8x8To2x2(pMCUBuf, 0, m_numxMCU);
              }
              break;

              case JD_1_8:
              {
                jerr = ReconstructMCURowBL8x8To1x1(pMCUBuf, 0, m_numxMCU);
              }
              break;

              default:
                break;
            }
            if(JPEG_OK != jerr)
              return jerr;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_dct += c1 - c0;
#endif
            if(JD_PIXEL == m_dst.order) // pixel by pixel order
            {
#ifdef __TIMING__
              c0 = ippGetCpuClocks();
#endif
              jerr = UpSampling(i, 0, m_numxMCU);
              if(JPEG_OK != jerr)
                return jerr;
#ifdef __TIMING__
              c1 = ippGetCpuClocks();
              m_clk_ss += c1 - c0;
#endif

#ifdef __TIMING__
              c0 = ippGetCpuClocks();
#endif
             jerr = ColorConvert(i, 0, m_numxMCU);
              if(JPEG_OK != jerr)
                return jerr;
#ifdef __TIMING__
              c1 = ippGetCpuClocks();
              m_clk_cc += c1 - c0;
#endif
            }
            else          // plane order
            {
              if(m_jpeg_precision == 8)
              {
                jerr = ProcessBuffer(i);
                if(JPEG_OK != jerr)
                  return jerr;
              }
              else
                return JPEG_NOT_IMPLEMENTED; //not support 16-bit PLANE image
            }
          }
        }
      }
      break;

    case JPEG_LOSSLESS:
      if(m_curr_scan->ncomps == m_jpeg_ncomp)
      {
        jerr = DecodeScanLosslessIN();
        if(JPEG_OK != jerr)
          return jerr;
      }
      else
      {
        jerr = DecodeScanLosslessNI();
        if(JPEG_OK != jerr)
          return jerr;

        if(m_ac_scans_completed == m_jpeg_ncomp)
        {
          Ipp16s* pMCUBuf = m_block_buffer;

          for(i = 0; i < (int) m_numyMCU; i++)
          {
            if(m_curr_scan->jpeg_restart_interval && i*m_numxMCU % m_curr_scan->jpeg_restart_interval == 0)
              m_rst_go = 1;

#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            jerr = ReconstructMCURowLS(pMCUBuf, i);
            if(JPEG_OK != jerr)
              return jerr;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_diff += c1 - c0;
#endif

#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            jerr = ColorConvert(i, 0, m_numxMCU);
            if(JPEG_OK != jerr)
              return jerr;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_cc += c1 - c0;
#endif

            m_rst_go = 0;
          } // for m_numyMCU
        }
      }
      break;

    default:
        jerr = JPEG_ERR_INTERNAL;
        break;

    } // m_jpeg_mode

    if(JPEG_OK != jerr)
      return jerr;

    return JPEG_OK;
}

JERRCODE CJPEGDecoder::FindNextImage()
{
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif
  JERRCODE jerr = JPEG_OK;

  m_marker = JM_NONE;

  for(;;)
  {
    if(JM_NONE == m_marker)
    {
      jerr = NextMarker(&m_marker);
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
    }

    switch(m_marker)
    {
    case JM_EOI:
      jerr = ParseEOI();
      goto Exit;

    case JM_RST0:
    case JM_RST1:
    case JM_RST2:
    case JM_RST3:
    case JM_RST4:
    case JM_RST5:
    case JM_RST6:
    case JM_RST7:
      jerr = ParseRST();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
      break;

    case JM_SOS:
        break;

    default:
      TRC1("-> Unknown marker ",m_marker);
      TRC0("..Skipping");
      jerr = SkipMarker();
      if(JPEG_OK != jerr)
        return jerr;

      break;
    }
  }

Exit:

  return jerr;
} // JERRCODE CJPEGDecoder::FindNextImage()

JERRCODE CJPEGDecoder::ParseJPEGBitStream(JOPERATION op)
{
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif
  JERRCODE jerr = JPEG_OK;

  m_marker = JM_NONE;

  for(;;)
  {
    if(JM_NONE == m_marker)
    {
      jerr = NextMarker(&m_marker);
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
    }

    switch(m_marker)
    {
    case JM_SOI:
      jerr = ParseSOI();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
      break;

    case JM_APP0:
      jerr = ParseAPP0();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
      break;

    case JM_APP1:
      jerr = ParseAPP1();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
      break;

    case JM_APP14:
      jerr = ParseAPP14();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
      break;

    case JM_COM:
      jerr = ParseCOM();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
      break;

    case JM_DQT:
      jerr = ParseDQT();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
      break;

    case JM_SOF0:
      jerr = ParseSOF0();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
      break;

    case JM_SOF1:
      //jerr = ParseSOF1();
      //if(JPEG_OK != jerr)
      //{
      //  return jerr;
      //}
      //break;
      return JPEG_NOT_IMPLEMENTED;

    case JM_SOF2:
      //jerr = ParseSOF2();
      //if(JPEG_OK != jerr)
      //{
      //  return jerr;
      //}
      //break;
      return JPEG_NOT_IMPLEMENTED;

    case JM_SOF3:
      //jerr = ParseSOF3();
      //if(JPEG_OK != jerr)
      //{
      //  return jerr;
      //}
      //break;
      return JPEG_NOT_IMPLEMENTED;

    case JM_SOF5:
    case JM_SOF6:
    case JM_SOF7:
    case JM_SOF9:
    case JM_SOFA:
    case JM_SOFB:
    case JM_SOFD:
    case JM_SOFE:
    case JM_SOFF:
      return JPEG_NOT_IMPLEMENTED;

    case JM_DHT:
      jerr = ParseDHT();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
      break;

    case JM_DRI:
      jerr = ParseDRI();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }
      break;

    case JM_SOS:
      jerr = ParseSOS(op);
      if(JPEG_OK != jerr)
      {
        return jerr;
      }

      if(JO_READ_HEADER == op)
      {
          if(m_BitStreamIn.GetCurrPos() - (m_sos_len + 2) != 0)
          {
              jerr = m_BitStreamIn.Seek(-(m_sos_len + 2));
              if(JPEG_OK != jerr)
                  return jerr;
          }
          else
          {
              m_BitStreamIn.SetCurrPos(0);
          }

        // stop here, when we are reading header
        return JPEG_OK;
      }

      if(JO_READ_DATA == op)
      {
          jerr = ParseData();
          if(JPEG_OK != jerr)
            return jerr;

      }
      break;

      // actually, it should never happen, because in a single threaded
      // application RTSm markers go with SOS data. Multithreaded application
      // don't call this function and process NAL unit by unit separately.
/*
    case JM_RST0:
    case JM_RST1:
    case JM_RST2:
    case JM_RST3:
    case JM_RST4:
    case JM_RST5:
    case JM_RST6:
    case JM_RST7:
      jerr = ParseRST();
      if(JPEG_OK != jerr)
      {
        return jerr;
      }

      m_rst_go = 1;
      m_restarts_to_go = m_curr_scan->jpeg_restart_interval;

      if(JO_READ_DATA == op)
      {
          jerr = ParseData();
          if(JPEG_OK != jerr)
            return jerr;

      }
      break;*/

    case JM_EOI:
      jerr = ParseEOI();
      goto Exit;

    default:
      TRC1("-> Unknown marker ",m_marker);
      TRC0("..Skipping");
      jerr = SkipMarker();
      if(JPEG_OK != jerr)
        return jerr;

      break;
    }
  }

Exit:

  return jerr;
} // CJPEGDecoder::ParseJPEGBitStream()

JERRCODE CJPEGDecoder::Init(void)
{
  int i;
  int tr_buf_size = 0;
  CJPEGColorComponent* curr_comp;
  JERRCODE  jerr;

  if(m_init_done)
    return JPEG_OK;

  m_num_threads = get_num_threads();

  // TODO:
  //   need to add support for images with more than 4 components per frame

  if(m_dst.precision <= 8)
  {
    switch (m_jpeg_dct_scale)
    {
      case JD_1_1:
      default:
        {
          m_dd_factor = 1;
        }
        break;

      case JD_1_2:
        {
          m_dd_factor = 2;
        }
        break;

      case JD_1_4:
        {
          m_dd_factor = 4;
        }
        break;

      case JD_1_8:
        {
          m_dd_factor = 8;
        }
        break;
    }
  }

  for(i = 0; i < m_jpeg_ncomp; i++)
  {
    curr_comp = &m_ccomp[i];

    switch(m_jpeg_mode)
    {
    case JPEG_BASELINE:
    case JPEG_EXTENDED:
      {
        int ds = (m_dst.precision <= 8) ? sizeof(Ipp8u) : sizeof(Ipp16s);

        int lnz_ds               = curr_comp->m_vsampling * curr_comp->m_hsampling;
        curr_comp->m_lnz_bufsize = lnz_ds * m_numxMCU;
        curr_comp->m_lnz_ds      = lnz_ds;

        curr_comp->m_cc_height = m_mcuHeight;
        curr_comp->m_cc_step   = m_numxMCU * m_mcuWidth * ds;

        curr_comp->m_ss_height = curr_comp->m_cc_height / curr_comp->m_v_factor;
        curr_comp->m_ss_step   = curr_comp->m_cc_step   / curr_comp->m_h_factor;

        if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2)
        {
          curr_comp->m_ss_height += 2; // add border lines (top and bottom)
        }

        tr_buf_size = m_numxMCU * m_nblock * DCTSIZE2 * sizeof(Ipp16s) * m_num_threads;

        break;
      } // JPEG_BASELINE

      case JPEG_PROGRESSIVE:
      {
        int lnz_ds               = curr_comp->m_vsampling * curr_comp->m_hsampling;
        curr_comp->m_lnz_bufsize = lnz_ds * m_numxMCU;
        curr_comp->m_lnz_ds      = lnz_ds;

        curr_comp->m_cc_height = m_mcuHeight;
        curr_comp->m_cc_step   = m_numxMCU * m_mcuWidth;

        curr_comp->m_ss_height = curr_comp->m_cc_height / curr_comp->m_v_factor;
        curr_comp->m_ss_step   = curr_comp->m_cc_step   / curr_comp->m_h_factor;

        if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2)
        {
          curr_comp->m_ss_height += 2; // add border lines (top and bottom)
        }

        tr_buf_size = m_numxMCU * m_numyMCU * m_nblock * DCTSIZE2 * sizeof(Ipp16s);

        break;
      } // JPEG_PROGRESSIVE

      case JPEG_LOSSLESS:
      {
        int lnz_ds               = curr_comp->m_vsampling * curr_comp->m_hsampling;
        curr_comp->m_lnz_bufsize = lnz_ds * m_numxMCU;
        curr_comp->m_lnz_ds      = lnz_ds;

        curr_comp->m_cc_height = m_mcuHeight;
        curr_comp->m_cc_step   = m_numxMCU * m_mcuWidth * sizeof(Ipp16s);

        curr_comp->m_ss_height = curr_comp->m_cc_height / curr_comp->m_v_factor;
        curr_comp->m_ss_step   = curr_comp->m_cc_step   / curr_comp->m_h_factor;

        if(m_curr_scan->ncomps == m_jpeg_ncomp)
          tr_buf_size = m_numxMCU * m_nblock * sizeof(Ipp16s);
        else
          tr_buf_size = m_numxMCU * m_numyMCU * m_nblock * sizeof(Ipp16s);

        curr_comp->m_curr_row = (Ipp16s*)malloc(curr_comp->m_cc_step * sizeof(Ipp16s));
        if(0 == curr_comp->m_curr_row)
          return JPEG_ERR_ALLOC;

        curr_comp->m_prev_row = (Ipp16s*)malloc(curr_comp->m_cc_step * sizeof(Ipp16s));
        if(0 == curr_comp->m_prev_row)
          return JPEG_ERR_ALLOC;

        break;
      } // JPEG_LOSSLESS

      default:
        return JPEG_ERR_PARAMS;
    } // m_jpeg_mode

    // color convert buffer
    jerr = curr_comp->CreateBufferCC(m_num_threads);
    if(JPEG_OK != jerr)
      return jerr;

    jerr = curr_comp->CreateBufferSS(m_num_threads);
    if(JPEG_OK != jerr)
      return jerr;

    jerr = curr_comp->CreateBufferLNZ(m_num_threads);
    if(JPEG_OK != jerr)
      return jerr;

  } // m_jpeg_ncomp

  if(0 == m_block_buffer)
  {
    m_block_buffer = (Ipp16s*)malloc(tr_buf_size);
    if(0 == m_block_buffer)
    {
      return JPEG_ERR_ALLOC;
    }

    memset((Ipp8u*)m_block_buffer, 0, tr_buf_size);
  }

  m_state.Create();

  m_init_done = 1;

  return JPEG_OK;
} // CJPEGDecoder::Init()

static
Ipp32u JPEG_BPP[JC_MAX] =
{
  1, // JC_UNKNOWN = 0,
  1, // JC_GRAY    = 1,
  3, // JC_RGB     = 2,
  3, // JC_BGR     = 3,
  2, // JC_YCBCR   = 4,
  4, // JC_CMYK    = 5,
  4, // JC_YCCK    = 6,
  4, // JC_BGRA    = 7,
  4, // JC_RGBA    = 8,

  1, // JC_IMC3    = 9,
  1 //JC_NV12    = 10
};

#define iRY  0x00004c8b
#define iGY  0x00009646
#define iBY  0x00001d2f
#define iRu  0x00002b33
#define iGu  0x000054cd
#define iBu  0x00008000
#define iGv  0x00006b2f
#define iBv  0x000014d1

JERRCODE CJPEGDecoder::ColorConvert(Ipp32u rowMCU, Ipp32u colMCU, Ipp32u maxMCU)
{
  int       cc_h;
  IppiSize  roi;
  IppStatus status;
  Ipp32u bpp;

  cc_h = m_curr_scan->mcuHeight * m_curr_scan->min_v_factor;
  if (rowMCU == m_curr_scan->numyMCU - 1)
  {
    cc_h -= m_curr_scan->yPadding;
  }
  roi.height = (cc_h + m_dd_factor - 1) / m_dd_factor;

  roi.width  = (maxMCU - colMCU) * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
  if (maxMCU == m_curr_scan->numxMCU)
  {
      roi.width -= m_curr_scan->xPadding;
  }

  if(roi.height == 0)
    return JPEG_OK;


  if(JC_RGB == m_jpeg_color && (JC_BGRA == m_dst.color || m_jpeg_ncomp != m_curr_scan->ncomps))
  {
      int     srcStep;
      Ipp8u*  pSrc8u[3];
      int     dstStep;
      Ipp8u*  pDst8u;

      srcStep = m_ccomp[0].m_cc_step;

      pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
      pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
      pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;

      dstStep = m_dst.lineStep[0];
      bpp = JPEG_BPP[m_dst.color % JC_MAX];

      pDst8u   = m_dst.p.Data8u[0] + 
          rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep / m_dd_factor + 
          colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor * bpp;

      for(int n = m_curr_scan->first_comp; n < m_curr_scan->first_comp + m_curr_scan->ncomps; n++)
      {
          for(int i=0; i<roi.height; i++)
              for(int j=0; j<roi.width; j++)
              {
                  pDst8u[i*dstStep + j*4 + 2 - n] = pSrc8u[n][i*srcStep + j];
                  pDst8u[i*dstStep + j*4 + 3] = 0xFF;
              }
      }
  }
  else if (JC_YCBCR == m_jpeg_color && JC_NV12 == m_dst.color)
  {      
      int     srcStep[3];
      Ipp8u*  pSrc8u[3];
      int     dstStep[2];
      Ipp8u*  pDst8u[2];

      srcStep[0] = m_ccomp[0].m_cc_step;
      srcStep[1] = m_ccomp[1].m_cc_step;
      srcStep[2] = m_ccomp[2].m_cc_step;

      pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
      pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor / 2;
      pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor / 2;

      dstStep[0] = m_dst.lineStep[0];
      dstStep[1] = m_dst.lineStep[1];

      pDst8u[0]   = m_dst.p.Data8u[0] + 
          rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep[0] / m_dd_factor + 
          colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
      pDst8u[1]   = m_dst.p.Data8u[1] + 
          rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep[1] / (2 * m_dd_factor) + 
          colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;

      for(int n = m_curr_scan->first_comp; n < m_curr_scan->first_comp + m_curr_scan->ncomps; n++)
      {
          if(n == 0)
          {
              for(int i=0; i<roi.height; i++)
                  for(int j=0; j<roi.width; j++)
                  {
                      pDst8u[0][i*dstStep[0] + j] = pSrc8u[0][i*srcStep[0] + j];
                  }
          }
          else
          {
              for(int i=0; i < roi.height >> 1; i++)
                  for(int j=0; j < roi.width  >> 1; j++)
                  {
                      pDst8u[1][i*dstStep[1] + j * 2 + n - 1] = pSrc8u[n][i*srcStep[n] + j];
                  }
          }
      }
  }
  else if(JC_YCBCR == m_jpeg_color && JC_YCBCR == m_dst.color && JS_444 == m_dst.sampling && m_jpeg_ncomp != m_curr_scan->ncomps)
  {
      int     srcStep[3];
      Ipp8u*  pSrc8u[3];
      int     dstStep[3];
      Ipp8u*  pDst8u[3];

      srcStep[0] = m_ccomp[0].m_cc_step;
      srcStep[1] = m_ccomp[0].m_cc_step;
      srcStep[2] = m_ccomp[0].m_cc_step;

      pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
      pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
      pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;

      dstStep[0] = m_dst.lineStep[0];
      dstStep[1] = m_dst.lineStep[1];
      dstStep[2] = m_dst.lineStep[2];

      pDst8u[0] = m_dst.p.Data8u[0] + 
          rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep[0] / m_dd_factor + 
          colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
      pDst8u[1] = m_dst.p.Data8u[1] + 
          rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep[1] / m_dd_factor + 
          colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
      pDst8u[2] = m_dst.p.Data8u[2] + 
          rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep[2] / m_dd_factor + 
          colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;

      for(int n = m_curr_scan->first_comp; n < m_curr_scan->first_comp + m_curr_scan->ncomps; n++)
      {
          for(int i=0; i<roi.height; i++)
              for(int j=0; j<roi.width; j++)
              {
                  pDst8u[n][i*dstStep[n] + j] = pSrc8u[n][i*srcStep[n] + j];
              }
      }
  }
  else if(JC_GRAY == m_jpeg_color && m_dst.color == JC_BGRA)
  {
      int    srcStep;
      Ipp8u* pSrc8u;
      int    dstStep;
      Ipp8u* pDst8u;

      srcStep = m_ccomp[0].m_cc_step;

      pSrc8u = m_ccomp[0].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth;

      dstStep = m_dst.lineStep[0];
      bpp = JPEG_BPP[m_dst.color % JC_MAX];
    
      pDst8u   = m_dst.p.Data8u[0] + 
          rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep / m_dd_factor + 
          colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor * bpp;

      for(int i=0; i<roi.height; i++)
          for(int j=0; j<roi.width; j++)
          {
              pDst8u[i*dstStep + j*4 + 0] = pSrc8u[i*srcStep + j];
              pDst8u[i*dstStep + j*4 + 1] = pSrc8u[i*srcStep + j];
              pDst8u[i*dstStep + j*4 + 2] = pSrc8u[i*srcStep + j];
              pDst8u[i*dstStep + j*4 + 3] = 0xFF;
          }
  }
  else if(JC_GRAY == m_jpeg_color && m_dst.color == JC_NV12)
  {
      int    srcStep;
      Ipp8u* pSrc8u; 
      int     dstStep[2];
      Ipp8u*  pDst8u[2];

      srcStep = m_ccomp[0].m_cc_step;
      pSrc8u = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);

      dstStep[0] = m_dst.lineStep[0];
      dstStep[1] = m_dst.lineStep[1];

      pDst8u[0]   = m_dst.p.Data8u[0] + 
          rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep[0] / m_dd_factor + 
          colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
      pDst8u[1]   = m_dst.p.Data8u[1] + 
          rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep[1] / (2 * m_dd_factor) + 
          colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;

      for(int i=0; i<roi.height >> 1; i++)
          for(int j=0; j<roi.width; j++)
          {
              pDst8u[0][ (i<<1)   *dstStep[0] + j] = pSrc8u[ (i<<1)   *srcStep + j];
              pDst8u[0][((i<<1)+1)*dstStep[0] + j] = pSrc8u[((i<<1)+1)*srcStep + j];

              pDst8u[1][i*dstStep[1] + j] = 0x80;
          }
  }
  else if(m_jpeg_ncomp == m_curr_scan->ncomps)
  {
      if (JC_RGB == m_jpeg_color && JC_NV12 == m_dst.color)
      {
          int     srcStep;
          Ipp8u*  pSrc8u[3];
          int     dstStep[2];
          Ipp8u*  pDst8u[2];
          int     r0,r1,r2,r3, g0,g1,g2,g3, b0,b1,b2,b3;

          srcStep = m_ccomp[0].m_cc_step;

          pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth;
          pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth;
          pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth;

          dstStep[0] = m_dst.lineStep[0];
          dstStep[1] = m_dst.lineStep[1];

          pDst8u[0]   = m_dst.p.Data8u[0] + 
              rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep[0] / m_dd_factor + 
              colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;
          pDst8u[1]   = m_dst.p.Data8u[1] + 
              rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep[1] / (2 * m_dd_factor) + 
              colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor;

          for(int i=0; i<roi.height >> 1; i++)
              for(int j=0; j<roi.width >> 1; j++)
          {
              r0 = pSrc8u[0][ (i<<1)   *srcStep + (j<<1)  ];
              g0 = pSrc8u[1][ (i<<1)   *srcStep + (j<<1)  ];
              b0 = pSrc8u[2][ (i<<1)   *srcStep + (j<<1)  ];              
              r1 = pSrc8u[0][ (i<<1)   *srcStep + (j<<1)+1];
              g1 = pSrc8u[1][ (i<<1)   *srcStep + (j<<1)+1];
              b1 = pSrc8u[2][ (i<<1)   *srcStep + (j<<1)+1];
              r2 = pSrc8u[0][((i<<1)+1)*srcStep + (j<<1)  ];
              g2 = pSrc8u[1][((i<<1)+1)*srcStep + (j<<1)  ];
              b2 = pSrc8u[2][((i<<1)+1)*srcStep + (j<<1)  ];
              r3 = pSrc8u[0][((i<<1)+1)*srcStep + (j<<1)+1];
              g3 = pSrc8u[1][((i<<1)+1)*srcStep + (j<<1)+1];
              b3 = pSrc8u[2][((i<<1)+1)*srcStep + (j<<1)+1];

              pDst8u[0][ (i<<1)   *dstStep[0] + (j<<1)  ] = (Ipp8u)(( iRY*r0 + iGY*g0 + iBY*b0 + 0x008000) >> 16);
              pDst8u[0][ (i<<1)   *dstStep[0] + (j<<1)+1] = (Ipp8u)(( iRY*r1 + iGY*g1 + iBY*b1 + 0x008000) >> 16);
              pDst8u[0][((i<<1)+1)*dstStep[0] + (j<<1)  ] = (Ipp8u)(( iRY*r2 + iGY*g2 + iBY*b2 + 0x008000) >> 16);
              pDst8u[0][((i<<1)+1)*dstStep[0] + (j<<1)+1] = (Ipp8u)(( iRY*r3 + iGY*g3 + iBY*b3 + 0x008000) >> 16);

              r0 = r0+r1+r2+r3;
              g0 = g0+g1+g2+g3;
              b0 = b0+b1+b2+b3;

              pDst8u[1][i*dstStep[1] + (j<<1)  ] = (Ipp8u)((-iRu*r0 - iGu*g0 + iBu*b0 + 0x2000000) >> 18);
              pDst8u[1][i*dstStep[1] + (j<<1)+1] = (Ipp8u)(( iBu*r0 - iGv*g0 - iBv*b0 + 0x2000000) >> 18);
          }
      }
      else if (JC_YCBCR == m_jpeg_color && JC_BGRA == m_dst.color)
      {
          int     srcStep;
          const Ipp8u*  pSrc8u[3];
          int     dstStep;
          Ipp8u*  pDst8u;

          srcStep = m_ccomp[0].m_cc_step;

          pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth;
          pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth;
          pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (0) + colMCU * m_curr_scan->mcuWidth;

          dstStep = m_dst.lineStep[0];
          bpp = JPEG_BPP[m_dst.color % JC_MAX];

          pDst8u   = m_dst.p.Data8u[0] + rowMCU * m_curr_scan->mcuHeight * m_curr_scan->min_v_factor * dstStep / m_dd_factor + 
              colMCU * m_curr_scan->mcuWidth * m_curr_scan->min_h_factor * bpp;

          status = ippiYCbCrToBGR_JPEG_8u_P3C4R(pSrc8u, srcStep, pDst8u, dstStep, roi, 0xFF);

          if(ippStsNoErr != status)
          {
              LOG1("IPP Error: ippiYCbCrToBGR_JPEG_8u_P3C3R() failed - ",status);
              return JPEG_ERR_INTERNAL;
          }
      }
  }

  return JPEG_OK;

#if 0
  int       dstStep;
  int       cc_h;
  Ipp8u*    pDst8u  = 0;
  Ipp16u*   pDst16u = 0;
  IppiSize  roi;
  IppStatus status;
  const Ipp32u thread_id = 0;
  Ipp32u bpp;

  cc_h = m_curr_scan->mcuHeight;

  if (rowMCU == m_curr_scan->numyMCU - 1)
  {
    cc_h = m_curr_scan->mcuHeight - m_curr_scan->yPadding;
  }

  roi.width  = (maxMCU - colMCU) * m_curr_scan->mcuWidth;
  if (maxMCU == m_curr_scan->numxMCU)
  {
      roi.width -= m_curr_scan->xPadding;
  }
  roi.height = (cc_h + m_dd_factor - 1) / m_dd_factor;

  if(roi.height == 0)
    return JPEG_OK;

  dstStep = m_dst.lineStep[0];
  bpp = JPEG_BPP[m_dst.color % JC_MAX];

  if(m_dst.precision <= 8)
    pDst8u   = m_dst.p.Data8u[0] + rowMCU * m_curr_scan->mcuHeight * dstStep / m_dd_factor + colMCU * m_curr_scan->mcuWidth * bpp;
  else
    pDst16u  = (Ipp16u*)((Ipp8u*)m_dst.p.Data16s[0] + rowMCU * m_curr_scan->mcuHeight * dstStep + colMCU * m_curr_scan->mcuWidth * bpp);

  if(m_jpeg_color == JC_UNKNOWN && m_dst.color == JC_UNKNOWN)
  {
    switch(m_jpeg_ncomp)
    {
    case 1:
      {
        int     srcStep;
        Ipp8u*  pSrc8u;
        Ipp16u* pSrc16u;

        srcStep = m_ccomp[0].m_cc_step;

        if(m_dst.precision <= 8)
        {
          pSrc8u = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);

          status = ippiCopy_8u_C1R(pSrc8u,srcStep,pDst8u,dstStep,roi);
        }
        else
        {
          pSrc16u = m_ccomp[0].GetCCBufferPtr<Ipp16u> (colMCU);

          status = ippiCopy_16s_C1R((Ipp16s*)pSrc16u,srcStep,(Ipp16s*)pDst16u,dstStep,roi);
        }

        if(ippStsNoErr != status)
        {
          LOG1("IPP Error: ippiCopy_8u_C1R() failed - ",status);
          return JPEG_ERR_INTERNAL;
        }
      }
      break;

    case 3:
      {
        int     srcStep;
        Ipp8u*  pSrc8u[3];
        Ipp16u* pSrc16u[3];

        srcStep = m_ccomp[0].m_cc_step;

        if(m_dst.precision <= 8)
        {
          pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
          pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
          pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

          status = ippiCopy_8u_P3C3R(pSrc8u,srcStep,pDst8u,dstStep,roi);
        }
        else
        {
          pSrc16u[0] = m_ccomp[0].GetCCBufferPtr<Ipp16u> (colMCU);
          pSrc16u[1] = m_ccomp[1].GetCCBufferPtr<Ipp16u> (colMCU);
          pSrc16u[2] = m_ccomp[2].GetCCBufferPtr<Ipp16u> (colMCU);

          status = ippiCopy_16s_P3C3R((Ipp16s**)pSrc16u,srcStep,(Ipp16s*)pDst16u,dstStep,roi);
        }

        if(ippStsNoErr != status)
        {
          LOG1("IPP Error: ippiCopy_8u_P3C3R() failed - ",status);
          return JPEG_ERR_INTERNAL;
        }
      }
      break;

    case 4:
      {
        int     srcStep;
        Ipp8u*  pSrc8u[4];
        Ipp16u* pSrc16u[4];

        srcStep = m_ccomp[0].m_cc_step;

        if(m_dst.precision <= 8)
        {
          pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
          pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
          pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);
          pSrc8u[3] = m_ccomp[3].GetCCBufferPtr<Ipp8u> (colMCU);

          status = ippiCopy_8u_P4C4R(pSrc8u,srcStep,pDst8u,dstStep,roi);
        }
        else
        {
          pSrc16u[0] = m_ccomp[0].GetCCBufferPtr<Ipp16u> (colMCU);
          pSrc16u[1] = m_ccomp[1].GetCCBufferPtr<Ipp16u> (colMCU);
          pSrc16u[2] = m_ccomp[2].GetCCBufferPtr<Ipp16u> (colMCU);
          pSrc16u[3] = m_ccomp[3].GetCCBufferPtr<Ipp16u> (colMCU);

          status = ippiCopy_16s_P4C4R((Ipp16s**)pSrc16u,srcStep,(Ipp16s*)pDst16u,dstStep,roi);
        }

        if(ippStsNoErr != status)
        {
          LOG1("IPP Error: ippiCopy_8u_P4C4R() failed - ",status);
          return JPEG_ERR_INTERNAL;
        }
      }
      break;

    default:
      return JPEG_NOT_IMPLEMENTED;
    }
  }

  // Gray to Gray
  if(m_jpeg_color == JC_GRAY && m_dst.color == JC_GRAY)
  {
    int     srcStep;
    Ipp8u*  pSrc8u;
    Ipp16u* pSrc16u;

    srcStep = m_ccomp[0].m_cc_step;

    if(m_dst.precision <= 8)
    {
      pSrc8u = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);

      status = ippiCopy_8u_C1R(pSrc8u,srcStep,pDst8u,dstStep,roi);
    }
    else
    {
      pSrc16u = m_ccomp[0].GetCCBufferPtr<Ipp16u> (colMCU);

      status = ippiCopy_16s_C1R((Ipp16s*)pSrc16u,srcStep,(Ipp16s*)pDst16u,dstStep,roi);
    }

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiCopy_8u_C1R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // Gray to RGB
  if(m_jpeg_color == JC_GRAY && m_dst.color == JC_RGB)
  {
    int    srcStep;
    Ipp8u* pSrc8u[3];

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);

    status = ippiCopy_8u_P3C3R(pSrc8u,srcStep,pDst8u,dstStep,roi);

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiCopy_8u_P3C3R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // Gray to BGR
  if(m_jpeg_color == JC_GRAY && m_dst.color == JC_BGR)
  {
    int    srcStep;
    Ipp8u* pSrc8u[3];

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);

    status = ippiCopy_8u_P3C3R(pSrc8u,srcStep,pDst8u,dstStep,roi);

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiCopy_8u_P3C3R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // Gray to YCbCr
  if(m_jpeg_color == JC_GRAY && m_dst.color == JC_YCBCR && 
      m_dst.sampling == JS_422H)
  {
    int    srcStep;
    Ipp8u* pSrc8u; 

    srcStep = m_ccomp[0].m_cc_step;
    pSrc8u = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);

    for(int i=0; i<roi.height; i++)
        for(int j=0; j<roi.width >> 1; j++)
        {
            pDst8u[i*dstStep + (j<<2)  ] = pSrc8u[i*srcStep + (j<<1)];
            pDst8u[i*dstStep + (j<<2)+1] = 0x80;
            pDst8u[i*dstStep + (j<<2)+2] = pSrc8u[i*srcStep + (j<<1)+1];
            pDst8u[i*dstStep + (j<<2)+3] = 0x80;
        }
  }

  // RGB to RGB
  if(m_jpeg_color == JC_RGB && m_dst.color == JC_RGB)
  {
    int     srcStep;
    Ipp8u*  pSrc8u[3];
    Ipp16u* pSrc16u[3];

    srcStep = m_ccomp[0].m_cc_step;

    if(m_dst.precision <= 8)
    {
      pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
      pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
      pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

      status = ippiCopy_8u_P3C3R(pSrc8u,srcStep,pDst8u,dstStep,roi);
    }
    else
    {
      pSrc16u[0] = m_ccomp[0].GetCCBufferPtr<Ipp16u> (colMCU);
      pSrc16u[1] = m_ccomp[1].GetCCBufferPtr<Ipp16u> (colMCU);
      pSrc16u[2] = m_ccomp[2].GetCCBufferPtr<Ipp16u> (colMCU);

      status = ippiCopy_16s_P3C3R((Ipp16s**)pSrc16u,srcStep,(Ipp16s*)pDst16u,dstStep,roi);
    }

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiCopy_8u_P3C3R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // RGB to BGR
  if(m_jpeg_color == JC_RGB && m_dst.color == JC_BGR)
  {
    int     srcStep;
    Ipp8u*  pSrc8u[3];
    Ipp16u* pSrc16u[3];

    srcStep = m_ccomp[0].m_cc_step;

    if(m_dst.precision <= 8)
    {
      pSrc8u[2] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
      pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
      pSrc8u[0] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

      status = ippiCopy_8u_P3C3R(pSrc8u,srcStep,pDst8u,dstStep,roi);
    }
    else
    {
      pSrc16u[2] = m_ccomp[0].GetCCBufferPtr<Ipp16u> (colMCU);
      pSrc16u[1] = m_ccomp[1].GetCCBufferPtr<Ipp16u> (colMCU);
      pSrc16u[0] = m_ccomp[2].GetCCBufferPtr<Ipp16u> (colMCU);

      status = ippiCopy_16s_P3C3R((Ipp16s**)pSrc16u,srcStep,(Ipp16s*)pDst16u,dstStep,roi);
    }

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiCopy_8u_P3C3R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // RGB to YCbCr
  if(m_jpeg_color == JC_RGB && m_dst.color == JC_YCBCR &&
      m_dst.sampling == JS_422H)
  {
    int srcStep;
    const Ipp8u* pSrc8u[3];
    int g,g1; int r,r1; int b,b1;

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

    for(int i=0; i<roi.height; i++)
        for(int j=0; j<roi.width >> 1; j++)
        {
            b = pSrc8u[2][i*srcStep + (j<<1)];
            g = pSrc8u[1][i*srcStep + (j<<1)];
            r = pSrc8u[0][i*srcStep + (j<<1)];
            b1= pSrc8u[2][i*srcStep + (j<<1)+1];
            g1= pSrc8u[1][i*srcStep + (j<<1)+1];
            r1= pSrc8u[0][i*srcStep + (j<<1)+1];

            pDst8u[i*dstStep + (j<<2)  ] = (Ipp8u)(( iRY*r + iGY*g + iBY*b + 0x008000) >> 16);
            pDst8u[i*dstStep + (j<<2)+2] = (Ipp8u)(( iRY*r1  + iGY*g1 + iBY*b1 + 0x008000) >> 16);

            r += r1;b += b1;g += g1;
            pDst8u[i*dstStep + (j<<2)+1] = (Ipp8u)((-iRu*r - iGu*g + iBu*b + 0x1000000) >> 17);
            pDst8u[i*dstStep + (j<<2)+3] = (Ipp8u)((iBu*r - iGv*g - iBv*b + 0x1000000) >> 17);
        }
  }

  // YCbCr to Gray
  if(m_jpeg_color == JC_YCBCR && m_dst.color == JC_GRAY)
  {
    int    srcStep;
    Ipp8u* pSrc8u;

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);

    status = ippiCopy_8u_C1R(pSrc8u,srcStep,pDst8u,dstStep,roi);

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiCopy_8u_C1R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // YCbCr to RGB
  if(m_jpeg_color == JC_YCBCR && m_dst.color == JC_RGB)
  {
    int srcStep;
    const Ipp8u* pSrc8u[3];

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

    status = ippiYCbCrToRGB_JPEG_8u_P3C3R(pSrc8u,srcStep,pDst8u,dstStep,roi);

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiYCbCrToRGB_JPEG_8u_P3C3R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // YCbCr to BGR
  if(m_jpeg_color == JC_YCBCR && m_dst.color == JC_BGR)
  {
    int srcStep;
    const Ipp8u* pSrc8u[3];

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

    status = ippiYCbCrToBGR_JPEG_8u_P3C3R(pSrc8u,srcStep,pDst8u,dstStep,roi);

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiYCbCrToBGR_JPEG_8u_P3C3R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // YCbCr to RGBA (with alfa channel set to 0xFF)
  if(m_jpeg_color == JC_YCBCR && m_dst.color == JC_RGBA)
  {
    int srcStep;
    const Ipp8u* pSrc8u[3];

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

    status = ippiYCbCrToRGB_8u_P3C4R(pSrc8u,srcStep,pDst8u,dstStep,roi, 0xFF);

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiYCbCrToRGB_8u_P3C4R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // YCbCr to BGRA (with alfa channel set to 0xFF)
  if(m_jpeg_color == JC_YCBCR && m_dst.color == JC_BGRA)
  {
    int srcStep;
    const Ipp8u* pSrc8u[3];

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

    status = ippiYCbCrToBGR_8u_P3C4R(pSrc8u,srcStep,pDst8u,dstStep,roi, 0xFF);

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiYCbCrToBGR_8u_P3C4R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // YCbCr to YCbCr (422H sampling)
  if(m_jpeg_color == JC_YCBCR && m_dst.color == JC_YCBCR &&
     m_jpeg_sampling == JS_422H && m_dst.sampling == JS_422H)
  {
    int    srcStep[3];
    const Ipp8u* pSrc8u[3];

    srcStep[0] = m_ccomp[0].m_cc_step;
    srcStep[1] = m_ccomp[1].m_cc_step;
    srcStep[2] = m_ccomp[2].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

    status = ippiYCbCr422_8u_P3C2R(pSrc8u,srcStep,pDst8u,dstStep,roi);

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiYCbCr422_8u_P3C2R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // YCbCr to YCbCr (not 422H sampling)
  if(m_jpeg_color == JC_YCBCR && m_dst.color == JC_YCBCR &&
     m_jpeg_sampling != JS_422H && m_dst.sampling == JS_422H)
  {
    int    srcStep;
    const Ipp8u* pSrc8u[3];

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

    for(int i=0; i<roi.height; i++)
        for(int j=0; j<roi.width >> 1; j++)
        {
            pDst8u[i*dstStep + (j<<2)  ] = pSrc8u[0][i*srcStep + (j<<1)];
            pDst8u[i*dstStep + (j<<2)+1] = ((pSrc8u[1][i*srcStep + (j<<1)] + pSrc8u[1][i*srcStep + (j<<1)+1] + 1) >> 1);            
            pDst8u[i*dstStep + (j<<2)+2] = pSrc8u[0][i*srcStep + (j<<1)+1];
            pDst8u[i*dstStep + (j<<2)+3] = ((pSrc8u[2][i*srcStep + (j<<1)] + pSrc8u[2][i*srcStep + (j<<1)+1] + 1) >> 1);  
        }
  }

  // CMYK to CMYK
  if(m_jpeg_color == JC_CMYK && m_dst.color == JC_CMYK)
  {
    int    srcStep;
    Ipp8u* pSrc8u[4];

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[3] = m_ccomp[3].GetCCBufferPtr<Ipp8u> (colMCU);

    status = ippiCopy_8u_P4C4R(pSrc8u,srcStep,pDst8u,dstStep,roi);

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiCopy_8u_P4C4R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  // YCCK to CMYK
  if(m_jpeg_color == JC_YCCK && m_dst.color == JC_CMYK)
  {
    int    srcStep;
    const Ipp8u* pSrc8u[4];

    srcStep = m_ccomp[0].m_cc_step;

    pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);
    pSrc8u[3] = m_ccomp[3].GetCCBufferPtr<Ipp8u> (colMCU);

    status = ippiYCCKToCMYK_JPEG_8u_P4C4R(pSrc8u,srcStep,pDst8u,dstStep,roi);

    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiYCCKToCMYK_JPEG_8u_P4C4R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }

  if (m_dst.color == JC_IMC3)
  {
      switch (m_jpeg_color)
      {
      case JC_YCBCR:
          {
            int    srcStep[3];
            const Ipp8u* pSrc8u[3];

            srcStep[0] = m_ccomp[0].m_cc_step;
            srcStep[1] = m_ccomp[1].m_cc_step;
            srcStep[2] = m_ccomp[2].m_cc_step;

            pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
            pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
            pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

            int    dstStep[3];
            Ipp8u* pDst8u[3];

            dstStep[0] = m_dst.lineStep[0];
            dstStep[1] = m_dst.lineStep[1];
            dstStep[2] = m_dst.lineStep[2];

            size_t offset = rowMCU * m_mcuHeight * dstStep[0] / m_dd_factor + colMCU * m_mcuWidth;
            pDst8u[0] = m_dst.p.Data8u[0] + offset;
            pDst8u[1] = m_dst.p.Data8u[1] + (offset >> 1);
            pDst8u[2] = m_dst.p.Data8u[2] + (offset >> 1);

            status = ippiYCbCr422ToYCbCr420_8u_P3R(pSrc8u, srcStep, pDst8u, dstStep, roi);

            if(ippStsNoErr != status)
            {
              LOG1("IPP Error: ippiYCbCr422_8u_P3C2R() failed - ",status);
              return JPEG_ERR_INTERNAL;
            }
          }
          break;
      default:
          return JPEG_ERR_INTERNAL;
      }
  }

  if (m_dst.color == JC_NV12)
  {
      ChromaType color = GetChromaType();
      switch (color)
      {
      case CHROMA_TYPE_YUV422H_2Y:
          {
            int    srcStep[3];
            const Ipp8u* pSrc8u[3];

            srcStep[0] = m_ccomp[0].m_cc_step;
            srcStep[1] = m_ccomp[1].m_cc_step;
            srcStep[2] = m_ccomp[2].m_cc_step;

            pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
            pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
            pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

            int    dstStep[2];
            Ipp8u* pDst8u[2];

            dstStep[0] = m_dst.lineStep[0];
            dstStep[1] = m_dst.lineStep[1];

            size_t offset = rowMCU * m_mcuHeight * dstStep[0] / m_dd_factor + colMCU * m_mcuWidth;
            pDst8u[0] = m_dst.p.Data8u[0] + offset;
            pDst8u[1] = m_dst.p.Data8u[1] + (offset >> 1);

            status = ippiYCbCr422ToYCbCr420_8u_P3P2R(pSrc8u, srcStep, pDst8u[0], dstStep[0], pDst8u[1], dstStep[1], roi);

            if(ippStsNoErr != status)
            {
              LOG1("IPP Error: ippiYCbCr422_8u_P3C2R() failed - ",status);
              return JPEG_ERR_INTERNAL;
            }
          }
          break;
      case CHROMA_TYPE_YUV444:
          {
            Ipp32u    srcStep[3];
            const Ipp8u* pSrc8u[3];

            srcStep[0] = m_ccomp[0].m_cc_step;
            srcStep[1] = m_ccomp[1].m_cc_step;
            srcStep[2] = m_ccomp[2].m_cc_step;

            pSrc8u[0] = m_ccomp[0].GetCCBufferPtr<Ipp8u> (colMCU);
            pSrc8u[1] = m_ccomp[1].GetCCBufferPtr<Ipp8u> (colMCU);
            pSrc8u[2] = m_ccomp[2].GetCCBufferPtr<Ipp8u> (colMCU);

            int    dstStep[2];
            Ipp8u* pDst8u[2];

            dstStep[0] = m_dst.lineStep[0];
            dstStep[1] = m_dst.lineStep[1];

            size_t offset = rowMCU * m_mcuHeight * dstStep[0] / m_dd_factor + colMCU * m_mcuWidth;
            pDst8u[0] = m_dst.p.Data8u[0] + offset;
            pDst8u[1] = m_dst.p.Data8u[1] + (offset >> 1);

            ConvertFrom_YUV444_To_YV12(pSrc8u, srcStep[0], pDst8u, dstStep[0], roi);

            /*if(ippStsNoErr != status)
            {
              LOG1("IPP Error: ippiYCbCr422_8u_P3C2R() failed - ",status);
              return JPEG_ERR_INTERNAL;
            }*/
          }
          break;

      case CHROMA_TYPE_YUV420:
      default:
          {
            int    srcStep[3];
            const Ipp8u* pSrc8u[3];

            srcStep[0] = m_ccomp[0].m_cc_step;
            srcStep[1] = m_ccomp[1].m_ss_step;
            srcStep[2] = m_ccomp[2].m_ss_step;

            pSrc8u[0] = m_ccomp[0].GetCCBufferPtr(thread_id);
            pSrc8u[1] = m_ccomp[1].GetSSBufferPtr(thread_id) + m_ccomp[1].m_ss_step;
            pSrc8u[2] = m_ccomp[2].GetSSBufferPtr(thread_id) + m_ccomp[2].m_ss_step;

            int    dstStep[2];
            Ipp8u* pDst8u[2];

            dstStep[0] = m_dst.lineStep[0];
            dstStep[1] = m_dst.lineStep[1];

            size_t offset = rowMCU * m_mcuHeight * dstStep[0] / m_dd_factor + colMCU * m_mcuWidth;
            pDst8u[0] = m_dst.p.Data8u[0] + offset;
            pDst8u[1] = m_dst.p.Data8u[1] + (offset >> 1);

            status = ippiYCbCr420_8u_P3P2R(pSrc8u, srcStep, pDst8u[0], dstStep[0], pDst8u[1], dstStep[1], roi);

            if(ippStsNoErr != status)
            {
              LOG1("IPP Error: ippiYCbCr422_8u_P3C2R() failed - ",status);
              return JPEG_ERR_INTERNAL;
            }
          }
          break;
      }
  }

  return JPEG_OK;
#endif

} // JERRCODE CJPEGDecoder::ColorConvert(Ipp32u rowCMU, Ipp32u colMCU, Ipp32u maxMCU)


JERRCODE CJPEGDecoder::UpSampling(Ipp32u rowMCU, Ipp32u colMCU, Ipp32u maxMCU)
{
  int i, j, k, n, c;
  int need_upsampling;
  CJPEGColorComponent* curr_comp;
  IppStatus status;
  rowMCU;

  // if image format is YCbCr and destination format is NV12 need special upsampling (see below)
  if(JC_YCBCR != m_jpeg_color || JC_NV12 != m_dst.color)
  {
      for(k = m_curr_scan->first_comp; k < m_curr_scan->first_comp + m_curr_scan->ncomps; k++)
      {
        curr_comp       = &m_ccomp[k];
        need_upsampling = curr_comp->m_need_upsampling;

        // sampling 444
        // nothing to do for 444

        // sampling 422H
        if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 1 && need_upsampling)
        {
          int    srcStep;
          int    dstStep;
          Ipp8u* pSrc;
          Ipp8u* pDst;
          Ipp32u srcWidth, intervalSize, tileSize, pixelToProcess;

          need_upsampling = 0;

          srcWidth = (maxMCU - colMCU) * 8 * curr_comp->m_scan_hsampling;
          srcStep = curr_comp->m_ss_step;
          dstStep = curr_comp->m_cc_step;

          // set the pointer to source buffer
          pSrc = curr_comp->GetSSBufferPtr<Ipp8u> (0) + 8 * colMCU * curr_comp->m_scan_hsampling;
          // set the pointer to destination buffer
          pDst = curr_comp->GetCCBufferPtr<Ipp8u> (0) + 8 * colMCU * curr_comp->m_h_factor;

          intervalSize = m_curr_scan->jpeg_restart_interval ? 
                         m_curr_scan->jpeg_restart_interval * 8 * curr_comp->m_scan_hsampling : 
                         srcWidth / m_dd_factor;

          tileSize =  (m_curr_scan->jpeg_restart_interval && !colMCU) ? 
                      (m_curr_scan->jpeg_restart_interval - (m_curr_scan->numxMCU * rowMCU) % m_curr_scan->jpeg_restart_interval)* 8 * curr_comp->m_scan_hsampling :
                      intervalSize;

          for(i = 0; i < m_mcuHeight / m_dd_factor; i++)
          {
              j = 0;
              pixelToProcess = IPP_MIN(tileSize, srcWidth / m_dd_factor);
              while(j < (int) srcWidth / m_dd_factor)
              {
                  status = ippiSampleUpRowH2V1_Triangle_JPEG_8u_C1(pSrc + j, pixelToProcess , pDst + j * 2);
                  if(ippStsNoErr != status)
                  {
                    LOG0("Error: ippiSampleUpRowH2V1_Triangle_JPEG_8u_C1() failed!");
                    return JPEG_ERR_INTERNAL;
                  }
                  j += pixelToProcess;
                  pixelToProcess = IPP_MIN(intervalSize, srcWidth / m_dd_factor - j);
              }

              pSrc += srcStep;
              pDst += dstStep;
          }
        }

        // sampling 422V
        if(curr_comp->m_h_factor == 1 && curr_comp->m_v_factor == 2 && need_upsampling)
        {
          int    srcStep;
          Ipp8u* pSrc;
          Ipp8u* pDst;
          Ipp32u srcWidth;

          need_upsampling = 0;

          srcWidth = (maxMCU - colMCU) * 8 * curr_comp->m_hsampling;
          srcStep = curr_comp->m_ss_step;

          // set the pointer to source buffer
          pSrc = curr_comp->GetSSBufferPtr<Ipp8u> (0) + 8 * colMCU * curr_comp->m_scan_hsampling;
          // set the pointer to destination buffer
          pDst = curr_comp->GetCCBufferPtr<Ipp8u> (0) + 8 * colMCU * curr_comp->m_h_factor;

          for(i = 0; i < (m_mcuHeight >> 1) - 1; i++)
          {
            status = ippsCopy_8u(pSrc,pDst,srcWidth);
            if(ippStsNoErr != status)
            {
              LOG0("Error: ippsCopy_8u() failed!");
              return JPEG_ERR_INTERNAL;
            }
            pDst += srcStep; // dstStep is the same as srcStep

            for(n = 0; n < (int)srcWidth; n++)
                pDst[n] = (pSrc[n] + pSrc[srcStep + n]) >> 1;

            pDst += srcStep; // dstStep is the same as srcStep
            pSrc += srcStep;
          }

          status = ippsCopy_8u(pSrc,pDst,srcWidth);
          pDst += srcStep;

          status = ippsCopy_8u(pSrc,pDst,srcWidth);
        }

        // sampling 420
        if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2 && need_upsampling)
        {
          int    ddShift;
          int    srcStep;
          int    dstStep;
          Ipp8u* pSrc;
          Ipp8u* pDst;
          Ipp32u srcWidth, intervalSize, tileSize, pixelToProcess;

          need_upsampling = 0;

          srcWidth = (maxMCU - colMCU) * 8 * curr_comp->m_scan_hsampling;
          srcStep = curr_comp->m_ss_step;
          dstStep = curr_comp->m_cc_step;

          // set the pointer to source buffer
          pSrc = curr_comp->GetSSBufferPtr<Ipp8u> (0) + 8 * colMCU * curr_comp->m_scan_hsampling;
          // set the pointer to destination buffer
          pDst = curr_comp->GetCCBufferPtr<Ipp8u> (0) + 8 * colMCU * curr_comp->m_h_factor;

          intervalSize = m_curr_scan->jpeg_restart_interval ? 
                         m_curr_scan->jpeg_restart_interval * 8 * curr_comp->m_scan_hsampling : 
                         srcWidth / m_dd_factor;

          tileSize =  (m_curr_scan->jpeg_restart_interval && !colMCU) ? 
                      (m_curr_scan->jpeg_restart_interval - (m_curr_scan->numxMCU * rowMCU) % m_curr_scan->jpeg_restart_interval)* 8 * curr_comp->m_scan_hsampling :
                      intervalSize;

          // filling the zero row
          j = 0;
          pixelToProcess = IPP_MIN(tileSize, srcWidth / m_dd_factor);
          while(j < (int) srcWidth / m_dd_factor)
          {
              status = ippiSampleUpRowH2V2_Triangle_JPEG_8u_C1(pSrc + j, pSrc + j, pixelToProcess, pDst + j * 2);    
              if(ippStsNoErr != status)
              {
                LOG0("Error: ippiSampleUpRowH2V2_Triangle_JPEG_8u_C1() failed!");
                return JPEG_ERR_INTERNAL;
              }
              j += pixelToProcess;
              pixelToProcess = IPP_MIN(intervalSize, srcWidth / m_dd_factor - j);
          }
          pDst += dstStep;

          ddShift = (m_dd_factor == 1) ? 1 : (m_dd_factor == 2) ? 2 : (m_dd_factor == 4) ? 3 : 4;
          for(i = 0; i < (m_mcuHeight >> ddShift) - 1; i++)
          {
              // filling odd rows
              j = 0;
              pixelToProcess = IPP_MIN(tileSize, srcWidth / m_dd_factor);
              while(j < (int) srcWidth / m_dd_factor)
              {
                  status = ippiSampleUpRowH2V2_Triangle_JPEG_8u_C1(pSrc + j, pSrc + srcStep + j, pixelToProcess, pDst + j * 2);    
                  if(ippStsNoErr != status)
                  {
                    LOG0("Error: ippiSampleUpRowH2V2_Triangle_JPEG_8u_C1() failed!");
                    return JPEG_ERR_INTERNAL;
                  }
                  j += pixelToProcess;
                  pixelToProcess = IPP_MIN(intervalSize, srcWidth / m_dd_factor - j);
              }
              pDst += dstStep;

              // filling even rows
              j = 0;
              pixelToProcess = IPP_MIN(tileSize, srcWidth / m_dd_factor);
              while(j < (int) srcWidth / m_dd_factor)
              {
                  status = ippiSampleUpRowH2V2_Triangle_JPEG_8u_C1(pSrc + srcStep + j, pSrc + j, pixelToProcess, pDst + j * 2);    
                  if(ippStsNoErr != status)
                  {
                    LOG0("Error: ippiSampleUpRowH2V2_Triangle_JPEG_8u_C1() failed!");
                    return JPEG_ERR_INTERNAL;
                  }
                  j += pixelToProcess;
                  pixelToProcess = IPP_MIN(intervalSize, srcWidth / m_dd_factor - j);
              }
              pDst += dstStep;
              pSrc += srcStep;
          }

          // filling the last row
          j = 0;
          pixelToProcess = IPP_MIN(tileSize, srcWidth / m_dd_factor);
          while(j < (int) srcWidth / m_dd_factor)
          {
              status = ippiSampleUpRowH2V2_Triangle_JPEG_8u_C1(pSrc + j, pSrc + j, pixelToProcess, pDst + j * 2);    
              if(ippStsNoErr != status)
              {
                LOG0("Error: ippiSampleUpRowH2V2_Triangle_JPEG_8u_C1() failed!");
                return JPEG_ERR_INTERNAL;
              }
              j += pixelToProcess;
              pixelToProcess = IPP_MIN(intervalSize, srcWidth / m_dd_factor - j);
          }
        } // 420

        // sampling 411
        if(curr_comp->m_h_factor == 4 && curr_comp->m_v_factor == 1 && need_upsampling)
        {
          int    srcStep;
          int    dstStep;
          Ipp8u* pSrc;
          Ipp8u* pTmp;
          Ipp8u* pDst;
          Ipp32u srcWidth, intervalSize, tileSize, pixelToProcess;

          need_upsampling = 0;

          srcWidth = (maxMCU - colMCU) * 8 * curr_comp->m_scan_hsampling;
          srcStep = curr_comp->m_ss_step;
          dstStep = curr_comp->m_cc_step;

          // set the pointer to source buffer
          pSrc = curr_comp->GetSSBufferPtr<Ipp8u> (0) + 8 * colMCU * curr_comp->m_scan_hsampling;
          // set the pointer to temporary buffer
          pTmp = (Ipp8u*)malloc(2 * srcWidth / m_dd_factor);
          // set the pointer to destination buffer
          pDst = curr_comp->GetCCBufferPtr<Ipp8u> (0) + 8 * colMCU * curr_comp->m_h_factor;
         
          intervalSize = m_curr_scan->jpeg_restart_interval ? 
                         m_curr_scan->jpeg_restart_interval * 8 * curr_comp->m_scan_hsampling : 
                         srcWidth / m_dd_factor;

          tileSize =  (m_curr_scan->jpeg_restart_interval && !colMCU) ? 
                      (m_curr_scan->jpeg_restart_interval - (m_curr_scan->numxMCU * rowMCU) % m_curr_scan->jpeg_restart_interval)* 8 * curr_comp->m_scan_hsampling :
                      intervalSize;

          for(i = 0; i < m_mcuHeight / m_dd_factor; i++)
          {
              j = 0;
              pixelToProcess = IPP_MIN(tileSize, srcWidth / m_dd_factor);
              while(j < (int) srcWidth / m_dd_factor)
              {
                  status = ippiSampleUpRowH2V1_Triangle_JPEG_8u_C1(pSrc + j, pixelToProcess, pTmp + j * 2);
                  if(ippStsNoErr != status)
                  {
                    free(pTmp);
                    LOG0("Error: ippiSampleUpRowH2V1_Triangle_JPEG_8u_C1() failed!");
                    return JPEG_ERR_INTERNAL;
                  }
                  j += pixelToProcess;
                  pixelToProcess = IPP_MIN(intervalSize, srcWidth / m_dd_factor - j);
              }
              
              j = 0;
              pixelToProcess = IPP_MIN(2 * tileSize, 2 * srcWidth / m_dd_factor);
              while(j < 2 * (int) srcWidth / m_dd_factor)
              {
                  status = ippiSampleUpRowH2V1_Triangle_JPEG_8u_C1(pTmp + j, pixelToProcess, pDst + j * 2);
                  if(ippStsNoErr != status)
                  {
                    free(pTmp);
                    LOG0("Error: ippiSampleUpRowH2V1_Triangle_JPEG_8u_C1() failed!");
                    return JPEG_ERR_INTERNAL;
                  }
                  j += pixelToProcess;
                  pixelToProcess = IPP_MIN(2 * intervalSize, 2 * srcWidth / m_dd_factor - j);
              }

              pSrc += srcStep;
              pDst += dstStep;
          }
          free(pTmp);

        } // 411

        // arbitrary sampling
        if((curr_comp->m_h_factor != 1 || curr_comp->m_v_factor != 1) && need_upsampling)
        {
          int    srcStep;
          int    dstStep;
          int    v_step;
          int    h_step;
          int    val;
          Ipp8u* pSrc;
          Ipp8u* pDst;
          Ipp8u* p;
          Ipp32u srcWidth;

          srcWidth = (maxMCU - colMCU) * 8 * curr_comp->m_scan_hsampling;
          srcStep = curr_comp->m_ss_step;
          dstStep = curr_comp->m_cc_step;

          h_step  = curr_comp->m_h_factor;
          v_step  = curr_comp->m_v_factor;

          // set the pointer to source buffer
          pSrc = curr_comp->GetSSBufferPtr<Ipp8u> (0) + 8 * colMCU * curr_comp->m_scan_hsampling;
          // set the pointer to destination buffer
          pDst = curr_comp->GetCCBufferPtr<Ipp8u> (0) + 8 * colMCU * curr_comp->m_h_factor;

          for(n = 0; n < curr_comp->m_ss_height; n++)
          {
            p = pDst;
            for(i = 0; i < (int) srcWidth; i++)
            {
              val = pSrc[i];
              for(j = 0; j < h_step; j++)
                pDst[j] = (Ipp8u)val;

              pDst += h_step;
            } //  for i

            for(c = 0; c < v_step - 1; c++)
            {
              status = ippsCopy_8u(p,pDst,srcWidth);
              if(ippStsNoErr != status)
              {
                LOG0("Error: ippsCopy_8u() failed!");
                return JPEG_ERR_INTERNAL;
              }

              pDst += dstStep;
            } //for c

            pDst = p + dstStep*v_step;

            pSrc += srcStep;
          } // for n
        } // if
      } // for m_jpeg_ncomp
  }
  else
  {
      // for all scan components 
      for(k = m_curr_scan->first_comp; k < m_curr_scan->first_comp + m_curr_scan->ncomps; k++)
      {
        curr_comp       = &m_ccomp[k];

        // downsampling for both dimentions
        if(JS_444 == m_jpeg_sampling && k != 0)
        {
            int    srcStep;
            int    tmpStep;
            Ipp8u* pSrc;
            Ipp8u* pTmp;
            Ipp8u* pDst;
            IppiSize srcRoiSize;
            IppiSize tmpRoiSize;

            srcStep = curr_comp->m_cc_step;
            tmpStep = srcStep;            

            // set the pointer to source buffer
            pSrc = curr_comp->GetCCBufferPtr<Ipp8u> (0) + 8 * colMCU;
            // set the pointer to temporary buffer
            pTmp = (Ipp8u*)malloc(tmpStep * m_curr_scan->mcuHeight / 2);
            // set the pointer to destination buffer
            pDst = curr_comp->GetCCBufferPtr<Ipp8u> (0) + 8 * colMCU / 2;  

            // set ROI of source
            srcRoiSize.width = (maxMCU - colMCU) * 8;
            srcRoiSize.height = m_curr_scan->mcuHeight;

            // set ROI of source
            tmpRoiSize.width = (maxMCU - colMCU) * 8 / 2;
            tmpRoiSize.height = m_curr_scan->mcuHeight / 2;

            status = ippiSampleDownH2V2_JPEG_8u_C1R(pSrc, srcStep, srcRoiSize, pTmp, tmpStep, tmpRoiSize);
            if(ippStsNoErr != status)
            {
                free(pTmp);
                LOG0("Error: ippiSampleDownH2V2_JPEG_8u_C1R() failed!");
                return JPEG_ERR_INTERNAL;
            }

            status = ippsCopy_8u(pTmp, pDst, tmpStep * m_curr_scan->mcuHeight / 2);
            if(ippStsNoErr != status)
            {
                free(pTmp);
                LOG0("Error: ippsCopy_8u() failed!");
                return JPEG_ERR_INTERNAL;
            }

            free(pTmp);
        }

        // vertical downsampling 
        if(JS_422H == m_jpeg_sampling && k != 0)
        {
            int    srcStep;
            int    dstStep;
            Ipp8u* pSrc;
            Ipp8u* pDst;
            IppiSize srcRoiSize;

            srcStep = curr_comp->m_ss_step;
            dstStep = curr_comp->m_cc_step;     

            // set the pointer to source buffer
            pSrc = curr_comp->GetSSBufferPtr<Ipp8u> (0) + 8 * colMCU;
            // set the pointer to destination buffer
            pDst = curr_comp->GetCCBufferPtr<Ipp8u> (0) + 8 * colMCU;  

            // set ROI of source
            srcRoiSize.width = (maxMCU - colMCU) * 8;
            srcRoiSize.height = m_curr_scan->mcuHeight;

            for(n = 0; n < srcRoiSize.height / 2; n++)
            {
                status = ippsCopy_8u(pSrc, pDst, srcRoiSize.width);
                if(ippStsNoErr != status)
                {
                    LOG0("Error: ippsCopy_8u() failed!");
                    return JPEG_ERR_INTERNAL;
                }

                pSrc += 2 * srcStep;  
                pDst += dstStep;   
            }
        }

        //horisontal downsampling
        if(JS_422V == m_jpeg_sampling && k != 0)
        {
            int    srcStep;
            int    dstStep;
            Ipp8u* pSrc;
            Ipp8u* pDst;
            IppiSize srcRoiSize;
            IppiSize dstRoiSize;

            srcStep = curr_comp->m_ss_step;
            dstStep = curr_comp->m_cc_step;

            // set the pointer to source buffer
            pSrc = curr_comp->GetSSBufferPtr<Ipp8u> (0) + m_curr_scan->mcuWidth * colMCU;
            // set the pointer to destination buffer
            pDst = curr_comp->GetCCBufferPtr<Ipp8u> (0) + m_curr_scan->mcuWidth * colMCU / 2;

            // set ROI of source
            srcRoiSize.width = (maxMCU - colMCU) * m_curr_scan->mcuWidth;
            srcRoiSize.height = 8;

            // set ROI of destination
            dstRoiSize.width = (maxMCU - colMCU) * m_curr_scan->mcuWidth / 2;
            dstRoiSize.height = 8;

            status = ippiSampleDownH2V1_JPEG_8u_C1R(pSrc, srcStep, srcRoiSize, pDst, dstStep, dstRoiSize);
            if(ippStsNoErr != status)
            {
                LOG0("Error: ippiSampleDownH2V2_JPEG_8u_C1R() failed!");
                return JPEG_ERR_INTERNAL;
            }
        }

        // just copy data
        if(JS_420 == m_jpeg_sampling && k != 0)
        {
            int    srcStep;
            int    dstStep;
            Ipp8u* pSrc;
            Ipp8u* pDst;
            IppiSize srcRoiSize;

            srcStep = curr_comp->m_ss_step;
            dstStep = curr_comp->m_cc_step;

            // set the pointer to source buffer
            pSrc = curr_comp->GetSSBufferPtr<Ipp8u> (0) + 8 * colMCU;
            // set the pointer to destination buffer
            pDst = curr_comp->GetCCBufferPtr<Ipp8u> (0) + 8 * colMCU;

            // set ROI of source
            srcRoiSize.width = (maxMCU - colMCU) * 8;
            srcRoiSize.height = 8;

            for(n = 0; n < srcRoiSize.height; n++)
            {
                status = ippsCopy_8u(pSrc, pDst, srcRoiSize.width);
                if(ippStsNoErr != status)
                {
                    LOG0("Error: ippsCopy_8u() failed!");
                    return JPEG_ERR_INTERNAL;
                }

                pSrc += srcStep;  
                pDst += dstStep;   
            }
        }

        // even colomns from even rows, odd colomns from odd rows
        if(JS_411 == m_jpeg_sampling && k != 0)
        {
            int    srcStep;
            int    dstStep;
            Ipp8u* pSrc;
            Ipp8u* pDst;
            IppiSize srcRoiSize;

            srcStep = curr_comp->m_ss_step;
            dstStep = curr_comp->m_cc_step;

            // set the pointer to source buffer
            pSrc = curr_comp->GetSSBufferPtr<Ipp8u> (0) + 8 * colMCU;
            // set the pointer to destination buffer
            pDst = curr_comp->GetCCBufferPtr<Ipp8u> (0) + 8 * colMCU * 2;  

            // set ROI of source
            srcRoiSize.width = (maxMCU - colMCU) * 8;
            srcRoiSize.height = m_curr_scan->mcuHeight;

            for(n = 0; n < srcRoiSize.height / 2; n++)
            { 
                for(i = 0; i < srcRoiSize.width; i++)
                {
                    pDst[n*dstStep + 2*i  ] = pSrc[2*n*srcStep + i];
                    pDst[n*dstStep + 2*i+1] = pSrc[2*n*srcStep + srcStep + i];
                }
            }
        }
      }
  }

  return JPEG_OK;
} // JERRCODE CJPEGDecoder::UpSampling(Ipp32u rowMCU, Ipp32u colMCU, Ipp32u maxMCU)


JERRCODE CJPEGDecoder::ProcessBuffer(int nMCURow, int thread_id)
{
  int                  c;
  int                  yPadd = 0;
  int                  srcStep = 0;
  int                  dstStep = 0;
  int                  copyHeight = 0;
  int                  ssHeight;
  Ipp8u*               pSrc8u  = 0;
  Ipp8u*               pDst8u  = 0;
  Ipp16u*              pSrc16u = 0;
  Ipp16u*              pDst16u = 0;
  IppiSize             roi;
  IppStatus            status;
  CJPEGColorComponent* curr_comp;

  if(m_jpeg_precision <= 8)
  {
    for(c = 0; c < m_jpeg_ncomp; c++)
    {
      curr_comp = &m_ccomp[c];

      if(curr_comp->m_v_factor == 1  && curr_comp->m_h_factor == 1)
      {
        srcStep    = curr_comp->m_cc_step;
        pSrc8u     = curr_comp->GetCCBufferPtr(thread_id);
        copyHeight = curr_comp->m_ss_height;
        yPadd      = m_yPadding;
      }

      if(curr_comp->m_v_factor == 2  && curr_comp->m_h_factor == 2)
      {
        srcStep    = curr_comp->m_ss_step;
        pSrc8u     = curr_comp->GetSSBufferPtr(thread_id);
        copyHeight = curr_comp->m_ss_height - 2;
        yPadd      = m_yPadding/2;
        pSrc8u     = pSrc8u + srcStep; // skip upper border line
      }

      if(curr_comp->m_v_factor == 1  && curr_comp->m_h_factor == 2)
      {
        srcStep    = curr_comp->m_ss_step;
        pSrc8u     = curr_comp->GetSSBufferPtr(thread_id);
        copyHeight = curr_comp->m_ss_height;
        yPadd      = m_yPadding;
      }

      ssHeight = copyHeight;

      if(nMCURow == (int) m_numyMCU - 1)
      {
      copyHeight -= yPadd;
    }

    roi.width  = srcStep    / m_dd_factor;
    roi.height = copyHeight / m_dd_factor;

    if(roi.height == 0)
      return JPEG_OK;

    pDst8u = m_dst.p.Data8u[c] + nMCURow * ssHeight * m_dst.lineStep[c] / m_dd_factor;

    status = ippiCopy_8u_C1R(pSrc8u, srcStep, pDst8u, m_dst.lineStep[c], roi);
    if(ippStsNoErr != status)
    {
      LOG1("IPP Error: ippiCopy_8u_C1R() failed - ",status);
      return JPEG_ERR_INTERNAL;
    }
  }
  }
  else      // 16-bit(>= 8) planar image with YCBCR color and 444 sampling
  {
    for(c = 0; c < m_jpeg_ncomp; c++)
    {
      curr_comp  = &m_ccomp[c];
      srcStep    = curr_comp->m_cc_step;
      pSrc16u    = (Ipp16u*)curr_comp->GetCCBufferPtr(thread_id);

      copyHeight = m_ccHeight;

      if(nMCURow == (int) m_numyMCU - 1)
      {
        copyHeight = m_mcuHeight - m_yPadding;
      }

      roi.width  = m_dst.width;
      roi.height = copyHeight;

      if(roi.height == 0)
        return JPEG_OK;

      dstStep  = m_dst.lineStep[c];

      pDst16u  = (Ipp16u*)((Ipp8u*)m_dst.p.Data16s[c] + nMCURow * m_mcuHeight * dstStep);

      status   = ippiCopy_16s_C1R((Ipp16s*)pSrc16u, srcStep, (Ipp16s*)pDst16u, dstStep, roi);
      if(ippStsNoErr != status)
      {
        LOG1("IPP Error: ippiCopy_16s_C1R() failed - ",status);
        return JPEG_ERR_INTERNAL;
      }
    } // for c
  }

  return JPEG_OK;
} // JERRCODE CJPEGDecoder::ProcessBuffer()


JERRCODE CJPEGDecoder::DecodeHuffmanMCURowBL(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU)
{
  int       n, k, l;
  Ipp32u j;
  int       srcLen;
  int       currPos;
  Ipp8u*    src;
  JERRCODE  jerr;
  IppStatus status;

  src    = m_BitStreamIn.GetDataPtr();
  srcLen = m_BitStreamIn.GetDataLen();

  for(j = colMCU; j < maxMCU; j++)
  {
    if(m_curr_scan->jpeg_restart_interval)
    {
      if(m_restarts_to_go == 0)
      {
        jerr = ProcessRestart();
        if(JPEG_OK != jerr)
        {
          LOG0("Error: ProcessRestart() failed!");
          return jerr;
        }
      }
    }

    for(n = m_curr_scan->first_comp; n < m_curr_scan->first_comp + m_curr_scan->ncomps; n++)
    {
      Ipp16s*                lastDC = &m_ccomp[n].m_lastDC;
      IppiDecodeHuffmanSpec* dctbl  = m_dctbl[m_ccomp[n].m_dc_selector];
      IppiDecodeHuffmanSpec* actbl  = m_actbl[m_ccomp[n].m_ac_selector];

      for(k = 0; k < m_ccomp[n].m_scan_vsampling; k++)
      {
        for(l = 0; l < m_ccomp[n].m_scan_hsampling; l++)
        {
          m_BitStreamIn.FillBuffer(SAFE_NBYTES);

          currPos = m_BitStreamIn.GetCurrPos();

          status = ippiDecodeHuffman8x8_JPEG_1u16s_C1(
                     src,srcLen,&currPos,pMCUBuf,lastDC,(int*)&m_marker,
                     dctbl,actbl,m_state);

          m_BitStreamIn.SetCurrPos(currPos);

          if(ippStsNoErr > status)
          {
            LOG0("Error: ippiDecodeHuffman8x8_JPEG_1u16s_C1() failed!");
            m_marker = JM_NONE;
            return JPEG_ERR_INTERNAL;
          }

          pMCUBuf += DCTSIZE2;
        } // for m_hsampling
      } // for m_vsampling
    } // for m_jpeg_ncomp

    m_restarts_to_go--;
  } // for m_numxMCU

  return JPEG_OK;

#if 0
  int       n, k, l;
  Ipp32u j;
  int       srcLen;
  int       currPos;
  int       curr_lnz;
  Ipp8u*    src;
  Ipp8u*    lnzBuf;
  JERRCODE  jerr;
  IppStatus status;
  const Ipp32u thread_id = 0;

  src    = m_BitStreamIn.GetDataPtr();
  srcLen = m_BitStreamIn.GetDataLen();

  Ipp8u* plnz = ((Ipp8u*)((IppiDecodeHuffmanState*)m_state) + 16*sizeof(Ipp8u));

  for(j = colMCU; j < maxMCU; j++)
  {
    if(m_curr_scan->jpeg_restart_interval)
    {
      if(m_restarts_to_go == 0)
      {
        jerr = ProcessRestart();
        if(JPEG_OK != jerr)
        {
          LOG0("Error: ProcessRestart() failed!");
          return jerr;
        }
      }
    }

    for(n = 0; n < m_jpeg_ncomp; n++)
    {
      Ipp16s*                lastDC = &m_ccomp[n].m_lastDC;
      IppiDecodeHuffmanSpec* dctbl  = m_dctbl[m_ccomp[n].m_dc_selector];
      IppiDecodeHuffmanSpec* actbl  = m_actbl[m_ccomp[n].m_ac_selector];
      curr_lnz                      = j * m_ccomp[n].m_lnz_ds;

      lnzBuf = m_ccomp[n].GetLNZBufferPtr(thread_id);

      for(k = 0; k < m_ccomp[n].m_vsampling; k++)
      {
        for(l = 0; l < m_ccomp[n].m_hsampling; l++)
        {
          m_BitStreamIn.FillBuffer(SAFE_NBYTES);

          currPos = m_BitStreamIn.GetCurrPos();

          status = ippiDecodeHuffman8x8_JPEG_1u16s_C1(
                     src,srcLen,&currPos,pMCUBuf,lastDC,(int*)&m_marker,
                     dctbl,actbl,m_state);

          lnzBuf[curr_lnz] = *plnz;
          curr_lnz++;

          m_BitStreamIn.SetCurrPos(currPos);

          if(ippStsNoErr > status)
          {
            LOG0("Error: ippiDecodeHuffman8x8_JPEG_1u16s_C1() failed!");
            m_marker = JM_NONE;
            return JPEG_ERR_INTERNAL;
          }

          pMCUBuf += DCTSIZE2;
        } // for m_hsampling
      } // for m_vsampling
    } // for m_jpeg_ncomp

    m_restarts_to_go--;
  } // for m_numxMCU

  return JPEG_OK;
#endif
} // JERRCODE CJPEGDecoder::DecodeHuffmanMCURowBL(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU)


JERRCODE CJPEGDecoder::DecodeHuffmanMCURowLS(Ipp16s* pMCUBuf)
{
  int       c;
  Ipp8u*    src;
  Ipp16s*   dst[4];
  int       srcLen;
  int       currPos;
  const IppiDecodeHuffmanSpec* dctbl[4];
  JERRCODE  jerr;
  IppStatus status;

  for(c = 0; c < m_jpeg_ncomp; c++)
  {
    dst[c] = pMCUBuf + c * m_numxMCU;
    dctbl[c] = m_dctbl[m_ccomp[c].m_dc_selector];
  }

  src    = m_BitStreamIn.GetDataPtr();
  srcLen = m_BitStreamIn.GetDataLen();

#if defined (HUFF_ROW_API)

  if(m_curr_scan->jpeg_restart_interval)
  {
    if(m_restarts_to_go == 0)
    {
      jerr = ProcessRestart();
      if(JPEG_OK != jerr)
      {
        LOG0("Error: ProcessRestart() failed!");
        return jerr;
      }
    }
  }

  m_BitStreamIn.FillBuffer();

  currPos = m_BitStreamIn.GetCurrPos();

  status = ippiDecodeHuffmanRow_JPEG_1u16s_C1P4(
             src,srcLen,&currPos,dst, m_numxMCU, m_jpeg_ncomp, (int*)&m_marker,
             dctbl,m_state);

  m_BitStreamIn.SetCurrPos(currPos);

  if(ippStsNoErr > status)
  {
    LOG0("Error: ippiDecodeHuffmanRow_JPEG_1u16s_C1P4() failed!");
    return JPEG_ERR_INTERNAL;
  }

  m_restarts_to_go -= m_numxMCU;

#else

  int                    j, h, v;
  CJPEGColorComponent*   curr_comp;

  for(j = 0; j < m_numxMCU; j++)
  {
    if(m_curr_scan->jpeg_restart_interval)
    {
      if(m_restarts_to_go == 0)
      {
        jerr = ProcessRestart();
        if(JPEG_OK != jerr)
        {
          LOG0("Error: ProcessRestart() failed!");
          return jerr;
        }
      }
    }

    for(c = 0; c < m_jpeg_ncomp; c++)
    {
      curr_comp = &m_ccomp[c];

      for(v = 0; v < curr_comp->m_vsampling; v++)
      {
        for(h = 0; h < curr_comp->m_hsampling; h++)
        {
          m_BitStreamIn.FillBuffer(SAFE_NBYTES);

          currPos = m_BitStreamIn.GetCurrPos();

          status = ippiDecodeHuffmanOne_JPEG_1u16s_C1(
                     src,srcLen,&currPos,dst[c],(int*)&m_marker,
                     dctbl[c],m_state);

          m_BitStreamIn.SetCurrPos(currPos);

          if(ippStsNoErr > status)
          {
            LOG0("Error: ippiDecodeHuffmanOne_JPEG_1u16s_C1() failed!");
            return JPEG_ERR_INTERNAL;
          }

          dst[c]++;
        } // for m_hsampling
      } // for m_vsampling
    } // for m_jpeg_ncomp

    m_restarts_to_go --;
  } // for m_numxMCU
#endif

  return JPEG_OK;
} // CJPEGDecoder::DecodeHuffmanMCURowLS()


JERRCODE CJPEGDecoder::ReconstructMCURowBL8x8_NxN(Ipp16s* pMCUBuf,
                                                  Ipp32u colMCU,
                                                  Ipp32u maxMCU)
{
  int       c, k, l, curr_lnz;
  Ipp32u mcu_col;
  Ipp8u*    lnz     = 0;
  Ipp8u*    dst     = 0;
  int       dstStep = m_ccWidth;
  Ipp16u*   qtbl;
  IppStatus status;
  CJPEGColorComponent* curr_comp;
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif
  const int thread_id = 0;

  for(mcu_col = colMCU; mcu_col < maxMCU; mcu_col++)
  {
    for(c = 0; c < m_jpeg_ncomp; c++)
    {
      curr_comp = &m_ccomp[c];
      lnz       = m_ccomp[c].GetLNZBufferPtr(thread_id);
      curr_lnz  = mcu_col * curr_comp->m_lnz_ds;

      qtbl = m_qntbl[curr_comp->m_q_selector];

      for(k = 0; k < curr_comp->m_vsampling; k++)
      {
        if(curr_comp->m_hsampling == m_max_hsampling &&
           curr_comp->m_vsampling == m_max_vsampling)
        {
          dstStep = curr_comp->m_cc_step;
          dst     = curr_comp->GetCCBufferPtr(thread_id) + mcu_col*8*curr_comp->m_hsampling + k*8*dstStep;
        }
        else
        {
          dstStep = curr_comp->m_ss_step;
          dst     = curr_comp->GetSSBufferPtr(thread_id) + mcu_col*8*curr_comp->m_hsampling + k*8*dstStep;

          curr_comp->m_need_upsampling = 1;
        }

        // skip border row (when 244 or 411 sampling)
        if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2)
        {
          dst += dstStep;
        }

        for(l = 0; l < curr_comp->m_hsampling; l++)
        {
          dst += l*8;

#ifdef __TIMING__
          c0 = ippGetCpuClocks();
#endif

          if(lnz[curr_lnz] == 1)  // 1x1
          {
            status = ippiDCTQuantInv8x8LS_1x1_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);
          }
          else if(lnz[curr_lnz] < 5 && pMCUBuf[16] == 0) //2x2
          {
            status = ippiDCTQuantInv8x8LS_2x2_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);
          }
          else if(lnz[curr_lnz] <= 24 //4x4
                 && pMCUBuf[32] == 0
                 && pMCUBuf[33] == 0
                 && pMCUBuf[34] == 0
                 && pMCUBuf[4]  == 0
                 && pMCUBuf[12] == 0)
          {
            status = ippiDCTQuantInv8x8LS_4x4_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);
          }
          else      // 8x8
          {
            status = ippiDCTQuantInv8x8LS_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);
          }

          curr_lnz = curr_lnz + 1;

          if(ippStsNoErr > status)
          {
            LOG0("Error: ippiDCTQuantInv8x8_NxNLS_JPEG_16s8u_C1R() failed!");
            return JPEG_ERR_INTERNAL;
          }
#ifdef __TIMING__
          c1 = ippGetCpuClocks();
          m_clk_dct += c1 - c0;
#endif

          pMCUBuf += DCTSIZE2;
        } // for m_hsampling
      } // for m_vsampling
    } // for m_jpeg_ncomp
  } // for m_numxMCU

  return JPEG_OK;
} // CJPEGDecoder::ReconstructMCURowBL8x8_NxN()


JERRCODE CJPEGDecoder::ReconstructMCURowBL8x8(Ipp16s* pMCUBuf,
                                              Ipp32u colMCU,
                                              Ipp32u maxMCU)
{
    int       c, k, l;
  Ipp32u mcu_col;
  Ipp8u*    dst     = 0;
  Ipp8u*    p       = 0;
  int       dstStep = m_ccWidth;
  Ipp16u*   qtbl;
  IppStatus status;
  CJPEGColorComponent* curr_comp;
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif
  const Ipp32u thread_id = 0;

  for(mcu_col = colMCU; mcu_col < maxMCU; mcu_col++)
  {
    for(c = m_curr_scan->first_comp; c < m_curr_scan->first_comp + m_curr_scan->ncomps; c++)
    {
      curr_comp = &m_ccomp[c];

      qtbl = m_qntbl[curr_comp->m_q_selector];

      for(k = 0; k < curr_comp->m_scan_vsampling; k++)
      {
        if(curr_comp->m_hsampling == m_max_hsampling &&
           curr_comp->m_vsampling == m_max_vsampling)
        {
          dstStep = curr_comp->m_cc_step;
          dst     = curr_comp->GetCCBufferPtr(thread_id) + mcu_col*8*curr_comp->m_scan_hsampling + k*8*dstStep;
        }
        else
        {
          dstStep = curr_comp->m_ss_step;
          dst     = curr_comp->GetSSBufferPtr(thread_id) + mcu_col*8*curr_comp->m_scan_hsampling + k*8*dstStep;

          curr_comp->m_need_upsampling = 1;
        }

        for(l = 0; l < curr_comp->m_scan_hsampling; l++)
        {
          p = dst + l*8;

#ifdef __TIMING__
          c0 = ippGetCpuClocks();
#endif

          status = ippiDCTQuantInv8x8LS_JPEG_16s8u_C1R(pMCUBuf, p, dstStep, qtbl);

          if(ippStsNoErr > status)
          {
            LOG0("Error: ippiDCTQuantInv8x8LS_JPEG_16s8u_C1R() failed!");
            return JPEG_ERR_INTERNAL;
          }
#ifdef __TIMING__
          c1 = ippGetCpuClocks();
          m_clk_dct += c1 - c0;
#endif

          pMCUBuf += DCTSIZE2;
        } // for m_hsampling
      } // for m_vsampling
    } // for m_jpeg_ncomp
  } // for m_numxMCU

  return JPEG_OK;

#if 0
  int       c, k, l;
  Ipp32u mcu_col;
  Ipp8u*    dst     = 0;
  Ipp8u*    p       = 0;
  int       dstStep = m_ccWidth;
  Ipp16u*   qtbl;
  IppStatus status;
  CJPEGColorComponent* curr_comp;
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif
  const Ipp32u thread_id = 0;

  for(mcu_col = colMCU; mcu_col < maxMCU; mcu_col++)
  {
    for(c = 0; c < m_jpeg_ncomp; c++)
    {
      curr_comp = &m_ccomp[c];

      qtbl = m_qntbl[curr_comp->m_q_selector];

      for(k = 0; k < curr_comp->m_vsampling; k++)
      {
        if(curr_comp->m_hsampling == m_max_hsampling &&
           curr_comp->m_vsampling == m_max_vsampling)
        {
          dstStep = curr_comp->m_cc_step;
          dst     = curr_comp->GetCCBufferPtr(thread_id) + mcu_col*8*curr_comp->m_hsampling + k*8*dstStep;
        }
        else
        {
          dstStep = curr_comp->m_ss_step;
          dst     = curr_comp->GetSSBufferPtr(thread_id) + mcu_col*8*curr_comp->m_hsampling + k*8*dstStep;

          curr_comp->m_need_upsampling = 1;
        }

        // skip border row (when 244 or 411 sampling)
        if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2)
        {
          dst += dstStep;
        }

        for(l = 0; l < curr_comp->m_hsampling; l++)
        {
          p = dst + l*8;

#ifdef __TIMING__
          c0 = ippGetCpuClocks();
#endif

          status = ippiDCTQuantInv8x8LS_JPEG_16s8u_C1R(pMCUBuf, p, dstStep, qtbl);

          if(ippStsNoErr > status)
          {
            LOG0("Error: ippiDCTQuantInv8x8LS_JPEG_16s8u_C1R() failed!");
            return JPEG_ERR_INTERNAL;
          }
#ifdef __TIMING__
          c1 = ippGetCpuClocks();
          m_clk_dct += c1 - c0;
#endif

          pMCUBuf += DCTSIZE2;
        } // for m_hsampling
      } // for m_vsampling
    } // for m_jpeg_ncomp
  } // for m_numxMCU

  return JPEG_OK;
#endif
} // CJPEGDecoder::ReconstructMCURowBL8x8()


JERRCODE CJPEGDecoder::ReconstructMCURowBL8x8To4x4(Ipp16s* pMCUBuf,
                                                   Ipp32u colMCU,
                                                   Ipp32u maxMCU)
{
  int       c, k, l;
  Ipp32u mcu_col;
  Ipp8u*    dst     = 0;
  int       dstStep = m_ccWidth;
  Ipp16u*   qtbl;
  IppStatus status;
  CJPEGColorComponent* curr_comp;
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif
  const int thread_id = 0;

  for(mcu_col = colMCU; mcu_col < maxMCU; mcu_col++)
  {
    for(c = 0; c < m_jpeg_ncomp; c++)
    {
      curr_comp = &m_ccomp[c];
      qtbl = m_qntbl[curr_comp->m_q_selector];

      for(k = 0; k < curr_comp->m_vsampling; k++)
      {
        if(curr_comp->m_hsampling == m_max_hsampling &&
           curr_comp->m_vsampling == m_max_vsampling)
        {
          dstStep = curr_comp->m_cc_step;
          dst     = curr_comp->GetCCBufferPtr(thread_id) + mcu_col*4*curr_comp->m_hsampling + k*4*dstStep;

          for(l = 0; l < curr_comp->m_hsampling; l++)
          {
            dst += ((l == 0) ? 0 : 1)*4;

#ifdef __TIMING__
          c0 = ippGetCpuClocks();
#endif
          status = ippiDCTQuantInv8x8To4x4LS_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);

          if(ippStsNoErr > status)
          {
            LOG0("Error: ippiDCTQuantInv8x8To4x4LS_JPEG_16s8u_C1R() failed!");
            return JPEG_ERR_INTERNAL;
          }
#ifdef __TIMING__
          c1 = ippGetCpuClocks();
          m_clk_dct += c1 - c0;
#endif
            pMCUBuf += DCTSIZE2;
          } // for m_hsampling
        }
        else
        {
          if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2 && m_dst.order == JD_PIXEL)
          {
            dstStep = curr_comp->m_cc_step;
            dst     = curr_comp->GetCCBufferPtr(thread_id) + mcu_col*8*curr_comp->m_hsampling + k*8*dstStep;

            for(l = 0; l < curr_comp->m_hsampling; l++)
            {
              dst += ((l == 0) ? 0 : 1)*8;

#ifdef __TIMING__
              c0 = ippGetCpuClocks();
#endif
              status = ippiDCTQuantInv8x8LS_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);

              if(ippStsNoErr > status)
              {
                LOG0("Error: ippiDCTQuantInv8x8LS_JPEG_16s8u_C1R() failed!");
                return JPEG_ERR_INTERNAL;
              }
#ifdef __TIMING__
              c1 = ippGetCpuClocks();
              m_clk_dct += c1 - c0;
#endif
              pMCUBuf += DCTSIZE2;
            } // for m_hsampling
          }
          else
          {
            dstStep = curr_comp->m_ss_step;
            dst     = curr_comp->GetSSBufferPtr(thread_id) + mcu_col*4*curr_comp->m_hsampling + k*4*dstStep;

            curr_comp->m_need_upsampling = 1;

            // skip border row (when 244 or 411 sampling)
            if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2)
            {
              dst += dstStep;
            }

            for(l = 0; l < curr_comp->m_hsampling; l++)
            {
              dst += ((l == 0) ? 0 : 1)*4;

#ifdef __TIMING__
              c0 = ippGetCpuClocks();
#endif
              status = ippiDCTQuantInv8x8To4x4LS_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);

              if(ippStsNoErr > status)
              {
                LOG0("Error: ippiDCTQuantInv8x8To4x4LS_JPEG_16s8u_C1R() failed!");
                return JPEG_ERR_INTERNAL;
              }
#ifdef __TIMING__
              c1 = ippGetCpuClocks();
              m_clk_dct += c1 - c0;
#endif
              pMCUBuf += DCTSIZE2;
            } // for m_hsampling
          }
        } // for m_hsampling
      } // for m_vsampling
    } // for m_jpeg_ncomp
  } // for m_numxMCU

  return JPEG_OK;
} // CJPEGDecoder::ReconstructMCURowBL8x8To4x4()


JERRCODE CJPEGDecoder::ReconstructMCURowBL8x8To2x2(Ipp16s* pMCUBuf,
                                                   Ipp32u colMCU,
                                                   Ipp32u maxMCU)
{
  int       c, k, l;
  Ipp32u mcu_col;
  Ipp8u*    dst     = 0;
  int       dstStep = m_ccWidth;
  Ipp16u*   qtbl;
  IppStatus status;
  CJPEGColorComponent* curr_comp;
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif
  const int thread_id = 0;

  for(mcu_col = colMCU; mcu_col < maxMCU; mcu_col++)
  {
    for(c = 0; c < m_jpeg_ncomp; c++)
    {
      curr_comp = &m_ccomp[c];
      qtbl = m_qntbl[curr_comp->m_q_selector];

      for(k = 0; k < curr_comp->m_vsampling; k++)
      {
        if(curr_comp->m_hsampling == m_max_hsampling &&
           curr_comp->m_vsampling == m_max_vsampling)
        {
          dstStep = curr_comp->m_cc_step;
          dst     = curr_comp->GetCCBufferPtr(thread_id) + mcu_col*2*curr_comp->m_hsampling + k*2*dstStep;

          for(l = 0; l < curr_comp->m_hsampling; l++)
          {
            dst += ((l == 0) ? 0 : 1)*2;

#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            status = ippiDCTQuantInv8x8To2x2LS_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);

            if(ippStsNoErr > status)
            {
              LOG0("Error: ippiDCTQuantInv8x8To2x2LS_JPEG_16s8u_C1R() failed!");
              return JPEG_ERR_INTERNAL;
            }
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_dct += c1 - c0;
#endif
            pMCUBuf += DCTSIZE2;
          } // for m_hsampling
        }
        else
        {
          if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2 && m_dst.order == JD_PIXEL)
          {
            dstStep = curr_comp->m_cc_step;
            dst     = curr_comp->GetCCBufferPtr(thread_id) + mcu_col*4*curr_comp->m_hsampling + k*4*dstStep;

            for(l = 0; l < curr_comp->m_hsampling; l++)
            {
              dst += ((l == 0) ? 0 : 1)*4;

#ifdef __TIMING__
              c0 = ippGetCpuClocks();
#endif
              status = ippiDCTQuantInv8x8To4x4LS_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);

              if(ippStsNoErr > status)
              {
                LOG0("Error: ippiDCTQuantInv8x8To4x4LS_JPEG_16s8u_C1R() failed!");
                return JPEG_ERR_INTERNAL;
              }
#ifdef __TIMING__
              c1 = ippGetCpuClocks();
              m_clk_dct += c1 - c0;
#endif
              pMCUBuf += DCTSIZE2;
            } // for m_hsampling
          }
          else
          {
            dstStep = curr_comp->m_ss_step;
            dst     = curr_comp->GetSSBufferPtr(thread_id) + mcu_col*2*curr_comp->m_hsampling + k*2*dstStep;

            curr_comp->m_need_upsampling = 1;

            // skip border row (when 244 or 411 sampling)
            if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2)
            {
              dst += dstStep;
            }

            for(l = 0; l < curr_comp->m_hsampling; l++)
            {
              dst += ((l == 0) ? 0 : 1)*2;

#ifdef __TIMING__
              c0 = ippGetCpuClocks();
#endif
              status = ippiDCTQuantInv8x8To2x2LS_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);

              if(ippStsNoErr > status)
              {
                LOG0("Error: ippiDCTQuantInv8x8To2x2LS_JPEG_16s8u_C1R() failed!");
                return JPEG_ERR_INTERNAL;
              }
#ifdef __TIMING__
              c1 = ippGetCpuClocks();
              m_clk_dct += c1 - c0;
#endif
              pMCUBuf += DCTSIZE2;
            } // for m_hsampling
          }
        } // for m_hsampling
      } // for m_vsampling
    } // for m_jpeg_ncomp
  } // for m_numxMCU

  return JPEG_OK;
} // CJPEGDecoder::ReconstructMCURowBL8x8To2x2()


JERRCODE CJPEGDecoder::ReconstructMCURowBL8x8To1x1(Ipp16s* pMCUBuf,
                                                   Ipp32u colMCU,
                                                   Ipp32u maxMCU)
{
  int       c, k, l;
  Ipp32u mcu_col;
  Ipp8u*    dst     = 0;
  int       dstStep = m_ccWidth;
  Ipp16u*   qtbl;
  IppStatus status;

  CJPEGColorComponent* curr_comp;
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif
  const int thread_id = 0;

  for(mcu_col = colMCU; mcu_col < maxMCU; mcu_col++)
  {
    for(c = 0; c < m_jpeg_ncomp; c++)
    {
      curr_comp = &m_ccomp[c];
      qtbl = m_qntbl[curr_comp->m_q_selector];

      for(k = 0; k < curr_comp->m_vsampling; k++)
      {
        if(curr_comp->m_hsampling == m_max_hsampling &&
           curr_comp->m_vsampling == m_max_vsampling)
        {
          dstStep = curr_comp->m_cc_step;
          dst     = curr_comp->GetCCBufferPtr(thread_id) + mcu_col*curr_comp->m_hsampling + k*dstStep;

          for(l = 0; l < curr_comp->m_hsampling; l++)
          {
            dst += (l == 0) ? 0 : 1;

#ifdef __TIMING__
          c0 = ippGetCpuClocks();
#endif
          DCT_QUANT_INV8x8To1x1LS(pMCUBuf, dst, qtbl);

#ifdef __TIMING__
          c1 = ippGetCpuClocks();
          m_clk_dct += c1 - c0;
#endif
            pMCUBuf += DCTSIZE2;
          } // for m_hsampling
        }
        else
        {
          if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2 && m_dst.order == JD_PIXEL)
          {
            dstStep = curr_comp->m_cc_step;
            dst     = curr_comp->GetCCBufferPtr(thread_id) + mcu_col*2*curr_comp->m_hsampling + k*2*dstStep;

            for(l = 0; l < curr_comp->m_hsampling; l++)
            {
              dst += ((l == 0) ? 0 : 1)*2;

#ifdef __TIMING__
              c0 = ippGetCpuClocks();
#endif
              status = ippiDCTQuantInv8x8To2x2LS_JPEG_16s8u_C1R(pMCUBuf, dst, dstStep, qtbl);

              if(ippStsNoErr > status)
              {
                LOG0("Error: ippiDCTQuantInv8x8To2x2LS_JPEG_16s8u_C1R() failed!");
                return JPEG_ERR_INTERNAL;
              }
#ifdef __TIMING__
              c1 = ippGetCpuClocks();
              m_clk_dct += c1 - c0;
#endif
              pMCUBuf += DCTSIZE2;
            } // for m_hsampling
          }
          else
          {
            dstStep = curr_comp->m_ss_step;
            dst     = curr_comp->GetSSBufferPtr(thread_id) + mcu_col*curr_comp->m_hsampling + k*dstStep;

            curr_comp->m_need_upsampling = 1;

            // skip border row (when 244 or 411 sampling)
            if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2)
            {
              dst += dstStep;
            }

            for(l = 0; l < curr_comp->m_hsampling; l++)
            {
              dst += (l == 0) ? 0 : 1;

#ifdef __TIMING__
              c0 = ippGetCpuClocks();
#endif
              DCT_QUANT_INV8x8To1x1LS(pMCUBuf, dst, qtbl);

#ifdef __TIMING__
              c1 = ippGetCpuClocks();
              m_clk_dct += c1 - c0;
#endif
              pMCUBuf += DCTSIZE2;
            } // for m_hsampling
          } // if
        } // for m_hsampling
      } // for m_vsampling
    } // for m_jpeg_ncomp
  } // for m_numxMCU

  return JPEG_OK;
} // CJPEGDecoder::ReconstructMCURowBL8x8To1x1()


JERRCODE CJPEGDecoder::ReconstructMCURowEX(Ipp16s* pMCUBuf,
                                           Ipp32u colMCU,
                                           Ipp32u maxMCU)
{
  int       c, k, l;
  Ipp32u mcu_col;
  Ipp16u*   dst = 0;
  int       dstStep;
  Ipp32f*   qtbl;
  IppStatus status;
  CJPEGColorComponent* curr_comp;
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif
  const int thread_id = 0;

  for(mcu_col = colMCU; mcu_col < maxMCU; mcu_col++)
  {
    for(c = 0; c < m_jpeg_ncomp; c++)
    {
      curr_comp = &m_ccomp[c];

      qtbl = m_qntbl[curr_comp->m_q_selector];

      for(k = 0; k < curr_comp->m_vsampling; k++)
      {
        if(curr_comp->m_hsampling == m_max_hsampling &&
           curr_comp->m_vsampling == m_max_vsampling)
        {
          dstStep = curr_comp->m_cc_step;
          dst     = (Ipp16u*)(curr_comp->GetCCBufferPtr(thread_id) + mcu_col*8*curr_comp->m_hsampling*sizeof(Ipp16s) + k*8*dstStep*sizeof(Ipp16s));
        }
        else
        {
          dstStep = curr_comp->m_ss_step;
          dst     = (Ipp16u*)(curr_comp->GetSSBufferPtr(thread_id) + mcu_col*8*curr_comp->m_hsampling*sizeof(Ipp16s) + k*8*dstStep*sizeof(Ipp16s));

          curr_comp->m_need_upsampling = 1;
        }

        // skip border row (when 244 or 411 sampling)
        if(curr_comp->m_h_factor == 2 && curr_comp->m_v_factor == 2)
        {
          dst += dstStep;
        }

        for(l = 0; l < curr_comp->m_hsampling; l++)
        {
          dst += l*8;

#ifdef __TIMING__
          c0 = ippGetCpuClocks();
#endif

          status = ippiDCTQuantInv8x8LS_JPEG_16s16u_C1R(
                     pMCUBuf,dst,dstStep,qtbl);
          if(ippStsNoErr > status)
          {
            LOG0("Error: ippiDCTQuantInv8x8LS_JPEG_16s16u_C1R() failed!");
            return JPEG_ERR_INTERNAL;
          }


#ifdef __TIMING__
          c1 = ippGetCpuClocks();
          m_clk_dct += c1 - c0;
#endif

          pMCUBuf += DCTSIZE2;
        } // for m_hsampling
      } // for m_vsampling
    } // for m_jpeg_ncomp
  } // for m_numxMCU

  return JPEG_OK;
} // CJPEGDecoder::ReconstructMCURowEX()


JERRCODE CJPEGDecoder::ReconstructMCURowLS(
  Ipp16s* pMCUBuf,
  int     nMCURow,
  int     thread_id)
{
  int       n;
  int       dstStep;
  Ipp16s*   ptr;
  Ipp16s*   pCurrRow;
  Ipp16s*   pPrevRow;
  Ipp8u*    pDst8u  = 0;
  Ipp16s*   pDst16s = 0;
  IppiSize  roi;
  IppStatus status;
  CJPEGColorComponent* curr_comp;

  thread_id = thread_id; // remove warning

  roi.width  = m_dst.width;
  roi.height = 1;

  for(n = 0; n < m_jpeg_ncomp; n++)
  {
    curr_comp = &m_ccomp[n];

    if(m_dst.precision <= 8)
    {
      dstStep = curr_comp->m_cc_step;
      pDst8u  = curr_comp->GetCCBufferPtr(thread_id);
    }
    else
    {
      dstStep = curr_comp->m_cc_step;
      pDst16s = (Ipp16s*)curr_comp->GetCCBufferPtr(thread_id);
    }

    if(m_jpeg_ncomp == m_curr_scan->ncomps)
      ptr = pMCUBuf + n*m_numxMCU;
    else
      ptr = pMCUBuf + n*m_numxMCU*m_numyMCU + nMCURow*m_numxMCU;

    pCurrRow = curr_comp->m_curr_row;
    pPrevRow = curr_comp->m_prev_row;

    if(0 != nMCURow && 0 == m_rst_go)
    {
      status = ippiReconstructPredRow_JPEG_16s_C1(
        ptr,pPrevRow,pCurrRow,m_dst.width,m_ss);
    }
    else
    {
      status = ippiReconstructPredFirstRow_JPEG_16s_C1(
        ptr,pCurrRow,m_dst.width,m_jpeg_precision,m_al);
    }

    if(ippStsNoErr != status)
    {
      return JPEG_ERR_INTERNAL;
    }

    // do point-transform if any
    status = ippsLShiftC_16s(pCurrRow,m_al,pPrevRow,m_dst.width);
    if(ippStsNoErr != status)
    {
      return JPEG_ERR_INTERNAL;
    }

    if(m_dst.precision <= 8)
    {
      status = ippiAndC_16u_C1IR(0xFF, (Ipp16u*)pPrevRow, m_dst.width*sizeof(Ipp16s),roi);
      status = ippiConvert_16u8u_C1R((Ipp16u*)pPrevRow,m_dst.width*sizeof(Ipp16s),pDst8u,dstStep,roi);
    }
    else
      status = ippsCopy_16s(pPrevRow,pDst16s,m_dst.width);

    if(ippStsNoErr != status)
    {
      return JPEG_ERR_INTERNAL;
    }

    curr_comp->m_curr_row = pPrevRow;
    curr_comp->m_prev_row = pCurrRow;
  } // for m_jpeg_ncomp

  m_rst_go = 0;

  return JPEG_OK;
} // CJPEGDecoder::ReconstructMCURowLS()


JERRCODE CJPEGDecoder::DecodeScanBaseline(void)
{
    IppStatus status;
  JERRCODE  jerr = JPEG_OK;
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif

  status = ippiDecodeHuffmanStateInit_JPEG_8u(m_state);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  if(0 == m_curr_scan->scan_no)
  {
      m_curr_scan->first_comp = 0;
  }
  else if(1 == m_curr_scan->scan_no && (2 == m_curr_scan->ncomps || (1 == m_curr_scan->ncomps && 3 == m_num_scans)))
  {
      m_curr_scan->first_comp = 1;
  }
  else if((1 == m_curr_scan->scan_no && 1 == m_curr_scan->ncomps && 2 == m_num_scans) || 2 == m_curr_scan->scan_no)
  {
      m_curr_scan->first_comp = 2;
  }
  else
  {
      return JPEG_ERR_SOS_DATA;
  }

  m_marker = JM_NONE;

  // workaround for 8-bit qnt tables in 12-bit scans
  if(m_qntbl[0].m_initialized && m_qntbl[0].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[0].ConvertToHighPrecision();

  if(m_qntbl[1].m_initialized && m_qntbl[1].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[1].ConvertToHighPrecision();

  if(m_qntbl[2].m_initialized && m_qntbl[2].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[2].ConvertToHighPrecision();

  if(m_qntbl[3].m_initialized && m_qntbl[3].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[3].ConvertToHighPrecision();

  // workaround for 16-bit qnt tables in 8-bit scans
  if(m_qntbl[0].m_initialized && m_qntbl[0].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[0].ConvertToLowPrecision();

  if(m_qntbl[1].m_initialized && m_qntbl[1].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[1].ConvertToLowPrecision();

  if(m_qntbl[2].m_initialized && m_qntbl[2].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[2].ConvertToLowPrecision();

  if(m_qntbl[3].m_initialized && m_qntbl[3].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[3].ConvertToLowPrecision();

  if(m_dctbl[0].IsEmpty())
  {
    jerr = m_dctbl[0].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_dctbl[0].Init(0,0,(Ipp8u*)&DefaultLuminanceDCBits[0],(Ipp8u*)&DefaultLuminanceDCValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_dctbl[1].IsEmpty())
  {
    jerr = m_dctbl[1].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_dctbl[1].Init(1,0,(Ipp8u*)&DefaultChrominanceDCBits[0],(Ipp8u*)&DefaultChrominanceDCValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_actbl[0].IsEmpty())
  {
    jerr = m_actbl[0].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_actbl[0].Init(0,1,(Ipp8u*)&DefaultLuminanceACBits[0],(Ipp8u*)&DefaultLuminanceACValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_actbl[1].IsEmpty())
  {
    jerr = m_actbl[1].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_actbl[1].Init(1,1,(Ipp8u*)&DefaultChrominanceACBits[0],(Ipp8u*)&DefaultChrominanceACValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

    {
        Ipp16s* pMCUBuf;
        Ipp32u rowMCU, colMCU, maxMCU;
        Ipp32u numxMCU = m_curr_scan->numxMCU;
        Ipp32u numyMCU = m_curr_scan->numyMCU;

        // the pointer to Buffer for a current thread.
        pMCUBuf = m_block_buffer;

        // set the iterators
        rowMCU = 0;
        colMCU = 0;
        maxMCU = numxMCU;
        if (m_curr_scan->jpeg_restart_interval)
        {
            rowMCU = m_mcu_decoded / numxMCU;
            colMCU = m_mcu_decoded % numxMCU;
            maxMCU = (numxMCU < colMCU + m_mcu_to_decode) ?
                     (numxMCU) :
                     (colMCU + m_mcu_to_decode);
        }

        while (rowMCU < numyMCU)
        {
            // decode a MCU row
#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            ippsZero_16s(pMCUBuf, m_numxMCU * m_nblock * DCTSIZE2);

            jerr = DecodeHuffmanMCURowBL(pMCUBuf, colMCU, maxMCU);
            if (JPEG_OK != jerr)
                return jerr;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_huff += (c1 - c0);
#endif

            // reconstruct a MCU row
#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            if(m_jpeg_precision == 12)
                jerr = ReconstructMCURowEX(pMCUBuf, colMCU, maxMCU);
            else
            {
                switch (m_jpeg_dct_scale)
                {
                default:
                case JD_1_1:
                    {
                        if(m_use_qdct)
                            jerr = ReconstructMCURowBL8x8_NxN(pMCUBuf, colMCU, maxMCU);
                        else
                            jerr = ReconstructMCURowBL8x8(pMCUBuf, colMCU, maxMCU);
                    }
                    break;

                case JD_1_2:
                    {
                        jerr = ReconstructMCURowBL8x8To4x4(pMCUBuf, colMCU, maxMCU);
                    }
                    break;

                case JD_1_4:
                    {
                        jerr = ReconstructMCURowBL8x8To2x2(pMCUBuf, colMCU, maxMCU);
                    }
                    break;

                case JD_1_8:
                    {
                        jerr = ReconstructMCURowBL8x8To1x1(pMCUBuf, colMCU, maxMCU);
                    }
                    break;
                }
            }

            if (JPEG_OK != jerr)
                return jerr;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_dct += c1 - c0;
#endif

#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif

            jerr = UpSampling(rowMCU, colMCU, maxMCU);
            if (JPEG_OK != jerr)
                return jerr;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_ss += c1 - c0;
#endif

#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            jerr = ColorConvert(rowMCU, colMCU, maxMCU);
            if (JPEG_OK != jerr)
                return jerr;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_cc += c1 - c0;
#endif

            // increment interators
            if (m_curr_scan->jpeg_restart_interval)
            {
                m_mcu_decoded += (maxMCU - colMCU);
                m_mcu_to_decode -= (maxMCU - colMCU);
                if (0 == m_mcu_to_decode)
                {
                    return JPEG_OK;
                }
                maxMCU = (numxMCU < m_mcu_to_decode) ?
                         (numxMCU) :
                         (m_mcu_to_decode);
            }
            else
            {
                maxMCU = numxMCU;
            }

            rowMCU += 1;
            colMCU = 0;
        }
    }

    return JPEG_OK;
} // CJPEGDecoder::DecodeScanBaseline()


JERRCODE CJPEGDecoder::DecodeScanBaselineIN(void)
{
  IppStatus status;
  JERRCODE  jerr = JPEG_OK;
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif

  status = ippiDecodeHuffmanStateInit_JPEG_8u(m_state);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  m_marker = JM_NONE;

  // workaround for 8-bit qnt tables in 12-bit scans
  if(m_qntbl[0].m_initialized && m_qntbl[0].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[0].ConvertToHighPrecision();

  if(m_qntbl[1].m_initialized && m_qntbl[1].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[1].ConvertToHighPrecision();

  if(m_qntbl[2].m_initialized && m_qntbl[2].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[2].ConvertToHighPrecision();

  if(m_qntbl[3].m_initialized && m_qntbl[3].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[3].ConvertToHighPrecision();

  // workaround for 16-bit qnt tables in 8-bit scans
  if(m_qntbl[0].m_initialized && m_qntbl[0].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[0].ConvertToLowPrecision();

  if(m_qntbl[1].m_initialized && m_qntbl[1].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[1].ConvertToLowPrecision();

  if(m_qntbl[2].m_initialized && m_qntbl[2].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[2].ConvertToLowPrecision();

  if(m_qntbl[3].m_initialized && m_qntbl[3].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[3].ConvertToLowPrecision();

  if(m_dctbl[0].IsEmpty())
  {
    jerr = m_dctbl[0].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_dctbl[0].Init(0,0,(Ipp8u*)&DefaultLuminanceDCBits[0],(Ipp8u*)&DefaultLuminanceDCValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_dctbl[1].IsEmpty())
  {
    jerr = m_dctbl[1].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_dctbl[1].Init(1,0,(Ipp8u*)&DefaultChrominanceDCBits[0],(Ipp8u*)&DefaultChrominanceDCValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_actbl[0].IsEmpty())
  {
    jerr = m_actbl[0].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_actbl[0].Init(0,1,(Ipp8u*)&DefaultLuminanceACBits[0],(Ipp8u*)&DefaultLuminanceACValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_actbl[1].IsEmpty())
  {
    jerr = m_actbl[1].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_actbl[1].Init(1,1,(Ipp8u*)&DefaultChrominanceACBits[0],(Ipp8u*)&DefaultChrominanceACValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

    {
        Ipp16s* pMCUBuf;
        Ipp32u rowMCU, colMCU, maxMCU;

        // the pointer to Buffer for a current thread.
        pMCUBuf = m_block_buffer;

        // set the iterators
        rowMCU = 0;
        colMCU = 0;
        maxMCU = m_numxMCU;
        if (m_curr_scan->jpeg_restart_interval)
        {
            rowMCU = m_mcu_decoded / m_numxMCU;
            colMCU = m_mcu_decoded % m_numxMCU;
            maxMCU = (m_numxMCU < colMCU + m_mcu_to_decode) ?
                     (m_numxMCU) :
                     (colMCU + m_mcu_to_decode);
        }

        while (rowMCU < m_numyMCU)
        {
            // decode a MCU row
#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            ippsZero_16s(pMCUBuf,m_numxMCU * m_nblock * DCTSIZE2);

            jerr = DecodeHuffmanMCURowBL(pMCUBuf, colMCU, maxMCU);
            if (JPEG_OK != jerr)
                return jerr;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_huff += (c1 - c0);
#endif

            // reconstruct a MCU row
#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            if(m_jpeg_precision == 12)
                jerr = ReconstructMCURowEX(pMCUBuf, colMCU, maxMCU);
            else
            {
                switch (m_jpeg_dct_scale)
                {
                default:
                case JD_1_1:
                    {
                        if(m_use_qdct)
                            jerr = ReconstructMCURowBL8x8_NxN(pMCUBuf, colMCU, maxMCU);
                        else
                            jerr = ReconstructMCURowBL8x8(pMCUBuf, colMCU, maxMCU);
                    }
                    break;

                case JD_1_2:
                    {
                        jerr = ReconstructMCURowBL8x8To4x4(pMCUBuf, colMCU, maxMCU);
                    }
                    break;

                case JD_1_4:
                    {
                        jerr = ReconstructMCURowBL8x8To2x2(pMCUBuf, colMCU, maxMCU);
                    }
                    break;

                case JD_1_8:
                    {
                        jerr = ReconstructMCURowBL8x8To1x1(pMCUBuf, colMCU, maxMCU);
                    }
                    break;
                }
            }

            if(JPEG_OK != jerr)
                continue;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_dct += c1 - c0;
#endif

#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif

            jerr = UpSampling(rowMCU, colMCU, maxMCU);
            if(JPEG_OK != jerr)
                continue;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_ss += c1 - c0;
#endif

#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            jerr = ColorConvert(rowMCU, colMCU, maxMCU);
            if(JPEG_OK != jerr)
                continue;
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_cc += c1 - c0;
#endif

            // increment interators
            if (m_curr_scan->jpeg_restart_interval)
            {
                m_mcu_decoded += (maxMCU - colMCU);
                m_mcu_to_decode -= (maxMCU - colMCU);
                if (0 == m_mcu_to_decode)
                {
                    return JPEG_ERR_BUFF;
                }
                maxMCU = (m_numxMCU < m_mcu_to_decode) ?
                         (m_numxMCU) :
                         (m_mcu_to_decode);
            }
            else
            {
                maxMCU = m_numxMCU;
            }

            rowMCU += 1;
            colMCU = 0;
        }
    }

    return JPEG_OK;

} // CJPEGDecoder::DecodeScanBaselineIN()


JERRCODE CJPEGDecoder::DecodeScanBaselineIN_P(void)
{
  IppStatus status;
  JERRCODE  jerr = JPEG_OK;
#ifdef __TIMING__
  Ipp64u   c0;
  Ipp64u   c1;
#endif

  status = ippiDecodeHuffmanStateInit_JPEG_8u(m_state);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  m_marker = JM_NONE;

  // workaround for 8-bit qnt tables in 12-bit scans
  if(m_qntbl[0].m_initialized && m_qntbl[0].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[0].ConvertToHighPrecision();

  if(m_qntbl[1].m_initialized && m_qntbl[1].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[1].ConvertToHighPrecision();

  if(m_qntbl[2].m_initialized && m_qntbl[2].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[2].ConvertToHighPrecision();

  if(m_qntbl[3].m_initialized && m_qntbl[3].m_precision == 0 && m_jpeg_precision == 12)
    m_qntbl[3].ConvertToHighPrecision();

  // workaround for 16-bit qnt tables in 8-bit scans
  if(m_qntbl[0].m_initialized && m_qntbl[0].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[0].ConvertToLowPrecision();

  if(m_qntbl[1].m_initialized && m_qntbl[1].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[1].ConvertToLowPrecision();

  if(m_qntbl[2].m_initialized && m_qntbl[2].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[2].ConvertToLowPrecision();

  if(m_qntbl[3].m_initialized && m_qntbl[3].m_precision == 1 && m_jpeg_precision == 8)
    m_qntbl[3].ConvertToLowPrecision();

  if(m_dctbl[0].IsEmpty())
  {
    jerr = m_dctbl[0].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_dctbl[0].Init(0,0,(Ipp8u*)&DefaultLuminanceDCBits[0],(Ipp8u*)&DefaultLuminanceDCValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_dctbl[1].IsEmpty())
  {
    jerr = m_dctbl[1].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_dctbl[1].Init(1,0,(Ipp8u*)&DefaultChrominanceDCBits[0],(Ipp8u*)&DefaultChrominanceDCValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_actbl[0].IsEmpty())
  {
    jerr = m_actbl[0].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_actbl[0].Init(0,1,(Ipp8u*)&DefaultLuminanceACBits[0],(Ipp8u*)&DefaultLuminanceACValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_actbl[1].IsEmpty())
  {
    jerr = m_actbl[1].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_actbl[1].Init(1,1,(Ipp8u*)&DefaultChrominanceACBits[0],(Ipp8u*)&DefaultChrominanceACValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

#ifdef _OPENMP
#pragma omp parallel default(shared) if(m_jpeg_sampling != JS_420)
#endif
  {
    int     i;
    int     idThread = 0;
    Ipp16s* pMCUBuf;  // the pointer to Buffer for a current thread.

#ifdef _OPENMP
    idThread = omp_get_thread_num(); // the thread id of the calling thread.
#endif

    pMCUBuf = m_block_buffer + idThread * m_numxMCU * m_nblock * DCTSIZE2;

    i = 0;

    while(i < (int) m_numyMCU)
    {
#ifdef _OPENMP
#pragma omp critical (IPP_JPEG_OMP)
#endif
      {
        if(i < (int) m_numyMCU)
        {
#ifdef __TIMING__
          c0 = ippGetCpuClocks();
#endif
          ippsZero_16s(pMCUBuf,m_numxMCU * m_nblock * DCTSIZE2);

          jerr = DecodeHuffmanMCURowBL(pMCUBuf, 0, m_numxMCU);
//          if(JPEG_OK != jerr)
//            i = m_numyMCU;
#ifdef __TIMING__
          c1 = ippGetCpuClocks();
          m_clk_huff += (c1 - c0);
#endif
        }
      }

      if(i < (int) m_numyMCU)
      {
#ifdef __TIMING__
        c0 = ippGetCpuClocks();
#endif
        if(m_jpeg_precision == 12)
          jerr = ReconstructMCURowEX(pMCUBuf, 0, m_numxMCU);
        else
        {
        switch (m_jpeg_dct_scale)
        {
          case JD_1_1:
          {
            if(m_use_qdct)
              jerr = ReconstructMCURowBL8x8_NxN(pMCUBuf, 0, m_numxMCU);
            else
              jerr = ReconstructMCURowBL8x8(pMCUBuf, 0, m_numxMCU);
          }
          break;

          case JD_1_2:
          {
            jerr = ReconstructMCURowBL8x8To4x4(pMCUBuf, 0, m_numxMCU);
          }
          break;

          case JD_1_4:
          {
            jerr = ReconstructMCURowBL8x8To2x2(pMCUBuf, 0, m_numxMCU);
          }
          break;

          case JD_1_8:
          {
            jerr = ReconstructMCURowBL8x8To1x1(pMCUBuf, 0, m_numxMCU);
          }
          break;

          default:
            break;
          }
        }

        if(JPEG_OK != jerr)
          continue;
#ifdef __TIMING__
        c1 = ippGetCpuClocks();
        m_clk_dct += c1 - c0;
#endif

#ifdef __TIMING__
        c0 = ippGetCpuClocks();
#endif

        jerr = ProcessBuffer(i,idThread);
        if(JPEG_OK != jerr)
          continue;

#ifdef __TIMING__
        c1 = ippGetCpuClocks();
        m_clk_ss += c1 - c0;
#endif
      }

      i++;
    } // for m_numyMCU
  } // OMP

  return JPEG_OK;
} // CJPEGDecoder::DecodeScanBaselineIN_P()


JERRCODE CJPEGDecoder::DecodeScanBaselineNI(void)
{
  int       i, j, k, l, c;
  int       srcLen;
  int       currPos;
  Ipp8u*    src;
  Ipp16s*   block;
  JERRCODE  jerr;
  IppStatus status;
#ifdef __TIMING__
  Ipp64u    c0;
  Ipp64u    c1;
#endif

  m_ac_scans_completed += m_curr_scan->ncomps;

  status = ippiDecodeHuffmanStateInit_JPEG_8u(m_state);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  m_marker = JM_NONE;

  src    = m_BitStreamIn.GetDataPtr();
  srcLen = m_BitStreamIn.GetDataLen();

  if(m_dctbl[0].IsEmpty())
  {
    jerr = m_dctbl[0].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_dctbl[0].Init(0,0,(Ipp8u*)&DefaultLuminanceDCBits[0],(Ipp8u*)&DefaultLuminanceDCValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_dctbl[1].IsEmpty())
  {
    jerr = m_dctbl[1].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_dctbl[1].Init(1,0,(Ipp8u*)&DefaultChrominanceDCBits[0],(Ipp8u*)&DefaultChrominanceDCValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_actbl[0].IsEmpty())
  {
    jerr = m_actbl[0].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_actbl[0].Init(0,1,(Ipp8u*)&DefaultLuminanceACBits[0],(Ipp8u*)&DefaultLuminanceACValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  if(m_actbl[1].IsEmpty())
  {
    jerr = m_actbl[1].Create();
    if(JPEG_OK != jerr)
      return jerr;

    jerr = m_actbl[1].Init(1,1,(Ipp8u*)&DefaultChrominanceACBits[0],(Ipp8u*)&DefaultChrominanceACValues[0]);
    if(JPEG_OK != jerr)
      return jerr;
  }

  for(i = 0; i < (int) m_numyMCU; i++)
  {
    for(k = 0; k < m_ccomp[m_curr_comp_no].m_vsampling; k++)
    {
      if(i*m_ccomp[m_curr_comp_no].m_vsampling*8 + k*8 >= m_jpeg_height)
        break;

      for(j = 0; j < (int) m_numxMCU; j++)
      {
        for(c = 0; c < m_curr_scan->ncomps; c++)
        {
          block = m_block_buffer + (DCTSIZE2*m_nblock*(j+(i*m_numxMCU)));

          // skip any relevant components
          for(c = 0; c < m_ccomp[m_curr_comp_no].m_comp_no; c++)
          {
            block += (DCTSIZE2*m_ccomp[c].m_nblocks);
          }

          // Skip over relevant 8x8 blocks from this component.
          block += (k * DCTSIZE2 * m_ccomp[m_curr_comp_no].m_hsampling);

          for(l = 0; l < m_ccomp[m_curr_comp_no].m_hsampling; l++)
          {
            // Ignore the last column(s) of the image.
            if(((j*m_ccomp[m_curr_comp_no].m_hsampling*8) + (l*8)) >= m_jpeg_width)
              break;

            if(m_curr_scan->jpeg_restart_interval)
            {
              if(m_restarts_to_go == 0)
              {
                jerr = ProcessRestart();
                if(JPEG_OK != jerr)
                {
                  LOG0("Error: ProcessRestart() failed!");
                  return jerr;
                }
              }
            }

            Ipp16s*                lastDC = &m_ccomp[m_curr_comp_no].m_lastDC;
            IppiDecodeHuffmanSpec* dctbl = m_dctbl[m_ccomp[m_curr_comp_no].m_dc_selector];
            IppiDecodeHuffmanSpec* actbl = m_actbl[m_ccomp[m_curr_comp_no].m_ac_selector];

            m_BitStreamIn.FillBuffer(SAFE_NBYTES);

            currPos = m_BitStreamIn.GetCurrPos();

#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            status = ippiDecodeHuffman8x8_JPEG_1u16s_C1(
                       src,srcLen,&currPos,block,lastDC,(int*)&m_marker,
                       dctbl,actbl,m_state);
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_huff += (c1 - c0);
#endif

            m_BitStreamIn.SetCurrPos(currPos);

            if(ippStsNoErr > status)
            {
              LOG0("Error: ippiDecodeHuffman8x8_JPEG_1u16s_C1() failed!");
              return JPEG_ERR_INTERNAL;
            }

            block += DCTSIZE2;

            m_restarts_to_go --;
          } // for m_hsampling
        } // for m_scan_ncomps
      } // for m_numxMCU
    } // for m_vsampling
  } // for m_numyMCU

  return JPEG_OK;
} // CJPEGDecoder::DecodeScanBaselineNI()


JERRCODE CJPEGDecoder::DecodeScanProgressive(void)
{
  int       i, j, k, n, l, c;
  int       srcLen;
  int       currPos;
  Ipp8u*    src;
  Ipp16s*   block;
  JERRCODE  jerr;
  IppStatus status;

  status = ippiDecodeHuffmanStateInit_JPEG_8u(m_state);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  m_marker = JM_NONE;

  src    = m_BitStreamIn.GetDataPtr();
  srcLen = m_BitStreamIn.GetDataLen();

  if(m_ss != 0 && m_se != 0)
  {
    // AC scan
    for(i = 0; i < (int) m_numyMCU; i++)
    {
      for(k = 0; k < m_ccomp[m_curr_comp_no].m_vsampling; k++)
      {
        if(i*m_ccomp[m_curr_comp_no].m_vsampling*8 + k*8 >= m_jpeg_height)
          break;

        for(j = 0; j < (int) m_numxMCU; j++)
        {
          block = m_block_buffer + (DCTSIZE2*m_nblock*(j+(i*m_numxMCU)));

          // skip any relevant components
          for(c = 0; c < m_ccomp[m_curr_comp_no].m_comp_no; c++)
          {
            block += (DCTSIZE2*m_ccomp[c].m_nblocks);
          }

          // Skip over relevant 8x8 blocks from this component.
          block += (k * DCTSIZE2 * m_ccomp[m_curr_comp_no].m_hsampling);

          for(l = 0; l < m_ccomp[m_curr_comp_no].m_hsampling; l++)
          {
            // Ignore the last column(s) of the image.
            if(((j*m_ccomp[m_curr_comp_no].m_hsampling*8) + (l*8)) >= m_jpeg_width)
              break;

            if(m_curr_scan->jpeg_restart_interval)
            {
              if(m_restarts_to_go == 0)
              {
                jerr = ProcessRestart();
                if(JPEG_OK != jerr)
                {
                  LOG0("Error: ProcessRestart() failed!");
                  return jerr;
                }
              }
            }

            IppiDecodeHuffmanSpec* actbl = m_actbl[m_ccomp[m_curr_comp_no].m_ac_selector];

            if(m_ah == 0)
            {
              m_BitStreamIn.FillBuffer(SAFE_NBYTES);

              currPos = m_BitStreamIn.GetCurrPos();

              status = ippiDecodeHuffman8x8_ACFirst_JPEG_1u16s_C1(
                         src,srcLen,&currPos,block,(int*)&m_marker,
                         m_ss,m_se,m_al,actbl,m_state);

              m_BitStreamIn.SetCurrPos(currPos);

              if(ippStsNoErr > status)
              {
                LOG0("Error: ippiDecodeHuffman8x8_ACFirst_JPEG_1u16s_C1() failed!");
                return JPEG_ERR_INTERNAL;
              }
            }
            else
            {
              m_BitStreamIn.FillBuffer(SAFE_NBYTES);

              currPos = m_BitStreamIn.GetCurrPos();

              status = ippiDecodeHuffman8x8_ACRefine_JPEG_1u16s_C1(
                         src,srcLen,&currPos,block,(int*)&m_marker,
                         m_ss,m_se,m_al,actbl,m_state);

              m_BitStreamIn.SetCurrPos(currPos);

              if(ippStsNoErr > status)
              {
                LOG0("Error: ippiDecodeHuffman8x8_ACRefine_JPEG_1u16s_C1() failed!");
                return JPEG_ERR_INTERNAL;
              }
            }

            block += DCTSIZE2;

            m_restarts_to_go --;
          } // for m_hsampling
        } // for m_numxMCU
      } // for m_vsampling
    } // for m_numyMCU

    if(m_al == 0 && m_se == 63)
    {
      m_ccomp[m_curr_comp_no].m_ac_scan_completed = 1;
    }
  }
  else
  {
    // DC scan
    for(i = 0; i < (int) m_numyMCU; i++)
    {
      for(j = 0; j < (int) m_numxMCU; j++)
      {
        if(m_curr_scan->jpeg_restart_interval)
        {
          if(m_restarts_to_go == 0)
          {
            jerr = ProcessRestart();
            if(JPEG_OK != jerr)
            {
              LOG0("Error: ProcessRestart() failed!");
              return jerr;
            }
          }
        }

        block = m_block_buffer + (DCTSIZE2*m_nblock*(j+(i*m_numxMCU)));

        if(m_ah == 0)
        {
          // first DC scan
          for(n = 0; n < m_jpeg_ncomp; n++)
          {
            Ipp16s*                lastDC = &m_ccomp[n].m_lastDC;
            IppiDecodeHuffmanSpec* dctbl  = m_dctbl[m_ccomp[n].m_dc_selector];

            for(k = 0; k < m_ccomp[n].m_vsampling; k++)
            {
              for(l = 0; l < m_ccomp[n].m_hsampling; l++)
              {
                m_BitStreamIn.FillBuffer(SAFE_NBYTES);

                currPos = m_BitStreamIn.GetCurrPos();

                status = ippiDecodeHuffman8x8_DCFirst_JPEG_1u16s_C1(
                           src,srcLen,&currPos,block,lastDC,(int*)&m_marker,
                           m_al,dctbl,m_state);

                m_BitStreamIn.SetCurrPos(currPos);

                if(ippStsNoErr > status)
                {
                  LOG0("Error: ippiDecodeHuffman8x8_DCFirst_JPEG_1u16s_C1() failed!");
                  return JPEG_ERR_INTERNAL;
                }

                block += DCTSIZE2;
              } // for m_hsampling
            } // for m_vsampling
          } // for m_jpeg_ncomp
        }
        else
        {
          // refine DC scan
          for(n = 0; n < m_jpeg_ncomp; n++)
          {
            for(k = 0; k < m_ccomp[n].m_vsampling; k++)
            {
              for(l = 0; l < m_ccomp[n].m_hsampling; l++)
              {
                m_BitStreamIn.FillBuffer(SAFE_NBYTES);

                currPos = m_BitStreamIn.GetCurrPos();

                status = ippiDecodeHuffman8x8_DCRefine_JPEG_1u16s_C1(
                           src,srcLen,&currPos,block,(int*)&m_marker,
                           m_al,m_state);

                m_BitStreamIn.SetCurrPos(currPos);

                if(ippStsNoErr > status)
                {
                  LOG0("Error: ippiDecodeHuffman8x8_DCRefine_JPEG_1u16s_C1() failed!");
                  return JPEG_ERR_INTERNAL;
                }

                block += DCTSIZE2;
              } // for m_hsampling
            } // for m_vsampling
          } // for m_jpeg_ncomp
        }
        m_restarts_to_go --;
      } // for m_numxMCU
    } // for m_numyMCU

    if(m_al == 0)
    {
      m_dc_scan_completed = 1;
    }
  }

  return JPEG_OK;
} // CJPEGDecoder::DecodeScanProgressive()


JERRCODE CJPEGDecoder::DecodeScanLosslessIN(void)
{
  int       i;
  Ipp16s*   pMCUBuf;
  JERRCODE  jerr;
  IppStatus status;
#ifdef __TIMING__
  Ipp64u    c0;
  Ipp64u    c1;
#endif

  status = ippiDecodeHuffmanStateInit_JPEG_8u(m_state);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  m_marker = JM_NONE;

  m_ac_scans_completed += m_curr_scan->ncomps;

  pMCUBuf = m_block_buffer;

  for(i = 0; i < (int) m_numyMCU; i++)
  {
#ifdef __TIMING__
    c0 = ippGetCpuClocks();
#endif
    jerr = DecodeHuffmanMCURowLS(pMCUBuf);
    if(JPEG_OK != jerr)
    {
      return jerr;
    }
#ifdef __TIMING__
    c1 = ippGetCpuClocks();
    m_clk_huff += c1 - c0;
#endif

#ifdef __TIMING__
    c0 = ippGetCpuClocks();
#endif
    jerr = ReconstructMCURowLS(pMCUBuf, i);
    if(JPEG_OK != jerr)
    {
      return jerr;
    }
#ifdef __TIMING__
    c1 = ippGetCpuClocks();
    m_clk_diff += c1 - c0;
#endif

    if(m_curr_scan->ncomps == m_jpeg_ncomp)
    {
#ifdef __TIMING__
      c0 = ippGetCpuClocks();
#endif
      jerr = ColorConvert(i, 0, m_numxMCU);
      if(JPEG_OK != jerr)
        return jerr;
#ifdef __TIMING__
      c1 = ippGetCpuClocks();
      m_clk_cc += c1 - c0;
#endif
    }
  } // for m_numyMCU

  return JPEG_OK;
} // CJPEGDecoder::DecodeScanLosslessIN()


JERRCODE CJPEGDecoder::DecodeScanLosslessNI(void)
{
  int       i, j, n, v, h;
  Ipp8u*    src;
  int       srcLen;
  int       currPos;
  Ipp16s*   ptr;
  Ipp16s*   pMCUBuf;
  JERRCODE  jerr;
  IppStatus status;
#ifdef __TIMING__
  Ipp64u    c0;
  Ipp64u    c1;
#endif

  status = ippiDecodeHuffmanStateInit_JPEG_8u(m_state);
  if(ippStsNoErr != status)
  {
    return JPEG_ERR_INTERNAL;
  }

  m_marker = JM_NONE;

  m_ac_scans_completed += m_curr_scan->ncomps;

  pMCUBuf = m_block_buffer + m_curr_comp_no*m_numxMCU*m_numyMCU;

  src    = m_BitStreamIn.GetDataPtr();
  srcLen = m_BitStreamIn.GetDataLen();

  for(i = 0; i < (int) m_numyMCU; i++)
  {
    for(j = 0; j < (int) m_numxMCU; j++)
    {
      if(m_curr_scan->jpeg_restart_interval)
      {
        if(m_restarts_to_go == 0)
        {
          jerr = ProcessRestart();
          if(JPEG_OK != jerr)
          {
            LOG0("Error: ProcessRestart() failed!");
            return jerr;
          }
        }
      }

      for(n = 0; n < m_curr_scan->ncomps; n++)
      {
        CJPEGColorComponent*   curr_comp = &m_ccomp[m_curr_comp_no];
        IppiDecodeHuffmanSpec* dctbl = m_dctbl[curr_comp->m_dc_selector];

        ptr = pMCUBuf + j*m_mcuWidth;

        for(v = 0; v < curr_comp->m_vsampling; v++)
        {
          for(h = 0; h < curr_comp->m_hsampling; h++)
          {
            m_BitStreamIn.FillBuffer(SAFE_NBYTES);

            currPos = m_BitStreamIn.GetCurrPos();

#ifdef __TIMING__
            c0 = ippGetCpuClocks();
#endif
            status = ippiDecodeHuffmanOne_JPEG_1u16s_C1(
              src,srcLen,&currPos,ptr,(int*)&m_marker,
              dctbl,m_state);
#ifdef __TIMING__
            c1 = ippGetCpuClocks();
            m_clk_huff += c1 - c0;
#endif

            m_BitStreamIn.SetCurrPos(currPos);

            if(ippStsNoErr > status)
            {
              LOG0("Error: ippiDecodeHuffmanOne_JPEG_1u16s_C1() failed!");
              return JPEG_ERR_INTERNAL;
            }

            ptr++;
          } // for m_hsampling
        } // for m_vsampling
      } // for m_jpeg_ncomp

      m_restarts_to_go --;
    } // for m_numxMCU

    pMCUBuf += m_numxMCU;
  } // for m_numyMCU

  return JPEG_OK;
} // CJPEGDecoder::DecodeScanLosslessNI()


JERRCODE CJPEGDecoder::ReadHeader(
  int*    width,
  int*    height,
  int*    nchannels,
  JCOLOR* color,
  JSS*    sampling,
  int*    precision)
{
  int      du_width;
  int      du_height;
  JERRCODE jerr;

  // parse bitstream up to SOS marker
  jerr = ParseJPEGBitStream(JO_READ_HEADER);

  if(JPEG_OK != jerr)
  {
    LOG0("Error: ParseJPEGBitStream() failed");
    return jerr;
  }

  if(JPEG_UNKNOWN == m_jpeg_mode)
    return JPEG_ERR_BAD_DATA;

  // DU block dimensions (8x8 for DCT based modes and 1x1 for lossless mode)
  du_width  = (JPEG_LOSSLESS == m_jpeg_mode) ? 1 : 8;
  du_height = (JPEG_LOSSLESS == m_jpeg_mode) ? 1 : 8;

  // MCU dimensions
  m_mcuWidth  = du_width  * IPP_MAX(m_max_hsampling,1);
  m_mcuHeight = du_height * IPP_MAX(m_max_vsampling,1);

  // num of MCUs in whole image
  m_numxMCU = (m_jpeg_width  + (m_mcuWidth  - 1)) / m_mcuWidth;
  m_numyMCU = (m_jpeg_height + (m_mcuHeight - 1)) / m_mcuHeight;

  // not completed MCUs should be padded
  m_xPadding = m_numxMCU * m_mcuWidth  - m_jpeg_width;
  m_yPadding = m_numyMCU * m_mcuHeight - m_jpeg_height;

  // dimensions of internal buffer for color conversion
  m_ccWidth  = m_mcuWidth * m_numxMCU;
  m_ccHeight = m_mcuHeight;

  *width     = m_jpeg_width;
  *height    = m_jpeg_height;
  *nchannels = m_jpeg_ncomp;
  *color     = m_jpeg_color;
  *sampling  = m_jpeg_sampling;
  *precision = m_jpeg_precision;

  return JPEG_OK;
} // CJPEGDecoder::ReadHeader()

JERRCODE CJPEGDecoder::ReadData(void)
{
    return ParseJPEGBitStream(JO_READ_DATA);

} // CJPEGDecoder::ReadData(void)

JERRCODE CJPEGDecoder::ReadData(Ipp32u restartNum, Ipp32u restartsToDecode)
{
    JERRCODE jerr = JPEG_OK;

    m_marker = JM_NONE;

    // find the start of VLC unit
    jerr = NextMarker(&m_marker);
    if (JPEG_OK != jerr)
    {
        return jerr;
    }

    // parse the VLC unit's header
    switch (m_marker)
    {
    case JM_SOS:
        jerr = ParseSOS(JO_READ_DATA);
        if (JPEG_OK != jerr)
        {
            return jerr;
        }
        break;

    case JM_RST0:
    case JM_RST1:
    case JM_RST2:
    case JM_RST3:
    case JM_RST4:
    case JM_RST5:
    case JM_RST6:
    case JM_RST7:
        jerr = ParseRST();
        if (JPEG_OK != jerr)
        {
            return jerr;
        }

        // reset DC predictors
        for (int n = 0; n < m_jpeg_ncomp; n++)
        {
            m_ccomp[n].m_lastDC = 0;
        }

        break;

    default:
        return JPEG_ERR_SOS_DATA;
        break;
    }

    // set the number of MCU to process
    if(0 == m_curr_scan->scan_no && 
        0 != m_scans[0].jpeg_restart_interval)
    {
        m_mcu_decoded = restartNum * m_scans[0].jpeg_restart_interval;
    }
    else if(1 == m_curr_scan->scan_no && 
        0 != m_scans[0].jpeg_restart_interval && 
        0 != m_scans[1].jpeg_restart_interval)
    {
        m_mcu_decoded = (restartNum - (m_scans[0].numxMCU * m_scans[0].numyMCU + m_scans[0].jpeg_restart_interval - 1) / m_scans[0].jpeg_restart_interval) * 
            m_scans[1].jpeg_restart_interval;
    }
    else if(2 == m_curr_scan->scan_no && 
        0 != m_scans[0].jpeg_restart_interval && 
        0 != m_scans[1].jpeg_restart_interval && 
        0 != m_scans[2].jpeg_restart_interval)
    {
        m_mcu_decoded = (restartNum - (m_scans[0].numxMCU * m_scans[0].numyMCU + m_scans[0].jpeg_restart_interval - 1) / m_scans[0].jpeg_restart_interval - 
            (m_scans[1].numxMCU * m_scans[1].numyMCU + m_scans[1].jpeg_restart_interval - 1) / m_scans[1].jpeg_restart_interval) * m_scans[2].jpeg_restart_interval;
    }
    else
    {
        m_mcu_decoded = 0;
    }

    m_mcu_to_decode = (m_curr_scan->jpeg_restart_interval) ?
                      (m_curr_scan->jpeg_restart_interval * restartsToDecode) :
                      (m_curr_scan->numxMCU * m_curr_scan->numyMCU);

    m_restarts_to_go = m_curr_scan->jpeg_restart_interval;

    // decode VLC unit data
    jerr = ParseData();

    return jerr;

} // CJPEGDecoder::ReadData(Ipp32u restartNum)

JERRCODE CJPEGDecoder::ReadPictureHeaders(void)
{
    return JPEG_OK;
}

ChromaType CJPEGDecoder::GetChromaType()
{
/*
 0:YUV400 (grayscale image)
    1:YUV420        Y: h=2 v=2, Cb/Cr: h=1 v=1
 2:YUV422H_2Y       Y: h=2 v=1, Cb/Cr: h=1 v=1
    3:YUV444        Y: h=1 v=1, Cb/Cr: h=1 v=1
    4:YUV411        Y: h=4 v=1, Cb/Cr: h=1 v=1
 5:YUV422V_2Y       Y: h=1 v=2, Cb/Cr: h=1 v=1
 6:YUV422H_4Y       Y: h=2 v=1, Cb/Cr: h=1 v=2
 7:YUV422V_4Y       Y: h=1 v=2, Cb/Cr: h=2 v=1
*/

    if (m_ccomp[0].m_hsampling == 4)
    {
        VM_ASSERT(m_ccomp[0].m_vsampling == 1);
        return CHROMA_TYPE_YUV411; // YUV411
    }

    if (m_ccomp[0].m_hsampling == 1) // YUV422V_4Y, YUV422V_2Y, YUV444
    {
        if (m_ccomp[0].m_vsampling == 1) // YUV444
        {
            return CHROMA_TYPE_YUV444;
        }
        else
        {
            VM_ASSERT(m_ccomp[0].m_vsampling == 2);
            return (m_ccomp[1].m_hsampling == 1) ? CHROMA_TYPE_YUV422V_2Y : CHROMA_TYPE_YUV422V_4Y; // YUV422V_2Y, YUV422V_4Y
        }
    }

    if (m_ccomp[0].m_hsampling == 2) // YUV420, YUV422H_2Y, YUV422H_4Y
    {
        if (m_ccomp[0].m_vsampling == 1)
        {
            return (m_ccomp[1].m_vsampling == 1) ? CHROMA_TYPE_YUV422H_2Y : CHROMA_TYPE_YUV422H_4Y; // YUV422H_2Y, YUV422H_4Y
        }
        else
        {
            VM_ASSERT(m_ccomp[0].m_vsampling == 2);
            return CHROMA_TYPE_YUV420; // YUV420;
        }
    }

    return CHROMA_TYPE_YUV400;
}

void CopyPlane4To2(Ipp8u  *pDstUV,
                   Ipp32s iDstStride,
                   const Ipp8u  *pSrcU,
                   const Ipp8u  *pSrcV,
                   Ipp32s iSrcStride,
                   IppiSize size)
{
    Ipp32s y, x;

    for (y = 0; y < size.height / 2; y += 1)
    {
        for (x = 0; x < size.width / 2; x += 1)
        {
            pDstUV[2*x] = pSrcU[x * 2];
            pDstUV[2*x + 1] = pSrcV[x * 2];
        }

        pSrcU += 2*iSrcStride;
        pSrcV += 2*iSrcStride;
        pDstUV += iDstStride;
    }

} // void CopyPlane(Ipp8u *pDst,

void DownSamplingPlane(Ipp8u  *pSrcU, Ipp8u  *pSrcV, Ipp32s iSrcStride, IppiSize size)
{
    Ipp32s y, x;

    for (y = 0; y < size.height / 2; y += 1)
    {
        for (x = 0; x < size.width / 2; x += 1)
        {
            pSrcU[x] = pSrcU[2*x];
            pSrcV[x] = pSrcV[2*x];
        }

        pSrcU += iSrcStride;
        pSrcV += iSrcStride;
    }

} // void CopyPlane(Ipp8u *pDst,

void CopyPlane(Ipp8u  *pDst,
               Ipp32s iDstStride,
               const Ipp8u  *pSrc,
               Ipp32s iSrcStride,
               IppiSize size)
{
    ippiCopy_8u_C1R(pSrc,
                    iSrcStride,
                    pDst,
                    iDstStride,
                    size);

} // void CopyPlane(Ipp8u *pDst,

void ConvertFrom_YUV444_To_YV12(const Ipp8u *src[3], Ipp32u srcPitch, Ipp8u * dst[2], Ipp32u dstPitch, IppiSize size)
{
    CopyPlane(dst[0],
              dstPitch,
              src[0],
              srcPitch,
              size);

    CopyPlane4To2(dst[1],
                  dstPitch,
                  src[1],
                  src[2],
                  srcPitch,
                  size);
}

void ConvertFrom_YUV422V_To_YV12(Ipp8u *src[3], Ipp32u srcPitch, Ipp8u * [2], Ipp32u , IppiSize size)
{
    DownSamplingPlane(src[1], src[2], srcPitch, size);
}

void ConvertFrom_YUV422H_4Y_To_NV12(const Ipp8u *src[3], Ipp32u srcPitch, Ipp8u * dst[2], Ipp32u dstPitch, IppiSize size)
{
    CopyPlane(dst[0],
              dstPitch,
              src[0],
              srcPitch,
              size);

    CopyPlane4To2(dst[1],
                  dstPitch,
                  src[1],
                  src[2],
                  srcPitch,
                  size);
}

void ConvertFrom_YUV422V_4Y_To_NV12(const Ipp8u *src[3], Ipp32u srcPitch, Ipp8u * dst[2], Ipp32u dstPitch, IppiSize size)
{
    CopyPlane(dst[0],
              dstPitch,
              src[0],
              srcPitch,
              size);

    CopyPlane4To2(dst[1],
                  dstPitch,
                  src[1],
                  src[2],
                  srcPitch,
                  size);
}


#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
