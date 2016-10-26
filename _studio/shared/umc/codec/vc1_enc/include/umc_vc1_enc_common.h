//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_COMMON
#define _ENCODER_VC1_COMMON

#include "ippvc.h"
#include "ippi.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_enc_statistic.h"
#include "umc_me.h"

#define GetMVPredictionB                \
    if (left)                           \
    {                                   \
        mvC_F = &mv1;                   \
        mvC_B = &mv4;                   \
        left->GetMV(mvC_F,1,true);      \
        left->GetMV(mvC_B,1,false);     \
    }                                   \
    if (top)                            \
    {                                   \
        mvA_F = &mv2;                   \
        mvA_B = &mv5;                   \
        top->GetMV(mvA_F,2,true);       \
        top->GetMV(mvA_B,2,false);      \
    }                                   \
    if (topRight)                       \
    {                                   \
        mvB_F = &mv3;                   \
        mvB_B = &mv6;                   \
        topRight->GetMV(mvB_F,2,true);  \
        topRight->GetMV(mvB_B,2,false); \
    }                                   \
    if (!topRight && topLeft)           \
    {                                   \
        mvB_F = &mv3;                   \
        mvB_B = &mv6;                   \
        topLeft->GetMV(mvB_F,3,true);   \
        topLeft->GetMV(mvB_B,3,false);  \
    }
 #define    GetMVPredictionP(forward)   \
    if (left)                           \
    {                                   \
        left->GetMV(&mv1,1,forward);    \
        mvC = &mv1;                     \
    }                                   \
    if (top)                            \
    {                                   \
        top->GetMV(&mv2,2,forward);     \
        mvA = &mv2;                     \
    }                                   \
    if (topRight)                       \
    {                                   \
        topRight->GetMV(&mv3,2,forward);\
        mvB = &mv3;                     \
    }                                   \
    else if(topLeft)                    \
    {                                   \
        topLeft->GetMV(&mv3,3,forward); \
        mvB = &mv3;                     \
    }
 #define    GetMVPredictionPField(forward)  \
    if (left)                               \
    {                                       \
        if (!left->isIntra(1))              \
        {                                   \
            left->GetMV_F(&mv1,1,forward);    \
            mvC =&mv1;                      \
        }                                   \
    }                                       \
    if (top)                                \
    {                                       \
        if (!top->isIntra(2))               \
        {                                   \
            top->GetMV_F(&mv2,2,forward);     \
            mvA = &mv2;                     \
        }                                   \
    }                                       \
    if (topRight)                           \
    {                                       \
        if (!topRight->isIntra(2))          \
        {                                   \
            topRight->GetMV_F(&mv3,2,forward);\
            mvB =&mv3;                      \
        }                                   \
    }                                       \
    else if(topLeft)                        \
    {                                       \
        if (!topLeft->isIntra(2))           \
        {                                   \
            topLeft->GetMV_F(&mv3,2,forward); \
            mvB =&mv3;                      \
        }                                   \
    }
#define    GetMVPredictionP_0(forward)  \
    if (left)                           \
    {                                   \
        left->GetMV(&mv1,1,forward);    \
        mvC = &mv1;                     \
    }                                   \
    if (top)                            \
    {                                   \
        top->GetMV(&mv2,2,forward);     \
        mvA = &mv2;                     \
    }                                   \
    if (topLeft)                        \
    {                                   \
        topLeft->GetMV(&mv3,3,forward); \
        mvB = &mv3;                     \
    }                                   \
    else if(top)                        \
    {                                   \
        top->GetMV(&mv3,3,forward);     \
        mvB = &mv3;                     \
    }

#define    GetMVPredictionP_1(forward)  \
    pCurMBInfo->GetMV(&mv1,0,forward);  \
    mvC = &mv1;                         \
    if (top)                            \
    {                                   \
        top->GetMV(&mv2,3,forward);     \
        mvA = &mv2;                     \
    }                                   \
    if (topRight)                       \
    {                                   \
        topRight->GetMV(&mv3,2,forward);\
        mvB = &mv3;                     \
    }                                   \
    else if(top)                        \
    {                                   \
        top->GetMV(&mv3,2,forward);     \
        mvB = &mv3;                     \
    }

#define    GetMVPredictionP_2(forward)  \
    if (left)                           \
    {                                   \
        left->GetMV(&mv1,3,forward);    \
        mvC = &mv1;                     \
    }                                   \
    pCurMBInfo->GetMV(&mv2,0,forward);  \
    mvA = &mv2;                         \
    pCurMBInfo->GetMV(&mv3,1,forward);  \
    mvB = &mv3;                         \

#define    GetMVPredictionP_3(forward)  \
    pCurMBInfo->GetMV(&mv1,2,forward);  \
    mvC = &mv1;                         \
    pCurMBInfo->GetMV(&mv2,1,forward);  \
    mvA = &mv2;                         \
    pCurMBInfo->GetMV(&mv3,0,forward);  \
    mvB = &mv3;                         \

#define    GetMVPredictionP_0_F(forward)    \
    if (left)                               \
    {                                       \
        if (!left->isIntra(1))              \
        {                                   \
            left->GetMV_F(&mv1,1,forward);  \
            mvC = &mv1;                     \
        }                                   \
    }                                       \
    if (top)                                \
    {                                       \
        if (!top->isIntra(2))               \
        {                                   \
            top->GetMV_F(&mv2,2,forward);   \
            mvA = &mv2;                     \
        }                                   \
    }                                       \
    if (topLeft)                            \
    {                                       \
        if (!topLeft->isIntra(3))           \
        {                                   \
            topLeft->GetMV_F(&mv3,3,forward);\
            mvB = &mv3;                      \
        }                                   \
    }                                       \
    else if(top)                            \
    {                                       \
        if (!top->isIntra(3))               \
        {                                   \
            top->GetMV_F(&mv3,3,forward);   \
            mvB = &mv3;                     \
        }                                   \
    }

#define    GetMVPredictionP_1_F(forward)    \
    pCurMBInfo->GetMV_F(&mv1,0,forward);    \
    mvC = &mv1;                             \
    if (top)                                \
    {                                       \
        if (!top->isIntra(3))               \
        {                                   \
            top->GetMV_F(&mv2,3,forward);   \
            mvA = &mv2;                     \
        }                                   \
    }                                       \
    if (topRight)                           \
    {                                       \
        if (!topRight->isIntra(2))          \
        {                                   \
            topRight->GetMV_F(&mv3,2,forward);\
            mvB = &mv3;                     \
        }                                   \
    }                                       \
    else if(top)                            \
    {                                       \
        if (!top->isIntra(2))               \
        {                                   \
            top->GetMV_F(&mv3,2,forward);   \
            mvB = &mv3;                     \
        }                                   \
    }

#define    GetMVPredictionP_2_F(forward)    \
    if (left)                               \
    {                                       \
        if (!left->isIntra(3))              \
        {                                   \
            left->GetMV_F(&mv1,3,forward);  \
            mvC = &mv1;                     \
        }                                   \
    }                                       \
    pCurMBInfo->GetMV_F(&mv2,0,forward);    \
    mvA = &mv2;                             \
    pCurMBInfo->GetMV_F(&mv3,1,forward);    \
    mvB = &mv3;                             

#define    GetMVPredictionP_3_F(forward)    \
    pCurMBInfo->GetMV_F(&mv1,2,forward);    \
    mvC = &mv1;                             \
    pCurMBInfo->GetMV_F(&mv2,1,forward);    \
    mvA = &mv2;                             \
    pCurMBInfo->GetMV_F(&mv3,0,forward);    \
    mvB = &mv3;                             \

#define GetMVPredictionPBlk(blk)            \
    mvA = mvB =  mvC = 0;                   \
    switch (blk)                            \
    {                                       \
    case 0:                                 \
        GetMVPredictionP_0(true);  break;   \
    case 1:                                 \
        GetMVPredictionP_1(true);  break;   \
    case 2:                                 \
        GetMVPredictionP_2(true);  break;   \
    case 3:                                 \
        GetMVPredictionP_3(true);  break;   \
    };
#define GetMVPredictionPBlk_F(blk)            \
    mvA = mvB =  mvC = 0;                   \
    switch (blk)                            \
    {                                       \
    case 0:                                 \
        GetMVPredictionP_0_F(true);  break;   \
    case 1:                                 \
        GetMVPredictionP_1_F(true);  break;   \
    case 2:                                 \
        GetMVPredictionP_2_F(true);  break;   \
    case 3:                                 \
        GetMVPredictionP_3_F(true);  break;   \
    };
