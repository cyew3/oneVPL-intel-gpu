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

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_huffman.h"

static const Ipp32u bc_lut_2[] = {0,1,2,3};
static const Ipp32u bc_lut_1[] = {4,0,1,3};


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

VC1Status DecodePictHeaderParams_ProgressiveBpicture_Adv (VC1Context* pContext)
{
    VC1Status vc1Res = VC1_OK;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    VC1SequenceLayerHeader* seqLayerHeader = &pContext->m_seqLayerHeader;

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_BFRAMES,
                                            VM_STRING("B frame type  \n"));
#endif

    //extended MV range flag
    MVRangeDecode(pContext);

    //motion vector mode
    VC1_GET_BITS(1, picLayerHeader->MVMODE);
    picLayerHeader->MVMODE =(picLayerHeader->MVMODE==1)? VC1_MVMODE_1MV:VC1_MVMODE_HPELBI_1MV;


    //B frame direct mode macroblock bit syntax element
    DecodeBitplane(pContext, &picLayerHeader->m_DirectMB,
                   seqLayerHeader->widthMB, seqLayerHeader->heightMB,0);

    //skipped macroblock bit syntax element
    DecodeBitplane(pContext, &picLayerHeader->SKIPMB,
                   seqLayerHeader->widthMB, seqLayerHeader->heightMB,0);

    //motion vector table
    VC1_GET_BITS(2, picLayerHeader->MVTAB);       //MVTAB

    //coded block pattern table
    VC1_GET_BITS(2,picLayerHeader->CBPTAB);       //CBPTAB

    vc1Res = VOPDQuant(pContext);

    if(seqLayerHeader->VSTRANSFORM)
    {
        //macroblock-level transform type flag
        VC1_GET_BITS(1, picLayerHeader->TTMBF);

        if(picLayerHeader->TTMBF)
        {
            //frame-level transform type
            VC1_GET_BITS(2, picLayerHeader->TTFRM_ORIG);
            picLayerHeader->TTFRM = 1 << picLayerHeader->TTFRM_ORIG;
        }
        else
        {
            picLayerHeader->TTFRM = VC1_BLK_INTER;
        }
    }
    else
    {
        picLayerHeader->TTFRM = VC1_BLK_INTER8X8;
    }


    VC1_GET_BITS(1, picLayerHeader->TRANSACFRM);//TRANSACFRM
    if(picLayerHeader->TRANSACFRM == 1)
    {
        VC1_GET_BITS(1, picLayerHeader->TRANSACFRM);
        picLayerHeader->TRANSACFRM++;
    }

    VC1_GET_BITS(1, picLayerHeader->TRANSDCTAB);       //TRANSDCTAB

#ifdef ALLOW_SW_VC1_FALLBACK
    ChooseTTMB_TTBLK_SBP(pContext);
    picLayerHeader->m_pCurrMVDifftbl = pContext->m_vlcTbl->MVDIFF_PB_TABLES[picLayerHeader->MVTAB];    //MVTAB
    picLayerHeader->m_pCurrCBPCYtbl = pContext->m_vlcTbl->CBPCY_PB_TABLES[picLayerHeader->CBPTAB];       //CBPTAB
    ChooseACTable(pContext, picLayerHeader->TRANSACFRM, picLayerHeader->TRANSACFRM);//TRANSACFRM
    ChooseDCTable(pContext, picLayerHeader->TRANSDCTAB);       //TRANSDCTAB
#endif
    return vc1Res;
}


VC1Status DecodePictHeaderParams_InterlaceBpicture_Adv(VC1Context* pContext)
{
    VC1Status vc1Res = VC1_OK;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    VC1SequenceLayerHeader* seqLayerHeader = &pContext->m_seqLayerHeader;

    Ipp32u tempValue;

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_BFRAMES,
                                            VM_STRING("B frame type  \n"));
