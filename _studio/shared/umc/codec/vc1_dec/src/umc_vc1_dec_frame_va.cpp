//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef UMC_RESTRICTED_CODE_VA

#ifndef UMC_RESTRICTED_CODE_VA
#include "umc_va_base.h"
#endif
#include "umc_vc1_dec_frame_descr_va.h"
#include "umc_vc1_dec_frame_descr.h"
#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_common.h"
using namespace UMC::VC1Common;

//#define VC1_DEBUG_ON

#define DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER 21
#define DXVA2_VC1BITPLANE_EXT_BUFFER       22




namespace UMC
{
    using namespace VC1Exceptions;

#ifdef UMC_VA_DXVA
    void VC1PackerDXVA::VC1SetSliceBuffer()
    {
        UMCVACompBuffer* CompBuf;
        m_pSliceInfo = (DXVA_SliceInfo*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        if (CompBuf->GetBufferSize() < sizeof(DXVA_SliceInfo))//*VC1SLICEINPARAL!!!!!!!!!! MAY_BE. Real H/W gives 4096 bytes
            throw vc1_exception(mem_allocation_er);
        //memset(m_pSliceInfo,0,sizeof(DXVA_SliceInfo));
    }
    void VC1PackerDXVA::VC1SetPictureBuffer()
    {
        UMCVACompBuffer* CompBuf;
        m_pPicPtr = (DXVA_PictureParameters*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &CompBuf,sizeof(DXVA_PictureParameters));
        if (CompBuf->GetBufferSize() < sizeof(DXVA_PictureParameters))
            throw vc1_exception(mem_allocation_er);
        //memset(m_pPicPtr,0,sizeof(DXVA_PictureParameters));
    }
    //to support multislice mode. Save number of slices value in reserved bits of first slice
    void VC1PackerDXVA::VC1SetBuffersSize(Ipp32u SliceBufIndex, Ipp32u PicBuffIndex)
    {
        //_MAY_BE_
        UMCVACompBuffer* CompBuf;
        //DXVA_SliceInfo* pSliceInfo = (DXVA_SliceInfo*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        CompBuf->SetDataSize((Ipp32s)(sizeof(DXVA_SliceInfo)*SliceBufIndex));
        CompBuf->SetNumOfItem(SliceBufIndex);
        m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER,&CompBuf);
        CompBuf->SetDataSize(sizeof(DXVA_PictureParameters));
    }

    void VC1PackerDXVA::VC1PackOneSlice  (VC1Context* pContext,
        SliceParams* slparams,
        Ipp32u BufIndex, // only in future realisations
        Ipp32u MBOffset,
        Ipp32u SliceDataSize,
        Ipp32u StartCodeOffset,
        Ipp32u ChoppingType)
    {
        // we should use next buffer for next slice in case of "Whole" mode
        Ipp32u mbShift = 0;
        if (BufIndex)
            ++m_pSliceInfo;
        if (pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
            mbShift = 32; //SC shift
        m_pSliceInfo->wHorizontalPosition = 0;
        m_pSliceInfo->wVerticalPosition = slparams->MBStartRow;
        m_pSliceInfo->dwSliceBitsInBuffer = SliceDataSize*8; //- m_pSliceInfo->bStartCodeBitOffset;
        m_pSliceInfo->wMBbitOffset = (WORD)((31-(BYTE)slparams->m_bitOffset) + MBOffset*8 + mbShift); //32 - Start Code
        m_pSliceInfo->dwSliceDataLocation = StartCodeOffset;
        m_pSliceInfo->bStartCodeBitOffset = 0;
        m_pSliceInfo->bReservedBits = 0;
        m_pSliceInfo->wQuantizerScaleCode = 0;
        //m_pSliceInfo->wNumberMBsInSlice = slparams->MBRowsToDecode*pContext->m_seqLayerHeader.widthMB;
        // bug of Power VR
        m_pSliceInfo->wNumberMBsInSlice = slparams->MBStartRow;

        m_pSliceInfo->wBadSliceChopping = (Ipp8u)ChoppingType;

#ifdef VC1_DEBUG_ON
        printf("Bytes in slice = %d\n",SliceDataSize);
        printf("Start code byte offset in the buffer = %d\n",StartCodeOffset);
        printf("bitoffset at the start of MB data = %d\n",m_pSliceInfo->wMBbitOffset);
#endif
    }
    void VC1PackerDXVA::VC1PackWholeSliceSM (VC1Context* pContext,
        Ipp32u MBOffset,
        Ipp32u SliceDataSize)
    {
        m_pSliceInfo->wHorizontalPosition = 0;
        m_pSliceInfo->wVerticalPosition = 0;
        m_pSliceInfo->dwSliceBitsInBuffer = SliceDataSize*8; //- m_pSliceInfo->bStartCodeBitOffset;
        m_pSliceInfo->wMBbitOffset = (WORD)((31-(BYTE)pContext->m_bitstream.bitOffset) + MBOffset*8);
        m_pSliceInfo->dwSliceDataLocation = 0;
        m_pSliceInfo->bStartCodeBitOffset = 0;
        m_pSliceInfo->bReservedBits = 0;
        m_pSliceInfo->wQuantizerScaleCode = 0;
        m_pSliceInfo->wBadSliceChopping = 0;

#ifdef VC1_DEBUG_ON
        printf("Bytes in slice = %d\n",SliceDataSize);
        printf("bitoffset at the start of MB data = %d\n",m_pSliceInfo->wMBbitOffset);
#endif
    }

    void VC1PackerDXVA::VC1PackPicParams (VC1Context* pContext,
        DXVA_PictureParameters* ptr,
        VideoAccelerator*              va)
    {
        //DXVA_PicParams_VC1* ptr = m_pPicPtr;
        //DXVA_PictureParameters* ptr = m_pPicPtr;
        //memset(ptr,0,sizeof(DXVA_PictureParameters));

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            // Menlow, LRB
#if 1
            if (VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)
            {
                ptr->wPicHeightInMBminus1 = (WORD)pContext->m_seqLayerHeader.CODED_HEIGHT;
            }
            else
            {
                ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) -1);
            }
#else
            // ATI
            ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) -1);
#endif


            ptr->wPicWidthInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_WIDTH + 1) - 1);
        }
        else
        {
            /* Height in MBs minus 1 */
            ptr->wPicHeightInMBminus1 = (WORD)(pContext->m_seqLayerHeader.heightMB - 1);
            /* Width in MBs minus 1 */
            ptr->wPicWidthInMBminus1 = (WORD)(pContext->m_seqLayerHeader.widthMB - 1);
        }

        ptr->bMacroblockWidthMinus1  = 15;
        ptr->bMacroblockHeightMinus1 = 15;
        ptr->bBlockWidthMinus1       = 7;
        ptr->bBlockHeightMinus1      = 7;
        ptr->bBPPminus1              = 7;

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            // interlace fields
            if (VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)
            {
                ptr->bPicExtrapolation = 0x2;
                if (pContext->m_picLayerHeader->CurrField == 0)
                {
                    if (pContext->m_seqLayerHeader.PULLDOWN)
                    {
                        if (pContext->m_picLayerHeader->TFF)
                            ptr->bPicStructure = 1;
                        else
                            ptr->bPicStructure = 2;
                    }
                    else
                        ptr->bPicStructure = 1;
                }
                else
                {
                    if (pContext->m_picLayerHeader->TFF)
                        ptr->bPicStructure = 2;
                    else
                        ptr->bPicStructure = 1;
                }
            }
            else  // frames
            {
                ptr->bPicStructure = 0x03;    //Frame
                if (VC1_Progressive == pContext->m_picLayerHeader->FCM)
                    ptr->bPicExtrapolation = 1; //Progressive
                else
                    ptr->bPicExtrapolation = 2; //Interlace
            }
        }
        else
        {
            // all frames are progressivw
            ptr->bPicExtrapolation = 1;
            ptr->bPicStructure = 3;
        }

        ptr->bSecondField = pContext->m_picLayerHeader->CurrField;
        ptr->bPicIntra = 0;
        ptr->bBidirectionalAveragingMode = 0x80;

        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicIntra = 1;
        }
        else if  (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
            ptr->bPicIntra = 1;
        }
        else if (VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
        }


        //iWMA
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,3,1,1);

        // intensity compensation
        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if (pContext->m_bIntensityCompensation)
                ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,1);

        }

        //bicubic as default
        ptr->bMVprecisionAndChromaRelation = 0x04;
        if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE)&&
            (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
        {
            if (VC1_MVMODE_HPELBI_1MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bMVprecisionAndChromaRelation |= 0x08;

        }

        if (pContext->m_seqLayerHeader.FASTUVMC)
            ptr->bMVprecisionAndChromaRelation |= 0x01;

        // 4:2:0
        ptr->bChromaFormat = 1;
        ptr->bPicScanFixed  = 1;
        ptr->bPicScanMethod = 0;
        ptr->bPicReadbackRequests = 0;
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bRcontrol = (BYTE)(pContext->m_picLayerHeader->RNDCTRL);
        else
            ptr->bRcontrol = (BYTE)(pContext->m_seqLayerHeader.RNDCTRL);

        ptr->bPicDeblocked = 0;

        //range reduction
        if (VC1_PROFILE_MAIN == pContext->m_seqLayerHeader.PROFILE &&
            pContext->m_seqLayerHeader.RANGERED)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked,5,1,pContext->m_picLayerHeader->RANGEREDFRM);
        }
        if (pContext->m_seqLayerHeader.LOOPFILTER && 
            pContext->DeblockInfo.isNeedDbl)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 1, 1, 1);
        }
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {

            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 2, 1, pContext->m_picLayerHeader->POSTPROC);
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 3, 1, pContext->m_picLayerHeader->POSTPROC);
        }


        if (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE)
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap

        if (VC1_PROFILE_MAIN == pContext->m_seqLayerHeader.PROFILE &&
            VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE &&
            pContext->m_seqLayerHeader.LOOPFILTER)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 7, 1, 1);
        }

        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 7, 1, pContext->m_seqLayerHeader.POSTPROCFLAG);
        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 6, 1, pContext->m_seqLayerHeader.PULLDOWN);
        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 5, 1, pContext->m_seqLayerHeader.INTERLACE);
        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 4, 1, pContext->m_seqLayerHeader.TFCNTRFLAG);
        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 3, 1, pContext->m_seqLayerHeader.FINTERPFLAG);
        // ToDO!!!!!!!!!
        if (VC1_PROFILE_ADVANCED != pContext->m_seqLayerHeader.PROFILE)
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 2, 1, 0); //RESERVED!!!!!!!!
        else
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 2, 1, 1);

        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 1, 1, 0); // PSF _MAY_BE need
        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 0, 1, pContext->m_seqLayerHeader.EXTENDED_DMV);

        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 7, 1, pContext->m_seqLayerHeader.PANSCAN_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 6, 1, pContext->m_seqLayerHeader.REFDIST_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 5, 1, pContext->m_seqLayerHeader.LOOPFILTER);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 4, 1, pContext->m_seqLayerHeader.FASTUVMC);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 3, 1, pContext->m_seqLayerHeader.EXTENDED_MV);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 1, 2, pContext->m_seqLayerHeader.DQUANT);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 0, 1, pContext->m_seqLayerHeader.VSTRANSFORM);

        ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 6, 2, pContext->m_seqLayerHeader.QUANTIZER);
        if (VC1_PROFILE_ADVANCED != pContext->m_seqLayerHeader.PROFILE)
        {
            ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 5, 1, pContext->m_seqLayerHeader.MULTIRES);
            ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 4, 1, pContext->m_seqLayerHeader.SYNCMARKER);
            ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 3, 1, pContext->m_seqLayerHeader.RANGERED);
            ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 0, 3, pContext->m_seqLayerHeader.MAXBFRAMES);
        }

        ptr->bPic4MVallowed = 0;
        if (VC1_FrameInterlace == pContext->m_picLayerHeader->FCM &&
            1 == pContext->m_picLayerHeader->MV4SWITCH &&
            VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPic4MVallowed = 1;
        }
        else if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE))
        {
            if (VC1_MVMODE_MIXED_MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bPic4MVallowed = 1;
        }
        ptr->wDecodedPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);

        // Menlow/EagleLake 
#if 1
        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
            pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG ||
            pContext->m_picLayerHeader->RANGEREDFRM)
        {
            //va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
            if(VC1_IS_REFERENCE(pContext->m_picLayerHeader->PTYPE))
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndex);
            else
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndexPrev);

        }
#endif 

        if (1 == ptr->bPicIntra)
        {
            ptr->wForwardRefPictureIndex = 0xFFFF;
            ptr->wBackwardRefPictureIndex = 0xFFFF;
        }
        else if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->wBackwardRefPictureIndex = 0xFFFF;
            if (VC1_FieldInterlace != pContext->m_picLayerHeader->FCM ||
                0 == pContext->m_picLayerHeader->CurrField)
            {
                ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            }
            else
            {
                if (1 == pContext->m_picLayerHeader->NUMREF)
                {
                    // first frame MAY_BE. Need to check
                    if (-1 != pContext->m_frmBuff.m_iPrevIndex)
                        ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                    else
                        ptr->wForwardRefPictureIndex = 0;

                }
                else if (1 == pContext->m_picLayerHeader->REFFIELD)
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                }
                else
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);
                }
            }
        }
        else
        {
            ptr->wForwardRefPictureIndex  = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            ptr->wBackwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iNextIndex);
        }

        //Range Mapping
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG)
            {
                ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 7, 1, pContext->m_seqLayerHeader.RANGE_MAPY_FLAG);
                ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 4, 3, pContext->m_seqLayerHeader.RANGE_MAPY);
            }
            if (pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG)
            {
                ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 3, 1,pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG);
                ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 0, 3, pContext->m_seqLayerHeader.RANGE_MAPUV);
            }
        }
        else if (VC1_PROFILE_MAIN == pContext->m_seqLayerHeader.PROFILE && pContext->m_picLayerHeader->RANGEREDFRM)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 7, 1, pContext->m_picLayerHeader->RANGEREDFRM);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 4, 3, 7);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 3, 1, pContext->m_picLayerHeader->RANGEREDFRM);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 0, 3, 7);
        }
        else
        {
            ptr->bPicOBMC = 0;
        }
        ptr->bPicBinPB = 0; // RESPIC - need to add
        ptr->bMV_RPS = 0;
        ptr->bReservedBits = BYTE(pContext->m_picLayerHeader->PQUANT);

        ptr->wBitstreamFcodes = 0;
        ptr->wBitstreamPCEelements = 0;

        if (pContext->m_bIntensityCompensation)
        {
            if (3 == ptr->bPicStructure)
            {
                ptr->wBitstreamFcodes = WORD(pContext->m_picLayerHeader->LUMSCALE + 32);
                ptr->wBitstreamPCEelements = WORD(pContext->m_picLayerHeader->LUMSHIFT);
            }
            else
            {
                if (0 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE1 + 32);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT1);

                    //bottom field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->m_picLayerHeader->LUMSCALE1 + 32);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->m_picLayerHeader->LUMSHIFT1);
                }
                else if (1 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE1 + 32);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT1);
                    // bottom field not compensated
                    ptr->wBitstreamFcodes |= 32;
                }
                else if (2 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    //MAY_BE. NEED TO CORRECT !!!!!!!!!!!!!!!!!!
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE1 + 32);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT1);
                    // bottom field not compensated
                    ptr->wBitstreamFcodes |= 32;
                }
            }
        }
        else
        {
            ptr->wBitstreamFcodes = 32;
            if (ptr->bPicStructure != 3)
            {
                ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, 32);
            }
        }
        ptr->bBitstreamConcealmentNeed = 0;
        ptr->bBitstreamConcealmentMethod = 0;

#ifdef VC1_DEBUG_ON
        for (int i=0; i < sizeof(DXVA_PictureParameters); i++ )
            printf("The byte %d\t equal %x\n",i,*((Ipp8u*)ptr+i));

        printf("New frame \n");

        printf("pParams->wDecodedPictureIndex =%d\n", ptr->wDecodedPictureIndex);
        printf("pParams->wDeblockedPictureIndex =%d\n", ptr->wDeblockedPictureIndex);
        printf("pParams->wForwardRefPictureIndex =%d\n", ptr->wForwardRefPictureIndex);
        printf("pParams->wBackwardRefPictureIndex =%d\n", ptr->wBackwardRefPictureIndex);
        printf("pParams->wPicWidthInMBminus1 =%d\n", ptr->wPicWidthInMBminus1);
        printf("pParams->wPicHeightInMBminus1 =%d\n", ptr->wPicHeightInMBminus1);
        printf("pParams->bMacroblockWidthMinus1 =%d\n", ptr->bMacroblockWidthMinus1);
        printf("pParams->bMacroblockHeightMinus1 =%d\n", ptr->bMacroblockHeightMinus1);
        printf("pParams->bBlockWidthMinus1 =%d\n", ptr->bBlockWidthMinus1);
        printf("pParams->bBlockHeightMinus1 =%d\n", ptr->bBlockWidthMinus1);
        printf("pParams->bBPPminus1  =%d\n", ptr->bBlockWidthMinus1);
        printf("pParams->bPicStructure  =%d\n", ptr->bPicStructure);
        printf("pParams->bSecondField  =%d\n", ptr->bSecondField);
        printf("pParams->bPicIntra  =%d\n", ptr->bPicIntra);
        printf("pParams->bPicBackwardPrediction  =%d\n", ptr->bPicBackwardPrediction);
        printf("pParams->bBidirectionalAveragingMode =%d\n", ptr->bBidirectionalAveragingMode);
        printf("pParams->bMVprecisionAndChromaRelation =%d\n", ptr->bMVprecisionAndChromaRelation);
        printf("pParams->bChromaFormat =%d\n", ptr->bChromaFormat);
        printf("pParams->bPicScanFixed =%d\n", ptr->bPicScanFixed);
        printf("pParams->bPicScanMethod =%d\n", ptr->bPicScanMethod);
        printf("pParams->bPicReadbackRequests =%d\n", ptr->bPicReadbackRequests);
        printf("pParams->bRcontrol  =%d\n", ptr->bRcontrol );
        printf("pParams->bPicSpatialResid8  =%d\n", ptr->bPicSpatialResid8);
        printf("pParams->bPicOverflowBlocks  =%d\n", ptr->bPicOverflowBlocks);
        printf("pParams->bPicExtrapolation  =%d\n", ptr->bPicExtrapolation);
        printf("pParams->bPicDeblocked  =%d\n", ptr->bPicDeblocked);
        printf("pParams->bPicDeblockConfined  =%d\n", ptr->bPicDeblockConfined);
        printf("pParams->bPic4MVallowed  =%d\n", ptr->bPic4MVallowed);
        printf("pParams->bPicOBMC  =%d\n", ptr->bPicOBMC);
        printf("pParams->bPicBinPB  =%d\n", ptr->bPicBinPB);
        printf("pParams->bMV_RPS   =%d\n", ptr->bMV_RPS);
        printf("pParams->bReservedBits   =%d\n", ptr->bReservedBits);
        printf("pParams->wBitstreamFcodes   =%d\n", ptr->wBitstreamFcodes);
        printf("pParams->wBitstreamPCEelements   =%d\n", ptr->wBitstreamPCEelements);
        printf("pParams->bBitstreamConcealmentNeed   =%d\n", ptr->bBitstreamConcealmentNeed);
        printf("pParams->bBitstreamConcealmentNeed   =%d\n", ptr->bBitstreamConcealmentMethod);