#define GetMVPredictionBBlk_F(blk, bForw)   \
    mvA = mvB =  mvC = 0;                   \
    switch (blk)                            \
    {                                       \
    case 0:                                 \
    GetMVPredictionP_0_F(bForw);  break;    \
    case 1:                                 \
    GetMVPredictionP_1_F(bForw);  break;    \
    case 2:                                 \
    GetMVPredictionP_2_F(bForw);  break;    \
    case 3:                                 \
    GetMVPredictionP_3_F(bForw);  break;    \
    };

#define GetMVPredictionBBlk(blk)             \
    mvA = mvB =  mvC = 0;                    \
    switch (blk)                             \
    {                                        \
    case 0:                                  \
        GetMVPredictionP_0(false);  break;   \
    case 1:                                  \
        GetMVPredictionP_1(false);  break;   \
    case 2:                                  \
        GetMVPredictionP_2(false);  break;   \
    case 3:                                  \
        GetMVPredictionP_3(false);  break;   \
    };

#define _SetIntraInterLumaFunctions                                                                                 \
    IntraTransformQuantFunction    IntraTransformQuantACFunctionLuma = (m_bUniformQuant) ?                          \
    ((MEParams&&MEParams->UseTrellisQuantization)? IntraTransformQuantTrellis: IntraTransformQuantUniform) :        \
    IntraTransformQuantNonUniform;                                                                                  \
    IntraTransformQuantFunction    IntraTransformQuantACFunctionChroma = (m_bUniformQuant) ?                        \
    ((MEParams&&MEParams->UseTrellisQuantizationChroma)? IntraTransformQuantTrellis: IntraTransformQuantUniform) :  \
    IntraTransformQuantNonUniform;                                                                                  \
    IntraTransformQuantFunction    IntraTransformQuantACFunction[6] =                                               \
    {                                                                                                               \
        IntraTransformQuantACFunctionLuma,                                                                          \
        IntraTransformQuantACFunctionLuma,                                                                          \
        IntraTransformQuantACFunctionLuma,                                                                          \
        IntraTransformQuantACFunctionLuma,                                                                          \
        IntraTransformQuantACFunctionChroma,                                                                        \
        IntraTransformQuantACFunctionChroma                                                                         \
    };                                                                                                              \
    InterTransformQuantFunction    InterTransformQuantACFunctionLuma = (m_bUniformQuant) ?                          \
    ((MEParams&&MEParams->UseTrellisQuantization)? InterTransformQuantTrellis: InterTransformQuantUniform) :        \
    InterTransformQuantNonUniform;                                                                                  \
    InterTransformQuantFunction    InterTransformQuantACFunctionChroma = (m_bUniformQuant) ?                        \
    ((MEParams&&MEParams->UseTrellisQuantizationChroma)? InterTransformQuantTrellis: InterTransformQuantUniform) :  \
    InterTransformQuantNonUniform;                                                                                  \
    InterTransformQuantFunction    InterTransformQuantACFunction[6] =                                               \
    {                                                                                                               \
        InterTransformQuantACFunctionLuma,                                                                          \
        InterTransformQuantACFunctionLuma,                                                                          \
        InterTransformQuantACFunctionLuma,                                                                          \
        InterTransformQuantACFunctionLuma,                                                                          \
        InterTransformQuantACFunctionChroma,                                                                        \
        InterTransformQuantACFunctionChroma                                                                         \
    };

namespace UMC_VC1_ENCODER
{
UMC::Status     Enc_IFrameUpdate        (UMC::MeBase* pME, UMC::MeParams* MEParams);
UMC::Status     Enc_I_FieldUpdate       (UMC::MeBase* pME, UMC::MeParams* MEParams);
UMC::Status     Enc_PFrameUpdate        (UMC::MeBase* pME, UMC::MeParams* MEParams);
UMC::Status     Enc_P_FieldUpdate       (UMC::MeBase* pME, UMC::MeParams* MEParams);
UMC::Status     Enc_PFrameUpdateMixed   (UMC::MeBase* pME, UMC::MeParams* MEParams);
UMC::Status     Enc_BFrameUpdate        (UMC::MeBase* pME, UMC::MeParams* MEParams);
UMC::Status     Enc_B_FieldUpdate       (UMC::MeBase* pME, UMC::MeParams* MEParams);

UMC::Status     Enc_Frame               (UMC::MeBase* pME, UMC::MeParams* MEParams);

bool          IsBottomField(bool bTopFieldFirst, bool bSecondField);
UMC::Status   SetFieldPlane(Ipp8u* pFieldPlane[3], Ipp8u* pPlane[3], Ipp32u planeStep[3], bool bBottom, Ipp32s firstRow=0);
UMC::Status   SetFieldPlane(Ipp8u** pFieldPlane, Ipp8u* pPlane, Ipp32u planeStep, bool bBottom, Ipp32s firstRow=0 );

UMC::Status   SetFieldStep(Ipp32u  fieldStep[3], Ipp32u planeStep[3]);
UMC::Status   SetFieldStep(Ipp32s* fieldStep, Ipp32u* planeStep);

UMC::Status   Set1RefFrwFieldPlane(Ipp8u* pPlane[3],       Ipp32u planeStep[3],
                                   Ipp8u* pFrwPlane[3],    Ipp32u frwStep[3],
                                   Ipp8u* pRaisedPlane[3], Ipp32u raisedStep[3],
                                   eReferenceFieldType     uiReferenceFieldType,
                                   bool bSecondField,      bool bBottom, Ipp32s firstRow=0);

UMC::Status   Set1RefFrwFieldPlane(Ipp8u** pPlane,       Ipp32s *planeStep,
                                   Ipp8u* pFrwPlane,    Ipp32u frwStep,
                                   Ipp8u* pRaisedPlane, Ipp32u raisedStep,
                                   eReferenceFieldType     uiReferenceFieldType,
                                   bool bSecondField,      bool bBottom, Ipp32s firstRow=0);

UMC::Status   Set2RefFrwFieldPlane(Ipp8u* pPlane[2][3], Ipp32u planeStep[2][3],
                                   Ipp8u* pFrwPlane[3], Ipp32u frwStep[3],
                                   Ipp8u* pRaisedPlane[3], Ipp32u raisedStep[3],
                                   bool bSecondField, bool bBottom, Ipp32s firstRow=0);

UMC::Status   Set2RefFrwFieldPlane(Ipp8u** pPlane1, Ipp32s* planeStep1,
                                   Ipp8u** pPlane2, Ipp32s* planeStep2,
                                   Ipp8u* pFrwPlane, Ipp32u frwStep,
                                   Ipp8u* pRaisedPlane, Ipp32u raisedStep,
                                   bool bSecondField, bool bBottom, Ipp32s firstRow=0);

UMC::Status   SetBkwFieldPlane(Ipp8u* pPlane[2][3], Ipp32u planeStep[2][3],
                               Ipp8u* pBkwPlane[3], Ipp32u bkwStep[3],
                               bool bBottom, Ipp32s firstRow=0);

UMC::Status   SetBkwFieldPlane(Ipp8u** pPlane1, Ipp32s* planeStep1,
                               Ipp8u** pPlane2, Ipp32s* planeStep2,
                               Ipp8u* pBkwPlane, Ipp32u bkwStep,
                               bool bBottom, Ipp32s firstRow=0);

bool IsFieldPicture(ePType PicType);

typedef struct
{
    Ipp8u               uiDecTypeAC;        //[0,2] - it's used to choose decoding table
    Ipp8u               uiCBPTab;           //[0,4] - it's used to choose decoding table
    Ipp8u               uiMVTab;            //[0,4] - it's used to choose decoding table
    Ipp8u               uiDecTypeDCIntra;   //[0,1] - it's used to choose decoding table
}VLCTablesIndex;

typedef struct
{
    eMVModes             uiMVMode;      /*    VC1_ENC_1MV_HALF_BILINEAR
                                        //    VC1_ENC_1MV_QUARTER_BICUBIC
                                        //    VC1_ENC_1MV_HALF_BICUBIC
                                        //    VC1_ENC_MIXED_QUARTER_BICUBIC
                                        */
    Ipp8u               uiMVRangeIndex;
    Ipp8u               uiDMVRangeIndex;
    Ipp8u               nReferenceFrameDist;
    eReferenceFieldType uiReferenceFieldType;
    sFraction           uiBFraction;     // 0xff/0xff - BI frame
    VLCTablesIndex      sVLCTablesIndex;
    bool                bMVMixed;
    bool                bTopFieldFirst;
}InitPictureParams ;



