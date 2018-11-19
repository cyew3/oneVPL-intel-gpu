// Copyright (c) 2008-2018 Intel Corporation
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
UMC::Status   SetFieldPlane(uint8_t* pFieldPlane[3], uint8_t* pPlane[3], uint32_t planeStep[3], bool bBottom, int32_t firstRow=0);
UMC::Status   SetFieldPlane(uint8_t** pFieldPlane, uint8_t* pPlane, uint32_t planeStep, bool bBottom, int32_t firstRow=0 );

UMC::Status   SetFieldStep(uint32_t  fieldStep[3], uint32_t planeStep[3]);
UMC::Status   SetFieldStep(int32_t* fieldStep, uint32_t* planeStep);

UMC::Status   Set1RefFrwFieldPlane(uint8_t* pPlane[3],       uint32_t planeStep[3],
                                   uint8_t* pFrwPlane[3],    uint32_t frwStep[3],
                                   uint8_t* pRaisedPlane[3], uint32_t raisedStep[3],
                                   eReferenceFieldType     uiReferenceFieldType,
                                   bool bSecondField,      bool bBottom, int32_t firstRow=0);

UMC::Status   Set1RefFrwFieldPlane(uint8_t** pPlane,       int32_t *planeStep,
                                   uint8_t* pFrwPlane,    uint32_t frwStep,
                                   uint8_t* pRaisedPlane, uint32_t raisedStep,
                                   eReferenceFieldType     uiReferenceFieldType,
                                   bool bSecondField,      bool bBottom, int32_t firstRow=0);

UMC::Status   Set2RefFrwFieldPlane(uint8_t* pPlane[2][3], uint32_t planeStep[2][3],
                                   uint8_t* pFrwPlane[3], uint32_t frwStep[3],
                                   uint8_t* pRaisedPlane[3], uint32_t raisedStep[3],
                                   bool bSecondField, bool bBottom, int32_t firstRow=0);

UMC::Status   Set2RefFrwFieldPlane(uint8_t** pPlane1, int32_t* planeStep1,
                                   uint8_t** pPlane2, int32_t* planeStep2,
                                   uint8_t* pFrwPlane, uint32_t frwStep,
                                   uint8_t* pRaisedPlane, uint32_t raisedStep,
                                   bool bSecondField, bool bBottom, int32_t firstRow=0);

UMC::Status   SetBkwFieldPlane(uint8_t* pPlane[2][3], uint32_t planeStep[2][3],
                               uint8_t* pBkwPlane[3], uint32_t bkwStep[3],
                               bool bBottom, int32_t firstRow=0);

UMC::Status   SetBkwFieldPlane(uint8_t** pPlane1, int32_t* planeStep1,
                               uint8_t** pPlane2, int32_t* planeStep2,
                               uint8_t* pBkwPlane, uint32_t bkwStep,
                               bool bBottom, int32_t firstRow=0);

bool IsFieldPicture(ePType PicType);

typedef struct
{
    uint8_t               uiDecTypeAC;        //[0,2] - it's used to choose decoding table
    uint8_t               uiCBPTab;           //[0,4] - it's used to choose decoding table
    uint8_t               uiMVTab;            //[0,4] - it's used to choose decoding table
    uint8_t               uiDecTypeDCIntra;   //[0,1] - it's used to choose decoding table
}VLCTablesIndex;

typedef struct
{
    eMVModes             uiMVMode;      /*    VC1_ENC_1MV_HALF_BILINEAR
                                        //    VC1_ENC_1MV_QUARTER_BICUBIC
                                        //    VC1_ENC_1MV_HALF_BICUBIC
                                        //    VC1_ENC_MIXED_QUARTER_BICUBIC
                                        */
    uint8_t               uiMVRangeIndex;
    uint8_t               uiDMVRangeIndex;
    uint8_t               nReferenceFrameDist;
    eReferenceFieldType uiReferenceFieldType;
    sFraction           uiBFraction;     // 0xff/0xff - BI frame
    VLCTablesIndex      sVLCTablesIndex;
    bool                bMVMixed;
    bool                bTopFieldFirst;
}InitPictureParams ;



 //-------------FORWARD-TRANSFORM-QUANTIZATION---------------------------
#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
        IppStatus _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(int16_t* pSrcDst, uint32_t srcDstStep);
        IppStatus _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R(int16_t* pSrcDst, uint32_t srcDstStep);
        IppStatus _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R(int16_t* pSrcDst, uint32_t srcDstStep);
        IppStatus _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R(int16_t* pSrcDst, uint32_t srcDstStep);
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
    IppStatus _own_ippiQuantIntraUniform(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant);
    IppStatus _own_ippiQuantIntraNonUniform(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant);

    IppStatus _own_ippiQuantInterUniform(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant, mfxSize roiSize);
    IppStatus _own_ippiQuantInterNonUniform(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant, mfxSize roiSize);
#endif // _VC1_ENC_OWN_FUNCTIONS
#endif //UMC_RESTRICTED_CODE

    IppStatus _own_ippiQuantIntraTrellis(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant, int8_t *pRoundControl,int32_t roundControlStep);
    IppStatus _own_ippiQuantInterTrellis(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant, mfxSize roiSize,int8_t *pRoundControl,int32_t roundControlStep);