#endif
    }

    Ipp32u VC1PackerDXVA::VC1PackBitStreamSM (VC1Context* pContext,
        Ipp32u Size,
        Ipp8u* pOriginalData,
        Ipp32u ByteOffset,
        bool   isNeedToAddSC) // offset of the first byte of pbs in buffer
    {
        UMCVACompBuffer* CompBuf;
        Ipp8u* pBitstream = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);
        Ipp32u DrvBufferSize = CompBuf->GetBufferSize();
        Ipp32u RemainBytes = 0;

        if (DrvBufferSize < (Size + ByteOffset)) // we don't have enough buffer
        {
            RemainBytes = Size + ByteOffset - DrvBufferSize;
        }
        ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, Size - RemainBytes);
#ifdef VC1_DEBUG_ON
        printf("Start of Data buffer = %x\n",*((Ipp32u*)(pBitstream)));
#endif
        CompBuf->SetDataSize(ByteOffset + Size - RemainBytes);
        return RemainBytes;
    }
    Ipp32u VC1PackerDXVA::VC1PackBitStreamAdv (VC1Context* pContext,
        Ipp32u Size,
        Ipp8u* pOriginalData,
        Ipp32u ByteOffset)
    {
        UMCVACompBuffer* CompBuf;

        Ipp8u* pBitstream = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);
        //Ipp32u DrvBufferSize = 4000; //= CompBuf->GetBufferSize();
        Ipp32u DrvBufferSize = CompBuf->GetBufferSize();
        Ipp32u RemainBytes = 0;

        if (DrvBufferSize < (Size + ByteOffset)) // we don't have enough buffer
        {
            RemainBytes = Size + ByteOffset - DrvBufferSize;
        }
        ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, Size - RemainBytes);


#ifdef VC1_DEBUG_ON
        printf("Start of Data buffer = %x\n",*((Ipp32u*)(pBitstream)));
#endif
        CompBuf->SetDataSize(ByteOffset + Size - RemainBytes );
        // for correct .WMV files support. By now the driver can process only the whole frame/field
        return RemainBytes;
    }

    void VC1PackerDXVA::VC1PackBitplaneBuffers(VC1Context* pContext)
    {
        // There is no bitplanes in standard GUIDs
        /*
        UMCVACompBuffer* CompBuf;
        Ipp8u* ptr = 0;//(Ipp8u*)m_va->GetCompBuffer(DXVA_BITPLANE_BUFFER, &CompBuf);
        Ipp32s i;
        Ipp32s bitplane_size = pContext->m_seqLayerHeader.heightMB*pContext->m_seqLayerHeader.widthMB; //need to update for fields

        VC1Bitplane* lut_bitplane[3];
        VC1Bitplane* check_bitplane = 0;

        CompBuf->SetDataSize(bitplane_size);

        if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
        bitplane_size /= 2;

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
        lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
        lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
        lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
        break;
        case VC1_P_FRAME:
        lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
        lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
        lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
        break;
        case VC1_B_FRAME:
        lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
        lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
        lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
        break;
        case VC1_SKIPPED_FRAME:
        return;
        }
        for (i=0;i<3;i++)
        {
        if (lut_bitplane[i]->m_databits)
        check_bitplane = lut_bitplane[i];

        }
        if (check_bitplane)
        {
        for (i=0;i<3;i++)
        {
        if (!lut_bitplane[i]->m_databits)
        lut_bitplane[i] = check_bitplane;
        }

        for (i=0; i < bitplane_size;)
        {
        *ptr = lut_bitplane[0]->m_databits[i] + (lut_bitplane[1]->m_databits[i] << 1) +
        (lut_bitplane[2]->m_databits[i] << 2) + (lut_bitplane[0]->m_databits[i+1] << 4) +
        (lut_bitplane[1]->m_databits[i+1] << 5) + (lut_bitplane[2]->m_databits[i+1] << 6);
        i += 2;
        ++ptr;
        }
        }
        */
    }



    void VC1PackerDXVA::VC1PackBitStreamForOneSlice (VC1Context* pContext, Ipp32u Size)
    {
        UMCVACompBuffer* CompBuf;
        Ipp8u* pSliceData = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER,&CompBuf);
        ippsCopy_8u((Ipp8u*)(pContext->m_bitstream.pBitstream-1),pSliceData,Size + 4);
        CompBuf->SetDataSize(Size + 4);
        SwapData((Ipp8u*)pSliceData, Size + 4);
    }

#endif


