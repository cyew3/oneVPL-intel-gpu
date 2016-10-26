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

#ifndef _ENCODER_VC1_PICTURE_ADV_H_
#define _ENCODER_VC1_PICTURE_ADV_H_

#include "umc_vc1_enc_bitstream.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_enc_sequence_adv.h"
#include "umc_vc1_enc_mb.h"
#include "umc_vc1_enc_mb_template.h"
#include "umc_me.h"
#include "umc_vme.h"
#include "umc_vc1_enc_planes.h"
#include "umc_vc1_enc_deblocking.h"
#include "umc_vc1_enc_smoothing_adv.h"


namespace UMC_VC1_ENCODER
{

class VC1EncoderPictureADV
{
private:

/*All picture types*/
    VC1EncoderSequenceADV*      m_pSequenceHeader;
    ePType                      m_uiPictureType;
    bool                        m_bFrameInterpolation;
    Ipp8u                       m_uiQuantIndex;                 // [1, 31]
    bool                        m_bHalfQuant;                   // can be true if  m_uiQuantIndex<8
    bool                        m_bUniformQuant;
    //Ipp8u                     m_uiResolution;                 //[0,3] - full, half (horizontal, vertical scale)
    Ipp8u                       m_uiDecTypeAC1;                 //[0,2] - it's used to choose decoding table
    Ipp8u                       m_uiDecTypeDCIntra;             //[0,1] - it's used to choose decoding table

    /* ------------------------------I picture ---------------- */

    Ipp8u                       m_uiDecTypeAC2;                 //[0,2] - it's used to choose decoding table

    /* ------------------------------P picture----------------- */

    eMVModes                    m_uiMVMode;                     //    VC1_ENC_1MV_HALF_BILINEAR
                                                                //    VC1_ENC_1MV_QUARTER_BICUBIC
                                                                //    VC1_ENC_1MV_HALF_BICUBIC
                                                                //    VC1_ENC_MIXED_QUARTER_BICUBIC

    bool                        m_bIntensity;                   // only for progressive
    Ipp8u                       m_uiIntensityLumaScale;         // prog: [0,63], if m_bIntensity
                                                                // field: for TOP field[0,63] , if  m_uiFieldIntensityType != VC1_ENC_NONE
    Ipp8u                       m_uiIntensityLumaShift;         // prog: [0,63], if m_bIntensity
                                                                // field: for TOP field[0,63] , if  m_uiFieldIntensityType != VC1_ENC_NONE
    Ipp8u                       m_uiMVTab;                      // prog: [0,4] - table for MV coding
                                                                // field:[0,3] - if m_uiReferenceFieldType != VC1_ENC_BOTH_FIELD
                                                                // field:[0,7] - if m_uiReferenceFieldType == VC1_ENC_BOTH_FIELD
    Ipp8u                       m_uiCBPTab;                     // prog:  [0,4] - table for CBP coding
                                                                // field: [0,7]
    Ipp8u                       m_uiAltPQuant;                  // if picture DQuant!=0
    eQuantMode                  m_QuantMode;                    // VC1_ENC_QUANT_SINGLE, if   picture DQuant==0
    bool                        m_bVSTransform;                 // if sequence(m_bVSTransform) variable-size transform
    eTransformType              m_uiTransformType;              // VC1_ENC_8x8_TRANSFORM, if  m_bVSTransform == false

    /*----------------- All frames- adv--------------------------- */
    /*--------------------I frame - adv--------------------------- */

    Ipp8u                       m_uiRoundControl;                // 0,1;
    Ipp8u                       m_uiCondOverlap;                 //VC1_ENC_COND_OVERLAP_NO, VC1_ENC_COND_OVERLAP_SOME, VC1_ENC_COND_OVERLAP_ALL

    /*--------------------P frame - adv--------------------------- */
    Ipp8u                       m_uiMVRangeIndex;                //progr:  [0,3] - 64x32, 128x64, 512x128,  1024x256
                                                                 //fields half: [0,3] - 128x64,256x128,1024x256, 248x512
    /*--------------------B frame - adv--------------------------- */
    sFraction                   m_uiBFraction;                  // 0xff/oxff - BI frame

