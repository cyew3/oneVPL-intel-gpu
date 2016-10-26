//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

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

UMC::Status SetFieldPlane(Ipp8u* pFieldPlane[3],  Ipp8u*  pPlane[3], Ipp32u planeStep[3], bool bBottom, Ipp32s nRow)
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

UMC::Status SetFieldPlane(Ipp8u** pFieldPlane,  Ipp8u*  pPlane, Ipp32u planeStep, bool bBottom, Ipp32s nRow)
{
     if(pPlane)
      *pFieldPlane = pPlane + planeStep *(bBottom + (nRow<<4));
   else
         return UMC::UMC_ERR_NULL_PTR;
     return UMC::UMC_OK;
}

UMC::Status   SetFieldStep(Ipp32u fieldStep[3], Ipp32u planeStep[3])
{
    fieldStep[0] = planeStep[0] <<1;
    fieldStep[1] = planeStep[1] <<1;
    fieldStep[2] = planeStep[2] <<1;

    return UMC::UMC_OK;
}

UMC::Status   SetFieldStep(Ipp32s* fieldStep, Ipp32u planeStep)
{
    fieldStep[0] = planeStep <<1;
    return UMC::UMC_OK;
}

UMC::Status   Set1RefFrwFieldPlane(Ipp8u* pPlane[3],       Ipp32u planeStep[3],
                                   Ipp8u* pFrwPlane[3],    Ipp32u frwStep[3],
                                   Ipp8u* pRaisedPlane[3], Ipp32u raisedStep[3],
                                   eReferenceFieldType     uiReferenceFieldType,
                                   bool bSecondField,      bool bBottom, Ipp32s nRow)
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


UMC::Status   Set1RefFrwFieldPlane(Ipp8u** pPlane,       Ipp32s* planeStep,
                                   Ipp8u* pFrwPlane,    Ipp32u frwStep,
                                   Ipp8u* pRaisedPlane, Ipp32u raisedStep,
                                   eReferenceFieldType     uiReferenceFieldType,
                                   bool bSecondField,      bool bBottom, Ipp32s nRow)
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

UMC::Status   Set2RefFrwFieldPlane(Ipp8u* pPlane[2][3], Ipp32u planeStep[2][3],
                                   Ipp8u* pFrwPlane[3], Ipp32u frwStep[3],
                                   Ipp8u* pRaisedPlane[3], Ipp32u raisedStep[3],
                                   bool bSecondField, bool bBottom, Ipp32s nRow)
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

UMC::Status   Set2RefFrwFieldPlane(Ipp8u** pPlane1, Ipp32s* planeStep1,
                                   Ipp8u** pPlane2, Ipp32s* planeStep2,
                                   Ipp8u* pFrwPlane, Ipp32u frwStep,
                                   Ipp8u* pRaisedPlane, Ipp32u raisedStep,
                                   bool bSecondField, bool bBottom, Ipp32s nRow)
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

UMC::Status   SetBkwFieldPlane(Ipp8u* pPlane[2][3], Ipp32u planeStep[2][3],
                               Ipp8u* pBkwPlane[3], Ipp32u bkwStep[3],
                               bool bBottom, Ipp32s nRow)
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

UMC::Status   SetBkwFieldPlane(Ipp8u** pPlane1, Ipp32s* planeStep1,
                               Ipp8u** pPlane2, Ipp32s* planeStep2,
                               Ipp8u* pBkwPlane, Ipp32u bkwStep,
                               bool bBottom, Ipp32s nRow)
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

static const Ipp16s TableFwdTransform8x8[64] =
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

static const Ipp16s TableFwdTransform4x4[16] =
{
    15420,  15420,      15420,      15420,
    19751,  8978,       -8978,      -19751,
    15420,  -15420,     -15420,     15420,
    8978 ,  -19751,     19751 ,     -8978

};

#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
#define _A(x) (((x)<0)?-(x):(x))
IppStatus _own_FilterDeblockingLuma_VerEdge_VC1_8u_C1IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    Ipp8u* pRPixel;
    Ipp16s p1, p2, p3, p4, p5, p6, p7, p8;
    Ipp16s a0, a1, a2, a3;
    Ipp16s clip, d;
    Ipp32s i;
    Ipp32s count;

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
                a3 = IPP_MIN(_A(a1), _A(a2));
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
                    pRPixel[-1] = (Ipp8u)(p4 - d);
                    pRPixel[0] = (Ipp8u)(p5 + d);
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
                    a3 = IPP_MIN(_A(a1), _A(a2));
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
                        pRPixel[-1] = (Ipp8u)(p4 - d);
                        pRPixel[0] = (Ipp8u)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

IppStatus _own_FilterDeblockingChroma_VerEdge_VC1_8u_C1IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag)
{
    static Ipp32s EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    IppStatus ret = ippStsNoErr;
    Ipp8u* pRPixel;
    Ipp16s p1, p2, p3, p4, p5, p6, p7, p8;
    Ipp16s a0, a1, a2, a3;
    Ipp16s clip, d;
    Ipp32s i;
    Ipp32s count;

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
                a3 = IPP_MIN(_A(a1), _A(a2));
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
                    pRPixel[-1] = (Ipp8u)(p4 - d);
                    pRPixel[0] = (Ipp8u)(p5 + d);
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
                    a3 = IPP_MIN(_A(a1), _A(a2));
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
                        pRPixel[-1] = (Ipp8u)(p4 - d);
                        pRPixel[0] = (Ipp8u)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

IppStatus _own_FilterDeblockingLuma_HorEdge_VC1_8u_C1IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    Ipp8u* pRPixel;
    Ipp16s p1, p2, p3, p4, p5, p6, p7, p8;
    Ipp16s a0, a1, a2, a3;
    Ipp16s clip, d;
    Ipp32s i;
    Ipp32s count;

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
                a3 = IPP_MIN(_A(a1), _A(a2));
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
                    pRPixel[-1*srcdstStep] = (Ipp8u)(p4 - d);
                    pRPixel[0] = (Ipp8u)(p5 + d);
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
                    a3 = IPP_MIN(_A(a1), _A(a2));
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
                        pRPixel[-1*srcdstStep] = (Ipp8u)(p4 - d);
                        pRPixel[0] = (Ipp8u)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}


IppStatus _own_FilterDeblockingChroma_HorEdge_VC1_8u_C1IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,Ipp32s EdgeDisabledFlag)
{
    IppStatus ret = ippStsNoErr;
    static Ipp32s EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    Ipp8u* pRPixel;
    Ipp16s p1, p2, p3, p4, p5, p6, p7, p8;
    Ipp16s a0, a1, a2, a3;
    Ipp16s clip, d;
    Ipp32s i;
    Ipp32s count;

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
                a3 = IPP_MIN(_A(a1), _A(a2));
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
                    pRPixel[-1*srcdstStep] = (Ipp8u)(p4 - d);
                    pRPixel[0] = (Ipp8u)(p5 + d);
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
                    a3 = IPP_MIN(_A(a1), _A(a2));
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
                        pRPixel[-1*srcdstStep] = (Ipp8u)(p4 - d);
                        pRPixel[0] = (Ipp8u)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}

#undef _A

Ipp32s SubBlockPattern(VC1Block* _pBlk, VC1SingletonBlock* _sBlk)
{
    Ipp32s subbpattern = 0;
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
IppStatus _own_ippiTransformBlock8x8Fwd_VC1_16s_C1R(Ipp16s* pSrcDst, Ipp32u srcDstStep)
{
    int                 i,j,l;
    Ipp32s              TempBlock[8][8];
    Ipp16s*             pBlock              = pSrcDst;
    const Ipp16s*       pCoef               = TableFwdTransform8x8;
    Ipp32s              temp                = 0;

    for (i=0; i<8 ;i++)
    {
        for (j=0;j<8;j++)
        {
            temp = 1<<(12-1);
            for (l=0; l<8; l++)
            {
                temp += pBlock[l]*pCoef[l];
            }

            TempBlock[i][j] = (Ipp16s)(temp>>12);
            pCoef +=8;
        }
        pCoef       = TableFwdTransform8x8;
        pBlock = (Ipp16s*)((Ipp8u*)pBlock + srcDstStep);
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

            *(Ipp16s*)((Ipp8u*)pBlock + i*srcDstStep) = (Ipp16s)(temp>>20);
            pCoef +=8;
        }
        pCoef       = TableFwdTransform8x8;
        pBlock     += 1;
    }
    return ippStsNoErr;
}

// 4 colums x 8 rows
IppStatus _own_ippiTransformBlock4x8Fwd_VC1_16s_C1R(Ipp16s* pSrcDst, Ipp32u srcDstStep)
{
    int                 i,j,l;
    Ipp32s              TempBlock[8][4];
    Ipp16s*             pBlock              = pSrcDst;
    const Ipp16s*       pCoef               = TableFwdTransform4x4;
    Ipp32s              temp                = 0;

    for (i=0; i<8 ;i++)
    {
        for (j=0;j<4;j++)
        {
            temp = 1<<(11-1);
            for (l=0; l<4; l++)
            {
                temp += pBlock[l]*pCoef[l];
            }

            TempBlock[i][j] = (Ipp16s)(temp>>11);
            pCoef +=4;
        }
        pCoef       = TableFwdTransform4x4;
        pBlock = (Ipp16s*)((Ipp8u*)pBlock + srcDstStep);
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

            *(Ipp16s*)((Ipp8u*)pBlock + i*srcDstStep) = (Ipp16s)(temp>>20);
            pCoef +=8;
        }
        pCoef       = TableFwdTransform8x8;
        pBlock     += 1;
    }
    return ippStsNoErr;
}

// 8 colums x 4 rows
IppStatus _own_ippiTransformBlock8x4Fwd_VC1_16s_C1R(Ipp16s* pSrcDst, Ipp32u srcDstStep)
{
    int                 i,j,l;
    Ipp32s              TempBlock[4][8];
    Ipp16s*             pBlock              = pSrcDst;
    const Ipp16s*       pCoef               = TableFwdTransform8x8;
    Ipp32s              temp                = 0;

    for (i=0; i<4 ;i++)
    {
        for (j=0;j<8;j++)
        {
            temp = 1<<(12-1);
            for (l=0; l<8; l++)
            {
                temp += pBlock[l]*pCoef[l];
            }

            TempBlock[i][j] = (Ipp16s)(temp>>12);
            pCoef +=8;
        }
        pCoef       = TableFwdTransform8x8;
        pBlock = (Ipp16s*)((Ipp8u*)pBlock + srcDstStep);
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

            *(Ipp16s*)((Ipp8u*)pBlock + i*srcDstStep) = (Ipp16s)(temp>>19);
            pCoef +=4;
        }
        pCoef       = TableFwdTransform4x4;
        pBlock     += 1;
    }
    return ippStsNoErr;
}

