//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include <memory.h>

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_common_defs.h"
#include "ipps.h"

void Smoothing_I_Adv(VC1Context* pContext, Ipp32s Height)
{
    if(pContext->m_seqLayerHeader.OVERLAP == 0)
        return;

    {
        VC1MB* pCurrMB = pContext->m_pCurrMB;
        Ipp32s notTop = VC1_IS_NO_TOP_MB(pCurrMB->LeftTopRightPositionFlag);
        Ipp32s Width = pContext->m_seqLayerHeader.widthMB;
        Ipp32s MaxWidth = pContext->m_seqLayerHeader.MaxWidthMB;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;
        Ipp32u CurrFieldFlag = (pCurrMB->FIELDTX)<<1 | pCurrMB->FIELDTX;
        Ipp32u LeftCurrFieldFlag = 0;

        Ipp16s* CurrBlock = pContext->m_pBlock;
        Ipp8u* YPlane = pCurrMB->currYPlane;
        Ipp8u* UPlane = pCurrMB->currUPlane;
        Ipp8u* VPlane = pCurrMB->currVPlane;
        Ipp32s YPitch = pCurrMB->currYPitch;
        Ipp32s UPitch = pCurrMB->currUPitch;
        Ipp32s VPitch = pCurrMB->currVPitch;

        Ipp32s CurrOverlap = pCurrMB->Overlap;
        Ipp32s LeftOverlap;
        Ipp32s TopLeftOverlap;
        Ipp32s TopOverlap;


        Ipp32s i, j;

       for (j = 0; j< Height; j++)
       {
           notTop = VC1_IS_NO_TOP_MB(pCurrMB->LeftTopRightPositionFlag);

           if(notTop)
           {
                //first MB in row
               CurrFieldFlag = (pCurrMB->FIELDTX)<<1 | pCurrMB->FIELDTX;

               //internal vertical smoothing
               if(CurrOverlap)
               {
                    _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6, VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8,  VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8,     YPitch,
                                                                CurrFieldFlag,  EdgeDisabledFlag);
               }

                for (i = 1; i < Width; i++)
                {
                    LeftCurrFieldFlag = pCurrMB->FIELDTX<<1;

                    CurrBlock  += 8*8*6;
                    pCurrMB++;

                    YPlane = pCurrMB->currYPlane;
                    UPlane = pCurrMB->currUPlane;
                    VPlane = pCurrMB->currVPlane;

                    CurrFieldFlag = (pCurrMB->FIELDTX)<<1 | pCurrMB->FIELDTX;
                    LeftCurrFieldFlag |= pCurrMB->FIELDTX;

                    CurrOverlap    = pCurrMB->Overlap;
                    LeftOverlap    = (pCurrMB - 1)->Overlap;
                    TopLeftOverlap = (pCurrMB - MaxWidth - 1)->Overlap;

                    //LUMA

                    if(CurrOverlap)
                    {

                        //internal vertical smoothing
                        _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6,        VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock + 8,        VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane + 8,           YPitch,
                                                                    CurrFieldFlag,        EdgeDisabledFlag);
                    }

                    if(LeftOverlap)
                    {
                        if(CurrOverlap)
                        {
                            //left boundary vertical smoothing
                            _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*6+14,VC1_PIXEL_IN_LUMA*2,
                                                                        CurrBlock,            VC1_PIXEL_IN_LUMA*2,
                                                                        YPlane,               YPitch,
                                                                        LeftCurrFieldFlag,    EdgeDisabledFlag);
                            //CHROMA
                            //U vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*2 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*4,     VC1_PIXEL_IN_CHROMA*2,
                                                                        UPlane,                UPitch);

                            //V vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*5,   VC1_PIXEL_IN_CHROMA*2,
                                                                        VPlane,              VPitch);
                        }

                        if(TopLeftOverlap && (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
                        {
                            //left MB Upper horizontal edge
                            _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*2 +6*VC1_PIXEL_IN_LUMA, VC1_PIXEL_IN_LUMA*2,
                                                                        CurrBlock - 8*8*6,   VC1_PIXEL_IN_LUMA*2,
                                                                        YPlane - 16,         YPitch,
                                                                        EdgeDisabledFlag);
                            //CHROMA
                            //U top horizontal smoothing
                            _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*4 +6*VC1_PIXEL_IN_CHROMA,        VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock - 2*64, VC1_PIXEL_IN_CHROMA*2,
                                                                        UPlane - 8,       UPitch);

                            //V top horizontal smoothing
                            _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*5 +6*VC1_PIXEL_IN_CHROMA,      VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock - 64, VC1_PIXEL_IN_CHROMA*2,
                                                                        VPlane - 8,     VPitch);
                        }

                        if(pContext->m_picLayerHeader->FCM != VC1_FrameInterlace)
                        {
                            //left MB internal horizontal edge
                            _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - 8*8*4 - 32, VC1_PIXEL_IN_LUMA*2,
                                                                        CurrBlock - 8*8*4,      VC1_PIXEL_IN_LUMA*2,
                                                                        YPlane - 16 + 8*YPitch, YPitch,
                                                                        EdgeDisabledFlag);
                        }

                    }
                }

                //RIGHT MB

                //LUMA

                TopOverlap     = (pCurrMB - MaxWidth)->Overlap;

                if(CurrOverlap && (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
                {
                    //MB internal horizontal edge
                    _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock + 8*8*2 - 32, VC1_PIXEL_IN_LUMA*2,
                                                            CurrBlock + 8*8*2,      VC1_PIXEL_IN_LUMA*2,
                                                            YPlane + 8*YPitch, YPitch,
                                                            EdgeDisabledFlag);
                   if(TopOverlap)
                    {
                        //MB Upper horizontal edge
                        _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*2 +6*VC1_PIXEL_IN_LUMA,     VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock,  VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane,     YPitch,
                                                                    EdgeDisabledFlag);
                        //U top horizontal smoothing
                        _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*4 +6*VC1_PIXEL_IN_CHROMA,            VC1_PIXEL_IN_CHROMA*2,
                                                                    CurrBlock + 4*64,  VC1_PIXEL_IN_CHROMA*2,
                                                                    UPlane,            UPitch);

                        //V top horizontal smoothing
                        _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*5 +6*VC1_PIXEL_IN_CHROMA,           VC1_PIXEL_IN_CHROMA*2,
                                                                    CurrBlock + 64*5, VC1_PIXEL_IN_CHROMA*2,
                                                                    VPlane,           VPitch);
                    }
                }
                if ( j < (Height-1))
                {
                    CurrBlock  += 8*8*6;
                    pCurrMB++;

                    CurrBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
                    pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
                    YPlane = pCurrMB->currYPlane;
                    UPlane = pCurrMB->currUPlane;
                    VPlane = pCurrMB->currVPlane;

                    CurrOverlap    = pCurrMB->Overlap;
                    LeftOverlap    = (pCurrMB - 1)->Overlap;
                    TopLeftOverlap = (pCurrMB - MaxWidth - 1)->Overlap;
                    TopOverlap     = (pCurrMB - MaxWidth)->Overlap;
                }
        }
        else
        {

                //first MB in row
                CurrFieldFlag = (pCurrMB->FIELDTX)<<1 | pCurrMB->FIELDTX;

                //internal vertical smoothing
                if(CurrOverlap)
                {
                    _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6,  VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8,  VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8,     YPitch,
                                                                CurrFieldFlag,  EdgeDisabledFlag);
                }

                for (i = 1; i < Width; i++)
                {
                    LeftCurrFieldFlag = pCurrMB->FIELDTX<<1;

                    CurrBlock  += 8*8*6;
                    pCurrMB++;

                    CurrFieldFlag = (pCurrMB->FIELDTX)<<1 | pCurrMB->FIELDTX;
                    LeftCurrFieldFlag |= pCurrMB->FIELDTX;

                    YPlane = pCurrMB->currYPlane;
                    UPlane = pCurrMB->currUPlane;
                    VPlane = pCurrMB->currVPlane;

                    CurrOverlap    = pCurrMB->Overlap;
                    LeftOverlap    = (pCurrMB - 1)->Overlap;

                    //LUMA

                    if(CurrOverlap)
                    {
                        //internal vertical smoothing
                        _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6,  VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock + 8,  VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane + 8,     YPitch,
                                                                    CurrFieldFlag,  EdgeDisabledFlag);
                    }

                    if(LeftOverlap)
                    {
                        if(CurrOverlap)
                        {
                            //left boundary vertical smoothing
                            _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*6 + 14,  VC1_PIXEL_IN_LUMA*2,
                                                                        CurrBlock,               VC1_PIXEL_IN_LUMA*2,
                                                                        YPlane,                  YPitch,
                                                                        LeftCurrFieldFlag,       EdgeDisabledFlag);
                            //CHROMA
                            //U vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*2 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*4,     VC1_PIXEL_IN_CHROMA*2,
                                                                        UPlane,                UPitch);

                            //V vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock +  8*8*5,  VC1_PIXEL_IN_CHROMA*2,
                                                                            VPlane,             VPitch);
                        }


                        if(pContext->m_picLayerHeader->FCM != VC1_FrameInterlace)
                        {
                            //left MB internal horizontal edge
                            _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - 8*8*4 - 32,   VC1_PIXEL_IN_LUMA*2,
                                                                        CurrBlock - 8*8*4,        VC1_PIXEL_IN_LUMA*2,
                                                                        YPlane - 16 + 8*YPitch,   YPitch,
                                                                        EdgeDisabledFlag);
                        }
                   }
                }

                if(CurrOverlap && (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
                {
                    //MB internal horizontal edge
                    _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock + 8*8*2 - 32,  VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8*8*2,       VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8*YPitch,       YPitch,
                                                                EdgeDisabledFlag);
                }

                if (j< (Height-1))
                {
                    CurrBlock  += 8*8*6;
                    pCurrMB++;
                    CurrBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
                    pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);

                    YPlane = pCurrMB->currYPlane;
                    UPlane = pCurrMB->currUPlane;
                    VPlane = pCurrMB->currVPlane;

                    CurrOverlap    = pCurrMB->Overlap;
                    LeftOverlap    = (pCurrMB - 1)->Overlap;
                }
             }
        }
    }
}