#ifndef _VC1_ENC_OWN_FUNCTIONS_

    #define _own_ippiQuantIntraUniform     ippiQuantIntraUniform_VC1_16s_C1IR
    #define _own_ippiQuantIntraNonUniform  ippiQuantIntraNonuniform_VC1_16s_C1IR
    #define _own_ippiQuantInterUniform     ippiQuantInterUniform_VC1_16s_C1IR
    #define _own_ippiQuantInterNonUniform  ippiQuantInterNonuniform_VC1_16s_C1IR

#endif // not _VC1_ENC_OWN_FUNCTIONS_


    inline IppStatus IntraTransformQuantUniform(int16_t* pSrcDst, int32_t srcDstStep,
                                                int32_t DCQuant, int32_t doubleQuant,
                                                int8_t* /*pRoundControl*/, int32_t /*roundControlStep*/)
    {
        IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        Sts = _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
        VC1_ENC_IPP_CHECK(Sts);

        pSrcDst[0] = pSrcDst[0]/(int16_t)DCQuant;
        Sts =  _own_ippiQuantIntraUniform(pSrcDst, srcDstStep, doubleQuant);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }

    inline IppStatus IntraTransformQuantNonUniform(int16_t* pSrcDst, int32_t srcDstStep,
                                                   int32_t DCQuant, int32_t doubleQuant,
                                                   int8_t* /*pRoundControl*/, int32_t /*roundControlStep*/)
    {
        IppStatus Sts = ippStsNoErr;

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        Sts = _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
        VC1_ENC_IPP_CHECK(Sts);

        pSrcDst[0] = pSrcDst[0]/(int16_t)DCQuant;
        Sts =  _own_ippiQuantIntraNonUniform(pSrcDst, srcDstStep, doubleQuant);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }

#define _sign(x) (((x)<0)?-1:!!(x))
#define _abs(x)   ((x<0)? -(x):(x))
    inline IppStatus IntraTransformQuantTrellis(int16_t* pSrcDst, int32_t srcDstStep,
                                                int32_t DCQuant, int32_t doubleQuant,
                                                int8_t* pRoundControl, int32_t roundControlStep)
    {
        IppStatus Sts = ippStsNoErr;

        IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        Sts = _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(pSrcDst,srcDstStep);
        VC1_ENC_IPP_CHECK(Sts);

      //  pSrcDst[0] = (pSrcDst[0])/(int16_t)DCQuant;
        int16_t abs_pixel    = _abs(pSrcDst[0]);
        int16_t round        = (pRoundControl[0])*DCQuant;
        int16_t quant_pixel  = 0;

        quant_pixel = (round < abs_pixel)?(abs_pixel - round)/DCQuant:0;

        pSrcDst[0] = (pSrcDst[0]<0)? -quant_pixel:(int16_t)quant_pixel;
        Sts =  _own_ippiQuantIntraTrellis(pSrcDst, srcDstStep, doubleQuant,pRoundControl,roundControlStep);
        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }
#undef abs
#undef sign

    inline IppStatus InterTransformQuantUniform(int16_t* pSrcDst, int32_t srcDstStep,
                                                int32_t TransformType, int32_t doubleQuant,
                                                int8_t* /*pRoundControl*/, int32_t /*roundControlStep*/)
    {
        IppStatus Sts = ippStsNoErr;
        mfxSize roiSize = {8, 8};

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

                Sts = _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep),
                                                                                             srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterUniform((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep), srcDstStep, doubleQuant, roiSize);

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

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep),
                                                                                            srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterUniform((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep),
                                                                        srcDstStep, doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep)+4,
                                                                                                srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterUniform((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep)+4, srcDstStep, doubleQuant, roiSize);
                break;
            default:
                return ippStsErr;
                break;
        }
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }

    inline IppStatus InterTransformQuantNonUniform(int16_t* pSrcDst, int32_t srcDstStep,
                                                   int32_t TransformType, int32_t doubleQuant,
                                                   int8_t* /*pRoundControl*/, int32_t /*roundControlStep*/)
    {
        IppStatus Sts = ippStsNoErr;
        mfxSize roiSize = {8, 8};

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

                Sts = _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep),
                                                                                            srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep),
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

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep),
                                                                                             srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep), srcDstStep,
                                                                                    doubleQuant, roiSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep)+4,
                                                                                               srcDstStep);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiQuantInterNonUniform((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep)+4, srcDstStep, doubleQuant, roiSize);
                break;
            default:
                return ippStsErr;
                break;
        }
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }
    inline IppStatus InterTransformQuantTrellis(int16_t* pSrcDst, int32_t srcDstStep,
                                                int32_t TransformType, int32_t doubleQuant,
                                                int8_t* pRoundControl, int32_t roundControlStep)
    {
        IppStatus Sts = ippStsNoErr;
        mfxSize roiSize = {8, 8};

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

            Sts = _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep),
                srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts = _own_ippiQuantInterTrellis((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep), srcDstStep, doubleQuant, roiSize,pRoundControl + 4*roundControlStep,roundControlStep);

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

            Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep),
                srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts = _own_ippiQuantInterTrellis((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep),srcDstStep, doubleQuant, roiSize, pRoundControl+4*roundControlStep,roundControlStep);
            VC1_ENC_IPP_CHECK(Sts);

            Sts = _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep)+4,
                srcDstStep);
            VC1_ENC_IPP_CHECK(Sts);
            Sts = _own_ippiQuantInterTrellis((int16_t*)((uint8_t*)pSrcDst+4*srcDstStep)+4, srcDstStep, doubleQuant, roiSize,pRoundControl +4*roundControlStep + 4,roundControlStep);
            break;
        default:
            return ippStsErr;
            break;
        }
        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }
    typedef IppStatus (*IntraTransformQuantFunction)  (int16_t* pSrcDst, int32_t srcDstStep,
                                                       int32_t DCQuant, int32_t doubleQuant,
                                                       int8_t* pRoundControl, int32_t roundControlStep);
    typedef IppStatus (*InterTransformQuantFunction)  (int16_t* pSrcDst, int32_t srcDstStep,
                                                       int32_t TransformType, int32_t doubleQuant,
                                                       int8_t* pRoundControl, int32_t roundControlStep);

 //-------------INVERSE-TRANSFORM-QUANTIZATION---------------------------
