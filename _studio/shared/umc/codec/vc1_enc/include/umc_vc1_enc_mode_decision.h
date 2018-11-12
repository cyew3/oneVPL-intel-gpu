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