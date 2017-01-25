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
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "ippcore.h"

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_common.h"
#include "umc_structures.h"

#ifdef ALLOW_SW_VC1_FALLBACK
#include "umc_vc1_common_mvdiff_tbl.h"
#endif

using namespace UMC;
using namespace UMC::VC1Common;

static const Ipp32u FRateExtD_tbl[] = {2,2,6,7,18,22,26};
static const Ipp32u FRateExtN_tbl[] = {24,8,12,12,24,24,24};

static void reset_index(VC1Context* pContext)
{
    pContext->m_frmBuff.m_iPrevIndex  = -1;
    pContext->m_frmBuff.m_iNextIndex  = -1;
}


//3.1    Sequence-level Syntax and Semantics, figure 7
VC1Status SequenceLayer(VC1Context* pContext)
{
    Ipp32u reserved;
    Ipp32u i=0;
    Ipp32u tempValue;

    pContext->m_seqLayerHeader.ColourDescriptionPresent = 0;

    VC1_GET_BITS(2, pContext->m_seqLayerHeader.PROFILE);
    if (!VC1_IS_VALID_PROFILE(pContext->m_seqLayerHeader.PROFILE))
        return VC1_WRN_INVALID_STREAM;

    if(pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
    {
        VC1_GET_BITS(3, pContext->m_seqLayerHeader.LEVEL);

        VC1_GET_BITS(2,tempValue);     //CHROMAFORMAT
    }
    else
    {
        //remain bites of profile
        VC1_GET_BITS(2, pContext->m_seqLayerHeader.LEVEL);
    }

    VC1_GET_BITS(3, pContext->m_seqLayerHeader.FRMRTQ_POSTPROC);

    VC1_GET_BITS(5, pContext->m_seqLayerHeader.BITRTQ_POSTPROC);

    if(pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
    {

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.POSTPROCFLAG);

        VC1_GET_BITS(12, pContext->m_seqLayerHeader.MAX_CODED_WIDTH);

        VC1_GET_BITS(12, pContext->m_seqLayerHeader.MAX_CODED_HEIGHT);

        pContext->m_seqLayerHeader.CODED_HEIGHT = pContext->m_seqLayerHeader.MAX_CODED_HEIGHT;
        pContext->m_seqLayerHeader.CODED_WIDTH  = pContext->m_seqLayerHeader.MAX_CODED_WIDTH;

        Ipp32u width = 0;
        Ipp32u height = 0;

        width = 2*(pContext->m_seqLayerHeader.CODED_WIDTH+1);
        height = 2*(pContext->m_seqLayerHeader.CODED_HEIGHT+1);

        pContext->m_seqLayerHeader.widthMB  = (Ipp16u)((width+15)/VC1_PIXEL_IN_LUMA);
        pContext->m_seqLayerHeader.heightMB = (Ipp16u)((height+15)/VC1_PIXEL_IN_LUMA);

        width = 2*(pContext->m_seqLayerHeader.MAX_CODED_WIDTH+1);
        height = 2*(pContext->m_seqLayerHeader.MAX_CODED_HEIGHT+1);

        pContext->m_seqLayerHeader.MaxWidthMB  = (Ipp16u)((width+15)/VC1_PIXEL_IN_LUMA);
        pContext->m_seqLayerHeader.MaxHeightMB = (Ipp16u)((height+15)/VC1_PIXEL_IN_LUMA);

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.PULLDOWN);

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.INTERLACE);

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.TFCNTRFLAG);
    }
    else
    {
        VC1_GET_BITS(1, pContext->m_seqLayerHeader.LOOPFILTER);

        VC1_GET_BITS(1, reserved);

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.MULTIRES);

        VC1_GET_BITS(1, reserved);

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.FASTUVMC);

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.EXTENDED_MV);

        VC1_GET_BITS(2, pContext->m_seqLayerHeader.DQUANT);

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.VSTRANSFORM);

        VC1_GET_BITS(1, reserved);

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.OVERLAP);

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.SYNCMARKER);

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.RANGERED);

        VC1_GET_BITS(3, pContext->m_seqLayerHeader.MAXBFRAMES);

        VC1_GET_BITS(2, pContext->m_seqLayerHeader.QUANTIZER);
    }

    VC1_GET_BITS(1, pContext->m_seqLayerHeader.FINTERPFLAG);

    if(pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
    {
        VC1_GET_BITS(2, reserved);

        VC1_GET_BITS(1, tempValue);//DISPLAY_EXT

        if(tempValue)//DISPLAY_EXT
        {
            VC1_GET_BITS(14,tempValue);     //DISP_HORIZ_SIZE

            VC1_GET_BITS(14,tempValue);     //DISP_VERT_SIZE

            VC1_GET_BITS(1,tempValue); //ASPECT_RATIO_FLAG

            if(tempValue)
            {
                VC1_GET_BITS(4,tempValue);        //ASPECT_RATIO

                if(tempValue==15)
                {
                    VC1_GET_BITS(8, pContext->m_seqLayerHeader.AspectRatioW);        //ASPECT_HORIZ_SIZE

                    VC1_GET_BITS(8, pContext->m_seqLayerHeader.AspectRatioH);      //ASPECT_VERT_SIZE
                }
                else
                {
                    pContext->m_seqLayerHeader.AspectRatioW = 0;
                    pContext->m_seqLayerHeader.AspectRatioH = 0;
                }
            }

            VC1_GET_BITS(1,tempValue);      //FRAMERATE_FLAG

            if(tempValue)       //FRAMERATE_FLAG
            {
                VC1_GET_BITS(1,tempValue);    //FRAMERATEIND

                if(!tempValue)      //FRAMERATEIND
                {
                    VC1_GET_BITS(8, pContext->m_seqLayerHeader.FRAMERATENR);      //FRAMERATENR

                    VC1_GET_BITS(4,pContext->m_seqLayerHeader.FRAMERATEDR);  //FRAMERATEDR
                }
                else
                {
                    VC1_GET_BITS(16,tempValue);     //FRAMERATEEXP
                }

            }

            VC1_GET_BITS(1,tempValue);      //COLOR_FORMAT_FLAG

            if(tempValue)       //COLOR_FORMAT_FLAG
            {
                pContext->m_seqLayerHeader.ColourDescriptionPresent = 1;
                VC1_GET_BITS(8, tempValue);         //COLOR_PRIM
                pContext->m_seqLayerHeader.ColourPrimaries = (Ipp16u)tempValue;
                VC1_GET_BITS(8, tempValue); //TRANSFER_CHAR
                pContext->m_seqLayerHeader.TransferCharacteristics = (Ipp16u)tempValue;
                VC1_GET_BITS(8, tempValue);      //MATRIX_COEF
                pContext->m_seqLayerHeader.MatrixCoefficients = (Ipp16u)tempValue;
            }

        }

        VC1_GET_BITS(1, pContext->m_seqLayerHeader.HRD_PARAM_FLAG);
        if(pContext->m_seqLayerHeader.HRD_PARAM_FLAG)
        {
            VC1_GET_BITS(5,pContext->m_seqLayerHeader.HRD_NUM_LEAKY_BUCKETS);
            VC1_GET_BITS(4,tempValue);//BIT_RATE_EXPONENT
            VC1_GET_BITS(4,tempValue);//BUFFER_SIZE_EXPONENT

            for(i=0; i<pContext->m_seqLayerHeader.HRD_NUM_LEAKY_BUCKETS; i++)
            {
                VC1_GET_BITS(16,tempValue);//HRD_RATE[i]
                VC1_GET_BITS(16,tempValue);//HRD_BUFFER[i]
            }
        }

    }
    // need to parse struct_B in case of simple/main profiles
    if (pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
    {
        // last in struct_C
        VC1_GET_BITS(1, tempValue);
        // skip struct_A and 0x000000C
        pContext->m_bitstream.pBitstream += 3;
        VC1_GET_BITS(3, pContext->m_seqLayerHeader.LEVEL);
        VC1_GET_BITS(1,tempValue); // CBR1
        VC1_GET_BITS(4,tempValue); // RES1
        VC1_GET_BITS(12,tempValue); // HRD_BUFFER
        VC1_GET_BITS(12,tempValue);// HRD_BUFFER
        VC1_GET_BITS(32,tempValue);// HRD_RATE
        VC1_GET_BITS(32,tempValue);// FRAME_RATE
    }

    return VC1_OK;

}

VC1Status EntryPointLayer(VC1Context* pContext)
{
    Ipp32u i=0;
    Ipp32u tempValue;
    Ipp32u width, height;

    VC1_GET_BITS(1, pContext->m_seqLayerHeader.BROKEN_LINK);
    VC1_GET_BITS(1, pContext->m_seqLayerHeader.CLOSED_ENTRY);
    VC1_GET_BITS(1, pContext->m_seqLayerHeader.PANSCAN_FLAG);
    VC1_GET_BITS(1, pContext->m_seqLayerHeader.REFDIST_FLAG);
    VC1_GET_BITS(1, pContext->m_seqLayerHeader.LOOPFILTER);
    VC1_GET_BITS(1, pContext->m_seqLayerHeader.FASTUVMC);
    VC1_GET_BITS(1, pContext->m_seqLayerHeader.EXTENDED_MV);
    VC1_GET_BITS(2, pContext->m_seqLayerHeader.DQUANT);
    VC1_GET_BITS(1, pContext->m_seqLayerHeader.VSTRANSFORM);
    VC1_GET_BITS(1, pContext->m_seqLayerHeader.OVERLAP);
    VC1_GET_BITS(2, pContext->m_seqLayerHeader.QUANTIZER);

    if (pContext->m_seqLayerHeader.CLOSED_ENTRY)
        pContext->m_seqLayerHeader.BROKEN_LINK = 0;

    if (pContext->m_seqLayerHeader.BROKEN_LINK && !pContext->m_seqLayerHeader.CLOSED_ENTRY)
        reset_index(pContext);

    if(pContext->m_seqLayerHeader.HRD_PARAM_FLAG == 1)
    {
        for(i=0; i<pContext->m_seqLayerHeader.HRD_NUM_LEAKY_BUCKETS;i++)
        {
            VC1_GET_BITS(8, tempValue);       //m_hrd_buffer_fullness.HRD_FULLNESS[i]
        }
    }

    VC1_GET_BITS(1, tempValue);    //CODED_SIZE_FLAG
    if (tempValue == 1)
    {
        VC1_GET_BITS(12, width);
        VC1_GET_BITS(12, height);
        
        if (pContext->m_seqLayerHeader.CODED_WIDTH > pContext->m_seqLayerHeader.MAX_CODED_WIDTH)
            return VC1_FAIL;
        
        if (pContext->m_seqLayerHeader.CODED_HEIGHT > pContext->m_seqLayerHeader.MAX_CODED_HEIGHT)
            return VC1_FAIL;

        if (pContext->m_seqLayerHeader.CODED_WIDTH > width)
        {
            pContext->m_seqLayerHeader.CLOSED_ENTRY = 1;
            pContext->m_seqLayerHeader.BROKEN_LINK  = 1;
    }
        
        if (pContext->m_seqLayerHeader.CODED_HEIGHT > height)
        {
            pContext->m_seqLayerHeader.CLOSED_ENTRY = 1;
            pContext->m_seqLayerHeader.BROKEN_LINK  = 1;
        }
        
        pContext->m_seqLayerHeader.CODED_WIDTH = width;
        pContext->m_seqLayerHeader.CODED_HEIGHT = height;        
    }
    else
    {
        pContext->m_seqLayerHeader.CODED_WIDTH = pContext->m_seqLayerHeader.MAX_CODED_WIDTH;
        pContext->m_seqLayerHeader.CODED_HEIGHT = pContext->m_seqLayerHeader.MAX_CODED_HEIGHT;
    }

    width = 2*(pContext->m_seqLayerHeader.CODED_WIDTH+1);
    height = 2*(pContext->m_seqLayerHeader.CODED_HEIGHT+1);

    pContext->m_seqLayerHeader.widthMB  = (Ipp16u)((width+15)/VC1_PIXEL_IN_LUMA);
    pContext->m_seqLayerHeader.heightMB = (Ipp16u)((height+15)/VC1_PIXEL_IN_LUMA);

    if (pContext->m_seqLayerHeader.EXTENDED_MV == 1)
    {
        VC1_GET_BITS(1, pContext->m_seqLayerHeader.EXTENDED_DMV);
    }

    VC1_GET_BITS(1, pContext->m_seqLayerHeader.RANGE_MAPY_FLAG);   //RANGE_MAPY_FLAG
    if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG == 1)
    {
        VC1_GET_BITS(3,pContext->m_seqLayerHeader.RANGE_MAPY);
    }
    else
        pContext->m_seqLayerHeader.RANGE_MAPY = -1;

    VC1_GET_BITS(1, pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG); //RANGE_MAPUV_FLAG

    if (pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG == 1)
    {
        VC1_GET_BITS(3,pContext->m_seqLayerHeader.RANGE_MAPUV);
    }
    else
        pContext->m_seqLayerHeader.RANGE_MAPUV = -1;

    return VC1_OK;

}