#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
    IppStatus _own_ippiQuantInvIntraUniform_VC1_16s_C1R(const int16_t* pSrc, int32_t srcStep,
                                                        int16_t* pDst, int32_t dstStep,
                                                          int32_t doubleQuant, mfxSize* pDstSizeNZ);
    IppStatus _own_ippiQuantInvIntraNonUniform_VC1_16s_C1R(const int16_t* pSrc, int32_t srcStep,
                                                        int16_t* pDst, int32_t dstStep,
                                                          int32_t doubleQuant, mfxSize* pDstSizeNZ);
    IppStatus _own_ippiQuantInvInterUniform_VC1_16s_C1R(const int16_t* pSrc, int32_t srcStep,
                                                     int16_t* pDst, int32_t dstStep,
                                                       int32_t doubleQuant, mfxSize roiSize,
                                                       mfxSize* pDstSizeNZ);
    IppStatus _own_ippiQuantInvInterNonUniform_VC1_16s_C1R(const int16_t* pSrc, int32_t srcStep,
                                                     int16_t* pDst, int32_t dstStep,
                                                       int32_t doubleQuant, mfxSize roiSize,
                                                       mfxSize* pDstSizeNZ);
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
IppStatus _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R        (int16_t* pSrcLeft, int32_t srcLeftStep,
                                                               int16_t* pSrcRight, int32_t srcRightStep,
                                                               uint8_t* pDst, int32_t dstStep,
                                                               uint32_t fieldNeighbourFlag,
                                                               uint32_t edgeDisableFlag);
IppStatus _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R        (int16_t* pSrcUpper, int32_t srcUpperStep,
                                                               int16_t* pSrcBottom, int32_t srcBottomStep,
                                                               uint8_t* pDst, int32_t dstStep,
                                                               uint32_t edgeDisableFlag);
IppStatus _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R      (int16_t* pSrcUpper, int32_t srcUpperStep,
                                                               int16_t* pSrcBottom, int32_t srcBottomStep,
                                                               uint8_t* pDst, int32_t dstStep);
IppStatus _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R      (int16_t* pSrcLeft, int32_t srcLeftStep,
                                                               int16_t* pSrcRight, int32_t srcRightStep,
                                                               uint8_t* pDst, int32_t dstStep);
    #endif // _VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R      ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R
    #define _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R      ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R
    #define _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R    ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R
    #define _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R    ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R
#endif // not _VC1_ENC_OWN_FUNCTIONS_

IppStatus _own_ippiSmoothingChroma_NV12_HorEdge_VC1_16s8u_C2R (int16_t* pSrcUpperU, int32_t srcUpperStepU,
                                                               int16_t* pSrcBottomU, int32_t srcBottomStepU,
                                                               int16_t* pSrcUpperV, int32_t srcUpperStepV,
                                                               int16_t* pSrcBottomV, int32_t srcBottomStepV,
                                                               uint8_t* pDst, int32_t dstStep);

IppStatus _own_ippiSmoothingChroma_NV12_VerEdge_VC1_16s8u_C2R (int16_t* pSrcLeftU,  int32_t srcLeftStepU,
                                                               int16_t* pSrcRightU, int32_t srcRightStepU,
                                                               int16_t* pSrcLeftV,  int32_t srcLeftStepV,
                                                               int16_t* pSrcRightV, int32_t srcRightStepV,
                                                               uint8_t* pDst, int32_t dstStep);

//-------------FORWARD-TRANSFORM-QUANTIZATION---------------------------
#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
        IppStatus _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR(int16_t* pSrcDst, uint32_t srcDstStep, mfxSize DstSize);
        IppStatus _own_ippiTransformBlock4x8Inv_VC1_16s_C1IR(int16_t* pSrcDst, uint32_t srcDstStep, mfxSize DstSize);
        IppStatus _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR(int16_t* pSrcDst, uint32_t srcDstStep, mfxSize DstSize);
        IppStatus _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR(int16_t* pSrcDst, uint32_t srcDstStep, mfxSize DstSize);
    #endif // _VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#ifndef _VC1_ENC_OWN_FUNCTIONS_

    #define _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR      ippiTransform8x8Inv_VC1_16s_C1IR
    #define _own_ippiTransformBlock4x8Inv_VC1_16s_C1IR      ippiTransform4x8Inv_VC1_16s_C1IR
    #define _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR      ippiTransform8x4Inv_VC1_16s_C1IR
    #define _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR      ippiTransform4x4Inv_VC1_16s_C1IR