// 4 colums x 4 rows
IppStatus _own_ippiTransformBlock4x4Fwd_VC1_16s_C1R(Ipp16s* pSrcDst, Ipp32u srcDstStep)
{
    int                 i,j,l;
    Ipp32s              TempBlock[4][4];
    Ipp16s*             pBlock              = pSrcDst;
    const Ipp16s*       pCoef               = TableFwdTransform4x4;
    Ipp32s              temp                = 0;

    for (i=0; i<4 ;i++)
    {
        for (j=0;j<4;j++)
        {
            temp = 1<<(11-1);
            for (l=0; l<4; l++)
            {
                temp += pBlock[l]*pCoef[l];
            }

            TempBlock[i][j] = (Ipp16s)(temp>>11);
            pCoef +=4;
        }
        pCoef       = TableFwdTransform4x4;
        pBlock = (Ipp16s*)((Ipp8u*)pBlock + srcDstStep);
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

            *(Ipp16s*)((Ipp8u*)pBlock + i*srcDstStep) = (Ipp16s)(temp>>19);
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
IppStatus _own_FilterDeblockingChroma_NV12_VerEdge_VC1_8u_C2IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,
                                                               Ipp32s EdgeDisabledFlagU, Ipp32s EdgeDisabledFlagV)
{
    static Ipp32s EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    IppStatus ret = ippStsNoErr;
    Ipp8u *pRPixelU = NULL, *pRPixelV = NULL;
    Ipp16s p1 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0, p7 = 0, p8 = 0;
    Ipp16s a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    Ipp16s clip = 0, d = 0;
    Ipp32s i = 0;
    Ipp32s count = 0;

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
                a3 = IPP_MIN(_A(a1), _A(a2));
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
                    pRPixelU[-2] = (Ipp8u)(p4 - d);
                    pRPixelU[0]       = (Ipp8u)(p5 + d);
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
                    a3 = IPP_MIN(_A(a1), _A(a2));
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
                        pRPixelU[-2] = (Ipp8u)(p4 - d);
                        pRPixelU[0]       = (Ipp8u)(p5 + d);
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
                a3 = IPP_MIN(_A(a1), _A(a2));
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
                    pRPixelV[-2] = (Ipp8u)(p4 - d);
                    pRPixelV[0] = (Ipp8u)(p5 + d);
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
                    a3 = IPP_MIN(_A(a1), _A(a2));
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
                        pRPixelV[-2] = (Ipp8u)(p4 - d);
                        pRPixelV[0]  = (Ipp8u)(p5 + d);
                    }
                }
            }
        }
    }
    return ret;
}
IppStatus _own_FilterDeblockingChroma_NV12_HorEdge_VC1_8u_C2IR(Ipp8u* pSrcDst,Ipp32s pQuant, Ipp32s srcdstStep,
                                                               Ipp32s EdgeDisabledFlagU, Ipp32s EdgeDisabledFlagV)
{
    IppStatus ret = ippStsNoErr;
    static Ipp32s EdgeTable[2] = {IPPVC_EDGE_HALF_1,IPPVC_EDGE_HALF_2};
    Ipp8u *pRPixelU = NULL, *pRPixelV = NULL;
    Ipp16s p1 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0, p7 = 0, p8 = 0;
    Ipp16s a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    Ipp16s clip = 0, d = 0;
    Ipp32s i = 0;
    Ipp32s count = 0;
    
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
                a3 = IPP_MIN(_A(a1), _A(a2));
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
                    pRPixelU[-1*srcdstStep] = (Ipp8u)(p4 - d);
                    pRPixelU[0] = (Ipp8u)(p5 + d);
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
                    a3 = IPP_MIN(_A(a1), _A(a2));
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
                        pRPixelU[-1*srcdstStep] = (Ipp8u)(p4 - d);
                        pRPixelU[0] = (Ipp8u)(p5 + d);
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
                a3 = IPP_MIN(_A(a1), _A(a2));
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
                    pRPixelV[-1*srcdstStep] = (Ipp8u)(p4 - d);
                    pRPixelV[0] = (Ipp8u)(p5 + d);
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
                    a3 = IPP_MIN(_A(a1), _A(a2));
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
                        pRPixelV[-1*srcdstStep] = (Ipp8u)(p4 - d);
                        pRPixelV[0] = (Ipp8u)(p5 + d);
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
IppStatus _own_ippiQuantIntraUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant)
{
    int i,j;
    Ipp16s* pBlock = pSrcDst;
    Ipp32s intra = 1;
    for(i = 0; i < 8; i++)
    {
        for (j = intra; j < 8; j++)
        {

            pBlock[j] = pBlock[j]/doubleQuant;

        }
        intra  = 0;
        pBlock = (Ipp16s*)((Ipp8u*) pBlock + srcDstStep);
    }

    return ippStsNoErr;
}

IppStatus _own_ippiQuantIntraNonUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant)
{
    int i,j;
    Ipp16s* pBlock = pSrcDst;
    Ipp8u   Quant  = doubleQuant>>1;
    Ipp32s intra = 1;

    for(i = 0; i < 8; i++)
    {
        for (j = intra; j < 8; j++)
        {
            pBlock[j] = (pBlock[j]-_sign(pBlock[j])*Quant)/doubleQuant;
        }
        intra  = 0;
        pBlock = (Ipp16s*)((Ipp8u*) pBlock + srcDstStep);
    }
    return ippStsNoErr;
}

IppStatus _own_ippiQuantInterUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant, IppiSize roiSize)
{
    int i,j;
    Ipp16s* pBlock = pSrcDst;

    for(i = 0; i < roiSize.height; i++)
    {
        for (j = 0; j < roiSize.width; j++)
        {

            pBlock[j] = pBlock[j]/doubleQuant;

        }
        pBlock = (Ipp16s*)((Ipp8u*) pBlock + srcDstStep);
    }

    return ippStsNoErr;
}

IppStatus _own_ippiQuantInterNonUniform(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant, IppiSize roiSize)
{
    int i,j;
    Ipp16s* pBlock = pSrcDst;
    Ipp8u   Quant  = doubleQuant>>1;

    for(i = 0; i < roiSize.height; i++)
    {
        for (j = 0; j < roiSize.width; j++)
        {
            pBlock[j] = (pBlock[j]-_sign(pBlock[j])*Quant)/doubleQuant;
        }

        pBlock = (Ipp16s*)((Ipp8u*) pBlock + srcDstStep);
    }
    return ippStsNoErr;
}
#undef _sign
#endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //UMC_RESTRICTED_CODE

