//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 - 2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_SAO_FILTER_H__
#define __MFX_H265_SAO_FILTER_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_bitstream.h"
#include "mfx_h265_optimization.h"

#include <vector>

namespace H265Enc {

//#define MAX_SAO_TRUNCATED_BITDEPTH     (10)

//#define SAO_ENCODE_ALLOW_USE_PREDEBLOCK (0)
//
//#define SAO_ENCODING_CHOICE              0  ///< I0184: picture early termination

# define DISTORTION_PRECISION_ADJUSTMENT(x) (x)

#define NUM_SAO_BO_CLASSES_LOG2  5
#define NUM_SAO_BO_CLASSES  (1<<NUM_SAO_BO_CLASSES_LOG2)

#define NUM_SAO_EO_TYPES_LOG2 2

#define MAX_NUM_SAO_CLASSES  32  //(NUM_SAO_EO_GROUPS > NUM_SAO_BO_GROUPS)?NUM_SAO_EO_GROUPS:NUM_SAO_BO_GROUPS

#define SAO_MAX_OFFSET_QVAL (7)

enum SaoCabacStateMarkers
{
  SAO_CABACSTATE_BLK_CUR = 0,
  SAO_CABACSTATE_BLK_NEXT,
  SAO_CABACSTATE_BLK_MID,
  SAO_CABACSTATE_BLK_TEMP,
  NUM_SAO_CABACSTATE_MARKERS
};


enum SaoEOClasses
{
  SAO_CLASS_EO_FULL_VALLEY = 0,
  SAO_CLASS_EO_HALF_VALLEY = 1,
  SAO_CLASS_EO_PLAIN       = 2,
  SAO_CLASS_EO_HALF_PEAK   = 3,
  SAO_CLASS_EO_FULL_PEAK   = 4,
  NUM_SAO_EO_CLASSES,
};


enum SaoModes
{
  SAO_MODE_OFF = 0,
  SAO_MODE_ON,
  SAO_MODE_MERGE,
  NUM_SAO_MODES
};


enum SaoMergeTypes
{
  SAO_MERGE_LEFT =0,
  SAO_MERGE_ABOVE,
  NUM_SAO_MERGE_TYPES
};


enum SaoBaseTypes
{
  SAO_TYPE_EO_0 = 0,
  SAO_TYPE_EO_90,
  SAO_TYPE_EO_135,
  SAO_TYPE_EO_45,

  SAO_TYPE_BO,

  NUM_SAO_BASE_TYPES
};


enum SaoComponentIdx
{
  SAO_Y =0,
  SAO_Cb,
  SAO_Cr,
  NUM_SAO_COMPONENTS
};

//---------------------------------------------------------
// configuration of SAO algorithm:
// how many components are used for SAO analysis: 1 - Y only, 2 - invalid, 3 - YUV
#define NUM_USED_SAO_COMPONENTS (1)

// enable/disable merge mode of SAO
//#define SAO_MODE_MERGE_ENABLED  (1)

// predeblocked statistics
//#define MFX_HEVC_SAO_PREDEBLOCKED_ENABLED (1)
//---------------------------------------------------------

//struct SaoCtuStatistics //data structure for SAO statistics
//{
//  Ipp64s diff[MAX_NUM_SAO_CLASSES];
//  Ipp64s count[MAX_NUM_SAO_CLASSES];
//
//  SaoCtuStatistics(){}
//  ~SaoCtuStatistics(){}
//
//  void Reset()
//  {
//    memset(diff, 0, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
//    memset(count, 0, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
//  }
//
//  //const
//  SaoCtuStatistics& operator=(const SaoCtuStatistics& src)
//  {
//    small_memcpy(diff, src.diff, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
//    small_memcpy(count, src.count, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
//
//    return *this;
//  }
//
//};


struct SaoOffsetParam
{
  SaoOffsetParam();
  ~SaoOffsetParam();
  void Reset();

  //const
  SaoOffsetParam& operator= (const SaoOffsetParam& src);

  int mode_idx;     //ON, OFF, MERGE
  int type_idx;     //EO_0, EO_90, EO_135, EO_45, BO. MERGE: left, above
  int typeAuxInfo; //BO: starting band index
  int offset[MAX_NUM_SAO_CLASSES];

};


struct SaoCtuParam
{
  SaoCtuParam();
  ~SaoCtuParam();
  void Reset();

  //const
  SaoCtuParam& operator= (const SaoCtuParam& src);

  SaoOffsetParam& operator[](int compIdx){ return m_offsetParam[compIdx];}

private:
  SaoOffsetParam m_offsetParam[NUM_SAO_COMPONENTS];

};


class SaoEncodeFilter
{
public:
     SaoEncodeFilter(void);
     ~SaoEncodeFilter();