VC1Status GetNextPicHeader_Adv(VC1Context* pContext)
{
    VC1Status vc1Sts = VC1_OK;

    memset(pContext->m_picLayerHeader, 0, sizeof(VC1PictureLayerHeader));

    vc1Sts = DecodePictureHeader_Adv(pContext);

    return vc1Sts;
}

VC1Status GetNextPicHeader(VC1Context* pContext, bool isExtHeader)
{
    VC1Status vc1Sts = VC1_OK;

    memset(pContext->m_picLayerHeader, 0, sizeof(VC1PictureLayerHeader));

    vc1Sts = DecodePictureHeader(pContext, isExtHeader);

    return vc1Sts;
}

#ifdef _OWN_FUNCTION
//range map
void _own_ippiRangeMap_VC1_8u_C1R(Ipp8u* pSrc, Ipp32s srcStep,
                                  Ipp8u* pDst, Ipp32s dstStep,
                                  IppiSize roiSize,
                                  Ipp32s rangeMapParam)
{
    Ipp32s i=0;
    Ipp32s j=0;
    Ipp32s temp;

    for (i = 0; i < roiSize.height; i++)
    {
        for (j = 0; j < roiSize.width; j++)
        {
            temp = pSrc[i*srcStep+j];

            temp = (temp - 128)*(rangeMapParam+9)+4;
            temp = temp>>3;
            temp = temp+128;
            pDst[i*dstStep+j] = (Ipp8u)VC1_CLIP(temp);
         }
    }
}
#endif

