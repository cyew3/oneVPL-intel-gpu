//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#ifndef __UMC_VP8_DECODER__
#define __UMC_VP8_DECODER__

#include "umc_video_decoder.h"
#include "umc_media_buffer.h"
#include "umc_frame_data.h"
#include "umc_frame_allocator.h"

#include "vp8_dec_defs.h"
#include "vp8_bool_dec.h"
#include "ipps.h"

namespace UMC
{

class VP8VideoDecoder: public VideoDecoder
{

public:
   // Default constructor
  VP8VideoDecoder(void);

  // Default destructor
  virtual ~VP8VideoDecoder(void);

  // Initialize for subsequent frame decoding.
  virtual Status Init(BaseCodecParams *init);

  // Get next frame
  virtual Status GetFrame(MediaData* in, MediaData* out);
  virtual Status GetFrame(MediaData* in, FrameData **out);

  // Get video stream information, valid after initialization
  virtual Status GetInfo(BaseCodecParams* info);

  // Close  decoding & free all allocated resources
  virtual Status Close(void);

  // Reset decoder to initial state
  virtual Status Reset(void);

  virtual Status SetParams(BaseCodecParams* params);
  
  virtual Status GetPerformance(Ipp64f *perf);

  //reset skip frame counter
  virtual Status ResetSkipCount(void);

  // increment skip frame counter
  virtual Status SkipVideoFrame(Ipp32s);

  // get skip frame counter statistic
  virtual Ipp32u GetNumOfSkippedFrames();
  
  virtual void SetFrameAllocator(FrameAllocator * frameAllocator);

private:

  Status InitBooleanDecoder(Ipp8u *pBitStream, Ipp32s dataSize, Ipp32s dec_number);

  Status UpdateSegmentation(vp8BooleanDecoder *pBooldec);
  Status UpdateLoopFilterDeltas(vp8BooleanDecoder *pBooldec);

//  void SetMbQuant(vp8_MbInfo *pMb);

  void UpdateCoeffProbabilitites(void);

  Status DecodeInitDequantization(void);
  Status DecodeFrameHeader(MediaData* in);

  Status DecodeMbModes_Intra(vp8BooleanDecoder *pBoolDec);
  Status DecodeMbModesMVs_Inter(vp8BooleanDecoder *pBoolDec);

  Status DecodeFirstPartition(void);
  Ipp32s DecodeMbCoeffs(vp8BooleanDecoder *pBoolDec, Ipp32s mb_row, Ipp32s mb_col);

  void DecodeMbRow(vp8BooleanDecoder *pBoolDec, Ipp32s row);
  void ReconstructMbRow(vp8BooleanDecoder *pBoolDec, Ipp32s row);

  void DequantMbCoeffs(vp8_MbInfo* pMb);

  void InverseDCT_Mb(vp8_MbInfo* pMb, Ipp16s* pOut, Ipp32u outStep); 

  void PredictMbIntra(Ipp16s*      pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col);
  void PredictMbIntra4x4(Ipp16s*   pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col);
  void PredictMbIntraUV(Ipp16s*    pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col);
  void PredictMbIntra16x16(Ipp16s* pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col);

  void PredictMbInter(Ipp16s*      pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col);
  void PredictMbInter16x16(Ipp16s* pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col);
  void PredictMbInter8x16(Ipp16s*  pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col);
  void PredictMbInter16x8(Ipp16s*  pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col);
  void PredictMbInter8x8(Ipp16s*   pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col);
  void PredictMbInter4x4(Ipp16s*   pMbData, Ipp32u dataStep, Ipp32s mb_row, Ipp32s mb_col);

  void LoopFilterNormal(void);
  void LoopFilterSimple(void);
  void LoopFilterInit(void);

  Status AllocFrames(void);
  void RefreshFrames(void);

  void ExtendFrameBorders(vp8_FrameData* currFrame);

  Ipp8u DecodeValue_Prob128(vp8BooleanDecoder *pBooldec, Ipp32u numbits);
  Ipp8u DecodeValue(vp8BooleanDecoder *pBooldec, Ipp8u prob, Ipp32u numbits);

private:

  Ipp8u                  m_IsInitialized;
  vp8_MbInfo            *m_pMbInfo;
  vp8_MbInfo             m_MbExternal;
  vp8_FrameInfo          m_FrameInfo;
  vp8_QuantInfo          m_QuantInfo;
  vp8BooleanDecoder      m_BoolDecoder[VP8_MAX_NUMBER_OF_PARTITIONS];
  vp8_RefreshInfo        m_RefreshInfo;
  vp8_FrameProbabilities m_FrameProbs;
  vp8_FrameProbabilities m_FrameProbs_saved;

  VideoDecoderParams    m_Params;

  vp8_FrameData         m_FrameData[VP8_NUM_OF_REF_FRAMES];
  vp8_FrameData*        m_CurrFrame;
  Ipp8u                 m_RefFrameIndx[VP8_NUM_OF_REF_FRAMES];

  FrameAllocator *      m_frameAllocator;
};

} // namespace UMC


#endif // __UMC_VP8_DECODER__
#endif // UMC_ENABLE_VP8_VIDEO_DECODER