#endif

    {
        //B picture fraction
        Ipp8s  z1;
        Ipp16s z2;
        DecodeHuffmanPair(&pContext->m_bitstream.pBitstream,
                                    &pContext->m_bitstream.bitOffset,
                                    pContext->m_vlcTbl->BFRACTION,
                                    &z1, &z2);
        VM_ASSERT (z2 != VC1_BRACTION_INVALID);
        VM_ASSERT (!(z2 == VC1_BRACTION_BI && seqLayerHeader->PROFILE==VC1_PROFILE_ADVANCED));

        if (z2 == VC1_BRACTION_BI)
        {
            picLayerHeader->PTYPE = VC1_BI_FRAME;
        }
        picLayerHeader->BFRACTION = (z1*2>=z2)?1:0;
        picLayerHeader->ScaleFactor = ((256+z2/2)/z2)*z1;
        picLayerHeader->BFRACTION_orig = VC1_VA_Bfraction_tbl[z1-1][z2-2];
    }


    //extended MV range flag
    MVRangeDecode(pContext);

    //extended differential MV Range Flag
    if(seqLayerHeader->EXTENDED_DMV == 1)
    {
        VC1_GET_BITS(1, picLayerHeader->DMVRANGE);
        if(picLayerHeader->DMVRANGE==0)
        {
            //binary code 0
            picLayerHeader->DMVRANGE = VC1_DMVRANGE_NONE;
        }
        else
        {
            VC1_GET_BITS(1, picLayerHeader->DMVRANGE);
            if(picLayerHeader->DMVRANGE==0)
            {
               //binary code 10
               picLayerHeader->DMVRANGE = VC1_DMVRANGE_HORIZONTAL_RANGE;
            }
            else
            {
                VC1_GET_BITS(1, picLayerHeader->DMVRANGE);
                if(picLayerHeader->DMVRANGE==0)
                {
                    //binary code 110
                    picLayerHeader->DMVRANGE = VC1_DMVRANGE_VERTICAL_RANGE;
                }
                else
                {
                    //binary code 111
                    picLayerHeader->DMVRANGE = VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE;
                }
            }
        }
    }

    //intensity compensation
    VC1_GET_BITS(1, tempValue);       //INTCOMP

    //B frame direct mode macroblock bit syntax element
    DecodeBitplane(pContext, &picLayerHeader->m_DirectMB,
                   seqLayerHeader->widthMB, seqLayerHeader->heightMB,0);

    //skipped macroblock bit syntax element
    DecodeBitplane(pContext, &picLayerHeader->SKIPMB,
                   seqLayerHeader->widthMB, seqLayerHeader->heightMB,0);


    //in B pic MVMODE always VC1_MVMODE_1MV
    picLayerHeader->MVMODE = VC1_MVMODE_1MV;

    //motion vector table
    VC1_GET_BITS(2, picLayerHeader->MBMODETAB);       //MBMODETAB

    //motion vector table
    VC1_GET_BITS(2, picLayerHeader->MVTAB);       //MVTAB


    //coded block pattern table
    VC1_GET_BITS(3,picLayerHeader->CBPTAB);       //CBPTAB

    VC1_GET_BITS(2, picLayerHeader->MV2BPTAB);       //MV2BPTAB

    VC1_GET_BITS(2, picLayerHeader->MV4BPTAB)        //MV4BPTAB;

    vc1Res = VOPDQuant(pContext);

    if(seqLayerHeader->VSTRANSFORM)
    {
        //macroblock-level transform type flag
        VC1_GET_BITS(1, picLayerHeader->TTMBF);

        if(picLayerHeader->TTMBF)
        {
            //frame-level transform type
            VC1_GET_BITS(2, picLayerHeader->TTFRM_ORIG);
            picLayerHeader->TTFRM = 1 << picLayerHeader->TTFRM_ORIG;
        }
        else
        {
            picLayerHeader->TTFRM = VC1_BLK_INTER;
        }
    }
    else
    {
        picLayerHeader->TTFRM = VC1_BLK_INTER8X8;
    }

    //frame-level transform AC Coding set index
    VC1_GET_BITS(1, picLayerHeader->TRANSACFRM);//TRANSACFRM
    if(picLayerHeader->TRANSACFRM == 1)
    {
        VC1_GET_BITS(1, picLayerHeader->TRANSACFRM);
        picLayerHeader->TRANSACFRM++;
    }

    VC1_GET_BITS(1, picLayerHeader->TRANSDCTAB);       //TRANSDCTAB

#ifdef ALLOW_SW_VC1_FALLBACK
    ChooseTTMB_TTBLK_SBP(pContext);
    ChooseMBModeInterlaceFrame(pContext, 0, picLayerHeader->MBMODETAB);
    ChooseACTable(pContext, picLayerHeader->TRANSACFRM, picLayerHeader->TRANSACFRM);//TRANSACFRM
    ChooseDCTable(pContext, picLayerHeader->TRANSDCTAB);       //TRANSDCTAB
    picLayerHeader->m_pMV2BP = pContext->m_vlcTbl->MV2BP_TABLES[picLayerHeader->MV2BPTAB];
    picLayerHeader->m_pMV4BP = pContext->m_vlcTbl->MV4BP_TABLES[picLayerHeader->MV4BPTAB];
    picLayerHeader->m_pCurrCBPCYtbl = pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[picLayerHeader->CBPTAB];       //CBPTAB
    picLayerHeader->m_pCurrMVDifftbl = pContext->m_vlcTbl->MV_INTERLACE_TABLES[8 + picLayerHeader->MVTAB]; //MVTAB
