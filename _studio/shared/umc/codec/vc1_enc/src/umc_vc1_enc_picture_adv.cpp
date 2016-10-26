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

#include "umc_vc1_enc_picture_adv.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_tables.h"
#include "umc_vc1_common_zigzag_tbl.h"
#include "ippvc.h"
#include "ippi.h"
#include "umc_vc1_enc_block.h"
#include "umc_vc1_enc_block_template.h"
#include "umc_vc1_enc_debug.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_enc_statistic.h"
#include "umc_vc1_enc_pred.h"
#include "umc_vc1_enc_mode_decision.h"
#include <new.h>

namespace UMC_VC1_ENCODER
{
    UMC::Status  VC1EncoderPictureADV::CheckParameters(vm_char* pLastError, bool bSecondField)
    {
        if (!m_pSequenceHeader)
        {
            vm_string_sprintf(pLastError,VM_STRING("Error. pic. header parameter: seq. header is NULL\n"));
            return UMC::UMC_ERR_FAILED;
        }

        if (m_uiDecTypeAC1>2)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: DecTypeAC1\n"));
            m_uiDecTypeAC1 = 0;
        }

        if (m_uiDecTypeDCIntra>1)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: DecTypeAC2\n"));
            m_uiDecTypeDCIntra = 0;
        }
        if (!m_uiQuantIndex || m_uiQuantIndex >31)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiQuantIndex\n"));
            m_uiQuantIndex = 1;
        }
        if (m_bHalfQuant && m_uiQuantIndex>8)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_bHalfQuant\n"));
            m_bHalfQuant = false;
        }

        switch (m_pSequenceHeader->GetQuantType())
        {
        case VC1_ENC_QTYPE_IMPL:
            m_bUniformQuant = (m_uiQuantIndex>8)? false:true;
            m_uiQuant       = quantValue[m_uiQuantIndex];
            break;
        case VC1_ENC_QTYPE_UF:
            m_bUniformQuant = true;
            m_uiQuant       = m_uiQuantIndex;
            break;
        case VC1_ENC_QTYPE_NUF:
            m_bUniformQuant = false;
            m_uiQuant       = m_uiQuantIndex;
            break;
        }
        if (!m_pSequenceHeader->IsInterlace())
        {
            m_bUVSamplingInterlace  = 0;
            m_nReferenceFrameDist   = 0;
        }
        else if (!m_pSequenceHeader->IsReferenceFrameDistance())
        {
            m_nReferenceFrameDist = 0;
        }
        switch(m_uiPictureType)
        {
        case VC1_ENC_I_FRAME:
            return CheckParametersI(pLastError);
        case VC1_ENC_I_I_FIELD:
            return CheckParametersIField(pLastError);
        case VC1_ENC_I_P_FIELD:
            if(!bSecondField)
                return CheckParametersIField(pLastError);
            else
                return CheckParametersPField(pLastError);
        case VC1_ENC_P_FRAME:
            return CheckParametersP(pLastError);
        case VC1_ENC_P_I_FIELD:
            if(!bSecondField)
                return CheckParametersPField(pLastError);
            else
                return CheckParametersIField(pLastError);
        case VC1_ENC_P_P_FIELD:
            return CheckParametersPField(pLastError);
        case VC1_ENC_B_FRAME:
            return CheckParametersB(pLastError);
        case VC1_ENC_B_BI_FIELD:
            if(!bSecondField)
                return CheckParametersBField(pLastError);
            else
                return CheckParametersIField(pLastError);
        case VC1_ENC_BI_B_FIELD:
            if(!bSecondField)
                return CheckParametersIField(pLastError);
            else
                return CheckParametersBField(pLastError);
        case VC1_ENC_B_B_FIELD:
            return CheckParametersBField(pLastError);
        default:
            assert(0);
            break;

        }
        return UMC::UMC_OK;
    }

    UMC::Status  VC1EncoderPictureADV::CheckParametersI(vm_char* pLastError)
    {
        if (m_uiDecTypeAC2>2)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: DecTypeAC2\n"));
            m_uiDecTypeAC2 = 0;
        }

        if (m_uiRoundControl>1)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiRoundControl>1\n"));
            m_uiRoundControl = 0;
        }
        if (m_uiCondOverlap !=  VC1_ENC_COND_OVERLAP_NO   &&
            m_uiCondOverlap !=  VC1_ENC_COND_OVERLAP_SOME &&
            m_uiCondOverlap !=  VC1_ENC_COND_OVERLAP_ALL)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: CondOverlap\n"));
            m_uiCondOverlap = VC1_ENC_COND_OVERLAP_NO;
        }

        return UMC::UMC_OK;
    }

    UMC::Status  VC1EncoderPictureADV::CheckParametersIField(vm_char* pLastError)
    {
        if (m_uiDecTypeAC2>2)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: DecTypeAC2\n"));
            m_uiDecTypeAC2 = 0;
        }

        if (m_uiRoundControl>1)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiRoundControl>1\n"));
            m_uiRoundControl = 0;
        }
        if (m_uiCondOverlap !=  VC1_ENC_COND_OVERLAP_NO   &&
            m_uiCondOverlap !=  VC1_ENC_COND_OVERLAP_SOME &&
            m_uiCondOverlap !=  VC1_ENC_COND_OVERLAP_ALL)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: CondOverlap\n"));
            m_uiCondOverlap = VC1_ENC_COND_OVERLAP_NO;
        }

        return UMC::UMC_OK;
    }

    UMC::Status  VC1EncoderPictureADV::CheckParametersP(vm_char* pLastError)
    {

        if  (!m_pSequenceHeader->IsExtendedMV() && m_uiMVRangeIndex>0 || m_uiMVRangeIndex>3)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: MVRangeIndex\n"));
            m_uiMVRangeIndex = 0;
        }
        if (m_bIntensity)
        {
            if (m_uiIntensityLumaScale>63)
            {
                vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaScale=63\n"));
                m_uiIntensityLumaScale=63;
            }
            if (m_uiIntensityLumaShift>63)
            {
                vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaShift=63\n"));
                m_uiIntensityLumaShift=63;
            }
        }
        else
        {
            if (m_uiIntensityLumaScale>0)
            {
                vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaScale=0\n"));
                m_uiIntensityLumaScale=0;
            }
            if (m_uiIntensityLumaShift>0)
            {
                vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaShift=0\n"));
                m_uiIntensityLumaShift=0;
            }
        }
        if (m_uiMVTab>4)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: MVTab=4\n"));
            m_uiMVTab=4;
        }
        if (m_uiCBPTab>4)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: CBPTab=4\n"));
            m_uiCBPTab=4;
        }
        if (m_uiAltPQuant>0 && m_pSequenceHeader->GetDQuant()==0)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: AltPQuant=0\n"));
            m_uiAltPQuant=0;
        }
        else if (m_pSequenceHeader->GetDQuant()!=0 && (m_uiAltPQuant==0||m_uiAltPQuant>31))
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: AltPQuant=Quant\n"));
            m_uiAltPQuant = m_uiQuant;
        }
        if ( m_QuantMode != VC1_ENC_QUANT_SINGLE && m_pSequenceHeader->GetDQuant()==0)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: QuantMode = VC1_ENC_QUANT_SINGLE\n"));
            m_QuantMode = VC1_ENC_QUANT_SINGLE;
        }
        if (m_bVSTransform && !m_pSequenceHeader->IsVSTransform())
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: VSTransform = false\n"));
            m_bVSTransform = false;
        }
        if (!m_bVSTransform && m_uiTransformType!= VC1_ENC_8x8_TRANSFORM)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: TransformType == VC1_ENC_8x8_TRANSFORM\n"));
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
        }
        return UMC::UMC_OK;
    }

    UMC::Status  VC1EncoderPictureADV::CheckParametersPField(vm_char* pLastError)
    {

        if  (!m_pSequenceHeader->IsExtendedMV() && m_uiMVRangeIndex>0 || m_uiMVRangeIndex > 3)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: MVRangeIndex\n"));
            m_uiMVRangeIndex = 0;
        }

        if(!m_pSequenceHeader->IsExtendedDMV() && m_uiDMVRangeIndex > 0 ||m_uiDMVRangeIndex > 3)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: DMVRangeIndex\n"));
            m_uiDMVRangeIndex = 0;
        }

        if(!(m_uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST ||
            m_uiReferenceFieldType == VC1_ENC_REF_FIELD_SECOND  ||
            m_uiReferenceFieldType == VC1_ENC_REF_FIELD_BOTH))
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: ReferenceFieldType\n"));
            m_uiReferenceFieldType = VC1_ENC_REF_FIELD_FIRST;
        }

        if (m_bIntensity)
        {
            if (m_uiIntensityLumaScale>63)
            {
                vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaScale=63\n"));
                m_uiIntensityLumaScale = 63;
            }
            if (m_uiIntensityLumaShift > 63)
            {
                vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaShift=63\n"));
                m_uiIntensityLumaShift=63;
            }
        }
        else
        {
            if (m_uiIntensityLumaScale > 0)
            {
                vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaScale=0\n"));
                m_uiIntensityLumaScale=0;
            }
            if (m_uiIntensityLumaShift > 0)
            {
                vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaShift=0\n"));
                m_uiIntensityLumaShift=0;
            }
        }
        if (m_uiMVTab>4)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: MVTab=4\n"));
            m_uiMVTab=4;
        }
        if (m_uiCBPTab>4)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: CBPTab=4\n"));
            m_uiCBPTab=4;
        }
        if (m_uiAltPQuant>0 && m_pSequenceHeader->GetDQuant()==0)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: AltPQuant=0\n"));
            m_uiAltPQuant=0;
        }
        else if (m_pSequenceHeader->GetDQuant()!=0 && (m_uiAltPQuant==0||m_uiAltPQuant>31))
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: AltPQuant=Quant\n"));
            m_uiAltPQuant = m_uiQuant;
        }
        if ( m_QuantMode != VC1_ENC_QUANT_SINGLE && m_pSequenceHeader->GetDQuant()==0)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: QuantMode = VC1_ENC_QUANT_SINGLE\n"));
            m_QuantMode = VC1_ENC_QUANT_SINGLE;
        }
        if (m_bVSTransform && !m_pSequenceHeader->IsVSTransform())
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: VSTransform = false\n"));
            m_bVSTransform = false;
        }
        if (!m_bVSTransform && m_uiTransformType!= VC1_ENC_8x8_TRANSFORM)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: TransformType == VC1_ENC_8x8_TRANSFORM\n"));
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
        }

        if(m_bRepeateFirstField)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_bRepeateFirstField == false, true not supported\n"));
            m_bRepeateFirstField = false;
        }

        if(m_nReferenceFrameDist > 16)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_nReferenceFrameDist > 16\n"));
            m_nReferenceFrameDist = 0;
        }

        return UMC::UMC_OK;
    }

    UMC::Status  VC1EncoderPictureADV::CheckParametersB(vm_char* pLastError)
    {
        if  (!m_pSequenceHeader->IsExtendedMV() && m_uiMVRangeIndex>0 || m_uiMVRangeIndex > 3)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: MVRangeIndex\n"));
            m_uiMVRangeIndex = 0;
        }

        if ((m_uiBFraction.denom < 2|| m_uiBFraction.denom >8)&& (m_uiBFraction.denom != 0xff))
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiBFraction.denom\n"));
            m_uiBFraction.denom = 2;
        }
        if ((m_uiBFraction.num  < 1|| m_uiBFraction.num >= m_uiBFraction.denom)&& (m_uiBFraction.num != 0xff))
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiBFraction.num\n"));
            m_uiBFraction.num = 1;
        }
        if (m_bVSTransform && !m_pSequenceHeader->IsVSTransform())
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: VSTransform = false\n"));
            m_bVSTransform = false;
        }
        if (!m_bVSTransform && m_uiTransformType!= VC1_ENC_8x8_TRANSFORM)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: TransformType == VC1_ENC_8x8_TRANSFORM\n"));
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
        }

        return UMC::UMC_OK;
    }

    UMC::Status  VC1EncoderPictureADV::CheckParametersBField(vm_char* pLastError)
    {
        if  (!m_pSequenceHeader->IsExtendedMV() && m_uiMVRangeIndex>0 || m_uiMVRangeIndex > 3)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: MVRangeIndex\n"));
            m_uiMVRangeIndex = 0;
        }

        if(!m_pSequenceHeader->IsExtendedDMV() && m_uiDMVRangeIndex > 0 ||m_uiDMVRangeIndex > 3)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: DMVRangeIndex\n"));
            m_uiDMVRangeIndex = 0;
        }

        if ((m_uiBFraction.denom < 2|| m_uiBFraction.denom >8)&& (m_uiBFraction.denom != 0xff))
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiBFraction.denom\n"));
            m_uiBFraction.denom = 2;
        }
        if ((m_uiBFraction.num  < 1|| m_uiBFraction.num >= m_uiBFraction.denom)&& (m_uiBFraction.num != 0xff))
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiBFraction.num\n"));
            m_uiBFraction.num = 1;
        }
        if(m_uiReferenceFieldType != VC1_ENC_REF_FIELD_BOTH)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: ReferenceFieldType\n"));
            m_uiReferenceFieldType = VC1_ENC_REF_FIELD_BOTH;
        }
        if (m_bVSTransform && !m_pSequenceHeader->IsVSTransform())
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: VSTransform = false\n"));
            m_bVSTransform = false;
        }
        if (!m_bVSTransform && m_uiTransformType!= VC1_ENC_8x8_TRANSFORM)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: TransformType == VC1_ENC_8x8_TRANSFORM\n"));
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
        }

        if(m_nReferenceFrameDist > 16 && m_nReferenceFrameDist == 0)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_nReferenceFrameDist > 16\n"));
            m_nReferenceFrameDist = 1;
        }

        return UMC::UMC_OK;
    }

    UMC::Status VC1EncoderPictureADV::Init (VC1EncoderSequenceADV* SequenceHeader,
        VC1EncoderMBs* pMBs, VC1EncoderCodedMB* pCodedMB)
    {

        if (!SequenceHeader || !pMBs || !pCodedMB)
            return UMC::UMC_ERR_NULL_PTR;

        Close();

        m_pSequenceHeader = SequenceHeader;

        m_pMBs = pMBs;
        m_pCodedMB = pCodedMB;

        return UMC::UMC_OK;
    }


    UMC::Status VC1EncoderPictureADV::Close()
    {

        Reset();
        ResetPlanes();
        return UMC::UMC_OK;
    }


    UMC::Status  VC1EncoderPictureADV::SetPlaneParams  (Frame* pFrame, ePlaneType type)
    {
        Ipp8u** pPlane;
        Ipp32u* pPlaneStep;
        Ipp32u *padding;
        bool *interlace;
        Ipp32u temp;
        bool t;
        switch(type)
        {
        case  VC1_ENC_CURR_PLANE:
            pPlane     = m_pPlane;
            pPlaneStep = m_uiPlaneStep;
            padding    = &temp;
            interlace  = &t;
            break;
        case  VC1_ENC_RAISED_PLANE:
            pPlane     = m_pRaisedPlane;
            pPlaneStep = m_uiRaisedPlaneStep;
            padding    = &m_uiRaisedPlanePadding;
            interlace  = &t;
            break;
        case  VC1_ENC_FORWARD_PLANE:
            pPlane     = m_pForwardPlane;
            pPlaneStep = m_uiForwardPlaneStep;
            padding    = &m_uiForwardPlanePadding;
            interlace  = &m_bForwardInterlace;
            break;
        case  VC1_ENC_BACKWARD_PLANE:
            pPlane     = m_pBackwardPlane;
            pPlaneStep = m_uiBackwardPlaneStep;
            padding    = &m_uiBackwardPlanePadding;
            interlace  = &m_bBackwardInterlace;
            break;
        default:
            return UMC::UMC_ERR_FAILED;
        }
        pPlane[0]       = pFrame->GetYPlane();
        pPlane[1]       = pFrame->GetUPlane();
        pPlane[2]       = pFrame->GetVPlane();
        pPlaneStep[0]   = pFrame->GetYStep();
        pPlaneStep[1]   = pFrame->GetUStep();
        pPlaneStep[2]   = pFrame->GetVStep();
        *padding        = pFrame->GetPaddingSize();
        *interlace      = pFrame->isInterlace();

        return UMC::UMC_OK;
    }


    UMC::Status VC1EncoderPictureADV::SetInitPictureParams(InitPictureParams* InitPicParam)
    {
        m_uiMVMode             = InitPicParam->uiMVMode;
        m_uiBFraction.num      = InitPicParam->uiBFraction.num;
        m_uiBFraction.denom    = InitPicParam->uiBFraction.denom;
        m_uiMVRangeIndex       = InitPicParam->uiMVRangeIndex;
        m_uiReferenceFieldType = InitPicParam->uiReferenceFieldType;
        m_nReferenceFrameDist  = InitPicParam->nReferenceFrameDist;
        m_uiMVTab              = InitPicParam->sVLCTablesIndex.uiMVTab;
        m_uiDecTypeAC1         = InitPicParam->sVLCTablesIndex.uiDecTypeAC;
        m_uiCBPTab             = InitPicParam->sVLCTablesIndex.uiCBPTab;    
        m_uiDecTypeDCIntra     = InitPicParam->sVLCTablesIndex.uiDecTypeDCIntra;

        m_bUVSamplingInterlace = 0;                         // can be true if interlace mode is switch
        m_bRepeateFirstField   = false;
        m_bTopFieldFirst       = InitPicParam->bTopFieldFirst;

        return UMC::UMC_OK;
    }


    UMC::Status VC1EncoderPictureADV::SetPictureParams(ePType Type, Ipp16s* pSavedMV, Ipp8u* pRefType, bool bSecondField, Ipp32u OverlapSmoothing)
    {

        m_uiPictureType     =  Type;
        m_pSavedMV  = pSavedMV;
        m_pRefType = pRefType;

        switch (m_uiPictureType)
        {
        case VC1_ENC_I_FRAME:
            return SetPictureParamsI(OverlapSmoothing);
        case VC1_ENC_P_FRAME:
            return SetPictureParamsP();
        case VC1_ENC_B_FRAME:
            return SetPictureParamsB();
        case VC1_ENC_SKIP_FRAME:
            return UMC::UMC_OK;
        case VC1_ENC_I_I_FIELD:
            return SetPictureParamsIField(OverlapSmoothing);
        case VC1_ENC_P_I_FIELD:
            if(!bSecondField)
                return SetPictureParamsPField();
            else
                return SetPictureParamsIField(OverlapSmoothing);
        case VC1_ENC_I_P_FIELD:
            if(!bSecondField)
                return SetPictureParamsIField(OverlapSmoothing);
            else
                return SetPictureParamsPField();
        case VC1_ENC_P_P_FIELD:
            return SetPictureParamsPField();
        case VC1_ENC_B_BI_FIELD:
            if(!bSecondField)
                return SetPictureParamsBField();
            else
                return SetPictureParamsIField(OverlapSmoothing);
        case VC1_ENC_BI_B_FIELD:
            if(!bSecondField)
                return SetPictureParamsIField(OverlapSmoothing);
            else
                return SetPictureParamsBField();
        case VC1_ENC_B_B_FIELD:
            return SetPictureParamsBField();

        }
        return UMC::UMC_ERR_NOT_IMPLEMENTED;

    }

    UMC::Status VC1EncoderPictureADV::SetPictureQuantParams(Ipp8u uiQuantIndex, bool bHalfQuant)
    {
        m_uiQuantIndex = uiQuantIndex;
        m_bHalfQuant = bHalfQuant;

        if (m_uiQuantIndex > 31)
            return UMC::UMC_ERR_FAILED;

        if (m_uiQuantIndex == 0)
            return UMC::UMC_ERR_FAILED;

        switch (m_pSequenceHeader->GetQuantType())
        {
        case VC1_ENC_QTYPE_IMPL:
            m_bUniformQuant = (m_uiQuantIndex>8)? false:true;
            m_uiQuant       = quantValue[m_uiQuantIndex];
            break;
        case VC1_ENC_QTYPE_EXPL:
            m_bUniformQuant = true;                         // can be true or false
            m_uiQuant       = m_uiQuantIndex;
            break;
        case VC1_ENC_QTYPE_UF:
            m_bUniformQuant = true;
            m_uiQuant       = m_uiQuantIndex;
            break;
        case VC1_ENC_QTYPE_NUF:
            m_bUniformQuant = false;
            m_uiQuant       = m_uiQuantIndex;
            break;
        default:
            return UMC::UMC_ERR_FAILED;
        }

        return UMC::UMC_OK;
    }

    UMC::Status VC1EncoderPictureADV::SetPictureParamsI(Ipp32u OverlapSmoothing)
    {
        //m_uiBFraction.denom     = 2;                              // [2,8]
        //m_uiBFraction.num       = 1;                              // [1, m_uiBFraction.denom - 1]
        m_uiDecTypeAC2          = 0;                                // [0,2] - it's used to choose decoding table

        m_uiRoundControl        = 0;
        switch (OverlapSmoothing)
        {
        case 0:
        case 1:
            m_uiCondOverlap         = VC1_ENC_COND_OVERLAP_NO;
            break;
        case 2:
            m_uiCondOverlap         = VC1_ENC_COND_OVERLAP_SOME;
            break;
        default:
            m_uiCondOverlap         = VC1_ENC_COND_OVERLAP_ALL;
            break;
        }
        return UMC::UMC_OK;
    }
    UMC::Status VC1EncoderPictureADV::SetPictureParamsIField(Ipp32u OverlapSmoothing)
    {
        m_nReferenceFrameDist = 0;
        m_uiRoundControl      = 0;

        m_uiDecTypeAC2          = 0;                                // [0,2] - it's used to choose decoding table

        switch (OverlapSmoothing)
        {
        case 0:
        case 1:
            m_uiCondOverlap         = VC1_ENC_COND_OVERLAP_NO;
            break;
        case 2:
            m_uiCondOverlap         = VC1_ENC_COND_OVERLAP_SOME;
            break;
        default:
            m_uiCondOverlap         = VC1_ENC_COND_OVERLAP_ALL;
            break;
        }

        return UMC::UMC_OK;
    }

    UMC::Status VC1EncoderPictureADV::SetPictureParamsPField()
    {

        if (m_pSequenceHeader->GetDQuant()==0)
        {
            m_uiAltPQuant=0;
        }
        else
        {
            m_uiAltPQuant = m_uiQuant;          // [1,31]
        }

        switch( m_pSequenceHeader->GetDQuant())
        {
        case 0:
            m_QuantMode = VC1_ENC_QUANT_SINGLE;
            break;
        case 1:
            m_QuantMode = VC1_ENC_QUANT_SINGLE;//        VC1_ENC_QUANT_SINGLE,
            //        VC1_ENC_QUANT_MB_ANY,
            //        VC1_ENC_QUANT_MB_PAIR,
            //        VC1_ENC_QUANT_EDGE_ALL,
            //        VC1_ENC_QUANT_EDGE_LEFT,
            //        VC1_ENC_QUANT_EDGE_TOP,
            //        VC1_ENC_QUANT_EDGE_RIGHT,
            //        VC1_ENC_QUANT_EDGE_BOTTOM,
            //        VC1_ENC_QUANT_EDGES_LEFT_TOP,
            //        VC1_ENC_QUANT_EDGES_TOP_RIGHT,
            //        VC1_ENC_QUANT_EDGES_RIGHT_BOTTOM,
            //        VC1_ENC_QUANT_EDGSE_BOTTOM_LEFT,
            break;
        case 2:
            m_QuantMode = VC1_ENC_QUANT_MB_PAIR;
            break;
        }
        if (!m_pSequenceHeader->IsVSTransform())
        {
            m_bVSTransform = false;
        }
        else
        {
            m_bVSTransform = false;                          // true & false

        }
        if (!m_bVSTransform)
        {
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
        }
        else
        {
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;      //VC1_ENC_8x8_TRANSFORM
            //VC1_ENC_8x4_TRANSFORM
            //VC1_ENC_4x8_TRANSFORM
            //VC1_ENC_4x4_TRANSFORM
        }

        return UMC::UMC_OK;
    }

    UMC::Status VC1EncoderPictureADV::SetPictureParamsP()
    {

        if (m_pSequenceHeader->GetDQuant()==0)
        {
            m_uiAltPQuant=0;
        }
        else
        {
            m_uiAltPQuant = m_uiQuant;          // [1,31]
        }

        switch( m_pSequenceHeader->GetDQuant())
        {
        case 0:
            m_QuantMode = VC1_ENC_QUANT_SINGLE;
            break;
        case 1:
            m_QuantMode = VC1_ENC_QUANT_SINGLE;//        VC1_ENC_QUANT_SINGLE,
            //        VC1_ENC_QUANT_MB_ANY,
            //        VC1_ENC_QUANT_MB_PAIR,
            //        VC1_ENC_QUANT_EDGE_ALL,
            //        VC1_ENC_QUANT_EDGE_LEFT,
            //        VC1_ENC_QUANT_EDGE_TOP,
            //        VC1_ENC_QUANT_EDGE_RIGHT,
            //        VC1_ENC_QUANT_EDGE_BOTTOM,
            //        VC1_ENC_QUANT_EDGES_LEFT_TOP,
            //        VC1_ENC_QUANT_EDGES_TOP_RIGHT,
            //        VC1_ENC_QUANT_EDGES_RIGHT_BOTTOM,
            //        VC1_ENC_QUANT_EDGSE_BOTTOM_LEFT,
            break;
        case 2:
            m_QuantMode = VC1_ENC_QUANT_MB_PAIR;
            break;
        }
        if (!m_pSequenceHeader->IsVSTransform())
        {
            m_bVSTransform = false;
        }
        else
        {
            m_bVSTransform = false;                          // true & false

        }
        if (!m_bVSTransform)
        {
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
        }
        else
        {
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;      //VC1_ENC_8x8_TRANSFORM
            //VC1_ENC_8x4_TRANSFORM
            //VC1_ENC_4x8_TRANSFORM
            //VC1_ENC_4x4_TRANSFORM

        }

        return UMC::UMC_OK;
    }

    UMC::Status VC1EncoderPictureADV::SetPictureParamsB()
    {
        //m_uiMVMode = VC1_ENC_1MV_HALF_BILINEAR;                      //    VC1_ENC_1MV_HALF_BILINEAR
        //                                                             //    VC1_ENC_1MV_QUARTER_BICUBIC

        m_uiCBPTab  =0;                                             //[0,4]

        if (m_pSequenceHeader->GetDQuant()==0)
        {
            m_uiAltPQuant=0;
        }
        else
        {
            m_uiAltPQuant = m_uiQuant;          // [1,31]
        }

        switch( m_pSequenceHeader->GetDQuant())
        {
        case 0:
            m_QuantMode = VC1_ENC_QUANT_SINGLE;
            break;
        case 1:
            m_QuantMode = VC1_ENC_QUANT_SINGLE;//        VC1_ENC_QUANT_SINGLE,
            //        VC1_ENC_QUANT_MB_ANY,
            //        VC1_ENC_QUANT_MB_PAIR,
            //        VC1_ENC_QUANT_EDGE_ALL,
            //        VC1_ENC_QUANT_EDGE_LEFT,
            //        VC1_ENC_QUANT_EDGE_TOP,
            //        VC1_ENC_QUANT_EDGE_RIGHT,
            //        VC1_ENC_QUANT_EDGE_BOTTOM,
            //        VC1_ENC_QUANT_EDGES_LEFT_TOP,
            //        VC1_ENC_QUANT_EDGES_TOP_RIGHT,
            //        VC1_ENC_QUANT_EDGES_RIGHT_BOTTOM,
            //        VC1_ENC_QUANT_EDGSE_BOTTOM_LEFT,
            break;
        case 2:
            m_QuantMode = VC1_ENC_QUANT_MB_PAIR;
            break;
        }
        if (!m_pSequenceHeader->IsVSTransform())
        {
            m_bVSTransform = false;  // true & false
        }
        else
        {
            m_bVSTransform = false;

        }
        if (!m_bVSTransform)
        {
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
        }
        else
        {
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;      //VC1_ENC_8x8_TRANSFORM
            //VC1_ENC_8x4_TRANSFORM
            //VC1_ENC_4x8_TRANSFORM
            //VC1_ENC_4x4_TRANSFORM
        }
        return UMC::UMC_OK;
    }

    UMC::Status VC1EncoderPictureADV::SetPictureParamsBField()
    {
        //m_uiMVMode = VC1_ENC_1MV_HALF_BILINEAR;                      //    VC1_ENC_1MV_HALF_BILINEAR
        //                                                             //    VC1_ENC_1MV_QUARTER_BICUBIC

        m_uiCBPTab  =0;                                             //[0,4]

        if (m_pSequenceHeader->GetDQuant()==0)
        {
            m_uiAltPQuant=0;
        }
        else
        {
            m_uiAltPQuant = m_uiQuant;          // [1,31]
        }

        switch( m_pSequenceHeader->GetDQuant())
        {
        case 0:
            m_QuantMode = VC1_ENC_QUANT_SINGLE;
            break;
        case 1:
            m_QuantMode = VC1_ENC_QUANT_SINGLE;//        VC1_ENC_QUANT_SINGLE,
            //        VC1_ENC_QUANT_MB_ANY,
            //        VC1_ENC_QUANT_MB_PAIR,
            //        VC1_ENC_QUANT_EDGE_ALL,
            //        VC1_ENC_QUANT_EDGE_LEFT,
            //        VC1_ENC_QUANT_EDGE_TOP,
            //        VC1_ENC_QUANT_EDGE_RIGHT,
            //        VC1_ENC_QUANT_EDGE_BOTTOM,
            //        VC1_ENC_QUANT_EDGES_LEFT_TOP,
            //        VC1_ENC_QUANT_EDGES_TOP_RIGHT,
            //        VC1_ENC_QUANT_EDGES_RIGHT_BOTTOM,
            //        VC1_ENC_QUANT_EDGSE_BOTTOM_LEFT,
            break;
        case 2:
            m_QuantMode = VC1_ENC_QUANT_MB_PAIR;
            break;
        }
        if (!m_pSequenceHeader->IsVSTransform())
        {
            m_bVSTransform = false;  // true & false
        }
        else
        {
            m_bVSTransform = false;

        }
        if (!m_bVSTransform)
        {
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
        }
        else
        {
            m_uiTransformType = VC1_ENC_8x8_TRANSFORM;      //VC1_ENC_8x8_TRANSFORM
            //VC1_ENC_8x4_TRANSFORM
            //VC1_ENC_4x8_TRANSFORM
            //VC1_ENC_4x4_TRANSFORM
        }
        return UMC::UMC_OK;
    }


    UMC::Status VC1EncoderPictureADV::WriteIPictureHeader(VC1EncoderBitStreamAdv* pCodedPicture)
    {
        UMC::Status     err             =   UMC::UMC_OK;
        static const  Ipp8u condoverVLC[6]  =   {0,1,  3,2,   2,2};


        err = pCodedPicture->PutStartCode(0x0000010D);
        if (err != UMC::UMC_OK) return err;


        if (m_pSequenceHeader->IsInterlace())
        {
            err = pCodedPicture->PutBits(0,1); // progressive frame
            if (err != UMC::UMC_OK) return err;
        }

        //picture type - I frame
        err = pCodedPicture->PutBits(0x06,3);
        if (err != UMC::UMC_OK) return err;

        if (m_pSequenceHeader->IsFrameCounter())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsPullDown())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsPanScan())
        {
            assert(0);
        }
        err = pCodedPicture->PutBits(m_uiRoundControl,1);
        if (err != UMC::UMC_OK) return err;

        if (m_pSequenceHeader->IsInterlace())
        {
            err = pCodedPicture->PutBits(m_bUVSamplingInterlace,1);
            if (err != UMC::UMC_OK) return err;
        }

        if (m_pSequenceHeader->IsFrameInterpolation())
        {
            err =pCodedPicture->PutBits(m_bFrameInterpolation,1);
            if (err != UMC::UMC_OK)
                return err;
        }

        err = pCodedPicture->PutBits(m_uiQuantIndex, 5);
        if (err != UMC::UMC_OK)
            return err;

        if (m_uiQuantIndex <= 8)
        {
            err = pCodedPicture->PutBits(m_bHalfQuant, 1);
            if (err != UMC::UMC_OK)
                return err;
        }
        if (m_pSequenceHeader->GetQuantType()== VC1_ENC_QTYPE_EXPL)
        {
            err = pCodedPicture->PutBits(m_bUniformQuant,1);
            if (err != UMC::UMC_OK)
                return err;
        }
        if (m_pSequenceHeader->IsPostProc())
        {
            assert(0);
        }

        assert( m_bRawBitplanes == true);

        //raw bitplane for AC prediction
        err = pCodedPicture->PutBits(0,5);
        if (err != UMC::UMC_OK)   return err;

        if (m_pSequenceHeader->IsOverlap() && m_uiQuant<=8)
        {
            err = pCodedPicture->PutBits(condoverVLC[2*m_uiCondOverlap],condoverVLC[2*m_uiCondOverlap+1]);
            if (err != UMC::UMC_OK)   return err;
            if (VC1_ENC_COND_OVERLAP_SOME == m_uiCondOverlap)
            {
                //bitplane
                assert( m_bRawBitplanes == true);
                //raw bitplane for AC prediction
                err = pCodedPicture->PutBits(0,5);
                if (err != UMC::UMC_OK)   return err;
            }
        }

        err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC1],ACTableCodesVLC[2*m_uiDecTypeAC1+1]);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC2],ACTableCodesVLC[2*m_uiDecTypeAC2+1]);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiDecTypeDCIntra,1);




        return err;
    }  

    UMC::Status VC1EncoderPictureADV::PAC_IFrame(UMC::MeParams* pME, Ipp32s firstRow, Ipp32s nRows)
    {
        Ipp32s                      i=0, j=0, blk = 0;
        bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);

        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      h = (m_pSequenceHeader->GetPictureHeight()+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth()+15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;


        //inverse transform quantization
        //forward transform quantization

        IntraTransformQuantFunction    TransformQuantACFunctionLuma = (m_bUniformQuant) ? 
            ((pME&&pME->UseTrellisQuantization)? IntraTransformQuantTrellis: IntraTransformQuantUniform) :
            IntraTransformQuantNonUniform;
        IntraTransformQuantFunction    TransformQuantACFunctionChroma = (m_bUniformQuant) ? 
            ((pME&&pME->UseTrellisQuantizationChroma)? IntraTransformQuantTrellis: IntraTransformQuantUniform) :
            IntraTransformQuantNonUniform;

        IntraTransformQuantFunction    TransformQuantACFunction[6] = 
        {
            TransformQuantACFunctionLuma,
            TransformQuantACFunctionLuma,
            TransformQuantACFunctionLuma,
            TransformQuantACFunctionLuma,
            TransformQuantACFunctionChroma,
            TransformQuantACFunctionChroma
        };    
        //inverse transform quantization
        IntraInvTransformQuantFunction InvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :  IntraInvTransformQuantNonUniform;
        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
        bool                        dACPrediction   = true;


        IppiSize                    blkSize     = {8,8};
        IppiSize                    blkSizeLuma = {16,16};

        Ipp8u deblkPattern = (firstRow != hMB - 1)? 0 : 0x4;     //3 bits: right 1 bit - 0 - left/ 1 - not left
        //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row, 10 - top bottom row
        Ipp8u deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFE : 0;

        Ipp8u smoothMask = (m_pSequenceHeader->IsOverlap() && ((m_uiQuant >= 9) || m_uiCondOverlap != VC1_ENC_COND_OVERLAP_NO)) ? 0xFF : 0;                                                                                                  

        Ipp8u smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
        //0/1 - top/not top, 0/1 - left/not left
        SmoothInfo_I_Adv smoothInfo = {0};

        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];
        Ipp16s                      *pSavedMV       = (m_pSavedMV)? m_pSavedMV + w*firstRow*2:0;


        if (pSavedMV)
        {
            memset(pSavedMV,0,w*(hMB - firstRow)*2*sizeof(Ipp16s));
        }
        err = m_pMBs->Reset();
        if (err != UMC::UMC_OK)
            return err;

#ifdef VC1_ENC_DEBUG_ON
        pDebug->SetCurrMB(false, 0, firstRow);
        pDebug->SetSliceInfo(firstRow, firstRow + nRows);
        //pDebug->SetCurrMBFirst();
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
        pDebug->SetVTSFlag(false);
#endif

        for (i = firstRow; i < hMB; i++)
        {
            Ipp8u*  pCurMBRow[3] =  { m_pPlane[0] + i*m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE ,
                                      m_pPlane[1] + i*m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
                                      m_pPlane[2] + i*m_uiPlaneStep[2]*VC1_ENC_CHROMA_SIZE};

            Ipp8u *pRFrameY = 0;
            Ipp8u *pRFrameU = 0;
            Ipp8u *pRFrameV = 0;

            if (bRaiseFrame)
            {
                pRFrameY = m_pRaisedPlane[0]+VC1_ENC_LUMA_SIZE*(i*m_uiRaisedPlaneStep[0]);
                pRFrameU = m_pRaisedPlane[1]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[1]);
                pRFrameV = m_pRaisedPlane[2]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[2]);
            }

            for (j=0; j < w; j++)
            {
                Ipp8u               MBPattern  = 0;
                Ipp8u               CBPCY      = 0;
                VC1EncoderMBInfo*   pCurMBInfo = 0;
                VC1EncoderMBData*   pCurMBData = 0;
                VC1EncoderMBData*   pRecMBData = 0;

                Ipp32s              xLuma      = VC1_ENC_LUMA_SIZE*j;
                Ipp32s              xChroma    = VC1_ENC_CHROMA_SIZE*j;
                NeighbouringMBsData MBs;
                VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);

                pCurMBInfo  =   m_pMBs->GetCurrMBInfo();
                pCurMBData  =   m_pMBs->GetCurrMBData();
                pRecMBData  =   m_pMBs->GetRecCurrMBData();

                MBs.LeftMB    = m_pMBs->GetLeftMBData();
                MBs.TopMB     = m_pMBs->GetTopMBData();
                MBs.TopLeftMB = m_pMBs->GetTopLeftMBData();
                pCompressedMB ->Init(VC1_ENC_I_MB);
                pCurMBInfo->Init(true);

                /*------------------- Compressing  ------------------------------------------------------*/

                pCurMBData->CopyMBProg(pCurMBRow[0],m_uiPlaneStep[0],pCurMBRow[1], pCurMBRow[2], m_uiPlaneStep[1],j);


                for (blk = 0; blk<6; blk++)
                {
                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                    _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],  blkSize, 0);
                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                    STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                    TransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                        DCQuant, doubleQuant,pME->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                    STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                }
                STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                // intra prediction
                dACPrediction = DCACPredictionFrame( pCurMBData,
                    &MBs,
                    pRecMBData,
                    0,
                    direction);
                STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                for (blk=0; blk<6; blk++)
                {
                    pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                        pRecMBData->m_uiBlockStep[blk],
                        ZagTables_I[direction[blk]* dACPrediction],
                        blk);
                }

                MBPattern = pCompressedMB->GetMBPattern();

                VC1EncoderMBInfo* t = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* l = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* tl = m_pMBs->GetTopLeftMBInfo();

                CBPCY = Get_CBPCY(MBPattern, (t)? t->GetPattern():0, (l)? l->GetPattern():0, (tl)? tl->GetPattern():0);

                pCurMBInfo->SetMBPattern(MBPattern);

                pCompressedMB->SetACPrediction(dACPrediction);
                pCompressedMB->SetMBCBPCY(CBPCY);


#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(VC1_ENC_I_MB);
                pDebug->SetCPB(MBPattern, CBPCY);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                /*--------------Reconstruction (if is needed)--------------------------------------------------*/
                STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                if (bRaiseFrame)
                {
                    for (blk=0;blk<6; blk++)
                    {
                        STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                        InvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                            DCQuant, doubleQuant);
                        STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                        IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                        _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk], blkSize, 0);
                        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    }

                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                    pRecMBData->PasteMBProg(pRFrameY,m_uiRaisedPlaneStep[0],pRFrameU,pRFrameV,m_uiRaisedPlaneStep[1],j);

                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                    //smoothing
                    {
                        if((j + i * w)%5)
                            pCompressedMB->SetOverFlag(true);
                        else
                            pCompressedMB->SetOverFlag(false);

                        pCurMBInfo->SetOverlap((m_uiQuant >= 9) || (m_uiCondOverlap == VC1_ENC_COND_OVERLAP_ALL) 
                            || ((m_uiCondOverlap == VC1_ENC_COND_OVERLAP_SOME) && pCompressedMB->GetOverFlag()));
                        smoothInfo.pCurRData       = pRecMBData;
                        smoothInfo.pLeftTopRData   = m_pMBs->GetRecTopLeftMBData();
                        smoothInfo.pLeftRData      = m_pMBs->GetRecLeftMBData();
                        smoothInfo.pTopRData       = m_pMBs->GetRecTopMBData();
                        smoothInfo.pRFrameY        = pRFrameY;
                        smoothInfo.uiRDataStepY    = m_uiRaisedPlaneStep[0];
                        smoothInfo.pRFrameU        = pRFrameU;
                        smoothInfo.uiRDataStepU    = m_uiRaisedPlaneStep[1];
                        smoothInfo.pRFrameV        = pRFrameV;
                        smoothInfo.uiRDataStepV    = m_uiRaisedPlaneStep[2];
                        smoothInfo.curOverflag     = (pCurMBInfo->GetOverlap()) ? 0xFFFF : 0;
                        if(l)
                            smoothInfo.leftOverflag = (l->GetOverlap())  ? 0xFFFF : 0;
                        else
                            smoothInfo.leftOverflag = 0;
                        if(tl)
                            smoothInfo.leftTopOverflag = (tl->GetOverlap()) ? 0xFFFF : 0;
                        else
                            smoothInfo.leftTopOverflag = 0;

                        if(t)
                            smoothInfo.topOverflag = (t->GetOverlap())  ? 0xFFFF : 0;
                        else
                            smoothInfo.topOverflag = 0;

                        m_pSM_I_MB[smoothPattern](&smoothInfo, j);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                        smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;
                    }

                    //deblocking
                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                    Ipp8u *DeblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};

                    m_pDeblk_I_MB[deblkPattern](DeblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, j);

                    deblkPattern = deblkPattern | 0x1;
                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                }

                STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);
                /*------------------------------------------------------------------------------------------------*/
#ifdef VC1_ENC_DEBUG_ON
                if(i != firstRow)
                {
                    pDebug->SetDblkHorEdgeLuma(0, 0, 15, 15);
                    pDebug->SetDblkHorEdgeU(0,15);
                    pDebug->SetDblkHorEdgeV(0, 15);
                }
                else
                {
                    pDebug->SetDblkHorEdgeLuma(15, 0, 15, 15);
                    pDebug->SetDblkHorEdgeU(15, 15);
                    pDebug->SetDblkHorEdgeV(15, 15);
                }

                pDebug->SetDblkVerEdgeU(0, 15);
                pDebug->SetDblkVerEdgeV(0, 15);
                pDebug->SetDblkVerEdgeLuma(0, 0, 15, 15);
#endif
                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }

            deblkPattern = (deblkPattern  | 0x2 | ( (! (Ipp8u)((i + 1 - (hMB -1)) >> 31)<<2 )))& deblkMask;
            smoothPattern = 0x6 & smoothMask;

            //Row deblocking
            //STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            //        if (m_pSequenceHeader->IsLoopFilter()&& bRaiseFrame && (i != firstRow || firstRow == hMB-1) )
            //        {
            //STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
            //            Ipp8u *planes[3] = {m_pRaisedPlane[0] + i*m_uiRaisedPlaneStep[0]*VC1_ENC_LUMA_SIZE,
            //                                m_pRaisedPlane[1] + i*m_uiRaisedPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
            //                                m_pRaisedPlane[2] + i*m_uiRaisedPlaneStep[2]*VC1_ENC_CHROMA_SIZE};
            //
            //            if(i != hMB - 1)
            //                Deblock_I_FrameRow(planes, m_uiRaisedPlaneStep, w, m_uiQuant);
            //            else
            //                if(firstRow != hMB - 1)
            //                  Deblock_I_FrameBottomRow(planes, m_uiRaisedPlaneStep, w, m_uiQuant);
            //                 else 
            //                  Deblock_I_FrameTopBottomRow(planes, m_uiRaisedPlaneStep, w, m_uiQuant);
            //STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            //        }
            //STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;


        }

#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame && i == h)
            pDebug->PrintRestoredFrame(m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
            m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
            m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
#endif

        return err;
    }

    UMC::Status VC1EncoderPictureADV::PACIField(UMC::MeParams* pME,bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        Ipp32s                      i=0, j=0, blk = 0;
        bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);
        UMC::Status                 err = UMC::UMC_OK;

        bool                        bBottom      =  IsBottomField(m_bTopFieldFirst, bSecondField);
        Ipp32u                      pCurMBStep[3]    =  {0};
        Ipp32u                      pRaisedMBStep[3] =  {0};
        Ipp8u*                      pCurMBRow[3]     =  {0};

        Ipp8u*                      pRaisedMBRow[3]  =  {0};

        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        Ipp32u                      fieldShift = (bSecondField)? h : 0;

        //forward transform quantization

        IntraTransformQuantFunction    TransformQuantACFunctionLuma = (m_bUniformQuant) ? 
            ((pME&&pME->UseTrellisQuantization)? IntraTransformQuantTrellis: IntraTransformQuantUniform) :
            IntraTransformQuantNonUniform;
        IntraTransformQuantFunction    TransformQuantACFunctionChroma = (m_bUniformQuant) ? 
            ((pME&&pME->UseTrellisQuantizationChroma)? IntraTransformQuantTrellis: IntraTransformQuantUniform) :
            IntraTransformQuantNonUniform;

        IntraTransformQuantFunction    TransformQuantACFunction[6] = 
        {
            TransformQuantACFunctionLuma,
            TransformQuantACFunctionLuma,
            TransformQuantACFunctionLuma,
            TransformQuantACFunctionLuma,
            TransformQuantACFunctionChroma,
            TransformQuantACFunctionChroma
        };    

        //inverse transform quantization
        IntraInvTransformQuantFunction InvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :  IntraInvTransformQuantNonUniform;
        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
        bool                        dACPrediction   = true;

        IppiSize                    blkSize     = {8,8};
        IppiSize                    blkSizeLuma = {16,16};

        Ipp8u deblkPattern = (firstRow != hMB - 1)? 0 : 0x4;     //3 bits: right 1 bit - 0 - left/ 1 - not left
        //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row, 10 - top bottom row
        Ipp8u deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFE : 0;


        Ipp8u smoothMask = (m_pSequenceHeader->IsOverlap() && ((m_uiQuant >= 9) || m_uiCondOverlap != VC1_ENC_COND_OVERLAP_NO)) ? 0xFF : 0;                                                                                                  

        Ipp8u smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
        //0/1 - top/not top, 0/1 - left/not left
        SmoothInfo_I_Adv smoothInfo = {0};

        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];

        Ipp16s*                     pSavedMV        =  (m_pSavedMV)?  m_pSavedMV + w*2*(fieldShift + firstRow):0;
        Ipp8u*                      pRefType        =  (m_pRefType)?  m_pRefType + w*(fieldShift   + firstRow):0;

        VC1EncoderCodedMB*          pCodedMB        =  m_pCodedMB + w*h*bSecondField;

        SetFieldStep(pCurMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pCurMBRow, m_pPlane, m_uiPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);
        if(bRaiseFrame)
        {
            SetFieldStep(pRaisedMBStep, m_uiPlaneStep);
            err = SetFieldPlane(pRaisedMBRow, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, firstRow);
            VC1_ENC_CHECK(err);
        }


        if (pSavedMV)
        {
            memset(pSavedMV,0,w*(hMB - firstRow)*2*sizeof(Ipp16s));
        }
        if (pRefType)
        {
            memset(pRefType,0,w*(hMB - firstRow)*sizeof(Ipp8u));
        }

        err = m_pMBs->Reset();
        if (err != UMC::UMC_OK)
            return err;

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
        pDebug->SetSliceInfo(firstRow + fieldShift, firstRow + nRows + fieldShift);
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
        pDebug->SetVTSFlag(false);
#endif

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                Ipp8u               MBPattern  = 0;
                Ipp8u               CBPCY      = 0;

                VC1EncoderMBInfo*   pCurMBInfo = 0;
                VC1EncoderMBData*   pCurMBData = 0;
                VC1EncoderMBData*   pRecMBData = 0;
                Ipp32s              xLuma      = VC1_ENC_LUMA_SIZE*j;
                Ipp32s              xChroma    = VC1_ENC_CHROMA_SIZE*j;
                NeighbouringMBsData MBs;
                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);


                pCurMBInfo  =   m_pMBs->GetCurrMBInfo();
                pCurMBData  =   m_pMBs->GetCurrMBData();
                pRecMBData  =   m_pMBs->GetRecCurrMBData();


                MBs.LeftMB    = m_pMBs->GetLeftMBData();
                MBs.TopMB     = m_pMBs->GetTopMBData();
                MBs.TopLeftMB = m_pMBs->GetTopLeftMBData();
                pCompressedMB ->Init(VC1_ENC_I_MB);
                pCurMBInfo->Init(true);

                /*------------------- Compressing  ------------------------------------------------------*/

                pCurMBData->CopyMBProg(pCurMBRow[0],pCurMBStep[0],pCurMBRow[1], pCurMBRow[2], pCurMBStep[1],j);


                for (blk = 0; blk<6; blk++)
                {
                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                    _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],  blkSize, 0);
                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                    STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                    TransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                        DCQuant, doubleQuant,pME->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                    STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                }
                STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                // intra prediction
                dACPrediction = DCACPredictionFrame( pCurMBData,
                    &MBs,
                    pRecMBData,
                    0,
                    direction);
                STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                for (blk=0; blk<6; blk++)
                {
                    pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                        pRecMBData->m_uiBlockStep[blk],
                        ZagTables_I[direction[blk]* dACPrediction],
                        blk);
                }

                MBPattern = pCompressedMB->GetMBPattern();

                VC1EncoderMBInfo* t = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* l = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* tl = m_pMBs->GetTopLeftMBInfo();

                CBPCY = Get_CBPCY(MBPattern, (t)? t->GetPattern():0, (l)? l->GetPattern():0, (tl)? tl->GetPattern():0);

                pCurMBInfo->SetMBPattern(MBPattern);

                pCompressedMB->SetACPrediction(dACPrediction);
                pCompressedMB->SetMBCBPCY(CBPCY);

                //----coding---------------

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(VC1_ENC_I_MB);
                pDebug->SetCPB(MBPattern, CBPCY);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                /*--------------Reconstruction (if is needed)--------------------------------------------------*/
                STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                if (bRaiseFrame)
                {

                    for (blk=0;blk<6; blk++)
                    {
                        STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                        InvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                            DCQuant, doubleQuant);
                        STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                        IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                        _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk], blkSize, 0);
                        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    }

                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                    pRecMBData->PasteMBProg(pRaisedMBRow[0],pRaisedMBStep[0],pRaisedMBRow[1],pRaisedMBRow[2],pRaisedMBStep[1],j);

                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                    //smoothing
                    {
                        pCompressedMB->SetOverFlag(false);

                        pCurMBInfo->SetOverlap((m_uiQuant >= 9) || (m_uiCondOverlap == VC1_ENC_COND_OVERLAP_ALL) 
                            || ((m_uiCondOverlap == VC1_ENC_COND_OVERLAP_SOME) && pCompressedMB->GetOverFlag()));

                        smoothInfo.pCurRData       = pRecMBData;
                        smoothInfo.pLeftTopRData   = m_pMBs->GetRecTopLeftMBData();
                        smoothInfo.pLeftRData      = m_pMBs->GetRecLeftMBData();
                        smoothInfo.pTopRData       = m_pMBs->GetRecTopMBData();
                        smoothInfo.pRFrameY        = pRaisedMBRow[0];
                        smoothInfo.uiRDataStepY    = m_uiRaisedPlaneStep[0]*2;
                        smoothInfo.pRFrameU        = pRaisedMBRow[1];
                        smoothInfo.uiRDataStepU    = m_uiRaisedPlaneStep[1]*2;
                        smoothInfo.pRFrameV        = pRaisedMBRow[2];
                        smoothInfo.uiRDataStepV    = m_uiRaisedPlaneStep[2]*2;
                        smoothInfo.curOverflag     = (pCurMBInfo->GetOverlap()) ? 0xFFFF : 0;
                        if(l)
                            smoothInfo.leftOverflag = (l->GetOverlap())  ? 0xFFFF : 0;
                        else
                            smoothInfo.leftOverflag = 0;
                        if(tl)
                            smoothInfo.leftTopOverflag = (tl->GetOverlap()) ? 0xFFFF : 0;
                        else
                            smoothInfo.leftTopOverflag = 0;

                        if(t)
                            smoothInfo.topOverflag = (t->GetOverlap())  ? 0xFFFF : 0;
                        else
                            smoothInfo.topOverflag = 0;

                            m_pSM_I_MB[smoothPattern](&smoothInfo, j);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                        smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;
                    }

                    //deblocking
                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                    Ipp8u *DeblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};

                        m_pDeblk_I_MB[deblkPattern](DeblkPlanes, pRaisedMBStep, m_uiQuant, j);

                    deblkPattern = deblkPattern | 0x1;
                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                }

                STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);
                /*------------------------------------------------------------------------------------------------*/
#ifdef VC1_ENC_DEBUG_ON
                if(i != firstRow)
                {
                    pDebug->SetDblkHorEdgeLuma(0, 0, 15, 15);
                    pDebug->SetDblkHorEdgeU(0,15);
                    pDebug->SetDblkHorEdgeV(0, 15);
                }
                else
                {
                    pDebug->SetDblkHorEdgeLuma(15, 0, 15, 15);
                    pDebug->SetDblkHorEdgeU(15,15);
                    pDebug->SetDblkHorEdgeV(15, 15);
                }

                pDebug->SetDblkVerEdgeLuma(0, 0, 15, 15);
                pDebug->SetDblkVerEdgeU(0, 15);
                pDebug->SetDblkVerEdgeV(0, 15);
#endif
                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }

            deblkPattern = (deblkPattern  | 0x2 | ( (! (Ipp8u)((i + 1 - (hMB -1)) >> 31)<<2 )))& deblkMask;
            smoothPattern = 0x6 & smoothMask;

            //Row deblocking
            //STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            //        if (m_pSequenceHeader->IsLoopFilter()&& bRaiseFrame && (i != firstRow || firstRow == hMB-1) )
            //        {
            //STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
            //           Ipp8u *planes[3] = {0};
            //           err = SetFieldPlane(planes, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, i);
            //           VC1_ENC_CHECK(err);
            //
            //            Ipp32u step[3] = {0};
            //            SetFieldStep(step, m_uiPlaneStep);
            //            VC1_ENC_CHECK(err);
            //
            //            if(i != hMB - 1)
            //                Deblock_I_FrameRow(planes, step, w, m_uiQuant);
            //            else
            //                if(firstRow != hMB - 1)
            //                  Deblock_I_FrameBottomRow(planes, step, w, m_uiQuant);
            //                else 
            //                 Deblock_I_FrameTopBottomRow(planes, step, w, m_uiQuant);
            //STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            //        }
            ////STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);


            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;

            pCurMBRow[0]+= pCurMBStep[0]*VC1_ENC_LUMA_SIZE;
            pCurMBRow[1]+= pCurMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pCurMBRow[2]+= pCurMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pRaisedMBRow[0]+= pRaisedMBStep[0]*VC1_ENC_LUMA_SIZE;
            pRaisedMBRow[1]+= pRaisedMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pRaisedMBRow[2]+= pRaisedMBStep[2]*VC1_ENC_CHROMA_SIZE;

        }

#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame && bSecondField && i == h)
            pDebug->PrintRestoredFrame( m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
            m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
            m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
#endif
        return err;
    }

    UMC::Status VC1EncoderPictureADV::PAC_PField2Ref(UMC::MeParams* MEParams,bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;
        bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);

        bool                        bBottom      =  IsBottomField(m_bTopFieldFirst, bSecondField);

        Ipp8u*                      pCurMBRow[3]    = {0};
        Ipp32u                      pCurMBStep[3]   = {0};    
        Ipp8u*                      pRaisedMBRow[3] = {0};
        Ipp32u                      pRaisedMBStep[3]= {0} ;

        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        Ipp8u*                      pForwMBRow  [2][3]  = {0};
        Ipp32u                      pForwMBStep [2][3]  = {0};

        VC1EncoderCodedMB*          pCodedMB = m_pCodedMB + w*h*bSecondField;

        //forward transform quantization

        _SetIntraInterLumaFunctions;

        //inverse transform quantization
        IntraInvTransformQuantFunction IntraInvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :
            IntraInvTransformQuantNonUniform;

        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (m_bUniformQuant) ? InterInvTransformQuantUniform :
            InterInvTransformQuantNonUniform;
        CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())? GetChromaMVFast:GetChromaMV;

        bool                        bIsBilinearInterpolation = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR);
        InterpolateFunction         InterpolateLumaFunction  = (bIsBilinearInterpolation)? _own_ippiInterpolateQPBilinear_VC1_8u_C1R: _own_ippiInterpolateQPBicubic_VC1_8u_C1R;

        // interpolation + MB Level padding

        InterpolateFunctionPadding  InterpolateLumaFunctionPadding = (bIsBilinearInterpolation)? own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R:    own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;

        IppVCInterpolateBlock_8u    InterpolateBlockY[2];
        IppVCInterpolateBlock_8u    InterpolateBlockU[2];
        IppVCInterpolateBlock_8u    InterpolateBlockV[2];



        Ipp8u                       tempInterpolationBuffer[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];

        IppVCInterpolate_8u         sYInterpolation;
        IppVCInterpolate_8u         sUInterpolation;
        IppVCInterpolate_8u         sVInterpolation;

        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
        bool                        dACPrediction   = true;

        IppiSize                    blkSize     = {8,8};
        IppiSize                    blkSizeLuma = {16,16};

        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];

        Ipp16s*                     pSavedMV        =  (m_pSavedMV)?  m_pSavedMV + w*2*(h*bSecondField + firstRow):0;
        Ipp8u*                      pRefType        =  (m_pRefType)?  m_pRefType + w*(h*bSecondField   + firstRow):0;

        bool                        bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR ||
            m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

        bool                        bSubBlkTS           = m_pSequenceHeader->IsVSTransform() && (!(m_bVSTransform &&m_uiTransformType==VC1_ENC_8x8_TRANSFORM));
        fGetExternalEdge            pGetExternalEdge    = GetFieldExternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS
        fGetInternalEdge            pGetInternalEdge    = GetInternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS

        Ipp8u deblkPattern = (firstRow != hMB - 1)? 0 : 0x8;   //4 bits: right 1 bit - 0 - left/1 - not left,
        //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row, 10 - top bottom row
        //middle 1 bit - 1 - right/0 - not right

        Ipp8u                       deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFC : 0;

        Ipp8u smoothMask = (m_pSequenceHeader->IsOverlap() && (m_uiQuant >= 9)) ? 0xFF : 0;
        Ipp8u smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
        //0/1 - top/not top, 0/1 - left/not left

        SmoothInfo_P_Adv smoothInfo = {0};


        IppStatus                   ippSts  = ippStsNoErr;
        sScaleInfo                  scInfo;
        Ipp32u                      th = /*(bMVHalf)?16:*/32;

        Ipp32u                      padding[2][2] = {{m_uiForwardPlanePadding, m_uiForwardPlanePadding}, // first field
        {m_uiRaisedPlanePadding,  m_uiForwardPlanePadding}};// second field
        Ipp32u                      oppositePadding[2][2] = {{(m_bForwardInterlace)? 0:1,(m_bForwardInterlace)? 0:1 }, 
        {0,(m_bForwardInterlace)? 0:1}};
#ifdef VC1_ENC_DEBUG_ON
        Ipp32s                      fieldShift = (bSecondField)? h : 0;
        pDebug->SetRefNum(2);
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
        pDebug->SetSliceInfo(firstRow  + fieldShift, firstRow + nRows + fieldShift);
        pDebug->SetInterpolType(bIsBilinearInterpolation);
        pDebug->SetRounControl(m_uiRoundControl);
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
        pDebug->SetVTSFlag(bSubBlkTS);
#endif

        SetFieldStep(pCurMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pCurMBRow, m_pPlane, m_uiPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(pRaisedMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pRaisedMBRow, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);


        assert (m_uiReferenceFieldType ==VC1_ENC_REF_FIELD_BOTH );

        InitScaleInfo(&scInfo,bSecondField,bBottom,m_nReferenceFrameDist,m_uiMVRangeIndex);

        if (pSavedMV && pRefType)
        {
            memset(pSavedMV,0,w*(hMB - firstRow)*2*sizeof(Ipp16s));
            memset(pRefType,0,w*(hMB - firstRow)*sizeof(Ipp8u));
        }


        err = m_pMBs->Reset();
        if (err != UMC::UMC_OK)
            return err;

        SetInterpolationParams(&sYInterpolation,&sUInterpolation,&sVInterpolation,
            tempInterpolationBuffer,true, true);


        err = Set2RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep, bSecondField, bBottom, 0);
        VC1_ENC_CHECK(err);

        SetInterpolationParams(&InterpolateBlockY[0],&InterpolateBlockU[0],&InterpolateBlockV[0], tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[0], pForwMBStep[0], true);

        SetInterpolationParams(&InterpolateBlockY[1],&InterpolateBlockU[1],&InterpolateBlockV[1], tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[1], pForwMBStep[1], true);


        err = Set2RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep, bSecondField, bBottom, firstRow);
        VC1_ENC_CHECK(err);

#ifdef VC1_ENC_CHECK_MV
        err = CheckMEMV_PField(MEParams,bMVHalf,firstRow,nRows);
        assert(err == UMC::UMC_OK);
        VC1_ENC_CHECK(err);
#endif

        /* -------------------------------------------------------------------------*/
        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {

                Ipp32s              xLuma        =  VC1_ENC_LUMA_SIZE*j;
                Ipp32s              xChroma      =  VC1_ENC_CHROMA_SIZE*j;

                sCoordinate         MVInt       = {0,0};
                sCoordinate         MVQuarter   = {0,0};
                sCoordinate         MV          = {0,0};
                Ipp8u               MBPattern   = 0;
                Ipp8u               CBPCY       = 0;

                VC1EncoderMBInfo  * pCurMBInfo  = m_pMBs->GetCurrMBInfo();
                VC1EncoderMBData  * pCurMBData  = m_pMBs->GetCurrMBData();
                VC1EncoderMBData*   pRecMBData =  m_pMBs->GetRecCurrMBData();

                VC1EncoderMBInfo* left        = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* topLeft     = m_pMBs->GetTopLeftMBInfo();
                VC1EncoderMBInfo* top         = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* topRight    = m_pMBs->GetTopRightMBInfo();

                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);
                eMBType MBType;

                switch (MEParams->pSrc->MBs[j + i*w].MbType)
                {
                case UMC::ME_MbIntra:
                    {
                        NeighbouringMBsData MBs;

                        MBs.LeftMB    = ((left)? left->isIntra():0)         ? m_pMBs->GetLeftMBData():0;
                        MBs.TopMB     = ((top)? top->isIntra():0)           ? m_pMBs->GetTopMBData():0;
                        MBs.TopLeftMB = ((topLeft)? topLeft->isIntra():0)   ? m_pMBs->GetTopLeftMBData():0;
                        MV.x = 0;
                        MV.y =0;
                        MBType = VC1_ENC_P_MB_INTRA;
                        pCompressedMB->Init(VC1_ENC_P_MB_INTRA);
                        pCurMBInfo->Init(true);

                        /*-------------------------- Compression ----------------------------------------------------------*/

                        pCurMBData->CopyMBProg(pCurMBRow[0],pCurMBStep[0],pCurMBRow[1], pCurMBRow[2], pCurMBStep[1],j);

                        //only intra blocks:
                        for (blk = 0; blk<6; blk++)
                        {

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                            _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                        }

                        STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                        dACPrediction = DCACPredictionFrame( pCurMBData,&MBs,
                            pRecMBData, 0,direction);
                        STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                        for (blk=0; blk<6; blk++)
                        {
                            pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                pRecMBData->m_uiBlockStep[blk],
                                VC1_Inter_InterlaceIntra_8x8_Scan_Adv,
                                blk);
                        }

                        MBPattern = pCompressedMB->GetMBPattern();
                        CBPCY = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetACPrediction(dACPrediction);
                        pCompressedMB->SetMBCBPCY(CBPCY);
                        pCurMBInfo->SetEdgesIntra(i==0, j==0);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(VC1_ENC_P_MB_INTRA);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                        /*-------------------------- Reconstruction ----------------------------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            for (blk=0;blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                    DCQuant, doubleQuant);
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],blkSize, 0);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pRecMBData->PasteMBProg(pRaisedMBRow[0],pRaisedMBStep[0],pRaisedMBRow[1],pRaisedMBRow[2],pRaisedMBStep[1],j);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRaisedMBRow[0];
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0]*2;
                                smoothInfo.pRFrameU       = pRaisedMBRow[1];
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1]*2;
                                smoothInfo.pRFrameV       = pRaisedMBRow[2];
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2]*2;
                                smoothInfo.curIntra       = 0x3F;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            {
                                STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};

                                m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                        /*------------------------------------------- ----------------------------------------------------------*/
                    }
                    break;
                case UMC::ME_MbFrw:
                case UMC::ME_MbFrwSkipped:
                    {
                        /*--------------------------------------- Inter --------------------------------------------------*/
                        sCoordinate MVIntChroma     = {0,0};
                        sCoordinate MVQuarterChroma = {0,0};
                        sCoordinate MVChroma       =  {0,0};

                        sCoordinate *mvC=0, *mvB=0, *mvA=0;
                        sCoordinate  mvCEx = {0};
                        sCoordinate  mvAEx = {0};
                        sCoordinate  mv1, mv2, mv3;
                        sCoordinate  mvPred[2] = {0};
                        bool         bSecondDominantPred = false;
                        sCoordinate  mvDiff;
                        eTransformType              BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                                                                        m_uiTransformType, m_uiTransformType,
                                                                        m_uiTransformType, m_uiTransformType};                        
                        Ipp8u        hybridReal = 0; 


                        MBType = (UMC::ME_MbFrw == MEParams->pSrc->MBs[j + i*w].MbType)?
VC1_ENC_P_MB_1MV:VC1_ENC_P_MB_SKIP_1MV;

                        MV.x        = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                        MV.y        = MEParams->pSrc->MBs[j + i*w].MV[0]->y;
                        MV.bSecond  = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME


                        pCurMBInfo->Init(false);
                        pCompressedMB->Init(MBType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        /*MV prediction -!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
                        GetMVPredictionPField(true);
                        bSecondDominantPred = PredictMVField2(mvA,mvB,mvC, mvPred, &scInfo,bSecondField,&mvAEx,&mvCEx, false, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPred, 0);
#endif
                        //ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMBMin,MVPredMBMax);
                        if (mvA && mvC)
                        {
                            sCoordinate*  mvCPred = (mvC->bSecond == MV.bSecond)? mvC:&mvCEx;
                            sCoordinate*  mvAPred = (mvA->bSecond == MV.bSecond)? mvA:&mvAEx;
                            hybridReal = HybridPrediction(&(mvPred[MV.bSecond]),&MV,mvAPred,mvCPred,th);

                        }
                        pCompressedMB->SetHybrid(hybridReal);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetPredFlag(bSecondDominantPred!=MV.bSecond, 0);
                        pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == MV.bSecond || mvA==0)? mvA:&mvAEx,
                            /*mvB,*/ (mvC && mvC->bSecond == MV.bSecond || mvC==0)? mvC:&mvCEx, 0);
                        pDebug->SetHybrid(hybridReal, 0);
#endif

                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        if (VC1_ENC_P_MB_SKIP_1MV == MBType)
                        {
                            //correct ME results
                            VM_ASSERT(MV.x==mvPred[MV.bSecond].x && MV.y==mvPred[MV.bSecond].y);
                            MV.x=mvPred[MV.bSecond].x;
                            MV.y=mvPred[MV.bSecond].y;
                        }
                        pCurMBInfo->SetMV_F(MV);

                        mvDiff.x = MV.x - mvPred[MV.bSecond].x;
                        mvDiff.y = MV.y - mvPred[MV.bSecond].y;
                        mvDiff.bSecond = (MV.bSecond != bSecondDominantPred);
                        pCompressedMB->SetdMV_F(mvDiff);

                        if (VC1_ENC_P_MB_SKIP_1MV != MBType || bRaiseFrame)
                        {
                            sCoordinate t = {MV.x,MV.y + ((!MV.bSecond )? (2-4*(!bBottom)):0),MV .bSecond};
                            GetIntQuarterMV(t,&MVInt, &MVQuarter);
                            /*interpolation*/
                            CalculateChroma(MV,&MVChroma);
                            pCurMBInfo->SetMVChroma(MVChroma);
                            MVChroma.y = MVChroma.y + ((!MV.bSecond)? (2-4*(!bBottom)):0);
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(t.x, t.y, 0);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 4);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 5);
#endif
                            Ipp32u ind = (bSecondField)? 1: 0;
                            if (padding[ind][MV.bSecond])
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolation, pForwMBRow[MV.bSecond][0], pForwMBStep[MV.bSecond][0], &MVInt, &MVQuarter, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolation);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolation, &sVInterpolation, pForwMBRow[MV.bSecond][1], pForwMBRow[MV.bSecond][2],pForwMBStep[MV.bSecond][1], 
                                                   &MVIntChroma, &MVQuarterChroma, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            }
                            else
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockY[MV.bSecond], j, i , &MVInt, &MVQuarter);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockY[MV.bSecond],0,0,oppositePadding[ind][MV.bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockU[MV.bSecond], &InterpolateBlockV[MV.bSecond], 
                                                        oppositePadding[ind][MV.bSecond],true,m_uiRoundControl,bBottom,
                                                         j, i ,  &MVIntChroma, &MVQuarterChroma);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);

                            }

                        } //interpolation

                        if (VC1_ENC_P_MB_SKIP_1MV != MBType)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = sYInterpolation.pDst; 
                                pIn.refYStep = sYInterpolation.dstStep;      
                                pIn.pURef    = sUInterpolation.pDst; 
                                pIn.refUStep = sUInterpolation.dstStep;
                                pIn.pVRef    = sVInterpolation.pDst; 
                                pIn.refVStep = sVInterpolation.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif               
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],pCurMBStep[0], pCurMBRow[1],pCurMBRow[2], pCurMBStep[1], 
                                sYInterpolation.pDst, sYInterpolation.dstStep,
                                sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                                j,0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant, MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern = pCompressedMB->GetMBPattern();
                            CBPCY = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                            if (MBPattern==0 && mvDiff.x == 0 && mvDiff.y == 0)
                            {
                                pCompressedMB->ChangeType(VC1_ENC_P_MB_SKIP_1MV);
                                MBType = VC1_ENC_P_MB_SKIP_1MV;
                            }
                        }//VC1_ENC_P_MB_SKIP_1MV != MBType
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(MBType);
                        pDebug->SetMVInfoField(&MV,  mvPred[MV.bSecond].x,mvPred[MV.bSecond].y, 0);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 4);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);

                        if (pCompressedMB->isSkip() )
                        {
                            pDebug->SetMBAsSkip();
                        }
                        else
                        {
                            pDebug->SetCPB(MBPattern, CBPCY);
                            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        }
#endif
                        /*---------------------------Reconstruction ------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                            sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                            pRaisedMBRow[0], pRaisedMBStep[0], 
                                                            pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                                            0,j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                                sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                                pRaisedMBRow[0], pRaisedMBStep[0], 
                                                                pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],0,j);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRaisedMBRow[0];
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0]*2;
                                smoothInfo.pRFrameU       = pRaisedMBRow[1];
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1]*2;
                                smoothInfo.pRFrameV       = pRaisedMBRow[2];
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2]*2;
                                smoothInfo.curIntra       = 0;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                                pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                                pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                                pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                        Ipp32u ind = (bSecondField)? 1: 0;
                        pDebug->SetInterpInfo(&sYInterpolation, &sUInterpolation, &sVInterpolation, 0, padding[ind][MV.bSecond]);
#endif
                    }
                    break;
                default:
                    VM_ASSERT(0);
                    return UMC::UMC_ERR_FAILED;
                }

                if (pSavedMV && pRefType)
                {
                    *(pSavedMV ++) = MV.x;
                    *(pSavedMV ++) = MV.y;
                    *(pRefType ++) = (MBType == VC1_ENC_P_MB_INTRA)? 0 : MV.bSecond + 1;
                }


#ifdef VC1_ENC_DEBUG_ON
                if(!bSubBlkTS)
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(), 15, 15);
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(), 15, 15);
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(),15);
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), 15);
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), 15);
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), 15);
                }
                else
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(),
                        pCurMBInfo->GetLumaAdUppEdge(), pCurMBInfo->GetLumaAdBotEdge() );
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(),
                        pCurMBInfo->GetLumaAdLefEdge(), pCurMBInfo->GetLumaAdRigEdge());
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(), pCurMBInfo->GetUAdHorEdge());
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), pCurMBInfo->GetVAdHorEdge());
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), pCurMBInfo->GetUAdVerEdge());
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), pCurMBInfo->GetVAdVerEdge());
                }
#endif

                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }

            deblkPattern = (deblkPattern | 0x4 | ( (! (Ipp8u)((i + 1 - (hMB -1)) >> 31)<<3))) & deblkMask;
            smoothPattern = 0x6 & smoothMask;

            //Row deblocking
            //STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            //        if (m_pSequenceHeader->IsLoopFilter() && bRaiseFrame && (i != firstRow || firstRow == hMB-1))
            //        {
            //STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
            //           Ipp8u *planes[3] = {0};
            //           err = SetFieldPlane(planes, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, i);
            //           VC1_ENC_CHECK(err);
            //
            //            Ipp32u step[3] = {0};
            //            SetFieldStep(step, m_uiPlaneStep);
            //            VC1_ENC_CHECK(err);
            //
            //            m_pMBs->StartRow();
            //            if(bSubBlkTS)
            //            {
            //                if(i != hMB-1)
            //                    Deblock_P_RowVts(planes, step, w, m_uiQuant, m_pMBs);
            //                else
            //                     if(firstRow != hMB-1)
            //                       Deblock_P_BottomRowVts(planes, step, w, m_uiQuant, m_pMBs);
            //                     else
            //                       Deblock_P_TopBottomRowVts(planes, step, w, m_uiQuant, m_pMBs);
            //            }
            //            else
            //            {
            //                if(i != hMB - 1)
            //                    Deblock_P_RowNoVts(planes, step, w, m_uiQuant, m_pMBs);
            //                else
            //                    if(firstRow != hMB -1)
            //                      Deblock_P_BottomRowNoVts(planes, step, w, m_uiQuant, m_pMBs);
            //                    else
            //                      Deblock_P_TopBottomRowNoVts(planes, step, w, m_uiQuant, m_pMBs);
            //            }
            //STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            //        }
            //STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);
            //        
            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;

            pCurMBRow[0]+= pCurMBStep[0]*VC1_ENC_LUMA_SIZE;
            pCurMBRow[1]+= pCurMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pCurMBRow[2]+= pCurMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pRaisedMBRow[0]+= pRaisedMBStep[0]*VC1_ENC_LUMA_SIZE;
            pRaisedMBRow[1]+= pRaisedMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pRaisedMBRow[2]+= pRaisedMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pForwMBRow[0][0]+= pForwMBStep[0][0]*VC1_ENC_LUMA_SIZE;
            pForwMBRow[0][1]+= pForwMBStep[0][1]*VC1_ENC_CHROMA_SIZE;
            pForwMBRow[0][2]+= pForwMBStep[0][2]*VC1_ENC_CHROMA_SIZE;

            pForwMBRow[1][0]+= pForwMBStep[1][0]*VC1_ENC_LUMA_SIZE;
            pForwMBRow[1][1]+= pForwMBStep[1][1]*VC1_ENC_CHROMA_SIZE;
            pForwMBRow[1][2]+= pForwMBStep[1][2]*VC1_ENC_CHROMA_SIZE;

        }

#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame && bSecondField && i == h)
            pDebug->PrintRestoredFrame( m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
            m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
            m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
#endif
        return err;
    }

    UMC::Status VC1EncoderPictureADV::PAC_PField2RefMixed(UMC::MeParams* MEParams,bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;
        bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);

        bool                        bBottom      =  IsBottomField(m_bTopFieldFirst, bSecondField);

        Ipp8u*                      pCurMBRow[3]    = {0};
        Ipp32u                      pCurMBStep[3]   = {0};    
        Ipp8u*                      pRaisedMBRow[3] = {0};
        Ipp32u                      pRaisedMBStep[3]= {0} ;

        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        Ipp8u*                      pForwMBRow  [2][3]  = {0};
        Ipp32u                      pForwMBStep [2][3]  = {0};

        VC1EncoderCodedMB*          pCodedMB = m_pCodedMB + w*h*bSecondField;

        //forward transform quantization

        _SetIntraInterLumaFunctions;

        //inverse transform quantization
        IntraInvTransformQuantFunction IntraInvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :  IntraInvTransformQuantNonUniform;

        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (m_bUniformQuant) ? InterInvTransformQuantUniform :  InterInvTransformQuantNonUniform;
        CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())? GetChromaMVFast:GetChromaMV;

        InterpolateFunction         InterpolateLumaFunction  = _own_ippiInterpolateQPBicubic_VC1_8u_C1R;         


        // interpolation + MB Level padding

        InterpolateFunctionPadding  InterpolateLumaFunctionPadding   = own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;


        Ipp8u                       tempInterpolationBuffer[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];

        IppVCInterpolateBlock_8u    InterpolateBlockY[2];
        IppVCInterpolateBlock_8u    InterpolateBlockU[2];
        IppVCInterpolateBlock_8u    InterpolateBlockV[2];
        IppVCInterpolateBlock_8u    InterpolateBlockYBlk[2][4];



        IppVCInterpolate_8u         sYInterpolation;
        IppVCInterpolate_8u         sUInterpolation;
        IppVCInterpolate_8u         sVInterpolation;
        IppVCInterpolate_8u         sYInterpolationBlk[4];


        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
        bool                        dACPrediction   = true;

        IppiSize                    blkSize     = {8,8};
        IppiSize                    blkSizeLuma = {16,16};

        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];

        Ipp16s*                     pSavedMV        =  (m_pSavedMV)?  m_pSavedMV + w*2*(h*bSecondField + firstRow):0;
        Ipp8u*                      pRefType        =  (m_pRefType)?  m_pRefType + w*(h*bSecondField   + firstRow):0;

        bool                        bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR ||
            m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

        bool                        bSubBlkTS           = m_pSequenceHeader->IsVSTransform() && (!(m_bVSTransform &&m_uiTransformType==VC1_ENC_8x8_TRANSFORM));
        fGetExternalEdge            pGetExternalEdge    = GetFieldExternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS
        fGetInternalEdge            pGetInternalEdge    = GetFieldInternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS

        Ipp8u deblkPattern = (firstRow != hMB - 1)? 0 : 0x8;   //4 bits: right 1 bit - 0 - left/1 - not left,
        //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row, 10 - top bottom row
        //middle 1 bit - 1 - right/0 - not right

        Ipp8u                       deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFC : 0;

        Ipp8u smoothMask = (m_pSequenceHeader->IsOverlap() && (m_uiQuant >= 9)) ? 0xFF : 0;
        Ipp8u smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
        //0/1 - top/not top, 0/1 - left/not left

        SmoothInfo_P_Adv smoothInfo = {0};

        IppStatus                   ippSts  = ippStsNoErr;
        sScaleInfo                  scInfo;
        Ipp32u                      th = /*(bMVHalf)?16:*/32;

        Ipp32u                      padding[2][2] = {{m_uiForwardPlanePadding, m_uiForwardPlanePadding}, // first field
        {m_uiRaisedPlanePadding,  m_uiForwardPlanePadding}};// second field
        Ipp32u                      oppositePadding[2][2] = {{(m_bForwardInterlace)? 0:1,(m_bForwardInterlace)? 0:1 }, 
        {0,(m_bForwardInterlace)? 0:1}};
#ifdef VC1_ENC_DEBUG_ON
        Ipp32u                      fieldShift = (bSecondField)? h : 0;
        bool                        bIsBilinearInterpolation = false;
        pDebug->SetRefNum(2);
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
        pDebug->SetSliceInfo(firstRow+fieldShift, firstRow + nRows+fieldShift);
        pDebug->SetInterpolType(bIsBilinearInterpolation);
        pDebug->SetRounControl(m_uiRoundControl);
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
        pDebug->SetVTSFlag(bSubBlkTS);
#endif

        SetFieldStep(pCurMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pCurMBRow, m_pPlane, m_uiPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(pRaisedMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pRaisedMBRow, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);

        assert (m_uiReferenceFieldType ==VC1_ENC_REF_FIELD_BOTH );

        InitScaleInfo(&scInfo,bSecondField,bBottom,m_nReferenceFrameDist,m_uiMVRangeIndex);

        if (pSavedMV && pRefType)
        {
            memset(pSavedMV,0,w*(hMB - firstRow)*2*sizeof(Ipp16s));
            memset(pRefType,0,w*(hMB - firstRow)*sizeof(Ipp8u));
        }


        err = m_pMBs->Reset();
        if (err != UMC::UMC_OK)
            return err;

        SetInterpolationParams(&sYInterpolation,&sUInterpolation,&sVInterpolation,
            tempInterpolationBuffer,true, true);

        SetInterpolationParams4MV(sYInterpolationBlk,&sUInterpolation,&sVInterpolation,
            tempInterpolationBuffer,true, true);


        err = Set2RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep, bSecondField, bBottom, 0);
        VC1_ENC_CHECK(err);

        SetInterpolationParams(&InterpolateBlockY[0],&InterpolateBlockU[0],&InterpolateBlockV[0], tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[0], pForwMBStep[0], true);

        SetInterpolationParams(&InterpolateBlockY[1],&InterpolateBlockU[1],&InterpolateBlockV[1], tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[1], pForwMBStep[1], true);

        SetInterpolationParams4MV (InterpolateBlockYBlk[0], tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[0], pForwMBStep[0], true);
        SetInterpolationParams4MV (InterpolateBlockYBlk[1], tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[1], pForwMBStep[1], true);

        err = Set2RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep, bSecondField, bBottom, firstRow);
        VC1_ENC_CHECK(err);


#ifdef VC1_ENC_CHECK_MV
        err = CheckMEMV_P_MIXED(MEParams,bMVHalf,firstRow, nRows, true);
        //err = CheckMEMV_PField(MEParams,bMVHalf,firstRow,nRows);
        assert(err == UMC::UMC_OK);
        VC1_ENC_CHECK(err);
#endif

        /* -------------------------------------------------------------------------*/
        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {

                Ipp32s              xLuma        =  VC1_ENC_LUMA_SIZE*j;
                Ipp32s              xChroma      =  VC1_ENC_CHROMA_SIZE*j;

                sCoordinate         MV          = {0,0};
                Ipp8u               MBPattern   = 0;
                Ipp8u               CBPCY       = 0;

                VC1EncoderMBInfo  * pCurMBInfo  = m_pMBs->GetCurrMBInfo();
                VC1EncoderMBData  * pCurMBData  = m_pMBs->GetCurrMBData();
                VC1EncoderMBData  * pRecMBData  =   m_pMBs->GetRecCurrMBData();

                VC1EncoderMBInfo* left        = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* topLeft     = m_pMBs->GetTopLeftMBInfo();
                VC1EncoderMBInfo* top         = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* topRight    = m_pMBs->GetTopRightMBInfo();

                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);
                eMBType MBType;


                switch (MEParams->pSrc->MBs[j + i*w].MbType)
                {
                case UMC::ME_MbIntra:
                    MBType = VC1_ENC_P_MB_INTRA;
                    break;
                case UMC::ME_MbFrw:
                    MBType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_P_MB_4MV:VC1_ENC_P_MB_1MV;
                    break;
                case UMC::ME_MbFrwSkipped:
                    MBType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_P_MB_SKIP_4MV:VC1_ENC_P_MB_SKIP_1MV;
                    break;
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
                }

                switch (MBType)
                {
                case VC1_ENC_P_MB_INTRA:
                    {
                        NeighbouringMBsData MBs;

                        MBs.LeftMB    = ((left)? left->isIntra():0)         ? m_pMBs->GetLeftMBData():0;
                        MBs.TopMB     = ((top)? top->isIntra():0)           ? m_pMBs->GetTopMBData():0;
                        MBs.TopLeftMB = ((topLeft)? topLeft->isIntra():0)   ? m_pMBs->GetTopLeftMBData():0;
                        MV.x = 0;
                        MV.y =0;
                        MBType = VC1_ENC_P_MB_INTRA;
                        pCompressedMB->Init(VC1_ENC_P_MB_INTRA);
                        pCurMBInfo->Init(true);

                        /*-------------------------- Compression ----------------------------------------------------------*/
                        
                        pCurMBData->CopyMBProg(pCurMBRow[0],pCurMBStep[0],pCurMBRow[1], pCurMBRow[2], pCurMBStep[1],j);

                        //only intra blocks:
                        for (blk = 0; blk<6; blk++)
                        {

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                            _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                        }

                        STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                        dACPrediction = DCACPredictionFrame( pCurMBData,&MBs,
                            pRecMBData, 0,direction);
                        STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                        for (blk=0; blk<6; blk++)
                        {
                            pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                pRecMBData->m_uiBlockStep[blk],
                                VC1_Inter_InterlaceIntra_8x8_Scan_Adv,
                                blk);
                        }

                        MBPattern = pCompressedMB->GetMBPattern();
                        CBPCY = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetACPrediction(dACPrediction);
                        pCompressedMB->SetMBCBPCY(CBPCY);
                        pCurMBInfo->SetEdgesIntra(i==0, j==0);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(VC1_ENC_P_MB_INTRA);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                        /*-------------------------- Reconstruction ----------------------------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            for (blk=0;blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                    DCQuant, doubleQuant);
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],blkSize, 0);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pRecMBData->PasteMBProg(pRaisedMBRow[0],pRaisedMBStep[0],pRaisedMBRow[1],pRaisedMBRow[2],pRaisedMBStep[1],j);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRaisedMBRow[0];
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0]*2;
                                smoothInfo.pRFrameU       = pRaisedMBRow[1];
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1]*2;
                                smoothInfo.pRFrameV       = pRaisedMBRow[2];
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2]*2;
                                smoothInfo.curIntra       = 0x3F;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            {
                                STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};

                                m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                        /*------------------------------------------- ----------------------------------------------------------*/
                    }
                    break;
                case VC1_ENC_P_MB_1MV:
                case VC1_ENC_P_MB_SKIP_1MV:
                    {
                        /*--------------------------------------- Inter --------------------------------------------------*/
                        sCoordinate         MVInt       = {0,0};
                        sCoordinate         MVQuarter   = {0,0};

                        sCoordinate MVIntChroma     = {0,0};
                        sCoordinate MVQuarterChroma = {0,0};
                        sCoordinate MVChroma       =  {0,0};

                        sCoordinate *mvC=0, *mvB=0, *mvA=0;
                        sCoordinate  mvCEx = {0};
                        sCoordinate  mvAEx = {0};
                        sCoordinate  mv1, mv2, mv3;
                        sCoordinate  mvPred[2] = {0};
                        bool         bSecondDominantPred = false;
                        sCoordinate  mvDiff;
                        eTransformType              BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                                                    m_uiTransformType, m_uiTransformType,
                                                    m_uiTransformType, m_uiTransformType};

                        Ipp8u        hybridReal = 0; 


                        MV.x        = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                        MV.y        = MEParams->pSrc->MBs[j + i*w].MV[0]->y;
                        MV.bSecond  = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME


                        pCurMBInfo->Init(false);
                        pCompressedMB->Init(MBType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        /*MV prediction -!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
                        GetMVPredictionPField(true);
                        bSecondDominantPred = PredictMVField2(mvA,mvB,mvC, mvPred, &scInfo,bSecondField,&mvAEx,&mvCEx, false, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPred, 0);
#endif
                        //ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMBMin,MVPredMBMax);
                        if (mvA && mvC)
                        {
                            sCoordinate*  mvCPred = (mvC->bSecond == MV.bSecond)? mvC:&mvCEx;
                            sCoordinate*  mvAPred = (mvA->bSecond == MV.bSecond)? mvA:&mvAEx;
                            hybridReal = HybridPrediction(&(mvPred[MV.bSecond]),&MV,mvAPred,mvCPred,th);
                        }
                        pCompressedMB->SetHybrid(hybridReal);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetPredFlag(bSecondDominantPred!=MV.bSecond, 0);
                        pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == MV.bSecond || mvA==0)? mvA:&mvAEx,
                            /*mvB,*/ (mvC && mvC->bSecond == MV.bSecond || mvC==0)? mvC:&mvCEx, 0);
                        pDebug->SetHybrid(hybridReal, 0);
#endif

                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        if (VC1_ENC_P_MB_SKIP_1MV == MBType)
                        {
                            //correct ME results
                            VM_ASSERT(MV.x==mvPred[MV.bSecond].x && MV.y==mvPred[MV.bSecond].y);
                            MV.x=mvPred[MV.bSecond].x;
                            MV.y=mvPred[MV.bSecond].y;
                        }
                        pCurMBInfo->SetMV_F(MV);

                        mvDiff.x = MV.x - mvPred[MV.bSecond].x;
                        mvDiff.y = MV.y - mvPred[MV.bSecond].y;
                        mvDiff.bSecond = (MV.bSecond != bSecondDominantPred);
                        pCompressedMB->SetdMV_F(mvDiff);

                        if (VC1_ENC_P_MB_SKIP_1MV != MBType || bRaiseFrame)
                        {
                            sCoordinate t = {MV.x,MV.y + ((!MV.bSecond )? (2-4*(!bBottom)):0),MV .bSecond};
                            GetIntQuarterMV(t,&MVInt, &MVQuarter);
                            /*interpolation*/
                            CalculateChroma(MV,&MVChroma);
                            pCurMBInfo->SetMVChroma_F(MVChroma);
                            MVChroma.y = MVChroma.y + ((!MV.bSecond)? (2-4*(!bBottom)):0);
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(t.x, t.y, 0);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 4);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 5);
#endif
                            Ipp32u ind = (bSecondField)? 1: 0;
                            if (padding[ind][MV.bSecond])
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);


                                SetInterpolationParamsLuma(&sYInterpolation, pForwMBRow[MV.bSecond][0], pForwMBStep[MV.bSecond][0], &MVInt, &MVQuarter, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolation);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolation, &sVInterpolation, pForwMBRow[MV.bSecond][1], pForwMBRow[MV.bSecond][2],pForwMBStep[MV.bSecond][1], 
                                                   &MVIntChroma, &MVQuarterChroma, j);                         


                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            }
                            else
                            {

                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockY[MV.bSecond], j, i , &MVInt, &MVQuarter);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockY[MV.bSecond],0,0,oppositePadding[ind][MV.bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockU[MV.bSecond], &InterpolateBlockV[MV.bSecond], 
                                                        oppositePadding[ind][MV.bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChroma, &MVQuarterChroma);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);

                            }

                        } //interpolation

                        if (VC1_ENC_P_MB_SKIP_1MV != MBType)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = sYInterpolation.pDst; 
                                pIn.refYStep = sYInterpolation.dstStep;      
                                pIn.pURef    = sUInterpolation.pDst; 
                                pIn.refUStep = sUInterpolation.dstStep;
                                pIn.pVRef    = sVInterpolation.pDst; 
                                pIn.refVStep = sVInterpolation.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],pCurMBStep[0], pCurMBRow[1],pCurMBRow[2], pCurMBStep[1], 
                                sYInterpolation.pDst, sYInterpolation.dstStep,
                                sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                                j,0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern = pCompressedMB->GetMBPattern();
                            CBPCY = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                            if (MBPattern==0 && mvDiff.x == 0 && mvDiff.y == 0)
                            {
                                pCompressedMB->ChangeType(VC1_ENC_P_MB_SKIP_1MV);
                                MBType = VC1_ENC_P_MB_SKIP_1MV;
                            }
                        }//VC1_ENC_P_MB_SKIP_1MV != MBType
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(MBType);
                        pDebug->SetMVInfoField(&MV,  mvPred[MV.bSecond].x,mvPred[MV.bSecond].y, 0);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 4);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);

                        if (pCompressedMB->isSkip() )
                        {
                            pDebug->SetMBAsSkip();
                        }
                        else
                        {
                            pDebug->SetCPB(MBPattern, CBPCY);
                            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        }
#endif
                        /*---------------------------Reconstruction ------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                            sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                            pRaisedMBRow[0], pRaisedMBStep[0], 
                                                            pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                                            0, j);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                                sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                                pRaisedMBRow[0], pRaisedMBStep[0], 
                                                                pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],0,j);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRaisedMBRow[0];
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0]*2;
                                smoothInfo.pRFrameU       = pRaisedMBRow[1];
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1]*2;
                                smoothInfo.pRFrameV       = pRaisedMBRow[2];
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2]*2;
                                smoothInfo.curIntra       = 0;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                                pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                                pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                                pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                        Ipp32u ind = (bSecondField)? 1: 0;
                        pDebug->SetInterpInfo(&sYInterpolation, &sUInterpolation, &sVInterpolation, 0, padding[ind][MV.bSecond]);
#endif
                    }
                    break;

                case VC1_ENC_P_MB_4MV:
                case VC1_ENC_P_MB_SKIP_4MV:
                    {
                        /*--------------------------------------- Inter --------------------------------------------------*/
                        sCoordinate         MVInt;     
                        sCoordinate         MVQuarter; 
                        sCoordinate         pMV [4]         = {0};

                        sCoordinate         MVIntChroma             = {0,0};
                        sCoordinate         MVQuarterChroma         = {0,0};
                        sCoordinate         MVChroma                = {0,0};

                        sCoordinate         mvPred[4][2] = {0};
                        bool                bSecondDominantPred = false;
                        sCoordinate         mvDiff;
                        eTransformType      BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType};

                        pCurMBInfo->Init(false);
                        pCompressedMB->Init(MBType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);

                        for (blk = 0; blk <4; blk++)
                        {
                            Ipp32u blkPosX = (blk&0x1)<<3;
                            Ipp32u blkPosY = (blk/2)<<3;

                            Ipp8u               hybridReal = 0; 

                            sCoordinate         *mvC=0, *mvB=0, *mvA=0;
                            sCoordinate         mvCEx = {0};
                            sCoordinate         mvAEx = {0};
                            sCoordinate         mv1, mv2, mv3;

                            pMV[blk].x         = MEParams->pSrc->MBs[j + i*w].MV[0][blk].x;
                            pMV[blk].y         = MEParams->pSrc->MBs[j + i*w].MV[0][blk].y;
                            pMV[blk].bSecond   = (MEParams->pSrc->MBs[j + i*w].Refindex[0][blk] == 1);

                            bool MV_Second = pMV[blk].bSecond;

                            GetMVPredictionPBlk_F(blk);
                            bSecondDominantPred = PredictMVField2(mvA,mvB,mvC, mvPred[blk], &scInfo,bSecondField,&mvAEx,&mvCEx, false, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetScaleType(bSecondDominantPred, 0, blk);
#endif
                            //ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMBMin,MVPredMBMax);
                            if (mvA && mvC)
                            {
                                sCoordinate*  mvCPred = (mvC->bSecond == MV_Second)? mvC:&mvCEx;
                                sCoordinate*  mvAPred = (mvA->bSecond == MV_Second)? mvA:&mvAEx;                                
                                hybridReal = HybridPrediction(&(mvPred[blk][MV_Second]),&pMV[blk],mvAPred,mvCPred,th);
             
                            } // mvA && mvC
                            pCompressedMB->SetHybrid(hybridReal, blk);
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetMVInfo(&pMV[blk], mvPred[blk][pMV[blk].bSecond].x,mvPred[blk][pMV[blk].bSecond].y, 0, blk);
                            pDebug->SetPredFlag(bSecondDominantPred!=MV_Second, 0, blk);
                            pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == MV_Second || mvA==0)? mvA:&mvAEx,
                                /*mvB,*/ (mvC && mvC->bSecond == MV_Second || mvC==0)? mvC:&mvCEx, 0, blk);
                            pDebug->SetHybrid(hybridReal, 0, blk);
#endif

                            STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                            if (VC1_ENC_P_MB_SKIP_4MV == MBType)
                            {
                                //correct ME results
                                VM_ASSERT(pMV[blk].x==mvPred[blk][MV_Second].x && pMV[blk].y==mvPred[blk][MV_Second].y);
                                pMV[blk].x=mvPred[blk][MV_Second].x;
                                pMV[blk].y=mvPred[blk][MV_Second].y;
                            }
                            pCurMBInfo->SetMV_F(pMV[blk], blk);

                            mvDiff.x = pMV[blk].x - mvPred[blk][MV_Second].x;
                            mvDiff.y = pMV[blk].y - mvPred[blk][MV_Second].y;
                            mvDiff.bSecond = (MV_Second != bSecondDominantPred);
                            pCompressedMB->SetBlockdMV_F(mvDiff, blk);

                            if (VC1_ENC_P_MB_SKIP_4MV != MBType || bRaiseFrame)
                            {
                                sCoordinate t = {pMV[blk].x,pMV[blk].y + ((!MV_Second)? (2-4*(!bBottom)):0),MV_Second};
                                GetIntQuarterMV(t,&MVInt, &MVQuarter);
#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetIntrpMV(t.x, t.y, 0, blk);
#endif
                                Ipp32u ind = (bSecondField)? 1: 0;
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                if (padding[ind][MV_Second])
                                {
                                    SetInterpolationParamsLumaBlk(&sYInterpolationBlk[blk], pForwMBRow[MV_Second][0], pForwMBStep[MV_Second][0], &MVInt, &MVQuarter, j,blk);
                                    
                                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                    ippSts = InterpolateLumaFunction(&sYInterpolationBlk[blk]);
                                    VC1_ENC_IPP_CHECK(ippSts);

                                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime); 
                                    
                                }
                                else
                                {
                                    SetInterpolationParamsLumaBlk(&InterpolateBlockYBlk[MV_Second][blk], j, i, &MVInt, &MVQuarter,blk);

                                    IPP_STAT_START_TIME(m_IppStat->IppStartTime); 

                                    ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYBlk[MV_Second][blk],0,0,oppositePadding[ind][MV_Second],true,m_uiRoundControl,bBottom);
                                    VC1_ENC_IPP_CHECK(ippSts);

                                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);                        
                                }
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            } //interpolation
                        } // for (luma blocks


                        pCurMBInfo->GetLumaVectorFields(&MV, true);

                        /*-------------------------- interpolation for chroma---------------------------------*/

                        if (VC1_ENC_P_MB_SKIP_4MV != MBType || bRaiseFrame)
                        {                       
                            CalculateChroma(MV,&MVChroma);
                            pCurMBInfo->SetMVChroma_F(MVChroma);
                            MVChroma.y = MVChroma.y + ((!MV.bSecond)? (2-4*(!bBottom)):0);
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);

                            Ipp32u ind = (bSecondField)? 1: 0;

                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            if (padding[ind][MV.bSecond])
                            {   
#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 4);
                                pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 5);
#endif                        
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                InterpolateChroma (&sUInterpolation, &sVInterpolation, pForwMBRow[MV.bSecond][1], pForwMBRow[MV.bSecond][2],pForwMBStep[MV.bSecond][1], 
                                                   &MVIntChroma, &MVQuarterChroma, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                
                            }
                            else
                            { 

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);      
                                
                                InterpolateChromaPad   (&InterpolateBlockU[MV.bSecond], &InterpolateBlockV[MV.bSecond], 
                                                        oppositePadding[ind][MV.bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChroma, &MVQuarterChroma);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);                /////////////////////////////////////////////
                        }
                        /////////////////////////////////////////////////////////////////////////////////////////////////////////////
                        if (VC1_ENC_P_MB_SKIP_4MV != MBType)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = sYInterpolation.pDst; 
                                pIn.refYStep = sYInterpolation.dstStep;      
                                pIn.pURef    = sUInterpolation.pDst; 
                                pIn.refUStep = sUInterpolation.dstStep;
                                pIn.pVRef    = sVInterpolation.pDst; 
                                pIn.refVStep = sVInterpolation.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],pCurMBStep[0], pCurMBRow[1],pCurMBRow[2], pCurMBStep[1], 
                                sYInterpolation.pDst, sYInterpolation.dstStep,
                                sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                                j,0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern = pCompressedMB->GetMBPattern();
                            CBPCY = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);

                        }//VC1_ENC_P_MB_SKIP_1MV != MBType
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(MBType);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 4);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);

                        if (pCompressedMB->isSkip() )
                        {
                            pDebug->SetMBAsSkip();
                        }
                        else
                        {
                            pDebug->SetCPB(MBPattern, CBPCY);
                            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        }
#endif
                        /*---------------------------Reconstruction ------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                            sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                            pRaisedMBRow[0], pRaisedMBStep[0], 
                                                            pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                                            0 , j);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                                sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                                pRaisedMBRow[0], pRaisedMBStep[0], 
                                                                pRaisedMBRow[1], pRaisedMBRow [2], pRaisedMBStep[1],0,j);
                               
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRaisedMBRow[0];
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0]*2;
                                smoothInfo.pRFrameU       = pRaisedMBRow[1];
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1]*2;
                                smoothInfo.pRFrameV       = pRaisedMBRow[2];
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2]*2;
                                smoothInfo.curIntra       = 0;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                                pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                                pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                                pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                        Ipp32u ind = (bSecondField)? 1: 0;
                        pDebug->SetInterpInfo(&sYInterpolation, &sUInterpolation, &sVInterpolation, 0, padding[ind][MV.bSecond]);
#endif
                    }
                    break;
                default:
                    VM_ASSERT(0);
                    return UMC::UMC_ERR_FAILED;
                }

                if (pSavedMV && pRefType)
                {
                    *(pSavedMV ++) = MV.x;
                    *(pSavedMV ++) = MV.y;
                    *(pRefType ++) = (MBType == VC1_ENC_P_MB_INTRA)? 0 : MV.bSecond + 1;
                }


#ifdef VC1_ENC_DEBUG_ON
                if(!bSubBlkTS)
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(), 15, 15);
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(), 15, 15);
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(),15);
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), 15);
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), 15);
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), 15);
                }
                else
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(),
                        pCurMBInfo->GetLumaAdUppEdge(), pCurMBInfo->GetLumaAdBotEdge() );
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(),
                        pCurMBInfo->GetLumaAdLefEdge(), pCurMBInfo->GetLumaAdRigEdge());
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(), pCurMBInfo->GetUAdHorEdge());
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), pCurMBInfo->GetVAdHorEdge());
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), pCurMBInfo->GetUAdVerEdge());
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), pCurMBInfo->GetVAdVerEdge());
                }
#endif

                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }

            deblkPattern = (deblkPattern | 0x4 | ( (! (Ipp8u)((i + 1 - (hMB -1)) >> 31)<<3))) & deblkMask;
            smoothPattern = 0x6 & smoothMask;

            //Row deblocking
            //STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            //        if (m_pSequenceHeader->IsLoopFilter() && bRaiseFrame && (i != firstRow || firstRow == hMB-1))
            //        {
            //STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
            //          Ipp8u *DeblkPlanes[3] = {m_pRaisedPlane[0] + i*m_uiRaisedPlaneStep[0]*VC1_ENC_LUMA_SIZE,
            //                                   m_pRaisedPlane[1] + i*m_uiRaisedPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
            //                                   m_pRaisedPlane[2] + i*m_uiRaisedPlaneStep[2]*VC1_ENC_CHROMA_SIZE};
            //            m_pMBs->StartRow();
            //            if(bSubBlkTS)
            //            {
            //                if(i < hMB-1)
            //                    Deblock_P_RowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                    if(firstRow != hMB - 1)
            //                       Deblock_P_BottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                    else
            //                       Deblock_P_TopBottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //            }
            //            else
            //            {
            //                if(i < hMB-1)
            //                    Deblock_P_RowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                if(firstRow != hMB - 1)
            //                    Deblock_P_BottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                    Deblock_P_TopBottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //            }
            //STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            //        }
            //STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);
            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;

            pCurMBRow[0]+= pCurMBStep[0]*VC1_ENC_LUMA_SIZE;
            pCurMBRow[1]+= pCurMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pCurMBRow[2]+= pCurMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pRaisedMBRow[0]+= pRaisedMBStep[0]*VC1_ENC_LUMA_SIZE;
            pRaisedMBRow[1]+= pRaisedMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pRaisedMBRow[2]+= pRaisedMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pForwMBRow[0][0]+= pForwMBStep[0][0]*VC1_ENC_LUMA_SIZE;
            pForwMBRow[0][1]+= pForwMBStep[0][1]*VC1_ENC_CHROMA_SIZE;
            pForwMBRow[0][2]+= pForwMBStep[0][2]*VC1_ENC_CHROMA_SIZE;

            pForwMBRow[1][0]+= pForwMBStep[1][0]*VC1_ENC_LUMA_SIZE;
            pForwMBRow[1][1]+= pForwMBStep[1][1]*VC1_ENC_CHROMA_SIZE;
            pForwMBRow[1][2]+= pForwMBStep[1][2]*VC1_ENC_CHROMA_SIZE;

        }

#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame && bSecondField && i == h)
            pDebug->PrintRestoredFrame( m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
            m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
            m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
#endif
        return err;
    }

    UMC::Status VC1EncoderPictureADV::PAC_PField1Ref(UMC::MeParams* MEParams,bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;
        bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);

        bool                        bBottom      =  IsBottomField(m_bTopFieldFirst, bSecondField);

        Ipp8u*                      pCurMBRow[3]    = {0};
        Ipp32u                      pCurMBStep[3]   = {0};
        Ipp8u*                      pRaisedMBRow[3] = {0};
        Ipp32u                      pRaisedMBStep[3]= {0};

        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        Ipp8u*                      pForwMBRow  [3]  = {0};
        Ipp32u                      pForwMBStep [3]  = {0};

        VC1EncoderCodedMB*          pCodedMB = &(m_pCodedMB[w*h*bSecondField]);

        //forward transform quantization

        _SetIntraInterLumaFunctions;

        //inverse transform quantization
        IntraInvTransformQuantFunction IntraInvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :IntraInvTransformQuantNonUniform;

        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (m_bUniformQuant) ? InterInvTransformQuantUniform : InterInvTransformQuantNonUniform;
        CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())?GetChromaMVFast:GetChromaMV;

        bool                        bIsBilinearInterpolation = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR);
        InterpolateFunction         InterpolateLumaFunction  = (bIsBilinearInterpolation)?_own_ippiInterpolateQPBilinear_VC1_8u_C1R:_own_ippiInterpolateQPBicubic_VC1_8u_C1R;

        Ipp8u                       tempInterpolationBuffer[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];

        IppVCInterpolate_8u         sYInterpolation;
        IppVCInterpolate_8u         sUInterpolation;
        IppVCInterpolate_8u         sVInterpolation;

        // interpolation + MB Level padding
        InterpolateFunctionPadding  InterpolateLumaFunctionPadding = (bIsBilinearInterpolation)?own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R: own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;

        IppVCInterpolateBlock_8u    InterpolateBlockY;
        IppVCInterpolateBlock_8u    InterpolateBlockU;
        IppVCInterpolateBlock_8u    InterpolateBlockV;


        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
        bool                        dACPrediction   = true;

        IppiSize                    blkSize     = {8,8};
        IppiSize                    blkSizeLuma = {16,16};

        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];

        Ipp16s*                     pSavedMV        =  (m_pSavedMV)?  m_pSavedMV + w*2*(h*bSecondField + firstRow):0;
        Ipp8u*                      pRefType        =  (m_pRefType)?  m_pRefType + w*(h*bSecondField   + firstRow):0;

        bool                        bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR ||
            m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

        bool                        bSubBlkTS           = m_pSequenceHeader->IsVSTransform() && (!(m_bVSTransform &&m_uiTransformType==VC1_ENC_8x8_TRANSFORM));
        fGetExternalEdge            pGetExternalEdge    = GetExternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS
        fGetInternalEdge            pGetInternalEdge    = GetInternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS

        Ipp8u deblkPattern = (firstRow != hMB - 1)? 0 : 0x8;   //4 bits: right 1 bit - 0 - left/1 - not left,
        //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row, 10 - top bottom row
        //middle 1 bit - 1 - right/0 - not right

        Ipp8u                       deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFC : 0;

        Ipp8u smoothMask = (m_pSequenceHeader->IsOverlap() && (m_uiQuant >= 9)) ? 0xFF : 0;
        Ipp8u smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
        //0/1 - top/not top, 0/1 - left/not left

        SmoothInfo_P_Adv smoothInfo = {0};

        IppStatus                   ippSts  = ippStsNoErr;
        sScaleInfo                  scInfo;
        Ipp32u                      th = /*(bMVHalf)?16:*/32;
        Ipp8s                       mv_t0 = (m_uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)?
            (bBottom)? 2:-2:0;

        Ipp32u                      padding[2][2] = {{m_uiForwardPlanePadding, m_uiForwardPlanePadding}, // first field
        {m_uiRaisedPlanePadding,  m_uiForwardPlanePadding}};// second field
        Ipp32u                      oppositePadding[2][2] = {{(m_bForwardInterlace)? 0:1,(m_bForwardInterlace)? 0:1 }, 
        {0,(m_bForwardInterlace)? 0:1}};
#ifdef VC1_ENC_DEBUG_ON
        Ipp32u                      fieldShift = (bSecondField)? h : 0;
        pDebug->SetRefNum(1);
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
        pDebug->SetSliceInfo(firstRow + fieldShift, firstRow + nRows + fieldShift);
        pDebug->SetInterpolType(bIsBilinearInterpolation);
        pDebug->SetRounControl(m_uiRoundControl);
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
        pDebug->SetVTSFlag(bSubBlkTS);
#endif
        assert (m_uiReferenceFieldType != VC1_ENC_REF_FIELD_BOTH);    // VC1_ENC_REF_FIELD_FIRST
        // VC1_ENC_REF_FIELD_SECOND
        InitScaleInfo(&scInfo,bSecondField,bBottom,m_nReferenceFrameDist,m_uiMVRangeIndex);

        if (pSavedMV && pRefType)
        {
            memset(pSavedMV,0,w*(hMB - firstRow)*2*sizeof(Ipp16s));
            memset(pRefType,(m_uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)? 1:2,w*(hMB - firstRow)*sizeof(Ipp8u));
        }


        err = m_pMBs->Reset();
        VC1_ENC_CHECK(err);

        SetFieldStep(pCurMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pCurMBRow, m_pPlane, m_uiPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(pRaisedMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pRaisedMBRow, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);

        SetInterpolationParams(&sYInterpolation,&sUInterpolation,&sVInterpolation,
            tempInterpolationBuffer,true, true);

        err = Set1RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep,  m_uiReferenceFieldType,
            bSecondField, bBottom, 0);
        VC1_ENC_CHECK(err);

        SetInterpolationParams(&InterpolateBlockY,&InterpolateBlockU,&InterpolateBlockV, tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow, pForwMBStep, true);

        err = Set1RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep,  m_uiReferenceFieldType,
            bSecondField, bBottom, firstRow);
        VC1_ENC_CHECK(err);
#ifdef VC1_ENC_CHECK_MV
        err = CheckMEMV_PField(MEParams,bMVHalf,firstRow,nRows);
        assert(err == UMC::UMC_OK);
#endif

        /* -------------------------------------------------------------------------*/
        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                Ipp32s              xLuma        =  VC1_ENC_LUMA_SIZE*j;
                Ipp32s              xChroma      =  VC1_ENC_CHROMA_SIZE*j;

                sCoordinate         MVInt       = {0,0};
                sCoordinate         MVQuarter   = {0,0};
                sCoordinate         MV          = {0,0};
                Ipp8u               MBPattern   = 0;
                Ipp8u               CBPCY       = 0;

                VC1EncoderMBInfo  * pCurMBInfo  = m_pMBs->GetCurrMBInfo();
                VC1EncoderMBData  * pCurMBData  = m_pMBs->GetCurrMBData();
                VC1EncoderMBData  * pRecMBData  =   m_pMBs->GetRecCurrMBData();

                VC1EncoderMBInfo* left        = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* topLeft     = m_pMBs->GetTopLeftMBInfo();
                VC1EncoderMBInfo* top         = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* topRight    = m_pMBs->GetTopRightMBInfo();

                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);
                eMBType MBType;

                switch (MEParams->pSrc->MBs[j + i*w].MbType)
                {
                case UMC::ME_MbIntra:
                    {
                        NeighbouringMBsData MBs;

                        MBs.LeftMB    = ((left)? left->isIntra():0)         ? m_pMBs->GetLeftMBData():0;
                        MBs.TopMB     = ((top)? top->isIntra():0)           ? m_pMBs->GetTopMBData():0;
                        MBs.TopLeftMB = ((topLeft)? topLeft->isIntra():0)   ? m_pMBs->GetTopLeftMBData():0;
                        MV.x = 0;
                        MV.y =0;
                        MBType = VC1_ENC_P_MB_INTRA;
                        pCompressedMB->Init(VC1_ENC_P_MB_INTRA);
                        pCurMBInfo->Init(true);

                        if (pRefType)
                            *pRefType  = 0;

                        /*-------------------------- Compression ----------------------------------------------------------*/
                        pCurMBData->CopyMBProg(pCurMBRow[0],pCurMBStep[0],pCurMBRow[1], pCurMBRow[2], pCurMBStep[1],j);

                        //only intra blocks:
                        for (blk = 0; blk<6; blk++)
                        {

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                            _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                        }

                        STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                        dACPrediction = DCACPredictionFrame( pCurMBData,&MBs,
                            pRecMBData, 0,direction);
                        STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                        for (blk=0; blk<6; blk++)
                        {
                            pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                pRecMBData->m_uiBlockStep[blk],
                                VC1_Inter_InterlaceIntra_8x8_Scan_Adv,
                                blk);
                        }

                        MBPattern = pCompressedMB->GetMBPattern();
                        CBPCY = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetACPrediction(dACPrediction);
                        pCompressedMB->SetMBCBPCY(CBPCY);
                        pCurMBInfo->SetEdgesIntra(i==0, j==0);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(VC1_ENC_P_MB_INTRA);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                        /*-------------------------- Reconstruction ----------------------------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            for (blk=0;blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                    DCQuant, doubleQuant);
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],blkSize, 0);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pRecMBData->PasteMBProg(pRaisedMBRow[0],pRaisedMBStep[0],pRaisedMBRow[1],pRaisedMBRow[2],pRaisedMBStep[1],j);
                           
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRaisedMBRow[0];
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0]*2;
                                smoothInfo.pRFrameU       = pRaisedMBRow[1];
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1]*2;
                                smoothInfo.pRFrameV       = pRaisedMBRow[2];
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2]*2;
                                smoothInfo.curIntra       = 0x3F;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            {
                                STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};

                                m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                        /*------------------------------------------- ----------------------------------------------------------*/
                    }
                    break;
                case UMC::ME_MbFrw:
                case UMC::ME_MbFrwSkipped:
                    {
                        /*--------------------------------------- Inter --------------------------------------------------*/
                        sCoordinate MVIntChroma     = {0,0};
                        sCoordinate MVQuarterChroma = {0,0};
                        sCoordinate MVChroma       =  {0,0};

                        sCoordinate *mvC=0, *mvB=0, *mvA=0;
                        sCoordinate  mv1, mv2, mv3;
                        sCoordinate  mvPred = {0,0};
                        sCoordinate  mvDiff = {0,0};
                        eTransformType              BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                                                                        m_uiTransformType, m_uiTransformType,
                                                                        m_uiTransformType, m_uiTransformType};

                        Ipp8u        hybridReal = 0;


                        MBType = (UMC::ME_MbFrw == MEParams->pSrc->MBs[j + i*w].MbType)? VC1_ENC_P_MB_1MV:VC1_ENC_P_MB_SKIP_1MV;

                        MV.x        = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                        MV.y        = MEParams->pSrc->MBs[j + i*w].MV[0]->y;


                        pCurMBInfo->Init(false);
                        pCompressedMB->Init(MBType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        /*MV prediction -!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
                        GetMVPredictionPField(true);
                        PredictMVField1(mvA,mvB,mvC, &mvPred);

                        if (mvA && mvC)
                        {
                            hybridReal = HybridPrediction(&mvPred,&MV,mvA ,mvC ,th);
                        }
                        pCompressedMB->SetHybrid(hybridReal);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetPredFlag(0, 0);
                        pDebug->SetFieldMVPred1Ref(mvA, mvC, 0);
                        pDebug->SetHybrid(hybridReal, 0);
#endif
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        if (VC1_ENC_P_MB_SKIP_1MV == MBType)
                        {
                            //correct ME results
                            assert(MV.x==mvPred.x && MV.y==mvPred.y);
                            MV.x=mvPred.x;
                            MV.y=mvPred.y;
                        }
                        pCurMBInfo->SetMV(MV);
                        mvDiff.x = MV.x - mvPred.x;
                        mvDiff.y = MV.y - mvPred.y;
                        pCompressedMB->SetdMV(mvDiff);

                        if (VC1_ENC_P_MB_SKIP_1MV != MBType || bRaiseFrame)
                        {
                            /*interpolation*/

                            sCoordinate t = {MV.x, MV.y + mv_t0, 0};
                            GetIntQuarterMV(t,&MVInt, &MVQuarter);

                            CalculateChroma(MV,&MVChroma);
                            pCurMBInfo->SetMVChroma(MVChroma);
                            MVChroma.y = MVChroma.y + mv_t0;
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);

#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(t.x, t.y, 0);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 4);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 5);
#endif
                            Ipp32u ind0 = (bSecondField)? 1: 0;
                            Ipp32u ind1 = (m_uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)?0:1;
                            if (padding[ind0][ind1])
                            {

                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolation, pForwMBRow[0], pForwMBStep[0], &MVInt, &MVQuarter, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolation);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolation, &sVInterpolation, pForwMBRow[1], pForwMBRow[2],pForwMBStep[1], 
                                                   &MVIntChroma, &MVQuarterChroma, j);    

                                
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            }
                            else
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockY, j, i , &MVInt, &MVQuarter);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockY,0,0,oppositePadding[ind0][ind1],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockU, &InterpolateBlockV, 
                                                        oppositePadding[ind0][ind1],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChroma, &MVQuarterChroma);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);                

                            }
                        } //interpolation

                        if (VC1_ENC_P_MB_SKIP_1MV != MBType)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = sYInterpolation.pDst; 
                                pIn.refYStep = sYInterpolation.dstStep;      
                                pIn.pURef    = sUInterpolation.pDst; 
                                pIn.refUStep = sUInterpolation.dstStep;
                                pIn.pVRef    = sVInterpolation.pDst; 
                                pIn.refVStep = sVInterpolation.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],pCurMBStep[0], pCurMBRow[1],pCurMBRow[2], pCurMBStep[1], 
                                sYInterpolation.pDst, sYInterpolation.dstStep,
                                sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                                j,0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern = pCompressedMB->GetMBPattern();
                            CBPCY = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                            if (MBPattern==0 && mvDiff.x == 0 && mvDiff.y == 0)
                            {
                                pCompressedMB->ChangeType(VC1_ENC_P_MB_SKIP_1MV);
                                MBType = VC1_ENC_P_MB_SKIP_1MV;
                            }
                        }//VC1_ENC_P_MB_SKIP_1MV != MBType
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(MBType);
                        pDebug->SetMVInfoField(&MV,  mvPred.x,mvPred.y, 0);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 4);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);

                        if (pCompressedMB->isSkip() )
                        {
                            pDebug->SetMBAsSkip();
                        }
                        else
                        {
                            pDebug->SetCPB(MBPattern, CBPCY);
                            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        }
#endif
                        /*---------------------------Reconstruction ------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                            sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                            pRaisedMBRow[0], pRaisedMBStep[0], 
                                                            pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                                            0, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                                sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                                pRaisedMBRow[0], pRaisedMBStep[0], 
                                                                pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],0,j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRaisedMBRow[0];
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0]*2;
                                smoothInfo.pRFrameU       = pRaisedMBRow[1];
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1]*2;
                                smoothInfo.pRFrameV       = pRaisedMBRow[2];
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2]*2;
                                smoothInfo.curIntra       = 0;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                                pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                                pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                                pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                        Ipp32u ind0 = (bSecondField)? 1: 0;
                        Ipp32u ind1 = (m_uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)?0:1;

                        pDebug->SetInterpInfo(&sYInterpolation, &sUInterpolation, &sVInterpolation, 0, padding[ind0][ind1]);
#endif
                    }
                    break;
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
                }

                if (m_pSavedMV && pRefType)
                {
                    *(pSavedMV ++) = MV.x;
                    *(pSavedMV ++) = MV.y;
                    pRefType ++;
                }

#ifdef VC1_ENC_DEBUG_ON
                if(!bSubBlkTS)
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(), 15, 15);
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(), 15, 15);
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(),15);
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), 15);
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), 15);
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), 15);
                }
                else
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(),
                        pCurMBInfo->GetLumaAdUppEdge(), pCurMBInfo->GetLumaAdBotEdge() );
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(),
                        pCurMBInfo->GetLumaAdLefEdge(), pCurMBInfo->GetLumaAdRigEdge());
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(), pCurMBInfo->GetUAdHorEdge());
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), pCurMBInfo->GetVAdHorEdge());
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), pCurMBInfo->GetUAdVerEdge());
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), pCurMBInfo->GetVAdVerEdge());
                }
#endif

                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }

            deblkPattern = (deblkPattern | 0x4 | ( (! (Ipp8u)((i + 1 - (hMB -1)) >> 31)<<3))) & deblkMask;
            smoothPattern = 0x6 & smoothMask;

            //Row deblocking
            //STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            //        if (m_pSequenceHeader->IsLoopFilter() && bRaiseFrame && (i != firstRow || firstRow == hMB-1))
            //        {
            //STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
            //
            //           Ipp8u *planes[3] = {0};
            //           err = SetFieldPlane(planes, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, i);
            //           VC1_ENC_CHECK(err);
            //
            //            Ipp32u step[3] = {0};
            //            SetFieldStep(step, m_uiPlaneStep);
            //
            //            m_pMBs->StartRow();
            //            if(bSubBlkTS)
            //            {
            //                if(i < hMB - 1)
            //                    Deblock_P_RowVts(planes, step, w, m_uiQuant, m_pMBs);
            //                else
            //                if(firstRow != hMB - 1)
            //                    Deblock_P_BottomRowVts(planes, step, w, m_uiQuant, m_pMBs);
            //                else
            //                    Deblock_P_TopBottomRowVts(planes, step, w, m_uiQuant, m_pMBs);
            //            }
            //            else
            //            {
            //                if(i < hMB - 1)
            //                    Deblock_P_RowNoVts(planes, step, w, m_uiQuant, m_pMBs);
            //                else
            //                if(firstRow != hMB - 1)
            //                    Deblock_P_BottomRowNoVts(planes, step, w, m_uiQuant, m_pMBs);
            //                else
            //                    Deblock_P_TopBottomRowNoVts(planes, step, w, m_uiQuant, m_pMBs);
            //            }
            //STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            //        }
            //STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;

            pCurMBRow[0]+= pCurMBStep[0]*VC1_ENC_LUMA_SIZE;
            pCurMBRow[1]+= pCurMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pCurMBRow[2]+= pCurMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pRaisedMBRow[0]+= pRaisedMBStep[0]*VC1_ENC_LUMA_SIZE;
            pRaisedMBRow[1]+= pRaisedMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pRaisedMBRow[2]+= pRaisedMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pForwMBRow[0]+= pForwMBStep[0]*VC1_ENC_LUMA_SIZE;
            pForwMBRow[1]+= pForwMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pForwMBRow[2]+= pForwMBStep[2]*VC1_ENC_CHROMA_SIZE;

        }

#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame && bSecondField && i == h)
            pDebug->PrintRestoredFrame( m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
            m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
            m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
#endif


        return err;
    }
    UMC::Status VC1EncoderPictureADV::PAC_PField1RefMixed(UMC::MeParams* MEParams,bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;
        bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);

        bool                        bBottom      =  IsBottomField(m_bTopFieldFirst, bSecondField);

        Ipp8u*                      pCurMBRow[3]    = {0};
        Ipp32u                      pCurMBStep[3]   = {0};
        Ipp8u*                      pRaisedMBRow[3] = {0};
        Ipp32u                      pRaisedMBStep[3]= {0};

        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        Ipp8u*                      pForwMBRow  [3]  = {0};
        Ipp32u                      pForwMBStep [3]  = {0};

        VC1EncoderCodedMB*          pCodedMB = &(m_pCodedMB[w*h*bSecondField]);

        //forward transform quantization

        _SetIntraInterLumaFunctions;

        //inverse transform quantization
        IntraInvTransformQuantFunction IntraInvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :
            IntraInvTransformQuantNonUniform;

        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (m_bUniformQuant) ? InterInvTransformQuantUniform :
            InterInvTransformQuantNonUniform;
        CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())?
GetChromaMVFast:GetChromaMV;

        InterpolateFunction         InterpolateLumaFunction  = _own_ippiInterpolateQPBicubic_VC1_8u_C1R;

        Ipp8u                       tempInterpolationBuffer[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];

        IppVCInterpolate_8u         sYInterpolation;
        IppVCInterpolate_8u         sUInterpolation;
        IppVCInterpolate_8u         sVInterpolation;
        IppVCInterpolateBlock_8u    InterpolateBlockYBlk[4];


        // interpolation + MB Level padding
        InterpolateFunctionPadding  InterpolateLumaFunctionPadding   =  own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;

        IppVCInterpolateBlock_8u    InterpolateBlockY;
        IppVCInterpolateBlock_8u    InterpolateBlockU;
        IppVCInterpolateBlock_8u    InterpolateBlockV;
        IppVCInterpolate_8u         sYInterpolationBlk[4];


        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
        bool                        dACPrediction   = true;

        IppiSize                    blkSize     = {8,8};
        IppiSize                    blkSizeLuma = {16,16};

        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];

        Ipp16s*                     pSavedMV        =  (m_pSavedMV)?  m_pSavedMV + w*2*(h*bSecondField + firstRow):0;
        Ipp8u*                      pRefType        =  (m_pRefType)?  m_pRefType + w*(h*bSecondField   + firstRow):0;

        bool                        bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR ||
            m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

        bool                        bSubBlkTS           = m_pSequenceHeader->IsVSTransform() && (!(m_bVSTransform &&m_uiTransformType==VC1_ENC_8x8_TRANSFORM));
        fGetExternalEdge            pGetExternalEdge    = GetExternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS
        fGetInternalEdge            pGetInternalEdge    = GetInternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS

        Ipp8u deblkPattern = (firstRow != hMB - 1)? 0 : 0x8;   //4 bits: right 1 bit - 0 - left/1 - not left,
        //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row, 10 - top bottom row
        //middle 1 bit - 1 - right/0 - not right

        Ipp8u                       deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFC : 0;

        Ipp8u smoothMask = (m_pSequenceHeader->IsOverlap() && (m_uiQuant >= 9)) ? 0xFF : 0;
        Ipp8u smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
        //0/1 - top/not top, 0/1 - left/not left

        SmoothInfo_P_Adv smoothInfo = {0};


        IppStatus                   ippSts  = ippStsNoErr;
        sScaleInfo                  scInfo;
        Ipp32u                      th = /*(bMVHalf)?16:*/32;
        Ipp8s                       mv_t0 = (m_uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)?
            (bBottom)? 2:-2:0;

        Ipp32u                      padding[2][2] = {{m_uiForwardPlanePadding, m_uiForwardPlanePadding}, // first field
        {m_uiRaisedPlanePadding,  m_uiForwardPlanePadding}};// second field
        Ipp32u                      oppositePadding[2][2] = {{(m_bForwardInterlace)? 0:1,(m_bForwardInterlace)? 0:1 }, 
        {0,(m_bForwardInterlace)? 0:1}};


#ifdef VC1_ENC_DEBUG_ON
        bool                        bIsBilinearInterpolation = false;
        Ipp32u                      fieldShift = (bSecondField)? h : 0;
        pDebug->SetRefNum(1);
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
        pDebug->SetSliceInfo(firstRow + fieldShift, firstRow + nRows + fieldShift);
        pDebug->SetInterpolType(bIsBilinearInterpolation);
        pDebug->SetRounControl(m_uiRoundControl);
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
        pDebug->SetVTSFlag(bSubBlkTS);
#endif
        assert (m_uiReferenceFieldType != VC1_ENC_REF_FIELD_BOTH);    // VC1_ENC_REF_FIELD_FIRST
        // VC1_ENC_REF_FIELD_SECOND
        InitScaleInfo(&scInfo,bSecondField,bBottom,m_nReferenceFrameDist,m_uiMVRangeIndex);

        if (pSavedMV && pRefType)
        {
            memset(pSavedMV,0,w*(hMB - firstRow)*2*sizeof(Ipp16s));
            memset(pRefType,(m_uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)? 1:2,w*(hMB - firstRow)*sizeof(Ipp8u));
        }


        err = m_pMBs->Reset();
        VC1_ENC_CHECK(err);

        SetFieldStep(pCurMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pCurMBRow, m_pPlane, m_uiPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(pRaisedMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pRaisedMBRow, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);

        SetInterpolationParams(&sYInterpolation,&sUInterpolation,&sVInterpolation,
            tempInterpolationBuffer,true, true);
        SetInterpolationParams4MV(sYInterpolationBlk,&sUInterpolation,&sVInterpolation,
            tempInterpolationBuffer,true, true);

        err = Set1RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep,  m_uiReferenceFieldType,
            bSecondField, bBottom, 0);



        VC1_ENC_CHECK(err);

        SetInterpolationParams(&InterpolateBlockY,&InterpolateBlockU,&InterpolateBlockV, tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow, pForwMBStep, true);

        SetInterpolationParams4MV (InterpolateBlockYBlk, tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow, pForwMBStep, true);



        err = Set1RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep,  m_uiReferenceFieldType,
            bSecondField, bBottom, firstRow);
        VC1_ENC_CHECK(err);




#ifdef VC1_ENC_CHECK_MV
        err = CheckMEMV_P_MIXED(MEParams,bMVHalf,firstRow,nRows, true);
        assert(err == UMC::UMC_OK);
#endif

        /* -------------------------------------------------------------------------*/
        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                Ipp32s              xLuma        =  VC1_ENC_LUMA_SIZE*j;
                Ipp32s              xChroma      =  VC1_ENC_CHROMA_SIZE*j;

                //sCoordinate         MVInt       = {0,0};
                //sCoordinate         MVQuarter   = {0,0};
                sCoordinate         MV          = {0,0};
                Ipp8u               MBPattern   = 0;
                Ipp8u               CBPCY       = 0;

                VC1EncoderMBInfo  * pCurMBInfo  = m_pMBs->GetCurrMBInfo();
                VC1EncoderMBData  * pCurMBData  = m_pMBs->GetCurrMBData();
                VC1EncoderMBData  * pRecMBData  =   m_pMBs->GetRecCurrMBData();

                VC1EncoderMBInfo* left        = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* topLeft     = m_pMBs->GetTopLeftMBInfo();
                VC1EncoderMBInfo* top         = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* topRight    = m_pMBs->GetTopRightMBInfo();

                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);
                eMBType MBType;


                switch (MEParams->pSrc->MBs[j + i*w].MbType)
                {
                case UMC::ME_MbIntra:
                    MBType = VC1_ENC_P_MB_INTRA;
                    break;
                case UMC::ME_MbFrw:
                    MBType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_P_MB_4MV:VC1_ENC_P_MB_1MV;
                    break;
                case UMC::ME_MbFrwSkipped:
                    MBType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_P_MB_SKIP_4MV:VC1_ENC_P_MB_SKIP_1MV;
                    break;
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
                }

                switch (MBType)
                {
                case VC1_ENC_P_MB_INTRA:
                    {
                        NeighbouringMBsData MBs;

                        MBs.LeftMB    = ((left)? left->isIntra():0)         ? m_pMBs->GetLeftMBData():0;
                        MBs.TopMB     = ((top)? top->isIntra():0)           ? m_pMBs->GetTopMBData():0;
                        MBs.TopLeftMB = ((topLeft)? topLeft->isIntra():0)   ? m_pMBs->GetTopLeftMBData():0;
                        MV.x = 0;
                        MV.y =0;
                        MBType = VC1_ENC_P_MB_INTRA;
                        pCompressedMB->Init(VC1_ENC_P_MB_INTRA);
                        pCurMBInfo->Init(true);

                        if (pRefType)
                            *pRefType  = 0;

                        /*-------------------------- Compression ----------------------------------------------------------*/
                        pCurMBData->CopyMBProg(pCurMBRow[0],pCurMBStep[0],pCurMBRow[1], pCurMBRow[2], pCurMBStep[1],j);

                        //only intra blocks:
                        for (blk = 0; blk<6; blk++)
                        {

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                            _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                        }

                        STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                        dACPrediction = DCACPredictionFrame( pCurMBData,&MBs,
                            pRecMBData, 0,direction);
                        STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                        for (blk=0; blk<6; blk++)
                        {
                            pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                pRecMBData->m_uiBlockStep[blk],
                                VC1_Inter_InterlaceIntra_8x8_Scan_Adv,
                                blk);
                        }

                        MBPattern = pCompressedMB->GetMBPattern();
                        CBPCY = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetACPrediction(dACPrediction);
                        pCompressedMB->SetMBCBPCY(CBPCY);
                        pCurMBInfo->SetEdgesIntra(i==0, j==0);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(VC1_ENC_P_MB_INTRA);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                        /*-------------------------- Reconstruction ----------------------------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            for (blk=0;blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                    DCQuant, doubleQuant);
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],blkSize, 0);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pRecMBData->PasteMBProg(pRaisedMBRow[0],pRaisedMBStep[0],pRaisedMBRow[1],pRaisedMBRow[2],pRaisedMBStep[1],j);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRaisedMBRow[0];
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0]*2;
                                smoothInfo.pRFrameU       = pRaisedMBRow[1];
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1]*2;
                                smoothInfo.pRFrameV       = pRaisedMBRow[2];
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2]*2;
                                smoothInfo.curIntra       = 0;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            {
                                STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};

                                m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                        /*------------------------------------------- ----------------------------------------------------------*/
                    }
                    break;
                case VC1_ENC_P_MB_1MV:
                case VC1_ENC_P_MB_SKIP_1MV:
                    {
                        /*--------------------------------------- Inter --------------------------------------------------*/
                        sCoordinate MVIntChroma     = {0,0};
                        sCoordinate MVQuarterChroma = {0,0};
                        sCoordinate MVChroma       =  {0,0};

                        sCoordinate *mvC=0, *mvB=0, *mvA=0;
                        sCoordinate  mv1, mv2, mv3;
                        sCoordinate  mvPred = {0,0};
                        sCoordinate  mvDiff = {0,0};
                        eTransformType              BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                                                                        m_uiTransformType, m_uiTransformType,m_uiTransformType, m_uiTransformType};

                        sCoordinate         MVInt       = {0,0};
                        sCoordinate         MVQuarter   = {0,0};

                        Ipp8u        hybridReal = 0;


                        MBType = (UMC::ME_MbFrw == MEParams->pSrc->MBs[j + i*w].MbType)?
VC1_ENC_P_MB_1MV:VC1_ENC_P_MB_SKIP_1MV;

                        MV.x        = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                        MV.y        = MEParams->pSrc->MBs[j + i*w].MV[0]->y;


                        pCurMBInfo->Init(false);
                        pCompressedMB->Init(MBType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        /*MV prediction -!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
                        GetMVPredictionPField(true);
                        PredictMVField1(mvA,mvB,mvC, &mvPred);

                        if (mvA && mvC)
                        {
                           hybridReal = HybridPrediction(&mvPred,&MV,mvA ,mvC ,th);
                        }
                        pCompressedMB->SetHybrid(hybridReal);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetPredFlag(0, 0);
                        pDebug->SetFieldMVPred1Ref(mvA, mvC, 0);
                        pDebug->SetHybrid(hybridReal, 0);
#endif
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        if (VC1_ENC_P_MB_SKIP_1MV == MBType)
                        {
                            //correct ME results
                            assert(MV.x==mvPred.x && MV.y==mvPred.y);
                            MV.x=mvPred.x;
                            MV.y=mvPred.y;
                        }
                        pCurMBInfo->SetMV(MV);
                        mvDiff.x = MV.x - mvPred.x;
                        mvDiff.y = MV.y - mvPred.y;
                        pCompressedMB->SetdMV(mvDiff);

                        if (VC1_ENC_P_MB_SKIP_1MV != MBType || bRaiseFrame)
                        {
                            /*interpolation*/

                            sCoordinate t = {MV.x, MV.y + mv_t0, 0};
                            GetIntQuarterMV(t,&MVInt, &MVQuarter);

                            CalculateChroma(MV,&MVChroma);
                            pCurMBInfo->SetMVChroma(MVChroma);
                            MVChroma.y = MVChroma.y + mv_t0;
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);

#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(t.x, t.y, 0);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 4);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 5);
#endif
                            Ipp32u ind0 = (bSecondField)? 1: 0;
                            Ipp32u ind1 = (m_uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)?0:1;
                            if (padding[ind0][ind1])
                            {

                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolation, pForwMBRow[0], pForwMBStep[0], &MVInt, &MVQuarter, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolation);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolation, &sVInterpolation, pForwMBRow[1], pForwMBRow[2],pForwMBStep[1], 
                                    &MVIntChroma, &MVQuarterChroma, j);    

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            }
                            else
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockY, j, i , &MVInt, &MVQuarter);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockY,0,0,oppositePadding[ind0][ind1],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockU, &InterpolateBlockV, 
                                                        oppositePadding[ind0][ind1],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChroma, &MVQuarterChroma);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime); 

                            }
                        } //interpolation

                        if (VC1_ENC_P_MB_SKIP_1MV != MBType)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = sYInterpolation.pDst; 
                                pIn.refYStep = sYInterpolation.dstStep;      
                                pIn.pURef    = sUInterpolation.pDst; 
                                pIn.refUStep = sUInterpolation.dstStep;
                                pIn.pVRef    = sVInterpolation.pDst; 
                                pIn.refVStep = sVInterpolation.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],pCurMBStep[0], pCurMBRow[1],pCurMBRow[2], pCurMBStep[1], 
                                                        sYInterpolation.pDst, sYInterpolation.dstStep,
                                                        sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                                                        j,0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern = pCompressedMB->GetMBPattern();
                            CBPCY = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                            if (MBPattern==0 && mvDiff.x == 0 && mvDiff.y == 0)
                            {
                                pCompressedMB->ChangeType(VC1_ENC_P_MB_SKIP_1MV);
                                MBType = VC1_ENC_P_MB_SKIP_1MV;
                            }
                        }//VC1_ENC_P_MB_SKIP_1MV != MBType
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(MBType);
                        pDebug->SetMVInfoField(&MV,  mvPred.x,mvPred.y, 0);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 4);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);

                        if (pCompressedMB->isSkip() )
                        {
                            pDebug->SetMBAsSkip();
                        }
                        else
                        {
                            pDebug->SetCPB(MBPattern, CBPCY);
                            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        }
#endif
                        /*---------------------------Reconstruction ------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                            sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                            pRaisedMBRow[0], pRaisedMBStep[0], 
                                                            pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                                            0,j);

                               IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                                sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                                pRaisedMBRow[0], pRaisedMBStep[0], 
                                                                pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],0,j);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRaisedMBRow[0];
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0]*2;
                                smoothInfo.pRFrameU       = pRaisedMBRow[1];
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1]*2;
                                smoothInfo.pRFrameV       = pRaisedMBRow[2];
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2]*2;
                                smoothInfo.curIntra       = 0;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                                pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                                pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                                pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                        Ipp32u ind0 = (bSecondField)? 1: 0;
                        Ipp32u ind1 = (m_uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)?0:1;

                        pDebug->SetInterpInfo(&sYInterpolation, &sUInterpolation, &sVInterpolation, 0, padding[ind0][ind1]);
#endif
                    }
                    break;
                case VC1_ENC_P_MB_4MV:
                case VC1_ENC_P_MB_SKIP_4MV:
                    {
                        /*--------------------------------------- Inter --------------------------------------------------*/
                        sCoordinate         MVInt;     
                        sCoordinate         MVQuarter; 
                        sCoordinate         pMV [4]         = {0};

                        sCoordinate         MVIntChroma             = {0,0};
                        sCoordinate         MVQuarterChroma         = {0,0};
                        sCoordinate         MVChroma                = {0,0};

                        sCoordinate         mvPred[4]= {0};

                        sCoordinate         mvDiff;
                        eTransformType      BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType};
                        bool                MV_Second = (m_uiReferenceFieldType == VC1_ENC_REF_FIELD_FIRST)?0:1;

                        pCurMBInfo->Init(false);
                        pCompressedMB->Init(MBType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);

                        for (blk = 0; blk <4; blk++)
                        {
                            Ipp32u blkPosX = (blk&0x1)<<3;
                            Ipp32u blkPosY = (blk/2)<<3;

                            Ipp8u               hybridReal = 0; 

                            sCoordinate         *mvC=0, *mvB=0, *mvA=0;
                            sCoordinate         mv1, mv2, mv3;

                            pMV[blk].x         = MEParams->pSrc->MBs[j + i*w].MV[0][blk].x;
                            pMV[blk].y         = MEParams->pSrc->MBs[j + i*w].MV[0][blk].y;
                            pMV[blk].bSecond   = MV_Second;

                            GetMVPredictionPBlk_F(blk);
                            PredictMVField1(mvA,mvB,mvC, &mvPred[blk]);

                            //ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMBMin,MVPredMBMax);
                            if (mvA && mvC)
                            {
                                hybridReal = HybridPrediction(&(mvPred[blk]),&pMV[blk],mvA ,mvC ,th);
                                
                            } // mvA && mvC
                            pCompressedMB->SetHybrid(hybridReal, blk);
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetMVInfo(&pMV[blk], mvPred[blk].x,mvPred[blk].y, 0, blk);
                            pDebug->SetFieldMVPred1Ref( mvA, mvC, 0, blk);
                            pDebug->SetHybrid(hybridReal, 0, blk);
#endif

                            STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                            if (VC1_ENC_P_MB_SKIP_4MV == MBType)
                            {
                                //correct ME results
                                VM_ASSERT(pMV[blk].x==mvPred[blk].x && pMV[blk].y==mvPred[blk].y);
                                pMV[blk].x=mvPred[blk].x;
                                pMV[blk].y=mvPred[blk].y;
                            }
                            pCurMBInfo->SetMV_F(pMV[blk], blk);

                            mvDiff.x = pMV[blk].x - mvPred[blk].x;
                            mvDiff.y = pMV[blk].y - mvPred[blk].y;
                            mvDiff.bSecond = 0;
                            pCompressedMB->SetBlockdMV_F(mvDiff, blk);

                            if (VC1_ENC_P_MB_SKIP_4MV != MBType || bRaiseFrame)
                            {
                                sCoordinate t = {pMV[blk].x,pMV[blk].y + ((!MV_Second)? (2-4*(!bBottom)):0),MV_Second};
                                GetIntQuarterMV(t,&MVInt, &MVQuarter);
#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetIntrpMV(t.x, t.y, 0, blk);
#endif
                                Ipp32u ind = (bSecondField)? 1: 0;
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                if (padding[ind][MV_Second])
                                {
                                    SetInterpolationParamsLumaBlk(&sYInterpolationBlk[blk], pForwMBRow[0], pForwMBStep[0], &MVInt, &MVQuarter, j,blk);

                                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                    ippSts = InterpolateLumaFunction(&sYInterpolationBlk[blk]);
                                    VC1_ENC_IPP_CHECK(ippSts);
                                    
                                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                    
                                }
                                else
                                {
                                    SetInterpolationParamsLumaBlk(&InterpolateBlockYBlk[blk], j, i, &MVInt, &MVQuarter,blk);
                                    
                                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);                        
                                    ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYBlk[blk],0,0,oppositePadding[ind][MV_Second],true,m_uiRoundControl,bBottom);
                                    if (ippSts != ippStsNoErr)
                                        return UMC::UMC_ERR_OPEN_FAILED;
                                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);                        
                                }
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            } //interpolation
                        } // for (luma blocks

                        pCurMBInfo->GetLumaVectorFields(&MV, true);

                        /*-------------------------- interpolation for chroma---------------------------------*/

                        if (VC1_ENC_P_MB_SKIP_4MV != MBType || bRaiseFrame)
                        {                       
                            CalculateChroma(MV,&MVChroma);
                            pCurMBInfo->SetMVChroma(MVChroma);
                            MVChroma.y = MVChroma.y + ((!MV.bSecond)? (2-4*(!bBottom)):0);
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);

                            Ipp32u ind = (bSecondField)? 1: 0;

                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            if (padding[ind][MV.bSecond])
                            {   
#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 4);
                                pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, 0, 5);
#endif
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                InterpolateChroma (&sUInterpolation, &sVInterpolation, pForwMBRow[1], pForwMBRow[2],pForwMBStep[1], 
                                                   &MVIntChroma, &MVQuarterChroma, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            else
                            {    
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                InterpolateChromaPad   (&InterpolateBlockU, &InterpolateBlockV, 
                                                        oppositePadding[ind][MV.bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChroma, &MVQuarterChroma);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);                /////////////////////////////////////////////
                        }
                        /////////////////////////////////////////////////////////////////////////////////////////////////////////////
                        if (VC1_ENC_P_MB_SKIP_4MV != MBType)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = sYInterpolation.pDst; 
                                pIn.refYStep = sYInterpolation.dstStep;      
                                pIn.pURef    = sUInterpolation.pDst; 
                                pIn.refUStep = sUInterpolation.dstStep;
                                pIn.pVRef    = sVInterpolation.pDst; 
                                pIn.refVStep = sVInterpolation.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],pCurMBStep[0], pCurMBRow[1],pCurMBRow[2], pCurMBStep[1], 
                                                        sYInterpolation.pDst, sYInterpolation.dstStep,
                                                        sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                                                        j,0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern = pCompressedMB->GetMBPattern();
                            CBPCY = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);

                        }//VC1_ENC_P_MB_SKIP_1MV != MBType
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(MBType);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 4);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, 0, 5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);

                        if (pCompressedMB->isSkip() )
                        {
                            pDebug->SetMBAsSkip();
                        }
                        else
                        {
                            pDebug->SetCPB(MBPattern, CBPCY);
                            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        }
#endif
                        /*---------------------------Reconstruction ------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                    sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                    pRaisedMBRow[0], pRaisedMBStep[0], 
                                    pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                    0,j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                                sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                                pRaisedMBRow[0], pRaisedMBStep[0], 
                                                                pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],0,j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRaisedMBRow[0];
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0]*2;
                                smoothInfo.pRFrameU       = pRaisedMBRow[1];
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1]*2;
                                smoothInfo.pRFrameV       = pRaisedMBRow[2];
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2]*2;
                                smoothInfo.curIntra       = 0;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                                pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                                pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                                pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                        Ipp32u ind = (bSecondField)? 1: 0;
                        pDebug->SetInterpInfo(&sYInterpolation, &sUInterpolation, &sVInterpolation, 0, padding[ind][MV.bSecond]);
#endif
                    }
                    break;
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
                }

                if (m_pSavedMV && pRefType)
                {
                    *(pSavedMV ++) = MV.x;
                    *(pSavedMV ++) = MV.y;
                    pRefType ++;
                }

#ifdef VC1_ENC_DEBUG_ON
                if(!bSubBlkTS)
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(), 15, 15);
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(), 15, 15);
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(),15);
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), 15);
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), 15);
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), 15);
                }
                else
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(),
                        pCurMBInfo->GetLumaAdUppEdge(), pCurMBInfo->GetLumaAdBotEdge() );
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(),
                        pCurMBInfo->GetLumaAdLefEdge(), pCurMBInfo->GetLumaAdRigEdge());
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(), pCurMBInfo->GetUAdHorEdge());
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), pCurMBInfo->GetVAdHorEdge());
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), pCurMBInfo->GetUAdVerEdge());
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), pCurMBInfo->GetVAdVerEdge());
                }
#endif

                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }

            deblkPattern = (deblkPattern | 0x4 | ( (! (Ipp8u)((i + 1 - (hMB -1)) >> 31)<<3))) & deblkMask;
            smoothPattern = 0x6 & smoothMask;

            //Row deblocking
            //STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            //        if (m_pSequenceHeader->IsLoopFilter() && bRaiseFrame && (i != firstRow || firstRow == hMB-1))
            //        {
            //STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
            //           Ipp8u *planes[3] = {0};
            //           err = SetFieldPlane(planes, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, i);
            //           VC1_ENC_CHECK(err);
            //
            //            Ipp32u step[3] = {0};
            //            SetFieldStep(step, m_uiPlaneStep);
            //            VC1_ENC_CHECK(err);
            //
            //            m_pMBs->StartRow();
            //            if(bSubBlkTS)
            //            {
            //                if(i < hMB - 1)
            //                    Deblock_P_RowVts(planes, step, w, m_uiQuant, m_pMBs);
            //                else
            //                  if(firstRow != hMB -1)
            //                    Deblock_P_BottomRowVts(planes, step, w, m_uiQuant, m_pMBs);
            //                  else
            //                    Deblock_P_TopBottomRowVts(planes, step, w, m_uiQuant, m_pMBs);
            //            }
            //            else
            //            {
            //                if(i < hMB - 1)
            //                    Deblock_P_RowNoVts(planes, step, w, m_uiQuant, m_pMBs);
            //                else
            //                  if(firstRow != hMB -1)
            //                    Deblock_P_BottomRowNoVts(planes, step, w, m_uiQuant, m_pMBs);
            //                  else
            //                    Deblock_P_BottomRowNoVts(planes, step, w, m_uiQuant, m_pMBs);
            //            }
            //STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            //        }
            //STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;

            pCurMBRow[0]+= pCurMBStep[0]*VC1_ENC_LUMA_SIZE;
            pCurMBRow[1]+= pCurMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pCurMBRow[2]+= pCurMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pRaisedMBRow[0]+= pRaisedMBStep[0]*VC1_ENC_LUMA_SIZE;
            pRaisedMBRow[1]+= pRaisedMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pRaisedMBRow[2]+= pRaisedMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pForwMBRow[0]+= pForwMBStep[0]*VC1_ENC_LUMA_SIZE;
            pForwMBRow[1]+= pForwMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pForwMBRow[2]+= pForwMBStep[2]*VC1_ENC_CHROMA_SIZE;

        }

#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame && bSecondField && i == h)
            pDebug->PrintRestoredFrame( m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
            m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
            m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
#endif


        return err;
    }

    UMC::Status VC1EncoderPictureADV::VLC_I_Frame(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status err = UMC::UMC_OK;
        Ipp32s   i=0, j=0, blk = 0;
        Ipp32s   h = (m_pSequenceHeader->GetPictureHeight()+15)/16;
        Ipp32s   w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s   hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        eCodingSet   LumaCodingSet   = LumaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC2];
        eCodingSet   ChromaCodingSet = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet* pACTablesVLC[6] = {&ACTablesSet[LumaCodingSet],    &ACTablesSet[LumaCodingSet],
            &ACTablesSet[LumaCodingSet],    &ACTablesSet[LumaCodingSet],
            &ACTablesSet[ChromaCodingSet],  &ACTablesSet[ChromaCodingSet]};

        const Ipp32u* pDCTableVLC[6]  = {DCTables[m_uiDecTypeDCIntra][0],    DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],    DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],    DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        bool Overlap = (m_uiCondOverlap == VC1_ENC_COND_OVERLAP_SOME) && (m_uiQuant <= 8);

#ifdef VC1_ENC_DEBUG_ON
        pDebug->SetCurrMB(false, 0, firstRow);
        //pDebug->SetCurrMBFirst();
#endif

        //Picture header
        //err = WriteIPictureHeader(pCodedPicture);
        //if (err != UMC::UMC_OK)
        //     return err;

        //MB coded data
        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);
#ifdef VC1_ME_MB_STATICTICS
                {
                    m_MECurMbStat->MbType = UMC::ME_MbIntra;
                    m_MECurMbStat->whole = 0;
                    memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                    m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;

                    pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
                }
#endif

                err = pCompressedMB->WriteMBHeaderI_ADV(pCodedPicture, m_bRawBitplanes, Overlap);
                if (err != UMC::UMC_OK)
                    return err;

                STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                for (blk = 0; blk<6; blk++)
                {
                    err =pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLC[blk],m_uiQuant,blk);
                    VC1_ENC_CHECK (err)

                        err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesVLC[blk],&ACEscInfo,blk);
                    VC1_ENC_CHECK (err)
                }//for
                STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)
            return err;
    }
    UMC::Status VC1EncoderPictureADV::VLC_IField(VC1EncoderBitStreamAdv* pCodedPicture, bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status err = UMC::UMC_OK;
        Ipp32s      i=0, j=0, blk = 0;
        Ipp32s      h   = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s      w   = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        VC1EncoderCodedMB* pCodedMB = m_pCodedMB + w*h*bSecondField;

        eCodingSet   LumaCodingSet   = LumaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC2];
        eCodingSet   ChromaCodingSet = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet* pACTablesVLC[6] = {&ACTablesSet[LumaCodingSet],    &ACTablesSet[LumaCodingSet],
            &ACTablesSet[LumaCodingSet],    &ACTablesSet[LumaCodingSet],
            &ACTablesSet[ChromaCodingSet],  &ACTablesSet[ChromaCodingSet]};

        const Ipp32u* pDCTableVLC[6]  = {DCTables[m_uiDecTypeDCIntra][0],    DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],    DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],    DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        bool Overlap = (m_uiCondOverlap == VC1_ENC_COND_OVERLAP_SOME) && (m_uiQuant <= 8);

#ifdef VC1_ENC_DEBUG_ON
        Ipp32u      fieldShift = (bSecondField)? h : 0;
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
#endif

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);
#ifdef VC1_ME_MB_STATICTICS
                {
                    m_MECurMbStat->MbType = UMC::ME_MbIntra;
                    m_MECurMbStat->whole = 0;
                    memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                    m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;

                    pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
                }
#endif
                err = pCompressedMB->WriteMBHeaderI_ADV(pCodedPicture, m_bRawBitplanes, Overlap);
                if (err != UMC::UMC_OK)
                    return err;

                STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                for (blk = 0; blk<6; blk++)
                {
                    err =pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLC[blk],m_uiQuant,blk);
                    VC1_ENC_CHECK (err)

                        err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesVLC[blk],&ACEscInfo,blk);
                    VC1_ENC_CHECK (err)
                }//for
                STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)

            return err;
    }

    UMC::Status VC1EncoderPictureADV::SetInterpolationParams (  IppVCInterpolateBlock_8u* pY,
                                                                IppVCInterpolateBlock_8u* pU,
                                                                IppVCInterpolateBlock_8u* pV,
                                                                Ipp8u* buffer,
                                                                Ipp32u w,
                                                                Ipp32u h,
                                                                Ipp8u **pPlane,
                                                                Ipp32u *pStep,
                                                                bool bField)
    {
        UMC::Status ret = UMC::UMC_OK;
        Ipp32u shift   = (bField)? 1:0;

        pY->pSrc [0]            = pPlane[0];
        pY->srcStep             = pStep[0] >> shift;
        pY->pDst[0]             = buffer;
        pY->dstStep             = VC1_ENC_LUMA_SIZE;
        pV->pointVector.x       = 0;
        pV->pointVector.y       = 0;
        pY->sizeBlock.height    = VC1_ENC_LUMA_SIZE;
        pY->sizeBlock.width     = VC1_ENC_LUMA_SIZE;
        pY->sizeFrame.width     = w;
        pY->sizeFrame.height    = h;

        pU->pSrc[0]            = pPlane[1];
        pU->srcStep            = pStep[1]>>shift;
        pU->pDst[0]            = pY->pDst[0] + VC1_ENC_LUMA_SIZE*VC1_ENC_LUMA_SIZE;
        pU->dstStep            = (VC1_ENC_CHROMA_SIZE) << 1;
        pV->pointVector.x      = 0;
        pV->pointVector.y      = 0;
        pU->sizeBlock.height   = VC1_ENC_CHROMA_SIZE;
        pU->sizeBlock.width    = VC1_ENC_CHROMA_SIZE;
        pU->sizeFrame.width    = w >> 1;
        pU->sizeFrame.height   = h >> 1;

        pV->pSrc[0]             = pPlane[2];
        pV->srcStep             = pStep[2]>>shift;
        pV->pDst[0]             = pU->pDst[0] + VC1_ENC_CHROMA_SIZE;
        pV->dstStep             = pU->dstStep;
        pV->pointVector.x       = 0;
        pV->pointVector.y       = 0;
        pV->sizeBlock.height    = VC1_ENC_CHROMA_SIZE;
        pV->sizeBlock.width     = VC1_ENC_CHROMA_SIZE;
        pV->sizeFrame.width     = w >> 1;
        pV->sizeFrame.height    = h >> 1;

        return ret;
    }
    UMC::Status VC1EncoderPictureADV::SetInterpolationParams4MV (   IppVCInterpolateBlock_8u* pY,
        Ipp8u* buffer,
        Ipp32u w,
        Ipp32u h,
        Ipp8u **pPlane,
        Ipp32u *pStep,
        bool bField)
    {
        UMC::Status ret = UMC::UMC_OK;
        Ipp32u lumaShift   = (bField)? 1:0;


        pY[0].pSrc [0]           = pPlane[0];
        pY[0].srcStep            = pStep[0]>>lumaShift;
        pY[0].pDst[0]            = buffer;
        pY[0].dstStep            = VC1_ENC_LUMA_SIZE;
        pY[0].pointVector.x       = 0;
        pY[0].pointVector.y       = 0;
        pY[0].sizeBlock.height    = 8;
        pY[0].sizeBlock.width     = 8;
        pY[0].sizeFrame.width     = w;
        pY[0].sizeFrame.height    = h ;

        pY[1].pSrc [0]           = pPlane[0];
        pY[1].srcStep            = pStep[0]>>lumaShift;
        pY[1].pDst[0]            = buffer + 8;
        pY[1].dstStep            = VC1_ENC_LUMA_SIZE;
        pY[1].pointVector.x       = 0;
        pY[1].pointVector.y       = 0;
        pY[1].sizeBlock.height    = 8;
        pY[1].sizeBlock.width     = 8;
        pY[1].sizeFrame.width     = w ;
        pY[1].sizeFrame.height    = h ;

        pY[2].pSrc [0]           = pPlane[0];
        pY[2].srcStep            = pStep[0]>>lumaShift;
        pY[2].pDst[0]            = buffer + VC1_ENC_LUMA_SIZE*8;
        pY[2].dstStep            = VC1_ENC_LUMA_SIZE;
        pY[2].pointVector.x       = 0;
        pY[2].pointVector.y       = 0;
        pY[2].sizeBlock.height    = 8;
        pY[2].sizeBlock.width     = 8;
        pY[2].sizeFrame.width     = w;
        pY[2].sizeFrame.height    = h;

        pY[3].pSrc [0]           = pPlane[0];
        pY[3].srcStep            = pStep[0]>>lumaShift;
        pY[3].pDst[0]            = buffer + VC1_ENC_LUMA_SIZE*8 + 8;
        pY[3].dstStep            = VC1_ENC_LUMA_SIZE;
        pY[3].pointVector.x       = 0;
        pY[3].pointVector.y       = 0;
        pY[3].sizeBlock.height    = 8;
        pY[3].sizeBlock.width     = 8;
        pY[3].sizeFrame.width     = w;
        pY[3].sizeFrame.height    = h;


        return ret;
    }
    UMC::Status VC1EncoderPictureADV::SetInterpolationParams(IppVCInterpolate_8u* pY,
                                                             IppVCInterpolate_8u* pU,
                                                             IppVCInterpolate_8u* pV,
                                                             Ipp8u* buffer,
                                                             bool bForward,
                                                             bool bField)
    {
        UMC::Status ret = UMC::UMC_OK;
        Ipp8u **pPlane;
        Ipp32u *pStep;
        Ipp8u n = (bField)? 1:0;


        if (bForward)
        {
            pPlane = m_pForwardPlane;
            pStep  = m_uiForwardPlaneStep;
        }
        else
        {
            pPlane = m_pBackwardPlane;
            pStep  = m_uiBackwardPlaneStep;
        }

        pY->pSrc            = pPlane[0];
        pY->srcStep         = pStep[0]<<n;
        pY->pDst            = buffer;
        pY->dstStep         = VC1_ENC_LUMA_SIZE;
        pY->dx              = 0;
        pY->dy              = 0;
        pY->roundControl    = m_uiRoundControl;
        pY->roiSize.height  = VC1_ENC_LUMA_SIZE;
        pY->roiSize.width   = VC1_ENC_LUMA_SIZE;

        pU->pSrc            = pPlane[1];
        pU->srcStep         = pStep[1]<<n;
        pU->pDst            = pY->pDst + VC1_ENC_LUMA_SIZE*VC1_ENC_LUMA_SIZE;
        pU->dstStep         = (VC1_ENC_CHROMA_SIZE)<<1;
        pU->dx              = 0;
        pU->dy              = 0;
        pU->roundControl    = m_uiRoundControl;
        pU->roiSize.height  = VC1_ENC_CHROMA_SIZE;
        pU->roiSize.width   = VC1_ENC_CHROMA_SIZE;

        pV->pSrc            = pPlane[2];
        pV->srcStep         = pStep[2]<<n;
        pV->pDst            = pU->pDst + VC1_ENC_CHROMA_SIZE;
        pV->dstStep         = pU->dstStep;
        pV->dx              = 0;
        pV->dy              = 0;
        pV->roundControl    = m_uiRoundControl;
        pV->roiSize.height  = VC1_ENC_CHROMA_SIZE;
        pV->roiSize.width   = VC1_ENC_CHROMA_SIZE;

        return ret;
    }

    //motion estimation
    UMC::Status  VC1EncoderPictureADV::SetInterpolationParams4MV( IppVCInterpolate_8u* pY,
        IppVCInterpolate_8u* pU,
        IppVCInterpolate_8u* pV,
        Ipp8u* buffer,
        bool bForward,
        bool bField)
    {
        UMC::Status ret = UMC::UMC_OK;
        Ipp8u **pPlane;
        Ipp32u *pStep;
        Ipp32u Shift = (bField)? 1:0;


        if (bForward)
        {
            pPlane = m_pForwardPlane;
            pStep  = m_uiForwardPlaneStep;
        }
        else
        {
            pPlane = m_pBackwardPlane;
            pStep  = m_uiBackwardPlaneStep;
        }

        pY[0].pSrc            = pPlane[0];
        pY[0].srcStep         = pStep[0] <<Shift;
        pY[0].pDst            = buffer;
        pY[0].dstStep         = VC1_ENC_LUMA_SIZE;
        pY[0].dx              = 0;
        pY[0].dy              = 0;
        pY[0].roundControl    = m_uiRoundControl;
        pY[0].roiSize.height  = 8;
        pY[0].roiSize.width   = 8;

        pY[1].pSrc            = pPlane[0];
        pY[1].srcStep         = pStep[0]<<Shift;
        pY[1].pDst            = buffer+8;
        pY[1].dstStep         = VC1_ENC_LUMA_SIZE;
        pY[1].dx              = 0;
        pY[1].dy              = 0;
        pY[1].roundControl    = m_uiRoundControl;
        pY[1].roiSize.height  = 8;
        pY[1].roiSize.width   = 8;

        pY[2].pSrc            = pPlane[0];
        pY[2].srcStep         = pStep[0]<<Shift;
        pY[2].pDst            = buffer+8*VC1_ENC_LUMA_SIZE;
        pY[2].dstStep         = VC1_ENC_LUMA_SIZE;
        pY[2].dx              = 0;
        pY[2].dy              = 0;
        pY[2].roundControl    = m_uiRoundControl;
        pY[2].roiSize.height  = 8;
        pY[2].roiSize.width   = 8;

        pY[3].pSrc            = pPlane[0];
        pY[3].srcStep         = pStep[0]<<Shift;
        pY[3].pDst            = buffer+8*VC1_ENC_LUMA_SIZE+8;
        pY[3].dstStep         = VC1_ENC_LUMA_SIZE;
        pY[3].dx              = 0;
        pY[3].dy              = 0;
        pY[3].roundControl    = m_uiRoundControl;
        pY[3].roiSize.height  = 8;
        pY[3].roiSize.width   = 8;

        pU->pSrc            = pPlane[1];
        pU->srcStep         = pStep[1]<<Shift;
        pU->pDst            = buffer + VC1_ENC_LUMA_SIZE*VC1_ENC_LUMA_SIZE;
        pU->dstStep         = VC1_ENC_CHROMA_SIZE << 1;
        pU->dx              = 0;
        pU->dy              = 0;
        pU->roundControl    = m_uiRoundControl;
        pU->roiSize.height  = VC1_ENC_CHROMA_SIZE;
        pU->roiSize.width   = VC1_ENC_CHROMA_SIZE;

        pV->pSrc            = pPlane[2];
        pV->srcStep         = pStep[2]<<Shift;
        pV->pDst            = pU->pDst + VC1_ENC_CHROMA_SIZE;
        pV->dstStep         = pU->dstStep;
        pV->dx              = 0;
        pV->dy              = 0;
        pV->roundControl    = m_uiRoundControl;
        pV->roiSize.height  = VC1_ENC_CHROMA_SIZE;
        pV->roiSize.width   = VC1_ENC_CHROMA_SIZE;

        return ret;
    }

    UMC::Status VC1EncoderPictureADV::PAC_PFrame(UMC::MeParams* MEParams, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;

        Ipp32s                      h = (m_pSequenceHeader->GetPictureHeight()+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth()+15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);

        Ipp8u*                      pCurMBRow[3] = {m_pPlane[0]+ firstRow*m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE,
                                                    m_pPlane[1]+ firstRow*m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
                                                    m_pPlane[2]+ firstRow*m_uiPlaneStep[2]*VC1_ENC_CHROMA_SIZE};

        Ipp8u*                      pFMBRow  [3] = {m_pForwardPlane[0]+ firstRow*m_uiForwardPlaneStep[0]*VC1_ENC_LUMA_SIZE, 
                                                    m_pForwardPlane[1]+ firstRow*m_uiForwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE, 
                                                    m_pForwardPlane[2]+ firstRow*m_uiForwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE};

        //forward transform quantization

        _SetIntraInterLumaFunctions;

        //inverse transform quantization
        IntraInvTransformQuantFunction IntraInvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :
            IntraInvTransformQuantNonUniform;

        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (m_bUniformQuant) ? InterInvTransformQuantUniform :
            InterInvTransformQuantNonUniform;
        CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())? GetChromaMVFast:GetChromaMV;

        bool                        bIsBilinearInterpolation = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR);
        InterpolateFunction         InterpolateLumaFunction  = (bIsBilinearInterpolation)? _own_ippiInterpolateQPBilinear_VC1_8u_C1R:_own_ippiInterpolateQPBicubic_VC1_8u_C1R;

        InterpolateFunctionPadding  InterpolateLumaFunctionPadding = (bIsBilinearInterpolation)? own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R: own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;



        Ipp8u                       tempInterpolationBuffer[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];

        IppVCInterpolate_8u         sYInterpolation;
        IppVCInterpolate_8u         sUInterpolation;
        IppVCInterpolate_8u         sVInterpolation;

        IppVCInterpolateBlock_8u    InterpolateBlockY;
        IppVCInterpolateBlock_8u    InterpolateBlockU;
        IppVCInterpolateBlock_8u    InterpolateBlockV;

        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
        bool                        dACPrediction   = true;

        IppiSize                    blkSize     = {8,8};
        IppiSize                    blkSizeLuma = {16,16};

        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];

        Ipp16s*                     pSavedMV = (m_pSavedMV)? m_pSavedMV + w*firstRow*2:0;

        sCoordinate                 MVPredMBMin = {-60,-60};
        sCoordinate                 MVPredMBMax = {(Ipp16s)w*16*4-4, (Ipp16s)h*16*4-4};

        bool                        bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR || m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

        bool                        bSubBlkTS           = m_pSequenceHeader->IsVSTransform() && (!(m_bVSTransform &&m_uiTransformType==VC1_ENC_8x8_TRANSFORM));
        fGetExternalEdge            pGetExternalEdge    = GetExternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS
        fGetInternalEdge            pGetInternalEdge    = GetInternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS

        Ipp8u deblkPattern = (firstRow != hMB - 1)? 0 : 0x8;   //4 bits: right 1 bit - 0 - left/1 - not left,
        //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row, 10 - top bottom row
        //middle 1 bit - 1 - right/0 - not right
        Ipp8u deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFC : 0;

        Ipp8u smoothMask = (m_pSequenceHeader->IsOverlap() && (m_uiQuant >= 9)) ? 0xFF : 0;
        Ipp8u smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
        //0/1 - top/not top, 0/1 - left/not left

        SmoothInfo_P_Adv smoothInfo = {0};


        IppStatus                   ippSts  = ippStsNoErr;

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst();
        pDebug->SetCurrMB(false, 0, firstRow);
        pDebug->SetSliceInfo(firstRow, firstRow + nRows);
        pDebug->SetInterpolType(bIsBilinearInterpolation);
        pDebug->SetRounControl(m_uiRoundControl);
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
        pDebug->SetVTSFlag(bSubBlkTS);
#endif
        if (pSavedMV)
        {
            memset(pSavedMV,0,w*(hMB - firstRow)*2*sizeof(Ipp16s));
        }


        err = m_pMBs->Reset();
        if (err != UMC::UMC_OK)
            return err;
        SetInterpolationParams(&sYInterpolation,&sUInterpolation,&sVInterpolation,
            tempInterpolationBuffer,true, false);

        SetInterpolationParams(&InterpolateBlockY,&InterpolateBlockU,&InterpolateBlockV, tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            m_pForwardPlane, m_uiForwardPlaneStep, false);
#ifdef VC1_ENC_CHECK_MV
        err = CheckMEMV_P(MEParams,bMVHalf,firstRow,nRows);
        assert(err == UMC::UMC_OK);
#endif



        /* -------------------------------------------------------------------------*/
        for (i = firstRow; i < hMB; i++)
        {
            Ipp8u *pRFrameY = 0;
            Ipp8u *pRFrameU = 0;
            Ipp8u *pRFrameV = 0;

            if (bRaiseFrame)
            {
                pRFrameY = m_pRaisedPlane[0]+VC1_ENC_LUMA_SIZE*(i*m_uiRaisedPlaneStep[0]);
                pRFrameU = m_pRaisedPlane[1]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[1]);
                pRFrameV = m_pRaisedPlane[2]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[2]);
            }
            for (j=0; j < w; j++)
            {

                Ipp32s              xLuma        =  VC1_ENC_LUMA_SIZE*j;
                Ipp32s              xChroma      =  VC1_ENC_CHROMA_SIZE*j;

                //Ipp32s              posY        =  VC1_ENC_LUMA_SIZE*i;
                sCoordinate         MVInt       = {0,0};
                sCoordinate         MVQuarter   = {0,0};
                sCoordinate         MV          = {0,0};
                Ipp8u               MBPattern   = 0;
                Ipp8u               CBPCY       = 0;

                VC1EncoderMBInfo  * pCurMBInfo  = m_pMBs->GetCurrMBInfo();
                VC1EncoderMBData  * pCurMBData  = m_pMBs->GetCurrMBData();
                VC1EncoderMBData  * pRecMBData  = m_pMBs->GetRecCurrMBData();

                VC1EncoderMBInfo* left        = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* topLeft     = m_pMBs->GetTopLeftMBInfo();
                VC1EncoderMBInfo* top         = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* topRight    = m_pMBs->GetTopRightMBInfo();
                VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);
                eMBType MBType;

                switch (MEParams->pSrc->MBs[j + i*w].MbType)
                {
                case UMC::ME_MbIntra:
                    {
                        NeighbouringMBsData MBs;

                        MBs.LeftMB    = ((left)? left->isIntra():0)         ? m_pMBs->GetLeftMBData():0;
                        MBs.TopMB     = ((top)? top->isIntra():0)           ? m_pMBs->GetTopMBData():0;
                        MBs.TopLeftMB = ((topLeft)? topLeft->isIntra():0)   ? m_pMBs->GetTopLeftMBData():0;
                        MV.x = 0;
                        MV.y =0;
                        MBType = VC1_ENC_P_MB_INTRA;
                        pCompressedMB->Init(VC1_ENC_P_MB_INTRA);
                        pCurMBInfo->Init(true);

                        /*-------------------------- Compression ----------------------------------------------------------*/

                        pCurMBData->CopyMBProg(pCurMBRow[0],m_uiPlaneStep[0],pCurMBRow[1], pCurMBRow[2], m_uiPlaneStep[1],j);

                        //only intra blocks:
                        for (blk = 0; blk<6; blk++)
                        {

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                            _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                        }

                        STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                        dACPrediction = DCACPredictionFrame( pCurMBData,&MBs,
                            pRecMBData, 0,direction);
                        STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                        for (blk=0; blk<6; blk++)
                        {
                            pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                pRecMBData->m_uiBlockStep[blk],
                                VC1_Inter_8x8_Scan,
                                blk);
                        }

                        MBPattern = pCompressedMB->GetMBPattern();
                        CBPCY = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetACPrediction(dACPrediction);
                        pCompressedMB->SetMBCBPCY(CBPCY);
                        pCurMBInfo->SetEdgesIntra(i==0, j==0);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(VC1_ENC_P_MB_INTRA);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                        /*-------------------------- Reconstruction ----------------------------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            for (blk=0;blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                    DCQuant, doubleQuant);
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],blkSize, 0);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pRecMBData->PasteMBProg(pRFrameY,m_uiRaisedPlaneStep[0],pRFrameU,pRFrameV,m_uiRaisedPlaneStep[1],j);
                           
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);


                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRFrameY;
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
                                smoothInfo.pRFrameU       = pRFrameU;
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
                                smoothInfo.pRFrameV       = pRFrameV;
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];
                                smoothInfo.curIntra       = 0x3F;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            {
                                STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                Ipp8u *pDblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};

                                m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                        /*------------------------------------------- ----------------------------------------------------------*/
                    }
                    break;
                case UMC::ME_MbFrw:
                case UMC::ME_MbFrwSkipped:
                    {
                        /*--------------------------------------- Inter --------------------------------------------------*/
                        sCoordinate MVIntChroma     = {0,0};
                        sCoordinate MVQuarterChroma = {0,0};
                        sCoordinate MVChroma       =  {0,0};

                        sCoordinate *mvC=0, *mvB=0, *mvA=0;
                        sCoordinate  mv1, mv2, mv3;
                        sCoordinate  mvPred;
                        sCoordinate  mvDiff;
                        eTransformType              BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                                                                        m_uiTransformType, m_uiTransformType,
                                                                        m_uiTransformType, m_uiTransformType};

                        Ipp8u                       hybrid = 0;
                        MBType = (UMC::ME_MbFrw == MEParams->pSrc->MBs[j + i*w].MbType)? VC1_ENC_P_MB_1MV:VC1_ENC_P_MB_SKIP_1MV;

                        MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                        MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                        pCurMBInfo->Init(false);
                        pCompressedMB->Init(MBType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        /*MV prediction */
                        GetMVPredictionP(true);
                        PredictMV(mvA,mvB,mvC, &mvPred);
                        ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMBMin,MVPredMBMax);
                        hybrid = HybridPrediction(&mvPred,&MV,mvA,mvC,32);
                        pCompressedMB->SetHybrid(hybrid);
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        if (VC1_ENC_P_MB_SKIP_1MV == MBType)
                        {
                            //correct ME results
                            VM_ASSERT(MV.x==mvPred.x && MV.y==mvPred.y);
                            MV.x=mvPred.x;
                            MV.y=mvPred.y;
                        }
                        pCurMBInfo->SetMV(MV);
                        GetIntQuarterMV(MV,&MVInt, &MVQuarter);

                        if (VC1_ENC_P_MB_SKIP_1MV != MBType || bRaiseFrame)
                        {
                            /*interpolation*/
                            CalculateChroma(MV,&MVChroma);
                            pCurMBInfo->SetMVChroma(MVChroma);
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);
                            if (m_uiForwardPlanePadding)
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolation, pFMBRow[0], m_uiForwardPlaneStep[0], &MVInt, &MVQuarter, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolation);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolation, &sVInterpolation, pFMBRow[1], pFMBRow[2],m_uiForwardPlaneStep[1], 
                                                    &MVIntChroma, &MVQuarterChroma, j);
  
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            }
                            else
                            {
                                bool bOpposite = (m_bForwardInterlace)? 1:0;

                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockY, j, i , &MVInt, &MVQuarter);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockY,0,0,bOpposite,false,m_uiRoundControl,0);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockU, &InterpolateBlockV, 
                                                        bOpposite,false,m_uiRoundControl,0,
                                                        j, i ,  &MVIntChroma, &MVQuarterChroma);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);              

                            }
                        } //interpolation

                        if (VC1_ENC_P_MB_SKIP_1MV != MBType)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = sYInterpolation.pDst; 
                                pIn.refYStep = sYInterpolation.dstStep;      
                                pIn.pURef    = sUInterpolation.pDst; 
                                pIn.refUStep = sUInterpolation.dstStep;
                                pIn.pVRef    = sVInterpolation.pDst; 
                                pIn.refVStep = sVInterpolation.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 0;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            mvDiff.x = MV.x - mvPred.x;
                            mvDiff.y = MV.y - mvPred.y;
                            pCompressedMB->SetdMV(mvDiff);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],m_uiPlaneStep[0], pCurMBRow[1],pCurMBRow[2], m_uiPlaneStep[1], 
                                                        sYInterpolation.pDst, sYInterpolation.dstStep,
                                                        sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                                                        j,0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Adv[BlockTSTypes[blk]],
                                    blk);
                            }

                            MBPattern = pCompressedMB->GetMBPattern();
                            CBPCY = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                            if (MBPattern==0 && mvDiff.x == 0 && mvDiff.y == 0)
                            {
                                pCompressedMB->ChangeType(VC1_ENC_P_MB_SKIP_1MV);
                                MBType = VC1_ENC_P_MB_SKIP_1MV;
                            }
                        }//VC1_ENC_P_MB_SKIP_1MV != MBType
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(MBType);
                        pDebug->SetMVInfo(&MV, mvPred.x,mvPred.y, 0);
                        pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 4);
                        pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);

                        if (pCompressedMB->isSkip() )
                        {
                            pDebug->SetMBAsSkip();
                        }
                        else
                        {
                            pDebug->SetCPB(MBPattern, CBPCY);
                            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        }
#endif
                        /*---------------------------Reconstruction ------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                            sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                            pRFrameY, m_uiRaisedPlaneStep[0], 
                                                            pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],                             
                                                            0,j);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                                sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                                pRFrameY, m_uiRaisedPlaneStep[0], 
                                                                pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],0,j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }


                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRFrameY;
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
                                smoothInfo.pRFrameU       = pRFrameU;
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
                                smoothInfo.pRFrameV       = pRFrameV;
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];
                                smoothInfo.curIntra       = 0;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                                pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                                pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                                pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                                GetInternalBlockEdge(pCurMBInfo, YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetInterpInfo(&sYInterpolation, &sUInterpolation, &sVInterpolation, 0, m_uiForwardPlanePadding);
#endif
                    }
                    break;
                default:
                    VM_ASSERT(0);
                    return UMC::UMC_ERR_FAILED;
                }

                if (m_pSavedMV)
                {
                    *(pSavedMV ++) = MV.x;
                    *(pSavedMV ++) = MV.y;
                }

#ifdef VC1_ENC_DEBUG_ON
                if(!bSubBlkTS)
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(), 15, 15);
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(), 15, 15);
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(),15);
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), 15);
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), 15);
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), 15);
                }
                else
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(),
                        pCurMBInfo->GetLumaAdUppEdge(), pCurMBInfo->GetLumaAdBotEdge() );
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(),
                        pCurMBInfo->GetLumaAdLefEdge(), pCurMBInfo->GetLumaAdRigEdge());
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(), pCurMBInfo->GetUAdHorEdge());
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), pCurMBInfo->GetVAdHorEdge());
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), pCurMBInfo->GetUAdVerEdge());
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), pCurMBInfo->GetVAdVerEdge());
                }
#endif

                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }

            deblkPattern = (deblkPattern | 0x4 | ( (! (Ipp8u)((i + 1 - (hMB -1)) >> 31)<<3))) & deblkMask;
            smoothPattern = 0x6 & smoothMask;

            //Row deblocking
            //STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            //        if (m_pSequenceHeader->IsLoopFilter() && bRaiseFrame && (i != firstRow || firstRow == hMB-1))
            //        {
            //STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
            //          Ipp8u *DeblkPlanes[3] = {m_pRaisedPlane[0] + i*m_uiRaisedPlaneStep[0]*VC1_ENC_LUMA_SIZE,
            //                                   m_pRaisedPlane[1] + i*m_uiRaisedPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
            //                                   m_pRaisedPlane[2] + i*m_uiRaisedPlaneStep[2]*VC1_ENC_CHROMA_SIZE};
            //            m_pMBs->StartRow();
            //            if(bSubBlkTS)
            //            {
            //                if(i < hMB-1)
            //                    Deblock_P_RowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                  if(firstRow != hMB -1)
            //                    Deblock_P_BottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                  else
            //                    Deblock_P_TopBottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //            }
            //            else
            //            {
            //                if(i < hMB-1)
            //                    Deblock_P_RowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                  if(firstRow != hMB -1)
            //                    Deblock_P_BottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                  else
            //                    Deblock_P_TopBottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //            }
            //STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            //        }
            //STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;

            pCurMBRow[0]+= m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE;
            pCurMBRow[1]+= m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
            pCurMBRow[2]+= m_uiPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

            pFMBRow[0]+= m_uiForwardPlaneStep[0]*VC1_ENC_LUMA_SIZE;
            pFMBRow[1]+= m_uiForwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
            pFMBRow[2]+= m_uiForwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

        }

#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame  && i == h)
            pDebug->PrintRestoredFrame(m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
            m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
            m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
#endif


        return err;
    }
    UMC::Status VC1EncoderPictureADV::VLC_P_Frame(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;
        Ipp32s                      h = (m_pSequenceHeader->GetPictureHeight()+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth()+15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        bool                  bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

        const Ipp16u*         pCBPCYTable = VLCTableCBPCY_PB[m_uiCBPTab];

        eCodingSet   LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet*   pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra])};
        const sACTablesSet*   pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

        const Ipp32u*  pDCTableVLCIntra[6]  ={DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],
            DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        const Ipp16s (*pTTMBVLC)[4][6]   = 0;
        const Ipp8u  (* pTTBlkVLC)[6]    = 0;
        const Ipp8u   *pSubPattern4x4VLC = 0;

        bool bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR || m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst();
        pDebug->SetCurrMB(false, 0, firstRow);
#endif

        if (m_uiQuant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_uiQuant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
                {
                    m_MECurMbStat->whole = 0;
                    memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                    m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;

                    pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
                }
#endif

                switch (pCompressedMB->GetMBType())
                {
                case VC1_ENC_P_MB_INTRA:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->MbType = UMC::ME_MbIntra;
                        Ipp32u MBStart = pCodedPicture->GetCurrBit();
#endif
                        err = pCompressedMB->WriteMBHeaderP_INTRA    ( pCodedPicture,
                            m_bRawBitplanes,
                            MVDiffTablesVLC[m_uiMVTab],
                            pCBPCYTable);
                        VC1_ENC_CHECK (err)



                            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                        //Blocks coding
                        for (blk = 0; blk<6; blk++)
                        {
                            err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLCIntra[blk],m_uiQuant,blk);
                            VC1_ENC_CHECK (err)
                                err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesSetIntra[blk],&ACEscInfo,blk);
                            VC1_ENC_CHECK (err)
                        }//for
                        STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->whole = (Ipp16u)(pCodedPicture->GetCurrBit()- MBStart);
#endif
                        break;
                    }
                case VC1_ENC_P_MB_1MV:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->MbType = UMC::ME_MbFrw;
#endif

                        err = pCompressedMB->WritePMB1MV(pCodedPicture,
                            m_bRawBitplanes,
                            m_uiMVTab,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo);



                        VC1_ENC_CHECK (err)
                            break;
                    }
                case VC1_ENC_P_MB_SKIP_1MV:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                        err = pCompressedMB->WritePMB_SKIP(pCodedPicture, m_bRawBitplanes);
                        VC1_ENC_CHECK (err)
                    }
                    break;
                }
#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)

            return err;
    }

    UMC::Status VC1EncoderPictureADV::VLC_PField1RefMixed(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;

        bool bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);
        bool bMVHalf                = false;

        const Ipp32u*               pCBPCYTable = CBPCYFieldTable_VLC[m_uiCBPTab];
        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        VC1EncoderCodedMB*          pCodedMB = m_pCodedMB + w*h*bSecondField;
        const Ipp8u*                pMBTypeFieldTable_VLC = MBTypeFieldMixedTable_VLC[m_uiMBModeTab];
        const Ipp8u*                pMV4BP = MV4BP[m_ui4MVCBPTab];


        eCodingSet   LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet*   pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra])};
        const sACTablesSet*   pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

        const Ipp32u*  pDCTableVLCIntra[6]  ={DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],
            DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        const Ipp16s (*pTTMBVLC)[4][6]   = 0;
        const Ipp8u  (* pTTBlkVLC)[6]    = 0;
        const Ipp8u   *pSubPattern4x4VLC = 0;

        sMVFieldInfo MVFieldInfo;

        initMVFieldInfo((m_uiDMVRangeIndex & 0x01)!=0,(m_uiDMVRangeIndex & 0x02)!=0, MVModeField1RefTable_VLC[m_uiMVTab],&MVFieldInfo);

        //err = WritePFieldHeader(pCodedPicture);
        //if (err != UMC::UMC_OK) return err;

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
#endif

        if (m_uiQuant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_uiQuant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
                {
                    m_MECurMbStat->whole = 0;
                    memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                    m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;
                    pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
                }
#endif

                switch (pCompressedMB->GetMBType())
                {
                case VC1_ENC_P_MB_INTRA:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->MbType = UMC::ME_MbIntra;
#endif

                        err = pCompressedMB->WriteMBHeaderPField_INTRA( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            pCBPCYTable);
                        VC1_ENC_CHECK (err)



                            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                        //Blocks coding
                        for (blk = 0; blk<6; blk++)
                        {
                            err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLCIntra[blk],m_uiQuant,blk);
                            VC1_ENC_CHECK (err)
                                err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesSetIntra[blk],&ACEscInfo,blk);
                            VC1_ENC_CHECK (err)
                        }//for
                        STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
                        break;
                    }
                case VC1_ENC_P_MB_1MV:
                case VC1_ENC_P_MB_SKIP_1MV:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_P_MB_1MV)
                            m_MECurMbStat->MbType = UMC::ME_MbFrw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                        err = pCompressedMB->WritePMB1MVField ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo);

                        VC1_ENC_CHECK (err)
                            break;
                    }
                case VC1_ENC_P_MB_4MV:
                case VC1_ENC_P_MB_SKIP_4MV:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_P_MB_4MV)
                            m_MECurMbStat->MbType = UMC::ME_MbFrw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                        err = pCompressedMB->WritePMB1MVFieldMixed ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            pMV4BP,
                            &ACEscInfo);

                        VC1_ENC_CHECK (err)
                            break;
                    } 
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
                }
#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)

            return err;
    }

    UMC::Status VC1EncoderPictureADV::VLC_PField1Ref(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;

        bool bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);
        bool bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR || m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        const Ipp32u*               pCBPCYTable = CBPCYFieldTable_VLC[m_uiCBPTab];
        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        VC1EncoderCodedMB*          pCodedMB = m_pCodedMB + w*h*bSecondField;
        const Ipp8u*                pMBTypeFieldTable_VLC = MBTypeFieldTable_VLC[m_uiMBModeTab];


        eCodingSet   LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet*   pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra])};
        const sACTablesSet*   pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

        const Ipp32u*  pDCTableVLCIntra[6]  ={DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],
            DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        const Ipp16s (*pTTMBVLC)[4][6]   = 0;
        const Ipp8u  (* pTTBlkVLC)[6]    = 0;
        const Ipp8u   *pSubPattern4x4VLC = 0;

        sMVFieldInfo MVFieldInfo;

        initMVFieldInfo((m_uiDMVRangeIndex & 0x01)!=0,(m_uiDMVRangeIndex & 0x02)!=0, MVModeField1RefTable_VLC[m_uiMVTab],&MVFieldInfo);

        //err = WritePFieldHeader(pCodedPicture);
        //if (err != UMC::UMC_OK) return err;

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
#endif

        if (m_uiQuant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_uiQuant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
                {
                    m_MECurMbStat->whole = 0;
                    memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                    m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;
                    pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
                }
#endif

                switch (pCompressedMB->GetMBType())
                {
                case VC1_ENC_P_MB_INTRA:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->MbType = UMC::ME_MbIntra;
#endif

                        err = pCompressedMB->WriteMBHeaderPField_INTRA( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            pCBPCYTable);
                        VC1_ENC_CHECK (err)



                            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                        //Blocks coding
                        for (blk = 0; blk<6; blk++)
                        {
                            err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLCIntra[blk],m_uiQuant,blk);
                            VC1_ENC_CHECK (err)
                                err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesSetIntra[blk],&ACEscInfo,blk);
                            VC1_ENC_CHECK (err)
                        }//for
                        STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
                        break;
                    }
                case VC1_ENC_P_MB_1MV:
                case VC1_ENC_P_MB_SKIP_1MV:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_P_MB_1MV)
                            m_MECurMbStat->MbType = UMC::ME_MbFrw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                        err = pCompressedMB->WritePMB1MVField ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo);

                        VC1_ENC_CHECK (err)
                            break;
                    }
                    /*           case VC1_ENC_P_MB_SKIP_1MV:
                    {
                    #ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
                    #endif
                    err = pCompressedMB->WritePMB_SKIP(pCodedPicture, m_bRawBitplanes);
                    VC1_ENC_CHECK (err)
                    }
                    break;*/
                }
#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)

            return err;
    }
    UMC::Status VC1EncoderPictureADV::VLC_PField2Ref(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;

        bool bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);
        bool bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR || m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        const Ipp32u*               pCBPCYTable = CBPCYFieldTable_VLC[m_uiCBPTab];
        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;


        VC1EncoderCodedMB*          pCodedMB = m_pCodedMB + w*h*bSecondField;
        const Ipp8u*                pMBTypeFieldTable_VLC = MBTypeFieldTable_VLC[m_uiMBModeTab];


        eCodingSet   LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet*   pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra])};
        const sACTablesSet*   pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

        const Ipp32u*  pDCTableVLCIntra[6]  ={DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],
            DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        const Ipp16s (*pTTMBVLC)[4][6]   = 0;
        const Ipp8u  (* pTTBlkVLC)[6]    = 0;
        const Ipp8u   *pSubPattern4x4VLC = 0;

        sMVFieldInfo MVFieldInfo;

        initMVFieldInfo((m_uiDMVRangeIndex & 0x01)!=0,(m_uiDMVRangeIndex & 0x02)!=0, MVModeField2RefTable_VLC[m_uiMVTab],&MVFieldInfo);

        //err = WritePFieldHeader(pCodedPicture);
        //if (err != UMC::UMC_OK) return err;

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
#endif

        if (m_uiQuant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_uiQuant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
                {
                    m_MECurMbStat->whole = 0;
                    memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                    m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;
                    pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
                }
#endif

                switch (pCompressedMB->GetMBType())
                {
                case VC1_ENC_P_MB_INTRA:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->MbType = UMC::ME_MbIntra;
#endif

                        err = pCompressedMB->WriteMBHeaderPField_INTRA( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            pCBPCYTable);
                        VC1_ENC_CHECK (err)



                            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                        //Blocks coding
                        for (blk = 0; blk<6; blk++)
                        {
                            err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLCIntra[blk],m_uiQuant,blk);
                            VC1_ENC_CHECK (err)
                                err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesSetIntra[blk],&ACEscInfo,blk);
                            VC1_ENC_CHECK (err)
                        }//for
                        STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
                        break;
                    }
                case VC1_ENC_P_MB_1MV:
                case VC1_ENC_P_MB_SKIP_1MV:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_P_MB_1MV)
                            m_MECurMbStat->MbType = UMC::ME_MbFrw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                        err = pCompressedMB->WritePMB2MVField ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo);

                        VC1_ENC_CHECK (err)
                            break;
                    }
                    /*           case VC1_ENC_P_MB_SKIP_1MV:
                    {
                    #ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
                    #endif
                    err = pCompressedMB->WritePMB_SKIP(pCodedPicture, m_bRawBitplanes);
                    VC1_ENC_CHECK (err)
                    }
                    break;*/
                }
#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)

            return err;
    }


    UMC::Status VC1EncoderPictureADV::VLC_PField2RefMixed(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField,  Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;

        bool bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

        if (m_uiMVMode != VC1_ENC_MIXED_QUARTER_BICUBIC)
            return UMC::UMC_ERR_FAILED;

        bool                        bMVHalf = 0;
        const Ipp32u*               pCBPCYTable = CBPCYFieldTable_VLC[m_uiCBPTab];
        const Ipp8u*                pMV4BP = MV4BP[m_ui4MVCBPTab];
        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        VC1EncoderCodedMB*          pCodedMB = m_pCodedMB + w*h*bSecondField;
        const Ipp8u*                pMBTypeFieldTable_VLC = MBTypeFieldMixedTable_VLC[m_uiMBModeTab];


        eCodingSet   LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet*   pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra])};
        const sACTablesSet*   pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

        const Ipp32u*  pDCTableVLCIntra[6]  ={DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],
            DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        const Ipp16s (*pTTMBVLC)[4][6]   = 0;
        const Ipp8u  (* pTTBlkVLC)[6]    = 0;
        const Ipp8u   *pSubPattern4x4VLC = 0;

        sMVFieldInfo MVFieldInfo;

        initMVFieldInfo((m_uiDMVRangeIndex & 0x01)!=0,(m_uiDMVRangeIndex & 0x02)!=0, MVModeField2RefTable_VLC[m_uiMVTab],&MVFieldInfo);

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
#endif

        if (m_uiQuant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_uiQuant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
                {
                    m_MECurMbStat->whole = 0;
                    memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                    m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;
                    pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
                }
#endif

                switch (pCompressedMB->GetMBType())
                {
                case VC1_ENC_P_MB_INTRA:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->MbType = UMC::ME_MbIntra;
#endif

                        err = pCompressedMB->WriteMBHeaderPField_INTRA( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            pCBPCYTable);
                        VC1_ENC_CHECK (err)



                            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                        //Blocks coding
                        for (blk = 0; blk<6; blk++)
                        {
                            err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLCIntra[blk],m_uiQuant,blk);
                            VC1_ENC_CHECK (err)
                                err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesSetIntra[blk],&ACEscInfo,blk);
                            VC1_ENC_CHECK (err)
                        }//for
                        STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
                        break;
                    }
                case VC1_ENC_P_MB_1MV:
                case VC1_ENC_P_MB_SKIP_1MV:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_P_MB_1MV)
                            m_MECurMbStat->MbType = UMC::ME_MbFrw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                        err = pCompressedMB->WritePMB2MVField ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo);

                        VC1_ENC_CHECK (err)
                            break;
                    }
                case VC1_ENC_P_MB_4MV:
                case VC1_ENC_P_MB_SKIP_4MV:
                    {

                        err = pCompressedMB->WritePMB2MVFieldMixed ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            pMV4BP);

                        VC1_ENC_CHECK (err)               
                            break;
                    }
                }
#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)

            return err;
    }

    UMC::Status VC1EncoderPictureADV::VLC_BField(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;

        bool bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);
        bool bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR || m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        const Ipp32u*               pCBPCYTable = CBPCYFieldTable_VLC[m_uiCBPTab];
        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        VC1EncoderCodedMB*          pCodedMB = m_pCodedMB + w*h*bSecondField;
        const Ipp8u*                pMBTypeFieldTable_VLC = MBTypeFieldTable_VLC[m_uiMBModeTab];


        eCodingSet   LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet   CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet*   pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra])};
        const sACTablesSet*   pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

        const Ipp32u*  pDCTableVLCIntra[6]  ={DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],
            DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        const Ipp16s (*pTTMBVLC)[4][6]   = 0;
        const Ipp8u  (* pTTBlkVLC)[6]    = 0;
        const Ipp8u   *pSubPattern4x4VLC = 0;

        sMVFieldInfo MVFieldInfo;

        initMVFieldInfo((m_uiDMVRangeIndex & 0x01)!=0,(m_uiDMVRangeIndex & 0x02)!=0, MVModeField2RefTable_VLC[m_uiMVTab],&MVFieldInfo);

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
#endif

        if (m_uiQuant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_uiQuant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
                {
                    m_MECurMbStat->whole = 0;
                    memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                    m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;
                    pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
                }
#endif

                switch (pCompressedMB->GetMBType())
                {
                case VC1_ENC_B_MB_INTRA:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->MbType = UMC::ME_MbIntra;
#endif

                        err = pCompressedMB->WriteMBHeaderPField_INTRA( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            pCBPCYTable);
                        VC1_ENC_CHECK (err)



                            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                        //Blocks coding
                        for (blk = 0; blk<6; blk++)
                        {
                            err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLCIntra[blk],m_uiQuant,blk);
                            VC1_ENC_CHECK (err)
                                err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesSetIntra[blk],&ACEscInfo,blk);
                            VC1_ENC_CHECK (err)
                        }//for
                        STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
                        break;
                    }
                case VC1_ENC_B_MB_F:
                case VC1_ENC_B_MB_SKIP_F:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_F)
                            m_MECurMbStat->MbType = UMC::ME_MbFrw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                        err = pCompressedMB->WriteBMBFieldForward  ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            m_bRawBitplanes);

                        VC1_ENC_CHECK (err)
                            break;

                    }
                case VC1_ENC_B_MB_B:
                case VC1_ENC_B_MB_SKIP_B:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_B)
                            m_MECurMbStat->MbType = UMC::ME_MbBkw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbBkwSkipped;
#endif
                        err = pCompressedMB->WriteBMBFieldBackward ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            m_bRawBitplanes);

                        VC1_ENC_CHECK (err)
                            break;


                    }
                case VC1_ENC_B_MB_FB:
                case VC1_ENC_B_MB_SKIP_FB:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_FB)
                            m_MECurMbStat->MbType = UMC::ME_MbBidir;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbBidirSkipped;
#endif
                        err = pCompressedMB->WriteBMBFieldInterpolated ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            m_bRawBitplanes);

                        VC1_ENC_CHECK (err)
                            break;


                    }
                case VC1_ENC_B_MB_DIRECT:
                case VC1_ENC_B_MB_SKIP_DIRECT:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_DIRECT)
                            m_MECurMbStat->MbType = UMC::ME_MbDirect;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbDirectSkipped;
#endif
                        err = pCompressedMB->WriteBMBFieldDirect      ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            m_bRawBitplanes);

                        VC1_ENC_CHECK (err)
                            break;

                    }
                default:
                    return UMC::UMC_ERR_FAILED;
                }
#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)

            return err;
    }
    UMC::Status VC1EncoderPictureADV::VLC_BFieldMixed(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;

        bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);
        bool                        bMVHalf =  false;

        const Ipp32u*               pCBPCYTable = CBPCYFieldTable_VLC[m_uiCBPTab];
        const Ipp8u*                pMV4BP = MV4BP[m_ui4MVCBPTab];

        Ipp32s                      h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        VC1EncoderCodedMB*          pCodedMB = m_pCodedMB + w*h*bSecondField;
        const Ipp8u*                pMBTypeFieldTable_VLC = MBTypeFieldMixedTable_VLC[m_uiMBModeTab];


        eCodingSet                  LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet                  ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet                  CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet*   pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra])};
        const sACTablesSet*   pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

        const Ipp32u*  pDCTableVLCIntra[6]  ={DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],
            DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        const Ipp16s (*pTTMBVLC)[4][6]   = 0;
        const Ipp8u  (* pTTBlkVLC)[6]    = 0;
        const Ipp8u   *pSubPattern4x4VLC = 0;

        sMVFieldInfo MVFieldInfo;

        initMVFieldInfo((m_uiDMVRangeIndex & 0x01)!=0,(m_uiDMVRangeIndex & 0x02)!=0, MVModeField2RefTable_VLC[m_uiMVTab],&MVFieldInfo);

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
#endif

        if (m_uiQuant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_uiQuant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
                {
                    m_MECurMbStat->whole = 0;
                    memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                    memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                    m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;
                    pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
                }
#endif

                switch (pCompressedMB->GetMBType())
                {
                case VC1_ENC_B_MB_INTRA:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->MbType = UMC::ME_MbIntra;
#endif

                        err = pCompressedMB->WriteMBHeaderPField_INTRA( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            pCBPCYTable);
                        VC1_ENC_CHECK (err)



                            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                        //Blocks coding
                        for (blk = 0; blk<6; blk++)
                        {
                            err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLCIntra[blk],m_uiQuant,blk);
                            VC1_ENC_CHECK (err)
                                err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesSetIntra[blk],&ACEscInfo,blk);
                            VC1_ENC_CHECK (err)
                        }//for
                        STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
                        break;
                    }
                case VC1_ENC_B_MB_F:
                case VC1_ENC_B_MB_SKIP_F:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_F)
                            m_MECurMbStat->MbType = UMC::ME_MbFrw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif                
                        err = pCompressedMB->WriteBMBFieldForward  ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            m_bRawBitplanes);

                        VC1_ENC_CHECK (err)
                            break;

                    }
                case VC1_ENC_B_MB_F_4MV:
                case VC1_ENC_B_MB_SKIP_F_4MV:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_F)
                            m_MECurMbStat->MbType = UMC::ME_MbFrw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif                
                        err = pCompressedMB->WriteBMBFieldForwardMixed  ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            pMV4BP,
                            m_bRawBitplanes);

                        VC1_ENC_CHECK (err)
                            break;

                    }
                case VC1_ENC_B_MB_B:
                case VC1_ENC_B_MB_SKIP_B:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_B)
                            m_MECurMbStat->MbType = UMC::ME_MbBkw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbBkwSkipped;
#endif
                        err = pCompressedMB->WriteBMBFieldBackward ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            m_bRawBitplanes);

                        VC1_ENC_CHECK (err)
                            break;


                    }
                case VC1_ENC_B_MB_B_4MV:
                case VC1_ENC_B_MB_SKIP_B_4MV:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_B)
                            m_MECurMbStat->MbType = UMC::ME_MbBkw;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbBkwSkipped;
#endif
                        err = pCompressedMB->WriteBMBFieldBackwardMixed ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            pMV4BP,
                            m_bRawBitplanes);

                        VC1_ENC_CHECK (err)
                            break;


                    }
                case VC1_ENC_B_MB_FB:
                case VC1_ENC_B_MB_SKIP_FB:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_FB)
                            m_MECurMbStat->MbType = UMC::ME_MbBidir;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbBidirSkipped;
#endif
                        err = pCompressedMB->WriteBMBFieldInterpolated ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            bMVHalf,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            m_bRawBitplanes);

                        VC1_ENC_CHECK (err)
                            break;


                    }
                case VC1_ENC_B_MB_DIRECT:
                case VC1_ENC_B_MB_SKIP_DIRECT:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_DIRECT)
                            m_MECurMbStat->MbType = UMC::ME_MbDirect;
                        else
                            m_MECurMbStat->MbType = UMC::ME_MbDirectSkipped;
#endif
                        err = pCompressedMB->WriteBMBFieldDirect      ( pCodedPicture,
                            pMBTypeFieldTable_VLC,
                            &MVFieldInfo,
                            m_uiMVRangeIndex,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo,
                            m_bRawBitplanes);

                        VC1_ENC_CHECK (err)
                            break;

                    }
                default:
                    return UMC::UMC_ERR_FAILED;
                }
#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)

            return err;
    }
    UMC::Status VC1EncoderPictureADV::PAC_PFrameMixed(UMC::MeParams* MEParams, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status                 err = UMC::UMC_OK;
        Ipp32s                      i=0, j=0, blk = 0;

        Ipp32s                      h = (m_pSequenceHeader->GetPictureHeight()+15)/16;
        Ipp32s                      w = (m_pSequenceHeader->GetPictureWidth()+15)/16;
        Ipp32s                      hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);

        Ipp8u*                      pCurMBRow[3] = {m_pPlane[0] + firstRow*m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE,
                                                    m_pPlane[1] + firstRow*m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE,        
                                                    m_pPlane[2] + firstRow*m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE};

        Ipp8u*                      pFMBRow  [3] = {m_pForwardPlane[0]+ firstRow*m_uiForwardPlaneStep[0]*VC1_ENC_LUMA_SIZE, 
                                                    m_pForwardPlane[1]+ firstRow*m_uiForwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE, 
                                                    m_pForwardPlane[2]+ firstRow*m_uiForwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE};
        //forward transform quantization

        _SetIntraInterLumaFunctions;

        //inverse transform quantization
        IntraInvTransformQuantFunction IntraInvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :
            IntraInvTransformQuantNonUniform;

        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (m_bUniformQuant) ? InterInvTransformQuantUniform :
            InterInvTransformQuantNonUniform;

        CalculateChromaFunction    CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())? GetChromaMVFast:GetChromaMV;


        InterpolateFunction          InterpolateLumaFunction     =  _own_ippiInterpolateQPBicubic_VC1_8u_C1R;
        Ipp8u                       tempInterpolationBuffer[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];
        InterpolateFunctionPadding  InterpolateLumaFunctionPadding = own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;

        IppVCInterpolate_8u         sYInterpolation = {0};
        IppVCInterpolate_8u         sUInterpolation = {0};
        IppVCInterpolate_8u         sVInterpolation = {0};
        IppVCInterpolate_8u         sYInterpolationBlk[4]={0};
        IppVCInterpolateBlock_8u    InterpolateBlockY;
        IppVCInterpolateBlock_8u    InterpolateBlockU;
        IppVCInterpolateBlock_8u    InterpolateBlockV;
        IppVCInterpolateBlock_8u    InterpolateBlockBlk[4];

        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
        Ipp8s                       dACPrediction   = 1;

        IppiSize                    blkSize     = {8,8};
        IppiSize                    blkSizeLuma = {16,16};

        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];
        Ipp16s                      defPredictor    =  0;
        Ipp16s*                     pSavedMV = (m_pSavedMV)?  m_pSavedMV + w*2*firstRow:0;

        sCoordinate                 MVPredMBMin = {-60,-60};
        sCoordinate                 MVPredMBMax = {(Ipp16s)w*16*4-4, (Ipp16s)h*16*4-4};

        sCoordinate                 MVPredMBMinB = {-28,-28};
        sCoordinate                 MVPredMBMaxB = {(Ipp16s)w*16*4-4, (Ipp16s)h*16*4-4};


        bool                        bMVHalf = false;

        bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

        bool                        bSubBlkTS           = m_pSequenceHeader->IsVSTransform() && (!(m_bVSTransform &&m_uiTransformType==VC1_ENC_8x8_TRANSFORM));
        fGetExternalEdge            pGetExternalEdge    = GetExternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS
        fGetInternalEdge            pGetInternalEdge    = GetInternalEdge[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS

        Ipp8u deblkPattern = (firstRow != hMB - 1)? 0 : 0x8;   //4 bits: right 1 bit - 0 - left/1 - not left,
        //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row, 10 - top bottom row
        //middle 1 bit - 1 - right/0 - not right

        Ipp8u deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFC : 0;

        Ipp8u smoothMask = (m_pSequenceHeader->IsOverlap() && (m_uiQuant >= 9)) ? 0xFF : 0;
        Ipp8u smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
        //0/1 - top/not top, 0/1 - left/not left

        SmoothInfo_P_Adv smoothInfo = {0};

        IppStatus                  ippSts  = ippStsNoErr;

#ifdef VC1_ENC_DEBUG_ON
        bool                        bIsBilinearInterpolation   = false;
        //pDebug->SetCurrMBFirst();
        pDebug->SetCurrMB(false, 0, firstRow);
        pDebug->SetSliceInfo(firstRow, firstRow + nRows);
        pDebug->SetInterpolType(bIsBilinearInterpolation);
        pDebug->SetRounControl(m_uiRoundControl);
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
        pDebug->SetVTSFlag(bSubBlkTS);
#endif

        if (pSavedMV)
        {
            memset(pSavedMV,0,w*(hMB - firstRow)*2*sizeof(Ipp16s));
        }


        err = m_pMBs->Reset();
        if (err != UMC::UMC_OK)
            return err;

        SetInterpolationParams(&sYInterpolation,&sUInterpolation,&sVInterpolation,
            tempInterpolationBuffer,true, false);
        SetInterpolationParams4MV(sYInterpolationBlk,&sUInterpolation,&sVInterpolation,
            tempInterpolationBuffer,true);

        SetInterpolationParams(&InterpolateBlockY,&InterpolateBlockU,&InterpolateBlockV, tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            m_pForwardPlane, m_uiForwardPlaneStep, false);
        SetInterpolationParams4MV (InterpolateBlockBlk, tempInterpolationBuffer,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            m_pForwardPlane, m_uiForwardPlaneStep, false);
#ifdef VC1_ENC_CHECK_MV
        err = CheckMEMV_P_MIXED(MEParams,bMVHalf,firstRow,nRows);
        assert(err == UMC::UMC_OK);
#endif

        /* -------------------------------------------------------------------------*/
        for (i = firstRow; i < hMB; i++)
        {
            Ipp8u *pRFrameY = 0;
            Ipp8u *pRFrameU = 0;
            Ipp8u *pRFrameV = 0;

            if (bRaiseFrame)
            {
                pRFrameY = m_pRaisedPlane[0]+VC1_ENC_LUMA_SIZE*  (i*m_uiRaisedPlaneStep[0]);
                pRFrameU = m_pRaisedPlane[1]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[1]);
                pRFrameV = m_pRaisedPlane[2]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[2]);
            }
            for (j=0; j < w; j++)
            {

                //bool                bIntra      = false;
                Ipp32s              xLuma        =  VC1_ENC_LUMA_SIZE*j;
                Ipp32s              xChroma      =  VC1_ENC_CHROMA_SIZE*j;

                //Ipp32s              posY        =  VC1_ENC_LUMA_SIZE*i;
                Ipp8u               MBPattern   = 0;
                Ipp8u               CBPCY       = 0;

                VC1EncoderMBInfo  * pCurMBInfo  = m_pMBs->GetCurrMBInfo();
                VC1EncoderMBData  * pCurMBData  = m_pMBs->GetCurrMBData();
                VC1EncoderMBData  * pRecMBData  = m_pMBs->GetRecCurrMBData();

                VC1EncoderMBInfo* left          = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* topLeft       = m_pMBs->GetTopLeftMBInfo();
                VC1EncoderMBInfo* top           = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* topRight      = m_pMBs->GetTopRightMBInfo();
                VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);

                Ipp8u          intraPattern     = 0; //from UMC_ME: position 1<<VC_ENC_PATTERN_POS(blk) (if not skiped)



                eTransformType  BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                    m_uiTransformType, m_uiTransformType,
                    m_uiTransformType, m_uiTransformType};

                eMBType MBType;

                switch (MEParams->pSrc->MBs[j + i*w].MbType)
                {
                case UMC::ME_MbIntra:
                    MBType = VC1_ENC_P_MB_INTRA;
                    break;
                case UMC::ME_MbFrw:
                    MBType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_P_MB_4MV:VC1_ENC_P_MB_1MV;
                    break;
                case UMC::ME_MbMixed:
                    MBType = VC1_ENC_P_MB_4MV;
                    intraPattern = (Ipp8u)( (UMC::ME_MbIntra==MEParams->pSrc->MBs[j + i*w].BlockType[0])<<VC_ENC_PATTERN_POS(0)|
                        (UMC::ME_MbIntra==MEParams->pSrc->MBs[j + i*w].BlockType[1])<<VC_ENC_PATTERN_POS(1)|
                        (UMC::ME_MbIntra==MEParams->pSrc->MBs[j + i*w].BlockType[2])<<VC_ENC_PATTERN_POS(2)|
                        (UMC::ME_MbIntra==MEParams->pSrc->MBs[j + i*w].BlockType[3])<<VC_ENC_PATTERN_POS(3));
                    break;
                case UMC::ME_MbFrwSkipped:
                    MBType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_P_MB_SKIP_4MV:VC1_ENC_P_MB_SKIP_1MV;
                    break;
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
                }


                pCompressedMB->Init(MBType);

                /*if (bCalculateVSTransform && MBType!= VC1_ENC_P_MB_INTRA)
                {
                //copy from ME class
                for(blk = 0; blk < 4; blk++)
                {
                BlockTSTypes[blk] = BlkTransformTypeTabl[(MEParams.pSrc->MBs[j + i*w].BlockTrans>>(blk<<1))&0x03];
                }
                BlockTSTypes[4] = VC1_ENC_8x8_TRANSFORM;
                BlockTSTypes[5] = VC1_ENC_8x8_TRANSFORM;

                pCompressedMB->SetTSType(BlockTSTypes);
                }*/

                switch  (MBType)
                {
                case VC1_ENC_P_MB_INTRA:
                    {
                        NeighbouringMBsData MBs;
                        NeighbouringMBsIntraPattern     MBsIntraPattern;

                        MBs.LeftMB    = m_pMBs->GetLeftMBData();
                        MBs.TopMB     = m_pMBs->GetTopMBData();
                        MBs.TopLeftMB = m_pMBs->GetTopLeftMBData();

                        MBsIntraPattern.LeftMB      = (left)?       left->GetIntraPattern():0;
                        MBsIntraPattern.TopMB       = (top)?        top->GetIntraPattern():0;
                        MBsIntraPattern.TopLeftMB   = (topLeft)?    topLeft->GetIntraPattern():0;



                        pCurMBInfo->Init(true);

                        pCurMBData->CopyMBProg(pCurMBRow[0],m_uiPlaneStep[0],pCurMBRow[1], pCurMBRow[2], m_uiPlaneStep[1],j);

                        //only intra blocks:
                        for (blk = 0; blk<6; blk++)
                        {
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                            _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                        }

                        STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                        dACPrediction = DCACPredictionFrame4MVIntraMB( pCurMBData,&MBs,&MBsIntraPattern,pRecMBData, defPredictor,direction);
                        STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                        for (blk=0; blk<6; blk++)
                        {
                            pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                pRecMBData->m_uiBlockStep[blk],
                                VC1_Inter_8x8_Scan,
                                blk);
                        }

                        MBPattern = pCompressedMB->GetMBPattern();
                        CBPCY = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetACPrediction(dACPrediction);
                        pCompressedMB->SetMBCBPCY(CBPCY);
                        pCurMBInfo->SetEdgesIntra(i==0, j==0);

                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            for (blk=0;blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                    DCQuant, doubleQuant);
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],blkSize,0);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            pRecMBData->PasteMBProg(pRFrameY,m_uiRaisedPlaneStep[0],pRFrameU,pRFrameV,m_uiRaisedPlaneStep[1],j);

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRFrameY;
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
                                smoothInfo.pRFrameU       = pRFrameU;
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
                                smoothInfo.pRFrameV       = pRFrameV;
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];
                                smoothInfo.curIntra       = 0x3F;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            //deblocking
                            {
                                STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                Ipp8u *pDblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};

                                m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                        if (m_pSavedMV)
                        {
                            *(pSavedMV ++) = 0;
                            *(pSavedMV ++) = 0;
                        }
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(VC1_ENC_P_MB_INTRA);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                    }
                    break;
                case VC1_ENC_P_MB_1MV:
                case VC1_ENC_P_MB_SKIP_1MV:
                    {

                        sCoordinate    MVIntChroma     = {0,0};
                        sCoordinate    MVQuarterChroma = {0,0};
                        sCoordinate    MVChroma        = {0,0};
                        sCoordinate    MVInt           = {0,0};
                        sCoordinate    MVQuarter       = {0,0};
                        sCoordinate    MV              = { MEParams->pSrc->MBs[j + i*w].MV[0]->x,MEParams->pSrc->MBs[j + i*w].MV[0]->y};

                        sCoordinate    *mvC=0, *mvB=0, *mvA=0;
                        sCoordinate     mv1,    mv2,    mv3;
                        sCoordinate     mvPred;
                        sCoordinate     mvDiff;
                        Ipp8u           hybrid = 0;

                        pCurMBInfo->Init(false);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        /*MV prediction */
                        GetMVPredictionP(true)
                        PredictMV(mvA,mvB,mvC, &mvPred);
                        ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMBMin,MVPredMBMax);
                        hybrid = HybridPrediction(&mvPred,&MV,mvA,mvC,32);
                        pCompressedMB->SetHybrid(hybrid);
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        if (VC1_ENC_P_MB_SKIP_1MV == MBType)
                        {
                            //correct ME results
                            VM_ASSERT(MV.x==mvPred.x && MV.y==mvPred.y);
                            MV.x=mvPred.x;
                            MV.y=mvPred.y;
                        }
                        pCurMBInfo->SetMV(MV);
                        GetIntQuarterMV(MV,&MVInt, &MVQuarter);

                        if (MBType != VC1_ENC_P_MB_SKIP_1MV || bRaiseFrame)
                        {
                            CalculateChroma(MV,&MVChroma);
                            pCurMBInfo->SetMVChroma(MVChroma);
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);
                            if (m_uiForwardPlanePadding)
                            {

                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolation, pFMBRow[0], m_uiForwardPlaneStep[0], &MVInt, &MVQuarter, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolation);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolation, &sVInterpolation, pFMBRow[1], pFMBRow[2],m_uiForwardPlaneStep[1], 
                                    &MVIntChroma, &MVQuarterChroma, j);

 
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            }
                            else
                            {
                                bool bOpposite = (m_bForwardInterlace)? 1:0;

                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockY, j, i , &MVInt, &MVQuarter);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockY,0,0,bOpposite,false,m_uiRoundControl,0);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockU, &InterpolateBlockV, 
                                                        bOpposite,false,m_uiRoundControl,0,
                                                        j, i ,  &MVIntChroma, &MVQuarterChroma);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            }
                        } //interpolation
                        if (VC1_ENC_P_MB_SKIP_1MV != MBType)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = sYInterpolation.pDst; 
                                pIn.refYStep = sYInterpolation.dstStep;      
                                pIn.pURef    = sUInterpolation.pDst; 
                                pIn.refUStep = sUInterpolation.dstStep;
                                pIn.pVRef    = sVInterpolation.pDst; 
                                pIn.refVStep = sVInterpolation.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 0;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            mvDiff.x = MV.x - mvPred.x;
                            mvDiff.y = MV.y - mvPred.y;
                            pCompressedMB->SetdMV(mvDiff);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],m_uiPlaneStep[0], pCurMBRow[1],pCurMBRow[2], m_uiPlaneStep[1], 
                                                        sYInterpolation.pDst, sYInterpolation.dstStep,
                                                        sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                                                        j,0);
                          
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Adv[BlockTSTypes[blk]],
                                    blk);
                            }

                            MBPattern = pCompressedMB->GetMBPattern();
                            CBPCY = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                            if (MBPattern==0 && mvDiff.x == 0 && mvDiff.y == 0)
                            {
                                pCompressedMB->ChangeType(VC1_ENC_P_MB_SKIP_1MV);
                                MBType = VC1_ENC_P_MB_SKIP_1MV;
                            }
                        }// not skipped



                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern!=0)
                            {
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk = 0; blk <6 ; blk++)
                                {
                                    InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                        pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                        doubleQuant,BlockTSTypes[blk]);
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                            sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                            pRFrameY, m_uiRaisedPlaneStep[0], 
                                                            pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],                             
                                                            0,j);
                            }
                            else
                            {
                                pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                                sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                                pRFrameY, m_uiRaisedPlaneStep[0], 
                                                                pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],0,j);

                            }

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRFrameY;
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
                                smoothInfo.pRFrameU       = pRFrameU;
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
                                smoothInfo.pRFrameV       = pRFrameV;
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];
                                smoothInfo.curIntra       = 0;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                                pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                                pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                                pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                                GetInternalBlockEdge(pCurMBInfo, YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};

                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }

                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                        if (m_pSavedMV)
                        {

                            *(pSavedMV ++) = (MVInt.x<<2) + MVQuarter.x;
                            *(pSavedMV ++) = (MVInt.y<<2) + MVQuarter.y;
                        }
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(MBType);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetMVInfo(&MV, mvPred.x, mvPred.y, 0);
                        pDebug->SetIntrpMV(MV.x, MV.y, 0);
                        pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 4);
                        pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 5);
                        if (MBType != VC1_ENC_P_MB_SKIP_1MV)
                        {
                            pDebug->SetCPB(MBPattern, CBPCY);
                            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        }
                        else
                        {
                            pDebug->SetMBAsSkip();
                        }

                        //interpolation
                        {
                            pDebug->SetInterpInfo(&sYInterpolation, &sUInterpolation, &sVInterpolation, 0, m_uiForwardPlanePadding);
                        }
#endif

                    }
                    break;

                case VC1_ENC_P_MB_4MV:
                case VC1_ENC_P_MB_SKIP_4MV:
                    {
                        sCoordinate    MVInt[4]           = {0};
                        sCoordinate    MVQuarter[4]       = {0};
                        sCoordinate    MV[4]              = {   {MEParams->pSrc->MBs[j + i*w].MV[0][0].x,MEParams->pSrc->MBs[j + i*w].MV[0][0].y},
                                                                {MEParams->pSrc->MBs[j + i*w].MV[0][1].x,MEParams->pSrc->MBs[j + i*w].MV[0][1].y},
                                                                {MEParams->pSrc->MBs[j + i*w].MV[0][2].x,MEParams->pSrc->MBs[j + i*w].MV[0][2].y},
                                                                {MEParams->pSrc->MBs[j + i*w].MV[0][3].x,MEParams->pSrc->MBs[j + i*w].MV[0][3].y}};
                        //sCoordinate    MVPred[4]       = {0};
                        sCoordinate    MVLuma          = {0,0};
                        sCoordinate    MVChroma        = {0,0};
                        sCoordinate    MVIntChroma     = {0,0};
                        sCoordinate    MVQuarterChroma = {0,0};

                        sCoordinate    *mvC=0, *mvB=0, *mvA=0;
                        sCoordinate     mv1,    mv2,    mv3;
                        sCoordinate     mvPred[4]={0};
                        sCoordinate     mvDiff;
                        Ipp8u           hybrid = 0;
                        //bool            bInterpolation  = false;
                        bool            bNullMV         = true;
                        NeighbouringMBsData             MBs;
                        NeighbouringMBsIntraPattern     MBsIntraPattern;

                        MBs.LeftMB    = m_pMBs->GetLeftMBData();
                        MBs.TopMB     = m_pMBs->GetTopMBData();
                        MBs.TopLeftMB = m_pMBs->GetTopLeftMBData();

                        MBsIntraPattern.LeftMB      = (left)?       left->GetIntraPattern():0;
                        MBsIntraPattern.TopMB       = (top)?        top->GetIntraPattern():0;
                        MBsIntraPattern.TopLeftMB   = (topLeft)?    topLeft->GetIntraPattern():0;

                        pCurMBInfo->Init(false);

#ifdef VC1_ENC_DEBUG_ON
                        if (VC1_ENC_P_MB_SKIP_4MV == MBType  )
                        {
                            pDebug->SetMBType(VC1_ENC_P_MB_SKIP_4MV);
                            pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                            pDebug->SetMBAsSkip();
                        }
                        else
                        {
                            pDebug->SetMBType(VC1_ENC_P_MB_4MV);
                            pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        }
#endif
                        // --- MV Prediction ---- //
                        if (VC1_ENC_P_MB_SKIP_4MV != MBType )
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = sYInterpolation.pDst; 
                                pIn.refYStep = sYInterpolation.dstStep;      
                                pIn.pURef    = sUInterpolation.pDst; 
                                pIn.refUStep = sUInterpolation.dstStep;
                                pIn.pVRef    = sVInterpolation.pDst; 
                                pIn.refVStep = sVInterpolation.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = intraPattern;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 0;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                            }
                            if (0 != intraPattern )
                                memset (tempInterpolationBuffer,128,VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS*sizeof(Ipp8u));

                            for (blk = 0; blk<4; blk++)
                            {
                                if ((intraPattern&(1<<VC_ENC_PATTERN_POS(blk))) == 0)
                                {
                                    Ipp32u blkPosX = (blk&0x1)<<3;
                                    Ipp32u blkPosY = (blk/2)<<3;

                                    pCurMBInfo->SetMV(MV[blk],blk, true);
                                    STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                                    GetMVPredictionPBlk(blk);
                                    PredictMV(mvA,mvB,mvC, &mvPred[blk]);

                                    ScalePredict(&mvPred[blk], ((j<<4) + blkPosX)<<2,((i<<4) + blkPosY)<<2,MVPredMBMinB,MVPredMBMaxB);
                                    hybrid = HybridPrediction(&mvPred[blk],&MV[blk],mvA,mvC,32);
                                    pCompressedMB->SetHybrid(hybrid,blk);
                                    STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                                    mvDiff.x = MV[blk].x - mvPred[blk].x;
                                    mvDiff.y = MV[blk].y - mvPred[blk].y;

                                    bNullMV = ((mvDiff.x!=0)||(mvDiff.y!=0))? false: bNullMV;
                                    pCompressedMB->SetBlockdMV(mvDiff,blk);
#ifdef VC1_ENC_DEBUG_ON
                                    pDebug->SetMVInfo(&MV[blk], mvPred[blk].x,mvPred[blk].y, 0, blk);
                                    pDebug->SetMVDiff(mvDiff.x, mvDiff.y, 0, blk);
                                    pDebug->SetIntrpMV(MV[blk].x, MV[blk].y, 0, blk);
#endif
                                }
                                else
                                {
                                    pCurMBInfo->SetIntraBlock(blk);
                                    pCompressedMB->SetIntraBlock(blk);

                                    BlockTSTypes[blk]=VC1_ENC_8x8_TRANSFORM;
#ifdef VC1_ENC_DEBUG_ON
                                    pDebug->SetBlockAsIntra(blk);
#endif
                                }
                            } // for

                            pCompressedMB->SetTSType(BlockTSTypes);

                            if (!pCurMBInfo->GetLumaMV(&MVLuma))
                            {   //chroma intra type
                                intraPattern = intraPattern | (1<<VC_ENC_PATTERN_POS(4))|(1<<VC_ENC_PATTERN_POS(5));
                                pCurMBInfo->SetIntraBlock(4);
                                pCurMBInfo->SetIntraBlock(5);
                                pCompressedMB->SetIntraBlock(4);
                                pCompressedMB->SetIntraBlock(5);
                                BlockTSTypes[4]=VC1_ENC_8x8_TRANSFORM;
                                BlockTSTypes[5]=VC1_ENC_8x8_TRANSFORM;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetBlockAsIntra(4);
                                pDebug->SetBlockAsIntra(5);
#endif
                            }
                        }
                        else  // prediction for skip MB
                        {
                            for (blk = 0; blk<4; blk++)
                            {
                                Ipp32u blkPosX = (blk&0x1)<<3;
                                Ipp32u blkPosY = (blk/2)<<3;

                                STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                                GetMVPredictionPBlk(blk);
                                PredictMV(mvA,mvB,mvC, &mvPred[blk]);
                                ScalePredict(&mvPred[blk], ((j<<4) + blkPosX)<<2,((i<<4) + blkPosY)<<2,MVPredMBMinB,MVPredMBMaxB);
                                hybrid = HybridPrediction(&mvPred[blk],&MV[blk],mvA,mvC,32);
                                pCompressedMB->SetHybrid(hybrid,blk);
                                STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                                //check MV from ME
                                assert( mvPred[blk].x == MV[blk].x && mvPred[blk].y == MV[blk].y);
                                MV[blk].x = mvPred[blk].x;
                                MV[blk].y = mvPred[blk].y;
                                pCurMBInfo->SetMV(MV[blk],blk, true);
                                GetIntQuarterMV(MV[blk],&MVInt[blk], &MVQuarter[blk]);
#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetMVInfo(&MV[blk], mvPred[blk].x,mvPred[blk].y, 0, blk);
                                pDebug->SetIntrpMV(MV[blk].x, MV[blk].y, 0, blk);
#endif

                            }
                            pCurMBInfo->GetLumaMV(&MVLuma);
                            intraPattern = 0;
                        }

                        // --- Interpolation --- //
                        if (VC1_ENC_P_MB_SKIP_4MV != MBType   || bRaiseFrame)
                        {
                            for (blk = 0; blk<4; blk++)
                            {
                                Ipp32u blkPosX = (blk&0x1)<<3;
                                Ipp32u blkPosY = (blk/2)<<3;

                                // Luma blocks
                                if ((intraPattern&(1<<VC_ENC_PATTERN_POS(blk))) == 0)
                                {
                                    GetIntQuarterMV(MV[blk],&MVInt[blk], &MVQuarter[blk]);
                                    if (m_uiForwardPlanePadding)
                                    {

                                        SetInterpolationParamsLumaBlk(&sYInterpolationBlk[blk], pFMBRow[0], m_uiForwardPlaneStep[0], &MVInt[blk], &MVQuarter[blk], j,blk);

                                        IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                        ippSts = InterpolateLumaFunction(&sYInterpolationBlk[blk]);
                                        VC1_ENC_IPP_CHECK(ippSts);

                                        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                    }
                                    else
                                    {                           
                                        bool bOpposite = (m_bForwardInterlace)? 1:0;
                                        STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);

                                        SetInterpolationParamsLumaBlk(&InterpolateBlockBlk[blk], j, i , &MVInt[blk], &MVQuarter[blk], blk);                                
                                        IPP_STAT_START_TIME(m_IppStat->IppStartTime);    

                                        ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockBlk[blk],0,0,bOpposite,false,m_uiRoundControl,0);
                                        VC1_ENC_IPP_CHECK(ippSts);

                                        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                        STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);                              

                                    }

#ifdef VC1_ENC_DEBUG_ON
                                    //interpolation
                                    pDebug->SetInterpInfoLuma(sYInterpolationBlk[blk].pDst, sYInterpolationBlk[blk].dstStep, blk, 0);
#endif
                                }//if
                            }//for blk
                            if ((intraPattern&(1<<VC_ENC_PATTERN_POS(4))) == 0)
                            {
                                CalculateChroma(MVLuma,&MVChroma);
                                pCurMBInfo->SetMVChroma(MVChroma);
                                GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);

                                if (m_uiForwardPlanePadding)
                                {
                                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                    InterpolateChroma (&sUInterpolation, &sVInterpolation, pFMBRow[1], pFMBRow[2],m_uiForwardPlaneStep[1], 
                                                        &MVIntChroma, &MVQuarterChroma, j);

                                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                                    
                                }
                                else
                                {

                                    bool bOpposite = (m_bForwardInterlace)? 1:0;
                                    STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);

                                    InterpolateChromaPad   (&InterpolateBlockU, &InterpolateBlockV, 
                                                            bOpposite,false,m_uiRoundControl,0,
                                                            j, i ,  &MVIntChroma, &MVQuarterChroma);                                  

                                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                    STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);                       
                                }

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 4);
                                pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 5);
                                pDebug->SetInterpInfoChroma(sUInterpolation.pDst, sUInterpolation.dstStep,
                                    sVInterpolation.pDst, sVInterpolation.dstStep, 0);
#endif
                            } // chroma


                        }//if interpolation
                        //Chroma blocks

                        // --- Compressing --- //
                        if (VC1_ENC_P_MB_SKIP_4MV != MBType)
                        {
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],m_uiPlaneStep[0], pCurMBRow[1],pCurMBRow[2], m_uiPlaneStep[1], 
                                                        sYInterpolation.pDst, sYInterpolation.dstStep,
                                                        sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                                                        j,0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            for (blk = 0; blk<6; blk++)
                            {
                                if (!pCurMBInfo->isIntra(blk))
                                {
                                    InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                        BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                }
                                else
                                {
                                    IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                        DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                }
                            }
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);


                            dACPrediction = DCACPredictionFrame4MVBlockMixed   (pCurMBData,
                                pCurMBInfo->GetIntraPattern(),
                                &MBs,
                                &MBsIntraPattern,
                                pRecMBData,
                                defPredictor,
                                direction);

                            for (blk = 0; blk<6; blk++)
                            {
                                pCompressedMB->SaveResidual(pRecMBData->m_pBlock[blk],
                                    pRecMBData->m_uiBlockStep[blk],
                                    ZagTables_Adv[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern = pCompressedMB->GetMBPattern();
                            CBPCY = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                            pCompressedMB->SetACPrediction(dACPrediction);
                            if (MBPattern==0 && intraPattern==0 && bNullMV)
                            {
                                pCompressedMB->ChangeType(VC1_ENC_P_MB_SKIP_4MV);
                                MBType = VC1_ENC_P_MB_SKIP_4MV;
#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetMBType(VC1_ENC_P_MB_SKIP_4MV);
                                pDebug->SetMBAsSkip();
#endif
                            }
#ifdef VC1_ENC_DEBUG_ON
                            if (MBType != VC1_ENC_P_MB_SKIP_4MV)
                            {
                                pDebug->SetCPB(MBPattern, CBPCY);
                                pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                            }
#endif
                        } // end compressing

#ifdef VC1_ENC_DEBUG_ON
                        if (MBType != VC1_ENC_P_MB_SKIP_4MV)
                        {
                            pDebug->SetCPB(MBPattern, CBPCY);
                            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        }
#endif

                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern!=0 || intraPattern!=0)
                            {
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk = 0; blk <6 ; blk++)
                                {
                                    if (!pCurMBInfo->isIntra(blk))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                    else
                                    {
                                        IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            DCQuant,doubleQuant);
                                    }
                                } // for
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                            sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                            pRFrameY, m_uiRaisedPlaneStep[0], 
                                                            pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],                             
                                                            0,j);

                            }
                            else
                            {
                                pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                                sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                                pRFrameY, m_uiRaisedPlaneStep[0], 
                                                                pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],0,j);

                            }

                            //smoothing
                            {
                                smoothInfo.pCurRData      = pRecMBData;
                                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                                smoothInfo.pRFrameY       = pRFrameY;
                                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
                                smoothInfo.pRFrameU       = pRFrameU;
                                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
                                smoothInfo.pRFrameV       = pRFrameV;
                                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];
                                smoothInfo.curIntra       = 0;
                                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

#ifdef VC1_ENC_DEBUG_ON
                                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
                            }

                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                                pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                                pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                                pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                                pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                                GetInternalBlockEdge(pCurMBInfo, YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};

                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }

                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                        if (m_pSavedMV)
                        {
                            sCoordinate MVIntLuma    ={0};
                            sCoordinate MVQuarterLuma={0};
                            //pull back only simple/main profiles
                            GetIntQuarterMV(MVLuma,&MVIntLuma,&MVQuarterLuma);
                            *(pSavedMV ++) = (MVIntLuma.x<<2) + MVQuarterLuma.x;
                            *(pSavedMV ++) = (MVIntLuma.y<<2) + MVQuarterLuma.y;

                        }
                    }
                    break;

                }

#ifdef VC1_ENC_DEBUG_ON
                if(!bSubBlkTS)
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(), 15, 15);
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(), 15, 15);
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(),15);
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), 15);
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), 15);
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), 15);
                }
                else
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(),
                        pCurMBInfo->GetLumaAdUppEdge(), pCurMBInfo->GetLumaAdBotEdge() );
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(),
                        pCurMBInfo->GetLumaAdLefEdge(), pCurMBInfo->GetLumaAdRigEdge());
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(), pCurMBInfo->GetUAdHorEdge());
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), pCurMBInfo->GetVAdHorEdge());
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), pCurMBInfo->GetUAdVerEdge());
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), pCurMBInfo->GetVAdVerEdge());
                }
#endif
                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }

            deblkPattern = (deblkPattern | 0x4 | ( (! (Ipp8u)((i + 1 - (hMB -1)) >> 31)<<3))) & deblkMask;
            smoothPattern = 0x6 & smoothMask;

            //Row deblocking
            //STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            //        if (m_pSequenceHeader->IsLoopFilter() && bRaiseFrame && (i != firstRow || firstRow == hMB-1))
            //        {
            //STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
            //          Ipp8u *DeblkPlanes[3] = {m_pRaisedPlane[0] + i*m_uiRaisedPlaneStep[0]*VC1_ENC_LUMA_SIZE,
            //                            m_pRaisedPlane[1] + i*m_uiRaisedPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
            //                            m_pRaisedPlane[2] + i*m_uiRaisedPlaneStep[2]*VC1_ENC_CHROMA_SIZE};
            //            m_pMBs->StartRow();
            //
            //            if(bSubBlkTS)
            //            {
            //                if(i < hMB -1)
            //                    Deblock_P_RowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                  if(firstRow != hMB -1)
            //                    Deblock_P_BottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                  else
            //                    Deblock_P_TopBottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //            }
            //            else
            //            {
            //                if(i < hMB -1)
            //                    Deblock_P_RowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                  if(firstRow != hMB -1)
            //                    Deblock_P_BottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                    Deblock_P_TopBottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //            }
            //STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            //        }
            //STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime)

            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;

            pCurMBRow[0]+= m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE;
            pCurMBRow[1]+= m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
            pCurMBRow[2]+= m_uiPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

            pFMBRow[0]+= m_uiForwardPlaneStep[0]*VC1_ENC_LUMA_SIZE;
            pFMBRow[1]+= m_uiForwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
            pFMBRow[2]+= m_uiForwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

        }


#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame && i == h)
            pDebug->PrintRestoredFrame(m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
            m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
            m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
#endif

        return err;
    }

    UMC::Status VC1EncoderPictureADV::VLC_P_FrameMixed(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status    err = UMC::UMC_OK;
        Ipp32s         i = 0, j = 0;
        Ipp32s         h = (m_pSequenceHeader->GetPictureHeight()+15)/16;
        Ipp32s         w = (m_pSequenceHeader->GetPictureWidth()+15)/16;
        Ipp32s         hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        const Ipp16u*  pCBPCYTable = VLCTableCBPCY_PB[m_uiCBPTab];

        eCodingSet     LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet     ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet     CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet* pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra])};

        const sACTablesSet* pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

        const Ipp32u*       pDCTableVLCIntra[6] = {DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],
            DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo          ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        const Ipp16s                (*pTTMBVLC)[4][6] =  0;
        const Ipp8u                 (* pTTBlkVLC)[6] = 0;
        const Ipp8u                 *pSubPattern4x4VLC=0;



        bool bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);
        bool bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR || m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst();
        pDebug->SetCurrMB(false, 0, firstRow);
#endif

        //err = WritePPictureHeader(pCodedPicture);
        //if (err != UMC::UMC_OK)
        //     return err;

        if (m_uiQuant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_uiQuant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->whole = 0;
                memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                m_MECurMbStat->qp = 2*m_uiQuant + m_bHalfQuant;

                pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
#endif

                switch  (pCompressedMB->GetMBType())
                {
                case VC1_ENC_P_MB_INTRA:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbIntra;
#endif
                    err = pCompressedMB->WritePMBMixed_INTRA (pCodedPicture,m_bRawBitplanes,m_uiQuant,
                        MVDiffTablesVLC[m_uiMVTab],
                        pCBPCYTable,
                        pDCTableVLCIntra,
                        pACTablesSetIntra,
                        &ACEscInfo);
                    VC1_ENC_CHECK (err)


                        break;

                case VC1_ENC_P_MB_1MV:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbFrw;
#endif
                    err = pCompressedMB->WritePMB1MVMixed(pCodedPicture, m_bRawBitplanes,m_uiMVTab,
                        m_uiMVRangeIndex, pCBPCYTable,
                        bCalculateVSTransform,
                        bMVHalf,
                        pTTMBVLC,
                        pTTBlkVLC,
                        pSubPattern4x4VLC,
                        pACTablesSetInter,
                        &ACEscInfo);
                    VC1_ENC_CHECK (err)
                        break;

                case VC1_ENC_P_MB_SKIP_1MV:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                    err = pCompressedMB->WritePMB1MVSkipMixed(pCodedPicture, m_bRawBitplanes);
                    VC1_ENC_CHECK (err)
                        break;
                case VC1_ENC_P_MB_SKIP_4MV:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                    err = pCompressedMB->WritePMB4MVSkipMixed(pCodedPicture, m_bRawBitplanes);
                    VC1_ENC_CHECK (err)
                        break;

                case VC1_ENC_P_MB_4MV:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbMixed;
#endif

                    err = pCompressedMB->WritePMB4MVMixed       (pCodedPicture,
                        m_bRawBitplanes,
                        m_uiQuant,
                        m_uiMVTab,
                        m_uiMVRangeIndex,
                        pCBPCYTable,
                        MVDiffTablesVLC[m_uiMVTab],
                        bCalculateVSTransform,
                        bMVHalf,
                        pTTMBVLC,
                        pTTBlkVLC,
                        pSubPattern4x4VLC,
                        pDCTableVLCIntra,
                        pACTablesSetIntra,
                        pACTablesSetInter,
                        &ACEscInfo);
                    VC1_ENC_CHECK (err)
                        break;
                }
#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)

            return err;
    }

    UMC::Status VC1EncoderPictureADV::PAC_BFrame(UMC::MeParams* MEParams, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status         err = UMC::UMC_OK;
        Ipp32s              i=0, j=0, blk = 0;

        Ipp8u*              pCurMBRow[3],* pFMB[3], *pBMB[3];
        Ipp32s              h = (m_pSequenceHeader->GetPictureHeight()+15)/16;
        Ipp32s              w = (m_pSequenceHeader->GetPictureWidth()+15)/16;
        Ipp32s              hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        _SetIntraInterLumaFunctions;

        CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())? GetChromaMVFast:GetChromaMV;
        // ------------------------------------------------------------------------------------------------ //

        bool                        bIsBilinearInterpolation = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR);
        InterpolateFunction         InterpolateLumaFunction  = (bIsBilinearInterpolation)?_own_ippiInterpolateQPBilinear_VC1_8u_C1R:_own_ippiInterpolateQPBicubic_VC1_8u_C1R;

        InterpolateFunctionPadding  InterpolateLumaFunctionPadding = (bIsBilinearInterpolation)? own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R:own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;

        IppVCInterpolate_8u         sYInterpolationF;
        IppVCInterpolate_8u         sUInterpolationF;
        IppVCInterpolate_8u         sVInterpolationF;
        IppVCInterpolate_8u         sYInterpolationB;
        IppVCInterpolate_8u         sUInterpolationB;
        IppVCInterpolate_8u         sVInterpolationB;

        IppVCInterpolateBlock_8u    InterpolateBlockYF;
        IppVCInterpolateBlock_8u    InterpolateBlockUF;
        IppVCInterpolateBlock_8u    InterpolateBlockVF;

        IppVCInterpolateBlock_8u    InterpolateBlockYB;
        IppVCInterpolateBlock_8u    InterpolateBlockUB;
        IppVCInterpolateBlock_8u    InterpolateBlockVB;



        Ipp8u                       tempInterpolationBufferF[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];
        Ipp8u                       tempInterpolationBufferB[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];
        IppStatus                   ippSts = ippStsNoErr;

        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];

        IppiSize                    blkSize     = {8,8};

        sCoordinate                 MVPredMin = {-60,-60};
        sCoordinate                 MVPredMax = {((Ipp16s)w*16-1)*4, ((Ipp16s)h*16-1)*4};

        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];
        Ipp16s                      defPredictor    =  0;

        bool                        bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR ||
            m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst();
        pDebug->SetCurrMB(false, 0, firstRow);
        pDebug->SetSliceInfo(firstRow, firstRow + nRows);
        pDebug->SetHalfCoef(bMVHalf);
        pDebug->SetDeblkFlag(false);
        pDebug->SetVTSFlag(false);
        pDebug->SetInterpolType(true);
#endif


        pCurMBRow[0]      =   m_pPlane[0]+ firstRow*m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE;        //Luma
        pCurMBRow[1]      =   m_pPlane[1]+ firstRow*m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE;        //Cb
        pCurMBRow[2]      =   m_pPlane[2]+ firstRow*m_uiPlaneStep[2]*VC1_ENC_CHROMA_SIZE;        //Cr

        pFMB[0]      =   m_pForwardPlane[0]+ firstRow*m_uiForwardPlaneStep[0]*VC1_ENC_LUMA_SIZE;        //Luma
        pFMB[1]      =   m_pForwardPlane[1]+ firstRow*m_uiForwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE;        //Cb
        pFMB[2]      =   m_pForwardPlane[2]+ firstRow*m_uiForwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE;        //Cr

        pBMB[0]      =   m_pBackwardPlane[0]+ firstRow*m_uiBackwardPlaneStep[0]*VC1_ENC_LUMA_SIZE;        //Luma
        pBMB[1]      =   m_pBackwardPlane[1]+ firstRow*m_uiBackwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE;        //Cb
        pBMB[2]      =   m_pBackwardPlane[2]+ firstRow*m_uiBackwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE;        //Cr

        err = m_pMBs->Reset();
        if (err != UMC::UMC_OK)
            return err;

        SetInterpolationParams(&sYInterpolationF,&sUInterpolationF,&sVInterpolationF,
            tempInterpolationBufferF,true);
        SetInterpolationParams(&sYInterpolationB,&sUInterpolationB,&sVInterpolationB,
            tempInterpolationBufferB,false);

        SetInterpolationParams(&InterpolateBlockYF,&InterpolateBlockUF,&InterpolateBlockVF, tempInterpolationBufferF,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            m_pForwardPlane, m_uiForwardPlaneStep, false);
        SetInterpolationParams(&InterpolateBlockYB,&InterpolateBlockUB,&InterpolateBlockVB, tempInterpolationBufferB,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            m_pBackwardPlane, m_uiBackwardPlaneStep, false);

#ifdef VC1_ENC_CHECK_MV
        err = CheckMEMV_B(MEParams, bMVHalf, firstRow, nRows);
        assert(err == UMC::UMC_OK);
#endif

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderMBInfo  *         pCurMBInfo     = 0;
                VC1EncoderMBData  *         pCurMBData     = 0;
                VC1EncoderMBData  *         pRecMBData     = 0;
                VC1EncoderCodedMB*          pCompressedMB  = &(m_pCodedMB[w*i+j]);
                Ipp32s                      posX           =  VC1_ENC_LUMA_SIZE*j;
                Ipp32s                      posXChroma     =  VC1_ENC_CHROMA_SIZE*j;
                Ipp8u                       MBPattern       = 0;
                Ipp8u                       CBPCY           = 0;
                eMBType                     mbType         =  VC1_ENC_B_MB_F; //From ME

                //Ipp32s                      posY        =  VC1_ENC_LUMA_SIZE*i;

                sCoordinate MVF         ={0,0};
                sCoordinate MVB         ={0,0};

                pCurMBInfo  =   m_pMBs->GetCurrMBInfo();
                pCurMBData  =   m_pMBs->GetCurrMBData();
                pRecMBData  =   m_pMBs->GetRecCurrMBData();

                VC1EncoderMBInfo* left        = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* topLeft     = m_pMBs->GetTopLeftMBInfo();
                VC1EncoderMBInfo* top         = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* topRight    = m_pMBs->GetTopRightMBInfo();

                MVF.x  = MEParams->pSrc->MBs[j + i*w].MV[0][0].x;
                MVF.y  = MEParams->pSrc->MBs[j + i*w].MV[0][0].y;
                MVB.x  = MEParams->pSrc->MBs[j + i*w].MV[1][0].x;
                MVB.y  = MEParams->pSrc->MBs[j + i*w].MV[1][0].y;

                switch (MEParams->pSrc->MBs[(j + i*w)].MbType)
                {
                case UMC::ME_MbIntra:
                    MVF.x = MVF.y = MVB.x = MVB.y = 0;
                    mbType = VC1_ENC_B_MB_INTRA;
                    break;

                case UMC::ME_MbFrw:

                    mbType = VC1_ENC_B_MB_F;
                    break;

                case UMC::ME_MbFrwSkipped:

                    mbType = VC1_ENC_B_MB_SKIP_F;
                    break;

                case UMC::ME_MbBkw:

                    mbType = VC1_ENC_B_MB_B;
                    break;
                case UMC::ME_MbBkwSkipped:

                    mbType = VC1_ENC_B_MB_SKIP_B;
                    break;
                case UMC::ME_MbBidir:

                    mbType = VC1_ENC_B_MB_FB;
                    break;

                case UMC::ME_MbBidirSkipped:

                    mbType = VC1_ENC_B_MB_SKIP_FB;
                    break;

                case UMC::ME_MbDirect:

                    mbType = VC1_ENC_B_MB_DIRECT;
                    break;
                case UMC::ME_MbDirectSkipped:

                    mbType = VC1_ENC_B_MB_SKIP_DIRECT;
                    break;
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
                }
                switch(mbType)
                {
                case VC1_ENC_B_MB_INTRA:
                    {
                        bool                dACPrediction   = true;
                        NeighbouringMBsData MBs;

                        MBs.LeftMB    = ((left)? left->isIntra():0)         ? m_pMBs->GetLeftMBData():0;
                        MBs.TopMB     = ((top)? top->isIntra():0)           ? m_pMBs->GetTopMBData():0;
                        MBs.TopLeftMB = ((topLeft)? topLeft->isIntra():0)   ? m_pMBs->GetTopLeftMBData():0;

                        pCompressedMB->Init(VC1_ENC_B_MB_INTRA);
                        pCurMBInfo->Init(true);

                        pCurMBData->CopyMBProg(pCurMBRow[0],m_uiPlaneStep[0],pCurMBRow[1], pCurMBRow[2], m_uiPlaneStep[1],j);

                        //only intra blocks:
                        for (blk = 0; blk<6; blk++)
                        {
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                            _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                        }
                        STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                        // should be changed on DCACPredictionPFrameSM
                        dACPrediction = DCACPredictionFrame     ( pCurMBData,&MBs,
                            pRecMBData, defPredictor,direction);
                        STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                        for (blk=0; blk<6; blk++)
                        {
                            pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                pRecMBData->m_uiBlockStep[blk],
                                VC1_Inter_8x8_Scan,
                                blk);
                        }
                        MBPattern = pCompressedMB->GetMBPattern();
                        CBPCY = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetACPrediction(dACPrediction);
                        pCompressedMB->SetMBCBPCY(CBPCY);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(VC1_ENC_B_MB_INTRA);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                        break;
                    }
                case VC1_ENC_B_MB_SKIP_F:
                    {
                        sCoordinate  *mvC=0,   *mvB=0,   *mvA=0;
                        sCoordinate  mv1={0,0}, mv2={0,0}, mv3={0,0};
                        sCoordinate  mvPred   = {0,0};

                        pCompressedMB->Init(mbType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        GetMVPredictionP(true);
                        PredictMV(mvA,mvB,mvC, &mvPred);
                        ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMin,MVPredMax);
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        //assert (mvPred.x == MVF.x && mvPred.y == MVF.y);

                        MVF.x = mvPred.x;  MVF.y = mvPred.y;

                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV(MVF,true);
                        pCurMBInfo->SetMV(MVB,false);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetMBAsSkip();
                        pDebug->SetMVInfo(&MVF, mvPred.x, mvPred.y, 0);
                        pDebug->SetMVInfo(&MVB, mvPred.x, mvPred.y, 1);
                        pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                        pDebug->SetIntrpMV(MVB.x, MVB.y, 1);
#endif
                        break;
                    }
                case VC1_ENC_B_MB_SKIP_B:
                    {
                        sCoordinate  *mvC=0,   *mvB=0,   *mvA=0;
                        sCoordinate  mv1={0,0}, mv2={0,0}, mv3={0,0};
                        sCoordinate  mvPred   = {0,0};

                        pCompressedMB->Init(mbType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        GetMVPredictionP(false);
                        PredictMV(mvA,mvB,mvC, &mvPred);
                        ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMin,MVPredMax);
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        assert (mvPred.x == MVB.x && mvPred.y == MVB.y);

                        MVB.x = mvPred.x;  MVB.y = mvPred.y;

                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV(MVF,true);
                        pCurMBInfo->SetMV(MVB,false);


#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetMBAsSkip();
                        pDebug->SetMVInfo(&MVF, mvPred.x, mvPred.y, 0);
                        pDebug->SetMVInfo(&MVB, mvPred.x, mvPred.y, 1);
                        pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                        pDebug->SetIntrpMV(MVB.x, MVB.y, 1);
#endif
                        break;
                    }
                case VC1_ENC_B_MB_F:
                case VC1_ENC_B_MB_B:
                    {
                        sCoordinate  *mvC=0,   *mvB=0,   *mvA=0;
                        sCoordinate  mv1={0,0}, mv2={0,0}, mv3={0,0};
                        sCoordinate  mvPred   = {0,0};
                        sCoordinate  mvDiff   = {0,0};
                        bool         bForward = (mbType == VC1_ENC_B_MB_F);

                        IppVCInterpolate_8u*         pYInterpolation= &sYInterpolationF;
                        IppVCInterpolate_8u*         pUInterpolation= &sUInterpolationF;
                        IppVCInterpolate_8u*         pVInterpolation= &sVInterpolationF;

                        IppVCInterpolateBlock_8u*    pInterpolateBlockY = &InterpolateBlockYF;
                        IppVCInterpolateBlock_8u*    pInterpolateBlockU = &InterpolateBlockUF;
                        IppVCInterpolateBlock_8u*    pInterpolateBlockV = &InterpolateBlockVF;

                        bool                         padded   = (m_uiForwardPlanePadding!=0);
                        Ipp8u                        opposite = (m_bForwardInterlace)?1:0;

                        Ipp8u**                      pPredMB   = pFMB;
                        Ipp32u*                      pPredStep = m_uiForwardPlaneStep;

                        sCoordinate*                 pMV                = &MVF;

                        sCoordinate                  MVInt             = {0,0};
                        sCoordinate                  MVQuarter         = {0,0};

                        sCoordinate                  MVChroma          = {0,0};
                        sCoordinate                  MVIntChroma       = {0,0};
                        sCoordinate                  MVQuarterChroma   = {0,0};


                        eTransformType  BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType};



                        if (!bForward)
                        {
                            pYInterpolation= &sYInterpolationB;
                            pUInterpolation= &sUInterpolationB;
                            pVInterpolation= &sVInterpolationB;

                            pInterpolateBlockY = &InterpolateBlockYB;
                            pInterpolateBlockU = &InterpolateBlockUB;
                            pInterpolateBlockV = &InterpolateBlockVB;
                            padded   = (m_uiBackwardPlanePadding!=0);
                            opposite = (m_bBackwardInterlace)?1:0;

                            pPredMB   = pBMB;
                            pPredStep = m_uiBackwardPlaneStep;
                            pMV       = &MVB;
                        }

                        pCompressedMB->Init(mbType);


                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV(MVF,true);
                        pCurMBInfo->SetMV(MVB,false);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        GetMVPredictionP(bForward);
                        PredictMV(mvA,mvB,mvC, &mvPred);
                        ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMin,MVPredMax);
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        mvDiff.x = (pMV->x - mvPred.x);
                        mvDiff.y = (pMV->y - mvPred.y);
                        pCompressedMB->SetdMV(mvDiff,bForward);

                        GetIntQuarterMV(*pMV,&MVInt,&MVQuarter);
                        CalculateChroma(*pMV,&MVChroma);
                        GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);
                        if (padded)
                        {
                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            SetInterpolationParamsLuma(pYInterpolation, pPredMB[0], pPredStep[0], &MVInt, &MVQuarter, j);
                            ippSts = InterpolateLumaFunction(pYInterpolation);
                            VC1_ENC_IPP_CHECK(ippSts);

                            InterpolateChroma (pUInterpolation, pVInterpolation, pPredMB[1], pPredMB[2],pPredStep[1], 
                                &MVIntChroma, &MVQuarterChroma, j);
                            
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                        }
                        else
                        {
                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                            SetInterpolationParamsLuma(pInterpolateBlockY, j, i , &MVInt, &MVQuarter);
                            ippSts = InterpolateLumaFunctionPadding(pInterpolateBlockY,0,0,opposite,false,m_uiRoundControl,0);
                            VC1_ENC_IPP_CHECK(ippSts);

                            InterpolateChromaPad   (pInterpolateBlockU, pInterpolateBlockV, 
                                                    opposite,false,m_uiRoundControl,0,
                                                    j, i ,  &MVIntChroma, &MVQuarterChroma);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime); 

                        }
                        if (bCalculateVSTransform)
                        {
#ifndef OWN_VS_TRANSFORM
                            GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                            VC1EncMD_P pIn;
                            pIn.pYSrc    = pCurMBRow[0]+posX; 
                            pIn.srcYStep = m_uiPlaneStep[0];
                            pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                            pIn.srcUStep = m_uiPlaneStep[1];
                            pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                            pIn.srcVStep = m_uiPlaneStep[2];

                            pIn.pYRef    = pYInterpolation->pDst; 
                            pIn.refYStep = pYInterpolation->dstStep;      
                            pIn.pURef    = pUInterpolation->pDst; 
                            pIn.refUStep = pUInterpolation->dstStep;
                            pIn.pVRef    = pVInterpolation->pDst; 
                            pIn.refVStep = pVInterpolation->dstStep;

                            pIn.quant         = doubleQuant;
                            pIn.bUniform      = m_bUniformQuant;
                            pIn.intraPattern  = 0;
                            pIn.DecTypeAC1    = m_uiDecTypeAC1;
                            pIn.pScanMatrix   = ZagTables_Adv;
                            pIn.bField        = 0;
                            pIn.CBPTab        = m_uiCBPTab;

                            GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif      

                        }
                        pCompressedMB->SetTSType(BlockTSTypes);

                        STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                        IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                        pCurMBData->CopyDiffMBProg (pCurMBRow[0],m_uiPlaneStep[0], pCurMBRow[1],pCurMBRow[2], m_uiPlaneStep[1], 
                                                    pYInterpolation->pDst, pYInterpolation->dstStep,
                                                    pUInterpolation->pDst,pVInterpolation->pDst, pUInterpolation->dstStep,
                                                    j,0);

                        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                        MBPattern = 0;
                        for (blk = 0; blk<6; blk++)
                        {
                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                            pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                pCurMBData->m_uiBlockStep[blk],
                                ZagTables_Adv[BlockTSTypes[blk]],
                                blk);
                        }
                        MBPattern   = pCompressedMB->GetMBPattern();
                        CBPCY       = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetMBCBPCY(CBPCY);
                        if (MBPattern==0 && mvDiff.x == 0 && mvDiff.y == 0)
                        {
                            if (bForward)
                            {
                                pCompressedMB->ChangeType(VC1_ENC_B_MB_SKIP_F);
                                mbType = VC1_ENC_B_MB_SKIP_F;
                            }
                            else
                            {
                                pCompressedMB->ChangeType(VC1_ENC_B_MB_SKIP_B);
                                mbType = VC1_ENC_B_MB_SKIP_B;
                            }
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetMBAsSkip();
#endif
                        }
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetMVInfo(&MVF, mvPred.x, mvPred.y, 0);
                        pDebug->SetMVInfo(&MVB, mvPred.x, mvPred.y, 1);
                        pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                        pDebug->SetIntrpMV(MVB.x, MVB.y, 1);

                        pDebug->SetMVInfo(&MVChroma,  0, 0, !bForward, 4);
                        pDebug->SetMVInfo(&MVChroma,  0, 0, !bForward, 5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        pDebug->SetInterpInfo(pYInterpolation,  pUInterpolation, pVInterpolation, !bForward, m_uiForwardPlanePadding);
#endif
                        break;
                    }
                case VC1_ENC_B_MB_FB:
                    {
                        sCoordinate  *mvC_F=0,   *mvB_F=0,   *mvA_F=0;
                        sCoordinate  *mvC_B=0,   *mvB_B=0,   *mvA_B=0;
                        sCoordinate  mv1={0,0}, mv2={0,0}, mv3={0,0};
                        sCoordinate  mv4={0,0}, mv5={0,0}, mv6={0,0};
                        sCoordinate  mvPredF    = {0,0};
                        sCoordinate  mvPredB    = {0,0};
                        sCoordinate  mvDiffF    = {0,0};
                        sCoordinate  mvDiffB    = {0,0};
                        sCoordinate  MVIntF     = {0,0};
                        sCoordinate  MVQuarterF = {0,0};
                        sCoordinate  MVIntB     = {0,0};
                        sCoordinate  MVQuarterB = {0,0};

                        sCoordinate  MVChromaF       = {0,0};
                        sCoordinate  MVIntChromaF    = {0,0};
                        sCoordinate  MVQuarterChromaF= {0,0};
                        sCoordinate  MVChromaB       = {0,0};
                        sCoordinate  MVIntChromaB    = {0,0};
                        sCoordinate  MVQuarterChromaB= {0,0};


                        eTransformType  BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType};


                        pCompressedMB->Init(mbType);


                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV(MVF,true);
                        pCurMBInfo->SetMV(MVB,false);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        GetMVPredictionB;
                        PredictMV(mvA_F,mvB_F,mvC_F, &mvPredF);
                        PredictMV(mvA_B,mvB_B,mvC_B, &mvPredB);
                        ScalePredict(&mvPredF, j*16*4,i*16*4,MVPredMin,MVPredMax);
                        ScalePredict(&mvPredB, j*16*4,i*16*4,MVPredMin,MVPredMax);
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        mvDiffF.x = (MVF.x - mvPredF.x);
                        mvDiffF.y = (MVF.y - mvPredF.y);
                        pCompressedMB->SetdMV(mvDiffF,true);

                        mvDiffB.x = (MVB.x - mvPredB.x);
                        mvDiffB.y = (MVB.y - mvPredB.y);
                        pCompressedMB->SetdMV(mvDiffB,false);

                        GetIntQuarterMV(MVF,&MVIntF,&MVQuarterF);
                        CalculateChroma(MVF,&MVChromaF);
                        GetIntQuarterMV(MVChromaF,&MVIntChromaF,&MVQuarterChromaF);

                        GetIntQuarterMV(MVB,&MVIntB,&MVQuarterB);
                        CalculateChroma(MVB,&MVChromaB);
                        GetIntQuarterMV(MVChromaB,&MVIntChromaB,&MVQuarterChromaB);

                        STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);

                        if (m_uiForwardPlanePadding)
                        {
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            SetInterpolationParamsLuma(&sYInterpolationF, pFMB[0], m_uiForwardPlaneStep[0], &MVIntF, &MVQuarterF, j);
                            ippSts = InterpolateLumaFunction(&sYInterpolationF);
                            VC1_ENC_IPP_CHECK(ippSts);

                            InterpolateChroma (&sUInterpolationF, &sVInterpolationF, pFMB[1], pFMB[2],m_uiForwardPlaneStep[1], 
                                &MVIntChromaF, &MVQuarterChromaF, j);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                
                        }
                        else
                        {                
                            bool bOpposite = (m_bForwardInterlace)? 1:0;

                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                            SetInterpolationParamsLuma(&InterpolateBlockYF, j, i , &MVIntF, &MVQuarterF);
                            ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYF,0,0,bOpposite,false,m_uiRoundControl,0);
                            VC1_ENC_IPP_CHECK(ippSts);

                            InterpolateChromaPad   (&InterpolateBlockUF, &InterpolateBlockVF, 
                                                    bOpposite,false,m_uiRoundControl,0,
                                                    j, i ,  &MVIntChromaF, &MVQuarterChromaF);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime); 

                        }
                        if (m_uiBackwardPlanePadding)
                        {
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                            SetInterpolationParamsLuma(&sYInterpolationB, pBMB[0], m_uiBackwardPlaneStep[0], &MVIntB, &MVQuarterB, j);
                            ippSts = InterpolateLumaFunction(&sYInterpolationB);
                            VC1_ENC_IPP_CHECK(ippSts);

                            InterpolateChroma (&sUInterpolationB, &sVInterpolationB, pBMB[1], pBMB[2],m_uiBackwardPlaneStep[1], 
                                                &MVIntChromaB, &MVQuarterChromaB, j);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                        }
                        else
                        {
                            bool bOpposite = (m_bBackwardInterlace)? 1:0;

                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                            SetInterpolationParamsLuma(&InterpolateBlockYB, j, i , &MVIntB, &MVQuarterB);
                            ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYB,0,0,bOpposite,false,m_uiRoundControl,0);
                            VC1_ENC_IPP_CHECK(ippSts);

                            InterpolateChromaPad   (&InterpolateBlockUB, &InterpolateBlockVB, 
                                                    bOpposite,false,m_uiRoundControl,0,
                                                    j, i ,  &MVIntChromaB, &MVQuarterChromaB);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);  
                        }

                        if (bCalculateVSTransform)
                        {
#ifndef OWN_VS_TRANSFORM
                            GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                            VC1EncMD_B pIn;
                            pIn.pYSrc    = pCurMBRow[0]+posX; 
                            pIn.srcYStep = m_uiPlaneStep[0];
                            pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                            pIn.srcUStep = m_uiPlaneStep[1];
                            pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                            pIn.srcVStep = m_uiPlaneStep[2];

                            pIn.pYRef[0]    = sYInterpolationF.pDst; 
                            pIn.refYStep[0] = sYInterpolationF.dstStep;      
                            pIn.pURef[0]    = sUInterpolationF.pDst; 
                            pIn.refUStep[0] = sUInterpolationF.dstStep;
                            pIn.pVRef[0]    = sVInterpolationF.pDst; 
                            pIn.refVStep[0] = sVInterpolationF.dstStep;

                            pIn.pYRef[1]    = sYInterpolationB.pDst; 
                            pIn.refYStep[1] = sYInterpolationB.dstStep;      
                            pIn.pURef[1]    = sUInterpolationB.pDst; 
                            pIn.refUStep[1] = sUInterpolationB.dstStep;
                            pIn.pVRef[1]    = sVInterpolationB.pDst; 
                            pIn.refVStep[1] = sVInterpolationB.dstStep;

                            pIn.quant         = doubleQuant;
                            pIn.bUniform      = m_bUniformQuant;
                            pIn.intraPattern  = 0;
                            pIn.DecTypeAC1    = m_uiDecTypeAC1;
                            pIn.pScanMatrix   = ZagTables_Adv;
                            pIn.bField        = 0;
                            pIn.CBPTab        = m_uiCBPTab;

                            GetVSTTypeB_RD (&pIn, BlockTSTypes)  ; 
#endif                   
                        }
                        pCompressedMB->SetTSType(BlockTSTypes);
                        STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                        IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                        pCurMBData->CopyBDiffMBProg( pCurMBRow[0],  m_uiPlaneStep[0], pCurMBRow[1],  pCurMBRow[2],  m_uiPlaneStep[1], 
                                                    sYInterpolationF.pDst,  sYInterpolationF.dstStep,
                                                    sUInterpolationF.pDst,  sVInterpolationF.pDst,   sUInterpolationF.dstStep,
                                                    sYInterpolationB.pDst,  sYInterpolationB.dstStep,
                                                    sUInterpolationB.pDst,  sVInterpolationB.pDst,   sUInterpolationB.dstStep,
                                                    j, 0);

                        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                        for (blk = 0; blk<6; blk++)
                        {
                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                            pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                pCurMBData->m_uiBlockStep[blk],
                                ZagTables_Adv[BlockTSTypes[blk]],
                                blk);
                        }
                        MBPattern   = pCompressedMB->GetMBPattern();
                        CBPCY       = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetMBCBPCY(CBPCY);
                        if (MBPattern==0 && mvDiffF.x == 0 && mvDiffF.y == 0 && mvDiffB.x == 0 && mvDiffB.y == 0)
                        {
                            pCompressedMB->ChangeType(VC1_ENC_B_MB_SKIP_FB);
                            mbType = VC1_ENC_B_MB_SKIP_FB;
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetMBAsSkip();
#endif
                        }

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetMVInfo(&MVF, mvPredF.x, mvPredF.y, 0);
                        pDebug->SetMVInfo(&MVB, mvPredB.x, mvPredB.y, 1);
                        pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                        pDebug->SetIntrpMV(MVB.x, MVB.y, 1);

                        pDebug->SetMVDiff(mvDiffF.x, mvDiffF.y, 0);
                        pDebug->SetMVDiff(mvDiffB.x, mvDiffB.y, 1);

                        pDebug->SetMVInfo(&MVChromaF,  0, 0, 0, 4);
                        pDebug->SetMVInfo(&MVChromaF,  0, 0, 0, 5);
                        pDebug->SetMVInfo(&MVChromaB,  0, 0, 1, 4);
                        pDebug->SetMVInfo(&MVChromaB,  0, 0, 1, 5);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        pDebug->SetInterpInfo(&sYInterpolationF,  &sUInterpolationF, &sVInterpolationF, 0, m_uiForwardPlanePadding);
                        pDebug->SetInterpInfo(&sYInterpolationB,  &sUInterpolationB, &sVInterpolationB, 1, m_uiBackwardPlanePadding);
#endif
                        break;
                    }
                case VC1_ENC_B_MB_SKIP_FB:
                    {
                        sCoordinate  *mvC_F=0,   *mvB_F=0,   *mvA_F=0;
                        sCoordinate  *mvC_B=0,   *mvB_B=0,   *mvA_B=0;
                        sCoordinate  mv1={0,0}, mv2={0,0}, mv3={0,0};
                        sCoordinate  mv4={0,0}, mv5={0,0}, mv6={0,0};
                        sCoordinate  mvPredF    = {0,0};
                        sCoordinate  mvPredB    = {0,0};

                        pCompressedMB->Init(mbType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        GetMVPredictionB
                            PredictMV(mvA_F,mvB_F,mvC_F, &mvPredF);
                        PredictMV(mvA_B,mvB_B,mvC_B, &mvPredB);
                        ScalePredict(&mvPredF, j*16*4,i*16*4,MVPredMin,MVPredMax);
                        ScalePredict(&mvPredB, j*16*4,i*16*4,MVPredMin,MVPredMax);
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        //assert (MVF.x == mvPredF.x && MVF.y == mvPredF.y &&
                        //        MVB.x == mvPredB.x && MVB.y == mvPredB.y);

                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV(mvPredF,true);
                        pCurMBInfo->SetMV(mvPredB,false);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetMBAsSkip();

                        pDebug->SetMVInfo(&MVF, mvPredF.x, mvPredF.y, 0);
                        pDebug->SetMVInfo(&MVB, mvPredB.x, mvPredB.y, 1);
                        pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                        pDebug->SetIntrpMV(MVB.x, MVB.y, 1);

                        //pDebug->SetMVInfo(MVChroma.x, MVChroma.y,  0, 0, !bForward, 4);
                        //pDebug->SetMVInfo(MVChroma.x, MVChroma.y,  0, 0, !bForward, 5);

#endif
                        break;
                    }
                case VC1_ENC_B_MB_DIRECT:
                    {
                        //sCoordinate  mvPredF    = {0,0};
                        //sCoordinate  mvPredB    = {0,0};
                        sCoordinate  MVIntF     = {0,0};
                        sCoordinate  MVQuarterF = {0,0};
                        sCoordinate  MVIntB     = {0,0};
                        sCoordinate  MVQuarterB = {0,0};

                        sCoordinate  MVChromaF       = {0,0};
                        sCoordinate  MVIntChromaF    = {0,0};
                        sCoordinate  MVQuarterChromaF= {0,0};
                        sCoordinate  MVChromaB       = {0,0};
                        sCoordinate  MVIntChromaB    = {0,0};
                        sCoordinate  MVQuarterChromaB= {0,0};

                        eTransformType  BlockTSTypes[6] = { m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType};

                        //direct
                        pCompressedMB->Init(mbType);


                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV(MVF,true);
                        pCurMBInfo->SetMV(MVB,false);

                        GetIntQuarterMV(MVF,&MVIntF,&MVQuarterF);
                        CalculateChroma(MVF,&MVChromaF);
                        GetIntQuarterMV(MVChromaF,&MVIntChromaF,&MVQuarterChromaF);

                        GetIntQuarterMV(MVB,&MVIntB,&MVQuarterB);
                        CalculateChroma(MVB,&MVChromaB);
                        GetIntQuarterMV(MVChromaB,&MVIntChromaB,&MVQuarterChromaB);

                        STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                        if (m_uiForwardPlanePadding)
                        {
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            SetInterpolationParamsLuma(&sYInterpolationF, pFMB[0], m_uiForwardPlaneStep[0], &MVIntF, &MVQuarterF, j);
                            ippSts = InterpolateLumaFunction(&sYInterpolationF);
                            VC1_ENC_IPP_CHECK(ippSts);

                            InterpolateChroma (&sUInterpolationF, &sVInterpolationF, pFMB[1], pFMB[2],m_uiForwardPlaneStep[1], 
                                &MVIntChromaF, &MVQuarterChromaF, j);
    
                             IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                        }
                        else
                        {
                            bool bOpposite = (m_bForwardInterlace)? 1:0;

                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                            SetInterpolationParamsLuma(&InterpolateBlockYF, j, i , &MVIntF, &MVQuarterF);
                            ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYF,0,0,bOpposite,false,m_uiRoundControl,0);
                            VC1_ENC_IPP_CHECK(ippSts);

                            InterpolateChromaPad   (&InterpolateBlockUF, &InterpolateBlockVF, 
                                                    bOpposite,false,m_uiRoundControl,0,
                                                    j, i ,  &MVIntChromaF, &MVQuarterChromaF);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);  

                        }

                        if (m_uiBackwardPlanePadding)
                        {
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            SetInterpolationParamsLuma(&sYInterpolationB, pBMB[0], m_uiBackwardPlaneStep[0], &MVIntB, &MVQuarterB, j);
                            ippSts = InterpolateLumaFunction(&sYInterpolationB);
                            VC1_ENC_IPP_CHECK(ippSts);

                            InterpolateChroma (&sUInterpolationB, &sVInterpolationB, pBMB[1], pBMB[2],m_uiBackwardPlaneStep[1], 
                                &MVIntChromaB, &MVQuarterChromaB, j);
                            
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                        }
                        else
                        {
                            bool bOpposite = (m_bBackwardInterlace)? 1:0;

                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                            SetInterpolationParamsLuma(&InterpolateBlockYB, j, i , &MVIntB, &MVQuarterB);
                            ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYB,0,0,bOpposite,false,m_uiRoundControl,0);
                            VC1_ENC_IPP_CHECK(ippSts);

                            InterpolateChromaPad   (&InterpolateBlockUB, &InterpolateBlockVB, 
                                                    bOpposite,false,m_uiRoundControl,0,
                                                    j, i ,  &MVIntChromaB, &MVQuarterChromaB);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);  


                        }
                        if (bCalculateVSTransform)
                        {
#ifndef OWN_VS_TRANSFORM
                            GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                            VC1EncMD_B pIn;
                            pIn.pYSrc    = pCurMBRow[0]+posX; 
                            pIn.srcYStep = m_uiPlaneStep[0];
                            pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                            pIn.srcUStep = m_uiPlaneStep[1];
                            pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                            pIn.srcVStep = m_uiPlaneStep[2];

                            pIn.pYRef[0]    = sYInterpolationF.pDst; 
                            pIn.refYStep[0] = sYInterpolationF.dstStep;      
                            pIn.pURef[0]    = sUInterpolationF.pDst; 
                            pIn.refUStep[0] = sUInterpolationF.dstStep;
                            pIn.pVRef[0]    = sVInterpolationF.pDst; 
                            pIn.refVStep[0] = sVInterpolationF.dstStep;

                            pIn.pYRef[1]    = sYInterpolationB.pDst; 
                            pIn.refYStep[1] = sYInterpolationB.dstStep;      
                            pIn.pURef[1]    = sUInterpolationB.pDst; 
                            pIn.refUStep[1] = sUInterpolationB.dstStep;
                            pIn.pVRef[1]    = sVInterpolationB.pDst; 
                            pIn.refVStep[1] = sVInterpolationB.dstStep;

                            pIn.quant         = doubleQuant;
                            pIn.bUniform      = m_bUniformQuant;
                            pIn.intraPattern  = 0;
                            pIn.DecTypeAC1    = m_uiDecTypeAC1;
                            pIn.pScanMatrix   = ZagTables_Adv;
                            pIn.bField        = 0;
                            pIn.CBPTab        = m_uiCBPTab;

                            GetVSTTypeB_RD (&pIn, BlockTSTypes)  ; 
#endif                   
                        }
                        pCompressedMB->SetTSType(BlockTSTypes);

                        STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                        IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                        pCurMBData->CopyBDiffMBProg( pCurMBRow[0],  m_uiPlaneStep[0], pCurMBRow[1],  pCurMBRow[2],  m_uiPlaneStep[1], 
                            sYInterpolationF.pDst,  sYInterpolationF.dstStep,
                            sUInterpolationF.pDst,  sVInterpolationF.pDst,   sUInterpolationF.dstStep,
                            sYInterpolationB.pDst,  sYInterpolationB.dstStep,
                            sUInterpolationB.pDst,  sVInterpolationB.pDst,   sUInterpolationB.dstStep,
                            j, 0);

                        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);


                        for (blk = 0; blk<6; blk++)
                        {
                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                            pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                pCurMBData->m_uiBlockStep[blk],
                                ZagTables_Adv[BlockTSTypes[blk]],
                                blk);
                        }
                        MBPattern   = pCompressedMB->GetMBPattern();
                        CBPCY       = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetMBCBPCY(CBPCY);


                        if (MBPattern==0)
                        {
                            pCompressedMB->ChangeType(VC1_ENC_B_MB_SKIP_DIRECT);
                            mbType = VC1_ENC_B_MB_SKIP_DIRECT;
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetMBAsSkip();
#endif
                        }

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMVInfo(&MVF, 0, 0, 0);
                        pDebug->SetMVInfo(&MVB, 0, 0, 1);
                        pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                        pDebug->SetIntrpMV(MVB.x, MVB.y, 1);
                        pDebug->SetMVInfo(&MVChromaF,  0, 0, 0, 4);
                        pDebug->SetMVInfo(&MVChromaF,  0, 0, 0, 5);
                        pDebug->SetMVInfo(&MVChromaB,  0, 0, 1, 4);
                        pDebug->SetMVInfo(&MVChromaB,  0, 0, 1, 5);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetMBType(mbType);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        pDebug->SetInterpInfo(&sYInterpolationF,  &sUInterpolationF, &sVInterpolationF, 0, m_uiForwardPlanePadding);
                        pDebug->SetInterpInfo(&sYInterpolationB,  &sUInterpolationB, &sVInterpolationB, 1, m_uiBackwardPlanePadding);
#endif
                        break;
                    }
                case VC1_ENC_B_MB_SKIP_DIRECT:
                    {
                        pCompressedMB->Init(mbType);
                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV(MVF,true);
                        pCurMBInfo->SetMV(MVB,false);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetMBAsSkip();
                        pDebug->SetMVInfo(&MVF, 0, 0, 0);
                        pDebug->SetMVInfo(&MVB, 0, 0, 1);
#endif
                        break;
                    }
                default:
                    return UMC::UMC_ERR_FAILED;
                }

                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }
            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;

            pCurMBRow[0]+= m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE;
            pCurMBRow[1]+= m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
            pCurMBRow[2]+= m_uiPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

            pFMB[0]+= m_uiForwardPlaneStep[0]*VC1_ENC_LUMA_SIZE;
            pFMB[1]+= m_uiForwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
            pFMB[2]+= m_uiForwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

            pBMB[0]+= m_uiBackwardPlaneStep[0]*VC1_ENC_LUMA_SIZE;
            pBMB[1]+= m_uiBackwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
            pBMB[2]+= m_uiBackwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE;
        }


        return err;
    }


    UMC::Status VC1EncoderPictureADV::PACBField(UMC::MeParams* MEParams, bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status         err = UMC::UMC_OK;
        Ipp32s              i=0, j=0, blk = 0;
        bool                bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);
        bool                bBottom     = ((m_bTopFieldFirst && bSecondField) || ((!m_bTopFieldFirst)&&(!bSecondField)));

        Ipp32u              pCurMBStep[3];
        Ipp8u*              pCurMBRow[3];
        Ipp32u              pRaisedMBStep[3];
        Ipp8u*              pRaisedMBRow[3];


        Ipp32s              h   = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s              w   = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s              hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        VC1EncoderCodedMB*  pCodedMB = &(m_pCodedMB[w*h*bSecondField]);


        Ipp8u*              pForwMBRow  [2][3]  = {0};
        Ipp32u              pForwMBStep [2][3]  = {0};
        Ipp8u*              pBackwMBRow  [2][3]  = {0};
        Ipp32u              pBackwMBStep [2][3]  = {0};

        _SetIntraInterLumaFunctions;
        //inverse transform quantization
        IntraInvTransformQuantFunction IntraInvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :
            IntraInvTransformQuantNonUniform;

        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (m_bUniformQuant) ? InterInvTransformQuantUniform :
            InterInvTransformQuantNonUniform;
        CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())? GetChromaMVFast:GetChromaMV;
        // ------------------------------------------------------------------------------------------------ //

        bool                        bIsBilinearInterpolation = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR);
        InterpolateFunction         InterpolateLumaFunction  = (bIsBilinearInterpolation)?_own_ippiInterpolateQPBilinear_VC1_8u_C1R:   _own_ippiInterpolateQPBicubic_VC1_8u_C1R;

        InterpolateFunctionPadding  InterpolateLumaFunctionPadding = (bIsBilinearInterpolation)?own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R: own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;

        IppVCInterpolate_8u         sYInterpolationF;
        IppVCInterpolate_8u         sUInterpolationF;
        IppVCInterpolate_8u         sVInterpolationF;
        IppVCInterpolate_8u         sYInterpolationB;
        IppVCInterpolate_8u         sUInterpolationB;
        IppVCInterpolate_8u         sVInterpolationB;

        IppVCInterpolateBlock_8u    InterpolateBlockYF[2];
        IppVCInterpolateBlock_8u    InterpolateBlockUF[2];
        IppVCInterpolateBlock_8u    InterpolateBlockVF[2];

        IppVCInterpolateBlock_8u    InterpolateBlockYB[2];
        IppVCInterpolateBlock_8u    InterpolateBlockUB[2];
        IppVCInterpolateBlock_8u    InterpolateBlockVB[2];

        Ipp8u                       tempInterpolationBufferF[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];
        Ipp8u                       tempInterpolationBufferB[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];
        IppStatus                   ippSts = ippStsNoErr;

        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];


        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];
        Ipp16s                      defPredictor    =  0;

        IppiSize                    blkSize     = {8,8};
        IppiSize                    blkSizeLuma = {16,16};

        bool                        bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR ||
            m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

        sScaleInfo                  scInfoForw;
        sScaleInfo                  scInfoBackw;

        Ipp16s*                     pSavedMV        =  (m_pSavedMV)?  m_pSavedMV + w*2*(h*bSecondField + firstRow):0;
        Ipp8u*                      pRefType        =  (m_pRefType)?  m_pRefType + w*(h*bSecondField   + firstRow):0;

        Ipp8u                       scaleFactor  = (Ipp8u)((BFractionScaleFactor[m_uiBFraction.denom][m_uiBFraction.num]*(Ipp32u)m_nReferenceFrameDist)>>8);

        Ipp8u deblkPattern = (firstRow != hMB - 1)? 0 : 0x8;   //4 bits: right 1 bit - 0 - left/1 - not left,
        //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row, 10 - top bottom row
        //middle 1 bit - 1 - right/0 - not right

        bool                        bSubBlkTS           = m_pSequenceHeader->IsVSTransform() && (!(m_bVSTransform &&m_uiTransformType==VC1_ENC_8x8_TRANSFORM));

        Ipp8u deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFC : 0;

        assert (m_uiReferenceFieldType == VC1_ENC_REF_FIELD_BOTH );
        assert (pSavedMV && pRefType );

        // backward, second field, second reference
        Ipp32u   padding[2][2][2] = {{{m_uiForwardPlanePadding, m_uiForwardPlanePadding}, // first field
        {m_uiRaisedPlanePadding,  m_uiForwardPlanePadding}},// second field
        {{m_uiBackwardPlanePadding, m_uiBackwardPlanePadding}, // first field
        {m_uiBackwardPlanePadding,  m_uiBackwardPlanePadding}}, };// second field

        // backward, second field, second reference
        Ipp32u   oppositePadding[2][2][2] = {{{(m_bForwardInterlace)? 0:1,(m_bForwardInterlace)? 0:1 },
        {0,(m_bForwardInterlace)? 0:1}},
        {{(m_bBackwardInterlace)? 0:1,(m_bBackwardInterlace)? 0:1 },
        {(m_bBackwardInterlace)? 0:1,(m_bBackwardInterlace)? 0:1}}};


        InitScaleInfo(&scInfoForw, bSecondField,bBottom,(Ipp8u)scaleFactor,m_uiMVRangeIndex);
        InitScaleInfoBackward(&scInfoBackw,bSecondField,bBottom,((m_nReferenceFrameDist - scaleFactor-1)>=0)?(m_nReferenceFrameDist - scaleFactor-1):0,m_uiMVRangeIndex);

#ifdef VC1_ENC_DEBUG_ON
        Ipp32u              fieldShift = (bSecondField)? h : 0;

        pDebug->SetRefNum(2);
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
        pDebug->SetSliceInfo(firstRow + fieldShift, firstRow + nRows + fieldShift);
        pDebug->SetHalfCoef(bMVHalf);
        pDebug->SetVTSFlag(bSubBlkTS);
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
#endif
        err = m_pMBs->Reset();
        if (err != UMC::UMC_OK)
            return err;

        SetInterpolationParams(&sYInterpolationF,&sUInterpolationF,&sVInterpolationF,
            tempInterpolationBufferF,true, true);
        SetInterpolationParams(&sYInterpolationB,&sUInterpolationB,&sVInterpolationB,
            tempInterpolationBufferB,false, true);

        SetFieldStep(pCurMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pCurMBRow, m_pPlane, m_uiPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(pRaisedMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pRaisedMBRow, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);

        err = Set2RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep, bSecondField, bBottom, 0);
        VC1_ENC_CHECK(err);

        err = SetBkwFieldPlane(pBackwMBRow, pBackwMBStep,
            m_pBackwardPlane, m_uiBackwardPlaneStep, bBottom, 0);
        VC1_ENC_CHECK(err);

        SetInterpolationParams(&InterpolateBlockYF[0],&InterpolateBlockUF[0],&InterpolateBlockVF[0], tempInterpolationBufferF,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[0], pForwMBStep[0], true);

        SetInterpolationParams(&InterpolateBlockYF[1],&InterpolateBlockUF[1],&InterpolateBlockVF[1], tempInterpolationBufferF,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[1], pForwMBStep[1], true);

        SetInterpolationParams(&InterpolateBlockYB[0],&InterpolateBlockUB[0],&InterpolateBlockVB[0], tempInterpolationBufferB,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pBackwMBRow[0], pBackwMBStep[0], true);

        SetInterpolationParams(&InterpolateBlockYB[1],&InterpolateBlockUB[1],&InterpolateBlockVB[1], tempInterpolationBufferB,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pBackwMBRow[1], pBackwMBStep[1], true);

        err = Set2RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep, bSecondField, bBottom, firstRow);
        VC1_ENC_CHECK(err);

        err = SetBkwFieldPlane(pBackwMBRow, pBackwMBStep,
            m_pBackwardPlane, m_uiBackwardPlaneStep, bBottom, firstRow);
        VC1_ENC_CHECK(err);


#ifdef VC1_ENC_CHECK_MV
        err = CheckMEMV_BField(MEParams, bMVHalf, firstRow, nRows);
        assert(err == UMC::UMC_OK);
#endif

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderMBInfo  *         pCurMBInfo     = 0;
                VC1EncoderMBData  *         pCurMBData     = 0;
                VC1EncoderMBData  *         pRecMBData = 0;
                VC1EncoderCodedMB*          pCompressedMB  = &(pCodedMB[w*i+j]);
                Ipp32s                      posX           =  VC1_ENC_LUMA_SIZE*j;
                Ipp32s                      posXChroma     =  VC1_ENC_CHROMA_SIZE*j;
                Ipp8u                       MBPattern       = 0;
                Ipp8u                       CBPCY           = 0;
                eMBType                     mbType         =  VC1_ENC_B_MB_F; //From ME

                sCoordinate MVF         ={0};
                sCoordinate MVB         ={0};

                pCurMBInfo  =   m_pMBs->GetCurrMBInfo();
                pCurMBData  =   m_pMBs->GetCurrMBData();
                pRecMBData  =   m_pMBs->GetRecCurrMBData();

                VC1EncoderMBInfo* left        = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* topLeft     = m_pMBs->GetTopLeftMBInfo();
                VC1EncoderMBInfo* top         = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* topRight    = m_pMBs->GetTopRightMBInfo();

                switch (MEParams->pSrc->MBs[(j + i*w)].MbType)
                {
                case UMC::ME_MbIntra:
                    mbType = VC1_ENC_B_MB_INTRA;
                    break;
                case UMC::ME_MbFrw:

                    mbType = VC1_ENC_B_MB_F;
                    MVF.x  = MEParams->pSrc->MBs[j + i*w].MV[0][0].x;
                    MVF.y  = MEParams->pSrc->MBs[j + i*w].MV[0][0].y;
                    MVF.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    break;

                case UMC::ME_MbFrwSkipped:

                    mbType = VC1_ENC_B_MB_SKIP_F;
                    MVF.x  = MEParams->pSrc->MBs[j + i*w].MV[0][0].x;
                    MVF.y  = MEParams->pSrc->MBs[j + i*w].MV[0][0].y;
                    MVF.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    break;

                case UMC::ME_MbBkw:

                    mbType = VC1_ENC_B_MB_B;
                    MVB.x  = MEParams->pSrc->MBs[j + i*w].MV[1][0].x;
                    MVB.y  = MEParams->pSrc->MBs[j + i*w].MV[1][0].y;
                    MVB.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;
                case UMC::ME_MbBkwSkipped:

                    mbType = VC1_ENC_B_MB_SKIP_B;
                    MVB.x  = MEParams->pSrc->MBs[j + i*w].MV[1][0].x;
                    MVB.y  = MEParams->pSrc->MBs[j + i*w].MV[1][0].y;
                    MVB.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;
                case UMC::ME_MbBidir:

                    mbType = VC1_ENC_B_MB_FB;
                    MVF.x  = MEParams->pSrc->MBs[j + i*w].MV[0][0].x;
                    MVF.y  = MEParams->pSrc->MBs[j + i*w].MV[0][0].y;
                    MVF.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    MVB.x  = MEParams->pSrc->MBs[j + i*w].MV[1][0].x;
                    MVB.y  = MEParams->pSrc->MBs[j + i*w].MV[1][0].y;
                    MVB.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;

                case UMC::ME_MbBidirSkipped:

                    mbType = VC1_ENC_B_MB_SKIP_FB;

                    MVF.x  = MEParams->pSrc->MBs[j + i*w].MV[0][0].x;
                    MVF.y  = MEParams->pSrc->MBs[j + i*w].MV[0][0].y;
                    MVF.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    MVB.x  = MEParams->pSrc->MBs[j + i*w].MV[1][0].x;
                    MVB.y  = MEParams->pSrc->MBs[j + i*w].MV[1][0].y;
                    MVB.bSecond =  (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;

                case UMC::ME_MbDirect:

                    mbType = VC1_ENC_B_MB_DIRECT;

                    MVF.x  =  MEParams->pSrc->MBs[j + i*w].MV[0][0].x;//MEParams.MVDirectFW[(j + i*w)].x;
                    MVF.y  =  MEParams->pSrc->MBs[j + i*w].MV[0][0].y;//MEParams.MVDirectFW[(j + i*w)].y;
                    MVF.bSecond =(MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    MVB.x  =  MEParams->pSrc->MBs[j + i*w].MV[1][0].x;//MEParams.MVDirectBW[(j + i*w)].x;
                    MVB.y  =  MEParams->pSrc->MBs[j + i*w].MV[1][0].y;//MEParams.MVDirectBW[(j + i*w)].y;
                    MVB.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;
                case UMC::ME_MbDirectSkipped:

                    mbType = VC1_ENC_B_MB_SKIP_DIRECT;

                    MVF.x  =  MEParams->pSrc->MBs[j + i*w].MV[0][0].x;//MEParams.MVDirectFW[(j + i*w)].x;
                    MVF.y  =  MEParams->pSrc->MBs[j + i*w].MV[0][0].y;//MEParams.MVDirectFW[(j + i*w)].y;
                    MVF.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    MVB.x  =  MEParams->pSrc->MBs[j + i*w].MV[1][0].x;//MEParams.MVDirectBW[(j + i*w)].x;
                    MVB.y  =  MEParams->pSrc->MBs[j + i*w].MV[1][0].y;//MEParams.MVDirectBW[(j + i*w)].y;
                    MVB.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
                }
                switch(mbType)
                {
                case VC1_ENC_B_MB_INTRA:
                    {
                        bool                dACPrediction   = true;
                        NeighbouringMBsData MBs;

                        MBs.LeftMB    = ((left)? left->isIntra():0)         ? m_pMBs->GetLeftMBData():0;
                        MBs.TopMB     = ((top)? top->isIntra():0)           ? m_pMBs->GetTopMBData():0;
                        MBs.TopLeftMB = ((topLeft)? topLeft->isIntra():0)   ? m_pMBs->GetTopLeftMBData():0;

                        pCompressedMB->Init(VC1_ENC_B_MB_INTRA);
                        pCurMBInfo->Init(true);

                        pCurMBData->CopyMBProg(pCurMBRow[0],pCurMBStep[0],pCurMBRow[1], pCurMBRow[2], pCurMBStep[1],j);

                        //only intra blocks:
                        for (blk = 0; blk<6; blk++)
                        {
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                            _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                        }
                        STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                        // should be changed on DCACPredictionPFrameSM
                        dACPrediction = DCACPredictionFrame     ( pCurMBData,&MBs,
                            pRecMBData, defPredictor,direction);
                        STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                        for (blk=0; blk<6; blk++)
                        {
                            pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                pRecMBData->m_uiBlockStep[blk],
                                VC1_Inter_InterlaceIntra_8x8_Scan_Adv,
                                blk);
                        }
                        MBPattern = pCompressedMB->GetMBPattern();
                        CBPCY = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetACPrediction(dACPrediction);
                        pCompressedMB->SetMBCBPCY(CBPCY);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(VC1_ENC_B_MB_INTRA);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            for (blk=0;blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                    DCQuant, doubleQuant);
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],blkSize, 0);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pRecMBData->PasteMBProg(pRaisedMBRow[0],pRaisedMBStep[0],pRaisedMBRow[1],pRaisedMBRow[2],pRaisedMBStep[1],j);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            //deblocking
                            {
                                STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};

                                pCurMBInfo->SetExternalEdgeHor(0,0,0);
                                pCurMBInfo->SetExternalEdgeVer(0,0,0);
                                pCurMBInfo->SetInternalEdge(0,0);

                                pCurMBInfo->SetInternalBlockEdge(15, 15, 15, 15, //hor
                                    15, 15, 15, 15);// ver


                                m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);
                        break;
                    }

                case VC1_ENC_B_MB_F:
                case VC1_ENC_B_MB_B:
                case VC1_ENC_B_MB_SKIP_F:
                case VC1_ENC_B_MB_SKIP_B:
                    {
                        sCoordinate  *mvC=0,   *mvB=0,   *mvA=0;
                        sCoordinate  mv1={0}, mv2={0}, mv3={0};
                        sCoordinate  mvAEx = {0}, mvCEx = {0};
                        sCoordinate  mvPred[2]   = {0};
                        sCoordinate  mvPred1[2]   = {0};
                        sCoordinate  mvDiff   = {0};
                        bool         bForward = ((mbType == VC1_ENC_B_MB_F )|| (mbType == VC1_ENC_B_MB_SKIP_F));
                        bool         bSecondDominantPred = false;
                        bool         bSecondDominantPred1 = false;
                        bool         bSkip = ((mbType == VC1_ENC_B_MB_SKIP_B )|| (mbType == VC1_ENC_B_MB_SKIP_F));

                        IppVCInterpolate_8u*         pYInterpolation= &sYInterpolationF;
                        IppVCInterpolate_8u*         pUInterpolation= &sUInterpolationF;
                        IppVCInterpolate_8u*         pVInterpolation= &sVInterpolationF;
                        IppVCInterpolateBlock_8u*   pInterpolateBlockY = InterpolateBlockYF;
                        IppVCInterpolateBlock_8u*   pInterpolateBlockU = InterpolateBlockUF;
                        IppVCInterpolateBlock_8u*   pInterpolateBlockV = InterpolateBlockVF;


                        sCoordinate*                 pMV                = &MVF;
                        sCoordinate*                 pMV1               = &MVB;

                        Ipp8u**                      pPredMB   = pForwMBRow[pMV->bSecond];
                        Ipp32u*                      pPredStep = pForwMBStep[pMV->bSecond];

                        sCoordinate                  MVInt             = {0};
                        sCoordinate                  MVQuarter         = {0};

                        sCoordinate                  MVChroma          = {0};
                        sCoordinate                  MVIntChroma       = {0};
                        sCoordinate                  MVQuarterChroma   = {0};
                        sScaleInfo *                 pInfo             = &scInfoForw;
                        sScaleInfo *                 pInfo1            = &scInfoBackw;
                        eTransformType  BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType};



                        if (!bForward)
                        {
                            pMV       = &MVB;
                            pMV1      = &MVF;
                            pYInterpolation = &sYInterpolationB;
                            pUInterpolation = &sUInterpolationB;
                            pVInterpolation = &sVInterpolationB;
                            pPredMB         = pBackwMBRow [pMV->bSecond];
                            pPredStep       = pBackwMBStep[pMV->bSecond];
                            pInfo           = &scInfoBackw;
                            pInfo1          = &scInfoForw;
                            pInterpolateBlockY = InterpolateBlockYB;
                            pInterpolateBlockU = InterpolateBlockUB;
                            pInterpolateBlockV = InterpolateBlockVB;
                        }

                        pCompressedMB->Init(mbType);


                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        GetMVPredictionPField(bForward);
                        bSecondDominantPred =  PredictMVField2(mvA,mvB,mvC, mvPred, pInfo,bSecondField,&mvAEx,&mvCEx, !bForward, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPred, bForward);
#endif

                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetPredFlag(bSecondDominantPred!=pMV->bSecond, !bForward);
                        pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == pMV->bSecond || mvA==0)? mvA:&mvAEx,
                            /*mvB,*/ (mvC && mvC->bSecond == pMV->bSecond || mvC==0)? mvC:&mvCEx,!bForward);
                        pDebug->SetHybrid(0, !bForward);
#endif

                        // The second vector is restored by prediction
                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);

                        GetMVPredictionPField(!bForward)
                            bSecondDominantPred1 =  PredictMVField2(mvA,mvB,mvC, mvPred1, pInfo1,bSecondField,&mvAEx,&mvCEx, bForward, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPred, !bForward);
#endif
                        pMV1->x         = mvPred1[bSecondDominantPred1].x;
                        pMV1->y         = mvPred1[bSecondDominantPred1].y;
                        pMV1->bSecond   = bSecondDominantPred1;

                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        if (bSkip )
                        {
                            assert (pMV->x == mvPred[pMV->bSecond].x && pMV->y == mvPred[pMV->bSecond].y);
                            pMV->x = mvPred[pMV->bSecond].x; //ME correction
                            pMV->y = mvPred[pMV->bSecond].y; //ME correction
                        }
                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV_F(MVF,true);
                        pCurMBInfo->SetMV_F(MVB,false);

                        mvDiff.x = (pMV->x - mvPred[pMV->bSecond].x);
                        mvDiff.y = (pMV->y - mvPred[pMV->bSecond].y);
                        mvDiff.bSecond = (pMV->bSecond != bSecondDominantPred);
                        pCompressedMB->SetdMV_F(mvDiff,bForward);
                        if (!bSkip || bRaiseFrame)
                        {
                            sCoordinate t = {pMV->x,pMV->y + ((!pMV->bSecond )? (2-4*(!bBottom)):0),pMV->bSecond};

                            GetIntQuarterMV(t,&MVInt,&MVQuarter);
                            CalculateChroma(*pMV,&MVChroma);
                            pCurMBInfo->SetMVChroma_F(MVChroma);

                            MVChroma.y = (!pMV->bSecond )? MVChroma.y + (2-4*(!bBottom)):MVChroma.y;
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);

#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(t.x, t.y, !bForward);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, !bForward, 4);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, !bForward, 5);
#endif
                            Ipp32u ind = (bSecondField)? 1: 0;
                            if (padding[!bForward][ind][pMV->bSecond])
                            {

                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(pYInterpolation, pPredMB[0], pPredStep[0], &MVInt, &MVQuarter, j);
                                ippSts = InterpolateLumaFunction(pYInterpolation);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (pUInterpolation, pVInterpolation, pPredMB[1], pPredMB[2],pPredStep[1], 
                                    &MVIntChroma, &MVQuarterChroma, j);


                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            }
                            else
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&pInterpolateBlockY[pMV->bSecond], j, i , &MVInt, &MVQuarter);
                                ippSts = InterpolateLumaFunctionPadding(&pInterpolateBlockY[pMV->bSecond],0,0,oppositePadding[!bForward][ind][pMV->bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&pInterpolateBlockU[pMV->bSecond], &pInterpolateBlockV[pMV->bSecond], 
                                                        oppositePadding[!bForward][ind][pMV->bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChroma, &MVQuarterChroma);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime); 

                            }

                        }
                        if (!bSkip)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+posX; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = pYInterpolation->pDst; 
                                pIn.refYStep = pYInterpolation->dstStep;      
                                pIn.pURef    = pUInterpolation->pDst; 
                                pIn.refUStep = pUInterpolation->dstStep;
                                pIn.pVRef    = pVInterpolation->pDst; 
                                pIn.refVStep = pVInterpolation->dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif          
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],pCurMBStep[0], pCurMBRow[1],pCurMBRow[2], pCurMBStep[1], 
                                pYInterpolation->pDst, pYInterpolation->dstStep,
                                pUInterpolation->pDst,pVInterpolation->pDst, pUInterpolation->dstStep,
                                j,0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);


                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](  pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern   = pCompressedMB->GetMBPattern();
                            CBPCY       = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                        }
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetMVInfoField(&MVF, mvPred[MVF.bSecond].x, mvPred[MVF.bSecond].y, 0);
                        pDebug->SetMVInfoField(&MVB, mvPred[MVB.bSecond].x, mvPred[MVB.bSecond].y, 1);

                        pDebug->SetMVInfoField(&MVChroma,  0, 0, !bForward, 4);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, !bForward, 5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);

                        Ipp32u ind = (bSecondField)? 1: 0;
                        pDebug->SetInterpInfo(pYInterpolation,  pUInterpolation, pVInterpolation, !bForward, padding[!bForward][ind][pMV->bSecond]);
#endif
                        /*---------------------------Reconstruction ------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumMBProg (pYInterpolation->pDst, pYInterpolation->dstStep, 
                                                            pUInterpolation->pDst, pVInterpolation->pDst,pUInterpolation->dstStep, 
                                                            pRaisedMBRow[0], pRaisedMBStep[0], 
                                                            pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                                            0,j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumSkipMBProg (pYInterpolation->pDst,pYInterpolation->dstStep,
                                                                pUInterpolation->pDst, pVInterpolation->pDst, pUInterpolation->dstStep,
                                                                pRaisedMBRow[0], pRaisedMBStep[0], 
                                                                pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],0,j);  
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pCurMBInfo->SetExternalEdgeHor(0,0,0);
                                pCurMBInfo->SetExternalEdgeVer(0,0,0);
                                pCurMBInfo->SetInternalEdge(0,0);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }// DEBLOCKING

                            STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                            Ipp32u ind = (bSecondField)? 1: 0;
                            pDebug->SetInterpInfo(pYInterpolation, pUInterpolation, pVInterpolation, 0, padding[!bForward][ind][pMV->bSecond]);
#endif
                        } //bRaiseFrame
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);
                        break;
                    }
                case VC1_ENC_B_MB_FB:
                case VC1_ENC_B_MB_SKIP_FB:
                    {
                        sCoordinate  *mvC=0,   *mvB=0,   *mvA=0;

                        sCoordinate  mv1={0}, mv2={0}, mv3={0};
                        sCoordinate  mvAEx = {0};
                        sCoordinate  mvCEx = {0};

                        sCoordinate  mvPredB[2] = {0};
                        sCoordinate  mvPredF[2] = {0};
                        sCoordinate  mvDiffF    = {0};
                        sCoordinate  mvDiffB    = {0};
                        sCoordinate  MVIntF     = {0};
                        sCoordinate  MVQuarterF = {0};
                        sCoordinate  MVIntB     = {0};
                        sCoordinate  MVQuarterB = {0};

                        sCoordinate  MVChromaF       = {0};
                        sCoordinate  MVIntChromaF    = {0};
                        sCoordinate  MVQuarterChromaF= {0};
                        sCoordinate  MVChromaB       = {0};
                        sCoordinate  MVIntChromaB    = {0};
                        sCoordinate  MVQuarterChromaB= {0};

                        bool         bSecondDominantPredF = false;
                        bool         bSecondDominantPredB = false;

                        eTransformType  BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType};

                        pCompressedMB->Init(mbType);


                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        GetMVPredictionPField(true)
                            bSecondDominantPredF =  PredictMVField2(mvA,mvB,mvC, mvPredF, &scInfoForw,bSecondField,&mvAEx,&mvCEx, false, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPredF, 0);
#endif
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetPredFlag(bSecondDominantPredF!=MVF.bSecond, 0);
                        pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == MVF.bSecond || mvA==0)? mvA:&mvAEx,
                            /*mvB,*/ (mvC && mvC->bSecond == MVF.bSecond || mvC==0)? mvC:&mvCEx,0);
                        pDebug->SetHybrid(0, 0);
#endif
                        GetMVPredictionPField(false)
                            bSecondDominantPredB =  PredictMVField2(mvA,mvB,mvC, mvPredB, &scInfoBackw,bSecondField,&mvAEx,&mvCEx, true, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPredB, 1);
#endif
                        //ScalePredict(&mvPredF, j*16*4,i*16*4,MVPredMin,MVPredMax);
                        //ScalePredict(&mvPredB, j*16*4,i*16*4,MVPredMin,MVPredMax);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetPredFlag(bSecondDominantPredB!=MVB.bSecond, 1);
                        pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == MVB.bSecond || mvA==0)? mvA:&mvAEx,
                            /*mvB,*/ (mvC && mvC->bSecond == MVB.bSecond || mvC==0)? mvC:&mvCEx,1);
                        pDebug->SetHybrid(0, 1);
#endif
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);
                        if  (mbType == VC1_ENC_B_MB_SKIP_FB)
                        {
                            assert (MVF.x == mvPredF[MVF.bSecond].x && MVF.y == mvPredF[MVF.bSecond].y);
                            assert (MVB.x == mvPredB[MVB.bSecond].x && MVB.y == mvPredB[MVB.bSecond].y);
                            MVF.x = mvPredF[MVF.bSecond].x ;
                            MVF.y = mvPredF[MVF.bSecond].y ;
                            MVB.x = mvPredB[MVB.bSecond].x ;
                            MVB.y = mvPredB[MVB.bSecond].y ;
                        }

                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV_F(MVF,true);
                        pCurMBInfo->SetMV_F(MVB,false);

                        mvDiffF.x = (MVF.x - mvPredF[MVF.bSecond].x);
                        mvDiffF.y = (MVF.y - mvPredF[MVF.bSecond].y);
                        mvDiffF.bSecond = (MVF.bSecond != bSecondDominantPredF);
                        pCompressedMB->SetdMV_F(mvDiffF,true);

                        mvDiffB.x = (MVB.x - mvPredB[MVB.bSecond].x);
                        mvDiffB.y = (MVB.y - mvPredB[MVB.bSecond].y);
                        mvDiffB.bSecond =(MVB.bSecond != bSecondDominantPredB);
                        pCompressedMB->SetdMV_F(mvDiffB,false);

                        if (mbType != VC1_ENC_B_MB_SKIP_FB || bRaiseFrame)
                        {
                            sCoordinate tF = {MVF.x,MVF.y + ((!MVF.bSecond )? (2-4*(!bBottom)):0),MVF.bSecond};
                            sCoordinate tB = {MVB.x,MVB.y + ((!MVB.bSecond )? (2-4*(!bBottom)):0),MVB.bSecond};

                            GetIntQuarterMV(tF,&MVIntF,&MVQuarterF);
                            CalculateChroma(MVF,&MVChromaF);
                            MVChromaF.y = (!MVF.bSecond )? MVChromaF.y + (2-4*(!bBottom)):MVChromaF.y ;
                            GetIntQuarterMV(MVChromaF,&MVIntChromaF,&MVQuarterChromaF);

                            GetIntQuarterMV(tB,&MVIntB,&MVQuarterB);
                            CalculateChroma(MVB,&MVChromaB);
                            MVChromaB.y = (!MVB.bSecond )? MVChromaB.y + (2-4*(!bBottom)):MVChromaB.y;
                            GetIntQuarterMV(MVChromaB,&MVIntChromaB,&MVQuarterChromaB);

#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(tF.x, tF.y, 0);
                            pDebug->SetIntrpMV(MVChromaF.x, MVChromaF.y, 0, 4);
                            pDebug->SetIntrpMV(MVChromaF.x, MVChromaF.y, 0, 5);

                            pDebug->SetIntrpMV(tB.x, tB.y, 1);
                            pDebug->SetIntrpMV(MVChromaB.x, MVChromaB.y, 1, 4);
                            pDebug->SetIntrpMV(MVChromaB.x, MVChromaB.y, 1, 5);
#endif


                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            Ipp32u ind = (bSecondField)? 1: 0;
                            if (padding[0][ind][MVF.bSecond])
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolationF, pForwMBRow[MVF.bSecond][0], pForwMBStep[MVF.bSecond][0], &MVIntF, &MVQuarterF, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolationF);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolationF, &sVInterpolationF, pForwMBRow[MVF.bSecond][1], pForwMBRow[MVF.bSecond][2],pForwMBStep[MVF.bSecond][1], 
                                    &MVIntChromaF, &MVQuarterChromaF, j);

                                 IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            else
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockYF[MVF.bSecond], j, i , &MVIntF, &MVQuarterF);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYF[MVF.bSecond],0,0,oppositePadding[0][ind][MVF.bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockUF[MVF.bSecond], &InterpolateBlockVF[MVF.bSecond], 
                                                        oppositePadding[0][ind][MVF.bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChromaF, &MVQuarterChromaF);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
             
                            }
                            if (padding[1][ind][MVB.bSecond])
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolationB, pBackwMBRow[MVB.bSecond][0], pBackwMBStep[MVB.bSecond][0], &MVIntB, &MVQuarterB, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolationB);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolationB, &sVInterpolationB, pBackwMBRow[MVB.bSecond][1], pBackwMBRow[MVB.bSecond][2],pBackwMBStep[MVB.bSecond][1], 
                                    &MVIntChromaB, &MVQuarterChromaB, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            else
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockYB[MVB.bSecond], j, i , &MVIntB, &MVQuarterB);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYB[MVB.bSecond],0,0,oppositePadding[1][ind][MVB.bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockUB[MVB.bSecond], &InterpolateBlockVB[MVB.bSecond], 
                                                        oppositePadding[1][ind][MVB.bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChromaB, &MVQuarterChromaB);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
        
                            }

                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);


                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                        }
                        if (mbType != VC1_ENC_B_MB_SKIP_FB)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_B pIn;
                                pIn.pYSrc    = pCurMBRow[0]+posX; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef[0]    = sYInterpolationF.pDst; 
                                pIn.refYStep[0] = sYInterpolationF.dstStep;      
                                pIn.pURef[0]    = sUInterpolationF.pDst; 
                                pIn.refUStep[0] = sUInterpolationF.dstStep;
                                pIn.pVRef[0]    = sVInterpolationF.pDst; 
                                pIn.refVStep[0] = sVInterpolationF.dstStep;

                                pIn.pYRef[1]    = sYInterpolationB.pDst; 
                                pIn.refYStep[1] = sYInterpolationB.dstStep;      
                                pIn.pURef[1]    = sUInterpolationB.pDst; 
                                pIn.refUStep[1] = sUInterpolationB.dstStep;
                                pIn.pVRef[1]    = sVInterpolationB.pDst; 
                                pIn.refVStep[1] = sVInterpolationB.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeB_RD (&pIn, BlockTSTypes)  ; 
#endif                   
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            pCurMBData->CopyBDiffMBProg( pCurMBRow[0],  pCurMBStep[0], pCurMBRow[1],  pCurMBRow[2],  pCurMBStep[1], 
                                sYInterpolationF.pDst,  sYInterpolationF.dstStep,
                                sUInterpolationF.pDst,  sVInterpolationF.pDst,   sUInterpolationF.dstStep,
                                sYInterpolationB.pDst,  sYInterpolationB.dstStep,
                                sUInterpolationB.pDst,  sVInterpolationB.pDst,   sUInterpolationB.dstStep,
                                j, 0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8s));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern   = pCompressedMB->GetMBPattern();
                            CBPCY       = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                            if (MBPattern==0 && mvDiffF.x == 0 && mvDiffF.y == 0 && mvDiffB.x == 0 && mvDiffB.y == 0)
                            {
                                pCompressedMB->ChangeType(VC1_ENC_B_MB_SKIP_FB);
                                mbType = VC1_ENC_B_MB_SKIP_FB;
#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetMBAsSkip();
#endif
                            }
                        }

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetMVInfoField(&MVF, mvPredF[MVF.bSecond].x, mvPredF[MVF.bSecond].y, 0);
                        pDebug->SetMVInfoField(&MVB, mvPredB[MVB.bSecond].x, mvPredB[MVB.bSecond].y, 1);
                        pDebug->SetMVInfoField(&MVChromaF,  0, 0, 0, 4);
                        pDebug->SetMVInfoField(&MVChromaF,  0, 0, 0, 5);
                        pDebug->SetMVInfoField(&MVChromaB,  0, 0, 1, 4);
                        pDebug->SetMVInfoField(&MVChromaB,  0, 0, 1, 5);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        Ipp32u ind = (bSecondField)? 1: 0;

                        pDebug->SetInterpInfo(&sYInterpolationF,  &sUInterpolationF, &sVInterpolationF, 0, padding[0][ind][MVF.bSecond]);
                        pDebug->SetInterpInfo(&sYInterpolationB,  &sUInterpolationB, &sVInterpolationB, 1, padding[1][ind][MVF.bSecond]);
#endif
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteBSumMBProg(sYInterpolationF.pDst, sYInterpolationF.dstStep, 
                                                            sUInterpolationF.pDst, sVInterpolationF.pDst, sUInterpolationF.dstStep, 
                                                            sYInterpolationB.pDst, sYInterpolationB.dstStep, 
                                                            sUInterpolationB.pDst, sVInterpolationB.pDst, sUInterpolationB.dstStep,
                                                            pRaisedMBRow[0], pRaisedMBStep[0], 
                                                            pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                                            0, j);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteBSumSkipMBProg(sYInterpolationF.pDst, sYInterpolationF.dstStep, 
                                    sUInterpolationF.pDst, sVInterpolationF.pDst, sUInterpolationF.dstStep, 
                                    sYInterpolationB.pDst, sYInterpolationB.dstStep, 
                                    sUInterpolationB.pDst, sVInterpolationB.pDst, sUInterpolationB.dstStep,
                                    pRaisedMBRow[0], pRaisedMBStep[0], 
                                    pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                    0, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pCurMBInfo->SetExternalEdgeHor(0,0,0);
                                pCurMBInfo->SetExternalEdgeVer(0,0,0);
                                pCurMBInfo->SetInternalEdge(0,0);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }// DEBLOCKING

                            STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                            //pDebug->SetInterpInfo(pYInterpolation, pUInterpolation, pVInterpolation, 0);
#endif
                        } //bRaiseFrame
                        break;
                    }
                case VC1_ENC_B_MB_DIRECT:
                case VC1_ENC_B_MB_SKIP_DIRECT:

                    {
                        //sCoordinate  mvPredF    = {0,0};
                        //sCoordinate  mvPredB    = {0,0};
                        sCoordinate  MVIntF     = {0};
                        sCoordinate  MVQuarterF = {0};
                        sCoordinate  MVIntB     = {0};
                        sCoordinate  MVQuarterB = {0};

                        sCoordinate  MVChromaF       = {0,0};
                        sCoordinate  MVIntChromaF    = {0,0};
                        sCoordinate  MVQuarterChromaF= {0,0};
                        sCoordinate  MVChromaB       = {0,0};
                        sCoordinate  MVIntChromaB    = {0,0};
                        sCoordinate  MVQuarterChromaB= {0,0};

                        eTransformType  BlockTSTypes[6] = { m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType};

                        //direct
                        pCompressedMB->Init(mbType);

                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV_F(MVF,true);
                        pCurMBInfo->SetMV_F(MVB,false);

                        if (mbType !=VC1_ENC_B_MB_SKIP_DIRECT || bRaiseFrame)
                        {
                            sCoordinate tF = {MVF.x,MVF.y + ((!MVF.bSecond )? (2-4*(!bBottom)):0),MVF.bSecond};
                            sCoordinate tB = {MVB.x,MVB.y + ((!MVB.bSecond )? (2-4*(!bBottom)):0),MVB.bSecond};

                            GetIntQuarterMV(tF,&MVIntF,&MVQuarterF);
                            CalculateChroma(MVF,&MVChromaF);
                            MVChromaF.y = (!MVF.bSecond )? MVChromaF.y + (2-4*(!bBottom)):MVChromaF.y ;
                            GetIntQuarterMV(MVChromaF,&MVIntChromaF,&MVQuarterChromaF);

                            GetIntQuarterMV(tB,&MVIntB,&MVQuarterB);
                            CalculateChroma(MVB,&MVChromaB);
                            MVChromaB.y = (!MVB.bSecond )? MVChromaB.y + (2-4*(!bBottom)):MVChromaB.y;
                            GetIntQuarterMV(MVChromaB,&MVIntChromaB,&MVQuarterChromaB);

#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(tF.x, tF.y, 0);
                            pDebug->SetIntrpMV(MVChromaF.x, MVChromaF.y, 0, 4);
                            pDebug->SetIntrpMV(MVChromaF.x, MVChromaF.y, 0, 5);

                            pDebug->SetIntrpMV(tB.x, tB.y, 1);
                            pDebug->SetIntrpMV(MVChromaB.x, MVChromaB.y, 1, 4);
                            pDebug->SetIntrpMV(MVChromaB.x, MVChromaB.y, 1, 5);
#endif
                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            Ipp32u ind = (bSecondField)? 1: 0;
                            if (padding[0][ind][MVF.bSecond])
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolationF, pForwMBRow[MVF.bSecond][0], pForwMBStep[MVF.bSecond][0], &MVIntF, &MVQuarterF, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolationF);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolationF, &sVInterpolationF, pForwMBRow[MVF.bSecond][1], pForwMBRow[MVF.bSecond][2],pForwMBStep[MVF.bSecond][1], 
                                    &MVIntChromaF, &MVQuarterChromaF, j);   

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            else
                            {

                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockYF[MVF.bSecond], j, i , &MVIntF, &MVQuarterF);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYF[MVF.bSecond],0,0,oppositePadding[0][ind][MVF.bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockUF[MVF.bSecond], &InterpolateBlockVF[MVF.bSecond], 
                                                        oppositePadding[0][ind][MVF.bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChromaF, &MVQuarterChromaF);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
               
                            }
                            if (padding[1][ind][MVB.bSecond])
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolationB, pBackwMBRow[MVB.bSecond][0], pBackwMBStep[MVB.bSecond][0], &MVIntB, &MVQuarterB, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolationB);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolationB, &sVInterpolationB, pBackwMBRow[MVB.bSecond][1], pBackwMBRow[MVB.bSecond][2],pBackwMBStep[MVB.bSecond][1], 
                                    &MVIntChromaB, &MVQuarterChromaB, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                
                            }
                            else
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockYB[MVB.bSecond], j, i , &MVIntB, &MVQuarterB);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYB[MVB.bSecond],0,0,oppositePadding[1][ind][MVB.bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockUB[MVB.bSecond], &InterpolateBlockVB[MVB.bSecond], 
                                    oppositePadding[1][ind][MVB.bSecond],true,m_uiRoundControl,bBottom,
                                    j, i ,  &MVIntChromaB, &MVQuarterChromaB);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);   
                            }

                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);

                        }
                        if (mbType !=VC1_ENC_B_MB_SKIP_DIRECT)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_B pIn;
                                pIn.pYSrc    = pCurMBRow[0]+posX; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef[0]    = sYInterpolationF.pDst; 
                                pIn.refYStep[0] = sYInterpolationF.dstStep;      
                                pIn.pURef[0]    = sUInterpolationF.pDst; 
                                pIn.refUStep[0] = sUInterpolationF.dstStep;
                                pIn.pVRef[0]    = sVInterpolationF.pDst; 
                                pIn.refVStep[0] = sVInterpolationF.dstStep;

                                pIn.pYRef[1]    = sYInterpolationB.pDst; 
                                pIn.refYStep[1] = sYInterpolationB.dstStep;      
                                pIn.pURef[1]    = sUInterpolationB.pDst; 
                                pIn.refUStep[1] = sUInterpolationB.dstStep;
                                pIn.pVRef[1]    = sVInterpolationB.pDst; 
                                pIn.refVStep[1] = sVInterpolationB.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeB_RD (&pIn, BlockTSTypes)  ; 
#endif                   
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyBDiffMBProg( pCurMBRow[0],  pCurMBStep[0], pCurMBRow[1],  pCurMBRow[2],  pCurMBStep[1], 
                                sYInterpolationF.pDst,  sYInterpolationF.dstStep,
                                sUInterpolationF.pDst,  sVInterpolationF.pDst,   sUInterpolationF.dstStep,
                                sYInterpolationB.pDst,  sYInterpolationB.dstStep,
                                sUInterpolationB.pDst,  sVInterpolationB.pDst,   sUInterpolationB.dstStep,
                                j, 0);
 
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);


                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern   = pCompressedMB->GetMBPattern();
                            CBPCY       = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                        }


#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMVInfoField(&MVF, 0, 0, 0);
                        pDebug->SetMVInfoField(&MVB, 0, 0, 1);
                        pDebug->SetMVInfoField(&MVChromaF,  0, 0, 0, 4);
                        pDebug->SetMVInfoField(&MVChromaF,  0, 0, 0, 5);
                        pDebug->SetMVInfoField(&MVChromaB,  0, 0, 1, 4);
                        pDebug->SetMVInfoField(&MVChromaB,  0, 0, 1, 5);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetMBType(mbType);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);

                        Ipp32u ind = (bSecondField)? 1: 0;

                        pDebug->SetInterpInfo(&sYInterpolationF,  &sUInterpolationF, &sVInterpolationF, 0, padding[0][ind][MVF.bSecond]);
                        pDebug->SetInterpInfo(&sYInterpolationB,  &sUInterpolationB, &sVInterpolationB, 1, padding[1][ind][MVB.bSecond]);
#endif
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteBSumMBProg(sYInterpolationF.pDst, sYInterpolationF.dstStep, 
                                                            sUInterpolationF.pDst, sVInterpolationF.pDst, sUInterpolationF.dstStep, 
                                                            sYInterpolationB.pDst, sYInterpolationB.dstStep, 
                                                            sUInterpolationB.pDst, sVInterpolationB.pDst, sUInterpolationB.dstStep,
                                                            pRaisedMBRow[0], pRaisedMBStep[0], 
                                                            pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                                            0, j);
                              
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteBSumSkipMBProg(sYInterpolationF.pDst, sYInterpolationF.dstStep, 
                                    sUInterpolationF.pDst, sVInterpolationF.pDst, sUInterpolationF.dstStep, 
                                    sYInterpolationB.pDst, sYInterpolationB.dstStep, 
                                    sUInterpolationB.pDst, sVInterpolationB.pDst, sUInterpolationB.dstStep,
                                    pRaisedMBRow[0], pRaisedMBStep[0], 
                                    pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                    0, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pCurMBInfo->SetExternalEdgeHor(0,0,0);
                                pCurMBInfo->SetExternalEdgeVer(0,0,0);
                                pCurMBInfo->SetInternalEdge(0,0);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }// DEBLOCKING

                            STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                            //pDebug->SetInterpInfo(pYInterpolation, pUInterpolation, pVInterpolation, 0);
#endif
                        } //bRaiseFrame
                        break;
                    }

                default:
                    return UMC::UMC_ERR_FAILED;
                }

#ifdef VC1_ENC_DEBUG_ON
                if(!bSubBlkTS)
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(), 15, 15);
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(), 15, 15);
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(),15);
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), 15);
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), 15);
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), 15);
                }
                else
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(),
                        pCurMBInfo->GetLumaAdUppEdge(), pCurMBInfo->GetLumaAdBotEdge() );
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(),
                        pCurMBInfo->GetLumaAdLefEdge(), pCurMBInfo->GetLumaAdRigEdge());
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(), pCurMBInfo->GetUAdHorEdge());
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), pCurMBInfo->GetVAdHorEdge());
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), pCurMBInfo->GetUAdVerEdge());
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), pCurMBInfo->GetVAdVerEdge());
                }
#endif
                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }

            deblkPattern = (deblkPattern | 0x4 | ( (! (Ipp8u)((i + 1 - (hMB -1)) >> 31)<<3))) & deblkMask;

            ////Row deblocking
            //STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            //        if (m_pSequenceHeader->IsLoopFilter() && bRaiseFrame && (i != firstRow || firstRow == hMB-1))
            //        {
            //STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
            //          Ipp8u *DeblkPlanes[3] = {m_pRaisedPlane[0] + i*m_uiRaisedPlaneStep[0]*VC1_ENC_LUMA_SIZE,
            //                                   m_pRaisedPlane[1] + i*m_uiRaisedPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
            //                                   m_pRaisedPlane[2] + i*m_uiRaisedPlaneStep[2]*VC1_ENC_CHROMA_SIZE};
            //            m_pMBs->StartRow();
            //            if(bSubBlkTS)
            //            {
            //                if(i < hMB - 1)
            //                    Deblock_P_RowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                  if(firstRow != hMB -1)
            //                    Deblock_P_BottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                    Deblock_P_TopBottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //            }
            //            else
            //            {
            //                if(i < hMB - 1)
            //                    Deblock_P_RowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                  if(firstRow != hMB -1)
            //                    Deblock_P_BottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                    Deblock_P_TopBottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //            }
            //STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            //        }
            //STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;

            pCurMBRow[0]+= pCurMBStep[0]*VC1_ENC_LUMA_SIZE;
            pCurMBRow[1]+= pCurMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pCurMBRow[2]+= pCurMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pRaisedMBRow[0]+= pRaisedMBStep[0]*VC1_ENC_LUMA_SIZE;
            pRaisedMBRow[1]+= pRaisedMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pRaisedMBRow[2]+= pRaisedMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pForwMBRow[0][0]+= pForwMBStep[0][0]*VC1_ENC_LUMA_SIZE;
            pForwMBRow[0][1]+= pForwMBStep[0][1]*VC1_ENC_CHROMA_SIZE;
            pForwMBRow[0][2]+= pForwMBStep[0][2]*VC1_ENC_CHROMA_SIZE;

            pForwMBRow[1][0]+= pForwMBStep[1][0]*VC1_ENC_LUMA_SIZE;
            pForwMBRow[1][1]+= pForwMBStep[1][1]*VC1_ENC_CHROMA_SIZE;
            pForwMBRow[1][2]+= pForwMBStep[1][2]*VC1_ENC_CHROMA_SIZE;

            pBackwMBRow[0][0]+= pBackwMBStep[0][0]*VC1_ENC_LUMA_SIZE;
            pBackwMBRow[0][1]+= pBackwMBStep[0][1]*VC1_ENC_CHROMA_SIZE;
            pBackwMBRow[0][2]+= pBackwMBStep[0][2]*VC1_ENC_CHROMA_SIZE;

            pBackwMBRow[1][0]+= pBackwMBStep[1][0]*VC1_ENC_LUMA_SIZE;
            pBackwMBRow[1][1]+= pBackwMBStep[1][1]*VC1_ENC_CHROMA_SIZE;
            pBackwMBRow[1][2]+= pBackwMBStep[1][2]*VC1_ENC_CHROMA_SIZE;

        }

#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame && bSecondField && i == h)
            pDebug->PrintRestoredFrame(m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
            m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
            m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 1);
#endif

        return err;
    }
    UMC::Status VC1EncoderPictureADV::PACBFieldMixed(UMC::MeParams* MEParams, bool bSecondField, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status         err = UMC::UMC_OK;
        Ipp32s              i=0, j=0, blk = 0;
        bool                bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);
        bool                bBottom     = ((m_bTopFieldFirst && bSecondField) || ((!m_bTopFieldFirst)&&(!bSecondField)));

        Ipp32u              pCurMBStep[3];
        Ipp8u*              pCurMBRow[3];
        Ipp32u              pRaisedMBStep[3];
        Ipp8u*              pRaisedMBRow[3];


        Ipp32s              h   = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32s              w   = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32s              hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        VC1EncoderCodedMB*  pCodedMB = &(m_pCodedMB[w*h*bSecondField]);


        Ipp8u*              pForwMBRow  [2][3]  = {0};
        Ipp32u              pForwMBStep [2][3]  = {0};
        Ipp8u*              pBackwMBRow  [2][3]  = {0};
        Ipp32u              pBackwMBStep [2][3]  = {0};

        _SetIntraInterLumaFunctions;

        //inverse transform quantization
        IntraInvTransformQuantFunction IntraInvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :
            IntraInvTransformQuantNonUniform;

        InterInvTransformQuantFunction InterInvTransformQuantACFunction = (m_bUniformQuant) ? InterInvTransformQuantUniform :
            InterInvTransformQuantNonUniform;
        CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())?
GetChromaMVFast:GetChromaMV;
        // ------------------------------------------------------------------------------------------------ //



        /*    interpolation parameters for frame padding      */

        IppVCInterpolate_8u         sYInterpolationF;
        IppVCInterpolate_8u         sUInterpolationF;
        IppVCInterpolate_8u         sVInterpolationF;
        IppVCInterpolate_8u         sYInterpolationB;
        IppVCInterpolate_8u         sUInterpolationB;
        IppVCInterpolate_8u         sVInterpolationB;

        IppVCInterpolate_8u         sYInterpolationBlkF[4];
        IppVCInterpolate_8u         sYInterpolationBlkB[4];

        InterpolateFunction         InterpolateLumaFunction  = _own_ippiInterpolateQPBicubic_VC1_8u_C1R;


        /*    interpolation parameters for MB padding      */

        IppVCInterpolateBlock_8u    InterpolateBlockYF[2];
        IppVCInterpolateBlock_8u    InterpolateBlockUF[2];
        IppVCInterpolateBlock_8u    InterpolateBlockVF[2];

        IppVCInterpolateBlock_8u    InterpolateBlockYB[2];
        IppVCInterpolateBlock_8u    InterpolateBlockUB[2];
        IppVCInterpolateBlock_8u    InterpolateBlockVB[2];

        IppVCInterpolateBlock_8u    InterpolateBlockYBlkF[2][4];
        IppVCInterpolateBlock_8u    InterpolateBlockYBlkB[2][4];

        InterpolateFunctionPadding  InterpolateLumaFunctionPadding   = own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;

        /*    */
        Ipp8u                       tempInterpolationBufferF[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];
        Ipp8u                       tempInterpolationBufferB[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];

        IppStatus                   ippSts = ippStsNoErr;

        eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];


        Ipp8u                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
        Ipp8u                       DCQuant         =  DCQuantValues[m_uiQuant];
        Ipp16s                      defPredictor    =  0;

        IppiSize                    blkSize     = {8,8};
        IppiSize                    blkSizeLuma = {16,16};

        bool                        bMVHalf =  false;

        bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

        sScaleInfo                  scInfoForw;
        sScaleInfo                  scInfoBackw;

        Ipp16s*                     pSavedMV        =  (m_pSavedMV)?  m_pSavedMV + w*2*(h*bSecondField + firstRow):0;
        Ipp8u*                      pRefType        =  (m_pRefType)?  m_pRefType + w*(h*bSecondField   + firstRow):0;

        Ipp8u                       scaleFactor  = (Ipp8u)((BFractionScaleFactor[m_uiBFraction.denom][m_uiBFraction.num]*(Ipp32u)m_nReferenceFrameDist)>>8);

        Ipp8u deblkPattern = (firstRow != hMB - 1)? 0 : 0x8;   //4 bits: right 1 bit - 0 - left/1 - not left,
        //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row, 10 - top bottom row
        //middle 1 bit - 1 - right/0 - not right

        bool                        bSubBlkTS           = m_pSequenceHeader->IsVSTransform() && (!(m_bVSTransform &&m_uiTransformType==VC1_ENC_8x8_TRANSFORM));

        Ipp8u deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFC : 0;

        assert (m_uiReferenceFieldType == VC1_ENC_REF_FIELD_BOTH );
        assert (pSavedMV && pRefType );

        // backward, second field, second reference
        Ipp32u   padding[2][2][2] = {{{m_uiForwardPlanePadding, m_uiForwardPlanePadding}, // first field
        {m_uiRaisedPlanePadding,  m_uiForwardPlanePadding}},// second field
        {{m_uiBackwardPlanePadding, m_uiBackwardPlanePadding}, // first field
        {m_uiBackwardPlanePadding,  m_uiBackwardPlanePadding}}, };// second field

        // backward, second field, second reference
        Ipp32u   oppositePadding[2][2][2] = {{{(m_bForwardInterlace)? 0:1,(m_bForwardInterlace)? 0:1 },
        {0,(m_bForwardInterlace)? 0:1}},
        {{(m_bBackwardInterlace)? 0:1,(m_bBackwardInterlace)? 0:1 },
        {(m_bBackwardInterlace)? 0:1,(m_bBackwardInterlace)? 0:1}}};


        InitScaleInfo(&scInfoForw, bSecondField,bBottom,(Ipp8u)scaleFactor,m_uiMVRangeIndex);
        InitScaleInfoBackward(&scInfoBackw,bSecondField,bBottom,((m_nReferenceFrameDist - scaleFactor-1)>=0)?(m_nReferenceFrameDist - scaleFactor-1):0,m_uiMVRangeIndex);

#ifdef VC1_ENC_DEBUG_ON
        bool       bIsBilinearInterpolation =  false;
        Ipp32u     fieldShift = (bSecondField)? h : 0;

        pDebug->SetRefNum(2);
        //pDebug->SetCurrMBFirst(bSecondField);
        pDebug->SetCurrMB(bSecondField, 0, firstRow);
        pDebug->SetSliceInfo(firstRow + fieldShift, firstRow + nRows + fieldShift);
        pDebug->SetHalfCoef(bMVHalf);
        pDebug->SetVTSFlag(bSubBlkTS);
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
#endif

        err = m_pMBs->Reset();
        if (err != UMC::UMC_OK)
            return err;

        /*    interpolation parameters for frame padding      */

        SetInterpolationParams(&sYInterpolationF,&sUInterpolationF,&sVInterpolationF,
            tempInterpolationBufferF,true, true);
        SetInterpolationParams(&sYInterpolationB,&sUInterpolationB,&sVInterpolationB,
            tempInterpolationBufferB,false, true);
        SetInterpolationParams4MV(sYInterpolationBlkF,&sUInterpolationF,&sVInterpolationF,
            tempInterpolationBufferF,true, true);
        SetInterpolationParams4MV(sYInterpolationBlkB,&sUInterpolationB,&sVInterpolationB,
            tempInterpolationBufferB,false, true);

        /*    plane parameters                                  */

        SetFieldStep(pCurMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pCurMBRow, m_pPlane, m_uiPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);
        SetFieldStep(pRaisedMBStep, m_uiPlaneStep);
        err = SetFieldPlane(pRaisedMBRow, m_pRaisedPlane, m_uiRaisedPlaneStep,bBottom, firstRow);
        VC1_ENC_CHECK(err);   
        err = Set2RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep, bSecondField, bBottom, 0); 
        VC1_ENC_CHECK(err);

        err = SetBkwFieldPlane(pBackwMBRow, pBackwMBStep,
            m_pBackwardPlane, m_uiBackwardPlaneStep, bBottom, 0);
        VC1_ENC_CHECK(err);

        /*    interpolation parameters for MB padding      */
        SetInterpolationParams(&InterpolateBlockYF[0],&InterpolateBlockUF[0],&InterpolateBlockVF[0], tempInterpolationBufferF,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[0], pForwMBStep[0], true);

        SetInterpolationParams(&InterpolateBlockYF[1],&InterpolateBlockUF[1],&InterpolateBlockVF[1], tempInterpolationBufferF,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[1], pForwMBStep[1], true);

        SetInterpolationParams(&InterpolateBlockYB[0],&InterpolateBlockUB[0],&InterpolateBlockVB[0], tempInterpolationBufferB,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pBackwMBRow[0], pBackwMBStep[0], true);

        SetInterpolationParams(&InterpolateBlockYB[1],&InterpolateBlockUB[1],&InterpolateBlockVB[1], tempInterpolationBufferB,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pBackwMBRow[1], pBackwMBStep[1], true);

        SetInterpolationParams4MV (InterpolateBlockYBlkF[0], tempInterpolationBufferF,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[0], pForwMBStep[0], true);
        SetInterpolationParams4MV (InterpolateBlockYBlkF[1], tempInterpolationBufferF,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pForwMBRow[1], pForwMBStep[1], true);

        SetInterpolationParams4MV (InterpolateBlockYBlkB[0], tempInterpolationBufferB,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pBackwMBRow[0], pBackwMBStep[0], true);
        SetInterpolationParams4MV (InterpolateBlockYBlkB[1], tempInterpolationBufferB,
            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
            pBackwMBRow[1], pBackwMBStep[1], true);


        /*    update plane parameters            */

        err = Set2RefFrwFieldPlane(pForwMBRow, pForwMBStep, m_pForwardPlane, m_uiForwardPlaneStep,
            m_pRaisedPlane, m_uiRaisedPlaneStep, bSecondField, bBottom, firstRow);
        VC1_ENC_CHECK(err);

        err = SetBkwFieldPlane(pBackwMBRow, pBackwMBStep,
            m_pBackwardPlane, m_uiBackwardPlaneStep, bBottom, firstRow);
        VC1_ENC_CHECK(err);

#ifdef VC1_ENC_CHECK_MV
        err = CheckMEMV_BFieldMixed(MEParams, bMVHalf, firstRow, nRows);
        assert(err == UMC::UMC_OK);
#endif

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderMBInfo  *         pCurMBInfo     = 0;
                VC1EncoderMBData  *         pCurMBData     = 0;
                VC1EncoderMBData  *         pRecMBData     = 0;

                VC1EncoderCodedMB*          pCompressedMB  = &(pCodedMB[w*i+j]);
                Ipp32s                      posX           =  VC1_ENC_LUMA_SIZE*j;
                Ipp32s                      posXChroma     =  VC1_ENC_CHROMA_SIZE*j;
                Ipp8u                       MBPattern       = 0;
                Ipp8u                       CBPCY           = 0;
                eMBType                     mbType         =  VC1_ENC_B_MB_F; //From ME

                sCoordinate MVF         ={0};
                sCoordinate MVB         ={0};

                pCurMBInfo  =   m_pMBs->GetCurrMBInfo();
                pCurMBData  =   m_pMBs->GetCurrMBData();
                pRecMBData  =   m_pMBs->GetRecCurrMBData();

                VC1EncoderMBInfo* left        = m_pMBs->GetLeftMBInfo();
                VC1EncoderMBInfo* topLeft     = m_pMBs->GetTopLeftMBInfo();
                VC1EncoderMBInfo* top         = m_pMBs->GetTopMBInfo();
                VC1EncoderMBInfo* topRight    = m_pMBs->GetTopRightMBInfo();

                switch (MEParams->pSrc->MBs[(j + i*w)].MbType)
                {
                case UMC::ME_MbIntra:
                    mbType = VC1_ENC_B_MB_INTRA;
                    break;
                case UMC::ME_MbFrw:

                    mbType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_B_MB_F_4MV:VC1_ENC_B_MB_F;
                    MVF.x  = MEParams->pSrc->MBs[j + i*w].MV[0][0].x;
                    MVF.y  = MEParams->pSrc->MBs[j + i*w].MV[0][0].y;
                    MVF.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    break;

                case UMC::ME_MbFrwSkipped:

                    mbType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_B_MB_SKIP_F_4MV:VC1_ENC_B_MB_SKIP_F;
                    MVF.x  = MEParams->pSrc->MBs[j + i*w].MV[0][0].x;
                    MVF.y  = MEParams->pSrc->MBs[j + i*w].MV[0][0].y;
                    MVF.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    break;

                case UMC::ME_MbBkw:

                    mbType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_B_MB_B_4MV:VC1_ENC_B_MB_B;
                    MVB.x  = MEParams->pSrc->MBs[j + i*w].MV[1][0].x;
                    MVB.y  = MEParams->pSrc->MBs[j + i*w].MV[1][0].y;
                    MVB.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;

                case UMC::ME_MbBkwSkipped:

                    mbType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_B_MB_SKIP_B_4MV:VC1_ENC_B_MB_SKIP_B;
                    MVB.x  = MEParams->pSrc->MBs[j + i*w].MV[1][0].x;
                    MVB.y  = MEParams->pSrc->MBs[j + i*w].MV[1][0].y;
                    MVB.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;

                case UMC::ME_MbBidir:

                    mbType = VC1_ENC_B_MB_FB;
                    MVF.x  = MEParams->pSrc->MBs[j + i*w].MV[0][0].x;
                    MVF.y  = MEParams->pSrc->MBs[j + i*w].MV[0][0].y;
                    MVF.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    MVB.x  = MEParams->pSrc->MBs[j + i*w].MV[1][0].x;
                    MVB.y  = MEParams->pSrc->MBs[j + i*w].MV[1][0].y;
                    MVB.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;

                case UMC::ME_MbBidirSkipped:

                    mbType = VC1_ENC_B_MB_SKIP_FB;

                    MVF.x  = MEParams->pSrc->MBs[j + i*w].MV[0][0].x;
                    MVF.y  = MEParams->pSrc->MBs[j + i*w].MV[0][0].y;
                    MVF.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    MVB.x  = MEParams->pSrc->MBs[j + i*w].MV[1][0].x;
                    MVB.y  = MEParams->pSrc->MBs[j + i*w].MV[1][0].y;
                    MVB.bSecond =  (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;

                case UMC::ME_MbDirect:

                    mbType = VC1_ENC_B_MB_DIRECT;

                    MVF.x  =  MEParams->pSrc->MBs[j + i*w].MV[0][0].x;//MEParams.MVDirectFW[(j + i*w)].x;
                    MVF.y  =  MEParams->pSrc->MBs[j + i*w].MV[0][0].y;//MEParams.MVDirectFW[(j + i*w)].y;
                    MVF.bSecond =(MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    MVB.x  =  MEParams->pSrc->MBs[j + i*w].MV[1][0].x;//MEParams.MVDirectBW[(j + i*w)].x;
                    MVB.y  =  MEParams->pSrc->MBs[j + i*w].MV[1][0].y;//MEParams.MVDirectBW[(j + i*w)].y;
                    MVB.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;
                case UMC::ME_MbDirectSkipped:

                    mbType = VC1_ENC_B_MB_SKIP_DIRECT;

                    MVF.x  =  MEParams->pSrc->MBs[j + i*w].MV[0][0].x;//MEParams.MVDirectFW[(j + i*w)].x;
                    MVF.y  =  MEParams->pSrc->MBs[j + i*w].MV[0][0].y;//MEParams.MVDirectFW[(j + i*w)].y;
                    MVF.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[0][0] == 1); //from ME
                    MVB.x  =  MEParams->pSrc->MBs[j + i*w].MV[1][0].x;//MEParams.MVDirectBW[(j + i*w)].x;
                    MVB.y  =  MEParams->pSrc->MBs[j + i*w].MV[1][0].y;//MEParams.MVDirectBW[(j + i*w)].y;
                    MVB.bSecond = (MEParams->pSrc->MBs[j + i*w].Refindex[1][0] == 1); //from ME
                    break;
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
                }
                switch(mbType)
                {
                case VC1_ENC_B_MB_INTRA:
                    {
                        bool                dACPrediction   = true;
                        NeighbouringMBsData MBs;

                        MBs.LeftMB    = ((left)? left->isIntra():0)         ? m_pMBs->GetLeftMBData():0;
                        MBs.TopMB     = ((top)? top->isIntra():0)           ? m_pMBs->GetTopMBData():0;
                        MBs.TopLeftMB = ((topLeft)? topLeft->isIntra():0)   ? m_pMBs->GetTopLeftMBData():0;

                        pCompressedMB->Init(VC1_ENC_B_MB_INTRA);
                        pCurMBInfo->Init(true);

                        pCurMBData->CopyMBProg(pCurMBRow[0],pCurMBStep[0],pCurMBRow[1], pCurMBRow[2], pCurMBStep[1],j);

                        //only intra blocks:
                        for (blk = 0; blk<6; blk++)
                        {
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                            _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                            IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                            STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime );
                        }
                        STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                        // should be changed on DCACPredictionPFrameSM
                        dACPrediction = DCACPredictionFrame     ( pCurMBData,&MBs,
                            pRecMBData, defPredictor,direction);
                        STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                        for (blk=0; blk<6; blk++)
                        {
                            pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                pRecMBData->m_uiBlockStep[blk],
                                VC1_Inter_InterlaceIntra_8x8_Scan_Adv,
                                blk);
                        }
                        MBPattern = pCompressedMB->GetMBPattern();
                        CBPCY = MBPattern;
                        pCurMBInfo->SetMBPattern(MBPattern);
                        pCompressedMB->SetACPrediction(dACPrediction);
                        pCompressedMB->SetMBCBPCY(CBPCY);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(VC1_ENC_B_MB_INTRA);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            for (blk=0;blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                    DCQuant, doubleQuant);
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],blkSize, 0);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pRecMBData->PasteMBProg(pRaisedMBRow[0],pRaisedMBStep[0],pRaisedMBRow[1],pRaisedMBRow[2],pRaisedMBStep[1],j);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            //deblocking
                            {
                                STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};

                                m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                            }
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);
                        break;
                    }

                case VC1_ENC_B_MB_F:
                case VC1_ENC_B_MB_B:
                case VC1_ENC_B_MB_SKIP_F:
                case VC1_ENC_B_MB_SKIP_B:
                    {
                        sCoordinate  *mvC=0,   *mvB=0,   *mvA=0;
                        sCoordinate  mv1={0}, mv2={0}, mv3={0};
                        sCoordinate  mvAEx = {0}, mvCEx = {0};
                        sCoordinate  mvPred[2]   = {0};
                        sCoordinate  mvPred1[2]   = {0};
                        sCoordinate  mvDiff   = {0};
                        bool         bForward = ((mbType == VC1_ENC_B_MB_F )|| (mbType == VC1_ENC_B_MB_SKIP_F));
                        bool         bSecondDominantPred = false;
                        bool         bSecondDominantPred1 = false;
                        bool         bSkip = ((mbType == VC1_ENC_B_MB_SKIP_B )|| (mbType == VC1_ENC_B_MB_SKIP_F));

                        IppVCInterpolate_8u*         pYInterpolation= &sYInterpolationF;
                        IppVCInterpolate_8u*         pUInterpolation= &sUInterpolationF;
                        IppVCInterpolate_8u*         pVInterpolation= &sVInterpolationF;
                        IppVCInterpolateBlock_8u*   pInterpolateBlockY = InterpolateBlockYF;
                        IppVCInterpolateBlock_8u*   pInterpolateBlockU = InterpolateBlockUF;
                        IppVCInterpolateBlock_8u*   pInterpolateBlockV = InterpolateBlockVF;


                        sCoordinate*                 pMV                = &MVF;
                        sCoordinate*                 pMV1               = &MVB;

                        Ipp8u**                      pPredMB   = pForwMBRow[pMV->bSecond];
                        Ipp32u*                      pPredStep = pForwMBStep[pMV->bSecond];

                        sCoordinate                  MVInt             = {0};
                        sCoordinate                  MVQuarter         = {0};

                        sCoordinate                  MVChroma          = {0};
                        sCoordinate                  MVIntChroma       = {0};
                        sCoordinate                  MVQuarterChroma   = {0};
                        sScaleInfo *                 pInfo             = &scInfoForw;
                        sScaleInfo *                 pInfo1            = &scInfoBackw;
                        eTransformType  BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType};


                        if (!bForward)
                        {
                            pMV       = &MVB;
                            pMV1      = &MVF;
                            pYInterpolation = &sYInterpolationB;
                            pUInterpolation = &sUInterpolationB;
                            pVInterpolation = &sVInterpolationB;
                            pPredMB         = pBackwMBRow [pMV->bSecond];
                            pPredStep       = pBackwMBStep[pMV->bSecond];
                            pInfo           = &scInfoBackw;
                            pInfo1          = &scInfoForw;
                            pInterpolateBlockY = InterpolateBlockYB;
                            pInterpolateBlockU = InterpolateBlockUB;
                            pInterpolateBlockV = InterpolateBlockVB;
                        }

                        pCompressedMB->Init(mbType);


                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        GetMVPredictionPField(bForward);
                        bSecondDominantPred =  PredictMVField2(mvA,mvB,mvC, mvPred, pInfo,bSecondField,&mvAEx,&mvCEx, !bForward, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPred, bForward);
#endif

                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetPredFlag(bSecondDominantPred!=pMV->bSecond, !bForward);
                        pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == pMV->bSecond || mvA==0)? mvA:&mvAEx,
                            /*mvB,*/ (mvC && mvC->bSecond == pMV->bSecond || mvC==0)? mvC:&mvCEx,!bForward);
                        pDebug->SetHybrid(0, !bForward);
#endif

                        // The second vector is restored by prediction
                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);

                        GetMVPredictionPField(!bForward);
                        bSecondDominantPred1 =  PredictMVField2(mvA,mvB,mvC, mvPred1, pInfo1,bSecondField,&mvAEx,&mvCEx, bForward, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPred, !bForward);
#endif
                        pMV1->x         = mvPred1[bSecondDominantPred1].x;
                        pMV1->y         = mvPred1[bSecondDominantPred1].y;
                        pMV1->bSecond   = bSecondDominantPred1;

                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        if (bSkip )
                        {
                            assert (pMV->x == mvPred[pMV->bSecond].x && pMV->y == mvPred[pMV->bSecond].y);
                            pMV->x = mvPred[pMV->bSecond].x; //ME correction
                            pMV->y = mvPred[pMV->bSecond].y; //ME correction
                        }
                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV_F(MVF,true);
                        pCurMBInfo->SetMV_F(MVB,false);

                        mvDiff.x = (pMV->x - mvPred[pMV->bSecond].x);
                        mvDiff.y = (pMV->y - mvPred[pMV->bSecond].y);
                        mvDiff.bSecond = (pMV->bSecond != bSecondDominantPred);
                        pCompressedMB->SetdMV_F(mvDiff,bForward);
                        if (!bSkip || bRaiseFrame)
                        {
                            sCoordinate t = {pMV->x,pMV->y + ((!pMV->bSecond )? (2-4*(!bBottom)):0),pMV->bSecond};

                            GetIntQuarterMV(t,&MVInt,&MVQuarter);
                            CalculateChroma(*pMV,&MVChroma);

                            MVChroma.y = (!pMV->bSecond )? MVChroma.y + (2-4*(!bBottom)):MVChroma.y;
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);

#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(t.x, t.y, !bForward);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, !bForward, 4);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, !bForward, 5);
#endif
                            Ipp32u ind = (bSecondField)? 1: 0;
                            if (padding[!bForward][ind][pMV->bSecond])
                            {

                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(pYInterpolation, pPredMB[0], pPredStep[0], &MVInt, &MVQuarter, j);
                                ippSts = InterpolateLumaFunction(pYInterpolation);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (pUInterpolation, pVInterpolation, pPredMB[1], pPredMB[2],pPredStep[1], 
                                                   &MVIntChroma, &MVQuarterChroma, j);
                                                 
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            }
                            else
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&pInterpolateBlockY[pMV->bSecond], j, i , &MVInt, &MVQuarter);
                                ippSts = InterpolateLumaFunctionPadding(&pInterpolateBlockY[pMV->bSecond],0,0,oppositePadding[!bForward][ind][pMV->bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&pInterpolateBlockU[pMV->bSecond], &pInterpolateBlockV[pMV->bSecond], 
                                                        oppositePadding[!bForward][ind][pMV->bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChroma, &MVQuarterChroma);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime); 

                            }

                        }
                        if (!bSkip)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+posX; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = pYInterpolation->pDst; 
                                pIn.refYStep = pYInterpolation->dstStep;      
                                pIn.pURef    = pUInterpolation->pDst; 
                                pIn.refUStep = pUInterpolation->dstStep;
                                pIn.pVRef    = pVInterpolation->pDst; 
                                pIn.refVStep = pVInterpolation->dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif          
                            } 
                            pCompressedMB->SetTSType(BlockTSTypes);
                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                                        
                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],pCurMBStep[0], pCurMBRow[1],pCurMBRow[2], pCurMBStep[1], 
                                                        pYInterpolation->pDst, pYInterpolation->dstStep,
                                                        pUInterpolation->pDst,pVInterpolation->pDst, pUInterpolation->dstStep,
                                                        j,0);
                           IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);


                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](  pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern   = pCompressedMB->GetMBPattern();
                            CBPCY       = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                        }
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetMVInfoField(&MVF, mvPred[MVF.bSecond].x, mvPred[MVF.bSecond].y, 0);
                        pDebug->SetMVInfoField(&MVB, mvPred[MVB.bSecond].x, mvPred[MVB.bSecond].y, 1);

                        pDebug->SetMVInfoField(&MVChroma,  0, 0, !bForward, 4);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, !bForward, 5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);

                        Ipp32u ind = (bSecondField)? 1: 0;
                        pDebug->SetInterpInfo(pYInterpolation,  pUInterpolation, pVInterpolation, !bForward, padding[!bForward][ind][pMV->bSecond]);
#endif
                        /*---------------------------Reconstruction ------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteSumMBProg (pYInterpolation->pDst, pYInterpolation->dstStep, 
                                                            pUInterpolation->pDst, pVInterpolation->pDst,pUInterpolation->dstStep, 
                                                            pRaisedMBRow[0], pRaisedMBStep[0], 
                                                            pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                                            0, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                pRecMBData->PasteSumSkipMBProg (pYInterpolation->pDst,pYInterpolation->dstStep,
                                                                pUInterpolation->pDst, pVInterpolation->pDst, pUInterpolation->dstStep,
                                                                pRaisedMBRow[0], pRaisedMBStep[0], 
                                                                pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],0,j); 

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pCurMBInfo->SetExternalEdgeHor(0,0,0);
                                pCurMBInfo->SetExternalEdgeVer(0,0,0);
                                pCurMBInfo->SetInternalEdge(0,0);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }// DEBLOCKING

                            STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                            Ipp32u ind = (bSecondField)? 1: 0;
                            pDebug->SetInterpInfo(pYInterpolation, pUInterpolation, pVInterpolation, 0, padding[!bForward][ind][pMV->bSecond]);
#endif
                        } //bRaiseFrame
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);
                        break;
                    }
                case VC1_ENC_B_MB_FB:
                case VC1_ENC_B_MB_SKIP_FB:
                    {
                        sCoordinate  *mvC=0,   *mvB=0,   *mvA=0;

                        sCoordinate  mv1={0}, mv2={0}, mv3={0};
                        sCoordinate  mvAEx = {0};
                        sCoordinate  mvCEx = {0};

                        sCoordinate  mvPredB[2] = {0};
                        sCoordinate  mvPredF[2] = {0};
                        sCoordinate  mvDiffF    = {0};
                        sCoordinate  mvDiffB    = {0};
                        sCoordinate  MVIntF     = {0};
                        sCoordinate  MVQuarterF = {0};
                        sCoordinate  MVIntB     = {0};
                        sCoordinate  MVQuarterB = {0};

                        sCoordinate  MVChromaF       = {0};
                        sCoordinate  MVIntChromaF    = {0};
                        sCoordinate  MVQuarterChromaF= {0};
                        sCoordinate  MVChromaB       = {0};
                        sCoordinate  MVIntChromaB    = {0};
                        sCoordinate  MVQuarterChromaB= {0};

                        bool         bSecondDominantPredF = false;
                        bool         bSecondDominantPredB = false;

                        eTransformType  BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType};

                        pCompressedMB->Init(mbType);


                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                        GetMVPredictionPField(true)
                            bSecondDominantPredF =  PredictMVField2(mvA,mvB,mvC, mvPredF, &scInfoForw,bSecondField,&mvAEx,&mvCEx, false, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPredF, 0);
#endif
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetPredFlag(bSecondDominantPredF!=MVF.bSecond, 0);
                        pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == MVF.bSecond || mvA==0)? mvA:&mvAEx,
                            /*mvB,*/ (mvC && mvC->bSecond == MVF.bSecond || mvC==0)? mvC:&mvCEx,0);
                        pDebug->SetHybrid(0, 0);
#endif
                        GetMVPredictionPField(false)
                            bSecondDominantPredB =  PredictMVField2(mvA,mvB,mvC, mvPredB, &scInfoBackw,bSecondField,&mvAEx,&mvCEx, true, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPredB, 1);
#endif
                        //ScalePredict(&mvPredF, j*16*4,i*16*4,MVPredMin,MVPredMax);
                        //ScalePredict(&mvPredB, j*16*4,i*16*4,MVPredMin,MVPredMax);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetPredFlag(bSecondDominantPredB!=MVB.bSecond, 1);
                        pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == MVB.bSecond || mvA==0)? mvA:&mvAEx,
                            /*mvB,*/ (mvC && mvC->bSecond == MVB.bSecond || mvC==0)? mvC:&mvCEx,1);
                        pDebug->SetHybrid(0, 1);
#endif
                        STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);
                        if  (mbType == VC1_ENC_B_MB_SKIP_FB)
                        {
                            assert (MVF.x == mvPredF[MVF.bSecond].x && MVF.y == mvPredF[MVF.bSecond].y);
                            assert (MVB.x == mvPredB[MVB.bSecond].x && MVB.y == mvPredB[MVB.bSecond].y);
                            MVF.x = mvPredF[MVF.bSecond].x ;
                            MVF.y = mvPredF[MVF.bSecond].y ;
                            MVB.x = mvPredB[MVB.bSecond].x ;
                            MVB.y = mvPredB[MVB.bSecond].y ;
                        }

                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV_F(MVF,true);
                        pCurMBInfo->SetMV_F(MVB,false);

                        mvDiffF.x = (MVF.x - mvPredF[MVF.bSecond].x);
                        mvDiffF.y = (MVF.y - mvPredF[MVF.bSecond].y);
                        mvDiffF.bSecond = (MVF.bSecond != bSecondDominantPredF);
                        pCompressedMB->SetdMV_F(mvDiffF,true);

                        mvDiffB.x = (MVB.x - mvPredB[MVB.bSecond].x);
                        mvDiffB.y = (MVB.y - mvPredB[MVB.bSecond].y);
                        mvDiffB.bSecond =(MVB.bSecond != bSecondDominantPredB);
                        pCompressedMB->SetdMV_F(mvDiffB,false);

                        if (mbType != VC1_ENC_B_MB_SKIP_FB || bRaiseFrame)
                        {
                            sCoordinate tF = {MVF.x,MVF.y + ((!MVF.bSecond )? (2-4*(!bBottom)):0),MVF.bSecond};
                            sCoordinate tB = {MVB.x,MVB.y + ((!MVB.bSecond )? (2-4*(!bBottom)):0),MVB.bSecond};

                            GetIntQuarterMV(tF,&MVIntF,&MVQuarterF);
                            CalculateChroma(MVF,&MVChromaF);
                            MVChromaF.y = (!MVF.bSecond )? MVChromaF.y + (2-4*(!bBottom)):MVChromaF.y ;
                            GetIntQuarterMV(MVChromaF,&MVIntChromaF,&MVQuarterChromaF);

                            GetIntQuarterMV(tB,&MVIntB,&MVQuarterB);
                            CalculateChroma(MVB,&MVChromaB);
                            MVChromaB.y = (!MVB.bSecond )? MVChromaB.y + (2-4*(!bBottom)):MVChromaB.y;
                            GetIntQuarterMV(MVChromaB,&MVIntChromaB,&MVQuarterChromaB);

#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(tF.x, tF.y, 0);
                            pDebug->SetIntrpMV(MVChromaF.x, MVChromaF.y, 0, 4);
                            pDebug->SetIntrpMV(MVChromaF.x, MVChromaF.y, 0, 5);

                            pDebug->SetIntrpMV(tB.x, tB.y, 1);
                            pDebug->SetIntrpMV(MVChromaB.x, MVChromaB.y, 1, 4);
                            pDebug->SetIntrpMV(MVChromaB.x, MVChromaB.y, 1, 5);
#endif


                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            Ipp32u ind = (bSecondField)? 1: 0;
                            if (padding[0][ind][MVF.bSecond])
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolationF, pForwMBRow[MVF.bSecond][0], pForwMBStep[MVF.bSecond][0], &MVIntF, &MVQuarterF, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolationF);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolationF, &sVInterpolationF, pForwMBRow[MVF.bSecond][1], pForwMBRow[MVF.bSecond][2],pForwMBStep[MVF.bSecond][1], 
                                    &MVIntChromaF, &MVQuarterChromaF, j);   

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            else
                            {                                
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockYF[MVF.bSecond], j, i , &MVIntF, &MVQuarterF);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYF[MVF.bSecond],0,0,oppositePadding[0][ind][MVF.bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockUF[MVF.bSecond], &InterpolateBlockVF[MVF.bSecond], 
                                    oppositePadding[0][ind][MVF.bSecond],true,m_uiRoundControl,bBottom,
                                    j, i ,  &MVIntChromaF, &MVQuarterChromaF);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);                                
                            }
                            if (padding[1][ind][MVB.bSecond])
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolationB, pBackwMBRow[MVB.bSecond][0], pBackwMBStep[MVB.bSecond][0], &MVIntB, &MVQuarterB, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolationB);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolationB, &sVInterpolationB, pBackwMBRow[MVB.bSecond][1], pBackwMBRow[MVB.bSecond][2],pBackwMBStep[MVB.bSecond][1], 
                                    &MVIntChromaB, &MVQuarterChromaB, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            }
                            else
                            {
                                
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockYB[MVB.bSecond], j, i , &MVIntB, &MVQuarterB);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYB[MVB.bSecond],0,0,oppositePadding[1][ind][MVB.bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockUB[MVB.bSecond], &InterpolateBlockVB[MVB.bSecond], 
                                    oppositePadding[1][ind][MVB.bSecond],true,m_uiRoundControl,bBottom,
                                    j, i ,  &MVIntChromaB, &MVQuarterChromaB);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);                  
                            }

                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);


                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                        }
                        if (mbType != VC1_ENC_B_MB_SKIP_FB)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_B pIn;
                                pIn.pYSrc    = pCurMBRow[0]+posX; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef[0]    = sYInterpolationF.pDst; 
                                pIn.refYStep[0] = sYInterpolationF.dstStep;      
                                pIn.pURef[0]    = sUInterpolationF.pDst; 
                                pIn.refUStep[0] = sUInterpolationF.dstStep;
                                pIn.pVRef[0]    = sVInterpolationF.pDst; 
                                pIn.refVStep[0] = sVInterpolationF.dstStep;

                                pIn.pYRef[1]    = sYInterpolationB.pDst; 
                                pIn.refYStep[1] = sYInterpolationB.dstStep;      
                                pIn.pURef[1]    = sUInterpolationB.pDst; 
                                pIn.refUStep[1] = sUInterpolationB.dstStep;
                                pIn.pVRef[1]    = sVInterpolationB.pDst; 
                                pIn.refVStep[1] = sVInterpolationB.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeB_RD (&pIn, BlockTSTypes)  ; 
#endif                   
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            pCurMBData->CopyBDiffMBProg( pCurMBRow[0],  pCurMBStep[0], pCurMBRow[1],  pCurMBRow[2],  pCurMBStep[1], 
                                sYInterpolationF.pDst,  sYInterpolationF.dstStep,
                                sUInterpolationF.pDst,  sVInterpolationF.pDst,   sUInterpolationF.dstStep,
                                sYInterpolationB.pDst,  sYInterpolationB.dstStep,
                                sUInterpolationB.pDst,  sVInterpolationB.pDst,   sUInterpolationB.dstStep,
                                j, 0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern   = pCompressedMB->GetMBPattern();
                            CBPCY       = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                            if (MBPattern==0 && mvDiffF.x == 0 && mvDiffF.y == 0 && mvDiffB.x == 0 && mvDiffB.y == 0)
                            {
                                pCompressedMB->ChangeType(VC1_ENC_B_MB_SKIP_FB);
                                mbType = VC1_ENC_B_MB_SKIP_FB;
#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetMBAsSkip();
#endif
                            }
                        }

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetMVInfoField(&MVF, mvPredF[MVF.bSecond].x, mvPredF[MVF.bSecond].y, 0);
                        pDebug->SetMVInfoField(&MVB, mvPredB[MVB.bSecond].x, mvPredB[MVB.bSecond].y, 1);
                        pDebug->SetMVInfoField(&MVChromaF,  0, 0, 0, 4);
                        pDebug->SetMVInfoField(&MVChromaF,  0, 0, 0, 5);
                        pDebug->SetMVInfoField(&MVChromaB,  0, 0, 1, 4);
                        pDebug->SetMVInfoField(&MVChromaB,  0, 0, 1, 5);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        Ipp32u ind = (bSecondField)? 1: 0;

                        pDebug->SetInterpInfo(&sYInterpolationF,  &sUInterpolationF, &sVInterpolationF, 0, padding[0][ind][MVF.bSecond]);
                        pDebug->SetInterpInfo(&sYInterpolationB,  &sUInterpolationB, &sVInterpolationB, 1, padding[1][ind][MVF.bSecond]);
#endif
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteBSumMBProg(sYInterpolationF.pDst, sYInterpolationF.dstStep, 
                                    sUInterpolationF.pDst, sVInterpolationF.pDst, sUInterpolationF.dstStep, 
                                    sYInterpolationB.pDst, sYInterpolationB.dstStep, 
                                    sUInterpolationB.pDst, sVInterpolationB.pDst, sUInterpolationB.dstStep,
                                    pRaisedMBRow[0], pRaisedMBStep[0], 
                                    pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                    0, j);
         
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteBSumSkipMBProg(sYInterpolationF.pDst, sYInterpolationF.dstStep, 
                                    sUInterpolationF.pDst, sVInterpolationF.pDst, sUInterpolationF.dstStep, 
                                    sYInterpolationB.pDst, sYInterpolationB.dstStep, 
                                    sUInterpolationB.pDst, sVInterpolationB.pDst, sUInterpolationB.dstStep,
                                    pRaisedMBRow[0], pRaisedMBStep[0], 
                                    pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                    0, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pCurMBInfo->SetExternalEdgeHor(0,0,0);
                                pCurMBInfo->SetExternalEdgeVer(0,0,0);
                                pCurMBInfo->SetInternalEdge(0,0);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }// DEBLOCKING

                            STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                            //pDebug->SetInterpInfo(pYInterpolation, pUInterpolation, pVInterpolation, 0);
#endif
                        } //bRaiseFrame
                        break;
                    }
                case VC1_ENC_B_MB_DIRECT:
                case VC1_ENC_B_MB_SKIP_DIRECT:

                    {
                        //sCoordinate  mvPredF    = {0,0};
                        //sCoordinate  mvPredB    = {0,0};
                        sCoordinate  MVIntF     = {0};
                        sCoordinate  MVQuarterF = {0};
                        sCoordinate  MVIntB     = {0};
                        sCoordinate  MVQuarterB = {0};

                        sCoordinate  MVChromaF       = {0,0};
                        sCoordinate  MVIntChromaF    = {0,0};
                        sCoordinate  MVQuarterChromaF= {0,0};
                        sCoordinate  MVChromaB       = {0,0};
                        sCoordinate  MVIntChromaB    = {0,0};
                        sCoordinate  MVQuarterChromaB= {0,0};

                        eTransformType  BlockTSTypes[6] = { m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType,
                            m_uiTransformType};

                        //direct
                        pCompressedMB->Init(mbType);


                        pCurMBInfo->Init(false);
                        pCurMBInfo->SetMV_F(MVF,true);
                        pCurMBInfo->SetMV_F(MVB,false);

                        if (mbType !=VC1_ENC_B_MB_SKIP_DIRECT || bRaiseFrame)
                        {
                            sCoordinate tF = {MVF.x,MVF.y + ((!MVF.bSecond )? (2-4*(!bBottom)):0),MVF.bSecond};
                            sCoordinate tB = {MVB.x,MVB.y + ((!MVB.bSecond )? (2-4*(!bBottom)):0),MVB.bSecond};

                            GetIntQuarterMV(tF,&MVIntF,&MVQuarterF);
                            CalculateChroma(MVF,&MVChromaF);
                            MVChromaF.y = (!MVF.bSecond )? MVChromaF.y + (2-4*(!bBottom)):MVChromaF.y ;
                            GetIntQuarterMV(MVChromaF,&MVIntChromaF,&MVQuarterChromaF);

                            GetIntQuarterMV(tB,&MVIntB,&MVQuarterB);
                            CalculateChroma(MVB,&MVChromaB);
                            MVChromaB.y = (!MVB.bSecond )? MVChromaB.y + (2-4*(!bBottom)):MVChromaB.y;
                            GetIntQuarterMV(MVChromaB,&MVIntChromaB,&MVQuarterChromaB);

#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(tF.x, tF.y, 0);
                            pDebug->SetIntrpMV(MVChromaF.x, MVChromaF.y, 0, 4);
                            pDebug->SetIntrpMV(MVChromaF.x, MVChromaF.y, 0, 5);

                            pDebug->SetIntrpMV(tB.x, tB.y, 1);
                            pDebug->SetIntrpMV(MVChromaB.x, MVChromaB.y, 1, 4);
                            pDebug->SetIntrpMV(MVChromaB.x, MVChromaB.y, 1, 5);
#endif
                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            Ipp32u ind = (bSecondField)? 1: 0;
                            if (padding[0][ind][MVF.bSecond])
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                SetInterpolationParamsLuma(&sYInterpolationF, pForwMBRow[MVF.bSecond][0], pForwMBStep[MVF.bSecond][0], &MVIntF, &MVQuarterF, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolationF);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolationF, &sVInterpolationF, pForwMBRow[MVF.bSecond][1], pForwMBRow[MVF.bSecond][2],pForwMBStep[MVF.bSecond][1], 
                                    &MVIntChromaF, &MVQuarterChromaF, j);   

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            else
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockYF[MVF.bSecond], j, i , &MVIntF, &MVQuarterF);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYF[MVF.bSecond],0,0,oppositePadding[0][ind][MVF.bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockUF[MVF.bSecond], &InterpolateBlockVF[MVF.bSecond], 
                                                        oppositePadding[0][ind][MVF.bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChromaF, &MVQuarterChromaF);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);

                                                  
                            }
                            if (padding[1][ind][MVB.bSecond])
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                SetInterpolationParamsLuma(&sYInterpolationB, pBackwMBRow[MVB.bSecond][0], pBackwMBStep[MVB.bSecond][0], &MVIntB, &MVQuarterB, j);
                                ippSts = InterpolateLumaFunction(&sYInterpolationB);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChroma (&sUInterpolationB, &sVInterpolationB, pBackwMBRow[MVB.bSecond][1], pBackwMBRow[MVB.bSecond][2],pBackwMBStep[MVB.bSecond][1], 
                                    &MVIntChromaB, &MVQuarterChromaB, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            else
                            {
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                                SetInterpolationParamsLuma(&InterpolateBlockYB[MVB.bSecond], j, i , &MVIntB, &MVQuarterB);
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYB[MVB.bSecond],0,0,oppositePadding[1][ind][MVB.bSecond],true,m_uiRoundControl,bBottom);
                                VC1_ENC_IPP_CHECK(ippSts);

                                InterpolateChromaPad   (&InterpolateBlockUB[MVB.bSecond], &InterpolateBlockVB[MVB.bSecond], 
                                                        oppositePadding[1][ind][MVB.bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChromaB, &MVQuarterChromaB);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);


                            }

                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);

                        }
                        if (mbType !=VC1_ENC_B_MB_SKIP_DIRECT)
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_B pIn;
                                pIn.pYSrc    = pCurMBRow[0]+posX; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef[0]    = sYInterpolationF.pDst; 
                                pIn.refYStep[0] = sYInterpolationF.dstStep;      
                                pIn.pURef[0]    = sUInterpolationF.pDst; 
                                pIn.refUStep[0] = sUInterpolationF.dstStep;
                                pIn.pVRef[0]    = sVInterpolationF.pDst; 
                                pIn.refVStep[0] = sVInterpolationF.dstStep;

                                pIn.pYRef[1]    = sYInterpolationB.pDst; 
                                pIn.refYStep[1] = sYInterpolationB.dstStep;      
                                pIn.pURef[1]    = sUInterpolationB.pDst; 
                                pIn.refUStep[1] = sUInterpolationB.dstStep;
                                pIn.pVRef[1]    = sVInterpolationB.pDst; 
                                pIn.refVStep[1] = sVInterpolationB.dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeB_RD (&pIn, BlockTSTypes)  ; 
#endif                   
                            }
                            pCompressedMB->SetTSType(BlockTSTypes);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyBDiffMBProg( pCurMBRow[0],  pCurMBStep[0], pCurMBRow[1],  pCurMBRow[2],  pCurMBStep[1], 
                                sYInterpolationF.pDst,  sYInterpolationF.dstStep,
                                sUInterpolationF.pDst,  sVInterpolationF.pDst,   sUInterpolationF.dstStep,
                                sYInterpolationB.pDst,  sYInterpolationB.dstStep,
                                sUInterpolationB.pDst,  sVInterpolationB.pDst,   sUInterpolationB.dstStep,
                                j, 0);
                   
                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);


                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern   = pCompressedMB->GetMBPattern();
                            CBPCY       = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);
                        }


#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMVInfoField(&MVF, 0, 0, 0);
                        pDebug->SetMVInfoField(&MVB, 0, 0, 1);
                        pDebug->SetMVInfoField(&MVChromaF,  0, 0, 0, 4);
                        pDebug->SetMVInfoField(&MVChromaF,  0, 0, 0, 5);
                        pDebug->SetMVInfoField(&MVChromaB,  0, 0, 1, 4);
                        pDebug->SetMVInfoField(&MVChromaB,  0, 0, 1, 5);
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetMBType(mbType);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);

                        Ipp32u ind = (bSecondField)? 1: 0;

                        pDebug->SetInterpInfo(&sYInterpolationF,  &sUInterpolationF, &sVInterpolationF, 0, padding[0][ind][MVF.bSecond]);
                        pDebug->SetInterpInfo(&sYInterpolationB,  &sUInterpolationB, &sVInterpolationB, 1, padding[1][ind][MVB.bSecond]);
#endif
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                pRecMBData->PasteBSumMBProg(sYInterpolationF.pDst, sYInterpolationF.dstStep, 
                                    sUInterpolationF.pDst, sVInterpolationF.pDst, sUInterpolationF.dstStep, 
                                    sYInterpolationB.pDst, sYInterpolationB.dstStep, 
                                    sUInterpolationB.pDst, sVInterpolationB.pDst, sUInterpolationB.dstStep,
                                    pRaisedMBRow[0], pRaisedMBStep[0], 
                                    pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                    0, j);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                pRecMBData->PasteBSumSkipMBProg (sYInterpolationF.pDst, sYInterpolationF.dstStep,
                                                                 sUInterpolationF.pDst, sVInterpolationF.pDst, sUInterpolationF.dstStep,
                                                                 sYInterpolationB.pDst, sYInterpolationB.dstStep,
                                                                 sUInterpolationB.pDst, sVInterpolationB.pDst, sUInterpolationB.dstStep,
                                                                 pRaisedMBRow[0], pRaisedMBStep[0],
                                                                 pRaisedMBRow[1], pRaisedMBRow[2],pRaisedMBStep[1],
                                                                 0 , j);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pCurMBInfo->SetExternalEdgeHor(0,0,0);
                                pCurMBInfo->SetExternalEdgeVer(0,0,0);
                                pCurMBInfo->SetInternalEdge(0,0);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }// DEBLOCKING

                            STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                            //pDebug->SetInterpInfo(pYInterpolation, pUInterpolation, pVInterpolation, 0);
#endif
                        } //bRaiseFrame
                        break;
                    }

                case VC1_ENC_B_MB_F_4MV:
                case VC1_ENC_B_MB_SKIP_F_4MV:
                case VC1_ENC_B_MB_B_4MV:
                case VC1_ENC_B_MB_SKIP_B_4MV:
                    {
                        /*--------------------------------------- Inter --------------------------------------------------*/
                        sCoordinate         MVInt;     
                        sCoordinate         MVQuarter; 
                        sCoordinate         pMV [4]         = {0};
                        sCoordinate         pMV1            = {0};     

                        sCoordinate         MVIntChroma             = {0,0};
                        sCoordinate         MVQuarterChroma         = {0,0};
                        sCoordinate         MVChroma                = {0,0};

                        sCoordinate         mvPred[4][2]    = {0};
                        sCoordinate         mvPred1[2]   = {0};

                        bool                bSecondDominantPred = false;
                        bool                bSecondDominantPred1 = false;


                        sCoordinate         *mvC=0, *mvB=0, *mvA=0;
                        sCoordinate         mvCEx = {0};
                        sCoordinate         mvAEx = {0};
                        sCoordinate         mv1, mv2, mv3;


                        sCoordinate         mvDiff = {0};
                        eTransformType      BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType,
                            m_uiTransformType, m_uiTransformType};
                        Ipp32u              backward = (mbType == VC1_ENC_B_MB_F_4MV || mbType == VC1_ENC_B_MB_SKIP_F_4MV)? 0:1;

                        IppVCInterpolate_8u*        pUInterpolation = &sUInterpolationF;
                        IppVCInterpolate_8u*        pVInterpolation = &sVInterpolationF;
                        IppVCInterpolate_8u*        pYInterpolationBlk = sYInterpolationBlkF;

                        IppVCInterpolateBlock_8u*   pInterpolateBlockU= InterpolateBlockUF;
                        IppVCInterpolateBlock_8u*   pInterpolateBlockV= InterpolateBlockVF;

                        IppVCInterpolateBlock_8u*   pInterpolateBlockYBlk[2] = {InterpolateBlockYBlkF[0], InterpolateBlockYBlkF[1]};
                        Ipp8u**                     pMBRow [2] = {pForwMBRow[0],  pForwMBRow[1]};
                        Ipp32u*                     pMBStep[2] = {pForwMBStep[0], pForwMBStep[1]};
                        sScaleInfo*                 pScInfo = &scInfoForw;
                        sScaleInfo *                pInfo1  = &scInfoBackw;
                        sCoordinate                 MV= {0};

                        if (backward)
                        {
                            pUInterpolation = &sUInterpolationB;
                            pVInterpolation = &sVInterpolationB;
                            pYInterpolationBlk = sYInterpolationBlkB;

                            pInterpolateBlockU= InterpolateBlockUB;
                            pInterpolateBlockV= InterpolateBlockVB;

                            pInterpolateBlockYBlk[0] = InterpolateBlockYBlkB[0]; 
                            pInterpolateBlockYBlk[1] = InterpolateBlockYBlkB[1];  
                            pMBRow [0] = pBackwMBRow[0];  
                            pMBRow [1] = pBackwMBRow[1];
                            pMBStep[0] = pBackwMBStep[0];
                            pMBStep[1] = pBackwMBStep[1];

                            pScInfo = &scInfoBackw;
                            pInfo1  = &scInfoForw;
                        }


                        pCurMBInfo->Init(false);
                        pCompressedMB->Init(mbType);

                        STATISTICS_START_TIME(m_TStat->Inter_StartTime);

                        for (blk = 0; blk <4; blk++)
                        {
                            Ipp32u blkPosX = (blk&0x1)<<3;
                            Ipp32u blkPosY = (blk/2)<<3;                   

                            pMV[blk].x         = MEParams->pSrc->MBs[j + i*w].MV[backward][blk].x;
                            pMV[blk].y         = MEParams->pSrc->MBs[j + i*w].MV[backward][blk].y;
                            pMV[blk].bSecond   = (MEParams->pSrc->MBs[j + i*w].Refindex[backward][blk] == 1);

                            bool MV_Second = pMV[blk].bSecond;

                            STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                            GetMVPredictionBBlk_F(blk, !backward);
                            bSecondDominantPred = PredictMVField2(mvA,mvB,mvC, mvPred[blk], pScInfo,bSecondField,&mvAEx,&mvCEx, backward!=0, bMVHalf);
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetScaleType(bSecondDominantPred, backward, blk);
                            pDebug->SetMVInfo(&pMV[blk], mvPred[blk][pMV[blk].bSecond].x,mvPred[blk][pMV[blk].bSecond].y, backward, blk);
                            pDebug->SetPredFlag(bSecondDominantPred!=MV_Second, backward, blk);
                            pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == MV_Second || mvA==0)? mvA:&mvAEx,
                                /*mvB,*/ (mvC && mvC->bSecond == MV_Second || mvC==0)? mvC:&mvCEx, backward, blk);
#endif


                            STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                            if (VC1_ENC_B_MB_SKIP_F_4MV == mbType || VC1_ENC_B_MB_SKIP_B_4MV == mbType)
                            {
                                //correct ME results
                                VM_ASSERT(pMV[blk].x==mvPred[blk][MV_Second].x && pMV[blk].y==mvPred[blk][MV_Second].y);
                                pMV[blk].x=mvPred[blk][MV_Second].x;
                                pMV[blk].y=mvPred[blk][MV_Second].y;
                            }
                            pCurMBInfo->SetMV_F(pMV[blk],  blk, !backward);
                            //pCurMBInfo->SetMV_F(pMV1[blk], blk,  backward);

                            mvDiff.x = pMV[blk].x - mvPred[blk][MV_Second].x;
                            mvDiff.y = pMV[blk].y - mvPred[blk][MV_Second].y;
                            mvDiff.bSecond = (MV_Second != bSecondDominantPred);
                            pCompressedMB->SetBlockdMV_F(mvDiff, blk,!backward);

                            if (VC1_ENC_B_MB_SKIP_F_4MV == mbType || VC1_ENC_B_MB_SKIP_B_4MV == mbType || bRaiseFrame)
                            {
                                sCoordinate t = {pMV[blk].x,pMV[blk].y + ((!MV_Second)? (2-4*(!bBottom)):0),MV_Second};
                                GetIntQuarterMV(t,&MVInt, &MVQuarter);
#ifdef VC1_ENC_DEBUG_ON
                                pDebug->SetIntrpMV(t.x, t.y, backward, blk);
#endif
                                Ipp32u ind = (bSecondField)? 1: 0;
                                STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                if (padding[backward][ind][MV_Second])
                                {
                                    SetInterpolationParamsLumaBlk(&pYInterpolationBlk[blk], pMBRow[MV_Second][0], pMBStep[MV_Second][0], &MVInt, &MVQuarter, j,blk);

                                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                    ippSts = InterpolateLumaFunction(&pYInterpolationBlk[blk]);
                                    VC1_ENC_IPP_CHECK(ippSts);

                                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                                }
                                else
                                {
                                    SetInterpolationParamsLumaBlk(&pInterpolateBlockYBlk[MV_Second][blk], j, i, &MVInt, &MVQuarter, blk);
                                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);                        
                                    ippSts = InterpolateLumaFunctionPadding(&pInterpolateBlockYBlk[MV_Second][blk],0,0,oppositePadding[backward][ind][MV_Second],true,m_uiRoundControl,bBottom);
                                    if (ippSts != ippStsNoErr)
                                        return UMC::UMC_ERR_OPEN_FAILED;
                                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);                        
                                }
                                STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                            } //interpolation
                        } // for (luma blocks


                        // The second vector is restored by prediction
                        mvC=0;
                        mvB=0;
                        mvA=0;
                        GetMVPredictionPField(backward !=0);
                        bSecondDominantPred1 =  PredictMVField2(mvA,mvB,mvC, mvPred1, pInfo1,bSecondField,&mvAEx,&mvCEx, !backward, bMVHalf);
                        pMV1.x         = mvPred1[bSecondDominantPred1].x;
                        pMV1.y         = mvPred1[bSecondDominantPred1].y;
                        pMV1.bSecond   = bSecondDominantPred1;
                        pCurMBInfo->SetMV_F(pMV1, backward !=0);

                        //ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMBMin,MVPredMBMax);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetScaleType(bSecondDominantPred1, !backward, blk);
                        pDebug->SetMVInfo(&pMV1, mvPred1[bSecondDominantPred1].x,mvPred1[bSecondDominantPred1].y, !backward);
                        pDebug->SetPredFlag(bSecondDominantPred1!=bSecondDominantPred1, !backward);
                        pDebug->SetFieldMVPred2Ref((mvA && mvA->bSecond == bSecondDominantPred1 || mvA==0)? mvA:&mvAEx,
                            /*mvB,*/ (mvC && mvC->bSecond == bSecondDominantPred1 || mvC==0)? mvC:&mvCEx, !backward);
#endif                
#ifdef VC1_ENC_DEBUG_ON
                        Ipp32u ind = (bSecondField)? 1: 0;

                        pDebug->SetInterpInfo(&sYInterpolationF,  &sUInterpolationF, &sVInterpolationF, 0, padding[0][ind][MVF.bSecond]);
                        pDebug->SetInterpInfo(&sYInterpolationB,  &sUInterpolationB, &sVInterpolationB, 1, padding[1][ind][MVB.bSecond]);
#endif
                        pCurMBInfo->GetLumaVectorFields(&MV,true,!backward);

                        /*-------------------------- interpolation for chroma---------------------------------*/

                        if (VC1_ENC_B_MB_F_4MV == mbType || VC1_ENC_B_MB_B_4MV == mbType || bRaiseFrame)
                        {                       
                            CalculateChroma(MV,&MVChroma);
                            pCurMBInfo->SetMVChroma_F(MVChroma,!backward);
                            MVChroma.y = MVChroma.y + ((!MV.bSecond)? (2-4*(!bBottom)):0);
                            GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);

#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, backward, 4);
                            pDebug->SetIntrpMV(MVChroma.x, MVChroma.y, backward, 5);
#endif                       

                            Ipp32u ind = (bSecondField)? 1: 0;

                            STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                            if (padding[backward][ind][MV.bSecond])
                            {                                
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                                InterpolateChroma (pUInterpolation, pVInterpolation, pMBRow[MV.bSecond][1], pMBRow[MV.bSecond][2],pMBStep[MV.bSecond][1], 
                                                    &MVIntChroma, &MVQuarterChroma, j);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            else
                            {                        
                                
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);      
                                
                                InterpolateChromaPad   (&pInterpolateBlockU[MV.bSecond], &pInterpolateBlockV[MV.bSecond], 
                                                        oppositePadding[backward][ind][MV.bSecond],true,m_uiRoundControl,bBottom,
                                                        j, i ,  &MVIntChroma, &MVQuarterChroma);

                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }
                            STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);                /////////////////////////////////////////////
                        }


#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetInterpInfo(&sYInterpolationF,  &sUInterpolationF, &sVInterpolationF, 0, padding[0][ind][MVF.bSecond]);
                        pDebug->SetInterpInfo(&sYInterpolationB,  &sUInterpolationB, &sVInterpolationB, 1, padding[1][ind][MVB.bSecond]);
#endif

                        /////////////////////////////////////////////////////////////////////////////////////////////////////////////
                        if (VC1_ENC_B_MB_F_4MV == mbType || VC1_ENC_B_MB_B_4MV == mbType )
                        {
                            if (bCalculateVSTransform)
                            {
#ifndef OWN_VS_TRANSFORM
                                GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                                VC1EncMD_P pIn;
                                pIn.pYSrc    = pCurMBRow[0]+posX; 
                                pIn.srcYStep = m_uiPlaneStep[0];
                                pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                                pIn.srcUStep = m_uiPlaneStep[1];
                                pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                                pIn.srcVStep = m_uiPlaneStep[2];

                                pIn.pYRef    = pYInterpolationBlk->pDst; 
                                pIn.refYStep = pYInterpolationBlk->dstStep;      
                                pIn.pURef    = pUInterpolation->pDst; 
                                pIn.refUStep = pUInterpolation->dstStep;
                                pIn.pVRef    = pVInterpolation->pDst; 
                                pIn.refVStep = pVInterpolation->dstStep;

                                pIn.quant         = doubleQuant;
                                pIn.bUniform      = m_bUniformQuant;
                                pIn.intraPattern  = 0;
                                pIn.DecTypeAC1    = m_uiDecTypeAC1;
                                pIn.pScanMatrix   = ZagTables_Adv;
                                pIn.bField        = 1;
                                pIn.CBPTab        = m_uiCBPTab;

                                GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif              
                            }           
                            pCompressedMB->SetTSType(BlockTSTypes);

                            IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                            pCurMBData->CopyDiffMBProg (pCurMBRow[0],pCurMBStep[0], pCurMBRow[1],pCurMBRow[2], pCurMBStep[1], 
                                pYInterpolationBlk->pDst, pYInterpolationBlk->dstStep,
                                pUInterpolation->pDst,pVInterpolation->pDst, pUInterpolation->dstStep,
                                j,0);

                            IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                            for (blk = 0; blk<6; blk++)
                            {
                                STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                                InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                    BlockTSTypes[blk], doubleQuant, MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(Ipp8u));
                                STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                                pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                    pCurMBData->m_uiBlockStep[blk],
                                    ZagTables_Fields[BlockTSTypes[blk]],
                                    blk);
                            }
                            MBPattern = pCompressedMB->GetMBPattern();
                            CBPCY = MBPattern;
                            pCurMBInfo->SetMBPattern(MBPattern);
                            pCompressedMB->SetMBCBPCY(CBPCY);

                        }//VC1_ENC_P_MB_SKIP_1MV != MBType
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(mbType);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, backward, 4);
                        pDebug->SetMVInfoField(&MVChroma,  0, 0, backward ,5);
                        pDebug->SetQuant(m_uiQuant,m_bHalfQuant);

                        if (pCompressedMB->isSkip() )
                        {
                            pDebug->SetMBAsSkip();
                        }
                        else
                        {
                            pDebug->SetCPB(MBPattern, CBPCY);
                            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                        }
#endif
                        /*---------------------------Reconstruction ------------------------------------*/
                        STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                        if (bRaiseFrame)
                        {
                            if (MBPattern != 0)
                            {
                                pRecMBData->Reset();
                                STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                                for (blk=0;blk<6; blk++)
                                {
                                    if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                                    {
                                        InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                                    }
                                }
                                STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                pRecMBData->PasteSumMBProg (pYInterpolationBlk->pDst, pYInterpolationBlk->dstStep, 
                                                            pUInterpolation->pDst, pVInterpolation->pDst,pUInterpolation->dstStep, 
                                                            pRaisedMBRow[0], pRaisedMBStep[0], 
                                                            pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],                             
                                                            0,j);
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            } //(MBPattern != 0)
                            else
                            {
                                IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                                pRecMBData->PasteSumSkipMBProg (pYInterpolationBlk->pDst,pYInterpolationBlk->dstStep,
                                                                pUInterpolation->pDst, pVInterpolation->pDst, pUInterpolation->dstStep,
                                                                pRaisedMBRow[0], pRaisedMBStep[0], 
                                                                pRaisedMBRow[1], pRaisedMBRow[2], pRaisedMBStep[1],0,j);                                
                                IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                            }

                            //deblocking
                            if (m_pSequenceHeader->IsLoopFilter())
                            {
                                Ipp8u YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                                Ipp8u UFlag0 = 0,UFlag1 = 0;
                                Ipp8u VFlag0 = 0,VFlag1 = 0;

                                pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                                pCurMBInfo->SetVSTPattern(BlockTSTypes);

                                pCurMBInfo->SetExternalEdgeHor(0,0,0);
                                pCurMBInfo->SetExternalEdgeVer(0,0,0);
                                pCurMBInfo->SetInternalEdge(0,0);

                                GetInternalBlockEdge(pCurMBInfo,
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);//ver

                                pCurMBInfo->SetInternalBlockEdge(
                                    YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                    YFlag2,YFlag3, UFlag1,VFlag1);// ver

                                //deblocking
                                {
                                    STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                                    Ipp8u *pDblkPlanes[3] = {pRaisedMBRow[0], pRaisedMBRow[1], pRaisedMBRow[2]};
                                    m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, pRaisedMBStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                                    deblkPattern = deblkPattern | 0x1 | ((!(Ipp8u)((j + 1 - (w -1)) >> 31)<<1));
                                    STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                                }
                            }// DEBLOCKING
                        }
                        STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetInterpInfo(&pYInterpolationBlk[0], pUInterpolation, pVInterpolation, 0, padding[backward][ind][MV.bSecond]);
#endif
                    }
                    break;            
                default:                
                    return UMC::UMC_ERR_FAILED;
                }

#ifdef VC1_ENC_DEBUG_ON
                if(!bSubBlkTS)
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(), 15, 15);
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(), 15, 15);
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(),15);
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), 15);
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), 15);
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), 15);
                }
                else
                {
                    pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(),
                        pCurMBInfo->GetLumaAdUppEdge(), pCurMBInfo->GetLumaAdBotEdge() );
                    pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(),
                        pCurMBInfo->GetLumaAdLefEdge(), pCurMBInfo->GetLumaAdRigEdge());
                    pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(), pCurMBInfo->GetUAdHorEdge());
                    pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), pCurMBInfo->GetVAdHorEdge());
                    pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), pCurMBInfo->GetUAdVerEdge());
                    pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), pCurMBInfo->GetVAdVerEdge());
                }
#endif
                err = m_pMBs->NextMB();
                if (err != UMC::UMC_OK && j < w-1)
                    return err;

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
            }

            deblkPattern = (deblkPattern | 0x4 | ( (! (Ipp8u)((i + 1 - (hMB -1)) >> 31)<<3))) & deblkMask;

            ////Row deblocking
            //STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            //        if (m_pSequenceHeader->IsLoopFilter() && bRaiseFrame && (i != firstRow || firstRow == hMB-1))
            //        {
            //STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
            //          Ipp8u *DeblkPlanes[3] = {m_pRaisedPlane[0] + i*m_uiRaisedPlaneStep[0]*VC1_ENC_LUMA_SIZE,
            //                                   m_pRaisedPlane[1] + i*m_uiRaisedPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
            //                                   m_pRaisedPlane[2] + i*m_uiRaisedPlaneStep[2]*VC1_ENC_CHROMA_SIZE};
            //            m_pMBs->StartRow();
            //            if(bSubBlkTS)
            //            {
            //                if(i < hMB - 1)
            //                    Deblock_P_RowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                  if(firstRow != hMB -1)
            //                    Deblock_P_BottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                    Deblock_P_TopBottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //            }
            //            else
            //            {
            //                if(i < hMB - 1)
            //                    Deblock_P_RowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                  if(firstRow != hMB -1)
            //                    Deblock_P_BottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //                else
            //                    Deblock_P_TopBottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
            //            }
            //STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            //        }
            //STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

            err = m_pMBs->NextRow();
            if (err != UMC::UMC_OK)
                return err;

            pCurMBRow[0]+= pCurMBStep[0]*VC1_ENC_LUMA_SIZE;
            pCurMBRow[1]+= pCurMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pCurMBRow[2]+= pCurMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pRaisedMBRow[0]+= pRaisedMBStep[0]*VC1_ENC_LUMA_SIZE;
            pRaisedMBRow[1]+= pRaisedMBStep[1]*VC1_ENC_CHROMA_SIZE;
            pRaisedMBRow[2]+= pRaisedMBStep[2]*VC1_ENC_CHROMA_SIZE;

            pForwMBRow[0][0]+= pForwMBStep[0][0]*VC1_ENC_LUMA_SIZE;
            pForwMBRow[0][1]+= pForwMBStep[0][1]*VC1_ENC_CHROMA_SIZE;
            pForwMBRow[0][2]+= pForwMBStep[0][2]*VC1_ENC_CHROMA_SIZE;

            pForwMBRow[1][0]+= pForwMBStep[1][0]*VC1_ENC_LUMA_SIZE;
            pForwMBRow[1][1]+= pForwMBStep[1][1]*VC1_ENC_CHROMA_SIZE;
            pForwMBRow[1][2]+= pForwMBStep[1][2]*VC1_ENC_CHROMA_SIZE;

            pBackwMBRow[0][0]+= pBackwMBStep[0][0]*VC1_ENC_LUMA_SIZE;
            pBackwMBRow[0][1]+= pBackwMBStep[0][1]*VC1_ENC_CHROMA_SIZE;
            pBackwMBRow[0][2]+= pBackwMBStep[0][2]*VC1_ENC_CHROMA_SIZE;

            pBackwMBRow[1][0]+= pBackwMBStep[1][0]*VC1_ENC_LUMA_SIZE;
            pBackwMBRow[1][1]+= pBackwMBStep[1][1]*VC1_ENC_CHROMA_SIZE;
            pBackwMBRow[1][2]+= pBackwMBStep[1][2]*VC1_ENC_CHROMA_SIZE;

        }

#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame && bSecondField && i == h)
            pDebug->PrintRestoredFrame(m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
            m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
            m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 1);
#endif

        return err;
    }

    UMC::Status VC1EncoderPictureADV::VLC_B_Frame(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status    err = UMC::UMC_OK;
        Ipp32s         i = 0, j = 0, blk = 0;

        Ipp32s         h = (m_pSequenceHeader->GetPictureHeight()+15)/16;
        Ipp32s         w = (m_pSequenceHeader->GetPictureWidth()+15)/16;
        Ipp32s         hMB = (nRows > 0 && (firstRow + nRows)<h)? firstRow + nRows : h;

        const Ipp16u*  pCBPCYTable = VLCTableCBPCY_PB[m_uiCBPTab];

        eCodingSet     LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet     ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
        eCodingSet     CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

        const sACTablesSet*   pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[LumaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra]),
            &(ACTablesSet[ChromaCodingSetIntra])};

        const sACTablesSet*   pACTablesSetInter = &(ACTablesSet[CodingSetInter]);


        const Ipp32u*  pDCTableVLCIntra[6]  = {DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][0],
            DCTables[m_uiDecTypeDCIntra][1],
            DCTables[m_uiDecTypeDCIntra][1]};

        sACEscInfo  ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                           0, 0};

        bool bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);
        bool bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR || m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

        const Ipp16s                (*pTTMBVLC)[4][6] =  0;
        const Ipp8u                 (* pTTBlkVLC)[6] = 0;
        const Ipp8u                 *pSubPattern4x4VLC=0;

#ifdef VC1_ENC_DEBUG_ON
        //pDebug->SetCurrMBFirst();
        pDebug->SetCurrMB(false, 0, firstRow);
#endif

        //err = WriteBPictureHeader(pCodedPicture);
        //if (err != UMC::UMC_OK)
        //     return err;

        if (m_uiQuant<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_uiQuant<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        for (i = firstRow; i < hMB; i++)
        {
            for (j=0; j < w; j++)
            {
                VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->whole = 0;
                memset(m_MECurMbStat->MVF, 0,   4*sizeof(Ipp16u));
                memset(m_MECurMbStat->MVB, 0,   4*sizeof(Ipp16u));
                memset(m_MECurMbStat->coeff, 0, 6*sizeof(Ipp16u));
                m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;

                pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
#endif

                switch(pCompressedMB->GetMBType())
                {
                case VC1_ENC_B_MB_INTRA:
                    {
#ifdef VC1_ME_MB_STATICTICS
                        m_MECurMbStat->MbType = UMC::ME_MbIntra;
#endif

                        err = pCompressedMB->WriteMBHeaderB_INTRA  (pCodedPicture,
                            m_bRawBitplanes,
                            MVDiffTablesVLC[m_uiMVTab],
                            pCBPCYTable);
                        VC1_ENC_CHECK (err)

                            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                        for (blk = 0; blk<6; blk++)
                        {
                            err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLCIntra[blk],m_uiQuant,blk);
                            VC1_ENC_CHECK (err)
                                err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesSetIntra[blk],&ACEscInfo,blk);
                            VC1_ENC_CHECK (err)
                        }//for
                        STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
                    }
                    break;
                case VC1_ENC_B_MB_SKIP_F:
                case VC1_ENC_B_MB_SKIP_B:
                case VC1_ENC_B_MB_SKIP_FB:
#ifdef VC1_ME_MB_STATICTICS
                    if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_SKIP_F)
                        m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
                    else if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_SKIP_F)
                        m_MECurMbStat->MbType = UMC::ME_MbBkwSkipped;
                    else
                        m_MECurMbStat->MbType = UMC::ME_MbBidirSkipped;
#endif

                    err = pCompressedMB->WriteBMB_SKIP_NONDIRECT(  pCodedPicture,
                        m_bRawBitplanes,
                        2*m_uiBFraction.num < m_uiBFraction.denom);
                    VC1_ENC_CHECK (err)
                        break;
                case VC1_ENC_B_MB_F:
                case VC1_ENC_B_MB_B:
                case VC1_ENC_B_MB_FB:
#ifdef VC1_ME_MB_STATICTICS
                    if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_F)
                        m_MECurMbStat->MbType = UMC::ME_MbFrw;
                    else if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_B)
                        m_MECurMbStat->MbType = UMC::ME_MbBkw;
                    else
                        m_MECurMbStat->MbType = UMC::ME_MbBidir;
#endif

                    err = pCompressedMB->WriteBMB  (pCodedPicture,
                        m_bRawBitplanes,
                        m_uiMVTab,
                        m_uiMVRangeIndex,
                        pCBPCYTable,
                        bCalculateVSTransform,
                        bMVHalf,
                        pTTMBVLC,
                        pTTBlkVLC,
                        pSubPattern4x4VLC,
                        pACTablesSetInter,
                        &ACEscInfo,
                        2*m_uiBFraction.num < m_uiBFraction.denom);
                    VC1_ENC_CHECK (err)
                        break;
                case VC1_ENC_B_MB_DIRECT:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbDirect;
#endif
                    err = pCompressedMB->WriteBMB_DIRECT  (
                        pCodedPicture,
                        m_bRawBitplanes,
                        pCBPCYTable,
                        bCalculateVSTransform,
                        pTTMBVLC,
                        pTTBlkVLC,
                        pSubPattern4x4VLC,
                        pACTablesSetInter,
                        &ACEscInfo);
                    VC1_ENC_CHECK (err)
                        break;
                case VC1_ENC_B_MB_SKIP_DIRECT:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbDirectSkipped;
#endif
                    err = pCompressedMB->WriteBMB_SKIP_DIRECT(  pCodedPicture,
                        m_bRawBitplanes);
                    VC1_ENC_CHECK (err)

                        break;
                }

#ifdef VC1_ENC_DEBUG_ON
                pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat++;
#endif
            }
        }

        err = pCodedPicture->AddLastBits();
        VC1_ENC_CHECK (err)

            return err;
    }

    UMC::Status VC1EncoderPictureADV::WritePPictureHeader(VC1EncoderBitStreamAdv* pCodedPicture)
    {
        UMC::Status     err           =   UMC::UMC_OK;
        Ipp8s           diff          =   m_uiAltPQuant -  m_uiQuant - 1;


        err = pCodedPicture->PutStartCode(0x0000010D);
        if (err != UMC::UMC_OK) return err;


        if (m_pSequenceHeader->IsInterlace())
        {
            err = pCodedPicture->PutBits(0,1); // progressive frame
            if (err != UMC::UMC_OK) return err;
        }
        //picture type - P frame
        err = pCodedPicture->PutBits(0x00,1);
        if (err != UMC::UMC_OK) return err;

        if (m_pSequenceHeader->IsFrameCounter())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsPullDown())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsPanScan())
        {
            assert(0);
        }
        err = pCodedPicture->PutBits(m_uiRoundControl,1);
        if (err != UMC::UMC_OK) return err;

        if (m_pSequenceHeader->IsInterlace())
        {
            err = pCodedPicture->PutBits(m_bUVSamplingInterlace,1);
            if (err != UMC::UMC_OK) return err;
        }
        if (m_pSequenceHeader->IsFrameInterpolation())
        {
            err =pCodedPicture->PutBits(m_bFrameInterpolation,1);
            if (err != UMC::UMC_OK)
                return err;
        }
        err = pCodedPicture->PutBits(m_uiQuantIndex, 5);
        if (err != UMC::UMC_OK)
            return err;

        if (m_uiQuantIndex <= 8)
        {
            err = pCodedPicture->PutBits(m_bHalfQuant, 1);
            if (err != UMC::UMC_OK)
                return err;
        }
        if (m_pSequenceHeader->GetQuantType()== VC1_ENC_QTYPE_EXPL)
        {
            err = pCodedPicture->PutBits(m_bUniformQuant,1);
            if (err != UMC::UMC_OK)
                return err;
        }
        if (m_pSequenceHeader->IsPostProc())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsExtendedMV())
        {
            err = pCodedPicture->PutBits(MVRangeCodesVLC[m_uiMVRangeIndex*2],MVRangeCodesVLC[m_uiMVRangeIndex*2+1]);
            if (err != UMC::UMC_OK)
                return err;
        }
        err = pCodedPicture->PutBits(MVModeP[m_bIntensity*2 + (m_uiQuant <= 12)][2*m_uiMVMode],MVModeP[m_bIntensity*2 + (m_uiQuant <= 12)][2*m_uiMVMode+1]);
        if (err != UMC::UMC_OK)
            return err;

        if (m_bIntensity)
        {
            err = pCodedPicture->PutBits(m_uiIntensityLumaScale,6);
            if (err != UMC::UMC_OK)
                return err;
            err = pCodedPicture->PutBits(m_uiIntensityLumaShift,6);
            if (err != UMC::UMC_OK)
                return err;
        }
        assert( m_bRawBitplanes == true);
        if (m_uiMVMode == VC1_ENC_MIXED_QUARTER_BICUBIC)
        {
            //raw bitplane for MB type
            err = pCodedPicture->PutBits(0,5);
            if (err != UMC::UMC_OK)
                return err;
        }
        // raw bitplane for skip MB
        err = pCodedPicture->PutBits(0,5);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiMVTab,2);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiCBPTab,2);
        if (err != UMC::UMC_OK)
            return err;

        switch (m_pSequenceHeader->GetDQuant())
        {
        case 2:
            if (diff>=0 && diff<7)
            {
                err = pCodedPicture->PutBits(diff,3);
            }
            else
            {
                err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
            }
            if (err != UMC::UMC_OK)
                return err;
            break;
        case 1:
            err = pCodedPicture->PutBits( m_QuantMode != VC1_ENC_QUANT_SINGLE, 1);
            if (err != UMC::UMC_OK)
                return err;
            if (m_QuantMode != VC1_ENC_QUANT_SINGLE)
            {
                err =  pCodedPicture->PutBits(QuantProfileTableVLC[2*m_QuantMode], QuantProfileTableVLC[2*m_QuantMode+1]);
                if (err != UMC::UMC_OK)
                    return err;
                if (m_QuantMode != VC1_ENC_QUANT_MB_ANY)
                {
                    if (diff>=0 && diff<7)
                    {
                        err = pCodedPicture->PutBits(diff,3);
                    }
                    else
                    {
                        err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
                    }
                    if (err != UMC::UMC_OK)
                        return err;
                }
            }
            break;
        default:
            break;
        }

        if (m_pSequenceHeader->IsVSTransform())
        {
            err = pCodedPicture->PutBits(m_bVSTransform,1);
            if (err != UMC::UMC_OK)
                return err;
            if (m_bVSTransform)
            {
                err = pCodedPicture->PutBits(m_uiTransformType,2);
                if (err != UMC::UMC_OK)
                    return err;
            }
        }
        err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC1],ACTableCodesVLC[2*m_uiDecTypeAC1+1]);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiDecTypeDCIntra,1);
        return err;
    }
    UMC::Status VC1EncoderPictureADV::WriteSkipPictureHeader(VC1EncoderBitStreamAdv* pCodedPicture)
    {
        UMC::Status     err           =   UMC::UMC_OK;

        err = pCodedPicture->PutStartCode(0x0000010D);
        if (err != UMC::UMC_OK) return err;


        if (m_pSequenceHeader->IsInterlace())
        {
            err = pCodedPicture->PutBits(0,1); // progressive frame
            if (err != UMC::UMC_OK) return err;
        }
        //picture type - skip frame
        err = pCodedPicture->PutBits(0x0F,4);
        if (err != UMC::UMC_OK) return err;

        if (m_pSequenceHeader->IsFrameCounter())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsPullDown())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsPanScan())
        {
            assert(0);
        }
        return err;
    }
    UMC::Status VC1EncoderPictureADV::WriteBPictureHeader(VC1EncoderBitStreamAdv* pCodedPicture)
    {
        UMC::Status     err           =   UMC::UMC_OK;
        Ipp8s           diff          =   m_uiAltPQuant -  m_uiQuant - 1;

        err = pCodedPicture->PutStartCode(0x0000010D);
        if (err != UMC::UMC_OK) return err;

        if (m_pSequenceHeader->IsInterlace())
        {
            err = pCodedPicture->PutBits(0,1); // progressive frame
            if (err != UMC::UMC_OK) return err;
        }
        //picture type - B frame
        err = pCodedPicture->PutBits(0x02,2);
        if (err != UMC::UMC_OK) return err;

        if (m_pSequenceHeader->IsFrameCounter())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsPullDown())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsPanScan())
        {
            assert(0);
        }
        err = pCodedPicture->PutBits(m_uiRoundControl,1);
        if (err != UMC::UMC_OK) return err;

        if (m_pSequenceHeader->IsInterlace())
        {

            err = pCodedPicture->PutBits(m_bUVSamplingInterlace,1);
            if (err != UMC::UMC_OK) return err;
        }
        if (m_pSequenceHeader->IsFrameInterpolation())
        {
            err =pCodedPicture->PutBits(m_bFrameInterpolation,1);
            if (err != UMC::UMC_OK)
                return err;
        }
        err = pCodedPicture->PutBits(BFractionVLC[m_uiBFraction.denom ][2*m_uiBFraction.num],BFractionVLC[m_uiBFraction.denom ][2*m_uiBFraction.num+1]);
        if (err!= UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiQuantIndex, 5);
        if (err != UMC::UMC_OK)
            return err;

        if (m_uiQuantIndex <= 8)
        {
            err = pCodedPicture->PutBits(m_bHalfQuant, 1);
            if (err != UMC::UMC_OK)
                return err;
        }
        if (m_pSequenceHeader->GetQuantType()== VC1_ENC_QTYPE_EXPL)
        {
            err = pCodedPicture->PutBits(m_bUniformQuant,1);
            if (err != UMC::UMC_OK)
                return err;
        }
        if (m_pSequenceHeader->IsPostProc())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsExtendedMV())
        {
            err = pCodedPicture->PutBits(MVRangeCodesVLC[m_uiMVRangeIndex*2],MVRangeCodesVLC[m_uiMVRangeIndex*2+1]);
            if (err != UMC::UMC_OK)
                return err;
        }
        //MV mode
        err = pCodedPicture->PutBits(m_uiMVMode == VC1_ENC_1MV_QUARTER_BICUBIC,1);
        if (err != UMC::UMC_OK)
            return err;

        // raw bitplane for direct MB
        err = pCodedPicture->PutBits(0,5);
        if (err != UMC::UMC_OK)
            return err;

        // raw bitplane for skip MB
        err = pCodedPicture->PutBits(0,5);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiMVTab,2);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiCBPTab,2);
        if (err != UMC::UMC_OK)
            return err;

        switch (m_pSequenceHeader->GetDQuant())
        {
        case 2:
            if (diff>=0 && diff<7)
            {
                err = pCodedPicture->PutBits(diff,3);
            }
            else
            {
                err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
            }
            if (err != UMC::UMC_OK)
                return err;
            break;
        case 1:
            err = pCodedPicture->PutBits( m_QuantMode != VC1_ENC_QUANT_SINGLE, 1);
            if (err != UMC::UMC_OK)
                return err;
            if (m_QuantMode != VC1_ENC_QUANT_SINGLE)
            {
                err =  pCodedPicture->PutBits(QuantProfileTableVLC[2*m_QuantMode], QuantProfileTableVLC[2*m_QuantMode+1]);
                if (err != UMC::UMC_OK)
                    return err;
                if (m_QuantMode != VC1_ENC_QUANT_MB_ANY)
                {
                    if (diff>=0 && diff<7)
                    {
                        err = pCodedPicture->PutBits(diff,3);
                    }
                    else
                    {
                        err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
                    }
                    if (err != UMC::UMC_OK)
                        return err;
                }
            }
            break;
        default:
            break;
        }
        if (m_pSequenceHeader->IsVSTransform())
        {
            err = pCodedPicture->PutBits(m_bVSTransform,1);
            if (err != UMC::UMC_OK)
                return err;
            if (m_bVSTransform)
            {
                err = pCodedPicture->PutBits(m_uiTransformType,2);
                if (err != UMC::UMC_OK)
                    return err;
            }
        }
        err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC1],ACTableCodesVLC[2*m_uiDecTypeAC1+1]);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiDecTypeDCIntra,1);
        return err;
    }


    UMC::Status VC1EncoderPictureADV::WriteFieldPictureHeader(VC1EncoderBitStreamAdv* pCodedPicture)
    {
        UMC::Status     err           =   UMC::UMC_OK;
        bool            bRef          =   false;
        static  Ipp16u   refDistVLC [17*2] =
        {
            0x0000  ,   2,
            0x0001  ,   2,
            0x0002  ,   2,
            0x0006    ,    3    ,
            0x000e    ,    4    ,
            0x001e    ,    5    ,
            0x003e    ,    6    ,
            0x007e    ,    7    ,
            0x00fe    ,    8    ,
            0x01fe    ,    9    ,
            0x03fe    ,    10    ,
            0x07fe    ,    11    ,
            0x0ffe    ,    12    ,
            0x1ffe    ,    13    ,
            0x3ffe    ,    14    ,
            0x7ffe    ,    15    ,
            0xfffe    ,    16
        };

        assert(m_pSequenceHeader->IsInterlace());

        err = pCodedPicture->PutStartCode(0x0000010D);
        if (err != UMC::UMC_OK) return err;

        err = pCodedPicture->PutBits(3,2); // field interlace
        if (err != UMC::UMC_OK) return err;

        switch(m_uiPictureType)
        {
        case VC1_ENC_I_I_FIELD:
            err = pCodedPicture->PutBits(0,3);
            bRef = true;
            break;
        case VC1_ENC_I_P_FIELD:
            err = pCodedPicture->PutBits(1,3);
            bRef = true;
            break;
        case VC1_ENC_P_I_FIELD:
            err = pCodedPicture->PutBits(2,3);
            bRef = true;
            break;
        case VC1_ENC_P_P_FIELD:
            err = pCodedPicture->PutBits(3,3);
            bRef = true;
            break;
        case VC1_ENC_B_B_FIELD:
            err = pCodedPicture->PutBits(4,3);
            break;
        case VC1_ENC_B_BI_FIELD:
            err = pCodedPicture->PutBits(5,3);
            break;
        case VC1_ENC_BI_B_FIELD:
            err = pCodedPicture->PutBits(6,3);
            break;
        case VC1_ENC_BI_BI_FIELD:
            err = pCodedPicture->PutBits(7,3);
            break;
        default:
            return UMC::UMC_ERR_FAILED;
        }
        if (err != UMC::UMC_OK) return err;
        if (m_pSequenceHeader->IsFrameCounter())
        {
            assert(0);
        }
        if (m_pSequenceHeader->IsPullDown())
        {
            //only interlaced case
            err = pCodedPicture->PutBits(m_bTopFieldFirst,1);
            if (err != UMC::UMC_OK) return err;
            err = pCodedPicture->PutBits(m_bRepeateFirstField,1);
            if (err != UMC::UMC_OK) return err;
        }
        if (m_pSequenceHeader->IsPanScan())
        {
            assert(0);
        }

        err = pCodedPicture->PutBits(m_uiRoundControl,1);
        if (err != UMC::UMC_OK) return err;

        err = pCodedPicture->PutBits(m_bUVSamplingInterlace,1);
        if (err != UMC::UMC_OK) return err;

        if (bRef)
        {
            if (m_pSequenceHeader->IsReferenceFrameDistance())
            {
                m_nReferenceFrameDist = (m_nReferenceFrameDist>16)? 16 : m_nReferenceFrameDist;
                err = pCodedPicture->PutBits(refDistVLC[m_nReferenceFrameDist<<1],refDistVLC[(m_nReferenceFrameDist<<1)+1]);
                if (err != UMC::UMC_OK) return err;
            }
        }
        else
        {
            err = pCodedPicture->PutBits(BFractionVLC[m_uiBFraction.denom ][2*m_uiBFraction.num],BFractionVLC[m_uiBFraction.denom ][2*m_uiBFraction.num+1]);
            if (err!= UMC::UMC_OK) return err;
        }
        return err;
    }
    UMC::Status VC1EncoderPictureADV::WriteIFieldHeader(VC1EncoderBitStreamAdv* pCodedPicture)
    {
        UMC::Status     err               =   UMC::UMC_OK;
        Ipp8u         condoverVLC[6]    =   {0,1,  3,2,    2,2};

        err = pCodedPicture->PutBits(m_uiQuantIndex, 5);
        if (err != UMC::UMC_OK)
            return err;

        if (m_uiQuantIndex <= 8)
        {
            err = pCodedPicture->PutBits(m_bHalfQuant, 1);
            if (err != UMC::UMC_OK)
                return err;
        }
        if (m_pSequenceHeader->GetQuantType()== VC1_ENC_QTYPE_EXPL)
        {
            err = pCodedPicture->PutBits(m_bUniformQuant,1);
            if (err != UMC::UMC_OK)
                return err;
        }
        if (m_pSequenceHeader->IsPostProc())
        {
            assert(0);
        }

        assert( m_bRawBitplanes == true);

        //raw bitplane for AC prediction
        err = pCodedPicture->PutBits(0,5);
        if (err != UMC::UMC_OK)   return err;

        if (m_pSequenceHeader->IsOverlap() && m_uiQuant<=8)
        {
            err = pCodedPicture->PutBits(condoverVLC[2*m_uiCondOverlap],condoverVLC[2*m_uiCondOverlap+1]);
            if (err != UMC::UMC_OK)   return err;
            if (VC1_ENC_COND_OVERLAP_SOME == m_uiCondOverlap)
            {
                //bitplane
                assert( m_bRawBitplanes == true);
                //raw bitplane for AC prediction
                err = pCodedPicture->PutBits(0,5);
                if (err != UMC::UMC_OK)   return err;
            }
        }

        err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC1],ACTableCodesVLC[2*m_uiDecTypeAC1+1]);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC2],ACTableCodesVLC[2*m_uiDecTypeAC2+1]);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiDecTypeDCIntra,1);
        return err;
    }
    UMC::Status VC1EncoderPictureADV::WritePFieldHeader(VC1EncoderBitStreamAdv* pCodedPicture)
    {
        UMC::Status     err         =   UMC::UMC_OK;
        bool            bIntensity  =   (m_uiFieldIntensityType!=VC1_ENC_FIELD_NONE);
        Ipp8s           diff          =   m_uiAltPQuant -  m_uiQuant - 1;


        err = pCodedPicture->PutBits(m_uiQuantIndex, 5);
        if (err != UMC::UMC_OK)
            return err;

        if (m_uiQuantIndex <= 8)
        {
            err = pCodedPicture->PutBits(m_bHalfQuant, 1);
            if (err != UMC::UMC_OK) return err;
        }
        if (m_pSequenceHeader->GetQuantType()== VC1_ENC_QTYPE_EXPL)
        {
            err = pCodedPicture->PutBits(m_bUniformQuant,1);
            if (err != UMC::UMC_OK) return err;
        }
        if (m_pSequenceHeader->IsPostProc())
        {
            assert(0);
        }
        pCodedPicture->PutBits(m_uiReferenceFieldType == VC1_ENC_REF_FIELD_BOTH, 1);
        if (err != UMC::UMC_OK) return err;

        if (m_uiReferenceFieldType != VC1_ENC_REF_FIELD_BOTH)
        {
            pCodedPicture->PutBits(m_uiReferenceFieldType == VC1_ENC_REF_FIELD_SECOND, 1);
            if (err != UMC::UMC_OK) return err;
        }
        if (m_pSequenceHeader->IsExtendedMV())
        {
            err = pCodedPicture->PutBits(MVRangeCodesVLC[m_uiMVRangeIndex*2],MVRangeCodesVLC[m_uiMVRangeIndex*2+1]);
            if (err != UMC::UMC_OK)
                return err;
        }

        if (m_pSequenceHeader->IsExtendedDMV())
        {
            err = pCodedPicture->PutBits(MVRangeCodesVLC[m_uiDMVRangeIndex*2],MVRangeCodesVLC[m_uiDMVRangeIndex*2+1]);
            if (err != UMC::UMC_OK)
                return err;
        }

        err = pCodedPicture->PutBits(MVModeP[bIntensity*2 + (m_uiQuant <= 12)][2*m_uiMVMode],MVModeP[bIntensity*2 + (m_uiQuant <= 12)][2*m_uiMVMode+1]);
        if (err != UMC::UMC_OK)
            return err;

        if (bIntensity)
        {
            switch(m_uiFieldIntensityType)
            {
            case VC1_ENC_TOP_FIELD:
                err = pCodedPicture->PutBits(0,2);
                if (err != UMC::UMC_OK)return err;
                err = pCodedPicture->PutBits(m_uiIntensityLumaScale,6);
                if (err != UMC::UMC_OK)return err;
                err = pCodedPicture->PutBits(m_uiIntensityLumaShift,6);
                break;
            case VC1_ENC_BOTTOM_FIELD:
                err = pCodedPicture->PutBits(1,2);
                if (err != UMC::UMC_OK)return err;
                err = pCodedPicture->PutBits(m_uiIntensityLumaScaleB,6);
                if (err != UMC::UMC_OK)return err;
                err = pCodedPicture->PutBits(m_uiIntensityLumaShiftB,6);
                if (err != UMC::UMC_OK)return err;
                break;
            case VC1_ENC_BOTH_FIELD:
                err = pCodedPicture->PutBits(1,1);
                if (err != UMC::UMC_OK)return err;
                err = pCodedPicture->PutBits(m_uiIntensityLumaScale,6);
                if (err != UMC::UMC_OK)return err;
                err = pCodedPicture->PutBits(m_uiIntensityLumaShift,6);
                if (err != UMC::UMC_OK)return err;
                err = pCodedPicture->PutBits(m_uiIntensityLumaScaleB,6);
                if (err != UMC::UMC_OK)return err;
                err = pCodedPicture->PutBits(m_uiIntensityLumaShiftB,6);
                if (err != UMC::UMC_OK)return err;
                break;
            default:
                assert(0);
                return UMC::UMC_ERR_FAILED;
            }
        }
        err = pCodedPicture->PutBits(m_uiMBModeTab,3);
        if (err != UMC::UMC_OK)return err;

        err = pCodedPicture->PutBits(m_uiMVTab,2+(m_uiReferenceFieldType == VC1_ENC_REF_FIELD_BOTH));
        if (err != UMC::UMC_OK)return err;

        err = pCodedPicture->PutBits(m_uiCBPTab,3);
        if (err != UMC::UMC_OK)return err;

        if (m_uiMVMode == VC1_ENC_MIXED_QUARTER_BICUBIC)
        {
            err = pCodedPicture->PutBits(m_ui4MVCBPTab,2);
            if (err != UMC::UMC_OK)return err;
        }
        switch (m_pSequenceHeader->GetDQuant())
        {
        case 2:
            if (diff>=0 && diff<7)
            {
                err = pCodedPicture->PutBits(diff,3);
            }
            else
            {
                err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
            }
            if (err != UMC::UMC_OK)
                return err;
            break;
        case 1:
            err = pCodedPicture->PutBits( m_QuantMode != VC1_ENC_QUANT_SINGLE, 1);
            if (err != UMC::UMC_OK)
                return err;
            if (m_QuantMode != VC1_ENC_QUANT_SINGLE)
            {
                err =  pCodedPicture->PutBits(QuantProfileTableVLC[2*m_QuantMode], QuantProfileTableVLC[2*m_QuantMode+1]);
                if (err != UMC::UMC_OK)
                    return err;
                if (m_QuantMode != VC1_ENC_QUANT_MB_ANY)
                {
                    if (diff>=0 && diff<7)
                    {
                        err = pCodedPicture->PutBits(diff,3);
                    }
                    else
                    {
                        err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
                    }
                    if (err != UMC::UMC_OK)
                        return err;
                }
            }
            break;
        default:
            break;
        }

        if (m_pSequenceHeader->IsVSTransform())
        {
            err = pCodedPicture->PutBits(m_bVSTransform,1);
            if (err != UMC::UMC_OK)
                return err;
            if (m_bVSTransform)
            {
                err = pCodedPicture->PutBits(m_uiTransformType,2);
                if (err != UMC::UMC_OK)
                    return err;
            }
        }
        err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC1],ACTableCodesVLC[2*m_uiDecTypeAC1+1]);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiDecTypeDCIntra,1);
        return err;
    }
    UMC::Status VC1EncoderPictureADV::WriteBFieldHeader(VC1EncoderBitStreamAdv* pCodedPicture)
    {
        UMC::Status     err         =   UMC::UMC_OK;
        //bool            bIntensity  =   (m_uiFieldIntensityType!=VC1_ENC_FIELD_NONE);
        Ipp8s           diff          =  m_uiAltPQuant -  m_uiQuant - 1;


        err = pCodedPicture->PutBits(m_uiQuantIndex, 5);
        if (err != UMC::UMC_OK)
            return err;

        if (m_uiQuantIndex <= 8)
        {
            err = pCodedPicture->PutBits(m_bHalfQuant, 1);
            if (err != UMC::UMC_OK) return err;
        }
        if (m_pSequenceHeader->GetQuantType()== VC1_ENC_QTYPE_EXPL)
        {
            err = pCodedPicture->PutBits(m_bUniformQuant,1);
            if (err != UMC::UMC_OK) return err;
        }
        if (m_pSequenceHeader->IsPostProc())
        {
            assert(0);
        }

        if (m_pSequenceHeader->IsExtendedMV())
        {
            err = pCodedPicture->PutBits(MVRangeCodesVLC[m_uiMVRangeIndex*2],MVRangeCodesVLC[m_uiMVRangeIndex*2+1]);
            if (err != UMC::UMC_OK)
                return err;
        }

        if (m_pSequenceHeader->IsExtendedDMV())
        {
            err = pCodedPicture->PutBits(MVRangeCodesVLC[m_uiDMVRangeIndex*2],MVRangeCodesVLC[m_uiDMVRangeIndex*2+1]);
            if (err != UMC::UMC_OK)
                return err;
        }

        err = pCodedPicture->PutBits(MVModeBField[m_uiQuant <= 12][2*m_uiMVMode],MVModeBField[m_uiQuant <= 12][2*m_uiMVMode+1]);
        if (err != UMC::UMC_OK)
            return err;

        // raw bitplane for forward MB
        err = pCodedPicture->PutBits(0,5);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiMBModeTab,3);
        if (err != UMC::UMC_OK)return err;

        err = pCodedPicture->PutBits(m_uiMVTab,3);
        if (err != UMC::UMC_OK)return err;

        err = pCodedPicture->PutBits(m_uiCBPTab,3);
        if (err != UMC::UMC_OK)return err;

        if (m_uiMVMode == VC1_ENC_MIXED_QUARTER_BICUBIC)
        {
            err = pCodedPicture->PutBits(m_ui4MVCBPTab,2);
            if (err != UMC::UMC_OK)return err;
        }
        switch (m_pSequenceHeader->GetDQuant())
        {
        case 2:
            if (diff>=0 && diff<7)
            {
                err = pCodedPicture->PutBits(diff,3);
            }
            else
            {
                err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
            }
            if (err != UMC::UMC_OK)
                return err;
            break;
        case 1:
            err = pCodedPicture->PutBits( m_QuantMode != VC1_ENC_QUANT_SINGLE, 1);
            if (err != UMC::UMC_OK)
                return err;
            if (m_QuantMode != VC1_ENC_QUANT_SINGLE)
            {
                err =  pCodedPicture->PutBits(QuantProfileTableVLC[2*m_QuantMode], QuantProfileTableVLC[2*m_QuantMode+1]);
                if (err != UMC::UMC_OK)
                    return err;
                if (m_QuantMode != VC1_ENC_QUANT_MB_ANY)
                {
                    if (diff>=0 && diff<7)
                    {
                        err = pCodedPicture->PutBits(diff,3);
                    }
                    else
                    {
                        err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
                    }
                    if (err != UMC::UMC_OK)
                        return err;
                }
            }
            break;
        default:
            break;
        }

        if (m_pSequenceHeader->IsVSTransform())
        {
            err = pCodedPicture->PutBits(m_bVSTransform,1);
            if (err != UMC::UMC_OK)
                return err;
            if (m_bVSTransform)
            {
                err = pCodedPicture->PutBits(m_uiTransformType,2);
                if (err != UMC::UMC_OK)
                    return err;
            }
        }
        err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC1],ACTableCodesVLC[2*m_uiDecTypeAC1+1]);
        if (err != UMC::UMC_OK)
            return err;

        err = pCodedPicture->PutBits(m_uiDecTypeDCIntra,1);
        return err;
    }
    UMC::Status VC1EncoderPictureADV::WriteMBQuantParameter(VC1EncoderBitStreamAdv* pCodedPicture, Ipp8u Quant)
    {
        UMC::Status err = UMC::UMC_OK;

        if (m_QuantMode == VC1_ENC_QUANT_MB_ANY)
        {
            Ipp16s diff = Quant - m_uiQuant;
            if (diff < 7 && diff>=0)
            {
                err = pCodedPicture->PutBits(diff,3);
            }
            else
            {
                err = pCodedPicture->PutBits((7<<5)+ Quant,3+5);
            }

        }
        else if (m_QuantMode ==VC1_ENC_QUANT_MB_PAIR)
        {
            err = pCodedPicture->PutBits(m_uiAltPQuant == Quant,1);

        }
        return err;
    }

    UMC::Status VC1EncoderPictureADV::CheckMEMV_P(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status UmcSts = UMC::UMC_OK;
        Ipp32u i = 0;
        Ipp32u j = 0;
        Ipp32u h = (nRows>0)? (nRows + firstRow) :(m_pSequenceHeader->GetPictureHeight()+15)/16;
        Ipp32u w = (m_pSequenceHeader->GetPictureWidth() +15)/16;

        sCoordinate         MV          = {0,0};

        for(i = firstRow; i < h; i++)
        {
            for(j = 0; j < w; j++)
            {
                if(MEParams->pSrc->MBs[j + i*w].MbType != UMC::ME_MbIntra)
                {
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    ////correction MV
                    //MV.x  = (bMVHalf)? (MEParams.pSrc->MBs[j + i*w].MVF->x>>1)<<1:MEParams.pSrc->MBs[j + i*w].MVF->x;
                    //MV.y  = (bMVHalf)? (MEParams.pSrc->MBs[j + i*w].MVF->y>>1)<<1:MEParams.pSrc->MBs[j + i*w].MVF->y;

                    //if ((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) > (Ipp32s)(w*VC1_ENC_LUMA_SIZE))
                    //{
                    //    MV.x = (w - j)*VC1_ENC_LUMA_SIZE + (MV.x & 0x03);
                    //    //UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    //}
                    //if ((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) > (Ipp32s)(h*VC1_ENC_LUMA_SIZE))
                    //{
                    //    MV.y = (h - i)*VC1_ENC_LUMA_SIZE + (MV.y & 0x03);
                    //    //UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    //}
                    //
                    //if ((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < -16)
                    //{
                    //    MV.x = (1 - (Ipp32s)j)*VC1_ENC_LUMA_SIZE ;
                    //    //UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    //}

                    //if ((MV.y>>2) + (Ipp32s)(i*VC1_ENC_LUMA_SIZE) < -16)
                    //{
                    //    MV.y = (1 - (Ipp32s)i)*VC1_ENC_LUMA_SIZE ;
                    //    //UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    //}
                }
            }
        }

        return UmcSts;
    }

    UMC::Status VC1EncoderPictureADV::CheckMEMV_PField(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status UmcSts = UMC::UMC_OK;
        Ipp32u i = 0;
        Ipp32u j = 0;
        Ipp32u h = (nRows>0)? (nRows + firstRow) :(m_pSequenceHeader->GetPictureHeight()/2+15)/16;
        Ipp32u w = (m_pSequenceHeader->GetPictureWidth() +15)/16;

        sCoordinate         MV          = {0,0};

        for(i = firstRow; i < h; i++)
        {
            for(j = 0; j < w; j++)
            {
                if(MEParams->pSrc->MBs[j + i*w].MbType != UMC::ME_MbIntra)
                {
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }


                }
            }
        }

        return UmcSts;
    }

    UMC::Status VC1EncoderPictureADV::CheckMEMV_P_MIXED(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow, Ipp32s nRows, bool bField)
    {
        UMC::Status UmcSts = UMC::UMC_OK;
        Ipp32u i = 0;
        Ipp32u j = 0;
        Ipp32u hPic = (bField)? m_pSequenceHeader->GetPictureHeight()/ 2: m_pSequenceHeader->GetPictureHeight();
        Ipp32u h = (nRows>0)? (nRows + firstRow) :(hPic+15)/16;
        Ipp32u w = (m_pSequenceHeader->GetPictureWidth() +15)/16;
        Ipp32u blk = 0;

        sCoordinate         MV          = {0,0};

        typedef enum _mbPartType
        {
            Intra,
            MV_1,
            MV_4,
            Error
        }mbPartType;
        for(i = firstRow; i < h; i++)
        {
            for(j = 0; j < w; j++)
            {
                mbPartType type;
                if(MEParams->pSrc->MBs[j + i*w].MbType == UMC::ME_MbIntra)
                {
                    type = Intra;
                }
                else if(MEParams->pSrc->MBs[j + i*w].MbType == UMC::ME_MbFrw ||
                    MEParams->pSrc->MBs[j + i*w].MbType == UMC::ME_MbFrwSkipped)
                {
                    if(MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb16x16)
                    {
                        type = MV_1;
                    }
                    else if(MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)
                    {
                        type = MV_4;
                    }
                    else
                    {
                        type = Error;
                    }
                }
                else if(MEParams->pSrc->MBs[j + i*w].MbType == UMC::ME_MbMixed)
                {
                    type = MV_4;
                }
                else
                {
                    type = Error;
                }
                switch (type)
                {
                case Intra:
                    break;
                case MV_1:
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.y >>1)<<1!= MV.y)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }
                    }
                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_CHROMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_CHROMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    break;
                case MV_4:
                    for (blk = 0; blk<4; blk++)
                    {
                        MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0][blk].x;
                        MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0][blk].y;

                        int xShift = (blk & 0x01)<<3;
                        int yShift = (blk>>1)<<3;


                        if((MV.x>>2)+(Ipp32s)(j<<4) + xShift < MEParams->PicRange.top_left.x)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.y>>2)+(Ipp32s)(i<<4) + yShift < MEParams->PicRange.top_left.y )
                        {
                            assert(0); 
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.x>>2)+(Ipp32s)(j<<4) + xShift + 8 > MEParams->PicRange.bottom_right.x)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.y>>2)+(Ipp32s)(i<<4) + yShift + 8 > MEParams->PicRange.bottom_right.y)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }
                    }
                    break;
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
                }

            }
        }
        return UmcSts;
    }
    UMC::Status VC1EncoderPictureADV::CheckMEMV_B(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status UmcSts = UMC::UMC_OK;
        Ipp32u i = 0;
        Ipp32u j = 0;
        Ipp32u h = (nRows>0)? (nRows + firstRow) :(m_pSequenceHeader->GetPictureHeight()+15)/16;
        Ipp32u w = (m_pSequenceHeader->GetPictureWidth() +15)/16;

        sCoordinate         MV          = {0,0};

        //sCoordinate                 MVMin = {-16*4,-16*4};
        //sCoordinate                 MVMax = {w*16*4,h*16*4};


        for(i = firstRow; i < h; i++)
        {
            for(j = 0; j < w; j++)
            {
                switch (MEParams->pSrc->MBs[j + i*w].MbType)
                {
                case UMC::ME_MbIntra:
                    break;
                case UMC::ME_MbFrw:
                case UMC::ME_MbFrwSkipped:
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                    break;
                case UMC::ME_MbBkw:
                case UMC::ME_MbBkwSkipped:
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[1]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[1]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);

                    break;
                case UMC::ME_MbBidir:
                case UMC::ME_MbBidirSkipped:
                case UMC::ME_MbDirect:
                case UMC::ME_MbDirectSkipped:

                    //forward
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);

                    //backward
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[1]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[1]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    break;
                default:
                    assert(0);
                    break;

                }
            }
        }

        return UmcSts;
    }

    UMC::Status VC1EncoderPictureADV::CheckMEMV_BField(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status UmcSts = UMC::UMC_OK;
        Ipp32u i = 0;
        Ipp32u j = 0;
        Ipp32u h = (nRows>0)? (nRows + firstRow) :(m_pSequenceHeader->GetPictureHeight()/2+15)/16;
        Ipp32u       w = (m_pSequenceHeader->GetPictureWidth() +15)/16;

        sCoordinate         MV          = {0,0};

        //sCoordinate                 MVMin = {-16*4,-16*4};
        //sCoordinate                 MVMax = {w*16*4,h*16*4};


        for(i = firstRow; i < h; i++)
        {
            for(j = 0; j < w; j++)
            {
                switch (MEParams->pSrc->MBs[j + i*w].MbType)
                {
                case UMC::ME_MbIntra:
                    break;
                case UMC::ME_MbFrw:
                case UMC::ME_MbFrwSkipped:

                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    break;
                case UMC::ME_MbBkw:
                case UMC::ME_MbBkwSkipped:

                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[1]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[1]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    break;
                case UMC::ME_MbBidir:
                case UMC::ME_MbBidirSkipped:
                case UMC::ME_MbDirect:
                case UMC::ME_MbDirectSkipped:

                    //forward
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);

                    //backward

                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[1]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[1]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    break;
                default:
                    assert(0);
                    break;

                }
            }
        }

        return UmcSts;
    }
    UMC::Status VC1EncoderPictureADV::CheckMEMV_BFieldMixed(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow, Ipp32s nRows)
    {
        UMC::Status UmcSts = UMC::UMC_OK;
        Ipp32u i = 0;
        Ipp32u j = 0;
        Ipp32u h = (nRows>0)? (nRows + firstRow) :(m_pSequenceHeader->GetPictureHeight()/2+15)/16;
        Ipp32u w = (m_pSequenceHeader->GetPictureWidth() +15)/16;

        for(i = firstRow; i < h; i++)
        {
            for(j = 0; j < w; j++)
            {
                switch (MEParams->pSrc->MBs[j + i*w].MbType)
                {
                case UMC::ME_MbIntra:
                    break;
                case UMC::ME_MbFrw:
                case UMC::ME_MbFrwSkipped:
                case UMC::ME_MbBkw:
                case UMC::ME_MbBkwSkipped:
                    {
                        UMC::MeMV *pMV = 0;
                        if (MEParams->pSrc->MBs[j + i*w].MbType == UMC::ME_MbFrw || 
                            MEParams->pSrc->MBs[j + i*w].MbType == UMC::ME_MbFrwSkipped)
                        {
                            pMV  = MEParams->pSrc->MBs[j + i*w].MV[0];
                        }
                        else
                        {
                            pMV  = MEParams->pSrc->MBs[j + i*w].MV[1];
                        }

                        if (MEParams->pSrc->MBs[j + i*w].MbPart != UMC::ME_Mb8x8)
                        {
                            if(bMVHalf)
                            {
                                if((pMV->x>>1)<<1 != pMV->x)
                                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                                if((pMV->y >>1)<<1!= pMV->y)
                                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                            }

                            if((pMV->x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                            {
                                assert(0);
                                UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                            }

                            if((pMV->y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                            {
                                assert(0);
                                UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                            }

                            if((pMV->x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                            {
                                assert(0);
                                UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                            }

                            if((pMV->y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                            {
                                assert(0);
                                UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                            }
                            //correction MV
                            //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                            if (pMV->x >(MEParams->SearchRange.x<<2) || pMV->x <(-(MEParams->SearchRange.x<<2)))
                            {
                                assert(0);
                                UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                            }
                            if (pMV->y >(MEParams->SearchRange.y<<2) || pMV->y <(-(MEParams->SearchRange.y<<2)))
                            {
                                assert(0);
                                UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                            }
                        }
                        else
                        {
                            for(int blk = 0; blk <4; blk++)
                            {
                                int xShift = (blk & 0x01)<<3;
                                int yShift = (blk>>1)<<3;

                                if((pMV[blk].x>>2)+(Ipp32s)(j<<4) + xShift < MEParams->PicRange.top_left.x)
                                {
                                    assert(0);
                                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                                }

                                if((pMV[blk].y>>2)+(Ipp32s)(i<<4) + yShift < MEParams->PicRange.top_left.y )
                                {
                                    assert(0);
                                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                                }

                                if((pMV[blk].x>>2)+(Ipp32s)(j<<4) + xShift+ 8 > MEParams->PicRange.bottom_right.x)
                                {
                                    assert(0);
                                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                                }

                                if((pMV[blk].y>>2)+(Ipp32s)(i<<4)  + yShift + 8  > MEParams->PicRange.bottom_right.y)
                                {
                                    assert(0);
                                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                                }
                                //correction MV
                                //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                                if (pMV[blk].x >(MEParams->SearchRange.x<<2) || pMV[blk].x <(-(MEParams->SearchRange.x<<2)))
                                {
                                    assert(0);
                                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                                }
                                if (pMV[blk].y >(MEParams->SearchRange.y<<2) || pMV[blk].y <(-(MEParams->SearchRange.y<<2)))
                                {
                                    assert(0);
                                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                                }                        }

                        }
                    }
                    break;

                case UMC::ME_MbBidir:
                case UMC::ME_MbBidirSkipped:
                case UMC::ME_MbDirect:
                case UMC::ME_MbDirectSkipped:
                    {

                        sCoordinate         MV          = {0,0};

                        //forward
                        MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                        MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                        if(bMVHalf)
                        {
                            if((MV.x>>1)<<1 != MV.x)
                                UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                            if((MV.y >>1)<<1!= MV.y)
                                UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        //correction MV
                        //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);

                        //backward

                        if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }
                        if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        MV.x  = MEParams->pSrc->MBs[j + i*w].MV[1]->x;
                        MV.y  = MEParams->pSrc->MBs[j + i*w].MV[1]->y;

                        if(bMVHalf)
                        {
                            if((MV.x>>1)<<1 != MV.x)
                                UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                            if((MV.y >>1)<<1!= MV.y)
                                UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.x>>2)+(Ipp32s)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        if((MV.y>>2)+(Ipp32s)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        //correction MV
                        //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                        if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }
                        if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                        {
                            assert(0);
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                        }
                    }
                    break;
                default:
                    assert(0);
                    break;

                }
            }
        }

        return UmcSts;
    }
#ifdef VC1_ME_MB_STATICTICS
    void VC1EncoderPictureADV::SetMEStat(UMC::MeMbStat*   stat, bool bSecondField)
    {
        if(!bSecondField)
            m_MECurMbStat = stat;
        else
        {
            Ipp32u h = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
            Ipp32u w = (m_pSequenceHeader->GetPictureWidth() +15)/16;

            m_MECurMbStat = stat + w*h/2;
        }
    }
#endif
}

#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
