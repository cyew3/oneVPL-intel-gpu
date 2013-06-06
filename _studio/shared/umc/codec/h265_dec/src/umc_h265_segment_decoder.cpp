/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_bitstream_inlines.h"
#include "umc_h265_segment_decoder_templates.h"
#include "umc_h265_bitstream.h"
#include "h265_tr_quant.h"
#include "vm_event.h"
#include "vm_thread.h"

#define COEFBLOCK_OPT

namespace UMC_HEVC_DECODER
{

H265SegmentDecoder::H265SegmentDecoder(TaskBroker_H265 * pTaskBroker)
{
    m_pCoefficientsBuffer = NULL;
    m_nAllocatedCoefficientsBuffer = 0;
    m_pTaskBroker = pTaskBroker;

    m_pSlice = 0;

    bit_depth_luma = 8;
    bit_depth_chroma = 8;

    //h265 members from TDecCu:
    m_curCU = NULL;
    m_ppcCU = NULL;
    m_TopNgbrs = NULL;
    m_TopMVInfo = NULL;
    m_CurrCTBFlags = NULL;
    m_CurrCTB = NULL;
    m_CurrCTBStride = 0;

    // prediction buffer
    m_Prediction.reset(new H265Prediction()); //PREDICTION

    m_ppcYUVResi = 0;
    m_TrQuant = 0;

    m_MaxDepth = 0;

#ifdef COEFBLOCK_OPT
    g_SigLastScan_inv[0][0] = 0;
#endif

} // H265SegmentDecoder::H265SegmentDecoder(H264SliceStore &Store)

H265SegmentDecoder::~H265SegmentDecoder(void)
{
    Release();
} // H265SegmentDecoder::~H265SegmentDecoder(void)

/**
 \param    uiMaxDepth    total number of allowable depth
 \param    uiMaxWidth    largest CU width
 \param    uiMaxHeight   largest CU height
 */

void H265SegmentDecoder::create(H265SeqParamSet* pSPS)
{
    Ipp32u MaxDepth = pSPS->MaxCUDepth;
    Ipp32u MaxWidth = pSPS->MaxCUWidth;
    Ipp32u MaxHeight = pSPS->MaxCUHeight;

    m_DecodeDQPFlag = false;
    
    if (m_MaxDepth != MaxDepth + 1)
    {
        delete [] m_ppcCU;
        m_ppcCU = new H265CodingUnit* [MaxDepth];
    }
    
    if (m_MaxDepth != MaxDepth + 1)
    {
        m_MaxDepth = MaxDepth + 1;
        for (Ipp32u ind = 0; ind < MaxDepth; ind++)
        {
            Ipp32u NumPartitions = 1 << ((m_MaxDepth - ind - 1) << 1);
            Ipp32u Width = MaxWidth  >> ind;
            Ipp32u Height = MaxHeight >> ind;

            m_ppcCU[ind] = new H265CodingUnit;
            m_ppcCU[ind]->create(NumPartitions, Width, Height, true, MaxWidth >> (m_MaxDepth - 1));
        }
    }

    m_MaxDepth = MaxDepth + 1;

    if (!m_ppcYUVResi || m_ppcYUVResi->lumaSize().width != MaxWidth || m_ppcYUVResi->lumaSize().height != MaxHeight)
    {
        // initialize partition order.
        Ipp32u* Tmp = &g_ZscanToRaster[0];
        initZscanToRaster(m_MaxDepth, 1, 0, Tmp);
        initRasterToZscan(MaxWidth, MaxHeight, m_MaxDepth );

        // initialize conversion matrix from partition index to pel
        initRasterToPelXY(MaxWidth, MaxHeight, m_MaxDepth);

        if (!m_ppcYUVResi)
        {
            m_ppcYUVResi = new H265DecYUVBufferPadded;
            m_ppcYUVReco = new H265DecYUVBufferPadded;
        }
        else
        {
            m_ppcYUVResi->destroy();
            m_ppcYUVReco->destroy();
        }

        m_ppcYUVResi->create(MaxWidth, MaxHeight, sizeof(Ipp16s), sizeof(Ipp16s), g_MaxCUWidth, g_MaxCUHeight, g_MaxCUDepth);
        m_ppcYUVReco->create(MaxWidth, MaxHeight, sizeof(Ipp16s), sizeof(Ipp16s), g_MaxCUWidth, g_MaxCUHeight, g_MaxCUDepth);
    }

    if (!m_TrQuant)
        m_TrQuant = new H265TrQuant(); //TRQUANT

    m_TrQuant->Init(g_MaxCUWidth, g_MaxCUHeight, m_pSeqParamSet->m_maxTrSize);

    if (m_TopNgbrsHolder.size() < pSPS->NumPartitionsInFrameWidth + 2 || m_TopMVInfoHolder.size() < pSPS->NumPartitionsInFrameWidth + 2)
    {
        // Zero element is left-top diagonal from zero CTB
        m_TopNgbrsHolder.resize(pSPS->NumPartitionsInFrameWidth + 2);
        m_TopNgbrs = &m_TopNgbrsHolder[1];
        // Zero element is left-top diagonal from zero CTB
        m_TopMVInfoHolder.resize(pSPS->NumPartitionsInFrameWidth + 2);
        m_TopMVInfo = &m_TopMVInfoHolder[1];
    }

    m_CurrCTBStride = (pSPS->NumPartitionsInCUSize + 2);
    if (m_CurrCTBHolder.size() < m_CurrCTBStride * m_CurrCTBStride)
    {
        m_CurrCTBHolder.resize(m_CurrCTBStride * m_CurrCTBStride);
        m_CurrCTB = &m_CurrCTBHolder[m_CurrCTBStride + 1];
        m_CurrCTBFlagsHolder.resize(m_CurrCTBStride * m_CurrCTBStride);
        m_CurrCTBFlags = &m_CurrCTBFlagsHolder[m_CurrCTBStride + 1];
    }

    m_SAO.init(pSPS);

#ifdef COEFBLOCK_OPT
    if (!g_SigLastScan_inv[0][0])
    {
        for(int i = 0; i < 3; i++)
        {
            g_SigLastScan_inv[i][0] = (Ipp16u*)malloc(2*16);
            g_SigLastScan_inv[i][1] = (Ipp16u*)malloc(2*64);
            g_SigLastScan_inv[i][2] = (Ipp16u*)malloc(2*16*16);
            g_SigLastScan_inv[i][3] = (Ipp16u*)malloc(2*32*32);
            for(int j = 0; j < 16; j++)
            {
                g_SigLastScan_inv[i][0][g_SigLastScan[i][0][j]] = j;
            }
            for(int j = 0; j < 64; j++)
            {
                g_SigLastScan_inv[i][1][g_SigLastScan[i][1][j]] = j;
            }
            for(int j = 0; j < 16*16; j++)
            {
                g_SigLastScan_inv[i][2][g_SigLastScan[i][2][j]] = j;
            }
            for(int j = 0; j < 32*32; j++)
            {
               g_SigLastScan_inv[i][3][g_SigLastScan[i][3][j]] = j;
            }
        }
    }
#endif
}

void H265SegmentDecoder::destroy()
{
    m_SAO.destroy();
}

void H265SegmentDecoder::Release(void)
{
    if (m_pCoefficientsBuffer)
        delete [] (Ipp32s*)m_pCoefficientsBuffer;

    m_pCoefficientsBuffer = NULL;
    m_nAllocatedCoefficientsBuffer = 0;

    delete m_TrQuant;
    m_TrQuant = NULL; //TRQUANT

#ifdef COEFBLOCK_OPT
    if (g_SigLastScan_inv[0][0])
    {
        for(int i = 0; i < 3; i++)
        {
            free(g_SigLastScan_inv[i][0]);
            free(g_SigLastScan_inv[i][1]);
            free(g_SigLastScan_inv[i][2]);
            free(g_SigLastScan_inv[i][3]);
        }
    }

   g_SigLastScan_inv[0][0] = 0;
#endif

    if (m_ppcCU)
    {
        for (Ipp32u ind = 0; ind < m_MaxDepth - 1; ind++)
        {
            m_ppcCU[ind]->destroy();
            delete m_ppcCU[ind];
            m_ppcCU[ind] = NULL;
        }

        delete [] m_ppcCU;
        m_ppcCU = NULL;
    }

    if (m_ppcYUVResi)
    {
        m_ppcYUVResi->destroy();
        delete m_ppcYUVResi;
        m_ppcYUVResi = NULL;

        m_ppcYUVReco->destroy();
        delete m_ppcYUVReco;
        m_ppcYUVReco = NULL;
    }

} // void H265SegmentDecoder::Release(void)

UMC::Status H265SegmentDecoder::Init(Ipp32s iNumber)
{
    // release object before initialization
    Release();

    // save ordinal number
    m_iNumber = iNumber;

    // ADB
    m_pCoefficientsBuffer = (H265CoeffsPtrCommon)(new Ipp32s[COEFFICIENTS_BUFFER_SIZE_H265 + UMC::DEFAULT_ALIGN_VALUE]);
    m_nAllocatedCoefficientsBuffer = 1;

    return UMC::UMC_OK;

} // Status H265SegmentDecoder::Init(Ipp32s sNumber)

H265CoeffsPtrCommon H265SegmentDecoder::GetCoefficientsBuffer(Ipp32u nNum)
{
    return UMC::align_pointer<H265CoeffsPtrCommon> (m_pCoefficientsBuffer +
                                     COEFFICIENTS_BUFFER_SIZE_H265 * nNum, UMC::DEFAULT_ALIGN_VALUE);

} // Ipp16s *H265SegmentDecoder::GetCoefficientsBuffer(Ipp32u nNum)

SegmentDecoderHPBase_H265 *CreateSD_H265(Ipp32s bit_depth_luma,
                               Ipp32s bit_depth_chroma,
                               bool is_field,
                               Ipp32s color_format,
                               bool is_high_profile)
{
    if (bit_depth_chroma > 8 || bit_depth_luma > 8)
    {
        return CreateSD_ManyBits_H265(bit_depth_luma,
                                 bit_depth_chroma,
                                 is_field,
                                 color_format,
                                 is_high_profile);
    }
    else
    {
        if (is_field)
        {
            return CreateSegmentDecoderWrapper<Ipp16s, Ipp8u, Ipp8u, true>::CreateSoftSegmentDecoder(color_format, is_high_profile);
        } else {
            return CreateSegmentDecoderWrapper<Ipp16s, Ipp8u, Ipp8u, false>::CreateSoftSegmentDecoder(color_format, is_high_profile);
        }
    }

} // SegmentDecoderHPBase_H265 *CreateSD(Ipp32s bit_depth_luma,

static
void InitializeSDCreator()
{
    CreateSegmentDecoderWrapper<Ipp16s, Ipp8u, Ipp8u, true>::CreateSoftSegmentDecoder(0, false);
    CreateSegmentDecoderWrapper<Ipp16s, Ipp8u, Ipp8u, false>::CreateSoftSegmentDecoder(0, false);
}

class SDInitializer
{
public:
    SDInitializer()
    {
        InitializeSDCreator();
        InitializeSDCreator_ManyBits_H265();
    }
};

static SDInitializer tableInitializer;

void H265SegmentDecoder::parseSaoMaxUvlc(Ipp32u& val, Ipp32u maxSymbol)
{
    if (maxSymbol == 0)
    {
        val = 0;
        return;
    }

    Ipp32u code;
    Ipp32s  i;

    code = m_pBitStream->DecodeSingleBinEP_CABAC();

    if (code == 0)
    {
        val = 0;
        return;
    }

    i = 1;
    while (1)
    {
        code = m_pBitStream->DecodeSingleBinEP_CABAC();

        if (code == 0)
        {
            break;
        }

        i++;

        if (i == maxSymbol)
        {
            break;
        }
    }

    val = i;
}

void H265SegmentDecoder::parseSaoUflc(Ipp32s length, Ipp32u&  riVal)
{
    riVal = m_pBitStream->DecodeBypassBins_CABAC(length);
}

void H265SegmentDecoder::parseSaoMerge(Ipp32u&  ruiVal)
{
    ruiVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SAO_MERGE_FLAG_HEVC]);
}

void H265SegmentDecoder::parseSaoTypeIdx(Ipp32u&  ruiVal)
{
    Ipp32u uiCode;
    uiCode = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SAO_TYPE_IDX_HEVC]);

    if (uiCode == 0)
    {
        ruiVal = 0;
    }
    else
    {
        uiCode = m_pBitStream->DecodeSingleBinEP_CABAC();

        if (uiCode == 0)
        {
            ruiVal = 5;
        }
        else
        {
            ruiVal = 1;
        }
    }
}

void H265SegmentDecoder::parseSaoOffset(SAOLCUParam* psSaoLcuParam, Ipp32u compIdx)
{
    Ipp32u uiSymbol;
    static Ipp32s iTypeLength[MAX_NUM_SAO_TYPE] =
    {
        SAO_EO_LEN,
        SAO_EO_LEN,
        SAO_EO_LEN,
        SAO_EO_LEN,
        SAO_BO_LEN
    };

    if (compIdx==2)
    {
        uiSymbol = (Ipp32u)( psSaoLcuParam->m_typeIdx + 1);
    }
    else
    {
        parseSaoTypeIdx(uiSymbol);
    }

    psSaoLcuParam->m_typeIdx = (Ipp32s)uiSymbol - 1;

    if (uiSymbol)
    {
        psSaoLcuParam->m_length = iTypeLength[psSaoLcuParam->m_typeIdx];

        Ipp32s bitDepth = compIdx ? g_bitDepthC : g_bitDepthY;
        Ipp32s offsetTh = 1 << IPP_MIN(bitDepth - 5, 5);

        if( psSaoLcuParam->m_typeIdx == SAO_BO )
        {
            for (Ipp32s i = 0; i < psSaoLcuParam->m_length; i++)
            {
                parseSaoMaxUvlc(uiSymbol, offsetTh -1 );
                psSaoLcuParam->m_offset[i] = uiSymbol;
            }

            for (Ipp32s i = 0; i < psSaoLcuParam->m_length; i++)
            {
                if (psSaoLcuParam->m_offset[i] != 0)
                {
                    uiSymbol = m_pBitStream->DecodeSingleBinEP_CABAC();

                    if (uiSymbol)
                    {
                        psSaoLcuParam->m_offset[i] = -psSaoLcuParam->m_offset[i] ;
                    }
                }
            }
            parseSaoUflc(5, uiSymbol);
            psSaoLcuParam->m_subTypeIdx = uiSymbol;
        }
        else if( psSaoLcuParam->m_typeIdx < 4 )
        {
            parseSaoMaxUvlc(uiSymbol, offsetTh -1 ); psSaoLcuParam->m_offset[0] = (Ipp32s)uiSymbol;
            parseSaoMaxUvlc(uiSymbol, offsetTh -1 ); psSaoLcuParam->m_offset[1] = (Ipp32s)uiSymbol;
            parseSaoMaxUvlc(uiSymbol, offsetTh -1 ); psSaoLcuParam->m_offset[2] = -(Ipp32s)uiSymbol;
            parseSaoMaxUvlc(uiSymbol, offsetTh -1 ); psSaoLcuParam->m_offset[3] = -(Ipp32s)uiSymbol;
            if (compIdx != 2)
            {
                parseSaoUflc(2, uiSymbol );
                psSaoLcuParam->m_subTypeIdx = (Ipp32s)uiSymbol;
                psSaoLcuParam->m_typeIdx += psSaoLcuParam->m_subTypeIdx;
            }
        }
    }
    else
    {
        psSaoLcuParam->m_length = 0;
    }
}

inline void copySaoOneLcuParam(SAOLCUParam* dst,  SAOLCUParam* src)
{
    Ipp32s i;
    dst->m_typeIdx = src->m_typeIdx;

    if (dst->m_typeIdx != -1)
    {
        dst->m_subTypeIdx = src->m_subTypeIdx;
        dst->m_length = src->m_length;
        for (i = 0; i < dst->m_length; i++)
        {
            dst->m_offset[i] = src->m_offset[i];
        }
    }
    else
    {
        dst->m_length = 0;
        for (i = 0; i < SAO_BO_LEN; i++)
        {
            dst->m_offset[i] = 0;
        }
    }
}

void H265SegmentDecoder::DecodeSAOOneLCU(H265CodingUnit* pCU)
{
    if (m_pSeqParamSet->sample_adaptive_offset_enabled_flag && (m_pSliceHeader->m_SaoEnabledFlag ||
                                     m_pSliceHeader->m_SaoEnabledFlagChroma))
    {
        H265DecoderFrame* m_Frame = pCU->m_Frame;
        SAOParams *saoParam  = &m_pSliceHeader->m_SAOParam;
        Ipp32u curCUAddr = pCU->CUAddr;
        Ipp32s numCuInWidth  = m_Frame->m_CodingData->m_WidthInCU;
        Ipp32s cuAddrInSlice = curCUAddr - m_Frame->m_CodingData->getCUOrderMap(m_pSliceHeader->SliceCurStartCUAddr/m_Frame->getCD()->getNumPartInCU());
        Ipp32s cuAddrUpInSlice  = cuAddrInSlice - numCuInWidth;
        Ipp32s rx = curCUAddr % numCuInWidth;
        Ipp32s ry = curCUAddr / numCuInWidth;
        Ipp32s allowMergeLeft = 1;
        Ipp32s allowMergeUp   = 1;

        saoParam->m_bSaoFlag[0] = m_pSliceHeader->m_SaoEnabledFlag;

        //if (curCUAddr == iStartCUAddr)
        {
            saoParam->m_bSaoFlag[1] = m_pSliceHeader->m_SaoEnabledFlagChroma;
        }

        if (rx != 0)
        {
            if (m_Frame->m_CodingData->getTileIdxMap(curCUAddr - 1) != m_Frame->m_CodingData->getTileIdxMap(curCUAddr))
            {
                allowMergeLeft = 0;
            }
        }

        if (ry != 0)
        {
            if (m_Frame->m_CodingData->getTileIdxMap(curCUAddr - numCuInWidth) != m_Frame->m_CodingData->getTileIdxMap(curCUAddr))
            {
                allowMergeUp = 0;
            }
        }

        parseSaoOneLcuInterleaving(rx, ry, saoParam, pCU, cuAddrInSlice, cuAddrUpInSlice, allowMergeLeft, allowMergeUp);
    }
}

