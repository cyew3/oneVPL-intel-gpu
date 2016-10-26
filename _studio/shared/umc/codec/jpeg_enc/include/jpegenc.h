//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __JPEGENC_H__
#define __JPEGENC_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_ENCODER)

#include "ippdefs.h"
#include "ippcore.h"
#include "ipps.h"
#include "ippi.h"
#include "ippj.h"
#include "basestreamout.h"
#include "jpegbase.h"
#include "encqtbl.h"
#include "enchtbl.h"
#include "colorcomp.h"
#include "bitstreamout.h"


class CBaseStreamOutput;

typedef struct _JPEG_SCAN
{
  int ncomp;
  int id[MAX_COMPS_PER_SCAN];
  int Ss;
  int Se;
  int Ah;
  int Al;

} JPEG_SCAN;


class CJPEGEncoder
{
public:

  CJPEGEncoder(void);
  virtual ~CJPEGEncoder(void);

  JERRCODE SetSource(
    Ipp8u*   pSrc,
    int      srcStep,
    IppiSize srcSize,
    int      srcChannels,
    JCOLOR   srcColor,
    JSS      srcSampling  = JS_444,
    int      srcPrecision = 8);

  JERRCODE SetSource(
    Ipp16s*  pSrc,
    int      srcStep,
    IppiSize srcSize,
    int      srcChannels,
    JCOLOR   srcColor,
    JSS      srcSampling = JS_444,
    int      srcPrecision = 16);

  JERRCODE SetSource(
    Ipp8u*   pSrc[4],
    int      srcStep[4],
    IppiSize srcSize,
    int      srcChannels,
    JCOLOR   srcColor,
    JSS      srcSampling  = JS_411,
    int      srcPrecision = 8);

  JERRCODE SetSource(
    Ipp16s*  pSrc[4],
    int      srcStep[4],
    IppiSize srcSize,
    int      srcChannels,
    JCOLOR   srcColor,
    JSS      srcSampling  = JS_444,
    int      srcPrecision = 16);

  JERRCODE SetDestination(
    CBaseStreamOutput* pStreamOut);

  JERRCODE SetParams(
             JMODE mode,
             JCOLOR color,
             JSS sampling,
             int restart_interval,
             int interleaved,
             int piecesCountInField,
             int piecePosInField,
             int numScan,
             int piecePosInScan,
             int huff_opt,
             int quality,
             JTMODE threading_mode = JT_OLD);

  JERRCODE SetParams(
             JMODE mode,
             JCOLOR color,
             JSS sampling,
             int restart_interval,
             int huff_opt,
             int point_transform,
             int predictor);

  JERRCODE InitHuffmanTable(Ipp8u bits[16], Ipp8u vals[256], int tbl_id, HTBL_CLASS tbl_class);
  JERRCODE InitQuantTable(Ipp8u  qnt[64], int tbl_id, int quality);
  JERRCODE InitQuantTable(Ipp16u qnt[64], int tbl_id, int quality);

  JERRCODE AttachHuffmanTable(int tbl_id, HTBL_CLASS tbl_class, int comp_no);
  JERRCODE AttachQuantTable(int tbl_id, int comp_no);

  JERRCODE WriteHeader(void);
  JERRCODE WriteData(void);

  int NumOfBytes(void) { return m_BitStreamOut.NumOfBytes(); }

  JERRCODE SetComment( int comment_size, char* comment = 0);
  JERRCODE SetJFIFApp0Resolution( JRESUNITS units, int xdensity, int ydensity);

#ifdef __TIMING__
  Ipp64u   m_clk_dct;
  Ipp64u   m_clk_ss;
  Ipp64u   m_clk_cc;
  Ipp64u   m_clk_diff;
  Ipp64u   m_clk_huff;
#endif

  Ipp16u   GetNumQuantTables(void);
  JERRCODE FillQuantTable(int numTable, Ipp16u* pTable);

  Ipp16u   GetNumACTables(void);
  JERRCODE FillACTable(int numTable, Ipp8u* pBits, Ipp8u* pValues);

  Ipp16u   GetNumDCTables(void);
  JERRCODE FillDCTable(int numTable, Ipp8u* pBits, Ipp8u* pValues);

  JERRCODE SetQuantTable(int numTable, Ipp16u* pTable);
  JERRCODE SetACTable(int numTable, Ipp8u* pBits, Ipp8u* pValues);
  JERRCODE SetDCTable(int numTable, Ipp8u* pBits, Ipp8u* pValues);

  JERRCODE SetDefaultQuantTable(Ipp16u quality);
  JERRCODE SetDefaultACTable();
  JERRCODE SetDefaultDCTable();

  bool     IsQuantTableInited();
  bool     IsACTableInited();
  bool     IsDCTableInited();

protected:
  IMAGE      m_src;

  CBitStreamOutput m_BitStreamOut;
  CBitStreamOutput* m_BitStreamOutT;

  int        m_jpeg_ncomp;
  int        m_jpeg_precision;
  JSS        m_jpeg_sampling;
  JCOLOR     m_jpeg_color;
  int        m_jpeg_quality;
  int        m_jpeg_restart_interval;
  JMODE      m_jpeg_mode;
  char*      m_jpeg_comment;

  int        m_numxMCU;
  int        m_numyMCU;
  int        m_mcuWidth;
  int        m_mcuHeight;
  int        m_ccWidth;
  int        m_ccHeight;
  int        m_xPadding;
  int        m_yPadding;

