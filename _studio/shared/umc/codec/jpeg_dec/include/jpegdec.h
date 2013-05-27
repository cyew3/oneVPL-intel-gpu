/*
//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//    Copyright (c) 2001-2013 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __JPEGDEC_H__
#define __JPEGDEC_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#ifdef _OPENMP
#include "omp.h"
#endif
#ifndef __IPPDEFS_H__
#include "ippdefs.h"
#endif
#ifndef __IPPCORE_H__
#include "ippcore.h"
#endif
#ifndef __IPPS_H__
#include "ipps.h"
#endif
#ifndef __IPPI_H__
#include "ippi.h"
#endif
#ifndef __IPPCC_H__
#include "ippcc.h"
#endif
#ifndef __IPPJ_H__
#include "ippj.h"
#endif
#ifndef __JPEGBASE_H__
#include "jpegbase.h"
#endif
#ifndef __DECQTBL_H__
#include "decqtbl.h"
#endif
#ifndef __DECHTBL_H__
#include "dechtbl.h"
#endif
#ifndef __COLORCOMP_H__
#include "colorcomp.h"
#endif
#ifndef __BITSTREAMIN_H__
#include "bitstreamin.h"
#endif


class CBaseStreamInput;

namespace UMC
{
    class MJPEGVideoDecoderMFX;
    class MJPEGVideoDecoderMFX_HW;
};

class CJPEGDecoder
{
    friend class UMC::MJPEGVideoDecoderMFX;
    friend class UMC::MJPEGVideoDecoderMFX_HW;
public:

  CJPEGDecoder(void);
  virtual ~CJPEGDecoder(void);

  void Reset(void);

  JERRCODE SetSource(
    CBaseStreamInput* pStreamIn);

  JERRCODE SetDestination(
    Ipp8u*   pDst,
    int      dstStep,
    IppiSize dstSize,
    int      dstChannels,
    JCOLOR   dstColor,
    JSS      dstSampling = JS_444,
    int      dstPrecision = 8,
    JDD      dstDctScale = JD_1_1);

  JERRCODE SetDestination(
    Ipp16s*  pDst,
    int      dstStep,
    IppiSize dstSize,
    int      dstChannels,
    JCOLOR   dstColor,
    JSS      dstSampling = JS_444,
    int      dstPrecision = 16);

  JERRCODE SetDestination(
    Ipp8u*   pDst[4],
    int      dstStep[4],
    IppiSize dstSize,
    int      dstChannels,
    JCOLOR   dstColor,
    JSS      dstSampling = JS_420,
    int      dstPrecision = 8,
    JDD      dstDctScale = JD_1_1);

  JERRCODE SetDestination(
    Ipp16s*  pDst[4],
    int      dstStep[4],
    IppiSize dstSize,
    int      dstChannels,
    JCOLOR   dstColor,
    JSS      dstSampling = JS_444,
    int      dstPrecision = 16);

  JERRCODE ReadHeader(
    int*     width,
    int*     height,
    int*     nchannels,
    JCOLOR*  color,
    JSS*     sampling,
    int*     precision);

  JERRCODE ReadPictureHeaders(void);
  // Read the whole image data
  JERRCODE ReadData(void);
  // Read only VLC NAL data unit. Don't you mind my using h264 slang ? :)
  JERRCODE ReadData(Ipp32u restartNum, Ipp32u restartsToDecode);

  void SetInColor(JCOLOR color)        { m_jpeg_color = color; }
  void SetDCTType(int dct_type)        { m_use_qdct = dct_type; }
  void Comment(Ipp8u** buf, int* size) { *buf = m_jpeg_comment; *size = m_jpeg_comment_size; }
  int  GetNumDecodedBytes(void)        { return m_BitStreamIn.GetNumUsedBytes(); }

  JMODE Mode(void)                     { return m_jpeg_mode; }

  int    IsJPEGCommentDetected(void)   { return m_jpeg_comment_detected; }
  int    IsExifAPP1Detected(void)      { return m_exif_app1_detected; }
  Ipp8u* GetExifAPP1Data(void)         { return m_exif_app1_data; }
  int    GetExifAPP1DataSize(void)     { return m_exif_app1_data_size; }

  int    IsAVI1APP0Detected(void)      { return m_avi1_app0_detected; }
  int    GetAVI1APP0Polarity(void)     { return m_avi1_app0_polarity; }

  int    GetSOSLen(void)               { return m_sos_len; }

  Ipp16u   GetNumQuantTables(void);
  JERRCODE FillQuantTable(int numTable, Ipp16u* pTable);

  Ipp16u   GetNumACTables(void);
  JERRCODE FillACTable(int numTable, Ipp8u* pBits, Ipp8u* pValues);

  Ipp16u   GetNumDCTables(void);
  JERRCODE FillDCTable(int numTable, Ipp8u* pBits, Ipp8u* pValues);

  bool     IsInterleavedScan(void);

protected:
  int       m_jpeg_width;
  int       m_jpeg_height;
  int       m_jpeg_ncomp;
  int       m_jpeg_precision;
  JSS       m_jpeg_sampling;
  JCOLOR    m_jpeg_color;
  int       m_jpeg_quality;
  JMODE     m_jpeg_mode;

  JDD       m_jpeg_dct_scale;
  int       m_dd_factor;
  int       m_use_qdct;

  // JPEG embedded comments variables
  int      m_jpeg_comment_detected;
  int      m_jpeg_comment_size;
  Ipp8u*   m_jpeg_comment;

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

  // Exif APP1 related variables
  int      m_exif_app1_detected;
  int      m_exif_app1_data_size;
  Ipp8u*   m_exif_app1_data;

  // Adobe APP14 related variables
  int      m_adobe_app14_detected;
  int      m_adobe_app14_version;
  int      m_adobe_app14_flags0;
  int      m_adobe_app14_flags1;
  int      m_adobe_app14_transform;

  int      m_precision;
  int      m_max_hsampling;
  int      m_max_vsampling;
  Ipp32u   m_numxMCU;
  Ipp32u   m_numyMCU;
  int      m_mcuWidth;
  int      m_mcuHeight;
  int      m_ccWidth;
  int      m_ccHeight;
  int      m_xPadding;
  int      m_yPadding;
  int      m_rst_go;
  // Number of MCU already decoded
  Ipp32u   m_mcu_decoded;
  // Number of MCU remain in the current VLC unit
  Ipp32u   m_mcu_to_decode;
  int      m_restarts_to_go;
  int      m_next_restart_num;
  int      m_sos_len;
  int      m_curr_comp_no;
  int      m_num_scans;
  JSCAN    m_scans[MAX_SCANS_PER_FRAME];
  JSCAN*   m_curr_scan;
  int      m_ss;
  int      m_se;
  int      m_al;
  int      m_ah;
  int      m_dc_scan_completed;
  int      m_ac_scans_completed;
  int      m_init_done;
  JMARKER  m_marker;

  Ipp16s*  m_block_buffer;
  int      m_num_threads;
  int      m_nblock;
  int      m_sof_find;

#ifdef __TIMING__
  Ipp64u   m_clk_dct;

  Ipp64u   m_clk_dct1x1;
  Ipp64u   m_clk_dct2x2;
  Ipp64u   m_clk_dct4x4;
  Ipp64u   m_clk_dct8x8;

  Ipp64u   m_clk_ss;
  Ipp64u   m_clk_cc;
  Ipp64u   m_clk_diff;
  Ipp64u   m_clk_huff;
#endif

  IMAGE                       m_dst;
  CBitStreamInput             m_BitStreamIn;
  CJPEGColorComponent         m_ccomp[MAX_COMPS_PER_SCAN];
  CJPEGDecoderQuantTable      m_qntbl[MAX_QUANT_TABLES];
  CJPEGDecoderHuffmanTable    m_dctbl[MAX_HUFF_TABLES];
  CJPEGDecoderHuffmanTable    m_actbl[MAX_HUFF_TABLES];
  CJPEGDecoderHuffmanState    m_state;

protected:
  JERRCODE Init(void);
  JERRCODE Clean(void);
  JERRCODE ColorConvert(Ipp32u rowCMU, Ipp32u colMCU, Ipp32u maxMCU);
  JERRCODE UpSampling(Ipp32u rowMCU, Ipp32u colMCU, Ipp32u maxMCU);

  JERRCODE FindNextImage();
  JERRCODE FindSOI();

  JERRCODE ParseData();
  JERRCODE ParseJPEGBitStream(JOPERATION op);
  JERRCODE ParseSOI(void);
  JERRCODE ParseEOI(void);
  JERRCODE ParseAPP0(void);
  JERRCODE ParseAPP1(void);
  JERRCODE ParseAPP14(void);
  JERRCODE ParseSOF0(void);
  JERRCODE ParseSOF1(void);
  JERRCODE ParseSOF2(void);
  JERRCODE ParseSOF3(void);
  JERRCODE ParseDRI(void);
  JERRCODE ParseRST(void);
  JERRCODE ParseSOS(JOPERATION op);
  JERRCODE ParseDQT(void);
  JERRCODE ParseDHT(void);
  JERRCODE ParseCOM(void);

  JERRCODE DecodeScanBaseline(void);     // interleaved / non-interleaved scans
  JERRCODE DecodeScanBaselineIN(void);   // interleaved scan
  JERRCODE DecodeScanBaselineIN_P(void); // interleaved scan for plane image
  JERRCODE DecodeScanBaselineNI(void);   // non-interleaved scan
  JERRCODE DecodeScanLosslessIN(void);
  JERRCODE DecodeScanLosslessNI(void);
  JERRCODE DecodeScanProgressive(void);

  JERRCODE ProcessRestart(void);

  JERRCODE NextMarker(JMARKER* marker);
  JERRCODE SkipMarker(void);

  // huffman decode mcu row lossless process
  JERRCODE DecodeHuffmanMCURowLS(Ipp16s* pMCUBuf);

  // huffman decode mcu row baseline process
  JERRCODE DecodeHuffmanMCURowBL(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU);

  // inverse DCT, de-quantization, level-shift for mcu row
  JERRCODE ReconstructMCURowBL8x8_NxN(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU);
  JERRCODE ReconstructMCURowBL8x8(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU);
  JERRCODE ReconstructMCURowBL8x8To4x4(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU);
  JERRCODE ReconstructMCURowBL8x8To2x2(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU);
  JERRCODE ReconstructMCURowBL8x8To1x1(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU);
  JERRCODE ReconstructMCURowEX(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU);

  JERRCODE ProcessBuffer(int nMCURow, int thread_id = 0);
  // reconstruct mcu row lossless process
  JERRCODE ReconstructMCURowLS(Ipp16s* pMCUBuf, int nMCURow,int thread_id = 0);

  JERRCODE DetectSampling(void);

  ChromaType GetChromaType();
};

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif // __JPEGDEC_H__
