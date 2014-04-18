/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_SEGMENT_DECODER_TEMPLATES_H
#define __UMC_H265_SEGMENT_DECODER_TEMPLATES_H

#include "umc_h265_dec_internal_cabac.h"
#include "umc_h265_timing.h"
#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_slice_decoding.h"

namespace UMC_HEVC_DECODER
{

// in past it was template. class doesn't contains data. only virtual functions.
class SegmentDecoderRoutines
{
public:

    // Decode one CTB
    bool DecodeCodingUnit_CABAC(H265SegmentDecoderMultiThreaded *sd)
    {
        if (sd->m_cu->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
        {
            sd->m_DecodeDQPFlag = true;
        }

        Ipp32u IsLast = 0;
        sd->DecodeCUCABAC(0, 0, IsLast);
        return IsLast > 0;
    } // void DecodeCodingUnit_CABAC(H265SegmentDecoderMultiThreaded *sd)

    // Decode CTB range
    virtual UMC::Status DecodeSegment(Ipp32s curCUAddr, Ipp32s &nBorder, H265SegmentDecoderMultiThreaded * sd)
    {
        UMC::Status umcRes = UMC::UMC_OK;
        Ipp32s rsCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);

        for (;;)
        {
            sd->m_cu = sd->m_pCurrentFrame->getCU(rsCUAddr);
            sd->m_cu->initCU(sd, rsCUAddr);

            START_TICK;
            sd->DecodeSAOOneLCU();
            bool is_last = DecodeCodingUnit_CABAC(sd); //decode CU
            END_TICK(decode_time);

            if (is_last)
            {
                umcRes = UMC::UMC_ERR_END_OF_STREAM;
                nBorder = curCUAddr + 1;
                if (sd->m_pPicParamSet->entropy_coding_sync_enabled_flag && rsCUAddr % sd->m_pSeqParamSet->WidthInCU == 1)
                {
                    // Save CABAC context after 2nd CTB
                    MFX_INTERNAL_CPY(sd->m_pBitStream->wpp_saved_cabac_context, sd->m_pBitStream->context_hevc, sizeof(sd->m_pBitStream->context_hevc));
                }
                break;
            }

            Ipp32s newCUAddr = curCUAddr + 1;
            Ipp32s newRSCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(newCUAddr);

            if (newRSCUAddr >= sd->m_pCurrentFrame->m_CodingData->m_NumCUsInFrame ||
                sd->m_pCurrentFrame->m_CodingData->getTileIdxMap(rsCUAddr) != sd->m_pCurrentFrame->m_CodingData->getTileIdxMap(newRSCUAddr))
                break;

            if (sd->m_pPicParamSet->entropy_coding_sync_enabled_flag)
            {
                Ipp32u CUX = rsCUAddr % sd->m_pSeqParamSet->WidthInCU;
                bool end_of_row = (CUX == sd->m_pSeqParamSet->WidthInCU - 1);

                if (end_of_row)
                {
                    Ipp32u uVal = sd->m_pBitStream->DecodeTerminatingBit_CABAC();
                    VM_ASSERT(uVal);
                }

                if (CUX == 1)
                {
                    // Save CABAC context after 2nd CTB
                    MFX_INTERNAL_CPY(sd->m_pBitStream->wpp_saved_cabac_context, sd->m_pBitStream->context_hevc, sizeof(sd->m_pBitStream->context_hevc));
                }

                if (end_of_row)
                {
                    // Reset CABAC state
                    sd->m_pBitStream->InitializeDecodingEngine_CABAC();
                    sd->m_context->SetNewQP(sd->m_pSliceHeader->SliceQP);

                    // Should load CABAC context from saved buffer
                    if (sd->m_pSeqParamSet->WidthInCU > 1 &&
                        sd->m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(rsCUAddr + 2 - sd->m_pSeqParamSet->WidthInCU) >= sd->m_pSliceHeader->SliceCurStartCUAddr / sd->m_pCurrentFrame->m_CodingData->m_NumPartitions)
                    {
                        // Restore saved CABAC context
                        MFX_INTERNAL_CPY(sd->m_pBitStream->context_hevc, sd->m_pBitStream->wpp_saved_cabac_context, sizeof(sd->m_pBitStream->context_hevc));
                    }
                    else
                    {
                        // Reset CABAC contexts
                        sd->m_pSlice->InitializeContexts();
                    }
                }
            }

            sd->m_context->UpdateCurrCUContext(rsCUAddr, newRSCUAddr);

            if (newCUAddr >= nBorder)
            {
                break;
            }

            curCUAddr = newCUAddr;
            rsCUAddr = newRSCUAddr;
        }

        return umcRes;
    }

    // Reconstruct CTB range
    virtual UMC::Status ReconstructSegment(Ipp32s curCUAddr, Ipp32s nBorder, H265SegmentDecoderMultiThreaded * sd)
    {
        UMC::Status umcRes = UMC::UMC_OK;
        Ipp32s rsCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);

        sd->m_cu = sd->m_pCurrentFrame->getCU(rsCUAddr);

        for (;;)
        {
            START_TICK1;
            sd->ReconstructCU(0, 0);
            END_TICK1(reconstruction_time);

            curCUAddr++;
            Ipp32s newRSCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);

            if (newRSCUAddr >= sd->m_pCurrentFrame->m_CodingData->m_NumCUsInFrame)
                break;

            sd->m_context->UpdateRecCurrCTBContext(rsCUAddr, newRSCUAddr);

            if (curCUAddr >= nBorder)
            {
                break;
            }

            rsCUAddr = newRSCUAddr;
            sd->m_cu = sd->m_pCurrentFrame->getCU(rsCUAddr);
        }

        return umcRes;
    }

    // Both decode and reconstruct a CTB range
    virtual UMC::Status DecodeSegmentCABAC_Single_H265(Ipp32s curCUAddr, Ipp32s & nBorder, H265SegmentDecoderMultiThreaded * sd)
    {
        UMC::Status umcRes = UMC::UMC_OK;
        Ipp32s rsCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);

        H265CoeffsPtrCommon saveBuffer = sd->m_context->m_coeffsWrite;

        for (;;)
        {
            sd->m_cu = sd->m_pCurrentFrame->getCU(rsCUAddr);
            sd->m_cu->initCU(sd, rsCUAddr);

            sd->m_context->m_coeffsRead = saveBuffer;
            sd->m_context->m_coeffsWrite = saveBuffer;

            START_TICK;
            sd->DecodeSAOOneLCU();
            bool is_last = DecodeCodingUnit_CABAC(sd); //decode CU
            END_TICK(decode_time);

            START_TICK1;
            sd->ReconstructCU(0, 0);
            END_TICK1(reconstruction_time);

            if (is_last)
            {
                umcRes = UMC::UMC_ERR_END_OF_STREAM;
                nBorder = curCUAddr + 1;

                if (sd->m_pPicParamSet->entropy_coding_sync_enabled_flag && rsCUAddr % sd->m_pSeqParamSet->WidthInCU == 1)
                {
                    // Save CABAC context after 2nd CTB
                    MFX_INTERNAL_CPY(sd->m_pBitStream->wpp_saved_cabac_context, sd->m_pBitStream->context_hevc, sizeof(sd->m_pBitStream->context_hevc));
                }
                break;
            }

            Ipp32s newCUAddr = curCUAddr + 1;
            Ipp32s newRSCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(newCUAddr);

            if (newRSCUAddr >= sd->m_pCurrentFrame->m_CodingData->m_NumCUsInFrame)
                break;

            if (sd->m_pCurrentFrame->m_CodingData->getTileIdxMap(rsCUAddr) !=
                sd->m_pCurrentFrame->m_CodingData->getTileIdxMap(newRSCUAddr))
            {
                sd->m_context->ResetRowBuffer();
                sd->m_context->SetNewQP(sd->m_pSliceHeader->SliceQP);
                sd->m_context->ResetRecRowBuffer();
                sd->m_pBitStream->DecodeTerminatingBit_CABAC();

                // reset CABAC engine
                sd->m_pBitStream->InitializeDecodingEngine_CABAC();
                sd->m_pSlice->InitializeContexts();
            }
            else
            {
                if (sd->m_pPicParamSet->entropy_coding_sync_enabled_flag)
                {
                    Ipp32u CUX = rsCUAddr % sd->m_pSeqParamSet->WidthInCU;
                    bool end_of_row = (CUX == sd->m_pSeqParamSet->WidthInCU - 1);

                    if (end_of_row)
                    {
                        Ipp32u uVal = sd->m_pBitStream->DecodeTerminatingBit_CABAC();
                        VM_ASSERT(uVal);
                    }

                    if (CUX == 1)
                    {
                        // Save CABAC context after 2nd CTB
                        MFX_INTERNAL_CPY(sd->m_pBitStream->wpp_saved_cabac_context, sd->m_pBitStream->context_hevc, sizeof(sd->m_pBitStream->context_hevc));
                    }

                    if (end_of_row)
                    {
                        // Reset CABAC state
                        sd->m_pBitStream->InitializeDecodingEngine_CABAC();
                        sd->m_context->SetNewQP(sd->m_pSliceHeader->SliceQP);

                        // Should load CABAC context from saved buffer
                        if (sd->m_pSeqParamSet->WidthInCU > 1 &&
                            sd->m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(rsCUAddr + 2 - sd->m_pSeqParamSet->WidthInCU) >= sd->m_pSliceHeader->SliceCurStartCUAddr / sd->m_pCurrentFrame->m_CodingData->m_NumPartitions)
                        {
                            // Restore saved CABAC context
                            MFX_INTERNAL_CPY(sd->m_pBitStream->context_hevc, sd->m_pBitStream->wpp_saved_cabac_context, sizeof(sd->m_pBitStream->context_hevc));
                        }
                        else
                        {
                            // Reset CABAC contexts
                            sd->m_pSlice->InitializeContexts();
                        }
                    }
                }
                sd->m_context->UpdateCurrCUContext(rsCUAddr, newRSCUAddr);
                sd->m_context->UpdateRecCurrCTBContext(rsCUAddr, newRSCUAddr);
            }

            if (newCUAddr >= nBorder)
            {
                break;
            }

            curCUAddr = newCUAddr;
            rsCUAddr = newRSCUAddr;
        }

        return umcRes;
    }
};

} // end namespace UMC_HEVC_DECODER

#endif // __UMC_H265_SEGMENT_DECODER_TEMPLATES_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