#if defined (UMC_VA_DXVA) && (UMC_VA_DXVA_SA)
    void VC1PackerSA::VC1SetSliceBuffer()
    {
        UMCVACompBuffer* CompBuf;
        m_pSliceInfo = (DXVA_SliceInfoVC1*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf,VC1SLICEINPARAL*sizeof(DXVA_SliceInfoVC1));
        if (CompBuf->GetBufferSize() < VC1SLICEINPARAL*sizeof(DXVA_SliceInfoVC1))
            throw vc1_exception(mem_allocation_er);
        memset(m_pSliceInfo,0,sizeof(DXVA_SliceInfoVC1));
    }
    void VC1PackerSA::VC1SetPictureBuffer()
    {
        UMCVACompBuffer* CompBuf;
        m_pPicPtr = (DXVA_PicParams_VC1*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &CompBuf,VC1SLICEINPARAL*sizeof(DXVA_PicParams_VC1));
        if (CompBuf->GetBufferSize() < VC1SLICEINPARAL*sizeof(DXVA_PicParams_VC1))
            throw vc1_exception(mem_allocation_er);
        memset(m_pPicPtr,0,sizeof(DXVA_PicParams_VC1));
    }
    //to support multislice mode. Save number of slices value in reserved bits of first slice
    void VC1PackerSA::VC1SetBuffersSize(Ipp32u SliceBufIndex, Ipp32u PicBuffIndex)
    {
        UMCVACompBuffer* CompBuf;
        DXVA_SliceInfoVC1* pSliceInfo = (DXVA_SliceInfoVC1*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        pSliceInfo->bReservedBits = (BYTE)SliceBufIndex; //need to fix - maximum slice number in frame = 512
        CompBuf->SetDataSize(SliceBufIndex*sizeof(DXVA_SliceInfoVC1));
        m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER,&CompBuf);
        CompBuf->SetDataSize(PicBuffIndex*sizeof(DXVA_SliceInfoVC1));
    }

    void VC1PackerSA::VC1PackOneSlice    (VC1Context* pContext,
        SliceParams* slparams,
        Ipp32u BufIndex, // only in future realisations
        Ipp32u MBOffset,
        Ipp32u SliceDataSize,
        Ipp32u StartCodeOffset)
    {
        memset(m_pSliceInfo, 0, sizeof(DXVA_SliceInfoVC1));
        m_pSliceInfo->bStartCodeBitOffset = (WORD)((31-(BYTE)slparams->m_bitOffset) + MBOffset*8 + 32); //32 - Start Code
        m_pSliceInfo->dwSliceDataLocation = StartCodeOffset;
        m_pSliceInfo->dwSliceBitsInBuffer = SliceDataSize*8 - m_pSliceInfo->bStartCodeBitOffset;
        m_pSliceInfo->wVerticalPosition = slparams->MBStartRow;
        m_pSliceInfo->wNumberMBsInSlice = (WORD)((slparams->MBEndRow-slparams->MBStartRow)*pContext->m_seqLayerHeader.widthMB);
        m_pSliceInfo->wPictureIndex = (WORD)BufIndex;
        ++m_pSliceInfo;
    }
    void VC1PackerSA::VC1PackWholeSliceSM  (VC1Context* pContext,
        Ipp32u MBOffset,
        Ipp32u SliceDataSize)
    {
        memset(m_pSliceInfo, 0, sizeof(DXVA_SliceInfoVC1));
        m_pSliceInfo->bStartCodeBitOffset =(WORD)((31-(BYTE)pContext->m_bitstream.bitOffset) + MBOffset*8); //32 - Start Code
        m_pSliceInfo->dwSliceDataLocation = 0;
        m_pSliceInfo->dwSliceBitsInBuffer = SliceDataSize*8;
        m_pSliceInfo->wVerticalPosition = 0;
        m_pSliceInfo->wNumberMBsInSlice = pContext->m_seqLayerHeader.widthMB*pContext->m_seqLayerHeader.heightMB;
        m_pSliceInfo->wPictureIndex = 0;
        ++m_pSliceInfo;
    }
    void VC1PackerSA::VC1PackPicParamsForOneSlice (VC1Context* pContext)
    {
        DXVA_PicParams_VC1* ptr = m_pPicPtr;
        memset(ptr,0,sizeof(DXVA_PicParams_VC1));
        ptr->wDecodedPictureIndex     = pContext->m_frmBuff.m_iCurrIndex;
        ptr->wDeblockedPictureIndex   = pContext->m_frmBuff.m_iCurrIndex;
        ptr->wForwardRefPictureIndex  = pContext->m_frmBuff.m_iPrevIndex;
        ptr->wBackwardRefPictureIndex = pContext->m_frmBuff.m_iNextIndex;
        ptr->wPicWidthInMBminus1  = (WORD)pContext->m_seqLayerHeader.widthMB  - 1;
        ptr->wPicHeightInMBminus1 = (WORD)pContext->m_seqLayerHeader.heightMB - 1;
        ptr->bMacroblockWidthMinus1  = 15;
        ptr->bMacroblockHeightMinus1 = 15;
        ptr->bBlockWidthMinus1       = 7;
        ptr->bBlockHeightMinus1      = 7;
        ptr->MaxCodedWidth           = (WORD)pContext->m_seqLayerHeader.MAX_CODED_WIDTH;
        ptr->MaxCodedHeight          = (WORD)pContext->m_seqLayerHeader.MAX_CODED_HEIGHT;
        ptr->Pulldown                = (WORD)pContext->m_seqLayerHeader.PULLDOWN;
        ptr->ScaleFactor             = (WORD)pContext->m_picLayerHeader->ScaleFactor;


        ptr->bPicStructure = 0x03;    //Frame
        ptr->bPicExtrapolation = 0x01; //Progressive

        if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace )
        {
            ptr->bPicExtrapolation = 0x2;
            if (pContext->m_picLayerHeader->TFF)
                ptr->bPicStructure = 0x01;
            else
                ptr->bPicStructure = 0x02;
        }

        if (pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
            ptr->bPicExtrapolation = 0x02;


        //need to define
        ptr->VariableSizedTransformFlag = pContext->m_seqLayerHeader.VSTRANSFORM;
        ptr->MbLevelTransformTypeFlag   = pContext->m_picLayerHeader->TTMBF;
        ptr->FrameLevelTransformType    = pContext->m_picLayerHeader->TTFRM_ORIG;
        ptr->ClosedEntry = pContext->m_seqLayerHeader.CLOSED_ENTRY;
        ptr->BrokenLink  = pContext->m_seqLayerHeader.BROKEN_LINK;
        ptr->ConditionalOverlapFlag = pContext->m_picLayerHeader->CONDOVER;
        ptr->FastUVMCflag = pContext->m_seqLayerHeader.FASTUVMC;
        ptr->Profile      = pContext->m_seqLayerHeader.PROFILE;
        ptr->Bfraction    = pContext->m_picLayerHeader->BFRACTION;
        ptr->PictureType  = pContext->m_picLayerHeader->PTYPE;
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 5, 1, pContext->m_seqLayerHeader.LOOPFILTER); // loopfilter

        ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 5, 1, (pContext->m_picLayerHeader->RANGEREDFRM >> 3)); // range red for simple/main
        ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap

        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 6, 1,pContext->m_seqLayerHeader.OVERLAP); // overlap
        ptr->bRcontrol = (BYTE)pContext->m_picLayerHeader->RNDCTRL;
        //ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 7, 1, pContext->m_seqLayerHeader.RANGE_MAPY_FLAG);
        if (pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 7, 1, pContext->m_seqLayerHeader.RANGE_MAPY_FLAG);
            if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG)
                ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 4, 3, pContext->m_seqLayerHeader.RANGE_MAPY);

            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 3, 1,pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG);
            if (pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG)
                ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 0, 3, pContext->m_seqLayerHeader.RANGE_MAPUV);
        }
        else if (pContext->m_picLayerHeader->RANGEREDFRM)//modify i/f by varistar
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 0, 3,*pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].pRANGE_MAPY);


        ptr->bSecondField = pContext->m_picLayerHeader->CurrField;
        ptr->bPicIntra = 0;
        ptr->bBidirectionalAveragingMode = 0x80;

        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicIntra = 1;
        }
        else if  (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
            ptr->bPicIntra = 1;
        }
        else if (VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
        }

        //intensity compensation
        ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,pContext->m_bIntensityCompensation);
        if (pContext->m_bIntensityCompensation)
        {
            if (0x03 == ptr->bPicStructure) //frame
            {
                ptr->wBitstreamFcodes = (WORD)pContext->m_picLayerHeader->LUMSCALE;
                ptr->wBitstreamPCEelements = (WORD)pContext->m_picLayerHeader->LUMSHIFT;
            }
        }




        //ptr->wBitFields1 = 0;
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 0, 2, pContext->m_picLayerHeader->TRANSACFRM);
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 2, 2, pContext->m_picLayerHeader->TRANSACFRM2);
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 4, 1, pContext->m_picLayerHeader->TRANSDCTAB);
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 5, 2, pContext->m_picLayerHeader->MVMODE);
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 7, 2, pContext->m_picLayerHeader->MVMODE2);
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 9, 3, pContext->m_picLayerHeader->MVTAB);
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 12, 3, pContext->m_picLayerHeader->CBPTAB);
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 15, 3, pContext->m_picLayerHeader->MBMODETAB);
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 18, 2, pContext->m_picLayerHeader->MV2BPTAB);
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 20, 1, pContext->m_picLayerHeader->MV4SWITCH);
        ptr->wBitFields1 = bit_set(ptr->wBitFields1, 21, 2, pContext->m_picLayerHeader->MV4BPTAB);





        // need to add parameters for tables we haven't it now
        //ptr->wBitFields2 = 0;
        ptr->wBitFields2 = bit_set(ptr->wBitFields2, 0, 2, pContext->m_seqLayerHeader.DQUANT);
        ptr->wBitFields2 = bit_set(ptr->wBitFields2, 2, 1, pContext->m_picLayerHeader->HALFQP);
        ptr->wBitFields2 = bit_set(ptr->wBitFields2, 3, 5, pContext->m_picLayerHeader->PQUANT); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! not one bit!!!!!!
        ptr->wBitFields2 = bit_set(ptr->wBitFields2, 8, 1, pContext->m_picLayerHeader->PQUANTIZER);
        ptr->wBitFields2 = bit_set(ptr->wBitFields2, 9, 1, pContext->m_picLayerHeader->m_DQuantFRM);
        ptr->wBitFields2 = bit_set(ptr->wBitFields2, 10, 2, pContext->m_picLayerHeader->m_DQProfile);
        ptr->wBitFields2 = bit_set(ptr->wBitFields2, 12, 1, pContext->m_picLayerHeader->m_DQBILevel);
        ptr->wBitFields2 = bit_set(ptr->wBitFields2, 13, 5, pContext->m_picLayerHeader->m_AltPQuant);
        ptr->wBitFields2 = bit_set(ptr->wBitFields2, 18, 5 ,pContext->m_picLayerHeader->PQINDEX);
        ptr->wBitFields2 = bit_set(ptr->wBitFields2, 23, 2 ,pContext->m_picLayerHeader->DQSBEdge);


        //ptr->wBitFields3 = 0;
        ptr->wBitFields3 = (UCHAR)bit_set(ptr->wBitFields3, 0, 1, pContext->m_seqLayerHeader.REFDIST_FLAG);
        ptr->wBitFields3 = (UCHAR)bit_set(ptr->wBitFields3, 1, 1, pContext->m_picLayerHeader->REFDIST);
        ptr->wBitFields3 = (UCHAR)bit_set(ptr->wBitFields3, 2, 1, pContext->m_picLayerHeader->NUMREF);
        ptr->wBitFields3 = (UCHAR)bit_set(ptr->wBitFields3, 3, 1, pContext->m_picLayerHeader->REFFIELD);


        //ptr->wBitFields4 = 0;
        ptr->wBitFields4 = (UCHAR)bit_set(ptr->wBitFields4, 0, 1, pContext->m_seqLayerHeader.EXTENDED_MV);
        ptr->wBitFields4 = (UCHAR)bit_set(ptr->wBitFields4, 1, 2, pContext->m_picLayerHeader->MVRANGE);
        ptr->wBitFields4 = (UCHAR)bit_set(ptr->wBitFields4, 3, 1, pContext->m_seqLayerHeader.EXTENDED_DMV);
        ptr->wBitFields4 = (UCHAR)bit_set(ptr->wBitFields4, 4, 2, pContext->m_picLayerHeader->DMVRANGE);

        //ptr->RawCodingFlag = 0;
        if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->MVTYPEMB))
            ptr->RawCodingFlag |= 1;
        if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->m_DirectMB))
            ptr->RawCodingFlag |= 1 << 1;
        if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->SKIPMB))
            ptr->RawCodingFlag |= 1 << 2;
        if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FIELDTX))
            ptr->RawCodingFlag |= 1 << 3;
        if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FORWARDMB))
            ptr->RawCodingFlag |= 1 << 4;
        if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->ACPRED))
            ptr->RawCodingFlag |= 1 << 5;
        if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->OVERFLAGS))
            ptr->RawCodingFlag |= 1 << 6;

        ++m_pPicPtr;
    }
    void VC1PackerSA::VC1PackBitStreamSM (VC1Context* pContext)
    {
        UMCVACompBuffer* CompBuf;
        Ipp8u* pBitstream = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);
        CompBuf->SetDataSize((pContext->m_FrameSize & 0xFFFFFFF8) + 8);
        ippsCopy_8u((Ipp8u*)(pContext->m_bitstream.pBitstream),pBitstream,(pContext->m_FrameSize & 0xFFFFFFF8) + 8 + 4); // (bufferSize & 0xFFFFFFF8) + 8 - skip frames
    }
    void VC1PackerSA::VC1PackBitStreamAdv (VC1Context* pContext,Ipp32u Size)
    {
        UMCVACompBuffer* CompBuf;
        Ipp8u* pBitstream = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);
        CompBuf->SetDataSize((pContext->m_FrameSize & 0xFFFFFFF8) + 4);
        ippsCopy_8u((Ipp8u*)(pContext->m_bitstream.pBitstream-1),pBitstream,(pContext->m_FrameSize & 0xFFFFFFF8) + 8 + 4); // (bufferSize & 0xFFFFFFF8) + 8 - skip frames
    }

    void VC1PackerSA::VC1PackBitplaneBuffers(VC1Context* pContext)
    {
        UMCVACompBuffer* CompBuf;
        Ipp8u* ptr = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITPLANE_BUFFER, &CompBuf);
        int i;
        int bitplane_size = pContext->m_seqLayerHeader.heightMB*pContext->m_seqLayerHeader.widthMB; //need to update for fields

        VC1Bitplane* lut_bitplane[3];
        VC1Bitplane* check_bitplane = 0;

        CompBuf->SetDataSize(bitplane_size);

        if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
            bitplane_size /= 2;

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        case VC1_SKIPPED_FRAME:
            return;
        }
        for (i=0;i<3;i++)
        {
            if (lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }
        if (check_bitplane)
        {
            for (i=0;i<3;i++)
            {
                if (!lut_bitplane[i]->m_databits)
                    lut_bitplane[i] = check_bitplane;
            }

            for (i=0; i < bitplane_size;)
            {
                *ptr = lut_bitplane[0]->m_databits[i] + (lut_bitplane[1]->m_databits[i] << 1) +
                    (lut_bitplane[2]->m_databits[i] << 2) + (lut_bitplane[0]->m_databits[i+1] << 4) +
                    (lut_bitplane[1]->m_databits[i+1] << 5) + (lut_bitplane[2]->m_databits[i+1] << 6);
                i += 2;
                ++ptr;
            }
        }
    }





    Ipp32u VC1UnPackerSA::VC1GetNumOfSlices()
    {
        DXVA_SliceInfoVC1* pSliceInfo = (DXVA_SliceInfoVC1*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER);
        return pSliceInfo->bReservedBits;
    }
    Ipp8u*  VC1UnPackerSA::VC1GetBitsream()
    {
        return (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER);
    }

    void VC1UnPackerSA::VC1UnPackPicParams(VC1Context* pContext, Ipp32u picIndex)
    {
        UMCVACompBuffer* CompBuf;
        DXVA_PicParams_VC1* ptr = (DXVA_PicParams_VC1*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &CompBuf);
        ptr += picIndex;

        pContext->m_frmBuff.m_iCurrIndex = ptr->wDecodedPictureIndex;
        pContext->m_frmBuff.m_iPrevIndex = ptr->wForwardRefPictureIndex;
        pContext->m_frmBuff.m_iNextIndex = ptr->wBackwardRefPictureIndex;

        *pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].pRANGE_MAPY = -1; // to check filling


        pContext->m_seqLayerHeader.widthMB = ptr->wPicWidthInMBminus1 + 1;
        pContext->m_seqLayerHeader.heightMB = ptr->wPicHeightInMBminus1 + 1;

        pContext->m_seqLayerHeader.MAX_CODED_WIDTH = ptr->MaxCodedWidth;
        pContext->m_seqLayerHeader.MAX_CODED_HEIGHT = ptr->MaxCodedHeight;
        pContext->m_picLayerHeader->ScaleFactor = ptr->ScaleFactor;
        //        pContext->m_picLayerHeader->FCM = ptr->FrameCodingMode;


        pContext->m_picLayerHeader->POSTPROC = 1;
        pContext->m_seqLayerHeader.PROFILE = ptr->Profile;

        pContext->m_picLayerHeader->RANGEREDFRM =  bit_get(ptr->bPicDeblocked, 5, 1); // range red for simple/main


        if (pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
        {
            pContext->m_seqLayerHeader.RANGE_MAPY_FLAG = bit_get(ptr->bPicOBMC, 7, 1);
            if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG)
            {
                pContext->m_seqLayerHeader.RANGE_MAPY = bit_get(ptr->bPicOBMC, 4, 3);
            }
            else
                pContext->m_seqLayerHeader.RANGE_MAPY = -1;

            pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG = bit_get(ptr->bPicOBMC, 3, 1);
            if (pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG)
            {
                pContext->m_seqLayerHeader.RANGE_MAPUV = bit_get(ptr->bPicOBMC, 0, 3);
            }
            else
                pContext->m_seqLayerHeader.RANGE_MAPUV = -1;

        }
        else //modify i/f by varistar
        {
            if (pContext->m_picLayerHeader->RANGEREDFRM)
            {
                *pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].pRANGE_MAPY = bit_get(ptr->bPicOBMC, 0, 3);
            }
            else
                *pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iCurrIndex].pRANGE_MAPY = -1;

        }




        pContext->m_seqLayerHeader.VSTRANSFORM = ptr->VariableSizedTransformFlag;
        pContext->m_picLayerHeader->TTMBF = ptr->MbLevelTransformTypeFlag;
        pContext->m_picLayerHeader->TTFRM = ptr->FrameLevelTransformType;
        pContext->m_seqLayerHeader.CLOSED_ENTRY = ptr->ClosedEntry;
        pContext->m_seqLayerHeader.BROKEN_LINK = ptr->BrokenLink;
        pContext->m_picLayerHeader->CONDOVER = ptr->ConditionalOverlapFlag;
        pContext->m_seqLayerHeader.FASTUVMC = ptr->FastUVMCflag;
        pContext->m_picLayerHeader->BFRACTION = ptr->Bfraction; //need to define
        pContext->m_picLayerHeader->PTYPE = ptr->PictureType;

        if (ptr->PictureType == VC1_B_FRAME)
            pContext->m_frmBuff.m_iDisplayIndex = ptr->wDecodedPictureIndex;


        if (0x03 == ptr->bPicStructure)    //Frame
        {
            if (0x1 == ptr->bPicExtrapolation)
                pContext->m_picLayerHeader->FCM = VC1_Progressive;
            else if (0x2 == ptr->bPicExtrapolation)
                pContext->m_picLayerHeader->FCM = VC1_FrameInterlace;
        } else if (0x01 == ptr->bPicStructure)
        {
            pContext->m_picLayerHeader->FCM = VC1_FieldInterlace;
            pContext->m_picLayerHeader->TFF = 1;
        }
        else
        {
            pContext->m_picLayerHeader->FCM = VC1_FieldInterlace;
            pContext->m_picLayerHeader->TFF = 0;
        }
        if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
            pContext->m_picLayerHeader->CurrField = ptr->bSecondField;

        pContext->m_seqLayerHeader.LOOPFILTER = bit_get(ptr->bPicSpatialResid8, 5, 1); // loopfilter
        pContext->m_picLayerHeader->RNDCTRL = ptr->bRcontrol;
        pContext->m_seqLayerHeader.OVERLAP = bit_get(ptr->bPicDeblocked, 6, 1); // overlap


        pContext->m_bIntensityCompensation = bit_get(ptr->bBidirectionalAveragingMode,4,1);
        if (pContext->m_bIntensityCompensation)
        {
            if (0x03 == ptr->bPicStructure) //frame
            {
                pContext->m_picLayerHeader->LUMSCALE = ptr->wBitstreamFcodes;
                pContext->m_picLayerHeader->LUMSHIFT = ptr->wBitstreamPCEelements;
                FillTablesForIntensityCompensation(pContext, pContext->m_picLayerHeader->LUMSCALE,pContext->m_picLayerHeader->LUMSHIFT);
            }
        }

        pContext->m_picLayerHeader->TRANSACFRM = bit_get(ptr->wBitFields1, 0, 2);
        pContext->m_picLayerHeader->TRANSACFRM2 = bit_get(ptr->wBitFields1, 2, 2);
        pContext->m_picLayerHeader->TRANSDCTAB = bit_get(ptr->wBitFields1, 4, 1);
        pContext->m_picLayerHeader->MVMODE = bit_get(ptr->wBitFields1, 5, 2);
        pContext->m_picLayerHeader->MVMODE2 = bit_get(ptr->wBitFields1, 7, 2);
        pContext->m_picLayerHeader->MVTAB = bit_get(ptr->wBitFields1, 9, 3);
        pContext->m_picLayerHeader->CBPTAB = bit_get(ptr->wBitFields1, 12, 3);
        pContext->m_picLayerHeader->MBMODETAB = bit_get(ptr->wBitFields1, 15, 3);
        pContext->m_picLayerHeader->MV2BPTAB = bit_get(ptr->wBitFields1, 18, 2);
        pContext->m_picLayerHeader->MV4SWITCH = bit_get(ptr->wBitFields1, 20, 1);
        pContext->m_picLayerHeader->MV4BPTAB = bit_get(ptr->wBitFields1, 21, 2);


        pContext->m_seqLayerHeader.DQUANT = bit_get(ptr->wBitFields2, 0, 2);
        pContext->m_picLayerHeader->HALFQP = bit_get(ptr->wBitFields2, 2, 1);
        pContext->m_picLayerHeader->PQUANT = bit_get(ptr->wBitFields2, 3, 5); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! not one bit!!!!!!
        pContext->m_picLayerHeader->PQUANTIZER = bit_get(ptr->wBitFields2, 8, 1);
        pContext->m_picLayerHeader->m_DQuantFRM = bit_get(ptr->wBitFields2, 9, 1);
        pContext->m_picLayerHeader->m_DQProfile = bit_get(ptr->wBitFields2, 10, 2);
        pContext->m_picLayerHeader->m_DQBILevel = bit_get(ptr->wBitFields2, 12, 1);
        pContext->m_picLayerHeader->m_AltPQuant = bit_get(ptr->wBitFields2, 13, 5);
        pContext->m_picLayerHeader->PQINDEX = bit_get(ptr->wBitFields2, 18, 5);
        pContext->m_picLayerHeader->DQSBEdge = bit_get(ptr->wBitFields2, 23, 2 );



        pContext->m_seqLayerHeader.REFDIST_FLAG = bit_get(ptr->wBitFields3, 0, 1);
        pContext->m_picLayerHeader->REFDIST = bit_get(ptr->wBitFields3, 1, 1);
        pContext->m_picLayerHeader->NUMREF =  bit_get(ptr->wBitFields3, 2, 1);
        pContext->m_picLayerHeader->REFFIELD= bit_get(ptr->wBitFields3, 3, 1);

        pContext->m_seqLayerHeader.EXTENDED_MV = bit_get(ptr->wBitFields4, 0, 1);
        pContext->m_picLayerHeader->MVRANGE = bit_get(ptr->wBitFields4, 1, 2);
        pContext->m_seqLayerHeader.EXTENDED_DMV = bit_get(ptr->wBitFields4, 3, 1);
        pContext->m_picLayerHeader->DMVRANGE = bit_get(ptr->wBitFields4, 4, 2);


        if (ptr->RawCodingFlag & 1)
            pContext->m_picLayerHeader->MVTYPEMB.m_imode = VC1_BITPLANE_RAW_MODE;
        else
            pContext->m_picLayerHeader->MVTYPEMB.m_imode = VC1_BITPLANE_NORM2_MODE; // NO RAW MODE!!
        if (ptr->RawCodingFlag & 2)
            pContext->m_picLayerHeader->m_DirectMB.m_imode = VC1_BITPLANE_RAW_MODE;
        else
            pContext->m_picLayerHeader->m_DirectMB.m_imode = VC1_BITPLANE_NORM2_MODE;
        if (ptr->RawCodingFlag & 4)
            pContext->m_picLayerHeader->SKIPMB.m_imode = VC1_BITPLANE_RAW_MODE;
        else
            pContext->m_picLayerHeader->SKIPMB.m_imode = VC1_BITPLANE_NORM2_MODE;
        if (ptr->RawCodingFlag & 8)
            pContext->m_picLayerHeader->FIELDTX.m_imode = VC1_BITPLANE_RAW_MODE;
        else
            pContext->m_picLayerHeader->FIELDTX.m_imode = VC1_BITPLANE_NORM2_MODE;
        if (ptr->RawCodingFlag & 16)
            pContext->m_picLayerHeader->FORWARDMB.m_imode = VC1_BITPLANE_RAW_MODE;
        else
            pContext->m_picLayerHeader->FORWARDMB.m_imode = VC1_BITPLANE_NORM2_MODE;
        if (ptr->RawCodingFlag & 32)
            pContext->m_picLayerHeader->ACPRED.m_imode = VC1_BITPLANE_RAW_MODE;
        else
            pContext->m_picLayerHeader->ACPRED.m_imode = VC1_BITPLANE_NORM2_MODE;
        if (ptr->RawCodingFlag & 64)
            pContext->m_picLayerHeader->OVERFLAGS.m_imode = VC1_BITPLANE_RAW_MODE;
        else
            pContext->m_picLayerHeader->OVERFLAGS.m_imode = VC1_BITPLANE_NORM2_MODE;
    }
    void VC1UnPackerSA::VC1UnPackBitplaneBuffers(VC1Context* pContext)
    {
        Ipp8u* ptr = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITPLANE_BUFFER);
        int i;
        int bitplane_size = pContext->m_seqLayerHeader.heightMB*pContext->m_seqLayerHeader.widthMB; //need to update for fields

        VC1Bitplane* lut_bitplane[3];

        if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
            bitplane_size /= 2;

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        case VC1_SKIPPED_FRAME:
            return;
        }
        lut_bitplane[0]->m_databits = pContext->m_pBitplane.m_databits;
        lut_bitplane[1]->m_databits = pContext->m_pBitplane.m_databits + pContext->m_seqLayerHeader.heightMB*
            pContext->m_seqLayerHeader.widthMB;
        lut_bitplane[2]->m_databits = pContext->m_pBitplane.m_databits +
            pContext->m_seqLayerHeader.heightMB*
            pContext->m_seqLayerHeader.widthMB*2;

        Ipp32s bitplane_offset = 0;
        for (i=0; i < bitplane_size; i++)
        {
            if (lut_bitplane[0])
                lut_bitplane[0]->m_databits[i] = (Ipp8u)((ptr[i/2] & ((1<<(bitplane_offset*4)) )) >> (bitplane_offset*4));
            if (lut_bitplane[1])
                lut_bitplane[1]->m_databits[i] = (Ipp8u)((ptr[i/2] & ((1<<(1+bitplane_offset*4)) )) >> (1+bitplane_offset*4));
            if (lut_bitplane[2])
                lut_bitplane[2]->m_databits[i] = (Ipp8u)((ptr[i/2] & ((1<<(2+bitplane_offset*4)) )) >> (2+bitplane_offset*4));
            bitplane_offset = ! bitplane_offset;
        }
    }
    void VC1UnPackerSA::VC1SetSliceBuffer()
    {
        m_pSliceInfo = (DXVA_SliceInfoVC1*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER);
    }
    //return Picture Index
    Ipp32u VC1UnPackerSA::VC1UnPackOneSlice(VC1Context* pContext, SliceParams* slparams)
    {
        // MAY_BE. NEED TO THINK
        Ipp32u picIndex = 0;//m_pSliceInfo->wPictureIndex;
        Ipp32u MBoffset = m_pSliceInfo->bStartCodeBitOffset/32;
        slparams->m_bitOffset = 31 - (m_pSliceInfo->bStartCodeBitOffset - MBoffset*32);
        slparams->m_pstart = (Ipp32u*)((Ipp8u*)pContext->m_bitstream.pBitstream + m_pSliceInfo->dwSliceDataLocation);
        slparams->m_pstart += MBoffset;

        slparams->MBStartRow = m_pSliceInfo->wVerticalPosition;
        slparams->MBRowsToDecode = (Ipp16u)(m_pSliceInfo->wNumberMBsInSlice/pContext->m_seqLayerHeader.widthMB);
        slparams->MBEndRow = m_pSliceInfo->wVerticalPosition + slparams->MBRowsToDecode;
        ++m_pSliceInfo;
        return picIndex;

    };
#endif