 //-------------FORWARD-TRANSFORM-QUANTIZATION---------------------------
#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
        IppStatus _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(Ipp16s* pSrcDst, Ipp32u srcDstStep);
        IppStatus _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R(Ipp16s* pSrcDst, Ipp32u srcDstStep);
        IppStatus _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R(Ipp16s* pSrcDst, Ipp32u srcDstStep);
        IppStatus _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R(Ipp16s* pSrcDst, Ipp32u srcDstStep);
    #endif // _VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#ifndef _VC1_ENC_OWN_FUNCTIONS_

    #define _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R      ippiTransform8x8Fwd_VC1_16s_C1IR
    #define _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R      ippiTransform4x8Fwd_VC1_16s_C1IR
    #define _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R      ippiTransform8x4Fwd_VC1_16s_C1IR
    #define _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R      ippiTransform4x4Fwd_VC1_16s_C1IR

#endif // not _VC1_ENC_OWN_FUNCTIONS_


#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
    IppStatus _own_ippiQuantIntraUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant);
    IppStatus _own_ippiQuantIntraNonUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant);

    IppStatus _own_ippiQuantInterUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant, IppiSize roiSize);
    IppStatus _own_ippiQuantInterNonUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant, IppiSize roiSize);
#endif // _VC1_ENC_OWN_FUNCTIONS
#endif //UMC_RESTRICTED_CODE

    IppStatus _own_ippiQuantIntraTrellis(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant, Ipp8s *pRoundControl,Ipp32s roundControlStep);
    IppStatus _own_ippiQuantInterTrellis(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant, IppiSize roiSize,Ipp8s *pRoundControl,Ipp32s roundControlStep);



#ifndef _VC1_ENC_OWN_FUNCTIONS_

    #define _own_ippiQuantIntraUniform     ippiQuantIntraUniform_VC1_16s_C1IR
    #define _own_ippiQuantIntraNonUniform  ippiQuantIntraNonuniform_VC1_16s_C1IR
    #define _own_ippiQuantInterUniform     ippiQuantInterUniform_VC1_16s_C1IR
    #define _own_ippiQuantInterNonUniform  ippiQuantInterNonuniform_VC1_16s_C1IR

#endif // not _VC1_ENC_OWN_FUNCTIONS_


    inline IppStatus IntraTransformQuantUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,
                                                Ipp32s DCQuant, Ipp32s doubleQuant,
                                                Ipp8s* /*pRoundControl*/, Ipp32s /*roundControlStep*/)
    {
        IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        Sts = _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
        VC1_ENC_IPP_CHECK(Sts);

        pSrcDst[0] = pSrcDst[0]/(Ipp16s)DCQuant;
        Sts =  _own_ippiQuantIntraUniform(pSrcDst, srcDstStep, doubleQuant);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }

    inline IppStatus IntraTransformQuantNonUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,
                                                   Ipp32s DCQuant, Ipp32s doubleQuant,
                                                   Ipp8s* /*pRoundControl*/, Ipp32s /*roundControlStep*/)
    {
        IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        Sts = _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
        VC1_ENC_IPP_CHECK(Sts);

        pSrcDst[0] = pSrcDst[0]/(Ipp16s)DCQuant;
        Sts =  _own_ippiQuantIntraNonUniform(pSrcDst, srcDstStep, doubleQuant);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }

#define _sign(x) (((x)<0)?-1:!!(x))
#define _abs(x)   ((x<0)? -(x):(x))
    inline IppStatus IntraTransformQuantTrellis(Ipp16s* pSrcDst, Ipp32s srcDstStep,
                                                Ipp32s DCQuant, Ipp32s doubleQuant,
                                                Ipp8s* pRoundControl, Ipp32s roundControlStep)
    {
        IppStatus Sts = ippStsNoErr;

        IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        Sts = _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
        VC1_ENC_IPP_CHECK(Sts);

      //  pSrcDst[0] = (pSrcDst[0])/(Ipp16s)DCQuant;
        Ipp16s abs_pixel    = _abs(pSrcDst[0]);
        Ipp16s round        = (pRoundControl[0])*DCQuant;
        Ipp16s quant_pixel  = 0;

        quant_pixel = (round < abs_pixel)?(abs_pixel - round)/DCQuant:0;

        pSrcDst[0] = (pSrcDst[0]<0)? -quant_pixel:(Ipp16s)quant_pixel;
        Sts =  _own_ippiQuantIntraTrellis(pSrcDst, srcDstStep, doubleQuant,pRoundControl,roundControlStep);
        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }
#undef abs
#undef sign

    inline IppStatus InterTransformQuantUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,
                                                Ipp32s TransformType, Ipp32s doubleQuant,
                                                Ipp8s* /*pRoundControl*/, Ipp32s /*roundControlStep*/)
    {
        IppStatus Sts = ippStsNoErr;
        IppiSize roiSize = {8, 8};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        switch (TransformType)
        {
            case VC1_ENC_8x8_TRANSFORM:
                Sts = _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(pSrcDst, srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterUniform(pSrcDst, srcDstStep, doubleQuant, roiSize);
                break;
            case VC1_ENC_4x8_TRANSFORM:
                roiSize.width  = 4;
                roiSize.height = 8;

                Sts =  _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts =  _own_ippiQuantInterUniform(pSrcDst, srcDstStep, doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts =  _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R(pSrcDst+4,srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts =  _own_ippiQuantInterUniform(pSrcDst + 4, srcDstStep, doubleQuant, roiSize);
                break;
            case VC1_ENC_8x4_TRANSFORM:
                roiSize.width  = 8;
                roiSize.height = 4;

                Sts = _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterUniform(pSrcDst, srcDstStep, doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep),
                                                                                             srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterUniform((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep), srcDstStep, doubleQuant, roiSize);

                break;
            case VC1_ENC_4x4_TRANSFORM:
                roiSize.height = 4;
                roiSize.width  = 4;

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterUniform(pSrcDst, srcDstStep, doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R(pSrcDst + 4,srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterUniform(pSrcDst + 4, srcDstStep, doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep),
                                                                                            srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterUniform((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep),
                                                                        srcDstStep, doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep)+4,
                                                                                                srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterUniform((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep)+4, srcDstStep, doubleQuant, roiSize);
                break;
            default:
                return ippStsErr;
                break;
        }
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }

    inline IppStatus InterTransformQuantNonUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,
                                                   Ipp32s TransformType, Ipp32s doubleQuant,
                                                   Ipp8s* /*pRoundControl*/, Ipp32s /*roundControlStep*/)
    {
        IppStatus Sts = ippStsNoErr;
        IppiSize roiSize = {8, 8};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        switch (TransformType)
        {
            case VC1_ENC_8x8_TRANSFORM:
                Sts = _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(pSrcDst, srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform(pSrcDst, srcDstStep, doubleQuant, roiSize);
                break;
            case VC1_ENC_4x8_TRANSFORM:
                roiSize.width  = 4;
                roiSize.height = 8;

                Sts = _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform(pSrcDst, srcDstStep, doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts =  _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R(pSrcDst+4,srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform(pSrcDst + 4, srcDstStep, doubleQuant, roiSize);

                break;
            case VC1_ENC_8x4_TRANSFORM:
                roiSize.width  = 8;
                roiSize.height = 4;

                Sts = _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform(pSrcDst, srcDstStep, doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep),
                                                                                            srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep),
                                                                        srcDstStep, doubleQuant, roiSize);
                break;
            case VC1_ENC_4x4_TRANSFORM:
                roiSize.height = 4;
                roiSize.width  = 4;

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform(pSrcDst, srcDstStep, doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R(pSrcDst + 4,srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform(pSrcDst + 4, srcDstStep, doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep),
                                                                                             srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep), srcDstStep,
                                                                                    doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep)+4,
                                                                                               srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep)+4, srcDstStep, doubleQuant, roiSize);
                break;
            default:
                return ippStsErr;
                break;
        }
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }
    inline IppStatus InterTransformQuantTrellis(Ipp16s* pSrcDst, Ipp32s srcDstStep,
                                                Ipp32s TransformType, Ipp32s doubleQuant,
                                                Ipp8s* pRoundControl, Ipp32s roundControlStep)
    {
        IppStatus Sts = ippStsNoErr;
        IppiSize roiSize = {8, 8};

        IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        switch (TransformType)
        {
        case VC1_ENC_8x8_TRANSFORM:
            Sts = _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(pSrcDst, srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts = _own_ippiQuantInterTrellis(pSrcDst, srcDstStep, doubleQuant, roiSize,pRoundControl,roundControlStep);
            break;
        case VC1_ENC_4x8_TRANSFORM:
            roiSize.width  = 4;
            roiSize.height = 8;

            Sts =  _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts =  _own_ippiQuantInterTrellis(pSrcDst, srcDstStep, doubleQuant, roiSize,pRoundControl,roundControlStep);
            VC1_ENC_IPP_CHECK(Sts);

            Sts =  _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R(pSrcDst+4,srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts =  _own_ippiQuantInterTrellis(pSrcDst + 4, srcDstStep, doubleQuant, roiSize,pRoundControl+4,roundControlStep);
            break;
        case VC1_ENC_8x4_TRANSFORM:
            roiSize.width  = 8;
            roiSize.height = 4;

            Sts = _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts = _own_ippiQuantInterTrellis(pSrcDst, srcDstStep, doubleQuant, roiSize,pRoundControl,roundControlStep);
            VC1_ENC_IPP_CHECK(Sts);

            Sts = _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep),
                srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts = _own_ippiQuantInterTrellis((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep), srcDstStep, doubleQuant, roiSize,pRoundControl + 4*roundControlStep,roundControlStep);

            break;
        case VC1_ENC_4x4_TRANSFORM:
            roiSize.height = 4;
            roiSize.width  = 4;

            Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts = _own_ippiQuantInterTrellis(pSrcDst, srcDstStep, doubleQuant, roiSize,pRoundControl,roundControlStep);
            VC1_ENC_IPP_CHECK(Sts);

            Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R(pSrcDst + 4,srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts = _own_ippiQuantInterTrellis(pSrcDst + 4, srcDstStep, doubleQuant, roiSize,pRoundControl+4,roundControlStep);
            VC1_ENC_IPP_CHECK(Sts);

            Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep),
                srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts = _own_ippiQuantInterTrellis((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep),srcDstStep, doubleQuant, roiSize, pRoundControl+4*roundControlStep,roundControlStep);
            VC1_ENC_IPP_CHECK(Sts);

            Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep)+4,
                srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts = _own_ippiQuantInterTrellis((Ipp16s*)((Ipp8u*)pSrcDst+4*srcDstStep)+4, srcDstStep, doubleQuant, roiSize,pRoundControl +4*roundControlStep + 4,roundControlStep);
            break;
        default:
            return ippStsErr;
            break;
        }
        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }
    typedef IppStatus (*IntraTransformQuantFunction)  (Ipp16s* pSrcDst, Ipp32s srcDstStep,
                                                       Ipp32s DCQuant, Ipp32s doubleQuant,
                                                       Ipp8s* pRoundControl, Ipp32s roundControlStep);
    typedef IppStatus (*InterTransformQuantFunction)  (Ipp16s* pSrcDst, Ipp32s srcDstStep,
                                                       Ipp32s TransformType, Ipp32s doubleQuant,
                                                       Ipp8s* pRoundControl, Ipp32s roundControlStep);

 //-------------INVERSE-TRANSFORM-QUANTIZATION---------------------------
#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
    IppStatus _own_ippiQuantInvIntraUniform_VC1_16s_C1R(const Ipp16s* pSrc, Ipp32s srcStep,
                                                        Ipp16s* pDst, Ipp32s dstStep,
                                                          Ipp32s doubleQuant, IppiSize* pDstSizeNZ);
    IppStatus _own_ippiQuantInvIntraNonUniform_VC1_16s_C1R(const Ipp16s* pSrc, Ipp32s srcStep,
                                                        Ipp16s* pDst, Ipp32s dstStep,
                                                          Ipp32s doubleQuant, IppiSize* pDstSizeNZ);
    IppStatus _own_ippiQuantInvInterUniform_VC1_16s_C1R(const Ipp16s* pSrc, Ipp32s srcStep,
                                                     Ipp16s* pDst, Ipp32s dstStep,
                                                       Ipp32s doubleQuant, IppiSize roiSize,
                                                       IppiSize* pDstSizeNZ);
    IppStatus _own_ippiQuantInvInterNonUniform_VC1_16s_C1R(const Ipp16s* pSrc, Ipp32s srcStep,
                                                     Ipp16s* pDst, Ipp32s dstStep,
                                                       Ipp32s doubleQuant, IppiSize roiSize,
                                                       IppiSize* pDstSizeNZ);
#endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_ippiQuantInvIntraUniform_VC1_16s_C1R     ippiQuantInvIntraUniform_VC1_16s_C1R
    #define _own_ippiQuantInvIntraNonUniform_VC1_16s_C1R  ippiQuantInvIntraNonuniform_VC1_16s_C1R
    #define _own_ippiQuantInvInterUniform_VC1_16s_C1R     ippiQuantInvInterUniform_VC1_16s_C1R
    #define _own_ippiQuantInvInterNonUniform_VC1_16s_C1R  ippiQuantInvInterNonuniform_VC1_16s_C1R
#endif // not _VC1_ENC_OWN_FUNCTIONS_

 //-------------SMOOTHING---------------------------

#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
IppStatus _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R        (Ipp16s* pSrcLeft, Ipp32s srcLeftStep,
                                                               Ipp16s* pSrcRight, Ipp32s srcRightStep,
                                                               Ipp8u* pDst, Ipp32s dstStep,
                                                               Ipp32u fieldNeighbourFlag,
                                                               Ipp32u edgeDisableFlag);
IppStatus _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R        (Ipp16s* pSrcUpper, Ipp32s srcUpperStep,
                                                               Ipp16s* pSrcBottom, Ipp32s srcBottomStep,
                                                               Ipp8u* pDst, Ipp32s dstStep,
                                                               Ipp32u edgeDisableFlag);
IppStatus _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R      (Ipp16s* pSrcUpper, Ipp32s srcUpperStep,
                                                               Ipp16s* pSrcBottom, Ipp32s srcBottomStep,
                                                               Ipp8u* pDst, Ipp32s dstStep);
IppStatus _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R      (Ipp16s* pSrcLeft, Ipp32s srcLeftStep,
                                                               Ipp16s* pSrcRight, Ipp32s srcRightStep,
                                                               Ipp8u* pDst, Ipp32s dstStep);
    #endif // _VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R      ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R
    #define _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R      ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R
    #define _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R    ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R
    #define _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R    ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R
#endif // not _VC1_ENC_OWN_FUNCTIONS_

IppStatus _own_ippiSmoothingChroma_NV12_HorEdge_VC1_16s8u_C2R (Ipp16s* pSrcUpperU, Ipp32s srcUpperStepU,
                                                               Ipp16s* pSrcBottomU, Ipp32s srcBottomStepU,
                                                               Ipp16s* pSrcUpperV, Ipp32s srcUpperStepV,
                                                               Ipp16s* pSrcBottomV, Ipp32s srcBottomStepV,
                                                               Ipp8u* pDst, Ipp32s dstStep);

IppStatus _own_ippiSmoothingChroma_NV12_VerEdge_VC1_16s8u_C2R (Ipp16s* pSrcLeftU,  Ipp32s srcLeftStepU,
                                                               Ipp16s* pSrcRightU, Ipp32s srcRightStepU,
                                                               Ipp16s* pSrcLeftV,  Ipp32s srcLeftStepV,
                                                               Ipp16s* pSrcRightV, Ipp32s srcRightStepV,
                                                               Ipp8u* pDst, Ipp32s dstStep);

//-------------FORWARD-TRANSFORM-QUANTIZATION---------------------------
#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
        IppStatus _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR(Ipp16s* pSrcDst, Ipp32u srcDstStep, IppiSize DstSize);
        IppStatus _own_ippiTransformBlock4x8Inv_VC1_16s_C1IR(Ipp16s* pSrcDst, Ipp32u srcDstStep, IppiSize DstSize);
        IppStatus _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR(Ipp16s* pSrcDst, Ipp32u srcDstStep, IppiSize DstSize);
        IppStatus _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR(Ipp16s* pSrcDst, Ipp32u srcDstStep, IppiSize DstSize);
    #endif // _VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#ifndef _VC1_ENC_OWN_FUNCTIONS_

    #define _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR      ippiTransform8x8Inv_VC1_16s_C1IR
    #define _own_ippiTransformBlock4x8Inv_VC1_16s_C1IR      ippiTransform4x8Inv_VC1_16s_C1IR
    #define _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR      ippiTransform8x4Inv_VC1_16s_C1IR
    #define _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR      ippiTransform4x4Inv_VC1_16s_C1IR

#endif // not _VC1_ENC_OWN_FUNCTIONS_

    inline IppStatus IntraInvTransformQuantUniform(Ipp16s* pSrc, Ipp32s srcStep,
                                                   Ipp16s* pDst, Ipp32s dstStep,
                                                   Ipp32s DCQuant, Ipp32s doubleQuant)
    {
        IppStatus Sts = ippStsNoErr;
        IppiSize DstSize = {8, 8};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        Sts = _own_ippiQuantInvIntraUniform_VC1_16s_C1R(pSrc, srcStep, pDst, dstStep,
                                                                doubleQuant, &DstSize);
        pDst[0] = pSrc[0]*(Ipp16s)DCQuant;
        VC1_ENC_IPP_CHECK(Sts);
        Sts = _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR(pDst,dstStep,DstSize);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

         return Sts;
    }

    inline IppStatus IntraInvTransformQuantNonUniform(Ipp16s* pSrc, Ipp32s srcStep,
                                                      Ipp16s* pDst, Ipp32s dstStep,
                                                      Ipp32s DCQuant, Ipp32s doubleQuant)
    {
        IppStatus Sts = ippStsNoErr;
        IppiSize DstSize = {8, 8};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        Sts = _own_ippiQuantInvIntraNonUniform_VC1_16s_C1R(pSrc, srcStep, pDst, dstStep,
                                                                    doubleQuant, &DstSize);
        pDst[0] = pSrc[0]*(Ipp16s)DCQuant;
        VC1_ENC_IPP_CHECK(Sts);
        Sts = _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR(pDst,dstStep,DstSize);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

         return Sts;
    }

    inline IppStatus InterInvTransformQuantUniform(Ipp16s* pSrc, Ipp32s srcStep,
                                                   Ipp16s* pDst, Ipp32s dstStep,
                                                   Ipp32s doubleQuant, Ipp32s TransformType)
    {
        IppStatus Sts = ippStsNoErr;
        IppiSize roiSize = {8, 8};
        IppiSize dstSize = {8, 8};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        switch (TransformType)
        {
            case VC1_ENC_8x8_TRANSFORM:
                Sts = _own_ippiQuantInvInterUniform_VC1_16s_C1R(pSrc, srcStep, pDst, dstStep,
                                                                  doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR(pDst,dstStep,dstSize);
                break;
            case VC1_ENC_4x8_TRANSFORM:
                roiSize.width  = 4;
                roiSize.height = 8;

                Sts = _own_ippiQuantInvInterUniform_VC1_16s_C1R(pSrc, srcStep, pDst, dstStep,
                                                                doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = Sts = _own_ippiTransformBlock4x8Inv_VC1_16s_C1IR(pDst,dstStep,dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiQuantInvInterUniform_VC1_16s_C1R(pSrc + 4, srcStep, pDst + 4, dstStep,
                                                                        doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = Sts = _own_ippiTransformBlock4x8Inv_VC1_16s_C1IR(pDst + 4, dstStep, dstSize);

                break;
            case VC1_ENC_8x4_TRANSFORM:
                roiSize.width  = 8;
                roiSize.height = 4;

                Sts = _own_ippiQuantInvInterUniform_VC1_16s_C1R(pSrc, srcStep,
                                        pDst, dstStep,  doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR(pDst,dstStep,dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts =_own_ippiQuantInvInterUniform_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrc + 4*srcStep), srcStep,
                    (Ipp16s*)((Ipp8u*)pDst + 4*dstStep) , dstStep,  doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR((Ipp16s*)((Ipp8u*)pDst + 4*dstStep),dstStep,dstSize);
                break;
            case VC1_ENC_4x4_TRANSFORM:
                roiSize.height = 4;
                roiSize.width  = 4;

                Sts = _own_ippiQuantInvInterUniform_VC1_16s_C1R(pSrc, srcStep,
                                                            pDst, dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR(pDst,dstStep,dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiQuantInvInterUniform_VC1_16s_C1R(pSrc + 4, srcStep,
                                                        pDst + 4, dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR(pDst+4,dstStep,dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiQuantInvInterUniform_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrc+4*srcStep), srcStep,
                    (Ipp16s*)((Ipp8u*)pDst+4*dstStep), dstStep,  doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR((Ipp16s*)((Ipp8u*)pDst + 4*dstStep),dstStep,
                                                                                            dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiQuantInvInterUniform_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrc + 4*srcStep)+4, srcStep,
                    (Ipp16s*)((Ipp8u*)pDst + 4*dstStep)+4, dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR((Ipp16s*)((Ipp8u*)pDst + 4*dstStep)+4,dstStep, dstSize);
                break;
            default:
                return ippStsErr;
                break;
        }
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }

    inline IppStatus InterInvTransformQuantNonUniform(Ipp16s* pSrc, Ipp32s srcStep,
                                                      Ipp16s* pDst, Ipp32s dstStep,
                                                      Ipp32s doubleQuant, Ipp32s TransformType)
    {
        IppStatus Sts = ippStsNoErr;
        IppiSize roiSize = {8, 8};
        IppiSize dstSize = {8, 8};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        switch (TransformType)
        {
            case VC1_ENC_8x8_TRANSFORM:
                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R(pSrc, srcStep, pDst, dstStep,
                    doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = Sts = _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR(pDst, dstStep, dstSize);
                break;
            case VC1_ENC_4x8_TRANSFORM:
                roiSize.width  = 4;
                roiSize.height = 8;

                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R(pSrc, srcStep, pDst, dstStep,
                    doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x8Inv_VC1_16s_C1IR(pDst, dstStep, dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts =_own_ippiQuantInvInterNonUniform_VC1_16s_C1R(pSrc + 4, srcStep,
                    pDst + 4, dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x8Inv_VC1_16s_C1IR(pDst + 4, dstStep, dstSize);
                break;
            case VC1_ENC_8x4_TRANSFORM:
                roiSize.width  = 8;
                roiSize.height = 4;

                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R(pSrc, srcStep, pDst, dstStep,
                    doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR(pDst,dstStep,dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrc + 4*srcStep), srcStep,
                    (Ipp16s*)((Ipp8u*)pDst + 4*dstStep) , dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR((Ipp16s*)((Ipp8u*)pDst + 4*dstStep),dstStep,dstSize);
                break;
            case VC1_ENC_4x4_TRANSFORM:
                roiSize.height = 4;
                roiSize.width  = 4;

                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R(pSrc, srcStep,
                    pDst, dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR(pDst,dstStep,dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R(pSrc + 4, srcStep,
                    pDst + 4, dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR(pDst+4,dstStep,dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrc+4*srcStep), srcStep,
                    (Ipp16s*)((Ipp8u*)pDst+4*dstStep), dstStep,  doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR((Ipp16s*)((Ipp8u*)pDst + 4*dstStep),dstStep, dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R((Ipp16s*)((Ipp8u*)pSrc + 4*srcStep)+4, srcStep,
                    (Ipp16s*)((Ipp8u*)pDst + 4*dstStep)+4, dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR((Ipp16s*)((Ipp8u*)pDst + 4*dstStep)+4,dstStep, dstSize);
                break;
            default:
                return ippStsErr;
                break;
        }

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
        return Sts;
    }

    typedef IppStatus (*IntraInvTransformQuantFunction)  (Ipp16s* pSrc, Ipp32s srcStep,
                                                       Ipp16s* pDst, Ipp32s dstStep,
                                                       Ipp32s DCQuant, Ipp32s doubleQuant);
    typedef IppStatus (*InterInvTransformQuantFunction)  (Ipp16s* pSrc, Ipp32s srcStep,
                                                       Ipp16s* pDst, Ipp32s dstStep,
                                                       Ipp32s doubleQuant, Ipp32s TransformType);

#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
        IppStatus _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag);
        IppStatus _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag);
        IppStatus _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag);
        IppStatus _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag);
    #endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR     ippiFilterDeblockingLuma_VerEdge_VC1_8u_C1IR
    #define _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR   ippiFilterDeblockingChroma_VerEdge_VC1_8u_C1IR
    #define _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR     ippiFilterDeblockingLuma_HorEdge_VC1_8u_C1IR
    #define _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR   ippiFilterDeblockingChroma_HorEdge_VC1_8u_C1IR
#endif // not _VC1_ENC_OWN_FUNCTIONS_


IppStatus _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,
                                                               Ipp32s EdgeDisabledFlagU, Ipp32s EdgeDisabledFlagV);
IppStatus _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,
                                                               Ipp32s EdgeDisabledFlagU, Ipp32s EdgeDisabledFlagV);

    ///////////////////

    typedef void        (*CalculateChromaFunction)(sCoordinate LumaMV, sCoordinate * pChroma);

    Ipp8u               Get_CBPCY(Ipp32u MBPatternCur, Ipp32u CBPCYTop, Ipp32u CBPCYLeft, Ipp32u CBPCYULeft);
    //eDirection          GetDCPredDirectionSM(Ipp16s dcA, Ipp16s dcB, Ipp16s dcC);

 //---BEGIN------------------------Copy block, MB functions-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
    IppStatus    _own_Copy8x8_16x16_8u16s  (const Ipp8u* pSrc, Ipp32u srcStep, Ipp16s* pDst, Ipp32u dstStep, IppiSize roiSize);
    IppStatus    _own_ippiConvert_16s8u_C1R(const Ipp16s* pSrc,Ipp32u srcStep, Ipp8u* pDst,  Ipp32u dstStep, IppiSize roiSize );

    #endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_Copy8x8_16x16_8u16s   ippiConvert_8u16s_C1R
    #define _own_ippiConvert_16s8u_C1R ippiConvert_16s8u_C1R
#endif
 //---END------------------------Copy block, MB functions-----------------------------------------------


    //void                Diff_1x7_16s(Ipp16s* pSrcDst, Ipp16s step, const Ipp16s* pDst);
    //void                Diff_7x1_16s(Ipp16s* pSrc, const Ipp16s* pDst);
    Ipp32s              SumSqDiff_1x7_16s(Ipp16s* pSrc, Ipp32u step, Ipp16s* pPred);
    Ipp32s              SumSqDiff_7x1_16s(Ipp16s* pSrc, Ipp16s* pPred);
    //Ipp32u              GetNumZeros(Ipp16s* pSrc, Ipp32u srcStep, bool bIntra);
    //Ipp32u              GetNumZeros(Ipp16s* pSrc, Ipp32u srcStep);

    Ipp8u               GetMode( Ipp8u &run,  Ipp16s &level, const Ipp8u *pTableDR, const Ipp8u *pTableDL, bool& sign);
    //bool                GetRLCode(Ipp32s run, Ipp32s level, IppVCHuffmanSpec_32s *pTable, Ipp32s &code, Ipp32s &len);
    //Ipp8u               GetLength_16s(Ipp16s value);
    //Ipp8u               Zigzag( Ipp16s*                pBlock,
    //                        Ipp32u                 blockStep,
    //                        bool                   bIntra,
    //                        const Ipp8u*           pScanMatrix,
    //                        Ipp8u*                 pRuns,
    //                        Ipp16s*                pLevels);

    void                Diff8x8 (const Ipp16s* pSrc1, Ipp32s src1Step,
                            const Ipp16s* pSrc2, Ipp32s src2Step,
                            Ipp16s* pDst, Ipp32s dstStep,Ipp32u pred);

 //---BEGIN------------------------Add constant to block-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
    IppStatus  _own_Diff8x8C_16s(Ipp16s c, Ipp16s*  pBlock, Ipp32u   blockStep, IppiSize roiSize, int scaleFactor);
    IppStatus  _own_Add8x8C_16s (Ipp16s c, Ipp16s*  pBlock, Ipp32u   blockStep, IppiSize roiSize, int scaleFactor);
    #endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE
#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_Diff8x8C_16s ippiSubC_16s_C1IRSfs
    #define _own_Add8x8C_16s  ippiAddC_16s_C1IRSfs
#endif
 //---End------------------------Add constant to block-----------------------------------------------


//---Begin------------------------Get difference-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
    IppStatus _own_ippiGetDiff8x8_8u16s_C1(Ipp8u*  pBlock,       Ipp32u   blockStep,
                            Ipp8u*  pPredBlock,   Ipp32u   predBlockStep,
                            Ipp16s* pDst,         Ipp32u   dstStep,
                            Ipp16s* pDstPredictor,Ipp32s  dstPredictorStep,
                            Ipp32s  mcType,       Ipp32s  roundControl);
    IppStatus _own_ippiGetDiff16x16_8u16s_C1(Ipp8u*  pBlock,       Ipp32u   blockStep,
                            Ipp8u*  pPredBlock,   Ipp32u   predBlockStep,
                            Ipp16s* pDst,         Ipp32u   dstStep,
                            Ipp16s* pDstPredictor,Ipp32s  dstPredictorStep,
                            Ipp32s  mcType,       Ipp32s  roundControl);
    #endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE
#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_ippiGetDiff8x8_8u16s_C1    ippiGetDiff8x8_8u16s_C1
    #define _own_ippiGetDiff16x16_8u16s_C1  ippiGetDiff16x16_8u16s_C1
#endif

//---End------------------------Get difference-----------------------------------------------

//---Begin---------------------Motion compensation-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_

    IppStatus  _own_Add16x16_8u16s(const Ipp8u*  pSrcRef,  Ipp32u   srcRefStep,
                              const Ipp16s* pSrcYData,Ipp32u   srcYDataStep,
                              Ipp8u* pDst, Ipp32u   dstStep,
                              Ipp32s mctype, Ipp32s roundControl);

    IppStatus  _own_Add8x8_8u16s  (const Ipp8u*  pSrcRef,  Ipp32u   srcRefStep,
                              const Ipp16s* pSrcYData,Ipp32u   srcYDataStep,
                              Ipp8u* pDst, Ipp32u   dstStep,
                              Ipp32s mctype, Ipp32s roundControl);
    #endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE
#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_Add16x16_8u16s    ippiMC16x16_8u_C1
    #define _own_Add8x8_8u16s      ippiMC8x8_8u_C1
#endif


//---End---------------------Motion compensation-----------------------------------------------


    void                Copy8x8_16s(Ipp16s*  pSrc, Ipp32u   srcStep,
                            Ipp16s*  pDst, Ipp32u   dstStep);
    //void                Copy16x16_16s( Ipp16s*  pSrc, Ipp32u   srcStep,
    //                        Ipp16s*  pDst, Ipp32u   dstStep);

    Ipp16s median3(Ipp16s a, Ipp16s b, Ipp16s c);
    Ipp16s median4(Ipp16s a, Ipp16s b, Ipp16s c, Ipp16s d);

    void PredictMV(sCoordinate* predA,sCoordinate* predB,sCoordinate* predC, sCoordinate* res);

    inline Ipp16s VC1abs(Ipp16s value);
    void ScalePredict(sCoordinate * MV, Ipp32s x,Ipp32s y,sCoordinate MVPredMin, sCoordinate MVPredMax);
    //Ipp32s FullSearch(const Ipp8u* pSrc, Ipp32u srcStep, const Ipp8u* pPred, Ipp32u predStep, sCoordinate Min, sCoordinate Max,sCoordinate * MV);
    //Ipp32s SumBlock16x16(const Ipp8u* pSrc, Ipp32u srcStep);
    //Ipp32s SumBlockDiff16x16(const Ipp8u* pSrc1, Ipp32u srcStep1,const Ipp8u* pSrc2, Ipp32u srcStep2);
    //Ipp32s SumBlockDiffBPred16x16(const Ipp8u* pSrc, Ipp32u srcStep,const Ipp8u* pPred1, Ipp32u predStep1,
    //                                                            const Ipp8u* pPred2, Ipp32u predStep2);
    void  GetMVDirectHalf  (Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void  GetMVDirectQuarter(Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void  GetChromaMV   (sCoordinate LumaMV, sCoordinate * pChroma);
    void  GetChromaMVFast(sCoordinate LumaMV, sCoordinate * pChroma);

    void GetIntQuarterMV(sCoordinate MV,sCoordinate *pMVInt, sCoordinate *pMVIntQuarter);
    //void GetQMV(sCoordinate *pMV,sCoordinate MVInt, sCoordinate MVIntQuarter);

    //void GetBlockType(Ipp16s* pBlock, Ipp32s step, Ipp8u Quant, eTransformType& type);
    //bool GetMBTSType(Ipp16s** ppBlock, Ipp32u* pStep, Ipp8u Quant , eTransformType* pTypes);

    //Ipp8u GetBlockPattern(Ipp16s* pSrc, Ipp32u srcStep);

    Ipp8u HybridPrediction(     sCoordinate       * mvPred,
                                const sCoordinate * MV,
                                const sCoordinate * mvA,
                                const sCoordinate * mvC,
                                Ipp32u              th);

//--Begin----------Interpolation---------------
#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
IppStatus _own_ippiInterpolateQPBicubic_VC1_8u_C1R   (const IppVCInterpolate_8u* inter_struct);
IppStatus _own_ippiInterpolateQPBilinear_VC1_8u_C1R  (const IppVCInterpolate_8u* inter_struct);

typedef IppStatus  ( /*__STDCALL*/ *InterpolateFunction)  (const IppVCInterpolate_8u* pParam);
typedef IppStatus  ( /*__STDCALL*/ *InterpolateFunctionPadding) (const IppVCInterpolateBlock_8u* interpolateInfo,    const Ipp32s*  pLUTTop, const Ipp32s*  pLUTBottom, Ipp32u  OppositePadding, Ipp32u  fieldPrediction, Ipp32u  RoundControl, Ipp32u  isPredBottom);

#endif
#endif

IppStatus _own_ippiInterpolateQPBilinear_VC1_8u_C1R_NV12  (const IppVCInterpolate_8u* inter_struct);

#ifndef _VC1_ENC_OWN_FUNCTIONS_

#if defined (_WIN32_WCE) && defined (_M_IX86) && defined (__stdcall)
  #define _IPP_STDCALL_CDECL
  #undef __stdcall
#endif

    typedef IppStatus  ( __STDCALL *InterpolateFunction)  (const IppVCInterpolate_8u* pParam);

    typedef IppStatus  (*InterpolateFunctionPadding) (const IppVCInterpolateBlock_8u* interpolateInfo,
                                                     const Ipp8u*                   pLUTTop,
                                                     const Ipp8u*                   pLUTBottom,
                                                     Ipp32u                          OppositePadding,
                                                     Ipp32u                          fieldPrediction,
                                                     Ipp32u                          RoundControl,
                                                     Ipp32u                          isPredBottom);
#if defined (_IPP_STDCALL_CDECL)
  #undef  _IPP_STDCALL_CDECL
  #define __stdcall __cdecl
#endif
    #define _own_ippiInterpolateQPBicubic_VC1_8u_C1R  ippiInterpolateQPBicubic_VC1_8u_C1R
    #define _own_ippiInterpolateQPBilinear_VC1_8u_C1R ippiInterpolateQPBilinear_VC1_8u_C1R
#endif
    inline void SetInterpolationParamsLuma(IppVCInterpolate_8u* pInterpolationY, Ipp8u* pRefYRow, Ipp32u refYStep, const sCoordinate* mvInt, const sCoordinate* mvQuarter, Ipp32u pos)
    {
        pInterpolationY->pSrc = pRefYRow + (pos << 4) +  mvInt->x + mvInt->y*(Ipp32s)refYStep;
        pInterpolationY->dx    = mvQuarter -> x;
        pInterpolationY->dy    = mvQuarter -> y;
    }
    inline void SetInterpolationParamsLumaBlk(IppVCInterpolate_8u* pInterpolationY, Ipp8u* pRefYRow, Ipp32u refYStep, const sCoordinate* mvInt, const sCoordinate* mvQuarter, Ipp32u pos, Ipp32u blk)
    {
        static const Ipp32u shiftX [4] = {0,8,0,8};
        static const Ipp32u shiftY [4] = {0,0,8,8};

        pInterpolationY->pSrc = pRefYRow + (pos << 4) +  mvInt->x + shiftX[blk] + (mvInt->y + shiftY[blk])*(Ipp32s)refYStep;
        pInterpolationY->dx    = mvQuarter -> x;
        pInterpolationY->dy    = mvQuarter -> y;
    }
    inline void InterpolateChroma_YV12(IppVCInterpolate_8u* pInterpolationU, IppVCInterpolate_8u* pInterpolationV,
                                         Ipp8u* pRefURow, Ipp8u* pRefVRow,Ipp32u refUVStep, 
                                         const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma, Ipp32u pos)
    {
        Ipp32s chromaShift    = (pos << 3) +  mvIntChroma->x + mvIntChroma->y*(Ipp32s)refUVStep;

        pInterpolationU->pSrc = pRefURow + chromaShift;
        pInterpolationV->pSrc = pRefVRow + chromaShift;

        pInterpolationU->dx    =  pInterpolationV->dx = mvQuarterChroma -> x;
        pInterpolationU->dy    =  pInterpolationV->dy = mvQuarterChroma -> y;

        _own_ippiInterpolateQPBilinear_VC1_8u_C1R(pInterpolationU);
        _own_ippiInterpolateQPBilinear_VC1_8u_C1R(pInterpolationV);

    }
    inline void InterpolateChroma_NV12(IppVCInterpolate_8u* pInterpolationUV, IppVCInterpolate_8u* /*pInterpolationV*/,
                                         Ipp8u* pRefUVRow, Ipp8u* /*pRefVRow*/,Ipp32u refUVStep, 
                                         const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma, Ipp32u pos)
    {
        Ipp32s chromaShift    = (pos << 4) +  (mvIntChroma->x <<1) + mvIntChroma->y*(Ipp32s)refUVStep;

        pInterpolationUV->pSrc   = pRefUVRow + chromaShift;
        pInterpolationUV->dx     = mvQuarterChroma -> x;
        pInterpolationUV->dy     = mvQuarterChroma -> y;

        _own_ippiInterpolateQPBilinear_VC1_8u_C1R_NV12(pInterpolationUV); // must be new function
    }
    typedef void (*pInterpolateChroma  )    (IppVCInterpolate_8u* pInterpolationU, IppVCInterpolate_8u* pInterpolationV,
                                               Ipp8u* pRefURow, Ipp8u* pRefVRow,Ipp32u refUVStep, 
                                               const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma, Ipp32u pos);

    
    /* -------------------- MB level padding --------------------------------- */

 IppStatus own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R(const IppVCInterpolateBlock_8u* interpolateInfo,
                                                      const   Ipp8u*                 pLUTTop,
                                                      const   Ipp8u*                 pLUTBottom,
                                                      Ipp32u                          OppositePadding,
                                                      Ipp32u                          fieldPrediction,
                                                      Ipp32u                          RoundControl,
                                                      Ipp32u                          isPredBottom);

 IppStatus own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R_NV12(const IppVCInterpolateBlock_8u* interpolateInfo,
                                                                const   Ipp8u*                 pLUTTop,
                                                                const   Ipp8u*                 pLUTBottom,
                                                                Ipp32u                          OppositePadding,
                                                                Ipp32u                          fieldPrediction,
                                                                Ipp32u                          RoundControl,
                                                                Ipp32u                          isPredBottom);

    inline void SetInterpolationParamsLuma(IppVCInterpolateBlock_8u* pInterpolationY, Ipp32u x, Ipp32u y , const sCoordinate* mvInt, const sCoordinate* mvQuarter)
    {
        pInterpolationY->pointBlockPos.x = (x<<4) + mvInt->x;
        pInterpolationY->pointBlockPos.y = (y<<4) + mvInt->y;
        pInterpolationY->pointVector.x   = mvQuarter -> x;
        pInterpolationY->pointVector.y   = mvQuarter -> y;
    }
    inline void SetInterpolationParamsLumaBlk(IppVCInterpolateBlock_8u* pInterpolationY, Ipp32u x, Ipp32u y , const sCoordinate* mvInt, const sCoordinate* mvQuarter, Ipp32u blk)
    {
        static const Ipp32u shiftX [4] = {0,8,0,8};
        static const Ipp32u shiftY [4] = {0,0,8,8};

        pInterpolationY->pointBlockPos.x = (x<<4) + mvInt->x +  shiftX [blk] ;
        pInterpolationY->pointBlockPos.y = (y<<4) + mvInt->y +  shiftY [blk];
        pInterpolationY->pointVector.x   = mvQuarter -> x;
        pInterpolationY->pointVector.y   = mvQuarter -> y;
    }
    inline void InterpolateChromaPad_YV12 (IppVCInterpolateBlock_8u* pInterpolationU, IppVCInterpolateBlock_8u* pInterpolationV, 
                                           Ipp32u  OppositePadding, Ipp32u  fieldPrediction, Ipp32u  RoundControl, Ipp32u  isPredBottom,
                                           Ipp32u x, Ipp32u y , const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma)
        
    {
        pInterpolationU->pointBlockPos.x = pInterpolationV->pointBlockPos.x = (x<<3) + mvIntChroma->x;
        pInterpolationU->pointBlockPos.y = pInterpolationV->pointBlockPos.y = (y<<3) + mvIntChroma->y;

        pInterpolationU->pointVector.x   = pInterpolationV->pointVector.x   = mvQuarterChroma -> x;
        pInterpolationU->pointVector.y   = pInterpolationV->pointVector.y   = mvQuarterChroma -> y;

        own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R(pInterpolationU,0,0,OppositePadding,fieldPrediction,RoundControl,isPredBottom);
        own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R(pInterpolationV,0,0,OppositePadding,fieldPrediction,RoundControl,isPredBottom);
    }

    inline void InterpolateChromaPad_NV12 (IppVCInterpolateBlock_8u* pInterpolationUV, IppVCInterpolateBlock_8u* /*pInterpolationV*/, 
                                           Ipp32u  OppositePadding, Ipp32u  fieldPrediction, Ipp32u  RoundControl, Ipp32u  isPredBottom,
                                           Ipp32u x, Ipp32u y , const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma)

    {
        pInterpolationUV->pointBlockPos.x =  (x<<4) + (mvIntChroma->x << 1);
        pInterpolationUV->pointBlockPos.y =  (y<<3) + mvIntChroma->y;

        pInterpolationUV->pointVector.x   =  mvQuarterChroma -> x;
        pInterpolationUV->pointVector.y   =  mvQuarterChroma -> y;

        own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R_NV12(pInterpolationUV,0,0,OppositePadding,fieldPrediction,RoundControl,isPredBottom);
    }

    typedef void (*pInterpolateChromaPad  )    (   IppVCInterpolateBlock_8u* pInterpolationU, IppVCInterpolateBlock_8u* pInterpolationV, 
                                                   Ipp32u  OppositePadding, Ipp32u  fieldPrediction, Ipp32u  RoundControl, Ipp32u  isPredBottom,
                                                   Ipp32u x, Ipp32u y , const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma);

//--End------------Interpolation---------------

    inline Ipp16u vc1_abs_16s(Ipp16s pSrc)
    {
        Ipp16u S;
        S = pSrc >> 15;
        S = (pSrc + S)^S;
        return S;
    }
    inline Ipp32u vc1_abs_32s(Ipp32s pSrc)
    {
        Ipp32u S;
        S = pSrc >> 31;
        S = (pSrc + S)^S;
        return S;
    }
    bool MaxAbsDiff(Ipp8u* pSrc1, Ipp32u step1,Ipp8u* pSrc2, Ipp32u step2,IppiSize roiSize,Ipp32u max);
    void InitScaleInfo(sScaleInfo* pInfo, bool bCurSecondField ,bool bBottom,
                        Ipp8u ReferenceFrameDist, Ipp8u MVRangeIndex);
    bool PredictMVField2(sCoordinate* predA,sCoordinate* predB,sCoordinate* predC,
                          sCoordinate* res, sScaleInfo* pInfo, bool bSecondField,
                          sCoordinate* predAEx,sCoordinate* predCEx, bool bBackward, bool bHalf);
    void PredictMVField1(sCoordinate* predA,sCoordinate* predB,sCoordinate* predC, sCoordinate* res);
    void initMVFieldInfo(bool extenedX, bool extendedY, const Ipp32u* pVLCTable ,sMVFieldInfo* MVFieldInfo);

    void  GetMVDirectCurrHalfBackHalf      (Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void  GetMVDirectCurrQuarterBackHalf   (Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void  GetMVDirectCurrHalfBackQuarter   (Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void  GetMVDirectCurrQuarterBackQuarter(Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void InitScaleInfoBackward (sScaleInfo* pInfo, bool bCurSecondField ,bool bBottom,  Ipp8u ReferenceFrameDist, Ipp8u MVRangeIndex);
    inline  bool  IsFrwFieldBottom(eReferenceFieldType     uiReferenceFieldType,
                                   bool bSecondField,      bool bBottom)
    {
        if (bSecondField)
        {
            if (uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)
            {
                return (!bBottom);
            }
            else
            {
                return bBottom;
            }
        }
        else
        {
            if (uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)
            {
                return (!bBottom);
            }
            else
            {
                return bBottom;
            }
        }
    }

IppStatus own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R(const IppVCInterpolateBlock_8u* interpolateInfo,
                                                     const Ipp8u*                   pLUTTop,
                                                     const Ipp8u*                   pLUTBottom,
                                                     Ipp32u                          OppositePadding,
                                                     Ipp32u                          fieldPrediction,
                                                     Ipp32u                          RoundControl,
                                                     Ipp32u                          isPredBottom);

IppStatus own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R(const IppVCInterpolateBlock_8u* interpolateInfo,
                                                      const   Ipp8u*                 pLUTTop,
                                                      const   Ipp8u*                 pLUTBottom,
                                                      Ipp32u                          OppositePadding,
                                                      Ipp32u                          fieldPrediction,
                                                      Ipp32u                          RoundControl,
                                                      Ipp32u                          isPredBottom);



   IppStatus CalculateCorrelationBlock_8u_P1R (    Ipp8u *pSrc, Ipp32u srcStep,
        Ipp8u *pRef, Ipp32u refStep,
        IppiSize roiSize, IppiSize blockSize,
        CorrelationParams* pOutPut);

    bool CalculateIntesityCompensationParams (CorrelationParams sCorrParams, Ipp32u &LUMSCALE, Ipp32u &LUMSHIFT);
    void get_iScale_iShift (Ipp32u LUMSCALE, Ipp32u LUMSHIFT, Ipp32s &iScale, Ipp32s &iShift); 
    void CreateICTable (Ipp32s iScale, Ipp32s iShift, Ipp32s* LUTY, Ipp32s* LUTUV);

    IppStatus   _own_ippiReplicateBorder_8u_C1R  (  Ipp8u * pSrc,  int srcStep, 
                                                    IppiSize srcRoiSize, IppiSize dstRoiSize, 
                                                    int topBorderWidth,  int leftBorderWidth);
    IppStatus   _own_ippiReplicateBorder_16u_C1R  ( Ipp16u * pSrc,  int srcStep, 
                                                    IppiSize srcRoiSize, IppiSize dstRoiSize, 
                                                    int topBorderWidth,  int leftBorderWidth);

    inline void IntensityCompChromaYV12 (Ipp8u *pSrcU,Ipp8u *pSrcV, Ipp32u UVStep,IppiSize size, Ipp32s* pLUTUV, const Ipp32s* pIntensityCompensationTbl)
    {
        ippiLUT_8u_C1IR(pSrcU,UVStep,size,pLUTUV,pIntensityCompensationTbl,257);
        ippiLUT_8u_C1IR(pSrcV,UVStep,size,pLUTUV,pIntensityCompensationTbl,257);
    }
    inline void IntensityCompChromaNV12 (Ipp8u *pSrcUV,Ipp8u */*pSrcV*/, Ipp32u UVStep,IppiSize size, Ipp32s* pLUTUV, const Ipp32s* pIntensityCompensationTbl)
    {
        size.width = size.width << 1;
        ippiLUT_8u_C1IR(pSrcUV,UVStep,size,pLUTUV,pIntensityCompensationTbl,257);
    }
    typedef void (*pIntensityCompChroma)(Ipp8u *pSrcU,Ipp8u *pSrcV, Ipp32u UVStep,IppiSize size, Ipp32s* pLUTUV, const Ipp32s* pIntensityCompensationTbl);




}

#endif
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