#endif

    return vc1Res;
}


VC1Status DecodeFieldHeaderParams_InterlaceFieldBpicture_Adv (VC1Context* pContext)
{
    VC1Status vc1Res = VC1_OK;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    VC1SequenceLayerHeader* seqLayerHeader = &pContext->m_seqLayerHeader;
    Ipp32u tempValue;

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_BFRAMES,
                                            VM_STRING("B frame type  \n"));
#endif

    picLayerHeader->NUMREF = 1;


    VC1_GET_BITS(5,picLayerHeader->PQINDEX);
    
    if(picLayerHeader->PQINDEX<=8)
    {
        VC1_GET_BITS(1,picLayerHeader->HALFQP);
    }
    else
        picLayerHeader->HALFQP = 0;


    if(seqLayerHeader->QUANTIZER == 1)
    {
        VC1_GET_BITS(1,picLayerHeader->PQUANTIZER);    //PQUANTIZER
    }
        
    CalculatePQuant(pContext);

    if(seqLayerHeader->POSTPROCFLAG)
    {
         //post processing
          VC1_GET_BITS(2,tempValue);        //POSTPROC
    }

    //extended MV range flag
    MVRangeDecode(pContext);

    //extended differential MV Range Flag
    DMVRangeDecode(pContext);

    //motion vector mode
    if(picLayerHeader->PQUANT > 12)
    {
        //MVMODE VLC    Mode
        //1             1 MV Half-pel bilinear
        //01            1 MV
        //001           1 MV Half-pel
        //000           Mixed MV
        Ipp32s bit_count = 1;
        VC1_GET_BITS(1, picLayerHeader->MVMODE);
        while((picLayerHeader->MVMODE == 0) && (bit_count < 3))
        {
            VC1_GET_BITS(1, picLayerHeader->MVMODE);
            bit_count++;
        }
        if (bit_count < 3)
            picLayerHeader->MVMODE = VC1_LUT_SET(bit_count,bc_lut_1);
        else
            if(picLayerHeader->MVMODE == 0)
                picLayerHeader->MVMODE = VC1_MVMODE_MIXED_MV;
            else
                picLayerHeader->MVMODE = VC1_MVMODE_HPEL_1MV;
    }
    else
    {
        //MVMODE VLC    Mode
        //1             1 MV
        //01            Mixed MV
        //001           1 MV Half-pel
        //000           1 MV Half-pel bilinear
        Ipp32s bit_count = 1;
        VC1_GET_BITS(1, picLayerHeader->MVMODE);
        while((picLayerHeader->MVMODE == 0) && (bit_count < 3))
        {
            VC1_GET_BITS(1, picLayerHeader->MVMODE);
            bit_count++;
        }
        if (bit_count < 3)
            picLayerHeader->MVMODE = VC1_LUT_SET(bit_count,bc_lut_2);
        else
            if(picLayerHeader->MVMODE == 0)
                picLayerHeader->MVMODE = VC1_MVMODE_HPELBI_1MV;
            else
                picLayerHeader->MVMODE = VC1_MVMODE_HPEL_1MV;
    }

    //FORWARDMB
    if (picLayerHeader->CurrField == 0)
    DecodeBitplane(pContext, &picLayerHeader->FORWARDMB,
                   seqLayerHeader->widthMB, (seqLayerHeader->heightMB+1)/2,0);
    else
    DecodeBitplane(pContext, &picLayerHeader->FORWARDMB,
                   seqLayerHeader->widthMB, (seqLayerHeader->heightMB+1)/2,
                   seqLayerHeader->MaxWidthMB * ((seqLayerHeader->heightMB+1)/2));

    //motion vector table
    VC1_GET_BITS(3, picLayerHeader->MBMODETAB);       //MBMODETAB

    VC1_GET_BITS(3, picLayerHeader->MVTAB);       //MVTAB

    //coded block pattern table
    VC1_GET_BITS(3, picLayerHeader->CBPTAB);       //CBPTAB

    if(picLayerHeader->MVMODE == VC1_MVMODE_MIXED_MV)
    {
        VC1_GET_BITS(2, picLayerHeader->MV4BPTAB)        //MV4BPTAB;
    }

    vc1Res = VOPDQuant(pContext);

    if(seqLayerHeader->VSTRANSFORM == 1)
    {
        //macroblock - level transform type flag
        VC1_GET_BITS(1, picLayerHeader->TTMBF);

        if(picLayerHeader->TTMBF)
        {
            //frame-level transform type
            VC1_GET_BITS(2, picLayerHeader->TTFRM_ORIG);
            picLayerHeader->TTFRM = 1 << picLayerHeader->TTFRM_ORIG;
        }
        else
            picLayerHeader->TTFRM = VC1_BLK_INTER;
    }
    else
    {
        picLayerHeader->TTFRM = VC1_BLK_INTER8X8;
    }

    //frame-level transform AC Coding set index
    VC1_GET_BITS(1, picLayerHeader->TRANSACFRM);//TRANSACFRM
    if(picLayerHeader->TRANSACFRM == 1)
    {
        VC1_GET_BITS(1, picLayerHeader->TRANSACFRM);
        picLayerHeader->TRANSACFRM++;
    }

    VC1_GET_BITS(1, picLayerHeader->TRANSDCTAB);       //TRANSDCTAB


    picLayerHeader->REFDIST = *pContext->pRefDist;