#if defined (UMC_VA_LINUX)

    enum
    {
        VC1_I_I_FRAME   = 0,
        VC1_I_P_FRAME   = 1,
        VC1_P_I_FRAME   = 2,
        VC1_P_P_FRAME   = 3,
        VC1_B_B_FRAME   = 4,
        VC1_B_BI_FRAME  = 5,
        VC1_BI_B_FRAME  = 6,
        VC1_BI_BI_FRAME = 7
    };

    void VC1PackerLVA::VC1SetSliceDataBuffer(Ipp32s size)
    {
        UMCVACompBuffer* pCompBuf;
        Ipp8u* ptr = (Ipp8u*)m_va->GetCompBuffer(VASliceDataBufferType, &pCompBuf, size);
        if (!pCompBuf || (pCompBuf->GetBufferSize() < size))
            throw vc1_exception(mem_allocation_er);
        memset(ptr, 0, size);
    }
    void VC1PackerLVA::VC1SetBitplaneBuffer(Ipp32s size)
    {
        UMCVACompBuffer* CompBuf;
        Ipp8u* ptr = (Ipp8u*)m_va->GetCompBuffer(VABitPlaneBufferType, &CompBuf, size);
        if (CompBuf->GetBufferSize() < size)
            throw vc1_exception(mem_allocation_er);
        memset(ptr, 0, size);
    }

    void VC1PackerLVA::VC1SetSliceParamBuffer(Ipp32u* pOffsets, Ipp32u* pValues)
    {
        const Ipp32u Slice = 0x0B010000;
        const Ipp32u Field = 0x0C010000;
        Ipp32s slice_counter = 1;

        pOffsets++;
        pValues++;
        while (*pOffsets)
        {
            if (pValues && (*pValues == Slice || *pValues == Field))
            {
                ++slice_counter;
            }
            ++pOffsets;
            ++pValues;
        }

        UMCVACompBuffer* pCompBuf;
        m_pSliceInfo = (VASliceParameterBufferVC1*)m_va->GetCompBuffer(VASliceParameterBufferType, &pCompBuf, slice_counter * sizeof(VASliceParameterBufferVC1));
        if (!pCompBuf || (static_cast<unsigned int>(pCompBuf->GetBufferSize()) < slice_counter * sizeof(VASliceParameterBufferVC1)))
            throw vc1_exception(mem_allocation_er);
        memset(m_pSliceInfo, 0, slice_counter * sizeof(VASliceParameterBufferVC1));
    }

    void VC1PackerLVA::VC1SetPictureBuffer()
    {
        UMCVACompBuffer* pCompBuf;
        m_pPicPtr = (VAPictureParameterBufferVC1*)m_va->GetCompBuffer(VAPictureParameterBufferType, &pCompBuf,sizeof(VAPictureParameterBufferVC1));
        if (!pCompBuf || (static_cast<unsigned int>(pCompBuf->GetBufferSize()) < sizeof(VAPictureParameterBufferVC1)))
            throw vc1_exception(mem_allocation_er);
        memset(m_pPicPtr, 0, sizeof(VAPictureParameterBufferType));
    }

    //to support multislice mode. Save number of slices value in reserved bits of first slice
    void VC1PackerLVA::VC1SetBuffersSize(Ipp32u SliceBufIndex, Ipp32u PicBuffIndex)
    {
        UMCVACompBuffer* CompBuf;
        m_va->GetCompBuffer(VASliceParameterBufferType,&CompBuf);
        CompBuf->SetDataSize(SliceBufIndex*sizeof(VASliceParameterBufferVC1));
#ifdef PRINT_VA_DEBUG
        printf("SliceBufIndex = %d \n",SliceBufIndex);
#endif
        CompBuf->SetNumOfItem(SliceBufIndex);
        m_va->GetCompBuffer(VAPictureParameterBufferType,&CompBuf);
        CompBuf->SetDataSize(sizeof(VAPictureParameterBufferVC1));
    }

    void VC1PackerLVA::VC1PackBitplaneBuffers(VC1Context* pContext)
    {
        UMCVACompBuffer* CompBuf;
        Ipp32s i;
        Ipp32s bitplane_size = 0;
        Ipp32s real_bitplane_size = 0;
        VC1Bitplane* lut_bitplane[3];
        VC1Bitplane* check_bitplane = NULL;

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        case VC1_SKIPPED_FRAME:
        default:
            return;
        }

        for (i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }
        if(check_bitplane)
        {

            bitplane_size = ((pContext->m_seqLayerHeader.heightMB+1)/2)*pContext->m_seqLayerHeader.widthMB;
            real_bitplane_size = pContext->m_seqLayerHeader.heightMB * pContext->m_seqLayerHeader.widthMB;
            if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
                bitplane_size /= 2;

            Ipp8u* ptr = (Ipp8u*)m_va->GetCompBuffer(VABitPlaneBufferType, &CompBuf, bitplane_size);
            if (!ptr)
                throw vc1_exception(mem_allocation_er);
            memset(ptr, 0, bitplane_size);
            for (i = 0; i < 3; i++)
            {
                if (!lut_bitplane[i]->m_databits)
                    lut_bitplane[i] = check_bitplane;
            }

            for (i = 0; i < real_bitplane_size - (real_bitplane_size & 0x1);)
            {
                *ptr = (lut_bitplane[0]->m_databits[i] << 4) + (lut_bitplane[1]->m_databits[i] << 5) +
                    (lut_bitplane[2]->m_databits[i] << 6) + lut_bitplane[0]->m_databits[i+1] +
                    (lut_bitplane[1]->m_databits[i+1] << 1) + (lut_bitplane[2]->m_databits[i+1] << 2);
                i += 2;
                ++ptr;
            }

            // last macroblock case
            if (real_bitplane_size & 0x1)
                *ptr = (lut_bitplane[0]->m_databits[i] << 4) + (lut_bitplane[1]->m_databits[i] << 5) +
                (lut_bitplane[2]->m_databits[i] << 6);

            CompBuf->SetDataSize(bitplane_size);
        }
    }

    Ipp32u VC1PackerLVA::VC1PackBitStreamAdv (VC1Context* pContext,
        Ipp32u& Size,
        Ipp8u* pOriginalData,
        Ipp32u OriginalSize,
        Ipp32u ByteOffset,
        Ipp8u& Flag_03)
    {
        UMCVACompBuffer* CompBuf;
        Ipp8u* pBitstream = (Ipp8u*)m_va->GetCompBuffer(VASliceDataBufferType, &CompBuf, OriginalSize);

        Ipp32u DrvBufferSize = CompBuf->GetBufferSize();
        Ipp8u* pEnd = pBitstream + ByteOffset + OriginalSize;
        Size = OriginalSize;

        if (DrvBufferSize < (OriginalSize + ByteOffset)) // we don't have enough buffer
        {
            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
        }

        if(Flag_03 == 1)
        {
            *(pBitstream + ByteOffset) = 0;
            ippsCopy_8u(pOriginalData + 1, pBitstream + ByteOffset + 1, OriginalSize - 1);
            Flag_03 = 0;
        }
        else if(Flag_03 == 2)
        {
            if (pContext->m_bitstream.bitOffset < 24)
            {
                ippsCopy_8u(pOriginalData + 1, pBitstream + ByteOffset, OriginalSize + 1);
                pEnd++;

                *(pBitstream + ByteOffset + 1) = 0;
                Size--;
            }
            else
            {
                ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize );
            }
            Flag_03 = 0;
        }
        else if(Flag_03 == 3)
        {
            if(pContext->m_bitstream.bitOffset < 16)
            {
                *(pBitstream + ByteOffset) = *pOriginalData;
                *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
                *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
                *(pBitstream + ByteOffset + 3) = *(pOriginalData + 4);
                ippsCopy_8u(pOriginalData + 5, pBitstream + ByteOffset + 4, OriginalSize - 4);
                Size--;
            }
            else
            {
                ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize );
            }

            Flag_03 = 0;
        }
        else if(Flag_03 == 4)
        {
            // *(pBitstream + ByteOffset) = *pOriginalData;
            // *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
            // *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
            // *(pBitstream + ByteOffset + 3) = *(pOriginalData + 3);
            //ippsCopy_8u(pOriginalData + 5, pBitstream + ByteOffset + 4, OriginalSize - 5);

            // Size--;
            // Flag_03 = 0;
            ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize);

            Flag_03 = 0;

        }
        else if(Flag_03 == 5)
        {
            *(pBitstream + ByteOffset) = 0;
            *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
            *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
            ippsCopy_8u(pOriginalData + 4, pBitstream + ByteOffset + 3, OriginalSize - 4);
            pEnd--;

            Size--;
            Flag_03 = 0;
        }
        else
        {
            ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize);
        }

        CompBuf->SetDataSize(ByteOffset + Size);

        if(*(pEnd-1) == 0x03 && *(pEnd -2) == 0x0 && *(pEnd-3) == 0x0)
        {
            *(pEnd-1) = 0;
        }

        return 0;
    }

    static int ConvertMvModeVC1Mfx2VA(int mv_mode)
    {
        switch (mv_mode) {
        case VC1_MVMODE_HPELBI_1MV:     return VAMvMode1MvHalfPelBilinear;
        case VC1_MVMODE_1MV:            return VAMvMode1Mv;
        case VC1_MVMODE_HPEL_1MV:       return VAMvMode1MvHalfPel;
        case VC1_MVMODE_MIXED_MV:       return VAMvModeMixedMv;
        case VC1_MVMODE_INTENSCOMP:     return VAMvModeIntensityCompensation;
        }
        return 0;
    }

    void VC1PackerLVA::VC1PackPicParams (VC1Context* pContext, VAPictureParameterBufferVC1* ptr, VideoAccelerator* va)
    {
        memset(ptr, 0, sizeof(VAPictureParameterBufferVC1));

        ptr->forward_reference_picture                                 = VA_INVALID_ID;
        ptr->backward_reference_picture                                = VA_INVALID_ID;
        ptr->inloop_decoded_picture                                    = VA_INVALID_ID;

        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
            pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG ||
            pContext->m_seqLayerHeader.RANGERED)
        {
            va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
            ptr->inloop_decoded_picture = va->GetSurfaceID(pContext->m_frmBuff.m_iCurrIndex);
        }
        else
            va->BeginFrame(pContext->m_frmBuff.m_iCurrIndex);


        ptr->sequence_fields.bits.pulldown = pContext->m_seqLayerHeader.PULLDOWN;
        ptr->sequence_fields.bits.interlace = pContext->m_seqLayerHeader.INTERLACE;
        ptr->sequence_fields.bits.tfcntrflag = pContext->m_seqLayerHeader.TFCNTRFLAG;
        ptr->sequence_fields.bits.finterpflag = pContext->m_seqLayerHeader.FINTERPFLAG;
        ptr->sequence_fields.bits.psf = 0;
        ptr->sequence_fields.bits.multires = pContext->m_seqLayerHeader.MULTIRES;
        ptr->sequence_fields.bits.overlap = pContext->m_seqLayerHeader.OVERLAP;
        ptr->sequence_fields.bits.syncmarker = pContext->m_seqLayerHeader.SYNCMARKER;
        ptr->sequence_fields.bits.rangered = pContext->m_seqLayerHeader.RANGERED;
        ptr->sequence_fields.bits.max_b_frames = pContext->m_seqLayerHeader.MAXBFRAMES;
        ptr->sequence_fields.bits.profile = pContext->m_seqLayerHeader.PROFILE;

        ptr->coded_width = 2 * (pContext->m_seqLayerHeader.CODED_WIDTH +1);
        ptr->coded_height = 2 * (pContext->m_seqLayerHeader.CODED_HEIGHT + 1);

        ptr->entrypoint_fields.bits.broken_link  = pContext->m_seqLayerHeader.BROKEN_LINK;
        ptr->entrypoint_fields.bits.closed_entry = pContext->m_seqLayerHeader.CLOSED_ENTRY;
        ptr->entrypoint_fields.bits.panscan_flag = pContext->m_seqLayerHeader.PANSCAN_FLAG;
        ptr->entrypoint_fields.bits.loopfilter = pContext->m_seqLayerHeader.LOOPFILTER;

        switch(pContext->m_picLayerHeader->CONDOVER)
        {
        case 0:
            ptr->conditional_overlap_flag = 0; break;
        case 2:
            ptr->conditional_overlap_flag = 1; break;
        default:
            ptr->conditional_overlap_flag = 2; break;
        }
        ptr->fast_uvmc_flag = pContext->m_seqLayerHeader.FASTUVMC;

        ptr->range_mapping_fields.bits.luma_flag = pContext->m_seqLayerHeader.RANGE_MAPY_FLAG;
        ptr->range_mapping_fields.bits.luma = pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ? pContext->m_seqLayerHeader.RANGE_MAPY : 0;
        ptr->range_mapping_fields.bits.chroma_flag = pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG;
        ptr->range_mapping_fields.bits.chroma = pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG ? pContext->m_seqLayerHeader.RANGE_MAPUV : 0;

        ptr->b_picture_fraction       = pContext->m_picLayerHeader->BFRACTION_orig;
        ptr->cbp_table                = pContext->m_picLayerHeader->CBPTAB;
        ptr->mb_mode_table            = pContext->m_picLayerHeader->MBMODETAB;
        ptr->range_reduction_frame = pContext->m_picLayerHeader->RANGEREDFRM;
        ptr->rounding_control =  pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED ? pContext->m_picLayerHeader->RNDCTRL : pContext->m_seqLayerHeader.RNDCTRL;
        ptr->post_processing          = pContext->m_picLayerHeader->POSTPROC;
        ptr->picture_resolution_index = 0;

        if (pContext->m_bIntensityCompensation)
        {
            if (VC1_FieldInterlace != pContext->m_picLayerHeader->FCM)
            {
                ptr->luma_scale = pContext->m_picLayerHeader->LUMSCALE;
                ptr->luma_shift = pContext->m_picLayerHeader->LUMSHIFT;
            }
            else
            {
                if (VC1_INTCOMP_BOTH_FIELD == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->luma_scale = pContext->m_picLayerHeader->LUMSCALE;
                    ptr->luma_shift = pContext->m_picLayerHeader->LUMSHIFT;

                    //bottom field
                    ptr->luma_scale2 = pContext->m_picLayerHeader->LUMSCALE1;
                    ptr->luma_shift2 = pContext->m_picLayerHeader->LUMSHIFT1;

                }
                else if (VC1_INTCOMP_BOTTOM_FIELD == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // bottom field
                    ptr->luma_scale2 = pContext->m_picLayerHeader->LUMSCALE1;
                    ptr->luma_shift2 = pContext->m_picLayerHeader->LUMSHIFT1;
                    // top field not compensated
                    ptr->luma_scale = 32;
                }
                else if (VC1_INTCOMP_TOP_FIELD == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->luma_scale = pContext->m_picLayerHeader->LUMSCALE;
                    ptr->luma_shift = pContext->m_picLayerHeader->LUMSHIFT;
                    // bottom field not compensated
                    ptr->luma_scale2 = 32;
                }
            }
        }
        else
        {
            ptr->luma_scale = 32;
            ptr->luma_scale2 = 32;
        }

        /*picture_fields.bits.picture_type*/
        if(pContext->m_picLayerHeader->FCM == VC1_Progressive || pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
            ptr->picture_fields.bits.picture_type = pContext->m_picLayerHeader->PTYPE;
        else
        {
            //field picture
            switch(pContext->m_picLayerHeader->PTypeField1)
            {
            case VC1_I_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_I_I_FRAME;
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_I_P_FRAME;
                else
                    VM_ASSERT(0);
                break;
            case VC1_P_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_P_P_FRAME;
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_P_I_FRAME;
                else
                    VM_ASSERT(0);
                break;
            case VC1_B_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_B_B_FRAME;
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_B_BI_FRAME;
                else
                    VM_ASSERT(0);
                break;
            case VC1_BI_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_BI_B_FRAME;
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_BI_BI_FRAME;
                else
                    VM_ASSERT(0);
                break;
            default:
                VM_ASSERT(0);
                break;
            }
        }
        ptr->picture_fields.bits.frame_coding_mode =  pContext->m_picLayerHeader->FCM;
        ptr->picture_fields.bits.top_field_first = pContext->m_picLayerHeader->TFF;
        ptr->picture_fields.bits.is_first_field = ! pContext->m_picLayerHeader->CurrField;
        ptr->picture_fields.bits.intensity_compensation = pContext->m_bIntensityCompensation;

#ifdef NO_INTERLACED_STREAMS
        if(pContext->m_seqLayerHeader.INTERLACE == 1 && pContext->m_picLayerHeader->FCM != 0)
        {
            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
        }
#endif

        ptr->bitplane_present.value = 0;
        VC1Bitplane* check_bitplane = 0;
        VC1Bitplane* lut_bitplane[3];

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        case VC1_SKIPPED_FRAME:
            return;
        default:
            return;
        }

        for (Ipp32u i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];
        }
        if (check_bitplane)
        {
            ptr->raw_coding.flags.mv_type_mb = VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->MVTYPEMB) && pContext->m_picLayerHeader->MVTYPEMB.m_databits ? 1 : 0;
            ptr->raw_coding.flags.direct_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->m_DirectMB) && pContext->m_picLayerHeader->m_DirectMB.m_databits ? 1 : 0;
            ptr->raw_coding.flags.skip_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->SKIPMB) && pContext->m_picLayerHeader->SKIPMB.m_databits ? 1 : 0;
            ptr->raw_coding.flags.field_tx =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FIELDTX) && pContext->m_picLayerHeader->FIELDTX.m_databits  ? 1 : 0;
            ptr->raw_coding.flags.forward_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FORWARDMB) && pContext->m_picLayerHeader->FORWARDMB.m_databits  ? 1 : 0;
            ptr->raw_coding.flags.ac_pred =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->ACPRED) && pContext->m_picLayerHeader->ACPRED.m_databits  ? 1 : 0;
            ptr->raw_coding.flags.overflags =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->OVERFLAGS) && pContext->m_picLayerHeader->OVERFLAGS.m_databits  ? 1 : 0;

            ptr->bitplane_present.value = 1;
            ptr->bitplane_present.flags.bp_mv_type_mb = VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->MVTYPEMB) ? 0 : 1;
            ptr->bitplane_present.flags.bp_direct_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->m_DirectMB) ? 0 : 1;
            ptr->bitplane_present.flags.bp_skip_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->SKIPMB) ? 0 : 1;
            ptr->bitplane_present.flags.bp_field_tx =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FIELDTX) ? 0 : 1;
            ptr->bitplane_present.flags.bp_forward_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FORWARDMB) ? 0 : 1;
            ptr->bitplane_present.flags.bp_ac_pred =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->ACPRED) ? 0 : 1;
            ptr->bitplane_present.flags.bp_overflags =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->OVERFLAGS) ? 0 : 1;
        }

        ptr->reference_fields.bits.reference_distance_flag = pContext->m_seqLayerHeader.REFDIST_FLAG;
        ptr->reference_fields.bits.reference_distance = pContext->m_picLayerHeader->REFDIST;
        ptr->reference_fields.bits.num_reference_pictures  = pContext->m_picLayerHeader->NUMREF;
        ptr->reference_fields.bits.reference_field_pic_indicator = pContext->m_picLayerHeader->REFFIELD;

        ptr->mv_fields.bits.mv_mode = (VAMvModeVC1)ConvertMvModeVC1Mfx2VA(pContext->m_picLayerHeader->MVMODE);
        ptr->mv_fields.bits.mv_mode2 = (VAMvModeVC1)ConvertMvModeVC1Mfx2VA(pContext->m_picLayerHeader->MVMODE2);
        ptr->mv_fields.bits.mv_table = pContext->m_picLayerHeader->MVTAB;
        ptr->mv_fields.bits.two_mv_block_pattern_table = pContext->m_picLayerHeader->MV2BPTAB;
        ptr->mv_fields.bits.four_mv_switch = pContext->m_picLayerHeader->MV4SWITCH;
        ptr->mv_fields.bits.four_mv_block_pattern_table = pContext->m_picLayerHeader->MV4BPTAB;
        ptr->mv_fields.bits.extended_mv_flag = pContext->m_seqLayerHeader.EXTENDED_MV;
        ptr->mv_fields.bits.extended_mv_range = pContext->m_picLayerHeader->MVRANGE;
        ptr->mv_fields.bits.extended_dmv_flag = pContext->m_seqLayerHeader.EXTENDED_DMV;
        ptr->mv_fields.bits.extended_dmv_range = pContext->m_picLayerHeader->DMVRANGE;

        ptr->pic_quantizer_fields.bits.dquant = pContext->m_seqLayerHeader.DQUANT;
        ptr->pic_quantizer_fields.bits.quantizer = pContext->m_seqLayerHeader.QUANTIZER;
        ptr->pic_quantizer_fields.bits.half_qp = pContext->m_picLayerHeader->HALFQP;
        ptr->pic_quantizer_fields.bits.pic_quantizer_scale = pContext->m_picLayerHeader->PQUANT;
        ptr->pic_quantizer_fields.bits.pic_quantizer_type = !pContext->m_picLayerHeader->QuantizationType;
        ptr->pic_quantizer_fields.bits.dq_frame = pContext->m_picLayerHeader->m_DQuantFRM;
        ptr->pic_quantizer_fields.bits.dq_profile = pContext->m_picLayerHeader->m_DQProfile;
        ptr->pic_quantizer_fields.bits.dq_sb_edge = pContext->m_picLayerHeader->m_DQProfile == VC1_DQPROFILE_SNGLEDGES  ? pContext->m_picLayerHeader->DQSBEdge : 0;
        ptr->pic_quantizer_fields.bits.dq_db_edge = pContext->m_picLayerHeader->m_DQProfile == VC1_DQPROFILE_DBLEDGES  ? pContext->m_picLayerHeader->DQSBEdge : 0;
        ptr->pic_quantizer_fields.bits.dq_binary_level = pContext->m_picLayerHeader->m_DQBILevel;
        ptr->pic_quantizer_fields.bits.alt_pic_quantizer = pContext->m_picLayerHeader->m_AltPQuant;

        ptr->transform_fields.bits.variable_sized_transform_flag = pContext->m_seqLayerHeader.VSTRANSFORM;
        ptr->transform_fields.bits.mb_level_transform_type_flag = pContext->m_picLayerHeader->TTMBF;
        ptr->transform_fields.bits.frame_level_transform_type = pContext->m_picLayerHeader->TTFRM_ORIG;
        ptr->transform_fields.bits.transform_ac_codingset_idx1 = pContext->m_picLayerHeader->TRANSACFRM;
        ptr->transform_fields.bits.transform_ac_codingset_idx2 = pContext->m_picLayerHeader->TRANSACFRM2;
        ptr->transform_fields.bits.intra_transform_dc_table = pContext->m_picLayerHeader->TRANSDCTAB;

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_B_FRAME:
            ptr->backward_reference_picture = va->GetSurfaceID(pContext->m_frmBuff.m_iNextIndex);
        case VC1_P_FRAME:
            ptr->forward_reference_picture =  va->GetSurfaceID(pContext->m_frmBuff.m_iPrevIndex);
            break;
        }
    }
    void  VC1PackerLVA::VC1PackOneSlice (VC1Context* pContext,
        SliceParams* slparams,
        Ipp32u SliceBufIndex,
        Ipp32u MBOffset,
        Ipp32u SliceDataSize,
        Ipp32u StartCodeOffset,
        Ipp32u ChoppingType) //compatibility with Windows code
    {
        if (SliceBufIndex)
            ++m_pSliceInfo;

        m_pSliceInfo->macroblock_offset = ((31-slparams->m_bitOffset) + MBOffset*8);
        m_pSliceInfo->slice_data_offset = StartCodeOffset; // offset in bytes
        m_pSliceInfo->slice_vertical_position = slparams->MBStartRow;
        m_pSliceInfo->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
        m_pSliceInfo->slice_data_size = SliceDataSize;

#ifdef PRINT_VA_DEBUG
        printf("macroblock_offset = %d \n",m_pSliceInfo->macroblock_offset);
        printf("slice_data_offset = %d \n",m_pSliceInfo->slice_data_offset);
        printf("slice_vertical_position = %d \n",m_pSliceInfo->slice_vertical_position);
        printf("slice_data_size = %d \n",m_pSliceInfo->slice_data_size);
#endif

    }
    void VC1PackerLVA::VC1PackWholeSliceSM (VC1Context* pContext,
        Ipp32u MBOffset,
        Ipp32u SliceDataSize)
    {
        m_pSliceInfo->slice_vertical_position = 0;
        m_pSliceInfo->slice_data_size = SliceDataSize;
        m_pSliceInfo->macroblock_offset = ((31-pContext->m_bitstream.bitOffset) + MBOffset*8);
        m_pSliceInfo->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
        m_pSliceInfo->slice_data_offset = 0;
    }


    void VC1PackerLVA::VC1PackBitStreamForOneSlice (VC1Context* pContext, Ipp32u Size)
    {
        UMCVACompBuffer* CompBuf;
        Ipp8u* pSliceData = (Ipp8u*)m_va->GetCompBuffer(VASliceDataBufferType,&CompBuf);
        ippsCopy_8u((Ipp8u*)(pContext->m_bitstream.pBitstream-1),pSliceData,Size+4);
        CompBuf->SetDataSize(Size+4);
        SwapData((Ipp8u*)pSliceData, Size + 4);
    }
