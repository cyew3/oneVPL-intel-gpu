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

enum
{
    SAO_OPT_ALL_MODES       = 1,
    SAO_OPT_FAST_MODES_ONLY = 2
};

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
  int saoMaxOffsetQVal;
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
         int maxDepth,
         int bitDepth,
         int saoOpt);

     void Close(void);

     template <typename PixType>
     void EstimateCtuSao(
         mfxFrameData* orgYuv,
         mfxFrameData* recYuv,
         bool* sliceEnabled,
         SaoCtuParam* saoParam);

     void ReconstructCtuSaoParam(SaoCtuParam& recParam);


private:
    template <typename PixType>
    void GetCtuSaoStatistics(
        mfxFrameData* orgYuv,
        mfxFrameData* recYuv);

    void GetBestCtuSaoParam(
        bool* sliceEnabled,
        mfxFrameData* srcYuv,
        SaoCtuParam* codedParam);

    void GetMergeList(
        int ctu,
        SaoCtuParam* mergeList[2]);

    void ModeDecision_Base(
        SaoCtuParam* mergeList[2],
        bool* sliceEnabled,
        SaoCtuParam& bestParam,
        double& bestCost,
        int cabac);

    void ModeDecision_Merge(
        SaoCtuParam* mergeList[2],
        bool* sliceEnabled,
        SaoCtuParam& bestParam,
        double& bestCost,
        int cabac);

    int GetNumWrittenBits( void )
    {
        int bits = m_bsf->GetNumBits(); return bits / 256;
    }

// SaoState
public:
    // per stream param
    IppiSize m_frameSize;
    Ipp32s   m_maxCuSize;

    Ipp32s   m_numCTU_inWidth;
    Ipp32s   m_numCTU_inHeight;
    Ipp32s   m_numSaoModes;

    Ipp32s   m_bitDepth;
    Ipp32s   m_saoMaxOffsetQVal;

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

template <typename PixType>
class SaoDecodeFilterData
{
public:
    Ipp32s *m_OffsetEo;
    PixType *m_OffsetBo;
    PixType *m_TmpU[2];
    PixType *m_TmpL[2];
};

template <typename PixType>
class SaoDecodeFilter
{
public:
     SaoDecodeFilter(void);
     ~SaoDecodeFilter();

     void Init(
         int row_width_max,
         int maxCUWidth,
         int maxDepth,
         int bitDepth,
         int num);

     void Close(void);

     void SetOffsetsLuma(
         SaoOffsetParam  &saoLCUParam,
         Ipp32s typeIdx,
         Ipp32s dataIdx);

//private:
    static const int LUMA_GROUP_NUM = 32;
    static const int SAO_BO_BITS = 5;
    static const int SAO_PRED_SIZE = 66;

    Ipp32s   m_OffsetEo2[LUMA_GROUP_NUM];
    Ipp32s   m_OffsetEoChroma[LUMA_GROUP_NUM];
    Ipp32s   m_OffsetEo2Chroma[LUMA_GROUP_NUM];

    Ipp32u   m_maxCuSize;

    Ipp32s   m_bitDepth;

    PixType   *m_ClipTable;
    PixType   *m_ClipTableBase;
    PixType   *m_lumaTableBo;

    SaoDecodeFilterData<PixType> *m_saoData;

private:
    SaoDecodeFilter(const SaoDecodeFilter& ){ /* do not create copies */ }
    SaoDecodeFilter& operator=(const SaoDecodeFilter&){ return *this;}

};

} // namespace

#endif // __MFX_H265_SAO_FILTER_H__

#endif // (MFX_ENABLE_H265_VIDEO_ENCODE)