//frame rate calculation
void MapFrameRateIntoMfx(Ipp32u& ENR, Ipp32u& EDR, Ipp16u FCode)
{
    Ipp32u FRateExtN;
    Ipp32u FRateExtD;

    if (ENR && EDR)
    {
        switch (ENR)
        {
        case 1:
            ENR = 24 * 1000;
            break;
        case 2:
            ENR = 25 * 1000;
            break;
        case 3:
            ENR = 30 * 1000;
            break;
        case 4:
            ENR = 50 * 1000;
            break;
        case 5:
            ENR = 60 * 1000;
            break;
        case 6:
            ENR = 48 * 1000;
            break;
        case 7:
            ENR = 72 * 1000;
            break;
        default:
            ENR = 24 * 1000;
            break;
        }
        switch (EDR)
        {
        case 1:
            EDR = 1000;
            break;
        case 2:
            EDR = 1001;
            break;
        default:
            EDR = 1000;
            break;
        }
        return;
    }
    else
    {
        if (FCode > 6)
        {
            ENR = 0;
            EDR = 0;
            return;
        }

        FRateExtN = FRateExtN_tbl[FCode];
        FRateExtD = FRateExtD_tbl[FCode];
        ENR = FRateExtN;
        EDR = FRateExtD;

        return;
    }
 
}
Ipp64f MapFrameRateIntoUMC(Ipp32u ENR,Ipp32u EDR, Ipp32u& FCode)
{
    Ipp64f frate;
    Ipp64f ENRf;
    Ipp64f EDRf;
    if (FCode > 6)
    {
        ENR = 0;
        EDR = 0;
        frate = (Ipp64f)30;
        return frate;
    }
    else
        FCode = 2 + FCode * 4; 

    if (ENR && EDR)
    {
        FCode = 1;
        switch (ENR)
        {
        case 1:
            ENR = 24 * 1000;
            break;
        case 2:
            ENR = 25 * 1000;
            break;
        case 3:
            ENR = 30 * 1000;
            break;
        case 4:
            ENR = 50 * 1000;
            break;
        case 5:
            ENR = 60 * 1000;
            break;
        case 6:
            ENR = 48 * 1000;
            break;
        case 7:
            ENR = 72 * 1000;
            break;
        default:
            ENR = 24 * 1000;
            break;
        }
        switch (EDR)
        {
        case 1:
            EDR = 1000;
            break;
        case 2:
            EDR = 1001;
            break;
        default:
            EDR = 1000;
            break;
        }
    }
    else
    {
        ENR = 1;
        EDR = 1;
    }
    ENRf = ENR;
    EDRf = EDR;
    frate = FCode * ENRf/EDRf;
    return frate;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef ALLOW_SW_VC1_FALLBACK
VC1Status FillTablesForIntensityCompensation(VC1Context* pContext,
    Ipp32u scale,
    Ipp32u shift)
{
    //Ipp32u index = pContext->m_frmBuff.m_iPrevIndex;
    /*scale, shift parameters are in [0,63]*/
    Ipp32s i;
    Ipp32s iscale = (scale) ? scale + 32 : -64;
    Ipp32s ishift = (scale) ? shift * 64 : (255 - 2 * shift) * 64;
    Ipp32s z = (scale) ? -1 : 2;
    Ipp32s j;


    ishift += (shift>31) ? z << 12 : 0;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1, VC1_INTENS,
        VM_STRING("shift=%d, scale=%d, iscale=%d, ishift=%d\n"),
        shift, scale, iscale, ishift);
#endif

    for (i = 0; i<256; i++)
    {
        j = (i*iscale + ishift + 32) >> 6;
        pContext->LumaTable[0][i] = (Ipp8u)VC1_CLIP(j);
        pContext->LumaTable[1][i] = (Ipp8u)VC1_CLIP(j);
        j = ((i - 128)*iscale + 128 * 64 + 32) >> 6;
        pContext->ChromaTable[0][i] = (Ipp8u)VC1_CLIP(j);
        pContext->ChromaTable[1][i] = (Ipp8u)VC1_CLIP(j);

#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1, VC1_INTENS,
            VM_STRING("LumaTable[i]=%d, ChromaTable[i]=%d\n"),
            pContext->LumaTable[0][i], pContext->ChromaTable[0][i]);
#endif
    }

    return VC1_OK;
}