#endif

#ifdef UMC_VA_DXVA
    void VC1PackerDXVA_EagleLake::VC1SetExtPictureBuffer()
    {
        UMCVACompBuffer* CompBuf;
        m_pExtPicInfo = (DXVA_ExtPicInfo*)m_va->GetCompBuffer(DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER, &CompBuf,sizeof(DXVA_ExtPicInfo));
        if (CompBuf->GetBufferSize() < sizeof(DXVA_ExtPicInfo))
            throw vc1_exception(mem_allocation_er);
        //memset(m_pPicPtr,0,sizeof(DXVA_ExtPicInfo));
    }

    void VC1PackerDXVA_EagleLake::VC1SetBuffersSize(Ipp32u SliceBufIndex, Ipp32u PicBuffIndex)
    {
        //_MAY_BE_
        UMCVACompBuffer* CompBuf;
        //DXVA_SliceInfo* pSliceInfo = (DXVA_SliceInfo*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        CompBuf->SetDataSize((Ipp32s)(sizeof(DXVA_SliceInfo)*SliceBufIndex));
        CompBuf->SetNumOfItem(SliceBufIndex);

        m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER,&CompBuf);
        CompBuf->SetDataSize(sizeof(DXVA_PictureParameters));

        m_va->GetCompBuffer(DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER,&CompBuf);
        CompBuf->SetDataSize(sizeof(DXVA_ExtPicInfo));
    }

    void VC1PackerDXVA_EagleLake::VC1PackExtPicParams (VC1Context* pContext,
        DXVA_ExtPicInfo* ptr,
        VideoAccelerator* va)
    {
        memset(ptr, 0, sizeof(DXVA_ExtPicInfo));
        //VM_ASSERT(VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE);

        //BFraction // ADVANCE
        //if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        ptr->bBScaleFactor = (Ipp8u)pContext->m_picLayerHeader->ScaleFactor;
        //else
        //    ptr->bBScaleFactor = 128;//(Ipp8u)pContext->m_picLayerHeader->ScaleFactor;

        //PQUANT
        if(pContext->m_seqLayerHeader.QUANTIZER == 0 
            && pContext->m_picLayerHeader->PQINDEX > 8)
        {
            ptr->bPQuant = (pContext->m_picLayerHeader->PQINDEX < 29)
                ? (Ipp8u)(pContext->m_picLayerHeader->PQINDEX - 3) 
                : (Ipp8u)(pContext->m_picLayerHeader->PQINDEX*2 - 31);
        }
        else
        {
            ptr->bPQuant = (Ipp8u)pContext->m_picLayerHeader->PQINDEX;
        }

        //ALTPQUANT
        ptr->bAltPQuant = (Ipp8u)pContext->m_picLayerHeader->m_AltPQuant;

        //PictureFlags
        if(pContext->m_picLayerHeader->FCM == VC1_Progressive)
        {
            ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 0);  //progressive
        }
        else if(pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
        {
            ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 1);  //interlace frame
        }
        else
        {
            if(pContext->m_picLayerHeader->TFF)
            {
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 2);  //interlace field, top field first
            }
            else
            {
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 3);  //interlace field, bottom field first
            }
        }

        if(pContext->m_picLayerHeader->FCM == VC1_Progressive || pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
        {
            //progressive or interlece frame
            switch(pContext->m_picLayerHeader->PTYPE)
            {
            case VC1_I_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 0);
                break;
            case VC1_P_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 1);
                break;
            case VC1_B_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 2);
                break;
            case VC1_BI_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 3);
                break;
            case VC1_SKIPPED_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 4);
                break;
            default:
                VM_ASSERT(0);
                break;
            }
        }
        else
        {
            //field picture
            switch(pContext->m_picLayerHeader->PTypeField1)
            {
            case VC1_I_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                {
                    //I/I field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 0);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                {
                    //I/P field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 1);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_P_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                {
                    //P/P field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 3);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                {
                    //P/I field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 2);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_B_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                {
                    //B/B field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 4);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                {
                    //B/BI field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 5);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_BI_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                {
                    //BI/B field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 6);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                {
                    //BI/BI field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 7);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            default:
                VM_ASSERT(0);
                break;
            }
        }

        ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 5, 2, pContext->m_picLayerHeader->CONDOVER);

        //PQuantFlags
        if(pContext->m_picLayerHeader->QuantizationType == VC1_QUANTIZER_UNIFORM)
        {
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 0, 1, 1);
        }
        else
        {
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 0, 1, 0);
        }

        ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 1, 1, pContext->m_picLayerHeader->HALFQP);

        switch(pContext->m_picLayerHeader->m_PQuant_mode)
        {
        case VC1_ALTPQUANT_NO:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 0);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_MB_LEVEL:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 3);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_ANY_VALUE:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 2);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_LEFT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 1);
            break;
        case VC1_ALTPQUANT_TOP:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 2);
            break;
        case VC1_ALTPQUANT_RIGTHT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 4);
            break;
        case VC1_ALTPQUANT_BOTTOM:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 8);
            break;
        case VC1_ALTPQUANT_LEFT_TOP:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 3);
            break;
        case VC1_ALTPQUANT_TOP_RIGTHT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 6);
            break;
        case VC1_ALTPQUANT_RIGTHT_BOTTOM:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 12);
            break;
        case VC1_ALTPQUANT_BOTTOM_LEFT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 9);
            break;
        case VC1_ALTPQUANT_EDGES:
        case VC1_ALTPQUANT_ALL:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 15);
            break;
        default:
            VM_ASSERT(0);
            break;
        }

        //MvRange

        if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_NONE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 0);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_HORIZONTAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 1);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 2);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 3);
        }
        else
        {
            VM_ASSERT(0);
        }

        if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_NONE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 0);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_HORIZONTAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 1);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 2);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 3);
        }
        else
        {
            VM_ASSERT(0);
        }

        //wMvReference

        Ipp32u FREFDIST = ((pContext->m_picLayerHeader->ScaleFactor * pContext->m_picLayerHeader->REFDIST) >> 8);
        Ipp32s BREFDIST = pContext->m_picLayerHeader->REFDIST - FREFDIST - 1;

        if (BREFDIST < 0)
            BREFDIST = 0;

        if(pContext->m_picLayerHeader->FCM == VC1_FieldInterlace && VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
            ptr->wMvReference = bit_set(ptr->wMvReference, 0, 4, FREFDIST);
        else
            ptr->wMvReference = bit_set(ptr->wMvReference, 0, 4, pContext->m_picLayerHeader->REFDIST);

        ptr->wMvReference = bit_set(ptr->wMvReference, 4, 4, BREFDIST);

        ptr->wMvReference = bit_set(ptr->wMvReference, 8, 1, pContext->m_picLayerHeader->NUMREF);
        ptr->wMvReference = bit_set(ptr->wMvReference, 9, 1, pContext->m_picLayerHeader->REFFIELD);
        ptr->wMvReference = bit_set(ptr->wMvReference, 10, 1, pContext->m_seqLayerHeader.FASTUVMC);
        ptr->wMvReference = bit_set(ptr->wMvReference, 11, 1, pContext->m_picLayerHeader->MV4SWITCH);

        switch(pContext->m_picLayerHeader->MVMODE)
        {
        case VC1_MVMODE_HPELBI_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 3);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 1);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_MIXED_MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 0);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_HPEL_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 2);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
            //case VC1_MVMODE_INTENSCOMP:
            //        //switch(pContext->m_picLayerHeader->MVMODE2)
            //        //{
            //        //    case VC1_MVMODE_HPELBI_1MV:
            //        //        ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 3);
            //        //        break;
            //        //    case VC1_MVMODE_1MV:
            //        //        ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 1);
            //        //        break;
            //        //    case VC1_MVMODE_MIXED_MV:
            //        //        ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 0);
            //        //        break;
            //        //    case VC1_MVMODE_HPEL_1MV:
            //        //        ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 2);
            //        //        break;
            //        //    default:
            //        //        VM_ASSERT(0);
            //        //        break;
            //        //}
            //    break;
        default:
            VM_ASSERT(0);
            break;
        }

        if(pContext->m_bIntensityCompensation)
        {    
            switch(pContext->m_picLayerHeader->INTCOMFIELD)
            {
            case VC1_INTCOMP_TOP_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 1);
                break;
            case VC1_INTCOMP_BOTTOM_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 2);
                break;
            case VC1_INTCOMP_BOTH_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 3);
                break;
            default:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
                break;
            }
        }

        //wTransformFlags
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 0, 3, pContext->m_picLayerHeader->CBPTAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 3, 1, pContext->m_picLayerHeader->TRANSDCTAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 4, 2, pContext->m_picLayerHeader->TRANSACFRM);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 6, 2, pContext->m_picLayerHeader->TRANSACFRM2);


        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 8, 3, pContext->m_picLayerHeader->MBMODETAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 11, 1, pContext->m_picLayerHeader->TTMBF);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 12, 2, pContext->m_picLayerHeader->TTFRM_ORIG);

        //MvTableFlags
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 0, 2, pContext->m_picLayerHeader->MV2BPTAB);
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 2, 2, pContext->m_picLayerHeader->MV4BPTAB);
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 4, 3, pContext->m_picLayerHeader->MVTAB);

        //RawCodingFlag
        ptr->bRawCodingFlag = 0;
        VC1Bitplane* check_bitplane = 0;
        VC1Bitplane* lut_bitplane[3];


        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        case VC1_SKIPPED_FRAME:
            return;
        default:
            return;
        }

        for (Ipp32u i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }
        if (!check_bitplane) // no bitplanes can return
        {
#ifdef VC1_DEBUG_ON_LOG
            for (int i=0; i < sizeof(DXVA_ExtPicInfo); i++ )

                printf("The byte %d\t equal %x\n",i,*((Ipp8u*)ptr+i));

            printf("ptr->bBScaleFactor =%d\n", ptr->bBScaleFactor);
            printf("ptr->bPQuant =%d\n", ptr->bPQuant);
            printf("ptr->bAltPQuant =%d\n", ptr->bAltPQuant);
            printf("ptr->bPictureFlags =%d\n", ptr->bPictureFlags);
            printf("ptr->bPQuantFlags =%d\n", ptr->bPQuantFlags);
            printf("ptr->bMvRange =%d\n", ptr->bMvRange);
            printf("ptr->wMvReference =%d\n", ptr->wMvReference);
            printf("ptr->wTransformFlags =%d\n", ptr->wTransformFlags);
            printf("ptr->bMvTableFlags =%d\n", ptr->bMvTableFlags);
            printf("ptr->bRawCodingFlag =%d\n", ptr->bRawCodingFlag);
#endif
            return;
        }
        else
            ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 7, 1, 1);


        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE ||
            VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FIELDTX)&&
                pContext->m_picLayerHeader->FIELDTX.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 0, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->ACPRED) &&
                pContext->m_picLayerHeader->ACPRED.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 1, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->OVERFLAGS)&&
                pContext->m_picLayerHeader->OVERFLAGS.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 2, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE ||
            VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {

            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->m_DirectMB)&&
                pContext->m_picLayerHeader->m_DirectMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 3, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->SKIPMB)&& 
                pContext->m_picLayerHeader->SKIPMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 4, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->MVTYPEMB)&&
                pContext->m_picLayerHeader->MVTYPEMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 5, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FORWARDMB)&&
                pContext->m_picLayerHeader->FORWARDMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 6, 1, 1);
            }
        }


#ifdef VC1_DEBUG_ON_LOG
        for (int i=0; i < sizeof(DXVA_ExtPicInfo); i++ )
            printf("The byte %d\t equal %x\n",i,*((Ipp8u*)ptr+i));

        printf("ptr->bBScaleFactor =%d\n", ptr->bBScaleFactor);
        printf("ptr->bPQuant =%d\n", ptr->bPQuant);
        printf("ptr->bAltPQuant =%d\n", ptr->bAltPQuant);
        printf("ptr->bPictureFlags =%d\n", ptr->bPictureFlags);
        printf("ptr->bPQuantFlags =%d\n", ptr->bPQuantFlags);
        printf("ptr->bMvRange =%d\n", ptr->bMvRange);
        printf("ptr->wMvReference =%d\n", ptr->wMvReference);
        printf("ptr->wTransformFlags =%d\n", ptr->wTransformFlags);
        printf("ptr->bMvTableFlags =%d\n", ptr->bMvTableFlags);
        printf("ptr->bRawCodingFlag =%d\n", ptr->bRawCodingFlag);
