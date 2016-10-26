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

#ifndef _ENCODER_VC1_MD_H_
#define _ENCODER_VC1_MD_H_

#include "ippdefs.h"
#include "umc_structures.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_enc_def.h"

namespace UMC_VC1_ENCODER
{
    struct VC1EncMD_P
    {
        Ipp8u*               pYSrc; 
        Ipp32u               srcYStep;
        Ipp8u*               pUSrc; 
        Ipp32u               srcUStep;
        Ipp8u*               pVSrc; 
        Ipp32u               srcVStep;

        Ipp8u*               pYRef; 
        Ipp32u               refYStep;      
        Ipp8u*               pURef; 
        Ipp32u               refUStep;
        Ipp8u*               pVRef; 
        Ipp32u               refVStep;

        Ipp32u               quant;
        Ipp32u               bUniform; 
        Ipp32u               intraPattern;
        Ipp32u               DecTypeAC1;
        const Ipp8u**        pScanMatrix;

        Ipp32u               CBPTab;
        bool                 bField;
    }; 
    struct VC1EncMD_B
    {
        Ipp8u*               pYSrc; 
        Ipp32u               srcYStep;
        Ipp8u*               pUSrc; 
        Ipp32u               srcUStep;
        Ipp8u*               pVSrc; 
        Ipp32u               srcVStep;

        Ipp8u*               pYRef[2]; 
        Ipp32u               refYStep[2];      
        Ipp8u*               pURef[2]; 
        Ipp32u               refUStep[2];
        Ipp8u*               pVRef[2]; 
        Ipp32u               refVStep[2];

        Ipp32u               quant;
        Ipp32u               bUniform; 
        Ipp32u               intraPattern;
        Ipp32u               DecTypeAC1;
        const Ipp8u**        pScanMatrix;

        Ipp32u               CBPTab;
        bool                 bField;
    }; 
    UMC::Status GetVSTTypeP_RD (VC1EncMD_P* pIn, eTransformType* pVSTTypeOut) ;
    UMC::Status GetVSTTypeB_RD (VC1EncMD_B* pIn, eTransformType* pVSTTypeOut) ;
}
#endif
#endif