#endif // not _VC1_ENC_OWN_FUNCTIONS_

    inline IppStatus IntraInvTransformQuantUniform(int16_t* pSrc, int32_t srcStep,
                                                   int16_t* pDst, int32_t dstStep,
                                                   int32_t DCQuant, int32_t doubleQuant)
    {
        IppStatus Sts = ippStsNoErr;
        mfxSize DstSize = {8, 8};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        Sts = _own_ippiQuantInvIntraUniform_VC1_16s_C1R(pSrc, srcStep, pDst, dstStep,
                                                                doubleQuant, &DstSize);
        pDst[0] = pSrc[0]*(int16_t)DCQuant;
        VC1_ENC_IPP_CHECK(Sts);
        Sts = _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR(pDst,dstStep,DstSize);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

         return Sts;
    }

    inline IppStatus IntraInvTransformQuantNonUniform(int16_t* pSrc, int32_t srcStep,
                                                      int16_t* pDst, int32_t dstStep,
                                                      int32_t DCQuant, int32_t doubleQuant)
    {
        IppStatus Sts = ippStsNoErr;
        mfxSize DstSize = {8, 8};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
        Sts = _own_ippiQuantInvIntraNonUniform_VC1_16s_C1R(pSrc, srcStep, pDst, dstStep,
                                                                    doubleQuant, &DstSize);
        pDst[0] = pSrc[0]*(int16_t)DCQuant;
        VC1_ENC_IPP_CHECK(Sts);
        Sts = _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR(pDst,dstStep,DstSize);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

         return Sts;
    }

    inline IppStatus InterInvTransformQuantUniform(int16_t* pSrc, int32_t srcStep,
                                                   int16_t* pDst, int32_t dstStep,
                                                   int32_t doubleQuant, int32_t TransformType)
    {
        IppStatus Sts = ippStsNoErr;
        mfxSize roiSize = {8, 8};
        mfxSize dstSize = {8, 8};

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

                Sts =_own_ippiQuantInvInterUniform_VC1_16s_C1R((int16_t*)((uint8_t*)pSrc + 4*srcStep), srcStep,
                    (int16_t*)((uint8_t*)pDst + 4*dstStep) , dstStep,  doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR((int16_t*)((uint8_t*)pDst + 4*dstStep),dstStep,dstSize);
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

                Sts = _own_ippiQuantInvInterUniform_VC1_16s_C1R((int16_t*)((uint8_t*)pSrc+4*srcStep), srcStep,
                    (int16_t*)((uint8_t*)pDst+4*dstStep), dstStep,  doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR((int16_t*)((uint8_t*)pDst + 4*dstStep),dstStep,
                                                                                            dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiQuantInvInterUniform_VC1_16s_C1R((int16_t*)((uint8_t*)pSrc + 4*srcStep)+4, srcStep,
                    (int16_t*)((uint8_t*)pDst + 4*dstStep)+4, dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR((int16_t*)((uint8_t*)pDst + 4*dstStep)+4,dstStep, dstSize);
                break;
            default:
                return ippStsErr;
                break;
        }
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

        return Sts;
    }

    inline IppStatus InterInvTransformQuantNonUniform(int16_t* pSrc, int32_t srcStep,
                                                      int16_t* pDst, int32_t dstStep,
                                                      int32_t doubleQuant, int32_t TransformType)
    {
        IppStatus Sts = ippStsNoErr;
        mfxSize roiSize = {8, 8};
        mfxSize dstSize = {8, 8};

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

                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R((int16_t*)((uint8_t*)pSrc + 4*srcStep), srcStep,
                    (int16_t*)((uint8_t*)pDst + 4*dstStep) , dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR((int16_t*)((uint8_t*)pDst + 4*dstStep),dstStep,dstSize);
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

                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R((int16_t*)((uint8_t*)pSrc+4*srcStep), srcStep,
                    (int16_t*)((uint8_t*)pDst+4*dstStep), dstStep,  doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR((int16_t*)((uint8_t*)pDst + 4*dstStep),dstStep, dstSize);
                VC1_ENC_IPP_CHECK(Sts);

                Sts = _own_ippiQuantInvInterNonUniform_VC1_16s_C1R((int16_t*)((uint8_t*)pSrc + 4*srcStep)+4, srcStep,
                    (int16_t*)((uint8_t*)pDst + 4*dstStep)+4, dstStep, doubleQuant, roiSize, &dstSize);
                VC1_ENC_IPP_CHECK(Sts);
                Sts = _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR((int16_t*)((uint8_t*)pDst + 4*dstStep)+4,dstStep, dstSize);
                break;
            default:
                return ippStsErr;
                break;
        }

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
        return Sts;
    }

    typedef IppStatus (*IntraInvTransformQuantFunction)  (int16_t* pSrc, int32_t srcStep,
                                                       int16_t* pDst, int32_t dstStep,
                                                       int32_t DCQuant, int32_t doubleQuant);
    typedef IppStatus (*InterInvTransformQuantFunction)  (int16_t* pSrc, int32_t srcStep,
                                                       int16_t* pDst, int32_t dstStep,
                                                       int32_t doubleQuant, int32_t TransformType);

#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
        IppStatus _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag);
        IppStatus _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag);
        IppStatus _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag);
        IppStatus _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag);
    #endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR     ippiFilterDeblockingLuma_VerEdge_VC1_8u_C1IR
    #define _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR   ippiFilterDeblockingChroma_VerEdge_VC1_8u_C1IR
    #define _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR     ippiFilterDeblockingLuma_HorEdge_VC1_8u_C1IR
    #define _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR   ippiFilterDeblockingChroma_HorEdge_VC1_8u_C1IR
#endif // not _VC1_ENC_OWN_FUNCTIONS_


IppStatus _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,
                                                               int32_t EdgeDisabledFlagU, int32_t EdgeDisabledFlagV);