void Smoothing_P_Adv(VC1Context* pContext, Ipp32s Height)
{
    if(pContext->m_seqLayerHeader.OVERLAP == 0)
        return;

    {
        VC1MB* pCurrMB = pContext->m_pCurrMB;
        Ipp32s notTop = VC1_IS_NO_TOP_MB(pCurrMB->LeftTopRightPositionFlag);
        Ipp32s Width = pContext->m_seqLayerHeader.widthMB;
        Ipp32s MaxWidth = pContext->m_seqLayerHeader.MaxWidthMB;
        Ipp32u EdgeDisabledFlag = 0;

        Ipp32u CurrFieldFlag = (pCurrMB->FIELDTX)<<1 | pCurrMB->FIELDTX;
        Ipp32u LeftCurrFieldFlag = 0;

        Ipp16s* CurrBlock = pContext->m_pBlock;
        Ipp8u* YPlane = pCurrMB->currYPlane;
        Ipp8u* UPlane = pCurrMB->currUPlane;
        Ipp8u* VPlane = pCurrMB->currVPlane;

        Ipp32s YPitch = pCurrMB->currYPitch;
        Ipp32s UPitch = pCurrMB->currUPitch;
        Ipp32s VPitch = pCurrMB->currVPitch;
        Ipp32s LeftIntra;
        Ipp32s TopLeftIntra;
        Ipp32s TopIntra;
        Ipp32s CurrIntra = pCurrMB->IntraFlag*pCurrMB->Overlap;

        Ipp32s i, j;

        for (j = 0; j< Height; j++)
        {
            notTop = VC1_IS_NO_TOP_MB(pCurrMB->LeftTopRightPositionFlag);

            if(notTop)
            {
                if(CurrIntra)
                {
                    //first block in row
                    CurrFieldFlag = (pCurrMB->FIELDTX)<<1 | pCurrMB->FIELDTX;


                    //internal vertical smoothing
                    EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_1_INTRA) * (IPPVC_EDGE_HALF_1))
                                      |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_2_3_INTRA) * (IPPVC_EDGE_HALF_2));

                    if(EdgeDisabledFlag)
                        _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6, VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock + 8, VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane    + 8, YPitch,
                                                                    CurrFieldFlag, EdgeDisabledFlag);
                }

                for (i = 1; i < Width; i++)
                {
                    LeftCurrFieldFlag = pCurrMB->FIELDTX<<1;

                    CurrBlock  += 8*8*6;
                    pCurrMB++;

                    YPlane = pCurrMB->currYPlane;
                    UPlane = pCurrMB->currUPlane;
                    VPlane = pCurrMB->currVPlane;

                    CurrIntra    = (pCurrMB)->IntraFlag*pCurrMB->Overlap;
                    LeftIntra    = (pCurrMB - 1)->IntraFlag*(pCurrMB - 1)->Overlap;
                    TopLeftIntra = (pCurrMB - MaxWidth - 1)->IntraFlag*(pCurrMB - MaxWidth - 1)->Overlap;

                    CurrFieldFlag = (pCurrMB->FIELDTX)<<1 | pCurrMB->FIELDTX;
                    LeftCurrFieldFlag |= pCurrMB->FIELDTX;

                    if(CurrIntra)
                    {
                        //////////////////////////////
                        //internal vertical smoothing
                        EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_1_INTRA) * (IPPVC_EDGE_HALF_1))
                                        |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_2_3_INTRA) * (IPPVC_EDGE_HALF_2));

                        if(EdgeDisabledFlag)
                            _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6,       VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock + 8,        VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane+8,             YPitch,
                                                                    CurrFieldFlag,        EdgeDisabledFlag);

                        if(LeftIntra)
                        {
                            //////////////////////////////////
                            //LUMA left boundary vertical smoothing
                            EdgeDisabledFlag =(((VC1_EDGE_MB(CurrIntra,VC1_BLOCK_0_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_1_INTRA)))
                                *(IPPVC_EDGE_HALF_1))|
                                (((VC1_EDGE_MB(CurrIntra,VC1_BLOCK_2_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_3_INTRA)))
                                *(IPPVC_EDGE_HALF_2));

                            if(EdgeDisabledFlag)
                            _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*6+14, VC1_PIXEL_IN_LUMA*2,
                                                                        CurrBlock,            VC1_PIXEL_IN_LUMA*2,
                                                                        YPlane,               YPitch,
                                                                        LeftCurrFieldFlag,    EdgeDisabledFlag);
                        }

                        if(((LeftIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA) && ((CurrIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA))
                        {
                            ///////////////////////
                            //U vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*2 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*4,     VC1_PIXEL_IN_CHROMA*2,
                                                                        UPlane,                UPitch);

                            ///////////////////////
                            //V vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*5,   VC1_PIXEL_IN_CHROMA*2,
                                                                        VPlane,              VPitch);
                        }
                    }



                    if(LeftIntra && (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
                    {
                        ////////////////////////////////
                        //left MB Upper horizontal edge
                        EdgeDisabledFlag =(((VC1_EDGE_MB(TopLeftIntra,VC1_BLOCK_2_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_0_INTRA)))
                            *(IPPVC_EDGE_HALF_1))|
                            (((VC1_EDGE_MB(TopLeftIntra,VC1_BLOCK_3_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_1_INTRA)))
                            *(IPPVC_EDGE_HALF_2));
                        _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*2 +6*VC1_PIXEL_IN_LUMA,              VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock - 8*8*6,   VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane - 16,         YPitch,
                                                                    EdgeDisabledFlag);
                        //////////////////////////////////
                        //left MB internal horizontal edge
                         EdgeDisabledFlag = (VC1_EDGE_MB(LeftIntra,VC1_BLOCKS_0_2_INTRA) * (IPPVC_EDGE_HALF_1))
                                        |(VC1_EDGE_MB(LeftIntra,VC1_BLOCKS_1_3_INTRA) * (IPPVC_EDGE_HALF_2));
                        _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - 8*8*4 - 32, VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock - 8*8*4,      VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane - 16 + 8*YPitch, YPitch,
                                                                    EdgeDisabledFlag);


                        if(((TopLeftIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA) && ((LeftIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA))
                        {
                            /////////////////////////////
                            //U top horizontal smoothing
                            _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*4 +6*VC1_PIXEL_IN_CHROMA,           VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock - 2*64, VC1_PIXEL_IN_CHROMA*2,
                                                                        UPlane - 8,       UPitch);

                            ////////////////////////////
                            //V top horizontal smoothing
                            _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - (MaxWidth+1)*8*8*6 + 64*5 +6*VC1_PIXEL_IN_CHROMA,         VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock - 64, VC1_PIXEL_IN_CHROMA*2,
                                                                        VPlane - 8,     VPitch);
                        }
                    }
                }

                //RIGHT MB
                //LUMA
                TopIntra = (pCurrMB - MaxWidth)->IntraFlag*(pCurrMB - MaxWidth)->Overlap;

                if(CurrIntra && (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
                {
                    ///////////////////////////
                    //MB Upper horizontal edge
                    EdgeDisabledFlag =(((VC1_EDGE_MB(TopIntra,VC1_BLOCK_2_INTRA)) && (VC1_EDGE_MB(CurrIntra, VC1_BLOCK_0_INTRA)))
                            *(IPPVC_EDGE_HALF_1))|
                            (((VC1_EDGE_MB(TopIntra,VC1_BLOCK_3_INTRA)) && (VC1_EDGE_MB(CurrIntra, VC1_BLOCK_1_INTRA)))
                            *(IPPVC_EDGE_HALF_2));
                    _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*2 +6*VC1_PIXEL_IN_LUMA,     VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock,   VC1_PIXEL_IN_LUMA*2,
                                                                YPlane,      YPitch,
                                                                EdgeDisabledFlag);
                    /////////////////////////////
                    //MB internal horizontal edge
                    EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_2_INTRA) * (IPPVC_EDGE_HALF_1))
                                    |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_1_3_INTRA) * (IPPVC_EDGE_HALF_2));
                    _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock + 8*8*2 - 32, VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8*8*2,      VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8*YPitch,      YPitch,
                                                                EdgeDisabledFlag);
                    if(((TopIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA) && ((CurrIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA))
                    {
                        /////////////////////////////
                        //U top horizontal smoothing
                        _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*4 +6*VC1_PIXEL_IN_CHROMA,            VC1_PIXEL_IN_CHROMA*2,
                                                                    CurrBlock + 4*64,  VC1_PIXEL_IN_CHROMA*2,
                                                                    UPlane,            UPitch);

                        /////////////////////////////
                        //V top horizontal smoothing
                        _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(CurrBlock - MaxWidth*8*8*6 + 64*5 +6*VC1_PIXEL_IN_CHROMA,           VC1_PIXEL_IN_CHROMA*2,
                                                                    CurrBlock + 64*5, VC1_PIXEL_IN_CHROMA*2,
                                                                    VPlane,           VPitch);
                    }

                    //copy last two srings of Left macroblock to SmoothUpperRows
                }
                if ( j< (Height-1))
                {
                    CurrBlock  += 8*8*6;
                    pCurrMB++;
                    CurrBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
                    pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);

                    YPlane = pCurrMB->currYPlane;
                    UPlane = pCurrMB->currUPlane;
                    VPlane = pCurrMB->currVPlane;

                    CurrIntra    = (pCurrMB)->IntraFlag*pCurrMB->Overlap;
                    LeftIntra    = (pCurrMB - 1)->IntraFlag*(pCurrMB - 1)->Overlap;
                    TopLeftIntra = (pCurrMB - MaxWidth - 1)->IntraFlag*(pCurrMB - MaxWidth - 1)->Overlap;
                }
            }
            else
            {

                if(CurrIntra)
                {
                   CurrFieldFlag = (pCurrMB->FIELDTX)<<1 | pCurrMB->FIELDTX;
                    ////////////////////////////
                    //first block in row
                    //internal vertical smoothing
                    EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_1_INTRA) * (IPPVC_EDGE_HALF_1))
                                    |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_2_3_INTRA) * (IPPVC_EDGE_HALF_2));

                    if(EdgeDisabledFlag)
                        _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6, VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8, VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8,    YPitch,
                                                                CurrFieldFlag, EdgeDisabledFlag);
                }

                for (i = 1; i < Width; i++)
                {
                    LeftCurrFieldFlag = pCurrMB->FIELDTX<<1;

                    CurrBlock  += 8*8*6;
                    pCurrMB++;

                    YPlane = pCurrMB->currYPlane;
                    UPlane = pCurrMB->currUPlane;
                    VPlane = pCurrMB->currVPlane;

                    CurrIntra    = (pCurrMB)->IntraFlag*pCurrMB->Overlap;
                    LeftIntra    = (pCurrMB - 1)->IntraFlag*(pCurrMB - 1)->Overlap;

                    CurrFieldFlag = (pCurrMB->FIELDTX)<<1 | pCurrMB->FIELDTX;
                    LeftCurrFieldFlag |= pCurrMB->FIELDTX;


                    //LUMA
                    if(CurrIntra)
                    {
                        /////////////////////////////
                        //internal vertical smoothing
                        EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_1_INTRA) * (IPPVC_EDGE_HALF_1))
                                        |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_2_3_INTRA) * (IPPVC_EDGE_HALF_2));

                        if(EdgeDisabledFlag)
                            _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock + 6,       VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock + 8,        VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane + 8,           YPitch,
                                                                    CurrFieldFlag,        EdgeDisabledFlag);

                        if(LeftIntra)
                        {
                            ///////////////////////////////////
                            //left boundary vertical smoothing
                            EdgeDisabledFlag =(((VC1_EDGE_MB(CurrIntra,VC1_BLOCK_0_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_1_INTRA)))
                                    *(IPPVC_EDGE_HALF_1))|
                                    (((VC1_EDGE_MB(CurrIntra,VC1_BLOCK_2_INTRA)) && (VC1_EDGE_MB(LeftIntra, VC1_BLOCK_3_INTRA)))
                                    *(IPPVC_EDGE_HALF_2));

                            if(EdgeDisabledFlag)
                                _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*6+14, VC1_PIXEL_IN_LUMA*2,
                                                                        CurrBlock,             VC1_PIXEL_IN_LUMA*2,
                                                                        YPlane,                YPitch,
                                                                        LeftCurrFieldFlag,     EdgeDisabledFlag);
                        }
                        //CHROMA
                        if(((LeftIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA) && ((CurrIntra & VC1_BLOCK_4_INTRA) == VC1_BLOCK_4_INTRA))
                        {
                            ///////////////////////
                            //U vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8*2 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*4,     VC1_PIXEL_IN_CHROMA*2,
                                                                        UPlane,                UPitch);
                            //////////////////////
                            //V vertical smoothing
                            _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(CurrBlock - 8*8 + 6, VC1_PIXEL_IN_CHROMA*2,
                                                                        CurrBlock + 8*8*5,    VC1_PIXEL_IN_CHROMA*2,
                                                                        VPlane,               VPitch);
                        }
                    }

                    if(LeftIntra && (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
                    {
                        ///////////////////////////////////
                        //left MB internal horizontal edge
                         EdgeDisabledFlag = (VC1_EDGE_MB(LeftIntra,VC1_BLOCKS_0_2_INTRA) * (IPPVC_EDGE_HALF_1))
                                           |(VC1_EDGE_MB(LeftIntra,VC1_BLOCKS_1_3_INTRA) * (IPPVC_EDGE_HALF_2));
                        _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock - 8*8*4 - 32, VC1_PIXEL_IN_LUMA*2,
                                                                    CurrBlock - 8*8*4,      VC1_PIXEL_IN_LUMA*2,
                                                                    YPlane - 16 + 8*YPitch, YPitch,
                                                                    EdgeDisabledFlag);

                        //copy last two srings of Left macroblock to SmoothUpperRows
                    }
                }

                //RIGHT MB
                //LUMA
                if(CurrIntra && (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
                {
                    /////////////////////////////
                    //MB internal horizontal edge
                    EdgeDisabledFlag = (VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_0_2_INTRA) * (IPPVC_EDGE_HALF_1))
                                    |(VC1_EDGE_MB(CurrIntra,VC1_BLOCKS_1_3_INTRA) * (IPPVC_EDGE_HALF_2));
                    _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(CurrBlock + 8*8*2 - 32, VC1_PIXEL_IN_LUMA*2,
                                                                CurrBlock + 8*8*2,      VC1_PIXEL_IN_LUMA*2,
                                                                YPlane + 8*YPitch, YPitch,
                                                                EdgeDisabledFlag);
                }

                CurrBlock  += 8*8*6;
                pCurrMB++;
                CurrBlock += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB)*8*8*6;
                pCurrMB += (pContext->m_seqLayerHeader.MaxWidthMB-pContext->m_pSingleMB->widthMB);
                YPlane = pCurrMB->currYPlane;
                UPlane = pCurrMB->currUPlane;
                VPlane = pCurrMB->currVPlane;

                CurrIntra    = (pCurrMB)->IntraFlag*pCurrMB->Overlap;
                LeftIntra    = (pCurrMB - 1)->IntraFlag*(pCurrMB - 1)->Overlap;
            }
        }
    }
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
