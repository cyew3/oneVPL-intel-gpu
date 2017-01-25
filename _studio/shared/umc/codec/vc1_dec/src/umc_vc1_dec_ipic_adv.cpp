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

VC1Status DecodePictHeaderParams_ProgressiveIpicture_Adv(VC1Context* pContext)
{
    VC1Status vc1Res = VC1_OK;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    VC1SequenceLayerHeader* seqLayerHeader = &pContext->m_seqLayerHeader;


#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,
                    VC1_BFRAMES,VM_STRING("I frame type  \n"));
#endif


    //AC Prediction
    DecodeBitplane(pContext, &picLayerHeader->ACPRED,
                   seqLayerHeader->widthMB,  seqLayerHeader->heightMB,0);

    if( (seqLayerHeader->OVERLAP==1) && (picLayerHeader->PQUANT<=8) )
    {
        //conditional overlap flag
        VC1_GET_BITS(1,picLayerHeader->CONDOVER);
        if(picLayerHeader->CONDOVER)
        {
            VC1_GET_BITS(1,picLayerHeader->CONDOVER);
            if(!picLayerHeader->CONDOVER)
            {
                //10
                picLayerHeader->CONDOVER = VC1_COND_OVER_FLAG_ALL;
            }
            else
            {
                //11
                picLayerHeader->CONDOVER = VC1_COND_OVER_FLAG_SOME;
                DecodeBitplane(pContext, &picLayerHeader->OVERFLAGS,
                            seqLayerHeader->widthMB, seqLayerHeader->heightMB,0);
            }
        }
        else
        {
            //0
            picLayerHeader->CONDOVER = VC1_COND_OVER_FLAG_NONE;
        }
    }


    //frame-level transform AC coding set index
    VC1_GET_BITS(1,picLayerHeader->TRANSACFRM);
    if(picLayerHeader->TRANSACFRM)
    {
        VC1_GET_BITS(1,picLayerHeader->TRANSACFRM);
        picLayerHeader->TRANSACFRM++;
    }


//frame-level transform AC table-2 index
    VC1_GET_BITS(1,picLayerHeader->TRANSACFRM2);
    if(picLayerHeader->TRANSACFRM2)
    {
        VC1_GET_BITS(1,picLayerHeader->TRANSACFRM2);
        picLayerHeader->TRANSACFRM2++;
    }

    //intra transform DC table
    VC1_GET_BITS(1,picLayerHeader->TRANSDCTAB);        //TRANSDCTAB

    //macroblock quantization
    vc1Res = VOPDQuant(pContext);

#ifdef ALLOW_SW_VC1_FALLBACK
    ChooseACTable(pContext, picLayerHeader->TRANSACFRM, picLayerHeader->TRANSACFRM2);
    ChooseDCTable(pContext, picLayerHeader->TRANSDCTAB);        //TRANSDCTAB
#endif

    return vc1Res;
}


VC1Status DecodePictHeaderParams_InterlaceIpicture_Adv(VC1Context* pContext)
{
    VC1Status vc1Res = VC1_OK;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    VC1SequenceLayerHeader* seqLayerHeader = &pContext->m_seqLayerHeader;

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_BFRAMES,
                                            VM_STRING("I frame type  \n"));
#endif

     //field transform flag
    DecodeBitplane(pContext, &picLayerHeader->FIELDTX,
                   seqLayerHeader->widthMB,  seqLayerHeader->heightMB,0);

    //AC Prediction
    DecodeBitplane(pContext, &picLayerHeader->ACPRED,
                   seqLayerHeader->widthMB,  seqLayerHeader->heightMB,0);

    if( (seqLayerHeader->OVERLAP==1) && (picLayerHeader->PQUANT<=8) )
    {
        //conditional overlap flag
        VC1_GET_BITS(1,picLayerHeader->CONDOVER);
        if(picLayerHeader->CONDOVER)
        {
            VC1_GET_BITS(1,picLayerHeader->CONDOVER);
            if(!picLayerHeader->CONDOVER)
            {
                //10
                picLayerHeader->CONDOVER = VC1_COND_OVER_FLAG_ALL;
            }
            else
            {
                //11
                picLayerHeader->CONDOVER = VC1_COND_OVER_FLAG_SOME;
                //conditional overlap macroblock pattern flags
                DecodeBitplane(pContext, &picLayerHeader->OVERFLAGS,
                            seqLayerHeader->widthMB, seqLayerHeader->heightMB,0);
            }
        }
        else
        {
            //0
            picLayerHeader->CONDOVER = VC1_COND_OVER_FLAG_NONE;
        }
    }

    //frame-level transform AC coding set index
    VC1_GET_BITS(1,picLayerHeader->TRANSACFRM);
    if(picLayerHeader->TRANSACFRM)
    {
        VC1_GET_BITS(1,picLayerHeader->TRANSACFRM);
        picLayerHeader->TRANSACFRM++;
    }
//frame-level transform AC table-2 index
    VC1_GET_BITS(1,picLayerHeader->TRANSACFRM2);
    if(picLayerHeader->TRANSACFRM2)
    {
        VC1_GET_BITS(1,picLayerHeader->TRANSACFRM2);
        picLayerHeader->TRANSACFRM2++;
    }

    //intra transform DC table
    VC1_GET_BITS(1,picLayerHeader->TRANSDCTAB);        //TRANSDCTAB

    //macroblock quantization
    vc1Res = VOPDQuant(pContext);

