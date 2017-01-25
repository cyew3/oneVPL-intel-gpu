//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include <string.h>
#include "umc_vc1_dec_seq.h"
#include "umc_vc1_huffman.h"
#include "umc_vc1_dec_debug.h"

// need for VA structures filling
static const Ipp32s VC1_VA_Bfraction_tbl[7][7] =
{
    0, 1, 3, 5,  114, 116, 122,
    0, 2, 0, 6,    0, 117,   0,
    0, 0, 4, 112,  0, 118, 123,
    0, 0, 0, 113,  0, 119,   0,
    0, 0, 0, 0,  115, 120, 124,
    0, 0, 0, 0,    0, 121,   0,
    0, 0, 0, 0,    0,   0, 125
};

VC1Status DecodePictureHeader (VC1Context* pContext,  bool isExtHeader)
{
    VC1Status vc1Sts = VC1_OK;
    Ipp32u tempValue;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    VC1SequenceLayerHeader* seqLayerHeader = &pContext->m_seqLayerHeader;
    Ipp32u SkFrameSize = 10;

    if (isExtHeader)
        SkFrameSize -= 8;

    if(seqLayerHeader->FINTERPFLAG == 1)
        VC1_GET_BITS(1, tempValue); //INTERPFRM

    VC1_GET_BITS(2, tempValue);       //FRMCNT

    if(seqLayerHeader->RANGERED == 1)
    {
        VC1_GET_BITS(1,picLayerHeader->RANGEREDFRM);
        picLayerHeader->RANGEREDFRM <<= 3;
    }
    else
        picLayerHeader->RANGEREDFRM = 0;

    if(pContext->m_FrameSize < SkFrameSize) //changed from 2
    {
        picLayerHeader->PTYPE = VC1_SKIPPED_FRAME;
    }

    else if(seqLayerHeader->MAXBFRAMES == 0)
    {
        VC1_GET_BITS(1, picLayerHeader->PTYPE);//0 = I, 1 = P
    }
    else
    {
        VC1_GET_BITS(1, picLayerHeader->PTYPE); //1 = P
        if(picLayerHeader->PTYPE == 0)
        {
            VC1_GET_BITS(1, picLayerHeader->PTYPE);
            if(picLayerHeader->PTYPE == 0)
            {
                picLayerHeader->PTYPE = VC1_B_FRAME;
                //it can be BI, will be detected in BFRACTION
            }
            else
            {
                picLayerHeader->PTYPE = VC1_I_FRAME;
            }
        }
        else
        {
            picLayerHeader->PTYPE = VC1_P_FRAME;
        }
    }

    if(picLayerHeader->PTYPE == VC1_B_FRAME)
    {
        Ipp8s  z1;
        Ipp16s z2;
        DecodeHuffmanPair(&pContext->m_bitstream.pBitstream,
                                    &pContext->m_bitstream.bitOffset,
                                    pContext->m_vlcTbl->BFRACTION,
                                    &z1,
                                    &z2);
        VM_ASSERT (z2 != VC1_BRACTION_INVALID);
        VM_ASSERT (!(z2 == VC1_BRACTION_BI && seqLayerHeader->PROFILE==VC1_PROFILE_ADVANCED));
        if(0 == z2)
            return VC1_ERR_INVALID_STREAM;
        if (z2 == VC1_BRACTION_BI)
        {
            picLayerHeader->PTYPE = VC1_BI_FRAME;
        }
        picLayerHeader->BFRACTION = (z1*2>=z2)?1:0;
        picLayerHeader->ScaleFactor = ((256+z2/2)/z2)*z1;
        picLayerHeader->BFRACTION_orig = VC1_VA_Bfraction_tbl[z1-1][z2-2];
    }

    return vc1Sts;
}


VC1Status Decode_PictureLayer(VC1Context* pContext)
{
    VC1Status vc1Sts = VC1_OK;
    switch(pContext->m_picLayerHeader->PTYPE)
    {
    case VC1_I_FRAME:
    case VC1_BI_FRAME:
#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_BFRAMES,
                                                VM_STRING("I frame type  \n"));
#endif
        vc1Sts = DecodePictureLayer_ProgressiveIpicture(pContext);

        break;
    case VC1_P_FRAME:
        //only for simple and main profile
        vc1Sts = DecodePictureLayer_ProgressivePpicture(pContext);
        break;

    case VC1_B_FRAME:
#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_BFRAMES,
                                                VM_STRING("B frame type  \n"));
#endif

        vc1Sts = DecodePictureLayer_ProgressiveBpicture(pContext);
        break;

    case VC1_SKIPPED_FRAME:
        pContext->m_frmBuff.m_iDisplayIndex = pContext->m_frmBuff.m_iCurrIndex;
        break;
    }
    return vc1Sts;
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