void H265SegmentDecoder::parseSaoOneLcuInterleaving(Ipp32s rx,
                                                    Ipp32s ry,
                                                    SAOParams* pSaoParam,
                                                    H265CodingUnit* pcCU,
                                                    Ipp32s iCUAddrInSlice,
                                                    Ipp32s iCUAddrUpInSlice,
                                                    Ipp32s allowMergeLeft,
                                                    Ipp32s allowMergeUp)
{
    Ipp32s iAddr = pcCU->CUAddr;
    Ipp32u uiSymbol;
    SAOLCUParam* saoLcuParam[3];

    saoLcuParam[0] = pcCU->m_Frame->m_saoLcuParam[0];
    saoLcuParam[1] = pcCU->m_Frame->m_saoLcuParam[1];
    saoLcuParam[2] = pcCU->m_Frame->m_saoLcuParam[2];

    for (Ipp32s iCompIdx=0; iCompIdx<3; iCompIdx++)
    {
        saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag    = 0;
        saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag  = 0;
        saoLcuParam[iCompIdx][iAddr].m_subTypeIdx     = 0;
        saoLcuParam[iCompIdx][iAddr].m_typeIdx        = -1;
        saoLcuParam[iCompIdx][iAddr].m_offset[0]     = 0;
        saoLcuParam[iCompIdx][iAddr].m_offset[1]     = 0;
        saoLcuParam[iCompIdx][iAddr].m_offset[2]     = 0;
        saoLcuParam[iCompIdx][iAddr].m_offset[3]     = 0;

    }

    if (pSaoParam->m_bSaoFlag[0] || pSaoParam->m_bSaoFlag[1])
    {
        if (rx>0 && iCUAddrInSlice!=0 && allowMergeLeft)
        {
            parseSaoMerge(uiSymbol);
            saoLcuParam[0][iAddr].m_mergeLeftFlag = (bool)uiSymbol;
        }
        if (saoLcuParam[0][iAddr].m_mergeLeftFlag==0)
        {
            if ((ry > 0) && (iCUAddrUpInSlice>=0) && allowMergeUp)
            {
                parseSaoMerge(uiSymbol);
                saoLcuParam[0][iAddr].m_mergeUpFlag = (bool)uiSymbol;
            }
        }
    }

    for (Ipp32s iCompIdx=0; iCompIdx<3; iCompIdx++)
    {
        if ((iCompIdx == 0  && pSaoParam->m_bSaoFlag[0]) || (iCompIdx > 0  && pSaoParam->m_bSaoFlag[1]) )
        {
            if (rx>0 && iCUAddrInSlice!=0 && allowMergeLeft)
            {
                saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag = saoLcuParam[0][iAddr].m_mergeLeftFlag;
            }
            else
            {
                saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag = 0;
            }

            if (saoLcuParam[iCompIdx][iAddr].m_mergeLeftFlag==0)
            {
                if ((ry > 0) && (iCUAddrUpInSlice>=0) && allowMergeUp)
                {
                    saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag = saoLcuParam[0][iAddr].m_mergeUpFlag;
                }
                else
                {
                    saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag = 0;
                }
                if (!saoLcuParam[iCompIdx][iAddr].m_mergeUpFlag)
                {
                    saoLcuParam[2][iAddr].m_typeIdx = saoLcuParam[1][iAddr].m_typeIdx;
                    parseSaoOffset(&(saoLcuParam[iCompIdx][iAddr]), iCompIdx);
                }
                else
                {
                    copySaoOneLcuParam(&saoLcuParam[iCompIdx][iAddr], &saoLcuParam[iCompIdx][iAddr-pcCU->m_Frame->m_CodingData->m_WidthInCU]);
                }
            }
            else
            {
                copySaoOneLcuParam(&saoLcuParam[iCompIdx][iAddr],  &saoLcuParam[iCompIdx][iAddr-1]);
            }
        }
        else
        {
            saoLcuParam[iCompIdx][iAddr].m_typeIdx = -1;
            saoLcuParam[iCompIdx][iAddr].m_subTypeIdx = 0;
        }
    }
}

void H265SegmentDecoder::ReadUnaryMaxSymbolCABAC(Ipp32u& uVal, Ipp32u CtxIdx, Ipp32s Offset, Ipp32u MaxSymbol)
{
    if (0 == MaxSymbol)
    {
        uVal = 0;
        return;
    }

    uVal = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx);
    //m_pcTDecBinIf->decodeBin( ruiSymbol, pcSCModel[0] );

    if (uVal == 0 || MaxSymbol == 1)
    {
        return;
    }

    Ipp32u val = 0;
    Ipp32u cont;

    do
    {
        cont = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx + Offset);
        //m_pcTDecBinIf->decodeBin( uiCont, pcSCModel[ iOffset ] );
        val++;
    }
    while( cont && ( val < MaxSymbol - 1 ) );

    if ( cont && ( val == MaxSymbol - 1 ) )
    {
        val++;
    }

    uVal = val;
}

void H265SegmentDecoder::DecodeCUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast)
{
    Ipp32u CurNumParts = m_pCurrentFrame->getCD()->getNumPartInCU() >> (Depth << 1);
    Ipp32u QNumParts = CurNumParts >> 2;

    bool boundaryFlag = false;
    Ipp32u LPelX = pCU->m_CUPelX + g_RasterToPelX[ g_ZscanToRaster[AbsPartIdx] ];
    Ipp32u RPelX = LPelX + (g_MaxCUWidth >> Depth) - 1;
    Ipp32u TPelY = pCU->m_CUPelY + g_RasterToPelY[ g_ZscanToRaster[AbsPartIdx] ];
    Ipp32u BPelY = TPelY + (g_MaxCUHeight >> Depth) - 1;

    bool bStartInCU = pCU->getSCUAddr() + AbsPartIdx + CurNumParts > m_pSliceHeader->m_sliceSegmentCurStartCUAddr && pCU->getSCUAddr() + AbsPartIdx < m_pSliceHeader->m_sliceSegmentCurStartCUAddr;
    if ((!bStartInCU) && (RPelX < m_pSeqParamSet->pic_width_in_luma_samples) && (BPelY < m_pSeqParamSet->pic_height_in_luma_samples))
    {
        DecodeSplitFlagCABAC(pCU, AbsPartIdx, Depth);
    }
    else
    {
        boundaryFlag = true;
    }

    if (((Depth < pCU->m_DepthArray[AbsPartIdx]) && (Depth < g_MaxCUDepth - g_AddCUDepth)) || boundaryFlag)
    {
        Ipp32u Idx = AbsPartIdx;
        if ((g_MaxCUWidth >> Depth) == m_pPicParamSet->MinCUDQPSize && m_pPicParamSet->cu_qp_delta_enabled_flag)
        {
            m_DecodeDQPFlag = true;
            pCU->setQPSubParts( pCU->getRefQP(AbsPartIdx), AbsPartIdx, Depth ); // set QP to default QP
        }

        for ( Ipp32u PartUnitIdx = 0; PartUnitIdx < 4; PartUnitIdx++ )
        {
            LPelX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[Idx]];
            TPelY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[Idx]];

            bool SubInSliceFlag = pCU->getSCUAddr() + Idx + QNumParts > m_pSliceHeader->m_sliceSegmentCurStartCUAddr;
            if (SubInSliceFlag)

            {
                if ((LPelX < m_pSeqParamSet->pic_width_in_luma_samples) && (TPelY < m_pSeqParamSet->pic_height_in_luma_samples))
                {
                    DecodeCUCABAC(pCU, Idx, Depth + 1, IsLast);
                }
                else
                {
                    pCU->setOutsideCUPart(Idx, Depth + 1);
                }
            }
            if (IsLast)
            {
                break;
            }

            Idx += QNumParts;
        }
        if ((g_MaxCUWidth >> Depth) == m_pPicParamSet->MinCUDQPSize && m_pPicParamSet->cu_qp_delta_enabled_flag)
        {
            if (m_DecodeDQPFlag)
            {
                Ipp32u QPSrcPartIdx;
                if (pCU->getSliceSegmentStartCU(AbsPartIdx) != m_pSliceHeader->m_sliceSegmentCurStartCUAddr)
                {
                    QPSrcPartIdx = m_pSliceHeader->m_sliceSegmentCurStartCUAddr % m_pCurrentFrame->getCD()->getNumPartInCU();
                }
                else
                {
                    QPSrcPartIdx = AbsPartIdx;
                }
                pCU->setQPSubParts(pCU->getRefQP(QPSrcPartIdx), AbsPartIdx, Depth); // set QP to default QP
            }
        }
        return;
    }
    if ((g_MaxCUWidth >> Depth) >= m_pPicParamSet->MinCUDQPSize && m_pPicParamSet->cu_qp_delta_enabled_flag)
    {
        m_DecodeDQPFlag = true;
        pCU->setQPSubParts(pCU->getRefQP(AbsPartIdx), AbsPartIdx, Depth ); // set QP to default QP
    }

    if (m_pPicParamSet->transquant_bypass_enable_flag)
    {
        DecodeCUTransquantBypassFlag(pCU, AbsPartIdx, Depth);
    }

    // decode CU mode and the partition size
    if (m_pSliceHeader->slice_type != I_SLICE)
    {
        DecodeSkipFlagCABAC(pCU, AbsPartIdx, Depth);
    }

    if (pCU->isSkipped(AbsPartIdx))
    {
        MVBuffer MvBufferNeighbours[MRG_MAX_NUM_CANDS << 1]; // double length for mv of both lists
        Ipp8u InterDirNeighbours[MRG_MAX_NUM_CANDS];
        Ipp32s numValidMergeCand = 0;

        for (Ipp32u ui = 0; ui < pCU->m_SliceHeader->m_MaxNumMergeCand; ++ui)
        {
            InterDirNeighbours[ui] = 0;
        }
        DecodeMergeIndexCABAC(pCU, 0, AbsPartIdx, Depth);
        Ipp32s MergeIndex = pCU->m_MergeIndex[AbsPartIdx];
        getInterMergeCandidates(pCU, AbsPartIdx, 0, MvBufferNeighbours, InterDirNeighbours, numValidMergeCand, MergeIndex);
        pCU->setInterDirSubParts(InterDirNeighbours[MergeIndex], AbsPartIdx, 0, Depth);

        H265MotionVector TmpMV(0, 0);
        Ipp32s PartWidth, PartHeight;
        pCU->getPartSize(AbsPartIdx, 0, PartWidth, PartHeight);

        Ipp32s PartX = LPelX >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
        Ipp32s PartY = TPelY >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
        PartWidth >>= m_pSeqParamSet->getQuadtreeTULog2MinSize();
        PartHeight >>= m_pSeqParamSet->getQuadtreeTULog2MinSize();

        H265MotionVector MV[2];
        RefIndexType RefIdx[2];
        RefIdx[0] = RefIdx[1] = -1;

        for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
        {
            if (m_pSliceHeader->m_numRefIdx[RefListIdx] > 0)
            {
                pCU->setMVPIdxSubParts(0, EnumRefPicList(RefListIdx), AbsPartIdx, 0, Depth);
                pCU->setMVPNumSubParts(0, EnumRefPicList(RefListIdx), AbsPartIdx, 0, Depth);

                pCU->m_CUMVbuffer[RefListIdx].setAllMVd(TmpMV, SIZE_2Nx2N, AbsPartIdx, Depth, 0);
                pCU->m_CUMVbuffer[RefListIdx].setAllMVBuffer(MvBufferNeighbours[2 * MergeIndex + RefListIdx], SIZE_2Nx2N, AbsPartIdx, Depth, 0);

                MVBuffer &mvb = MvBufferNeighbours[2 * MergeIndex + RefListIdx];
                if (mvb.RefIdx >= 0)
                {
                    RefIdx[RefListIdx] = mvb.RefIdx;
                    MV[RefListIdx] = mvb.MV;
                }
            }
        }

        UpdatePUInfo(pCU, PartX, PartY, PartWidth, PartHeight, RefIdx, MV);

        pCU->setTrStartSubParts(AbsPartIdx, Depth);
        FinishDecodeCU(pCU, AbsPartIdx, Depth, IsLast);
        UpdateNeighborBuffers(pCU, AbsPartIdx);
        return;
    }

    Ipp32s PredMode = DecodePredModeCABAC(pCU, AbsPartIdx, Depth);
    DecodePartSizeCABAC(pCU, AbsPartIdx, Depth);

    if (MODE_INTRA == PredMode)
    {
        // Inter mode is marked later when RefIdx is read from bitstream
        Ipp32s PartX = LPelX >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
        Ipp32s PartY = TPelY >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
        Ipp32s PartWidth = pCU->m_WidthArray[AbsPartIdx] >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
        Ipp32s PartHeight = pCU->m_HeightArray[AbsPartIdx] >> m_pSeqParamSet->getQuadtreeTULog2MinSize();

        pCU->setAllColFlags(COL_TU_INTRA, REF_PIC_LIST_0, PartX, PartY, PartWidth, PartHeight);
    }

    if (MODE_INTRA == pCU->m_PredModeArray[AbsPartIdx] && SIZE_2Nx2N == pCU->m_PartSizeArray[AbsPartIdx])
    {
        DecodeIPCMInfoCABAC(pCU, AbsPartIdx, Depth);
        pCU->setTrStartSubParts(AbsPartIdx, Depth);

        if (pCU->m_IPCMFlag[AbsPartIdx])
        {
            FinishDecodeCU(pCU, AbsPartIdx, Depth, IsLast);
            UpdateNeighborBuffers(pCU, AbsPartIdx);
            return;
        }
    }

    Ipp32u CurrWidth = pCU->m_WidthArray[AbsPartIdx];
    Ipp32u CurrHeight = pCU->m_HeightArray[AbsPartIdx];

    DecodePredInfoCABAC(pCU, AbsPartIdx, Depth);

    // Coefficient decoding
    bool CodeDQPFlag = m_DecodeDQPFlag;
    DecodeCoeff(pCU, AbsPartIdx, Depth, CurrWidth, CurrHeight, CodeDQPFlag);
    m_DecodeDQPFlag = CodeDQPFlag;

    FinishDecodeCU(pCU, AbsPartIdx, Depth, IsLast);
    UpdateNeighborBuffers(pCU, AbsPartIdx);
}

void H265SegmentDecoder::DecodeSplitFlagCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    if (Depth == g_MaxCUDepth - g_AddCUDepth)
    {
        pCU->setDepthSubParts(Depth, AbsPartIdx);
        return;
    }

    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SPLIT_CODING_UNIT_FLAG_HEVC] + getCtxSplitFlag(pCU, AbsPartIdx, Depth));
    pCU->setDepthSubParts( Depth + uVal, AbsPartIdx );

    return;
}

void H265SegmentDecoder::DecodeCUTransquantBypassFlag(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[TRANSQUANT_BYPASS_HEVC]);
    pCU->setCUTransquantBypassSubParts(uVal ? true : false, AbsPartIdx, Depth);
}

void H265SegmentDecoder::DecodeSkipFlagCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    if (I_SLICE == pCU->m_SliceHeader->slice_type)
    {
        return;
    }

    Ipp32u uVal = 0;
    Ipp32u CtxSkip = getCtxSkipFlag(pCU, AbsPartIdx);
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[SKIP_FLAG_HEVC] + CtxSkip);

    if (uVal)
    {
        pCU->setSkipFlagSubParts(true, AbsPartIdx, Depth);
        pCU->setPredModeSubParts(MODE_INTER, AbsPartIdx, Depth);
        pCU->setPartSizeSubParts(SIZE_2Nx2N, AbsPartIdx, Depth);
        pCU->setSizeSubParts(g_MaxCUWidth >> Depth, g_MaxCUHeight >> Depth, AbsPartIdx, Depth);
        pCU->setMergeFlagSubParts(true, AbsPartIdx, 0, Depth);
    }
}


void H265SegmentDecoder::DecodeMergeIndexCABAC(H265CodingUnit* pCU, Ipp32u PartIdx, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u MergeIndex = 0;

    ParseMergeIndexCABAC(pCU, MergeIndex);

    pCU->setMergeIndexSubParts(MergeIndex, AbsPartIdx, PartIdx, Depth);
}

void H265SegmentDecoder::ParseMergeIndexCABAC(H265CodingUnit* pCU, Ipp32u& MergeIndex)
{
    Ipp32u NumCand = MRG_MAX_NUM_CANDS;
    Ipp32u UnaryIdx = 0;
    NumCand = pCU->m_SliceHeader->m_MaxNumMergeCand;

    if (NumCand >= 1)
    {
        for ( ; UnaryIdx < NumCand - 1; ++UnaryIdx)
        {
            Ipp32u uVal = 0;

            if (UnaryIdx == 0)
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MERGE_IDX_HEVC]);
            else
                uVal = m_pBitStream->DecodeSingleBinEP_CABAC();

            if (0 == uVal)
                break;
        }
    }
    MergeIndex = UnaryIdx;
}


Ipp32s H265SegmentDecoder::DecodeMVPIdxPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartAddr, Ipp32u Depth, Ipp32u PartIdx, EnumRefPicList RefList, H265MotionVector &MV)
{
    Ipp32s MVPIdx = -1;

    H265MotionVector ZeroMV(0, 0);
    MV = ZeroMV;

    AMVPInfo* pAMVPInfo = &(pCU->m_CUMVbuffer[RefList].AMVPinfo);
    Ipp32s RefIdx = pCU->m_CUMVbuffer[RefList].RefIdx[AbsPartAddr];

    if (pCU->m_InterDir[AbsPartAddr] & (1 << RefList))
        ParseMVPIdxCABAC(MVPIdx);

    fillMVPCand(pCU, AbsPartAddr, PartIdx, RefList, RefIdx, pAMVPInfo);
    pCU->setMVPNumSubParts(pAMVPInfo->NumbOfCands, RefList, AbsPartAddr, PartIdx, Depth);
    pCU->setMVPIdxSubParts(MVPIdx, RefList, AbsPartAddr, PartIdx, Depth);
    EnumPartSize PartSize = (EnumPartSize)pCU->m_PartSizeArray[AbsPartAddr];

    if (RefIdx >= 0)
    {
        getMVPredAMVP(pCU, PartIdx, AbsPartAddr, RefList, MV);
        MV = MV + pCU->m_CUMVbuffer[RefList].MVd[AbsPartAddr];
    }

    pCU->m_CUMVbuffer[RefList].setAllMV(MV, PartSize, AbsPartAddr, Depth, PartIdx);
    return RefIdx;
}

void H265SegmentDecoder::ParseMVPIdxCABAC(Ipp32s& MVPIdx)
{
    Ipp32u uVal;
    ReadUnaryMaxSymbolCABAC(uVal, ctxIdxOffsetHEVC[MVP_IDX_HEVC], 1, AMVP_MAX_NUM_CANDS - 1);

    MVPIdx = uVal;
}

void H265SegmentDecoder::getMVPredAMVP(H265CodingUnit* pCU, Ipp32u PartIdx, Ipp32u PartAddr, EnumRefPicList RefPicList, H265MotionVector& m_MVPred)
{
    AMVPInfo* pAMVPInfo = &(pCU->m_CUMVbuffer[RefPicList].AMVPinfo);

    if (pAMVPInfo->NumbOfCands <= 1)
    {
        m_MVPred = pAMVPInfo->MVCandidate[0];

        pCU->setMVPIdxSubParts( 0, RefPicList, PartAddr, PartIdx, pCU->m_DepthArray[PartAddr]);
        pCU->setMVPNumSubParts( pAMVPInfo->NumbOfCands, RefPicList, PartAddr, PartIdx, pCU->m_DepthArray[PartAddr]);
        return;
    }

    VM_ASSERT(pCU->m_MVPIdx[RefPicList][PartAddr] >= 0);
    m_MVPred = pAMVPInfo->MVCandidate[pCU->m_MVPIdx[RefPicList][PartAddr]];
    return;
}


Ipp32s H265SegmentDecoder::DecodePredModeCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32s PredMode = MODE_INTER;

    if (I_SLICE == pCU->m_SliceHeader->slice_type)
    {
        PredMode = MODE_INTRA;
    }
    else
    {
        Ipp32u uVal;
        uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[PRED_MODE_HEVC]);
        PredMode += uVal;
    }

    pCU->setPredModeSubParts(EnumPredMode(PredMode), AbsPartIdx, Depth);
    return PredMode;
}

/** parse partition size
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth
 * \returns Void
 */