     void Init(
         int width,
         int height,
         int maxCUWidth,
         int maxDepth);

     void Close(void);

     void EstimateCtuSao(
         mfxFrameData* orgYuv,
         mfxFrameData* recYuv,
         bool* sliceEnabled,
         SaoCtuParam* saoParam);

     void ReconstructCtuSaoParam(SaoCtuParam& recParam);

     void ApplyCtuSao(
        mfxFrameData* srcYuv,
        mfxFrameData* resYuv,
        SaoCtuParam& saoParam,
        int ctu);

private:
    void GetCtuSaoStatistics(
        mfxFrameData* orgYuv,
        mfxFrameData* recYuv);

    void GetBestCtuSaoParam(
        bool* sliceEnabled,
        mfxFrameData* srcYuv,
        SaoCtuParam* codedParam);

    int getMergeList(
        int ctu,
        SaoCtuParam* blkParams,
        std::vector<SaoCtuParam*>& mergeList);

    void ModeDecision_Base(
        std::vector<SaoCtuParam*>& mergeList,
        bool* sliceEnabled,
        SaoCtuParam& modeParam,
        double& modeNormCost,
        int inCabacLabel);

    void ModeDecision_Merge(
        std::vector<SaoCtuParam*>& mergeList,
        bool* sliceEnabled,
        SaoCtuParam& modeParam,
        double& modeNormCost,
        int inCabacLabel);

    int GetNumWrittenBits( void )
    {
        int bits = m_bsf->GetNumBits(); return bits / 256;
    }

// SaoState
public:
    // per stream param
    IppiSize m_frameSize;
    Ipp32s   m_maxCUSize;

    Ipp32s   m_numCTU_inWidth;
    Ipp32s   m_numCTU_inHeight;

    // work state
    MFX_HEVC_PP::SaoCtuStatistics    m_statData[NUM_SAO_COMPONENTS][NUM_SAO_BASE_TYPES];
    Ipp8u               m_ctxSAO[NUM_SAO_CABACSTATE_MARKERS][NUM_CABAC_CONTEXT];

    MFX_HEVC_PP::SaoCtuStatistics    m_statData_predeblocked[NUM_SAO_COMPONENTS][NUM_SAO_BASE_TYPES];

public:
    // per CTU param
    int  m_ctb_addr;
    int  m_ctb_pelx;
    int  m_ctb_pely;

    double   m_labmda[NUM_SAO_COMPONENTS];
    H265BsFake *m_bsf;

    MFX_HEVC_PP::CTBBorders m_borders;

    Ipp8u* m_slice_ids;

    // output
    SaoCtuParam* m_codedParams_TotalFrame;
};

class SaoDecodeFilter
{
public:
     SaoDecodeFilter(void);
     ~SaoDecodeFilter();

     void Init(
         int width,
         int height,
         int maxCUWidth,
         int maxDepth);

     void Close(void);

     void SetOffsetsLuma(
         SaoOffsetParam  &saoLCUParam,
         Ipp32s typeIdx);

     Ipp8u   *m_TmpU[2];
     Ipp8u   *m_TmpL[2];

//private:
    static const int LUMA_GROUP_NUM = 32;
    static const int SAO_BO_BITS = 5;
    static const int SAO_PRED_SIZE = 66;

    Ipp32s   m_OffsetEo[LUMA_GROUP_NUM];
    Ipp32s   m_OffsetEo2[LUMA_GROUP_NUM];
    Ipp32s   m_OffsetEoChroma[LUMA_GROUP_NUM];
    Ipp32s   m_OffsetEo2Chroma[LUMA_GROUP_NUM];

    Ipp8u   *m_OffsetBo;
    /*Ipp8u   *m_OffsetBo2;
    Ipp8u   *m_OffsetBoChroma;
    Ipp8u   *m_OffsetBo2Chroma;*/
    Ipp8u   *m_ClipTable;
    Ipp8u   *m_ClipTableBase;
    Ipp8u   *m_lumaTableBo;

    Ipp32u   m_PicWidth;
    Ipp32u   m_PicHeight;
    Ipp32u   m_MaxCUSize;
    Ipp32u   m_SaoBitIncreaseY;
    Ipp32u   m_SaoBitIncreaseC;

private:
    SaoDecodeFilter(const SaoDecodeFilter& ){ /* do not create copies */ }
    SaoDecodeFilter& operator=(const SaoDecodeFilter&){ return *this;}

};

} // namespace

#endif // __MFX_H265_SAO_FILTER_H__

#endif // (MFX_ENABLE_H265_VIDEO_ENCODE)
