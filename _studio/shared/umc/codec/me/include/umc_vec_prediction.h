//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2009 Intel Corporation. All Rights Reserved.
//

#include <umc_defs.h>
#ifdef UMC_ENABLE_ME

#ifndef _UMC_VEC_PREDICTION_H_
#define _UMC_VEC_PREDICTION_H_

#include "umc_me_structures.h"


namespace UMC
{
    class MePredictCalculator
    {
        DYNAMIC_CAST_DECL_BASE(MePredictCalculator);
    public:

        //MePredictCalculator();
        virtual void Init(MeParams* pMeParams, MeCurrentMB* pCur); //just save external data pointers

        virtual MeMV GetPrediction();
        virtual MeMV GetPrediction(MeMV mv);
        virtual MeMV GetPrediction16x16() = 0;
        virtual MeMV GetPrediction8x8() = 0;

        virtual MeMV GetMvA(){return AMV;}
        virtual MeMV GetMvB(){return BMV;}
        virtual MeMV GetMvC(){return CMV;}
        virtual void ResetPredictors(void){return;}
        virtual MeMV GetDef_FieldMV(int /*RefDir*/, int /*BlkIdx*/){return MeMV(0);}
        virtual Ipp32s GetDef_Field(int /*RefDir*/, int /*BlkIdx*/){return false;}
        virtual void SetDefFrwBkw(MeMV &mvF, MeMV &mvB)
        {
            mvF = mvB = MeMV(0);
        };
        virtual bool GetNonDominant(Ipp32s RefDir, Ipp32s CurrIdx,Ipp32s BlkIdx)
        {
            return false;
        }
        virtual void SetDefMV(MeMV &mv, Ipp32s dir)
        {
            mv = MeMV(0);
        };

        virtual bool IsOutOfBound(MeMbPart mt, MePixelType pix, MeMV mv);
        void TrimSearchRange(MeMbPart mt, MePixelType pix, Ipp32s &x0, Ipp32s &x1, Ipp32s &y0, Ipp32s &y1 );
        Ipp32s GetT2();
        // only for AVS
        Ipp32s m_blockDist[2][4];                      // (Ipp32s [][]) block distances, depending on ref index
        Ipp32s m_distIdx[2][4];                        // (Ipp32s [][]) distance indecies of reference pictures

    protected:
        inline Ipp16s median3( Ipp16s a, Ipp16s b, Ipp16s c ){return IPP_MIN(a,b)^IPP_MIN(b,c)^IPP_MIN(c,a);};

        void SetMbEdges();

        MeMV GetCurrentBlockMV(int isBkw, Ipp32s idx);
        bool GetCurrentBlockSecondRef(int isBkw, Ipp32s idx);
        MeMbType GetCurrentBlockType(Ipp32s idx);
        Ipp32s GetT2_16x16();
        Ipp32s GetT2_8x8();

        //pointers to external data
        MeParams*    m_pMeParams;
        MeCurrentMB* m_pCur;
        MeMB*        m_pRes;

        //macro-block/block location status
        bool  MbLeftEnable;
        bool  MbTopEnable;
        bool  MbRightEnable;
        bool  MbTopLeftEnable;
        bool  MbTopRightEnable;

        bool  isAOutOfBound;
        bool  isBOutOfBound;
        bool  isCOutOfBound;

        bool  AMVbSecond;//is AMV the second field vector
        bool  BMVbSecond;//is BMV the second field vector
        bool  CMVbSecond;//is CMV the second field vector

        MeMV  AMV; //A predictor
        MeMV  BMV; //B predictor
        MeMV  CMV; //C predictor
        
        MeMV   mv1MVField[2];
        Ipp32s prefField1MV[2];

    };

    class MePredictCalculatorH264 : public MePredictCalculator
    {
        DYNAMIC_CAST_DECL(MePredictCalculatorH264, MePredictCalculator)
    public:
        typedef enum
        {
            Left,
            Top,
            TopRight,
            TopLeft

        } MeH264Neighbour;

        virtual MeMV GetPrediction16x16();
        virtual MeMV GetPrediction8x8();

        Ipp32s GetRestictedDirection() {return RestrictDirection;}
        void SetPredictionPSkip();
        void SetPredictionSpatialDirectSkip16x16();

    protected:
        void GetPredictor16x16(int index,int isBkw, MeMV* res);
        void SetSpatialDirectRefIdx(Ipp32s* RefIdxL0, Ipp32s* RefIdxL1);
        void ComputePredictors16x16(Ipp8s ListNum, Ipp32s RefIndex, MeMV* res);