void H265SegmentDecoder::DecodePartSizeCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uVal, uMode = 0;
    EnumPartSize Mode;

    if (MODE_INTRA == pCU->m_PredModeArray[AbsPartIdx])
    {
        uVal = 1;
        if (Depth == g_MaxCUDepth - g_AddCUDepth)
        {
            uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[PART_SIZE_HEVC]);
            //m_pcTDecBinIf->decodeBin( uiSymbol, m_cCUPartSizeSCModel.get( 0, 0, 0) );
        }
        Mode = uVal ? SIZE_2Nx2N : SIZE_NxN;
        Ipp32u TrLevel = 0;
        Ipp32u WidthInBit  = g_ConvertToBit[pCU->m_WidthArray[AbsPartIdx]] + 2;
        Ipp32u TrSizeInBit = g_ConvertToBit[pCU->m_SliceHeader->m_SeqParamSet->m_maxTrSize] + 2;
        TrLevel            = WidthInBit >= TrSizeInBit ? WidthInBit - TrSizeInBit : 0;
        if (Mode == SIZE_NxN)
        {
            pCU->setTrIdxSubParts(1 + TrLevel, AbsPartIdx, Depth);
        }
        else
        {
            pCU->setTrIdxSubParts(TrLevel, AbsPartIdx, Depth);
        }
    }
    else
    {
        Ipp32u MaxNumBits = 2;
        if (Depth == g_MaxCUDepth - g_AddCUDepth &&
            !((g_MaxCUWidth >> Depth) == 8 && (g_MaxCUHeight >> Depth) == 8))
        {
            MaxNumBits++;
        }
        for (Ipp32u ui = 0; ui < MaxNumBits; ui++)
        {
            uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[PART_SIZE_HEVC] + ui);
            if (uVal)
            {
                break;
            }
            uMode++;
        }
        Mode = (EnumPartSize) uMode;

        if (pCU->m_SliceHeader->m_SeqParamSet->m_AMPAcc[Depth])
        {
            if (Mode == SIZE_2NxN)
            {
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[AMP_SPLIT_POSITION_HEVC]);

                if (uVal == 0)
                {
                    uVal = m_pBitStream->DecodeSingleBinEP_CABAC();
                    Mode = (uVal == 0? SIZE_2NxnU : SIZE_2NxnD);
                }
            }
            else if (Mode == SIZE_Nx2N)
            {
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[AMP_SPLIT_POSITION_HEVC]);

                if (uVal == 0)
                {
                    uVal = m_pBitStream->DecodeSingleBinEP_CABAC();
                    Mode = (uVal == 0? SIZE_nLx2N : SIZE_nRx2N);
                }
            }
        }
    }
    pCU->setPartSizeSubParts(Mode, AbsPartIdx, Depth);
    pCU->setSizeSubParts(g_MaxCUWidth >> Depth, g_MaxCUHeight >> Depth, AbsPartIdx, Depth);
}

void H265SegmentDecoder::DecodeIPCMInfoCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    if (!pCU->m_SliceHeader->m_SeqParamSet->pcm_enabled_flag
        || (pCU->m_WidthArray[AbsPartIdx] > (1 << pCU->m_SliceHeader->m_SeqParamSet->log2_max_pcm_luma_coding_block_size))
        || (pCU->m_WidthArray[AbsPartIdx] < (1 << pCU->m_SliceHeader->m_SeqParamSet->log2_min_pcm_luma_coding_block_size)))
    {
        return;
    }

    Ipp32u uVal;
    bool readPCMSampleFlag = false;

    uVal = m_pBitStream->DecodeTerminatingBit_CABAC();

    if (uVal)
    {
        readPCMSampleFlag = true;
        DecodePCMAlignBits();
    }

    if (readPCMSampleFlag == true)
    {
        bool IpcmFlag = true;

        pCU->setPartSizeSubParts(SIZE_2Nx2N, AbsPartIdx, Depth);
        pCU->setSizeSubParts(g_MaxCUWidth >> Depth, g_MaxCUHeight >> Depth, AbsPartIdx, Depth);
        pCU->setTrIdxSubParts(0, AbsPartIdx, Depth);
        pCU->setIPCMFlagSubParts(IpcmFlag, AbsPartIdx, Depth);

        Ipp32u MinCoeffSize = pCU->m_Frame->getMinCUWidth() * pCU->m_Frame->getMinCUHeight();
        Ipp32u LumaOffset = MinCoeffSize * AbsPartIdx;
        Ipp32u ChromaOffset = LumaOffset >> 2;

        Ipp32u Width;
        Ipp32u Height;
        Ipp32u SampleBits;
        Ipp32u X, Y;

        // LUMA
        H265PlanePtrYCommon PCMSampleY = pCU->m_IPCMSampleY + LumaOffset;
        Width = pCU->m_WidthArray[AbsPartIdx];
        Height = pCU->m_HeightArray[AbsPartIdx];
        SampleBits = pCU->m_SliceHeader->m_SeqParamSet->getPCMBitDepthLuma();

        for (Y = 0; Y < Height; Y++)
        {
            for (X = 0; X < Width; X++)
            {
                Ipp32u Sample = 0;
                Sample = m_pBitStream->GetBits(SampleBits);
                PCMSampleY[X] = (H265PlaneYCommon)Sample;
            }
            PCMSampleY += Width;
        }

        // CHROMA U
        H265PlanePtrUVCommon PCMSampleUV = pCU->m_IPCMSampleCb + ChromaOffset;
        Width = pCU->m_WidthArray[AbsPartIdx] / 2;
        Height = pCU->m_HeightArray[AbsPartIdx] / 2;
        SampleBits = pCU->m_SliceHeader->m_SeqParamSet->getPCMBitDepthChroma();

        for (Y = 0; Y < Height; Y++)
        {
            for (X = 0; X < Width; X++)
            {
                Ipp32u Sample = 0;
                Sample = m_pBitStream->GetBits(SampleBits);
                PCMSampleUV[X] = (H265PlaneUVCommon)Sample;
            }
            PCMSampleUV += Width;
        }

        // CHROMA V
        PCMSampleUV = pCU->m_IPCMSampleCr + ChromaOffset;
        Width = pCU->m_WidthArray[AbsPartIdx] / 2;
        Height = pCU->m_HeightArray[AbsPartIdx] / 2;
        SampleBits = pCU->m_SliceHeader->m_SeqParamSet->getPCMBitDepthChroma();

        for (Y = 0; Y < Height; Y++)
        {
            for (X = 0; X < Width; X++)
            {
                Ipp32u Sample = 0;
                Sample = m_pBitStream->GetBits(SampleBits);
                PCMSampleUV[X] = (H265PlaneUVCommon)Sample;
            }
            PCMSampleUV += Width;
        }

        m_pBitStream->ResetBac_CABAC();
    }
}

// Decode PCM alignment zero bits.
void H265SegmentDecoder::DecodePCMAlignBits()
{
    Ipp32s iVal = m_pBitStream->getNumBitsUntilByteAligned();

    if (iVal)
        m_pBitStream->GetBits(iVal);
}

void H265SegmentDecoder::DecodePredInfoCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    if (MODE_INTRA == pCU->m_PredModeArray[AbsPartIdx])  // If it is Intra mode, encode intra prediction mode.
    {
        DecodeIntraDirLumaAngCABAC(pCU, AbsPartIdx, Depth);
        DecodeIntraDirChromaCABAC(pCU, AbsPartIdx, Depth);
    }
    else                                                                // if it is Inter mode, encode motion vector and reference index
    {
        DecodePUWiseCABAC(pCU, AbsPartIdx, Depth);
    }
}

void H265SegmentDecoder::DecodeIntraDirLumaAngCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    EnumPartSize mode = pCU->getPartitionSize(AbsPartIdx);
    Ipp32u PartNum = mode == SIZE_NxN ? 4 : 1;
    Ipp32u PartOffset = (pCU->m_Frame->m_CodingData->m_NumPartitions >> (pCU->m_DepthArray[AbsPartIdx] << 1)) >> 2;
    Ipp32u mpmPred[4];
    Ipp32u uVal;
    Ipp32s j, IPredMode;

    if (mode == SIZE_NxN)
        Depth++;

    for (j = 0; j < PartNum; j++)
    {
        uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTRA_LUMA_PRED_MODE_HEVC]);
        mpmPred[j] = uVal;
    }

    for (j = 0; j < PartNum; j++)
    {
        Ipp32s Preds[3] = {-1, -1, -1};
        Ipp32u PredNum = 3;

        getIntraDirLumaPredictor(pCU, AbsPartIdx + PartOffset * j, Preds);

        if (mpmPred[j])
        {
            uVal = m_pBitStream->DecodeSingleBinEP_CABAC();
            if (uVal)
            {
                uVal = m_pBitStream->DecodeSingleBinEP_CABAC();
                uVal++;
            }
            IPredMode = Preds[uVal];
        }
        else
        {
            IPredMode = 0;
            uVal = m_pBitStream->DecodeBypassBins_CABAC(5);
            IPredMode = uVal;

            //postponed sorting of MPMs (only in remaining branch)
            if (Preds[0] > Preds[1])
            {
                std::swap(Preds[0], Preds[1]);
            }
            if (Preds[0] > Preds[2])
            {
                std::swap(Preds[0], Preds[2]);
            }
            if (Preds[1] > Preds[2])
            {
                std::swap(Preds[1], Preds[2]);
            }
            for (Ipp32u i = 0; i < PredNum; i++)
            {
                IPredMode += (IPredMode >= Preds[i]);
            }
        }
        pCU->setLumaIntraDirSubParts(IPredMode, AbsPartIdx + PartOffset * j, Depth);
    }
}

void H265SegmentDecoder::DecodeIntraDirChromaCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uVal;

    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTRA_CHROMA_PRED_MODE_HEVC]);

    //switch codeword
    if (uVal == 0)
    {
        uVal = DM_CHROMA_IDX;
    }
    else
    {
        Ipp32u IPredMode;

        IPredMode = m_pBitStream->DecodeBypassBins_CABAC(2);
        Ipp32u AllowedChromaDir[NUM_CHROMA_MODE];
        pCU->getAllowedChromaDir(AbsPartIdx, AllowedChromaDir);
        uVal = AllowedChromaDir[IPredMode];
    }
    pCU->setChromIntraDirSubParts(uVal, AbsPartIdx, Depth);
    return;
}

void H265SegmentDecoder::DecodePUWiseCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    EnumPartSize PartSize = (EnumPartSize) pCU->m_PartSizeArray[AbsPartIdx];
    Ipp32u NumPU = (PartSize == SIZE_2Nx2N ? 1 : (PartSize == SIZE_NxN ? 4 : 2));
    Ipp32u PUOffset = (g_PUOffset[Ipp32u(PartSize)] << ((m_pSeqParamSet->MaxCUDepth - Depth) << 1)) >> 4;

    MVBuffer MvBufferNeighbours[MRG_MAX_NUM_CANDS << 1]; // double length for mv of both lists
    Ipp8u InterDirNeighbours[MRG_MAX_NUM_CANDS];

    for (Ipp32u ui = 0; ui < pCU->m_SliceHeader->m_MaxNumMergeCand; ui++ )
    {
        InterDirNeighbours[ui] = 0;
    }
    Ipp32s numValidMergeCand = 0;

    for (Ipp32u PartIdx = 0, SubPartIdx = AbsPartIdx; PartIdx < NumPU; PartIdx++, SubPartIdx += PUOffset)
    {
        DecodeMergeFlagCABAC(pCU, SubPartIdx, Depth, PartIdx);

        Ipp32s PartWidth, PartHeight;
        pCU->getPartSize(AbsPartIdx, PartIdx, PartWidth, PartHeight);

        Ipp32s LPelX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[SubPartIdx]];
        Ipp32s TPelY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[SubPartIdx]];
        Ipp32s PartX = LPelX >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
        Ipp32s PartY = TPelY >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
        PartWidth >>= m_pSeqParamSet->getQuadtreeTULog2MinSize();
        PartHeight >>= m_pSeqParamSet->getQuadtreeTULog2MinSize();

        H265MotionVector MV[2];
        RefIndexType RefIdx[2];
        RefIdx[0] = RefIdx[1] = -1;

        if (pCU->m_MergeFlag[SubPartIdx])
        {
            DecodeMergeIndexCABAC(pCU, PartIdx, SubPartIdx, Depth);
            Ipp32u MergeIndex = pCU->m_MergeIndex[SubPartIdx];

            if ((m_pPicParamSet->log2_parallel_merge_level - 2) > 0 && PartSize != SIZE_2Nx2N && pCU->m_WidthArray[AbsPartIdx] <= 8)
            {
                pCU->setPartSizeSubParts(SIZE_2Nx2N, AbsPartIdx, Depth);
                getInterMergeCandidates(pCU, AbsPartIdx, 0, MvBufferNeighbours, InterDirNeighbours, numValidMergeCand, -1);
                pCU->setPartSizeSubParts(PartSize, AbsPartIdx, Depth);
            }
            else
            {
                MergeIndex = pCU->m_MergeIndex[SubPartIdx];
                getInterMergeCandidates(pCU, SubPartIdx, PartIdx, MvBufferNeighbours, InterDirNeighbours, numValidMergeCand, MergeIndex);
            }
            pCU->setInterDirSubParts(InterDirNeighbours[MergeIndex], SubPartIdx, PartIdx, Depth);

            H265MotionVector TmpMV( 0, 0 );
            for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
            {
                if (pCU->m_SliceHeader->m_numRefIdx[RefListIdx] > 0)
                {
                    pCU->setMVPIdxSubParts(0, EnumRefPicList(RefListIdx), SubPartIdx, PartIdx, Depth);
                    pCU->setMVPNumSubParts(0, EnumRefPicList(RefListIdx), SubPartIdx, PartIdx, Depth);

                    pCU->m_CUMVbuffer[RefListIdx].setAllMVd(TmpMV, PartSize, SubPartIdx, Depth, PartIdx);
                    pCU->m_CUMVbuffer[RefListIdx].setAllMVBuffer(MvBufferNeighbours[2 * MergeIndex + RefListIdx], PartSize, SubPartIdx, Depth, PartIdx);

                    MVBuffer &mvb = MvBufferNeighbours[2 * MergeIndex + RefListIdx];
                    if (mvb.RefIdx >= 0)
                    {
                        RefIdx[RefListIdx] = mvb.RefIdx;
                        MV[RefListIdx] = mvb.MV;
                    }
                }
            }
        }
        else
        {
            DecodeInterDirPUCABAC(pCU, SubPartIdx, Depth, PartIdx);
            for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
            {
                if (pCU->m_SliceHeader->m_numRefIdx[EnumRefPicList(RefListIdx)] > 0)
                {
                    DecodeRefFrmIdxPUCABAC(pCU, SubPartIdx, Depth, PartIdx, EnumRefPicList(RefListIdx));
                    DecodeMVdPUCABAC(pCU, SubPartIdx, Depth, PartIdx, EnumRefPicList(RefListIdx));
                    RefIdx[RefListIdx] = DecodeMVPIdxPUCABAC(pCU, SubPartIdx, Depth, PartIdx, EnumRefPicList(RefListIdx), MV[RefListIdx]);
                }
            }
        }

        if ((pCU->m_InterDir[SubPartIdx] == 3) && pCU->isBipredRestriction(AbsPartIdx, PartIdx))
        {
            RefIdx[REF_PIC_LIST_1] = -1;
            pCU->m_CUMVbuffer[REF_PIC_LIST_1].setAllMV(H265MotionVector(0, 0), PartSize, SubPartIdx, Depth, PartIdx);
            pCU->m_CUMVbuffer[REF_PIC_LIST_1].setAllRefIdx(-1, PartSize, SubPartIdx, Depth, PartIdx);
            pCU->setInterDirSubParts(1, SubPartIdx, PartIdx, Depth);
        }

        UpdatePUInfo(pCU, PartX, PartY, PartWidth, PartHeight, RefIdx, MV);
    }
    return;
}

void H265SegmentDecoder::DecodeMergeFlagCABAC(H265CodingUnit* SubCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u PUIdx)
{
    // at least one merge candidate exists
    //below: parseMergeFlag( pcSubCU, uiAbsPartIdx, uiDepth, uiPUIdx ):
    Ipp32u uVal;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MERGE_FLAG_HEVC]);
    SubCU->setMergeFlagSubParts(uVal ? true : false, AbsPartIdx, PUIdx, Depth);
}

// decode inter direction for a PU block
void H265SegmentDecoder::DecodeInterDirPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u PartIdx)
{
    Ipp32u InterDir;

    if (P_SLICE == pCU->m_SliceHeader->slice_type)
    {
        InterDir = 1;
    }
    else
    {
        ParseInterDirCABAC(pCU, InterDir, AbsPartIdx, Depth);
        //m_pcEntropyDecoderIf->parseInterDir( pcCU, uiInterDir, uiAbsPartIdx, uiDepth );
    }

    pCU->setInterDirSubParts(InterDir, AbsPartIdx, PartIdx, Depth);
}

void H265SegmentDecoder::ParseInterDirCABAC(H265CodingUnit* pCU, Ipp32u &InterDir, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    assert(sizeof(Depth) != 0); //wtf unreferenced parameter
    Ipp32u uVal;
    Ipp32u Ctx = pCU->getCtxInterDir(AbsPartIdx);

    uVal = 0;
    if (pCU->m_PartSizeArray[AbsPartIdx] == SIZE_2Nx2N || pCU->m_HeightArray[AbsPartIdx] != 8)
        uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTER_DIR_HEVC] + Ctx);

    if (uVal)
    {
        uVal = 2;
    }
    else
    {
        uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[INTER_DIR_HEVC] + 4);
        VM_ASSERT(uVal == 0 || uVal == 1);
    }

    uVal++;
    InterDir = uVal;
    return;
}

void H265SegmentDecoder::DecodeRefFrmIdxPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u PartIdx, EnumRefPicList RefList)
{
    Ipp32s RefFrmIdx = 0;
    Ipp32s ParseRefFrmIdx = pCU->m_InterDir[AbsPartIdx] & (1 << RefList);

    if (m_pSliceHeader->m_numRefIdx[RefList] > 1 && ParseRefFrmIdx)
        ParseRefFrmIdxCABAC(pCU, RefFrmIdx, RefList);
    else if (!ParseRefFrmIdx)
        RefFrmIdx = NOT_VALID;
    else
        RefFrmIdx = 0;

    EnumPartSize PartSize = (EnumPartSize) pCU->m_PartSizeArray[AbsPartIdx];
    pCU->m_CUMVbuffer[RefList].setAllRefIdx(RefFrmIdx, PartSize, AbsPartIdx, Depth, PartIdx);
}

void H265SegmentDecoder::ParseRefFrmIdxCABAC(H265CodingUnit* pCU, Ipp32s& RefFrmIdx, EnumRefPicList RefList)
{
    Ipp32u uVal;
    Ipp32u Ctx = 0;

    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[REF_FRAME_IDX_HEVC] + Ctx);
    if (uVal)
    {
        Ipp32u RefNum = pCU->m_SliceHeader->m_numRefIdx[RefList] - 2;
        Ctx++;
        Ipp32u i;
        for(i = 0; i < RefNum; ++i)
        {
            if(i == 0)
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[REF_FRAME_IDX_HEVC] + Ctx);
            else
                uVal = m_pBitStream->DecodeSingleBinEP_CABAC();

            if(uVal == 0)
                break;
        }
        uVal = i + 1;
    }
    RefFrmIdx = uVal;
}