    /*--------------------Interlace--------------------------------*/
    bool                        m_bUVSamplingInterlace;           // if false - progressive
    Ipp8u                       m_nReferenceFrameDist;            // is used only in I/I, I/P, P/I, P/P
    eReferenceFieldType         m_uiReferenceFieldType;           // VC1_ENC_REF_FIELD_FIRST
                                                                  // VC1_ENC_REF_FIELD_SECOND
                                                                  // VC1_ENC_REF_FIELD_BOTH


    eFieldType                  m_uiFieldIntensityType;          // intensity compensation for field picture
                                                                 // VC1_ENC_FIELD_NONE
                                                                 // VC1_ENC_TOP_FIELD
                                                                 // VC1_ENC_BOTTOM_FIELD
                                                                 // VC1_ENC_BOTH_FIELD

    Ipp8u                       m_uiIntensityLumaScaleB;         // [0,63], for bottom field
    Ipp8u                       m_uiIntensityLumaShiftB;         // [0,63], for bottom field

    Ipp8u                       m_uiDMVRangeIndex;               // [0,3] - 64x32, 128x64, 512x128, 1024x256
    Ipp8u                       m_uiMBModeTab;                   // [0,7]
    Ipp8u                       m_ui4MVCBPTab;                   // [0,3]

    bool                        m_bTopFieldFirst;
    bool                        m_bRepeateFirstField;

private:
    Ipp8u*                      m_pPlane[3];
    Ipp32u                      m_uiPlaneStep[3];


    Ipp8u*                      m_pRaisedPlane[3];
    Ipp32u                      m_uiRaisedPlaneStep[3];
    Ipp32u                      m_uiRaisedPlanePadding;

    Ipp8u*                      m_pForwardPlane[3];
    Ipp32u                      m_uiForwardPlaneStep[3];
    Ipp32u                      m_uiForwardPlanePadding;
    bool                        m_bForwardInterlace;

    Ipp8u*                      m_pBackwardPlane[3];
    Ipp32u                      m_uiBackwardPlaneStep[3];
    Ipp32u                      m_uiBackwardPlanePadding;
    bool                        m_bBackwardInterlace;



    // those parameters are used for direct MB reconstruction.
    // array of nMB

    Ipp16s*                     m_pSavedMV;
    Ipp8u*                      m_pRefType; //0 - intra, 1 - First field, 2 - Second field

    VC1EncoderMBs*              m_pMBs;
    VC1EncoderCodedMB*          m_pCodedMB;
    Ipp8u                       m_uiQuant;
    bool                        m_bRawBitplanes;

    /*  functions for YV12 and NV12 color formats */

    pInterpolateChroma         InterpolateChroma;
    pInterpolateChromaPad      InterpolateChromaPad;

    //deblocking functions
    fDeblock_I_MB*              m_pDeblk_I_MB;
    fDeblock_P_MB**             m_pDeblk_P_MB;

