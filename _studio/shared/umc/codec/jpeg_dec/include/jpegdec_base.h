//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __JPEGDEC_BASE_H__
#define __JPEGDEC_BASE_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#include "jpegbase.h"
#include "decqtbl.h"
#include "dechtbl.h"
#include "colorcomp.h"
#include "bitstreamin.h"

class CJPEGDecoderBase
{
public:

  CJPEGDecoderBase(void);
  virtual ~CJPEGDecoderBase(void);

  virtual void Reset(void);

  JERRCODE SetSource(
    CBaseStreamInput* pStreamIn);

  virtual JERRCODE ReadHeader(
    int*     width,
    int*     height,
    int*     nchannels,
    JCOLOR*  color,
    JSS*     sampling,
    int*     precision);

  int  GetNumDecodedBytes(void)        { return m_BitStreamIn.GetNumUsedBytes(); }

  int    GetSOSLen(void)               { return m_sos_len; }

  Ipp16u   GetNumQuantTables(void);
  JERRCODE FillQuantTable(int numTable, Ipp16u* pTable);

  Ipp16u   GetNumACTables(void);
  JERRCODE FillACTable(int numTable, Ipp8u* pBits, Ipp8u* pValues);

  Ipp16u   GetNumDCTables(void);
  JERRCODE FillDCTable(int numTable, Ipp8u* pBits, Ipp8u* pValues);

  bool     IsInterleavedScan(void);

public:
  int       m_jpeg_width;
  int       m_jpeg_height;
  int       m_jpeg_ncomp;
  int       m_jpeg_precision;
  JSS       m_jpeg_sampling;
  JCOLOR    m_jpeg_color;
  JMODE     m_jpeg_mode;

  // JFIF APP0 related varibales
  int      m_jfif_app0_detected;
  int      m_jfif_app0_major;
  int      m_jfif_app0_minor;
  int      m_jfif_app0_units;
  int      m_jfif_app0_xDensity;
  int      m_jfif_app0_yDensity;
  int      m_jfif_app0_thumb_width;
  int      m_jfif_app0_thumb_height;

  // JFXX APP0 related variables
  int      m_jfxx_app0_detected;
  int      m_jfxx_thumbnails_type;

  // AVI1 APP0 related variables
  int      m_avi1_app0_detected;
  int      m_avi1_app0_polarity;
  int      m_avi1_app0_reserved;
  int      m_avi1_app0_field_size;
  int      m_avi1_app0_field_size2;

  // Adobe APP14 related variables
  int      m_adobe_app14_detected;
  int      m_adobe_app14_version;
  int      m_adobe_app14_flags0;
  int      m_adobe_app14_flags1;
  int      m_adobe_app14_transform;

  int      m_precision;
  int      m_max_hsampling;
  int      m_max_vsampling;
  // Number of MCU remain in the current VLC unit
  int      m_sos_len;
  int      m_curr_comp_no;
  int      m_num_scans;
  JSCAN    m_scans[MAX_SCANS_PER_FRAME];
  JSCAN*   m_curr_scan;
  int      m_ss;
  int      m_se;
  int      m_al;
  int      m_ah;
  JMARKER  m_marker;

  int      m_nblock;

  CBitStreamInput             m_BitStreamIn;
  CJPEGColorComponent         m_ccomp[MAX_COMPS_PER_SCAN];
  CJPEGDecoderQuantTable      m_qntbl[MAX_QUANT_TABLES];
  CJPEGDecoderHuffmanTable    m_dctbl[MAX_HUFF_TABLES];
  CJPEGDecoderHuffmanTable    m_actbl[MAX_HUFF_TABLES];

public:
  virtual JERRCODE Clean(void);

  JERRCODE FindSOI();

  virtual JERRCODE ParseJPEGBitStream(JOPERATION op);
  JERRCODE ParseSOI(void);
  JERRCODE ParseEOI(void);
  JERRCODE ParseAPP0(void);
  JERRCODE ParseAPP14(void);
  JERRCODE ParseSOF0(void);
  JERRCODE ParseDRI(void);
  JERRCODE ParseSOS(JOPERATION op);
  JERRCODE ParseDQT(void);
  JERRCODE ParseDHT(void);

  JERRCODE NextMarker(JMARKER* marker);
  JERRCODE SkipMarker(void);

  JERRCODE DetectSampling(void);
};

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif // __JPEGDEC_BASE_H__