VC1Status FillTablesForIntensityCompensation_Adv(VC1Context* pContext,
    Ipp32u scale,
    Ipp32u shift,
    Ipp32u bottom_field,
    Ipp32s index)
{
    /*scale, shift parameters are in [0,63]*/
    Ipp32s i;
    Ipp32s iscale = (scale) ? scale + 32 : -64;
    Ipp32s ishift = (scale) ? shift * 64 : (255 - 2 * shift) * 64;
    Ipp32s z = (scale) ? -1 : 2;
    Ipp32s j;
    Ipp8u *pY, *pU, *pV;
    IppiSize roiSize;

    roiSize.width = (pContext->m_seqLayerHeader.CODED_WIDTH + 1) << 1;
    roiSize.height = (pContext->m_seqLayerHeader.CODED_HEIGHT + 1) << 1;

    Ipp32s YPitch = pContext->m_frmBuff.m_pFrames[index].m_iYPitch;
    Ipp32s UPitch = pContext->m_frmBuff.m_pFrames[index].m_iUPitch;
    Ipp32s VPitch = pContext->m_frmBuff.m_pFrames[index].m_iVPitch;

    pY = pContext->m_frmBuff.m_pFrames[index].m_pY;
    pU = pContext->m_frmBuff.m_pFrames[index].m_pU;
    pV = pContext->m_frmBuff.m_pFrames[index].m_pV;

    if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
    {
        if (bottom_field)
        {
            pY += YPitch;
            pU += UPitch;
            pV += VPitch;
        }
        YPitch <<= 1;
        UPitch <<= 1;
        VPitch <<= 1;

        roiSize.height >>= 1;
    }

    ishift += (shift>31) ? z * 64 * 64 : 0;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1, VC1_INTENS,
        VM_STRING("shift=%d, scale=%d, iscale=%d, ishift=%d\n"),
        shift, scale, iscale, ishift);