//decode motion vector difference for a PU block
void H265SegmentDecoder::DecodeMVdPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u PartIdx, EnumRefPicList RefList)
{
    if (pCU->m_InterDir[AbsPartIdx] & (1 << RefList))
    {
        Ipp32u uVal;
        Ipp32s HorAbs, VerAbs;
        Ipp32u HorSign = 0, VerSign = 0;

        if (pCU->m_SliceHeader->m_MvdL1Zero && RefList == REF_PIC_LIST_1 && pCU->m_InterDir[AbsPartIdx] == 3)
        {
            HorAbs = 0;
            VerAbs = 0;
        }
        else
        {
            HorAbs = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MVD_HEVC]);
            VerAbs = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MVD_HEVC]);

            const bool HorAbsGr0 = HorAbs != 0;
            const bool VerAbsGr0 = VerAbs != 0;
            Ipp32u Ctx = 1;

            if(HorAbsGr0)
            {
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MVD_HEVC] + Ctx);
                HorAbs += uVal;
            }

            if(VerAbsGr0)
            {
                uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[MVD_HEVC] + Ctx);
                VerAbs += uVal;
            }

            if(HorAbsGr0)
            {
                if(2 == HorAbs)
                {
                    ReadEpExGolombCABAC(uVal, 1);
                    HorAbs += uVal;
                }

                HorSign = m_pBitStream->DecodeSingleBinEP_CABAC();
            }

            if(VerAbsGr0)
            {
                if(2 == VerAbs)
                {
                    ReadEpExGolombCABAC(uVal, 1);
                    VerAbs += uVal;
                }

                VerSign = m_pBitStream->DecodeSingleBinEP_CABAC();
            }
        }

        if (HorSign)
            HorAbs = -HorAbs;
        if (VerSign)
            VerAbs = -VerAbs;
        H265MotionVector MV(HorAbs, VerAbs);
        pCU->m_CUMVbuffer[RefList].setAllMVd(MV, (EnumPartSize)pCU->m_PartSizeArray[AbsPartIdx], AbsPartIdx, Depth, PartIdx);
    }
}

void H265SegmentDecoder::ReadEpExGolombCABAC(Ipp32u& Value, Ipp32u Count)
{
    Ipp32u uVal = 0;
    Ipp32u uBit = 1;

    while (uBit)
    {
        uBit = m_pBitStream->DecodeSingleBinEP_CABAC();
        uVal += uBit << Count++;
    }

    if (--Count)
    {
        Ipp32u bins;
        bins = m_pBitStream->DecodeBypassBins_CABAC(Count);
        uVal += bins;
    }

    Value = uVal;

    return;
}


// decode coefficients
void H265SegmentDecoder::DecodeCoeff(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u Width, Ipp32u Height, bool& CodeDQP)
{
    if (MODE_INTRA == pCU->m_PredModeArray[AbsPartIdx])
    {
    }
    else
    {
        Ipp32u QtRootCbf = 1;
        if (!(pCU->m_PartSizeArray[AbsPartIdx] == SIZE_2Nx2N && pCU->m_MergeFlag[AbsPartIdx]))
        {
            ParseQtRootCbfCABAC(QtRootCbf);
        }
        if (!QtRootCbf)
        {
            pCU->setCbfSubParts( 0, 0, 0, AbsPartIdx, Depth);
            pCU->setTrIdxSubParts( 0 , AbsPartIdx, Depth);
            pCU->setTrStartSubParts(AbsPartIdx, Depth);
            return;
        }
    }

    Ipp32u minCoeffSize = pCU->m_Frame->getMinCUWidth() * pCU->m_Frame->getMinCUHeight();
    Ipp32u lumaOffset = minCoeffSize * AbsPartIdx;
    DecodeTransform(pCU, lumaOffset, AbsPartIdx, Depth, Width, Height, 0, CodeDQP);
}

void H265SegmentDecoder::DecodeTransform(H265CodingUnit* pCU, Ipp32u offsetLuma,
                                         Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u  width, Ipp32u height,
                                         Ipp32u TrIdx, bool& CodeDQP)
{
    Ipp32u offsetChroma = offsetLuma >> 2;
    Ipp32u Subdiv;
    const Ipp32u Log2TrafoSize = g_ConvertToBit[pCU->m_SliceHeader->m_SeqParamSet->MaxCUWidth] + 2 - Depth;

    if (0 == TrIdx)
        m_bakAbsPartIdxCU = AbsPartIdx;

    if (Log2TrafoSize == 2)
    {
        Ipp32u partNum = pCU->m_Frame->getCD()->getNumPartInCU() >> ((Depth - 1) << 1);
        if ((AbsPartIdx % partNum) == 0)
        {
            m_BakAbsPartIdx   = AbsPartIdx;
            m_BakChromaOffset = offsetChroma;
        }
    }

    if (pCU->m_PredModeArray[AbsPartIdx] == MODE_INTRA && pCU->m_PartSizeArray[AbsPartIdx] == SIZE_NxN && Depth == pCU->m_DepthArray[AbsPartIdx])
    {
        Subdiv = 1;
    }
    else if ((pCU->m_SliceHeader->m_SeqParamSet->max_transform_hierarchy_depth_inter == 1) && (pCU->m_PredModeArray[AbsPartIdx] == MODE_INTER) && (pCU->m_PartSizeArray[AbsPartIdx] != SIZE_2Nx2N) && (Depth == pCU->m_DepthArray[AbsPartIdx]))
    {
        Subdiv = (Log2TrafoSize > pCU->getQuadtreeTULog2MinSizeInCU(AbsPartIdx));
    }

    else if (Log2TrafoSize > pCU->m_SliceHeader->m_SeqParamSet->log2_max_transform_block_size)
    {
        Subdiv = 1;
    }
    else if (Log2TrafoSize == pCU->m_SliceHeader->m_SeqParamSet->log2_min_transform_block_size)
    {
        Subdiv = 0;
    }
    else if (Log2TrafoSize == pCU->getQuadtreeTULog2MinSizeInCU(AbsPartIdx))
    {
        Subdiv = 0;
    }
    else
    {
        VM_ASSERT(Log2TrafoSize > pCU->getQuadtreeTULog2MinSizeInCU(AbsPartIdx));
        ParseTransformSubdivFlagCABAC(Subdiv, 5 - Log2TrafoSize);
    }

    const Ipp32u TrDepth = Depth - pCU->m_DepthArray[AbsPartIdx];

    {
        const bool FirstCbfOfCUFlag = TrDepth == 0;
        if (FirstCbfOfCUFlag)
        {
            pCU->setCbfSubParts( 0, TEXT_CHROMA_U, AbsPartIdx, Depth);
            pCU->setCbfSubParts( 0, TEXT_CHROMA_V, AbsPartIdx, Depth);
        }

        if (FirstCbfOfCUFlag || Log2TrafoSize > 2)
        {
            if (FirstCbfOfCUFlag || pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrDepth - 1))
                ParseQtCbfCABAC(pCU, AbsPartIdx, TEXT_CHROMA_U, TrDepth, Depth);
            if (FirstCbfOfCUFlag || pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrDepth - 1))
                ParseQtCbfCABAC(pCU, AbsPartIdx, TEXT_CHROMA_V, TrDepth, Depth);
        }
        else
        {
            pCU->setCbfSubParts(pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrDepth - 1) << TrDepth, TEXT_CHROMA_U, AbsPartIdx, Depth);
            pCU->setCbfSubParts(pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrDepth - 1) << TrDepth, TEXT_CHROMA_V, AbsPartIdx, Depth);
        }
    }

    if (Subdiv)
    {
        Ipp32u size;
        width  >>= 1;
        height >>= 1;
        size = width * height;
        TrIdx++;
        ++Depth;
        const Ipp32u QPartNum = pCU->m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);
        const Ipp32u StartAbsPartIdx = AbsPartIdx;
        Ipp32u YCbf = 0;
        Ipp32u UCbf = 0;
        Ipp32u VCbf = 0;

        for (Ipp32s i = 0; i < 4; i++)
        {
            Ipp32u nsAddr = AbsPartIdx;
            DecodeTransform(pCU, offsetLuma, AbsPartIdx, Depth, width, height, TrIdx, CodeDQP);

            YCbf |= pCU->getCbf(AbsPartIdx, TEXT_LUMA, TrDepth + 1);
            UCbf |= pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrDepth + 1);
            VCbf |= pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrDepth + 1);
            AbsPartIdx += QPartNum;
            offsetLuma += size;
        }

        for (Ipp32u ui = 0; ui < 4 * QPartNum; ++ui)
        {
            pCU->getCbf(TEXT_LUMA)[StartAbsPartIdx + ui] |= YCbf << TrDepth;
            pCU->getCbf(TEXT_CHROMA_U)[StartAbsPartIdx + ui] |= UCbf << TrDepth;
            pCU->getCbf(TEXT_CHROMA_V)[StartAbsPartIdx + ui] |= VCbf << TrDepth;
        }
    }
    else
    {
        assert(Depth >= pCU->m_DepthArray[AbsPartIdx]);
        pCU->setTrIdxSubParts(TrDepth, AbsPartIdx, Depth);
        pCU->setTrStartSubParts(AbsPartIdx, Depth);

        pCU->setCbfSubParts(0, TEXT_LUMA, AbsPartIdx, Depth);
        if (pCU->m_PredModeArray[AbsPartIdx] != MODE_INTRA && Depth == pCU->m_DepthArray[AbsPartIdx] && !pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, 0) && !pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, 0))
        {
            pCU->setCbfSubParts( 1 << TrDepth, TEXT_LUMA, AbsPartIdx, Depth);
        }
        else
        {
            ParseQtCbfCABAC(pCU, AbsPartIdx, TEXT_LUMA, TrDepth, Depth);
        }
        // transform_unit begin
        Ipp32u cbfY = pCU->getCbf(AbsPartIdx, TEXT_LUMA    , TrIdx);
        Ipp32u cbfU = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrIdx);
        Ipp32u cbfV = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrIdx);
        if (Log2TrafoSize == 2)
        {
            Ipp32u partNum = pCU->m_Frame->getCD()->getNumPartInCU() >> ((Depth - 1) << 1);
            if ((AbsPartIdx % partNum) == (partNum - 1))
            {
                cbfU = pCU->getCbf(m_BakAbsPartIdx, TEXT_CHROMA_U, TrIdx);
                cbfV = pCU->getCbf(m_BakAbsPartIdx, TEXT_CHROMA_V, TrIdx);
            }
        }
        if (cbfY || cbfU || cbfV)
        {
            // dQP: only for LCU
            if (pCU->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
            {
                if (CodeDQP)
                {
                    DecodeQP(pCU, m_bakAbsPartIdxCU);
                    CodeDQP = false;
                }
            }
        }
        if (cbfY)
        {
            Ipp32s trWidth = width;
            Ipp32s trHeight = height;
            ParseCoeffNxNCABAC(pCU, (pCU->m_TrCoeffY + offsetLuma), AbsPartIdx, trWidth, trHeight, Depth, TEXT_LUMA);
        }
        if (Log2TrafoSize > 2)
        {
            Ipp32s trWidth = width >> 1;
            Ipp32s trHeight = height >> 1;

            if (cbfU)
            {
                ParseCoeffNxNCABAC(pCU, (pCU->m_TrCoeffCb + offsetChroma), AbsPartIdx, trWidth, trHeight, Depth, TEXT_CHROMA_U);
            }
            if (cbfV)
            {
                ParseCoeffNxNCABAC(pCU, (pCU->m_TrCoeffCr + offsetChroma), AbsPartIdx, trWidth, trHeight, Depth, TEXT_CHROMA_V);
            }
        }
        else
        {
            Ipp32u partNum = pCU->m_Frame->getCD()->getNumPartInCU() >> ((Depth - 1) << 1);
            if ((AbsPartIdx % partNum) == (partNum - 1))
            {
                Ipp32s trWidth = width;
                Ipp32s trHeight = height;

                if (cbfU)
                {
                    ParseCoeffNxNCABAC(pCU, (pCU->m_TrCoeffCb + m_BakChromaOffset), m_BakAbsPartIdx, trWidth, trHeight, Depth, TEXT_CHROMA_U);
                }
                if (cbfV)
                {
                    ParseCoeffNxNCABAC(pCU, (pCU->m_TrCoeffCr + m_BakChromaOffset), m_BakAbsPartIdx, trWidth, trHeight, Depth, TEXT_CHROMA_V);
                }
            }
        }
        // transform_unit end
    }
}

void H265SegmentDecoder::ParseTransformSubdivFlagCABAC(Ipp32u& SubdivFlag, Ipp32u Log2TransformBlockSize)
{
    SubdivFlag = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[TRANS_SUBDIV_FLAG_HEVC] + Log2TransformBlockSize);
}

void H265SegmentDecoder::ParseQtCbfCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, EnumTextType Type, Ipp32u TrDepth, Ipp32u Depth)
{
    Ipp32u uVal;
    const Ipp32u Ctx = pCU->getCtxQtCbf(Type, TrDepth);

    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[QT_CBF_HEVC] + Ctx + NUM_QT_CBF_CTX * (Type ? TEXT_CHROMA : Type));
    //m_pcTDecBinIf->decodeBin( uiSymbol , m_cCUQtCbfSCModel.get( 0, eType ? eType - 1: eType, uiCtx ) );

    pCU->setCbfSubParts(uVal << TrDepth, Type, AbsPartIdx, Depth);
}

void H265SegmentDecoder::ParseQtRootCbfCABAC(Ipp32u& QtRootCbf)
{
    Ipp32u uVal;
    const Ipp32u Ctx = 0;
    uVal = m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffsetHEVC[QT_ROOT_CBF_HEVC] + Ctx);
    QtRootCbf = uVal;
}

void H265SegmentDecoder::DecodeQP(H265CodingUnit* pCU, Ipp32u AbsPartIdx)
{
    if (pCU->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
    {
        ParseDeltaQPCABAC(pCU, AbsPartIdx, pCU->m_DepthArray[AbsPartIdx]);
    }
}

void H265SegmentDecoder::ParseDeltaQPCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32s qp;
    Ipp32u uiDQp;
    Ipp32s iDQp;

    Ipp32u uVal;

    ReadUnaryMaxSymbolCABAC(uiDQp, ctxIdxOffsetHEVC[DQP_HEVC], 1, CU_DQP_TU_CMAX);

    if(uiDQp >= CU_DQP_TU_CMAX)
    {
        ReadEpExGolombCABAC(uVal, CU_DQP_EG_k);
        uiDQp += uVal;
    }

    if (uiDQp > 0)
    {
        Ipp32u Sign;
        Ipp32s QPBdOffsetY = pCU->m_SliceHeader->m_SeqParamSet->getQpBDOffsetY();

        Sign = m_pBitStream->DecodeSingleBinEP_CABAC();
        iDQp = uiDQp;
        if (Sign)
            iDQp = -iDQp;

        qp = (((Ipp32s)pCU->getRefQP(AbsPartIdx) + iDQp + 52 + 2 * QPBdOffsetY) % (52 + QPBdOffsetY)) - QPBdOffsetY;
    }
    else
    {
        iDQp=0;
        qp = pCU->getRefQP(AbsPartIdx);
    }

    pCU->setQPSubParts(qp, AbsPartIdx, Depth);
    pCU->m_CodedQP = qp;
}

void H265SegmentDecoder::ReadUnarySymbolCABAC(Ipp32u& Value, Ipp32s ctxIdx, Ipp32s Offset)
{
    Value = m_pBitStream->DecodeSingleBin_CABAC(ctxIdx);
    //m_pcTDecBinIf->decodeBin( ruiSymbol, pcSCModel[0] );

    if (!Value)
    {
        return;
    }

    Ipp32u uVal = 0;
    Ipp32u Cont;

    do
    {
        Cont = m_pBitStream->DecodeSingleBin_CABAC(ctxIdx + Offset);
        //m_pcTDecBinIf->decodeBin( uiCont, pcSCModel[ iOffset ] );
        uVal++;
    }
    while (Cont);

    Value = uVal;
}

void H265SegmentDecoder::FinishDecodeCU(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast)
{
    if (pCU->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
    {
        pCU->setQPSubParts(m_DecodeDQPFlag ? pCU->getRefQP(AbsPartIdx) : pCU->m_CodedQP, AbsPartIdx, Depth); // set QP to default QP
    }

    IsLast = DecodeSliceEnd(pCU, AbsPartIdx, Depth);
}

/**decode end-of-slice flag
 * \param pcCU
 * \param uiAbsPartIdx
 * \param uiDepth
 * \returns Bool
 */
bool H265SegmentDecoder::DecodeSliceEnd(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u IsLast;
    H265DecoderFrame* pFrame = pCU->m_Frame;
    H265SliceHeader* pSliceHeader = pCU->m_SliceHeader;
    Ipp32u CurNumParts = pFrame->getCD()->getNumPartInCU() >> (Depth << 1);
    Ipp32u Width = pSliceHeader->m_SeqParamSet->pic_width_in_luma_samples;
    Ipp32u Height = pSliceHeader->m_SeqParamSet->pic_height_in_luma_samples;
    Ipp32u GranularityWidth = g_MaxCUWidth;
    Ipp32u PosX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32u PosY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];

    if (((PosX + pCU->m_WidthArray[AbsPartIdx]) % GranularityWidth == 0 || (PosX + pCU->m_WidthArray[AbsPartIdx] == Width))
        && ((PosY + pCU->m_HeightArray[AbsPartIdx]) % GranularityWidth == 0|| (PosY + pCU->m_HeightArray[AbsPartIdx] == Height)))
    {
        IsLast = m_pBitStream->DecodeTerminatingBit_CABAC();
        //m_pcEntropyDecoder->decodeTerminatingBit( uiIsLast );
    }
    else
    {
        IsLast=0;
    }
    if (IsLast)
    {
        if (pSliceHeader->m_DependentSliceSegmentFlag)
        {
            pSliceHeader->m_sliceSegmentCurEndCUAddr = pCU->getSCUAddr() + AbsPartIdx + CurNumParts;
        }
        else
        {
            pSliceHeader->SliceCurEndCUAddr = pCU->getSCUAddr() + AbsPartIdx + CurNumParts;
            pSliceHeader->m_sliceSegmentCurEndCUAddr = pCU->getSCUAddr() + AbsPartIdx + CurNumParts;
        }
    }

    return (IsLast > 0);
}