IppStatus _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,
                                                               int32_t EdgeDisabledFlagU, int32_t EdgeDisabledFlagV);

    ///////////////////

    typedef void        (*CalculateChromaFunction)(sCoordinate LumaMV, sCoordinate * pChroma);

    uint8_t               Get_CBPCY(uint32_t MBPatternCur, uint32_t CBPCYTop, uint32_t CBPCYLeft, uint32_t CBPCYULeft);
    //eDirection          GetDCPredDirectionSM(int16_t dcA, int16_t dcB, int16_t dcC);

 //---BEGIN------------------------Copy block, MB functions-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
    IppStatus    _own_Copy8x8_16x16_8u16s  (const uint8_t* pSrc, uint32_t srcStep, int16_t* pDst, uint32_t dstStep, mfxSize roiSize);
    IppStatus    _own_ippiConvert_16s8u_C1R(const int16_t* pSrc,uint32_t srcStep, uint8_t* pDst,  uint32_t dstStep, mfxSize roiSize );

    #endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_Copy8x8_16x16_8u16s   ippiConvert_8u16s_C1R
    #define _own_ippiConvert_16s8u_C1R ippiConvert_16s8u_C1R
#endif
 //---END------------------------Copy block, MB functions-----------------------------------------------


    //void                Diff_1x7_16s(int16_t* pSrcDst, int16_t step, const int16_t* pDst);
    //void                Diff_7x1_16s(int16_t* pSrc, const int16_t* pDst);
    int32_t              SumSqDiff_1x7_16s(int16_t* pSrc, uint32_t step, int16_t* pPred);
    int32_t              SumSqDiff_7x1_16s(int16_t* pSrc, int16_t* pPred);
    //uint32_t              GetNumZeros(int16_t* pSrc, uint32_t srcStep, bool bIntra);
    //uint32_t              GetNumZeros(int16_t* pSrc, uint32_t srcStep);

    uint8_t               GetMode( uint8_t &run,  int16_t &level, const uint8_t *pTableDR, const uint8_t *pTableDL, bool& sign);
    //bool                GetRLCode(int32_t run, int32_t level, IppVCHuffmanSpec_32s *pTable, int32_t &code, int32_t &len);
    //uint8_t               GetLength_16s(int16_t value);
    //uint8_t               Zigzag( int16_t*                pBlock,
    //                        uint32_t                 blockStep,
    //                        bool                   bIntra,
    //                        const uint8_t*           pScanMatrix,
    //                        uint8_t*                 pRuns,
    //                        int16_t*                pLevels);

    void                Diff8x8 (const int16_t* pSrc1, int32_t src1Step,
                            const int16_t* pSrc2, int32_t src2Step,
                            int16_t* pDst, int32_t dstStep,uint32_t pred);

 //---BEGIN------------------------Add constant to block-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
    #ifdef _VC1_ENC_OWN_FUNCTIONS_
    IppStatus  _own_Diff8x8C_16s(int16_t c, int16_t*  pBlock, uint32_t   blockStep, mfxSize roiSize, int scaleFactor);
    IppStatus  _own_Add8x8C_16s (int16_t c, int16_t*  pBlock, uint32_t   blockStep, mfxSize roiSize, int scaleFactor);
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
    IppStatus _own_ippiGetDiff8x8_8u16s_C1(uint8_t*  pBlock,       uint32_t   blockStep,
                            uint8_t*  pPredBlock,   uint32_t   predBlockStep,
                            int16_t* pDst,         uint32_t   dstStep,
                            int16_t* pDstPredictor,int32_t  dstPredictorStep,
                            int32_t  mcType,       int32_t  roundControl);
    IppStatus _own_ippiGetDiff16x16_8u16s_C1(uint8_t*  pBlock,       uint32_t   blockStep,
                            uint8_t*  pPredBlock,   uint32_t   predBlockStep,
                            int16_t* pDst,         uint32_t   dstStep,
                            int16_t* pDstPredictor,int32_t  dstPredictorStep,
                            int32_t  mcType,       int32_t  roundControl);
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

    IppStatus  _own_Add16x16_8u16s(const uint8_t*  pSrcRef,  uint32_t   srcRefStep,
                              const int16_t* pSrcYData,uint32_t   srcYDataStep,
                              uint8_t* pDst, uint32_t   dstStep,
                              int32_t mctype, int32_t roundControl);

    IppStatus  _own_Add8x8_8u16s  (const uint8_t*  pSrcRef,  uint32_t   srcRefStep,
                              const int16_t* pSrcYData,uint32_t   srcYDataStep,
                              uint8_t* pDst, uint32_t   dstStep,
                              int32_t mctype, int32_t roundControl);
    #endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE
#ifndef _VC1_ENC_OWN_FUNCTIONS_
    #define _own_Add16x16_8u16s    ippiMC16x16_8u_C1
    #define _own_Add8x8_8u16s      ippiMC8x8_8u_C1
#endif