#endif
    }

    void VC1PackerDXVA_EagleLake::VC1PackPicParams (VC1Context* pContext,
        DXVA_PictureParameters* ptr,
        VideoAccelerator*              va)
    {
        memset(ptr, 0, sizeof(DXVA_PictureParameters));
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) - 1);
            ptr->wPicWidthInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_WIDTH + 1) - 1);
        }
        else
        {
            ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) - 1)/16;
            ptr->wPicWidthInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_WIDTH + 1) - 1)/16;
        }

        ptr->bMacroblockWidthMinus1  = 15;
        ptr->bMacroblockHeightMinus1 = 15;
        ptr->bBlockWidthMinus1       = 7;
        ptr->bBlockHeightMinus1      = 7;
        ptr->bBPPminus1              = 7;

        // interlace fields
        if (VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)
        {
            ptr->bPicExtrapolation = 0x2;
            if (pContext->m_picLayerHeader->CurrField == 0)
            {
                if (pContext->m_seqLayerHeader.PULLDOWN)
                {
                    if (pContext->m_picLayerHeader->TFF)
                        ptr->bPicStructure = 1;
                    else
                        ptr->bPicStructure = 2;
                }
                else
                    ptr->bPicStructure = 1;
            }
            else
            {
                if (pContext->m_picLayerHeader->TFF)
                    ptr->bPicStructure = 2;
                else
                    ptr->bPicStructure = 1;
            }
        }
        else  // frames
        {
            ptr->bPicStructure = 0x03;    //Frame
            if (VC1_Progressive == pContext->m_picLayerHeader->FCM)
                ptr->bPicExtrapolation = 1; //Progressive
            else
                ptr->bPicExtrapolation = 2; //Interlace
        }

        ptr->bSecondField = pContext->m_picLayerHeader->CurrField;
        ptr->bPicIntra = 0;
        ptr->bBidirectionalAveragingMode = 0x80;//0?

        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicIntra = 1;
        }
        else if  (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
            ptr->bPicIntra = 1;
        }
        else if (VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
        }

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,3,1,1);

        // intensity compensation
        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if (pContext->m_bIntensityCompensation && (pContext->m_picLayerHeader->INTCOMFIELD
                || (ptr->bPicStructure==0x3)))
                ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,1);

            if (pContext->m_bIntensityCompensation && 
                (VC1_PROFILE_ADVANCED != pContext->m_seqLayerHeader.PROFILE))
                ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,1);

        }

        //bicubic as default
        ptr->bMVprecisionAndChromaRelation = 0x04;
        if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE)&&
            (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
        {
            if (VC1_MVMODE_HPELBI_1MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bMVprecisionAndChromaRelation |= 0x08;

        }

        if (pContext->m_seqLayerHeader.FASTUVMC)
            ptr->bMVprecisionAndChromaRelation |= 0x01;

        // 4:2:0
        ptr->bChromaFormat = 1;
        ptr->bPicScanFixed  = 1;
        ptr->bPicScanMethod = 0;
        ptr->bPicReadbackRequests = 1;

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bRcontrol = (BYTE)(pContext->m_picLayerHeader->RNDCTRL);
        else
            ptr->bRcontrol = (BYTE)(pContext->m_seqLayerHeader.RNDCTRL);

        ptr->bPicDeblocked = 0;//1?

        // should be zero

        if (pContext->m_seqLayerHeader.LOOPFILTER &&
            pContext->DeblockInfo.isNeedDbl)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 1, 1, 1);
        }

        //ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 2, 1, pContext->m_picLayerHeader->POSTPROC);
        //ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 3, 1, pContext->m_picLayerHeader->POSTPROC);
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {

            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 2, 1, pContext->m_picLayerHeader->POSTPROC);
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 3, 1, pContext->m_picLayerHeader->POSTPROC);
            if (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE)
                ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap
        }
        else
        {
            if(pContext->DeblockInfo.isNeedDbl)
                ptr->bPicDeblocked = 2;

            if ((pContext->m_picLayerHeader->PQUANT >= 9) && (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE))
                ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap
        }


        //range reduction
        if (VC1_PROFILE_MAIN == pContext->m_seqLayerHeader.PROFILE &&
            pContext->m_seqLayerHeader.RANGERED)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked,5,1,(pContext->m_picLayerHeader->RANGEREDFRM>>3));
        }

        ptr->bPicDeblockConfined = 0;
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 7, 1, pContext->m_seqLayerHeader.POSTPROCFLAG);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 6, 1, pContext->m_seqLayerHeader.PULLDOWN);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 5, 1, pContext->m_seqLayerHeader.INTERLACE);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 4, 1, pContext->m_seqLayerHeader.TFCNTRFLAG);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 3, 1, pContext->m_seqLayerHeader.FINTERPFLAG);

            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 2, 1, 1);

            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 1, 1, 0); // PSF _MAY_BE need
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 0, 1, pContext->m_seqLayerHeader.EXTENDED_DMV);
        }

        //1?
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 7, 1, pContext->m_seqLayerHeader.PANSCAN_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 6, 1, pContext->m_seqLayerHeader.REFDIST_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 5, 1, pContext->m_seqLayerHeader.LOOPFILTER);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 4, 1, pContext->m_seqLayerHeader.FASTUVMC);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 3, 1, pContext->m_seqLayerHeader.EXTENDED_MV);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 1, 2, pContext->m_seqLayerHeader.DQUANT);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 0, 1, pContext->m_seqLayerHeader.VSTRANSFORM);

        ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 6, 2, pContext->m_seqLayerHeader.QUANTIZER);

        ptr->bPic4MVallowed = 0;
        if (VC1_FrameInterlace == pContext->m_picLayerHeader->FCM &&
            1 == pContext->m_picLayerHeader->MV4SWITCH &&
            VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPic4MVallowed = 1;
        }
        else if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE))
        {
            if (VC1_MVMODE_MIXED_MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bPic4MVallowed = 1;
        }

        ptr->wDecodedPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);
        ptr->wDeblockedPictureIndex  = ptr->wDecodedPictureIndex;

        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
            pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG || 
            pContext->m_seqLayerHeader.RANGERED)
        {

            if(VC1_IS_REFERENCE(pContext->m_picLayerHeader->PTYPE))
            {
                va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndex);
            }
            else
            {
                va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndexPrev);
            }
        }

        if (1 == ptr->bPicIntra)
        {
            ptr->wForwardRefPictureIndex =  0xFFFF;
            ptr->wBackwardRefPictureIndex = 0xFFFF;
        }
        else if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->wBackwardRefPictureIndex = 0xFFFF;
            if (VC1_FieldInterlace != pContext->m_picLayerHeader->FCM ||
                0 == pContext->m_picLayerHeader->CurrField)
            {
                ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            }
            else
            {
                if (1 == pContext->m_picLayerHeader->NUMREF)
                {
                    // first frame MAY_BE. Need to check
                    if (-1 != pContext->m_frmBuff.m_iPrevIndex)
                        ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                    else
                        ptr->wForwardRefPictureIndex = 0;

                }
                else if (1 == pContext->m_picLayerHeader->REFFIELD)
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                }
                else
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);
                }
            }
        }
        else
        {
            ptr->wForwardRefPictureIndex  = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            ptr->wBackwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iNextIndex);
        }

        ptr->bPicOBMC = 0;
        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 7, 1, pContext->m_seqLayerHeader.RANGE_MAPY_FLAG);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 4, 3, pContext->m_seqLayerHeader.RANGE_MAPY);

        }
        if (pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 3, 1,pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 0, 3, pContext->m_seqLayerHeader.RANGE_MAPUV);
        }

        ptr->bPicBinPB = 0; // RESPIC - need to add
        ptr->bMV_RPS = 0;
        ptr->bReservedBits = BYTE(pContext->m_picLayerHeader->PQUANT);

        ptr->wBitstreamFcodes = 0;
        ptr->wBitstreamPCEelements = 0;

        if (pContext->m_bIntensityCompensation)
        {
            if (3 == ptr->bPicStructure)
            {
                ptr->wBitstreamFcodes = WORD(pContext->m_picLayerHeader->LUMSCALE);
                ptr->wBitstreamPCEelements = WORD(pContext->m_picLayerHeader->LUMSHIFT);
            }
            else
            {
                if (3 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT);


                    //bottom field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->m_picLayerHeader->LUMSCALE1);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->m_picLayerHeader->LUMSHIFT1);                    

                }
                else if (2 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // bottom field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->m_picLayerHeader->LUMSCALE1);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->m_picLayerHeader->LUMSHIFT1);
                    //top from prev field
                    //if (pContext->LUMSCALE || pContext->LUMSHIFT)
                    //{
                    //    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->LUMSCALE);
                    //    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->LUMSHIFT);
                    //}
                    //else
                    // top field not compensated
                    ptr->wBitstreamFcodes |= (32<<8);
                }
                else if (1 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT);
                    //bottom from prev field
                    //if (pContext->LUMSCALE1 || pContext->LUMSHIFT1)
                    //{
                    //    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->LUMSCALE1);
                    //    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->LUMSHIFT1);     
                    //}
                    //else
                    // bottom field not compensated
                    ptr->wBitstreamFcodes |= 32;
                }
            }
        }
        else
        {
            ptr->wBitstreamFcodes = 32;
            if (ptr->bPicStructure != 3)
            {
                ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, 32);
            }
        }
        ptr->bBitstreamConcealmentNeed = 0;
        ptr->bBitstreamConcealmentMethod = 0;

#ifdef VC1_DEBUG_ON_LOG
        printf("New frame \n");

        for (int i=0; i < sizeof(DXVA_PictureParameters); i++ )
            printf("The byte %d\t equal %x\n",i,*((Ipp8u*)ptr+i));

        printf("pParams->wDecodedPictureIndex =%d\n", ptr->wDecodedPictureIndex);
        printf("pParams->wDeblockedPictureIndex =%d\n", ptr->wDeblockedPictureIndex);
        printf("pParams->wForwardRefPictureIndex =%d\n", ptr->wForwardRefPictureIndex);
        printf("pParams->wBackwardRefPictureIndex =%d\n", ptr->wBackwardRefPictureIndex);
        printf("pParams->wPicWidthInMBminus1 =%d\n", ptr->wPicWidthInMBminus1);
        printf("pParams->wPicHeightInMBminus1 =%d\n", ptr->wPicHeightInMBminus1);
        printf("pParams->bMacroblockWidthMinus1 =%d\n", ptr->bMacroblockWidthMinus1);
        printf("pParams->bMacroblockHeightMinus1 =%d\n", ptr->bMacroblockHeightMinus1);
        printf("pParams->bBlockWidthMinus1 =%d\n", ptr->bBlockWidthMinus1);
        printf("pParams->bBlockHeightMinus1 =%d\n", ptr->bBlockHeightMinus1);
        printf("pParams->bBPPminus1  =%d\n", ptr->bBPPminus1);
        printf("pParams->bPicStructure  =%d\n", ptr->bPicStructure);
        printf("pParams->bSecondField  =%d\n", ptr->bSecondField);
        printf("pParams->bPicIntra  =%d\n", ptr->bPicIntra);
        printf("pParams->bPicBackwardPrediction  =%d\n", ptr->bPicBackwardPrediction);
        printf("pParams->bBidirectionalAveragingMode =%d\n", ptr->bBidirectionalAveragingMode);
        printf("pParams->bMVprecisionAndChromaRelation =%d\n", ptr->bMVprecisionAndChromaRelation);
        printf("pParams->bChromaFormat =%d\n", ptr->bChromaFormat);
        printf("pParams->bPicScanFixed =%d\n", ptr->bPicScanFixed);
        printf("pParams->bPicScanMethod =%d\n", ptr->bPicScanMethod);
        printf("pParams->bPicReadbackRequests =%d\n", ptr->bPicReadbackRequests);
        printf("pParams->bRcontrol  =%d\n", ptr->bRcontrol );
        printf("pParams->bPicSpatialResid8  =%d\n", ptr->bPicSpatialResid8);
        printf("pParams->bPicOverflowBlocks  =%d\n", ptr->bPicOverflowBlocks);
        printf("pParams->bPicExtrapolation  =%d\n", ptr->bPicExtrapolation);
        printf("pParams->bPicDeblocked  =%d\n", ptr->bPicDeblocked);
        printf("pParams->bPicDeblockConfined  =%d\n", ptr->bPicDeblockConfined);
        printf("pParams->bPic4MVallowed  =%d\n", ptr->bPic4MVallowed);
        printf("pParams->bPicOBMC  =%d\n", ptr->bPicOBMC);
        printf("pParams->bPicBinPB  =%d\n", ptr->bPicBinPB);
        printf("pParams->bMV_RPS   =%d\n", ptr->bMV_RPS);
        printf("pParams->bReservedBits   =%d\n", ptr->bReservedBits);
        printf("pParams->wBitstreamFcodes   =%d\n", ptr->wBitstreamFcodes);
        printf("pParams->wBitstreamPCEelements   =%d\n", ptr->wBitstreamPCEelements);
        printf("pParams->bBitstreamConcealmentNeed   =%d\n", ptr->bBitstreamConcealmentNeed);
        printf("pParams->bBitstreamConcealmentNeed   =%d\n", ptr->bBitstreamConcealmentMethod);
#endif
    }

    Ipp32u VC1PackerDXVA_EagleLake::VC1PackBitStreamAdv (VC1Context* pContext,
        Ipp32u& Size,
        Ipp8u* pOriginalData,
        Ipp32u OriginalSize,
        Ipp32u ByteOffset,
        Ipp8u& Flag_03)
    {
        UMCVACompBuffer* CompBuf;

#ifdef VC1_DEBUG_ON_LOG
        static FILE* f = 0;
        static int num = 0;

        if(!f)
            f = fopen("BS.txt", "wb");
        fprintf(f, "SLICE    %d\n", num);
        num++;

        fprintf(f, "Size = %d\n", Size);
        fprintf(f, "ByteOffset = %d\n", ByteOffset); 
#endif

        Ipp8u* pBitstream = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);

        Ipp32u DrvBufferSize = CompBuf->GetBufferSize();
        Ipp32u RemainBytes = 0;

        Size = OriginalSize;

        if (DrvBufferSize < (OriginalSize + ByteOffset)) // we don't have enough buffer
        {
            RemainBytes = OriginalSize + ByteOffset - DrvBufferSize;
            Size = DrvBufferSize;
        }

        if(Flag_03 == 1)
        {
            *(pBitstream + ByteOffset) = 0;
            ippsCopy_8u(pOriginalData + 1, pBitstream + ByteOffset + 1, OriginalSize - 1 - RemainBytes);
            Flag_03 = 0;
        }
        else if(Flag_03 == 2)
        {
            if (pContext->m_bitstream.bitOffset < 24)
            {
                ippsCopy_8u(pOriginalData + 1, pBitstream + ByteOffset, OriginalSize - RemainBytes + 1);
                *(pBitstream + ByteOffset + 1) = 0;
                Size--;
            }
            else
            {
                ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize - RemainBytes);
            }
            Flag_03 = 0;
        }
        else if(Flag_03 == 3)
        {
            if(pContext->m_bitstream.bitOffset < 16)
            {
                *(pBitstream + ByteOffset) = *pOriginalData;
                *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
                *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
                *(pBitstream + ByteOffset + 3) = *(pOriginalData + 4);
                ippsCopy_8u(pOriginalData + 5, pBitstream + ByteOffset + 4, OriginalSize - 4 - RemainBytes);
                Size--;
            }
            else
            {
                ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize - RemainBytes);
            }

            Flag_03 = 0;
        }
        else if(Flag_03 == 4)
        {
            // *(pBitstream + ByteOffset) = *pOriginalData;
            // *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
            // *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
            // *(pBitstream + ByteOffset + 3) = *(pOriginalData + 3);
            //ippsCopy_8u(pOriginalData + 5, pBitstream + ByteOffset + 4, OriginalSize - 5 - RemainBytes);

            // Size--;
            // Flag_03 = 0;
            ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize - RemainBytes);

            Flag_03 = 0;

        }
        else if(Flag_03 == 5)
        {
            *(pBitstream + ByteOffset) = 0;
            *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
            *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
            ippsCopy_8u(pOriginalData + 4, pBitstream + ByteOffset + 3, OriginalSize - 4 - RemainBytes);

            Size--;
            Flag_03 = 0;
        }
        else
        {
            ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize - RemainBytes);
        }


#ifdef VC1_DEBUG_ON_LOG
        for(Ipp32u i = 0; i < Size; i++)
        {
            fprintf(f, "%x ", *(pBitstream + ByteOffset + i));

            if(i%10 == 0)
                fprintf(f, "\n");
        }

        fprintf(f, "\n\n\n");
#endif

#ifdef VC1_DEBUG_ON
        printf("Start of Data buffer = %x\n",*((Ipp32u*)(pBitstream)));
#endif
        CompBuf->SetDataSize(ByteOffset + Size);

        return RemainBytes;
    }

    void VC1PackerDXVA_EagleLake::VC1PackBitplaneBuffers(VC1Context* pContext)
    {
        UMCVACompBuffer* CompBuf;

        Ipp8u* ptr = 0;

        Ipp32s i = 0;
        Ipp32s j = 0;
        Ipp32s k = 0;

        Ipp32s h = pContext->m_seqLayerHeader.heightMB;

        VC1Bitplane* lut_bitplane[3];
        VC1Bitplane* check_bitplane = 0;

        Ipp32s bitplane_size = 0;

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        default: // VC1_SKIPPED_FRAME:
            return;
        }

        for (i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }

        if (check_bitplane)
        {
            ptr = (Ipp8u*)m_va->GetCompBuffer(DXVA2_VC1BITPLANE_EXT_BUFFER, &CompBuf);
#ifdef VC1_DEBUG_ON_LOG
            static FILE* f = 0;

            if(!f)
                f = fopen("Bitplane.txt", "wb");
#endif

            bitplane_size = ((pContext->m_seqLayerHeader.heightMB +1)/2)*(pContext->m_seqLayerHeader.MaxWidthMB); //need to update for fields
            if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
            {
                bitplane_size /= 2;
                h = (pContext->m_seqLayerHeader.heightMB+1)/2;
            }

            for (i = 0; i < 3; i++)
            {
                if (!lut_bitplane[i]->m_databits)
                    lut_bitplane[i] = check_bitplane;
            }

            for (i = 0; i < h; i++)
            {
                for (j = 0; j < pContext->m_seqLayerHeader.MaxWidthMB/2; j++)
                {
                    *ptr = lut_bitplane[0]->m_databits[k] + (lut_bitplane[1]->m_databits[k] << 1) +
                        (lut_bitplane[2]->m_databits[k] << 2) + (lut_bitplane[0]->m_databits[k+1] << 4) +
                        (lut_bitplane[1]->m_databits[k+1] << 5) + (lut_bitplane[2]->m_databits[k+1] << 6);
                    k += 2;

#ifdef VC1_DEBUG_ON_LOG
                    fprintf(f, "%x ", *ptr);
#endif
                    ++ptr;
                }
                if(2*(pContext->m_seqLayerHeader.MaxWidthMB/2) < pContext->m_seqLayerHeader.MaxWidthMB)
                {
                    *ptr = 0;
                    *ptr = lut_bitplane[0]->m_databits[k] + (lut_bitplane[1]->m_databits[k] << 1) +
                        (lut_bitplane[2]->m_databits[k] << 2);
                    k++;

#ifdef VC1_DEBUG_ON_LOG
                    fprintf(f, "%x ", *ptr);
#endif
                    ptr++;

                }
#ifdef VC1_DEBUG_ON_LOG
                fprintf(f, "\n");
#endif 
            }
#ifdef VC1_DEBUG_ON_LOG
            fprintf(f, "\n");
#endif
            CompBuf->SetDataSize(bitplane_size);
        }


    }

    void VC1PackerDXVA_EagleLake::VC1PackOneSlice  (VC1Context* pContext,
        SliceParams* slparams,
        Ipp32u BufIndex, // only in future realisations
        Ipp32u MBOffset,
        Ipp32u SliceDataSize,
        Ipp32u StartCodeOffset,
        Ipp32u ChoppingType)
    {
        if (BufIndex)
            ++m_pSliceInfo;

        m_pSliceInfo->wHorizontalPosition = 0;
        m_pSliceInfo->wVerticalPosition = slparams->MBStartRow;
        m_pSliceInfo->dwSliceBitsInBuffer = SliceDataSize*8;
        m_pSliceInfo->wMBbitOffset = (WORD)((31-(BYTE)slparams->m_bitOffset) + MBOffset*8);
        m_pSliceInfo->dwSliceDataLocation = StartCodeOffset;
        m_pSliceInfo->bStartCodeBitOffset = 0;
        m_pSliceInfo->bReservedBits = 0;
        m_pSliceInfo->wQuantizerScaleCode = 0;
        //m_pSliceInfo->wNumberMBsInSlice = slparams->MBRowsToDecode*pContext->m_seqLayerHeader.widthMB;
        // bug of Power VR
        m_pSliceInfo->wNumberMBsInSlice = slparams->MBStartRow;

        m_pSliceInfo->wBadSliceChopping = (Ipp16u)ChoppingType;

#ifdef VC1_DEBUG_ON_LOG

        printf("Bytes in slice = %d\n",SliceDataSize);
        printf("Start code byte offset in the buffer = %d\n",StartCodeOffset);
        printf("bitoffset at the start of MB data = %d\n",m_pSliceInfo->wMBbitOffset);
        printf("wVerticalPosition = %d\n",m_pSliceInfo->wVerticalPosition);
        printf("dwSliceBitsInBuffer = %d\n",m_pSliceInfo->dwSliceBitsInBuffer);
        printf("wNumberMBsInSlice = %d\n",m_pSliceInfo->wNumberMBsInSlice);
        printf("wBadSliceChopping = %d\n",m_pSliceInfo->wBadSliceChopping);
#endif
    }