#endif
    Ipp32u LUTindex = bottom_field + (pContext->m_picLayerHeader->CurrField << 1);

    if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
    {
        for (i = 0; i<256; i++)
        {
            j = (i*iscale + ishift + 32) >> 6;
            pContext->LumaTable[LUTindex][i] = (Ipp8u)VC1_CLIP(j);
            j = ((i - 128)*iscale + 128 * 64 + 32) >> 6;
            pContext->ChromaTable[LUTindex][i] = (Ipp8u)VC1_CLIP(j);
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1, VC1_INTENS,
                VM_STRING("LumaTable[i]=%d, ChromaTable[i]=%d\n"),
                pContext->LumaTable[LUTindex][i], pContext->ChromaTable[LUTindex][i]);
#endif
        }
    }
    else
    {
        for (i = 0; i<256; i++)
        {
            j = (i*iscale + ishift + 32) >> 6;
            pContext->LumaTable[0][i] = (Ipp8u)VC1_CLIP(j);
            pContext->LumaTable[1][i] = (Ipp8u)VC1_CLIP(j);
            j = ((i - 128)*iscale + 128 * 64 + 32) >> 6;
            pContext->ChromaTable[0][i] = (Ipp8u)VC1_CLIP(j);
            pContext->ChromaTable[1][i] = (Ipp8u)VC1_CLIP(j);
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1, VC1_INTENS,
                VM_STRING("LumaTable[i]=%d, ChromaTable[i]=%d\n"),
                pContext->LumaTable[LUTindex][i], pContext->ChromaTable[LUTindex][i]);
#endif
        }
    }
    return VC1_OK;
}
#endif

