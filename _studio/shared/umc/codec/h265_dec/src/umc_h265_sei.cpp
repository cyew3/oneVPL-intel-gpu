/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_bitstream_headers.h"
#include "umc_h265_headers.h"

namespace UMC_HEVC_DECODER
{

// Parse SEI message
Ipp32s H265HeadersBitstream::ParseSEI(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return sei_message(sps,current_sps,spl);
}

// Parse SEI message
Ipp32s H265HeadersBitstream::sei_message(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    Ipp32u code;
    Ipp32s payloadType = 0;

    PeakNextBits(m_pbs, m_bitOffset, 8, code);
    while (code  ==  0xFF)
    {
        /* fixed-pattern bit string using 8 bits written equal to 0xFF */
        GetNBits(m_pbs, m_bitOffset, 8, code);
        payloadType += 255;
        PeakNextBits(m_pbs, m_bitOffset, 8, code);
    }

    Ipp32s last_payload_type_byte;    //Ipp32u integer using 8 bits
    GetNBits(m_pbs, m_bitOffset, 8, last_payload_type_byte);

    payloadType += last_payload_type_byte;

    Ipp32s payloadSize = 0;

    PeakNextBits(m_pbs, m_bitOffset, 8, code);
    while( code  ==  0xFF )
    {
        /* fixed-pattern bit string using 8 bits written equal to 0xFF */
        GetNBits(m_pbs, m_bitOffset, 8, code);
        payloadSize += 255;
        PeakNextBits(m_pbs, m_bitOffset, 8, code);
    }

    Ipp32s last_payload_size_byte;    //Ipp32u integer using 8 bits

    GetNBits(m_pbs, m_bitOffset, 8, last_payload_size_byte);
    payloadSize += last_payload_size_byte;
    spl->Reset();
    spl->payLoadSize = payloadSize;

    if (payloadType < 0 || payloadType > SEI_RESERVED)
        payloadType = SEI_RESERVED;

    spl->payLoadType = (SEI_TYPE)payloadType;

    if (spl->payLoadSize > BytesLeft())
    {
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    Ipp32u * pbs;
    Ipp32u bitOffsetU;
    Ipp32s bitOffset;

    GetState(&pbs, &bitOffsetU);
    bitOffset = bitOffsetU;

    Ipp32s ret = sei_payload(sps, current_sps, spl);

    for (Ipp32u i = 0; i < spl->payLoadSize; i++)
    {
        SkipNBits(pbs, bitOffset, 8);
    }

    SetState(pbs, bitOffset);

    return ret;
}

// Parse SEI payload data
Ipp32s H265HeadersBitstream::sei_payload(const HeaderSet<H265SeqParamSet> & sps,Ipp32s current_sps,H265SEIPayLoad *spl)
{
    Ipp32u payloadType =spl->payLoadType;
    switch( payloadType)
    {
    case SEI_PIC_TIMING_TYPE:
        return pic_timing(sps,current_sps,spl);
    case SEI_RECOVERY_POINT_TYPE:
        return recovery_point(sps,current_sps,spl);
    }

    return reserved_sei_message(sps,current_sps,spl);
}

// Parse pic timing SEI data
Ipp32s H265HeadersBitstream::pic_timing(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad * spl)
{
    const H265SeqParamSet *csps = sps.GetHeader(current_sps);

    if (!csps)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (csps->frame_field_info_present_flag)
    {
        spl->SEI_messages.pic_timing.pic_struct = (DisplayPictureStruct_H265)GetBits(4);
        (Ipp8u)GetBits(2); // source_scan_type
        (Ipp8u)GetBits(1); // duplicate_flag
    }

    if (csps->m_hrdParameters.nal_hrd_parameters_present_flag || csps->m_hrdParameters.vcl_hrd_parameters_present_flag)
    {
        GetBits(csps->m_hrdParameters.au_cpb_removal_delay_length); // au_cpb_removal_delay_minus1
        spl->SEI_messages.pic_timing.pic_dpb_output_delay = GetBits(csps->m_hrdParameters.dpb_output_delay_length);

        if (csps->m_hrdParameters.sub_pic_hrd_params_present_flag)
        {
            GetBits(csps->m_hrdParameters.dpb_output_delay_du_length); //pic_dpb_output_du_delay
        }

        if (csps->m_hrdParameters.sub_pic_hrd_params_present_flag && csps->m_hrdParameters.sub_pic_cpb_params_in_pic_timing_sei_flag)
        {
            Ipp32u num_decoding_units_minus1 = GetVLCElementU();

            if (num_decoding_units_minus1 > csps->WidthInCU * csps->HeightInCU)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

            Ipp8u du_common_cpb_removal_delay_flag = (Ipp8u)GetBits(1);
            if (du_common_cpb_removal_delay_flag)
            {
                GetBits(csps->m_hrdParameters.du_cpb_removal_delay_increment_length); //du_common_cpb_removal_delay_increment_minus1
            }

            for (Ipp32u i = 0; i <= num_decoding_units_minus1; i++)
            {
                GetVLCElementU(); // num_nalus_in_du_minus1
                if (!du_common_cpb_removal_delay_flag && i < num_decoding_units_minus1)
                {
                    GetBits(csps->m_hrdParameters.du_cpb_removal_delay_increment_length); // du_cpb_removal_delay_increment_minus1
                }
            }
        }
    }

    AlignPointerRight();

    return current_sps;
}

// Parse recovery point SEI data
Ipp32s H265HeadersBitstream::recovery_point(const HeaderSet<H265SeqParamSet> & , Ipp32s current_sps, H265SEIPayLoad *spl)
{
    H265SEIPayLoad::SEIMessages::RecoveryPoint * recPoint = &(spl->SEI_messages.recovery_point);

    recPoint->recovery_poc_cnt = GetVLCElementS();

    recPoint->exact_match_flag = (Ipp8u)Get1Bit();
    recPoint->broken_link_flag = (Ipp8u)Get1Bit();

    return current_sps;
}

// Skip unrecognized SEI message payload
Ipp32s H265HeadersBitstream::reserved_sei_message(const HeaderSet<H265SeqParamSet> & , Ipp32s current_sps, H265SEIPayLoad *spl)
{
    for(Ipp32u i = 0; i < spl->payLoadSize; i++)
        SkipNBits(m_pbs, m_bitOffset, 8)
    AlignPointerRight();
    return current_sps;
}

}//namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