#endif//UMC_VA_DXVA
#ifdef UMC_VA_DXVA
    void VC1PackerDXVA_Protected::VC1SetExtPictureBuffer()
    {
        UMCVACompBuffer* CompBuf;
        m_pExtPicInfo = (DXVA_ExtPicInfo*)m_va->GetCompBuffer(DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER, &CompBuf,sizeof(DXVA_ExtPicInfo));
        if (CompBuf->GetBufferSize() < sizeof(DXVA_ExtPicInfo))
            throw vc1_exception(mem_allocation_er);
        //memset(m_pPicPtr,0,sizeof(DXVA_ExtPicInfo));
    }

    void VC1PackerDXVA_Protected::VC1SetBuffersSize(Ipp32u SliceBufIndex, Ipp32u PicBuffIndex)
    {
        //_MAY_BE_
        UMCVACompBuffer* CompBuf;
        //DXVA_SliceInfo* pSliceInfo = (DXVA_SliceInfo*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        CompBuf->SetDataSize((Ipp32s)(sizeof(DXVA_SliceInfo)*SliceBufIndex));
        CompBuf->SetNumOfItem(SliceBufIndex);

        m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER,&CompBuf);
        CompBuf->SetDataSize(sizeof(DXVA_PictureParameters));

        m_va->GetCompBuffer(DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER,&CompBuf);
        CompBuf->SetDataSize(sizeof(DXVA_ExtPicInfo));
    }

    void VC1PackerDXVA_Protected::VC1PackExtPicParams (VC1Context* pContext,
        DXVA_ExtPicInfo* ptr,
        VideoAccelerator* va)
    {
        memset(ptr, 0, sizeof(DXVA_ExtPicInfo));
        //VM_ASSERT(VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE);

        //BFraction // ADVANCE
        //if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        ptr->bBScaleFactor = (Ipp8u)pContext->m_picLayerHeader->ScaleFactor;
        //else
        //    ptr->bBScaleFactor = 128;//(Ipp8u)pContext->m_picLayerHeader->ScaleFactor;

        //PQUANT
        if(pContext->m_seqLayerHeader.QUANTIZER == 0 
            && pContext->m_picLayerHeader->PQINDEX > 8)
        {
            ptr->bPQuant = (pContext->m_picLayerHeader->PQINDEX < 29)
                ? (Ipp8u)(pContext->m_picLayerHeader->PQINDEX - 3) 
                : (Ipp8u)(pContext->m_picLayerHeader->PQINDEX*2 - 31);
        }
        else
        {
            ptr->bPQuant = (Ipp8u)pContext->m_picLayerHeader->PQINDEX;
        }

        //ALTPQUANT
        ptr->bAltPQuant = (Ipp8u)pContext->m_picLayerHeader->m_AltPQuant;

        //PictureFlags
        if(pContext->m_picLayerHeader->FCM == VC1_Progressive)
        {
            ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 0);  //progressive
        }
        else if(pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
        {
            ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 1);  //interlace fram
        }
        else
        {
            if(pContext->m_picLayerHeader->TFF)
            {
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 2);  //interlace field, top field first
            }
            else
            {
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 3);  //interlace field,bottom field first
            }
        }

        if(pContext->m_picLayerHeader->FCM == VC1_Progressive || pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
        {
            //progressive or interlece frame
            switch(pContext->m_picLayerHeader->PTYPE)
            {
            case VC1_I_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 0);
                break;
            case VC1_P_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 1);
                break;
            case VC1_B_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 2);
                break;
            case VC1_BI_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 3);
                break;
            case VC1_SKIPPED_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 4);
                break;
            default:
                VM_ASSERT(0);
                break;
            }
        }
        else
        {
            //field picture
            switch(pContext->m_picLayerHeader->PTypeField1)
            {
            case VC1_I_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                {
                    //I/I field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 0);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                {
                    //I/P field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 1);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_P_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                {
                    //P/P field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 3);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                {
                    //P/I field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 2);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_B_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                {
                    //B/B field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 4);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                {
                    //B/BI field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 5);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_BI_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                {
                    //BI/B field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 6);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                {
                    //BI/BI field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 7);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            default:
                VM_ASSERT(0);
                break;
            }
        }

        ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 5, 2, pContext->m_picLayerHeader->CONDOVER);

        //PQuantFlags
        if(pContext->m_picLayerHeader->QuantizationType == VC1_QUANTIZER_UNIFORM)
        {
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 0, 1, 1);
        }
        else
        {
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 0, 1, 0);
        }

        ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 1, 1, pContext->m_picLayerHeader->HALFQP);

        switch(pContext->m_picLayerHeader->m_PQuant_mode)
        {
        case VC1_ALTPQUANT_NO:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 0);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_MB_LEVEL:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 3);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_ANY_VALUE:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 2);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_LEFT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 1);
            break;
        case VC1_ALTPQUANT_TOP:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 2);
            break;
        case VC1_ALTPQUANT_RIGTHT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 4);
            break;
        case VC1_ALTPQUANT_BOTTOM:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 8);
            break;
        case VC1_ALTPQUANT_LEFT_TOP:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 3);
            break;
        case VC1_ALTPQUANT_TOP_RIGTHT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 6);
            break;
        case VC1_ALTPQUANT_RIGTHT_BOTTOM:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 12);
            break;
        case VC1_ALTPQUANT_BOTTOM_LEFT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 9);
            break;
        case VC1_ALTPQUANT_EDGES:
        case VC1_ALTPQUANT_ALL:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 15);
            break;
        default:
            VM_ASSERT(0);
            break;
        }

        //MvRange

        if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_NONE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 0);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_HORIZONTAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 1);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 2);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 3);
        }
        else
        {
            VM_ASSERT(0);
        }

        if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_NONE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 0);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_HORIZONTAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 1);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 2);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 3);
        }
        else
        {
            VM_ASSERT(0);
        }

        //wMvReference

        Ipp32u FREFDIST = ((pContext->m_picLayerHeader->ScaleFactor * pContext->m_picLayerHeader->REFDIST) >> 8);
        Ipp32s BREFDIST = pContext->m_picLayerHeader->REFDIST - FREFDIST - 1;

        if (BREFDIST < 0)
            BREFDIST = 0;

        if(pContext->m_picLayerHeader->FCM == VC1_FieldInterlace && VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
            ptr->wMvReference = bit_set(ptr->wMvReference, 0, 4, FREFDIST);
        else
            ptr->wMvReference = bit_set(ptr->wMvReference, 0, 4, pContext->m_picLayerHeader->REFDIST);

        ptr->wMvReference = bit_set(ptr->wMvReference, 4, 4, BREFDIST);

        ptr->wMvReference = bit_set(ptr->wMvReference, 8, 1, pContext->m_picLayerHeader->NUMREF);
        ptr->wMvReference = bit_set(ptr->wMvReference, 9, 1, pContext->m_picLayerHeader->REFFIELD);
        ptr->wMvReference = bit_set(ptr->wMvReference, 10, 1, pContext->m_seqLayerHeader.FASTUVMC);
        ptr->wMvReference = bit_set(ptr->wMvReference, 11, 1, pContext->m_picLayerHeader->MV4SWITCH);

        switch(pContext->m_picLayerHeader->MVMODE)
        {
        case VC1_MVMODE_HPELBI_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 3);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 1);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_MIXED_MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 0);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_HPEL_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 2);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
            //case VC1_MVMODE_INTENSCOMP:
            //        //switch(pContext->m_picLayerHeader->MVMODE2)
            //        //{
            //        //    case VC1_MVMODE_HPELBI_1MV:
            //        //        ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 3);
            //        //        break;
            //        //    case VC1_MVMODE_1MV:
            //        //        ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 1);
            //        //        break;
            //        //    case VC1_MVMODE_MIXED_MV:
            //        //        ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 0);
            //        //        break;
            //        //    case VC1_MVMODE_HPEL_1MV:
            //        //        ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 2);
            //        //        break;
            //        //    default:
            //        //        VM_ASSERT(0);
            //        //        break;
            //        //}
            //    break;
        default:
            VM_ASSERT(0);
            break;
        }

        if(pContext->m_bIntensityCompensation)
        {    
            switch(pContext->m_picLayerHeader->INTCOMFIELD)
            {
            case VC1_INTCOMP_TOP_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 1);
                break;
            case VC1_INTCOMP_BOTTOM_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 2);
                break;
            case VC1_INTCOMP_BOTH_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 3);
                break;
            default:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
                break;
            }
        }

        //wTransformFlags
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 0, 3, pContext->m_picLayerHeader->CBPTAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 3, 1, pContext->m_picLayerHeader->TRANSDCTAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 4, 2, pContext->m_picLayerHeader->TRANSACFRM);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 6, 2, pContext->m_picLayerHeader->TRANSACFRM2);


        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 8, 3, pContext->m_picLayerHeader->MBMODETAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 11, 1, pContext->m_picLayerHeader->TTMBF);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 12, 2, pContext->m_picLayerHeader->TTFRM_ORIG);

        //MvTableFlags
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 0, 2, pContext->m_picLayerHeader->MV2BPTAB);
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 2, 2, pContext->m_picLayerHeader->MV4BPTAB);
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 4, 3, pContext->m_picLayerHeader->MVTAB);

        //RawCodingFlag
        ptr->bRawCodingFlag = 0;
        VC1Bitplane* check_bitplane = 0;
        VC1Bitplane* lut_bitplane[3];


        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        case VC1_SKIPPED_FRAME:
            return;
        default:
            return;
        }

        for (Ipp32u i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }
        if (!check_bitplane) // no bitplanes can return
        {
#ifdef VC1_DEBUG_ON_LOG
            for (int i=0; i < sizeof(DXVA_ExtPicInfo); i++ )

                printf("The byte %d\t equal %x\n",i,*((Ipp8u*)ptr+i));

            printf("ptr->bBScaleFactor =%d\n", ptr->bBScaleFactor);
            printf("ptr->bPQuant =%d\n", ptr->bPQuant);
            printf("ptr->bAltPQuant =%d\n", ptr->bAltPQuant);
            printf("ptr->bPictureFlags =%d\n", ptr->bPictureFlags);
            printf("ptr->bPQuantFlags =%d\n", ptr->bPQuantFlags);
            printf("ptr->bMvRange =%d\n", ptr->bMvRange);
            printf("ptr->wMvReference =%d\n", ptr->wMvReference);
            printf("ptr->wTransformFlags =%d\n", ptr->wTransformFlags);
            printf("ptr->bMvTableFlags =%d\n", ptr->bMvTableFlags);
            printf("ptr->bRawCodingFlag =%d\n", ptr->bRawCodingFlag);
#endif
            return;
        }
        else
            ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 7, 1, 1);


        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE ||
            VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FIELDTX)&&
                pContext->m_picLayerHeader->FIELDTX.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 0, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->ACPRED) &&
                pContext->m_picLayerHeader->ACPRED.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 1, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->OVERFLAGS)&&
                pContext->m_picLayerHeader->OVERFLAGS.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 2, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE ||
            VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {

            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->m_DirectMB)&&
                pContext->m_picLayerHeader->m_DirectMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 3, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->SKIPMB)&& 
                pContext->m_picLayerHeader->SKIPMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 4, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->MVTYPEMB)&&
                pContext->m_picLayerHeader->MVTYPEMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 5, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FORWARDMB)&&
                pContext->m_picLayerHeader->FORWARDMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 6, 1, 1);
            }
        }


#ifdef VC1_DEBUG_ON_LOG
        for (int i=0; i < sizeof(DXVA_ExtPicInfo); i++ )
            printf("The byte %d\t equal %x\n",i,*((Ipp8u*)ptr+i));

        printf("ptr->bBScaleFactor =%d\n", ptr->bBScaleFactor);
        printf("ptr->bPQuant =%d\n", ptr->bPQuant);
        printf("ptr->bAltPQuant =%d\n", ptr->bAltPQuant);
        printf("ptr->bPictureFlags =%d\n", ptr->bPictureFlags);
        printf("ptr->bPQuantFlags =%d\n", ptr->bPQuantFlags);
        printf("ptr->bMvRange =%d\n", ptr->bMvRange);
        printf("ptr->wMvReference =%d\n", ptr->wMvReference);
        printf("ptr->wTransformFlags =%d\n", ptr->wTransformFlags);
        printf("ptr->bMvTableFlags =%d\n", ptr->bMvTableFlags);
        printf("ptr->bRawCodingFlag =%d\n", ptr->bRawCodingFlag);
