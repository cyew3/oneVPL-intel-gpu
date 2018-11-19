// Copyright (c) 2011-2018 Intel Corporation
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
  
  virtual Status GetPerformance(double *perf);

  //reset skip frame counter
  virtual Status ResetSkipCount(void);

  // increment skip frame counter
  virtual Status SkipVideoFrame(int32_t);

  // get skip frame counter statistic
  virtual uint32_t GetNumOfSkippedFrames();
  
  virtual void SetFrameAllocator(FrameAllocator * frameAllocator);

private:

  Status InitBooleanDecoder(uint8_t *pBitStream, int32_t dataSize, int32_t dec_number);

  Status UpdateSegmentation(vp8BooleanDecoder *pBooldec);
  Status UpdateLoopFilterDeltas(vp8BooleanDecoder *pBooldec);

//  void SetMbQuant(vp8_MbInfo *pMb);

  void UpdateCoeffProbabilitites(void);

  Status DecodeInitDequantization(void);
  Status DecodeFrameHeader(MediaData* in);

  Status DecodeMbModes_Intra(vp8BooleanDecoder *pBoolDec);
  Status DecodeMbModesMVs_Inter(vp8BooleanDecoder *pBoolDec);

  Status DecodeFirstPartition(void);
  int32_t DecodeMbCoeffs(vp8BooleanDecoder *pBoolDec, int32_t mb_row, int32_t mb_col);

  void DecodeMbRow(vp8BooleanDecoder *pBoolDec, int32_t row);
  void ReconstructMbRow(vp8BooleanDecoder *pBoolDec, int32_t row);

  void DequantMbCoeffs(vp8_MbInfo* pMb);

  void InverseDCT_Mb(vp8_MbInfo* pMb, int16_t* pOut, uint32_t outStep); 

  void PredictMbIntra(int16_t*      pMbData, uint32_t dataStep, int32_t mb_row, int32_t mb_col);
  void PredictMbIntra4x4(int16_t*   pMbData, uint32_t dataStep, int32_t mb_row, int32_t mb_col);
  void PredictMbIntraUV(int16_t*    pMbData, uint32_t dataStep, int32_t mb_row, int32_t mb_col);
  void PredictMbIntra16x16(int16_t* pMbData, uint32_t dataStep, int32_t mb_row, int32_t mb_col);

  void PredictMbInter(int16_t*      pMbData, uint32_t dataStep, int32_t mb_row, int32_t mb_col);
  void PredictMbInter16x16(int16_t* pMbData, uint32_t dataStep, int32_t mb_row, int32_t mb_col);
  void PredictMbInter8x16(int16_t*  pMbData, uint32_t dataStep, int32_t mb_row, int32_t mb_col);
  void PredictMbInter16x8(int16_t*  pMbData, uint32_t dataStep, int32_t mb_row, int32_t mb_col);
  void PredictMbInter8x8(int16_t*   pMbData, uint32_t dataStep, int32_t mb_row, int32_t mb_col);
  void PredictMbInter4x4(int16_t*   pMbData, uint32_t dataStep, int32_t mb_row, int32_t mb_col);

  void LoopFilterNormal(void);
  void LoopFilterSimple(void);
  void LoopFilterInit(void);

  Status AllocFrames(void);
  void RefreshFrames(void);

  void ExtendFrameBorders(vp8_FrameData* currFrame);

  uint8_t DecodeValue_Prob128(vp8BooleanDecoder *pBooldec, uint32_t numbits);
  uint8_t DecodeValue(vp8BooleanDecoder *pBooldec, uint8_t prob, uint32_t numbits);

private:

  uint8_t                  m_IsInitialized;
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
  uint8_t                 m_RefFrameIndx[VP8_NUM_OF_REF_FRAMES];

  FrameAllocator *      m_frameAllocator;
};

} // namespace UMC


#endif // __UMC_VP8_DECODER__
#endif // UMC_ENABLE_VP8_VIDEO_DECODER