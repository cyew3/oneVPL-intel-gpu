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

#ifndef _ENCODER_VC1_PRED
#define _ENCODER_VC1_PRED

#include "umc_vc1_enc_mb.h"
#include "ippi.h"


namespace UMC_VC1_ENCODER
{


bool DCACPredictionIntraFrameSM(VC1EncoderMBData*        pCurrMB,
                                NeighbouringMBsData*  pMBs,
                                VC1EncoderMBData*     pPredBlock,
                                Ipp16s                defPredictor,
                                eDirection*           direction);

bool DCACPredictionFrame(VC1EncoderMBData* pCurrMB,
                         NeighbouringMBsData*  pMBs,
                         VC1EncoderMBData* pPredBlock,
                         Ipp16s defPredictor,
                         eDirection* direction);

bool DCACPredictionFrame4MVIntraMB   (  VC1EncoderMBData*               pCurrMB,
                                        NeighbouringMBsData*            pMBs,
                                        NeighbouringMBsIntraPattern*    pMBsIntraPattern,
                                        VC1EncoderMBData*               pPredBlock,
                                        Ipp16s                          defPredictor,
                                        eDirection*                     direction);

Ipp8s DCACPredictionFrame4MVBlockMixed(  VC1EncoderMBData*               pCurrMB,
                                        Ipp32u                          currIntraPattern,
                                        NeighbouringMBsData*            pMBs,
                                        NeighbouringMBsIntraPattern*    pMBsIntraPattern,
                                        VC1EncoderMBData*               pPredBlock,
                                        Ipp16s                          defPredictor,
                                        eDirection*                     direction);
}
#endif
#endif
