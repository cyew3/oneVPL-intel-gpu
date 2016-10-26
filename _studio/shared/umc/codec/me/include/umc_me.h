//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2013 Intel Corporation. All Rights Reserved.
//

#include <umc_defs.h>
#ifdef UMC_ENABLE_ME

#ifndef _UMC_ME_H_
#define _UMC_ME_H_

#include <ippdefs.h>
//#define ME_GENERATE_PRESETS
//#define ME_DEBUG
#ifdef ME_DEBUG
#include "me_log.h"
#endif

#define ME_MB_ADR (999999)
//#define ME_MB_IF_ADR(x) (x==344) //inter
//#define ME_MB_IF_ADR(x) (x==300) //intra
#define ME_MB_IF_ADR(x) (false)




#include <ippvc.h>
#include <ippi.h>
#include  "ippcv.h"

#include "umc_me_sadt4x4.h"
#include "umc_me_structures.h"
#include "umc_vec_prediction.h"

//#define VC1ABS(value) ((value)*(2*((value)>>15)+1))
#define MB_NUM_ELEMENTS 256
#define B_NUM_ELEMENTS  64

#ifdef ME_DEBUG
extern int CbpcyTableIndex;
extern int MvTableIndex;
extern int AcTableIndex;
extern int EncMbAdr;
extern bool UseTrellisQuant;
#endif


/// UMC is namespace for ME component.
/// All ME enums, structures and classes resided in this workspace.
namespace UMC
{

class MePredictCalculator;

typedef enum
{
    ME_RG_RInter,
    ME_RG_DInter,
    ME_RG_JInter,
    ME_RG_RMV,
    ME_RG_RIntra,
    ME_RG_DIntra
}MeRegrFun;

typedef enum
{
    ME_RG_EMPTY_SET,
    ME_RG_VC1_FAST_SET,
    ME_RG_VC1_PRECISE_SET,
    ME_RG_AVS_FAST_SET
}MeRegrFunSet;

//table for presets for regression functions
extern const Ipp32s MeVc1FastRegrTableSize;
extern Ipp32s MeVc1FastRegrTable[];
extern const Ipp32s MeVc1PreciseRegrTableSize;
extern Ipp32s MeVc1PreciseRegrTable[];
extern const Ipp32s MeAVSFastRegrTableSize;
extern Ipp32s MeAVSFastRegrTable[];

class RegrFun{ //Piecewise-Linear Fitting, x from 0
    public:
        RegrFun(Ipp32s dx, Ipp32s N);
        ~RegrFun(){};

        static void SetQP(Ipp32s qp){m_QP = qp;}; // TODO: Check qp in all functions
        void LoadPresets(MeRegrFunSet set, MeRegrFun id);
        void ProcessFeedback(Ipp32s *x, Ipp32s *y, Ipp32s N);
        virtual Ipp32s Weight(Ipp32s x);

    //protected:  debug!!!!
        virtual void ComputeRegression(Ipp32s I, Ipp32s *x, Ipp32s *y, Ipp32s N)=0;
        virtual void AfterComputation(){};

        static const int NumOfRegInterval = 128;
        static const int MaxQpValue = 73;
        // TODO: set TargetHistorySize & FrameSize from without
        static const int FrameSize=1350; //number of MB in frame
#ifdef ME_GENERATE_PRESETS
        static const int m_TargetHistorySize = IPP_MAX_32S; //number of MB in history
#else
        static const int m_TargetHistorySize = 10*FrameSize; //number of MB in history
#endif

        Ipp32s m_dx[MaxQpValue];
        Ipp32s m_num; //number of fitting pieces
        Ipp32s m_FirstData; //first and last bin with data, all other are empty
        Ipp32s m_LastData;
        Ipp64f m_ax[MaxQpValue][NumOfRegInterval];
        Ipp64f m_ay[MaxQpValue][NumOfRegInterval];
        Ipp64f m_an[MaxQpValue][NumOfRegInterval];
        Ipp64f m_a[MaxQpValue][NumOfRegInterval];
        Ipp64f m_b[MaxQpValue][NumOfRegInterval];

        static Ipp32s m_QP; //current QP
};

class LowessRegrFun : public RegrFun
{
    public:
        LowessRegrFun(Ipp32s dx=32, Ipp32s N=128):RegrFun(dx, N){};

    protected:
        static const int Bandwidth = 30; //percent
        Ipp64f WeightPoint(Ipp64f x); //used during regression calculation

