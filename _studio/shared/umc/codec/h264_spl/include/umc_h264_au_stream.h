//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_H264_AU_STREAM_H__
#define __UMC_H264_AU_STREAM_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_SPLITTER)

#include "umc_structures.h"
#include "umc_media_data.h"
#include "umc_h264_parse_def.h"
#include "umc_h264_syntax_reader.h"

#include <vector>
#include <map>
#include <memory>

namespace UMC
{

/**
 * Takes NALU units in the AnnexB format
 */
class H264_AU_Stream
{
    typedef std::map < Ipp32u, H264SequenceSetParse> SEQ_SET_MAP;
    typedef std::map < Ipp32u, H264PictureSetParse> PIC_SET_MAP;

    /**
    * Stores the information about AU
    */
    struct AUStore
    {
        Ipp32u width;
        Ipp32u height;
        Ipp32u isIDR;
        Ipp32u pic_parameter_set_id;
        VEC_BUF buffer;

        AUStore()
        {
            isIDR = 0;
            width = 0;
            height = 0;
        }
    };

public:
    H264_AU_Stream(void);
    ~H264_AU_Stream(void);

    /**
     * Initializes the stream
     * @param p_iSize - the maximum frame size
     */
    void Init(Ipp32s p_iSize = 65536);

    /**
     * Parses the inter NALU dependecies
     * @param p_rawAnnexB - the NALU with the prevention Ox03 bytes
     * @remark - be sure to put only complete NALU here, - be sure not use pointer obtained by prior to PutData
     *           calls to LockOutputData and GetLastSets
     */
    Status PutData(MediaData & p_rawAnnexB);

    /**
     * Acquires the internally stored AU
     */
    Status LockOutputData(MediaData & o_dataAU);

    /**
     * Retrieve the last sets for the last AU
     * @return UMC_OK on success, UMC_ERR_FAILED on no such sets
     */
    Status GetActiveSets(MediaData & p_SequenceSet, MediaData & p_PictureSet);

    /**
     * Kept for consistency purposes
     */
    Status UnLockOutputData(MediaData & o_dataAU);

    /**
     * Swaps the internal buffers
     */
    void Swap();

private:

    /**
     * Parses the sequence header of the NALU
     * this is needed to get some header values in order to perform the AU detection
     */
    Status Read_seq_parameter_set_rbsp(H264SequenceSetParse & p_SequenceSet, H264_SyntaxReader & p_reader);

    /**
     * Reads the scaling list part
     */
    Status Read_scaling_list(H264_SyntaxReader & p_reader, Ipp32u p_iSizeOfScalingList);

    /**
    * Parses the picture header of the NALU
    * this is needed to get some header values in order to perform the AU detection
     */
    Status Read_pic_parameter_set_rbsp(H264PictureSetParse & p_PictureSet, H264_SyntaxReader & p_reader);

    /**
    * Parses the slice header of the NALU
    * this is needed to get some header values in order to perform the AU detection
    */
    Status Read_slice_header(H264SliceHeaderParse & p_SliceHeader, H264_SyntaxReader & p_reader, H264SequenceSetParse * & o_pSeqSet);

    /**
     * New slice
     */
    Status OnSlice(MediaData & p_dataNALU);

    /**
     * New picture set
     */
    Status OnPictureSet(MediaData & p_dataNALU);
    /**
     * New sequence set
     */
    Status OnSequenceSet(MediaData & p_dataNALU);

    /**
     * Appends the NALU to current AU
     */
    void AppendAU(MediaData & p_dataNALU);

    /**
     * Starts the new AU
     */
    Status StartAU();

    /**
     * Detects with the slices correspond to the same picture
     * @param p_newHeader - the newly arrived header
     */
    bool IsPictureSame(H264SliceHeaderParse & p_newHeader);

    /**
     * Searches for appropriate set if
     * If it were present earlier - returns the pointer to it; if no returns 0
     */
    H264PictureSetParse * FindPicSet(Ipp32u p_PicSetID);

    /**
    * Searches for appropriate set if
    * If it were present earlier - returns the pointer to it; if no returns 0
    */
    H264SequenceSetParse * FindSeqSet(Ipp32u p_SeqSetID);

    /**
     * Find both Seq and Pic params
     */
    Status FindSets(Ipp32u p_iPicSet, H264SequenceSetParse * & p_seq, H264PictureSetParse * & p_pic);

    /**
     * Sets the last size seen
     */
    void SetLastAttrs(H264SequenceSetParse & p_seqSet, H264SliceHeaderParse & p_Slice);

private:

    size_t SkipStartCode(MediaData & p_rawAnnexB);

    // The storage for sequence sets
    SEQ_SET_MAP m_mapSeq;
    // The storage for picture sets
    PIC_SET_MAP m_mapPic;
    // The storage for the AU
    AUStore m_au[2];
    Ipp32s m_iActiveUA;
    // Previous Slice
    H264SliceHeaderParse m_lastSlice;
    // Debugging info - indicates the ammount of data outputted
    Ipp32s m_iOutData;
};
} // namespace UMC

#endif // UMC_ENABLE_H264_SPLITTER
#endif // __UMC_H264_AU_STREAM_H__

