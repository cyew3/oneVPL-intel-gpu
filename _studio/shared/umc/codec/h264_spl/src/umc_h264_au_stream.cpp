//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_SPLITTER)

#include "umc_h264_au_stream.h"

#include <memory>

#define TRAILING_SPAN 8

namespace UMC
{

    H264_AU_Stream::H264_AU_Stream(void)
    {
        m_iActiveUA = 0;
        m_iOutData = 0;
    }

    H264_AU_Stream::~H264_AU_Stream(void){}

    void H264_AU_Stream::Init(Ipp32s p_iSize)
    {
        m_au[0].buffer.reserve(p_iSize + TRAILING_SPAN);
        m_au[1].buffer.reserve(p_iSize + TRAILING_SPAN);

        m_au[0].buffer.clear();
        m_au[1].buffer.clear();
    }

    Status H264_AU_Stream::PutData(MediaData & p_rawAnnexB)
    {
        if(p_rawAnnexB.GetDataSize() < 1){
            return UMC_ERR_NOT_ENOUGH_DATA;
        }

        Status status = UMC_ERR_NOT_ENOUGH_DATA;

        size_t offset = SkipStartCode(p_rawAnnexB);
        H264NALUOctet octet;
        octet.ReadOctet((Ipp8u *)p_rawAnnexB.GetDataPointer() + offset);

#ifdef AU_TRACING
        printf("--nalu type=%d", (Ipp32s)octet.Type);
#endif // AU_TRACING

        switch(octet.Type){
            case H264_NAL_SLICE_NON_IDR:
            case H264_NAL_SLICE_A:
            case H264_NAL_SLICE_B:
            case H264_NAL_SLICE_C:
            case H264_NAL_SLICE_IDR:
                status = OnSlice(p_rawAnnexB);
                if (m_au[m_iActiveUA].isIDR == 0 && octet.Type == H264_NAL_SLICE_IDR)
                    m_au[m_iActiveUA].isIDR = 1;
                break;
            case H264_NAL_SET_SEQ:
                status = OnSequenceSet(p_rawAnnexB);
                break;
            case H264_NAL_SET_PIC:
                status = OnPictureSet(p_rawAnnexB);
                break;
            case H264_NAL_AU_DEL:
                m_lastSlice.is_valid = false;
                status = StartAU();
                AppendAU(p_rawAnnexB);
                break;
            case H264_NAL_END_SEQ:
                m_lastSlice.is_valid = false;
                AppendAU(p_rawAnnexB);
                status = StartAU();
                break;
            case H264_NAL_END_STM:
                m_lastSlice.is_valid = false;
                AppendAU(p_rawAnnexB);
                status = StartAU();
                break;

            case H264_NAL_PREFIX:
            case H264_NAL_SLICE_EX:
            case H264_NAL_SUBSET_SPS:
                AppendAU(p_rawAnnexB);
                status = UMC_ERR_NOT_ENOUGH_DATA;
                break;

            default:
                // H264_NAL_FIL_DATA
                // H264_NAL_SLICE_SEI
                // H264_NAL_SET_SEQ_EXT
                // H264_NAL_SLICE_NI_05
                AppendAU(p_rawAnnexB);
                status = UMC_ERR_NOT_ENOUGH_DATA;
                break;
        }

#ifdef AU_TRACING
        printf("\n");
#endif // AU_TRACING
        return status;
    }

