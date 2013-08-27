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

#include "umc_h265_bitstream.h"
#include "umc_h265_bitstream_inlines.h"
#include "umc_h265_headers.h"

namespace UMC_HEVC_DECODER
{

Ipp32s H265Bitstream::ParseSEI(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return sei_message(sps,current_sps,spl);
}

Ipp32s H265Bitstream::sei_message(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    Ipp32u code;
    Ipp32s payloadType = 0;

    ippiNextBits(m_pbs, m_bitOffset, 8, code);
    while (code  ==  0xFF)
    {
        /* fixed-pattern bit string using 8 bits written equal to 0xFF */
        ippiGetNBits(m_pbs, m_bitOffset, 8, code);
        payloadType += 255;
        ippiNextBits(m_pbs, m_bitOffset, 8, code);
    }

    Ipp32s last_payload_type_byte;    //Ipp32u integer using 8 bits
    ippiGetNBits(m_pbs, m_bitOffset, 8, last_payload_type_byte);

    payloadType += last_payload_type_byte;

    Ipp32s payloadSize = 0;

    ippiNextBits(m_pbs, m_bitOffset, 8, code);
    while( code  ==  0xFF )
    {
        /* fixed-pattern bit string using 8 bits written equal to 0xFF */
        ippiGetNBits(m_pbs, m_bitOffset, 8, code);
        payloadSize += 255;
        ippiNextBits(m_pbs, m_bitOffset, 8, code);
    }

    Ipp32s last_payload_size_byte;    //Ipp32u integer using 8 bits

    ippiGetNBits(m_pbs, m_bitOffset, 8, last_payload_size_byte);
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
        ippiSkipNBits(pbs, bitOffset, 8);
    }

    SetState(pbs, bitOffset);

    return ret;
}

Ipp32s H265Bitstream::sei_payload(const HeaderSet<H265SeqParamSet> & sps,Ipp32s current_sps,H265SEIPayLoad *spl)
{
    Ipp32u payloadType =spl->payLoadType;
    switch( payloadType)
    {
    case SEI_BUFFERING_PERIOD_TYPE:
        return buffering_period(sps,current_sps,spl);
    case SEI_PIC_TIMING_TYPE:
        return pic_timing(sps,current_sps,spl);
    case SEI_USER_DATA_REGISTERED_TYPE:
        return user_data_registered_itu_t_t35(sps,current_sps,spl);
    case SEI_USER_DATA_UNREGISTERED_TYPE:
        return user_data_unregistered(sps,current_sps,spl);
    case SEI_RECOVERY_POINT_TYPE:
        return recovery_point(sps,current_sps,spl);
    }

    return reserved_sei_message(sps,current_sps,spl);
}

Ipp32s H265Bitstream::buffering_period(const HeaderSet<H265SeqParamSet> & , Ipp32s , H265SEIPayLoad *)
{
    Ipp32s seq_parameter_set_id = (Ipp8u) GetVLCElement(false);
/*
    const H265SeqParamSet *csps = sps.GetHeader(seq_parameter_set_id);
    H265SEIPayLoad::SEIMessages::BufferingPeriod &bps = spl->SEI_messages.buffering_period;

    // touch unreferenced parameters
    if (!csps)
    {
        return -1;
    }

    if (csps->nal_hrd_parameters_present_flag)
    {
        for(Ipp32s i=0;i<csps->cpb_cnt;i++)
        {
            bps.initial_cbp_removal_delay[0][i] = GetBits(csps->cpb_removal_delay_length);
            bps.initial_cbp_removal_delay_offset[0][i] = GetBits(csps->cpb_removal_delay_length);
        }

    }
    if (csps->vcl_hrd_parameters_present_flag)
    {
        for(Ipp32s i=0;i<csps->cpb_cnt;i++)
        {
            bps.initial_cbp_removal_delay[1][i] = GetBits(csps->cpb_removal_delay_length);
            bps.initial_cbp_removal_delay_offset[1][i] = GetBits(csps->cpb_removal_delay_length);
        }

    }

    AlignPointerRight();
*/
    return seq_parameter_set_id;
}

