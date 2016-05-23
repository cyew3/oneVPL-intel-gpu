//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 - 2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_SAO_FILTER_H__
#define __MFX_H265_SAO_FILTER_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_bitstream.h"
#include "mfx_h265_optimization.h"
#include "mfx_h265_frame.h"

#include <vector>

namespace H265Enc {

#define NUM_SAO_BO_CLASSES_LOG2  5
#define NUM_SAO_BO_CLASSES  (1<<NUM_SAO_BO_CLASSES_LOG2)

#define NUM_SAO_EO_TYPES_LOG2 2
#define MAX_NUM_SAO_CLASSES  32

enum
{
    SAO_OPT_ALL_MODES       = 1,
    SAO_OPT_FAST_MODES_ONLY = 2,
    SAO_OPT_ONE_MODE_ONLY   = 3
};

enum SaoCabacStateMarkers
{
  SAO_CABACSTATE_BLK_CUR = 0,
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

// enable/disable merge mode of SAO
#define SAO_MODE_MERGE_ENABLED  (1)

struct SaoOffsetParam
{
  int mode_idx;     //ON, OFF, MERGE
  int type_idx;     //EO_0, EO_90, EO_135, EO_45, BO. MERGE: left, above
  int typeAuxInfo; //BO: starting band index
  int offset[MAX_NUM_SAO_CLASSES];
  int saoMaxOffsetQVal;

private:
  SaoOffsetParam& operator= (const SaoOffsetParam& src);
};


struct SaoCtuParam
{
  SaoOffsetParam& operator[](int compIdx){ return m_offsetParam[compIdx];}

private:
  SaoCtuParam& operator= (const SaoCtuParam& src);
  SaoOffsetParam m_offsetParam[NUM_SAO_COMPONENTS];
};


class SaoEstimator
{
public:
    void GetBestSao_RdoCost(bool* sliceEnabled, SaoCtuParam* mergeList[2], SaoCtuParam* codedParam);
    void GetBestSao_BitCost(bool* sliceEnabled, SaoCtuParam* mergeList[2], SaoCtuParam* codedParam);

    Ipp32s   m_numSaoModes;
    Ipp32s   m_bitDepth;
    Ipp32s   m_saoMaxOffsetQVal;
    MFX_HEVC_PP::SaoCtuStatistics m_statData[NUM_SAO_COMPONENTS][NUM_SAO_BASE_TYPES];
    Ipp8u    m_ctxSAO[NUM_SAO_CABACSTATE_MARKERS][NUM_CABAC_CONTEXT];
    Ipp32f   m_labmda[NUM_SAO_COMPONENTS];
    H265BsFake *m_bsf;
};


template <typename PixType>
class SaoApplier
{
public:
     SaoApplier();
     ~SaoApplier();

     void Init(int maxCUWidth, int format, int maxDepth, int bitDepth, int num);
     void Close(void);
     void SetOffsetsLuma(SaoOffsetParam  &saoLCUParam, Ipp32s typeIdx, Ipp32s *offsetEo, PixType *offsetBo);

     void h265_ProcessSaoCuChroma(PixType* pRec, Ipp32s stride, Ipp32s saoType, PixType* tmpL, PixType* tmpU, Ipp32u maxCUWidth, 
         Ipp32u maxCUHeight, Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEoCb, PixType* pOffsetBoCb, Ipp32s* pOffsetEoCr,
         PixType* pOffsetBoCr, PixType* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY);

    static const int LUMA_GROUP_NUM = 32;
    static const int SAO_BO_BITS = 5;
    static const int SAO_PRED_SIZE = 66;    

    Ipp32u   m_maxCuSize;
    Ipp32s   m_bitDepth;
    Ipp32s   m_format;

    PixType   *m_ClipTable;
    PixType   *m_ClipTableChroma;
    PixType   *m_ClipTableBase;
    PixType   *m_lumaTableBo;
    PixType   *m_chromaTableBo;

    PixType *m_TmpU;
    PixType *m_TmpL;

    PixType *m_TmpU_Chroma;
    PixType *m_TmpL_Chroma;

    Ipp32s   m_ctbCromaWidthInPix;
    Ipp32s   m_ctbCromaHeightInPix;
    Ipp32s   m_ctbCromaWidthInRow;
    Ipp32s   m_ctbCromaHeightInRow;

    Ipp32s m_inited;

private:
    SaoApplier(const SaoApplier& ){ /* do not create copies */ }
    SaoApplier& operator=(const SaoApplier&){ return *this;}
};
} // namespace



#endif // __MFX_H265_SAO_FILTER_H__
#endif // (MFX_ENABLE_H265_VIDEO_ENCODE)