    //smoothing
    fSmooth_I_MB_Adv*           m_pSM_I_MB;
    fSmooth_P_MB_Adv*           m_pSM_P_MB;


#ifdef VC1_ME_MB_STATICTICS
    UMC::MeMbStat* m_MECurMbStat;
#endif

protected:
    void        Reset()
    {
        m_pSequenceHeader       = 0;
        m_uiPictureType         = VC1_ENC_I_FRAME;
        m_bFrameInterpolation   = 0;
        //m_bRangeRedution        = 0;
        //m_uiFrameCount          = 0;
        m_uiQuantIndex          = 0;
        m_bHalfQuant            = 0;
        m_bUniformQuant         = 0;
        m_uiMVRangeIndex        = 0;
        m_uiDMVRangeIndex       = 0;
        //m_uiResolution          = 0;
        m_uiDecTypeAC1          = 0;
        m_uiDecTypeAC2          = 0;
        m_uiDecTypeDCIntra      = 0;
        m_uiQuant               = 0;
        //m_pMBs                  = 0;
        m_uiBFraction.num       = 1;
        m_uiBFraction.denom     = 2;
        m_uiMVMode              = VC1_ENC_1MV_HALF_BILINEAR;
        m_bIntensity            = false;
        m_uiIntensityLumaScale  = 0;
        m_uiIntensityLumaShift  = 0;
        m_uiMVTab               = 0;
        m_uiCBPTab              = 0;
        m_uiAltPQuant           = 0;
        m_QuantMode             = VC1_ENC_QUANT_SINGLE;
        m_bVSTransform          = false;
        m_uiTransformType       = VC1_ENC_8x8_TRANSFORM;
        m_bRawBitplanes         = true;
        m_pSavedMV              = 0;
        m_uiRoundControl        = 0;
        m_uiCondOverlap         = VC1_ENC_COND_OVERLAP_NO;
        m_pCodedMB              = 0;
        m_bUVSamplingInterlace  = 0;
        m_nReferenceFrameDist   = 0;
        m_uiReferenceFieldType  = VC1_ENC_REF_FIELD_FIRST;
        m_uiFieldIntensityType  = VC1_ENC_FIELD_NONE;     // intensity compensation for second reference
                                         // used only if eReferenceFieldType = VC1_ENC_REF_FIELD_BOTH
        m_uiIntensityLumaScaleB = 0;     // [0,63], if m_bIntensity
        m_uiIntensityLumaShiftB = 0;

        m_uiMBModeTab           = 0;
        m_ui4MVCBPTab           = 0;
        m_bTopFieldFirst        = 1;
        m_pRefType              = 0;
        m_bRepeateFirstField    = false;
        m_uiForwardPlanePadding =0;
        m_bForwardInterlace     = false;
        m_uiBackwardPlanePadding =0;
        m_bBackwardInterlace     = false;
        m_uiRaisedPlanePadding   = 0;

#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat           = NULL;
#endif
    }
    void        ResetPlanes()
    {
       m_pPlane[0]                 = 0;
       m_pPlane[1]                 = 0;
       m_pPlane[2]                 = 0;
       m_uiPlaneStep[0]            = 0;
       m_uiPlaneStep[1]            = 0;
       m_uiPlaneStep[2]            = 0;

       m_pRaisedPlane[0]           = 0;
       m_pRaisedPlane[1]           = 0;
       m_pRaisedPlane[2]           = 0;
       m_uiRaisedPlaneStep[0]      = 0;
       m_uiRaisedPlaneStep[1]      = 0;
       m_uiRaisedPlaneStep[2]      = 0;

       m_pForwardPlane[0]          = 0;
       m_pForwardPlane[1]          = 0;
       m_pForwardPlane[2]          = 0;
       m_uiForwardPlaneStep[0]     = 0;
       m_uiForwardPlaneStep[1]     = 0;
       m_uiForwardPlaneStep[2]     = 0;

       m_pBackwardPlane[0]          = 0;
       m_pBackwardPlane[1]          = 0;
       m_pBackwardPlane[2]          = 0;
       m_uiBackwardPlaneStep[0]     = 0;
       m_uiBackwardPlaneStep[1]     = 0;
       m_uiBackwardPlaneStep[2]     = 0;
    }

    UMC::Status     CheckParametersI(vm_char* pLastError);
    UMC::Status     CheckParametersIField(vm_char* pLastError);
    UMC::Status     CheckParametersP(vm_char* pLastError);
    UMC::Status     CheckParametersPField(vm_char* pLastError);
    UMC::Status     CheckParametersB(vm_char* pLastError);
    UMC::Status     CheckParametersBField(vm_char* pLastError);

    UMC::Status     SetPictureParamsI(Ipp32u OverlapSmoothing);
    UMC::Status     SetPictureParamsP();
    UMC::Status     SetPictureParamsB();
    UMC::Status     SetPictureParamsIField(Ipp32u OverlapSmoothing);
    UMC::Status     SetPictureParamsPField();
    UMC::Status     SetPictureParamsBField();