    Status H264_AU_Stream::OnSlice(MediaData & p_dataNALU)
    {
        H264SliceHeaderParse slice;

        MediaData localData = p_dataNALU;

        size_t offset = SkipStartCode(localData);
        localData.MoveDataPointer((Ipp32s)offset);
        slice.octet.ReadOctet((Ipp8u *)localData.GetDataPointer());
        localData.MoveDataPointer(1);

        H264_SyntaxReader syntaxReader(localData);

        H264SequenceSetParse * pSeqSet = 0;

        if( UMC_OK != Read_slice_header(slice, syntaxReader, pSeqSet)){
            return UMC_ERR_FAILED;
        }

        if (m_lastSlice.is_valid && !IsPictureSame(slice)){
            Status status = StartAU();
            AppendAU(p_dataNALU);
            SetLastAttrs(*pSeqSet, slice);
            m_lastSlice = slice;
            m_lastSlice.is_valid = true;
            return status;
        }

        SetLastAttrs(*pSeqSet, slice);
        m_lastSlice = slice;
        m_lastSlice.is_valid = true;
        AppendAU(p_dataNALU);
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    Status H264_AU_Stream::OnPictureSet(MediaData & p_dataNALU)
    {
        if( p_dataNALU.GetDataSize() == 0){
            return UMC_ERR_NOT_ENOUGH_DATA;
        }

        H264PictureSetParse pic;

        MediaData localData = p_dataNALU;

        size_t offset = SkipStartCode(localData);
        localData.MoveDataPointer((Ipp32s)offset);
        pic.octet.ReadOctet((Ipp8u *)localData.GetDataPointer());
        localData.MoveDataPointer(1);

        Ipp8u * pData = (Ipp8u *)localData.GetDataPointer();
        pic.buffer.insert(pic.buffer.end(), pData, pData + localData.GetDataSize());

        H264_SyntaxReader syntaxReader(localData);

        if( UMC_OK != Read_pic_parameter_set_rbsp(pic, syntaxReader)){
            return UMC_ERR_FAILED;
        }

        m_mapPic[pic.pic_parameter_set_id] = pic;

        Status status = UMC_ERR_NOT_ENOUGH_DATA;
        if (m_lastSlice.is_valid)
        {
            status = StartAU();
        }
        AppendAU(p_dataNALU);
        m_lastSlice.is_valid = false;
        return status;
    }

    Status H264_AU_Stream::OnSequenceSet(MediaData & p_dataNALU)
    {
        if( p_dataNALU.GetDataSize() == 0){
            return UMC_ERR_NOT_ENOUGH_DATA;
        }

        H264SequenceSetParse seq;

        MediaData localData = p_dataNALU;

        H264_SyntaxReader syntaxReader(localData);

        size_t offset = SkipStartCode(localData);
        localData.MoveDataPointer((Ipp32s)offset);
        seq.octet.ReadOctet((Ipp8u *)localData.GetDataPointer());
        localData.MoveDataPointer(1);

        Ipp8u * pData = (Ipp8u *)localData.GetDataPointer();
        seq.buffer.insert(seq.buffer.end(), pData, pData + localData.GetDataSize());

        if( UMC_OK != Read_seq_parameter_set_rbsp(seq, syntaxReader)){
            return UMC_ERR_FAILED;
        }

        m_mapSeq[seq.seq_parameter_set_id] = seq;

        Status status = UMC_ERR_NOT_ENOUGH_DATA;
        if (m_lastSlice.is_valid)
        {
            status = StartAU();
        }

        AppendAU(p_dataNALU);
        m_lastSlice.is_valid = false;
        return status;
    }

    void H264_AU_Stream::AppendAU(MediaData & p_dataNALU)
    {
        Ipp8u * pData = (Ipp8u*)p_dataNALU.GetDataPointer();
        m_au[m_iActiveUA].buffer.insert(m_au[m_iActiveUA].buffer.end(), pData, pData + p_dataNALU.GetDataSize());
    }

    Status H264_AU_Stream::StartAU()
    {
        if( m_au[m_iActiveUA].buffer.empty()){
            return UMC_ERR_NOT_ENOUGH_DATA;
        }
        // Swapping buffers
        m_iActiveUA = ( m_iActiveUA + 1) % 2;
        m_au[m_iActiveUA].buffer.clear();
        m_au[m_iActiveUA].isIDR = 0;
        return UMC_OK;
    }

    void H264_AU_Stream::Swap()
    {
        m_iActiveUA = ( m_iActiveUA + 1) % 2;
    }

    Status H264_AU_Stream::LockOutputData(MediaData & o_dataAU)
    {
        Ipp32s iNum = ( m_iActiveUA + 1) % 2;
        // Ensuring that there is also a span in the end of NALU

        Ipp32s iTrail = (Ipp32s)(m_au[iNum].buffer.capacity() - m_au[iNum].buffer.size());
        if( iTrail < TRAILING_SPAN ){
            m_au[iNum].buffer.reserve(m_au[iNum].buffer.size() + TRAILING_SPAN);
        }

        if (m_au[iNum].buffer.size() == 0)
        {
            o_dataAU.SetBufferPointer(0, 0);
            o_dataAU.SetDataSize(0);
            return UMC_OK;
        }

        o_dataAU.SetBufferPointer(&(m_au[iNum].buffer[0]), m_au[iNum].buffer.size());
        o_dataAU.SetDataSize(m_au[iNum].buffer.size());

        m_iOutData += (Ipp32s)m_au[iNum].buffer.size();

        if (m_au[iNum].isIDR)
        {
            o_dataAU.SetFrameType(I_PICTURE);
        }
        else
        {
            o_dataAU.SetFrameType(VIDEO_FRAME);
        }
        return UMC_OK;
    }

    Status H264_AU_Stream::UnLockOutputData(MediaData & /*o_dataAU*/)
    {
        return UMC_OK;
    }

#define GO_OR_QUIT_SIMPLE( op ) if( UMC_OK != op ) return UMC_ERR_FAILED;
#define GO_OR_QUIT( op ) if( UMC_OK != p_reader.op ) return UMC_ERR_FAILED;

    Status H264_AU_Stream::Read_seq_parameter_set_rbsp(H264SequenceSetParse & p_SequenceSet, H264_SyntaxReader & p_reader)
    {
        Ipp32u iValue = 0;
        Ipp32s siValue = 0;
        Ipp32u profile_idc = 0;
        GO_OR_QUIT( ReadU(8, profile_idc) );
        GO_OR_QUIT( ReadU(1, iValue) );
        GO_OR_QUIT( ReadU(1, iValue) );
        GO_OR_QUIT( ReadU(1, iValue) );
        GO_OR_QUIT( ReadU(1, iValue) );
        GO_OR_QUIT( ReadU(4, iValue) );
        GO_OR_QUIT( ReadU(8, iValue) );
        GO_OR_QUIT( ReadUE(p_SequenceSet.seq_parameter_set_id) );

        if( profile_idc == 100 || profile_idc == 110
            || profile_idc == 122 || profile_idc == 144){
            Ipp32u chroma_format_idc = 0;
            GO_OR_QUIT( ReadUE( chroma_format_idc ));
            if( chroma_format_idc == 3 ){
                GO_OR_QUIT( ReadU(1, iValue) );
            }
            GO_OR_QUIT( ReadUE(iValue) );
            GO_OR_QUIT( ReadUE(iValue) );
            GO_OR_QUIT( ReadU(1, iValue) );

            Ipp32u seq_scaling_matrix_present_flag = 0;
            GO_OR_QUIT( ReadU(1, seq_scaling_matrix_present_flag) );
            if(seq_scaling_matrix_present_flag){
                for (Ipp32s i=0; i<8; i++) {
                    Ipp32u seq_scaling_matrix_present_flag_i = 0;
                    GO_OR_QUIT( ReadU(1, seq_scaling_matrix_present_flag_i) );
                    if(seq_scaling_matrix_present_flag_i){
                        if( i < 6){
                            GO_OR_QUIT_SIMPLE ( Read_scaling_list(p_reader , 16 ) );
                        } else {
                            GO_OR_QUIT_SIMPLE ( Read_scaling_list(p_reader , 64 ) );
                        }
                    }
                }
            }
        }

        GO_OR_QUIT( ReadUE(p_SequenceSet.log2_max_frame_num_minus4) );
        GO_OR_QUIT( ReadUE(p_SequenceSet.pic_order_cnt_type) );
        if( p_SequenceSet.pic_order_cnt_type == 0 ){
            GO_OR_QUIT( ReadUE(p_SequenceSet.log2_max_pic_order_cnt_lsb_minus4) );
        } else if( p_SequenceSet.pic_order_cnt_type == 1 ) {
            GO_OR_QUIT( ReadU( 1, p_SequenceSet.delta_pic_order_always_zero_flag) );
            GO_OR_QUIT( ReadSE(siValue) );
            GO_OR_QUIT( ReadSE(siValue) );
            Ipp32u num_ref_frames_in_pic_order_cnt_cycle = 0;
            GO_OR_QUIT( ReadUE(num_ref_frames_in_pic_order_cnt_cycle) );
            for (Ipp32u i=0; i<num_ref_frames_in_pic_order_cnt_cycle; i++)
            {
                GO_OR_QUIT( ReadSE(siValue) );
            }
        }
        GO_OR_QUIT( ReadUE(iValue) );
        GO_OR_QUIT( ReadU(1, iValue) );
        GO_OR_QUIT( ReadUE(p_SequenceSet.pic_width_in_mbs_minus1) );
        GO_OR_QUIT( ReadUE(p_SequenceSet.pic_height_in_map_units_minus1) );

        GO_OR_QUIT( ReadU(1, p_SequenceSet.frame_mbs_only_flag) );
        if( !p_SequenceSet.frame_mbs_only_flag){
            GO_OR_QUIT( ReadU(1, iValue) );
        }
        GO_OR_QUIT( ReadU(1, iValue) );
        GO_OR_QUIT( ReadU(1, p_SequenceSet.frame_cropping_flag) );
        if(p_SequenceSet.frame_cropping_flag){
            GO_OR_QUIT( ReadUE(p_SequenceSet.frame_crop_left_offset) );
            GO_OR_QUIT( ReadUE(p_SequenceSet.frame_crop_right_offset) );
            GO_OR_QUIT( ReadUE(p_SequenceSet.frame_crop_top_offset) );
            GO_OR_QUIT( ReadUE(p_SequenceSet.frame_crop_bottom_offset) );
        }

        /// No need to parse further -- Skipping

        return UMC_OK;
    }

    Status H264_AU_Stream::Read_scaling_list(H264_SyntaxReader & p_reader, Ipp32u p_iSizeOfScalingList)
    {
        Ipp32s lastScale = 8,
               nextScale = 8;
        for(Ipp32u j=0; j<p_iSizeOfScalingList; j++){
            if( nextScale != 0 ){
                Ipp32s delta_scale = 0;
                GO_OR_QUIT( ReadSE(delta_scale) );
                nextScale = (lastScale + delta_scale + 256 ) % 256;
            }
            Ipp32s n_lastScale = ( nextScale == 0 ) ? lastScale : nextScale;
            lastScale = n_lastScale;
        }
        return UMC_OK;
    }

    Status H264_AU_Stream::Read_pic_parameter_set_rbsp(H264PictureSetParse & p_PictureSet, H264_SyntaxReader & p_reader)
    {
        Ipp32u iValue = 0;
        GO_OR_QUIT( ReadUE(p_PictureSet.pic_parameter_set_id) );
        GO_OR_QUIT( ReadUE(p_PictureSet.seq_parameter_set_id) );
        GO_OR_QUIT( ReadU(1, iValue) );
        GO_OR_QUIT( ReadU(1, p_PictureSet.pic_order_present_flag) );

        /// No need to parse further -- Skipping

        return UMC_OK;
    }

    Status H264_AU_Stream::Read_slice_header(H264SliceHeaderParse & p_SliceHeader, H264_SyntaxReader & p_reader,
                                             H264SequenceSetParse * & o_pSeqSet)
    {
        GO_OR_QUIT( ReadUE( p_SliceHeader.first_mb_in_slice) );
        GO_OR_QUIT( ReadUE( p_SliceHeader.slice_type) );
        GO_OR_QUIT( ReadUE( p_SliceHeader.pic_parameter_set_id) );

        H264SequenceSetParse * pSeqSet = 0;
        H264PictureSetParse * pPicSet = 0;
        GO_OR_QUIT_SIMPLE ( FindSets ( p_SliceHeader.pic_parameter_set_id, pSeqSet, pPicSet ) );

        Ipp32u iFNLen = (pSeqSet->log2_max_frame_num_minus4 + 4);
        GO_OR_QUIT( ReadU( iFNLen, p_SliceHeader.frame_num) );

#ifdef AU_TRACING
        printf( " frame_num=%d", p_SliceHeader.frame_num);
#endif

        if(pSeqSet->frame_mbs_only_flag == 0){
            GO_OR_QUIT( ReadU( 1, p_SliceHeader.field_pic_flag) );
            if(p_SliceHeader.field_pic_flag){
                GO_OR_QUIT( ReadU( 1, p_SliceHeader.bottom_field_flag) );
            }
        }

        if(p_SliceHeader.octet.Type == 5){
            GO_OR_QUIT( ReadUE( p_SliceHeader.idr_pic_id) );
        }
        if(pSeqSet->pic_order_cnt_type == 0){
            Ipp32u iLen = (pSeqSet->log2_max_pic_order_cnt_lsb_minus4 + 4);
            GO_OR_QUIT( ReadU( iLen, p_SliceHeader.pic_order_cnt_lsb) );
            if(pPicSet->pic_order_present_flag && !p_SliceHeader.field_pic_flag){
                GO_OR_QUIT( ReadSE( p_SliceHeader.delta_pic_order_cnt_bottom ) );
            }
        }
        if(pSeqSet->pic_order_cnt_type == 1 && !pSeqSet->delta_pic_order_always_zero_flag){
            GO_OR_QUIT( ReadSE( p_SliceHeader.delta_pic_order_cnt[0] ) );
            if(pPicSet->pic_order_present_flag && !p_SliceHeader.field_pic_flag){
                GO_OR_QUIT( ReadSE( p_SliceHeader.delta_pic_order_cnt[1] ) );
            }
        }

        /// No need to parse further -- Skipping

        o_pSeqSet = pSeqSet;

        return UMC_OK;
    }

    H264PictureSetParse * H264_AU_Stream::FindPicSet(Ipp32u p_PicSetID)
    {
        PIC_SET_MAP::iterator iter = m_mapPic.find(p_PicSetID);
        if( m_mapPic.end() == iter ){
            return 0;
        }
        return &(iter->second);
    }

    H264SequenceSetParse * H264_AU_Stream::FindSeqSet(Ipp32u p_SeqSetID)
    {
        SEQ_SET_MAP::iterator iter = m_mapSeq.find(p_SeqSetID);
        if( m_mapSeq.end() == iter ){
            return 0;
        }
        return &(iter->second);
    }

    Status H264_AU_Stream::FindSets(Ipp32u p_iPicSet, H264SequenceSetParse * & p_seq, H264PictureSetParse * & p_pic)
    {
        H264PictureSetParse * pPicSet = FindPicSet(p_iPicSet);
        if( 0 == pPicSet){
            return UMC_ERR_FAILED;
        }
        H264SequenceSetParse * pSeqSet = FindSeqSet(pPicSet->seq_parameter_set_id);
        if( 0 == pSeqSet){
            return UMC_ERR_FAILED;
        }

        p_seq = pSeqSet;
        p_pic = pPicSet;
        return UMC_OK;
    }

    Status H264_AU_Stream::GetActiveSets(MediaData & p_SequenceSet, MediaData & p_PictureSet)
    {
        H264SequenceSetParse * pSeqSet = 0;
        H264PictureSetParse * pPicSet = 0;

        Ipp32s iNum = m_iActiveUA;

        GO_OR_QUIT_SIMPLE ( FindSets ( m_au[iNum].pic_parameter_set_id, pSeqSet, pPicSet ) );

        p_SequenceSet.SetBufferPointer(&pSeqSet->buffer[0], pSeqSet->buffer.size());
        p_SequenceSet.SetDataSize(pSeqSet->buffer.size());
        p_PictureSet.SetBufferPointer(&pPicSet->buffer[0], pPicSet->buffer.size());
        p_PictureSet.SetDataSize(pPicSet->buffer.size());

        return UMC_OK;
    }

    bool H264_AU_Stream::IsPictureSame(H264SliceHeaderParse & p_newHeader)
    {
        if ((p_newHeader.frame_num != m_lastSlice.frame_num) ||
            (p_newHeader.pic_parameter_set_id != m_lastSlice.pic_parameter_set_id) ||
            (p_newHeader.field_pic_flag != m_lastSlice.field_pic_flag) ||
            (p_newHeader.bottom_field_flag != m_lastSlice.bottom_field_flag)
            ){
            return false;
        }

        if ((p_newHeader.octet.NRI != m_lastSlice.octet.NRI) &&
            (0 == IPP_MIN(p_newHeader.octet.NRI, m_lastSlice.octet.NRI))){
            return false;
        }

        H264SequenceSetParse * pSeqSetNew = 0;
        H264PictureSetParse * pPicSetNew = 0;
        if( UMC_OK != FindSets(p_newHeader.pic_parameter_set_id, pSeqSetNew, pPicSetNew ) ){
            return false;
        }

        H264SequenceSetParse * pSeqSetLast = 0;
        H264PictureSetParse * pPicSetLast = 0;
        if ( UMC_OK != FindSets(m_lastSlice.pic_parameter_set_id, pSeqSetLast, pPicSetLast ) ){
            return false;
        }


        if (  0 == pSeqSetLast->pic_order_cnt_type  )
        {
            if ((p_newHeader.pic_order_cnt_lsb != m_lastSlice.pic_order_cnt_lsb) ||
                (p_newHeader.delta_pic_order_cnt_bottom != m_lastSlice.delta_pic_order_cnt_bottom)){
                    return false;
            }
        }  else {
            if ((p_newHeader.delta_pic_order_cnt[0] != m_lastSlice.delta_pic_order_cnt[0]) ||
                (p_newHeader.delta_pic_order_cnt[1] != m_lastSlice.delta_pic_order_cnt[1]))
                return false;
        }

        if (p_newHeader.octet.Type != m_lastSlice.octet.Type)
        {
            if ((H264_NAL_SLICE_IDR == p_newHeader.octet.Type) ||
                (H264_NAL_SLICE_IDR == m_lastSlice.octet.Type))
                return false;
        }
        else if (H264_NAL_SLICE_IDR == m_lastSlice.octet.Type)
        {
            if (p_newHeader.idr_pic_id != m_lastSlice.idr_pic_id)
                return false;
        }
        return true;
    }

    void H264_AU_Stream::SetLastAttrs(H264SequenceSetParse & p_seqSet, H264SliceHeaderParse & p_Slice)
    {
        m_au[m_iActiveUA].width =
            ( p_seqSet.pic_width_in_mbs_minus1 + 1) *16
            -(p_seqSet.frame_crop_left_offset+
            p_seqSet.frame_crop_right_offset)*2;
        m_au[m_iActiveUA].height =
            (2 - p_seqSet.frame_mbs_only_flag)*
            (p_seqSet.pic_height_in_map_units_minus1 + 1) *16
            -(p_seqSet.frame_crop_top_offset+
            p_seqSet.frame_crop_bottom_offset)*2*
            (2 - p_seqSet.frame_mbs_only_flag);

        m_au[m_iActiveUA].pic_parameter_set_id = p_Slice.pic_parameter_set_id;
    }

    size_t H264_AU_Stream::SkipStartCode(MediaData & p_rawAnnexB)
    {
        Ipp32s max_search_length = (Ipp32s)p_rawAnnexB.GetDataSize();
        Ipp32u t1,t2,t3;
        Ipp32s p;

        Ipp8u * data = (Ipp8u *)p_rawAnnexB.GetDataPointer();

        t1 = data[0];
        t2 = data[1];
        t3 = data[2];

        data += 3;

        for (p = 0; p < max_search_length - 2; p++)
        {
            if (t1==0 && t2==0 && t3==1)
            {
                return (data - (Ipp8u*)p_rawAnnexB.GetDataPointer());
            }

            t1=t2;
            t2=t3;
            t3 = data[0];
            data++;
        }

        return 0;
    }
}

#endif // UMC_ENABLE_H264_SPLITTER