#define _sign(x) (((x)<0)?-1:!!(x))
#define _abs(x)   ((x<0)? -(x):(x))
#define _mask  0x03
IppStatus _own_ippiQuantIntraTrellis(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant, Ipp8s *pRoundControl,Ipp32s roundControlStep)
{
    int i,j;
    Ipp16s* pBlock = pSrcDst;
    Ipp32s intra = 1;

    for(i = 0; i < 8; i++)
    {
        for (j = intra; j < 8; j++)
        {
            Ipp16s abs_pixel    = _abs(pBlock[j]);
            Ipp16s round        = (pRoundControl[j])*doubleQuant;
            Ipp16s quant_pixel  = 0;

            quant_pixel = (round < abs_pixel)?(abs_pixel - round)/doubleQuant:0;

            pBlock[j] = (pBlock[j]<0)? -quant_pixel:(Ipp16s)quant_pixel;
        }
        intra  = 0;
        pBlock = (Ipp16s*)((Ipp8u*) pBlock + srcDstStep);
        pRoundControl = pRoundControl + roundControlStep;
    }
    return ippStsNoErr;
}
IppStatus _own_ippiQuantInterTrellis(Ipp16s* pSrcDst, Ipp32s srcDstStep,Ipp32s doubleQuant, IppiSize roiSize,Ipp8s *pRoundControl, Ipp32s roundControlStep)
{
    int i,j;
    Ipp16s* pBlock = pSrcDst;

    for(i = 0; i < roiSize.height; i++)
    {
        for (j = 0; j < roiSize.width; j++)
        {
            Ipp16s abs_pixel    = _abs(pBlock[j]);
            Ipp16s round        = (pRoundControl[j])*doubleQuant;
            Ipp16s quant_pixel  = 0;

            quant_pixel = (round < abs_pixel)?(abs_pixel - round)/doubleQuant:0;

            pBlock[j] = (pBlock[j]<0)? -quant_pixel:(Ipp16s)quant_pixel;
        }
        pBlock = (Ipp16s*)((Ipp8u*) pBlock + srcDstStep);
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
IppStatus _own_ippiQuantInvIntraUniform_VC1_16s_C1R(const Ipp16s* pSrc, Ipp32s srcStep,
                                                     Ipp16s* pDst, Ipp32s dstStep,
                                                  Ipp32s doubleQuant, IppiSize* pDstSizeNZ)
{
    Ipp32s i = 1;
    Ipp32s j = 0;
    Ipp32s X[8] = {0};
    Ipp32s Y[8] = {0};
    Ipp16s S;

    const Ipp16s* pSrc1 = pSrc;
    Ipp16s* pDst1 = pDst;

    for(j = 0; j < 8; j++)
    {
       for(i; i < 8; i++)
       {
           pDst1[i] = (Ipp16s)(pSrc1[i]*doubleQuant);
           S = !pDst1[i] ;
           X[i] = X[i] + S;
           Y[j] = Y[j] + S;
       }
       i = 0;
       pSrc1 = (Ipp16s*)((Ipp8u*)pSrc1 + srcStep);
       pDst1 = (Ipp16s*)((Ipp8u*)pDst1 + dstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}

IppStatus _own_ippiQuantInvIntraNonUniform_VC1_16s_C1R(const Ipp16s* pSrc, Ipp32s srcStep,
                                                        Ipp16s* pDst, Ipp32s dstStep,
                                                          Ipp32s doubleQuant, IppiSize* pDstSizeNZ)
{
    Ipp32s i = 1;
    Ipp32s j = 0;
    const Ipp16s* pSrc1 = pSrc;
    Ipp16s* pDst1 = pDst;

    Ipp32s X[8] = {0};
    Ipp32s Y[8] = {0};
    Ipp16s S;

    for(j = 0; j < 8; j++)
    {
        for(i; i < 8; i++)
        {
            pDst1[i] = (Ipp16s)(pSrc1[i]*doubleQuant) + (Ipp16s)(_sign(pSrc1[i])*(doubleQuant>>1));
            S = !pDst1[i] ;
            X[i] = X[i] + S;
            Y[j] = Y[j] + S;
        }
        i = 0;
        pSrc1 = (Ipp16s*)((Ipp8u*)pSrc1 + srcStep);
        pDst1 = (Ipp16s*)((Ipp8u*)pDst1 + dstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}
IppStatus _own_ippiQuantInvInterUniform_VC1_16s_C1R(const Ipp16s* pSrc, Ipp32s srcStep,
                                                     Ipp16s* pDst, Ipp32s dstStep,
                                                       Ipp32s doubleQuant, IppiSize roiSize,
                                                       IppiSize* pDstSizeNZ)
{
    Ipp32s i = 0;
    Ipp32s j = 0;
    Ipp32s X[8] = {0};
    Ipp32s Y[8] = {0};
    Ipp16s S;

    const Ipp16s* pSrc1 = pSrc;
    Ipp16s* pDst1 = pDst;

    for(j = 0; j < roiSize.height; j++)
    {
       for(i = 0; i < roiSize.width; i++)
       {
           pDst1[i] = (Ipp16s)(pSrc1[i]*doubleQuant);
           S = !pDst1[i] ;
           X[i] = X[i] + S;
           Y[j] = Y[j] + S;
       }
       pSrc1 = (Ipp16s*)((Ipp8u*)pSrc1 + srcStep);
       pDst1 = (Ipp16s*)((Ipp8u*)pDst1 + dstStep);
    }

    for(i=7; i>=0 && X[i]==8; i--);

    pDstSizeNZ->width = i+1;

    for(j=7; j>=0 && Y[j]==8; j--);

    pDstSizeNZ->height = j+1;

    return ippStsNoErr;
}
IppStatus _own_ippiQuantInvInterNonUniform_VC1_16s_C1R(const Ipp16s* pSrc, Ipp32s srcStep,
                                                        Ipp16s* pDst, Ipp32s dstStep,
                                                          Ipp32s doubleQuant, IppiSize roiSize,
                                                          IppiSize* pDstSizeNZ)
{
    Ipp32s i = 0;
    Ipp32s j = 0;
    const Ipp16s* pSrc1 = pSrc;
    Ipp16s* pDst1 = pDst;
    Ipp32s X[8] = {0};
    Ipp32s Y[8] = {0};
    Ipp16s S;


    for(j = 0; j < roiSize.height; j++)
    {
        for(i = 0; i < roiSize.width; i++)
        {
            pDst1[i] = (Ipp16s)(pSrc1[i]*doubleQuant) + (Ipp16s)(_sign(pSrc1[i])*(doubleQuant>>1));
            S = !pDst1[i] ;
            X[i] = X[i] + S;
            Y[j] = Y[j] + S;
        }

        pSrc1 = (Ipp16s*)((Ipp8u*)pSrc1 + srcStep);
        pDst1 = (Ipp16s*)((Ipp8u*)pDst1 + dstStep);
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

Ipp8u Get_CBPCY(Ipp32u MBPatternCur, Ipp32u CBPCYTop, Ipp32u CBPCYLeft, Ipp32u CBPCYULeft)
{

    Ipp32u LT3   = (CBPCYULeft >> VC1_ENC_PAT_POS_Y3) & 0x01;
    Ipp32u T2    = (CBPCYTop   >> VC1_ENC_PAT_POS_Y2) & 0x01;
    Ipp32u T3    = (CBPCYTop   >> VC1_ENC_PAT_POS_Y3) & 0x01;
    Ipp32u L1    = (CBPCYLeft  >> VC1_ENC_PAT_POS_Y1) & 0x01;
    Ipp32u L3    = (CBPCYLeft  >> VC1_ENC_PAT_POS_Y3) & 0x01;
    Ipp32u CBPCY = MBPatternCur;
    Ipp32u Y0    = (CBPCY>>VC1_ENC_PAT_POS_Y0)&0x01;


    CBPCY ^=  (Ipp8u)(((LT3==T2)? L1:T2) << VC1_ENC_PAT_POS_Y0);
    CBPCY ^=  (Ipp8u)(((T2 ==T3)? Y0:T3) << VC1_ENC_PAT_POS_Y1);
    CBPCY ^=  (Ipp8u)(((L1 ==Y0)? L3:Y0) << VC1_ENC_PAT_POS_Y2);
    CBPCY ^=  (Ipp8u)(((Y0 ==((MBPatternCur>>VC1_ENC_PAT_POS_Y1)&0x01))?
                (MBPatternCur>>VC1_ENC_PAT_POS_Y2)&0x01:(MBPatternCur>>VC1_ENC_PAT_POS_Y1)&0x01) << VC1_ENC_PAT_POS_Y3);
    return (Ipp8u)CBPCY;
}



Ipp32s SumSqDiff_1x7_16s(Ipp16s* pSrc, Ipp32u step, Ipp16s* pPred)
{
    Ipp32s sum=0;

    for(int i=0; i<7 ;i++)
    {
        pPred  = (Ipp16s*)((Ipp8u*)pPred+step);
        pSrc  = (Ipp16s*)((Ipp8u*)pSrc+step);
        sum  += (pSrc[0]-pPred[0])*(pSrc[0]- pPred[0]) - pSrc[0]*pSrc[0];
    }
    return sum;
}
Ipp32s SumSqDiff_7x1_16s(Ipp16s* pSrc, Ipp16s* pPred)
{
    Ipp32s sum=0;

    for(int i=1; i<8 ;i++)
    {
        sum  += (pSrc[i]-pPred[i])*(pSrc[i]- pPred[i]) - pSrc[i]*pSrc[i];
    }
    return sum;
}

typedef void (*Diff8x8VC1) (const Ipp16s* pSrc1, Ipp32s src1Step,
              const Ipp16s* pSrc2, Ipp32s src2Step,
              Ipp16s* pDst,  Ipp32s dstStep, Ipp32u pred);

static void Diff8x8_NonPred(const Ipp16s* pSrc1, Ipp32s /*src1Step*/,
                            const Ipp16s* pSrc2, Ipp32s /*src2Step*/,
                            Ipp16s* pDst,  Ipp32s /*dstStep*/, Ipp32u /*pred*/)
{
    pDst[0] = pSrc1[0] - pSrc2[0];
}
static void Diff8x8_HorPred(const Ipp16s* pSrc1, Ipp32s /*src1Step*/,
              const Ipp16s* pSrc2, Ipp32s /*src2Step*/,
              Ipp16s* pDst,  Ipp32s /*dstStep*/, Ipp32u /*pred*/)
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

static void Diff8x8_VerPred(const Ipp16s* pSrc1, Ipp32s src1Step,
              const Ipp16s* pSrc2, Ipp32s src2Step,
              Ipp16s* pDst,  Ipp32s dstStep, Ipp32u /*pred*/)
{
    Ipp32s j;
    for(j=0;j<8;j++){
        (*pDst) = (*pSrc1) - (*pSrc2);
        pDst = (Ipp16s*)((Ipp8u*)pDst + dstStep);
        pSrc1 = (Ipp16s*)((Ipp8u*)pSrc1 + src1Step);
        pSrc2 = (Ipp16s*)((Ipp8u*)pSrc2 + src2Step);
    }
}

static void Diff8x8_Nothing(const Ipp16s* /*pSrc1*/, Ipp32s /*src1Step*/,
              const Ipp16s* /*pSrc2*/, Ipp32s /*src2Step*/,
              Ipp16s* /*pDst*/,  Ipp32s /*dstStep*/, Ipp32u /*pred*/)
{
}

Diff8x8VC1 Diff8x8FuncTab[] ={
    &Diff8x8_NonPred,
    &Diff8x8_VerPred,
    &Diff8x8_HorPred,
    &Diff8x8_Nothing
};

void Diff8x8 (const Ipp16s* pSrc1, Ipp32s src1Step,
              const Ipp16s* pSrc2, Ipp32s src2Step,
              Ipp16s* pDst,  Ipp32s dstStep, Ipp32u pred)
{
    Ipp32u index = pred & 0x03;
    IppiSize blkSize     = {8,8};

    ippiCopy_16s_C1R(pSrc1,src1Step,pDst,dstStep,blkSize);
    Diff8x8FuncTab[index](pSrc1,src1Step,pSrc2,src2Step,pDst,dstStep,pred);
}

 //---BEGIN------------------------Copy block, MB functions-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
IppStatus _own_Copy8x8_16x16_8u16s (const Ipp8u* pSrc, Ipp32u srcStep, Ipp16s* pDst, Ipp32u dstStep, IppiSize roiSize)
{
    for (int i = 0; i < roiSize.height; i++)
    {
        for(int j = 0; j < roiSize.width; j++)
        {
            pDst[j] = pSrc[j];
        }
        pSrc += srcStep;
        pDst = (Ipp16s*)((Ipp8u*)pDst + dstStep);
    }
    return ippStsNoErr;
}
#endif
#endif
//---END------------------------Copy block, MB functions-----------------------------------------------

//Ipp32u GetNumZeros(Ipp16s* pSrc, Ipp32u srcStep, bool bIntra)
//{
//    int         i,j;
//    Ipp32u      s=0;
//    Ipp16s*     pBlock = pSrc;
//
//    for(i = 0; i<8; i++)
//    {
//        for (j = bIntra; j<8; j++)
//        {
//            s+= !pBlock[j];
//        }
//        bIntra  = false;
//        pBlock = (Ipp16s*)((Ipp8u*) pBlock + srcStep);
//    }
//    //printf ("intra MB: num of coeff %d\n",64 - s - bIntra);
//    return s;
//}
//Ipp32u GetNumZeros(Ipp16s* pSrc, Ipp32u srcStep)
//{
//    int         i,j;
//    Ipp32u      s=0;
//    Ipp16s*     pBlock = pSrc;
//
//    for(i = 0; i<8; i++)
//    {
//        for (j = 0; j<8; j++)
//        {
//            s+= !pBlock[j];
//        }
//        pBlock = (Ipp16s*)((Ipp8u*) pBlock + srcStep);
//    }
//    //printf ("intra MB: num of coeff %d\n",64 - s);
//    return s;
//}
//Ipp8u GetBlockPattern(Ipp16s* pBlock, Ipp32u step)
//{
//    int i,j;
//    Ipp8u s[4]={0};
//    for (i=0;i<8;i++)
//    {
//        for(j=0;j<8;j++)
//        {
//            s[((i>>2)<<1)+(j>>2)] += !(pBlock[j]);
//        }
//        pBlock = (Ipp16s*)((Ipp8u*)pBlock + step);
//    }
//    //printf ("inter MB: num of coeff %d\n",64 - (s[0]+s[1]+s[2]+s[3]));
//    return (((s[0]<16)<<3)|((s[1]<16)<<2)|((s[2]<16)<<1)| (s[3]<16));
//
//}

Ipp8u GetMode( Ipp8u &run, Ipp16s &level, const Ipp8u *pTableDR, const Ipp8u *pTableDL, bool& sign)
{
    sign  = level < 0;
    level = (sign)? -level : level;
    if (run <= pTableDR[1])
    {
        Ipp8u maxLevel = pTableDL[run];
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
        Ipp8u maxRun = pTableDR[level];
        if (run <= (Ipp8u)(2*maxRun + 1)) // level starts from 0
        {
            run = run - (Ipp8u)maxRun - 1;
            return 2;
        }
    }
    return 3;
}
//bool GetRLCode(Ipp32s run, Ipp32s level, IppVCHuffmanSpec_32s *pTable, Ipp32s &code, Ipp32s &len)
//{
//    Ipp32s  maxRun = pTable[0] >> 20;
//    Ipp32s  addr;
//    Ipp32s  *table;
//
//    if(run > maxRun)
//    {
//       return false;
//    }
//    addr  = pTable[run + 1];
//    table = (Ipp32s*)((Ipp8s*)pTable + addr);
//    if(level <= table[0])
//    {
//        len  = *(table  + level) & 0x1f;
//        code = (*(table + level) >> 16);
//        return true;
//    }
//    return false;
//}
//Ipp8u        Zigzag       ( Ipp16s*                pBlock,
//                            Ipp32u                 blockStep,
//                            bool                   bIntra,
//                            const Ipp8u*           pScanMatrix,
//                            Ipp8u*                 pRuns,
//                            Ipp16s*                pLevels)
//{
//    Ipp32s      i = 0, pos = 0;
//    Ipp16s      value = 0;
//    Ipp8u       n_pairs = 0;
//
//    pRuns[n_pairs]  = 0;
//    for (i = bIntra; i<64; i++)
//    {
//        pos    = pScanMatrix[i];
//        value = *((Ipp16s*)((Ipp8u*)pBlock + blockStep*(pos/8)) + pos%8);
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
//Ipp8u GetLength_16s(Ipp16s value)
//{
//    int    i=0;
//    Ipp16u n = (Ipp16u)value;
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

IppStatus _own_Diff8x8C_16s(Ipp16s c, Ipp16s*  pBlock, Ipp32u   blockStep, IppiSize roiSize, int scaleFactor)
{
    int i,j;
    for (i=0;i<8;i++)
    {
        for (j=0;j<8;j++)
        {
            pBlock[j] -= c;
        }
        pBlock = (Ipp16s*)((Ipp8u*)pBlock + blockStep);
    }
    return ippStsNoErr;
}
IppStatus _own_Add8x8C_16s(Ipp16s c, Ipp16s*  pBlock, Ipp32u   blockStep, IppiSize roiSize, int scaleFactor)
{
    int i,j;
    for (i=0;i<8;i++)
    {
        for (j=0;j<8;j++)
        {
            pBlock[j] += c;
        }
        pBlock = (Ipp16s*)((Ipp8u*)pBlock + blockStep);
    }
    return ippStsNoErr;
}
#endif
#endif
//---End------------------------Add constant to block-----------------------------------------------

//---Begin------------------------Get difference-----------------------------------------------
#ifndef UMC_RESTRICTED_CODE
#ifdef _VC1_ENC_OWN_FUNCTIONS_
IppStatus _own_ippiGetDiff8x8_8u16s_C1(Ipp8u*  pBlock,       Ipp32u   blockStep,
                        Ipp8u*  pPredBlock,   Ipp32u   predBlockStep,
                        Ipp16s* pDst,         Ipp32u   dstStep,
                        Ipp16s* pDstPredictor,Ipp32s  dstPredictorStep,
                        Ipp32s  mcType,       Ipp32s  roundControl)
{
    int i,j;

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 8; j++)
        {
            pDst[j] = (pBlock[j] - pPredBlock[j]);
        }
        pDst = (Ipp16s*)((Ipp8u*)pDst + dstStep);
        pBlock     += blockStep;
        pPredBlock += predBlockStep;
    }
    return ippStsNoErr;
}
IppStatus _own_ippiGetDiff16x16_8u16s_C1(Ipp8u*  pBlock,       Ipp32u   blockStep,
                          Ipp8u*  pPredBlock,   Ipp32u   predBlockStep,
                          Ipp16s* pDst,         Ipp32u   dstStep,
                          Ipp16s* pDstPredictor,Ipp32s  dstPredictorStep,
                          Ipp32s  mcType,       Ipp32s  roundControl)
{
    int i,j;

    for (i = 0; i < 16; i++)
    {
        for (j = 0; j < 16; j++)
        {
            pDst[j] = (pBlock[j] - pPredBlock[j]);
        }
        pDst = (Ipp16s*)((Ipp8u*)pDst + dstStep);
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

IppStatus _own_Add16x16_8u16s(const Ipp8u*  pSrcRef,  Ipp32u   srcRefStep,
                         const Ipp16s* pSrcYData,Ipp32u   srcYDataStep,
                         Ipp8u* pDst, Ipp32u   dstStep,
                         Ipp32s mctype, Ipp32s roundControl)
{
    int i,j;
    //printf("\nLUMA\n");

    for (i = 0; i < 16; i++)
    {
        //printf("\n");
        for (j = 0; j < 16; j++)
        {
            pDst[j] = (Ipp8u)(VC1_CLIP(pSrcYData[j] + (Ipp16s)pSrcRef[j]));
            //printf(" %d", pDst[j]);
        }
        pSrcYData = (Ipp16s*)((Ipp8u*)pSrcYData + srcYDataStep);
        pSrcRef  += srcRefStep;
        pDst     = pDst + dstStep;
    }

    return ippStsNoErr;
}

IppStatus _own_Add8x8_8u16s ( const Ipp8u*  pSrcRef,  Ipp32u   srcRefStep,
                         const Ipp16s* pSrcYData,Ipp32u   srcYDataStep,
                         Ipp8u* pDst, Ipp32u   dstStep,
                         Ipp32s mctype, Ipp32s roundControl)
{
    int i,j;

    //printf("\nCHROMA\n");
    for (i = 0; i < 8; i++)
    {
        //printf("\n");
        for (j = 0; j < 8; j++)
        {
            pDst[j] = (Ipp8u)(VC1_CLIP(pSrcYData[j] + (Ipp16s)pSrcRef[j]));
            //printf(" %d", pDst[j]);
        }
        pSrcYData = (Ipp16s*)((Ipp8u*)pSrcYData + srcYDataStep);
        pSrcRef  += srcRefStep;
        pDst     = pDst + dstStep;
    }
    return ippStsNoErr;
}
#endif
#endif
//---End---------------------Motion compensation-----------------------------------------------

Ipp16s median3(Ipp16s a, Ipp16s b, Ipp16s c)
{
  Ipp32u d  = ((((Ipp32u)(a-b))&0x80000000) |
              ((((Ipp32u)(b-c))&0x80000000)>>1)|
              ((((Ipp32u)(a-c))&0x80000000)>>2));

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
Ipp16s median4(Ipp16s a, Ipp16s b, Ipp16s c, Ipp16s d)
{
  Ipp32u e  = ((((Ipp32u)(a-b))&0x80000000) |
              ((((Ipp32u)(b-c))&0x80000000)>>1)|
              ((((Ipp32u)(c-d))&0x80000000)>>2)|
              ((((Ipp32u)(a-c))&0x80000000)>>3)|
              ((((Ipp32u)(a-d))&0x80000000)>>4)|
              ((((Ipp32u)(b-d))&0x80000000)>>5));

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
Ipp16s VC1abs(Ipp16s value)
{
  Ipp16u s = value>>15;
  s = (value + s)^s;
  return s;
}

void InitScaleInfo(sScaleInfo* pInfo, bool bCurSecondField ,bool bBottom,
                   Ipp8u ReferenceFrameDist, Ipp8u MVRangeIndex)
{
    static Ipp16s SCALEOPP[2][4]   = {{128,     192,    213,    224},
                                      {128,     64,     43,     32}};
    static Ipp16s SCALESAME1[2][4] = {{512,     341,    307,    293},
                                      {512,     1024,   1563,   2048}};
    static Ipp16s SCALESAME2[2][4] = {{219,     236,    242,    245},
                                      {219,     204,    200,    198}};
    static Ipp16s SCALEZONEX1[2][4] = {{32,     48,     53,     56},
                                       {32,     16,     11,     8}};
    static Ipp16s SCALEZONEY1[2][4] = {{ 8,     12,     13,     14},
                                       { 8,     4,      3,      2}};
    static Ipp16s ZONEOFFSETX1[2][4] = {{37,    20,     14,     11},
                                        {37,    52,     56,     58}};
    static Ipp16s ZONEYOFFSET1[2][4] = {{10,    5,      4,      3},
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
                   Ipp8u ReferenceFrameDist, Ipp8u MVRangeIndex)
{
    static Ipp16s SCALEOPP[2][4]   = {{171,     205,    219,    288},
                                      {128,     64,     43,     32}};
    static Ipp16s SCALESAME1[2][4] = {{384,     320,    299,    288},
                                      {512,     1024,   1563,   2048}};
    static Ipp16s SCALESAME2[2][4] = {{230,     239,    244,    246},
                                      {219,     204,    200,    198}};
    static Ipp16s SCALEZONEX1[2][4] = {{43,     51,     55,     57},
                                       {32,     16,     11,     8}};
    static Ipp16s SCALEZONEY1[2][4] = {{11,     13,     14,     14},
                                       { 8,     4,      3,      2}};
    static Ipp16s ZONEOFFSETX1[2][4] = {{26,    17,     12,     10},
                                        {37,    52,     56,     58}};
    static Ipp16s ZONEYOFFSET1[2][4] = {{7,     4,      3,      3},
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
static Ipp16s scale_sameX(Ipp16s n,sScaleInfo* pInfo, bool bHalf)
{
    Ipp16s abs_n = VC1abs(n = (n>>((Ipp16u)bHalf)));
    Ipp32s s;
    if (abs_n>255)
    {
        return n<<((Ipp16u)bHalf);
    }
    else if (abs_n<pInfo->scale_zoneX)
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->scale_same1))>>8);
    }
    else
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->scale_same2))>>8);
        s = (n<0)? s - pInfo->zone_offsetX:s + pInfo->zone_offsetX;
    }
    s = (s>pInfo->rangeX-1)? pInfo->rangeX-1:s;
    s = (s<-pInfo->rangeX) ? -pInfo->rangeX :s;

    return (Ipp16s) (s<<((Ipp16u)bHalf));
}
static Ipp16s scale_oppX(Ipp16s n,sScaleInfo* pInfo, bool bHalf)
{
    Ipp32s s = (((Ipp32s)((n>>((Ipp32u)bHalf))*pInfo->scale_opp))>>8);
    return (Ipp16s) (s<<((Ipp16u)bHalf));
}
static Ipp16s scale_sameY(Ipp16s n,sScaleInfo* pInfo, bool bHalf)
{
    Ipp16s abs_n = VC1abs(n = (n>>((Ipp16u)bHalf)));
    Ipp32s s     = 0;
    if (abs_n>63)
    {
        return n<<((Ipp16u)bHalf);
    }
    else if (abs_n<pInfo->scale_zoneY)
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->scale_same1))>>8);
    }
    else
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->scale_same2))>>8);
        s = (n<0)? s - pInfo->zone_offsetY:s + pInfo->zone_offsetY;
    }
    s = (s>pInfo->rangeY/2-1)? pInfo->rangeY/2-1:s;
    s = (s<-pInfo->rangeY/2) ? -pInfo->rangeY/2 :s;

    return (Ipp16s) (s<<((Ipp16u)bHalf));
}
static Ipp16s scale_sameY_B(Ipp16s n,sScaleInfo* pInfo, bool bHalf)
{
    Ipp16s abs_n = VC1abs(n = (n>>((Ipp16u)bHalf)));
    Ipp32s s     = 0;
    if (abs_n>63)
    {
        return n<<((Ipp16u)bHalf);
    }
    else if (abs_n<pInfo->scale_zoneY)
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->scale_same1))>>8);
    }
    else
    {
        s = (Ipp16s)(((Ipp32s)(n*pInfo->scale_same2))>>8);
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
    return (Ipp16s) (s<<((Ipp16u)bHalf));
}
static Ipp16s scale_oppY(Ipp16s n,sScaleInfo* pInfo,  bool bHalf)
{
    Ipp32s s = (((Ipp32s)((n>>((Ipp16u)bHalf))*pInfo->scale_opp))>>8);

    return (Ipp16s) (s<<((Ipp16u)bHalf));

}
typedef Ipp16s (*fScaleX)(Ipp16s n, sScaleInfo* pInfo, bool bHalf);
typedef Ipp16s (*fScaleY)(Ipp16s n, sScaleInfo* pInfo, bool bHalf);

