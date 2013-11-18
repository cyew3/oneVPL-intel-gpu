//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_SAO_FILTER_H__
#define __MFX_H265_SAO_FILTER_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_cabac.h"

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


struct SaoCtuStatistics //data structure for SAO statistics
{
  Ipp64s diff[MAX_NUM_SAO_CLASSES];
  Ipp64s count[MAX_NUM_SAO_CLASSES];

  SaoCtuStatistics(){}
  ~SaoCtuStatistics(){}

  void Reset()
  {
    memset(diff, 0, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
    memset(count, 0, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
  }

  const SaoCtuStatistics& operator=(const SaoCtuStatistics& src)
  {
    memcpy(diff, src.diff, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);
    memcpy(count, src.count, sizeof(Ipp64s)*MAX_NUM_SAO_CLASSES);

    return *this;
  }

};


struct SaoOffsetParam
{
  SaoOffsetParam();
  ~SaoOffsetParam();
  void Reset();

  const SaoOffsetParam& operator= (const SaoOffsetParam& src);

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
  const SaoCtuParam& operator= (const SaoCtuParam& src);
  SaoOffsetParam& operator[](int compIdx){ return m_offsetParam[compIdx];}

private:
  SaoOffsetParam m_offsetParam[NUM_SAO_COMPONENTS];

};


class SAOFilter
{
public:
     SAOFilter(void);
     ~SAOFilter();

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
    // per stream param
    IppiSize m_frameSize;
    Ipp32s   m_maxCUSize;

    Ipp32s   m_numCTU_inWidth;
    Ipp32s   m_numCTU_inHeight;

    // work state
    SaoCtuStatistics    m_statData[NUM_SAO_COMPONENTS][NUM_SAO_BASE_TYPES];
    Ipp8u               m_ctxSAO[NUM_SAO_CABACSTATE_MARKERS][NUM_CABAC_CONTEXT];

public:
    // per CTU param
    int  m_ctb_addr;
    int  m_ctb_pelx;
    int  m_ctb_pely;

    double   m_labmda[NUM_SAO_COMPONENTS];
    H265BsFake *m_bsf;

    // output
    SaoCtuParam* m_codedParams_TotalFrame;
};

// ========================================================
// CABAC SAO
// ========================================================

template <class H265Bs>
static
    void h265_code_sao_merge(H265Bs *bs, Ipp32u code)
{
    bs->EncodeSingleBin_CABAC(CTX(bs,SAO_MERGE_FLAG_HEVC),code);
}

template <class H265Bs>
void h265_code_sao_ctb_param(
    H265Bs *bs,
    SaoCtuParam& saoBlkParam,
    bool* sliceEnabled,
    bool leftMergeAvail,
    bool aboveMergeAvail,
    bool onlyEstMergeInfo)
{
    bool isLeftMerge = false;
    bool isAboveMerge= false;
    Ipp32u code = 0;

    if(leftMergeAvail)
    {
        isLeftMerge = ((saoBlkParam[SAO_Y].mode_idx == SAO_MODE_MERGE) && (saoBlkParam[SAO_Y].type_idx == SAO_MERGE_LEFT));
        code = isLeftMerge ? 1 : 0;
        h265_code_sao_merge(bs, code);
    }

    if( aboveMergeAvail && !isLeftMerge)
    {
        isAboveMerge = ((saoBlkParam[SAO_Y].mode_idx == SAO_MODE_MERGE) && (saoBlkParam[SAO_Y].type_idx == SAO_MERGE_ABOVE));
        code = isAboveMerge ? 1 : 0;
        h265_code_sao_merge(bs, code);
    }

    if(onlyEstMergeInfo)
    {
        return; //only for RDO
    }

    if(!isLeftMerge && !isAboveMerge) //not merge mode
    {
        for(int compIdx=0; compIdx < 1; compIdx++)
        {
            h265_code_sao_ctb_offset_param(bs, compIdx, saoBlkParam[compIdx], sliceEnabled[compIdx]);
        }
    }

} // void h265_code_sao_ctb_param( ... )


// ========================================================
// SAO
// ========================================================
template <class H265Bs>
static inline
    void h265_code_sao_sign(H265Bs *bs,  Ipp32u code )
{
    bs->EncodeBinEP_CABAC(code);
}

template <class H265Bs>
static inline
    void h265_code_sao_max_uvlc (H265Bs *bs,  Ipp32u code, Ipp32u max_symbol)
{
    if (max_symbol == 0)
    {
        return;
    }

    Ipp32u i;
    Ipp8u code_last = (max_symbol > code);

    if (code == 0)
    {
        bs->EncodeBinEP_CABAC(0);
    }
    else
    {
        bs->EncodeBinEP_CABAC(1);
        for (i=0; i < code-1; i++)
        {
            bs->EncodeBinEP_CABAC(1);
        }
        if (code_last)
        {
            bs->EncodeBinEP_CABAC(0);
        }
    }
}

template <class H265Bs>
static inline
    void h265_code_sao_uvlc(H265Bs *bs, Ipp32u code)
{
    bs->EncodeBinsEP_CABAC(code, 5);
}


template <class H265Bs>
static inline
    void h265_code_sao_uflc(H265Bs *bs, Ipp32u code, Ipp32u length)
{
    bs->EncodeBinsEP_CABAC(code, length);
}

template <class H265Bs>
static
    void h265_code_sao_type_idx(H265Bs *bs, Ipp32u code)
{
    if (code == 0)
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,SAO_TYPE_IDX_HEVC),0);
    }
    else
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,SAO_TYPE_IDX_HEVC),1);
        {
            bs->EncodeBinEP_CABAC( code == 1 ? 0 : 1 );
        }
    }

} // void h265_code_sao_type_idx(H265Bs *bs, Ipp32u code)

