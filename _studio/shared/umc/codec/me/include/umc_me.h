// Copyright (c) 2007-2018 Intel Corporation
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
extern const int32_t MeVc1FastRegrTableSize;
extern int32_t MeVc1FastRegrTable[];
extern const int32_t MeVc1PreciseRegrTableSize;
extern int32_t MeVc1PreciseRegrTable[];
extern const int32_t MeAVSFastRegrTableSize;
extern int32_t MeAVSFastRegrTable[];

class RegrFun{ //Piecewise-Linear Fitting, x from 0
    public:
        RegrFun(int32_t dx, int32_t N);
        ~RegrFun(){};

        static void SetQP(int32_t qp){m_QP = qp;}; // TODO: Check qp in all functions
        void LoadPresets(MeRegrFunSet set, MeRegrFun id);
        void ProcessFeedback(int32_t *x, int32_t *y, int32_t N);
        virtual int32_t Weight(int32_t x);

    //protected:  debug!!!!
        virtual void ComputeRegression(int32_t I, int32_t *x, int32_t *y, int32_t N)=0;
        virtual void AfterComputation(){};

        static const int NumOfRegInterval = 128;
        static const int MaxQpValue = 73;
        // TODO: set TargetHistorySize & FrameSize from without
        static const int FrameSize=1350; //number of MB in frame
#ifdef ME_GENERATE_PRESETS
        static const int m_TargetHistorySize = MFX_MAX_32S; //number of MB in history
#else
        static const int m_TargetHistorySize = 10*FrameSize; //number of MB in history
#endif

        int32_t m_dx[MaxQpValue];
        int32_t m_num; //number of fitting pieces
        int32_t m_FirstData; //first and last bin with data, all other are empty
        int32_t m_LastData;
        double m_ax[MaxQpValue][NumOfRegInterval];
        double m_ay[MaxQpValue][NumOfRegInterval];
        double m_an[MaxQpValue][NumOfRegInterval];
        double m_a[MaxQpValue][NumOfRegInterval];
        double m_b[MaxQpValue][NumOfRegInterval];

        static int32_t m_QP; //current QP
};

class LowessRegrFun : public RegrFun
{
    public:
        LowessRegrFun(int32_t dx=32, int32_t N=128):RegrFun(dx, N){};

    protected:
        static const int Bandwidth = 30; //percent
        double WeightPoint(double x); //used during regression calculation

        virtual void ComputeRegression(int32_t I, int32_t *x, int32_t *y, int32_t N);
        virtual void AfterComputation();
};