static fScaleX pScaleX[2][2] = {{scale_oppX, scale_sameX},
                                {scale_sameX,scale_oppX}};
static fScaleY pScaleY[2][2] = {{scale_oppY, scale_sameY},
                                {scale_sameY_B, scale_oppY}};

bool PredictMVField2(sCoordinate* predA,sCoordinate* predB,sCoordinate* predC,
                     sCoordinate* res, sScaleInfo* pInfo, bool bSecondField,
                     sCoordinate* predAEx,sCoordinate* predCEx, bool bBackward, bool bHalf)
{
    Ipp8u n = (Ipp8u)(((!predA)<<2) + ((!predB)<<1) + (!predC));
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
    Ipp8u n = (Ipp8u)(((!predA)<<2) + ((!predB)<<1) + (!predC));
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

void Copy8x8_16s(Ipp16s*  pSrc, Ipp32u   srcStep, Ipp16s*  pDst, Ipp32u   dstStep)
{
   int i,j;
    for (i=0;i<8;i++)
    {
        for (j=0;j<8;j++)
        {
            pDst[j] = pSrc[j];
        }
        pDst = (Ipp16s*)((Ipp8u*)pDst + dstStep);
        pSrc = (Ipp16s*)((Ipp8u*)pSrc + srcStep);
    }
}
//void Copy16x16_16s(Ipp16s*  pSrc, Ipp32u   srcStep, Ipp16s*  pDst, Ipp32u   dstStep)
//{
//   int i,j;
//    for (i=0;i<16;i++)
//    {
//        for (j=0;j<16;j++)
//        {
//            pDst[j] = pSrc[j];
//        }
//        pDst = (Ipp16s*)((Ipp8u*)pDst + dstStep);
//        pSrc = (Ipp16s*)((Ipp8u*)pSrc + srcStep);
//    }
//}

void ScalePredict(sCoordinate * MV, Ipp32s x,Ipp32s y,sCoordinate MVPredMin, sCoordinate MVPredMax)
{
    x += MV->x;
    y += MV->y;

    if (x < MVPredMin.x)
    {
        MV->x = MV->x - (Ipp16s)(x- MVPredMin.x);
    }
    else if (x > MVPredMax.x)
    {
        MV-> x = MV-> x - (Ipp16s)(x-MVPredMax.x);
    }

    if (y < MVPredMin.y)
    {
        MV->y = MV->y - (Ipp16s)(y - MVPredMin.y);
    }
    else if (y > MVPredMax.y)
    {
        MV->y = MV->y - (Ipp16s)(y - MVPredMax.y);
    }
}
Ipp8u HybridPrediction(     sCoordinate * mvPred,
                            const sCoordinate * MV,
                            const sCoordinate * mvA,
                            const sCoordinate * mvC,
                            Ipp32u        th)
{
    Ipp8u hybrid = 0;

    if (mvA && mvC)
    {
        Ipp32u sumA = VC1abs(mvA->x - mvPred->x) + VC1abs(mvA->y - mvPred->y);
        Ipp32u sumC = VC1abs(mvC->x - mvPred->x) + VC1abs(mvC->y - mvPred->y);

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
//Ipp32s FullSearch(const Ipp8u* pSrc, Ipp32u srcStep, const Ipp8u* pPred, Ipp32u predStep, sCoordinate Min, sCoordinate Max,sCoordinate * MV)
//{
//    Ipp32s x, y;
//    Ipp32s MVx=0, MVy=0;
//    Ipp32s sum  = 0x7FFFFFFF;
//    Ipp32s temp = 0;
//    Ipp32s yStep=1, xStep=1;
//    const Ipp8u* prediction = pPred;
//
//
//
//    for (y = Min.y; y <Max.y ; y+=yStep )
//    {
//        pPred = prediction + y*(Ipp32s)predStep;
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
//Ipp32s SumBlockDiffBPred16x16(const Ipp8u* pSrc, Ipp32u srcStep,const Ipp8u* pPred1, Ipp32u predStep1,
//                                                                const Ipp8u* pPred2, Ipp32u predStep2)
//{
//    Ipp32s x, y;
//    Ipp32s sum = 0;
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
//Ipp32s SumBlockDiff16x16(const Ipp8u* pSrc1, Ipp32u srcStep1,const Ipp8u* pSrc2, Ipp32u srcStep2)
//{
//    Ipp32s x, y;
//    Ipp32s sum = 0;
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
//void GetBlockType(Ipp16s* pBlock, Ipp32s step, Ipp8u Quant, eTransformType& type)
//{
//    int i,j;
//    Ipp32s s[4]={0};
//    bool vEdge[2]={0};
//    bool hEdge[3]={0};
//
//    for (i=0;i<8;i++)
//    {
//        for(j=0;j<8;j++)
//        {
//            s[((i>>2)<<1)+(j>>2)] += pBlock[j];
//        }
//        pBlock = (Ipp16s*)((Ipp8u*)pBlock + step);
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
//bool GetMBTSType(Ipp16s** ppBlock, Ipp32u* pStep, Ipp8u Quant /*doubleQuant*/, eTransformType* pTypes /*BlockTSTypes*/)
//{
//    Ipp8u num [4]           = {0};
//    Ipp8u max_num           = 0;
//    eTransformType max_type = VC1_ENC_8x8_TRANSFORM;
//    Ipp32s blk;
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
//Ipp32s SumBlock16x16(const Ipp8u* pSrc, Ipp32u srcStep)
//{
//    Ipp32s x, y;
//    Ipp32s sum = 0;
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


void  GetMVDirectHalf  (Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (Ipp16s)(((x*(scaleFactor-256)+255)>>9)<<1);
     mvB->y = (Ipp16s)(((y*(scaleFactor-256)+255)>>9)<<1);
     mvF->x = (Ipp16s)(((x*scaleFactor+255)>>9)<<1);
     mvF->y = (Ipp16s)(((y*scaleFactor+255)>>9)<<1);

}
void  GetMVDirectQuarter(Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (Ipp16s)((x*(scaleFactor-256)+128)>>8);
     mvB->y = (Ipp16s)((y*(scaleFactor-256)+128)>>8);
     mvF->x = (Ipp16s)((x*scaleFactor+128)>>8);
     mvF->y = (Ipp16s)((y*scaleFactor+128)>>8);
}
void  GetMVDirectCurrHalfBackHalf (Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (Ipp16s)(((x<<1)*(scaleFactor-256)+255)>>9);
     mvB->y = (Ipp16s)(((y<<1)*(scaleFactor-256)+255)>>9);
     mvF->x = (Ipp16s)(((x<<1)*scaleFactor+255)>>9);
     mvF->y = (Ipp16s)(((y<<1)*scaleFactor+255)>>9);

}
void  GetMVDirectCurrQuarterBackHalf(Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (Ipp16s)(((x<<1)*(scaleFactor-256)+128)>>8);
     mvB->y = (Ipp16s)(((y<<1)*(scaleFactor-256)+128)>>8);
     mvF->x = (Ipp16s)(((x<<1)*scaleFactor+128)>>8);
     mvF->y = (Ipp16s)(((y<<1)*scaleFactor+128)>>8);
}
void  GetMVDirectCurrHalfBackQuarter (Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (Ipp16s)(((x*(scaleFactor-256)+255)>>9)<<1);
     mvB->y = (Ipp16s)(((y*(scaleFactor-256)+255)>>9)<<1);
     mvF->x = (Ipp16s)(((x*scaleFactor+255)>>9)<<1);
     mvF->y = (Ipp16s)(((y*scaleFactor+255)>>9)<<1);

}
void  GetMVDirectCurrQuarterBackQuarter(Ipp16s x, Ipp16s y, Ipp32s scaleFactor, sCoordinate * mvF, sCoordinate *mvB)
{
     mvB->x = (Ipp16s)((x*(scaleFactor-256)+128)>>8);
     mvB->y = (Ipp16s)((y*(scaleFactor-256)+128)>>8);
     mvF->x = (Ipp16s)((x*scaleFactor+128)>>8);
     mvF->y = (Ipp16s)((y*scaleFactor+128)>>8);
}
void GetChromaMV (sCoordinate LumaMV, sCoordinate * pChroma)
{
    static Ipp16s round[4]= {0,0,0,1};

    pChroma->x = (LumaMV.x + round[LumaMV.x&0x03])>>1;
    pChroma->y = (LumaMV.y + round[LumaMV.y&0x03])>>1;
    pChroma->bSecond = LumaMV.bSecond;
}


void GetChromaMVFast(sCoordinate LumaMV, sCoordinate * pChroma)
{
    static Ipp16s round [4]= {0,0,0,1};
    static Ipp16s round1[2][2] = {
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



typedef IppStatus (*_own_ippiBicubicInterpolate)     (const Ipp8u* pSrc, Ipp32s srcStep,
                   Ipp8u *pDst, Ipp32s dstStep, Ipp32s dx, Ipp32s dy, Ipp32s roundControl);

typedef IppStatus (*_own_ippiBilinearInterpolate)     (const Ipp8u* pSrc, Ipp32s srcStep,
                   Ipp8u *pDst, Ipp32s dstStep, Ipp32s dx, Ipp32s dy, Ipp32s roundControl);

static IppStatus _own_ippiInterpolate16x16QPBicubic_VC1_8u_C1R (const Ipp8u* pSrc,
                                                      Ipp32s srcStep,
                                                      Ipp8u *pDst,
                                                      Ipp32s dstStep,
                                                      Ipp32s dx,
                                                      Ipp32s dy,
                                                      Ipp32s roundControl)
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

static IppStatus _own_ippiInterpolate16x8QPBicubic_VC1_8u_C1R (const Ipp8u* pSrc,
                                                      Ipp32s srcStep,
                                                      Ipp8u *pDst,
                                                      Ipp32s dstStep,
                                                      Ipp32s dx,
                                                      Ipp32s dy,
                                                      Ipp32s roundControl)
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

static IppStatus _own_ippiInterpolate8x8QPBicubic_VC1_8u_C1R (const Ipp8u* pSrc,
                                                      Ipp32s srcStep,
                                                      Ipp8u *pDst,
                                                      Ipp32s dstStep,
                                                      Ipp32s dx,
                                                      Ipp32s dy,
                                                      Ipp32s roundControl)
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

static IppStatus _own_ippiInterpolate16x16QPBilinear_VC1_8u_C1R (const Ipp8u* pSrc,
                                                       Ipp32s srcStep,
                                                       Ipp8u *pDst,
                                                       Ipp32s dstStep,
                                                       Ipp32s dx,
                                                       Ipp32s dy,
                                                       Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;
    Ipp32s i, j;
    const Ipp32s F[4] = {4, 3, 2, 1};
    const Ipp32s G[4] = {0, 1, 2, 3};

    Ipp32s Mult1 = F[dx]*F[dy];
    Ipp32s Mult2 = F[dx]*G[dy];
    Ipp32s Mult3 = F[dy]*G[dx];
    Ipp32s Mult4 = G[dx]*G[dy];


    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d\t %d\t %d\n"),*pSource,X>>2,Y>>2);
   // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Predicted pels\n"));

        for(j = 0; j < 16; j++)
        {
            for(i = 0; i < 16; i++)
            {
                pDst[i] = (Ipp8u)((pSrc[i]           * Mult1 +
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

static IppStatus _own_ippiInterpolate8x8QPBilinear_VC1_8u_C1R   (const Ipp8u* pSrc,
                                                       Ipp32s srcStep,
                                                       Ipp8u *pDst,
                                                       Ipp32s dstStep,
                                                       Ipp32s dx,
                                                       Ipp32s dy,
                                                       Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;
    Ipp32s i, j;
    const Ipp32s F[4] = {4, 3, 2, 1};
    const Ipp32s G[4] = {0, 1, 2, 3};

    Ipp32s Mult1 = F[dx]*F[dy];
    Ipp32s Mult2 = F[dx]*G[dy];
    Ipp32s Mult3 = F[dy]*G[dx];
    Ipp32s Mult4 = G[dx]*G[dy];

    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d\t %d\t %d\n"),*pSource,X>>2,Y>>2);
   // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Predicted pels\n"));

        for(j = 0; j < 8; j++)
        {
            for(i = 0; i < 8; i++)
            {
                pDst[i] = (Ipp8u)((pSrc[i]           * Mult1 +
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



static IppStatus _own_ippiInterpolate8x4QPBilinear_VC1_8u_C1R   (const Ipp8u* pSrc,
                                                       Ipp32s srcStep,
                                                       Ipp8u *pDst,
                                                       Ipp32s dstStep,
                                                       Ipp32s dx,
                                                       Ipp32s dy,
                                                       Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;
    Ipp32s i, j;
    const Ipp32s F[4] = {4, 3, 2, 1};
    const Ipp32s G[4] = {0, 1, 2, 3};

    Ipp32s Mult1 = F[dx]*F[dy];
    Ipp32s Mult2 = F[dx]*G[dy];
    Ipp32s Mult3 = F[dy]*G[dx];
    Ipp32s Mult4 = G[dx]*G[dy];

    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d\t %d\t %d\n"),*pSource,X>>2,Y>>2);
   // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Predicted pels\n"));

        for(j = 0; j < 4; j++)
        {
            for(i = 0; i < 8; i++)
            {

                pDst[i] = (Ipp8u)((pSrc[i]           * Mult1 +
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


static IppStatus _own_ippiInterpolate4x4QPBilinear_VC1_8u_C1R   (const Ipp8u* pSrc,
                                                       Ipp32s srcStep,
                                                       Ipp8u *pDst,
                                                       Ipp32s dstStep,
                                                       Ipp32s dx,
                                                       Ipp32s dy,
                                                       Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;
    Ipp32s i, j;
    const Ipp32s F[4] = {4, 3, 2, 1};
    const Ipp32s G[4] = {0, 1, 2, 3};
    Ipp32s Mult1 = F[dx]*F[dy];
    Ipp32s Mult2 = F[dx]*G[dy];
    Ipp32s Mult3 = F[dy]*G[dx];
    Ipp32s Mult4 = G[dx]*G[dy];

    //VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("%d\t %d\t %d\n"),*pSource,X>>2,Y>>2);
   // VM_Debug::GetInstance().vm_debug_frame(-1,VC1_PRED,VM_STRING("Predicted pels\n"));

        for(j = 0; j < 4; j++)
        {
            for(i = 0; i < 4; i++)
            {
                 pDst[i] = (Ipp8u)((pSrc[i]           * Mult1 +
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
    Ipp32s pinterp = (inter_struct->roiSize.width >> 3) + (inter_struct->roiSize.height >> 3);

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


static IppStatus _own_ippiInterpolate8x8QPBilinear_VC1_8u_C1R_NV12   (const Ipp8u* pSrc, Ipp32s srcStep,
                                                                      Ipp8u *pDst,       Ipp32s dstStep,
                                                                      Ipp32s dx,         Ipp32s dy,
                                                                      Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;
    Ipp32s i, j;
    const Ipp32s F[4] = {4, 3, 2, 1};
    const Ipp32s G[4] = {0, 1, 2, 3};

    Ipp32s Mult1 = F[dx]*F[dy];
    Ipp32s Mult2 = F[dx]*G[dy];
    Ipp32s Mult3 = F[dy]*G[dx];
    Ipp32s Mult4 = G[dx]*G[dy];

    for(j = 0; j < 8; j++)
    {
        for(i = 0; i < 16; i += 2)
        {
            pDst[i] = (Ipp8u)((pSrc[i]               * Mult1 +
                               pSrc[i + srcStep]     * Mult2 +
                               pSrc[i + 2]           * Mult3 +
                               pSrc[i + srcStep + 2] * Mult4 +
                               8 - roundControl) >> 4);

            pDst[i + 1] = (Ipp8u)((pSrc[i + 1]               * Mult1 +
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


static IppStatus _own_ippiInterpolate8x4QPBilinear_VC1_8u_C1R_NV12   (const Ipp8u* pSrc, Ipp32s srcStep,
                                                                      Ipp8u *pDst,       Ipp32s dstStep,
                                                                      Ipp32s dx,         Ipp32s dy,
                                                                      Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;
    Ipp32s i, j;
    const Ipp32s F[4] = {4, 3, 2, 1};
    const Ipp32s G[4] = {0, 1, 2, 3};

    Ipp32s Mult1 = F[dx]*F[dy];
    Ipp32s Mult2 = F[dx]*G[dy];
    Ipp32s Mult3 = F[dy]*G[dx];
    Ipp32s Mult4 = G[dx]*G[dy];

    for(j = 0; j < 4; j++)
    {
        for(i = 0; i < 16; i += 2)
        {

            pDst[i] = (Ipp8u)((pSrc[i]               * Mult1 +
                               pSrc[i + srcStep]     * Mult2 +
                               pSrc[i + 2]           * Mult3 +
                               pSrc[i + srcStep + 2] * Mult4 +
                               8 - roundControl) >> 4);

            pDst[i + 1] = (Ipp8u)((pSrc[i + 1]               * Mult1 +
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


static IppStatus _own_ippiInterpolate4x4QPBilinear_VC1_8u_C1R_NV12   (const Ipp8u* pSrc, Ipp32s srcStep,
                                                                      Ipp8u *pDst,       Ipp32s dstStep,
                                                                      Ipp32s dx,         Ipp32s dy,
                                                                      Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;
    Ipp32s i, j;
    const Ipp32s F[4] = {4, 3, 2, 1};
    const Ipp32s G[4] = {0, 1, 2, 3};
    Ipp32s Mult1 = F[dx]*F[dy];
    Ipp32s Mult2 = F[dx]*G[dy];
    Ipp32s Mult3 = F[dy]*G[dx];
    Ipp32s Mult4 = G[dx]*G[dy];

    for(j = 0; j < 4; j++)
    {
        for(i = 0; i < 8; i += 2)
        {
            pDst[i] = (Ipp8u)((pSrc[i]           * Mult1 +
                pSrc[i+srcStep]   * Mult2 +
                pSrc[i+2]         * Mult3 +
                pSrc[i+srcStep+2] * Mult4 +
                8 - roundControl) >> 4);

            pDst[i] = (Ipp8u)((pSrc[i + 1]           * Mult1 +
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

typedef IppStatus (*_own_ippiBilinearInterpolate)     (const Ipp8u* pSrc, Ipp32s srcStep,
                   Ipp8u *pDst, Ipp32s dstStep, Ipp32s dx, Ipp32s dy, Ipp32s roundControl);

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
    Ipp32s pinterp = (inter_struct->roiSize.width >> 3) + (inter_struct->roiSize.height >> 3);

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

static IppStatus own_ippiTransform8x8Inv_VC1_16s_C1R(const Ipp16s  *pSrc, int srcStep,
                                                      Ipp16s  *pDst, int dstStep, IppiSize srcSizeNZ)
{
    if (srcSizeNZ.width <= 1 && srcSizeNZ.height <= 1) {
        int     i;
        Ipp16s  c = (Ipp16s)((((pSrc[0] * 12 + 4) >> 3) * 12 + 64) >> 7);

        for (i = 0; i < 8; i ++) {
            pDst[0] = pDst[1] = pDst[2] = pDst[3] = pDst[4] = pDst[5] = pDst[6] = pDst[7] = c;
            pDst = (Ipp16s*)((Ipp8u*)pDst+dstStep);
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
            pSrc = (Ipp16s*)((Ipp8u*)pSrc + srcStep);
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
            *(Ipp16s*)((Ipp8u*)pDst+0*dstStep) = (Ipp16s)((a0 + u1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+7*dstStep) = (Ipp16s)((a0 - u1 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+3*dstStep) = (Ipp16s)((a0 + u3) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+4*dstStep) = (Ipp16s)((a0 - u3 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+1*dstStep) = (Ipp16s)((a0 + v1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+6*dstStep) = (Ipp16s)((a0 - v1 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+2*dstStep) = (Ipp16s)((a0 + v3) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+5*dstStep) = (Ipp16s)((a0 - v3 + 1) >> 7);
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
            pSrc = (Ipp16s*)((Ipp8u*)pSrc + srcStep);
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
            *(Ipp16s*)((Ipp8u*)pDst+0*dstStep) = (Ipp16s)((u0 + u1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+7*dstStep) = (Ipp16s)((u0 - u1 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+3*dstStep) = (Ipp16s)((u2 + u3) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+4*dstStep) = (Ipp16s)((u2 - u3 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+1*dstStep) = (Ipp16s)((v0 + v1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+6*dstStep) = (Ipp16s)((v0 - v1 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+2*dstStep) = (Ipp16s)((v2 + v3) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+5*dstStep) = (Ipp16s)((v2 - v3 + 1) >> 7);
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
            pSrc = (Ipp16s*)((Ipp8u*)pSrc + srcStep);
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

            *(Ipp16s*)((Ipp8u*)pDst+0*dstStep) = (Ipp16s)((u0 + u1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+7*dstStep) = (Ipp16s)((u0 - u1 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+3*dstStep) = (Ipp16s)((u2 + u3) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+4*dstStep) = (Ipp16s)((u2 - u3 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+1*dstStep) = (Ipp16s)((v0 + v1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+6*dstStep) = (Ipp16s)((v0 - v1 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+2*dstStep) = (Ipp16s)((v2 + v3) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+5*dstStep) = (Ipp16s)((v2 - v3 + 1) >> 7);
            pDst ++;
            pBuff ++;
        }
    }
    return ippStsNoErr;
}

static IppStatus own_ippiTransform8x4Inv_VC1_16s_C1R(const Ipp16s  *pSrc, int      srcStep,
                                                      Ipp16s  *pDst, int      dstStep,      IppiSize srcSizeNZ)
{
    if (srcSizeNZ.width <= 1 && srcSizeNZ.height <= 1) {
        int     i;
        Ipp16s  c = (Ipp16s)((((pSrc[0] * 12 + 4) >> 3) * 17 + 64) >> 7);

        for (i = 0; i < 4; i ++) {
            pDst[0] = pDst[1] = pDst[2] = pDst[3] = pDst[4] = pDst[5] = pDst[6] = pDst[7] = c;
            pDst = (Ipp16s*)((Ipp8u*)pDst+dstStep);
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
            pSrc = (Ipp16s*)((Ipp8u*)pSrc + srcStep);
            pBuff += 8;
        }
        pBuff -= 32;
        for (i = 0; i < 8; i ++) {
            int a0 = (pBuff[0*8] + pBuff[2*8]) * 17 + 64;
            int s0 = (pBuff[0*8] - pBuff[2*8]) * 17 + 64;
            int a1 = pBuff[1*8] * 22 + pBuff[3*8] * 10;
            int s1 = pBuff[1*8] * 10 - pBuff[3*8] * 22;

            *(Ipp16s*)((Ipp8u*)pDst+0*dstStep) = (Ipp16s)((a0 + a1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+1*dstStep) = (Ipp16s)((s0 + s1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+2*dstStep) = (Ipp16s)((s0 - s1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+3*dstStep) = (Ipp16s)((a0 - a1) >> 7);
            pDst ++;
            pBuff ++;
        }
    }
    return ippStsNoErr;
}

static IppStatus own_ippiTransform4x8Inv_VC1_16s_C1R(const Ipp16s  *pSrc,  int      srcStep,
                                                      Ipp16s  *pDst,  int      dstStep,    IppiSize srcSizeNZ)
{

    if (srcSizeNZ.width <= 1 && srcSizeNZ.height <= 1) {
        int     i;
        Ipp16s  c = (Ipp16s)((((pSrc[0] * 17 + 4) >> 3) * 12 + 64) >> 7);

        for (i = 0; i < 8; i ++) {
            pDst[0] = pDst[1] = pDst[2] = pDst[3] = c;
            pDst = (Ipp16s*)((Ipp8u*)pDst+dstStep);
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
            pSrc = (Ipp16s*)((Ipp8u*)pSrc + srcStep);
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

            *(Ipp16s*)((Ipp8u*)pDst+0*dstStep) = (Ipp16s)((u0 + u1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+7*dstStep) = (Ipp16s)((u0 - u1 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+3*dstStep) = (Ipp16s)((u2 + u3) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+4*dstStep) = (Ipp16s)((u2 - u3 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+1*dstStep) = (Ipp16s)((v0 + v1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+6*dstStep) = (Ipp16s)((v0 - v1 + 1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+2*dstStep) = (Ipp16s)((v2 + v3) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+5*dstStep) = (Ipp16s)((v2 - v3 + 1) >> 7);
            pDst ++;
            pBuff ++;
        }
    }
    return ippStsNoErr;
}


static IppStatus own_ippiTransform4x4Inv_VC1_16s_C1R (const Ipp16s  *pSrc, int      srcStep,
                                                 Ipp16s  *pDst,       int      dstStep,      IppiSize srcSizeNZ)
{

    if (srcSizeNZ.width <= 1 && srcSizeNZ.height <= 1) {
        Ipp16s  c = (Ipp16s)((((pSrc[0] * 17 + 4) >> 3) * 17 + 64) >> 7);
        pDst[0] = pDst[1] = pDst[2] = pDst[3] = c;
        pDst = (Ipp16s*)((Ipp8u*)pDst+dstStep);
        pDst[0] = pDst[1] = pDst[2] = pDst[3] = c;
        pDst = (Ipp16s*)((Ipp8u*)pDst+dstStep);
        pDst[0] = pDst[1] = pDst[2] = pDst[3] = c;
        pDst = (Ipp16s*)((Ipp8u*)pDst+dstStep);
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
        pSrc = (Ipp16s*)((Ipp8u*)pSrc + srcStep);
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
            *(Ipp16s*)((Ipp8u*)pDst+0*dstStep) = (Ipp16s)((a0 + a1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+1*dstStep) = (Ipp16s)((a0 + s1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+2*dstStep) = (Ipp16s)((a0 - s1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+3*dstStep) = (Ipp16s)((a0 - a1) >> 7);
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
            pSrc = (Ipp16s*)((Ipp8u*)pSrc + srcStep);
            pBuff += 4;
        }
        pBuff -= 16;
        for (i = 0; i < 4; i ++) {
            int a0 = (pBuff[0*4] + pBuff[2*4]) * 17 + 64;
            int s0 = (pBuff[0*4] - pBuff[2*4]) * 17 + 64;
            int a1 = pBuff[1*4] * 22 + pBuff[3*4] * 10;
            int s1 = pBuff[1*4] * 10 - pBuff[3*4] * 22;
            *(Ipp16s*)((Ipp8u*)pDst+0*dstStep) = (Ipp16s)((a0 + a1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+1*dstStep) = (Ipp16s)((s0 + s1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+2*dstStep) = (Ipp16s)((s0 - s1) >> 7);
            *(Ipp16s*)((Ipp8u*)pDst+3*dstStep) = (Ipp16s)((a0 - a1) >> 7);
            pDst ++;
            pBuff ++;
        }
    }
    return ippStsNoErr;
}

IppStatus _own_ippiTransformBlock8x8Inv_VC1_16s_C1IR(Ipp16s* pSrcDst, Ipp32u srcDstStep, IppiSize DstSize)
{
    own_ippiTransform8x8Inv_VC1_16s_C1R(pSrcDst, srcDstStep, pSrcDst, srcDstStep, DstSize);
    return ippStsNoErr;
}

IppStatus _own_ippiTransformBlock4x8Inv_VC1_16s_C1IR(Ipp16s* pSrcDst, Ipp32u srcDstStep, IppiSize DstSize)
{
    own_ippiTransform4x8Inv_VC1_16s_C1R(pSrcDst, srcDstStep, pSrcDst, srcDstStep, DstSize);
    return ippStsNoErr;
}

IppStatus _own_ippiTransformBlock8x4Inv_VC1_16s_C1IR(Ipp16s* pSrcDst, Ipp32u srcDstStep, IppiSize DstSize)
{
    own_ippiTransform8x4Inv_VC1_16s_C1R(pSrcDst, srcDstStep, pSrcDst, srcDstStep, DstSize);
    return ippStsNoErr;
}

IppStatus _own_ippiTransformBlock4x4Inv_VC1_16s_C1IR(Ipp16s* pSrcDst, Ipp32u srcDstStep, IppiSize DstSize)
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
IppStatus _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R (Ipp16s* pSrcLeft, Ipp32s srcLeftStep,
                                                        Ipp16s* pSrcRight, Ipp32s srcRightStep,
                                                        Ipp8u* pDst, Ipp32s dstStep,
                                                        Ipp32u fieldNeighbourFlag,
                                                        Ipp32u edgeDisableFlag)
{
    Ipp32s i;
    Ipp8s r0, r1;
    Ipp16s *pSrcL = pSrcLeft;
    Ipp16s *pSrcR = pSrcRight;
    Ipp8u  *dst = pDst;

    Ipp16s x0,x1,x2,x3;
    Ipp16s f0, f1;

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

                    *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                    *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                    *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                    *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

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

                        *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                        *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                        *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                        *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

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

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

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

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

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

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

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

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

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

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

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

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

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

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

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

                *(dst-2)=(Ipp8u)VC1_CLIP(*(pSrcL));
                *(dst-1)=(Ipp8u)VC1_CLIP(*(pSrcL+1));
                *(dst+0)=(Ipp8u)VC1_CLIP(*(pSrcR));
                *(dst+1)=(Ipp8u)VC1_CLIP(*(pSrcR+1));

                pSrcL += srcLeftStep;
                pSrcR += srcRightStep;
                dst+=2*dstStep;
            }
        }
        break;
    }

    return ippStsNoErr;
}

IppStatus _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R (Ipp16s* pSrcUpper, Ipp32s srcUpperStep,
                                                        Ipp16s* pSrcBottom, Ipp32s srcBottomStep,
                                                        Ipp8u* pDst, Ipp32s dstStep,
                                                        Ipp32u edgeDisableFlag)
{
    Ipp32s i;
    Ipp8s r0=4, r1=3;
    Ipp16s *pSrcU = pSrcUpper;
    Ipp16s *pSrcB = pSrcBottom;
    Ipp8u  *dst = pDst;

    Ipp16s x0,x1,x2,x3;
    Ipp16s f0, f1;
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

            *(dst-2*dstStep)= (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
            *(dst-1*dstStep)= (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
            *(dst+0*dstStep)= (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
            *(dst+1*dstStep)= (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

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

                *(dst-2*dstStep)= (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
                *(dst-1*dstStep)= (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
                *(dst+0*dstStep)= (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
                *(dst+1*dstStep)= (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

                dst++;
                pSrcU++;
                pSrcB++;

                r0 = 7 - r0;
                r1 = 7 - r1;
            }

            return ippStsNoErr;
}


IppStatus _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R (Ipp16s* pSrcUpper, Ipp32s srcUpperStep,
                                                          Ipp16s* pSrcBottom, Ipp32s srcBottomStep,
                                                          Ipp8u* pDst, Ipp32s dstStep)
{
    Ipp32s i;
    Ipp8s r0=4, r1=3;
    Ipp16s *pSrcU = pSrcUpper;
    Ipp16s *pSrcB = pSrcBottom;
    Ipp8u  *dst = pDst;

    Ipp16s x0,x1,x2,x3;
    Ipp16s f0, f1;

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

        *(pSrcU    )            = (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
        *(pSrcU + srcUpperStep) = (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
        *(pSrcB    )            = (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
        *(pSrcB + srcBottomStep)= (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

        *(dst-2*dstStep)= (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
        *(dst-1*dstStep)= (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
        *(dst+0*dstStep)= (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
        *(dst+1*dstStep)= (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

        dst++;
        pSrcU++;
        pSrcB++;

        r0 = 7 - r0;
        r1 = 7 - r1;
    }
    return ippStsNoErr;
}

IppStatus _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R (Ipp16s* pSrcLeft, Ipp32s srcLeftStep,
                                                          Ipp16s* pSrcRight, Ipp32s srcRightStep,
                                                          Ipp8u* pDst, Ipp32s dstStep)
{
    Ipp32s i;
    Ipp8s r0, r1;

    Ipp16s x0,x1,x2,x3;
    Ipp16s f0, f1;

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

        *(pDst-2)=(Ipp8u)VC1_CLIP(*(pSrcLeft));
        *(pDst-1)=(Ipp8u)VC1_CLIP(*(pSrcLeft+1));
        *(pDst+0)=(Ipp8u)VC1_CLIP(*(pSrcRight));
        *(pDst+1)=(Ipp8u)VC1_CLIP(*(pSrcRight+1));

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

IppStatus _own_ippiSmoothingChroma_NV12_HorEdge_VC1_16s8u_C2R (Ipp16s* pSrcUpperU, Ipp32s srcUpperStepU,
                                                               Ipp16s* pSrcBottomU, Ipp32s srcBottomStepU,
                                                               Ipp16s* pSrcUpperV, Ipp32s srcUpperStepV,
                                                               Ipp16s* pSrcBottomV, Ipp32s srcBottomStepV,
                                                               Ipp8u* pDst, Ipp32s dstStep)
{
    Ipp32s i;
    Ipp8s  r0 = 4, r1 = 3;
    Ipp16s *pSrcU_U = pSrcUpperU;
    Ipp16s *pSrcB_U = pSrcBottomU;
    Ipp16s *pSrcU_V = pSrcUpperV;
    Ipp16s *pSrcB_V = pSrcBottomV;
    Ipp8u  *dst = pDst;

    Ipp16s x0,x1,x2,x3;
    Ipp16s f0, f1;

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

        *(pSrcU_U    )              = (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
        *(pSrcU_U + srcUpperStepU)  = (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
        *(pSrcB_U    )              = (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
        *(pSrcB_U + srcBottomStepU) = (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

        *(dst - 2*dstStep)= (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
        *(dst - 1*dstStep)= (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
        *(dst + 0*dstStep)= (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
        *(dst + 1*dstStep)= (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

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

        *(pSrcU_V    )              = (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
        *(pSrcU_V + srcUpperStepV)  = (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
        *(pSrcB_V    )              = (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
        *(pSrcB_V + srcBottomStepV) = (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

        *(dst - 2*dstStep)= (Ipp8u)VC1_CLIP(x0 + ((f0 + r0)>>3));
        *(dst - 1*dstStep)= (Ipp8u)VC1_CLIP(x1 + ((f1 + r1)>>3));
        *(dst + 0*dstStep)= (Ipp8u)VC1_CLIP(x2 + ((-f1 + r0)>>3));
        *(dst + 1*dstStep)= (Ipp8u)VC1_CLIP(x3 + ((-f0 + r1)>>3));

        pSrcU_V++;
        pSrcB_V++;
        dst++;

        r0 = 7 - r0;
        r1 = 7 - r1;
    }
    return ippStsNoErr;
}


IppStatus _own_ippiSmoothingChroma_NV12_VerEdge_VC1_16s8u_C2R (Ipp16s* pSrcLeftU,  Ipp32s srcLeftStepU,
                                                               Ipp16s* pSrcRightU, Ipp32s srcRightStepU,
                                                               Ipp16s* pSrcLeftV,  Ipp32s srcLeftStepV,
                                                               Ipp16s* pSrcRightV, Ipp32s srcRightStepV,
                                                               Ipp8u* pDst, Ipp32s dstStep)
{
    Ipp32s i;
    Ipp8s r0, r1;

    Ipp16s x0,x1,x2,x3;
    Ipp16s f0, f1;

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

        *(pDst - 4)=(Ipp8u)VC1_CLIP(*(pSrcLeftU));
        *(pDst - 2)=(Ipp8u)VC1_CLIP(*(pSrcLeftU + 1));
        *(pDst + 0)=(Ipp8u)VC1_CLIP(*(pSrcRightU));
        *(pDst + 2)=(Ipp8u)VC1_CLIP(*(pSrcRightU + 1));

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

        *(pDst - 4)=(Ipp8u)VC1_CLIP(*(pSrcLeftV));
        *(pDst - 2)=(Ipp8u)VC1_CLIP(*(pSrcLeftV + 1));
        *(pDst + 0)=(Ipp8u)VC1_CLIP(*(pSrcRightV));
        *(pDst + 2)=(Ipp8u)VC1_CLIP(*(pSrcRightV + 1));

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

#define IPP_MAX_8U     ( 0xFF )
#define OWN_MIN_8U      0

IppStatus _own_ippiConvert_16s8u_C1R( const Ipp16s* pSrc, Ipp32u srcStep, Ipp8u* pDst, Ipp32u dstStep, IppiSize roiSize )
{
    const Ipp16s *src = pSrc;
    Ipp8u *dst = pDst;
    int width = roiSize.width, height = roiSize.height;
    int h;
    int n;

    if( (srcStep == dstStep*2) && (dstStep == width) ) {
        width *= height;
        height = 1;
      }

    for( h = 0; h < height; h++ ) {

        for( n = 0; n < width; n++ ) {
            if( src[n] >= IPP_MAX_8U ) {
                dst[n] = IPP_MAX_8U;
            } else if( src[n] <= OWN_MIN_8U ) {
                dst[n] = OWN_MIN_8U;
            } else {
                dst[n] = (Ipp8u)src[n];
            }
        }
        src = (Ipp16s*)((Ipp8u*)src + srcStep);
        dst += dstStep;
    }
    return ippStsNoErr;
}

#endif //_VC1_ENC_OWN_FUNCTIONS_
#endif //_VC1_ENC_OWN_FUNCTIONS_
bool MaxAbsDiff(Ipp8u* pSrc1, Ipp32u step1,Ipp8u* pSrc2, Ipp32u step2,IppiSize roiSize,Ipp32u max)
{
    Ipp32u  sum = max;
    for (int i = 0; i<roiSize.height; i++)
    {
       Ipp16u t1 = (VC1abs(*(pSrc1++) - *(pSrc2++)));
       Ipp16u t2 = (VC1abs(*(pSrc1++) - *(pSrc2++)));

       for (int j = 0; j<roiSize.width-2; j++)
       {
           Ipp16u t3 = (VC1abs(*(pSrc1++) - *(pSrc2++)));
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
void initMVFieldInfo(bool extendedX,        bool extendedY,const Ipp32u* pVLCTable ,sMVFieldInfo* MVFieldInfo)
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
inline static Ipp32s GetEdgeValue(Ipp32s Edge, Ipp32s Current)
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
                                                     const Ipp8u*                   pLUTTop,
                                                     const Ipp8u*                   pLUTBottom,
                                                     Ipp32u                          OppositePadding,
                                                     Ipp32u                          fieldPrediction,
                                                     Ipp32u                          RoundControl,
                                                     Ipp32u                          isPredBottom)

{
    Ipp8u    temp_block [19*19]          = {0};
    IppiSize temp_block_size             = {interpolateInfo->sizeBlock.width + 3, interpolateInfo->sizeBlock.height + 3};
    Ipp32s   temp_block_step             = 19;
    Ipp8u*   p_temp_block_data_pointer   = temp_block;
    Ipp8u    shift = (fieldPrediction)? 1:0;

    IppiPoint position = {interpolateInfo->pointBlockPos.x,
        interpolateInfo->pointBlockPos.y};

    Ipp32u step = 0;

    Ipp32s TopFieldPOffsetTP = 0;
    Ipp32s BottomFieldPOffsetTP = 1;

    Ipp32s TopFieldPOffsetBP = 2;
    Ipp32s BottomFieldPOffsetBP = 1;


    Ipp32u SCoef = (fieldPrediction)?1:0; // in case of interlace fields we should use half of frame height


    // minimal distance from current point for interpolation.
    Ipp32s left = 1;
    Ipp32s right = interpolateInfo->sizeFrame.width - (interpolateInfo->sizeBlock.width + 2);
    Ipp32s top = 1; //<< shift;

    Ipp32s bottom = (interpolateInfo->sizeFrame.height >> SCoef)  - (interpolateInfo->sizeBlock.height + 2);
    Ipp32s   paddingLeft    = (position.x < left) ?  left  - position.x: 0;
    Ipp32s   paddingRight   = (position.x > right)?  position.x - right: 0;
    Ipp32s   paddingTop     = (position.y < top)?    (top - position.y): 0;
    Ipp32s   paddingBottom  = (position.y > bottom)? (position.y - bottom): 0;
    Ipp32s   RefBlockStep = interpolateInfo->srcStep << shift;
    const Ipp8u*   pRefBlock =    interpolateInfo->pSrc[0] + (position.x + paddingLeft - 1) +
                            (position.y + paddingTop  - 1)* RefBlockStep;
    const Ipp8u* pLUT[2];

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
        const Ipp8u* pTemp;
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
        const Ipp8u* pTemp;
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
        Ipp32s FieldPOffset[2] = {0,0};
        Ipp32s posy;



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
                const Ipp8u* pTemp;
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
                                                      const   Ipp8u*                 pLUTTop,
                                                      const   Ipp8u*                 pLUTBottom,
                                                      Ipp32u                          OppositePadding,
                                                      Ipp32u                          fieldPrediction,
                                                      Ipp32u                          RoundControl,
                                                      Ipp32u                          isPredBottom)

{
    Ipp8u    temp_block [19*19]          = {0};
    IppiSize temp_block_size             = {interpolateInfo->sizeBlock.width + 3, interpolateInfo->sizeBlock.height + 3};
    Ipp32s   temp_block_step             = 19;
    Ipp8u*   p_temp_block_data_pointer   = temp_block;
    Ipp8u    shift = (fieldPrediction)? 1:0;

    IppiPoint position = {interpolateInfo->pointBlockPos.x,
        interpolateInfo->pointBlockPos.y};

    Ipp32u step = 0;

    Ipp32s TopFieldPOffsetTP = 0;
    Ipp32s BottomFieldPOffsetTP = 1;

    Ipp32s TopFieldPOffsetBP = 2;
    Ipp32s BottomFieldPOffsetBP = 1;


    Ipp32u SCoef = (fieldPrediction)?1:0; // in case of interlace fields we should use half of frame height


    // minimal distance from current point for interpolation.
    Ipp32s left = 1;
    Ipp32s right = interpolateInfo->sizeFrame.width - (interpolateInfo->sizeBlock.width + 2);
    Ipp32s top = 1; //<< shift;

    Ipp32s bottom = (interpolateInfo->sizeFrame.height >> SCoef)  - (interpolateInfo->sizeBlock.height + 2);
    Ipp32s   paddingLeft    = (position.x < left) ?  left  - position.x: 0;
    Ipp32s   paddingRight   = (position.x > right)?  position.x - right: 0;
    Ipp32s   paddingTop     = (position.y < top)?    (top - position.y): 0;
    Ipp32s   paddingBottom  = (position.y > bottom)? (position.y - bottom): 0;
    Ipp32s   RefBlockStep = interpolateInfo->srcStep << shift;
    const Ipp8u*   pRefBlock =    interpolateInfo->pSrc[0] + (position.x + paddingLeft - 1) +
                            (position.y + paddingTop  - 1)* RefBlockStep;
    const Ipp8u* pLUT[2];
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
        const Ipp8u* pTemp;
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
        const Ipp8u* pTemp;
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
        Ipp32s FieldPOffset[2] = {0,0};
        Ipp32s posy;



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
                const Ipp8u* pTemp;
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
                                                      const   Ipp8u*                 pLUTTop,
                                                      const   Ipp8u*                 pLUTBottom,
                                                      Ipp32u                          OppositePadding,
                                                      Ipp32u                          fieldPrediction,
                                                      Ipp32u                          RoundControl,
                                                      Ipp32u                          isPredBottom)

{
    Ipp8u    temp_block [38*19]          = {0};
    IppiSize temp_block_size             = {(interpolateInfo->sizeBlock.width + 3)*2, interpolateInfo->sizeBlock.height + 3};
    Ipp32s   temp_block_step             = 38;
    Ipp8u*   p_temp_block_data_pointer   = temp_block;
    Ipp8u    shift = (fieldPrediction)? 1:0;

    IppiPoint position = {interpolateInfo->pointBlockPos.x, interpolateInfo->pointBlockPos.y};

    Ipp32s TopFieldPOffsetTP = 0;
    Ipp32s BottomFieldPOffsetTP = 1;

    Ipp32s TopFieldPOffsetBP = 2;
    Ipp32s BottomFieldPOffsetBP = 1;


    Ipp32u SCoef = (fieldPrediction)?1:0; // in case of interlace fields we should use half of frame height


    // minimal distance from current point for interpolation.
    Ipp32s left = 2;
    Ipp32s right = interpolateInfo->sizeFrame.width*2 - (interpolateInfo->sizeBlock.width + 2)*2;
    Ipp32s top = 1;

    Ipp32s bottom = (interpolateInfo->sizeFrame.height >> SCoef)  - (interpolateInfo->sizeBlock.height + 2);

    Ipp32s   paddingLeft    = (position.x < left) ?  left  - position.x : 0;
    Ipp32s   paddingRight   = (position.x > right)?  position.x - right : 0;
    Ipp32s   paddingTop     = (position.y < top)?    (top - position.y): 0;
    Ipp32s   paddingBottom  = (position.y > bottom)? (position.y - bottom): 0;

    Ipp32s   RefBlockStep = interpolateInfo->srcStep << shift; //step for UV plane

    const Ipp8u*   pRefBlockU =    interpolateInfo->pSrc[0] + (position.x + paddingLeft - 2) +
                                  (position.y + paddingTop  - 1)* RefBlockStep;

    const Ipp8u*   pRefBlockV =    interpolateInfo->pSrc[0] + 1 + (position.x + paddingLeft - 2) +
                                  (position.y + paddingTop  - 1)* RefBlockStep;
    const Ipp8u* pLUT[2];
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
        const Ipp8u* pTemp;
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
        const Ipp8u* pTemp;
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
        Ipp32s FieldPOffset[2] = {0,0};
        Ipp32s posy;

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
                const Ipp8u* pTemp;
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


IppStatus CalculateCorrelationBlock_8u_P1R (  Ipp8u *pSrc, Ipp32u srcStep,
                                              Ipp8u *pRef, Ipp32u refStep,
                                              IppiSize roiSize, IppiSize blockSize,
                                              CorrelationParams * pOutPut)
{

    pOutPut->correlation = 0;
    pOutPut->k = 0;
    pOutPut->b = 0;

    Ipp64f X = 0;
    Ipp64f Y = 0;
    Ipp64f XX = 0;
    Ipp64f YY = 0;
    Ipp64f XY = 0;

    Ipp32u nBlocks = 0;

    for (int i = 0; i < roiSize.height - blockSize.height + 1; i+= blockSize.height)
    {
        Ipp8u *pSrcRow = pSrc + (srcStep*i);
        Ipp8u *pRefRow = pRef + (refStep*i);

        for (int j = 0; j < roiSize.width - blockSize.width + 1; j+= blockSize.width)
        {
            Ipp64f Xi = 0;
            Ipp64f Yi = 0;
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
bool CalculateIntesityCompensationParams (CorrelationParams sCorrParams, Ipp32u &LUMSCALE, Ipp32u &LUMSHIFT)
{
 

    if ((sCorrParams.correlation > 0.97) &&
       ((sCorrParams.k >   1 + (3.0/64.0) ) || 
        (sCorrParams.k <   1 - (3.0/64.0) )  || 
        (sCorrParams.b >   2.0)   || 
        (sCorrParams.b <  -2.0)))
    {
        
        LUMSCALE =  (Ipp32u) (sCorrParams.k*64.0 - 31.0);
        LUMSHIFT =  ((Ipp32u)(sCorrParams.b + ((sCorrParams.b<0)? -0.5:0.5))) & 0x3F;


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
void get_iScale_iShift (Ipp32u LUMSCALE, Ipp32u LUMSHIFT, Ipp32s &iScale, Ipp32s &iShift) 
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
void CreateICTable (Ipp32s iScale, Ipp32s iShift, Ipp32s* LUTY, Ipp32s* LUTUV)
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
IppStatus   _own_ippiReplicateBorder_8u_C1R  (  Ipp8u * pSrc,  int srcStep, 
                                              IppiSize srcRoiSize, IppiSize dstRoiSize, 
                                              int topBorderWidth,  int leftBorderWidth)
{
    // upper
    Ipp8u* pBlock = pSrc;
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
    Ipp32s rightY = dstRoiSize.width - srcRoiSize.width - leftBorderWidth;
    rightY = (rightY>0)? rightY : 0;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        memset(pBlock+1,pBlock[0],rightY);
        pBlock += srcStep;
    }
    return ippStsNoErr;
}
IppStatus   _own_ippiReplicateBorder_16u_C1R  ( Ipp16u * pSrc,  int srcStep, 
                                               IppiSize srcRoiSize, IppiSize dstRoiSize, 
                                               int topBorderWidth,  int leftBorderWidth)
{
    // upper
    Ipp8u* pBlock = (Ipp8u*) pSrc;
    for (int i = 0; i < topBorderWidth; i++)
    {
        MFX_INTERNAL_CPY(pBlock - srcStep, pBlock, srcRoiSize.width*sizeof(Ipp16u));
        pBlock -= srcStep;
    }

    // bottom
    pBlock = (Ipp8u*)pSrc + (srcRoiSize.height  - 1)*srcStep;

    for (int i = 0; i < dstRoiSize.height - srcRoiSize.height - topBorderWidth; i++ )
    {
        MFX_INTERNAL_CPY(pBlock + srcStep, pBlock, srcRoiSize.width*sizeof(Ipp16u));
        pBlock += srcStep;
    }

    // left
    pBlock = (Ipp8u*)(pSrc) - srcStep * topBorderWidth;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        ippsSet_16s (*(Ipp16s*)pBlock, (Ipp16s*)pBlock-leftBorderWidth,leftBorderWidth);
        pBlock += srcStep;
    }

    //right

    pBlock = (Ipp8u*)(pSrc + srcRoiSize.width - 1) - srcStep * topBorderWidth ;
    Ipp32s rightY = dstRoiSize.width - srcRoiSize.width - leftBorderWidth;
    rightY = (rightY>0)? rightY : 0;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        ippsSet_16s (*(Ipp16s*)pBlock, ((Ipp16s*)pBlock)+ 1,rightY);
        pBlock += srcStep;
    }
    return ippStsNoErr;
}
}
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