void H265SegmentDecoder::ParseCoeffNxNCABAC(H265CodingUnit* pCU, H265CoeffsPtrCommon pCoef, Ipp32u AbsPartIdx, Ipp32u Width, Ipp32u Height, Ipp32u Depth, EnumTextType Type)
{
    memset(pCoef, 0, sizeof(H265CoeffsCommon) * Width*Height);

    assert(sizeof(Depth) != 0); //wtf unreferenced parameter
    if (Width > pCU->m_SliceHeader->m_SeqParamSet->m_maxTrSize)
    {
        Width = pCU->m_SliceHeader->m_SeqParamSet->m_maxTrSize;
        Height = pCU->m_SliceHeader->m_SeqParamSet->m_maxTrSize;
    }

    if (pCU->m_SliceHeader->m_PicParamSet->getUseTransformSkip())
        ParseTransformSkipFlags(pCU, AbsPartIdx, Width, Height, Depth, Type);

    Type = Type == TEXT_LUMA ? TEXT_LUMA : (Type == TEXT_NONE ? TEXT_NONE : TEXT_CHROMA);

    //----- parse significance map -----
    const Ipp32u Log2BlockSize = g_ConvertToBit[Width] + 2;
    const Ipp32u MaxNumCoeff  = Width * Height;
    const Ipp32u MaxNumCoeffM1 = MaxNumCoeff - 1;
    Ipp32u ScanIdx = pCU->getCoefScanIdx(AbsPartIdx, Width, Type == TEXT_LUMA, pCU->m_PredModeArray[AbsPartIdx] == MODE_INTRA);

    //===== decode last significant =====
    Ipp32u PosLastX;
    Ipp32u PosLastY;

    ParseLastSignificantXYCABAC(PosLastX, PosLastY, Width, Height, Type, ScanIdx);

    Ipp32u BlkPosLast = PosLastX + (PosLastY << Log2BlockSize);
    pCoef[BlkPosLast] = 1;

    //===== decode significance flags =====
    Ipp32u ScanPosLast = BlkPosLast;
    const Ipp16u* scan = g_SigLastScan[ScanIdx][Log2BlockSize - 2];

#ifdef COEFBLOCK_OPT
   ScanPosLast = g_SigLastScan_inv[ScanIdx][Log2BlockSize - 2][BlkPosLast];
#else
    for (ScanPosLast = 0; ScanPosLast < MaxNumCoeffM1; ScanPosLast++)
    {
        Ipp32u BlkPos = scan[ScanPosLast];
        if (BlkPosLast == BlkPos)
        {
            break;
        }
    }
#endif

    Ipp32u baseCoeffGroupCtxIdx = ctxIdxOffsetHEVC[SIG_COEFF_GROUP_FLAG_HEVC] + NUM_SIG_CG_FLAG_CTX * Type;
    Ipp32u baseCtxIdx = (Type == TEXT_LUMA) ? ctxIdxOffsetHEVC[SIG_FLAG_HEVC] : ctxIdxOffsetHEVC[SIG_FLAG_HEVC] + NUM_SIG_FLAG_CTX_LUMA;

    const Ipp32s LastScanSet = ScanPosLast >> LOG2_SCAN_SET_SIZE;
    Ipp32u c1 = 1;
    Ipp32u GoRiceParam       = 0;

    bool beValid;
    if (pCU->m_CUTransquantBypass[AbsPartIdx])
        beValid = false;
    else
// ML: FIXME: legit src\umc_h265_segment_decoder.cpp(2315): warning C4804: '>' : unsafe use of type 'bool' in operation
//            m_SignHideFlag is already 'bool' really need '> 0'?
        beValid = pCU->m_SliceHeader->m_PicParamSet->sign_data_hiding_flag > 0;
    Ipp32u absSum;

    Ipp32u SigCoeffGroupFlag[MLS_GRP_NUM];
    ::memset(SigCoeffGroupFlag, 0, sizeof(Ipp32u) * MLS_GRP_NUM);
    const Ipp32u NumBlkSide = Width >> (MLS_CG_SIZE >> 1);
    const Ipp16u * scanCG;

    scanCG = g_SigLastScan[ScanIdx][Log2BlockSize > 3 ? Log2BlockSize - 2 - 2 : 0];

    if (Log2BlockSize == 3)
        scanCG = g_sigLastScan8x8[ScanIdx];
    else if (Log2BlockSize == 5)
        scanCG = g_sigLastScanCG32x32;

    Ipp32s ScanPosSig = (Ipp32s) ScanPosLast;
    for (Ipp32s SubSet = LastScanSet; SubSet >= 0; SubSet--)
    {
        Ipp32s SubPos     = SubSet << LOG2_SCAN_SET_SIZE;
        GoRiceParam       = 0;
        Ipp32s numNonZero = 0;
        Ipp32s lastNZPosInCG = -1;
        Ipp32s firstNZPosInCG = SCAN_SET_SIZE;

        Ipp32s pos[SCAN_SET_SIZE];
        if (ScanPosSig == (Ipp32s) ScanPosLast)
        {
            lastNZPosInCG  = ScanPosSig;
            firstNZPosInCG = ScanPosSig;
            ScanPosSig--;
            pos[numNonZero] = BlkPosLast;
            numNonZero = 1;
        }

        // decode significant_coeffgroup_flag
        Ipp32s CGBlkPos = scanCG[SubSet];
        Ipp32s CGPosY   = CGBlkPos / NumBlkSide;
        Ipp32s CGPosX   = CGBlkPos - (CGPosY * NumBlkSide);

        if (SubSet == LastScanSet || SubSet == 0)
        {
            SigCoeffGroupFlag[CGBlkPos] = 1;
        }
        else
        {
            Ipp32u SigCoeffGroup;
            Ipp32u CtxSig = H265TrQuant::getSigCoeffGroupCtxInc(SigCoeffGroupFlag, CGPosX, CGPosY, Width, Height);

            SigCoeffGroup = m_pBitStream->DecodeSingleBin_CABAC(baseCoeffGroupCtxIdx + CtxSig);
            SigCoeffGroupFlag[CGBlkPos] = SigCoeffGroup;
        }

        // decode significant_coeff_flag
        Ipp32s patternSigCtx = H265TrQuant::calcPatternSigCtx(SigCoeffGroupFlag, CGPosX, CGPosY, Width, Height);
        Ipp32u BlkPos, PosY, PosX, Sig, CtxSig;

        for (; ScanPosSig >= SubPos; ScanPosSig--)
        {
            BlkPos  = scan[ScanPosSig];
            PosY    = BlkPos >> Log2BlockSize;
            PosX    = BlkPos - (PosY << Log2BlockSize);
            Sig     = 0;

            if (SigCoeffGroupFlag[CGBlkPos])
            {
                if (ScanPosSig > SubPos || SubSet == 0 || numNonZero)
                {
                    CtxSig = H265TrQuant::getSigCtxInc(patternSigCtx, ScanIdx, PosX, PosY, Log2BlockSize, Type);
                    Sig = m_pBitStream->DecodeSingleBin_CABAC(baseCtxIdx + CtxSig);
                }
                else
                {
                    Sig = 1;
                }
            }

            pCoef[BlkPos] = (Ipp16s) Sig;
            if (Sig)
            {
                pos[numNonZero] = BlkPos;
                numNonZero++;
                if (lastNZPosInCG == -1)
                    lastNZPosInCG = ScanPosSig;
                firstNZPosInCG = ScanPosSig;
            }
        }

        if (numNonZero)
        {
            bool signHidden = (lastNZPosInCG - firstNZPosInCG >= SBH_THRESHOLD);
            absSum = 0;
            Ipp32u CtxSet = (SubSet > 0 && Type == TEXT_LUMA) ? 2 : 0;
            Ipp32u Bin;

            if (c1 == 0)
                CtxSet++;

            c1 = 1;

            Ipp32u baseCtxIdx = (Type == TEXT_LUMA) ? ctxIdxOffsetHEVC[ONE_FLAG_HEVC] + 4 * CtxSet : ctxIdxOffsetHEVC[ONE_FLAG_HEVC] + NUM_ONE_FLAG_CTX_LUMA + 4 * CtxSet;

            Ipp32s absCoeff[SCAN_SET_SIZE];
            for (Ipp32s i = 0; i < numNonZero; i++)
                absCoeff[i] = 1;

            Ipp32s numC1Flag = IPP_MIN(numNonZero, C1FLAG_NUMBER);
            Ipp32s firstC2FlagIdx = -1;

            for (Ipp32s idx = 0; idx < numC1Flag; idx++)
            {
                Bin = m_pBitStream->DecodeSingleBin_CABAC(baseCtxIdx + c1);
                if (Bin == 1)
                {
                    c1 = 0;
                    if (firstC2FlagIdx == -1)
                        firstC2FlagIdx = idx;
                }
                else if ((c1 < 3) && (c1 > 0))
                  c1++;

                absCoeff[idx] = Bin + 1;
            }

            if (c1 == 0)
            {
                baseCtxIdx = (Type == TEXT_LUMA) ? ctxIdxOffsetHEVC[ABS_FLAG_HEVC] + CtxSet : ctxIdxOffsetHEVC[ABS_FLAG_HEVC] + NUM_ABS_FLAG_CTX_LUMA + CtxSet;
                if (firstC2FlagIdx != -1)
                {
                    Bin = m_pBitStream->DecodeSingleBin_CABAC(baseCtxIdx);
                    absCoeff[firstC2FlagIdx] = Bin + 2;
                }
            }

            Ipp32u coeffSigns;
            if (signHidden && beValid)
            {
                coeffSigns = m_pBitStream->DecodeBypassBins_CABAC(numNonZero - 1);
                coeffSigns <<= 32 - (numNonZero-1);
            }
            else
            {
                coeffSigns = m_pBitStream->DecodeBypassBins_CABAC(numNonZero);
                coeffSigns <<= 32 - numNonZero;
            }

            Ipp32s FirstCoeff2 = 1;
            if (c1 == 0 || numNonZero > C1FLAG_NUMBER)
            {
                for (Ipp32s idx = 0; idx < numNonZero; idx++)
                {
                    Ipp32s baseLevel  = (idx < C1FLAG_NUMBER)? (2 + FirstCoeff2) : 1;

                    if (absCoeff[ idx ] == baseLevel)
                    {
                        Ipp32u Level;
                        ReadCoefRemainExGolombCABAC(Level, GoRiceParam);
                        absCoeff[idx] = Level + baseLevel;
                        if (absCoeff[idx] > 3 * (1 << GoRiceParam))
                            GoRiceParam = IPP_MIN(GoRiceParam + 1, 4);
                    }

                    if (absCoeff[idx] >= 2)
                    {
                        FirstCoeff2 = 0;
                    }
                }
            }

            for (Ipp32s idx = 0; idx < numNonZero; idx++)
            {
                Ipp32s blkPos = pos[idx];

                // Signs applied later.
                pCoef[blkPos] = absCoeff[idx];
                absSum += absCoeff[idx];

                if (idx == numNonZero - 1 && signHidden && beValid)
                {
                    // Infer sign of 1st element.
                    if (absSum & 0x1)
                        pCoef[blkPos] = -pCoef[blkPos];
                }
                else
                {
                    Ipp32s sign = static_cast<Ipp32s>(coeffSigns) >> 31;
                    pCoef[blkPos] = (pCoef[blkPos] ^ sign) - sign;
                    coeffSigns <<= 1;
                }
                pCoef[blkPos] = Clip3(H265_COEFF_MIN, H265_COEFF_MAX, pCoef[blkPos]);

            }
        }
    }

    return;
}

void H265SegmentDecoder::ParseTransformSkipFlags(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Width, Ipp32u Height, Ipp32u Depth, EnumTextType Type)
{
    if (pCU->m_CUTransquantBypass[AbsPartIdx])
        return;

    if(Width != 4 || Height != 4)
        return;

    Ipp32u useTransformSkip;
    Ipp32u CtxIdx = ctxIdxOffsetHEVC[TRANSFORM_SKIP_HEVC] + (Type ? TEXT_CHROMA : TEXT_LUMA) * NUM_TRANSFORMSKIP_FLAG_CTX;
    useTransformSkip = m_pBitStream->DecodeSingleBin_CABAC(CtxIdx);

    if (Type != TEXT_LUMA)
    {
        const Ipp32u Log2TrafoSize = g_ConvertToBit[pCU->m_Frame->m_CodingData->m_MaxCUWidth] + 2 - Depth;
        if(Log2TrafoSize == 2)
        {
            Depth --;
        }
    }

    pCU->setTransformSkipSubParts(useTransformSkip, Type, AbsPartIdx, Depth);
}

/** Parse (X,Y) position of the last significant coefficient
 * \param uiPosLastX reference to X component of last coefficient
 * \param uiPosLastY reference to Y component of last coefficient
 * \param uiWidth block width
 * \param eTType plane type / luminance or chrominance
 * \param uiCTXIdx block size context
 * \param uiScanIdx scan type (zig-zag, hor, ver)
 * \returns Void
 * This method decodes the X and Y component within a block of the last significant coefficient.
 */
void H265SegmentDecoder::ParseLastSignificantXYCABAC(Ipp32u& PosLastX, Ipp32u& PosLastY, Ipp32u width, Ipp32u height, EnumTextType Type, Ipp32u ScanIdx)
{
    Ipp32u Last;
    Ipp32u CtxIdxX = ctxIdxOffsetHEVC[LAST_X_HEVC] + Type * NUM_CTX_LAST_FLAG_XY;
    Ipp32u CtxIdxY = ctxIdxOffsetHEVC[LAST_Y_HEVC] + Type * NUM_CTX_LAST_FLAG_XY;

    Ipp32s blkSizeOffsetX, blkSizeOffsetY, shiftX, shiftY;
    blkSizeOffsetX = Type ? 0: (g_ConvertToBit[width] * 3 + ((g_ConvertToBit[width] + 1) >> 2));
    blkSizeOffsetY = Type ? 0: (g_ConvertToBit[height] * 3 + ((g_ConvertToBit[height]+ 1) >> 2));
    shiftX= Type ? g_ConvertToBit[width] : ((g_ConvertToBit[width] + 3) >> 2);
    shiftY= Type ? g_ConvertToBit[height] : ((g_ConvertToBit[height] + 3) >> 2);

    // posX
    for (PosLastX = 0; PosLastX < g_GroupIdx[width - 1]; PosLastX++)
    {
        Last = m_pBitStream->DecodeSingleBin_CABAC(CtxIdxX + blkSizeOffsetX + (PosLastX >> shiftX));
        if (!Last)
            break;
    }

    for (PosLastY = 0; PosLastY < g_GroupIdx[height - 1 ]; PosLastY++)
    {
        Last = m_pBitStream->DecodeSingleBin_CABAC(CtxIdxY + blkSizeOffsetY + (PosLastY >> shiftY));
        if (!Last)
            break;
    }

    if (PosLastX > 3 )
    {
        Ipp32u Temp  = 0;
        Ipp32u Count = (PosLastX - 2) >> 1;
        for (Ipp32s i = Count - 1; i >= 0; i--)
        {
            Last = m_pBitStream->DecodeSingleBinEP_CABAC();
            Temp += Last << i;
        }
        PosLastX = g_MinInGroup[PosLastX] + Temp;
    }

    if (PosLastY > 3)
    {
        Ipp32u Temp  = 0;
        Ipp32s Count = (PosLastY - 2) >> 1;
        for (Ipp32s i = Count - 1; i >= 0; i--)
        {
            Last = m_pBitStream->DecodeSingleBinEP_CABAC();
            Temp += Last << i;
        }
        PosLastY = g_MinInGroup[PosLastY] + Temp;
    }
    if (ScanIdx == SCAN_VER)
    {
        std::swap(PosLastX, PosLastY);
    }
}

/** Parsing of coeff_abs_level_minus3
 * \param ruiSymbol reference to coeff_abs_level_minus3
 * \param ruiGoRiceParam reference to Rice parameter
 * \returns Void
 */
void H265SegmentDecoder::ReadCoefRemainExGolombCABAC(Ipp32u &Symbol, Ipp32u &Param)
{
    Ipp32u prefix = 0;
    Ipp32u CodeWord = 0;

    do
    {
        prefix++;
        CodeWord = m_pBitStream->DecodeSingleBinEP_CABAC();
    }
    while (CodeWord);

    CodeWord = 1 - CodeWord;
    prefix -= CodeWord;
    CodeWord = 0;

    if (prefix < COEF_REMAIN_BIN_REDUCTION)
    {
        CodeWord = m_pBitStream->DecodeBypassBins_CABAC(Param);
        Symbol = (prefix << Param) + CodeWord;
    }
    else
    {
        CodeWord = m_pBitStream->DecodeBypassBins_CABAC(prefix - COEF_REMAIN_BIN_REDUCTION + Param);
        Symbol = (((1 << (prefix - COEF_REMAIN_BIN_REDUCTION)) + COEF_REMAIN_BIN_REDUCTION - 1) << Param) + CodeWord;
    }
}

void H265SegmentDecoder::ReconstructCU(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{

    H265DecoderFrame* frame = pCU->m_Frame;

    bool BoundaryFlag = false;
    Ipp32u LPelX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32u RPelX = LPelX + (g_MaxCUWidth >> Depth) - 1;
    Ipp32u TPelY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];
    Ipp32u BPelY = TPelY + (g_MaxCUHeight >> Depth) - 1;

    Ipp32u CurNumParts = frame->getCD()->getNumPartInCU() >> (Depth << 1);
    H265SliceHeader* pSliceHeader = pCU->m_SliceHeader;
    bool bStartInCU = pCU->getSCUAddr() + AbsPartIdx + CurNumParts > pSliceHeader->m_sliceSegmentCurStartCUAddr && pCU->getSCUAddr() + AbsPartIdx < pSliceHeader->m_sliceSegmentCurStartCUAddr;
    if (bStartInCU || (RPelX >= pSliceHeader->m_SeqParamSet->pic_width_in_luma_samples) || (BPelY >= pSliceHeader->m_SeqParamSet->pic_height_in_luma_samples))
    {
        BoundaryFlag = true;
    }

    if (((Depth < pCU->m_DepthArray[AbsPartIdx]) && (Depth < g_MaxCUDepth - g_AddCUDepth)) || BoundaryFlag)
    {
        Ipp32u NextDepth = Depth + 1;
        Ipp32u QNumParts = pCU->m_NumPartition >> (NextDepth << 1);
        Ipp32u Idx = AbsPartIdx;
        for (Ipp32u PartIdx = 0; PartIdx < 4; PartIdx++, Idx += QNumParts)
        {
            if (BoundaryFlag)
            {
                LPelX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[Idx]];
                TPelY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[Idx]];
                bool binSlice = (pCU->getSCUAddr() + Idx + QNumParts > pSliceHeader->m_sliceSegmentCurStartCUAddr) && (pCU->getSCUAddr() + Idx < pSliceHeader->m_sliceSegmentCurEndCUAddr);
                bool insideFrame = binSlice && (LPelX < pSliceHeader->m_SeqParamSet->pic_width_in_luma_samples) && (TPelY < pSliceHeader->m_SeqParamSet->pic_height_in_luma_samples);
                if (!insideFrame)
                {
                    continue;
                }

            }

            ReconstructCU(pCU, Idx, NextDepth);
        }
        return;
    }

    m_ppcCU[Depth]->copySubCU(pCU, AbsPartIdx, Depth);

    switch (m_ppcCU[Depth]->m_PredModeArray[0])
    {
        case MODE_INTER:
            ReconInter(m_ppcCU[Depth], AbsPartIdx);
            break;
        case MODE_INTRA:
            ReconIntraQT(m_ppcCU[Depth], Depth);
            break;
        default:
            VM_ASSERT(0);
        break;
    }

    if (m_ppcCU[Depth]->isLosslessCoded(0) && !m_ppcCU[Depth]->m_IPCMFlag[0])
    {
        // Saving reconstruct contents in PCM buffer is necessary for later PCM restoration when SAO is enabled
        FillPCMBuffer(m_ppcCU[Depth], Depth);
    }
}

void H265SegmentDecoder::ReconIntraQT(H265CodingUnit* pCU, Ipp32u Depth)
{
    Ipp32u InitTrDepth = (pCU->m_PartSizeArray[0] == SIZE_2Nx2N ? 0 : 1);
    Ipp32u NumPart = pCU->getNumPartInter(0);
    Ipp32u NumQParts = pCU->m_NumPartition >> 2;

    if (pCU->m_IPCMFlag[0])
    {
        ReconPCM(pCU, Depth);
        return;
    }

    for (Ipp32u PU = 0; PU < NumPart; PU++)
    {
        IntraLumaRecQT(pCU, InitTrDepth, PU * NumQParts);
    }

    for (Ipp32u PU = 0; PU < NumPart; PU++)
    {
        IntraChromaRecQT(pCU, InitTrDepth, PU * NumQParts);
    }
}

void H265SegmentDecoder::ReconInter(H265CodingUnit* pCU, Ipp32u AbsPartIdx)
{
    // inter prediction
    m_Prediction->MotionCompensation(pCU, m_ppcYUVReco, AbsPartIdx);

    // clip for only non-zero cbp case
    if ((pCU->getCbf(0, TEXT_LUMA)) || (pCU->getCbf(0, TEXT_CHROMA_U)) || (pCU->getCbf(0, TEXT_CHROMA_V )))
    {
        // inter recon
        DecodeInterTexture(pCU, 0);
    }
}