#ifdef ALLOW_SW_VC1_FALLBACK
    ChooseACTable(pContext, picLayerHeader->TRANSACFRM, picLayerHeader->TRANSACFRM2);
    ChooseDCTable(pContext, picLayerHeader->TRANSDCTAB);        //TRANSDCTAB
#endif

    return vc1Res;
}

VC1Status DecodeFieldHeaderParams_InterlaceFieldIpicture_Adv(VC1Context* pContext)
{
    VC1Status vc1Res = VC1_OK;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    VC1SequenceLayerHeader* seqLayerHeader = &pContext->m_seqLayerHeader;

    Ipp32u tempValue;

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_BFRAMES,
                                            VM_STRING("I frame type  \n"));
#endif

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

    //AC Prediction
    if (picLayerHeader->CurrField == 0)
    DecodeBitplane(pContext, &picLayerHeader->ACPRED,
                   seqLayerHeader->widthMB, (seqLayerHeader->heightMB + 1)/2,0);
    else
    DecodeBitplane(pContext, &picLayerHeader->ACPRED,  seqLayerHeader->widthMB,
                   (seqLayerHeader->heightMB+1)/2,
                   seqLayerHeader->MaxWidthMB * ((seqLayerHeader->heightMB+1)/2));


    if( (seqLayerHeader->OVERLAP==1) && (picLayerHeader->PQUANT<=8) )
    {
        //conditional overlap flag
        VC1_GET_BITS(1,picLayerHeader->CONDOVER);
        if(picLayerHeader->CONDOVER)
        {
            VC1_GET_BITS(1,picLayerHeader->CONDOVER);
            if(!picLayerHeader->CONDOVER)
            {
                //10
                picLayerHeader->CONDOVER = VC1_COND_OVER_FLAG_ALL;
            }
            else
            {
                //11
                picLayerHeader->CONDOVER = VC1_COND_OVER_FLAG_SOME;
                //conditional overlap macroblock pattern flags

                if (picLayerHeader->CurrField == 0)
                    DecodeBitplane(pContext, &picLayerHeader->OVERFLAGS,
                    seqLayerHeader->widthMB, (seqLayerHeader->heightMB + 1)/2,0);
                else
                    DecodeBitplane(pContext, &picLayerHeader->OVERFLAGS,
                    seqLayerHeader->widthMB, (seqLayerHeader->heightMB + 1)/2,
                    seqLayerHeader->MaxWidthMB*((seqLayerHeader->heightMB + 1)/2));
            }
        }
        else
        {
            //0
            picLayerHeader->CONDOVER = VC1_COND_OVER_FLAG_NONE;
        }

    }

   //frame-level transform AC coding set index
    VC1_GET_BITS(1,picLayerHeader->TRANSACFRM);
    if(picLayerHeader->TRANSACFRM)
    {
        VC1_GET_BITS(1,picLayerHeader->TRANSACFRM);
        picLayerHeader->TRANSACFRM++;
    }

//frame-level transform AC table-2 index
    VC1_GET_BITS(1,picLayerHeader->TRANSACFRM2);
    if(picLayerHeader->TRANSACFRM2)
    {
        VC1_GET_BITS(1,picLayerHeader->TRANSACFRM2);
        picLayerHeader->TRANSACFRM2++;
    }

    //intra transform DC table
    VC1_GET_BITS(1,picLayerHeader->TRANSDCTAB);        //TRANSDCTAB

    //macroblock quantization
    vc1Res = VOPDQuant(pContext);

#ifdef ALLOW_SW_VC1_FALLBACK
    ChooseACTable(pContext, picLayerHeader->TRANSACFRM, picLayerHeader->TRANSACFRM2);
    ChooseDCTable(pContext, picLayerHeader->TRANSDCTAB);        //TRANSDCTAB
#endif

    return vc1Res;
}

#ifdef ALLOW_SW_VC1_FALLBACK
VC1Status Decode_InterlaceFieldIpicture_Adv(VC1Context* pContext)
{
    Ipp32s i, j;
    VC1Status vc1Res = VC1_OK;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;

    DecodeFieldHeaderParams_InterlaceFieldIpicture_Adv(pContext);

    if (pContext->m_picLayerHeader->is_slice)
        return vc1Res;

    for(i = 0; i < sMB->widthMB;i++)
    {
        for (j = 0; j < (sMB->heightMB/2); j++)
        {
            vc1Res = MBLayer_Field_InterlaceIpicture(pContext);
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

        pContext->DeblockInfo.start_pos =
            pContext->DeblockInfo.start_pos+pContext->DeblockInfo.HeightMB-deblock_offset;
        pContext->DeblockInfo.HeightMB = sMB->slice_currMBYpos+1;

        pContext->DeblockInfo.is_last_deblock = 1;

        Deblocking_ProgressiveIpicture_Adv(pContext);
    }

    return vc1Res;
};
#endif // #ifdef ALLOW_SW_VC1_FALLBACK
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