  int        m_num_scans;
  JSCAN      m_curr_scan;

  int        m_ss;
  int        m_se;
  int        m_al;
  int        m_ah;
  int        m_predictor;
  int        m_pt;
  int        m_optimal_htbl;
  JPEG_SCAN* m_scan_script;

  // Number of MCU already encoded
  Ipp32u     m_mcu_encoded;
  // Number of MCU remain in the current VLC unit
  Ipp32u     m_mcu_to_encode;

  Ipp16s*    m_block_buffer;
  int        m_block_buffer_size;
  int        m_num_threads;
  int        m_nblock;

  int        m_jfif_app0_xDensity;
  int        m_jfif_app0_yDensity;
  JRESUNITS  m_jfif_app0_units;

  int        m_num_rsti;
  int        m_rstiHeight;
  JTMODE     m_threading_mode;
  
  int        m_piecesCountInField;
  int        m_piecePosInField;
  int        m_piecePosInScan;

  bool       m_externalQuantTable;
  bool       m_externalHuffmanTable;

  Ipp16s**   m_lastDC;

  CJPEGEncoderHuffmanState*   m_state_t;

  CJPEGColorComponent        m_ccomp[MAX_COMPS_PER_SCAN];
  CJPEGEncoderQuantTable     m_qntbl[MAX_QUANT_TABLES];
  CJPEGEncoderHuffmanTable   m_dctbl[MAX_HUFF_TABLES];
  CJPEGEncoderHuffmanTable   m_actbl[MAX_HUFF_TABLES];
  CJPEGEncoderHuffmanState   m_state;

  JERRCODE Init(void);
  JERRCODE Clean(void);
  JERRCODE ColorConvert(Ipp32u rowMCU, Ipp32u colMCU, Ipp32u maxMCU/*int nMCURow, int thread_id = 0*/);
  JERRCODE DownSampling(Ipp32u rowMCU, Ipp32u colMCU, Ipp32u maxMCU/*int nMCURow, int thread_id = 0*/);

  JERRCODE WriteSOI(void);
  JERRCODE WriteEOI(void);
  JERRCODE WriteAPP0(void);
  JERRCODE WriteAPP14(void);
  JERRCODE WriteSOF0(void);
  JERRCODE WriteSOF1(void);
  JERRCODE WriteSOF2(void);
  JERRCODE WriteSOF3(void);
  JERRCODE WriteDRI(int restart_interval);
  JERRCODE WriteRST(int next_restart_num);
  JERRCODE WriteSOS(void);
  JERRCODE WriteSOS(int ncomp,int id[MAX_COMPS_PER_SCAN],int Ss,int Se,int Ah,int Al);
  JERRCODE WriteDQT(CJPEGEncoderQuantTable* tbl);
  JERRCODE WriteDHT(CJPEGEncoderHuffmanTable* tbl);
  JERRCODE WriteCOM(char* comment = 0);

  JERRCODE EncodeScanBaseline(void);

  JERRCODE EncodeScanBaselineRSTI(void);
  JERRCODE EncodeScanBaselineRSTI_P(void);

  JERRCODE EncodeHuffmanMCURowBL_RSTI(Ipp16s* pMCUBuf, int thread_id = 0);
  JERRCODE ProcessRestart(int id[MAX_COMPS_PER_SCAN],int Ss,int Se,int Ah,int Al, int nRSTI, int thread_id);
  JERRCODE WriteRST_T(int next_restart_num,  int thread_id = 0);

  JERRCODE EncodeScanExtended(void);
  JERRCODE EncodeScanExtended_P(void);
  JERRCODE EncodeScanLossless(void);
  JERRCODE EncodeScanProgressive(void);

  JERRCODE EncodeScan(int ncomp,int id[MAX_COMPS_PER_SCAN],int Ss,int Se,int Ah,int Al);
  JERRCODE SelectScanScripts(void);
  JERRCODE GenerateHuffmanTables(int ncomp,int id[MAX_COMPS_PER_SCAN],int Ss,int Se,int Ah,int Al);
  JERRCODE GenerateHuffmanTables(void);
  JERRCODE GenerateHuffmanTablesEX(void);

  JERRCODE ProcessRestart(int id[MAX_COMPS_PER_SCAN],int Ss,int Se,int Ah,int Al);
  JERRCODE ProcessRestart(int stat[2][256],int id[MAX_COMPS_PER_SCAN],int Ss,int Se,int Ah,int Al);

  JERRCODE EncodeHuffmanMCURowBL(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU);
  JERRCODE EncodeHuffmanMCURowLS(Ipp16s* pMCUBuf);

  JERRCODE TransformMCURowBL(Ipp16s* pMCUBuf, Ipp32u colMCU, Ipp32u maxMCU/*Ipp16s* pMCUBuf, int thread_id = 0*/);

  JERRCODE ProcessBuffer(Ipp32u rowMCU, Ipp32u colMCU, Ipp32u maxMCU);//(int nMCURow, int thread_id = 0);
  JERRCODE EncodeScanProgressive_P(void);

  JERRCODE TransformMCURowEX(Ipp16s* pMCUBuf, int thread_id = 0);
  JERRCODE TransformMCURowLS(Ipp16s* pMCUBuf, int nMCURow, int thread_id = 0);

};

#endif
#endif // __JPEGENC_H__