Ipp32s H265Bitstream::pic_timing(const HeaderSet<H265SeqParamSet> & , Ipp32s current_sps, H265SEIPayLoad *)
{
/*
    const Ipp8u NumClockTS[]={1,1,1,2,2,3,3,2,3};
    const H265SeqParamSet *csps = sps.GetHeader(current_sps);
    H265SEIPayLoad::SEIMessages::PicTiming &pts = spl->SEI_messages.pic_timing;

    if (!csps)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (csps->nal_hrd_parameters_present_flag || csps->vcl_hrd_parameters_present_flag)
    {
        pts.cbp_removal_delay = GetBits(csps->cpb_removal_delay_length);
        pts.dpb_output_delay = GetBits(csps->dpb_output_delay_length);
    }
    else
    {
        pts.cbp_removal_delay = (Ipp32u)INVALID_DPB_DELAY_H265;
        pts.dpb_output_delay = (Ipp32u)INVALID_DPB_DELAY_H265;
    }

    if (csps->pic_struct_present_flag)
    {
        Ipp8u picStruct = (Ipp8u)GetBits(4);

        if (picStruct > 8)
            return UMC::UMC_ERR_INVALID_STREAM;

        pts.pic_struct = (DisplayPictureStruct_H265)picStruct;

        for (Ipp32s i = 0; i < NumClockTS[pts.pic_struct]; i++)
        {
            pts.clock_timestamp_flag[i] = (Ipp8u)Get1Bit();
            if (pts.clock_timestamp_flag[i])
            {
                pts.clock_timestamps[i].ct_type = (Ipp8u)GetBits(2);
                pts.clock_timestamps[i].nunit_field_based_flag = (Ipp8u)Get1Bit();
                pts.clock_timestamps[i].counting_type = (Ipp8u)GetBits(5);
                pts.clock_timestamps[i].full_timestamp_flag = (Ipp8u)Get1Bit();
                pts.clock_timestamps[i].discontinuity_flag = (Ipp8u)Get1Bit();
                pts.clock_timestamps[i].cnt_dropped_flag = (Ipp8u)Get1Bit();
                pts.clock_timestamps[i].n_frames = (Ipp8u)GetBits(8);

                if (pts.clock_timestamps[i].full_timestamp_flag)
                {
                    pts.clock_timestamps[i].seconds_value = (Ipp8u)GetBits(6);
                    pts.clock_timestamps[i].minutes_value = (Ipp8u)GetBits(6);
                    pts.clock_timestamps[i].hours_value = (Ipp8u)GetBits(5);
                }
                else
                {
                    if (Get1Bit())
                    {
                        pts.clock_timestamps[i].seconds_value = (Ipp8u)GetBits(6);
                        if (Get1Bit())
                        {
                            pts.clock_timestamps[i].minutes_value = (Ipp8u)GetBits(6);
                            if (Get1Bit())
                            {
                                pts.clock_timestamps[i].hours_value = (Ipp8u)GetBits(5);
                            }
                        }
                    }
                }

                if(csps->time_offset_length > 0)
                    pts.clock_timestamps[i].time_offset = (Ipp8u)GetBits(csps->time_offset_length);
            }
        }
    }

    AlignPointerRight();
*/
    return current_sps;
    //return unparsed_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::user_data_registered_itu_t_t35(const HeaderSet<H265SeqParamSet> & , Ipp32s current_sps, H265SEIPayLoad *spl)
{
    H265SEIPayLoad::SEIMessages::UserDataRegistered * user_data = &(spl->SEI_messages.user_data_registered);
    Ipp32u code;
    ippiGetBits8(m_pbs, m_bitOffset, code);
    user_data->itu_t_t35_country_code = (Ipp8u)code;

    Ipp32u i = 1;

    user_data->itu_t_t35_country_code_extension_byte = 0;
    if (user_data->itu_t_t35_country_code == 0xff)
    {
        ippiGetBits8(m_pbs, m_bitOffset, code);
        user_data->itu_t_t35_country_code_extension_byte = (Ipp8u)code;
        i++;
    }

    spl->user_data.resize(spl->payLoadSize + 1);

    for(Ipp32s k = 0; i < spl->payLoadSize; i++, k++)
    {
        ippiGetBits8(m_pbs, m_bitOffset, code);
        spl->user_data[k] = (Ipp8u) code;
    }

    return current_sps;
}

Ipp32s H265Bitstream::user_data_unregistered(const HeaderSet<H265SeqParamSet> & sps, Ipp32s current_sps, H265SEIPayLoad *spl)
{
    return reserved_sei_message(sps, current_sps, spl);
}

Ipp32s H265Bitstream::recovery_point(const HeaderSet<H265SeqParamSet> & , Ipp32s current_sps, H265SEIPayLoad *spl)
{
    H265SEIPayLoad::SEIMessages::RecoveryPoint * recPoint = &(spl->SEI_messages.recovery_point);

    recPoint->recovery_frame_cnt = (Ipp8u)GetVLCElement(false);

    recPoint->exact_match_flag = (Ipp8u)Get1Bit();
    recPoint->broken_link_flag = (Ipp8u)Get1Bit();
    recPoint->changing_slice_group_idc = (Ipp8u)GetBits(2);

    if (recPoint->changing_slice_group_idc > 2)
        return -1;

    return current_sps;
}

Ipp32s H265Bitstream::reserved_sei_message(const HeaderSet<H265SeqParamSet> & , Ipp32s current_sps, H265SEIPayLoad *spl)
{
    for(Ipp32u i = 0; i < spl->payLoadSize; i++)
        ippiSkipNBits(m_pbs, m_bitOffset, 8)
    AlignPointerRight();
    return current_sps;
}

}//namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