        // direction -in,
        // ListNum -in
        // pMV - out
        // RefIdx - out
        void SetMVNeighbBlkParamDepPart(MeH264Neighbour direction,
                                             Ipp8s ListNum,
                                             MeMV* pMV,
                                             Ipp32s* RefIdx);

        Ipp32s RestrictDirection; //which od direction is restricted for prediction, 0 - frw, 1 - bckw, 2-bidir
    };

    class MePredictCalculatorVC1 : public MePredictCalculator
    {
        DYNAMIC_CAST_DECL(MePredictCalculatorVC1, MePredictCalculator);
    public:
        virtual void Init(MeParams* pMeParams, MeCurrentMB* pCur); //just save external data pointers
        virtual MeMV GetPrediction(MeMV mv);
        virtual MeMV GetPrediction16x16();
        virtual MeMV GetPrediction8x8();

        virtual void ResetPredictors(void)
        {
            for(Ipp32s i = 0; i < MAX_REF; i++)
                for(Ipp32s j = 0; j < 4; j++)
                {
                    m_CurPrediction[0][j][i].SetInvalid();
                    m_CurPrediction[1][j][i].SetInvalid();
                }
        }

        virtual MeMV GetMvA(){return m_AMV[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx];}
        virtual MeMV GetMvB(){return m_BMV[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx];}
        virtual MeMV GetMvC(){return m_CMV[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx];}
        virtual MeMV GetDef_FieldMV(int RefDir, int BlkIdx)
            {return m_CurPrediction[RefDir][BlkIdx][m_FieldPredictPrefer[RefDir][BlkIdx]];}
        virtual Ipp32s GetDef_Field(int RefDir, int BlkIdx)
            {return m_FieldPredictPrefer[RefDir][BlkIdx];}
        virtual void SetDefFrwBkw(MeMV &mvF, MeMV &mvB);
        virtual void SetDefMV(MeMV &mv, Ipp32s dir);
        virtual bool IsOutOfBound(MeMbPart mt, MePixelType pix, MeMV mv);

    protected:
        typedef Ipp32s (MePredictCalculatorVC1::*GetPredictorFunc)(int index,int isBkw, MeMV* res);
        GetPredictorFunc GetPredictor;

        Ipp32s GetPredictorMPEG2(int index,int isBkw, MeMV* res);
        Ipp32s GetPredictorVC1(int index,int isBkw, MeMV* res);
        Ipp32s GetPredictorVC1_hybrid(int index,int isBkw, MeMV* res);
        void   GetPredictorVC1_hybrid(MeMV cur, MeMV* res);
        Ipp32s GetPredictorVC1Field1(int index, int isBkw, MeMV* res);
        Ipp32s GetPredictorVC1Field2(int index, int isBkw, MeMV* res);
        Ipp32s GetPredictorVC1Field2Hybrid(int index, int isBkw, MeMV* res);

        void GetBlockVectorsABC_0(int isBkw);
        void GetBlockVectorsABC_1(int isBkw);
        void GetBlockVectorsABC_2(int isBkw);
        void GetBlockVectorsABC_3(int isBkw);
        void GetMacroBlockVectorsABC(int isBkw);
        void GetBlockVectorsABCField_0(int isBkw);
        void GetBlockVectorsABCField_1(int isBkw);
        void GetBlockVectorsABCField_2(int isBkw);
        void GetBlockVectorsABCField_3(int isBkw);
        void GetMacroBlockVectorsABCField(int isBkw);
        void ScalePredict(MeMV* MV);

        MeMV  m_AMV[2][4][MAX_REF]; //current A, the first index = frw, bkw, the second index - block number
        MeMV  m_BMV[2][4][MAX_REF]; //current B, the first index = frw, bkw, the second index - block number
        MeMV  m_CMV[2][4][MAX_REF]; //current C, the first index = frw, bkw, the second index - block number