void H265SegmentDecoder::DecodeInterTexture(H265CodingUnit* pCU, Ipp32u AbsPartIdx)
{
    Ipp32u Width = pCU->m_WidthArray[AbsPartIdx];
    Ipp32u Height = pCU->m_HeightArray[AbsPartIdx];

    m_TrQuant->InvRecurTransformNxN(pCU, 0, Width, Height, 0);
}

void H265SegmentDecoder::ReconPCM(H265CodingUnit* pCU, Ipp32u Depth)
{
    // Luma
    Ipp32u Width  = (g_MaxCUWidth >> Depth);
    Ipp32u Height = (g_MaxCUHeight >> Depth);

    H265PlanePtrYCommon pPcmY = pCU->m_IPCMSampleY;
    H265PlanePtrYCommon pRecoY = m_ppcYUVReco->GetLumaAddr();
    Ipp32u PitchLuma = m_ppcYUVReco->pitch_luma();

    DecodePCMTextureLuma(pCU, 0, pPcmY, pRecoY, PitchLuma, Width, Height);

    // Cb and Cr
    Ipp32u WidthChroma = (Width >> 1);
    Ipp32u HeightChroma = (Height >> 1);

    H265PlanePtrUVCommon pPcmCb = pCU->m_IPCMSampleCb;
    H265PlanePtrUVCommon pPcmCr = pCU->m_IPCMSampleCr;
    H265PlanePtrUVCommon pRecoCbCr = m_ppcYUVReco->GetCbCrAddr();
    Ipp32u PitchChroma = m_ppcYUVReco->pitch_chroma();

    DecodePCMTextureChroma(pCU, 0, pPcmCb, pPcmCr, pRecoCbCr, PitchChroma, WidthChroma, HeightChroma);
}


/** Function for deriving reconstructed luma/chroma samples of a PCM mode CU.
 * \param pcCU pointer to current CU
 * \param uiPartIdx part index
 * \param piPCM pointer to PCM code arrays
 * \param piReco pointer to reconstructed sample arrays
 * \param uiStride stride of reconstructed sample arrays
 * \param uiWidth CU width
 * \param uiHeight CU height
 * \param ttText texture component type
 * \returns Void
 */
void H265SegmentDecoder::DecodePCMTextureLuma(H265CodingUnit* pCU, Ipp16s PartIdx, H265PlanePtrYCommon pPCM, H265PlanePtrYCommon pReco,
                                              Ipp32u Stride, Ipp32u Width, Ipp32u Height)
{
    Ipp32u X, Y;
    H265PlanePtrYCommon pPicReco;
    Ipp32u PicStride;
    Ipp32u PcmLeftShiftBit;

    PicStride = pCU->m_Frame->pitch_luma();
    pPicReco = pCU->m_Frame->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartIdx);
    PcmLeftShiftBit = g_bitDepthY - pCU->m_SliceHeader->m_SeqParamSet->pcm_bit_depth_luma;

    for (Y = 0; Y < Height; Y++)
    {
        for (X = 0; X < Width; X++)
        {
            pReco[X] = pPCM[X] << PcmLeftShiftBit;
            pPicReco[X] = pReco[X];
        }
        pPCM += Width;
        pReco += Stride;
        pPicReco += PicStride;
    }
}

/** Function for deriving reconstructed luma/chroma samples of a PCM mode CU.
 * \param pcCU pointer to current CU
 * \param uiPartIdx part index
 * \param piPCM pointer to PCM code arrays
 * \param piReco pointer to reconstructed sample arrays
 * \param uiStride stride of reconstructed sample arrays
 * \param uiWidth CU width
 * \param uiHeight CU height
 * \param ttText texture component type
 * \returns Void
 */
void H265SegmentDecoder::DecodePCMTextureChroma(H265CodingUnit* pCU, Ipp16s PartIdx, H265PlanePtrUVCommon pPCMCb, H265PlanePtrUVCommon pPCMCr, H265PlanePtrUVCommon pReco, Ipp32u Stride, Ipp32u Width, Ipp32u Height)
{
    Ipp32u X, Y;
    H265PlanePtrUVCommon pPicReco;
    Ipp32u PicStride;
    Ipp32u PcmLeftShiftBit;

    PicStride = pCU->m_Frame->pitch_chroma();
    pPicReco = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + PartIdx);
    PcmLeftShiftBit = g_bitDepthC - pCU->m_SliceHeader->m_SeqParamSet->pcm_bit_depth_chroma;

    for (Y = 0; Y < Height; Y++)
    {
        for (X = 0; X < Width; X++)
        {
            pReco[X * 2] = pPCMCb[X] << PcmLeftShiftBit;
            pPicReco[X * 2] = pReco[X * 2];
            pReco[X * 2 + 1] = pPCMCr[X] << PcmLeftShiftBit;
            pPicReco[X * 2 + 1] = pReco[X * 2 + 1];
        }
        pPCMCb += Width;
        pPCMCr += Width;
        pReco += Stride;
        pPicReco += PicStride;
    }
}

void H265SegmentDecoder::FillPCMBuffer(H265CodingUnit* pCU, Ipp32u Depth)
{
    // Luma
    Ipp32u Width  = (g_MaxCUWidth >> Depth);
    Ipp32u Height = (g_MaxCUHeight >> Depth);

    H265PlanePtrYCommon pPcmY = pCU->m_IPCMSampleY;
    H265PlanePtrYCommon pRecoY = m_ppcYUVReco->GetLumaAddr();
    Ipp32u stride = m_ppcYUVReco->pitch_luma();

    for(Ipp32s y = 0; y < Height; y++)
    {
        for(Ipp32s x = 0; x < Width; x++)
        {
            pPcmY[x] = pRecoY[x];
        }
        pPcmY += Width;
        pRecoY += stride;
    }

    // Cb and Cr
    Width >>= 1;
    Height >>= 1;

    H265PlanePtrUVCommon pPcmCb = pCU->m_IPCMSampleCb;
    H265PlanePtrUVCommon pPcmCr = pCU->m_IPCMSampleCr;
    H265PlanePtrUVCommon pRecoCbCr = m_ppcYUVReco->GetCbCrAddr();

    stride = m_ppcYUVReco->pitch_chroma();

    for(Ipp32s y = 0; y < Height; y++)
    {
        for(Ipp32s x = 0; x < Width; x++)
        {
            pPcmCb[x] = pRecoCbCr[x * 2];
            pPcmCr[x] = pRecoCbCr[x * 2 + 1];
        }
        pPcmCr += Width;
        pPcmCb += Width;
        pRecoCbCr += stride;
    }
}

/** Function for deriving recontructed PU/CU Luma sample with QTree structure
 * \param pcCU pointer of current CU
 * \param uiTrDepth current tranform split depth
 * \param uiAbsPartIdx  part index
 * \param pcRecoYuv pointer to reconstructed sample arrays
 * \param pcPredYuv pointer to prediction sample arrays
 * \param pcResiYuv pointer to residue sample arrays
 *
 \ This function dervies recontructed PU/CU Luma sample with recursive QTree structure
 */
void H265SegmentDecoder::IntraLumaRecQT(H265CodingUnit* pCU,
                     Ipp32u TrDepth,
                     Ipp32u AbsPartIdx)
{
    Ipp32u FullDepth  = pCU->m_DepthArray[0] + TrDepth;
    Ipp32u TrMode = pCU->m_TrIdxArray[AbsPartIdx];
    if (TrMode == TrDepth)
    {
        IntraRecLumaBlk(pCU, TrDepth, AbsPartIdx);
    }
    else
    {
        Ipp32u NumQPart = pCU->m_Frame->getCD()->getNumPartInCU() >> ((FullDepth + 1) << 1);
        for (Ipp32u Part = 0; Part < 4; Part++)
        {
            IntraLumaRecQT(pCU, TrDepth + 1, AbsPartIdx + Part * NumQPart);
        }
    }
}

/** Function for deriving recontructed PU/CU chroma samples with QTree structure
 * \param pcCU pointer of current CU
 * \param uiTrDepth current tranform split depth
 * \param uiAbsPartIdx  part index
 * \param pcRecoYuv pointer to reconstructed sample arrays
 * \param pcPredYuv pointer to prediction sample arrays
 * \param pcResiYuv pointer to residue sample arrays
 *
 \ This function dervies recontructed PU/CU chroma samples with QTree recursive structure
 */
void H265SegmentDecoder::IntraChromaRecQT(H265CodingUnit* pCU,
                     Ipp32u TrDepth,
                     Ipp32u AbsPartIdx)
{
    Ipp32u FullDepth = pCU->m_DepthArray[0] + TrDepth;
    Ipp32u TrMode = pCU->m_TrIdxArray[AbsPartIdx];
    if (TrMode == TrDepth)
    {
        // NV12
        //IntraRecChromaBlkNV12(pCU, TrDepth, AbsPartIdx, pRecoYUV, pPredYUV, pResiYUV);
        IntraRecChromaBlk(pCU, TrDepth, AbsPartIdx);
    }
    else
    {
        Ipp32u NumQPart = pCU->m_Frame->getCD()->getNumPartInCU() >> ((FullDepth + 1) << 1);
        for (Ipp32u Part = 0; Part < 4; Part++)
        {
            IntraChromaRecQT(pCU, TrDepth + 1, AbsPartIdx + Part * NumQPart);
        }
    }
}

void H265SegmentDecoder::IntraRecLumaBlk(H265CodingUnit* pCU,
                         Ipp32u TrDepth,
                         Ipp32u AbsPartIdx)
{
    Ipp32u Width = pCU->m_WidthArray[0] >> TrDepth;
    Ipp32u Height = pCU->m_HeightArray[0] >> TrDepth;

    //===== init availability pattern =====
    pCU->m_Pattern->InitPattern(pCU, TrDepth, AbsPartIdx);
    pCU->m_Pattern->InitAdiPatternLuma(pCU, AbsPartIdx, TrDepth,
                                       m_Prediction->GetPredicBuf(),
                                       m_Prediction->GetPredicBufWidth(),
                                       m_Prediction->GetPredicBufHeight());

    //===== get prediction signal =====
    Ipp32u LumaPredMode = pCU->m_LumaIntraDir[AbsPartIdx];
    Ipp32u ZOrder = pCU->m_AbsIdxInLCU + AbsPartIdx;
    H265PlanePtrYCommon pRecIPred = pCU->m_Frame->GetLumaAddr(pCU->CUAddr, ZOrder);
    Ipp32u RecIPredStride = pCU->m_Frame->pitch_luma();
    m_Prediction->PredIntraLumaAng(pCU->m_Pattern, LumaPredMode, pRecIPred, RecIPredStride, Width, Height);

    //===== inverse transform =====
    if (!pCU->getCbf(AbsPartIdx, TEXT_LUMA, TrDepth))
        return;

    m_TrQuant->SetQPforQuant(pCU->m_QPArray[0], TEXT_LUMA, pCU->m_SliceHeader->m_SeqParamSet->m_QPBDOffsetY, 0);

    Ipp32s scalingListType = (pCU->m_PredModeArray[AbsPartIdx] ? 0 : 3) + g_Table[(Ipp32s)TEXT_LUMA];
    Ipp32u NumCoeffInc = (pCU->m_SliceHeader->m_SeqParamSet->MaxCUWidth * pCU->m_SliceHeader->m_SeqParamSet->MaxCUHeight) >> (pCU->m_SliceHeader->m_SeqParamSet->MaxCUDepth << 1);
    H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffY + (NumCoeffInc * AbsPartIdx);
    bool useTransformSkip = pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_LUMA]][AbsPartIdx] != 0;

    m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_LUMA, pCU->m_LumaIntraDir[AbsPartIdx],
        pRecIPred, RecIPredStride, pCoeff, Width, Height, scalingListType, useTransformSkip);
}

void H265SegmentDecoder::IntraRecChromaBlk(H265CodingUnit* pCU,
                         Ipp32u TrDepth,
                         Ipp32u AbsPartIdx)
{
    Ipp32u FullDepth  = pCU->m_DepthArray[0] + TrDepth;
    Ipp32u Log2TrSize = g_ConvertToBit[pCU->m_SliceHeader->m_SeqParamSet->MaxCUWidth >> FullDepth] + 2;
    if (Log2TrSize == 2)
    {
        assert(TrDepth > 0);
        TrDepth--;
        Ipp32u QPDiv = pCU->m_Frame->getCD()->getNumPartInCU() >> ((pCU->m_DepthArray[0] + TrDepth) << 1);
        bool FirstQFlag = ((AbsPartIdx % QPDiv) == 0);
        if (!FirstQFlag)
        {
            return;
        }
    }

    Ipp32u Width = pCU->m_WidthArray[0] >> (TrDepth + 1);
    Ipp32u Height = pCU->m_HeightArray[0] >> (TrDepth + 1);

    //===== init availability pattern =====
    pCU->m_Pattern->InitPattern(pCU, TrDepth, AbsPartIdx);

    pCU->m_Pattern->InitAdiPatternChroma(pCU, AbsPartIdx, TrDepth,
                                         m_Prediction->GetPredicBuf(),
                                         m_Prediction->GetPredicBufWidth(),
                                         m_Prediction->GetPredicBufHeight());

    //===== get prediction signal =====
    H265PlanePtrUVCommon pPatChroma = m_Prediction->GetPredicBuf();
    Ipp32u ChromaPredMode = pCU->m_ChromaIntraDir[0];
    if (ChromaPredMode == DM_CHROMA_IDX)
    {
        ChromaPredMode = pCU->m_LumaIntraDir[0];
    }

    Ipp32u ZOrder = pCU->m_AbsIdxInLCU + AbsPartIdx;
    H265PlanePtrUVCommon pRecIPred = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, ZOrder);
    Ipp32u RecIPredStride = pCU->m_Frame->pitch_chroma();

    m_Prediction->PredIntraChromaAng(pPatChroma, ChromaPredMode, pRecIPred, RecIPredStride, Width, Height);

    bool chromaUPresent = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_U, TrDepth) != 0;
    bool chromaVPresent = pCU->getCbf(AbsPartIdx, TEXT_CHROMA_V, TrDepth) != 0;

    if (!chromaUPresent && !chromaVPresent)
        return;

    //===== inverse transform =====
    Ipp32u NumCoeffInc = ((pCU->m_SliceHeader->m_SeqParamSet->MaxCUWidth * pCU->m_SliceHeader->m_SeqParamSet->MaxCUHeight) >> (pCU->m_SliceHeader->m_SeqParamSet->MaxCUDepth << 1)) >> 2;
    Ipp32u residualPitch = m_ppcYUVResi->pitch_chroma() >> 1;

    // Cb
    if (chromaUPresent)
    {
        Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->getChromaCbQpOffset() + pCU->m_SliceHeader->m_slice_qp_delta_cb;
        m_TrQuant->SetQPforQuant(pCU->m_QPArray[0], TEXT_CHROMA_U, pCU->m_SliceHeader->m_SeqParamSet->m_QPBDOffsetC, curChromaQpOffset);
        Ipp32s scalingListType = (pCU->m_PredModeArray[AbsPartIdx] == MODE_INTRA ? 0 : 3) + g_Table[TEXT_CHROMA_U];
        H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffCb + (NumCoeffInc * AbsPartIdx);
        H265CoeffsPtrCommon pResi = (H265CoeffsPtrCommon)m_ppcYUVResi->GetCbAddr();
        bool useTransformSkip = pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_CHROMA_U]][AbsPartIdx] != 0;
        m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_U, REG_DCT,
            pResi, residualPitch, pCoeff, Width, Height, scalingListType, useTransformSkip);
    }

    // Cr
    if (chromaVPresent)
    {
        Ipp32s curChromaQpOffset = pCU->m_SliceHeader->m_PicParamSet->getChromaCrQpOffset() + pCU->m_SliceHeader->m_slice_qp_delta_cr;
        m_TrQuant->SetQPforQuant(pCU->m_QPArray[0], TEXT_CHROMA_V, pCU->m_SliceHeader->m_SeqParamSet->m_QPBDOffsetC, curChromaQpOffset);
        Ipp32s scalingListType = (pCU->m_PredModeArray[AbsPartIdx] == MODE_INTRA ? 0 : 3) + g_Table[TEXT_CHROMA_V];
        H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffCr + (NumCoeffInc * AbsPartIdx);
        H265CoeffsPtrCommon pResi = (H265CoeffsPtrCommon)m_ppcYUVResi->GetCrAddr();
        bool useTransformSkip = pCU->m_TransformSkip[g_ConvertTxtTypeToIdx[TEXT_CHROMA_V]][AbsPartIdx] != 0;
        m_TrQuant->InvTransformNxN(pCU->m_CUTransquantBypass[AbsPartIdx], TEXT_CHROMA_V, REG_DCT,
            pResi, residualPitch, pCoeff, Width, Height, scalingListType, useTransformSkip);
    }

    //===== reconstruction =====
    {
        H265CoeffsPtrCommon p_ResiU = (H265CoeffsPtrCommon)m_ppcYUVResi->GetCbAddr();
        H265CoeffsPtrCommon p_ResiV = (H265CoeffsPtrCommon)m_ppcYUVResi->GetCrAddr();
        for (Ipp32u y = 0; y < Height; y++)
        {
            for (Ipp32u x = 0; x < Width; x++)
            {
                if (chromaUPresent)
                    pRecIPred[2*x] = (H265PlaneUVCommon)ClipC(pRecIPred[2*x] + p_ResiU[x]);
                if (chromaVPresent)
                    pRecIPred[2*x+1] = (H265PlaneUVCommon)ClipC(pRecIPred[2*x + 1] + p_ResiV[x]);

            }
            p_ResiU     += residualPitch;
            p_ResiV     += residualPitch;
            pRecIPred += RecIPredStride;
        }
    }
}

void H265SegmentDecoder::IntraRecQT(H265CodingUnit* pCU,
                                    Ipp32u TrDepth,
                                    Ipp32u AbsPartIdx)
{
    Ipp32u FullDepth = pCU->m_DepthArray[0] + TrDepth;
    Ipp32u TrMode = pCU->m_TrIdxArray[AbsPartIdx];
    if (TrMode == TrDepth)
    {
        IntraRecLumaBlk(pCU, TrDepth, AbsPartIdx);
        IntraRecChromaBlk(pCU, TrDepth, AbsPartIdx);
    }
    else
    {
        Ipp32u NumQPart = pCU->m_Frame->getCD()->getNumPartInCU() >> ((FullDepth + 1) << 1);
        for (Ipp32u Part = 0; Part < 4; Part++)
        {
            IntraRecQT(pCU, TrDepth + 1, AbsPartIdx + Part * NumQPart);
        }
    }
}