#endif
    }

    void VC1PackerDXVA_Protected::VC1PackPicParams (VC1Context* pContext,
        DXVA_PictureParameters* ptr,
        VideoAccelerator*              va)
    {
        memset(ptr, 0, sizeof(DXVA_PictureParameters));
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) - 1);
            ptr->wPicWidthInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_WIDTH + 1) - 1);
        }
        else
        {
            ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) - 1)/16;
            ptr->wPicWidthInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_WIDTH + 1) - 1)/16;
        }

        ptr->bMacroblockWidthMinus1  = 15;
        ptr->bMacroblockHeightMinus1 = 15;
        ptr->bBlockWidthMinus1       = 7;
        ptr->bBlockHeightMinus1      = 7;
        ptr->bBPPminus1              = 7;

        // interlace fields
        if (VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)
        {
            ptr->bPicExtrapolation = 0x2;
            if (pContext->m_picLayerHeader->CurrField == 0)
            {
                if (pContext->m_seqLayerHeader.PULLDOWN)
                {
                    if (pContext->m_picLayerHeader->TFF)
                        ptr->bPicStructure = 1;
                    else
                        ptr->bPicStructure = 2;
                }
                else
                    ptr->bPicStructure = 1;
            }
            else
            {
                if (pContext->m_picLayerHeader->TFF)
                    ptr->bPicStructure = 2;
                else
                    ptr->bPicStructure = 1;
            }
        }
        else  // frames
        {
            ptr->bPicStructure = 0x03;    //Frame
            if (VC1_Progressive == pContext->m_picLayerHeader->FCM)
                ptr->bPicExtrapolation = 1; //Progressive
            else
                ptr->bPicExtrapolation = 2; //Interlace
        }

        ptr->bSecondField = pContext->m_picLayerHeader->CurrField;
        ptr->bPicIntra = 0;
        ptr->bBidirectionalAveragingMode = 0x80;//0?

        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicIntra = 1;
        }
        else if  (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
            ptr->bPicIntra = 1;
        }
        else if (VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
        }

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,3,1,1);

        // intensity compensation
        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if (pContext->m_bIntensityCompensation && (pContext->m_picLayerHeader->INTCOMFIELD
                || (ptr->bPicStructure==0x3)))
                ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,1);

            if (pContext->m_bIntensityCompensation && 
                (VC1_PROFILE_ADVANCED != pContext->m_seqLayerHeader.PROFILE))
                ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,1);

        }

        //bicubic as default
        ptr->bMVprecisionAndChromaRelation = 0x04;
        if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE)&&
            (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
        {
            if (VC1_MVMODE_HPELBI_1MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bMVprecisionAndChromaRelation |= 0x08;

        }

        if (pContext->m_seqLayerHeader.FASTUVMC)
            ptr->bMVprecisionAndChromaRelation |= 0x01;

        // 4:2:0
        ptr->bChromaFormat = 1;
        ptr->bPicScanFixed  = 1;
        ptr->bPicScanMethod = 0;
        ptr->bPicReadbackRequests = 1;

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bRcontrol = (BYTE)(pContext->m_picLayerHeader->RNDCTRL);
        else
            ptr->bRcontrol = (BYTE)(pContext->m_seqLayerHeader.RNDCTRL);

        ptr->bPicDeblocked = 0;//1?

        // should be zero

        if (pContext->m_seqLayerHeader.LOOPFILTER &&
            pContext->DeblockInfo.isNeedDbl)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 1, 1, 1);
        }

        //ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 2, 1, pContext->m_picLayerHeader->POSTPROC);
        //ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 3, 1, pContext->m_picLayerHeader->POSTPROC);
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {

            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 2, 1, pContext->m_picLayerHeader->POSTPROC);
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 3, 1, pContext->m_picLayerHeader->POSTPROC);
            if (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE)
                ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap
        }
        else
        {
            if(pContext->DeblockInfo.isNeedDbl)
                ptr->bPicDeblocked = 2;

            if ((pContext->m_picLayerHeader->PQUANT >= 9) && (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE))
                ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap
        }


        //range reduction
        if (VC1_PROFILE_MAIN == pContext->m_seqLayerHeader.PROFILE &&
            pContext->m_seqLayerHeader.RANGERED)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked,5,1,(pContext->m_picLayerHeader->RANGEREDFRM>>3));
        }

        ptr->bPicDeblockConfined = 0;
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 7, 1, pContext->m_seqLayerHeader.POSTPROCFLAG);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 6, 1, pContext->m_seqLayerHeader.PULLDOWN);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 5, 1, pContext->m_seqLayerHeader.INTERLACE);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 4, 1, pContext->m_seqLayerHeader.TFCNTRFLAG);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 3, 1, pContext->m_seqLayerHeader.FINTERPFLAG);

            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 2, 1, 1);

            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 1, 1, 0); // PSF _MAY_BE need
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 0, 1, pContext->m_seqLayerHeader.EXTENDED_DMV);
        }

        //1?
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 7, 1, pContext->m_seqLayerHeader.PANSCAN_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 6, 1, pContext->m_seqLayerHeader.REFDIST_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 5, 1, pContext->m_seqLayerHeader.LOOPFILTER);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 4, 1, pContext->m_seqLayerHeader.FASTUVMC);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 3, 1, pContext->m_seqLayerHeader.EXTENDED_MV);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 1, 2, pContext->m_seqLayerHeader.DQUANT);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 0, 1, pContext->m_seqLayerHeader.VSTRANSFORM);

        ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 6, 2, pContext->m_seqLayerHeader.QUANTIZER);

        ptr->bPic4MVallowed = 0;
        if (VC1_FrameInterlace == pContext->m_picLayerHeader->FCM &&
            1 == pContext->m_picLayerHeader->MV4SWITCH &&
            VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPic4MVallowed = 1;
        }
        else if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE))
        {
            if (VC1_MVMODE_MIXED_MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bPic4MVallowed = 1;
        }

        ptr->wDecodedPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);
        ptr->wDeblockedPictureIndex  = ptr->wDecodedPictureIndex;

        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
            pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG || 
            pContext->m_seqLayerHeader.RANGERED)
        {

            if(VC1_IS_REFERENCE(pContext->m_picLayerHeader->PTYPE))
            {
                va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndex);
            }
            else
            {
                va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndexPrev);
            }
        }

        if (1 == ptr->bPicIntra)
        {
            ptr->wForwardRefPictureIndex =  0xFFFF;
            ptr->wBackwardRefPictureIndex = 0xFFFF;
        }
        else if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->wBackwardRefPictureIndex = 0xFFFF;
            if (VC1_FieldInterlace != pContext->m_picLayerHeader->FCM ||
                0 == pContext->m_picLayerHeader->CurrField)
            {
                ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            }
            else
            {
                if (1 == pContext->m_picLayerHeader->NUMREF)
                {
                    // first frame MAY_BE. Need to check
                    if (-1 != pContext->m_frmBuff.m_iPrevIndex)
                        ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                    else
                        ptr->wForwardRefPictureIndex = 0;

                }
                else if (1 == pContext->m_picLayerHeader->REFFIELD)
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                }
                else
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);
                }
            }
        }
        else
        {
            ptr->wForwardRefPictureIndex  = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            ptr->wBackwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iNextIndex);
        }

        ptr->bPicOBMC = 0;
        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 7, 1, pContext->m_seqLayerHeader.RANGE_MAPY_FLAG);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 4, 3, pContext->m_seqLayerHeader.RANGE_MAPY);

        }
        if (pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 3, 1,pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 0, 3, pContext->m_seqLayerHeader.RANGE_MAPUV);
        }

        ptr->bPicBinPB = 0; // RESPIC - need to add
        ptr->bMV_RPS = 0;
        ptr->bReservedBits = BYTE(pContext->m_picLayerHeader->PQUANT);

        ptr->wBitstreamFcodes = 0;
        ptr->wBitstreamPCEelements = 0;

        if (pContext->m_bIntensityCompensation)
        {
            if (3 == ptr->bPicStructure)
            {
                ptr->wBitstreamFcodes = WORD(pContext->m_picLayerHeader->LUMSCALE);
                ptr->wBitstreamPCEelements = WORD(pContext->m_picLayerHeader->LUMSHIFT);
            }
            else
            {
                if (3 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT);

                    //bottom field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->m_picLayerHeader->LUMSCALE1);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->m_picLayerHeader->LUMSHIFT1);                    

                }
                else if (2 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // bottom field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->m_picLayerHeader->LUMSCALE1);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->m_picLayerHeader->LUMSHIFT1);
                    //top from prev field
                    //if (pContext->LUMSCALE || pContext->LUMSHIFT)
                    //{
                    //    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->LUMSCALE);
                    //    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->LUMSHIFT);
                    //}
                    //else
                    // top field not compensated
                    ptr->wBitstreamFcodes |= (32<<8);
                }
                else if (1 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT);
                    //bottom from prev field
                    //if (pContext->LUMSCALE1 || pContext->LUMSHIFT1)
                    //{
                    //    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->LUMSCALE1);
                    //    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->LUMSHIFT1);     
                    //}
                    //else
                    // bottom field not compensated
                    ptr->wBitstreamFcodes |= 32;
                }
            }
        }
        else
        {
            ptr->wBitstreamFcodes = 32;
            if (ptr->bPicStructure != 3)
            {
                ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, 32);
            }
        }
        ptr->bBitstreamConcealmentNeed = 0;
        ptr->bBitstreamConcealmentMethod = 0;

#ifdef VC1_DEBUG_ON_LOG
        printf("New frame \n");

        for (int i=0; i < sizeof(DXVA_PictureParameters); i++ )
            printf("The byte %d\t equal %x\n",i,*((Ipp8u*)ptr+i));

        printf("pParams->wDecodedPictureIndex =%d\n", ptr->wDecodedPictureIndex);
        printf("pParams->wDeblockedPictureIndex =%d\n", ptr->wDeblockedPictureIndex);
        printf("pParams->wForwardRefPictureIndex =%d\n", ptr->wForwardRefPictureIndex);
        printf("pParams->wBackwardRefPictureIndex =%d\n", ptr->wBackwardRefPictureIndex);
        printf("pParams->wPicWidthInMBminus1 =%d\n", ptr->wPicWidthInMBminus1);
        printf("pParams->wPicHeightInMBminus1 =%d\n", ptr->wPicHeightInMBminus1);
        printf("pParams->bMacroblockWidthMinus1 =%d\n", ptr->bMacroblockWidthMinus1);
        printf("pParams->bMacroblockHeightMinus1 =%d\n", ptr->bMacroblockHeightMinus1);
        printf("pParams->bBlockWidthMinus1 =%d\n", ptr->bBlockWidthMinus1);
        printf("pParams->bBlockHeightMinus1 =%d\n", ptr->bBlockHeightMinus1);
        printf("pParams->bBPPminus1  =%d\n", ptr->bBPPminus1);
        printf("pParams->bPicStructure  =%d\n", ptr->bPicStructure);
        printf("pParams->bSecondField  =%d\n", ptr->bSecondField);
        printf("pParams->bPicIntra  =%d\n", ptr->bPicIntra);
        printf("pParams->bPicBackwardPrediction  =%d\n", ptr->bPicBackwardPrediction);
        printf("pParams->bBidirectionalAveragingMode =%d\n", ptr->bBidirectionalAveragingMode);
        printf("pParams->bMVprecisionAndChromaRelation =%d\n", ptr->bMVprecisionAndChromaRelation);
        printf("pParams->bChromaFormat =%d\n", ptr->bChromaFormat);
        printf("pParams->bPicScanFixed =%d\n", ptr->bPicScanFixed);
        printf("pParams->bPicScanMethod =%d\n", ptr->bPicScanMethod);
        printf("pParams->bPicReadbackRequests =%d\n", ptr->bPicReadbackRequests);
        printf("pParams->bRcontrol  =%d\n", ptr->bRcontrol );
        printf("pParams->bPicSpatialResid8  =%d\n", ptr->bPicSpatialResid8);
        printf("pParams->bPicOverflowBlocks  =%d\n", ptr->bPicOverflowBlocks);
        printf("pParams->bPicExtrapolation  =%d\n", ptr->bPicExtrapolation);
        printf("pParams->bPicDeblocked  =%d\n", ptr->bPicDeblocked);
        printf("pParams->bPicDeblockConfined  =%d\n", ptr->bPicDeblockConfined);
        printf("pParams->bPic4MVallowed  =%d\n", ptr->bPic4MVallowed);
        printf("pParams->bPicOBMC  =%d\n", ptr->bPicOBMC);
        printf("pParams->bPicBinPB  =%d\n", ptr->bPicBinPB);
        printf("pParams->bMV_RPS   =%d\n", ptr->bMV_RPS);
        printf("pParams->bReservedBits   =%d\n", ptr->bReservedBits);
        printf("pParams->wBitstreamFcodes   =%d\n", ptr->wBitstreamFcodes);
        printf("pParams->wBitstreamPCEelements   =%d\n", ptr->wBitstreamPCEelements);
        printf("pParams->bBitstreamConcealmentNeed   =%d\n", ptr->bBitstreamConcealmentNeed);
        printf("pParams->bBitstreamConcealmentNeed   =%d\n", ptr->bBitstreamConcealmentMethod);
#endif
    }

    void VC1PackerDXVA_Protected::VC1PackBitplaneBuffers(VC1Context* pContext)
    {
        UMCVACompBuffer* CompBuf;

        Ipp8u* ptr = (Ipp8u*)m_va->GetCompBuffer(DXVA2_VC1BITPLANE_EXT_BUFFER, &CompBuf);

        Ipp32s i = 0;
        Ipp32s j = 0;
        Ipp32s k = 0;

        Ipp32s h = pContext->m_seqLayerHeader.heightMB;

        VC1Bitplane* lut_bitplane[3] = {0, 0, 0};
        VC1Bitplane* check_bitplane = 0;

        Ipp32s bitplane_size = 0;

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        case VC1_SKIPPED_FRAME:
            return;
        }

        for (i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }

        if (check_bitplane)
        {
            ptr = (Ipp8u*)m_va->GetCompBuffer(DXVA2_VC1BITPLANE_EXT_BUFFER, &CompBuf);
#ifdef VC1_DEBUG_ON_LOG
            static FILE* f = 0;

            if(!f)
                f = fopen("Bitplane.txt", "wb");
#endif

            bitplane_size = (pContext->m_seqLayerHeader.heightMB+1)*(pContext->m_seqLayerHeader.widthMB)/2; //need to update for fields
            if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
            {
                bitplane_size /= 2;
                h = (pContext->m_seqLayerHeader.heightMB+1)/2;
            }

            for (i = 0; i < 3; i++)
            {
                if (!lut_bitplane[i]->m_databits)
                    lut_bitplane[i] = check_bitplane;
            }

            for (i = 0; i < h; i++)
            {
                for (j = 0; j < pContext->m_seqLayerHeader.widthMB/2; j++)
                {
                    *ptr = lut_bitplane[0]->m_databits[k] + (lut_bitplane[1]->m_databits[k] << 1) +
                        (lut_bitplane[2]->m_databits[k] << 2) + (lut_bitplane[0]->m_databits[k+1] << 4) +
                        (lut_bitplane[1]->m_databits[k+1] << 5) + (lut_bitplane[2]->m_databits[k+1] << 6);
                    k += 2;

#ifdef VC1_DEBUG_ON_LOG
                    fprintf(f, "%x ", *ptr);
#endif
                    ++ptr;
                }
                if(2*(pContext->m_seqLayerHeader.widthMB/2) < pContext->m_seqLayerHeader.widthMB)
                {
                    *ptr = 0;
                    *ptr = lut_bitplane[0]->m_databits[k] + (lut_bitplane[1]->m_databits[k] << 1) +
                        (lut_bitplane[2]->m_databits[k] << 2);
                    k++;

#ifdef VC1_DEBUG_ON_LOG
                    fprintf(f, "%x ", *ptr);
#endif
                    ptr++;

                }
#ifdef VC1_DEBUG_ON_LOG
                fprintf(f, "\n");
#endif 
            }
#ifdef VC1_DEBUG_ON_LOG
            fprintf(f, "\n");
#endif
            CompBuf->SetDataSize(bitplane_size);
        }


    }

    void VC1PackerDXVA_Protected::VC1PackOneSlice  (VC1Context* pContext,
        SliceParams* slparams,
        Ipp32u BufIndex, // only in future realisations
        Ipp32u MBOffset,
        Ipp32u SliceDataSize,
        Ipp32u StartCodeOffset,
        Ipp32u ChoppingType)
    {
        if (BufIndex)
            ++m_pSliceInfo;

        m_pSliceInfo->wHorizontalPosition = 0;
        m_pSliceInfo->wVerticalPosition = slparams->MBStartRow;
        m_pSliceInfo->dwSliceBitsInBuffer = SliceDataSize*8;
        m_pSliceInfo->wMBbitOffset = (WORD)((31-(BYTE)slparams->m_bitOffset) + MBOffset*8);
        m_pSliceInfo->dwSliceDataLocation = StartCodeOffset;
        m_pSliceInfo->bStartCodeBitOffset = 0;
        m_pSliceInfo->bReservedBits = 0;
        m_pSliceInfo->wQuantizerScaleCode = 0;
        //m_pSliceInfo->wNumberMBsInSlice = slparams->MBRowsToDecode*pContext->m_seqLayerHeader.widthMB;
        // bug of Power VR
        m_pSliceInfo->wNumberMBsInSlice = slparams->MBStartRow;

        m_pSliceInfo->wBadSliceChopping = (Ipp16u)ChoppingType;

#ifdef VC1_DEBUG_ON_LOG

        printf("Bytes in slice = %d\n",SliceDataSize);
        printf("Start code byte offset in the buffer = %d\n",StartCodeOffset);
        printf("bitoffset at the start of MB data = %d\n",m_pSliceInfo->wMBbitOffset);
        printf("wVerticalPosition = %d\n",m_pSliceInfo->wVerticalPosition);
        printf("dwSliceBitsInBuffer = %d\n",m_pSliceInfo->dwSliceBitsInBuffer);
        printf("wNumberMBsInSlice = %d\n",m_pSliceInfo->wNumberMBsInSlice);
        printf("wBadSliceChopping = %d\n",m_pSliceInfo->wBadSliceChopping);
#endif
    }


    Ipp32u VC1PackerDXVA_Protected::VC1PackBitStreamAdv (VC1Context* pContext,
        Ipp32u& Size,
        Ipp8u* pOriginalData,
        Ipp32u OriginalSize,
        Ipp32u ByteOffset,
        Ipp8u& Flag_03)
    {
        UMCVACompBuffer* CompBuf;

#ifdef VC1_DEBUG_ON_LOG
        static FILE* f = 0;
        static int num = 0;

        if(!f)
            f = fopen("BS.txt", "wb");
        fprintf(f, "SLICE    %d\n", num);
        num++;

        fprintf(f, "Size = %d\n", Size);
        fprintf(f, "ByteOffset = %d\n", ByteOffset); 
#endif

        Ipp8u* pBitstream = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);

        if(NULL != m_va->GetProtectedVA() && IS_PROTECTION_GPUCP_ANY(m_va->GetProtectedVA()->GetProtected()))
            CompBuf->SetPVPState(NULL, 0);

        Ipp32u DrvBufferSize = CompBuf->GetBufferSize();
        Ipp32u RemainBytes = 0;

        Size = OriginalSize;

        if (DrvBufferSize < (OriginalSize + ByteOffset)) // we don't have enough buffer
        {
            RemainBytes = OriginalSize + ByteOffset - DrvBufferSize;
            Size = DrvBufferSize;
        }

        if(Flag_03 == 1)
        {
            *(pBitstream + ByteOffset) = 0;
            ippsCopy_8u(pOriginalData + 1, pBitstream + ByteOffset + 1, OriginalSize - 1 - RemainBytes);
            Flag_03 = 0;
        }
        else if(Flag_03 == 2)
        {
            if (pContext->m_bitstream.bitOffset < 24)
            {
                ippsCopy_8u(pOriginalData + 1, pBitstream + ByteOffset, OriginalSize - RemainBytes + 1);
                *(pBitstream + ByteOffset + 1) = 0;
                Size--;
            }
            else
            {
                ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize - RemainBytes);
            }
            Flag_03 = 0;
        }
        else if(Flag_03 == 3)
        {
            if(pContext->m_bitstream.bitOffset < 16)
            {
                *(pBitstream + ByteOffset) = *pOriginalData;
                *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
                *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
                *(pBitstream + ByteOffset + 3) = *(pOriginalData + 4);
                ippsCopy_8u(pOriginalData + 5, pBitstream + ByteOffset + 4, OriginalSize - 4 - RemainBytes);
                Size--;
            }
            else
            {
                ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize - RemainBytes);
            }

            Flag_03 = 0;
        }
        else if(Flag_03 == 4)
        {
            // *(pBitstream + ByteOffset) = *pOriginalData;
            // *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
            // *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
            // *(pBitstream + ByteOffset + 3) = *(pOriginalData + 3);
            //ippsCopy_8u(pOriginalData + 5, pBitstream + ByteOffset + 4, OriginalSize - 5 - RemainBytes);

            // Size--;
            // Flag_03 = 0;
            ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize - RemainBytes);

            Flag_03 = 0;
        }
        else if(Flag_03 == 5)
        {
            *(pBitstream + ByteOffset) = 0;
            *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
            *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
            ippsCopy_8u(pOriginalData + 4, pBitstream + ByteOffset + 3, OriginalSize - 4 - RemainBytes);

            Size--;
            Flag_03 = 0;
        }
        else
        {
            ippsCopy_8u(pOriginalData, pBitstream + ByteOffset, OriginalSize - RemainBytes);
        }


#ifdef VC1_DEBUG_ON_LOG
        for(Ipp32u i = 0; i < Size; i++)
        {
            fprintf(f, "%x ", *(pBitstream + ByteOffset + i));

            if(i%10 == 0)
                fprintf(f, "\n");
        }

        fprintf(f, "\n\n\n");
#endif

#ifdef VC1_DEBUG_ON
        printf("Start of Data buffer = %x\n",*((Ipp32u*)(pBitstream)));
#endif
        CompBuf->SetDataSize(ByteOffset + Size);

        return RemainBytes;
    }

#endif//UMC_VA_DXVA
}
#endif //UMC_RESTRICTED_CODE_VA
#endif //UMC_ENABLE_VC1_VIDEO_DECODER