VC1Status MVRangeDecode(VC1Context* pContext)
{
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    if (pContext->m_seqLayerHeader.EXTENDED_MV == 1)
    {
        //MVRANGE;
        //0   256 128
        //10  512 256
        //110 2048 512
        //111 4096 1024

        VC1_GET_BITS(1, picLayerHeader->MVRANGE);

        if (picLayerHeader->MVRANGE)
        {
            VC1_GET_BITS(1, picLayerHeader->MVRANGE);
            if (picLayerHeader->MVRANGE)
            {
                VC1_GET_BITS(1, picLayerHeader->MVRANGE);
                picLayerHeader->MVRANGE += 1;
            }
            picLayerHeader->MVRANGE += 1;
        }
    }
    else
    {
        picLayerHeader->MVRANGE = 0;
    }
    
#ifdef ALLOW_SW_VC1_FALLBACK
    picLayerHeader->m_pCurrMVRangetbl = &VC1_MVRangeTbl[picLayerHeader->MVRANGE];
#endif
    return VC1_OK;
}

VC1Status DMVRangeDecode(VC1Context* pContext)
{
    if (pContext->m_seqLayerHeader.EXTENDED_DMV == 1)
    {
        VC1_GET_BITS(1, pContext->m_picLayerHeader->DMVRANGE);
        if (pContext->m_picLayerHeader->DMVRANGE == 0)
        {
            //binary code 0
            pContext->m_picLayerHeader->DMVRANGE = VC1_DMVRANGE_NONE;
        }
        else
        {
            VC1_GET_BITS(1, pContext->m_picLayerHeader->DMVRANGE);
            if (pContext->m_picLayerHeader->DMVRANGE == 0)
            {
                //binary code 10
                pContext->m_picLayerHeader->DMVRANGE = VC1_DMVRANGE_HORIZONTAL_RANGE;
            }
            else
            {
                VC1_GET_BITS(1, pContext->m_picLayerHeader->DMVRANGE);
                if (pContext->m_picLayerHeader->DMVRANGE == 0)
                {
                    //binary code 110
                    pContext->m_picLayerHeader->DMVRANGE = VC1_DMVRANGE_VERTICAL_RANGE;
                }
                else
                {
                    //binary code 111
                    pContext->m_picLayerHeader->DMVRANGE = VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE;
                }
            }
        }
    }

    return VC1_OK;
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