void H265SegmentDecoder::UpdateCurrCUContext(Ipp32u lastCUAddr, Ipp32u newCUAddr)
{
    // Set local pointers to real array start positions
    H265FrameHLDNeighborsInfo *CurrCTBFlags = &m_CurrCTBFlagsHolder[0];
    H265FrameHLDNeighborsInfo *TopNgbrs = &m_TopNgbrsHolder[0];
    H265TUData *CurrCTB = &m_CurrCTBHolder[0];
    H265TUData *TopMVInfo = &m_TopMVInfoHolder[0];

    VM_ASSERT(CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 1)].data == 0);

    Ipp32u lastCUX = (lastCUAddr % m_pSeqParamSet->WidthInCU) * m_pSeqParamSet->NumPartitionsInCUSize;
    Ipp32u lastCUY = (lastCUAddr / m_pSeqParamSet->WidthInCU) * m_pSeqParamSet->NumPartitionsInCUSize;
    Ipp32u newCUX = (newCUAddr % m_pSeqParamSet->WidthInCU) * m_pSeqParamSet->NumPartitionsInCUSize;
    Ipp32u newCUY = (newCUAddr / m_pSeqParamSet->WidthInCU) * m_pSeqParamSet->NumPartitionsInCUSize;

    if (newCUY > 0)
    {
        if (newCUX > lastCUX)
        {
            // Init top margin from previous row
            for (Ipp32s i = 0; i < m_CurrCTBStride; i++)
            {
                CurrCTBFlags[i].data = TopNgbrs[newCUX + i].data;
                CurrCTB[i].mvinfo[0] = TopMVInfo[newCUX + i].mvinfo[0];
                CurrCTB[i].mvinfo[1] = TopMVInfo[newCUX + i].mvinfo[1];
            }
            // Store bottom margin for next row if next CTB is to the right. This is necessary for left-top diagonal
            for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
            {
                TopNgbrs[lastCUX + i].data = CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].data;
                TopMVInfo[lastCUX + i].mvinfo[0] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[0];
                TopMVInfo[lastCUX + i].mvinfo[1] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[1];
            }
        }
        else if (newCUX < lastCUX)
        {
            // Store bottom margin for next row if next CTB is to the left-below. This is necessary for right-top diagonal
            for (Ipp32s i = 1; i < m_CurrCTBStride - 1; i++)
            {
                TopNgbrs[lastCUX + i].data = CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].data;
                TopMVInfo[lastCUX + i].mvinfo[0] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[0];
                TopMVInfo[lastCUX + i].mvinfo[1] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[1];
            }
            // Init top margin from previous row
            for (Ipp32u i = 0; i < m_CurrCTBStride; i++)
            {
                CurrCTBFlags[i].data = TopNgbrs[newCUX + i].data;
                CurrCTB[i].mvinfo[0] = TopMVInfo[newCUX + i].mvinfo[0];
                CurrCTB[i].mvinfo[1] = TopMVInfo[newCUX + i].mvinfo[1];
            }
        }
        else // New CTB right under previous CTB
        {
            // Copy data from bottom row
            for (Ipp32u i = 0; i < m_CurrCTBStride; i++)
            {
                CurrCTBFlags[i].data = CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].data;
                CurrCTB[i].mvinfo[0] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[0];
                CurrCTB[i].mvinfo[1] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[1];
            }
        }
    }
    else
    {
        // Should be reset from the beginning and not filled up until 2nd row
        for (Ipp32u i = 0; i < m_CurrCTBStride; i++)
            VM_ASSERT(CurrCTBFlags[i].data == 0);
    }

    if (newCUX != lastCUX)
    {
        // Store bottom margin for next row if next CTB is not underneath. This is necessary for left-top diagonal
        for (Ipp32u i = 1; i < m_CurrCTBStride - 1; i++)
        {
            TopNgbrs[lastCUX + i].data = CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].data;
            TopMVInfo[lastCUX + i].mvinfo[0] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[0];
            TopMVInfo[lastCUX + i].mvinfo[1] = CurrCTB[m_CurrCTBStride * (m_CurrCTBStride - 2) + i].mvinfo[1];
        }
    }

    if (lastCUY == newCUY)
    {
        // Copy left margin from right
        for (Ipp32u i = 1; i < m_CurrCTBStride - 1; i++)
            CurrCTBFlags[i * m_CurrCTBStride].data = CurrCTBFlags[i * m_CurrCTBStride + m_CurrCTBStride - 2].data;

        for (Ipp32u i = 1; i < m_CurrCTBStride - 1; i++)
        {
            CurrCTB[i * m_CurrCTBStride].mvinfo[0] = CurrCTB[i * m_CurrCTBStride + m_CurrCTBStride - 2].mvinfo[0];
            CurrCTB[i * m_CurrCTBStride].mvinfo[1] = CurrCTB[i * m_CurrCTBStride + m_CurrCTBStride - 2].mvinfo[1];
        }
    }
    else
    {
        for (Ipp32u i = 1; i < m_CurrCTBStride; i++)
            CurrCTBFlags[i * m_CurrCTBStride].data = 0;
    }

    VM_ASSERT(CurrCTBFlags[m_CurrCTBStride * (m_CurrCTBStride - 1)].data == 0);

    // Clean inside of CU context
    for (Ipp32u i = 1; i < m_CurrCTBStride; i++)
        for (Ipp32u j = 1; j < m_CurrCTBStride; j++)
            CurrCTBFlags[i * m_CurrCTBStride + j].data = 0;
}

void H265SegmentDecoder::ResetRowBuffer(void)
{
    H265FrameHLDNeighborsInfo *CurrCTBFlags = &m_CurrCTBFlagsHolder[0];
    H265FrameHLDNeighborsInfo *TopNgbrs = &m_TopNgbrsHolder[0];

    for (Ipp32u i = 0; i < m_pSeqParamSet->NumPartitionsInFrameWidth + 2; i++)
        TopNgbrs[i].data = 0;

    for (Ipp32u i = 0; i < m_CurrCTBStride * m_CurrCTBStride; i++)
        CurrCTBFlags[i].data = 0;
}

void H265SegmentDecoder::UpdatePUInfo(H265CodingUnit *pCU, Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight, RefIndexType *RefIdx, H265MotionVector *MV)
{
    VM_ASSERT(RefIdx[REF_PIC_LIST_0] >= 0 || RefIdx[REF_PIC_LIST_1] >= 0);

    for (Ipp32u RefListIdx = 0; RefListIdx < 2; RefListIdx++)
    {
        if (m_pSliceHeader->m_numRefIdx[RefListIdx] > 0)
        {
            if (RefIdx[RefListIdx] >= 0)
            {
                pCU->setAllColMV(MV[RefListIdx], RefListIdx, PartX, PartY, PartWidth, PartHeight);

                if (m_pRefPicList[RefListIdx][RefIdx[RefListIdx]].isLongReference)
                    pCU->setAllColFlags(COL_TU_LT_INTER, RefListIdx, PartX, PartY, PartWidth, PartHeight);
                else
                {
                    Ipp32s POCDelta = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[RefListIdx][RefIdx[RefListIdx]].refFrame->m_PicOrderCnt;

                    pCU->setAllColPOCDelta(POCDelta, RefListIdx, PartX, PartY, PartWidth, PartHeight);
                    pCU->setAllColFlags(COL_TU_ST_INTER, RefListIdx, PartX, PartY, PartWidth, PartHeight);
                }
            }
            else
            {
                pCU->setAllColFlags(COL_TU_INVALID_INTER, RefListIdx, PartX, PartY, PartWidth, PartHeight);
            }
        }
        else
        {
            pCU->setAllColFlags(COL_TU_INVALID_INTER, RefListIdx, PartX, PartY, PartWidth, PartHeight);
        }
    }

    Ipp32u mask = (1 << m_pSeqParamSet->MaxCUDepth) - 1;
    PartX &= mask;
    PartY &= mask;

    H265FrameHLDNeighborsInfo info;
    info.data = 0;
    info.members.IsAvailable = 1;

    // Bottom row
    for (Ipp32s i = 0; i < PartWidth; i++)
    {
        m_CurrCTBFlags[(PartY + PartHeight - 1) * m_CurrCTBStride + PartX + i].data = info.data;
        m_CurrCTB[(PartY + PartHeight - 1) * m_CurrCTBStride + PartX + i].mvinfo[REF_PIC_LIST_0].setMVBuffer(MV[REF_PIC_LIST_0], RefIdx[REF_PIC_LIST_0]);
        m_CurrCTB[(PartY + PartHeight - 1) * m_CurrCTBStride + PartX + i].mvinfo[REF_PIC_LIST_1].setMVBuffer(MV[REF_PIC_LIST_1], RefIdx[REF_PIC_LIST_1]);
    }

    // Right column
    for (Ipp32s i = 0; i < PartHeight; i++)
    {
        m_CurrCTBFlags[(PartY + i) * m_CurrCTBStride + PartX + PartWidth - 1].data = info.data;
        m_CurrCTB[(PartY + i) * m_CurrCTBStride + PartX + PartWidth - 1].mvinfo[REF_PIC_LIST_0].setMVBuffer(MV[REF_PIC_LIST_0], RefIdx[REF_PIC_LIST_0]);
        m_CurrCTB[(PartY + i) * m_CurrCTBStride + PartX + PartWidth - 1].mvinfo[REF_PIC_LIST_1].setMVBuffer(MV[REF_PIC_LIST_1], RefIdx[REF_PIC_LIST_1]);
    }
}

void H265SegmentDecoder::UpdateNeighborBuffers(H265CodingUnit* pCU, Ipp32u AbsPartIdx)
{
    Ipp32s i;

    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s PartSize = pCU->m_WidthArray[AbsPartIdx] >> 2;

    H265FrameHLDNeighborsInfo info;
    info.members.IsAvailable = 1;
    info.members.IsIntra = pCU->m_PredModeArray[AbsPartIdx];
    info.members.Depth = pCU->m_DepthArray[AbsPartIdx];
    info.members.SkipFlag = pCU->m_skipFlag[AbsPartIdx];
    info.members.IntraDir = pCU->m_LumaIntraDir[AbsPartIdx];
    Ipp16u data = info.data;

    if (SIZE_2Nx2N == pCU->m_PartSizeArray[AbsPartIdx])
    {
        for (i = 0; i < PartSize; i++)
        {
            // Bottom row
            m_CurrCTBFlags[m_CurrCTBStride * (YInc + PartSize - 1) + (XInc + i)].data = data;
            // Right column
            m_CurrCTBFlags[m_CurrCTBStride * (YInc + i) + (XInc + PartSize - 1)].data = data;
        }
    }
    else
    {
        H265FrameHLDNeighborsInfo info1, info2, info3;
        Ipp16u data1, data2, data3;
        Ipp32u PartOffset = (pCU->m_Frame->m_CodingData->m_NumPartitions >> (pCU->m_DepthArray[AbsPartIdx] << 1)) >> 2;

        info1.data = data;
        info2.data = data;
        info3.data = data;
        info1.members.IntraDir = pCU->m_LumaIntraDir[AbsPartIdx + PartOffset];
        info2.members.IntraDir = pCU->m_LumaIntraDir[AbsPartIdx + PartOffset * 2];
        info3.members.IntraDir = pCU->m_LumaIntraDir[AbsPartIdx + PartOffset * 3];
        data1 = info1.data;
        data2 = info2.data;
        data3 = info3.data;

        VM_ASSERT(PartSize >> 1);

        for (i = 0; i < PartSize >> 1; i++)
        {
            // Bottom row
            m_CurrCTBFlags[m_CurrCTBStride * (YInc + PartSize - 1) + (XInc + i)].data = data2;
            // Right column
            m_CurrCTBFlags[m_CurrCTBStride * (YInc + i) + (XInc + PartSize - 1)].data = data1;
        }

        for (i = PartSize >> 1; i < PartSize; i++)
        {
            // Bottom row
            m_CurrCTBFlags[m_CurrCTBStride * (YInc + PartSize - 1) + (XInc + i)].data = data3;
            // Right column
            m_CurrCTBFlags[m_CurrCTBStride * (YInc + i) + (XInc + PartSize - 1)].data = data3;
        }
    }
}

Ipp32u H265SegmentDecoder::getCtxSplitFlag(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uiCtx;

    uiCtx = 0;

    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->getQuadtreeTULog2MinSize();

    if ((m_CurrCTBFlags[YInc * m_CurrCTBStride + XInc - 1].members.Depth > Depth))
    {
        uiCtx = 1;
    }

    if ((m_CurrCTBFlags[(YInc - 1) * m_CurrCTBStride + XInc].members.Depth > Depth))
    {
        uiCtx += 1;
    }

    return uiCtx;
}

Ipp32u H265SegmentDecoder::getCtxSkipFlag(H265CodingUnit *pCU, Ipp32u AbsPartIdx)
{
    Ipp32u uiCtx = 0;

    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->getQuadtreeTULog2MinSize();

    if (m_CurrCTBFlags[YInc * m_CurrCTBStride + XInc - 1].members.SkipFlag)
    {
        uiCtx = 1;
    }

    if (m_CurrCTBFlags[(YInc - 1) * m_CurrCTBStride + XInc].members.SkipFlag)
    {
        uiCtx += 1;
    }

    return uiCtx;
}

void H265SegmentDecoder::getIntraDirLumaPredictor(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32s IntraDirPred[])
{
    Ipp32s LeftIntraDir = DC_IDX;
    Ipp32s AboveIntraDir = DC_IDX;
    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]] >> m_pSeqParamSet->getQuadtreeTULog2MinSize();

    Ipp32s PartOffsetY = (m_pSeqParamSet->NumPartitionsInCU >> (pCU->m_DepthArray[AbsPartIdx] << 1)) >> 1;
    Ipp32s PartOffsetX = PartOffsetY >> 1;

    // Get intra direction of left PU
    if ((AbsPartIdx & PartOffsetX) == 0)
    {
        LeftIntraDir = m_CurrCTBFlags[YInc * m_CurrCTBStride + XInc - 1].members.IsIntra ? m_CurrCTBFlags[YInc * m_CurrCTBStride + XInc - 1].members.IntraDir : DC_IDX;
    }
    else
    {
        LeftIntraDir = pCU->m_LumaIntraDir[AbsPartIdx - PartOffsetX];
    }

    // Get intra direction of above PU
    if ((AbsPartIdx & PartOffsetY) == 0)
    {
        if (YInc > 0)
            AboveIntraDir = m_CurrCTBFlags[(YInc - 1) * m_CurrCTBStride + XInc].members.IsIntra ? m_CurrCTBFlags[(YInc - 1) * m_CurrCTBStride + XInc].members.IntraDir : DC_IDX;
    }
    else
    {
        AboveIntraDir = pCU->m_LumaIntraDir[AbsPartIdx - PartOffsetY];
    }

    if (LeftIntraDir == AboveIntraDir)
    {
        if (LeftIntraDir > 1) // angular modes
        {
            IntraDirPred[0] = LeftIntraDir;
            IntraDirPred[1] = ((LeftIntraDir + 29) % 32) + 2;
            IntraDirPred[2] = ((LeftIntraDir - 1 ) % 32) + 2;
        }
        else //non-angular
        {
            IntraDirPred[0] = PLANAR_IDX;
            IntraDirPred[1] = DC_IDX;
            IntraDirPred[2] = VER_IDX;
        }
    }
    else
    {
        IntraDirPred[0] = LeftIntraDir;
        IntraDirPred[1] = AboveIntraDir;

        if (LeftIntraDir && AboveIntraDir) //both modes are non-planar
        {
            IntraDirPred[2] = PLANAR_IDX;
        }
        else
        {
            IntraDirPred[2] = (LeftIntraDir + AboveIntraDir) < 2 ? VER_IDX : DC_IDX;
        }
    }
}

static bool inline isDiffMER(Ipp32u plevel, Ipp32s xN, Ipp32s yN, Ipp32s xP, Ipp32s yP)
{
    if ((xN >> plevel) != (xP >> plevel))
    {
        return true;
    }
    if ((yN >> plevel) != (yP >> plevel))
    {
        return true;
    }
    return false;
}

bool H265SegmentDecoder::hasEqualMotion(Ipp32s dir1, Ipp32s dir2)
{
    if (m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_0].RefIdx != m_CurrCTB[dir2].mvinfo[REF_PIC_LIST_0].RefIdx ||
        m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_1].RefIdx != m_CurrCTB[dir2].mvinfo[REF_PIC_LIST_1].RefIdx)
        return false;

    if (m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_0].RefIdx >= 0 && m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_0].MV != m_CurrCTB[dir2].mvinfo[REF_PIC_LIST_0].MV)
        return false;

    if (m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_1].RefIdx >= 0 && m_CurrCTB[dir1].mvinfo[REF_PIC_LIST_1].MV != m_CurrCTB[dir2].mvinfo[REF_PIC_LIST_1].MV)
        return false;

    return true;
}

#define UPDATE_MV_INFO(dir)                                                                         \
    CandIsInter[Count] = true;                                                                      \
    if (m_CurrCTB[dir].mvinfo[REF_PIC_LIST_0].RefIdx >= 0)                                          \
    {                                                                                               \
        InterDirNeighbours[Count] = 1;                                                              \
        MVBufferNeighbours[Count << 1] = m_CurrCTB[dir].mvinfo[REF_PIC_LIST_0];                     \
    }                                                                                               \
    else                                                                                            \
        InterDirNeighbours[Count] = 0;                                                              \
                                                                                                    \
    if (B_SLICE == m_pSliceHeader->slice_type && m_CurrCTB[dir].mvinfo[REF_PIC_LIST_1].RefIdx >= 0) \
    {                                                                                               \
        InterDirNeighbours[Count] += 2;                                                             \
        MVBufferNeighbours[(Count << 1) + 1] = m_CurrCTB[dir].mvinfo[REF_PIC_LIST_1];               \
    }                                                                                               \
                                                                                                    \
    if (mrgCandIdx == Count)                                                                        \
    {                                                                                               \
        return;                                                                                     \
    }                                                                                               \
    Count++;