        virtual void ComputeRegression(Ipp32s I, Ipp32s *x, Ipp32s *y, Ipp32s N);
        virtual void AfterComputation();
};

class MvRegrFun : public LowessRegrFun
{
    public:
        MvRegrFun():LowessRegrFun(4,128){};
        virtual Ipp32s Weight(MeMV mv, MeMV pred);
};


///Base class for motion estimation.
///It contains standard independent functionality, including different motion estimation and mode decision algorithm.
class MeBase
{
    DYNAMIC_CAST_DECL_BASE(MeBase);
    public:
        MeBase();
        virtual ~MeBase();

        //virtual bool Init(MeInitParams *par); //allocates memory
        virtual bool Init(MeInitParams *par);
        virtual bool Init(MeInitParams *par, Ipp8u* ptr, Ipp32s &size);
        virtual bool EstimateFrame(MeParams *par); //par and res should be allocated by caller, except MeFrame::MBs
        virtual void Close(); //frees memory, also called from destructor
        static void SetError(vm_char *msg, bool condition=true); //save error message

    protected:
        //top level functions
        //Exist diffrent realizations for VC-1 and H264
        bool EstimateFrameInner(MeParams *par);
        void EstimateMB16x16(); //here both P and B frames are processing, results are written to m_ResMB
        virtual void EstimateMB8x8(); // call EstimateMB16x16 inside
        virtual bool EstimateSkip()=0;  //return true if early exit allowed, set m_cur->SkipCost[m_cur.BlkIdx], estimate 16x16, 8x8
        virtual void SetInterpPixelType(MeParams *par) = 0;
        virtual void InitVlcTableIndexes(){};
        virtual void ReInitVlcTableIndexes(MeParams *par){};
        virtual void SetVlcTableIndexes(MeParams *par){};

        //possible codec specific
        bool EarlyExitForSkip(); //return true if skip is good enough and further search may be skiped
        void EstimateInter(); //set m_cur->InterCost[RefDir][RefIdx][m_cur.BlkIdx]
        virtual void Estimate16x16Bidir(); //bidir costs is calculated here and Inter mode is choosen
        virtual bool Estimate16x16Direct(); //return true if early exit allowed
        void EstimateIntra();
        void UpdateVlcTableStatistic();
        void MakeVlcTableDecision();
        virtual MeCostRD MakeTransformDecisionInter(MeMbPart mt, MePixelType pix,
                                           MeMbType type,
                                           MeMV mvF, MeMV mvB,
                                           Ipp32s IdxF, Ipp32s IdxB);
        virtual Ipp32s MakeTransformDecisionInterFast(MeMbPart mt, MePixelType pix,
                                           MeMbType type,
                                           MeMV mvF, MeMV mvB,
                                           Ipp32s IdxF, Ipp32s IdxB);
        void MakeMBModeDecision16x16(); //chooses 16x16 mode, saves results to m_ResMB
        void MakeMBModeDecision16x16Org();
        void MakeMBModeDecision16x16ByFB();
        virtual void ModeDecision16x16ByFBFastFrw();
        void ModeDecision16x16ByFBFastBidir();
        virtual void ModeDecision16x16ByFBFullFrw();
        void ModeDecision16x16ByFBFullBidir();
        void MakeSkipModeDecision16x16BidirByFB();
        void SetModeDecision16x16BidirByFB(MeMV &mvF, MeMV &mvB);
        void MakeBlockModeDecision(); //chooses 8x8 mode, save results to m_cur
        void MakeMBModeDecision8x8(); //chooses between 16x16 and 8x8 mode
        void MakeMBModeDecision8x8Org();
        void MakeMBModeDecision8x8ByFB();

        //middle level
        bool IsSkipEnabled();
        void Interpolate(MeMbPart mt, MeInterpolation interp, MeMV mv, MeAdr* src, MeAdr *dst);

        //low level function
        void EstimatePointInter(MeMbPart mt, MePixelType pix, MeMV mv);
        Ipp32s EstimatePointAverage(MeMbPart mt, MePixelType pix, MeDecisionMetrics CostMetric, Ipp32s RefDirection0, Ipp32s RefIndex0, MeMV mv0, Ipp32s RefDirection1, Ipp32s RefIndex1, MeMV mv1);
        Ipp32s EstimatePoint(MeMbPart mt, MePixelType pix, MeDecisionMetrics CostMetric, MeMV mv);
        Ipp32s GetCost(MeDecisionMetrics CostMetric, MeMbPart mt, MeAdr *src, MeAdr* ref);
        void GetCost(MeMbPart mt, MeAdr* src, MeAdr* ref, Ipp32s numX, Ipp16u *sadArr);
        Ipp32s GetCostHDMR(MeDecisionMetrics CostMetric, MeMbPart mt, MeAdr* src, MeAdr* ref);
        virtual MeCostRD GetCostRD(MeDecisionMetrics CostMetric, MeMbPart mt, MeTransformType transf, MeAdr* src, MeAdr* ref);
        virtual void AddHeaderCost(MeCostRD &/*cost*/, MeTransformType * /*tansf*/, MeMbType /*MbType*/, Ipp32s /*RefIdxF*/,Ipp32s /*RefIdxB*/){};
        void DownsampleOne(Ipp32s level, Ipp32s x0, Ipp32s y0, Ipp32s w, Ipp32s h, MeAdr *adr);
        void DownsampleFrames();