        //MVs for prediction
        MeMV  m_CurPrediction[2][4][MAX_REF]; //prediction for current MB,the first index = frw, bkw,
                                              //the second index - block number
        Ipp32s  m_FieldPredictPrefer[2][4];//for field picture vectors prediction; 0 - for the forward, 1 - for the backward
                                         //the second index - block number
        MeMV MVPredMin;
        MeMV MVPredMax;
        Ipp32s Mult;
    };
// For AVS
//#if defined(UMC_ENABLE_AVS_VIDEO_ENCODER)
    class MePredictCalculatorAVS : public MePredictCalculator
    {
        DYNAMIC_CAST_DECL(MePredictCalculatorAVS, MePredictCalculator)
    public:
        virtual MeMV GetPrediction16x16();
        virtual MeMV GetPrediction8x8();
        //virtual MeMV GetPrediction16x16( bool isSkipMBlock);
        //virtual MeMV GetPrediction16x16(){return GetPrediction16x16 (false);};
        //virtual MeMV GetPrediction8x8( bool isSkipMBlock);
        //virtual MeMV GetPrediction8x8(){return GetPrediction8x8 (false);};
        //virtual MeMV GetPrediction(bool isSkipMBlock);
        virtual MeMV GetSkipPrediction() ;
        virtual void GetSkipDirectPrediction();
        //virtual MeMV GetSymmPrediction() ;

        virtual void ResetPredictors(void)
        {
            for(Ipp32s i = 0; i < MAX_REF; i++)
                for(Ipp32s j = 0; j < 4; j++)
                {
                    m_CurPrediction[0][j][i].SetInvalid();
                    m_CurPrediction[1][j][i].SetInvalid();
                }
        }

        virtual MeMV GetMvA(){return m_AMV[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx];}
        virtual MeMV GetMvB(){return m_BMV[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx];}
        virtual MeMV GetMvC(){return m_CMV[m_pCur->RefDir][m_pCur->BlkIdx][m_pCur->RefIdx];}
        //virtual MeMV GetDef_FieldMV(int RefDir, int BlkIdx)
        //    {return m_CurPrediction[RefDir][BlkIdx][m_FieldPredictPrefer[RefDir][BlkIdx]];}
        //virtual bool GetDef_Field(int RefDir, int BlkIdx)
        //    {return m_FieldPredictPrefer[RefDir][BlkIdx];}
        
        // Method for calculation backward motion vector according AVS standard
        MeMV CreateSymmetricalMotionVector(Ipp32s blockNum, MeMV mvFw);
        void ReconstructMotionVectorsBSliceDirect(MeMV *mvFw, MeMV *mvBw);

    protected:
        typedef bool (MePredictCalculatorAVS::*GetPredictorFunc)(int index,int isBkw, MeMV* res);
        GetPredictorFunc GetPredictor;
        void SetMbEdges();

        //bool GetPredictorAVS(int index,int isBkw, MeMV* res);

        void GetMotionVectorPredictor16x16(int isBkw, Ipp32s blockNum);
        void GetMotionVectorPredictor8x8(int isBkw, Ipp32s blockNum);
        void ReconstructDirectMotionVector(Ipp32s blockNum);
        MeMbPart GetCollocatedBlockDivision(void);
        void GetMotionVectorPredictor( MeMV mvA, MeMV mvB, MeMV mvC,
                               Ipp32s blockDist,  Ipp32s blockDistA,
                               Ipp32s blockDistB, Ipp32s blockDistC);
        
        MeMV  m_AMV[2][4][MAX_REF]; //current A, the first index = frw, bkw, the second index - block number
        MeMV  m_BMV[2][4][MAX_REF]; //current B, the first index = frw, bkw, the second index - block number
        MeMV  m_CMV[2][4][MAX_REF]; //current C, the first index = frw, bkw, the second index - block number

        MeMV m_predMVDirect[2][4];

        //MVs for prediction
        MeMV  m_CurPrediction[2][4][MAX_REF]; //prediction for current MB,the first index = frw, bkw,
                                              //the second index - block number
        Ipp32s  m_FieldPredictPrefer[2][4];//for field picture vectors prediction; 0 - for the forward, 1 - for the backward
                                         //the second index - block number
        struct AVS_NEIGHBOURS
        {
            MeMB *pNearest;

            MeMB *pLeft;
            MeMB *pTop;
            MeMB *pTopRight;
            MeMB *pTopLeft;
        } m_neighbours;

        MeMB *m_pMbInfo;                                     // (AVS_MB_INFO *) current macroblock's properties
        MeMB *m_pMbInfoLeft;                                 // (AVS_MB_INFO *) left macroblock's properties
        MeMB *m_pMbInfoTop;                                  // (AVS_MB_INFO *) upper macroblock's properties
        MeMB *m_pMbInfoTopRight;                             // (AVS_MB_INFO *) upper-right macroblock's properties
        MeMV *m_pPredMV;                                   // instead of m_mvPred
        MeMB *m_pMbInfoTopLeft;                              // (AVS_MB_INFO *) upper-left macroblock's properties
    };
//#endif //#if defined(UMC_ENABLE_AVS_VIDEO_ENCODER)
}

#endif
#endif