class MvRegrFun : public LowessRegrFun
{
    public:
        MvRegrFun():LowessRegrFun(4,128){};
        virtual int32_t Weight(MeMV mv, MeMV pred);
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
        virtual bool Init(MeInitParams *par, uint8_t* ptr, int32_t &size);
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
                                           int32_t IdxF, int32_t IdxB);
        virtual int32_t MakeTransformDecisionInterFast(MeMbPart mt, MePixelType pix,
                                           MeMbType type,
                                           MeMV mvF, MeMV mvB,
                                           int32_t IdxF, int32_t IdxB);
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
        int32_t EstimatePointAverage(MeMbPart mt, MePixelType pix, MeDecisionMetrics CostMetric, int32_t RefDirection0, int32_t RefIndex0, MeMV mv0, int32_t RefDirection1, int32_t RefIndex1, MeMV mv1);
        int32_t EstimatePoint(MeMbPart mt, MePixelType pix, MeDecisionMetrics CostMetric, MeMV mv);
        int32_t GetCost(MeDecisionMetrics CostMetric, MeMbPart mt, MeAdr *src, MeAdr* ref);
        void GetCost(MeMbPart mt, MeAdr* src, MeAdr* ref, int32_t numX, uint16_t *sadArr);
        int32_t GetCostHDMR(MeDecisionMetrics CostMetric, MeMbPart mt, MeAdr* src, MeAdr* ref);
        virtual MeCostRD GetCostRD(MeDecisionMetrics CostMetric, MeMbPart mt, MeTransformType transf, MeAdr* src, MeAdr* ref);
        virtual void AddHeaderCost(MeCostRD &/*cost*/, MeTransformType * /*tansf*/, MeMbType /*MbType*/, int32_t /*RefIdxF*/,int32_t /*RefIdxB*/){};
        void DownsampleOne(int32_t level, int32_t x0, int32_t y0, int32_t w, int32_t h, MeAdr *adr);
        void DownsampleFrames();

        //small  auxiliary functions
        void SetReferenceFrame(int32_t RefDirection, int32_t RefIndex);
        int32_t GetPlaneLevel(MePixelType pix);
        void MakeSrcAdr(MeMbPart mt, MePixelType pix, MeAdr* adr);
        void MakeRefAdrFromMV(MeMbPart mt, MePixelType pix, MeMV mv, MeAdr* adr);
        void MakeBlockAdr(int32_t level, MeMbPart mt, int32_t BlkIdx, MeAdr* adr);
        virtual void SetMB16x16B(MeMbType mbt, MeMV mvF, MeMV mvB, int32_t cost);
        //MeBlockType ConvertType(MeMbType t);
        void SetMB8x8(MeMbType mbt, MeMV mv[2][4], int32_t cost[4]);
        virtual MeMV GetChromaMV (MeMV mv){return mv;};
        virtual int32_t GetMvSize(int32_t /*dx*/,int32_t /*dy*/, bool /*bNonDominant*/, bool /*hybrid*/){return 0;};
        virtual bool GetHybrid(int32_t RefIdx){return m_cur.HybridPredictor[RefIdx];};

        //algorithms
        void EstimateMbInterFullSearch();
        void EstimateMbInterFullSearchDwn();
        void EstimateMbInterOneLevel(bool UpperLevel, int32_t level, MeMbPart mt, int32_t x0, int32_t x1, int32_t y0, int32_t y1);
        void EstimateMbInterFast();
        void FullSearch(MeMbPart mt, MePixelType pix, MeMV org, int32_t RangeX, int32_t RangeY);
        void DiamondSearch(MeMbPart mt, MePixelType pix, MeDiamondType dm);
        bool RefineSearch(MeMbPart mt, MePixelType pix);

        MeCurrentMB m_cur;
        MePredictCalculator* m_PredCalc;
        bool m_DirectOutOfBound;

        bool CheckParams();
        inline int32_t WeightMV(MeMV mv, MeMV predictor);


        //parameters
        MeParams*         m_par; //ptr to current frame parameters
        MeInitParams      m_InitPar; //initialization parameters
        MeChromaFormat    m_CRFormat;
        int32_t            m_SkippedThreshold;//pixel abs. diff. threshhold for the skipped macroblocks (unifirm metric)
        int32_t m_AllocatedSize; //number of allocated byte, valid after first call of Init
        void *m_OwnMem;
        int32_t  CostOnInterpolation[4];

        //1 feedback from encoder
        public:
            void ProcessFeedback(MeParams *pPar);
        protected:
            int32_t QpToIndex(int32_t QP, bool UniformQuant);
            void LoadRegressionPreset();

            double m_lambda;
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
                                           int32_t IdxF, int32_t IdxB);
    virtual int32_t MakeTransformDecisionInterFast(MeMbPart mt, MePixelType pix,
                                           MeMbType type,
                                           MeMV mvF, MeMV mvB,
                                           int32_t IdxF, int32_t IdxB);

    uint8_t GetCoeffMode( int32_t &run, int32_t &level, const uint8_t *pTableDR, const uint8_t *pTableDL);
    int32_t GetAcCoeffSize(int16_t *ptr, int32_t DC, MeTransformType transf, int32_t QP, bool luma, bool intra);
    int32_t GetOneAcCoeffSize(int32_t level, int32_t run, int32_t QP, bool dc, bool luma, bool intra, bool last);
    void QuantTrellis(int16_t *ptr, int8_t* prc,int32_t DC, MeTransformType transf, int32_t QP, bool luma, bool intra);
    virtual void AddHeaderCost(MeCostRD &cost, MeTransformType *tansf, MeMbType MbType,int32_t RefIdxF,int32_t RefIdxB);
    virtual int32_t GetMvSize(int32_t dx, int32_t dy, bool bNonDominant,bool hybrid);
    virtual MeCostRD GetCostRD(MeDecisionMetrics CostMetric, MeMbPart mt, MeTransformType transf, MeAdr* src, MeAdr* ref);

    int32_t GetDcPredictor(int32_t BlkIdx);
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
    void SetBlockDist(void* pSrc, uint32_t numOfByte) {MFX_INTERNAL_CPY(m_blockDist,pSrc,numOfByte);};
    void SetBlockIdx(void* pSrc, uint32_t numOfByte) {MFX_INTERNAL_CPY(m_distIdx,pSrc,numOfByte);};
    int32_t m_blockDist[2][4];                      // (int32_t [][]) block distances, depending on ref index
    int32_t m_distIdx[2][4];                        // (int32_t [][]) distance indecies of reference pictures

protected:
    // some variables need for AVS prediction
    //int32_t m_blockDist[2][4];                      // (int32_t [][]) block distances, depending on ref index
    //int32_t m_distIdx[2][4];                        // (int32_t [][]) distance indecies of reference pictures

    MePredictCalculatorAVS m_PredCalcAVS;
    void ModeDecision16x16ByFBFullFrw();
    void ModeDecision16x16ByFBFastFrw();
    bool EstimateSkip16x16();
    bool EstimateSkip8x8();
    void Estimate16x16Bidir(); //bidir costs is calculated here and Inter mode is choosen
    bool Estimate16x16Direct(); ////return true if early exit allowed

    // mv functions 
    virtual void SetMB16x16B(MeMbType mbt, MeMV mvF, MeMV mvB, int32_t cost);
    virtual int32_t GetMvSize(int32_t dx, int32_t dy, bool bNonDominant = false, bool hybrid = false);
    virtual MeMV GetChromaMV (MeMV mv);
    void GetChromaMV (MeMV LumaMV, MeMV* pChroma);
    void GetChromaMVFast(MeMV LumaMV, MeMV * pChroma);
};
//#endif

}//namespace
#endif
#endif

