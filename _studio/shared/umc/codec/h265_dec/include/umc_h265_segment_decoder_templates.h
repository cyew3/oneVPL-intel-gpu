/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
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

#pragma warning(disable: 4127)

class SegmentDecoderHPBase_H265
{
public:
    virtual ~SegmentDecoderHPBase_H265() {}

    virtual UMC::Status DecodeSegmentCABAC_Single_H265(Ipp32s curMB, Ipp32s nMacroBlocksToDecode,
        H265SegmentDecoderMultiThreaded * sd) = 0;
};

template <typename Coeffs, typename PlaneY, typename PlaneUV>
class MBDecoder_H265
{
public:


    bool DecodeCodingUnit_CABAC(H265SegmentDecoderMultiThreaded *sd)
    {
        if (sd->m_curCU->m_SliceHeader->m_PicParamSet->cu_qp_delta_enabled_flag)
        {
            sd->m_DecodeDQPFlag = true;
        }

        Ipp32u IsLast = 0;
        sd->DecodeCUCABAC(sd->m_curCU, 0, 0, IsLast);
        return IsLast > 0;
    } // void DecodeCodingUnit_CABAC(H265SegmentDecoderMultiThreaded *sd)
};

template <typename Coeffs, typename PlaneY, typename PlaneUV>
class MBReconstructor_H265
{
public:

    typedef PlaneY * PlanePtrY;
    typedef PlaneUV * PlanePtrUV;
    typedef Coeffs *  CoeffsPtr;

    virtual ~MBReconstructor_H265() {}

    void ReconstructCodingUnit(H265SegmentDecoderMultiThreaded * sd)
    {
        sd->ReconstructCU(sd->m_curCU, 0, 0);
    } // void ReconstructCodingUnit(H265SegmentDecoderMultiThreaded * sd)
};

template <class Decoder, class Reconstructor, typename Coeffs, typename PlaneY, typename PlaneUV>
class SegmentDecoderHP_H265 : public SegmentDecoderHPBase_H265,
    public Decoder,
    public Reconstructor
{
public:
    typedef PlaneY * PlanePtrY;
    typedef PlaneUV * PlanePtrUV;
    typedef Coeffs *  CoeffsPtr;

    typedef SegmentDecoderHP_H265<Decoder, Reconstructor, Coeffs,
        PlaneY, PlaneUV> ThisClassType;

    virtual UMC::Status DecodeSegmentCABAC_Single_H265(Ipp32s curCUAddr1, Ipp32s nBorder,
                                             H265SegmentDecoderMultiThreaded * sd)
    {
        Ipp32s curCUAddr = curCUAddr1;
        UMC::Status umcRes = UMC::UMC_OK;
        Ipp32s rsCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);

        if (!sd->m_pSlice->getDependentSliceSegmentFlag())
            sd->m_context->ResetRowBuffer();

        for (;;)
        {
            sd->m_curCU = sd->m_pCurrentFrame->getCU(rsCUAddr);
            sd->m_curCU->initCU(sd, rsCUAddr);

            START_TICK;
            sd->DecodeSAOOneLCU(sd->m_curCU);
            bool is_last = Decoder::DecodeCodingUnit_CABAC(sd); //decode CU
            END_TICK(decode_time);

            START_TICK1;
            Reconstructor::ReconstructCodingUnit(sd); //Reconstruct CU h265 must be here
            END_TICK1(reconstruction_time);

            if (is_last)
            {
                umcRes = UMC::UMC_ERR_END_OF_STREAM;
                nBorder = curCUAddr;
                break;
            }

            Ipp32s newCUAddr = curCUAddr + 1;
            Ipp32s newRSCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(newCUAddr);

            if (newCUAddr >= nBorder)
                break;

            if (sd->m_pCurrentFrame->m_CodingData->getTileIdxMap(rsCUAddr) !=
                sd->m_pCurrentFrame->m_CodingData->getTileIdxMap(newRSCUAddr))
            {
                sd->m_context->ResetRowBuffer();
                sd->m_pBitStream->DecodeTerminatingBit_CABAC();

                // reset CABAC engine
                sd->m_pBitStream->InitializeDecodingEngine_CABAC();
                sd->m_pSlice->InitializeContexts();
            }
            else
                sd->m_context->UpdateCurrCUContext(rsCUAddr, newRSCUAddr);

            curCUAddr = newCUAddr;
            rsCUAddr = newRSCUAddr;
        }

#if 0
        curCUAddr = curCUAddr1;
        rsCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);
        for (;;)
        {
            sd->m_curCU = sd->m_pCurrentFrame->getCU(rsCUAddr);

            START_TICK1;
            Reconstructor::ReconstructCodingUnit(sd); //Reconstruct CU h265 must be here
            END_TICK1(reconstruction_time);

            ++curCUAddr;
            if (curCUAddr >= nBorder)
                break;

            rsCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);
        }
#endif

        return umcRes;
    }
};

template <typename Coeffs, typename PlaneY, typename PlaneUV>
class CreateSoftSegmentDecoderWrapper
{
public:

    static SegmentDecoderHPBase_H265* CreateSegmentDecoder()
    {
        static SegmentDecoderHP_H265<
            MBDecoder_H265<Coeffs, PlaneY, PlaneUV>,
            MBReconstructor_H265<Coeffs, PlaneY, PlaneUV>,
            Coeffs, PlaneY, PlaneUV> k;
        return &k;
    }
};

template <typename Coeffs, typename PlaneY, typename PlaneUV>
class CreateSegmentDecoderWrapper
{
public:

    static SegmentDecoderHPBase_H265* CreateSoftSegmentDecoder()
    {
        static SegmentDecoderHPBase_H265* global_sds_array = 0;

        if (global_sds_array == 0)
        {
            global_sds_array = CreateSoftSegmentDecoderWrapper<Coeffs, PlaneY, PlaneUV>::CreateSegmentDecoder();
        }

        return global_sds_array;
    }
};

#pragma warning(default: 4127)

// declare functions for creating proper decoders
extern
SegmentDecoderHPBase_H265* CreateSD_H265(Ipp32s bit_depth_luma,
                               Ipp32s bit_depth_chroma);

} // end namespace UMC_HEVC_DECODER

#endif // __UMC_H264_SEGMENT_DECODER_TEMPLATES_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