    inline
    UMC::Status     SetReferenceFrameDist(Ipp8u distance)
    {
        m_nReferenceFrameDist = distance;
        return UMC::UMC_OK;
    }
    inline
    UMC::Status     SetUVSamplingInterlace(bool bUVSamplingInterlace)
    {
        m_bUVSamplingInterlace = bUVSamplingInterlace;
        return UMC::UMC_OK;
    }

    UMC::Status     WriteIPictureHeader(VC1EncoderBitStreamAdv* pCodedPicture);
    UMC::Status     WritePPictureHeader(VC1EncoderBitStreamAdv* pCodedPicture);
    UMC::Status     WriteBPictureHeader(VC1EncoderBitStreamAdv* pCodedPicture);
    UMC::Status     WriteSkipPictureHeader(VC1EncoderBitStreamAdv* pCodedPicture);

    UMC::Status     WriteFieldPictureHeader(VC1EncoderBitStreamAdv* pCodedPicture);
    UMC::Status     WriteIFieldHeader(VC1EncoderBitStreamAdv* pCodedPicture);
    UMC::Status     WritePFieldHeader(VC1EncoderBitStreamAdv* pCodedPicture);
    UMC::Status     WriteBFieldHeader(VC1EncoderBitStreamAdv* pCodedPicture);

    UMC::Status     WriteMBQuantParameter(VC1EncoderBitStreamAdv* pCodedPicture, Ipp8u Quant);

    UMC::Status     SetInterpolationParams(IppVCInterpolate_8u* pY,
                                           IppVCInterpolate_8u* pU,
                                           IppVCInterpolate_8u* pv,
                                           Ipp8u* buffer,
                                           bool bForward,
                                           bool bField = false);


