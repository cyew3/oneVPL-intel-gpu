// Copyright (c) 2008-2019 Intel Corporation
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

#include <math.h>

#include "ippvc.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_common_defs.h"
#include "stdio.h"
#include "assert.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_tables.h"
#include "umc_vc1_enc_debug.h"


namespace UMC_VC1_ENCODER
{

bool    IsBottomField(bool bTopFieldFirst, bool bSecondField)
{
    return ((bTopFieldFirst && bSecondField) || ((!bTopFieldFirst)&&(!bSecondField)));
}

UMC::Status SetFieldPlane(uint8_t* pFieldPlane[3],  uint8_t*  pPlane[3], uint32_t planeStep[3], bool bBottom, int32_t nRow)
{
     if(pPlane[0])
        pFieldPlane[0] = pPlane[0]+ planeStep[0]*(bBottom + (nRow<<5)) ;
     else
         return UMC::UMC_ERR_NULL_PTR;

     if(pPlane[1])
        pFieldPlane[1] = pPlane[1]+ planeStep[1]*(bBottom + (nRow<<4));
     else
         return UMC::UMC_ERR_NULL_PTR;

     if(pPlane[2])
        pFieldPlane[2] = pPlane[2]+ planeStep[2]*(bBottom + (nRow<<4)) ;
     else
         return UMC::UMC_ERR_NULL_PTR;

     return UMC::UMC_OK;
}

UMC::Status SetFieldPlane(uint8_t** pFieldPlane,  uint8_t*  pPlane, uint32_t planeStep, bool bBottom, int32_t nRow)
{
     if(pPlane)
      *pFieldPlane = pPlane + planeStep *(bBottom + (nRow<<4));
   else
         return UMC::UMC_ERR_NULL_PTR;
     return UMC::UMC_OK;
}

UMC::Status   SetFieldStep(uint32_t fieldStep[3], uint32_t planeStep[3])
{
    fieldStep[0] = planeStep[0] <<1;
    fieldStep[1] = planeStep[1] <<1;
    fieldStep[2] = planeStep[2] <<1;

    return UMC::UMC_OK;
}

UMC::Status   SetFieldStep(int32_t* fieldStep, uint32_t planeStep)
{
    fieldStep[0] = planeStep <<1;
    return UMC::UMC_OK;
}

UMC::Status   Set1RefFrwFieldPlane(uint8_t* pPlane[3],       uint32_t planeStep[3],
                                   uint8_t* pFrwPlane[3],    uint32_t frwStep[3],
                                   uint8_t* pRaisedPlane[3], uint32_t raisedStep[3],
                                   eReferenceFieldType     uiReferenceFieldType,
                                   bool bSecondField,      bool bBottom, int32_t nRow)
{
    UMC::Status err = UMC::UMC_OK;

    if (bSecondField)
    {
        if (uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)
        {
            err = SetFieldPlane(pPlane,  pRaisedPlane, raisedStep, (!bBottom),nRow);
            VC1_ENC_CHECK(err);
            SetFieldStep(planeStep, raisedStep);
        }
        else
        {
            err = SetFieldPlane(pPlane,  pFrwPlane, frwStep, (bBottom),nRow);
            VC1_ENC_CHECK(err);
            SetFieldStep(planeStep, frwStep);
        }
    }
    else
    {
        if (uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)
        {
            err = SetFieldPlane(pPlane,  pFrwPlane, frwStep, (!bBottom), nRow);
            VC1_ENC_CHECK(err);
            SetFieldStep(planeStep, frwStep);
        }
        else
        {
            err = SetFieldPlane(pPlane,  pFrwPlane, frwStep, (bBottom), nRow);
            VC1_ENC_CHECK(err);
            SetFieldStep(planeStep, frwStep);
        }
    }

    return err;
}


UMC::Status   Set1RefFrwFieldPlane(uint8_t** pPlane,       int32_t* planeStep,
                                   uint8_t* pFrwPlane,    uint32_t frwStep,
                                   uint8_t* pRaisedPlane, uint32_t raisedStep,
                                   eReferenceFieldType     uiReferenceFieldType,
                                   bool bSecondField,      bool bBottom, int32_t nRow)
{
    UMC::Status err = UMC::UMC_OK;

    if (bSecondField)
    {
        if (uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)
        {
            err = SetFieldPlane(pPlane,  pRaisedPlane, raisedStep, (!bBottom),nRow);
            VC1_ENC_CHECK(err);
            SetFieldStep(planeStep, raisedStep);
        }
        else
        {
            err = SetFieldPlane(pPlane,  pFrwPlane, frwStep, (bBottom),nRow);
            VC1_ENC_CHECK(err);
            SetFieldStep(planeStep, frwStep);
        }
    }
    else
    {
        if (uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)
        {
            err = SetFieldPlane(pPlane,  pFrwPlane, frwStep, (!bBottom), nRow);
            VC1_ENC_CHECK(err);
            SetFieldStep(planeStep, frwStep);
        }
        else
        {
            err = SetFieldPlane(pPlane,  pFrwPlane, frwStep, (bBottom), nRow);
            VC1_ENC_CHECK(err);
            SetFieldStep(planeStep, frwStep);
        }
    }

    return err;
}

UMC::Status   Set2RefFrwFieldPlane(uint8_t* pPlane[2][3], uint32_t planeStep[2][3],
                                   uint8_t* pFrwPlane[3], uint32_t frwStep[3],
                                   uint8_t* pRaisedPlane[3], uint32_t raisedStep[3],
                                   bool bSecondField, bool bBottom, int32_t nRow)
{
    UMC::Status err = UMC::UMC_OK;

    if (bSecondField)
    {
        err = SetFieldPlane(pPlane[0],  pRaisedPlane, raisedStep, (!bBottom), nRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(planeStep[0], raisedStep);

        err = SetFieldPlane(pPlane[1],  pFrwPlane, frwStep, bBottom, nRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(planeStep[1], frwStep);
    }
    else
    {
        err = SetFieldPlane(pPlane[0],  pFrwPlane, frwStep, (!bBottom), nRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(planeStep[0], frwStep);

        err = SetFieldPlane(pPlane[1],  pFrwPlane, frwStep, bBottom, nRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(planeStep[1], frwStep);
    }
    return err;
}

UMC::Status   Set2RefFrwFieldPlane(uint8_t** pPlane1, int32_t* planeStep1,
                                   uint8_t** pPlane2, int32_t* planeStep2,
                                   uint8_t* pFrwPlane, uint32_t frwStep,
                                   uint8_t* pRaisedPlane, uint32_t raisedStep,
                                   bool bSecondField, bool bBottom, int32_t nRow)
{
    UMC::Status err = UMC::UMC_OK;

    if (bSecondField)
    {
        err = SetFieldPlane(pPlane1,  pRaisedPlane, raisedStep, (!bBottom), nRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(planeStep1, raisedStep);

        err = SetFieldPlane(pPlane2,  pFrwPlane, frwStep, bBottom, nRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(planeStep2, frwStep);
    }
    else
    {
        err = SetFieldPlane(pPlane1,  pFrwPlane, frwStep, (!bBottom), nRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(planeStep1, frwStep);

        err = SetFieldPlane(pPlane2,  pFrwPlane, frwStep, bBottom, nRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(planeStep2, frwStep);
    }
    return err;
}

UMC::Status   SetBkwFieldPlane(uint8_t* pPlane[2][3], uint32_t planeStep[2][3],
                               uint8_t* pBkwPlane[3], uint32_t bkwStep[3],
                               bool bBottom, int32_t nRow)
{
    UMC::Status err = UMC::UMC_OK;

    err = SetFieldPlane(pPlane[0],  pBkwPlane, bkwStep, (!bBottom), nRow);
    VC1_ENC_CHECK(err);
    SetFieldStep(planeStep[0], bkwStep);

    err = SetFieldPlane(pPlane[1],  pBkwPlane, bkwStep, (bBottom), nRow);
    VC1_ENC_CHECK(err);
    SetFieldStep(planeStep[1], bkwStep);

    return err;
}

UMC::Status   SetBkwFieldPlane(uint8_t** pPlane1, int32_t* planeStep1,
                               uint8_t** pPlane2, int32_t* planeStep2,
                               uint8_t* pBkwPlane, uint32_t bkwStep,
                               bool bBottom, int32_t nRow)
{
    UMC::Status err = UMC::UMC_OK;

    err = SetFieldPlane(pPlane1,  pBkwPlane, bkwStep, (!bBottom), nRow);
    VC1_ENC_CHECK(err);
    SetFieldStep(planeStep1, bkwStep);

    err = SetFieldPlane(pPlane2,  pBkwPlane, bkwStep, (bBottom), nRow);
    VC1_ENC_CHECK(err);
    SetFieldStep(planeStep2, bkwStep);

    return err;
}
#ifdef VC1_ME_MB_STATICTICS
UMC::Status Enc_IFrameUpdate(UMC::MeBase* pME, UMC::MeParams* MEParams)
{
    UMC::Status   err = UMC::UMC_OK;
    MEParams->pSrc->type = UMC::ME_FrmIntra;
    pME->ProcessFeedback(MEParams);
    return err;
}
#else
UMC::Status Enc_IFrameUpdate(UMC::MeBase* /*pME*/, UMC::MeParams* /*MEParams*/)
{
    return UMC::UMC_OK;
}
#endif

#ifdef VC1_ME_MB_STATICTICS
UMC::Status Enc_I_FieldUpdate(UMC::MeBase* pME, UMC::MeParams* MEParams)
{
    UMC::Status   err = UMC::UMC_OK;
    pME->ProcessFeedback(MEParams);
    return err;
}
#else
UMC::Status Enc_I_FieldUpdate(UMC::MeBase* /*pME*/, UMC::MeParams* /*MEParams*/)
{
    return UMC::UMC_OK;
}
#endif


UMC::Status Enc_Frame(UMC::MeBase* pME, UMC::MeParams* MEParams)
{
    UMC::Status   err = UMC::UMC_OK;

    if(!(pME->EstimateFrame(MEParams)))
        return UMC::UMC_ERR_INVALID_PARAMS;

    return err;
}

#ifdef VC1_ME_MB_STATICTICS
UMC::Status Enc_PFrameUpdate(UMC::MeBase* pME, UMC::MeParams* MEParams)
{
    UMC::Status   err = UMC::UMC_OK;

    MEParams->pSrc->type = UMC::ME_FrmFrw;
    pME->ProcessFeedback(MEParams);
    return err;
}
#else
UMC::Status Enc_PFrameUpdate(UMC::MeBase* /*pME*/, UMC::MeParams* /*MEParams*/)
{
    return UMC::UMC_OK;
}
#endif

#ifdef VC1_ME_MB_STATICTICS
UMC::Status Enc_PFrameUpdateMixed(UMC::MeBase* pME, UMC::MeParams* MEParams)
{
    UMC::Status                 err = UMC::UMC_OK;
    MEParams->pSrc->type = UMC::ME_FrmFrw;
    pME->ProcessFeedback(MEParams);

    return err;
}
#else
UMC::Status Enc_PFrameUpdateMixed(UMC::MeBase* /*pME*/, UMC::MeParams* /*MEParams*/)
{
    return UMC::UMC_OK;
}
#endif

#ifdef VC1_ME_MB_STATICTICS
UMC::Status     Enc_P_FieldUpdate(UMC::MeBase* pME, UMC::MeParams* MEParams)
{
    UMC::Status                 err = UMC::UMC_OK;
    pME->ProcessFeedback(MEParams);
    return err;
}
#else
UMC::Status     Enc_P_FieldUpdate(UMC::MeBase* /*pME*/, UMC::MeParams* /*MEParams*/)
{
    return  UMC::UMC_OK;
}
#endif

#ifdef VC1_ME_MB_STATICTICS
UMC::Status Enc_BFrameUpdate(UMC::MeBase* pME, UMC::MeParams* MEParams)
{
    UMC::Status                 err = UMC::UMC_OK;
    MEParams->pSrc->type = UMC::ME_FrmBidir;
    //pME->ProcessFeedback(MEParams);
    return err;
}
#else
UMC::Status     Enc_BFrameUpdate(UMC::MeBase* /*pME*/, UMC::MeParams* /*MEParams*/)
{
    return  UMC::UMC_OK;
}
#endif

#ifdef VC1_ME_MB_STATICTICS
UMC::Status     Enc_B_FieldUpdate(UMC::MeBase* pME, UMC::MeParams* MEParams)
{
    UMC::Status                 err = UMC::UMC_OK;
    pME->ProcessFeedback(MEParams);
    return err;
}
#else
UMC::Status     Enc_B_FieldUpdate(UMC::MeBase* /*pME*/, UMC::MeParams* /*MEParams*/)
{
    return  UMC::UMC_OK;
}
#endif

bool IsFieldPicture(ePType PicType)
{
    return ((PicType == VC1_ENC_I_I_FIELD) || (PicType == VC1_ENC_P_I_FIELD)
        || (PicType == VC1_ENC_I_P_FIELD) || (PicType == VC1_ENC_P_P_FIELD)
        || (PicType == VC1_ENC_B_B_FIELD) || (PicType == VC1_ENC_B_BI_FIELD)
        || (PicType == VC1_ENC_BI_B_FIELD) || (PicType == VC1_ENC_BI_BI_FIELD));
}

static const int16_t TableFwdTransform8x8[64] =
{
    21845,   21845,  21845,  21845,  21845,  21845,  21845,  21845,
    29026,   27212,  16327,  7257,  -7257,  -16327, -27212, -29026,
    28728,   10773, -10773, -28728, -28728, -10773,  10773,  28728,
    27212,  -7257,  -29026, -16327,  16327,  29026,  7257,  -27212,
    21845,  -21845, -21845,  21845,  21845, -21845, -21845,  21845,
    16327,  -29026,  7257,   27212, -27212, -7257,   29026, -16327,
    10773,  -28728,  28728, -10773, -10773,  28728, -28728,  10773,
    7257,   -16327,  27212, -29026,  29026, -27212,  16327, -7257
};

static const int16_t TableFwdTransform4x4[16] =
{
    15420,  15420,      15420,      15420,
    19751,  8978,       -8978,      -19751,
    15420,  -15420,     -15420,     15420,
    8978 ,  -19751,     19751 ,     -8978

};

#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
#define _A(x) (((x)<0)?-(x):(x))
IppStatus _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    uint8_t* pRPixel;
    int16_t p1, p2, p3, p4, p5, p6, p7, p8;
    int16_t a0, a1, a2, a3;
    int16_t clip, d;
    int32_t i;
    int32_t count;

    for (count = 0; count < 4;count++)
    {
        pRPixel = pSrcDst + 2 * srcdstStep + 4*count*srcdstStep;

        if (!(EdgeDisabledFlag & (1 << count) ))
        {

            p4 = pRPixel[-1];
            p5 = pRPixel[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixel[-4];
            p2 = pRPixel[-3];
            p3 = pRPixel[-2];
            p6 = pRPixel[1];
            p7 = pRPixel[2];
            p8 = pRPixel[3];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (_A(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = MFX_MIN(_A(a1), _A(a2));
                if (a3 < _A(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (_A(d) > _A(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixel[-1] = (uint8_t)(p4 - d);
                    pRPixel[0] = (uint8_t)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;
            pRPixel -= 2 * srcdstStep;
            for (i=4; i--; pRPixel += srcdstStep)
            {
                if (i==1)
                    continue;

                p4 = pRPixel[-1];
                p5 = pRPixel[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixel[-4];
                p2 = pRPixel[-3];
                p3 = pRPixel[-2];
                p6 = pRPixel[1];
                p7 = pRPixel[2];
                p8 = pRPixel[3];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (_A(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = MFX_MIN(_A(a1), _A(a2));
                    if (a3 < _A(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (_A(d) > _A(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixel[-1] = (uint8_t)(p4 - d);
                        pRPixel[0] = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

IppStatus _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag)
{
    static int32_t EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    IppStatus ret = ippStsNoErr;
    uint8_t* pRPixel;
    int16_t p1, p2, p3, p4, p5, p6, p7, p8;
    int16_t a0, a1, a2, a3;
    int16_t clip, d;
    int32_t i;
    int32_t count;

    for (count = 0; count < 2;count++)
    {

        pRPixel = pSrcDst + 2 * srcdstStep + 4*count*srcdstStep;

        if (!(EdgeDisabledFlag & (EdgeTable[count]) ))
        {

            p4 = pRPixel[-1];
            p5 = pRPixel[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixel[-4];
            p2 = pRPixel[-3];
            p3 = pRPixel[-2];
            p6 = pRPixel[1];
            p7 = pRPixel[2];
            p8 = pRPixel[3];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (_A(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = MFX_MIN(_A(a1), _A(a2));
                if (a3 < _A(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (_A(d) > _A(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixel[-1] = (uint8_t)(p4 - d);
                    pRPixel[0] = (uint8_t)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;
            pRPixel -= 2 * srcdstStep;
            for (i=4; i--; pRPixel += srcdstStep)
            {
                if (i==1)
                    continue;

                p4 = pRPixel[-1];
                p5 = pRPixel[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixel[-4];
                p2 = pRPixel[-3];
                p3 = pRPixel[-2];
                p6 = pRPixel[1];
                p7 = pRPixel[2];
                p8 = pRPixel[3];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (_A(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = MFX_MIN(_A(a1), _A(a2));
                    if (a3 < _A(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (_A(d) > _A(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixel[-1] = (uint8_t)(p4 - d);
                        pRPixel[0] = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

IppStatus _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    uint8_t* pRPixel;
    int16_t p1, p2, p3, p4, p5, p6, p7, p8;
    int16_t a0, a1, a2, a3;
    int16_t clip, d;
    int32_t i;
    int32_t count;

    for (count = 0; count < 4;count++)
    {

        pRPixel = pSrcDst + 2 + 4*count;

        if (!(EdgeDisabledFlag & (1 << count) ))
        {

            p4 = pRPixel[-1*srcdstStep];
            p5 = pRPixel[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixel[-4*srcdstStep];
            p2 = pRPixel[-3*srcdstStep];
            p3 = pRPixel[-2*srcdstStep];
            p6 = pRPixel[1*srcdstStep];
            p7 = pRPixel[2*srcdstStep];
            p8 = pRPixel[3*srcdstStep];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (_A(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = MFX_MIN(_A(a1), _A(a2));
                if (a3 < _A(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (_A(d) > _A(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixel[-1*srcdstStep] = (uint8_t)(p4 - d);
                    pRPixel[0] = (uint8_t)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;
            pRPixel -= 2;
            for (i=4; i--; pRPixel += 1)
            {
                if (i==1)
                    continue;

                p4 = pRPixel[-1*srcdstStep];
                p5 = pRPixel[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixel[-4*srcdstStep];
                p2 = pRPixel[-3*srcdstStep];
                p3 = pRPixel[-2*srcdstStep];
                p6 = pRPixel[1*srcdstStep];
                p7 = pRPixel[2*srcdstStep];
                p8 = pRPixel[3*srcdstStep];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (_A(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = MFX_MIN(_A(a1), _A(a2));
                    if (a3 < _A(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (_A(d) > _A(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixel[-1*srcdstStep] = (uint8_t)(p4 - d);
                        pRPixel[0] = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}


IppStatus _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,int32_t EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    static int32_t EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    uint8_t* pRPixel;
    int16_t p1, p2, p3, p4, p5, p6, p7, p8;
    int16_t a0, a1, a2, a3;
    int16_t clip, d;
    int32_t i;
    int32_t count;

    for (count = 0; count < 2;count++)
    {
        pRPixel = pSrcDst + 2 + 4*count;

        if (!(EdgeDisabledFlag & (EdgeTable[count]) ))
        {

            p4 = pRPixel[-1*srcdstStep];
            p5 = pRPixel[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixel[-4*srcdstStep];
            p2 = pRPixel[-3*srcdstStep];
            p3 = pRPixel[-2*srcdstStep];
            p6 = pRPixel[1*srcdstStep];
            p7 = pRPixel[2*srcdstStep];
            p8 = pRPixel[3*srcdstStep];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (_A(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = MFX_MIN(_A(a1), _A(a2));
                if (a3 < _A(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (_A(d) > _A(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixel[-1*srcdstStep] = (uint8_t)(p4 - d);
                    pRPixel[0] = (uint8_t)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;
            pRPixel -= 2;
            for (i=4; i--; pRPixel += 1)
            {
                if (i==1)
                    continue;

                p4 = pRPixel[-1*srcdstStep];
                p5 = pRPixel[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixel[-4*srcdstStep];
                p2 = pRPixel[-3*srcdstStep];
                p3 = pRPixel[-2*srcdstStep];
                p6 = pRPixel[1*srcdstStep];
                p7 = pRPixel[2*srcdstStep];
                p8 = pRPixel[3*srcdstStep];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (_A(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = MFX_MIN(_A(a1), _A(a2));
                    if (a3 < _A(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (_A(d) > _A(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixel[-1*srcdstStep] = (uint8_t)(p4 - d);
                        pRPixel[0] = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

#undef _A

int32_t SubBlockPattern(VC1Block* _pBlk, VC1SingletonBlock* _sBlk)
{
    int32_t subbpattern = 0;
    if ((_sBlk->Coded ==0)&&(_pBlk->blkType < VC1_BLK_INTRA_TOP))
    {
        _pBlk->blkType = VC1_BLK_INTER8X8;
    }
    if (_sBlk->Coded ==0)
    {
        return 0;
    }

    switch (_pBlk->blkType)
    {
    case VC1_BLK_INTER4X4:
        subbpattern =  _sBlk->numCoef;
        break;

    case VC1_BLK_INTER8X4:
        if (_sBlk->numCoef & VC1_SBP_0)
        {
            subbpattern |= 0xC;
        }

        if (_sBlk->numCoef & VC1_SBP_1)
        {
            subbpattern |= 0x3;
        }
        break;

    case VC1_BLK_INTER4X8:
        if (_sBlk->numCoef & VC1_SBP_0)
        {
            subbpattern |= 0xA;
        }

        if (_sBlk->numCoef & VC1_SBP_1)
        {
            subbpattern |= 0x5;
        }
        break;

    default:
        subbpattern = 0xF;
    }
    return subbpattern;
}
IppStatus _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(int16_t* pSrcDst, uint32_t srcDstStep)
{
    int                 i,j,l;
    int32_t              TempBlock[8][8];
    int16_t*             pBlock              = pSrcDst;
    const int16_t*       pCoef               = TableFwdTransform8x8;
    int32_t              temp                = 0;

    for (i=0; i<8 ;i++)
    {
        for (j=0;j<8;j++)
        {
            temp = 1<<(12-1);
            for (l=0; l<8; l++)
            {
                temp += pBlock[l]*pCoef[l];
            }

            TempBlock[i][j] = (int16_t)(temp>>12);
            pCoef +=8;
        }
        pCoef       = TableFwdTransform8x8;
        pBlock = (int16_t*)((uint8_t*)pBlock + srcDstStep);
    }
    pCoef       = TableFwdTransform8x8;
    pBlock      = pSrcDst;
    for (j=0; j<8 ;j++)
    {
        for (i=0;i<8;i++)
        {
            temp = 1<<(20-1);
            for (l=0; l<8; l++)
            {
                temp += TempBlock[l][j]*pCoef[l];
            }

            *(int16_t*)((uint8_t*)pBlock + i*srcDstStep) = (int16_t)(temp>>20);
            pCoef +=8;
        }
        pCoef       = TableFwdTransform8x8;
        pBlock     += 1;
    }
    return ippStsNoErr;
}

// 4 colums x 8 rows
IppStatus _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R(int16_t* pSrcDst, uint32_t srcDstStep)
{
    int                 i,j,l;
    int32_t              TempBlock[8][4];
    int16_t*             pBlock              = pSrcDst;
    const int16_t*       pCoef               = TableFwdTransform4x4;
    int32_t              temp                = 0;

    for (i=0; i<8 ;i++)
    {
        for (j=0;j<4;j++)
        {
            temp = 1<<(11-1);
            for (l=0; l<4; l++)
            {
                temp += pBlock[l]*pCoef[l];
            }

            TempBlock[i][j] = (int16_t)(temp>>11);
            pCoef +=4;
        }
        pCoef       = TableFwdTransform4x4;
        pBlock = (int16_t*)((uint8_t*)pBlock + srcDstStep);
    }
    pCoef       = TableFwdTransform8x8;
    pBlock      = pSrcDst;
    for (j=0; j<4 ;j++)
    {
        for (i=0;i<8;i++)
        {
            temp = 1<<(20-1);
            for (l=0; l<8; l++)
            {
                temp += TempBlock[l][j]*pCoef[l];
            }

            *(int16_t*)((uint8_t*)pBlock + i*srcDstStep) = (int16_t)(temp>>20);
            pCoef +=8;
        }
        pCoef       = TableFwdTransform8x8;
        pBlock     += 1;
    }
    return ippStsNoErr;
}

// 8 colums x 4 rows
IppStatus _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R(int16_t* pSrcDst, uint32_t srcDstStep)
{
    int                 i,j,l;
    int32_t              TempBlock[4][8];
    int16_t*             pBlock              = pSrcDst;
    const int16_t*       pCoef               = TableFwdTransform8x8;
    int32_t              temp                = 0;

    for (i=0; i<4 ;i++)
    {
        for (j=0;j<8;j++)
        {
            temp = 1<<(12-1);
            for (l=0; l<8; l++)
            {
                temp += pBlock[l]*pCoef[l];
            }

            TempBlock[i][j] = (int16_t)(temp>>12);
            pCoef +=8;
        }
        pCoef       = TableFwdTransform8x8;
        pBlock = (int16_t*)((uint8_t*)pBlock + srcDstStep);
    }
    pCoef       = TableFwdTransform4x4;
    pBlock      = pSrcDst;
    for (j=0; j<8 ;j++)
    {
        for (i=0;i<4;i++)
        {
            temp = 1<<(19-1);
            for (l=0; l<4; l++)
            {
                temp += TempBlock[l][j]*pCoef[l];
            }

            *(int16_t*)((uint8_t*)pBlock + i*srcDstStep) = (int16_t)(temp>>19);
            pCoef +=4;
        }
        pCoef       = TableFwdTransform4x4;
        pBlock     += 1;
    }
    return ippStsNoErr;
}

// 4 colums x 4 rows
IppStatus _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R(int16_t* pSrcDst, uint32_t srcDstStep)
{
    int                 i,j,l;
    int32_t              TempBlock[4][4];
    int16_t*             pBlock              = pSrcDst;
    const int16_t*       pCoef               = TableFwdTransform4x4;
    int32_t              temp                = 0;

    for (i=0; i<4 ;i++)
    {
        for (j=0;j<4;j++)
        {
            temp = 1<<(11-1);
            for (l=0; l<4; l++)
            {
                temp += pBlock[l]*pCoef[l];
            }

            TempBlock[i][j] = (int16_t)(temp>>11);
            pCoef +=4;
        }
        pCoef       = TableFwdTransform4x4;
        pBlock = (int16_t*)((uint8_t*)pBlock + srcDstStep);
    }
    pCoef       = TableFwdTransform4x4;
    pBlock      = pSrcDst;
    for (j=0; j<4 ;j++)
    {
        for (i=0;i<4;i++)
        {
            temp = 1<<(19-1);
            for (l=0; l<4; l++)
            {
                temp += TempBlock[l][j]*pCoef[l];
            }

            *(int16_t*)((uint8_t*)pBlock + i*srcDstStep) = (int16_t)(temp>>19);
            pCoef +=4;
        }
        pCoef       = TableFwdTransform4x4;
        pBlock     += 1;
    }
    return ippStsNoErr;
}
#endif //_VC1_ENC_OWN_FUNCTIONS_
#endif // UMC_RESTRICTED_CODE

#define _A(x) (((x)<0)?-(x):(x))
IppStatus _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,
                                                               int32_t EdgeDisabledFlagU, int32_t EdgeDisabledFlagV)
{
    static int32_t EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    IppStatus ret = ippStsNoErr;
    uint8_t *pRPixelU = NULL, *pRPixelV = NULL;
    int16_t p1 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0, p7 = 0, p8 = 0;
    int16_t a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    int16_t clip = 0, d = 0;
    int32_t i = 0;
    int32_t count = 0;

    //U component
    for (count = 0; count < 2;count++)
    {
        pRPixelU = pSrcDst + 2 * srcdstStep + 4*count*srcdstStep;

        if (!(EdgeDisabledFlagU & (EdgeTable[count])))
        {

            p4 = pRPixelU[-2];
            p5 = pRPixelU[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixelU[-8];
            p2 = pRPixelU[-6];
            p3 = pRPixelU[-4];
            p6 = pRPixelU[2];
            p7 = pRPixelU[4];
            p8 = pRPixelU[6];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (_A(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = MFX_MIN(_A(a1), _A(a2));
                if (a3 < _A(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (_A(d) > _A(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixelU[-2] = (uint8_t)(p4 - d);
                    pRPixelU[0]       = (uint8_t)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;
            pRPixelU -= 2 * srcdstStep;
            for (i = 4; i--; pRPixelU += srcdstStep)
            {
                if (i == 1)
                    continue;

                p4 = pRPixelU[-2];
                p5 = pRPixelU[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixelU[-8];
                p2 = pRPixelU[-6];
                p3 = pRPixelU[-4];
                p6 = pRPixelU[2];
                p7 = pRPixelU[4];
                p8 = pRPixelU[6];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (_A(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = MFX_MIN(_A(a1), _A(a2));
                    if (a3 < _A(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (_A(d) > _A(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixelU[-2] = (uint8_t)(p4 - d);
                        pRPixelU[0]       = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }

    //V component
    for (count = 0; count < 2;count++)
    {
        pRPixelV = pSrcDst + 1 + 2 * srcdstStep + 4*count*srcdstStep;

        if (!(EdgeDisabledFlagV & (EdgeTable[count])))
        {

            p4 = pRPixelV[-2];
            p5 = pRPixelV[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixelV[-8];
            p2 = pRPixelV[-6];
            p3 = pRPixelV[-4];
            p6 = pRPixelV[2];
            p7 = pRPixelV[4];
            p8 = pRPixelV[6];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (_A(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = MFX_MIN(_A(a1), _A(a2));
                if (a3 < _A(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (_A(d) > _A(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixelV[-2] = (uint8_t)(p4 - d);
                    pRPixelV[0] = (uint8_t)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;
            pRPixelV -= 2 * srcdstStep;
            for (i=4; i--; pRPixelV += srcdstStep)
            {
                if (i==1)
                    continue;

                p4 = pRPixelV[-2];
                p5 = pRPixelV[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixelV[-8];
                p2 = pRPixelV[-6];
                p3 = pRPixelV[-4];
                p6 = pRPixelV[2];
                p7 = pRPixelV[4];
                p8 = pRPixelV[6];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (_A(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = MFX_MIN(_A(a1), _A(a2));
                    if (a3 < _A(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (_A(d) > _A(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixelV[-2] = (uint8_t)(p4 - d);
                        pRPixelV[0]  = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}
IppStatus _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(uint8_t* pSrcDst,int32_t pQuant, int32_t srcdstStep,
                                                               int32_t EdgeDisabledFlagU, int32_t EdgeDisabledFlagV)
{
    IppStatus ret = ippStsNoErr;
    static int32_t EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    uint8_t *pRPixelU = NULL, *pRPixelV = NULL;
    int16_t p1 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0, p7 = 0, p8 = 0;
    int16_t a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    int16_t clip = 0, d = 0;
    int32_t i = 0;
    int32_t count = 0;
    
    for (count = 0; count < 2; count++)
    {
        //U component
        pRPixelU = pSrcDst + (2 + 4*count)*2; //added step between 2 U chroma pixels (horizontal step)

        if (!(EdgeDisabledFlagU & (EdgeTable[count]) ))
        {

            p4 = pRPixelU[-1*srcdstStep];
            p5 = pRPixelU[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixelU[-4*srcdstStep];
            p2 = pRPixelU[-3*srcdstStep];
            p3 = pRPixelU[-2*srcdstStep];
            p6 = pRPixelU[1*srcdstStep];
            p7 = pRPixelU[2*srcdstStep];
            p8 = pRPixelU[3*srcdstStep];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (_A(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = MFX_MIN(_A(a1), _A(a2));
                if (a3 < _A(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (_A(d) > _A(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixelU[-1*srcdstStep] = (uint8_t)(p4 - d);
                    pRPixelU[0] = (uint8_t)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;

            pRPixelU -= 4;

            for (i = 4; i--; pRPixelU += 2) //added horizontal step
            {
                if (i == 1)
                    continue;

                p4 = pRPixelU[-1*srcdstStep];
                p5 = pRPixelU[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixelU[-4*srcdstStep];
                p2 = pRPixelU[-3*srcdstStep];
                p3 = pRPixelU[-2*srcdstStep];
                p6 = pRPixelU[1*srcdstStep];
                p7 = pRPixelU[2*srcdstStep];
                p8 = pRPixelU[3*srcdstStep];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (_A(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = MFX_MIN(_A(a1), _A(a2));
                    if (a3 < _A(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (_A(d) > _A(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixelU[-1*srcdstStep] = (uint8_t)(p4 - d);
                        pRPixelU[0] = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }

    for (count = 0; count < 2; count++)
    {
        //V component
        pRPixelV = pSrcDst + 1 + (2 + 4*count)*2;

        if (!(EdgeDisabledFlagV & (EdgeTable[count]) ))
        {

            p4 = pRPixelV[-1*srcdstStep];
            p5 = pRPixelV[0];

            if (!((p4-p5)/2))
                continue;

            p1 = pRPixelV[-4*srcdstStep];
            p2 = pRPixelV[-3*srcdstStep];
            p3 = pRPixelV[-2*srcdstStep];
            p6 = pRPixelV[1*srcdstStep];
            p7 = pRPixelV[2*srcdstStep];
            p8 = pRPixelV[3*srcdstStep];

            a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;
            if (_A(a0) < pQuant)
            {
                a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                a3 = MFX_MIN(_A(a1), _A(a2));
                if (a3 < _A(a0))
                {
                    d = 5*((VC1_SIGN(a0)*a3) - a0) / 8;
                    clip = (p4 - p5)/2;
                    if (d*clip > 0)
                    {
                        if (_A(d) > _A(clip))
                            d = clip;
                    }
                    else
                        d=0;
                    pRPixelV[-1*srcdstStep] = (uint8_t)(p4 - d);
                    pRPixelV[0] = (uint8_t)(p5 + d);
                }
                else
                    continue;
            }
            else
                continue;

            pRPixelV -= 4;

            for (i=4; i--; pRPixelV += 2)//added horizontal step
            {
                if (i==1)
                    continue;

                p4 = pRPixelV[-1*srcdstStep];
                p5 = pRPixelV[0];

                if (!((p4-p5)/2))
                    continue;

                p1 = pRPixelV[-4*srcdstStep];
                p2 = pRPixelV[-3*srcdstStep];
                p3 = pRPixelV[-2*srcdstStep];
                p6 = pRPixelV[1*srcdstStep];
                p7 = pRPixelV[2*srcdstStep];
                p8 = pRPixelV[3*srcdstStep];

                a0 = (2*(p3 - p6) - 5*(p4 - p5) + 4) >> 3;

                if (_A(a0) < pQuant)
                {
                    a1 = (2*(p1 - p4) - 5*(p2 - p3) + 4) >> 3;
                    a2 = (2*(p5 - p8) - 5*(p6 - p7) + 4) >> 3;
                    a3 = MFX_MIN(_A(a1), _A(a2));
                    if (a3 < _A(a0))
                    {
                        d = 5*((VC1_SIGN(a0) * a3) - a0) / 8;
                        clip = (p4 - p5)/2;
                        if (d*clip > 0)
                        {
                            if (_A(d) > _A(clip))
                                d = clip;
                        }
                        else
                            d=0;
                        pRPixelV[-1*srcdstStep] = (uint8_t)(p4 - d);
                        pRPixelV[0] = (uint8_t)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}
#undef _A

#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_

#define _sign(x) (((x)<0)?-1:!!(x))
IppStatus _own_ippiQuantIntraUniform(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant)
{
    int i,j;
    int16_t* pBlock = pSrcDst;
    int32_t intra = 1;
    for(i = 0; i < 8; i++)
    {
        for (j = intra; j < 8; j++)
        {

            pBlock[j] = pBlock[j]/doubleQuant;

        }
        intra  = 0;
        pBlock = (int16_t*)((uint8_t*) pBlock + srcDstStep);
    }

    return ippStsNoErr;
}

IppStatus _own_ippiQuantIntraNonUniform(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant)
{
    int i,j;
    int16_t* pBlock = pSrcDst;
    uint8_t   Quant  = doubleQuant>>1;
    int32_t intra = 1;

    for(i = 0; i < 8; i++)
    {
        for (j = intra; j < 8; j++)
        {
            pBlock[j] = (pBlock[j]-_sign(pBlock[j])*Quant)/doubleQuant;
        }
        intra  = 0;
        pBlock = (int16_t*)((uint8_t*) pBlock + srcDstStep);
    }
    return ippStsNoErr;
}

IppStatus _own_ippiQuantInterUniform(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant, mfxSize roiSize)
{
    int i,j;
    int16_t* pBlock = pSrcDst;

    for(i = 0; i < roiSize.height; i++)
    {
        for (j = 0; j < roiSize.width; j++)
        {

            pBlock[j] = pBlock[j]/doubleQuant;

        }
        pBlock = (int16_t*)((uint8_t*) pBlock + srcDstStep);
    }

    return ippStsNoErr;
}

IppStatus _own_ippiQuantInterNonUniform(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant, mfxSize roiSize)
{
    int i,j;
    int16_t* pBlock = pSrcDst;
    uint8_t   Quant  = doubleQuant>>1;

    for(i = 0; i < roiSize.height; i++)
    {
        for (j = 0; j < roiSize.width; j++)
        {
            pBlock[j] = (pBlock[j]-_sign(pBlock[j])*Quant)/doubleQuant;
        }

        pBlock = (int16_t*)((uint8_t*) pBlock + srcDstStep);
    }
    return ippStsNoErr;
}
#undef _sign
#endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#define _sign(x) (((x)<0)?-1:!!(x))
#define _abs(x)   ((x<0)? -(x):(x))
#define _mask  0x03
IppStatus _own_ippiQuantIntraTrellis(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant, int8_t *pRoundControl,int32_t roundControlStep)
{
    int i,j;
    int16_t* pBlock = pSrcDst;
    int32_t intra = 1;

    for(i = 0; i < 8; i++)
    {
        for (j = intra; j < 8; j++)
        {
            int16_t abs_pixel    = _abs(pBlock[j]);
            int16_t round        = (pRoundControl[j])*doubleQuant;
            int16_t quant_pixel  = 0;

            quant_pixel = (round < abs_pixel)?(abs_pixel - round)/doubleQuant:0;

            pBlock[j] = (pBlock[j]<0)? -quant_pixel:(int16_t)quant_pixel;
        }
        intra  = 0;
        pBlock = (int16_t*)((uint8_t*) pBlock + srcDstStep);
        pRoundControl = pRoundControl + roundControlStep;
    }
    return ippStsNoErr;
}
IppStatus _own_ippiQuantInterTrellis(int16_t* pSrcDst, int32_t srcDstStep,int32_t doubleQuant, mfxSize roiSize,int8_t *pRoundControl, int32_t roundControlStep)
{
    int i,j;
    int16_t* pBlock = pSrcDst;

    for(i = 0; i < roiSize.height; i++)
    {
        for (j = 0; j < roiSize.width; j++)
        {
            int16_t abs_pixel    = _abs(pBlock[j]);
            int16_t round        = (pRoundControl[j])*doubleQuant;
            int16_t quant_pixel  = 0;

            quant_pixel = (round < abs_pixel)?(abs_pixel - round)/doubleQuant:0;

            pBlock[j] = (pBlock[j]<0)? -quant_pixel:(int16_t)quant_pixel;
        }
        pBlock = (int16_t*)((uint8_t*) pBlock + srcDstStep);
        pRoundControl = pRoundControl + roundControlStep;
    }
    return ippStsNoErr;
}

#undef _sign
#undef _abs
#undef _mask


#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
#define _sign(x) (((x)<0)?-1:!!(x))
IppStatus _own_ippiQuantInvIntraUniform_VC1_16s_C1R(const int16_t* pSrc, int32_t srcStep,
                                                     int16_t* pDst, int32_t dstStep,
                                                  int32_t doubleQuant, mfxSize* pDstSizeNZ)
{
    int32_t i = 1;
    int32_t j = 0;
    int32_t X[8] = {0};
    int32_t Y[8] = {0};
    int16_t S;

    const int16_t* pSrc1 = pSrc;
    int16_t* pDst1 = pDst;

    for(j = 0; j < 8; j++)
    {
       for(i; i < 8; i++)
       {
           pDst1[i] = (int16_t)(pSrc1[i]*doubleQuant);
           S = !pDst1[i] ;
           X[i] = X[i] + S;
           Y[j] = Y[j] + S;
       }
       i = 0;
       pSrc1 = (int16_t*)((uint8_t*)pSrc1 + srcStep);
       pDst1 = (int16_t*)((uint8_t*)pDst1 + dstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}

IppStatus _own_ippiQuantInvIntraNonUniform_VC1_16s_C1R(const int16_t* pSrc, int32_t srcStep,
                                                        int16_t* pDst, int32_t dstStep,
                                                          int32_t doubleQuant, mfxSize* pDstSizeNZ)
{
    int32_t i = 1;
    int32_t j = 0;
    const int16_t* pSrc1 = pSrc;
    int16_t* pDst1 = pDst;

    int32_t X[8] = {0};
    int32_t Y[8] = {0};
    int16_t S;

    for(j = 0; j < 8; j++)
    {
        for(i; i < 8; i++)
        {
            pDst1[i] = (int16_t)(pSrc1[i]*doubleQuant) + (int16_t)(_sign(pSrc1[i])*(doubleQuant>>1));
            S = !pDst1[i] ;
            X[i] = X[i] + S;
            Y[j] = Y[j] + S;
        }
        i = 0;
        pSrc1 = (int16_t*)((uint8_t*)pSrc1 + srcStep);
        pDst1 = (int16_t*)((uint8_t*)pDst1 + dstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}
IppStatus _own_ippiQuantInvInterUniform_VC1_16s_C1R(const int16_t* pSrc, int32_t srcStep,
                                                     int16_t* pDst, int32_t dstStep,
                                                       int32_t doubleQuant, mfxSize roiSize,
                                                       mfxSize* pDstSizeNZ)
{
    int32_t i = 0;
    int32_t j = 0;
    int32_t X[8] = {0};
    int32_t Y[8] = {0};
    int16_t S;

    const int16_t* pSrc1 = pSrc;
    int16_t* pDst1 = pDst;

    for(j = 0; j < roiSize.height; j++)
    {
       for(i = 0; i < roiSize.width; i++)
       {
           pDst1[i] = (int16_t)(pSrc1[i]*doubleQuant);
           S = !pDst1[i] ;
           X[i] = X[i] + S;
           Y[j] = Y[j] + S;
       }
       pSrc1 = (int16_t*)((uint8_t*)pSrc1 + srcStep);
       pDst1 = (int16_t*)((uint8_t*)pDst1 + dstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}
IppStatus _own_ippiQuantInvInterNonUniform_VC1_16s_C1R(const int16_t* pSrc, int32_t srcStep,
                                                        int16_t* pDst, int32_t dstStep,
                                                          int32_t doubleQuant, mfxSize roiSize,
                                                          mfxSize* pDstSizeNZ)
{
    int32_t i = 0;
    int32_t j = 0;
    const int16_t* pSrc1 = pSrc;
    int16_t* pDst1 = pDst;
    int32_t X[8] = {0};
    int32_t Y[8] = {0};
    int16_t S;


    for(j = 0; j < roiSize.height; j++)
    {
        for(i = 0; i < roiSize.width; i++)
        {
            pDst1[i] = (int16_t)(pSrc1[i]*doubleQuant) + (int16_t)(_sign(pSrc1[i])*(doubleQuant>>1));
            S = !pDst1[i] ;
            X[i] = X[i] + S;
            Y[j] = Y[j] + S;
        }

        pSrc1 = (int16_t*)((uint8_t*)pSrc1 + srcStep);
        pDst1 = (int16_t*)((uint8_t*)pDst1 + dstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}
#undef _sign
#endif //_VC1_ENC_OWN_FUNCTIONS_
#endif // UMC_RESTRICTED_CODE

uint8_t Get_CBPCY(uint32_t MBPatternCur, uint32_t CBPCYTop, uint32_t CBPCYLeft, uint32_t CBPCYULeft)
{

    uint32_t LT3   = (CBPCYULeft >> VC1_ENC_PAT_POS_Y3) & 0x01;
    uint32_t T2    = (CBPCYTop   >> VC1_ENC_PAT_POS_Y2) & 0x01;
    uint32_t T3    = (CBPCYTop   >> VC1_ENC_PAT_POS_Y3) & 0x01;
    uint32_t L1    = (CBPCYLeft  >> VC1_ENC_PAT_POS_Y1) & 0x01;
    uint32_t L3    = (CBPCYLeft  >> VC1_ENC_PAT_POS_Y3) & 0x01;
    uint32_t CBPCY = MBPatternCur;
    uint32_t Y0    = (CBPCY>>VC1_ENC_PAT_POS_Y0)&0x01;


    CBPCY ^=  (uint8_t)(((LT3==T2)? L1:T2) << VC1_ENC_PAT_POS_Y0);
    CBPCY ^=  (uint8_t)(((T2 ==T3)? Y0:T3) << VC1_ENC_PAT_POS_Y1);
    CBPCY ^=  (uint8_t)(((L1 ==Y0)? L3:Y0) << VC1_ENC_PAT_POS_Y2);
    CBPCY ^=  (uint8_t)(((Y0 ==((MBPatternCur>>VC1_ENC_PAT_POS_Y1)&0x01))?
                (MBPatternCur>>VC1_ENC_PAT_POS_Y2)&0x01:(MBPatternCur>>VC1_ENC_PAT_POS_Y1)&0x01) << VC1_ENC_PAT_POS_Y3);
    return (uint8_t)CBPCY;
}



int32_t SumSqDiff_1x7_16s(int16_t* pSrc, uint32_t step, int16_t* pPred)
{
    int32_t sum=0;

    for(int i=0; i<7 ;i++)
    {
        pPred  = (int16_t*)((uint8_t*)pPred+step);
        pSrc  = (int16_t*)((uint8_t*)pSrc+step);
        sum  += (pSrc[0]-pPred[0])*(pSrc[0]- pPred[0]) - pSrc[0]*pSrc[0];
    }
    return sum;
}
int32_t SumSqDiff_7x1_16s(int16_t* pSrc, int16_t* pPred)
{
    int32_t sum=0;

    for(int i=1; i<8 ;i++)
    {
        sum  += (pSrc[i]-pPred[i])*(pSrc[i]- pPred[i]) - pSrc[i]*pSrc[i];
    }
    return sum;
}

typedef void (*Diff8x8VC1) (const int16_t* pSrc1, int32_t src1Step,
              const int16_t* pSrc2, int32_t src2Step,
              int16_t* pDst,  int32_t dstStep, uint32_t pred);

static void Diff8x8_NonPred(const int16_t* pSrc1, int32_t /*src1Step*/,
                            const int16_t* pSrc2, int32_t /*src2Step*/,
                            int16_t* pDst,  int32_t /*dstStep*/, uint32_t /*pred*/)
{
    pDst[0] = pSrc1[0] - pSrc2[0];
}
static void Diff8x8_HorPred(const int16_t* pSrc1, int32_t /*src1Step*/,
              const int16_t* pSrc2, int32_t /*src2Step*/,
              int16_t* pDst,  int32_t /*dstStep*/, uint32_t /*pred*/)
{
    pDst[0] = pSrc1[0] - pSrc2[0];
    pDst[1] = pSrc1[1] - pSrc2[1];
    pDst[2] = pSrc1[2] - pSrc2[2];
    pDst[3] = pSrc1[3] - pSrc2[3];
    pDst[4] = pSrc1[4] - pSrc2[4];
    pDst[5] = pSrc1[5] - pSrc2[5];
    pDst[6] = pSrc1[6] - pSrc2[6];
    pDst[7] = pSrc1[7] - pSrc2[7];
}

static void Diff8x8_VerPred(const int16_t* pSrc1, int32_t src1Step,
              const int16_t* pSrc2, int32_t src2Step,
              int16_t* pDst,  int32_t dstStep, uint32_t /*pred*/)
{
    int32_t j;
    for(j=0;j<8;j++){
        (*pDst) = (*pSrc1) - (*pSrc2);
        pDst = (int16_t*)((uint8_t*)pDst + dstStep);
        pSrc1 = (int16_t*)((uint8_t*)pSrc1 + src1Step);
        pSrc2 = (int16_t*)((uint8_t*)pSrc2 + src2Step);
    }
}

static void Diff8x8_Nothing(const int16_t* /*pSrc1*/, int32_t /*src1Step*/,
              const int16_t* /*pSrc2*/, int32_t /*src2Step*/,
              int16_t* /*pDst*/,  int32_t /*dstStep*/, uint32_t /*pred*/)
{
}

Diff8x8VC1 Diff8x8FuncTab[] ={
    &Diff8x8_NonPred,
    &Diff8x8_VerPred,
    &Diff8x8_HorPred,
    &Diff8x8_Nothing
};

void Diff8x8 (const int16_t* pSrc1, int32_t src1Step,
              const int16_t* pSrc2, int32_t src2Step,
              int16_t* pDst,  int32_t dstStep, uint32_t pred)
{
    uint32_t index = pred & 0x03;
    mfxSize blkSize     = {8,8};

    ippiCopy_16s_C1R(pSrc1,src1Step,pDst,dstStep,blkSize);
    Diff8x8FuncTab[index](pSrc1,src1Step,pSrc2,src2Step,pDst,dstStep,pred);
}

 //---BEGIN------------------------Copy block, MB functions-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
IppStatus _own_Copy8x8_16x16_8u16s (const uint8_t* pSrc, uint32_t srcStep, int16_t* pDst, uint32_t dstStep, mfxSize roiSize)
{
    for (int i = 0; i < roiSize.height; i++)
    {
        for(int j = 0; j < roiSize.width; j++)
        {
            pDst[j] = pSrc[j];
        }
        pSrc += srcStep;
        pDst = (int16_t*)((uint8_t*)pDst + dstStep);
    }
    return ippStsNoErr;
}
#endif
#endif
//---END------------------------Copy block, MB functions-----------------------------------------------

//uint32_t GetNumZeros(int16_t* pSrc, uint32_t srcStep, bool bIntra)
//{
//    int         i,j;
//    uint32_t      s=0;
//    int16_t*     pBlock = pSrc;
//
//    for(i = 0; i<8; i++)
//    {
//        for (j = bIntra; j<8; j++)
//        {
//            s+= !pBlock[j];
//        }
//        bIntra  = false;
//        pBlock = (int16_t*)((uint8_t*) pBlock + srcStep);
//    }
//    //printf ("intra MB: num of coeff %d\n",64 - s - bIntra);
//    return s;
//}
//uint32_t GetNumZeros(int16_t* pSrc, uint32_t srcStep)
//{
//    int         i,j;
//    uint32_t      s=0;
//    int16_t*     pBlock = pSrc;
//
//    for(i = 0; i<8; i++)
//    {
//        for (j = 0; j<8; j++)
//        {
//            s+= !pBlock[j];
//        }
//        pBlock = (int16_t*)((uint8_t*) pBlock + srcStep);
//    }
//    //printf ("intra MB: num of coeff %d\n",64 - s);
//    return s;
//}
//uint8_t GetBlockPattern(int16_t* pBlock, uint32_t step)
//{
//    int i,j;
//    uint8_t s[4]={0};
//    for (i=0;i<8;i++)
//    {
//        for(j=0;j<8;j++)
//        {
//            s[((i>>2)<<1)+(j>>2)] += !(pBlock[j]);
//        }
//        pBlock = (int16_t*)((uint8_t*)pBlock + step);
//    }
//    //printf ("inter MB: num of coeff %d\n",64 - (s[0]+s[1]+s[2]+s[3]));
//    return (((s[0]<16)<<3)|((s[1]<16)<<2)|((s[2]<16)<<1)| (s[3]<16));
//
//}

uint8_t GetMode( uint8_t &run, int16_t &level, const uint8_t *pTableDR, const uint8_t *pTableDL, bool& sign)
{
    sign  = level < 0;
    level = (sign)? -level : level;
    if (run <= pTableDR[1])
    {
        uint8_t maxLevel = pTableDL[run];
        if (level <= maxLevel)
        {
            return 0;
        }
        if (level <= 2*maxLevel)
        {
            level = level - maxLevel;
            return 1;
        }
    }
    if (level <= pTableDL[0])
    {
        uint8_t maxRun = pTableDR[level];
        if (run <= (uint8_t)(2*maxRun + 1)) // level starts from 0
        {
            run = run - (uint8_t)maxRun - 1;
            return 2;
        }
    }
    return 3;
}
//bool GetRLCode(int32_t run, int32_t level, IppVCHuffmanSpec_32s *pTable, int32_t &code, int32_t &len)
//{
//    int32_t  maxRun = pTable[0] >> 20;
//    int32_t  addr;
//    int32_t  *table;
//
//    if(run > maxRun)
//    {
//       return false;
//    }
//    addr  = pTable[run + 1];
//    table = (int32_t*)((int8_t*)pTable + addr);
//    if(level <= table[0])
//    {
//        len  = *(table  + level) & 0x1f;
//        code = (*(table + level) >> 16);
//        return true;
//    }
//    return false;
//}
//uint8_t        Zigzag       ( int16_t*                pBlock,
//                            uint32_t                 blockStep,
//                            bool                   bIntra,
//                            const uint8_t*           pScanMatrix,
//                            uint8_t*                 pRuns,
//                            int16_t*                pLevels)
//{
//    int32_t      i = 0, pos = 0;
//    int16_t      value = 0;
//    uint8_t       n_pairs = 0;
//
//    pRuns[n_pairs]  = 0;
//    for (i = bIntra; i<64; i++)
//    {
//        pos    = pScanMatrix[i];
//        value = *((int16_t*)((uint8_t*)pBlock + blockStep*(pos/8)) + pos%8);
//        if (!value)
//        {
//            pRuns[n_pairs]++;
//        }
//        else
//        {
//            pLevels [n_pairs++] = value;
//            pRuns   [n_pairs]  = 0;
//        }
//
//    }
//    return n_pairs;
//}
//uint8_t GetLength_16s(int16_t value)
//{
//    int    i=0;
//    uint16_t n = (uint16_t)value;
//    while (n>>i)
//    {
//        i++;
//    }
//    return i;
//
//}


//---BEGIN------------------------Add constant to block-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_

IppStatus _own_Diff8x8C_16s(int16_t c, int16_t*  pBlock, uint32_t   blockStep, mfxSize roiSize, int scaleFactor)
{
    int i,j;
    for (i=0;i<8;i++)
    {
        for (j=0;j<8;j++)
        {
            pBlock[j] -= c;
        }
        pBlock = (int16_t*)((uint8_t*)pBlock + blockStep);
    }
    return ippStsNoErr;
}
IppStatus _own_Add8x8C_16s(int16_t c, int16_t*  pBlock, uint32_t   blockStep, mfxSize roiSize, int scaleFactor)
{
    int i,j;
    for (i=0;i<8;i++)
    {
        for (j=0;j<8;j++)
        {
            pBlock[j] += c;
        }
        pBlock = (int16_t*)((uint8_t*)pBlock + blockStep);
    }
    return ippStsNoErr;
}
#endif
#endif
//---End------------------------Add constant to block-----------------------------------------------

//---Begin------------------------Get difference-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
IppStatus _own_ippiGetDiff8x8_8u16s_C1(uint8_t*  pBlock,       uint32_t   blockStep,
                        uint8_t*  pPredBlock,   uint32_t   predBlockStep,
                        int16_t* pDst,         uint32_t   dstStep,
                        int16_t* pDstPredictor,int32_t  dstPredictorStep,
                        int32_t  mcType,       int32_t  roundControl)
{
    int i,j;

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            pDst[j] = (pBlock[j] - pPredBlock[j]);
        }
        pDst = (int16_t*)((uint8_t*)pDst + dstStep);
        pBlock     += blockStep;
        pPredBlock += predBlockStep;
    }
    return ippStsNoErr;
}
IppStatus _own_ippiGetDiff16x16_8u16s_C1(uint8_t*  pBlock,       uint32_t   blockStep,
                          uint8_t*  pPredBlock,   uint32_t   predBlockStep,
                          int16_t* pDst,         uint32_t   dstStep,
                          int16_t* pDstPredictor,int32_t  dstPredictorStep,
                          int32_t  mcType,       int32_t  roundControl)
{
    int i,j;

    for (i = 0; i < 16; i++)
    {
        for (j = 0; j < 16; j++)
        {
            pDst[j] = (pBlock[j] - pPredBlock[j]);
        }
        pDst = (int16_t*)((uint8_t*)pDst + dstStep);
        pBlock     += blockStep;
        pPredBlock += predBlockStep;
    }
    return ippStsNoErr;
}
#endif
#endif
//---End------------------------Get difference-----------------------------------------------

//---Begin---------------------Motion compensation-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_

IppStatus _own_Add16x16_8u16s(const uint8_t*  pSrcRef,  uint32_t   srcRefStep,
                         const int16_t* pSrcYData,uint32_t   srcYDataStep,
                         uint8_t* pDst, uint32_t   dstStep,
                         int32_t mctype, int32_t roundControl)
{
    int i,j;
    //printf("\nLUMA\n");

    for (i = 0; i < 16; i++)
    {
        //printf("\n");
        for (j = 0; j < 16; j++)
        {
            pDst[j] = mfx::byte_clamp(pSrcYData[j] + (int16_t)pSrcRef[j]);
            //printf(" %d", pDst[j]);
        }
        pSrcYData = (int16_t*)((uint8_t*)pSrcYData + srcYDataStep);
        pSrcRef  += srcRefStep;
        pDst     = pDst + dstStep;
    }

    return ippStsNoErr;
}

IppStatus _own_Add8x8_8u16s ( const uint8_t*  pSrcRef,  uint32_t   srcRefStep,
                         const int16_t* pSrcYData,uint32_t   srcYDataStep,
                         uint8_t* pDst, uint32_t   dstStep,
                         int32_t mctype, int32_t roundControl)
{
    int i,j;

    //printf("\nCHROMA\n");
    for (i = 0; i < 8; i++)
    {
        //printf("\n");
        for (j = 0; j < 8; j++)
        {
            pDst[j] = mfx::byte_clamp(pSrcYData[j] + (int16_t)pSrcRef[j]);
            //printf(" %d", pDst[j]);
        }
        pSrcYData = (int16_t*)((uint8_t*)pSrcYData + srcYDataStep);
        pSrcRef  += srcRefStep;
        pDst     = pDst + dstStep;
    }
    return ippStsNoErr;
}
#endif
#endif
//---End---------------------Motion compensation-----------------------------------------------

int16_t median3(int16_t a, int16_t b, int16_t c)
{
  uint32_t d  = ((((uint32_t)(a-b))&0x80000000) |
              ((((uint32_t)(b-c))&0x80000000)>>1)|
              ((((uint32_t)(a-c))&0x80000000)>>2));

  switch(d)
  {
  case 0x60000000:
  case 0x80000000:
       return a;
  case 0xe0000000:
  case 0x00000000:
       return b;
  case 0x40000000:
  case 0xa0000000:
      return c;
  default:
      assert(0);
  }
  return 0;

}
int16_t median4(int16_t a, int16_t b, int16_t c, int16_t d)
{
  uint32_t e  = ((((uint32_t)(a-b))&0x80000000) |
              ((((uint32_t)(b-c))&0x80000000)>>1)|
              ((((uint32_t)(c-d))&0x80000000)>>2)|
              ((((uint32_t)(a-c))&0x80000000)>>3)|
              ((((uint32_t)(a-d))&0x80000000)>>4)|
              ((((uint32_t)(b-d))&0x80000000)>>5));

  switch(e)
  {
  case 0xAC000000:
  case 0xD0000000:
  case 0x2C000000:
  case 0x50000000:
       return (a+b)/2;
  case 0x7C000000:
  case 0x90000000:
  case 0x6C000000:
  case 0x80000000:
       return (a+c)/2;
  case 0x5C000000:
  case 0xA8000000:
  case 0x54000000:
  case 0xA0000000:
       return (a+d)/2;
  case 0xFC000000:
  case 0xBC000000:
  case 0x40000000:
  case 0x00000000:
       return (b+c)/2;
  case 0xDC000000:
  case 0xD8000000:
  case 0x24000000:
  case 0x20000000:
       return (b+d)/2;
  case 0xB8000000:
  case 0x98000000:
  case 0x64000000:
  case 0x44000000:
      return (c+d)/2;
  default:
      assert(0);
  }
  return 0;

}
void PredictMV(sCoordinate* predA,sCoordinate* predB,sCoordinate* predC, sCoordinate* res)
{
    res->x=0;
    res->y=0;

    if (predA)
    {
        predB  = (!predB)? res: predB;
        predC  = (!predC)? res: predC;
        res->x = median3(predA->x, predB->x, predC->x);
        res->y = median3(predA->y, predB->y, predC->y);
    }
    else if (predC)
    {
        res->x = predC->x;
        res->y = predC->y;
    }
}
int16_t VC1abs(int16_t value)
{
  uint16_t s = value>>15;
  s = (value + s)^s;
  return s;
}

void InitScaleInfo(sScaleInfo* pInfo, bool bCurSecondField ,bool bBottom,
                   uint8_t ReferenceFrameDist, uint8_t MVRangeIndex)
{
    static int16_t SCALEOPP[2][4]   = {{128,     192,    213,    224},
                                      {128,     64,     43,     32}};
    static int16_t SCALESAME1[2][4] = {{512,     341,    307,    293},
                                      {512,     1024,   1563,   2048}};
    static int16_t SCALESAME2[2][4] = {{219,     236,    242,    245},
                                      {219,     204,    200,    198}};
    static int16_t SCALEZONEX1[2][4] = {{32,     48,     53,     56},
                                       {32,     16,     11,     8}};
    static int16_t SCALEZONEY1[2][4] = {{ 8,     12,     13,     14},
                                       { 8,     4,      3,      2}};
    static int16_t ZONEOFFSETX1[2][4] = {{37,    20,     14,     11},
                                        {37,    52,     56,     58}};
    static int16_t ZONEYOFFSET1[2][4] = {{10,    5,      4,      3},
                                        {10,    13,     14,     15}};

    ReferenceFrameDist  = (ReferenceFrameDist>3)? 3:ReferenceFrameDist;

    pInfo->scale_opp    = SCALEOPP    [bCurSecondField][ReferenceFrameDist];
    pInfo->scale_same1  = SCALESAME1  [bCurSecondField][ReferenceFrameDist];
    pInfo->scale_same2  = SCALESAME2  [bCurSecondField][ReferenceFrameDist];
    pInfo->scale_zoneX  = SCALEZONEX1 [bCurSecondField][ReferenceFrameDist];
    pInfo->scale_zoneY  = SCALEZONEY1 [bCurSecondField][ReferenceFrameDist];
    pInfo->zone_offsetX = ZONEOFFSETX1[bCurSecondField][ReferenceFrameDist];
    pInfo->zone_offsetY = ZONEYOFFSET1[bCurSecondField][ReferenceFrameDist];

    pInfo->bBottom      =  bBottom;
    pInfo->rangeX       =  MVRange[2*MVRangeIndex]*4;
    pInfo->rangeY       =  MVRange[2*MVRangeIndex + 1]*4;
}
void InitScaleInfoBackward (sScaleInfo* pInfo, bool bCurSecondField ,bool bBottom,
                   uint8_t ReferenceFrameDist, uint8_t MVRangeIndex)
{
    static int16_t SCALEOPP[2][4]   = {{171,     205,    219,    288},
                                      {128,     64,     43,     32}};
    static int16_t SCALESAME1[2][4] = {{384,     320,    299,    288},
                                      {512,     1024,   1563,   2048}};
    static int16_t SCALESAME2[2][4] = {{230,     239,    244,    246},
                                      {219,     204,    200,    198}};
    static int16_t SCALEZONEX1[2][4] = {{43,     51,     55,     57},
                                       {32,     16,     11,     8}};
    static int16_t SCALEZONEY1[2][4] = {{11,     13,     14,     14},
                                       { 8,     4,      3,      2}};
    static int16_t ZONEOFFSETX1[2][4] = {{26,    17,     12,     10},
                                        {37,    52,     56,     58}};
    static int16_t ZONEYOFFSET1[2][4] = {{7,     4,      3,      3},
                                        {10,    13,     14,     15}};

    ReferenceFrameDist  = (ReferenceFrameDist>3)? 3:ReferenceFrameDist;

    pInfo->scale_opp    = SCALEOPP    [bCurSecondField][ReferenceFrameDist];
    pInfo->scale_same1  = SCALESAME1  [bCurSecondField][ReferenceFrameDist];
    pInfo->scale_same2  = SCALESAME2  [bCurSecondField][ReferenceFrameDist];
    pInfo->scale_zoneX  = SCALEZONEX1 [bCurSecondField][ReferenceFrameDist];
    pInfo->scale_zoneY  = SCALEZONEY1 [bCurSecondField][ReferenceFrameDist];
    pInfo->zone_offsetX = ZONEOFFSETX1[bCurSecondField][ReferenceFrameDist];
    pInfo->zone_offsetY = ZONEYOFFSET1[bCurSecondField][ReferenceFrameDist];

    pInfo->bBottom      =  bBottom;
    pInfo->rangeX       =  MVRange[2*MVRangeIndex]*4;
    pInfo->rangeY       =  MVRange[2*MVRangeIndex + 1]*4;
}
static int16_t scale_sameX(int16_t n,sScaleInfo* pInfo, bool bHalf)
{
    int16_t abs_n = VC1abs(n = (n>>((uint16_t)bHalf)));
    int32_t s;
    if (abs_n>255)
    {
        return n<<((uint16_t)bHalf);
    }
    else if (abs_n<pInfo->scale_zoneX)
    {
        s = (int16_t)(((int32_t)(n*pInfo->scale_same1))>>8);
    }
    else
    {
        s = (int16_t)(((int32_t)(n*pInfo->scale_same2))>>8);
        s = (n<0)? s - pInfo->zone_offsetX:s + pInfo->zone_offsetX;
    }
    s = (s>pInfo->rangeX-1)? pInfo->rangeX-1:s;
    s = (s<-pInfo->rangeX) ? -pInfo->rangeX :s;

    return (int16_t) (s<<((uint16_t)bHalf));
}
static int16_t scale_oppX(int16_t n,sScaleInfo* pInfo, bool bHalf)
{
    int32_t s = (((int32_t)((n>>((uint32_t)bHalf))*pInfo->scale_opp))>>8);
    return (int16_t) (s<<((uint16_t)bHalf));
}
static int16_t scale_sameY(int16_t n,sScaleInfo* pInfo, bool bHalf)
{
    int16_t abs_n = VC1abs(n = (n>>((uint16_t)bHalf)));
    int32_t s     = 0;
    if (abs_n>63)
    {
        return n<<((uint16_t)bHalf);
    }
    else if (abs_n<pInfo->scale_zoneY)
    {
        s = (int16_t)(((int32_t)(n*pInfo->scale_same1))>>8);
    }
    else
    {
        s = (int16_t)(((int32_t)(n*pInfo->scale_same2))>>8);
        s = (n<0)? s - pInfo->zone_offsetY:s + pInfo->zone_offsetY;
    }
    s = (s>pInfo->rangeY/2-1)? pInfo->rangeY/2-1:s;
    s = (s<-pInfo->rangeY/2) ? -pInfo->rangeY/2 :s;

    return (int16_t) (s<<((uint16_t)bHalf));
}
static int16_t scale_sameY_B(int16_t n,sScaleInfo* pInfo, bool bHalf)
{
    int16_t abs_n = VC1abs(n = (n>>((uint16_t)bHalf)));
    int32_t s     = 0;
    if (abs_n>63)
    {
        return n<<((uint16_t)bHalf);
    }
    else if (abs_n<pInfo->scale_zoneY)
    {
        s = (int16_t)(((int32_t)(n*pInfo->scale_same1))>>8);
    }
    else
    {
        s = (int16_t)(((int32_t)(n*pInfo->scale_same2))>>8);
        s = (n<0)? s - pInfo->zone_offsetY:s + pInfo->zone_offsetY;
    }
    if (pInfo->bBottom)
    {
        s = (s>pInfo->rangeY/2)? pInfo->rangeY/2:s;
        s = (s<-pInfo->rangeY/2+1) ? -pInfo->rangeY/2+1 :s;
    }
    else
    {
        s = (s>pInfo->rangeY/2-1)? pInfo->rangeY/2-1:s;
        s = (s<-pInfo->rangeY/2) ? -pInfo->rangeY/2 :s;
    }
    return (int16_t) (s<<((uint16_t)bHalf));
}
static int16_t scale_oppY(int16_t n,sScaleInfo* pInfo,  bool bHalf)
{
    int32_t s = (((int32_t)((n>>((uint16_t)bHalf))*pInfo->scale_opp))>>8);

    return (int16_t) (s<<((uint16_t)bHalf));

}
typedef int16_t (*fScaleX)(int16_t n, sScaleInfo* pInfo, bool bHalf);
typedef int16_t (*fScaleY)(int16_t n, sScaleInfo* pInfo, bool bHalf);

static fScaleX pScaleX[2][2] = {{scale_oppX, scale_sameX},
                                {scale_sameX,scale_oppX}};
static fScaleY pScaleY[2][2] = {{scale_oppY, scale_sameY},
                                {scale_sameY_B, scale_oppY}};

bool PredictMVField2(sCoordinate* predA,sCoordinate* predB,sCoordinate* predC,
                     sCoordinate* res, sScaleInfo* pInfo, bool bSecondField,
                     sCoordinate* predAEx,sCoordinate* predCEx, bool bBackward, bool bHalf)
{
    uint8_t n = (uint8_t)(((!predA)<<2) + ((!predB)<<1) + (!predC));
    res[0].bSecond = false;
    res[1].bSecond = true;
    bool  bBackwardFirst = (bBackward && !bSecondField);

    switch (n)
    {
    case 7: //111
        {
            res[0].x=0;
            res[0].y=0;
            res[1].x=0;
            res[1].y=0;

            return false;  // opposite field
        }
    case 6: //110
        {
            bool  bSame = (predC->bSecond);
            res[predC->bSecond].x  =  predC->x;
            res[predC->bSecond].y  =  predC->y;
            predCEx->x = res[!predC->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predC->x,pInfo, bHalf);
            predCEx->y = res[!predC->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predC->y,pInfo, bHalf);

            return predC->bSecond;
        }
    case 5: //101
        {
            bool  bSame = (predB->bSecond);
            res[predB->bSecond].x  =  predB->x;
            res[predB->bSecond].y  =  predB->y;
            res[!predB->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predB->x,pInfo, bHalf);
            res[!predB->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predB->y,pInfo, bHalf);

            return predB->bSecond;
        }
    case 3: //011
        {
            bool  bSame = (predA->bSecond );
            res[predA->bSecond].x  =  predA->x;
            res[predA->bSecond].y  =  predA->y;
            predAEx->x = res[!predA->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predA->x,pInfo, bHalf);
            predAEx->y = res[!predA->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predA->y,pInfo, bHalf);

            return predA->bSecond;
        }
    case 4: //100
        {
            sCoordinate B[2];
            sCoordinate C[2];
            bool  bSame = (predC->bSecond );

            C[predC->bSecond].x  =  predC->x;
            C[predC->bSecond].y  =  predC->y;
            predCEx->x = C[!predC->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predC->x,pInfo, bHalf);
            predCEx->y = C[!predC->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predC->y,pInfo, bHalf);

            bSame = (predB->bSecond );
            B[predB->bSecond].x  =  predB->x;
            B[predB->bSecond].y  =  predB->y;
            B[!predB->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predB->x,pInfo, bHalf);
            B[!predB->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predB->y,pInfo, bHalf);

            res[0].x = median3(C[0].x,B[0].x,0);
            res[1].x = median3(C[1].x,B[1].x,0);
            res[0].y = median3(C[0].y,B[0].y,0);
            res[1].y = median3(C[1].y,B[1].y,0);

            return (predB->bSecond != predC->bSecond)? false:predB->bSecond;
        }
    case 2: //010
        {
            sCoordinate A[2];
            sCoordinate C[2];
            bool  bSame = (predC->bSecond );

            C[predC->bSecond].x  =  predC->x;
            C[predC->bSecond].y  =  predC->y;
            predCEx->x =C[!predC->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predC->x,pInfo, bHalf);
            predCEx->y =C[!predC->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predC->y,pInfo, bHalf);

            bSame = (predA->bSecond );
            A[predA->bSecond].x  =  predA->x;
            A[predA->bSecond].y  =  predA->y;
            predAEx->x = A[!predA->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predA->x,pInfo, bHalf);
            predAEx->y = A[!predA->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predA->y,pInfo, bHalf);

            res[0].x = median3(C[0].x,A[0].x,0);
            res[1].x = median3(C[1].x,A[1].x,0);
            res[0].y = median3(C[0].y,A[0].y,0);
            res[1].y = median3(C[1].y,A[1].y,0);

            return (predA->bSecond != predC->bSecond)? false:predA->bSecond;
        }
    case 1: //001
        {
            sCoordinate A[2];
            sCoordinate B[2];
            bool  bSame = (predB->bSecond );

            B[predB->bSecond].x  =  predB->x;
            B[predB->bSecond].y  =  predB->y;
            B[!predB->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predB->x,pInfo, bHalf);
            B[!predB->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predB->y,pInfo, bHalf);

            bSame = (predA->bSecond);
            A[predA->bSecond].x  =  predA->x;
            A[predA->bSecond].y  =  predA->y;
            predAEx->x = A[!predA->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predA->x,pInfo, bHalf);
            predAEx->y = A[!predA->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predA->y,pInfo, bHalf);

            res[0].x = median3(B[0].x,A[0].x,0);
            res[1].x = median3(B[1].x,A[1].x,0);
            res[0].y = median3(B[0].y,A[0].y,0);
            res[1].y = median3(B[1].y,A[1].y,0);

            return (predA->bSecond != predB->bSecond)? false:predB->bSecond;
        }
    case 0:
        {
            sCoordinate A[2];
            sCoordinate B[2];
            sCoordinate C[2];

            bool  bSame = (predC->bSecond );

            C[predC->bSecond].x  =  predC->x;
            C[predC->bSecond].y  =  predC->y;
            predCEx->x = C[!predC->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predC->x,pInfo, bHalf);
            predCEx->y = C[!predC->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predC->y,pInfo, bHalf);

            bSame = (predB->bSecond );

            B[predB->bSecond].x  =  predB->x;
            B[predB->bSecond].y  =  predB->y;
            B[!predB->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predB->x,pInfo, bHalf);
            B[!predB->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predB->y,pInfo, bHalf);

            bSame = (predA->bSecond );

            A[predA->bSecond].x  =  predA->x;
            A[predA->bSecond].y  =  predA->y;
            predAEx->x =  A[!predA->bSecond].x =  pScaleX[bBackwardFirst][!bSame](predA->x,pInfo, bHalf);
            predAEx->y =  A[!predA->bSecond].y =  pScaleY[bBackwardFirst][!bSame](predA->y,pInfo, bHalf);

            res[0].x = median3(B[0].x,A[0].x,C[0].x);
            res[1].x = median3(B[1].x,A[1].x,C[1].x);
            res[0].y = median3(B[0].y,A[0].y,C[0].y);
            res[1].y = median3(B[1].y,A[1].y,C[1].y);

            return (predA->bSecond + predB->bSecond + predC->bSecond >1)? true:false;
        }
    }
    assert(0);
    return false;
}
void PredictMVField1(sCoordinate* predA,sCoordinate* predB,sCoordinate* predC, sCoordinate* res)
{
    uint8_t n = (uint8_t)(((!predA)<<2) + ((!predB)<<1) + (!predC));
#ifdef VC1_ENC_DEBUG_ON
        pDebug->SetScaleType(2, 0);
        pDebug->SetScaleType(2, 1);
#endif
    switch (n)
    {
    case 7: //111
        {
            res->x=0;
            res->y=0;
            return;
        }
    case 6: //110
        {
            res->x  =  predC->x;
            res->y  =  predC->y;
            return;
        }
    case 5: //101
        {
            res->x  =  predB->x;
            res->y  =  predB->y;
            return;
        }
    case 3: //011
        {
            res->x  =  predA->x;
            res->y  =  predA->y;
            return;
        }
    case 4: //100
        {
            res->x = median3(predC->x,predB->x,0);
            res->y = median3(predC->y,predB->y,0);
            return;
        }
    case 2: //010
        {
            res->x = median3(predC->x,predA->x,0);
            res->y = median3(predC->y,predA->y,0);
            return;
        }
    case 1: //001
        {
            res->x = median3(predB->x,predA->x,0);
            res->y = median3(predB->y,predA->y,0);
            return;
        }
    case 0:
        {
            res->x = median3(predB->x,predA->x,predC->x);
            res->y = median3(predB->y,predA->y,predC->y);
            return;
        }
    }
    assert(0);
    return;
}

void Copy8x8_16s(int16_t*  pSrc, uint32_t   srcStep, int16_t*  pDst, uint32_t   dstStep)
{
   int i,j;
    for (i=0;i<8;i++)
    {
        for (j=0;j<8;j++)
        {
            pDst[j] = pSrc[j];
        }
        pDst = (int16_t*)((uint8_t*)pDst + dstStep);
        pSrc = (int16_t*)((uint8_t*)pSrc + srcStep);
    }
}
//void Copy16x16_16s(int16_t*  pSrc, uint32_t   srcStep, int16_t*  pDst, uint32_t   dstStep)
//{
//   int i,j;
//    for (i=0;i<16;i++)
//    {
//        for (j=0;j<16;j++)
//        {
//            pDst[j] = pSrc[j];
//        }
//        pDst = (int16_t*)((uint8_t*)pDst + dstStep);
//        pSrc = (int16_t*)((uint8_t*)pSrc + srcStep);
//    }
//}

void ScalePredict(sCoordinate * MV, int32_t x,int32_t y,sCoordinate MVPredMin, sCoordinate MVPredMax)
{
    x += MV->x;
    y += MV->y;

    if (x < MVPredMin.x)
    {
        MV->x = MV->x - (int16_t)(x- MVPredMin.x);
    }
    else if (x > MVPredMax.x)
    {
        MV-> x = MV-> x - (int16_t)(x-MVPredMax.x);
    }

    if (y < MVPredMin.y)
    {
        MV->y = MV->y - (int16_t)(y - MVPredMin.y);
    }
    else if (y > MVPredMax.y)
    {
        MV->y = MV->y - (int16_t)(y - MVPredMax.y);
    }
}
uint8_t HybridPrediction(     sCoordinate * mvPred,
                            const sCoordinate * MV,
                            const sCoordinate * mvA,
                            const sCoordinate * mvC,
                            uint32_t        th)
{
    uint8_t hybrid = 0;

    if (mvA && mvC)
    {
        uint32_t sumA = VC1abs(mvA->x - mvPred->x) + VC1abs(mvA->y - mvPred->y);
        uint32_t sumC = VC1abs(mvC->x - mvPred->x) + VC1abs(mvC->y - mvPred->y);

        if (sumA > th || sumC>th)
        {
            if (VC1abs(mvA->x - MV->x) + VC1abs(mvA->y - MV->y)<
                VC1abs(mvC->x - MV->x) + VC1abs(mvC->y - MV->y))
            {
                hybrid = 2;
                mvPred->x = mvA->x;
                mvPred->y = mvA->y;

            }
            else
            {

                hybrid = 1;
                mvPred->x = mvC->x;
                mvPred->y = mvC->y;
            }
        }
    }
    return hybrid;
}



//#define _A(x) (((x)<0)?-(x):(x))
//int32_t FullSearch(const uint8_t* pSrc, uint32_t srcStep, const uint8_t* pPred, uint32_t predStep, sCoordinate Min, sCoordinate Max,sCoordinate * MV)
//{
//    int32_t x, y;
//    int32_t MVx=0, MVy=0;
//    int32_t sum  = 0x7FFFFFFF;
//    int32_t temp = 0;
//    int32_t yStep=1, xStep=1;
//    const uint8_t* prediction = pPred;
//
//
//
//    for (y = Min.y; y <Max.y ; y+=yStep )
//    {
//        pPred = prediction + y*(int32_t)predStep;
//        for (x = Min.x; x < Max.x; x+=xStep )
//        {
//            ippiSAD16x16_8u32s (pSrc, srcStep, pPred+x, predStep, &temp, 0);
//            temp += (VC1abs(x)+VC1abs(y))*16;
//            if (temp < sum)
//            {
//                sum = temp;
//                MVx = x;
//                MVy = y;
//
//            }
//        }
//
//    }
//    MV->x = MVx;
//    MV->y = MVy;
//    return sum;
//}
//int32_t SumBlockDiffBPred16x16(const uint8_t* pSrc, uint32_t srcStep,const uint8_t* pPred1, uint32_t predStep1,
//                                                                const uint8_t* pPred2, uint32_t predStep2)
//{
//    int32_t x, y;
//    int32_t sum = 0;
//    for(y = 0; y <16; y++)
//    {
//        for (x=0; x<16; x++)
//        {
//            sum += VC1abs(pSrc[x]-((pPred1[x]+pPred2[x])>>2));
//        }
//        pPred1 += predStep1;
//        pPred2 += predStep2;
//        pSrc   += srcStep;
//
//    }
//
//    return sum;
//
//}
//int32_t SumBlockDiff16x16(const uint8_t* pSrc1, uint32_t srcStep1,const uint8_t* pSrc2, uint32_t srcStep2)
//{
//    int32_t x, y;
//    int32_t sum = 0;
//    for(y = 0; y <16; y++)
//    {
//        for (x=0; x<16; x++)
//        {
//            sum += VC1abs(pSrc1[x]-pSrc1[x]);
//        }
//        pSrc1+=srcStep1;
//        pSrc2+=srcStep2;
//    }
//    return sum;
//
//}
//void GetBlockType(int16_t* pBlock, int32_t step, uint8_t Quant, eTransformType& type)
//{
//    int i,j;
//    int32_t s[4]={0};
//    bool vEdge[2]={0};
//    bool hEdge[3]={0};
//
//    for (i=0;i<8;i++)
//    {
//        for(j=0;j<8;j++)
//        {
//            s[((i>>2)<<1)+(j>>2)] += pBlock[j];
//        }
//        pBlock = (int16_t*)((uint8_t*)pBlock + step);
//    }
//    s[0] = s[0]>>3;
//    s[1] = s[1]>>3;
//    s[2] = s[2]>>3;
//    s[3] = s[3]>>3;
//
//    vEdge[0] = ((VC1abs(s[0]-s[1]))>= Quant);
//    vEdge[1] = ((VC1abs(s[2]-s[3]))>= Quant);
//    hEdge[0] = ((VC1abs(s[0]-s[2]))>= Quant);
//    hEdge[1] = ((VC1abs(s[1]-s[3]))>= Quant);
//
//    if (vEdge[0]||vEdge[1])
//    {
//            type = (hEdge[0]||hEdge[1])? VC1_ENC_4x4_TRANSFORM:VC1_ENC_4x8_TRANSFORM;
//    }
//    else
//    {
//            type = (hEdge[0]||hEdge[1])? VC1_ENC_8x4_TRANSFORM:VC1_ENC_8x8_TRANSFORM;
//    }
//
//    return;
//}
//bool GetMBTSType(int16_t** ppBlock, uint32_t* pStep, uint8_t Quant /*doubleQuant*/, eTransformType* pTypes /*BlockTSTypes*/)
//{
//    uint8_t num [4]           = {0};
//    uint8_t max_num           = 0;
//    eTransformType max_type = VC1_ENC_8x8_TRANSFORM;
//    int32_t blk;
//
//    for (blk = 0; blk<6; blk++)
//    {
//        GetBlockType(ppBlock[blk],
//                     pStep[blk],
//                     Quant,
//                     (pTypes[blk]));
//        assert(pTypes[blk]<4);
//        num[pTypes[blk]]++;
//        if (num[pTypes[blk]]>max_num)
//        {
//            max_num         = num[pTypes[blk]];
//            max_type        = pTypes[blk];
//        }
//    } // for
//    if (max_num > 4)
//    {
//        for (blk = 0; blk<6; blk++)
//        {
//             pTypes[blk] = max_type;
//        }
//        return true; //bMBTSType - one type for all blocks in macroblock
//    }
//    return false;
//}
//#undef _A
//int32_t SumBlock16x16(const uint8_t* pSrc, uint32_t srcStep)
//{
//    int32_t x, y;
//    int32_t sum = 0;
//    for(y = 0; y <16; y++)
//    {
//        for (x=0; x<16; x++)
//        {
//            sum += pSrc[x];
//        }
//        pSrc += srcStep;
//    }
//    return sum;
//}


void  GetMVDirectHalf  (int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (int16_t)(((x*(scaleFactor-256)+255)>>9)<<1);
     mvB->y = (int16_t)(((y*(scaleFactor-256)+255)>>9)<<1);
     mvF->x = (int16_t)(((x*scaleFactor+255)>>9)<<1);
     mvF->y = (int16_t)(((y*scaleFactor+255)>>9)<<1);

}
void  GetMVDirectQuarter(int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (int16_t)((x*(scaleFactor-256)+128)>>8);
     mvB->y = (int16_t)((y*(scaleFactor-256)+128)>>8);
     mvF->x = (int16_t)((x*scaleFactor+128)>>8);
     mvF->y = (int16_t)((y*scaleFactor+128)>>8);
}
void  GetMVDirectCurrHalfBackHalf (int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (int16_t)(((x<<1)*(scaleFactor-256)+255)>>9);
     mvB->y = (int16_t)(((y<<1)*(scaleFactor-256)+255)>>9);
     mvF->x = (int16_t)(((x<<1)*scaleFactor+255)>>9);
     mvF->y = (int16_t)(((y<<1)*scaleFactor+255)>>9);

}
void  GetMVDirectCurrQuarterBackHalf(int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (int16_t)(((x<<1)*(scaleFactor-256)+128)>>8);
     mvB->y = (int16_t)(((y<<1)*(scaleFactor-256)+128)>>8);
     mvF->x = (int16_t)(((x<<1)*scaleFactor+128)>>8);
     mvF->y = (int16_t)(((y<<1)*scaleFactor+128)>>8);
}
void  GetMVDirectCurrHalfBackQuarter (int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (int16_t)(((x*(scaleFactor-256)+255)>>9)<<1);
     mvB->y = (int16_t)(((y*(scaleFactor-256)+255)>>9)<<1);
     mvF->x = (int16_t)(((x*scaleFactor+255)>>9)<<1);
     mvF->y = (int16_t)(((y*scaleFactor+255)>>9)<<1);

}
void  GetMVDirectCurrQuarterBackQuarter(int16_t x, int16_t y, int32_t scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (int16_t)((x*(scaleFactor-256)+128)>>8);
     mvB->y = (int16_t)((y*(scaleFactor-256)+128)>>8);
     mvF->x = (int16_t)((x*scaleFactor+128)>>8);
     mvF->y = (int16_t)((y*scaleFactor+128)>>8);
}
void GetChromaMV (sCoordinate LumaMV, sCoordinate * pChroma)
{
    static int16_t round[4]= {0,0,0,1};

    pChroma->x = (LumaMV.x + round[LumaMV.x&0x03])>>1;
    pChroma->y = (LumaMV.y + round[LumaMV.y&0x03])>>1;
    pChroma->bSecond = LumaMV.bSecond;
}


void GetChromaMVFast(sCoordinate LumaMV, sCoordinate * pChroma)
{
    static int16_t round [4]= {0,0,0,1};
    static int16_t round1[2][2] = {
        {0, -1}, //sign = 0;
        {0,  1}  //sign = 1
    };

    pChroma->x = (LumaMV.x + round[LumaMV.x&0x03])>>1;
    pChroma->y = (LumaMV.y + round[LumaMV.y&0x03])>>1;
    pChroma->x = pChroma->x + round1[pChroma->x < 0][pChroma->x & 1];
    pChroma->y = pChroma->y + round1[pChroma->y < 0][pChroma->y & 1];
    pChroma->bSecond = LumaMV.bSecond;
}
void GetIntQuarterMV(sCoordinate MV,sCoordinate *pMVInt, sCoordinate *pMVIntQuarter)
{
    pMVInt->x = MV.x>>2;
    pMVInt->y = MV.y>>2;
    pMVIntQuarter->x = MV.x - (pMVInt->x<<2);
    pMVIntQuarter->y = MV.y - (pMVInt->y<<2);
}
//void GetQMV(sCoordinate *pMV,sCoordinate MVInt, sCoordinate MVIntQuarter)
//{
//    pMV->x = (MVInt.x<<2) + MVIntQuarter.x;
//    pMV->y = (MVInt.y<<2) + MVIntQuarter.y;
//}

////////////////////////////////////////////////////////////////
//--------Interpolation----------------------------------------

#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_

#define CLIP(x) (!(x&~255)?x:(x<0?0:255))

static const int BicubicFilterParams[4][4] =
{
    {-4,    53,     18,     -3}, // 1/4
    {-1,    9,      9,      -1}, // 1/2
    {-3,    18,     53,     -4}  // 3/4
};

static const char BicubicVertFilterShift[4][3] =
{
    {6, 4, 6},
    {5, 3, 5},
    {3, 1, 3},
    {5, 3, 5}
};

static const char BicubicHorizFilterShift[3][4] =
{
    {6, 7, 7, 7},
    {4, 7, 7, 7},
    {6, 7, 7, 7}
};



typedef IppStatus (*_own_ippiBicubicInterpolate)     (const uint8_t* pSrc, int32_t srcStep,
                   uint8_t *pDst, int32_t dstStep, int32_t dx, int32_t dy, int32_t roundControl);

typedef IppStatus (*_own_ippiBilinearInterpolate)     (const uint8_t* pSrc, int32_t srcStep,
                   uint8_t *pDst, int32_t dstStep, int32_t dx, int32_t dy, int32_t roundControl);

static IppStatus _own_ippiInterpolate16x16QPBicubic_VC1_8u_C1R (const uint8_t* pSrc,
                                                      int32_t srcStep,
                                                      uint8_t *pDst,
                                                      int32_t dstStep,
                                                      int32_t dx,
                                                      int32_t dy,
                                                      int32_t roundControl)
{
    IppStatus ret = ippStsNoErr;
    unsigned choose_int = ( (1==(0==(dx))) |(((1==(0 == (dy))) << 1)));

    short TempBlock[(17 + 3) * 17];
    int i, j;
    int PBPL          = 17 + 3;
    int R             = roundControl;

    short         *pDst16 = TempBlock + 1;
    short         *pSource16 = TempBlock + 1;
    int Pixel;
    int Fy0, Fy1, Fy2, Fy3;
    int Fx0, Fx1, Fx2, Fx3;

    const int (*FP) = BicubicFilterParams[(dy) - 1];
    int Abs, Shift;
    Fy0          = FP[0];
    Fy1          = FP[1];
    Fy2          = FP[2];
    Fy3          = FP[3];
    FP = BicubicFilterParams[dx - 1];
    Fx0          = FP[0];
    Fx1          = FP[1];
    Fx2          = FP[2];
    Fx3          = FP[3];

    switch(choose_int)
    {
    case 0:

        Shift           = BicubicVertFilterShift[(dx)][(dy) - 1];
        Abs             = 1 << (Shift - 1);
#ifdef VC1_ENC_DEBUG_ON
       // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("PelBicubicDiag\n") );
       // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d\t %d\t %d\n"),*pSource,X>>2,Y>>2);
       // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Source\n" ));
       // for(j = -1; j < 16+2; j++)
       // {
       //   for(i = -1; i < 16+2; i++)
       //   {
       //       VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d "), pSrc[i  + j*srcStep]);
       //   }
       //   VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n"));
       // }
       // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("F(%d, %d, %d, %d) abs=%d r=%d shift=%d\n") ,Fy0,Fy1,Fy2,Fy3,Abs,R,Shift);
       // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("after interpolate\n") );
       // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n"));
#endif

        /* vertical filter */
        for(j = 0; j < 16; j++)
        {
            for(i = -1; i < 3; i++)
            {
                Pixel = ((pSrc[i - srcStep] * Fy0 +
                    pSrc[i]             * Fy1 +
                    pSrc[i + srcStep]  * Fy2 +
                    pSrc[i + 2*srcStep]* Fy3 +
                    Abs - 1 + R) >> Shift);

                pDst16[i] = (short)Pixel;

            }
            for(;i < 16+2; i++)
            {
                Pixel = ((pSrc[i - srcStep]  * Fy0 +
                    pSrc[i]              * Fy1 +
                    pSrc[i  + srcStep]  * Fy2 +
                    pSrc[i  + 2*srcStep]* Fy3 +
                    Abs - 1 + R) >> Shift);
                pDst16[i] = (short)Pixel;
                pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                    pDst16[i-3] * Fx1 +
                    pDst16[i-2] * Fx2 +
                    pDst16[i-1] * Fx3 + 64 - R) >> 7);
#ifdef VC1_ENC_DEBUG_ON
//   VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i-3] );
#endif
            }
            pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                pDst16[i-3] * Fx1 +
                pDst16[i-2] * Fx2 +
                pDst16[i-1] * Fx3 + 64 - R) >> 7);
#ifdef VC1_ENC_DEBUG_ON
  //    VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i-3] );
#endif
            pDst   += dstStep;
            pSrc   += srcStep;
            pDst16 += PBPL;
#ifdef VC1_ENC_DEBUG_ON
    //     VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
#endif
        }

        break;
    case 1:

        Shift           = BicubicVertFilterShift[0][(dy) - 1];
        Abs             = 1 << (Shift - 1);
#ifdef VC1_ENC_DEBUG_ON
//      VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("PelBicubicVert\n") );
#endif


        for(j = 0; j < 16; j++)
        {
            for(i = 0; i < 16; i++)
            {

                pDst[i] = CLIP((pSrc[i - srcStep]   * Fy0 +
                    pSrc[i]             * Fy1 +
                    pSrc[i + srcStep]   * Fy2 +
                    pSrc[i + 2*srcStep] * Fy3 +
                    Abs - 1 + R) >> Shift);
#ifdef VC1_ENC_DEBUG_ON
    //    VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i] );
#endif
            }
            pSrc += srcStep;
            pDst += dstStep;
#ifdef VC1_ENC_DEBUG_ON
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
#endif
        }
        break;

    case 2:
        Shift           = BicubicHorizFilterShift[(dx) - 1][0];
        Abs             = 1 << (Shift - 1);
#ifdef VC1_ENC_DEBUG_ON
 //       VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("PelBicubicHoriz\n") );
#endif

        for(j = 0; j < 16; j++)
        {
            for(i = 0; i < 16; i++)
            {
                pDst[i] = CLIP((pSrc[i-1]   * Fx0 +
                    pSrc[i]     * Fx1 +
                    pSrc[i+1]   * Fx2 +
                    pSrc[i + 2] * Fx3 +
                    Abs - R) >> Shift);
#ifdef VC1_ENC_DEBUG_ON
//   VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i] );
#endif
            }
            pSrc += srcStep;
            pDst += dstStep;
            //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
        }
        break;
    case 3:
        for(j = 0; j < 16; j++)
        {
            for(i = 0; i < 16; i++)
            {
                pDst[i] = pSrc[i];
#ifdef VC1_ENC_DEBUG_ON
  //              VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i] );
#endif

            }
#ifdef VC1_ENC_DEBUG_ON
//            VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
#endif
            pSrc += srcStep;
            pDst += dstStep;
        }
        break;
    default:
        break;
    }
    return ret;
}

static IppStatus _own_ippiInterpolate16x8QPBicubic_VC1_8u_C1R (const uint8_t* pSrc,
                                                      int32_t srcStep,
                                                      uint8_t *pDst,
                                                      int32_t dstStep,
                                                      int32_t dx,
                                                      int32_t dy,
                                                      int32_t roundControl)
{
    IppStatus ret = ippStsNoErr;
    unsigned choose_int = ( (1==(0==(dx))) |(((1==(0 == (dy))) << 1)));
    static unsigned inter_flag = 0;

    short TempBlock[(17 + 3) * 17];
    int i, j;
    int PBPL          = 17 + 3;
    int R             = roundControl;

    short         *pDst16 = TempBlock + 1;
    short         *pSource16 = TempBlock + 1;
    int Pixel;
    int Fy0, Fy1, Fy2, Fy3;
    int Fx0, Fx1, Fx2, Fx3;
    const int (*FP) = BicubicFilterParams[(dy) - 1];
    int Abs, Shift;
    Fy0          = FP[0];
    Fy1          = FP[1];
    Fy2          = FP[2];
    Fy3          = FP[3];

    FP = BicubicFilterParams[dx - 1];
    Fx0          =  FP[0];
    Fx1          =  FP[1];
    Fx2          =  FP[2];
    Fx3          =  FP[3];

    switch(choose_int)
    {
    case 0:

        Shift           = BicubicVertFilterShift[(dx)][(dy) - 1];
        Abs             = 1 << (Shift - 1);
#ifdef VC1_ENC_DEBUG_ON
//       VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("PelBicubicDiag\n") );
//       VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d\t %d\t %d\n"),*pSource,X>>2,Y>>2);
        //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Source\n" ));
//        for(j = -1; j < 8+2; j++)
//        {
//            for(i = -1; i < 16+2; i++)
            //{
//                VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d "), pSrc[i  + j*srcStep]);
//            }
//            VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n"));
//        }
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("F(%d, %d, %d, %d) abs=%d r=%d shift=%d\n") ,Fy0,Fy1,Fy2,Fy3,Abs,R,Shift);
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("after interpolate\n") );
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n"));
#endif

        /* vertical filter */

            for(j = 0; j < 8; j++)
            {
                for(i = -1; i < 3; i++)
                {
                    Pixel = ((pSrc[i - srcStep] * Fy0 +
                        pSrc[i]             * Fy1 +
                        pSrc[i + srcStep]  * Fy2 +
                        pSrc[i + 2*srcStep]* Fy3 +
                        Abs - 1 + R) >> Shift);

                    pDst16[i] = (short)Pixel;

                }
                for(;i < 16+2; i++)
                {
                    Pixel = ((pSrc[i - srcStep]  * Fy0 +
                        pSrc[i]              * Fy1 +
                        pSrc[i  + srcStep]  * Fy2 +
                        pSrc[i  + 2*srcStep]* Fy3 +
                        Abs - 1 + R) >> Shift);
                    pDst16[i] = (short)Pixel;
                    pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                        pDst16[i-3] * Fx1 +
                        pDst16[i-2] * Fx2 +
                        pDst16[i-1] * Fx3 + 64 - R) >> 7);
#ifdef VC1_ENC_DEBUG_ON
//                    VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i-3] );
#endif
                }
                pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                    pDst16[i-3] * Fx1 +
                    pDst16[i-2] * Fx2 +
                    pDst16[i-1] * Fx3 + 64 - R) >> 7);
#ifdef VC1_ENC_DEBUG_ON
 //               VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i-3] );
#endif
                pDst   += dstStep;
                pSrc   += srcStep;
                pDst16 += PBPL;
#ifdef VC1_ENC_DEBUG_ON
  //              VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
#endif
            }
        break;
    case 1:

        Shift           = BicubicVertFilterShift[0][(dy) - 1];
        Abs             = 1 << (Shift - 1);
#ifdef VC1_ENC_DEBUG_ON
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("PelBicubicVert\n") );
#endif


            for(j = 0; j < 8; j++)
            {
                for(i = 0; i < 16; i++)
                {

                    pDst[i] = CLIP((pSrc[i - srcStep]   * Fy0 +
                                    pSrc[i]             * Fy1 +
                                    pSrc[i + srcStep]   * Fy2 +
                                    pSrc[i + 2*srcStep] * Fy3 +
                                    Abs - 1 + R) >> Shift);
#ifdef VC1_ENC_DEBUG_ON
//                    VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i] );
#endif
                }
                pSrc += srcStep;
                pDst += dstStep;
#ifdef VC1_ENC_DEBUG_ON
                //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
#endif
            }

        break;
    case 2:
        Shift           = BicubicHorizFilterShift[(dx) - 1][0];
        Abs             = 1 << (Shift - 1);
#ifdef VC1_ENC_DEBUG_ON
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("PelBicubicHoriz\n") );
#endif


            for(j = 0; j < 8; j++)
            {
                for(i = 0; i < 16; i++)
                {
                  pDst[i] = CLIP((pSrc[i-1]   * Fx0 +
                        pSrc[i]     * Fx1 +
                        pSrc[i+1]   * Fx2 +
                        pSrc[i + 2] * Fx3 +
                        Abs - R) >> Shift);
#ifdef VC1_ENC_DEBUG_ON
//                    VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i] );
#endif
                }
                pSrc += srcStep;
                pDst += dstStep;
#ifdef VC1_ENC_DEBUG_ON
//                VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
#endif
            }
    break;

    case 3:

            for(j = 0; j < 8; j++)
            {
                for(i = 0; i < 16; i++)
                {
                    //pPixels[I] = IC_SCAN(pSource[I], IC_matrix);
                    pDst[i] = pSrc[i];
#ifdef VC1_ENC_DEBUG_ON
//                    VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i] );
#endif

                }
#ifdef VC1_ENC_DEBUG_ON
//                VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
#endif
                pSrc += srcStep;
                pDst += dstStep;
            }
        break;
    default:
        break;
    }
    return ret;
}

static IppStatus _own_ippiInterpolate8x8QPBicubic_VC1_8u_C1R (const uint8_t* pSrc,
                                                      int32_t srcStep,
                                                      uint8_t *pDst,
                                                      int32_t dstStep,
                                                      int32_t dx,
                                                      int32_t dy,
                                                      int32_t roundControl)
{
    IppStatus ret = ippStsNoErr;

    unsigned choose_int = ( (1==(0==(dx))) |(((1==(0 == (dy))) << 1)));
    short TempBlock[(17 + 3) * 17];
    int i, j;
    int PBPL          = 17 + 3;
    int R             = roundControl;

    short         *pDst16 = TempBlock + 1;
    short         *pSource16 = TempBlock + 1;
    int Pixel;
    int Fy0, Fy1, Fy2, Fy3;
    int Fx0, Fx1, Fx2, Fx3;
    const int (*FP) = BicubicFilterParams[(dy) - 1];
    int Abs, Shift;
    Fy0          = FP[0];
    Fy1          = FP[1];
    Fy2          = FP[2];
    Fy3          = FP[3];

    FP = BicubicFilterParams[dx - 1];
    Fx0          = FP[0];
    Fx1          = FP[1];
    Fx2          = FP[2];
    Fx3          = FP[3];

    switch(choose_int)
    {
    case 0:

        Shift           = BicubicVertFilterShift[(dx)][(dy) - 1];
        Abs             = 1 << (Shift - 1);
#ifdef VC1_ENC_DEBUG_ON
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("PelBicubicDiag\n") );
#endif
        FP = BicubicFilterParams[dx - 1];
        Fx0          = (int)FP[0];
        Fx1          = (int)FP[1];
        Fx2          = (int)FP[2];
        Fx3          = (int)FP[3];

#ifdef VC1_ENC_DEBUG_ON
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Source\n" ));
//        for(j = -1; j < 8+2; j++)
//        {
//            for(i = -1; i < 8+2; i++)
//            {
//                VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d "), pSrc[i  + j*srcStep]);
//            }
//            VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n"));
//        }
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("F(%d, %d, %d, %d) abs=%d r=%d shift=%d\n") ,Fy0,Fy1,Fy2,Fy3,Abs,R,Shift);
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("after interpolate\n") );
//        VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n"));
#endif

        /* vertical filter */

            for(j = 0; j < 8; j++)
            {
                for(i = -1; i < 3; i++)
                {
                    Pixel = ((pSrc[i - srcStep] * Fy0 +
                        pSrc[i]             * Fy1 +
                        pSrc[i + srcStep]  * Fy2 +
                        pSrc[i + 2*srcStep]* Fy3 +
                        Abs - 1 + R) >> Shift);

                    pDst16[i] = (short)Pixel;

                }
                for(;i < 8+2; i++)
                {
                    Pixel = ((pSrc[i - srcStep]  * Fy0 +
                        pSrc[i]              * Fy1 +
                        pSrc[i  + srcStep]  * Fy2 +
                        pSrc[i  + 2*srcStep]* Fy3 +
                        Abs - 1 + R) >> Shift);
                    pDst16[i] = (short)Pixel;
                    pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                        pDst16[i-3] * Fx1 +
                        pDst16[i-2] * Fx2 +
                        pDst16[i-1] * Fx3 + 64 - R) >> 7);
#ifdef VC1_ENC_DEBUG_ON
//                    VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i-3] );
#endif
                }
                pDst[i-3] = CLIP((pDst16[i-4] * Fx0 +
                    pDst16[i-3] * Fx1 +
                    pDst16[i-2] * Fx2 +
                    pDst16[i-1] * Fx3 + 64 - R) >> 7);
#ifdef VC1_ENC_DEBUG_ON
 //               VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i-3] );
#endif
                pDst   += dstStep;
                pSrc   += srcStep;
                pDst16 += PBPL;
#ifdef VC1_ENC_DEBUG_ON
//                VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
#endif
            }
        break;
    case 1:

        Shift           = BicubicVertFilterShift[0][(dy) - 1];
        Abs             = 1 << (Shift - 1);
#ifdef VC1_ENC_DEBUG_ON
 //       VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("PelBicubicVert\n") );
#endif


            for(j = 0; j < 8; j++)
            {
                for(i = 0; i < 8; i++)
                {
                    pDst[i] = CLIP((pSrc[i - srcStep]   * Fy0 +
                                    pSrc[i]             * Fy1 +
                                    pSrc[i + srcStep]   * Fy2 +
                                    pSrc[i + 2*srcStep] * Fy3 +
                                    Abs - 1 + R) >> Shift);
                    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Source\n"));
                    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  \n"), pSrc[i - srcStep] );
                    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  \n"), pSrc[i] );
                    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  \n"), pSrc[i + srcStep] );
                    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  \n"), pSrc[i + 2*srcStep] );

                    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  \n"), pDst[i] );
                }
                pSrc += srcStep;
                pDst += dstStep;
#ifdef VC1_ENC_DEBUG_ON
  //              VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
#endif
            }
        break;
    case 2:
        Shift           = BicubicHorizFilterShift[(dx) - 1][0];
        Abs             = 1 << (Shift - 1);
#ifdef VC1_ENC_DEBUG_ON
 //       VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("PelBicubicHoriz\n") );
#endif

            for(j = 0; j < 8; j++)
            {
                for(i = 0; i < 8; i++)
                {
                     pDst[i] = CLIP((pSrc[i-1]   * Fx0 +
                        pSrc[i]     * Fx1 +
                        pSrc[i+1]   * Fx2 +
                        pSrc[i + 2] * Fx3 +
                        Abs - R) >> Shift);
#ifdef VC1_ENC_DEBUG_ON
  //                  VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i] );
#endif
                }
                pSrc += srcStep;
                pDst += dstStep;
#ifdef VC1_ENC_DEBUG_ON
 //               VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
#endif
            }
        break;
    case 3:

            for(j = 0; j < 8; j++)
            {
                for(i = 0; i < 8; i++)
                {
                    //pPixels[I] = IC_SCAN(pSource[I], IC_matrix);
                    pDst[i] = pSrc[i];
#ifdef VC1_ENC_DEBUG_ON
  //                  VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d  "), pDst[i] );
#endif

                }
                //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n") );
                pSrc += srcStep;
                pDst += dstStep;
            }
        break;
    default:
        break;
    }
    return ret;
}


static const _own_ippiBicubicInterpolate _own_ippiBicubicInterpolate_table[] = {
        (_own_ippiBicubicInterpolate)(_own_ippiInterpolate8x8QPBicubic_VC1_8u_C1R),
        (_own_ippiBicubicInterpolate)(_own_ippiInterpolate16x8QPBicubic_VC1_8u_C1R),
        (_own_ippiBicubicInterpolate)(_own_ippiInterpolate16x16QPBicubic_VC1_8u_C1R),
};


IppStatus _own_ippiInterpolateQPBicubic_VC1_8u_C1R   (const IppVCInterpolate_8u* inter_struct)
{
    IppStatus ret = ippStsNoErr;
    int pinterp = (inter_struct->roiSize.width >> 4) + (inter_struct->roiSize.height >> 4);
    ret= _own_ippiBicubicInterpolate_table[pinterp](inter_struct->pSrc,
                                               inter_struct->srcStep,
                                               inter_struct->pDst,
                                               inter_struct->dstStep,
                                               inter_struct->dx,
                                               inter_struct->dy,
                                               inter_struct->roundControl);
    return ret;
}

static IppStatus _own_ippiInterpolate16x16QPBilinear_VC1_8u_C1R (const uint8_t* pSrc,
                                                       int32_t srcStep,
                                                       uint8_t *pDst,
                                                       int32_t dstStep,
                                                       int32_t dx,
                                                       int32_t dy,
                                                       int32_t roundControl)
{
    IppStatus ret = ippStsNoErr;
    int32_t i, j;
    const int32_t F[4] = {4, 3, 2, 1};
    const int32_t G[4] = {0, 1, 2, 3};

    int32_t Mult1 = F[dx]*F[dy];
    int32_t Mult2 = F[dx]*G[dy];
    int32_t Mult3 = F[dy]*G[dx];
    int32_t Mult4 = G[dx]*G[dy];


    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d\t %d\t %d\n"),*pSource,X>>2,Y>>2);
   // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Predicted pels\n"));

        for(j = 0; j < 16; j++)
        {
            for(i = 0; i < 16; i++)
            {
                pDst[i] = (uint8_t)((pSrc[i]           * Mult1 +
                                           pSrc[i+srcStep]   * Mult2 +
                                           pSrc[i+1]         * Mult3 +
                                           pSrc[i+srcStep+1] * Mult4 +
                                           8 - roundControl) >> 4);
                //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d "),pDst[i]);
            }
            //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n"));
            pSrc += srcStep;
            pDst += dstStep;
        }
    return ret;
}

static IppStatus _own_ippiInterpolate8x8QPBilinear_VC1_8u_C1R   (const uint8_t* pSrc,
                                                       int32_t srcStep,
                                                       uint8_t *pDst,
                                                       int32_t dstStep,
                                                       int32_t dx,
                                                       int32_t dy,
                                                       int32_t roundControl)
{
    IppStatus ret = ippStsNoErr;
    int32_t i, j;
    const int32_t F[4] = {4, 3, 2, 1};
    const int32_t G[4] = {0, 1, 2, 3};

    int32_t Mult1 = F[dx]*F[dy];
    int32_t Mult2 = F[dx]*G[dy];
    int32_t Mult3 = F[dy]*G[dx];
    int32_t Mult4 = G[dx]*G[dy];

    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d\t %d\t %d\n"),*pSource,X>>2,Y>>2);
   // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Predicted pels\n"));

        for(j = 0; j < 8; j++)
        {
            for(i = 0; i < 8; i++)
            {
                pDst[i] = (uint8_t)((pSrc[i]           * Mult1 +
                                           pSrc[i+srcStep]   * Mult2 +
                                           pSrc[i+1]         * Mult3 +
                                           pSrc[i+srcStep+1] * Mult4 +
                                           8 - roundControl) >> 4);
             //   VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d "),pDst[i]);
            }
            //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n"));
            pSrc += srcStep;
            pDst += dstStep;
        }
    return ret;
}



static IppStatus _own_ippiInterpolate8x4QPBilinear_VC1_8u_C1R   (const uint8_t* pSrc,
                                                       int32_t srcStep,
                                                       uint8_t *pDst,
                                                       int32_t dstStep,
                                                       int32_t dx,
                                                       int32_t dy,
                                                       int32_t roundControl)
{
    IppStatus ret = ippStsNoErr;
    int32_t i, j;
    const int32_t F[4] = {4, 3, 2, 1};
    const int32_t G[4] = {0, 1, 2, 3};

    int32_t Mult1 = F[dx]*F[dy];
    int32_t Mult2 = F[dx]*G[dy];
    int32_t Mult3 = F[dy]*G[dx];
    int32_t Mult4 = G[dx]*G[dy];

    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d\t %d\t %d\n"),*pSource,X>>2,Y>>2);
   // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Predicted pels\n"));

        for(j = 0; j < 4; j++)
        {
            for(i = 0; i < 8; i++)
            {

                pDst[i] = (uint8_t)((pSrc[i]           * Mult1 +
                                           pSrc[i+srcStep]   * Mult2 +
                                           pSrc[i+1]         * Mult3 +
                                           pSrc[i+srcStep+1] * Mult4 +
                                           8 - roundControl) >> 4);
                //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d "),pDst[i]);
            }
            //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n"));
            pSrc += srcStep;
            pDst += dstStep;
        }
    return ret;
}


static IppStatus _own_ippiInterpolate4x4QPBilinear_VC1_8u_C1R   (const uint8_t* pSrc,
                                                       int32_t srcStep,
                                                       uint8_t *pDst,
                                                       int32_t dstStep,
                                                       int32_t dx,
                                                       int32_t dy,
                                                       int32_t roundControl)
{
    IppStatus ret = ippStsNoErr;
    int32_t i, j;
    const int32_t F[4] = {4, 3, 2, 1};
    const int32_t G[4] = {0, 1, 2, 3};
    int32_t Mult1 = F[dx]*F[dy];
    int32_t Mult2 = F[dx]*G[dy];
    int32_t Mult3 = F[dy]*G[dx];
    int32_t Mult4 = G[dx]*G[dy];

    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d\t %d\t %d\n"),*pSource,X>>2,Y>>2);
   // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Predicted pels\n"));

        for(j = 0; j < 4; j++)
        {
            for(i = 0; i < 4; i++)
            {
                 pDst[i] = (uint8_t)((pSrc[i]           * Mult1 +
                                           pSrc[i+srcStep]   * Mult2 +
                                           pSrc[i+1]         * Mult3 +
                                           pSrc[i+srcStep+1] * Mult4 +
                                           8 - roundControl) >> 4);
                //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d "),pDst[i]);
            }
            //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("\n"));
            pSrc += srcStep;
            pDst += dstStep;
        }
    return ret;
}

static const _own_ippiBilinearInterpolate _own_ippiBilinearInterpolate_table[] = {
        (_own_ippiBilinearInterpolate)(_own_ippiInterpolate4x4QPBilinear_VC1_8u_C1R ),
        (_own_ippiBilinearInterpolate)(_own_ippiInterpolate8x4QPBilinear_VC1_8u_C1R),
        (_own_ippiBilinearInterpolate)(_own_ippiInterpolate8x8QPBilinear_VC1_8u_C1R),
        (_own_ippiBilinearInterpolate)(NULL),
        (_own_ippiBilinearInterpolate)(_own_ippiInterpolate16x16QPBilinear_VC1_8u_C1R),
};


IppStatus _own_ippiInterpolateQPBilinear_VC1_8u_C1R  (const IppVCInterpolate_8u* inter_struct)
{
    IppStatus ret = ippStsNoErr;
    int32_t pinterp = (inter_struct->roiSize.width >> 3) + (inter_struct->roiSize.height >> 3);

    ret= _own_ippiBilinearInterpolate_table[pinterp](inter_struct->pSrc,
                                               inter_struct->srcStep,
                                               inter_struct->pDst,
                                               inter_struct->dstStep,
                                               inter_struct->dx,
                                               inter_struct->dy,
                                               inter_struct->roundControl);
    return ret;
}


#endif
#endif


static IppStatus _own_ippiInterpolate8x8QPBilinear_VC1_8u_C1R_NV12   (const uint8_t* pSrc, int32_t srcStep,
                                                                      uint8_t *pDst,       int32_t dstStep,
                                                                      int32_t dx,         int32_t dy,
                                                                      int32_t roundControl)
{
    IppStatus ret = ippStsNoErr;
    int32_t i, j;
    const int32_t F[4] = {4, 3, 2, 1};
    const int32_t G[4] = {0, 1, 2, 3};

    int32_t Mult1 = F[dx]*F[dy];
    int32_t Mult2 = F[dx]*G[dy];
    int32_t Mult3 = F[dy]*G[dx];
    int32_t Mult4 = G[dx]*G[dy];

    for(j = 0; j < 8; j++)
    {
        for(i = 0; i < 16; i += 2)
        {
            pDst[i] = (uint8_t)((pSrc[i]               * Mult1 +
                               pSrc[i + srcStep]     * Mult2 +
                               pSrc[i + 2]           * Mult3 +
                               pSrc[i + srcStep + 2] * Mult4 +
                               8 - roundControl) >> 4);

            pDst[i + 1] = (uint8_t)((pSrc[i + 1]               * Mult1 +
                                   pSrc[i + 1 + srcStep]     * Mult2 +
                                   pSrc[i + 1 + 2]           * Mult3 +
                                   pSrc[i + 1 +srcStep + 2] * Mult4 +
                                   8 - roundControl) >> 4);
        }

        pSrc += srcStep;
        pDst += dstStep;
    }
    return ret;
}


static IppStatus _own_ippiInterpolate8x4QPBilinear_VC1_8u_C1R_NV12   (const uint8_t* pSrc, int32_t srcStep,
                                                                      uint8_t *pDst,       int32_t dstStep,
                                                                      int32_t dx,         int32_t dy,
                                                                      int32_t roundControl)
{
    IppStatus ret = ippStsNoErr;
    int32_t i, j;
    const int32_t F[4] = {4, 3, 2, 1};
    const int32_t G[4] = {0, 1, 2, 3};

    int32_t Mult1 = F[dx]*F[dy];
    int32_t Mult2 = F[dx]*G[dy];
    int32_t Mult3 = F[dy]*G[dx];
    int32_t Mult4 = G[dx]*G[dy];

    for(j = 0; j < 4; j++)
    {
        for(i = 0; i < 16; i += 2)
        {

            pDst[i] = (uint8_t)((pSrc[i]               * Mult1 +
                               pSrc[i + srcStep]     * Mult2 +
                               pSrc[i + 2]           * Mult3 +
                               pSrc[i + srcStep + 2] * Mult4 +
                               8 - roundControl) >> 4);

            pDst[i + 1] = (uint8_t)((pSrc[i + 1]               * Mult1 +
                                   pSrc[i + 1 + srcStep]     * Mult2 +
                                   pSrc[i + 1 + 2]           * Mult3 +
                                   pSrc[i + 1 + srcStep + 2] * Mult4 +
                                   8 - roundControl) >> 4);
        }

        pSrc += srcStep;
        pDst += dstStep;
    }
    return ret;
}


static IppStatus _own_ippiInterpolate4x4QPBilinear_VC1_8u_C1R_NV12   (const uint8_t* pSrc, int32_t srcStep,
                                                                      uint8_t *pDst,       int32_t dstStep,
                                                                      int32_t dx,         int32_t dy,
                                                                      int32_t roundControl)
{
    IppStatus ret = ippStsNoErr;
    int32_t i, j;
    const int32_t F[4] = {4, 3, 2, 1};
    const int32_t G[4] = {0, 1, 2, 3};
    int32_t Mult1 = F[dx]*F[dy];
    int32_t Mult2 = F[dx]*G[dy];
    int32_t Mult3 = F[dy]*G[dx];
    int32_t Mult4 = G[dx]*G[dy];

    for(j = 0; j < 4; j++)
    {
        for(i = 0; i < 8; i += 2)
        {
            pDst[i] = (uint8_t)((pSrc[i]           * Mult1 +
                pSrc[i+srcStep]   * Mult2 +
                pSrc[i+2]         * Mult3 +
                pSrc[i+srcStep+2] * Mult4 +
                8 - roundControl) >> 4);

            pDst[i] = (uint8_t)((pSrc[i + 1]           * Mult1 +
                pSrc[i+1+srcStep]   * Mult2 +
                pSrc[i+1+2]         * Mult3 +
                pSrc[i+1+srcStep+2] * Mult4 +
                8 - roundControl) >> 4);
        }

        pSrc += srcStep;
        pDst += dstStep;
    }
    return ret;
}

typedef IppStatus (*_own_ippiBilinearInterpolate)     (const uint8_t* pSrc, int32_t srcStep,
                   uint8_t *pDst, int32_t dstStep, int32_t dx, int32_t dy, int32_t roundControl);

static const _own_ippiBilinearInterpolate _own_ippiBilinearInterpolate_table_NV12[] = {
        (_own_ippiBilinearInterpolate)(_own_ippiInterpolate4x4QPBilinear_VC1_8u_C1R_NV12 ),
        (_own_ippiBilinearInterpolate)(_own_ippiInterpolate8x4QPBilinear_VC1_8u_C1R_NV12),
        (_own_ippiBilinearInterpolate)(_own_ippiInterpolate8x8QPBilinear_VC1_8u_C1R_NV12),
        (_own_ippiBilinearInterpolate)(NULL),
        (_own_ippiBilinearInterpolate)(NULL),
};

IppStatus _own_ippiInterpolateQPBilinear_VC1_8u_C1R_NV12  (const IppVCInterpolate_8u* inter_struct)
{
    IppStatus ret = ippStsNoErr;
    int32_t pinterp = (inter_struct->roiSize.width >> 3) + (inter_struct->roiSize.height >> 3);

    if(pinterp >2)
        return ippStsBadArgErr;

    ret= _own_ippiBilinearInterpolate_table_NV12[pinterp](inter_struct->pSrc,
                                               inter_struct->srcStep,
                                               inter_struct->pDst,
                                               inter_struct->dstStep,
                                               inter_struct->dx,
                                               inter_struct->dy,
                                               inter_struct->roundControl);
    return ret;
}


////////////////////////////////////////////////////////////////
//--------Inverse transform----------------------------------------

#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_

static IppStatus own_ippiTransform8x8Inv_VC1_16s_C1R(const int16_t  *pSrc, int srcStep,
                                                      int16_t  *pDst, int dstStep, mfxSize srcSizeNZ)
{
    if (srcSizeNZ.width <= 1 && srcSizeNZ.height <= 1) {
        int     i;
        int16_t  c = (int16_t)((((pSrc[0] * 12 + 4) >> 3) * 12 + 64) >> 7);

        for (i = 0; i < 8; i ++) {
            pDst[0] = pDst[1] = pDst[2] = pDst[3] = pDst[4] = pDst[5] = pDst[6] = pDst[7] = c;
            pDst = (int16_t*)((uint8_t*)pDst+dstStep);
        }
    } else if (srcSizeNZ.width <= 2 && srcSizeNZ.height <= 2) {
        int     i, buff[16], *pBuff = buff;

        for (i = 0; i < 2; i ++) {
            int a0, u1, u3, v1, v3;

            a0 = pSrc[0] * 12 + 4;
            u1 = pSrc[1] * 16;
            u3 = pSrc[1] * 4;
            v1 = pSrc[1] * 15;
            v3 = pSrc[1] * 9;
            pBuff[0] = (a0 + u1) >> 3;
            pBuff[7] = (a0 - u1) >> 3;
            pBuff[3] = (a0 + u3) >> 3;
            pBuff[4] = (a0 - u3) >> 3;
            pBuff[1] = (a0 + v1) >> 3;
            pBuff[6] = (a0 - v1) >> 3;
            pBuff[2] = (a0 + v3) >> 3;
            pBuff[5] = (a0 - v3) >> 3;
            pSrc = (int16_t*)((uint8_t*)pSrc + srcStep);
            pBuff += 8;
        }
        pBuff -= 16;
        for (i = 0; i < 8; i ++) {
            int a0, u1, u3, v1, v3;

            a0 = pBuff[0*8] * 12 + 64;
            u1 = pBuff[1*8] * 16;
            u3 = pBuff[1*8] * 4;
            v1 = pBuff[1*8] * 15;
            v3 = pBuff[1*8] * 9;
            *(int16_t*)((uint8_t*)pDst+0*dstStep) = (int16_t)((a0 + u1) >> 7);
            *(int16_t*)((uint8_t*)pDst+7*dstStep) = (int16_t)((a0 - u1 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+3*dstStep) = (int16_t)((a0 + u3) >> 7);
            *(int16_t*)((uint8_t*)pDst+4*dstStep) = (int16_t)((a0 - u3 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+1*dstStep) = (int16_t)((a0 + v1) >> 7);
            *(int16_t*)((uint8_t*)pDst+6*dstStep) = (int16_t)((a0 - v1 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+2*dstStep) = (int16_t)((a0 + v3) >> 7);
            *(int16_t*)((uint8_t*)pDst+5*dstStep) = (int16_t)((a0 - v3 + 1) >> 7);
            pDst ++;
            pBuff ++;
        }
    } else if (srcSizeNZ.width <= 4 && srcSizeNZ.height <= 4) {
        int     i, buff[32], *pBuff = buff;

        for (i = 0; i < 4; i ++) {
            int a0, a2, s2, u0, u1, u2, u3, v0, v1, v2, v3;

            a0 = pSrc[0] * 12 + 4;
            a2 = pSrc[2] * 16;
            u0 = a0 + a2;
            u1 = pSrc[1] * 16 + pSrc[3] * 15;
            u2 = a0 - a2;
            u3 = pSrc[1] * 4 - pSrc[3] * 9;
            pBuff[0] = (u0 + u1) >> 3;
            pBuff[7] = (u0 - u1) >> 3;
            pBuff[3] = (u2 + u3) >> 3;
            pBuff[4] = (u2 - u3) >> 3;
            s2 = pSrc[2] * 6;
            v0 = a0 + s2;
            v1 = pSrc[1] * 15 - pSrc[3] * 4;
            v2 = a0 - s2;
            v3 = pSrc[1] * 9 - pSrc[3] * 16;
            pBuff[1] = (v0 + v1) >> 3;
            pBuff[6] = (v0 - v1) >> 3;
            pBuff[2] = (v2 + v3) >> 3;
            pBuff[5] = (v2 - v3) >> 3;
            pSrc = (int16_t*)((uint8_t*)pSrc + srcStep);
            pBuff += 8;
        }
        pBuff -= 32;
        for (i = 0; i < 8; i ++) {
            int a0, a2, s2, u0, u1, u2, u3, v0, v1, v2, v3;

            a0 = pBuff[0*8] * 12 + 64;
            a2 = pBuff[2*8] * 16;
            s2 = pBuff[2*8] * 6;
            u0 = a0 + a2;
            u1 = pBuff[1*8] * 16 + pBuff[3*8] * 15;
            u2 = a0 - a2;
            u3 = pBuff[1*8] * 4 - pBuff[3*8] * 9;
            v0 = a0 + s2;
            v1 = pBuff[1*8] * 15 - pBuff[3*8] * 4;
            v2 = a0 - s2;
            v3 = pBuff[1*8] * 9 - pBuff[3*8] * 16;
            *(int16_t*)((uint8_t*)pDst+0*dstStep) = (int16_t)((u0 + u1) >> 7);
            *(int16_t*)((uint8_t*)pDst+7*dstStep) = (int16_t)((u0 - u1 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+3*dstStep) = (int16_t)((u2 + u3) >> 7);
            *(int16_t*)((uint8_t*)pDst+4*dstStep) = (int16_t)((u2 - u3 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+1*dstStep) = (int16_t)((v0 + v1) >> 7);
            *(int16_t*)((uint8_t*)pDst+6*dstStep) = (int16_t)((v0 - v1 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+2*dstStep) = (int16_t)((v2 + v3) >> 7);
            *(int16_t*)((uint8_t*)pDst+5*dstStep) = (int16_t)((v2 - v3 + 1) >> 7);
            pDst ++;
            pBuff ++;
        }
    } else {
        int     i, buff[64], *pBuff = buff;

        for (i = 0; i < 8; i ++) {
            int a0 = (pSrc[0] + pSrc[4]) * 12 + 4;
            int s0 = (pSrc[0] - pSrc[4]) * 12 + 4;
            int a2 = pSrc[2] * 16 + pSrc[6] * 6;
            int s2 = pSrc[2] * 6 - pSrc[6] * 16;
            int u0 = a0 + a2;
            int u1 = pSrc[1] * 16 + pSrc[3] * 15 + pSrc[5] * 9 + pSrc[7] * 4;
            int u2 = a0 - a2;
            int u3 = pSrc[1] * 4 - pSrc[3] * 9 + pSrc[5] * 15 - pSrc[7] * 16;

            pBuff[0] = (u0 + u1) >> 3;
            pBuff[7] = (u0 - u1) >> 3;
            pBuff[3] = (u2 + u3) >> 3;
            pBuff[4] = (u2 - u3) >> 3;
            u0 = s0 + s2;
            u1 = pSrc[1] * 15 - pSrc[3] * 4 - pSrc[5] * 16 - pSrc[7] * 9;
            u2 = s0 - s2;
            u3 = pSrc[1] * 9 - pSrc[3] * 16 + pSrc[5] * 4 + pSrc[7] * 15;
            pBuff[1] = (u0 + u1) >> 3;
            pBuff[6] = (u0 - u1) >> 3;
            pBuff[2] = (u2 + u3) >> 3;
            pBuff[5] = (u2 - u3) >> 3;
            pSrc = (int16_t*)((uint8_t*)pSrc + srcStep);
            pBuff += 8;
        }
        pBuff -= 64;
        for (i = 0; i < 8; i ++) {
            int a0 = (pBuff[0*8] + pBuff[4*8]) * 12 + 64;
            int s0 = (pBuff[0*8] - pBuff[4*8]) * 12 + 64;
            int a2 = pBuff[2*8] * 16 + pBuff[6*8] * 6;
            int s2 = pBuff[2*8] * 6 - pBuff[6*8] * 16;
            int u0 = a0 + a2;
            int u1 = pBuff[1*8] * 16 + pBuff[3*8] * 15 + pBuff[5*8] * 9 + pBuff[7*8] * 4;
            int u2 = a0 - a2;
            int u3 = pBuff[1*8] * 4 - pBuff[3*8] * 9 + pBuff[5*8] * 15 - pBuff[7*8] * 16;
            int v0 = s0 + s2;
            int v1 = pBuff[1*8] * 15 - pBuff[3*8] * 4 - pBuff[5*8] * 16 - pBuff[7*8] * 9;
            int v2 = s0 - s2;
            int v3 = pBuff[1*8] * 9 - pBuff[3*8] * 16 + pBuff[5*8] * 4 + pBuff[7*8] * 15;

            *(int16_t*)((uint8_t*)pDst+0*dstStep) = (int16_t)((u0 + u1) >> 7);
            *(int16_t*)((uint8_t*)pDst+7*dstStep) = (int16_t)((u0 - u1 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+3*dstStep) = (int16_t)((u2 + u3) >> 7);
            *(int16_t*)((uint8_t*)pDst+4*dstStep) = (int16_t)((u2 - u3 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+1*dstStep) = (int16_t)((v0 + v1) >> 7);
            *(int16_t*)((uint8_t*)pDst+6*dstStep) = (int16_t)((v0 - v1 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+2*dstStep) = (int16_t)((v2 + v3) >> 7);
            *(int16_t*)((uint8_t*)pDst+5*dstStep) = (int16_t)((v2 - v3 + 1) >> 7);
            pDst ++;
            pBuff ++;
        }
    }
    return ippStsNoErr;
}

static IppStatus own_ippiTransform8x4Inv_VC1_16s_C1R(const int16_t  *pSrc, int      srcStep,
                                                      int16_t  *pDst, int      dstStep,      mfxSize srcSizeNZ)
{
    if (srcSizeNZ.width <= 1 && srcSizeNZ.height <= 1) {
        int     i;
        int16_t  c = (int16_t)((((pSrc[0] * 12 + 4) >> 3) * 17 + 64) >> 7);

        for (i = 0; i < 4; i ++) {
            pDst[0] = pDst[1] = pDst[2] = pDst[3] = pDst[4] = pDst[5] = pDst[6] = pDst[7] = c;
            pDst = (int16_t*)((uint8_t*)pDst+dstStep);
        }
    } else {
        int     i, buff[32], *pBuff = buff;

        for (i = 0; i < 4; i ++) {
            int a0 = (pSrc[0] + pSrc[4]) * 12 + 4;
            int s0 = (pSrc[0] - pSrc[4]) * 12 + 4;
            int a2 = pSrc[2] * 16 + pSrc[6] * 6;
            int s2 = pSrc[2] * 6 - pSrc[6] * 16;
            int u0 = a0 + a2;
            int u1 = pSrc[1] * 16 + pSrc[3] * 15 + pSrc[5] * 9 + pSrc[7] * 4;
            int u2 = a0 - a2;
            int u3 = pSrc[1] * 4 - pSrc[3] * 9 + pSrc[5] * 15 - pSrc[7] * 16;

            pBuff[0] = (u0 + u1) >> 3;
            pBuff[7] = (u0 - u1) >> 3;
            pBuff[3] = (u2 + u3) >> 3;
            pBuff[4] = (u2 - u3) >> 3;
            u0 = s0 + s2;
            u1 = pSrc[1] * 15 - pSrc[3] * 4 - pSrc[5] * 16 - pSrc[7] * 9;
            u2 = s0 - s2;
            u3 = pSrc[1] * 9 - pSrc[3] * 16 + pSrc[5] * 4 + pSrc[7] * 15;
            pBuff[1] = (u0 + u1) >> 3;
            pBuff[6] = (u0 - u1) >> 3;
            pBuff[2] = (u2 + u3) >> 3;
            pBuff[5] = (u2 - u3) >> 3;
            pSrc = (int16_t*)((uint8_t*)pSrc + srcStep);
            pBuff += 8;
        }
        pBuff -= 32;
        for (i = 0; i < 8; i ++) {
            int a0 = (pBuff[0*8] + pBuff[2*8]) * 17 + 64;
            int s0 = (pBuff[0*8] - pBuff[2*8]) * 17 + 64;
            int a1 = pBuff[1*8] * 22 + pBuff[3*8] * 10;
            int s1 = pBuff[1*8] * 10 - pBuff[3*8] * 22;

            *(int16_t*)((uint8_t*)pDst+0*dstStep) = (int16_t)((a0 + a1) >> 7);
            *(int16_t*)((uint8_t*)pDst+1*dstStep) = (int16_t)((s0 + s1) >> 7);
            *(int16_t*)((uint8_t*)pDst+2*dstStep) = (int16_t)((s0 - s1) >> 7);
            *(int16_t*)((uint8_t*)pDst+3*dstStep) = (int16_t)((a0 - a1) >> 7);
            pDst ++;
            pBuff ++;
        }
    }
    return ippStsNoErr;
}

static IppStatus own_ippiTransform4x8Inv_VC1_16s_C1R(const int16_t  *pSrc,  int      srcStep,
                                                      int16_t  *pDst,  int      dstStep,    mfxSize srcSizeNZ)
{

    if (srcSizeNZ.width <= 1 && srcSizeNZ.height <= 1) {
        int     i;
        int16_t  c = (int16_t)((((pSrc[0] * 17 + 4) >> 3) * 12 + 64) >> 7);

        for (i = 0; i < 8; i ++) {
            pDst[0] = pDst[1] = pDst[2] = pDst[3] = c;
            pDst = (int16_t*)((uint8_t*)pDst+dstStep);
        }
    } else {
        int     i, buff[32], *pBuff = buff;

        for (i = 0; i < 8; i ++) {
            int a0 = (pSrc[0] + pSrc[2]) * 17 + 4;
            int s0 = (pSrc[0] - pSrc[2]) * 17 + 4;
            int a1 = pSrc[1] * 22 + pSrc[3] * 10;
            int s1 = pSrc[1] * 10 - pSrc[3] * 22;

            pBuff[0] = (a0 + a1) >> 3;
            pBuff[1] = (s0 + s1) >> 3;
            pBuff[2] = (s0 - s1) >> 3;
            pBuff[3] = (a0 - a1) >> 3;
            pSrc = (int16_t*)((uint8_t*)pSrc + srcStep);
            pBuff += 4;
        }
        pBuff -= 32;
        for (i = 0; i < 4; i ++) {
            int a0 = (pBuff[0*4] + pBuff[4*4]) * 12 + 64;
            int s0 = (pBuff[0*4] - pBuff[4*4]) * 12 + 64;
            int a2 = pBuff[2*4] * 16 + pBuff[6*4] * 6;
            int s2 = pBuff[2*4] * 6 - pBuff[6*4] * 16;
            int u0 = a0 + a2;
            int u1 = pBuff[1*4] * 16 + pBuff[3*4] * 15 + pBuff[5*4] * 9 + pBuff[7*4] * 4;
            int u2 = a0 - a2;
            int u3 = pBuff[1*4] * 4 - pBuff[3*4] * 9 + pBuff[5*4] * 15 - pBuff[7*4] * 16;
            int v0 = s0 + s2;
            int v1 = pBuff[1*4] * 15 - pBuff[3*4] * 4 - pBuff[5*4] * 16 - pBuff[7*4] * 9;
            int v2 = s0 - s2;
            int v3 = pBuff[1*4] * 9 - pBuff[3*4] * 16 + pBuff[5*4] * 4 + pBuff[7*4] * 15;

            *(int16_t*)((uint8_t*)pDst+0*dstStep) = (int16_t)((u0 + u1) >> 7);
            *(int16_t*)((uint8_t*)pDst+7*dstStep) = (int16_t)((u0 - u1 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+3*dstStep) = (int16_t)((u2 + u3) >> 7);
            *(int16_t*)((uint8_t*)pDst+4*dstStep) = (int16_t)((u2 - u3 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+1*dstStep) = (int16_t)((v0 + v1) >> 7);
            *(int16_t*)((uint8_t*)pDst+6*dstStep) = (int16_t)((v0 - v1 + 1) >> 7);
            *(int16_t*)((uint8_t*)pDst+2*dstStep) = (int16_t)((v2 + v3) >> 7);
            *(int16_t*)((uint8_t*)pDst+5*dstStep) = (int16_t)((v2 - v3 + 1) >> 7);
            pDst ++;
            pBuff ++;
        }
    }
    return ippStsNoErr;
}


static IppStatus own_ippiTransform4x4Inv_VC1_16s_C1R (const int16_t  *pSrc, int      srcStep,
                                                 int16_t  *pDst,       int      dstStep,      mfxSize srcSizeNZ)
{

    if (srcSizeNZ.width <= 1 && srcSizeNZ.height <= 1) {
        int16_t  c = (int16_t)((((pSrc[0] * 17 + 4) >> 3) * 17 + 64) >> 7);
        pDst[0] = pDst[1] = pDst[2] = pDst[3] = c;
        pDst = (int16_t*)((uint8_t*)pDst+dstStep);
        pDst[0] = pDst[1] = pDst[2] = pDst[3] = c;
        pDst = (int16_t*)((uint8_t*)pDst+dstStep);
        pDst[0] = pDst[1] = pDst[2] = pDst[3] = c;
        pDst = (int16_t*)((uint8_t*)pDst+dstStep);
        pDst[0] = pDst[1] = pDst[2] = pDst[3] = c;
    } else if (srcSizeNZ.width <= 2 && srcSizeNZ.height <= 2) {
        int     i, buff[8], *pBuff = buff, a0, a1, s1;

        a0 = pSrc[0] * 17 + 4;
        a1 = pSrc[1] * 22;
        s1 = pSrc[1] * 10;
        pBuff[0] = (a0 + a1) >> 3;
        pBuff[1] = (a0 + s1) >> 3;
        pBuff[2] = (a0 - s1) >> 3;
        pBuff[3] = (a0 - a1) >> 3;
        pSrc = (int16_t*)((uint8_t*)pSrc + srcStep);
        a0 = pSrc[0] * 17 + 4;
        a1 = pSrc[1] * 22;
        s1 = pSrc[1] * 10;
        pBuff[4] = (a0 + a1) >> 3;
        pBuff[5] = (a0 + s1) >> 3;
        pBuff[6] = (a0 - s1) >> 3;
        pBuff[7] = (a0 - a1) >> 3;
        for (i = 0; i < 4; i ++) {
            a0 = pBuff[0*4] * 17 + 64;
            a1 = pBuff[1*4] * 22;
            s1 = pBuff[1*4] * 10;
            *(int16_t*)((uint8_t*)pDst+0*dstStep) = (int16_t)((a0 + a1) >> 7);
            *(int16_t*)((uint8_t*)pDst+1*dstStep) = (int16_t)((a0 + s1) >> 7);
            *(int16_t*)((uint8_t*)pDst+2*dstStep) = (int16_t)((a0 - s1) >> 7);
            *(int16_t*)((uint8_t*)pDst+3*dstStep) = (int16_t)((a0 - a1) >> 7);
            pDst ++;
            pBuff ++;
        }
    } else {
        int     i, buff[16], *pBuff = buff;

        for (i = 0; i < 4; i ++) {
            int a0 = (pSrc[0] + pSrc[2]) * 17 + 4;
            int s0 = (pSrc[0] - pSrc[2]) * 17 + 4;
            int a1 = pSrc[1] * 22 + pSrc[3] * 10;
            int s1 = pSrc[1] * 10 - pSrc[3] * 22;
            pBuff[0] = (a0 + a1) >> 3;
            pBuff[1] = (s0 + s1) >> 3;
            pBuff[2] = (s0 - s1) >> 3;
            pBuff[3] = (a0 - a1) >> 3;
            pSrc = (int16_t*)((uint8_t*)pSrc + srcStep);
            pBuff += 4;
        }
        pBuff -= 16;
        for (i = 0; i < 4; i ++) {
            int a0 = (pBuff[0*4] + pBuff[2*4]) * 17 + 64;
            int s0 = (pBuff[0*4] - pBuff[2*4]) * 17 + 64;
            int a1 = pBuff[1*4] * 22 + pBuff[3*4] * 10;
            int s1 = pBuff[1*4] * 10 - pBuff[3*4] * 22;
            *(int16_t*)((uint8_t*)pDst+0*dstStep) = (int16_t)((a0 + a1) >> 7);
            *(int16_t*)((uint8_t*)pDst+1*dstStep) = (int16_t)((s0 + s1) >> 7);
            *(int16_t*)((uint8_t*)pDst+2*dstStep) = (int16_t)((s0 - s1) >> 7);
            *(int16_t*)((uint8_t*)pDst+3*dstStep) = (int16_t)((a0 - a1) >> 7);
            pDst ++;
            pBuff ++;
        }
    }
    return ippStsNoErr;
}

IppStatus _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR(int16_t* pSrcDst, uint32_t srcDstStep, mfxSize DstSize)
{
    own_ippiTransform8x8Inv_VC1_16s_C1R(pSrcDst, srcDstStep, pSrcDst, srcDstStep, DstSize);
    return ippStsNoErr;
}

IppStatus _own_ippiTransformBlock4x8Inv_VC1_16s_C1IR(int16_t* pSrcDst, uint32_t srcDstStep, mfxSize DstSize)
{
    own_ippiTransform4x8Inv_VC1_16s_C1R(pSrcDst, srcDstStep, pSrcDst, srcDstStep, DstSize);
    return ippStsNoErr;
}

IppStatus _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR(int16_t* pSrcDst, uint32_t srcDstStep, mfxSize DstSize)
{
    own_ippiTransform8x4Inv_VC1_16s_C1R(pSrcDst, srcDstStep, pSrcDst, srcDstStep, DstSize);
    return ippStsNoErr;
}

IppStatus _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR(int16_t* pSrcDst, uint32_t srcDstStep, mfxSize DstSize)
{
    own_ippiTransform4x4Inv_VC1_16s_C1R(pSrcDst, srcDstStep, pSrcDst, srcDstStep, DstSize);
    return ippStsNoErr;
}


#endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //_VC1_ENC_OWN_FUNCTIONS_

////////////////////////////////////////////////////////////////
//--------Convert----------------------------------------

#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
IppStatus _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R (int16_t* pSrcLeft, int32_t srcLeftStep,
                                                        int16_t* pSrcRight, int32_t srcRightStep,
                                                        uint8_t* pDst, int32_t dstStep,
                                                        uint32_t fieldNeighbourFlag,
                                                        uint32_t edgeDisableFlag)
{
    int32_t i;
    int8_t r0, r1;
    int16_t *pSrcL = pSrcLeft;
    int16_t *pSrcR = pSrcRight;
    uint8_t  *dst = pDst;

    int16_t x0,x1,x2,x3;
    int16_t f0, f1;

    if(!edgeDisableFlag) return ippStsNoErr;

    srcLeftStep >>= 1;
    srcRightStep>>= 1;

    switch (fieldNeighbourFlag)
    {
    case 0:
        {
            //both blocks are not field
            r0 = 4; r1 = 3;
            if(edgeDisableFlag & IPPVC_EDGE_HALF_1)
                for(i=0; i < VC1_PIXEL_IN_BLOCK; i++)
                {
                    x0 = *(pSrcL);
                    x1 = *(pSrcL + 1);
                    x2 = *(pSrcR );
                    x3 = *(pSrcR + 1);

                    f0 = x3 - x0;
                    f1 = x2 + x3 - x0 - x1;

                    *(pSrcL)     = x0 + ((f0 + r0)>>3);
                    *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                    *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                    *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                    *(dst-2)=mfx::byte_clamp(*(pSrcL));
                    *(dst-1)=mfx::byte_clamp(*(pSrcL+1));
                    *(dst+0)=mfx::byte_clamp(*(pSrcR));
                    *(dst+1)=mfx::byte_clamp(*(pSrcR+1));

                    r0 = 7 - r0;
                    r1 = 7 - r1;
                    pSrcL += srcLeftStep;
                    pSrcR += srcRightStep;
                    dst+=dstStep;
                }
                pSrcL = pSrcLeft + VC1_PIXEL_IN_BLOCK*srcLeftStep;
                pSrcR = pSrcRight +VC1_PIXEL_IN_BLOCK*srcRightStep;
                dst = pDst + dstStep*VC1_PIXEL_IN_BLOCK;
                if(edgeDisableFlag & IPPVC_EDGE_HALF_2)
                    for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
                    {
                        x0 = *(pSrcL);
                        x1 = *(pSrcL + 1);
                        x2 = *(pSrcR );
                        x3 = *(pSrcR + 1);

                        f0 = x3 - x0;
                        f1 = x2 + x3 - x0 - x1;

                        *(pSrcL)     = x0 + ((f0 + r0)>>3);
                        *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                        *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                        *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                        *(dst-2)=mfx::byte_clamp(*(pSrcL));
                        *(dst-1)=mfx::byte_clamp(*(pSrcL+1));
                        *(dst+0)=mfx::byte_clamp(*(pSrcR));
                        *(dst+1)=mfx::byte_clamp(*(pSrcR+1));

                        r0 = 7 - r0;
                        r1 = 7 - r1;
                        pSrcL += srcLeftStep;
                        pSrcR += srcRightStep;
                        dst+=dstStep;
                    }
        }
        break;
    case 1:
        {
            //right - field,  left - not
            r0 = 4;
            r1 = 3;

            for(i=0; i < VC1_PIXEL_IN_BLOCK; i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=mfx::byte_clamp(*(pSrcL));
                *(dst-1)=mfx::byte_clamp(*(pSrcL+1));
                *(dst+0)=mfx::byte_clamp(*(pSrcR));
                *(dst+1)=mfx::byte_clamp(*(pSrcR+1));

                pSrcR += srcRightStep;
                pSrcL += 2*srcLeftStep;
                dst+=2*dstStep;
            }

            r0 = 3;
            r1 = 4;
            pSrcL = pSrcLeft + srcLeftStep;
            pSrcR = pSrcRight +VC1_PIXEL_IN_BLOCK*srcRightStep;
            dst = pDst + dstStep;

            for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=mfx::byte_clamp(*(pSrcL));
                *(dst-1)=mfx::byte_clamp(*(pSrcL+1));
                *(dst+0)=mfx::byte_clamp(*(pSrcR));
                *(dst+1)=mfx::byte_clamp(*(pSrcR+1));

                pSrcL += 2*srcLeftStep;
                pSrcR += srcRightStep;
                dst+=2*dstStep;
            }
        }
        break;
    case 2:
        {
            //left - field, right  - not
            r0 = 4;
            r1 = 3;

            for(i=0;i<4;i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=mfx::byte_clamp(*(pSrcL));
                *(dst-1)=mfx::byte_clamp(*(pSrcL+1));
                *(dst+0)=mfx::byte_clamp(*(pSrcR));
                *(dst+1)=mfx::byte_clamp(*(pSrcR+1));

                pSrcR += srcRightStep;
                pSrcL = pSrcL + VC1_PIXEL_IN_BLOCK*srcLeftStep;

                dst+=dstStep;

                r0 = 7 - r0;
                r1 = 7 - r1;

                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=mfx::byte_clamp(*(pSrcL));
                *(dst-1)=mfx::byte_clamp(*(pSrcL+1));
                *(dst+0)=mfx::byte_clamp(*(pSrcR));
                *(dst+1)=mfx::byte_clamp(*(pSrcR+1));

                pSrcL -= 7*srcLeftStep;
                pSrcR += srcRightStep;

                r0 = 7 - r0;
                r1 = 7 - r1;
                dst+=dstStep;
            }
            pSrcL = pSrcLeft + 4*srcLeftStep;

            for(i=0;i<4;i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=mfx::byte_clamp(*(pSrcL));
                *(dst-1)=mfx::byte_clamp(*(pSrcL+1));
                *(dst+0)=mfx::byte_clamp(*(pSrcR));
                *(dst+1)=mfx::byte_clamp(*(pSrcR+1));

                pSrcR += srcRightStep;
                pSrcL = pSrcL + VC1_PIXEL_IN_BLOCK*srcLeftStep;

                dst+=dstStep;

                r0 = 7 - r0;
                r1 = 7 - r1;

                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=mfx::byte_clamp(*(pSrcL));
                *(dst-1)=mfx::byte_clamp(*(pSrcL+1));
                *(dst+0)=mfx::byte_clamp(*(pSrcR));
                *(dst+1)=mfx::byte_clamp(*(pSrcR+1));

                pSrcL -= 7*srcLeftStep;
                pSrcR += srcRightStep;

                r0 = 7 - r0;
                r1 = 7 - r1;
                dst+=dstStep;
            }
        }
        break;
    case 3:
        {
            //both blocks are field
            r0 = 4; r1 = 3;
            for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=mfx::byte_clamp(*(pSrcL));
                *(dst-1)=mfx::byte_clamp(*(pSrcL+1));
                *(dst+0)=mfx::byte_clamp(*(pSrcR));
                *(dst+1)=mfx::byte_clamp(*(pSrcR+1));

                pSrcL += srcLeftStep;
                pSrcR += srcRightStep;
                dst+=2*dstStep;
            }

            r0 = 3; r1 = 4;
            pSrcL = pSrcLeft + VC1_PIXEL_IN_BLOCK*srcLeftStep;
            pSrcR = pSrcRight +VC1_PIXEL_IN_BLOCK*srcRightStep;
            dst = pDst + dstStep;
            for(i=0; i < VC1_PIXEL_IN_BLOCK; i++)
            {
                x0 = *(pSrcL);
                x1 = *(pSrcL + 1);
                x2 = *(pSrcR );
                x3 = *(pSrcR + 1);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcL)     = x0 + ((f0 + r0)>>3);
                *(pSrcL + 1) = x1 + ((f1 + r1)>>3);
                *(pSrcR )    = x2 + ((-f1 + r0)>>3);
                *(pSrcR+1)   = x3 + ((-f0 + r1)>>3);

                *(dst-2)=mfx::byte_clamp(*(pSrcL));
                *(dst-1)=mfx::byte_clamp(*(pSrcL+1));
                *(dst+0)=mfx::byte_clamp(*(pSrcR));
                *(dst+1)=mfx::byte_clamp(*(pSrcR+1));

                pSrcL += srcLeftStep;
                pSrcR += srcRightStep;
                dst+=2*dstStep;
            }
        }
        break;
    }

    return ippStsNoErr;
}

IppStatus _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R (int16_t* pSrcUpper, int32_t srcUpperStep,
                                                        int16_t* pSrcBottom, int32_t srcBottomStep,
                                                        uint8_t* pDst, int32_t dstStep,
                                                        uint32_t edgeDisableFlag)
{
    int32_t i;
    int8_t r0=4, r1=3;
    int16_t *pSrcU = pSrcUpper;
    int16_t *pSrcB = pSrcBottom;
    uint8_t  *dst = pDst;

    int16_t x0,x1,x2,x3;
    int16_t f0, f1;
    srcUpperStep/=2;
    srcBottomStep/=2;

    if(edgeDisableFlag & IPPVC_EDGE_HALF_1)
        for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
        {
            x0 = *(pSrcU);
            x1 = *(pSrcU+srcUpperStep);
            x2 = *(pSrcB);
            x3 = *(pSrcB+srcBottomStep);

            f0 = x3 - x0;
            f1 = x2 + x3 - x0 - x1;

            *(pSrcU    ) = ((7 * x0 + 0 * x1 + 0 * x2 + 1 * x3) + r0)>>3;
            *(pSrcU + srcUpperStep) = ((-1* x0 + 7 * x1 + 1 * x2 + 1 * x3) + r1)>>3;
            *(pSrcB    ) = ((1 * x0 + 1 * x1 + 7 * x2 + -1* x3) + r0)>>3;
            *(pSrcB + srcBottomStep) = ((1 * x0 + 0 * x1 + 0 * x2 + 7 * x3) + r1)>>3;

            *(dst-2*dstStep)= mfx::byte_clamp(x0 + ((f0 + r0)>>3));
            *(dst-1*dstStep)= mfx::byte_clamp(x1 + ((f1 + r1)>>3));
            *(dst+0*dstStep)= mfx::byte_clamp(x2 + ((-f1 + r0)>>3));
            *(dst+1*dstStep)= mfx::byte_clamp(x3 + ((-f0 + r1)>>3));

            dst++;
            pSrcU++;
            pSrcB++;

            r0 = 7 - r0;
            r1 = 7 - r1;
        }

        pSrcU = pSrcUpper + VC1_PIXEL_IN_BLOCK;
        pSrcB = pSrcBottom + VC1_PIXEL_IN_BLOCK;
        dst = pDst + 8;

        if(edgeDisableFlag & IPPVC_EDGE_HALF_2)
            for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
            {
                x0 = *(pSrcU);
                x1 = *(pSrcU+srcUpperStep);
                x2 = *(pSrcB);
                x3 = *(pSrcB+srcBottomStep);

                f0 = x3 - x0;
                f1 = x2 + x3 - x0 - x1;

                *(pSrcU    ) = ((7 * x0 + 0 * x1 + 0 * x2 + 1 * x3) + r0)>>3;
                *(pSrcU + srcUpperStep) = ((-1* x0 + 7 * x1 + 1 * x2 + 1 * x3) + r1)>>3;
                *(pSrcB    ) = ((1 * x0 + 1 * x1 + 7 * x2 + -1* x3) + r0)>>3;
                *(pSrcB + srcBottomStep) = ((1 * x0 + 0 * x1 + 0 * x2 + 7 * x3) + r1)>>3;

                *(dst-2*dstStep)= mfx::byte_clamp(x0 + ((f0 + r0)>>3));
                *(dst-1*dstStep)= mfx::byte_clamp(x1 + ((f1 + r1)>>3));
                *(dst+0*dstStep)= mfx::byte_clamp(x2 + ((-f1 + r0)>>3));
                *(dst+1*dstStep)= mfx::byte_clamp(x3 + ((-f0 + r1)>>3));

                dst++;
                pSrcU++;
                pSrcB++;

                r0 = 7 - r0;
                r1 = 7 - r1;
            }

            return ippStsNoErr;
}


IppStatus _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R (int16_t* pSrcUpper, int32_t srcUpperStep,
                                                          int16_t* pSrcBottom, int32_t srcBottomStep,
                                                          uint8_t* pDst, int32_t dstStep)
{
    int32_t i;
    int8_t r0=4, r1=3;
    int16_t *pSrcU = pSrcUpper;
    int16_t *pSrcB = pSrcBottom;
    uint8_t  *dst = pDst;

    int16_t x0,x1,x2,x3;
    int16_t f0, f1;

    srcUpperStep/=2;
    srcBottomStep/=2;

    for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
    {
        x0 = *(pSrcU);
        x1 = *(pSrcU+srcUpperStep);
        x2 = *(pSrcB);
        x3 = *(pSrcB+srcBottomStep);

        f0 = x3 - x0;
        f1 = x2 + x3 - x0 - x1;

        *(pSrcU    )            = mfx::byte_clamp(x0 + ((f0 + r0)>>3));
        *(pSrcU + srcUpperStep) = mfx::byte_clamp(x1 + ((f1 + r1)>>3));
        *(pSrcB    )            = mfx::byte_clamp(x2 + ((-f1 + r0)>>3));
        *(pSrcB + srcBottomStep)= mfx::byte_clamp(x3 + ((-f0 + r1)>>3));

        *(dst-2*dstStep)= mfx::byte_clamp(x0 + ((f0 + r0)>>3));
        *(dst-1*dstStep)= mfx::byte_clamp(x1 + ((f1 + r1)>>3));
        *(dst+0*dstStep)= mfx::byte_clamp(x2 + ((-f1 + r0)>>3));
        *(dst+1*dstStep)= mfx::byte_clamp(x3 + ((-f0 + r1)>>3));

        dst++;
        pSrcU++;
        pSrcB++;

        r0 = 7 - r0;
        r1 = 7 - r1;
    }
    return ippStsNoErr;
}

IppStatus _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R (int16_t* pSrcLeft, int32_t srcLeftStep,
                                                          int16_t* pSrcRight, int32_t srcRightStep,
                                                          uint8_t* pDst, int32_t dstStep)
{
    int32_t i;
    int8_t r0, r1;

    int16_t x0,x1,x2,x3;
    int16_t f0, f1;

    r0 = 4; r1 = 3;
    srcLeftStep/=2;
    srcRightStep/=2;


    for(i=0;i<VC1_PIXEL_IN_BLOCK;i++)
    {
        x0 = *(pSrcLeft);
        x1 = *(pSrcLeft + 1);
        x2 = *(pSrcRight );
        x3 = *(pSrcRight + 1);

        f0 = x3 - x0;
        f1 = x2 + x3 - x0 - x1;

        *(pSrcLeft)     = x0 + ((f0 + r0)>>3);
        *(pSrcLeft + 1) = x1 + ((f1 + r1)>>3);
        *(pSrcRight )   = x2 + ((-f1 + r0)>>3);
        *(pSrcRight+1)  = x3 + ((-f0 + r1)>>3);

        *(pDst-2)=mfx::byte_clamp(*(pSrcLeft));
        *(pDst-1)=mfx::byte_clamp(*(pSrcLeft+1));
        *(pDst+0)=mfx::byte_clamp(*(pSrcRight));
        *(pDst+1)=mfx::byte_clamp(*(pSrcRight+1));

        r0 = 7 - r0;
        r1 = 7 - r1;
        pSrcLeft += srcLeftStep;
        pSrcRight += srcRightStep;
        pDst+=dstStep;
    }
    return ippStsNoErr;
}
#endif
#endif

IppStatus _own_ippiSmoothingChroma_NV12_HorEdge_VC1_16s8u_C2R (int16_t* pSrcUpperU, int32_t srcUpperStepU,
                                                               int16_t* pSrcBottomU, int32_t srcBottomStepU,
                                                               int16_t* pSrcUpperV, int32_t srcUpperStepV,
                                                               int16_t* pSrcBottomV, int32_t srcBottomStepV,
                                                               uint8_t* pDst, int32_t dstStep)
{
    int32_t i;
    int8_t  r0 = 4, r1 = 3;
    int16_t *pSrcU_U = pSrcUpperU;
    int16_t *pSrcB_U = pSrcBottomU;
    int16_t *pSrcU_V = pSrcUpperV;
    int16_t *pSrcB_V = pSrcBottomV;
    uint8_t  *dst = pDst;

    int16_t x0,x1,x2,x3;
    int16_t f0, f1;

    srcUpperStepU  /= 2;
    srcBottomStepU /= 2;

    srcUpperStepV  /= 2;
    srcBottomStepV /= 2;

    for(i = 0;i < VC1_PIXEL_IN_BLOCK; i++)
    {
        //U
        x0 = *(pSrcU_U);
        x1 = *(pSrcU_U + srcUpperStepU);
        x2 = *(pSrcB_U);
        x3 = *(pSrcB_U + srcBottomStepU);

        f0 = x3 - x0;
        f1 = x2 + x3 - x0 - x1;

        *(pSrcU_U    )              = mfx::byte_clamp(x0 + ((f0 + r0)>>3));
        *(pSrcU_U + srcUpperStepU)  = mfx::byte_clamp(x1 + ((f1 + r1)>>3));
        *(pSrcB_U    )              = mfx::byte_clamp(x2 + ((-f1 + r0)>>3));
        *(pSrcB_U + srcBottomStepU) = mfx::byte_clamp(x3 + ((-f0 + r1)>>3));

        *(dst - 2*dstStep)= mfx::byte_clamp(x0 + ((f0 + r0)>>3));
        *(dst - 1*dstStep)= mfx::byte_clamp(x1 + ((f1 + r1)>>3));
        *(dst + 0*dstStep)= mfx::byte_clamp(x2 + ((-f1 + r0)>>3));
        *(dst + 1*dstStep)= mfx::byte_clamp(x3 + ((-f0 + r1)>>3));

        pSrcU_U++;
        pSrcB_U++;
        dst++;

        //V
        x0 = *(pSrcU_V);
        x1 = *(pSrcU_V + srcUpperStepV);
        x2 = *(pSrcB_V);
        x3 = *(pSrcB_V + srcBottomStepV);

        f0 = x3 - x0;
        f1 = x2 + x3 - x0 - x1;

        *(pSrcU_V    )              = mfx::byte_clamp(x0 + ((f0 + r0)>>3));
        *(pSrcU_V + srcUpperStepV)  = mfx::byte_clamp(x1 + ((f1 + r1)>>3));
        *(pSrcB_V    )              = mfx::byte_clamp(x2 + ((-f1 + r0)>>3));
        *(pSrcB_V + srcBottomStepV) = mfx::byte_clamp(x3 + ((-f0 + r1)>>3));

        *(dst - 2*dstStep)= mfx::byte_clamp(x0 + ((f0 + r0)>>3));
        *(dst - 1*dstStep)= mfx::byte_clamp(x1 + ((f1 + r1)>>3));
        *(dst + 0*dstStep)= mfx::byte_clamp(x2 + ((-f1 + r0)>>3));
        *(dst + 1*dstStep)= mfx::byte_clamp(x3 + ((-f0 + r1)>>3));

        pSrcU_V++;
        pSrcB_V++;
        dst++;

        r0 = 7 - r0;
        r1 = 7 - r1;
    }
    return ippStsNoErr;
}


IppStatus _own_ippiSmoothingChroma_NV12_VerEdge_VC1_16s8u_C2R (int16_t* pSrcLeftU,  int32_t srcLeftStepU,
                                                               int16_t* pSrcRightU, int32_t srcRightStepU,
                                                               int16_t* pSrcLeftV,  int32_t srcLeftStepV,
                                                               int16_t* pSrcRightV, int32_t srcRightStepV,
                                                               uint8_t* pDst, int32_t dstStep)
{
    int32_t i;
    int8_t r0, r1;

    int16_t x0,x1,x2,x3;
    int16_t f0, f1;

    r0 = 4; r1 = 3;
    srcLeftStepU  /= 2;
    srcRightStepU /= 2;

    srcLeftStepV  /= 2;
    srcRightStepV /= 2;

    for(i = 0; i < VC1_PIXEL_IN_BLOCK;i++)
    {
        //U
        x0 = *(pSrcLeftU);
        x1 = *(pSrcLeftU + 1);
        x2 = *(pSrcRightU );
        x3 = *(pSrcRightU + 1);

        f0 = x3 - x0;
        f1 = x2 + x3 - x0 - x1;

        *(pSrcLeftU)       = x0 + ((f0 + r0)>>3);
        *(pSrcLeftU + 1)   = x1 + ((f1 + r1)>>3);
        *(pSrcRightU )     = x2 + ((-f1 + r0)>>3);
        *(pSrcRightU + 1)  = x3 + ((-f0 + r1)>>3);

        *(pDst - 4)=mfx::byte_clamp(*(pSrcLeftU));
        *(pDst - 2)=mfx::byte_clamp(*(pSrcLeftU + 1));
        *(pDst + 0)=mfx::byte_clamp(*(pSrcRightU));
        *(pDst + 2)=mfx::byte_clamp(*(pSrcRightU + 1));

        pSrcLeftU  += srcLeftStepU;
        pSrcRightU += srcRightStepU;

        pDst++;

        //V
        x0 = *(pSrcLeftV);
        x1 = *(pSrcLeftV + 1);
        x2 = *(pSrcRightV );
        x3 = *(pSrcRightV + 1);

        f0 = x3 - x0;
        f1 = x2 + x3 - x0 - x1;

        *(pSrcLeftV)       = x0 + ((f0 + r0)>>3);
        *(pSrcLeftV + 1)   = x1 + ((f1 + r1)>>3);
        *(pSrcRightV )     = x2 + ((-f1 + r0)>>3);
        *(pSrcRightV + 1)  = x3 + ((-f0 + r1)>>3);

        *(pDst - 4)=mfx::byte_clamp(*(pSrcLeftV));
        *(pDst - 2)=mfx::byte_clamp(*(pSrcLeftV + 1));
        *(pDst + 0)=mfx::byte_clamp(*(pSrcRightV));
        *(pDst + 2)=mfx::byte_clamp(*(pSrcRightV + 1));

        pSrcLeftV += srcLeftStepV;
        pSrcRightV += srcRightStepV;

        pDst+=(dstStep - 1);

        r0 = 7 - r0;
        r1 = 7 - r1;
    }
    return ippStsNoErr;
}

////////////////////////////////////////////////////////////////
//--------Convert----------------------------------------

#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_

#define MFX_MAX_8U     ( 0xFF )
#define OWN_MIN_8U      0

IppStatus _own_ippiConvert_16s8u_C1R( const int16_t* pSrc, uint32_t srcStep, uint8_t* pDst, uint32_t dstStep, mfxSize roiSize )
{
    const int16_t *src = pSrc;
    uint8_t *dst = pDst;
    int width = roiSize.width, height = roiSize.height;
    int h;
    int n;

    if( (srcStep == dstStep*2) && (dstStep == width) ) {
        width *= height;
        height = 1;
      }

    for( h = 0; h < height; h++ ) {

        for( n = 0; n < width; n++ ) {
            if( src[n] >= MFX_MAX_8U ) {
                dst[n] = MFX_MAX_8U;
            } else if( src[n] <= OWN_MIN_8U ) {
                dst[n] = OWN_MIN_8U;
            } else {
                dst[n] = (uint8_t)src[n];
            }
        }
        src = (int16_t*)((uint8_t*)src + srcStep);
        dst += dstStep;
    }
    return ippStsNoErr;
}

#endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //_VC1_ENC_OWN_FUNCTIONS_
bool MaxAbsDiff(uint8_t* pSrc1, uint32_t step1,uint8_t* pSrc2, uint32_t step2,mfxSize roiSize,uint32_t max)
{
    uint32_t  sum = max;
    for (int i = 0; i<roiSize.height; i++)
    {
       uint16_t t1 = (VC1abs(*(pSrc1++) - *(pSrc2++)));
       uint16_t t2 = (VC1abs(*(pSrc1++) - *(pSrc2++)));

       for (int j = 0; j<roiSize.width-2; j++)
       {
           uint16_t t3 = (VC1abs(*(pSrc1++) - *(pSrc2++)));
           if (t2>sum &&(t1>sum || t3>sum))
           {
            return false;
           }
           t1 = t2;
           t2 = t3;
       }
       pSrc1 = pSrc1 + (step1 - roiSize.width);
       pSrc2 = pSrc2 + (step2 - roiSize.width);
    }
    return true;
}
void initMVFieldInfo(bool extendedX,        bool extendedY,const uint32_t* pVLCTable ,sMVFieldInfo* MVFieldInfo)
{
     MVFieldInfo->bExtendedX = extendedX;
     MVFieldInfo->bExtendedY = extendedY;

    if (extendedX)
    {
        MVFieldInfo->pMVSizeOffsetFieldIndexX = MVSizeOffsetFieldExIndex;
        MVFieldInfo->pMVSizeOffsetFieldX      = MVSizeOffsetFieldEx;
        MVFieldInfo->limitX                   = 511;
    }
    else
    {
        MVFieldInfo->pMVSizeOffsetFieldIndexX = MVSizeOffsetFieldIndex;
        MVFieldInfo->pMVSizeOffsetFieldX      = MVSizeOffsetField;
        MVFieldInfo->limitX                   = 256;
    }
    if (extendedY)
    {
        MVFieldInfo->pMVSizeOffsetFieldIndexY = MVSizeOffsetFieldExIndex;
        MVFieldInfo->pMVSizeOffsetFieldY      = MVSizeOffsetFieldEx;
        MVFieldInfo->limitY                   = 511;
    }
    else
    {
        MVFieldInfo->pMVSizeOffsetFieldIndexY = MVSizeOffsetFieldIndex;
        MVFieldInfo->pMVSizeOffsetFieldY      = MVSizeOffsetField;
        MVFieldInfo->limitY                   = 256;
    }
    MVFieldInfo->pMVModeField1RefTable_VLC = pVLCTable;

}
inline static int32_t GetEdgeValue(int32_t Edge, int32_t Current)
{
    if (Current <= 0)
        return 0;
    else
    {
        if (Current < Edge)
            return Current;
        else
            return (Edge - 1);
    }
}

IppStatus own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R(const IppVCInterpolateBlock_8u* interpolateInfo,
                                                     const uint8_t*                   pLUTTop,
                                                     const uint8_t*                   pLUTBottom,
                                                     uint32_t                          OppositePadding,
                                                     uint32_t                          fieldPrediction,
                                                     uint32_t                          RoundControl,
                                                     uint32_t                          isPredBottom)

{
    uint8_t    temp_block [19*19]          = {0};
    mfxSize temp_block_size             = {interpolateInfo->sizeBlock.width + 3, interpolateInfo->sizeBlock.height + 3};
    int32_t   temp_block_step             = 19;
    uint8_t*   p_temp_block_data_pointer   = temp_block;
    uint8_t    shift = (fieldPrediction)? 1:0;

    IppiPoint position = {interpolateInfo->pointBlockPos.x,
        interpolateInfo->pointBlockPos.y};

    uint32_t step = 0;

    int32_t TopFieldPOffsetTP = 0;
    int32_t BottomFieldPOffsetTP = 1;

    int32_t TopFieldPOffsetBP = 2;
    int32_t BottomFieldPOffsetBP = 1;


    uint32_t SCoef = (fieldPrediction)?1:0; // in case of interlace fields we should use half of frame height


    // minimal distance from current point for interpolation.
    int32_t left = 1;
    int32_t right = interpolateInfo->sizeFrame.width - (interpolateInfo->sizeBlock.width + 2);
    int32_t top = 1; //<< shift;

    int32_t bottom = (interpolateInfo->sizeFrame.height >> SCoef)  - (interpolateInfo->sizeBlock.height + 2);
    int32_t   paddingLeft    = (position.x < left) ?  left  - position.x: 0;
    int32_t   paddingRight   = (position.x > right)?  position.x - right: 0;
    int32_t   paddingTop     = (position.y < top)?    (top - position.y): 0;
    int32_t   paddingBottom  = (position.y > bottom)? (position.y - bottom): 0;
    int32_t   RefBlockStep = interpolateInfo->srcStep << shift;
    const uint8_t*   pRefBlock =    interpolateInfo->pSrc[0] + (position.x + paddingLeft - 1) +
                            (position.y + paddingTop  - 1)* RefBlockStep;
    const uint8_t* pLUT[2];

    if (isPredBottom)
    {
        pLUT[0] = (fieldPrediction)?pLUTBottom:pLUTTop;
        pLUT[1] = pLUTBottom;
    }
    else
    {
        pLUT[1] = (fieldPrediction)?pLUTTop:pLUTBottom;
        pLUT[0] = pLUTTop;
    }

    if ((position.y + paddingTop  - 1) % 2)
    {
        const uint8_t* pTemp;
        pTemp = pLUT[0];
        pLUT[0] = pLUT[1];
        pLUT[1] = pTemp;
    }

    if (OppositePadding == 0)
    {
        isPredBottom = 0;
    }



    temp_block_size.width  -=  (paddingLeft + paddingRight);
    temp_block_size.height -=  (paddingTop + paddingBottom);

    // all block out of plane
    if (temp_block_size.height <= 0)
    {
        const uint8_t* pTemp;
        temp_block_size.height = 0;
        if (paddingTop != 0)
        {
            // see paddingBottom
            paddingTop = (interpolateInfo->sizeBlock.height + 3);
            if ((OppositePadding)&(position.y % 2 != 0))
            {
                TopFieldPOffsetTP = 1;
                BottomFieldPOffsetTP = 0;
                pTemp = pLUT[0];
                pLUT[0] = pLUT[1];
                pLUT[1] = pTemp;
            }
        }
        if (paddingBottom != 0)
        {
            paddingBottom = (interpolateInfo->sizeBlock.height + 3);
            if ((OppositePadding)&(position.y % 2 == 0))
            {
                TopFieldPOffsetBP = 1;
                BottomFieldPOffsetBP = 2;
                pTemp = pLUT[0];
                pLUT[0] = pLUT[1];
                pLUT[1] = pTemp;
            }
        }

    }
    if (temp_block_size.width <= 0)
    {
        int32_t FieldPOffset[2] = {0,0};
        int32_t posy;



        if ((OppositePadding)&&(!fieldPrediction))
        {
            if (paddingTop)
            {
                if ((position.y - 1) % 2)
                {
                    FieldPOffset[0] = 1;
                    FieldPOffset[1] = 0;
                }
                else
                {
                    FieldPOffset[0] = 0;
                    FieldPOffset[1] = 1;
                }

            }
            else if (paddingBottom)
            {
                if ((position.y - 1) % 2)
                {
                    FieldPOffset[0] = 0;
                    FieldPOffset[1] = -1;
                }
                else
                {
                    FieldPOffset[0] = -1;
                    FieldPOffset[1] = 0;
                }
            }

        }
        // Compare with paddingRight case !!!!
        if (paddingLeft)
        {
            for (int nfield = 0; nfield < 2; nfield ++)
            {
                for (int i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                {
                    posy = position.y + i - 1;
                    if (GetEdgeValue(interpolateInfo->sizeFrame.height >> SCoef, posy))
                        posy += FieldPOffset[nfield];

                    temp_block[i*temp_block_step + interpolateInfo->sizeBlock.width + 3] = (pLUT [nfield])?
                                pLUT [nfield][interpolateInfo->pSrc[0][RefBlockStep * posy]]:
                                interpolateInfo->pSrc[0][RefBlockStep * posy];
                }

            }
            paddingLeft = (paddingLeft != 0)?(interpolateInfo->sizeBlock.width + 3):paddingLeft;
        }
        if (paddingRight)
        {
            for (int nfield = 0; nfield < 2; nfield ++)
            {
                for (int i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                {
                    posy = position.y + i - 1;
                    if (GetEdgeValue(interpolateInfo->sizeFrame.height >> SCoef, posy))
                        posy += FieldPOffset[nfield];

                    temp_block[i*temp_block_step] = (pLUT [nfield])?
                    pLUT [nfield][interpolateInfo->pSrc[0][RefBlockStep * posy + interpolateInfo->sizeFrame.width - 1]]:
                                  interpolateInfo->pSrc[0][RefBlockStep * posy + interpolateInfo->sizeFrame.width - 1];
                }

            }
            paddingRight = (paddingRight != 0)?(interpolateInfo->sizeBlock.width + 2):paddingRight;
        }
    }

    // prepare dst
    p_temp_block_data_pointer += paddingTop * temp_block_step + paddingLeft;
    {
         // copy block
        for (int nfield = 0; nfield < 2; nfield ++)
        {
            for (int i = nfield; i < temp_block_size.height; i+=2)
            {
                for (int j = 0; j < temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[i*temp_block_step + j] = (pLUT [nfield])?
                        pLUT[nfield][pRefBlock[i*RefBlockStep + j]]:
                    pRefBlock[i*RefBlockStep + j];
                }
            }
        }
    }
    if ((!OppositePadding)||(fieldPrediction))
    {
        if (!OppositePadding)
        {
            // interlace padding. Field prediction
            // top
            for (int i = -paddingTop; i < 0; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {

                    p_temp_block_data_pointer[i*temp_block_step + j] = (pLUT[0])?
                        pLUT[0][interpolateInfo->pSrc[0][position.x + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep]; //!!! - isPredBottom*interpolateInfo->srcStep]; ///in case of isPredBottom we should use th second string
                }
            }
            // bottom
            for (int i = 0; i < paddingBottom; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUT[1])?
                        pLUT[1][interpolateInfo->pSrc[0][position.x + ((interpolateInfo->sizeFrame.height >> SCoef) - 1)*RefBlockStep + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + ((interpolateInfo->sizeFrame.height >> SCoef) - 1)*RefBlockStep + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep];
                }
            }
        }
        else
        {
            // progressive padding. Field prediction
            // top
            for (int i = -paddingTop; i < 0; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {

                    p_temp_block_data_pointer[i*temp_block_step + j] = (pLUTTop)?
                        pLUTTop[interpolateInfo->pSrc[0][position.x + paddingLeft +j -left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + paddingLeft +j -left - isPredBottom*interpolateInfo->srcStep];
                }
            }
            // bottom
            for (int i = 0; i < paddingBottom; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUTBottom)?
                        pLUTBottom[interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - 1)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - 1)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep];
                }
            }
        }
    }
    else
    {
        // Field  padding. Progressive prediction
        if (paddingTop)
        {
            // top
            for (int i = 2; i <= paddingTop; i +=2)
            {
                for (int j = 0; j < temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[-i*temp_block_step + j] = (pLUT[0])?
                        pLUT[0][interpolateInfo->pSrc[0][position.x + paddingLeft + j +TopFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep]]:
                    interpolateInfo->pSrc[0][position.x + paddingLeft + j + TopFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep];
                }
            }
            for (int i = 1; i <= paddingTop; i +=2)
            {
                for (int j = 0; j < temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[-i*temp_block_step + j] = (pLUT[1])?
                        pLUT[1][interpolateInfo->pSrc[0][position.x + paddingLeft +j + BottomFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep]]:
                    interpolateInfo->pSrc[0][position.x + paddingLeft +j + BottomFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep];
                }
            }
        }
        if (paddingBottom)
        {
            if (paddingBottom % 2)
            {
                const uint8_t* pTemp;
                pTemp = pLUT[0];
                pLUT[0] = pLUT[1];
                pLUT[1] = pTemp;
            }
            // bottom
            for (int i = 0; i < paddingBottom; i +=2)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUT[1])?
                        pLUT[1][interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - TopFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j- left - isPredBottom*RefBlockStep]]:
                    interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - TopFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j- left - isPredBottom*RefBlockStep];
                }
            }
            for (int i = 1; i < paddingBottom; i +=2)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUT[0])?
                        pLUT[0][interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - BottomFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*RefBlockStep]]:
                    interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - BottomFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*RefBlockStep];
                }
            }
        }
    }

    for (int i = 0; i < paddingLeft; i ++)
    {
        for (int j = 0; j < interpolateInfo->sizeBlock.height + 3; j++)
        {
            temp_block [i+ j*temp_block_step ] = temp_block[paddingLeft+ j*temp_block_step];
        }
    }
    for (int i = 1; i <=paddingRight; i ++)
    {
        for (int j = 0; j < interpolateInfo->sizeBlock.height + 3; j++)
        {
            temp_block [interpolateInfo->sizeBlock.width + 3 - i + j*temp_block_step + step] =
                temp_block[ interpolateInfo->sizeBlock.width + 3 - paddingRight  - 1 + j*temp_block_step + step];
        }
    }

    IppVCInterpolate_8u inter_struct;
    inter_struct.srcStep = 19;
    inter_struct.pSrc = temp_block + 1 + inter_struct.srcStep;
    inter_struct.pDst = interpolateInfo->pDst[0];
    inter_struct.dstStep = interpolateInfo->dstStep;
    inter_struct.dx = interpolateInfo->pointVector.x;
    inter_struct.dy = interpolateInfo->pointVector.y;
    inter_struct.roundControl = RoundControl;
    inter_struct.roiSize.width = interpolateInfo->sizeBlock.width;
    inter_struct.roiSize.height = interpolateInfo->sizeBlock.height;

    return _own_ippiInterpolateQPBicubic_VC1_8u_C1R(&inter_struct);
}


IppStatus own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R(const IppVCInterpolateBlock_8u* interpolateInfo,
                                                      const   uint8_t*                 pLUTTop,
                                                      const   uint8_t*                 pLUTBottom,
                                                      uint32_t                          OppositePadding,
                                                      uint32_t                          fieldPrediction,
                                                      uint32_t                          RoundControl,
                                                      uint32_t                          isPredBottom)

{
    uint8_t    temp_block [19*19]          = {0};
    mfxSize temp_block_size             = {interpolateInfo->sizeBlock.width + 3, interpolateInfo->sizeBlock.height + 3};
    int32_t   temp_block_step             = 19;
    uint8_t*   p_temp_block_data_pointer   = temp_block;
    uint8_t    shift = (fieldPrediction)? 1:0;

    IppiPoint position = {interpolateInfo->pointBlockPos.x,
        interpolateInfo->pointBlockPos.y};

    uint32_t step = 0;

    int32_t TopFieldPOffsetTP = 0;
    int32_t BottomFieldPOffsetTP = 1;

    int32_t TopFieldPOffsetBP = 2;
    int32_t BottomFieldPOffsetBP = 1;


    uint32_t SCoef = (fieldPrediction)?1:0; // in case of interlace fields we should use half of frame height


    // minimal distance from current point for interpolation.
    int32_t left = 1;
    int32_t right = interpolateInfo->sizeFrame.width - (interpolateInfo->sizeBlock.width + 2);
    int32_t top = 1; //<< shift;

    int32_t bottom = (interpolateInfo->sizeFrame.height >> SCoef)  - (interpolateInfo->sizeBlock.height + 2);
    int32_t   paddingLeft    = (position.x < left) ?  left  - position.x: 0;
    int32_t   paddingRight   = (position.x > right)?  position.x - right: 0;
    int32_t   paddingTop     = (position.y < top)?    (top - position.y): 0;
    int32_t   paddingBottom  = (position.y > bottom)? (position.y - bottom): 0;
    int32_t   RefBlockStep = interpolateInfo->srcStep << shift;
    const uint8_t*   pRefBlock =    interpolateInfo->pSrc[0] + (position.x + paddingLeft - 1) +
                            (position.y + paddingTop  - 1)* RefBlockStep;
    const uint8_t* pLUT[2];
    if (isPredBottom)
    {
        pLUT[0] = (fieldPrediction)?pLUTBottom:pLUTTop;
        pLUT[1] = pLUTBottom;
    }
    else
    {
        pLUT[1] = (fieldPrediction)?pLUTTop:pLUTBottom;
        pLUT[0] = pLUTTop;
    }

    if ((position.y + paddingTop - 1) % 2)
    {
        const uint8_t* pTemp;
        pTemp = pLUT[0];
        pLUT[0] = pLUT[1];
        pLUT[1] = pTemp;
    }

    if (OppositePadding == 0)
    {
        isPredBottom = 0;
    }


    temp_block_size.width  -=  (paddingLeft + paddingRight);
    temp_block_size.height -=  (paddingTop + paddingBottom);

    // all block out of plane
    if (temp_block_size.height <= 0)
    {
        const uint8_t* pTemp;
        temp_block_size.height = 0;
        if (paddingTop != 0)
        {
            // see paddingBottom
            paddingTop = (interpolateInfo->sizeBlock.height + 3);
            if ((OppositePadding)&(position.y % 2 != 0))
            {
                TopFieldPOffsetTP = 1;
                BottomFieldPOffsetTP = 0;
                pTemp = pLUT[0];
                pLUT[0] = pLUT[1];
                pLUT[1] = pTemp;
            }
        }
        if (paddingBottom != 0)
        {
            paddingBottom = (interpolateInfo->sizeBlock.height + 3);
            if ((OppositePadding)&(position.y % 2 == 0))
            {
                TopFieldPOffsetBP = 1;
                BottomFieldPOffsetBP = 2;
                pTemp = pLUT[0];
                pLUT[0] = pLUT[1];
                pLUT[1] = pTemp;
            }
        }

    }

    if (temp_block_size.width <= 0)
    {
        int32_t FieldPOffset[2] = {0,0};
        int32_t posy;



        if ((OppositePadding)&&(!fieldPrediction))
        {
            if ((OppositePadding)&&(!fieldPrediction))
            {
                if (paddingTop)
                {
                    if ((position.y - 1) % 2)
                    {
                        FieldPOffset[0] = 1;
                        FieldPOffset[1] = 0;
                    }
                    else
                    {
                        FieldPOffset[0] = 0;
                        FieldPOffset[1] = 1;
                    }

                }
                else if (paddingBottom)
                {
                    if ((position.y - 1) % 2)
                    {
                        FieldPOffset[0] = 0;
                        FieldPOffset[1] = -1;
                    }
                    else
                    {
                        FieldPOffset[0] = -1;
                        FieldPOffset[1] = 0;
                    }
                }

            }

        }

        // Compare with paddingRight case !!!!
        if (paddingLeft)
        {
            for (int nfield = 0; nfield < 2; nfield ++)
            {
                for (int i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                {
                    posy = position.y + i - 1;
                    if (GetEdgeValue(interpolateInfo->sizeFrame.height >> SCoef, posy))
                        posy += FieldPOffset[nfield];

                    temp_block[i*temp_block_step + interpolateInfo->sizeBlock.width + 3] = (pLUT [nfield])?
                        pLUT [nfield][interpolateInfo->pSrc[0][RefBlockStep * posy]]:
                    interpolateInfo->pSrc[0][RefBlockStep * posy];
                }

            }
            paddingLeft = (paddingLeft != 0)?(interpolateInfo->sizeBlock.width + 3):paddingLeft;
        }
        if (paddingRight)
        {
            for (int nfield = 0; nfield < 2; nfield ++)
            {
                for (int i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                {
                    posy = position.y + i - 1;
                    if (GetEdgeValue(interpolateInfo->sizeFrame.height >> SCoef, posy))
                        posy += FieldPOffset[nfield];

                    temp_block[i*temp_block_step] = (pLUT [nfield])?
                        pLUT [nfield][interpolateInfo->pSrc[0][RefBlockStep * posy + interpolateInfo->sizeFrame.width - 1]]:
                    interpolateInfo->pSrc[0][RefBlockStep * posy + interpolateInfo->sizeFrame.width - 1];
                }

            }
            paddingRight = (paddingRight != 0)?(interpolateInfo->sizeBlock.width + 2):paddingRight;
        }
    }

    // prepare dst
    p_temp_block_data_pointer += paddingTop * temp_block_step + paddingLeft;
    {
         // copy block
        for (int nfield = 0; nfield < 2; nfield ++)
        {
            for (int i = nfield; i < temp_block_size.height; i+=2)
            {
                for (int j = 0; j < temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[i*temp_block_step + j] = (pLUT [nfield])?
                        pLUT[nfield][pRefBlock[i*RefBlockStep + j]]:
                    pRefBlock[i*RefBlockStep + j];
                }
            }
        }
    }

    if ((!OppositePadding)||(fieldPrediction))
    {
        if (!OppositePadding)
        {
            // interlace padding. Field prediction
            // top
            for (int i = -paddingTop; i < 0; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {

                    p_temp_block_data_pointer[i*temp_block_step + j] = (pLUT[0])?
                        pLUT[0][interpolateInfo->pSrc[0][position.x + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep]; //!!! - isPredBottom*interpolateInfo->srcStep]; ///in case of isPredBottom we should use th second string
                }
            }
            // bottom
            for (int i = 0; i < paddingBottom; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUT[1])?
                        pLUT[1][interpolateInfo->pSrc[0][position.x + ((interpolateInfo->sizeFrame.height >> SCoef) - 1)*RefBlockStep + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + ((interpolateInfo->sizeFrame.height >> SCoef) - 1)*RefBlockStep + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep];
                }
            }
        }
        else
        {
            // progressive padding. Field prediction
            // top
            for (int i = -paddingTop; i < 0; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {

                    p_temp_block_data_pointer[i*temp_block_step + j] = (pLUTTop)?
                        pLUTTop[interpolateInfo->pSrc[0][position.x + paddingLeft +j -left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + paddingLeft +j -left - isPredBottom*interpolateInfo->srcStep];
                }
            }
            // bottom
            for (int i = 0; i < paddingBottom; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUTBottom)?
                        pLUTBottom[interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - 1)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - 1)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep];
                }
            }
        }
    }
    else
    {

        // Field padding. Progressive prediction
        if (paddingTop)
        {
        // top
        for (int i = 2; i <= paddingTop; i +=2)
        {
            for (int j = 0; j < temp_block_size.width; j++)
            {
                p_temp_block_data_pointer[-i*temp_block_step + j] = (pLUT[0])?
                    pLUT[0][interpolateInfo->pSrc[0][position.x + paddingLeft + j +TopFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep]]:
                interpolateInfo->pSrc[0][position.x + paddingLeft + j + TopFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep];
            }
        }
        for (int i = 1; i <= paddingTop; i +=2)
        {
            for (int j = 0; j < temp_block_size.width; j++)
            {
                p_temp_block_data_pointer[-i*temp_block_step + j] = (pLUT[1])?
                    pLUT[1][interpolateInfo->pSrc[0][position.x + paddingLeft +j + BottomFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep]]:
                interpolateInfo->pSrc[0][position.x + paddingLeft +j + BottomFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep];
            }
        }
        }
        if (paddingBottom)
        {
            if (paddingBottom % 2)
            {
                const uint8_t* pTemp;
                pTemp = pLUT[0];
                pLUT[0] = pLUT[1];
                pLUT[1] = pTemp;
            }
            // bottom
            for (int i = 0; i < paddingBottom; i +=2)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUT[1])?
                        pLUT[1][interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - TopFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j- left - isPredBottom*RefBlockStep]]:
                    interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - TopFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j- left - isPredBottom*RefBlockStep];
                }
            }
            for (int i = 1; i < paddingBottom; i +=2)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUT[0])?
                        pLUT[0][interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - BottomFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*RefBlockStep]]:
                    interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - BottomFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*RefBlockStep];
                }
            }
        }
    }

    for (int i = 0; i < paddingLeft; i ++)
    {
        for (int j = 0; j < interpolateInfo->sizeBlock.height + 3; j++)
        {
            temp_block [i+ j*temp_block_step ] = temp_block[paddingLeft+ j*temp_block_step];
        }
    }
    for (int i = 1; i <=paddingRight; i ++)
    {
        for (int j = 0; j < interpolateInfo->sizeBlock.height + 3; j++)
        {
            temp_block [interpolateInfo->sizeBlock.width + 3 - i + j*temp_block_step + step] =
                temp_block[ interpolateInfo->sizeBlock.width + 3 - paddingRight  - 1 + j*temp_block_step + step];
        }
    }
    // prapere data for interpolation call
    IppVCInterpolate_8u inter_struct;
    inter_struct.srcStep = 19;
    inter_struct.pSrc = temp_block + 1 + inter_struct.srcStep;
    inter_struct.pDst = interpolateInfo->pDst[0];
    inter_struct.dstStep = interpolateInfo->dstStep;
    inter_struct.dx = interpolateInfo->pointVector.x;
    inter_struct.dy = interpolateInfo->pointVector.y;
    inter_struct.roundControl = RoundControl;
    inter_struct.roiSize.width = interpolateInfo->sizeBlock.width;
    inter_struct.roiSize.height = interpolateInfo->sizeBlock.height;
    return _own_ippiInterpolateQPBilinear_VC1_8u_C1R(&inter_struct);
}

IppStatus own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R_NV12(const IppVCInterpolateBlock_8u* interpolateInfo,
                                                      const   uint8_t*                 pLUTTop,
                                                      const   uint8_t*                 pLUTBottom,
                                                      uint32_t                          OppositePadding,
                                                      uint32_t                          fieldPrediction,
                                                      uint32_t                          RoundControl,
                                                      uint32_t                          isPredBottom)

{
    uint8_t    temp_block [38*19]          = {0};
    mfxSize temp_block_size             = {(interpolateInfo->sizeBlock.width + 3)*2, interpolateInfo->sizeBlock.height + 3};
    int32_t   temp_block_step             = 38;
    uint8_t*   p_temp_block_data_pointer   = temp_block;
    uint8_t    shift = (fieldPrediction)? 1:0;

    IppiPoint position = {interpolateInfo->pointBlockPos.x, interpolateInfo->pointBlockPos.y};

    int32_t TopFieldPOffsetTP = 0;
    int32_t BottomFieldPOffsetTP = 1;

    int32_t TopFieldPOffsetBP = 2;
    int32_t BottomFieldPOffsetBP = 1;


    uint32_t SCoef = (fieldPrediction)?1:0; // in case of interlace fields we should use half of frame height


    // minimal distance from current point for interpolation.
    int32_t left = 2;
    int32_t right = interpolateInfo->sizeFrame.width*2 - (interpolateInfo->sizeBlock.width + 2)*2;
    int32_t top = 1;

    int32_t bottom = (interpolateInfo->sizeFrame.height >> SCoef)  - (interpolateInfo->sizeBlock.height + 2);

    int32_t   paddingLeft    = (position.x < left) ?  left  - position.x : 0;
    int32_t   paddingRight   = (position.x > right)?  position.x - right : 0;
    int32_t   paddingTop     = (position.y < top)?    (top - position.y): 0;
    int32_t   paddingBottom  = (position.y > bottom)? (position.y - bottom): 0;

    int32_t   RefBlockStep = interpolateInfo->srcStep << shift; //step for UV plane

    const uint8_t*   pRefBlockU =    interpolateInfo->pSrc[0] + (position.x + paddingLeft - 2) +
                                  (position.y + paddingTop  - 1)* RefBlockStep;

    const uint8_t*   pRefBlockV =    interpolateInfo->pSrc[0] + 1 + (position.x + paddingLeft - 2) +
                                  (position.y + paddingTop  - 1)* RefBlockStep;
    const uint8_t* pLUT[2];
    if (isPredBottom)
    {
        pLUT[0] = (fieldPrediction)?pLUTBottom:pLUTTop;
        pLUT[1] = pLUTBottom;
    }
    else
    {
        pLUT[1] = (fieldPrediction)?pLUTTop:pLUTBottom;
        pLUT[0] = pLUTTop;
    }

    if ((position.y + paddingTop - 1) % 2)
    {
        const uint8_t* pTemp;
        pTemp = pLUT[0];
        pLUT[0] = pLUT[1];
        pLUT[1] = pTemp;
    }

    if (OppositePadding == 0)
    {
        isPredBottom = 0;
    }

    temp_block_size.width  -=  (paddingLeft + paddingRight);
    temp_block_size.height -=  (paddingTop + paddingBottom);

    // all block out of plane
    if (temp_block_size.height <= 0)
    {
        const uint8_t* pTemp;
        temp_block_size.height = 0;
        if (paddingTop != 0)
        {
            // see paddingBottom
            paddingTop = (interpolateInfo->sizeBlock.height + 3);
            if ((OppositePadding)&(position.y % 2 != 0))
            {
                TopFieldPOffsetTP = 1;
                BottomFieldPOffsetTP = 0;
                pTemp = pLUT[0];
                pLUT[0] = pLUT[1];
                pLUT[1] = pTemp;
            }
        }
        if (paddingBottom != 0)
        {
            paddingBottom = (interpolateInfo->sizeBlock.height + 3);
            if ((OppositePadding)&(position.y % 2 == 0))
            {
                TopFieldPOffsetBP = 1;
                BottomFieldPOffsetBP = 2;
                pTemp = pLUT[0];
                pLUT[0] = pLUT[1];
                pLUT[1] = pTemp;
            }
        }
    }

    if (temp_block_size.width <= 0)
    {
        int32_t FieldPOffset[2] = {0,0};
        int32_t posy;

        if ((OppositePadding)&&(!fieldPrediction))
        {
            if ((OppositePadding)&&(!fieldPrediction))
            {
                if (paddingTop)
                {
                    if ((position.y - 1) % 2)
                    {
                        FieldPOffset[0] = 1;
                        FieldPOffset[1] = 0;
                    }
                    else
                    {
                        FieldPOffset[0] = 0;
                        FieldPOffset[1] = 1;
                    }
                }
                else if (paddingBottom)
                {
                    if ((position.y - 1) % 2)
                    {
                        FieldPOffset[0] = 0;
                        FieldPOffset[1] = -1;
                    }
                    else
                    {
                        FieldPOffset[0] = -1;
                        FieldPOffset[1] = 0;
                    }
                }
            }
        }

        // Compare with paddingRight case !!!!
        if (paddingLeft)
        {
            for (int nfield = 0; nfield < 2; nfield ++)
            {
                for (int i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                {
                    posy = position.y + i - 1;

                    if (GetEdgeValue(interpolateInfo->sizeFrame.height >> SCoef, posy))
                        posy += FieldPOffset[nfield];

                    temp_block[i*temp_block_step + (interpolateInfo->sizeBlock.width + 3)*2] = (pLUT [nfield])?
                        pLUT [nfield][interpolateInfo->pSrc[0][RefBlockStep * posy]]:
                    interpolateInfo->pSrc[0][RefBlockStep * posy];

                    temp_block[i*temp_block_step + (interpolateInfo->sizeBlock.width + 3)*2 + 1] = (pLUT [nfield])?
                        pLUT [nfield][interpolateInfo->pSrc[0][RefBlockStep * posy + 1]]:
                    interpolateInfo->pSrc[0][RefBlockStep * posy + 1];
                }

            }
            paddingLeft = (paddingLeft != 0)?(interpolateInfo->sizeBlock.width + 3):paddingLeft;
        }

        if (paddingRight)
        {
            for (int nfield = 0; nfield < 2; nfield ++)
            {
                for (int i = nfield; i < interpolateInfo->sizeBlock.height + 3; i+=2)
                {
                    posy = position.y + i - 1;
                    if (GetEdgeValue(interpolateInfo->sizeFrame.height >> SCoef, posy))
                        posy += FieldPOffset[nfield];

                    temp_block[i*temp_block_step] = (pLUT [nfield])?
                        pLUT [nfield][interpolateInfo->pSrc[0][RefBlockStep * posy + interpolateInfo->sizeFrame.width - 1]]:
                    interpolateInfo->pSrc[0][RefBlockStep * posy + interpolateInfo->sizeFrame.width - 1];

                    temp_block[i*temp_block_step + 1] = (pLUT [nfield])?
                        pLUT [nfield][interpolateInfo->pSrc[0][RefBlockStep * posy + interpolateInfo->sizeFrame.width - 1 + 1]]:
                    interpolateInfo->pSrc[0][RefBlockStep * posy + interpolateInfo->sizeFrame.width - 1 + 1];
                }

            }
            paddingRight = (paddingRight != 0)?(interpolateInfo->sizeBlock.width + 2):paddingRight;
        }
    }

    // prepare dst
    p_temp_block_data_pointer += paddingTop * temp_block_step + paddingLeft;
    {
         // copy block
        for (int nfield = 0; nfield < 2; nfield ++)
        {
            for (int i = nfield; i < temp_block_size.height; i+=2)
            {
                for (int j = 0; j < temp_block_size.width; j+=2)
                {
                    p_temp_block_data_pointer[i*temp_block_step + j] = (pLUT [nfield])?
                                        pLUT[nfield][pRefBlockU[i*RefBlockStep + j]]:
                                    pRefBlockU[i*RefBlockStep + j];

                     p_temp_block_data_pointer[i*temp_block_step + j + 1] = (pLUT [nfield])?
                                        pLUT[nfield][pRefBlockV[i*RefBlockStep + j]]:
                                    pRefBlockV[i*RefBlockStep + j];
               }
            }
        }
    }

    if ((!OppositePadding)||(fieldPrediction))
    {
        if (!OppositePadding)
        {
            // interlace padding. Field prediction
            // top
            for (int i = -paddingTop; i < 0; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[i*temp_block_step + j] = (pLUT[0])?
                        pLUT[0][interpolateInfo->pSrc[0][position.x + paddingLeft + j - left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + paddingLeft  + j - left - isPredBottom*interpolateInfo->srcStep]; 
                }
            }
            // bottom
            for (int i = 0; i < paddingBottom; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUT[1])?
                        pLUT[1][interpolateInfo->pSrc[0][position.x + ((interpolateInfo->sizeFrame.height >> SCoef) - 1)*RefBlockStep + paddingLeft + j - left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + ((interpolateInfo->sizeFrame.height >> SCoef) - 1)*RefBlockStep + paddingLeft + j - left - isPredBottom*interpolateInfo->srcStep];
                }
            }
        }
        else
        {
            // progressive padding. Field prediction
            // top
            for (int i = -paddingTop; i < 0; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {

                    p_temp_block_data_pointer[i*temp_block_step + j] = (pLUTTop)?
                        pLUTTop[interpolateInfo->pSrc[0][position.x + paddingLeft + j -left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + paddingLeft + j -left - isPredBottom*interpolateInfo->srcStep];
                }
            }
            // bottom
            for (int i = 0; i < paddingBottom; i ++)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUTBottom)?
                        pLUTBottom[interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - 1)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep]]:
                    interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - 1)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*interpolateInfo->srcStep];
                }
            }
        }
    }
    else
    {

        // Field padding. Progressive prediction
        if (paddingTop)
        {
        // top
        for (int i = 2; i <= paddingTop; i +=2)
        {
            for (int j = 0; j < temp_block_size.width; j++)
            {
                p_temp_block_data_pointer[-i*temp_block_step + j] = (pLUT[0])?
                    pLUT[0][interpolateInfo->pSrc[0][position.x + paddingLeft + j +TopFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep]]:
                interpolateInfo->pSrc[0][position.x + paddingLeft + j + TopFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep];
            }
        }
        for (int i = 1; i <= paddingTop; i +=2)
        {
            for (int j = 0; j < temp_block_size.width; j++)
            {
                p_temp_block_data_pointer[-i*temp_block_step + j] = (pLUT[1])?
                    pLUT[1][interpolateInfo->pSrc[0][position.x + paddingLeft + j + BottomFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep]]:
                interpolateInfo->pSrc[0][position.x + paddingLeft + j + BottomFieldPOffsetTP*interpolateInfo->srcStep - left - isPredBottom*RefBlockStep];
            }
        }
        }
        if (paddingBottom)
        {
            if (paddingBottom % 2)
            {
                const uint8_t* pTemp;
                pTemp = pLUT[0];
                pLUT[0] = pLUT[1];
                pLUT[1] = pTemp;
            }
            // bottom
            for (int i = 0; i < paddingBottom; i +=2)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUT[1])?
                        pLUT[1][interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - TopFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j- left - isPredBottom*RefBlockStep]]:
                    interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - TopFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j- left - isPredBottom*RefBlockStep];
                }
            }
            for (int i = 1; i < paddingBottom; i +=2)
            {
                for (int j = 0; j <temp_block_size.width; j++)
                {
                    p_temp_block_data_pointer[(temp_block_size.height+i)*temp_block_step + j] = (pLUT[0])?
                        pLUT[0][interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - BottomFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*RefBlockStep]]:
                    interpolateInfo->pSrc[0][position.x + (interpolateInfo->sizeFrame.height - BottomFieldPOffsetBP)*interpolateInfo->srcStep + paddingLeft +j - left - isPredBottom*RefBlockStep];
                }
            }
        }
    }

    for (int i = 0; i < paddingLeft; i ++)
    {
        for (int j = 0; j < interpolateInfo->sizeBlock.height + 3; j++)
        {
            temp_block [i + j*temp_block_step] = temp_block[paddingLeft + j*temp_block_step + i];
        }
    }

    for (int i = 2; i <=paddingRight; i +=2)
    {
        for (int j = 0; j < interpolateInfo->sizeBlock.height + 3; j++)
        {
            temp_block [(interpolateInfo->sizeBlock.width + 3)*2 - i + 1 + j*temp_block_step] =
                temp_block[ (interpolateInfo->sizeBlock.width + 3)*2 - paddingRight  - 2 + 1 + j*temp_block_step];

            temp_block [(interpolateInfo->sizeBlock.width + 3)*2 - i + j*temp_block_step] =
                temp_block[ (interpolateInfo->sizeBlock.width + 3)*2 - paddingRight  - 2 + j*temp_block_step];
        }
    }

    // prapere data for interpolation call
    IppVCInterpolate_8u inter_struct;
    inter_struct.srcStep = 38;
    inter_struct.pSrc = temp_block + 2 + inter_struct.srcStep;
    inter_struct.pDst = interpolateInfo->pDst[0];
    inter_struct.dstStep = interpolateInfo->dstStep;
    inter_struct.dx = interpolateInfo->pointVector.x;
    inter_struct.dy = interpolateInfo->pointVector.y;
    inter_struct.roundControl = RoundControl;
    inter_struct.roiSize.width = interpolateInfo->sizeBlock.width;
    inter_struct.roiSize.height = interpolateInfo->sizeBlock.height;
    return _own_ippiInterpolateQPBilinear_VC1_8u_C1R_NV12(&inter_struct);
}


IppStatus CalculateCorrelationBlock_8u_P1R (  uint8_t *pSrc, uint32_t srcStep,
                                              uint8_t *pRef, uint32_t refStep,
                                              mfxSize roiSize, mfxSize blockSize,
                                              CorrelationParams * pOutPut)
{

    pOutPut->correlation = 0;
    pOutPut->k = 0;
    pOutPut->b = 0;

    double X = 0;
    double Y = 0;
    double XX = 0;
    double YY = 0;
    double XY = 0;

    uint32_t nBlocks = 0;

    for (int i = 0; i < roiSize.height - blockSize.height + 1; i+= blockSize.height)
    {
        uint8_t *pSrcRow = pSrc + (srcStep*i);
        uint8_t *pRefRow = pRef + (refStep*i);

        for (int j = 0; j < roiSize.width - blockSize.width + 1; j+= blockSize.width)
        {
            double Xi = 0;
            double Yi = 0;
            ippiMean_8u_C1R(pSrcRow + j, srcStep, blockSize, &Xi);
            ippiMean_8u_C1R(pRefRow + j, refStep, blockSize, &Yi);
            if (Xi == 0 || Xi == 255 || Yi == 0 || Yi == 255)
                continue;

            X += Xi;
            Y += Yi;
            XX += Xi*Xi;
            YY += Yi*Yi;
            XY += Xi*Yi;

            nBlocks++;

        }
    }
    if (nBlocks==0)
    {
        return ippStsNoErr;
    }

    X= X/nBlocks;
    Y= Y/nBlocks;
    XX = XX/nBlocks;
    YY = YY/nBlocks;
    XY = XY/nBlocks;

    if (XX!=X*X && YY != Y*Y)
    {
        pOutPut->correlation = (XY - X*Y) / sqrt((XX-X*X)*(YY-Y*Y));
        pOutPut->k           = (XY - X*Y) / (XX-X*X);

        pOutPut->k =(pOutPut->k > 1.5)? 1.48:pOutPut->k;
        pOutPut->k =(pOutPut->k < 0.5)? 0.05:pOutPut->k;

        pOutPut->b           = Y - pOutPut->k*X;

        pOutPut->b = (pOutPut->b > 31.0)? 31.0:pOutPut->b;
        pOutPut->b = (pOutPut->b <-31.0)? -31.0:pOutPut->b;

    }

    return ippStsNoErr;
}
bool CalculateIntesityCompensationParams (CorrelationParams sCorrParams, uint32_t &LUMSCALE, uint32_t &LUMSHIFT)
{
 

    if ((sCorrParams.correlation > 0.97) &&
       ((sCorrParams.k >   1 + (3.0/64.0) ) || 
        (sCorrParams.k <   1 - (3.0/64.0) )  || 
        (sCorrParams.b >   2.0)   || 
        (sCorrParams.b <  -2.0)))
    {
        
        LUMSCALE =  (uint32_t) (sCorrParams.k*64.0 - 31.0);
        LUMSHIFT =  ((uint32_t)(sCorrParams.b + ((sCorrParams.b<0)? -0.5:0.5))) & 0x3F;


        if (LUMSCALE < 64 && LUMSHIFT < 64) 
        {
           return true;  
        }
        else
        {
            assert (0);
        }        

    }
    return false;
}
void get_iScale_iShift (uint32_t LUMSCALE, uint32_t LUMSHIFT, int32_t &iScale, int32_t &iShift) 
{
    if (LUMSCALE == 0)
    {
        iScale = - 64;
        iShift = 255 * 64 - LUMSHIFT *2 * 64;
        if (LUMSHIFT > 31)
            iShift += 128 * 64;
    }
    else 
    {
        iScale = LUMSCALE + 32;
        if (LUMSHIFT > 31)
            iShift = LUMSHIFT * 64 - 64 * 64;
        else
            iShift = LUMSHIFT * 64;
    }   
}
void CreateICTable (int32_t iScale, int32_t iShift, int32_t* LUTY, int32_t* LUTUV)
{
    int i,j;
    // build LUTs

    for (i = 0; i < 257; i++)
    {
        j = (iScale * i + iShift + 32) >> 6;

        if (j > 255)
        {
            j = 255;
        }
        else if (j < 0)
        {
            j = 0;
        }        
        LUTY[i] = j;

        j = (iScale * (i - 128) + 128 * 64 + 32) >>6;

        if (j > 255)
        {
            j = 255;
        }
        else if (j < 0)
        {
            j = 0;
        }        
        LUTUV[i] = j;
    }
}
IppStatus   _own_ippiReplicateBorder_8u_C1R  (  uint8_t * pSrc,  int srcStep, 
                                              mfxSize srcRoiSize, mfxSize dstRoiSize, 
                                              int topBorderWidth,  int leftBorderWidth)
{
    // upper
    uint8_t* pBlock = pSrc;
    for (int i = 0; i < topBorderWidth; i++)
    {
        MFX_INTERNAL_CPY(pBlock - srcStep, pBlock, srcRoiSize.width);
        pBlock -= srcStep;
    }

    // bottom
    pBlock = pSrc + (srcRoiSize.height  - 1)*srcStep;

    for (int i = 0; i < dstRoiSize.height - srcRoiSize.height - topBorderWidth; i++ )
    {
        MFX_INTERNAL_CPY(pBlock + srcStep, pBlock, srcRoiSize.width);
        pBlock += srcStep;
    }

    // left
    pBlock = pSrc - srcStep * topBorderWidth;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        memset(pBlock-leftBorderWidth,pBlock[0],leftBorderWidth);
        pBlock += srcStep;
    }

    //right

    pBlock = pSrc - srcStep * topBorderWidth + srcRoiSize.width - 1;
    int32_t rightY = dstRoiSize.width - srcRoiSize.width - leftBorderWidth;
    rightY = (rightY>0)? rightY : 0;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        memset(pBlock+1,pBlock[0],rightY);
        pBlock += srcStep;
    }
    return ippStsNoErr;
}
IppStatus   _own_ippiReplicateBorder_16u_C1R  ( uint16_t * pSrc,  int srcStep, 
                                               mfxSize srcRoiSize, mfxSize dstRoiSize, 
                                               int topBorderWidth,  int leftBorderWidth)
{
    // upper
    uint8_t* pBlock = (uint8_t*) pSrc;
    for (int i = 0; i < topBorderWidth; i++)
    {
        MFX_INTERNAL_CPY(pBlock - srcStep, pBlock, srcRoiSize.width*sizeof(uint16_t));
        pBlock -= srcStep;
    }

    // bottom
    pBlock = (uint8_t*)pSrc + (srcRoiSize.height  - 1)*srcStep;

    for (int i = 0; i < dstRoiSize.height - srcRoiSize.height - topBorderWidth; i++ )
    {
        MFX_INTERNAL_CPY(pBlock + srcStep, pBlock, srcRoiSize.width*sizeof(uint16_t));
        pBlock += srcStep;
    }

    // left
    pBlock = (uint8_t*)(pSrc) - srcStep * topBorderWidth;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        ippsSet_16s (*(int16_t*)pBlock, (int16_t*)pBlock-leftBorderWidth,leftBorderWidth);
        pBlock += srcStep;
    }

    //right

    pBlock = (uint8_t*)(pSrc + srcRoiSize.width - 1) - srcStep * topBorderWidth ;
    int32_t rightY = dstRoiSize.width - srcRoiSize.width - leftBorderWidth;
    rightY = (rightY>0)? rightY : 0;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        ippsSet_16s (*(int16_t*)pBlock, ((int16_t*)pBlock)+ 1,rightY);
        pBlock += srcStep;
    }
    return ippStsNoErr;
}
}
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