        //small  auxiliary functions
        void SetReferenceFrame(Ipp32s RefDirection, Ipp32s RefIndex);
        Ipp32s GetPlaneLevel(MePixelType pix);
        void MakeSrcAdr(MeMbPart mt, MePixelType pix, MeAdr* adr);
        void MakeRefAdrFromMV(MeMbPart mt, MePixelType pix, MeMV mv, MeAdr* adr);
        void MakeBlockAdr(Ipp32s level, MeMbPart mt, Ipp32s BlkIdx, MeAdr* adr);
        virtual void SetMB16x16B(MeMbType mbt, MeMV mvF, MeMV mvB, Ipp32s cost);
        //MeBlockType ConvertType(MeMbType t);
        void SetMB8x8(MeMbType mbt, MeMV mv[2][4], Ipp32s cost[4]);
        virtual MeMV GetChromaMV (MeMV mv){return mv;};
        virtual Ipp32s GetMvSize(Ipp32s /*dx*/,Ipp32s /*dy*/, bool /*bNonDominant*/, bool /*hybrid*/){return 0;};
        virtual bool GetHybrid(Ipp32s RefIdx){return m_cur.HybridPredictor[RefIdx];};

        //algorithms
        void EstimateMbInterFullSearch();
        void EstimateMbInterFullSearchDwn();
        void EstimateMbInterOneLevel(bool UpperLevel, Ipp32s level, MeMbPart mt, Ipp32s x0, Ipp32s x1, Ipp32s y0, Ipp32s y1);
        void EstimateMbInterFast();
        void FullSearch(MeMbPart mt, MePixelType pix, MeMV org, Ipp32s RangeX, Ipp32s RangeY);
        void DiamondSearch(MeMbPart mt, MePixelType pix, MeDiamondType dm);
        bool RefineSearch(MeMbPart mt, MePixelType pix);

        MeCurrentMB m_cur;
        MePredictCalculator* m_PredCalc;
        bool m_DirectOutOfBound;

        bool CheckParams();
        inline Ipp32s WeightMV(MeMV mv, MeMV predictor);


        //parameters
        MeParams*         m_par; //ptr to current frame parameters
        MeInitParams      m_InitPar; //initialization parameters
        MeChromaFormat    m_CRFormat;
        Ipp32s            m_SkippedThreshold;//pixel abs. diff. threshhold for the skipped macroblocks (unifirm metric)
        Ipp32s m_AllocatedSize; //number of allocated byte, valid after first call of Init
        void *m_OwnMem;
        Ipp32s  CostOnInterpolation[4];

        //1 feedback from encoder
        public:
            void ProcessFeedback(MeParams *pPar);
        protected:
            Ipp32s QpToIndex(Ipp32s QP, bool UniformQuant);
            void LoadRegressionPreset();

            Ipp64f m_lambda;
            MeRegrFunSet  m_CurRegSet;
            LowessRegrFun m_InterRegFunD;
            LowessRegrFun m_InterRegFunR;
            LowessRegrFun m_InterRegFunJ;
            LowessRegrFun m_IntraRegFunD;
            LowessRegrFun m_IntraRegFunR;
            MvRegrFun m_MvRegrFunR;
            bool ChangeInterpPixelType;

#ifdef ME_DEBUG
        public:
            MeStat *pStat;
#endif
};

/// H.264 specific functionality, mostly MV prediction now.
/// There are no differences in public functions in comparison with MeBase.
class MeH264 : public MeBase
{
    DYNAMIC_CAST_DECL(MeH264,MeBase);
public:
    MeH264()
    {
        m_PredCalc = &m_PredCalcH264;
    }

    virtual bool EstimateSkip();
protected:
    MePredictCalculatorH264 m_PredCalcH264;

};


///VC1 specific functionality. Including RD mode decision and vector prediction calculation.
///There are no differences in public functions in comparison  with MeBase.
class MeVC1 : public MeBase
{
    DYNAMIC_CAST_DECL(MeVC1,MeBase);
public:
    MeVC1()
    {
        m_PredCalc = &m_PredCalcVC1;
        m_BitplanesRaw = true;
        InitVlcTableIndexes();
    }
    //top level functions
    virtual bool EstimateSkip();

protected:
    MePredictCalculatorVC1 m_PredCalcVC1;