    UMC::Status     CheckMEMV_P(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     CheckMEMV_P_MIXED(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow=0, Ipp32s nRows=0, bool bField=false);
    UMC::Status     CheckMEMV_B(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     CheckMEMV_PField(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     CheckMEMV_BField(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     CheckMEMV_BFieldMixed(UMC::MeParams* MEParams, bool bMVHalf,Ipp32s firstRow=0, Ipp32s nRows=0);

    UMC::Status     SetInterpolationParams4MV(  IppVCInterpolate_8u* pY,
                                                IppVCInterpolate_8u* pU,
                                                IppVCInterpolate_8u* pv,
                                                Ipp8u* buffer,
                                                bool bForward,
                                                bool bField = false);

    UMC::Status     SetInterpolationParams4MV (IppVCInterpolateBlock_8u* pY,
                                               Ipp8u* buffer,
                                               Ipp32u w,
                                               Ipp32u h,
                                               Ipp8u **pPlane,
                                               Ipp32u *pStep,
                                               bool bField);
public:

    VC1EncoderPictureADV(bool  bNV12)
    {
        Reset();
        ResetPlanes();
        InterpolateChroma       = (bNV12)? InterpolateChroma_NV12    : InterpolateChroma_YV12;
        InterpolateChromaPad    = (bNV12)? InterpolateChromaPad_NV12 : InterpolateChromaPad_YV12;
        m_pDeblk_I_MB           = (bNV12) ? Deblk_I_MBFunction_NV12 : Deblk_I_MBFunction_YV12;
        m_pDeblk_P_MB           = (bNV12) ? Deblk_P_MBFunction_NV12 : Deblk_P_MBFunction_YV12;
        m_pSM_I_MB              = (bNV12) ? Smooth_I_MBFunction_Adv_NV12 : Smooth_I_MBFunction_Adv_YV12;
        m_pSM_P_MB              = (bNV12) ? Smooth_P_MBFunction_Adv_NV12 : Smooth_P_MBFunction_Adv_YV12;
    }

    UMC::Status     SetInitPictureParams(InitPictureParams* InitPicParam);
    //UMC::Status     SetVLCTablesIndex(VLCTablesIndex*       VLCIndex);

    UMC::Status     SetPictureParams( ePType pType, Ipp16s* pSavedMV, Ipp8u* pRefType, bool bSecondField, Ipp32u OverlapSmoothing);
    UMC::Status     SetPlaneParams  ( Frame* pFrame, ePlaneType type );

    UMC::Status     Init(VC1EncoderSequenceADV* SequenceHeader, VC1EncoderMBs* pMBs, VC1EncoderCodedMB* pCodedMB);

    UMC::Status     Close();


    UMC::Status     CompletePicture(VC1EncoderBitStreamAdv* pCodedPicture, double dPTS, size_t len);
    UMC::Status     CheckParameters(vm_char* pLastError, bool bSecondField);

    UMC::Status     PAC_IFrame(UMC::MeParams*, Ipp32s firstRow, Ipp32s nRows);
    UMC::Status     PAC_PFrame(UMC::MeParams* MEParams, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     PAC_PFrameMixed(UMC::MeParams* MEParams, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     PAC_BFrame(UMC::MeParams* MEParams, Ipp32s firstRow=0, Ipp32s nRows=0);

    UMC::Status     PACIField(UMC::MeParams* MEParams, bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     PACPField(UMC::MeParams* MEParams, bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0)
    {
        UMC::Status err = UMC::UMC_OK;
        if(m_uiReferenceFieldType != VC1_ENC_REF_FIELD_BOTH)
            err = PAC_PField1Ref(MEParams,bSecondField,firstRow,nRows);
        else
            err = PAC_PField2Ref(MEParams,bSecondField,firstRow,nRows);

        return err;
    }

    UMC::Status     PACBField(UMC::MeParams* MEParams,bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     PACBFieldMixed(UMC::MeParams* MEParams,bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0);
   
    UMC::Status     VLC_PField2RefMixed(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow, Ipp32s nRows);
    UMC::Status     VLC_PField1RefMixed(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow, Ipp32s nRows);
    
    
    UMC::Status     VLC_IFrame(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow=0, Ipp32s nRows=0)
    {
        UMC::Status err = UMC::UMC_OK;
        if (firstRow == 0)
        {
            err = WriteIPictureHeader(pCodedPicture);
            if (err != UMC::UMC_OK) return err;
        }
        else
        {
           err = pCodedPicture->PutStartCode(0x0000010B);
           if (err != UMC::UMC_OK) return err; 
           err = pCodedPicture->PutBits(firstRow, 9);
           if (err != UMC::UMC_OK) return err;
           err = pCodedPicture->PutBits(0, 1);
           if (err != UMC::UMC_OK) return err;

        }
        return VLC_I_Frame(pCodedPicture,firstRow, nRows);
    }
    UMC::Status     VLC_PFrame(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow=0, Ipp32s nRows=0)
    {
        UMC::Status err = UMC::UMC_OK;
        if (firstRow == 0)
        {
            err = WritePPictureHeader(pCodedPicture);
            if (err != UMC::UMC_OK) return err;
        }
        else
        {
           err = pCodedPicture->PutStartCode(0x0000010B);
           if (err != UMC::UMC_OK) return err; 
           err = pCodedPicture->PutBits(firstRow, 9);
           if (err != UMC::UMC_OK) return err;
           err = pCodedPicture->PutBits(0, 1);
           if (err != UMC::UMC_OK) return err;
        }
        return VLC_P_Frame(pCodedPicture,firstRow, nRows);
    }    
    UMC::Status     VLC_PFrameMixed(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow=0, Ipp32s nRows=0)
    {
        UMC::Status err = UMC::UMC_OK;
        if (firstRow == 0)
        {
            err = WritePPictureHeader(pCodedPicture);
            if (err != UMC::UMC_OK) return err;
        }
        else
        {
           err = pCodedPicture->PutStartCode(0x0000010B);
           if (err != UMC::UMC_OK) return err; 
           err = pCodedPicture->PutBits(firstRow, 9);
           if (err != UMC::UMC_OK) return err;
           err = pCodedPicture->PutBits(0, 1);
           if (err != UMC::UMC_OK) return err;
        }
        return VLC_P_FrameMixed(pCodedPicture,firstRow, nRows);
    }    
   
    UMC::Status     VLC_BFrame(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow=0, Ipp32s nRows=0)
    {
        UMC::Status err = UMC::UMC_OK;
        if (firstRow == 0)
        {
            err = WriteBPictureHeader(pCodedPicture);
            if (err != UMC::UMC_OK) return err;
        }
        else
        {
           err = pCodedPicture->PutStartCode(0x0000010B);
           if (err != UMC::UMC_OK) return err; 
           err = pCodedPicture->PutBits(firstRow, 9);
           if (err != UMC::UMC_OK) return err;
           err = pCodedPicture->PutBits(0, 1);
           if (err != UMC::UMC_OK) return err;
        }
        return VLC_B_Frame(pCodedPicture,firstRow, nRows);
    }    



    UMC::Status     VLC_I_FieldPic(VC1EncoderBitStreamAdv* pCodedPicture, bool bSecondField,  Ipp32s firstRow=0, Ipp32s nRows=0)
    {
        UMC::Status err = UMC::UMC_OK;
        Ipp32u      h   = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32u      shift = (bSecondField)? h : 0;

        if (firstRow == 0)
        {
            if(!bSecondField)
            {
                err = WriteFieldPictureHeader(pCodedPicture);
                if (err != UMC::UMC_OK) return err;
            }
            else
            {
                err = pCodedPicture->PutStartCode(0x0000010C);
                if (err != UMC::UMC_OK) return err;      
            }
            err = WriteIFieldHeader(pCodedPicture);
            if (err != UMC::UMC_OK) return err;
       }
       else
       {
           err = pCodedPicture->PutStartCode(0x0000010B);
           if (err != UMC::UMC_OK) return err; 
           err = pCodedPicture->PutBits(firstRow + shift, 9);
           if (err != UMC::UMC_OK) return err;
           err = pCodedPicture->PutBits(0, 1);
           if (err != UMC::UMC_OK) return err;
       }


       err = VLC_IField(pCodedPicture, bSecondField, firstRow, nRows);
       return err;
    }

    UMC::Status     VLC_P_FieldPic(VC1EncoderBitStreamAdv* pCodedPicture, bool bSecondField,  Ipp32s firstRow=0, Ipp32s nRows=0)
    {
        UMC::Status err = UMC::UMC_OK;
        Ipp32u      h   = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32u      shift = (bSecondField)? h : 0;

        if (firstRow == 0)
        {
            if(!bSecondField)
            {
                err = WriteFieldPictureHeader(pCodedPicture);
                if (err != UMC::UMC_OK) return err;
            }
            else
            {
                err = pCodedPicture->PutStartCode(0x0000010C);
                if (err != UMC::UMC_OK) return err;      
            }
            err = WritePFieldHeader(pCodedPicture);
            if (err != UMC::UMC_OK) return err;
       }
       else
       {
           err = pCodedPicture->PutStartCode(0x0000010B);
           if (err != UMC::UMC_OK) return err; 
           err = pCodedPicture->PutBits(firstRow + shift, 9);
           if (err != UMC::UMC_OK) return err;
           err = pCodedPicture->PutBits(0, 1);
           if (err != UMC::UMC_OK) return err;
       }

        if(m_uiReferenceFieldType != VC1_ENC_REF_FIELD_BOTH)
           err = VLC_PField1Ref(pCodedPicture, bSecondField,firstRow,nRows);
        else
           err = VLC_PField2Ref(pCodedPicture, bSecondField,firstRow,nRows);

        return err;
    }

    UMC::Status     VLC_B_FieldPic(VC1EncoderBitStreamAdv* pCodedPicture, bool bSecondField,  Ipp32s firstRow=0, Ipp32s nRows=0)
    {
        UMC::Status err = UMC::UMC_OK;
        Ipp32u      h   = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32u      shift = (bSecondField)? h : 0;
        
        if (firstRow == 0)
        {
            if(!bSecondField)
            {
                err = WriteFieldPictureHeader(pCodedPicture);
                if (err != UMC::UMC_OK) return err;
            }
            else
            {
                err = pCodedPicture->PutStartCode(0x0000010C);
                if (err != UMC::UMC_OK) return err;      
            }
            err = WriteBFieldHeader(pCodedPicture);
            if (err != UMC::UMC_OK) return err;
       }
       else
       {
           err = pCodedPicture->PutStartCode(0x0000010B);
           if (err != UMC::UMC_OK) return err; 
           err = pCodedPicture->PutBits(firstRow  + shift, 9);
           if (err != UMC::UMC_OK) return err;
           err = pCodedPicture->PutBits(0, 1);
           if (err != UMC::UMC_OK) return err;
       }
        
       err = VLC_BField(pCodedPicture, bSecondField,firstRow,nRows);
       return err;
    }
    UMC::Status     VLC_B_FieldPicMixed(VC1EncoderBitStreamAdv* pCodedPicture, bool bSecondField,  Ipp32s firstRow=0, Ipp32s nRows=0)
    {
        UMC::Status err = UMC::UMC_OK;
        Ipp32u      h   = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32u      shift = (bSecondField)? h : 0;
        
        if (firstRow == 0)
        {
            if(!bSecondField)
            {
                err = WriteFieldPictureHeader(pCodedPicture);
                if (err != UMC::UMC_OK) return err;
            }
            else
            {
                err = pCodedPicture->PutStartCode(0x0000010C);
                if (err != UMC::UMC_OK) return err;      
            }
            err = WriteBFieldHeader(pCodedPicture);
            if (err != UMC::UMC_OK) return err;
       }
       else
       {
           err = pCodedPicture->PutStartCode(0x0000010B);
           if (err != UMC::UMC_OK) return err; 
           err = pCodedPicture->PutBits(firstRow  + shift, 9);
           if (err != UMC::UMC_OK) return err;
           err = pCodedPicture->PutBits(0, 1);
           if (err != UMC::UMC_OK) return err;
       }
        
       err = VLC_BFieldMixed(pCodedPicture, bSecondField,firstRow,nRows);
       return err;
    }
    UMC::Status     VLC_P_FieldPicMixed(VC1EncoderBitStreamAdv* pCodedPicture,
                                   bool bSecondField,  Ipp32s firstRow = 0, Ipp32s nRows = 0)
    {

        UMC::Status err = UMC::UMC_OK;
        Ipp32u      h   = ((m_pSequenceHeader->GetPictureHeight()/2)+15)/16;
        Ipp32u      shift = (bSecondField)? h : 0;

        if (firstRow == 0)
        {
            if(!bSecondField)
            {
                err = WriteFieldPictureHeader(pCodedPicture);
                if (err != UMC::UMC_OK) return err;
            }
            else
            {
                err = pCodedPicture->PutStartCode(0x0000010C);
                if (err != UMC::UMC_OK) return err;      
            }
            err = WritePFieldHeader(pCodedPicture);
            if (err != UMC::UMC_OK) return err;
       }
       else
       {
           err = pCodedPicture->PutStartCode(0x0000010B);
           if (err != UMC::UMC_OK) return err; 
           err = pCodedPicture->PutBits(firstRow + shift, 9);
           if (err != UMC::UMC_OK) return err;
           err = pCodedPicture->PutBits(0, 1);
           if (err != UMC::UMC_OK) return err;
       }

            if(m_uiReferenceFieldType != VC1_ENC_REF_FIELD_BOTH)
            {
               err = VLC_PField1RefMixed(pCodedPicture, bSecondField,firstRow,nRows);
            }
            else
            {
                err = VLC_PField2RefMixed(pCodedPicture, bSecondField,firstRow,nRows);                
            }
        return err;
    }

   UMC::Status     PACPFieldMixed(UMC::MeParams* MEParams, bool bSecondField, Ipp32s firstRow = 0, Ipp32s nRows = 0)
    {
        UMC::Status err = UMC::UMC_OK;
        if(m_uiReferenceFieldType != VC1_ENC_REF_FIELD_BOTH)
            err = PAC_PField1RefMixed(MEParams,bSecondField, firstRow, nRows);
        else
            err = PAC_PField2RefMixed(MEParams,bSecondField, firstRow, nRows);

        return err;
    }

    UMC::Status     VLC_SkipFrame(VC1EncoderBitStreamAdv*  pCodedPicture, Ipp32s /*firstRow*/=0, Ipp32s /*nRows*/=0)
    {
        return WriteSkipPictureHeader(pCodedPicture);
    };


    UMC::Status     PAC_PField1RefMixed(UMC::MeParams* MEParams,bool bSecondField, Ipp32s firstRow = 0, Ipp32s nRows = 0);
    UMC::Status     PAC_PField2RefMixed(UMC::MeParams* MEParams,bool bSecondField, Ipp32s firstRow = 0, Ipp32s nRows = 0);

    UMC::Status     SetPictureQuantParams(Ipp8u uiQuantIndex, bool bHalfQuant);
    //UMC::Status     VLC_BField(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField);
    UMC::Status     SetInterpolationParams (IppVCInterpolateBlock_8u* pY,
                                            IppVCInterpolateBlock_8u* pU,
                                            IppVCInterpolateBlock_8u* pV,
                                            Ipp8u* buffer,
                                            Ipp32u w,
                                            Ipp32u h,
                                            Ipp8u **pPlane,
                                            Ipp32u *pStep,
                                            bool bField);
    inline void SetIntesityCompensationParameters (bool bIntensity, Ipp32u scale, Ipp32u shift, bool bBottom = false)
    {
        m_bIntensity           = bIntensity;
        if (!bBottom)
        {
            m_uiIntensityLumaScale = (Ipp8u)scale;                               
            m_uiIntensityLumaShift = (Ipp8u)shift; 
            m_uiFieldIntensityType = (bIntensity)? 
                (eFieldType)(m_uiFieldIntensityType | VC1_ENC_TOP_FIELD) : (eFieldType)(m_uiFieldIntensityType & ~VC1_ENC_TOP_FIELD);
        }
        else
        {
            m_uiIntensityLumaScaleB = (Ipp8u)scale;
            m_uiIntensityLumaShiftB = (Ipp8u)shift;
            m_uiFieldIntensityType = (bIntensity)? 
                (eFieldType)(m_uiFieldIntensityType | VC1_ENC_BOTTOM_FIELD) : (eFieldType)(m_uiFieldIntensityType & ~VC1_ENC_BOTTOM_FIELD);
        }
    }
    inline void SetPictureTransformType (eTransformType  transformType)
    {
        if (m_pSequenceHeader->IsVSTransform())
        {
            m_bVSTransform = true;
            m_uiTransformType = transformType;
        }
    }

#ifdef VC1_ME_MB_STATICTICS
    void SetMEStat(UMC::MeMbStat*   stat, bool bSecondField);
#endif

protected:
    UMC::Status     PAC_PField1Ref(UMC::MeParams* MEParams,bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     PAC_PField2Ref(UMC::MeParams* MEParams,bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     VLC_PField1Ref(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     VLC_PField2Ref(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     VLC_IField(VC1EncoderBitStreamAdv* pCodedPicture, bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     VLC_BField(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     VLC_BFieldMixed(VC1EncoderBitStreamAdv* pCodedPicture,bool bSecondField, Ipp32s firstRow=0, Ipp32s nRows=0);
    
    UMC::Status     VLC_I_Frame(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow, Ipp32s nRows);
    UMC::Status     VLC_P_Frame(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow, Ipp32s nRows);
    UMC::Status     VLC_P_FrameMixed(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow, Ipp32s nRows);
    UMC::Status     VLC_B_Frame(VC1EncoderBitStreamAdv* pCodedPicture, Ipp32s firstRow, Ipp32s nRows);

};
}
#endif
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