template <class H265Bs>
void h265_code_sao_ctb_offset_param(H265Bs *bs, int compIdx, SaoOffsetParam& ctbParam, bool sliceEnabled)
{
    Ipp32u code;
    if(!sliceEnabled)
    {
        // aya!!!
        //VM_ASSERT(ctbParam.mode_idx == SAO_MODE_OFF);
        return;
    }

    //type
    if(compIdx == SAO_Y || compIdx == SAO_Cb)
    {
        if(ctbParam.mode_idx == SAO_MODE_OFF)
        {
            code =0;
        }
        else if(ctbParam.type_idx == SAO_TYPE_BO) //BO
        {
            code = 1;
        }
        else
        {
            VM_ASSERT(ctbParam.type_idx < SAO_TYPE_BO); //EO
            code = 2;
        }
        h265_code_sao_type_idx(bs, code);
    }

    if(ctbParam.mode_idx == SAO_MODE_ON)
    {
        int numClasses = (ctbParam.type_idx == SAO_TYPE_BO)?4:NUM_SAO_EO_CLASSES;
        int offset[4];
        int k=0;
        for(int i=0; i< numClasses; i++)
        {
            if(ctbParam.type_idx != SAO_TYPE_BO && i == SAO_CLASS_EO_PLAIN)
            {
                continue;
            }
            int classIdx = (ctbParam.type_idx == SAO_TYPE_BO)?(  (ctbParam.typeAuxInfo+i)% NUM_SAO_BO_CLASSES   ):i;
            offset[k] = ctbParam.offset[classIdx];
            k++;
        }

        for(int i=0; i< 4; i++)
        {

            code = (Ipp32u)( offset[i] < 0) ? (-offset[i]) : (offset[i]);
            h265_code_sao_max_uvlc (bs,  code, SAO_MAX_OFFSET_QVAL);
        }


        if(ctbParam.type_idx == SAO_TYPE_BO)
        {
            for(int i=0; i< 4; i++)
            {
                if(offset[i] != 0)
                {
                    h265_code_sao_sign(bs,  (offset[i] < 0) ? 1 : 0);
                }
            }

            h265_code_sao_uflc(bs, ctbParam.typeAuxInfo, NUM_SAO_BO_CLASSES_LOG2);
        }
        else //EO
        {
            if(compIdx == SAO_Y || compIdx == SAO_Cb)
            {
                VM_ASSERT(ctbParam.type_idx - SAO_TYPE_EO_0 >=0);
                h265_code_sao_uflc(bs, ctbParam.type_idx - SAO_TYPE_EO_0, NUM_SAO_EO_TYPES_LOG2);
            }
        }
    }

} // Void h265_code_sao_ctb_offset_param(Int compIdx, SaoOffsetParam& ctbParam, Bool sliceEnabled)


#endif // __MFX_H265_SAO_FILTER_H__
#endif // (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */
