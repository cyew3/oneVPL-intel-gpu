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

namespace UMC_HEVC_DECODER
{

#pragma warning(disable: 4127)

class SegmentDecoderHPBase_H265
{
public:
    virtual ~SegmentDecoderHPBase_H265() {}

    virtual UMC::Status DecodeSegmentCABAC(Ipp32u curMB, Ipp32u nMaxMBNumber, H265SegmentDecoderMultiThreaded * sd) = 0;

    virtual UMC::Status DecodeSegmentCABAC_Single_H265(Ipp32s curMB, Ipp32s nMacroBlocksToDecode,
        H265SegmentDecoderMultiThreaded * sd) = 0;

    virtual UMC::Status ReconstructSegment(Ipp32u curMB,Ipp32u nMaxMBNumber,
        H265SegmentDecoderMultiThreaded * sd) = 0;

    virtual void RestoreErrorRect(Ipp32s startMb, Ipp32s endMb, H265DecoderFrame *pRefFrame,
        H265SegmentDecoderMultiThreaded * sd) = 0;
};

template <typename Coeffs, typename PlaneY, typename PlaneUV, Ipp32s color_format, Ipp32s is_field, bool is_high_profile>
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

template <typename Coeffs, typename PlaneY, typename PlaneUV, Ipp32s color_format, Ipp32s is_field, bool is_high_profile>
class MBNullDecoder //:
    /*public ResidualDecoderCABAC<Coeffs, color_format, is_field>,
    public ResidualDecoderPCM<Coeffs, PlaneY, PlaneUV, color_format, is_field>*/
{
public:
    virtual
    ~MBNullDecoder(void)
    {
    } // MBNullDecoder(void)

    void DecodeCodingUnit_CABAC(H265SegmentDecoderMultiThreaded *)
    {
    } // void DecodeCodingUnit_CABAC
};

template <typename Coeffs, typename PlaneY, typename PlaneUV, Ipp32s color_format, Ipp32s is_field, bool is_high_profile, bool nv12_support = true>
class MBReconstructor_H265
{
public:

    enum
    {
        width_chroma_div = nv12_support ? 0 : (color_format < 3),  // for plane
        height_chroma_div = (color_format < 2),  // for plane
    };

    typedef PlaneY * PlanePtrY;
    typedef PlaneUV * PlanePtrUV;
    typedef Coeffs *  CoeffsPtr;

    virtual ~MBReconstructor_H265() {}

    void ReconstructCodingUnit(H265SegmentDecoderMultiThreaded * sd)
    {
        sd->ReconstructCU(sd->m_curCU, sd->m_curCU, 0, 0);
    } // void ReconstructCodingUnit(H265SegmentDecoderMultiThreaded * sd)
};

template <class Decoder, class Reconstructor, typename Coeffs, typename PlaneY, typename PlaneUV, Ipp32s color_format, Ipp32s is_field, bool is_high_profile>
class SegmentDecoderHP_H265 : public SegmentDecoderHPBase_H265,
    public Decoder,
    public Reconstructor
{
public:
    typedef PlaneY * PlanePtrY;
    typedef PlaneUV * PlanePtrUV;
    typedef Coeffs *  CoeffsPtr;

    typedef SegmentDecoderHP_H265<Decoder, Reconstructor, Coeffs,
        PlaneY, PlaneUV, color_format, is_field, is_high_profile> ThisClassType;

    UMC::Status DecodeSegmentCABAC(Ipp32u curMB,
                              Ipp32u nBorder,
                              H265SegmentDecoderMultiThreaded *sd)
    {
        UMC::Status umcRes = UMC::UMC_OK;
        bool (ThisClassType::*pDecFunc)(H265SegmentDecoderMultiThreaded *sd);

        sd->m_CurMBAddr = curMB;

        // select decoding function
        //switch (sd->m_pSliceHeader->slice_type)
        //{
            // intra coded slice
        //case I_SLICE:
//      //      pDecFunc = &ThisClassType::DecodeMacroblock_ISlice_CABAC; //renamed as DecodeCodingUnit_ISlice_CABAC
        //    pDecFunc = &ThisClassType::DecodeCodingUnit_CABAC;
        //    break;

            // predicted slice
        //case P_SLICE:
        //    pDecFunc = &ThisClassType::DecodeMacroblock_PSlice_CABAC;
        //    break;

            // bidirectional predicted slice
        //default:
        //    pDecFunc = &ThisClassType::DecodeMacroblock_BSlice_CABAC;
        //    break;
        //};

        pDecFunc = &ThisClassType::DecodeCodingUnit_CABAC;

        for (; curMB < nBorder; curMB += 1)
        {
            // align pointer to improve performance
            sd->m_pCoeffBlocksWrite = (H265CoeffsPtrCommon) (UMC::align_pointer<CoeffsPtr> (sd->m_pCoeffBlocksWrite, 16));

            //sd->UpdateCurrentMBInfo();

            (this->*pDecFunc)(sd);

            // decode end of slice
            {
                Ipp32s end_of_slice;

                end_of_slice = sd->m_pBitStream->DecodeSymbolEnd_CABAC(); //DecodeTerminatingBit_CABAC() ?

                if (end_of_slice)
                {
                    sd->m_pBitStream->TerminateDecode_CABAC();

                    sd->m_CurMBAddr += 1;
                    umcRes = UMC::UMC_ERR_END_OF_STREAM;
                    break;
                }
            }

            // set next MB addres
            sd->m_CurMBAddr += 1;
        }

        // save bitstream variables
        sd->m_pSlice->SetStateVariables(sd->m_MBSkipCount, sd->m_QuantPrev, sd->m_prev_dquant);
        return umcRes;
    }

    virtual UMC::Status DecodeSegmentCABAC_Single_H265(Ipp32s curCUAddr, Ipp32s nBorder,
                                             H265SegmentDecoderMultiThreaded * sd)
    {
        UMC::Status umcRes = UMC::UMC_OK;
        Ipp32s rsCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);

        if (!sd->m_pSlice->getDependentSliceSegmentFlag())
            sd->ResetRowBuffer();

        for (;;)
        {
            sd->m_curCU = sd->m_pCurrentFrame->getCU(rsCUAddr);
            sd->m_curCU->initCU(sd, rsCUAddr);

            START_TICK;
            sd->DecodeSAOOneLCU(sd->m_curCU);
            bool is_last = DecodeCodingUnit_CABAC(sd); //decode CU
            END_TICK(decode_time);

            START_TICK1;
            ReconstructCodingUnit(sd); //Reconstruct CU h265 must be here
            END_TICK1(reconstruction_time);

            if (is_last)
                break;

            Ipp32s newCUAddr = curCUAddr + 1;
            Ipp32s newRSCUAddr = sd->m_pCurrentFrame->m_CodingData->getCUOrderMap(newCUAddr);

            if (newCUAddr >= nBorder)
                break;

            if (sd->m_pCurrentFrame->m_CodingData->getTileIdxMap(rsCUAddr) !=
                sd->m_pCurrentFrame->m_CodingData->getTileIdxMap(newRSCUAddr))
            {
                sd->ResetRowBuffer();
                sd->m_pBitStream->DecodeTerminatingBit_CABAC();

                // reset CABAC engine
                sd->m_pBitStream->InitializeDecodingEngine_CABAC();
                sd->m_pSlice->InitializeContexts();
            }
            else
                sd->UpdateCurrCUContext(rsCUAddr, newRSCUAddr);

            curCUAddr = newCUAddr;
            rsCUAddr = newRSCUAddr;
        }

        return umcRes;
    }

    virtual UMC::Status ReconstructSegment(Ipp32u curMB, Ipp32u nBorder,
                                      H265SegmentDecoderMultiThreaded * sd)
    {
        UMC::Status umcRes = UMC::UMC_OK;
        void (ThisClassType::*pRecFunc)(H265SegmentDecoderMultiThreaded *sd);

        sd->m_CurMBAddr = curMB;


        // select reconstruct function
        //switch (sd->m_pSliceHeader->slice_type)
        //{
            // intra coded slice
        //case I_SLICE:
        //    pRecFunc = &ThisClassType::ReconstructCodingUnit;
        //    break;
        //
            // predicted slice
        //case P_SLICE:
        //    pRecFunc = &ThisClassType::ReconstructMacroblock_PSlice;
        //    break;

            // bidirectional predicted slice
        //default:
        //    pRecFunc = &ThisClassType::ReconstructMacroblock_BSlice;
        //    break;
        //};

        pRecFunc = &ThisClassType::ReconstructCodingUnit;

        // reconstruct starts here
        for (;curMB < nBorder; curMB += 1)
        {
            // align pointer to improve performance
            sd->m_pCoeffBlocksRead = (H265CoeffsPtrCommon) (UMC::align_pointer<CoeffsPtr> (sd->m_pCoeffBlocksRead, 16));

            //sd->UpdateCurrentMBInfo();

            (this->*pRecFunc)(sd);

            // set next MB addres
            sd->m_CurMBAddr += 1;
        }

        return umcRes;
    }

    virtual void RestoreErrorPlane(PlanePtrY pRefPlane, PlanePtrY pCurrentPlane, Ipp32s pitch,
        Ipp32s offsetX, Ipp32s offsetY, Ipp32s offsetXL, Ipp32s offsetYL,
        Ipp32s mb_width, Ipp32s fieldOffset, IppiSize mbSize)
    {
            IppiSize roiSize;

            roiSize.height = mbSize.height;
            roiSize.width = mb_width * mbSize.width - offsetX;
            Ipp32s offset = offsetX + offsetY*pitch;

            if (offsetYL == offsetY)
            {
                roiSize.width = offsetXL - offsetX + mbSize.width;
            }

            if (pRefPlane)
            {
                CopyPlane_H265((Ipp16s*)pRefPlane + offset + (fieldOffset*pitch >> 1),
                            pitch,
                            (Ipp16s*)pCurrentPlane + offset + (fieldOffset*pitch >> 1),
                            pitch,
                            roiSize);
            }
            else
            {
                SetPlane_H265(128, (Ipp8u*)pCurrentPlane + offset + (fieldOffset*pitch >> 1), pitch, roiSize);
            }

            if (offsetYL > offsetY)
            {
                roiSize.height = mbSize.height;
                roiSize.width = offsetXL + mbSize.width;
                offset = offsetYL*pitch;

                if (pRefPlane)
                {
                    CopyPlane_H265((Ipp16s*)pRefPlane + offset + (fieldOffset*pitch >> 1),
                                pitch,
                                (Ipp16s*)pCurrentPlane + offset + (fieldOffset*pitch >> 1),
                                pitch,
                                roiSize);
                }
                else
                {
                    SetPlane_H265(128, (Ipp8u*)pCurrentPlane + offset + (fieldOffset*pitch >> 1), pitch, roiSize);
                }
            }

            if (offsetYL - offsetY > mbSize.height)
            {
                roiSize.height = offsetYL - offsetY - mbSize.height;
                roiSize.width = mb_width * mbSize.width;
                offset = (offsetY + mbSize.height)*pitch;

                if (pRefPlane)
                {
                    CopyPlane_H265((Ipp16s*)pRefPlane + offset + (fieldOffset*pitch >> 1),
                                pitch,
                                (Ipp16s*)pCurrentPlane + offset + (fieldOffset*pitch >> 1),
                                pitch,
                                roiSize);
                }
                else
                {
                    SetPlane_H265(128, (Ipp8u*)pCurrentPlane + offset + (fieldOffset*pitch >> 1), pitch, roiSize);
                }
            }
    }

    virtual void RestoreErrorRect(Ipp32s startMb, Ipp32s endMb, H265DecoderFrame *pRefFrame,
        H265SegmentDecoderMultiThreaded * sd)
    {
        if (startMb > 0)
            startMb--;

        if (startMb >= endMb)
            return;

        H265DecoderFrame * pCurrentFrame = sd->m_pSlice->GetCurrentFrame();
        sd->mb_height = sd->m_pSlice->GetMBHeight();
        sd->mb_width = sd->m_pSlice->GetMBWidth();

        Ipp32s pitch_luma = pCurrentFrame->pitch_luma();
        Ipp32s pitch_chroma = pCurrentFrame->pitch_chroma();

        Ipp32s fieldOffset = 0;

        Ipp32s MBYAdjust = 0;

        Ipp32s offsetX, offsetY;
        offsetX = (startMb % sd->mb_width) * 16;
        offsetY = ((startMb / sd->mb_width) - MBYAdjust) * 16;

        Ipp32s offsetXL = ((endMb - 1) % sd->mb_width) * 16;
        Ipp32s offsetYL = (((endMb - 1) / sd->mb_width) - MBYAdjust) * 16;

        IppiSize mbSize;

        mbSize.width = 16;
        mbSize.height = 16;

        if (pRefFrame && pRefFrame->m_pYPlane)
        {
            RestoreErrorPlane((PlanePtrY)pRefFrame->m_pYPlane, (PlanePtrY)pCurrentFrame->m_pYPlane, pitch_luma,
                    offsetX, offsetY, offsetXL, offsetYL,
                    sd->mb_width, fieldOffset, mbSize);
        }
        else
        {
            RestoreErrorPlane(0, (PlanePtrY)pCurrentFrame->m_pYPlane, pitch_luma,
                    offsetX, offsetY, offsetXL, offsetYL,
                    sd->mb_width, fieldOffset, mbSize);
        }

        bool nv12_support = (pCurrentFrame->GetColorFormat() == UMC::NV12);
        if (nv12_support)
        {
            offsetY >>= 1;
            offsetYL >>= 1;

            mbSize.height >>= 1;

            if (pRefFrame && pRefFrame->m_pUVPlane)
            {
                RestoreErrorPlane((PlanePtrUV)pRefFrame->m_pUVPlane, (PlanePtrUV)pCurrentFrame->m_pUVPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
            }
            else
            {
                RestoreErrorPlane(0, (PlanePtrUV)pCurrentFrame->m_pUVPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
            }
        }
        else
        {
            switch (pCurrentFrame->m_chroma_format)
            {
            case CHROMA_FORMAT_420_H265: // YUV420
                offsetX >>= 1;
                offsetY >>= 1;
                offsetXL >>= 1;
                offsetYL >>= 1;

                mbSize.width >>= 1;
                mbSize.height >>= 1;

                break;
            case CHROMA_FORMAT_422_H265: // YUV422
                offsetX >>= 1;
                offsetXL >>= 1;

                mbSize.width >>= 1;
                break;
            case CHROMA_FORMAT_444_H265: // YUV444
                break;

            case CHROMA_FORMAT_400_H265: // YUV400
                return;
            default:
                VM_ASSERT(false);
                return;
            }

            if (pRefFrame && pRefFrame->m_pUPlane && pRefFrame->m_pVPlane)
            {
                RestoreErrorPlane((PlanePtrUV)pRefFrame->m_pUPlane, (PlanePtrUV)pCurrentFrame->m_pUPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
                RestoreErrorPlane((PlanePtrUV)pRefFrame->m_pVPlane, (PlanePtrUV)pCurrentFrame->m_pVPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
            }
            else
            {
                RestoreErrorPlane(0, (PlanePtrUV)pCurrentFrame->m_pUPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
                RestoreErrorPlane(0, (PlanePtrUV)pCurrentFrame->m_pVPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
            }
        }
    }



};

template <typename Coeffs, typename PlaneY, typename PlaneUV, bool is_field, Ipp32s color_format, bool is_high_profile>
class CreateSoftSegmentDecoderWrapper
{
public:

    static SegmentDecoderHPBase_H265* CreateSegmentDecoder()
    {
        static SegmentDecoderHP_H265<
            MBDecoder_H265<Coeffs, PlaneY, PlaneUV, color_format, is_field, is_high_profile>,
            MBReconstructor_H265<Coeffs, PlaneY, PlaneUV, color_format, is_field, is_high_profile>,
            Coeffs, PlaneY, PlaneUV, color_format, is_field, is_high_profile> k;
        return &k;
    }
};

template <typename Coeffs, typename PlaneY, typename PlaneUV, bool is_field>
class CreateSegmentDecoderWrapper
{
public:

    static SegmentDecoderHPBase_H265* CreateSoftSegmentDecoder(Ipp32s color_format, bool is_high_profile)
    {
        static SegmentDecoderHPBase_H265* global_sds_array[4][2] = {0};

        if (global_sds_array[0][0] == 0)
        {
#undef INIT
#define INIT(cf, hp) global_sds_array[cf][hp] = CreateSoftSegmentDecoderWrapper<Coeffs, PlaneY, PlaneUV, is_field, cf, hp>::CreateSegmentDecoder();

            INIT(3, true);
            INIT(2, true);
            INIT(1, true);
            INIT(0, true);
            INIT(3, false);
            INIT(2, false);
            INIT(1, false);
            INIT(0, false);
        }

        return global_sds_array[color_format][is_high_profile];
    }
};

#pragma warning(default: 4127)

// declare functions for creating proper decoders
extern
SegmentDecoderHPBase_H265* CreateSD_H265(Ipp32s bit_depth_luma,
                               Ipp32s bit_depth_chroma,
                               bool is_field,
                               Ipp32s color_format,
                               bool is_high_profile);
extern
SegmentDecoderHPBase_H265* CreateSD_ManyBits_H265(Ipp32s bit_depth_luma,
                                        Ipp32s bit_depth_chroma,
                                        bool is_field,
                                        Ipp32s color_format,
                                        bool is_high_profile);

} // end namespace UMC_HEVC_DECODER

#endif // __UMC_H264_SEGMENT_DECODER_TEMPLATES_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