#ifdef ALLOW_SW_VC1_FALLBACK
    ChooseTTMB_TTBLK_SBP(pContext);
    picLayerHeader->m_pCurrMVDifftbl = pContext->m_vlcTbl->MV_INTERLACE_TABLES[picLayerHeader->MVTAB]; //MVTAB
    picLayerHeader->m_pCurrCBPCYtbl = pContext->m_vlcTbl->CBPCY_PB_INTERLACE_TABLES[picLayerHeader->CBPTAB];     //CBPTAB
    if (picLayerHeader->MVMODE == VC1_MVMODE_MIXED_MV)
    {
        picLayerHeader->m_pMV4BP = pContext->m_vlcTbl->MV4BP_TABLES[picLayerHeader->MV4BPTAB];
    }

    ChooseMBModeInterlaceField(pContext, picLayerHeader->MBMODETAB);
    ChooseACTable(pContext, picLayerHeader->TRANSACFRM, picLayerHeader->TRANSACFRM); //TRANSACFRM
    ChooseDCTable(pContext, picLayerHeader->TRANSDCTAB);       //TRANSDCTAB
    ChoosePredScaleValueBPictbl(picLayerHeader);
#endif

    return vc1Res;
}

#ifdef ALLOW_SW_VC1_FALLBACK
VC1Status Decode_InterlaceFieldBpicture_Adv (VC1Context* pContext)
{
    Ipp32s i,j;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1Status vc1Res = VC1_OK;

    DecodeFieldHeaderParams_InterlaceFieldBpicture_Adv(pContext);

    for(i = 0; i < sMB->widthMB;i++)
    {
        for(j = 0; j < (sMB->heightMB+1)/2; j++)
        {
            vc1Res = MBLayer_Field_InterlacedBpicture(pContext);
            if(vc1Res != VC1_OK)
            {
                VM_ASSERT(0);
                break;
            }
            sMB->m_currMBXpos++;
            pContext->m_pBlock += 8*8*6;

            pContext->m_pCurrMB++;
            pContext->CurrDC++;
        }

        sMB->m_currMBXpos = 0;
        sMB->m_currMBYpos++;
        sMB->slice_currMBYpos++;
        pContext->CurrDC += (sMB->MaxWidthMB - sMB->widthMB);
        pContext->m_pBlock += (sMB->MaxWidthMB - sMB->widthMB)*8*8*6;
    }

    if ((pContext->m_seqLayerHeader.LOOPFILTER))
    {
        Ipp32u deblock_offset = 0;
        if (!pContext->DeblockInfo.is_last_deblock)
            deblock_offset = 1;

        pContext->DeblockInfo.start_pos = pContext->DeblockInfo.start_pos+pContext->DeblockInfo.HeightMB-deblock_offset;
        pContext->DeblockInfo.HeightMB = sMB->slice_currMBYpos+1;

        Deblocking_InterlaceFieldBpicture_Adv(pContext);
    }

    pContext->m_picLayerHeader->is_slice = 0;
    return vc1Res;
}
#endif // #ifdef ALLOW_SW_VC1_FALLBACK
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