//---End---------------------Motion compensation-----------------------------------------------


    void                Copy8x8_16s(int16_t*  pSrc, uint32_t   srcStep,
                            int16_t*  pDst, uint32_t   dstStep);
    //void                Copy16x16_16s( int16_t*  pSrc, uint32_t   srcStep,
    //                        int16_t*  pDst, uint32_t   dstStep);

    int16_t median3(int16_t a, int16_t b, int16_t c);
    int16_t median4(int16_t a, int16_t b, int16_t c, int16_t d);

    void PredictMV(sCoordinate* predA,sCoordinate* predB,sCoordinate* predC, sCoordinate* res);

    inline int16_t VC1abs(int16_t value);
    void ScalePredict(sCoordinate * MV, int32_t x,int32_t y,sCoordinate MVPredMin, sCoordinate MVPredMax);
    //int32_t FullSearch(const uint8_t* pSrc, uint32_t srcStep, const uint8_t* pPred, uint32_t predStep, sCoordinate Min, sCoordinate Max,sCoordinate * MV);
    //int32_t SumBlock16x16(const uint8_t* pSrc, uint32_t srcStep);
    //int32_t SumBlockDiff16x16(const uint8_t* pSrc1, uint32_t srcStep1,const uint8_t* pSrc2, uint32_t srcStep2);
    //int32_t SumBlockDiffBPred16x16(const uint8_t* pSrc, uint32_t srcStep,const uint8_t* pPred1, uint32_t predStep1,
    //                                                            const uint8_t* pPred2, uint32_t predStep2);
    void  GetMVDirectHalf  (int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void  GetMVDirectQuarter(int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void  GetChromaMV   (sCoordinate LumaMV, sCoordinate * pChroma);
    void  GetChromaMVFast(sCoordinate LumaMV, sCoordinate * pChroma);

    void GetIntQuarterMV(sCoordinate MV,sCoordinate *pMVInt, sCoordinate *pMVIntQuarter);
    //void GetQMV(sCoordinate *pMV,sCoordinate MVInt, sCoordinate MVIntQuarter);

    //void GetBlockType(int16_t* pBlock, int32_t step, uint8_t Quant, eTransformType& type);
    //bool GetMBTSType(int16_t** ppBlock, uint32_t* pStep, uint8_t Quant , eTransformType* pTypes);

    //uint8_t GetBlockPattern(int16_t* pSrc, uint32_t srcStep);

    uint8_t HybridPrediction(     sCoordinate       * mvPred,
                                const sCoordinate * MV,
                                const sCoordinate * mvA,
                                const sCoordinate * mvC,
                                uint32_t              th);

//--Begin----------Interpolation---------------
#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
IppStatus _own_ippiInterpolateQPBicubic_VC1_8u_C1R   (const IppVCInterpolate_8u* inter_struct);
IppStatus _own_ippiInterpolateQPBilinear_VC1_8u_C1R  (const IppVCInterpolate_8u* inter_struct);

typedef IppStatus  ( /*__STDCALL*/ *InterpolateFunction)  (const IppVCInterpolate_8u* pParam);
typedef IppStatus  ( /*__STDCALL*/ *InterpolateFunctionPadding) (const IppVCInterpolateBlock_8u* interpolateInfo,    const int32_t*  pLUTTop, const int32_t*  pLUTBottom, uint32_t  OppositePadding, uint32_t  fieldPrediction, uint32_t  RoundControl, uint32_t  isPredBottom);

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
                                                     const uint8_t*                   pLUTTop,
                                                     const uint8_t*                   pLUTBottom,
                                                     uint32_t                          OppositePadding,
                                                     uint32_t                          fieldPrediction,
                                                     uint32_t                          RoundControl,
                                                     uint32_t                          isPredBottom);
#if defined (_IPP_STDCALL_CDECL)
  #undef  _IPP_STDCALL_CDECL
  #define __stdcall __cdecl
#endif
    #define _own_ippiInterpolateQPBicubic_VC1_8u_C1R  ippiInterpolateQPBicubic_VC1_8u_C1R
    #define _own_ippiInterpolateQPBilinear_VC1_8u_C1R ippiInterpolateQPBilinear_VC1_8u_C1R
#endif
    inline void SetInterpolationParamsLuma(IppVCInterpolate_8u* pInterpolationY, uint8_t* pRefYRow, uint32_t refYStep, const sCoordinate* mvInt, const sCoordinate* mvQuarter, uint32_t pos)
    {
        pInterpolationY->pSrc = pRefYRow + (pos << 4) +  mvInt->x + mvInt->y*(int32_t)refYStep;
        pInterpolationY->dx    = mvQuarter -> x;
        pInterpolationY->dy    = mvQuarter -> y;
    }
    inline void SetInterpolationParamsLumaBlk(IppVCInterpolate_8u* pInterpolationY, uint8_t* pRefYRow, uint32_t refYStep, const sCoordinate* mvInt, const sCoordinate* mvQuarter, uint32_t pos, uint32_t blk)
    {
        static const uint32_t shiftX [4] = {0,8,0,8};
        static const uint32_t shiftY [4] = {0,0,8,8};

        pInterpolationY->pSrc = pRefYRow + (pos << 4) +  mvInt->x + shiftX[blk] + (mvInt->y + shiftY[blk])*(int32_t)refYStep;
        pInterpolationY->dx    = mvQuarter -> x;
        pInterpolationY->dy    = mvQuarter -> y;
    }
    inline void InterpolateChroma_YV12(IppVCInterpolate_8u* pInterpolationU, IppVCInterpolate_8u* pInterpolationV,
                                         uint8_t* pRefURow, uint8_t* pRefVRow,uint32_t refUVStep, 
                                         const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma, uint32_t pos)
    {
        int32_t chromaShift    = (pos << 3) +  mvIntChroma->x + mvIntChroma->y*(int32_t)refUVStep;

        pInterpolationU->pSrc = pRefURow + chromaShift;
        pInterpolationV->pSrc = pRefVRow + chromaShift;

        pInterpolationU->dx    =  pInterpolationV->dx = mvQuarterChroma -> x;
        pInterpolationU->dy    =  pInterpolationV->dy = mvQuarterChroma -> y;

        _own_ippiInterpolateQPBilinear_VC1_8u_C1R(pInterpolationU);
        _own_ippiInterpolateQPBilinear_VC1_8u_C1R(pInterpolationV);

    }
    inline void InterpolateChroma_NV12(IppVCInterpolate_8u* pInterpolationUV, IppVCInterpolate_8u* /*pInterpolationV*/,
                                         uint8_t* pRefUVRow, uint8_t* /*pRefVRow*/,uint32_t refUVStep, 
                                         const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma, uint32_t pos)
    {
        int32_t chromaShift    = (pos << 4) +  (mvIntChroma->x <<1) + mvIntChroma->y*(int32_t)refUVStep;

        pInterpolationUV->pSrc   = pRefUVRow + chromaShift;
        pInterpolationUV->dx     = mvQuarterChroma -> x;
        pInterpolationUV->dy     = mvQuarterChroma -> y;

        _own_ippiInterpolateQPBilinear_VC1_8u_C1R_NV12(pInterpolationUV); // must be new function
    }
    typedef void (*pInterpolateChroma  )    (IppVCInterpolate_8u* pInterpolationU, IppVCInterpolate_8u* pInterpolationV,
                                               uint8_t* pRefURow, uint8_t* pRefVRow,uint32_t refUVStep, 
                                               const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma, uint32_t pos);

    
    /* -------------------- MB level padding --------------------------------- */

 IppStatus own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R(const IppVCInterpolateBlock_8u* interpolateInfo,
                                                      const   uint8_t*                 pLUTTop,
                                                      const   uint8_t*                 pLUTBottom,
                                                      uint32_t                          OppositePadding,
                                                      uint32_t                          fieldPrediction,
                                                      uint32_t                          RoundControl,
                                                      uint32_t                          isPredBottom);

 IppStatus own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R_NV12(const IppVCInterpolateBlock_8u* interpolateInfo,
                                                                const   uint8_t*                 pLUTTop,
                                                                const   uint8_t*                 pLUTBottom,
                                                                uint32_t                          OppositePadding,
                                                                uint32_t                          fieldPrediction,
                                                                uint32_t                          RoundControl,
                                                                uint32_t                          isPredBottom);

    inline void SetInterpolationParamsLuma(IppVCInterpolateBlock_8u* pInterpolationY, uint32_t x, uint32_t y , const sCoordinate* mvInt, const sCoordinate* mvQuarter)
    {
        pInterpolationY->pointBlockPos.x = (x<<4) + mvInt->x;
        pInterpolationY->pointBlockPos.y = (y<<4) + mvInt->y;
        pInterpolationY->pointVector.x   = mvQuarter -> x;
        pInterpolationY->pointVector.y   = mvQuarter -> y;
    }
    inline void SetInterpolationParamsLumaBlk(IppVCInterpolateBlock_8u* pInterpolationY, uint32_t x, uint32_t y , const sCoordinate* mvInt, const sCoordinate* mvQuarter, uint32_t blk)
    {
        static const uint32_t shiftX [4] = {0,8,0,8};
        static const uint32_t shiftY [4] = {0,0,8,8};

        pInterpolationY->pointBlockPos.x = (x<<4) + mvInt->x +  shiftX [blk] ;
        pInterpolationY->pointBlockPos.y = (y<<4) + mvInt->y +  shiftY [blk];
        pInterpolationY->pointVector.x   = mvQuarter -> x;
        pInterpolationY->pointVector.y   = mvQuarter -> y;
    }
    inline void InterpolateChromaPad_YV12 (IppVCInterpolateBlock_8u* pInterpolationU, IppVCInterpolateBlock_8u* pInterpolationV, 
                                           uint32_t  OppositePadding, uint32_t  fieldPrediction, uint32_t  RoundControl, uint32_t  isPredBottom,
                                           uint32_t x, uint32_t y , const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma)
        
    {
        pInterpolationU->pointBlockPos.x = pInterpolationV->pointBlockPos.x = (x<<3) + mvIntChroma->x;
        pInterpolationU->pointBlockPos.y = pInterpolationV->pointBlockPos.y = (y<<3) + mvIntChroma->y;

        pInterpolationU->pointVector.x   = pInterpolationV->pointVector.x   = mvQuarterChroma -> x;
        pInterpolationU->pointVector.y   = pInterpolationV->pointVector.y   = mvQuarterChroma -> y;

        own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R(pInterpolationU,0,0,OppositePadding,fieldPrediction,RoundControl,isPredBottom);
        own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R(pInterpolationV,0,0,OppositePadding,fieldPrediction,RoundControl,isPredBottom);
    }

    inline void InterpolateChromaPad_NV12 (IppVCInterpolateBlock_8u* pInterpolationUV, IppVCInterpolateBlock_8u* /*pInterpolationV*/, 
                                           uint32_t  OppositePadding, uint32_t  fieldPrediction, uint32_t  RoundControl, uint32_t  isPredBottom,
                                           uint32_t x, uint32_t y , const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma)

    {
        pInterpolationUV->pointBlockPos.x =  (x<<4) + (mvIntChroma->x << 1);
        pInterpolationUV->pointBlockPos.y =  (y<<3) + mvIntChroma->y;

        pInterpolationUV->pointVector.x   =  mvQuarterChroma -> x;
        pInterpolationUV->pointVector.y   =  mvQuarterChroma -> y;

        own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R_NV12(pInterpolationUV,0,0,OppositePadding,fieldPrediction,RoundControl,isPredBottom);
    }

    typedef void (*pInterpolateChromaPad  )    (   IppVCInterpolateBlock_8u* pInterpolationU, IppVCInterpolateBlock_8u* pInterpolationV, 
                                                   uint32_t  OppositePadding, uint32_t  fieldPrediction, uint32_t  RoundControl, uint32_t  isPredBottom,
                                                   uint32_t x, uint32_t y , const sCoordinate* mvIntChroma, const sCoordinate* mvQuarterChroma);

//--End------------Interpolation---------------

    inline uint16_t vc1_abs_16s(int16_t pSrc)
    {
        uint16_t S;
        S = pSrc >> 15;
        S = (pSrc + S)^S;
        return S;
    }
    inline uint32_t vc1_abs_32s(int32_t pSrc)
    {
        uint32_t S;
        S = pSrc >> 31;
        S = (pSrc + S)^S;
        return S;
    }
    bool MaxAbsDiff(uint8_t* pSrc1, uint32_t step1,uint8_t* pSrc2, uint32_t step2,mfxSize roiSize,uint32_t max);
    void InitScaleInfo(sScaleInfo* pInfo, bool bCurSecondField ,bool bBottom,
                        uint8_t ReferenceFrameDist, uint8_t MVRangeIndex);
    bool PredictMVField2(sCoordinate* predA,sCoordinate* predB,sCoordinate* predC,
                          sCoordinate* res, sScaleInfo* pInfo, bool bSecondField,
                          sCoordinate* predAEx,sCoordinate* predCEx, bool bBackward, bool bHalf);
    void PredictMVField1(sCoordinate* predA,sCoordinate* predB,sCoordinate* predC, sCoordinate* res);
    void initMVFieldInfo(bool extenedX, bool extendedY, const uint32_t* pVLCTable ,sMVFieldInfo* MVFieldInfo);

    void  GetMVDirectCurrHalfBackHalf      (int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void  GetMVDirectCurrQuarterBackHalf   (int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void  GetMVDirectCurrHalfBackQuarter   (int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void  GetMVDirectCurrQuarterBackQuarter(int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB);
    void InitScaleInfoBackward (sScaleInfo* pInfo, bool bCurSecondField ,bool bBottom,  uint8_t ReferenceFrameDist, uint8_t MVRangeIndex);
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
                                                     const uint8_t*                   pLUTTop,
                                                     const uint8_t*                   pLUTBottom,
                                                     uint32_t                          OppositePadding,
                                                     uint32_t                          fieldPrediction,
                                                     uint32_t                          RoundControl,
                                                     uint32_t                          isPredBottom);

IppStatus own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R(const IppVCInterpolateBlock_8u* interpolateInfo,
                                                      const   uint8_t*                 pLUTTop,
                                                      const   uint8_t*                 pLUTBottom,
                                                      uint32_t                          OppositePadding,
                                                      uint32_t                          fieldPrediction,
                                                      uint32_t                          RoundControl,
                                                      uint32_t                          isPredBottom);



   IppStatus CalculateCorrelationBlock_8u_P1R (    uint8_t *pSrc, uint32_t srcStep,
        uint8_t *pRef, uint32_t refStep,
        mfxSize roiSize, mfxSize blockSize,
        CorrelationParams* pOutPut);

    bool CalculateIntesityCompensationParams (CorrelationParams sCorrParams, uint32_t &LUMSCALE, uint32_t &LUMSHIFT);
    void get_iScale_iShift (uint32_t LUMSCALE, uint32_t LUMSHIFT, int32_t &iScale, int32_t &iShift); 
    void CreateICTable (int32_t iScale, int32_t iShift, int32_t* LUTY, int32_t* LUTUV);

    IppStatus   _own_ippiReplicateBorder_8u_C1R  (  uint8_t * pSrc,  int srcStep, 
                                                    mfxSize srcRoiSize, mfxSize dstRoiSize, 
                                                    int topBorderWidth,  int leftBorderWidth);
    IppStatus   _own_ippiReplicateBorder_16u_C1R  ( uint16_t * pSrc,  int srcStep, 
                                                    mfxSize srcRoiSize, mfxSize dstRoiSize, 
                                                    int topBorderWidth,  int leftBorderWidth);

    inline void IntensityCompChromaYV12 (uint8_t *pSrcU,uint8_t *pSrcV, uint32_t UVStep,mfxSize size, int32_t* pLUTUV, const int32_t* pIntensityCompensationTbl)
    {
        ippiLUT_8u_C1IR(pSrcU,UVStep,size,pLUTUV,pIntensityCompensationTbl,257);
        ippiLUT_8u_C1IR(pSrcV,UVStep,size,pLUTUV,pIntensityCompensationTbl,257);
    }
    inline void IntensityCompChromaNV12 (uint8_t *pSrcUV,uint8_t */*pSrcV*/, uint32_t UVStep,mfxSize size, int32_t* pLUTUV, const int32_t* pIntensityCompensationTbl)
    {
        size.width = size.width << 1;
        ippiLUT_8u_C1IR(pSrcUV,UVStep,size,pLUTUV,pIntensityCompensationTbl,257);
    }
    typedef void (*pIntensityCompChroma)(uint8_t *pSrcU,uint8_t *pSrcV, uint32_t UVStep,mfxSize size, int32_t* pLUTUV, const int32_t* pIntensityCompensationTbl);




}

#endif
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