static Ipp32u PriorityList0[12] = {0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
static Ipp32u PriorityList1[12] = {1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};

void H265SegmentDecoder::getInterMergeCandidates(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32u PartIdx, MVBuffer *MVBufferNeighbours,
                                                 Ipp8u *InterDirNeighbours, Ipp32s &numValidMergeCand, Ipp32s mrgCandIdx)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

    numValidMergeCand = m_pSliceHeader->m_MaxNumMergeCand;
    bool CandIsInter[MRG_MAX_NUM_CANDS];
    for (Ipp32u ind = 0; ind < numValidMergeCand; ++ind)
    {
        CandIsInter[ind] = false;
        MVBufferNeighbours[ind << 1].RefIdx = NOT_VALID;
        MVBufferNeighbours[(ind << 1) + 1].RefIdx = NOT_VALID;
    }

    // compute the location of the current PU
    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s LPelX = pCU->m_CUPelX + XInc;
    Ipp32s TPelY = pCU->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->getQuadtreeTULog2MinSize();
    YInc >>= m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s PartX = LPelX >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s PartY = TPelY >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s TUPartNumberInCTB = m_CurrCTBStride * YInc + XInc;

    Ipp32s nPSW, nPSH;
    pCU->getPartSize(AbsPartIdx, PartIdx, nPSW, nPSH);
    Ipp32s PartWidth = nPSW >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s PartHeight = nPSH >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    EnumPartSize CurPS = (EnumPartSize)pCU->m_PartSizeArray[AbsPartIdx];

    Ipp32s Count = 0;

    // left
    Ipp32s leftAddr = TUPartNumberInCTB + m_CurrCTBStride * (PartHeight - 1) - 1;
    bool isAvailableA1 = m_CurrCTBFlags[leftAddr].members.IsAvailable && !m_CurrCTBFlags[leftAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX - 1, TPelY + nPSH - 1, LPelX, TPelY) &&
        !(PartIdx == 1 && (CurPS == SIZE_Nx2N || CurPS == SIZE_nLx2N || CurPS == SIZE_nRx2N));

    if (isAvailableA1)
    {
        UPDATE_MV_INFO(leftAddr);
    }

    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    // above
    Ipp32s aboveAddr = TUPartNumberInCTB - m_CurrCTBStride + (PartWidth - 1);
    bool isAvailableB1 = m_CurrCTBFlags[aboveAddr].members.IsAvailable && !m_CurrCTBFlags[aboveAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX + nPSW - 1, TPelY - 1, LPelX, TPelY) &&
        !(PartIdx == 1 && (CurPS == SIZE_2NxN || CurPS == SIZE_2NxnU || CurPS == SIZE_2NxnD));

    if (isAvailableB1 && (!isAvailableA1 || !hasEqualMotion(leftAddr, aboveAddr)))
    {
        UPDATE_MV_INFO(aboveAddr);
    }

    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    // above right
    Ipp32s aboveRightAddr = TUPartNumberInCTB - m_CurrCTBStride + PartWidth;
    bool isAvailableB0 = m_CurrCTBFlags[aboveRightAddr].members.IsAvailable && !m_CurrCTBFlags[aboveRightAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX + nPSW, TPelY - 1, LPelX, TPelY);

    if (isAvailableB0 && (!isAvailableB1 || !hasEqualMotion(aboveAddr, aboveRightAddr)))
    {
        UPDATE_MV_INFO(aboveRightAddr);
    }

    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    // left bottom
    Ipp32s leftBottomAddr = TUPartNumberInCTB - 1 + m_CurrCTBStride * PartHeight;
    bool isAvailableA0 = m_CurrCTBFlags[leftBottomAddr].members.IsAvailable && !m_CurrCTBFlags[leftBottomAddr].members.IsIntra &&
        isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX - 1, TPelY + nPSH, LPelX, TPelY);

    if (isAvailableA0 && (!isAvailableA1 || !hasEqualMotion(leftAddr, leftBottomAddr)))
    {
        UPDATE_MV_INFO(leftBottomAddr);
    }

    // early termination
    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    // above left
    if (Count < 4 && PartX > 0)
    {
        Ipp32s aboveLeftAddr = TUPartNumberInCTB - m_CurrCTBStride - 1;
        bool isAvailableB2 = m_CurrCTBFlags[aboveLeftAddr].members.IsAvailable && !m_CurrCTBFlags[aboveLeftAddr].members.IsIntra &&
            isDiffMER(m_pPicParamSet->log2_parallel_merge_level, LPelX - 1, TPelY - 1, LPelX, TPelY);

         if (isAvailableB2 &&
            (!isAvailableA1 || !hasEqualMotion(leftAddr, aboveLeftAddr)) &&
            (!isAvailableB1 || !hasEqualMotion(aboveAddr, aboveLeftAddr)))
        {
            UPDATE_MV_INFO(aboveLeftAddr);
        }
    }

    // early termination
    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    if (m_pSliceHeader->m_enableTMVPFlag)
    {
        Ipp32u bottomRightPartX = PartX + PartWidth;
        Ipp32u bottomRightPartY = PartY + PartHeight;
        Ipp32u centerPartX = PartX + (PartWidth >> 1);
        Ipp32u centerPartY = PartY + (PartHeight >> 1);
        bool ExistMV = false;
        bool checkBottomRight = true;

        if ((bottomRightPartX << m_pSeqParamSet->getQuadtreeTULog2MinSize()) >= m_pSeqParamSet->pic_width_in_luma_samples ||
            (bottomRightPartY << m_pSeqParamSet->getQuadtreeTULog2MinSize()) >= m_pSeqParamSet->pic_height_in_luma_samples ||
            YInc + PartHeight >= m_pCurrentFrame->getNumPartInCUSize()) // is not at the last column of LCU But is last row of LCU
        {
            checkBottomRight = false;
        }

        RefIndexType RefIdx = 0;
        H265MotionVector ColMV;
        Ipp8u dir = 0;
        Ipp32s ArrayAddr = Count;

        if (checkBottomRight)
            ExistMV = GetColMVP(REF_PIC_LIST_0, bottomRightPartX, bottomRightPartY, ColMV, RefIdx);

        if (!ExistMV)
            ExistMV = GetColMVP(REF_PIC_LIST_0, centerPartX, centerPartY, ColMV, RefIdx);

        if (ExistMV)
        {
            dir |= 1;
            MVBufferNeighbours[ArrayAddr << 1].setMVBuffer(ColMV, RefIdx);
        }

        if (B_SLICE == m_pSliceHeader->slice_type)
        {
            ExistMV = false;
            if (checkBottomRight)
                ExistMV = GetColMVP(REF_PIC_LIST_1, bottomRightPartX, bottomRightPartY, ColMV, RefIdx);

            if (!ExistMV)
                ExistMV = GetColMVP(REF_PIC_LIST_1, centerPartX, centerPartY, ColMV, RefIdx);

            if (ExistMV)
            {
                dir |= 2;
                MVBufferNeighbours[(ArrayAddr << 1) + 1].setMVBuffer(ColMV, RefIdx);
            }
        }

        if (dir != 0)
        {
            InterDirNeighbours[ArrayAddr] = dir;
            CandIsInter[ArrayAddr] = true;
            if (mrgCandIdx == Count)
            {
                return;
            }
            Count++;
        }
    }

    // early termination
    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    Ipp32s ArrayAddr = Count;
    Ipp32s Cutoff = ArrayAddr;

    if (m_pSliceHeader->slice_type == B_SLICE)
    {
        for (Ipp32u idx = 0; idx < Cutoff * (Cutoff - 1) && ArrayAddr != m_pSliceHeader->m_MaxNumMergeCand; idx++)
        {
            Ipp32s i = PriorityList0[idx];
            Ipp32s j = PriorityList1[idx];
            if (CandIsInter[i] && CandIsInter[j] && (InterDirNeighbours[i] & 0x1) && (InterDirNeighbours[j] & 0x2))
            {
                CandIsInter[ArrayAddr] = true;
                InterDirNeighbours[ArrayAddr] = 3;

                // get MV from cand[i] and cand[j]
                MVBufferNeighbours[ArrayAddr << 1].setMVBuffer(MVBufferNeighbours[i << 1].MV, MVBufferNeighbours[i << 1].RefIdx);
                MVBufferNeighbours[(ArrayAddr << 1) + 1].setMVBuffer(MVBufferNeighbours[(j << 1) + 1].MV, MVBufferNeighbours[(j << 1) + 1].RefIdx);

                Ipp32s RefPOCL0 = m_pSliceHeader->RefPOCList[REF_PIC_LIST_0][MVBufferNeighbours[(ArrayAddr << 1)].RefIdx];
                Ipp32s RefPOCL1 = m_pSliceHeader->RefPOCList[REF_PIC_LIST_1][MVBufferNeighbours[(ArrayAddr << 1) + 1].RefIdx];
                if (RefPOCL0 == RefPOCL1 && MVBufferNeighbours[(ArrayAddr << 1)].MV == MVBufferNeighbours[(ArrayAddr << 1) + 1].MV)
                {
                    CandIsInter[ArrayAddr] = false;
                }
                else
                {
                    ArrayAddr++;
                }
            }
        }
    }

    // early termination
    if (Count == m_pSliceHeader->m_MaxNumMergeCand)
        return;

    Ipp32s numRefIdx = (m_pSliceHeader->slice_type == B_SLICE) ? IPP_MIN(m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_0], m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_1]) : m_pSliceHeader->m_numRefIdx[REF_PIC_LIST_0];
    Ipp8s r = 0;
    Ipp32s refcnt = 0;

    while (ArrayAddr < m_pSliceHeader->m_MaxNumMergeCand)
    {
        CandIsInter[ArrayAddr] = true;
        InterDirNeighbours[ArrayAddr] = 1;
        MVBufferNeighbours[ArrayAddr << 1].setMVBuffer(H265MotionVector(0, 0), r);

        if (m_pSliceHeader->slice_type == B_SLICE)
        {
            InterDirNeighbours[ArrayAddr] = 3;
            MVBufferNeighbours[(ArrayAddr << 1) + 1].setMVBuffer(H265MotionVector(0, 0), r);
        }

        ArrayAddr++;

        if (refcnt == numRefIdx - 1)
            r = 0;
        else
        {
            r++;
            refcnt++;
        }
    }

    numValidMergeCand = ArrayAddr;
}

void H265SegmentDecoder::fillMVPCand(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32u PartIdx, EnumRefPicList RefPicList, Ipp32s RefIdx, AMVPInfo* pInfo)
{
    H265MotionVector MvPred;
    bool Added = false;

    pInfo->NumbOfCands = 0;
    if (RefIdx < 0)
    {
        return;
    }

    // compute the location of the current PU
    Ipp32s XInc = g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s YInc = g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];
    Ipp32s LPelX = pCU->m_CUPelX + XInc;
    Ipp32s TPelY = pCU->m_CUPelY + YInc;
    XInc >>= m_pSeqParamSet->getQuadtreeTULog2MinSize();
    YInc >>= m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s PartX = LPelX >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s PartY = TPelY >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s TUPartNumberInCTB = m_CurrCTBStride * YInc + XInc;

    Ipp32s nPSW, nPSH;
    pCU->getPartSize(AbsPartIdx, PartIdx, nPSW, nPSH);
    Ipp32s PartWidth = nPSW >> m_pSeqParamSet->getQuadtreeTULog2MinSize();
    Ipp32s PartHeight = nPSH >> m_pSeqParamSet->getQuadtreeTULog2MinSize();

    // Left predictor search
    Ipp32s leftAddr = TUPartNumberInCTB + m_CurrCTBStride * (PartHeight - 1) - 1;
    Ipp32s leftBottomAddr = TUPartNumberInCTB - 1 + m_CurrCTBStride * PartHeight;

    Added = AddMVPCand(pInfo, RefPicList, RefIdx, leftBottomAddr);
    if (!Added)
    {
        Added = AddMVPCand(pInfo, RefPicList, RefIdx, leftAddr);
    }
    if (!Added)
    {
        Added = AddMVPCandOrder(pInfo, RefPicList, RefIdx, leftBottomAddr);

        if (!Added)
        {
            Added = AddMVPCandOrder(pInfo, RefPicList, RefIdx, leftAddr);
        }
    }

    // Above predictor search
    Ipp32s aboveAddr = TUPartNumberInCTB - m_CurrCTBStride + (PartWidth - 1);
    Ipp32s aboveRightAddr = TUPartNumberInCTB - m_CurrCTBStride + PartWidth;
    Ipp32s aboveLeftAddr = TUPartNumberInCTB - m_CurrCTBStride - 1;

    Added = AddMVPCand(pInfo, RefPicList, RefIdx, aboveRightAddr);
    if (!Added)
    {
        Added = AddMVPCand(pInfo, RefPicList, RefIdx, aboveAddr);
    }
    if (!Added)
    {
        Added = AddMVPCand(pInfo, RefPicList, RefIdx, aboveLeftAddr);
    }

    if (pInfo->NumbOfCands == 2)
        Added = true;
    else
    {
        Added = m_CurrCTBFlags[leftBottomAddr].members.IsAvailable && !m_CurrCTBFlags[leftBottomAddr].members.IsIntra;

        if (!Added)
            Added = m_CurrCTBFlags[leftAddr].members.IsAvailable && !m_CurrCTBFlags[leftAddr].members.IsIntra;
    }

    if (!Added)
    {
        Added = AddMVPCandOrder(pInfo, RefPicList, RefIdx, aboveRightAddr);

        if (!Added)
        {
            Added = AddMVPCandOrder(pInfo, RefPicList, RefIdx, aboveAddr);
        }
        if (!Added)
        {
            Added = AddMVPCandOrder(pInfo, RefPicList, RefIdx, aboveLeftAddr);
        }
    }

    if (pInfo->NumbOfCands == 2)
    {
        if (pInfo->MVCandidate[0] == pInfo->MVCandidate[1])
        {
            pInfo->NumbOfCands = 1;
        }
    }

    if (m_pSliceHeader->m_enableTMVPFlag)
    {
        Ipp32u bottomRightPartX = PartX + PartWidth;
        Ipp32u bottomRightPartY = PartY + PartHeight;
        H265MotionVector ColMv;
        bool checkBottomRight = true;

        if ((bottomRightPartX << m_pSeqParamSet->getQuadtreeTULog2MinSize()) >= m_pSeqParamSet->pic_width_in_luma_samples ||
            (bottomRightPartY << m_pSeqParamSet->getQuadtreeTULog2MinSize()) >= m_pSeqParamSet->pic_height_in_luma_samples ||
            YInc + PartHeight >= m_pCurrentFrame->getNumPartInCUSize()) // is not at the last column of LCU But is last row of LCU
        {
            checkBottomRight = false;
        }

        if (checkBottomRight && GetColMVP(RefPicList, bottomRightPartX, bottomRightPartY, ColMv, RefIdx))
        {
            pInfo->MVCandidate[pInfo->NumbOfCands++] = ColMv;
        }
        else
        {
            Ipp32u centerPartX = PartX + (PartWidth >> 1);
            Ipp32u centerPartY = PartY + (PartHeight >> 1);

            if (GetColMVP(RefPicList, centerPartX, centerPartY, ColMv, RefIdx))
            {
                pInfo->MVCandidate[pInfo->NumbOfCands++] = ColMv;
            }
        }
    }

    if (pInfo->NumbOfCands > AMVP_MAX_NUM_CANDS)
    {
        pInfo->NumbOfCands = AMVP_MAX_NUM_CANDS;
    }
    while (pInfo->NumbOfCands < AMVP_MAX_NUM_CANDS)
    {
        pInfo->MVCandidate[pInfo->NumbOfCands].setZero();
        pInfo->NumbOfCands++;
    }

    return;
}

bool H265SegmentDecoder::AddMVPCand(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr)
{
    if (!m_CurrCTBFlags[NeibAddr].members.IsAvailable || m_CurrCTBFlags[NeibAddr].members.IsIntra)
        return false;

    if (m_CurrCTB[NeibAddr].mvinfo[RefPicList].RefIdx == RefIdx)
    {
        pInfo->MVCandidate[pInfo->NumbOfCands++] = m_CurrCTB[NeibAddr].mvinfo[RefPicList].MV;
        return true;
    }

    EnumRefPicList RefPicList2nd = EnumRefPicList(1 - RefPicList);
    RefIndexType NeibRefIdx = m_CurrCTB[NeibAddr].mvinfo[RefPicList2nd].RefIdx;

    if (NeibRefIdx >= 0)
    {
        if(m_pRefPicList[RefPicList][RefIdx].refFrame->m_PicOrderCnt == m_pRefPicList[RefPicList2nd][NeibRefIdx].refFrame->m_PicOrderCnt)
        {
            pInfo->MVCandidate[pInfo->NumbOfCands++] = m_CurrCTB[NeibAddr].mvinfo[RefPicList2nd].MV;
            return true;
        }
    }
    return false;
}

static Ipp32s GetDistScaleFactor(Ipp32s DiffPocB, Ipp32s DiffPocD)
{
    if (DiffPocD == DiffPocB)
    {
        return 4096;
    }
    else
    {
        Ipp32s TDB = Clip3(-128, 127, DiffPocB);
        Ipp32s TDD = Clip3(-128, 127, DiffPocD);
        Ipp32s X = (0x4000 + abs(TDD / 2)) / TDD;
        Ipp32s Scale = Clip3(-4096, 4095, (TDB * X + 32) >> 6);
        return Scale;
    }
}

bool H265SegmentDecoder::AddMVPCandOrder(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr)
{
    if (!m_CurrCTBFlags[NeibAddr].members.IsAvailable || m_CurrCTBFlags[NeibAddr].members.IsIntra)
        return false;

    Ipp32s CurrRefTb = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[RefPicList][RefIdx].refFrame->m_PicOrderCnt;
    bool IsCurrRefLongTerm = m_pRefPicList[RefPicList][RefIdx].isLongReference;

    for (Ipp32s i = 0; i < 2; i++)
    {
        RefIndexType NeibRefIdx = m_CurrCTB[NeibAddr].mvinfo[RefPicList].RefIdx;

        if (NeibRefIdx >= 0)
        {
            bool IsNeibRefLongTerm = m_pRefPicList[RefPicList][NeibRefIdx].isLongReference;

            if (IsNeibRefLongTerm == IsCurrRefLongTerm)
            {
                H265MotionVector MvPred = m_CurrCTB[NeibAddr].mvinfo[RefPicList].MV;

                if (IsCurrRefLongTerm)
                {
                    pInfo->MVCandidate[pInfo->NumbOfCands++] = MvPred;
                }
                else
                {
                    Ipp32s NeibRefTb = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[RefPicList][NeibRefIdx].refFrame->m_PicOrderCnt;
                    Ipp32s Scale = GetDistScaleFactor(CurrRefTb, NeibRefTb);

                    if (Scale == 4096)
                        pInfo->MVCandidate[pInfo->NumbOfCands++] = MvPred;
                    else
                        pInfo->MVCandidate[pInfo->NumbOfCands++] = MvPred.scaleMV(Scale);
                }

                return true;
            }
        }

        RefPicList = EnumRefPicList(1 - RefPicList);
    }

    return false;
}

bool H265SegmentDecoder::GetColMVP(EnumRefPicList refPicListIdx, Ipp32u PartX, Ipp32u PartY, H265MotionVector &rcMv, Ipp32s refIdx)
{
    bool isCurrRefLongTerm, isColRefLongTerm;

    Ipp32u Log2MinTUSize = m_pSeqParamSet->getQuadtreeTULog2MinSize();

    /* MV compression */
    if (Log2MinTUSize < 4)
    {
        Ipp32u shift = (4 - Log2MinTUSize);
        PartX = (PartX >> shift) << shift;
        PartY = (PartY >> shift) << shift;
    }

    EnumRefPicList colPicListIdx = EnumRefPicList((B_SLICE == m_pSliceHeader->slice_type) ? 1 - m_pSliceHeader->collocated_from_l0_flag : 0);
    H265DecoderFrame *colPic = m_pRefPicList[colPicListIdx][m_pSliceHeader->m_ColRefIdx].refFrame;
    Ipp32u colocatedPartNumber = PartY * m_pCurrentFrame->getNumPartInCUSize() * m_pCurrentFrame->getFrameWidthInCU() + PartX;

    // Intra mode is marked in ref list 0 only
    if (COL_TU_INTRA == colPic->m_CodingData->m_ColTUFlags[REF_PIC_LIST_0][colocatedPartNumber])
        return false;

    if (m_pSliceHeader->m_CheckLDC)
        colPicListIdx = refPicListIdx;
    else if (m_pSliceHeader->collocated_from_l0_flag)
        colPicListIdx = REF_PIC_LIST_1;
    else
        colPicListIdx = REF_PIC_LIST_0;

    if (COL_TU_INVALID_INTER == colPic->m_CodingData->m_ColTUFlags[colPicListIdx][colocatedPartNumber])
    {
        colPicListIdx = EnumRefPicList(1 - colPicListIdx);
        if (COL_TU_INVALID_INTER == colPic->m_CodingData->m_ColTUFlags[colPicListIdx][colocatedPartNumber])
            return false;
    }

    isCurrRefLongTerm = m_pRefPicList[refPicListIdx][refIdx].isLongReference;

    if (COL_TU_LT_INTER == colPic->m_CodingData->m_ColTUFlags[colPicListIdx][colocatedPartNumber])
        isColRefLongTerm = true;
    else
    {
        VM_ASSERT(COL_TU_ST_INTER == colPic->m_CodingData->m_ColTUFlags[colPicListIdx][colocatedPartNumber]);
        isColRefLongTerm = false;
    }

    if (isCurrRefLongTerm != isColRefLongTerm)
        return false;

    H265MotionVector &cMvPred = colPic->m_CodingData->m_ColTUMV[colPicListIdx][colocatedPartNumber];

    if (isCurrRefLongTerm)
    {
        rcMv = cMvPred;
    }
    else
    {
        Ipp32s currRefTb = m_pCurrentFrame->m_PicOrderCnt - m_pRefPicList[refPicListIdx][refIdx].refFrame->m_PicOrderCnt;
        Ipp32s colRefTb = colPic->m_CodingData->m_ColTUPOCDelta[colPicListIdx][colocatedPartNumber];
        Ipp32s scale = GetDistScaleFactor(currRefTb, colRefTb);

        if (scale == 4096)
            rcMv = cMvPred;
        else
            rcMv = cMvPred.scaleMV(scale);
    }

    return true;
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