    bool EstimateSkip16x16();
    void EstimateMB8x8();
    bool EstimateSkip8x8();
    void SetInterpPixelType(MeParams *par);
    virtual MeCostRD MakeTransformDecisionInter(MeMbPart mt, MePixelType pix,
                                           MeMbType type,
                                           MeMV mvF, MeMV mvB,
                                           Ipp32s IdxF, Ipp32s IdxB);
    virtual Ipp32s MakeTransformDecisionInterFast(MeMbPart mt, MePixelType pix,
                                           MeMbType type,
                                           MeMV mvF, MeMV mvB,
                                           Ipp32s IdxF, Ipp32s IdxB);

    Ipp8u GetCoeffMode( Ipp32s &run, Ipp32s &level, const Ipp8u *pTableDR, const Ipp8u *pTableDL);
    Ipp32s GetAcCoeffSize(Ipp16s *ptr, Ipp32s DC, MeTransformType transf, Ipp32s QP, bool luma, bool intra);
    Ipp32s GetOneAcCoeffSize(Ipp32s level, Ipp32s run, Ipp32s QP, bool dc, bool luma, bool intra, bool last);
    void QuantTrellis(Ipp16s *ptr, Ipp8s* prc,Ipp32s DC, MeTransformType transf, Ipp32s QP, bool luma, bool intra);
    virtual void AddHeaderCost(MeCostRD &cost, MeTransformType *tansf, MeMbType MbType,Ipp32s RefIdxF,Ipp32s RefIdxB);
    virtual Ipp32s GetMvSize(Ipp32s dx, Ipp32s dy, bool bNonDominant,bool hybrid);
    virtual MeCostRD GetCostRD(MeDecisionMetrics CostMetric, MeMbPart mt, MeTransformType transf, MeAdr* src, MeAdr* ref);

    Ipp32s GetDcPredictor(Ipp32s BlkIdx);
    virtual MeMV GetChromaMV (MeMV mv);
    void GetChromaMV (MeMV LumaMV, MeMV* pChroma);
    void GetChromaMVFast(MeMV LumaMV, MeMV * pChroma);
    void InitVlcTableIndexes();
    void ReInitVlcTableIndexes(MeParams *par);
    void SetVlcTableIndexes(MeParams *par);

    bool m_BitplanesRaw;
    int CbpcyTableIndex;
    int MvTableIndex;
    int AcTableIndex;

};

//#if defined(UMC_ENABLE_AVS_VIDEO_ENCODER)
class MeAVS : public MeBase
{
    DYNAMIC_CAST_DECL(MeAVS,MeBase);
public:
    MeAVS()
    {
        m_PredCalc = &m_PredCalcAVS;
        //m_PredCalc->
    }
    //top level functions
    virtual bool EstimateSkip();
    bool EstimateFrame(MeParams *par);
    void ProcessFeedback(MeParams *pPar);
    void SetInterpPixelType(MeParams *par){};
    //function for copy m_blockDist[][] and m_distIdx[][]
    void SetBlockDist(void* pSrc, Ipp32u numOfByte) {MFX_INTERNAL_CPY(m_blockDist,pSrc,numOfByte);};
    void SetBlockIdx(void* pSrc, Ipp32u numOfByte) {MFX_INTERNAL_CPY(m_distIdx,pSrc,numOfByte);};
    Ipp32s m_blockDist[2][4];                      // (Ipp32s [][]) block distances, depending on ref index
    Ipp32s m_distIdx[2][4];                        // (Ipp32s [][]) distance indecies of reference pictures

protected:
    // some variables need for AVS prediction
    //Ipp32s m_blockDist[2][4];                      // (Ipp32s [][]) block distances, depending on ref index
    //Ipp32s m_distIdx[2][4];                        // (Ipp32s [][]) distance indecies of reference pictures

    MePredictCalculatorAVS m_PredCalcAVS;
    void ModeDecision16x16ByFBFullFrw();
    void ModeDecision16x16ByFBFastFrw();
    bool EstimateSkip16x16();
    bool EstimateSkip8x8();
    void Estimate16x16Bidir(); //bidir costs is calculated here and Inter mode is choosen
    bool Estimate16x16Direct(); ////return true if early exit allowed

    // mv functions 
    virtual void SetMB16x16B(MeMbType mbt, MeMV mvF, MeMV mvB, Ipp32s cost);
    virtual Ipp32s GetMvSize(Ipp32s dx, Ipp32s dy, bool bNonDominant = false, bool hybrid = false);
    virtual MeMV GetChromaMV (MeMV mv);
    void GetChromaMV (MeMV LumaMV, MeMV* pChroma);
    void GetChromaMVFast(MeMV LumaMV, MeMV * pChroma);
};
//#endif

}//namespace
#endif
#endif

