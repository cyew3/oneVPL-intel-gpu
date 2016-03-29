////////////////////////////////////////////////////////////////////////////////
//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that aFgreement.
//        Copyright (c) 2004 - 2016 Intel Corporation. All Rights Reserved.
//

#if defined (__ICL)
//transfer of control bypasses initialization of.
// goto operator has possible issue with crossplatform
#pragma warning(disable:589)
#endif

#if PIXBITS == 8

#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

#elif PIXBITS == 16

#define PIXTYPE Ipp16u
#define COEFFSTYPE Ipp32s
#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s

#else

void H264EncoderFakeFunction() { UNSUPPORTED_PIXBITS; }

#endif //PIXBITS

#define H264BsFakeType H264ENC_MAKE_NAME(H264BsFake)
#define H264CoreEncoderType H264ENC_MAKE_NAME(H264CoreEncoder)
#define H264SliceType H264ENC_MAKE_NAME(H264Slice)
#define H264CurrentMacroblockDescriptorType H264ENC_MAKE_NAME(H264CurrentMacroblockDescriptor)
#define ME_InfType H264ENC_MAKE_NAME(ME_Inf)
#define H264EncoderFrameType H264ENC_MAKE_NAME(H264EncoderFrame)
#define EncoderRefPicListType H264ENC_MAKE_NAME(EncoderRefPicList)
#define EncoderRefPicListStructType H264ENC_MAKE_NAME(EncoderRefPicListStruct)

static H264EncoderFrameType * H264ENC_MAKE_NAME(FindDirectRefIdx)(
    H264SliceType *curr_slice,
    Ipp32u mb_col,
    Ipp32u ipos,                        // offset into MV and RefIndex storage
    H264EncoderFrameType **pRefPicList0,
    H264EncoderFrameType **pRefPicList1,
    H264MotionVector  **MVL0, // return colocated MV here
    Ipp8s &RefIndexL0,  // return ref index here
    Ipp32s numRefL0Active)
{
    H264ENC_UNREFERENCED_PARAMETER(numRefL0Active);
    VM_ASSERT(pRefPicList1[0]);

    // Set pointers to colocated list 0 ref index and MV
    //colPic = pRefPicList1[0]
    T_RefIdx * pRefRefIndexL0 = &pRefPicList1[0]->m_mbinfo.RefIdxs[LIST_0][mb_col].RefIdxs[ipos];
    *MVL0 = &pRefPicList1[0]->m_mbinfo.MV[LIST_0][mb_col].MotionVectors[ipos];

    VM_ASSERT(pRefRefIndexL0);
    VM_ASSERT(*MVL0);

    // Get ref index and MV from L0 of colocated ref MB if the
    // colocated L0 ref index is >=0. Else use L1.
    RefIndexL0 = *pRefRefIndexL0;
    if(RefIndexL0 >= 0)
        RefIndexL0 = curr_slice->MapColMBToList0[RefIndexL0][LIST_0];

    if( RefIndexL0 < 0 ){
        T_RefIdx * pRefRefIndexL1 = &pRefPicList1[0]->m_mbinfo.RefIdxs[LIST_1][mb_col].RefIdxs[ipos];
        *MVL0 = &pRefPicList1[0]->m_mbinfo.MV[LIST_1][mb_col].MotionVectors[ipos];
        VM_ASSERT(pRefRefIndexL1);
        VM_ASSERT(*MVL0);
        RefIndexL0 = *pRefRefIndexL1;
        if( RefIndexL0 >= 0 ) RefIndexL0 = curr_slice->MapColMBToList0[RefIndexL0][LIST_1];
    }
#if 0
    EncoderRefPicListStructType *ref_pic_list_struct;
    if(RefIndexL0 >= 0)
    {
        MVL0 = *pRefMVL0; // Use colocated L0
        // Get pointer to ref pic list 0 of colocated
        ref_pic_list_struct = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
            pRefPicList1[0],
            (pRefPicList1[0]->m_mbinfo.mbs + mb_col)->slice_id,
            LIST_0);
        VM_ASSERT(ref_pic_list_struct->m_RefPicList[RefIndexL0]);
        RefIndexL0 = MapColToList0( ref_pic_list_struct, pRefPicList0, RefIndexL0, numRefL0Active );
    }

    if(RefIndexL0 < 0){ // Use Ref L1
        // Set pointers to colocated list 1 ref index and MV
        T_RefIdx * pRefRefIndexL1 = &pRefPicList1[0]->m_mbinfo.RefIdxs[LIST_1][mb_col].RefIdxs[ipos];
        H264MotionVector  * pRefMVL1 = &pRefPicList1[0]->m_mbinfo.MV[LIST_1][mb_col].MotionVectors[ipos];
        VM_ASSERT(pRefRefIndexL1);
        VM_ASSERT(pRefMVL1);
        RefIndexL0 = *pRefRefIndexL1;

        if( RefIndexL0 >= 0 ){
            MVL0 = *pRefMVL1; // Use colocated L1
            // Get pointer to ref pic list 1 of colocated
            ref_pic_list_struct = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
                pRefPicList1[0],
                (pRefPicList1[0]->m_mbinfo.mbs + mb_col)->slice_id,
                LIST_1);
            VM_ASSERT(ref_pic_list_struct->m_RefPicList[RefIndexL0]);
            RefIndexL0 = MapColToList0( ref_pic_list_struct, pRefPicList0, RefIndexL0, numRefL0Active );
        }
    }
#endif //0

    if (pRefPicList1[0]->m_PictureStructureForDec == AFRM_STRUCTURE){
        VM_ASSERT(0);//can't happen
    }

    return (RefIndexL0 != -1) ? pRefPicList0[RefIndexL0] : 0;
}

static H264EncoderFrameType* H264ENC_MAKE_NAME(FindDirectRefIdxFLD)(
    Ipp32s mb_col,
    Ipp32u ipos,                        // offset into MV and RefIndex storage
    H264EncoderFrameType **pRefPicList0,
    H264EncoderFrameType **pRefPicList1,
    Ipp8s         *pFields0,
    Ipp8s         *, // pFields1,
    H264MotionVector  &MVL0, // return colocated MV here
    Ipp8s &RefIndexL0,        // return ref index here
    Ipp32s numRefL0Active)
{
    VM_ASSERT(pRefPicList1[0]);

    // Set pointers to colocated list 0 ref index and MV
    T_RefIdx * pRefRefIndexL0 = &pRefPicList1[0]->m_mbinfo.RefIdxs[LIST_0][mb_col].RefIdxs[ipos];
    H264MotionVector  *pRefMVL0 = &pRefPicList1[0]->m_mbinfo.MV[LIST_0][mb_col].MotionVectors[ipos];

    VM_ASSERT(pRefRefIndexL0);
    VM_ASSERT(pRefMVL0);

    // Get ref index and MV from L0 of colocated ref MB if the
    // colocated L0 ref index is >=0. Else use L1.
    RefIndexL0 = *pRefRefIndexL0;

    EncoderRefPicListStructType *ref_pic_list_struct;
    if (RefIndexL0 >= 0)
    {
        MVL0 = *pRefMVL0; // Use colocated L0
        // Get pointer to ref pic list 0 of colocated
        ref_pic_list_struct = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
            pRefPicList1[0],
            (pRefPicList1[0]->m_mbinfo.mbs + mb_col)->slice_id,
            LIST_0);
    } else { // Use Ref L1
        // Set pointers to colocated list 1 ref index and MV
        T_RefIdx * pRefRefIndexL1 = &pRefPicList1[0]->m_mbinfo.RefIdxs[LIST_1][mb_col].RefIdxs[ipos];
        H264MotionVector  * pRefMVL1 = &pRefPicList1[0]->m_mbinfo.MV[LIST_1][mb_col].MotionVectors[ipos];
        VM_ASSERT(pRefRefIndexL1);
        VM_ASSERT(pRefMVL1);
        RefIndexL0 = *pRefRefIndexL1;

        MVL0 = *pRefMVL1; // Use colocated L1

        // Get pointer to ref pic list 1 of colocated
        ref_pic_list_struct = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
            pRefPicList1[0],
            (pRefPicList1[0]->m_mbinfo.mbs + mb_col)->slice_id,
            LIST_1);
    }
    // Translate the reference index of the colocated to current
    // L0 index to the same reference picture, using PicNum or
    // LongTermPicNum as id criteria.
    Ipp8u num_ref;
    Ipp8u force_value;
    if (pRefPicList1[0]->m_PictureStructureForDec==FRM_STRUCTURE)
    {
        num_ref = 0;
        force_value = 3;
    } else if (pRefPicList1[0]->m_PictureStructureForDec==AFRM_STRUCTURE)
    {
        if (GetMBFieldDecodingFlag(pRefPicList1[0]->m_mbinfo.mbs[mb_col]))
        {
            Ipp8s field_selector = RefIndexL0&1;
            RefIndexL0>>=1;
            num_ref = (field_selector^(mb_col&1));
            force_value = 1;
        } else {
            num_ref = 0;
            force_value = 3;
        }
        VM_ASSERT(0);
    } else {
        num_ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
            ref_pic_list_struct->m_RefPicList[RefIndexL0],
            ref_pic_list_struct->m_Prediction[RefIndexL0]);
        force_value = 1;
    }
    bool bFound = false;

    if (H264ENC_MAKE_NAME(H264EncoderFrame_isShortTermRef1)(ref_pic_list_struct->m_RefPicList[RefIndexL0], num_ref))
    {
        Ipp32s RefPicNum = H264ENC_MAKE_NAME(H264EncoderFrame_PicNum)(
            ref_pic_list_struct->m_RefPicList[RefIndexL0],
            num_ref,
            force_value);
        Ipp32s RefPOC = ref_pic_list_struct->m_POC[RefIndexL0];
        // find matching reference frame on current slice list 0
        RefIndexL0 = 0;
        while (!bFound)
        {
            //            if (pRefPicList0[RefIndexL0] == NULL)
            //                break;  // reached end of valid entries without a match
            if( RefIndexL0 >= numRefL0Active ) break; //Don't go beyond L0 active refs
            Ipp8u num_cur = H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                pRefPicList0[RefIndexL0],
                pFields0[RefIndexL0]);
            if (H264ENC_MAKE_NAME(H264EncoderFrame_isShortTermRef1)(pRefPicList0[RefIndexL0], num_cur) &&
                H264ENC_MAKE_NAME(H264EncoderFrame_PicNum)(pRefPicList0[RefIndexL0], num_cur, force_value) == RefPicNum &&
                H264ENC_MAKE_NAME(H264EncoderFrame_PicOrderCnt)(pRefPicList0[RefIndexL0], 0, 3) == RefPOC)
                bFound = true;
            else
                RefIndexL0++;
        }

        if (!bFound)
        {
            RefIndexL0 = -1;
        }
    } else if (H264ENC_MAKE_NAME(H264EncoderFrame_isLongTermRef1)(ref_pic_list_struct->m_RefPicList[RefIndexL0], num_ref))
    {
        Ipp32s RefPicNum = H264ENC_MAKE_NAME(H264EncoderFrame_LongTermPicNum)(
            ref_pic_list_struct->m_RefPicList[RefIndexL0],
            num_ref,
            force_value);
        Ipp32s RefPOC = ref_pic_list_struct->m_POC[RefIndexL0];

        // find matching reference frame on current slice list 0
        RefIndexL0 = 0;
        while (!bFound)
        {
            //            if (pRefPicList0[RefIndexL0] == NULL)
            //                break;  // reached end of valid entries without a match
            if( RefIndexL0 >= numRefL0Active ) break; //Don't go beyond L0 active refs
            Ipp8u num_cur = H264ENC_MAKE_NAME(H264EncoderFrame_GetNumberByParity)(
                pRefPicList0[RefIndexL0],
                pFields0[RefIndexL0]);
            if (H264ENC_MAKE_NAME(H264EncoderFrame_isLongTermRef1)(pRefPicList0[RefIndexL0], num_cur) &&
                H264ENC_MAKE_NAME(H264EncoderFrame_LongTermPicNum)(pRefPicList0[RefIndexL0], num_cur, force_value) == RefPicNum &&
                H264ENC_MAKE_NAME(H264EncoderFrame_PicOrderCnt)(pRefPicList0[RefIndexL0], 0, 3) == RefPOC)
                bFound = true;
            else
                RefIndexL0++;
        }
        if (!bFound)
        {
            RefIndexL0 = -1;
        }
    } else {
        // colocated is in a reference that is not marked as Ipp16s-term
        // or long-term, should not happen
        // Well it can be happen since in case of num_ref_frames=1 and this frame is already unmarked as reference
        RefIndexL0 = -1;
    }

    return (RefIndexL0 != -1) ? pRefPicList0[RefIndexL0] : 0;
}

static H264EncoderFrameType* H264ENC_MAKE_NAME(FindDirectRefIdxMBAFF)(
    Ipp32u mb_col,
    Ipp32u mb_cur,
    Ipp32u ipos,                        // offset into MV and RefIndex storage
    Ipp8u mb_field_decoding_flag,
    H264EncoderFrameType **pRefPicList0,
    H264EncoderFrameType **pRefPicList1,
    H264MotionVector  &MVL0, // return colocated MV here
    Ipp8s &RefIndexL0,  // return ref index here
    Ipp32s numRefL0Active
    )
{
    Ipp32u scale_idx=GetMBFieldDecodingFlag(pRefPicList1[0]->m_mbinfo.mbs[mb_col]);
    Ipp32u back_scale_idx=mb_field_decoding_flag;
    Ipp32u field_selector=0;
    VM_ASSERT(pRefPicList1[0]);

    // Set pointers to colocated list 0 ref index and MV
    T_RefIdx * pRefRefIndexL0 = &pRefPicList1[0]->m_mbinfo.RefIdxs[LIST_0][mb_col].RefIdxs[ipos];
    H264MotionVector  *pRefMVL0 = &pRefPicList1[0]->m_mbinfo.MV[LIST_0][mb_col].MotionVectors[ipos];

    VM_ASSERT(pRefRefIndexL0);
    VM_ASSERT(pRefMVL0);

    // Get ref index and MV from L0 of colocated ref MB if the
    // colocated L0 ref index is >=0. Else use L1.
    RefIndexL0 = *pRefRefIndexL0;

    EncoderRefPicListStructType *ref_pic_list_struct;
    if (RefIndexL0 >= 0)
    {
        MVL0 = *pRefMVL0; // Use colocated L0
        // Get pointer to ref pic list 0 of colocated
        ref_pic_list_struct = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
            pRefPicList1[0],
            (pRefPicList1[0]->m_mbinfo.mbs + mb_col)->slice_id,
            LIST_0);
    } else { // Use Ref L1
        // Set pointers to colocated list 1 ref index and MV
        T_RefIdx * pRefRefIndexL1 = &pRefPicList1[0]->m_mbinfo.RefIdxs[LIST_1][mb_col].RefIdxs[ipos];
        H264MotionVector  * pRefMVL1 = &pRefPicList1[0]->m_mbinfo.MV[LIST_1][mb_col].MotionVectors[ipos];
        VM_ASSERT(pRefRefIndexL1);
        VM_ASSERT(pRefMVL1);
        RefIndexL0 = *pRefRefIndexL1;

        MVL0 = *pRefMVL1; // Use colocated L1

        // Get pointer to ref pic list 1 of colocated
        ref_pic_list_struct = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(
            pRefPicList1[0],
            (pRefPicList1[0]->m_mbinfo.mbs + mb_col)->slice_id,
            LIST_1);
    }

    if (RefIndexL0 == -1) // intra and PCM has ref_idx == -1
        return 0;
    if (pRefPicList1[0]->m_PictureStructureForDec==AFRM_STRUCTURE)
    {
        AdjustIndex(mb_cur&1,mb_field_decoding_flag,mb_col&1,
            GetMBFieldDecodingFlag(pRefPicList1[0]->m_mbinfo.mbs[mb_col]),RefIndexL0);
        field_selector = RefIndexL0&scale_idx;
        RefIndexL0>>=scale_idx;
    } else if (pRefPicList1[0]->m_PictureStructureForDec<FRM_STRUCTURE)
    {
        Ipp8u ref1field = pRefPicList1[0]->m_bottom_field_flag[(Ipp32s) mb_col >= (pRefPicList1[0]->totalMBs>>1)];
        field_selector = (ref1field!=ref_pic_list_struct->m_Prediction[RefIndexL0]);
    }
    VM_ASSERT(ref_pic_list_struct->m_RefPicList[RefIndexL0]);
    VM_ASSERT(pRefPicList1[0]->m_PictureStructureForDec!=FRM_STRUCTURE);

    bool bFound = false;
    // Translate the reference index of the colocated to current
    // L0 index to the same reference picture, using PicNum or
    // LongTermPicNum as id criteria.
    if (H264ENC_MAKE_NAME(H264EncoderFrame_isShortTermRef0)(ref_pic_list_struct->m_RefPicList[RefIndexL0]))
    {
        Ipp32s RefPicNum = H264ENC_MAKE_NAME(H264EncoderFrame_PicNum)(
            ref_pic_list_struct->m_RefPicList[RefIndexL0],
            0,
            3);
        Ipp32s RefPOC = ref_pic_list_struct->m_POC[RefIndexL0];

        // find matching reference frame on current slice list 0
        RefIndexL0 = 0;
        while (!bFound)
        {
            //            if (pRefPicList0[RefIndexL0] == NULL)
            //                break;  // reached end of valid entries without a match
            if (RefIndexL0 >= numRefL0Active) break; //Don't go beyond L0 active refs
            if (H264ENC_MAKE_NAME(H264EncoderFrame_isShortTermRef0)(pRefPicList0[RefIndexL0]) &&
                H264ENC_MAKE_NAME(H264EncoderFrame_PicNum)(pRefPicList0[RefIndexL0], 0, 3) == RefPicNum &&
                H264ENC_MAKE_NAME(H264EncoderFrame_PicOrderCnt)(pRefPicList0[RefIndexL0], 0, 3) == RefPOC)
                bFound = true;
            else
                RefIndexL0++;
        }

        if (!bFound)
        {
            RefIndexL0 = -1;
        }
    } else if (H264ENC_MAKE_NAME(H264EncoderFrame_isLongTermRef0)(ref_pic_list_struct->m_RefPicList[RefIndexL0]))
    {
        Ipp32s RefPicNum = H264ENC_MAKE_NAME(H264EncoderFrame_LongTermPicNum)(
            ref_pic_list_struct->m_RefPicList[RefIndexL0],
            0,
            3);
        Ipp32s RefPOC = ref_pic_list_struct->m_POC[RefIndexL0];

        // find matching reference frame on current slice list 0
        RefIndexL0 = 0;
        while (!bFound)
        {
            //            if (pRefPicList0[RefIndexL0] == NULL)
            //                break;  // reached end of valid entries without a match
            if( RefIndexL0 >= numRefL0Active ) break; //Don't go beyond L0 active refs
            if (H264ENC_MAKE_NAME(H264EncoderFrame_isLongTermRef0)(pRefPicList0[RefIndexL0]) &&
                H264ENC_MAKE_NAME(H264EncoderFrame_LongTermPicNum)(pRefPicList0[RefIndexL0], 0, 3) == RefPicNum &&
                H264ENC_MAKE_NAME(H264EncoderFrame_PicOrderCnt)(pRefPicList0[RefIndexL0], 0, 3) == RefPOC)
                bFound = true;
            else
                RefIndexL0++;
        }

        if (!bFound)
        {
            RefIndexL0 = -1;
        }
    } else {
        // colocated is in a reference that is not marked as Ipp16s-term
        // or long-term, should not happen
        // Well it can be happen since in case of num_ref_frames=1 and this frame is already unmarked as reference
        RefIndexL0 = -1;
    }

    if ((RefIndexL0 == -1))
        return 0;

    //RefIndexL0<<=back_scale_idx; // should not do it because we have field RefPicList
    // for mb_field_decoding_flag == 1
    RefIndexL0|=(field_selector&back_scale_idx);
    return pRefPicList0[RefIndexL0];
}

static void H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)(
    PIXTYPE *const        pDirB,      // pointer to current direct mode MB buffer
    const PIXTYPE *const  pPrev,      // pointer to previous ref plane buffer
    const PIXTYPE *const  pFutr,      // pointer to future ref plane buffer
    const Ipp32s          pitchPixels,  // reference buffers pitch in pixels
    const Ipp32u        uInterpType,// 0 = Skip, 1 = Default, 2 = Implicit Weighted
    const Ipp32s        W1,
    const Ipp32s        W0,
    const IppiSize & roiSize)
{
    if (!uInterpType) {
        for (Ipp32s i = 0, k = 0; i < roiSize.height; i ++, k += pitchPixels)
            MFX_INTERNAL_CPY(pDirB + i * 16, pPrev + k, roiSize.width * sizeof(PIXTYPE));
    } else if (uInterpType == 2) {
        for (Ipp32s i = 0, k = 0; i < roiSize.height; i ++, k += pitchPixels)
            for (Ipp32s j = 0; j < roiSize.width; j ++)
                pDirB[i * 16 + j] = (PIXTYPE) ((pPrev[k + j] * W0 + pFutr[k + j] * W1 + 32) >> 6);
    } else {
        H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(pPrev, pFutr, pDirB, roiSize.width, roiSize.height, pitchPixels);
    }
}

bool H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectTemporalMV)(
    void* state,
    H264SliceType *curr_slice,
    H264MacroblockRefIdxs  ref_direct[2],
    H264MacroblockMVs      mvs_direct[2])     // MVs used returned here.
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    Ipp32u uMB = curr_slice->m_cur_mb.uMB;
    H264EncoderFrameType **pRefPicList1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1,is_cur_mb_field,uMB&1)->m_RefPicList;
    H264EncoderFrameType **pRefPicList0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0,is_cur_mb_field,uMB&1)->m_RefPicList;
    Ipp8s *pFields1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1,is_cur_mb_field,uMB&1)->m_Prediction;
    Ipp8s *pFields0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0,is_cur_mb_field,uMB&1)->m_Prediction;
    Ipp8s scale=0;
    Ipp8s block_col=0;
    Ipp32s mb_col = H264ENC_MAKE_NAME(H264CoreEncoder_GetColocatedLocation)(state, curr_slice, pRefPicList1[0],pFields1[0],block_col,&scale);
    Ipp32s sb;

    H264EncoderFrameType * futr_frame = pRefPicList1[0];

    T_RefIdx * ref_direct_l0 = ref_direct[LIST_0].RefIdxs;
    T_RefIdx * ref_direct_l1 = ref_direct[LIST_1].RefIdxs;
    H264MotionVector * mvs_direct_l0 = mvs_direct[LIST_0].MotionVectors;
    H264MotionVector * mvs_direct_l1 = mvs_direct[LIST_1].MotionVectors;

    switch (futr_frame->m_mbinfo.mbs[mb_col].mbtype)
    {
    case MBTYPE_INTER_8x8:
    case MBTYPE_INTER_8x8_REF0:
    case MBTYPE_B_8x8:
        for (sb = 0; sb < 4; sb++)
        {
            core_enc->m_pCurrentFrame->m_mbinfo.mbs[uMB].sbtype[sb] =
                (futr_frame->m_mbinfo.mbs[mb_col].sbtype[sb] == SBTYPE_8x8) ||
                core_enc->m_SeqParamSet->direct_8x8_inference_flag ? SBTYPE_8x8 : SBTYPE_4x4;
        }
        break;
    default:
        for (sb = 0; sb < 4; sb++)
            core_enc->m_pCurrentFrame->m_mbinfo.mbs[uMB].sbtype[sb] = SBTYPE_8x8;
        break;
    }

    for (Ipp32s block = 0; block < 4; block++)
    {
        Ipp32s row = block & 2;
        Ipp32s col = (block & 1)*2;
        Ipp32s sb_pos = row*4 + col;

        Ipp32s inference_mvoffset = 0;
        if (core_enc->m_SeqParamSet->direct_8x8_inference_flag)
        {
            switch(block)
            {
            case 0:
                inference_mvoffset = 0;   // upper left corner
                break;
            case 1:
                inference_mvoffset = 1;   // upper right corner
                break;
            case 2:
                inference_mvoffset = 4;   // lower left corner
                break;
            case 3:
                inference_mvoffset = 4 + 1;   // lower right corner
                break;
            }
        }

        T_RefIdx ref_idx_l0;
        H264MotionVector mv_col;
        /*Ipp8s*/ scale = 0;
        /*Ipp8s*/ block_col = row*4 + col + inference_mvoffset;
        /*Ipp32s*/ mb_col = H264ENC_MAKE_NAME(H264CoreEncoder_GetColocatedLocation)(state, curr_slice, pRefPicList1[0],pFields1[0],block_col,&scale);

        if (IS_INTRA_MBTYPE(futr_frame->m_mbinfo.mbs[mb_col].mbtype))
        {
            for (Ipp32s ypos = 0; ypos < 2; ypos++) // 4 4x4 blocks
            {
                for (Ipp32s xpos = 0; xpos < 2; xpos++)
                {
                    Ipp32s pos = sb_pos + ypos*4 + xpos;
                    ref_direct_l0[pos] = 0;
                    mvs_direct_l0[pos] = mvs_direct_l1[pos] = null_mv;

                }
            }
        } else {
            H264MotionVector* ref_mvs = NULL;
            H264EncoderFrameType *prev_frame =
                core_enc->m_is_cur_pic_afrm?
                H264ENC_MAKE_NAME(FindDirectRefIdxMBAFF)(mb_col, uMB,block_col, pGetMBFieldDecodingFlag(curr_slice->m_cur_mb.GlobalMacroblockInfo),pRefPicList0,pRefPicList1, mv_col, ref_idx_l0, curr_slice->num_ref_idx_l0_active)
                :
            (is_cur_mb_field!=0)?
                H264ENC_MAKE_NAME(FindDirectRefIdxFLD)(mb_col, block_col, pRefPicList0,pRefPicList1, pFields0, pFields1, mv_col, ref_idx_l0,curr_slice->num_ref_idx_l0_active)
                :
            H264ENC_MAKE_NAME(FindDirectRefIdx)(curr_slice, mb_col, block_col, pRefPicList0,pRefPicList1, &ref_mvs, ref_idx_l0, curr_slice->num_ref_idx_l0_active);

            if (!prev_frame) return false;

            //            H264MotionVector  *ref_mvs = &futr_frame->m_mbinfo.MV[LIST_0][mb_col].MotionVectors[block_col];

            ref_direct_l0[sb_pos] = ref_idx_l0;
            ref_direct_l0[sb_pos + 1] = ref_idx_l0;
            ref_direct_l0[sb_pos + 4] = ref_idx_l0;
            ref_direct_l0[sb_pos + 5] = ref_idx_l0;
            Ipp32s uFwdRatio = pGetMBFieldDecodingFlag(curr_slice->m_cur_mb.GlobalMacroblockInfo)?
                curr_slice->DistScaleFactorMVAFF[uMB&1][uMB&1][(uMB&1)^(ref_idx_l0&1)][ref_idx_l0 >> 1] //FIXME
            : curr_slice->DistScaleFactorMV[ref_idx_l0][0];

            for (Ipp32s ypos = 0; ypos < 2; ypos++) // 4 4x4 blocks
            {
                for (Ipp32s xpos = 0; xpos < 2; xpos++)
                {
                    /*H264MotionVector*/ mv_col = core_enc->m_SeqParamSet->direct_8x8_inference_flag ? *ref_mvs :
                        *(ref_mvs + ypos*4 + xpos);

                switch(scale)
                {
                case 1:
                    mv_col.mvy /= 2;
                    break;
                case -1:
                    mv_col.mvy *= 2;
                    break;
                }

                H264MotionVector MVL0, MVL1;
                MVL0.mvx = (Ipp16s) ((uFwdRatio * mv_col.mvx + TR_RND) >> TR_SHIFT);
                MVL0.mvy = (Ipp16s) ((uFwdRatio * mv_col.mvy + TR_RND) >> TR_SHIFT);

                // derived the backward MV from the reference MV: MVb = MVf-MV = -(1-r1)*MV
                MVL1.mvx = (Ipp16s) (MVL0.mvx - mv_col.mvx);
                MVL1.mvy = (Ipp16s) (MVL0.mvy - mv_col.mvy);

                Ipp32s pos = sb_pos + ypos*4 + xpos;
                mvs_direct_l0[pos] = MVL0;
                mvs_direct_l1[pos] = MVL1;
                }
            }
        }
    }

    for (Ipp32s i = 0; i < 16; i++) // Set L1 refs to 0 for temporal direct prediction
        ref_direct_l1[i] = 0;

    return true;
}

///////////////////////////////////////////////////////////////////////////////
// computeDirectSpatialRefIdx
//
// Used for computing L0 (forward) and L1 (backward) ref index for B slice
// direct macroblock when spatial motion vector prediction is used.
// Returned index is minimum positive index of A,B,C neighbor macroblocks,
// following motion vector prediction availability rules. -1 is returned
// if none is available or positive.
//
///////////////////////////////////////////////////////////////////////////////
void H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectSpatialRefIdx)(
    void* state,
    H264SliceType *curr_slice,
    T_RefIdx &ref_idx_l0,
    T_RefIdx &ref_idx_l1)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u uRefIxlL0, uRefIxlL1;
    Ipp32u uRefIxaL0, uRefIxaL1;
    Ipp32u uRefIxrL0, uRefIxrL1;

    Ipp32u lbls=0,lbrs=0,tbls=0,tbrs=0,rbls=0,rbrs=0;

    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    H264MacroblockGlobalInfo *gmbs;
    gmbs = core_enc->m_pCurrentFrame->m_mbinfo.mbs;

    if (cur_mb.BlockNeighbours.mbs_left[0].mb_num >= 0)
    {
        lbls=(pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo)-GetMBFieldDecodingFlag(gmbs[cur_mb.BlockNeighbours.mbs_left[0].mb_num]))>0;
        lbrs=(pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo)-GetMBFieldDecodingFlag(gmbs[cur_mb.BlockNeighbours.mbs_left[0].mb_num]))<0;
        uRefIxlL0 = (Ipp32u)(core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[LIST_0][cur_mb.BlockNeighbours.mbs_left[0].mb_num].RefIdxs[cur_mb.BlockNeighbours.mbs_left[0].block_num] & 0x7f);
        uRefIxlL1 = (Ipp32u)(core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[LIST_1][cur_mb.BlockNeighbours.mbs_left[0].mb_num].RefIdxs[cur_mb.BlockNeighbours.mbs_left[0].block_num] & 0x7f);
    } else {
        // correct for left edge
        uRefIxlL0 = 0x7f; // -1
        uRefIxlL1 = 0x7f; // -1
    }

    if (cur_mb.BlockNeighbours.mb_above.mb_num >= 0)
    {
        tbls = (pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo)-GetMBFieldDecodingFlag(gmbs[cur_mb.BlockNeighbours.mb_above.mb_num]))>0;
        tbrs = (pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo)-GetMBFieldDecodingFlag(gmbs[cur_mb.BlockNeighbours.mb_above.mb_num]))<0;
        uRefIxaL0 = (Ipp32u)(core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[LIST_0][cur_mb.BlockNeighbours.mb_above.mb_num].RefIdxs[cur_mb.BlockNeighbours.mb_above.block_num] & 0x7f);
        uRefIxaL1 = (Ipp32u)(core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[LIST_1][cur_mb.BlockNeighbours.mb_above.mb_num].RefIdxs[cur_mb.BlockNeighbours.mb_above.block_num] & 0x7f);
    } else {
        // correct for top edge
        uRefIxaL0 = 0x7f; // -1
        uRefIxaL1 = 0x7f; // -1
    }

    // correct for upper right
    if (cur_mb.BlockNeighbours.mb_above_right.mb_num >= 0)
    {
        rbls = (pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo)-GetMBFieldDecodingFlag(gmbs[cur_mb.BlockNeighbours.mb_above_right.mb_num]))>0;
        rbrs = (pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo)-GetMBFieldDecodingFlag(gmbs[cur_mb.BlockNeighbours.mb_above_right.mb_num]))<0;
        uRefIxrL0 = (Ipp32u)(core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[LIST_0][cur_mb.BlockNeighbours.mb_above_right.mb_num].RefIdxs[cur_mb.BlockNeighbours.mb_above_right.block_num] & 0x7f);
        uRefIxrL1 = (Ipp32u)(core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[LIST_1][cur_mb.BlockNeighbours.mb_above_right.mb_num].RefIdxs[cur_mb.BlockNeighbours.mb_above_right.block_num] & 0x7f);
    } else if (cur_mb.BlockNeighbours.mb_above_left.mb_num >= 0)
    {
        rbls = (pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo)-GetMBFieldDecodingFlag(gmbs[cur_mb.BlockNeighbours.mb_above_left.mb_num]))>0;
        rbrs = (pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo)-GetMBFieldDecodingFlag(gmbs[cur_mb.BlockNeighbours.mb_above_left.mb_num]))<0;
        uRefIxrL0 = (Ipp32u)(core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[LIST_0][cur_mb.BlockNeighbours.mb_above_left.mb_num].RefIdxs[cur_mb.BlockNeighbours.mb_above_left.block_num] & 0x7f);
        uRefIxrL1 = (Ipp32u)(core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[LIST_1][cur_mb.BlockNeighbours.mb_above_left.mb_num].RefIdxs[cur_mb.BlockNeighbours.mb_above_left.block_num] & 0x7f);
    } else {
        uRefIxrL0 = 0x7f; // -1
        uRefIxrL1 = 0x7f; // -1
    }

    // Returned index is positiveMIN(a,b,c), -1 is returned only when all are <0.
    Ipp32u uRefIxL0 = MIN(((uRefIxlL0<<lbls)>>lbrs),
        MIN(((uRefIxaL0<<tbls)>>tbrs), ((uRefIxrL0<<rbls)>>rbrs)));
    Ipp32u uRefIxL1 = MIN(((uRefIxlL1<<lbls)>>lbrs),
        MIN(((uRefIxaL1<<tbls)>>tbrs), ((uRefIxrL1<<rbls)>>rbrs)));

    ref_idx_l0 = uRefIxL0 >= 0x3f ? -1 : (T_RefIdx)uRefIxL0;
    ref_idx_l1 = uRefIxL1 >= 0x3f ? -1 : (T_RefIdx)uRefIxL1;
}   // computeDirectSpatialRefIdx


void H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectSubblockSpatial)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32u block_idx,              // which 4x4 Block (UL Corner, Raster Order)
    Ipp32u uBlocksWide,         // 1, 2, or 4
    Ipp32u uBlocksHigh,         // 1, 2, or 4 (4x16 and 16x4 not permitted)
    H264MacroblockRefIdxs  ref_idxs_direct[2],
    H264MacroblockMVs      mvs_direct[2])
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    H264MacroblockGlobalInfo *gmbs = core_enc->m_pCurrentFrame->m_mbinfo.mbs;
    H264BlockLocation neighbours[MB_ALL_NEIGHBOURS];
    T_RefIdx ref_idx[2];
    Ipp32s refIdx[2][MB_ALL_NEIGHBOURS];
    const H264MotionVector *mvs[2][MB_ALL_NEIGHBOURS];
    H264MotionVector mvs_temp[2][MB_ALL_NEIGHBOURS];
    bool is_diff_ref_idxs[2][MB_ALL_NEIGHBOURS];
    Ipp32s n, list;

    neighbours[MB_A].block_num = block_idx;
    neighbours[MB_B].block_num = block_idx;
    neighbours[MB_C].block_num = block_idx + uBlocksWide - 1;

    if (core_enc->m_SliceHeader.MbaffFrameFlag)
    {
        H264ENC_MAKE_NAME(H264CoreEncoder_GetLeftLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_A], 0);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_B], false);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopRightLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_C]);

        if (neighbours[MB_C].mb_num < 0)
        {
            neighbours[MB_C].block_num = block_idx;
            H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLeftLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_C]);
        }
    }else{
        H264ENC_MAKE_NAME(H264CoreEncoder_GetLeftLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_A]);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_B]);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopRightLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_C]);

        if (neighbours[MB_C].mb_num < 0)
        {
            neighbours[MB_C].block_num = block_idx;
            H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLeftLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_C]);
        }
    }

    ref_idx[LIST_0] = -1;
    ref_idx[LIST_1] = -1;
    for ( n = MB_A; n <= MB_C; n++) {
        Ipp32s ls, rs;
        ls=(pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo)-GetMBFieldDecodingFlag(gmbs[neighbours[n].mb_num]))>0;
        rs=(pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo)-GetMBFieldDecodingFlag(gmbs[neighbours[n].mb_num]))<0;
        for (list = LIST_0; list <= LIST_1; list++) {
            mvs[list][n] = &null_mv;
            refIdx[list][n] = -1;
            if (neighbours[n].mb_num >= 0)
            {
                Ipp32s idx;
                if (IS_INTRA_MBTYPE(core_enc->m_pCurrentFrame->m_mbinfo.mbs[neighbours[n].mb_num].mbtype))
                    idx = -1;
                else
                    idx = core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[list][neighbours[n].mb_num].RefIdxs[neighbours[n].block_num];
                if (idx >= 0) {
                    idx = (idx<<ls)>>rs;  // mbaff scaling
                    if ( ref_idx[list] < 0 || idx < ref_idx[list])  // find minimal but >= 0
                        ref_idx[list] = idx;
                    refIdx[list][n] = idx;
                    mvs_temp[list][n] = core_enc->m_pCurrentFrame->m_mbinfo.MV[list][neighbours[n].mb_num].MotionVectors[neighbours[n].block_num];
                    mvs_temp[list][n].mvy = ((mvs_temp[list][n].mvy + ((mvs_temp[list][n].mvy < 0)&&ls)) << rs) >>ls;
                    mvs[list][n] = &mvs_temp[list][n];
                }
            }
        }
    }

    for (list = LIST_0; list <= LIST_1; list++) {
        H264MotionVector MVPred;
        is_diff_ref_idxs[list][n] = true;
        for ( n = MB_A; n <= MB_C; n++) {
            is_diff_ref_idxs[list][n] = neighbours[n].mb_num < 0 || (refIdx[list][n] != ref_idx[list]);
        }

        // 16x8 and 8x16 are not considered for direct
        if ((!is_diff_ref_idxs[list][MB_A] && is_diff_ref_idxs[list][MB_B] && is_diff_ref_idxs[list][MB_C]) ||
            (neighbours[MB_B].mb_num < 0 && neighbours[MB_C].mb_num < 0)) {
            //pred_type = MVPRED_A;
            MVPred.mvx = mvs[list][MB_A]->mvx;
            MVPred.mvy = mvs[list][MB_A]->mvy;
        } else if (is_diff_ref_idxs[list][MB_A] && !is_diff_ref_idxs[list][MB_B] && is_diff_ref_idxs[list][MB_C]) {
            //pred_type = MVPRED_B;
            MVPred.mvx = mvs[list][MB_B]->mvx;
            MVPred.mvy = mvs[list][MB_B]->mvy;
        } else if (is_diff_ref_idxs[list][MB_A] && is_diff_ref_idxs[list][MB_B] && !is_diff_ref_idxs[list][MB_C]) {
            //pred_type = MVPRED_C;
            MVPred.mvx = mvs[list][MB_C]->mvx;
            MVPred.mvy = mvs[list][MB_C]->mvy;
        } else {
            // pred_type = MVPRED_MEDIAN;
           MVPred.mvx = MIN(mvs[list][MB_A]->mvx, mvs[list][MB_B]->mvx) ^
                MIN(mvs[list][MB_B]->mvx, mvs[list][MB_C]->mvx) ^
                MIN(mvs[list][MB_C]->mvx, mvs[list][MB_A]->mvx);

           MVPred.mvy = MIN(mvs[list][MB_A]->mvy, mvs[list][MB_B]->mvy) ^
                MIN(mvs[list][MB_B]->mvy, mvs[list][MB_C]->mvy) ^
                MIN(mvs[list][MB_C]->mvy, mvs[list][MB_A]->mvy);
        }
        for (Ipp32u y = 0; y < uBlocksHigh; y++)
        for (Ipp32u x = 0; x < uBlocksWide; x++) {
            mvs_direct[list].MotionVectors[block_idx+y*4+x] = MVPred;
        }
    }

    if (core_enc->m_svc_layer.svc_ext.dependency_id > 0 || core_enc->m_svc_layer.svc_ext.quality_id > 0)
    if (ref_idx[0] == -1 && ref_idx[1] == -1)
        ref_idx[0] = ref_idx[1] = 0;

    for (list = LIST_0; list <= LIST_1; list++) {
        for (Ipp32u y = 0; y < uBlocksHigh; y++)
        for (Ipp32u x = 0; x < uBlocksWide; x++) {
            ref_idxs_direct[list].RefIdxs[block_idx+y*4+x] = ref_idx[list];
        }
    }

}   // computeDirectSubblockSpatialRefIdx


////////////////////////////////////////////////////////////////////////////////
//  Calc_One_MV_Predictor
//
// Calculate a motion vector predictor and delta for a single block of any
// permitted shape and position.
//
////////////////////////////////////////////////////////////////////////////////
void H264ENC_MAKE_NAME(H264CoreEncoder_Calc_One_MV_Predictor)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32u block_idx,              // which 4x4 Block (UL Corner, Raster Order)
    Ipp32u uList,               // 0, 1 for L0 or L1.
    Ipp32u uBlocksWide,         // 1, 2, or 4
    Ipp32u uBlocksHigh,         // 1, 2, or 4 (4x16 and 16x4 not permitted)
    H264MotionVector *pMVPred,        // resulting MV predictor
    H264MotionVector *pMVDelta,    // resulting MV delta
    bool   updateDMV)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264BlockLocation neighbours[MB_ALL_NEIGHBOURS];
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;

    neighbours[MB_A].block_num = block_idx;
    neighbours[MB_B].block_num = block_idx;
    neighbours[MB_C].block_num = block_idx + uBlocksWide - 1;

    if (core_enc->m_SliceHeader.MbaffFrameFlag)
    {
        H264ENC_MAKE_NAME(H264CoreEncoder_GetLeftLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_A], 0);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_B], false);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopRightLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_C]);

        if (neighbours[MB_C].mb_num < 0)
        {
            neighbours[MB_C].block_num = block_idx;
            H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLeftLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_C]);
        }
    }else{
        H264ENC_MAKE_NAME(H264CoreEncoder_GetLeftLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_A]);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_B]);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopRightLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_C]);

        if (neighbours[MB_C].mb_num < 0)
        {
            neighbours[MB_C].block_num = block_idx;
            H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLeftLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_C]);
        }
    }

    const T_RefIdx &curr_ref_idx = cur_mb.RefIdxs[uList]->RefIdxs[block_idx];

    const H264MotionVector *mvs[MB_ALL_NEIGHBOURS];
    H264MotionVector mvs_temp[MB_ALL_NEIGHBOURS];
    bool is_diff_ref_idxs[MB_ALL_NEIGHBOURS];

    H264MacroblockGlobalInfo *gmbs = core_enc->m_pCurrentFrame->m_mbinfo.mbs;
    for (Ipp32s n = MB_A; n <= MB_C; n++)
    {
        if (neighbours[n].mb_num >= 0)
        {
            T_RefIdx ref_idx =
                core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[uList][neighbours[n].mb_num].RefIdxs[neighbours[n].block_num];

            if (core_enc->m_SliceHeader.MbaffFrameFlag)
            {
                Ipp32u ls = (pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo) - GetMBFieldDecodingFlag(gmbs[neighbours[n].mb_num])) > 0;
                Ipp32u rs = (pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo) - GetMBFieldDecodingFlag(gmbs[neighbours[n].mb_num])) < 0;

                if (ref_idx == -1 || IS_INTRA_MBTYPE(core_enc->m_pCurrentFrame->m_mbinfo.mbs[neighbours[n].mb_num].mbtype))
                    mvs[n] = &null_mv;
                else{
                    mvs_temp[n] = core_enc->m_pCurrentFrame->m_mbinfo.MV[uList][neighbours[n].mb_num].MotionVectors[neighbours[n].block_num];
                    mvs_temp[n].mvy = ((mvs_temp[n].mvy + ((mvs_temp[n].mvy < 0)&&ls)) << rs) >>ls;
                    mvs[n] = &mvs_temp[n];
                }

                is_diff_ref_idxs[n] = ( ((ref_idx<<ls)>>rs) != curr_ref_idx );
            } else {
                if (ref_idx == -1 || IS_INTRA_MBTYPE(core_enc->m_pCurrentFrame->m_mbinfo.mbs[neighbours[n].mb_num].mbtype))
                    mvs[n] = &null_mv;
                else
                    mvs[n] = &core_enc->m_pCurrentFrame->m_mbinfo.MV[uList][neighbours[n].mb_num].MotionVectors[neighbours[n].block_num];

                is_diff_ref_idxs[n] = (curr_ref_idx != ref_idx);
            }
        }else{
            mvs[n] = &null_mv;
            is_diff_ref_idxs[n] = true;
        }
    }

    PredType   pred_type = MVPRED_MEDIAN;

    if (!is_diff_ref_idxs[MB_A] && is_diff_ref_idxs[MB_B] && is_diff_ref_idxs[MB_C])
        pred_type = MVPRED_A;
    else if (is_diff_ref_idxs[MB_A] && !is_diff_ref_idxs[MB_B] && is_diff_ref_idxs[MB_C])
        pred_type = MVPRED_B;
    else if (is_diff_ref_idxs[MB_A] && is_diff_ref_idxs[MB_B] && !is_diff_ref_idxs[MB_C])
        pred_type = MVPRED_C;

    if ((uBlocksHigh + uBlocksWide) == 6) // 8x16 and 16x8 block sizes
    {
        if (uBlocksHigh == 2) // 16x8
        {
            if ((block_idx == 0) && !is_diff_ref_idxs[MB_B])
            {
                // 16x8 - Top block - Above uses same ref and not Intra
                // Predict from Above in this case
                pred_type = MVPRED_B;
            } else {
                if ((block_idx == 8) && !is_diff_ref_idxs[MB_A])
                {
                    // 16x8 - Bottom block - Left uses same ref and not Intra
                    // Predict from Left in this case
                    pred_type = MVPRED_A;
                }
            }
        } else {    // 8x16
            if ((block_idx == 0) && !is_diff_ref_idxs[MB_A]) {
                // 8x16 - Left block - Left uses same ref and not Intra
                // Predict from Left in this case
                pred_type = MVPRED_A;
            } else if ((block_idx == 2) && !is_diff_ref_idxs[MB_C]) {
                // 8x16 - Right block - Above Right uses same ref and not Intra
                // Predict from Above-Right in this case
                pred_type = MVPRED_C;
            }
        }
    }

    if (neighbours[MB_B].mb_num < 0 && neighbours[MB_C].mb_num < 0)
    {
        pred_type = MVPRED_A;
    }

    switch(pred_type)
    {
    case MVPRED_MEDIAN:
        pMVPred[0].mvx = MIN(mvs[MB_A]->mvx, mvs[MB_B]->mvx) ^
            MIN(mvs[MB_B]->mvx, mvs[MB_C]->mvx) ^
            MIN(mvs[MB_C]->mvx, mvs[MB_A]->mvx);

        pMVPred[0].mvy = MIN(mvs[MB_A]->mvy, mvs[MB_B]->mvy) ^
            MIN(mvs[MB_B]->mvy, mvs[MB_C]->mvy) ^
            MIN(mvs[MB_C]->mvy, mvs[MB_A]->mvy);
        break;
    case MVPRED_A:
        pMVPred[0].mvx = mvs[MB_A]->mvx;
        pMVPred[0].mvy = mvs[MB_A]->mvy;
        break;
    case MVPRED_B:
        pMVPred[0].mvx = mvs[MB_B]->mvx;
        pMVPred[0].mvy = mvs[MB_B]->mvy;
        break;
    case MVPRED_C:
        pMVPred[0].mvx = mvs[MB_C]->mvx;
        pMVPred[0].mvy = mvs[MB_C]->mvy;
        break;
    default:
        VM_ASSERT(false);
    }

    pMVDelta[0].mvx = (Ipp32s)cur_mb.MVs[uList]->MotionVectors[block_idx].mvx -
        (Ipp32s)pMVPred[0].mvx;
    pMVDelta[0].mvy = (Ipp32s)cur_mb.MVs[uList]->MotionVectors[block_idx].mvy -
        (Ipp32s)pMVPred[0].mvy;

    if (updateDMV)
    {
        cur_mb.MVs[uList + 2]->MotionVectors[block_idx] = pMVDelta[0];
    }

    return;
}   // Calc_One_MV_Predictor


void H264ENC_MAKE_NAME(H264CoreEncoder_CDirectBOneMB_Interp)(
    void* state,
    H264SliceType *curr_slice,
    H264MacroblockRefIdxs  ref_direct[2],
    H264MacroblockMVs      mvs_direct[2])     // MVs used returned here.
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32s posx, posy;
    Ipp32s w1 = 0;
    Ipp32s w0 = 0;
    Ipp32u uMB = curr_slice->m_cur_mb.uMB;
    const Ipp32u uMBx = curr_slice->m_cur_mb.uMBx * 16;
    const Ipp32u uMBy = curr_slice->m_cur_mb.uMBy * 16;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    Ipp32s pitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels<<is_cur_mb_field;

    const Ipp32s uFrmWidth = core_enc->m_WidthInMBs << 4;
    const Ipp32s uFrmHeight = core_enc->m_HeightInMBs << 4;

    PIXTYPE *const pInterpBuf1 = curr_slice->m_pTempBuff4DirectB;
    PIXTYPE *const pInterpBuf2 = curr_slice->m_pMBEncodeBuffer + 256;
    PIXTYPE *const pDirB  = curr_slice->m_pPred4DirectB;

    Ipp32u uInterpType = core_enc->use_implicit_weighted_bipred ? 2 : 1;

    T_RefIdx * ref_direct_l0 = ref_direct[LIST_0].RefIdxs;
    T_RefIdx * ref_direct_l1 = ref_direct[LIST_1].RefIdxs;
    H264MotionVector * mvs_direct_l0 = mvs_direct[LIST_0].MotionVectors;
    H264MotionVector * mvs_direct_l1 = mvs_direct[LIST_1].MotionVectors;

    const PIXTYPE* pRefBlk;

    PIXTYPE * pPrev = 0;
    PIXTYPE * pFutr = 0;

    T_EncodeMBOffsets* pMBOffset = &core_enc->m_pMBOffsets[uMB];
    Ipp32s offset = pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] ;

    H264EncoderFrameType **pRefPicList1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1,is_cur_mb_field,uMB&1)->m_RefPicList;
    H264EncoderFrameType **pRefPicList0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0,is_cur_mb_field,uMB&1)->m_RefPicList;
    Ipp8s *pFields1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1,is_cur_mb_field,uMB&1)->m_Prediction;
    Ipp8s *pFields0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0,is_cur_mb_field,uMB&1)->m_Prediction;

#ifdef NO_PADDING
#if PIXBITS == 8
    IppVCInterpolateBlock_8u interpolateInfo, *pInterpolateInfo = &interpolateInfo;
#elif PIXBITS == 16
    IppVCInterpolateBlock_16u interpolateInfo, *pInterpolateInfo = &interpolateInfo;
#endif //PIXBITS
#endif // NO_PADDING
    posx = uMBx;
    posy = uMBy;

    for (Ipp32s sb = 0; sb < 4; sb++){
        Ipp32s sb_row = sb & 2;
        Ipp32s sb_col = (sb & 1)*2;

        Ipp32s sb_pos = sb_row*4 + sb_col;
        Ipp32s sb_offset = sb_row*64 + 4*sb_col;

        T_RefIdx ref_idx_l0 = ref_direct_l0[sb_pos];

        if (ref_idx_l0 != -1)
            pPrev = pRefPicList0[ref_idx_l0]->m_pYPlane + offset + sb_row*pitchPixels*4 + 4*sb_col + curr_slice->m_InitialOffset[pFields0[ref_idx_l0]];
        else
            pPrev = 0;

        T_RefIdx ref_idx_l1 = ref_direct_l1[sb_pos];
        if (ref_idx_l1 != -1){
            pFutr = pRefPicList1[ref_idx_l1]->m_pYPlane + offset + sb_row*pitchPixels*4 + 4*sb_col + curr_slice->m_InitialOffset[pFields1[ref_idx_l1]];
            if( pPrev ){
                w1 = curr_slice->DistScaleFactor[ref_idx_l0][ref_idx_l1]>> 2;
                w0 = 64 - w1;
            }
        }else
            pFutr = 0;

        bool is_8x8_same = core_enc->m_SeqParamSet->direct_8x8_inference_flag ||
            (curr_slice->m_cur_mb.GlobalMacroblockInfo->sbtype[sb] == SBTYPE_8x8);
        IppiSize sz = is_8x8_same ? size8x8 : size4x4;

#ifdef NO_PADDING
        pInterpolateInfo->sizeBlock = sz;
        pInterpolateInfo->sizeFrame.width = uFrmWidth;
        pInterpolateInfo->sizeFrame.height = uFrmHeight;
#endif // NO_PADDING
        posy = uMBy + sb_row*4;
        posx = uMBx + sb_col*4;
        for (Ipp32s ypos = 0; ypos < 2; ypos++){ // 4 4x4 blocks
            for (Ipp32s xpos = 0; xpos < 2; xpos++){
                Ipp32s mv_pos = sb_pos + ypos*4 + xpos;
                if (pPrev){
                    H264MotionVector mvL0 = mvs_direct_l0[mv_pos];
                    TRUNCATE_LO(mvL0.mvx, (uFrmWidth - posx + 1) << SUB_PEL_SHIFT);
                    TRUNCATE_HI(mvL0.mvx, (-16 - posx - 1) << SUB_PEL_SHIFT);
                    TRUNCATE_LO(mvL0.mvy, (uFrmHeight - posy + 1) << SUB_PEL_SHIFT);
                    TRUNCATE_HI(mvL0.mvy, (-16 - posy - 1) << SUB_PEL_SHIFT);

                    pRefBlk = MVADJUST(pPrev + ypos*pitchPixels*4 + xpos*4, pitchPixels, mvL0.mvx>>SUB_PEL_SHIFT, mvL0.mvy>>SUB_PEL_SHIFT);
                    PIXTYPE* pPrevBlk = pInterpBuf1 + sb_offset + ypos*16*4 + xpos*4;
#ifdef NO_PADDING
                    pInterpolateInfo->pSrc[0] = pRefPicList0[ref_idx_l0]->m_pYPlane + curr_slice->m_InitialOffset[pFields0[ref_idx_l0]] + (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
                    pInterpolateInfo->srcStep = pitchPixels;
                    pInterpolateInfo->pDst[0] = pPrevBlk;
                    pInterpolateInfo->dstStep = 16;
                    pInterpolateInfo->pointBlockPos.x = (sb_col + xpos)*4 + uMBx;
                    pInterpolateInfo->pointBlockPos.y = (sb_row + ypos)*4 + uMBy;
                    pInterpolateInfo->pointVector.x = mvL0.mvx;
                    pInterpolateInfo->pointVector.y = mvL0.mvy;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                    H264ENC_MAKE_NAME(ippiInterpolateLuma_H264)(pRefBlk, pitchPixels, pPrevBlk, 16, mvL0.mvx&3, mvL0.mvy&3, sz, core_enc->m_PicParamSet->bit_depth_luma);
#endif // NO_PADDING
                }
                if (pFutr){
                    H264MotionVector mvL1 = mvs_direct_l1[mv_pos];
                    TRUNCATE_LO(mvL1.mvx, (uFrmWidth - posx + 1) << SUB_PEL_SHIFT);
                    TRUNCATE_HI(mvL1.mvx, (-16 - posx - 1) << SUB_PEL_SHIFT);
                    TRUNCATE_LO(mvL1.mvy, (uFrmHeight - posy + 1) << SUB_PEL_SHIFT);
                    TRUNCATE_HI(mvL1.mvy, (-16 - posy - 1) << SUB_PEL_SHIFT);

                    pRefBlk = MVADJUST(pFutr + ypos*pitchPixels*4 + xpos*4, pitchPixels, mvL1.mvx>>SUB_PEL_SHIFT, mvL1.mvy>>SUB_PEL_SHIFT);
                    PIXTYPE* pFutrBlk = pInterpBuf2 + sb_offset + ypos*16*4 + xpos*4;
#ifdef NO_PADDING
                    pInterpolateInfo->pSrc[0] = pRefPicList1[ref_idx_l1]->m_pYPlane + curr_slice->m_InitialOffset[pFields1[ref_idx_l1]] + (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
                    pInterpolateInfo->srcStep = pitchPixels;
                    pInterpolateInfo->pDst[0] = pFutrBlk;
                    pInterpolateInfo->dstStep = 16;
                    pInterpolateInfo->pointBlockPos.x = (sb_col + xpos)*4 + uMBx;
                    pInterpolateInfo->pointBlockPos.y = (sb_row + ypos)*4 + uMBy;
                    pInterpolateInfo->pointVector.x = mvL1.mvx;
                    pInterpolateInfo->pointVector.y = mvL1.mvy;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                    H264ENC_MAKE_NAME(ippiInterpolateLuma_H264)(pRefBlk, pitchPixels, pFutrBlk, 16, mvL1.mvx & 3, mvL1.mvy & 3, sz, core_enc->m_PicParamSet->bit_depth_luma);
#endif // NO_PADDING
                }
                if( is_8x8_same ) break;
            }
            if( is_8x8_same ) break;
        }

        if (pFutr == 0){
            for (Ipp32s i = 0, k = 0; i < 8; i ++, k += 16)
                MFX_INTERNAL_CPY(pDirB + sb_offset + k, pInterpBuf1 + sb_offset + k, 8 * sizeof(PIXTYPE));
        }else if (pPrev == 0){
            for (Ipp32s i = 0, k = 0; i < 8; i ++, k += 16)
                MFX_INTERNAL_CPY(pDirB + sb_offset + k, pInterpBuf2 + sb_offset + k, 8 * sizeof(PIXTYPE));
        }else{
            H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)(
                pDirB + sb_offset, pInterpBuf1 + sb_offset,
                pInterpBuf2 + sb_offset, 16, uInterpType,
                w1, w0, size8x8);
        }
    }
} // CDirectBOneMB__Interp

// prediction for B direct, currently only for 8x8 blocks

void H264ENC_MAKE_NAME(H264CoreEncoder_CDirectB8x8_Interp)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s block,                             // block index in raster order (0-15)
    H264MacroblockRefIdxs  ref_direct[2],     // only correspondent idx and
    H264MacroblockMVs      mvs_direct[2])     // MV are read
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32s posx, posy;
    Ipp32s w1 = 0;
    Ipp32s w0 = 0;
    Ipp32u uMB = curr_slice->m_cur_mb.uMB;
    const Ipp32u uMBx = curr_slice->m_cur_mb.uMBx * 16;
    const Ipp32u uMBy = curr_slice->m_cur_mb.uMBy * 16;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    Ipp32s pitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels<<is_cur_mb_field;

    const Ipp32s uFrmWidth = core_enc->m_WidthInMBs << 4;
    const Ipp32s uFrmHeight = core_enc->m_HeightInMBs << 4;

    PIXTYPE *const pInterpBuf1 = curr_slice->m_pTempBuff4DirectB;
    PIXTYPE *const pInterpBuf2 = curr_slice->m_pMBEncodeBuffer + 256;
    PIXTYPE *const pDirB  = curr_slice->m_pPred4DirectB8x8; // special dst, other than for 16x16

    Ipp32u uInterpType = core_enc->use_implicit_weighted_bipred ? 2 : 1;

    T_RefIdx * ref_direct_l0 = ref_direct[LIST_0].RefIdxs;
    T_RefIdx * ref_direct_l1 = ref_direct[LIST_1].RefIdxs;
    H264MotionVector * mvs_direct_l0 = mvs_direct[LIST_0].MotionVectors;
    H264MotionVector * mvs_direct_l1 = mvs_direct[LIST_1].MotionVectors;

    const PIXTYPE* pRefBlk;

    PIXTYPE * pPrev = 0;
    PIXTYPE * pFutr = 0;

    T_EncodeMBOffsets* pMBOffset = &core_enc->m_pMBOffsets[uMB];
    Ipp32s offset = pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] ;

    //PIXTYPE *pPrevU = 0, *pPrevV = 0, *pFutrU = 0, *pFutrV = 0;
    //Ipp32s offset_chr = pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];

    H264EncoderFrameType **pRefPicList1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1,is_cur_mb_field,uMB&1)->m_RefPicList;
    H264EncoderFrameType **pRefPicList0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0,is_cur_mb_field,uMB&1)->m_RefPicList;
    Ipp8s *pFields1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1,is_cur_mb_field,uMB&1)->m_Prediction;
    Ipp8s *pFields0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0,is_cur_mb_field,uMB&1)->m_Prediction;

#ifdef NO_PADDING
#if PIXBITS == 8
    IppVCInterpolateBlock_8u interpolateInfo, *pInterpolateInfo = &interpolateInfo;
#elif PIXBITS == 16
    IppVCInterpolateBlock_16u interpolateInfo, *pInterpolateInfo = &interpolateInfo;
#endif //PIXBITS
#endif // NO_PADDING
    posx = uMBx;
    posy = uMBy;

    Ipp32s sb_row = block >> 2;
    Ipp32s sb_col = block & 3;
    Ipp32s subblock = (sb_row & 2) + (sb_col >> 1);

    Ipp32s sb_pos = block;
    Ipp32s sb_offset = sb_row*64 + 4*sb_col;

    T_RefIdx ref_idx_l0 = ref_direct_l0[sb_pos];
    T_RefIdx ref_idx_l1 = ref_direct_l1[sb_pos];

    if (ref_idx_l0 != -1)
        pPrev = pRefPicList0[ref_idx_l0]->m_pYPlane + offset + sb_row*pitchPixels*4 + 4*sb_col + curr_slice->m_InitialOffset[pFields0[ref_idx_l0]];
    else
        pPrev = 0;

    if (ref_idx_l1 != -1){
        pFutr = pRefPicList1[ref_idx_l1]->m_pYPlane + offset + sb_row*pitchPixels*4 + 4*sb_col + curr_slice->m_InitialOffset[pFields1[ref_idx_l1]];
        if( pPrev ){
            w1 = curr_slice->DistScaleFactor[ref_idx_l0][ref_idx_l1]>> 2;
            w0 = 64 - w1;
        }
    }else
        pFutr = 0;

    bool is_8x8_same = core_enc->m_SeqParamSet->direct_8x8_inference_flag ||
        (curr_slice->m_cur_mb.GlobalMacroblockInfo->sbtype[subblock] == SBTYPE_8x8);
    IppiSize sz = is_8x8_same ? size8x8 : size4x4;

#ifdef NO_PADDING
    pInterpolateInfo->sizeBlock = sz;
    pInterpolateInfo->sizeFrame.width = uFrmWidth;
    pInterpolateInfo->sizeFrame.height = uFrmHeight;
#endif // NO_PADDING
    posy = uMBy + sb_row*4;
    posx = uMBx + sb_col*4;
    for (Ipp32s ypos = 0; ypos < 2; ypos++){ // 4 4x4 blocks
        for (Ipp32s xpos = 0; xpos < 2; xpos++){
            Ipp32s mv_pos = sb_pos + ypos*4 + xpos;
            if (pPrev){
                H264MotionVector mvL0 = mvs_direct_l0[mv_pos];
                TRUNCATE_LO(mvL0.mvx, (uFrmWidth - posx + 1) << SUB_PEL_SHIFT);
                TRUNCATE_HI(mvL0.mvx, (-16 - posx - 1) << SUB_PEL_SHIFT);
                TRUNCATE_LO(mvL0.mvy, (uFrmHeight - posy + 1) << SUB_PEL_SHIFT);
                TRUNCATE_HI(mvL0.mvy, (-16 - posy - 1) << SUB_PEL_SHIFT);

                pRefBlk = MVADJUST(pPrev + ypos*pitchPixels*4 + xpos*4, pitchPixels, mvL0.mvx>>SUB_PEL_SHIFT, mvL0.mvy>>SUB_PEL_SHIFT);
                PIXTYPE* pPrevBlk = pInterpBuf1 + sb_offset + ypos*16*4 + xpos*4;
#ifdef NO_PADDING
                pInterpolateInfo->pSrc[0] = pRefPicList0[ref_idx_l0]->m_pYPlane + curr_slice->m_InitialOffset[pFields0[ref_idx_l0]] + (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
                pInterpolateInfo->srcStep = pitchPixels;
                pInterpolateInfo->pDst[0] = pPrevBlk;
                pInterpolateInfo->dstStep = 16;
                pInterpolateInfo->pointBlockPos.x = (sb_col + xpos)*4 + uMBx;
                pInterpolateInfo->pointBlockPos.y = (sb_row + ypos)*4 + uMBy;
                pInterpolateInfo->pointVector.x = mvL0.mvx;
                pInterpolateInfo->pointVector.y = mvL0.mvy;
                H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                H264ENC_MAKE_NAME(ippiInterpolateLuma_H264)(pRefBlk, pitchPixels, pPrevBlk, 16, mvL0.mvx&3, mvL0.mvy&3, sz, core_enc->m_PicParamSet->bit_depth_luma);
#endif // NO_PADDING
            }
            if (pFutr){
                H264MotionVector mvL1 = mvs_direct_l1[mv_pos];
                TRUNCATE_LO(mvL1.mvx, (uFrmWidth - posx + 1) << SUB_PEL_SHIFT);
                TRUNCATE_HI(mvL1.mvx, (-16 - posx - 1) << SUB_PEL_SHIFT);
                TRUNCATE_LO(mvL1.mvy, (uFrmHeight - posy + 1) << SUB_PEL_SHIFT);
                TRUNCATE_HI(mvL1.mvy, (-16 - posy - 1) << SUB_PEL_SHIFT);

                pRefBlk = MVADJUST(pFutr + ypos*pitchPixels*4 + xpos*4, pitchPixels, mvL1.mvx>>SUB_PEL_SHIFT, mvL1.mvy>>SUB_PEL_SHIFT);
                PIXTYPE* pFutrBlk = pInterpBuf2 + sb_offset + ypos*16*4 + xpos*4;
#ifdef NO_PADDING
                pInterpolateInfo->pSrc[0] = pRefPicList1[ref_idx_l1]->m_pYPlane + curr_slice->m_InitialOffset[pFields1[ref_idx_l1]] + (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
                pInterpolateInfo->srcStep = pitchPixels;
                pInterpolateInfo->pDst[0] = pFutrBlk;
                pInterpolateInfo->dstStep = 16;
                pInterpolateInfo->pointBlockPos.x = (sb_col + xpos)*4 + uMBx;
                pInterpolateInfo->pointBlockPos.y = (sb_row + ypos)*4 + uMBy;
                pInterpolateInfo->pointVector.x = mvL1.mvx;
                pInterpolateInfo->pointVector.y = mvL1.mvy;
                H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                H264ENC_MAKE_NAME(ippiInterpolateLuma_H264)(pRefBlk, pitchPixels, pFutrBlk, 16, mvL1.mvx & 3, mvL1.mvy & 3, sz, core_enc->m_PicParamSet->bit_depth_luma);
#endif // NO_PADDING
            }
            if( is_8x8_same ) break;
        }
        if( is_8x8_same ) break;
    }

    if (pFutr == 0){
        for (Ipp32s i = 0, k = 0; i < 8; i ++, k += 16)
            MFX_INTERNAL_CPY(pDirB + sb_offset + k, pInterpBuf1 + sb_offset + k, 8 * sizeof(PIXTYPE));
    }else if (pPrev == 0){
        for (Ipp32s i = 0, k = 0; i < 8; i ++, k += 16)
            MFX_INTERNAL_CPY(pDirB + sb_offset + k, pInterpBuf2 + sb_offset + k, 8 * sizeof(PIXTYPE));
    }else{
        H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)(
            pDirB + sb_offset, pInterpBuf1 + sb_offset,
            pInterpBuf2 + sb_offset, 16, uInterpType,
            w1, w0, size8x8);
    }

} // H264CoreEncoder_CDirectB8x8_Interp

bool H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectSpatialMV)(
    void* state,
    H264SliceType *curr_slice,
    H264MacroblockRefIdxs  ref_direct[2],
    H264MacroblockMVs      mvs_direct[2]     // MVs used returned here.
)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    T_RefIdx            fwd_idx, bwd_idx;
    H264MotionVector    fwd_mv, bwd_mv;
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    Ipp32u uMB = curr_slice->m_cur_mb.uMB;

    H264EncoderFrameType **pRefPicList1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1,is_cur_mb_field,uMB&1)->m_RefPicList;
    Ipp8s *pFields1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1,is_cur_mb_field,uMB&1)->m_Prediction;

    H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectSpatialRefIdx)(state, curr_slice, fwd_idx, bwd_idx);

    T_RefIdx * ref_direct_l0 = ref_direct[LIST_0].RefIdxs;
    T_RefIdx * ref_direct_l1 = ref_direct[LIST_1].RefIdxs;
    H264MotionVector * mvs_direct_l0 = mvs_direct[LIST_0].MotionVectors;
    H264MotionVector * mvs_direct_l1 = mvs_direct[LIST_1].MotionVectors;

    if (fwd_idx != -1) // Forward MV (L0)
    {
        H264MotionVector delta;
        curr_slice->m_cur_mb.RefIdxs[LIST_0]->RefIdxs[0] = fwd_idx;
        H264ENC_MAKE_NAME(H264CoreEncoder_Calc_One_MV_Predictor)(state, curr_slice, 0, 0, 4, 4, &fwd_mv, &delta, false);
    } else {
        fwd_mv = null_mv;
    }

    if (bwd_idx != -1) // Backward MV (L1)
    {
        H264MotionVector delta;
        curr_slice->m_cur_mb.RefIdxs[LIST_1]->RefIdxs[0] = bwd_idx;
        H264ENC_MAKE_NAME(H264CoreEncoder_Calc_One_MV_Predictor)(state, curr_slice, 0, 1, 4, 4, &bwd_mv, &delta, false);
    } else {
        bwd_mv = null_mv;
    }

    bool is_both_refs_not_exist = (fwd_idx == -1) && (bwd_idx == -1);

    // kta
    if (core_enc->m_svc_layer.svc_ext.dependency_id > 0 || core_enc->m_svc_layer.svc_ext.quality_id > 0) {

      if (is_both_refs_not_exist)
        fwd_idx = bwd_idx = 0;

      for (Ipp32s bl = 0; bl < 16; bl++) {
        mvs_direct_l0[bl] = fwd_mv;
        mvs_direct_l1[bl] = bwd_mv;
        ref_direct_l0[bl] = fwd_idx;
        ref_direct_l1[bl] = bwd_idx;
      }

      // ??? need that?
      for (Ipp32s bl = 0; bl < 4; bl++)
        curr_slice->m_cur_mb.GlobalMacroblockInfo->sbtype[bl] = SBTYPE_8x8;

      return true;
    }

    bool is_l1_pic_short_term = (H264ENC_MAKE_NAME(H264EncoderFrame_isShortTermRef0)(pRefPicList1[0]) != 0);

    Ipp8s sb_part_offset[4];
    Ipp32s mbs_col[4];
    if (core_enc->m_SeqParamSet->direct_8x8_inference_flag)
    {
        sb_part_offset[0] = 0;
        sb_part_offset[2] = 12;
        mbs_col[1] = mbs_col[0] = H264ENC_MAKE_NAME(H264CoreEncoder_GetColocatedLocation)(state, curr_slice, pRefPicList1[0], pFields1[0], sb_part_offset[0], 0);
        mbs_col[3] = mbs_col[2] = H264ENC_MAKE_NAME(H264CoreEncoder_GetColocatedLocation)(state, curr_slice, pRefPicList1[0], pFields1[0], sb_part_offset[2], 0);

        sb_part_offset[1] = sb_part_offset[0] + 3;
        sb_part_offset[3] = sb_part_offset[2] + 3;
    } else {
        sb_part_offset[0] = 0;
        sb_part_offset[2] = 8;

        mbs_col[1] = mbs_col[0] = H264ENC_MAKE_NAME(H264CoreEncoder_GetColocatedLocation)(state, curr_slice, pRefPicList1[0],pFields1[0], sb_part_offset[0], 0);
        mbs_col[3] = mbs_col[2] = H264ENC_MAKE_NAME(H264CoreEncoder_GetColocatedLocation)(state, curr_slice, pRefPicList1[0],pFields1[0], sb_part_offset[2], 0);

        sb_part_offset[1] = sb_part_offset[0] + 2;
        sb_part_offset[3] = sb_part_offset[2] + 2;
    }

    //bool is_all_8x8_same = true;
    bool is_all_4x4_same = true;

    for (Ipp32s sb = 0; sb < 4; sb++)
    {
        is_all_4x4_same = true;

        for (Ipp32s ypos=0; ypos < 2; ypos++) // 4 4x4 blocks
        {
            for (Ipp32s xpos=0; xpos < 2; xpos++)
            {
                bool is_use_zero_pred_l0 = false;
                bool is_use_zero_pred_l1 = false;

                Ipp32s sbcolpartoffset = (core_enc->m_SeqParamSet->direct_8x8_inference_flag) ?
                    sb_part_offset[sb] : sb_part_offset[sb] + ypos*4 + xpos;

                Ipp32s mb_col = mbs_col[sb];
                if ((fwd_idx != -1 || bwd_idx != -1) &&
                    IS_INTER_MBTYPE(pRefPicList1[0]->m_mbinfo.mbs[mb_col].mbtype) &&
                    is_l1_pic_short_term)
                {
                    T_RefIdx ref_col = pRefPicList1[0]->m_mbinfo.RefIdxs[LIST_0][mb_col].RefIdxs[sbcolpartoffset];
                    const H264MotionVector *mv_col;

                    if (ref_col >= 0)
                    {
                        mv_col = &pRefPicList1[0]->m_mbinfo.MV[LIST_0][mb_col].MotionVectors[sbcolpartoffset];
                    }else{
                        ref_col = pRefPicList1[0]->m_mbinfo.RefIdxs[LIST_1][mb_col].RefIdxs[sbcolpartoffset];
                        mv_col = &pRefPicList1[0]->m_mbinfo.MV[LIST_1][mb_col].MotionVectors[sbcolpartoffset];
                    }

                    if (ref_col == 0)
                    {
                        if (mv_col->mvx >= -1  &&
                            mv_col->mvx <=  1  &&
                            mv_col->mvy >= -1  &&
                            mv_col->mvy <=  1)
                        {
                            is_use_zero_pred_l0 = (fwd_idx == 0);
                            is_use_zero_pred_l1 = (bwd_idx == 0);
                            is_all_4x4_same = /*is_all_8x8_same =*/ false;
                        }
                    }
                }

                Ipp32s sbpartoffset = (ypos + (sb&2))*4 + (sb&1)*2 + xpos;

                mvs_direct_l0[sbpartoffset] = (fwd_idx != -1 && !is_use_zero_pred_l0) ?
fwd_mv : null_mv;
                ref_direct_l0[sbpartoffset] = is_both_refs_not_exist ? 0 : fwd_idx;

                mvs_direct_l1[sbpartoffset] = (bwd_idx != -1 && !is_use_zero_pred_l1) ?
bwd_mv : null_mv;
                ref_direct_l1[sbpartoffset] = is_both_refs_not_exist ? 0 : bwd_idx;
            }
        }

        curr_slice->m_cur_mb.GlobalMacroblockInfo->sbtype[sb] = is_all_4x4_same ? SBTYPE_8x8 : SBTYPE_4x4;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
//  Skip_MV_Predicted
//
//  Returns true if this MB uses a MV that is exactly predicted according to the
//  skip MB prediction rules.
//
////////////////////////////////////////////////////////////////////////////////
void H264ENC_MAKE_NAME(H264CoreEncoder_Skip_MV_Predicted)(
    void* state,
    H264SliceType *curr_slice,
    H264MotionVector* pMVPredicted,
    H264MotionVector *pMVOut  // Returns Skip MV if not NULL
    )
{
    if (pMVOut == NULL) return;
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264BlockLocation mb_a = curr_slice->m_cur_mb.BlockNeighbours.mbs_left[0];
    H264BlockLocation mb_b = curr_slice->m_cur_mb.BlockNeighbours.mb_above;

    // First, check to see if the above and left MBs are available
    if (mb_a.mb_num >= 0 && mb_b.mb_num >= 0)
    {
        H264GlobalMacroblocksDescriptor* mb =  &core_enc->m_pCurrentFrame->m_mbinfo;
        if( mb->RefIdxs[LIST_0][mb_a.mb_num].RefIdxs[mb_a.block_num] == 0 ){//Block A
            if( mb->MV[LIST_0][mb_a.mb_num].MotionVectors[mb_a.block_num].is_zero() || IS_INTRA_MBTYPE(mb->mbs[mb_a.mb_num].mbtype)){
                *pMVOut = null_mv;
                return;
            }
        }
        if( mb->RefIdxs[LIST_0][mb_b.mb_num].RefIdxs[mb_b.block_num] == 0 ){ // Block B
            if( mb->MV[LIST_0][mb_b.mb_num].MotionVectors[mb_b.block_num].is_zero() || IS_INTRA_MBTYPE(mb->mbs[mb_b.mb_num].mbtype)){
                *pMVOut = null_mv;
                return;
            }
        }
        // Otherwise the prediction is using the normal median prediction
        if( pMVPredicted != NULL ){
            *pMVOut = *pMVPredicted;
        }else{
            //H264MotionVector iMVPred;  // resulting MV predictors
            ME_InfType meinf;
            H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, 0, LIST_0, 4, 4, &meinf);
            meinf.useScPredMV = 0; // to check if enough
            meinf.useScPredMVAlways = 0;
            *pMVOut = meinf.predictedMV;
        }
    } else { // Above or left MB is not available, so zero is predicted.
        *pMVOut = null_mv;
    }
}   // Skip_MV_Predicted

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s is8x8)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    H264MacroblockLocalInfo sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
    H264MacroblockGlobalInfo sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;
    H264BsBase* pBitstream = curr_slice->m_pbitstream;

    H264ENC_MAKE_NAME(H264BsFake_Reset)(curr_slice->fakeBitstream);
    H264BsBase_CopyContextCABAC_InterB(&curr_slice->fakeBitstream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field, is8x8);
    curr_slice->m_pbitstream = (H264BsBase *)curr_slice->fakeBitstream;
    pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, is8x8);
    cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
    H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantInter_RD)(state, curr_slice);
    H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaInter_RD)(state, curr_slice);
    H264ENC_MAKE_NAME(H264CoreEncoder_Put_MB_Fake)(state, curr_slice);
    Ipp32s bs = H264ENC_MAKE_NAME(H264BsFake_GetBsOffset)(curr_slice->fakeBitstream);
    Ipp32s d = SSD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, cur_mb.mbInter.reconstruct, 16);
    Ipp32s uOffset = core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
    if (core_enc->m_PicParamSet->chroma_format_idc != 0) {
        //TODO 420 only !!
        d += SSD8x8(core_enc->m_pCurrentFrame->m_pUPlane + uOffset, cur_mb.mbPitchPixels, cur_mb.mbChromaInter.reconstruct, 16);
        d += SSD8x8(core_enc->m_pCurrentFrame->m_pVPlane + uOffset, cur_mb.mbPitchPixels, cur_mb.mbChromaInter.reconstruct+8, 16);
    }

    curr_slice->m_pbitstream = pBitstream;
    *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
    *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;
    return (d<<5) + cur_mb.lambda * bs;
}

#ifdef USE_NV12
Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s is8x8,
    Ipp32s isCurMBSeparated,
    Ipp16s *pResPredDiff,
    Ipp16s *pResPredDiffC)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    H264MacroblockLocalInfo sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
    H264MacroblockGlobalInfo sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;
    H264BsBase* pBitstream = curr_slice->m_pbitstream;

    __ALIGN16 PIXTYPE pUSrc[64];
    __ALIGN16 PIXTYPE pVSrc[64];
    PIXTYPE * pSrc;

    Ipp32s uOffset = core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];

    if (!isCurMBSeparated)
    {
        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
        for(Ipp32s i=0;i<8;i++){
            Ipp32s s = 8*i;
            pUSrc[s+0] = pSrc[0];
            pVSrc[s+0] = pSrc[1];
            pUSrc[s+1] = pSrc[2];
            pVSrc[s+1] = pSrc[3];
            pUSrc[s+2] = pSrc[4];
            pVSrc[s+2] = pSrc[5];
            pUSrc[s+3] = pSrc[6];
            pVSrc[s+3] = pSrc[7];
            pUSrc[s+4] = pSrc[8];
            pVSrc[s+4] = pSrc[9];
            pUSrc[s+5] = pSrc[10];
            pVSrc[s+5] = pSrc[11];
            pUSrc[s+6] = pSrc[12];
            pVSrc[s+6] = pSrc[13];
            pUSrc[s+7] = pSrc[14];
            pVSrc[s+7] = pSrc[15];

            pSrc[0] = pUSrc[s+0];
            pSrc[1] = pUSrc[s+1];
            pSrc[2] = pUSrc[s+2];
            pSrc[3] = pUSrc[s+3];
            pSrc[4] = pUSrc[s+4];
            pSrc[5] = pUSrc[s+5];
            pSrc[6] = pUSrc[s+6];
            pSrc[7] = pUSrc[s+7];
            pSrc[8] = pVSrc[s+0];
            pSrc[9] = pVSrc[s+1];
            pSrc[10] = pVSrc[s+2];
            pSrc[11] = pVSrc[s+3];
            pSrc[12] = pVSrc[s+4];
            pSrc[13] = pVSrc[s+5];
            pSrc[14] = pVSrc[s+6];
            pSrc[15] = pVSrc[s+7];

            pSrc += cur_mb.mbPitchPixels;
        }
    }

    H264ENC_MAKE_NAME(H264BsFake_Reset)(curr_slice->fakeBitstream);
    H264BsBase_CopyContextCABAC_InterB(&curr_slice->fakeBitstream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field, is8x8);
    curr_slice->m_pbitstream = (H264BsBase *)curr_slice->fakeBitstream;
    pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, is8x8);
    cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
    H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantInter_RD)(state, curr_slice, pResPredDiff);
    H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaInter_RD_NV12)(state, curr_slice, 1, pResPredDiffC);
    H264ENC_MAKE_NAME(H264CoreEncoder_Put_MB_Fake)(state, curr_slice);
    Ipp32s bs = H264ENC_MAKE_NAME(H264BsFake_GetBsOffset)(curr_slice->fakeBitstream);
    Ipp32s d = SSD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, cur_mb.mbInter.reconstruct, 16);
    if (core_enc->m_PicParamSet->chroma_format_idc != 0) {
        //TODO 420 only !!
        d += SSD8x8(core_enc->m_pCurrentFrame->m_pUPlane + uOffset, cur_mb.mbPitchPixels, cur_mb.mbChromaInter.reconstruct, 16);
        d += SSD8x8(core_enc->m_pCurrentFrame->m_pUPlane + uOffset + 8, cur_mb.mbPitchPixels, cur_mb.mbChromaInter.reconstruct+8, 16);
    }

    if (!isCurMBSeparated)
    {
        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
        for(Ipp32s i=0;i<8;i++){
            Ipp32s s = 8*i;

            pSrc[0] = pUSrc[s+0];
            pSrc[1] = pVSrc[s+0];
            pSrc[2] = pUSrc[s+1];
            pSrc[3] = pVSrc[s+1];
            pSrc[4] = pUSrc[s+2];
            pSrc[5] = pVSrc[s+2];
            pSrc[6] = pUSrc[s+3];
            pSrc[7] = pVSrc[s+3];
            pSrc[8] = pUSrc[s+4];
            pSrc[9] = pVSrc[s+4];
            pSrc[10] = pUSrc[s+5];
            pSrc[11] = pVSrc[s+5];
            pSrc[12] = pUSrc[s+6];
            pSrc[13] = pVSrc[s+6];
            pSrc[14] = pUSrc[s+7];
            pSrc[15] = pVSrc[s+7];

            pSrc += cur_mb.mbPitchPixels;
        }
    }

    curr_slice->m_pbitstream = pBitstream;
    *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
    *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;
    return (d<<5) + cur_mb.lambda * bs;
}
#endif // USE_NV12

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s is8x8,
    Ipp32s bestSAD)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    H264MacroblockLocalInfo sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
    H264MacroblockGlobalInfo sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;

    pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, is8x8);
    cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
    H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantInter_RD)(state, curr_slice);
    Ipp32s d = SSD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, cur_mb.mbInter.reconstruct, 16)<<5;
    if( d > bestSAD ){
        *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
        *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;
        return MAX_SAD;
    }
    if (core_enc->m_PicParamSet->chroma_format_idc != 0) {
        H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaInter_RD)(state, curr_slice);
        Ipp32s uOffset = core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
        //TODO 420 only !!
        d += SSD8x8(core_enc->m_pCurrentFrame->m_pUPlane + uOffset, cur_mb.mbPitchPixels, cur_mb.mbChromaInter.reconstruct, 16)<<5;
        d += SSD8x8(core_enc->m_pCurrentFrame->m_pVPlane + uOffset, cur_mb.mbPitchPixels, cur_mb.mbChromaInter.reconstruct+8, 16)<<5;
        if( d > bestSAD ){
            *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
            *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;
            return MAX_SAD;
        }
    }

    H264BsBase* pBitstream = curr_slice->m_pbitstream;
    H264BsFakeType* stream = curr_slice->fakeBitstream;
    H264ENC_MAKE_NAME(H264BsFake_Reset)(stream);
    H264BsBase_CopyContextCABAC_InterP(&stream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field, is8x8);
    curr_slice->m_pbitstream = (H264BsBase *)stream;
    H264ENC_MAKE_NAME(H264CoreEncoder_Put_MB_Fake)(state, curr_slice);
    Ipp32s bs = H264ENC_MAKE_NAME(H264BsFake_GetBsOffset)(stream);
    curr_slice->m_pbitstream = pBitstream;
    *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
    *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;
    d += cur_mb.lambda * bs;
    if( d > bestSAD ) return MAX_SAD;
    return d;
}

#ifdef USE_NV12
Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost_NV12)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s is8x8,
    Ipp32s isCurMBSeparated,
    Ipp32s bestSAD,
    Ipp16s *pResPredDiff,
    Ipp16s *pResPredDiffC)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    H264MacroblockLocalInfo sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
    H264MacroblockGlobalInfo sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;

    __ALIGN16 PIXTYPE pUSrc[64];
    __ALIGN16 PIXTYPE pVSrc[64];
    PIXTYPE * pSrc;

    pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, is8x8);
    cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;

    H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantInter_RD)(state, curr_slice, pResPredDiff);

    Ipp32s d = SSD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, cur_mb.mbInter.reconstruct, 16)<<5;

    if( d > bestSAD ){
        *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
        *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;
        return MAX_SAD;
    }

    if (core_enc->m_PicParamSet->chroma_format_idc == 1) {

        Ipp32s uOffset = core_enc->m_pMBOffsets[cur_mb.uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];

        if (!isCurMBSeparated)
        {
            pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
            for(Ipp32s i=0;i<8;i++){
                Ipp32s s = 8*i;
                pUSrc[s+0] = pSrc[0];
                pVSrc[s+0] = pSrc[1];
                pUSrc[s+1] = pSrc[2];
                pVSrc[s+1] = pSrc[3];
                pUSrc[s+2] = pSrc[4];
                pVSrc[s+2] = pSrc[5];
                pUSrc[s+3] = pSrc[6];
                pVSrc[s+3] = pSrc[7];
                pUSrc[s+4] = pSrc[8];
                pVSrc[s+4] = pSrc[9];
                pUSrc[s+5] = pSrc[10];
                pVSrc[s+5] = pSrc[11];
                pUSrc[s+6] = pSrc[12];
                pVSrc[s+6] = pSrc[13];
                pUSrc[s+7] = pSrc[14];
                pVSrc[s+7] = pSrc[15];

                pSrc[0] = pUSrc[s+0];
                pSrc[1] = pUSrc[s+1];
                pSrc[2] = pUSrc[s+2];
                pSrc[3] = pUSrc[s+3];
                pSrc[4] = pUSrc[s+4];
                pSrc[5] = pUSrc[s+5];
                pSrc[6] = pUSrc[s+6];
                pSrc[7] = pUSrc[s+7];
                pSrc[8] = pVSrc[s+0];
                pSrc[9] = pVSrc[s+1];
                pSrc[10] = pVSrc[s+2];
                pSrc[11] = pVSrc[s+3];
                pSrc[12] = pVSrc[s+4];
                pSrc[13] = pVSrc[s+5];
                pSrc[14] = pVSrc[s+6];
                pSrc[15] = pVSrc[s+7];

                pSrc += cur_mb.mbPitchPixels;
            }
        }

        H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaInter_RD_NV12)(state, curr_slice, 1, pResPredDiffC);

        //TODO 420 only !!
        d += SSD8x8(core_enc->m_pCurrentFrame->m_pUPlane + uOffset, cur_mb.mbPitchPixels, cur_mb.mbChromaInter.reconstruct, 16)<<5;
        d += SSD8x8(core_enc->m_pCurrentFrame->m_pUPlane + uOffset + 8, cur_mb.mbPitchPixels, cur_mb.mbChromaInter.reconstruct+8, 16)<<5;

        if (!isCurMBSeparated)
        {
            pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uOffset;
            for(Ipp32s i=0;i<8;i++){
                Ipp32s s = 8*i;

                pSrc[0] = pUSrc[s+0];
                pSrc[1] = pVSrc[s+0];
                pSrc[2] = pUSrc[s+1];
                pSrc[3] = pVSrc[s+1];
                pSrc[4] = pUSrc[s+2];
                pSrc[5] = pVSrc[s+2];
                pSrc[6] = pUSrc[s+3];
                pSrc[7] = pVSrc[s+3];
                pSrc[8] = pUSrc[s+4];
                pSrc[9] = pVSrc[s+4];
                pSrc[10] = pUSrc[s+5];
                pSrc[11] = pVSrc[s+5];
                pSrc[12] = pUSrc[s+6];
                pSrc[13] = pVSrc[s+6];
                pSrc[14] = pUSrc[s+7];
                pSrc[15] = pVSrc[s+7];

                pSrc += cur_mb.mbPitchPixels;
            }
        }

        if( d > bestSAD ){
            *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
            *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;
            return MAX_SAD;
        }
    }

    H264BsBase* pBitstream = curr_slice->m_pbitstream;
    H264BsFakeType* stream = curr_slice->fakeBitstream;
    H264ENC_MAKE_NAME(H264BsFake_Reset)(stream);
    H264BsBase_CopyContextCABAC_InterP(&stream->m_base, pBitstream, !curr_slice->m_is_cur_mb_field, is8x8);
    curr_slice->m_pbitstream = (H264BsBase *)stream;
    H264ENC_MAKE_NAME(H264CoreEncoder_Put_MB_Fake)(state, curr_slice);
    Ipp32s bs = H264ENC_MAKE_NAME(H264BsFake_GetBsOffset)(stream);
    curr_slice->m_pbitstream = pBitstream;
    *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
    *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;
    d += cur_mb.lambda * bs;

    if( d > bestSAD ) return MAX_SAD;
    return d;
}
#endif // USE_NV12

void H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32u block_idx,
    Ipp32u uList,               // 0, 1
    Ipp32u uBlocksWide,         // 1, 2, or 4
    Ipp32u uBlocksHigh,         // 1, 2, or 4 (4x16 and 16x4 not permitted)
    ME_InfType* meinf)          // resulting MV predictor here
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264BlockLocation neighbours[MB_ALL_NEIGHBOURS];
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    H264MotionVector *pMVPred = &meinf->predictedMV;  // resulting MV predictor
    Ipp32s checkScPredMV = !core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag &&
        pGetMBCropFlag(cur_mb.GlobalMacroblockInfo) &&
        !IS_INTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype) &&
        cur_mb.LocalMacroblockInfo->m_RefLayerRefIdxs[uList].RefIdxs[block_idx] == cur_mb.RefIdxs[uList]->RefIdxs[block_idx];

    meinf->useScPredMVAlways = checkScPredMV &&
        !curr_slice->m_adaptive_motion_prediction_flag && curr_slice->m_default_motion_prediction_flag;

    meinf->useScPredMV =  meinf->useScPredMVAlways || (checkScPredMV && curr_slice->m_adaptive_motion_prediction_flag);

 //meinf->useScPredMV = 0; // tmp to switch off!!!
    if (meinf->useScPredMV)
        meinf->scPredMV = cur_mb.LocalMacroblockInfo->m_RefLayerMVs[uList].MotionVectors[block_idx];

    neighbours[MB_A].block_num = block_idx;
    neighbours[MB_B].block_num = block_idx;
    neighbours[MB_C].block_num = block_idx + uBlocksWide - 1;
    if (core_enc->m_SliceHeader.MbaffFrameFlag) {
        H264ENC_MAKE_NAME(H264CoreEncoder_GetLeftLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_A], 0);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_B], false);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopRightLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_C]);
        if (neighbours[MB_C].mb_num < 0) {
            neighbours[MB_C].block_num = block_idx;
            H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLeftLocationForCurrentMBLumaMBAFF)(state, cur_mb, &neighbours[MB_C]);
        }
    } else {
        H264ENC_MAKE_NAME(H264CoreEncoder_GetLeftLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_A]);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_B]);
        H264ENC_MAKE_NAME(H264CoreEncoder_GetTopRightLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_C]);
        if (neighbours[MB_C].mb_num < 0) {
            neighbours[MB_C].block_num = block_idx;
            H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLeftLocationForCurrentMBLumaNonMBAFF)(cur_mb, &neighbours[MB_C]);
        }
    }
    const T_RefIdx &curr_ref_idx = cur_mb.RefIdxs[uList]->RefIdxs[block_idx];
    const H264MotionVector *mvs[MB_ALL_NEIGHBOURS];
    H264MotionVector mvs_temp[MB_ALL_NEIGHBOURS];
    bool is_diff_ref_idxs[MB_ALL_NEIGHBOURS];

    H264MacroblockGlobalInfo *gmbs = core_enc->m_pCurrentFrame->m_mbinfo.mbs;
    for (Ipp32s n = MB_A; n <= MB_C; n++) {
        if (neighbours[n].mb_num >= 0) {
            T_RefIdx ref_idx =  core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[uList][neighbours[n].mb_num].RefIdxs[neighbours[n].block_num];
            if (core_enc->m_SliceHeader.MbaffFrameFlag) {
                Ipp32u ls = (pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo) - GetMBFieldDecodingFlag(gmbs[neighbours[n].mb_num])) > 0;
                Ipp32u rs = (pGetMBFieldDecodingFlag(cur_mb.GlobalMacroblockInfo) - GetMBFieldDecodingFlag(gmbs[neighbours[n].mb_num])) < 0;
                if (ref_idx == -1 || IS_INTRA_MBTYPE(core_enc->m_pCurrentFrame->m_mbinfo.mbs[neighbours[n].mb_num].mbtype))
                    mvs[n] = &null_mv;
                else{
                    mvs_temp[n] = core_enc->m_pCurrentFrame->m_mbinfo.MV[uList][neighbours[n].mb_num].MotionVectors[neighbours[n].block_num];
                    mvs_temp[n].mvy = ((mvs_temp[n].mvy + ((mvs_temp[n].mvy < 0)&&ls)) << rs) >>ls;
                    mvs[n] = &mvs_temp[n];
                }
                is_diff_ref_idxs[n] = ( ((ref_idx<<ls)>>rs) != curr_ref_idx );
            } else {
                if (ref_idx == -1 || IS_INTRA_MBTYPE(core_enc->m_pCurrentFrame->m_mbinfo.mbs[neighbours[n].mb_num].mbtype))
                    mvs[n] = &null_mv;
                else
                    mvs[n] = &core_enc->m_pCurrentFrame->m_mbinfo.MV[uList][neighbours[n].mb_num].MotionVectors[neighbours[n].block_num];
                is_diff_ref_idxs[n] = (curr_ref_idx != ref_idx);
            }
        } else {
            mvs[n] = &null_mv;
            is_diff_ref_idxs[n] = true;
        }
    }
    PredType   pred_type = MVPRED_MEDIAN;
    if (!is_diff_ref_idxs[MB_A] && is_diff_ref_idxs[MB_B] && is_diff_ref_idxs[MB_C])
        pred_type = MVPRED_A;
    else if (is_diff_ref_idxs[MB_A] && !is_diff_ref_idxs[MB_B] && is_diff_ref_idxs[MB_C])
        pred_type = MVPRED_B;
    else if (is_diff_ref_idxs[MB_A] && is_diff_ref_idxs[MB_B] && !is_diff_ref_idxs[MB_C])
        pred_type = MVPRED_C;
    if ((uBlocksHigh + uBlocksWide) == 6) // 8x16 and 16x8 block sizes
    {
        if (uBlocksHigh == 2) // 16x8
        {
            if ((block_idx == 0) && !is_diff_ref_idxs[MB_B])
            {
                // 16x8 - Top block - Above uses same ref and not Intra. Predict from Above in this case
                pred_type = MVPRED_B;
            } else {
                if ((block_idx == 8) && !is_diff_ref_idxs[MB_A])
                {
                    // 16x8 - Bottom block - Left uses same ref and not Intra. Predict from Left in this case
                    pred_type = MVPRED_A;
                }
            }
        } else {    // 8x16
            if ((block_idx == 0) && !is_diff_ref_idxs[MB_A]) {
                // 8x16 - Left block - Left uses same ref and not Intra. Predict from Left in this case
                pred_type = MVPRED_A;
            } else if ((block_idx == 2) && !is_diff_ref_idxs[MB_C]) {
                // 8x16 - Right block - Above Right uses same ref and not Intra. Predict from Above-Right in this case
                pred_type = MVPRED_C;
            }
        }
    }
    if (neighbours[MB_B].mb_num < 0 && neighbours[MB_C].mb_num < 0)
        pred_type = MVPRED_A;
    switch(pred_type) {
    case MVPRED_MEDIAN:
        pMVPred[0].mvx = MIN(mvs[MB_A]->mvx, mvs[MB_B]->mvx) ^ MIN(mvs[MB_B]->mvx, mvs[MB_C]->mvx) ^ MIN(mvs[MB_C]->mvx, mvs[MB_A]->mvx);
        pMVPred[0].mvy = MIN(mvs[MB_A]->mvy, mvs[MB_B]->mvy) ^ MIN(mvs[MB_B]->mvy, mvs[MB_C]->mvy) ^ MIN(mvs[MB_C]->mvy, mvs[MB_A]->mvy);
        break;
    case MVPRED_A:
        pMVPred[0].mvx = mvs[MB_A]->mvx;
        pMVPred[0].mvy = mvs[MB_A]->mvy;
        break;
    case MVPRED_B:
        pMVPred[0].mvx = mvs[MB_B]->mvx;
        pMVPred[0].mvy = mvs[MB_B]->mvy;
        break;
    case MVPRED_C:
        pMVPred[0].mvx = mvs[MB_C]->mvx;
        pMVPred[0].mvy = mvs[MB_C]->mvy;
        break;
    default:
        VM_ASSERT(false);
    }
    if (!meinf->useScPredMV)
        meinf->scPredMV = *pMVPred;
}


#ifdef TABLE_FUNC

#if PIXBITS == 8

inline Ipp32s H264ENC_MAKE_NAME(SAD)(
    Ipp8u *pCur,
    Ipp32s pitchPixelsCur,
    Ipp8u *pRef,
    Ipp32s pitchPixelsRef,
    Ipp32s blockSize)
{
    Ipp32s sad;
    SAD_8u[blockSize](pCur, pitchPixelsCur, pRef, pitchPixelsRef, &sad, 0);
    return sad;
}

inline Ipp32s H264ENC_MAKE_NAME(SATD)(
    Ipp8u *pCur,
    Ipp32s pitchPixelsCur,
    Ipp8u *pRef,
    Ipp32s pitchPixelsRef,
    Ipp32s blockSize)
{
    return SATD_8u[blockSize](pCur, pitchPixelsCur, pRef, pitchPixelsRef);
}

Ipp16u bl_wxh[21] = {0, 0, (2 << 8) | 2, (2 << 8) | 4, (4 << 8) | 2, (4 << 8) | 4, (4 << 8) | 8, 0, 0, (8 << 8) | 4, (8 << 8) | 8, 0, (8 << 8) | 16, 0, 0, 0, 0, 0, (16 << 8) | 8, 0, (16 << 8) | 16};

inline Ipp32s SAD_ResPred(Ipp16s *pResPred,
                          Ipp32s stepR,
                          Ipp8u *pRef,
                          Ipp32s pitchPixelsRef,
                          Ipp32s blockSize)
{
  Ipp32s sad;
  Ipp32s h, w = bl_wxh[blockSize];
  Ipp32s i, j, diff;
  h = w & 0xff;
  w >>= 8;

  // pResPred should contain (src-resPred) !!!

  sad = 0;
  for (i = 0; i < h; i++)
  {
      for (j = 0; j < w; j++)
      {
          diff = pResPred[j] - pRef[j];
          sad += ABS(diff);
      }
      pRef += pitchPixelsRef;
      pResPred += stepR;
  }
  return sad;
}


inline Ipp32s SATD_ResPred(Ipp16s *pResPred,
                           Ipp32s stepR,
                           Ipp8u *pRef,
                           Ipp32s pitchPixelsRef,
                           Ipp32s blockSize)
{
  Ipp32s sad;
  Ipp32s h, w = bl_wxh[blockSize];
  Ipp32s i, j;
  Ipp32s x, y;
  __ALIGN16 Ipp16s diffBuffR[4][4];

  h = w & 0xff;
  w >>= 8;
  sad = 0;

  if (blockSize > 4) // w >= 4 && h >= 4
  {
      for (i = 0; i < h; i += 4)
      {
          for (j = 0; j < w; j += 4)
          {
              PIXTYPE *pPred = pRef + j;
              Ipp16s *pRes = pResPred + j;

              for (y = 0; y < 4; y++)
              {
                  for (x = 0; x < 4; x++)
                  {
                      diffBuffR[y][x] = pRes[x] - pPred[x];
                  }
                  pPred += pitchPixelsRef;
                  pRes += stepR;
              }
              sad += Hadamard4x4_AbsSum(diffBuffR);
          }
          pRef += 4 * pitchPixelsRef;
          pResPred += 4 * stepR;
      }
  } else
  {
      for (y = 0; y < h; y++)
      {
          for (x = 0; x < w; x++)
          {
              diffBuffR[y][x] = pResPred[x] - pRef[x];
          }
          pRef += pitchPixelsRef;
          pResPred += stepR;
      }
      sad = Hadamard_16s[blockSize-2](diffBuffR);
  }

  return sad;
}



#elif PIXBITS == 16

Ipp32s H264ENC_MAKE_NAME(SAD)(
    Ipp16u *pCur,
    Ipp32s pitchPixelsCur,
    Ipp16u *pRef,
    Ipp32s pitchPixelsRef,
    Ipp32s blockSize)
{
    Ipp32s sad;
    switch (blockSize) {
        case BS_16x16:
            sad = SAD16x16(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_16x8:
            sad = SAD16x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_8x16:
            sad = SAD8x16(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_8x8:
            sad = SAD8x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_8x4:
            sad = SAD8x4(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_4x8:
            sad = SAD4x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_4x4:
            sad = SAD4x4(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_4x2:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s d4 = pCur[2] - pRef[2];
            Ipp32s d5 = pCur[3] - pRef[3];
            Ipp32s d6 = pCur[pitchPixelsCur+2] - pRef[pitchPixelsRef+2];
            Ipp32s d7 = pCur[pitchPixelsCur+3] - pRef[pitchPixelsRef+3];
            sad = ABS(d0) + ABS(d1) + ABS(d2) + ABS(d3) + ABS(d4) + ABS(d5) + ABS(d6) + ABS(d7);
            break;
            }
        case BS_2x4:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            pCur += pitchPixelsCur * 2;
            pRef += pitchPixelsRef * 2;
            Ipp32s d4 = pCur[0] - pRef[0];
            Ipp32s d5 = pCur[1] - pRef[1];
            Ipp32s d6 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d7 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            sad = ABS(d0) + ABS(d1) + ABS(d2) + ABS(d3) + ABS(d4) + ABS(d5) + ABS(d6) + ABS(d7);
            break;
            }
        case BS_2x2:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            sad = ABS(d0) + ABS(d1) + ABS(d2) + ABS(d3);
            break;
            }
        default:
            sad = 0;
            break;
    }
    return sad;
}


Ipp32s H264ENC_MAKE_NAME(SATD)(
    Ipp16u *pCur,
    Ipp32s pitchPixelsCur,
    Ipp16u *pRef,
    Ipp32s pitchPixelsRef,
    Ipp32s blockSize)
{
    Ipp32s sad;
    switch (blockSize) {
        case BS_16x16:
            sad = SATD16x16(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_16x8:
            sad = SATD16x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_8x16:
            sad = SATD8x16(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_8x8:
            sad = SATD8x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_8x4:
            sad = SATD8x4(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_4x8:
            sad = SATD4x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_4x4:
            sad = SATD4x4(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_4x2:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s a0 = d0 + d2;
            Ipp32s a1 = d1 + d3;
            Ipp32s a2 = d0 - d2;
            Ipp32s a3 = d1 - d3;
            sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            d0 = pCur[2] - pRef[2];
            d1 = pCur[3] - pRef[3];
            d2 = pCur[pitchPixelsCur+2] - pRef[pitchPixelsRef+2];
            d3 = pCur[pitchPixelsCur+3] - pRef[pitchPixelsRef+3];
            a0 = d0 + d2;
            a1 = d1 + d3;
            a2 = d0 - d2;
            a3 = d1 - d3;
            sad += ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            break;
            }
        case BS_2x4:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s a0 = d0 + d2;
            Ipp32s a1 = d1 + d3;
            Ipp32s a2 = d0 - d2;
            Ipp32s a3 = d1 - d3;
            sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            pCur += pitchPixelsCur * 2;
            pRef += pitchPixelsRef * 2;
            d0 = pCur[0] - pRef[0];
            d1 = pCur[1] - pRef[1];
            d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            a0 = d0 + d2;
            a1 = d1 + d3;
            a2 = d0 - d2;
            a3 = d1 - d3;
            sad += ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            break;
            }
        case BS_2x2:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s a0 = d0 + d2;
            Ipp32s a1 = d1 + d3;
            Ipp32s a2 = d0 - d2;
            Ipp32s a3 = d1 - d3;
            sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            break;
            }
        default:
            sad = 0;
            break;
    }
    return sad;
}

#endif //PIXBITS

#else //TABLE_FUNC

Ipp32s H264ENC_MAKE_NAME(SAD)(
    PIXTYPE *pCur,
    Ipp32s pitchPixelsCur,
    PIXTYPE *pRef,
    Ipp32s pitchPixelsRef,
    Ipp32s blockSize)
{
    Ipp32s sad;
    switch (blockSize) {
        case BS_16x16:
            //sad = SAD16x16(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            ippiSAD16x16_8u32s(pCur, pitchPixelsCur, pRef, pitchPixelsRef, &sad, 0);
            break;
        case BS_16x8:
            //sad = SAD16x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            ippiSAD16x8_8u32s_C1R(pCur, pitchPixelsCur, pRef, pitchPixelsRef, &sad, 0);
            break;
        case BS_8x16:
            //sad = SAD8x16(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            ippiSAD8x16_8u32s_C1R(pCur, pitchPixelsCur, pRef, pitchPixelsRef, &sad, 0);
            break;
        case BS_8x8:
            //sad = SAD8x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            ippiSAD8x8_8u32s_C1R(pCur, pitchPixelsCur, pRef, pitchPixelsRef, &sad, 0);
            break;
        case BS_8x4:
            //sad = SAD8x4(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            ippiSAD8x4_8u32s_C1R(pCur, pitchPixelsCur, pRef, pitchPixelsRef, &sad, 0);
            break;
        case BS_4x8:
            //sad = SAD4x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            ippiSAD4x8_8u32s_C1R(pCur, pitchPixelsCur, pRef, pitchPixelsRef, &sad, 0);
            break;
        case BS_4x4:
            //sad = SAD4x4(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            ippiSAD4x4_8u32s(pCur, pitchPixelsCur, pRef, pitchPixelsRef, &sad, 0);
            break;
        case BS_4x2:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s d4 = pCur[2] - pRef[2];
            Ipp32s d5 = pCur[3] - pRef[3];
            Ipp32s d6 = pCur[pitchPixelsCur+2] - pRef[pitchPixelsRef+2];
            Ipp32s d7 = pCur[pitchPixelsCur+3] - pRef[pitchPixelsRef+3];
            sad = ABS(d0) + ABS(d1) + ABS(d2) + ABS(d3) + ABS(d4) + ABS(d5) + ABS(d6) + ABS(d7);
            break;
            }
        case BS_2x4:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            pCur += pitchPixelsCur * 2;
            pRef += pitchPixelsRef * 2;
            Ipp32s d4 = pCur[0] - pRef[0];
            Ipp32s d5 = pCur[1] - pRef[1];
            Ipp32s d6 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d7 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            sad = ABS(d0) + ABS(d1) + ABS(d2) + ABS(d3) + ABS(d4) + ABS(d5) + ABS(d6) + ABS(d7);
            break;
            }
        case BS_2x2:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            sad = ABS(d0) + ABS(d1) + ABS(d2) + ABS(d3);
            break;
            }
        default:
            sad = 0;
            break;
    }
    return sad;
}


Ipp32s H264ENC_MAKE_NAME(SATD)(
    PIXTYPE *pCur,
    Ipp32s pitchPixelsCur,
    PIXTYPE *pRef,
    Ipp32s pitchPixelsRef,
    Ipp32s blockSize)
{
    Ipp32s sad;
    switch (blockSize) {
        case BS_16x16:
            sad = SATD16x16(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_16x8:
            sad = SATD16x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_8x16:
            sad = SATD8x16(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_8x8:
            sad = SATD8x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_8x4:
            sad = SATD8x4(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_4x8:
            sad = SATD4x8(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_4x4:
            sad = SATD4x4(pCur, pitchPixelsCur, pRef, pitchPixelsRef);
            break;
        case BS_4x2:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s a0 = d0 + d2;
            Ipp32s a1 = d1 + d3;
            Ipp32s a2 = d0 - d2;
            Ipp32s a3 = d1 - d3;
            sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            d0 = pCur[2] - pRef[2];
            d1 = pCur[3] - pRef[3];
            d2 = pCur[pitchPixelsCur+2] - pRef[pitchPixelsRef+2];
            d3 = pCur[pitchPixelsCur+3] - pRef[pitchPixelsRef+3];
            a0 = d0 + d2;
            a1 = d1 + d3;
            a2 = d0 - d2;
            a3 = d1 - d3;
            sad += ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            break;
            }
        case BS_2x4:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s a0 = d0 + d2;
            Ipp32s a1 = d1 + d3;
            Ipp32s a2 = d0 - d2;
            Ipp32s a3 = d1 - d3;
            sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            pCur += pitchPixelsCur * 2;
            pRef += pitchPixelsRef * 2;
            d0 = pCur[0] - pRef[0];
            d1 = pCur[1] - pRef[1];
            d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            a0 = d0 + d2;
            a1 = d1 + d3;
            a2 = d0 - d2;
            a3 = d1 - d3;
            sad += ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            break;
            }
        case BS_2x2:
            {
            Ipp32s d0 = pCur[0] - pRef[0];
            Ipp32s d1 = pCur[1] - pRef[1];
            Ipp32s d2 = pCur[pitchPixelsCur+0] - pRef[pitchPixelsRef+0];
            Ipp32s d3 = pCur[pitchPixelsCur+1] - pRef[pitchPixelsRef+1];
            Ipp32s a0 = d0 + d2;
            Ipp32s a1 = d1 + d3;
            Ipp32s a2 = d0 - d2;
            Ipp32s a3 = d1 - d3;
            sad = ABS(a0 + a1) + ABS(a0 - a1) + ABS(a2 + a3) + ABS(a2 - a3);
            break;
            }
        default:
            sad = 0;
            break;
    }
    return sad;
}

#endif // TABLE_FUNC


static void H264ENC_MAKE_NAME(ME_IntPel)(
    ME_InfType *meInfo)
{
    Ipp32s cSAD, bSAD, mvSAD, xL, xR, yT, yB, rX, rY, bMVX, bMVY, blockSize, i, j;
    PIXTYPE *pCur = meInfo->pCur;
#ifndef NO_PADDING
    PIXTYPE *pRef = meInfo->pRef;
#endif
    Ipp32s   pitchPixels = meInfo->pitchPixels;
    H264MotionVector predictedMV = meInfo->predictedMV;
    Ipp16s* pRDQM = meInfo->pRDQM;
    Ipp8u* pBitsX = BitsForMV + BITSFORMV_OFFSET - predictedMV.mvx;
    Ipp8u* pBitsY = BitsForMV + BITSFORMV_OFFSET - predictedMV.mvy;
    Ipp8u* pBitsXpr = BitsForMV + BITSFORMV_OFFSET - meInfo->scPredMV.mvx;
    Ipp8u* pBitsYpr = BitsForMV + BITSFORMV_OFFSET - meInfo->scPredMV.mvy;
    Ipp32s tempMVX = 0, tempMVY = 0, tempSAD = 0, temprX = 0, temprY = 0;
    Ipp32s lB, rB, tB, bB, /*lrB,*/ tbB;
    Ipp32s border_width_LR, border_width_TB;
    static Ipp8u pass_counter;
    Ipp8u prevBorder = 0;
    Ipp32s corrX = 0, corrY = 0;
    Ipp16s *resPredBuf = meInfo->pResY;

    blockSize = meInfo->block.width + (meInfo->block.height >> 2);
    bMVX = meInfo->bestMV.mvx;
    bMVY = meInfo->bestMV.mvy;
    bSAD = meInfo->bestSAD;
    if (bSAD <= meInfo->threshold)
        goto end;

#ifdef NO_PADDING
    PIXTYPE interpBlockBuf[256];
    meInfo->interpolateInfo.pDst[0] = interpBlockBuf;
    meInfo->interpolateInfo.dstStep = 16;
    meInfo->interpolateInfo.sizeBlock = meInfo->block;
#endif // NO_PADDING
//========================= H264_PRESEARCH ===================================

    if(meInfo->flags & ANALYSE_ME_PRESEARCH)
    {

//------------------ log 6x6 -----------------------------

        rX = 3;
        rY = 3;
        xL = MAX(meInfo->xMin, bMVX - rX);
        xR = MIN(meInfo->xMax, bMVX + rX);
        yT = MAX(meInfo->yMin, bMVY - rY);
        yB = MIN(meInfo->yMax, bMVY + rY);
        Ipp32s k, l, m, n, xPos, yPos;
        static const Ipp32s bdJ[9] = {0, -1, 0, 1, 1, 1, 0, -1, -1}, bdI[9] = {0, -1, -1, -1, 0, 1, 1, 1, 0};
        static const Ipp32s bdN[9] = {8, 5, 3, 5, 3, 5, 3, 5, 3}, bdA[9][8] = {{1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 7, 8, 0, 0, 0}, {1, 2, 3, 0, 0, 0, 0, 0}, {1, 2, 3, 4, 5, 0, 0, 0}, {3, 4, 5, 0, 0, 0, 0, 0}, {3, 4, 5, 6, 7, 0, 0, 0}, {5, 6, 7, 0, 0, 0, 0, 0}, {5, 6, 7, 8, 1, 0, 0, 0}, {7, 8, 1, 0, 0, 0, 0, 0}};
        xPos = bMVX;
        yPos = bMVY;
        l = 0;
        for (;;) {
            n = l;
            l = 0;
            for (m = 0; m < bdN[n]; m ++) {
                k = bdA[n][m];
                j = xPos + bdJ[k] * rX;
                i = yPos + bdI[k] * rY;
                if (j >= xL && j <= xR && i >= yT && i <= yB) {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                        l = k;
                        bSAD = cSAD;
                        bMVX = j;
                        bMVY = i;
                        if (bSAD <= meInfo->threshold)
                            goto end;
                    }
                }
            }
            if (l == 0) {
                rX >>= 1;
                rY >>= 1;
                if (rX == 0 || rY == 0)
                    break;
            } else {
                xPos += bdJ[l] * rX;
                yPos += bdI[l] * rY;
            }
        }

//------------------ log 6x6 (end) -----------------------

        if ((abs(meInfo->bestMV.mvx - bMVX) <= 1) && (abs(meInfo->bestMV.mvy - bMVY) <= 1))
            goto end;
    }


//========================= H264_PRESEARCH (END) =============================

    xL = MAX(meInfo->xMin, bMVX - meInfo->rX);
    xR = MIN(meInfo->xMax, bMVX + meInfo->rX);
    yT = MAX(meInfo->yMin, bMVY - meInfo->rY);
    yB = MIN(meInfo->yMax, bMVY + meInfo->rY);
    rX = meInfo->rX;
    rY = meInfo->rY;

//========================= CONTINUED_SEARCH =================================

    if (meInfo->flags & ANALYSE_ME_CONTINUED_SEARCH)
    {
        temprX = rX;
        temprY = rY;
        tempSAD = bSAD;
        lB = 0;
        rB = 0;
        tB = 0;
        bB = 0;
        //lrB = 0;
        tbB = 0;
        border_width_LR = 0;
        border_width_TB = 0;
        prevBorder = 0;
        corrX = 0;
        corrY = 0;
    }

search_again:

//========================= CONTINUED_SEARCH (END) ===========================

    switch (meInfo->searchAlgo & 15) {
    case MV_SEARCH_TYPE_FTS:
        {
            enum TriangleType
            {
                T00, T01, T02, T03,
                T10, T11, T12, T13, T14, T15,
                T20, T21, T22, T23, T24, T25,
                T30, T31, T32, T33, T34, T35
            };

            enum Vertices
            {
                O, A, B
            };

            enum OperationResults
            {
                vx, vy, Ttype
            };

            struct Triangle
            {
                H264MotionVector V[3];
                TriangleType type;
                Ipp32s SADV[3];
                Ipp8u h;
                Ipp8u l;
            };

            //Vertex table [triangle_type][vertex][vector from V0]

            Ipp16s VertexTable[16][3][2] = {
                { { 0, 0},  { 0, 1},  { 1, 0} },  // T00
                { { 0, 0},  {-1, 0},  { 0, 1} },  // T01
                { { 0, 0},  { 0,-1},  {-1, 0} },  // T02
                { { 0, 0},  { 1, 0},  { 0,-1} },  // T03

                { { 0, 0},  { 2, 0},  { 1,-2} },  // T10
                { { 0, 0},  { 1, 2},  { 2, 0} },  // T11
                { { 0, 0},  {-1, 2},  { 1, 2} },  // T12
                { { 0, 0},  {-2, 0},  {-1, 2} },  // T13
                { { 0, 0},  {-1,-2},  {-2, 0} },  // T14
                { { 0, 0},  { 1,-2},  {-1,-2} },  // T15

                { { 0, 0},  { 4, 0},  { 2,-4} },  // T20
                { { 0, 0},  { 2, 4},  { 4, 0} },  // T21
                { { 0, 0},  {-2, 4},  { 2, 4} },  // T22
                { { 0, 0},  {-4, 0},  {-2, 4} },  // T23
                { { 0, 0},  {-2,-4},  {-4, 0} },  // T24
                { { 0, 0},  { 2,-4},  {-2,-4} },  // T25
            };

            //Reflection table [triangle_type][vertex][result]

            Ipp16s ReflectionTable[16][3][3] = {
                { { 1, 1, T02},  { 0,-2, T03},  {-2, 0, T01} },  // T00
                { {-1, 1, T03},  { 2, 0, T00},  { 0,-2, T02} },  // T01
                { {-1,-1, T00},  { 0, 2, T01},  { 2, 0, T03} },  // T02
                { { 1,-1, T01},  {-2, 0, T02},  { 0, 2, T00} },  // T03

                { { 3,-2, T13},  {-3,-2, T15},  { 0, 4, T11} },  // T10
                { { 3, 2, T14},  { 0,-4, T10},  {-3, 2, T12} },  // T11
                { { 0, 4, T15},  { 3,-2, T11},  {-3,-2, T13} },  // T12
                { {-3, 2, T10},  { 3, 2, T12},  { 0,-4, T14} },  // T13
                { {-3,-2, T11},  { 0, 4, T13},  { 3,-2, T15} },  // T14
                { { 0,-4, T12},  {-3, 2, T14},  { 3, 2, T10} },  // T15

                { { 6,-4, T23},  {-6,-4, T25},  { 0, 8, T21} },  // T20
                { { 6, 4, T24},  { 0,-8, T20},  {-6, 4, T22} },  // T21
                { { 0, 8, T25},  { 6,-4, T21},  {-6,-4, T23} },  // T22
                { {-6, 4, T20},  { 6, 4, T22},  { 0,-8, T24} },  // T23
                { {-6,-4, T21},  { 0, 8, T23},  { 6,-4, T25} },  // T24
                { { 0,-8, T22},  {-6, 4, T24},  { 6, 4, T20} }   // T25
            };

            //Reflection table [triangle_type][vertex][result]

            Ipp16s ReflectionTable_V0[16][3][3] = {
                { { 1, 1, T02},  { 0, 0, T03},  { 0, 0, T01} },  // T00
                { {-1, 1, T03},  { 0, 0, T00},  { 0, 0, T02} },  // T01
                { {-1,-1, T00},  { 0, 0, T01},  { 0, 0, T03} },  // T02
                { { 1,-1, T01},  { 0, 0, T02},  { 0, 0, T00} },  // T03

                { { 3,-2, T13},  { 0, 0, T15},  { 0, 0, T11} },  // T10
                { { 3, 2, T14},  { 0, 0, T10},  { 0, 0, T12} },  // T11
                { { 0, 4, T15},  { 0, 0, T11},  { 0, 0, T13} },  // T12
                { {-3, 2, T10},  { 0, 0, T12},  { 0, 0, T14} },  // T13
                { {-3,-2, T11},  { 0, 0, T13},  { 0, 0, T15} },  // T14
                { { 0,-4, T12},  { 0, 0, T14},  { 0, 0, T10} },  // T15

                { { 6,-4, T23},  { 0, 0, T25},  { 0, 0, T21} },  // T20
                { { 6, 4, T24},  { 0, 0, T20},  { 0, 0, T22} },  // T21
                { { 0, 8, T25},  { 0, 0, T21},  { 0, 0, T23} },  // T22
                { {-6, 4, T20},  { 0, 0, T22},  { 0, 0, T24} },  // T23
                { {-6,-4, T21},  { 0, 0, T23},  { 0, 0, T25} },  // T24
                { { 0,-8, T22},  { 0, 0, T24},  { 0, 0, T20} }   // T25
            };

            //Reflection+expansion table [triangle_type before reflection][vertex][test point]

            Ipp16s ExpansionTable[16][3][3] = {
                { { 2, 2, T14},  { 0,-2, T12},  {-2, 0, T11} },  // T00
                { {-2, 2, T10},  { 2, 0, T13},  { 0,-2, T12} },  // T01
                { {-2,-2, T11},  { 0, 2, T15},  { 2, 0, T14} },  // T02
                { { 1,-2, T13},  {-2, 0, T10},  { 0, 2, T15} },  // T03

                { { 5,-3, T23},  {-3,-3, T25},  { 1, 4, T21} },  // T10
                { { 5, 3, T24},  { 1,-4, T20},  {-3, 3, T22} },  // T11
                { { 0, 6, T25},  { 4,-1, T21},  {-4,-1, T23} },  // T12
                { {-5, 3, T20},  { 3, 3, T22},  {-1,-4, T24} },  // T13
                { {-5,-3, T21},  {-1, 4, T23},  { 3,-3, T25} },  // T14
                { { 0,-6, T22},  {-4, 1, T24},  { 4, 1, T20} },  // T15

                { { 8,-5, T33},  {-4,-4, T35},  { 2, 5, T31} },  // T20
                { { 8, 5, T34},  { 2,-5, T30},  {-4, 4, T32} },  // T21
                { { 0, 9, T35},  { 6, 2, T31},  {-6,-2, T33} },  // T22
                { {-8, 5, T30},  { 4, 4, T32},  {-2,-5, T34} },  // T23
                { {-8,-5, T31},  {-2, 5, T33},  { 4,-4, T35} },  // T24
                { {-8, 5, T32},  {-6, 2, T34},  { 6,-2, T30} }   // T25
            };

            TriangleType ContractionTable[16] = {
                T00, T00, T00, T00, //T00 - T03 - no contraction
                /*T10->*/ T03, /*T11->*/ T00, /*T12->*/ T00, /*T13->*/ T01, /*T14->*/ T02, /*T15->*/ T02,
                /*T20->*/ T10, /*T21->*/ T11, /*T22->*/ T12, /*T23->*/ T13, /*T24->*/ T14, /*T25->*/ T15
            };

            Vertices Exchange[3] = {O, B, A};


            H264MotionVector Vd,Vr,Vt,Ve,Vmin;
            Ipp32s Kmax, K, ExpansionLevel, maxLevel, k, Vmin_SAD, temp_SAD;
            Ipp8u TranslationFlag, TrianInsWindFlag/*, BackReflectFlag*/;

            Triangle Tcurrent, Ttemp;

//==================== Initialization ========================
            Kmax = 25;
            K = 0;
            maxLevel = 2;
            TranslationFlag = false;
            TrianInsWindFlag = false;
            //BackReflectFlag = false;

            Vd.mvx = 0;
            Vd.mvy = 0;
            Vmin.mvx = (Ipp16s)bMVX;
            Vmin.mvy = (Ipp16s)bMVY;
            //Vmin_SAD = SAD_M(Vmin);
            SAD_M(Vmin, Vmin_SAD);

            // setting initial triangle
            Tcurrent.type = T00;
            ExpansionLevel = 0;
            Tcurrent.V[0].mvx = bMVX;
            Tcurrent.V[0].mvy = bMVY;
            Tcurrent.V[A].mvx = Tcurrent.V[0].mvx + VertexTable[Tcurrent.type][A][vx];
            Tcurrent.V[A].mvy = Tcurrent.V[0].mvy + VertexTable[Tcurrent.type][A][vy];
            Tcurrent.V[B].mvx = Tcurrent.V[0].mvx + VertexTable[Tcurrent.type][B][vx];
            Tcurrent.V[B].mvy = Tcurrent.V[0].mvy + VertexTable[Tcurrent.type][B][vy];

            if (VertOutWind(Tcurrent.V[0]) || VertOutWind(Tcurrent.V[A]) || VertOutWind(Tcurrent.V[B]))
                goto local_search;

            Tcurrent.SADV[0] = Vmin_SAD;
            //Tcurrent.SADV[A] = SAD_M(Tcurrent.V[A]);
            SAD_M(Tcurrent.V[A], Tcurrent.SADV[A]);
            //Tcurrent.SADV[B] = SAD_M(Tcurrent.V[B]);
            SAD_M(Tcurrent.V[B], Tcurrent.SADV[B]);

//==================== Initialization (END) ==================

            while (K <= Kmax && Vmin_SAD > meInfo->threshold)
            {
                Ipp32s MaxSAD, Vr_SAD, Ve_SAD, Vt_SAD;

                MaxSAD = MAX(MAX(Tcurrent.SADV[0],Tcurrent.SADV[A]),Tcurrent.SADV[B]);
                Tcurrent.h = Tcurrent.SADV[0] == MaxSAD ? 0 : Tcurrent.SADV[A] == MaxSAD ? A : B;
                if (Tcurrent.h == 0)
                    Tcurrent.l = Tcurrent.SADV[A] <= Tcurrent.SADV[B] ? A : B;
                else if (Tcurrent.h == A)
                    Tcurrent.l = Tcurrent.SADV[0] <= Tcurrent.SADV[B] ? 0 : B;
                else // if (Tcurrent.h == B)
                    Tcurrent.l = Tcurrent.SADV[0] <= Tcurrent.SADV[A] ? 0 : A;

                if (Tcurrent.SADV[Tcurrent.l] < Vmin_SAD)
                {
                    Vmin = Tcurrent.V[Tcurrent.l];
                    Vmin_SAD = Tcurrent.SADV[Tcurrent.l];
                }

                // Transalion flag == 1
                if (TranslationFlag)
                {
                    Vt.mvx = Tcurrent.V[Tcurrent.l].mvx + Vd.mvx;
                    Vt.mvy = Tcurrent.V[Tcurrent.l].mvy + Vd.mvy;
                    //Vt_SAD = SAD_M(Vt);
                    SAD_M(Vt, Vt_SAD);

                    // translation checkpoint positive
                    if (Vt_SAD < Tcurrent.SADV[Tcurrent.l] && Vt.mvx >= xL && Vt.mvx <= xR && Vt.mvy >= yT && Vt.mvy <= yB)
                    {
                        Ttemp.l = Tcurrent.l;
                        Ttemp.type = Tcurrent.type;
                        Ttemp.V[Ttemp.l].mvx = Vt.mvx;
                        Ttemp.V[Ttemp.l].mvy = Vt.mvy;
                        if (Ttemp.l == 0)
                        {
                            Ttemp.V[A].mvx = Ttemp.V[0].mvx + VertexTable[Ttemp.type][A][vx];
                            Ttemp.V[A].mvy = Ttemp.V[0].mvy + VertexTable[Ttemp.type][A][vy];
                            Ttemp.V[B].mvx = Ttemp.V[0].mvx + VertexTable[Ttemp.type][B][vx];
                            Ttemp.V[B].mvy = Ttemp.V[0].mvy + VertexTable[Ttemp.type][B][vy];
                            TrianInsWindFlag = VertInWind(Ttemp.V[A]) && VertInWind(Ttemp.V[B]);
                        }
                        else
                        {
                            Ttemp.V[0].mvx = Vt.mvx - VertexTable[Ttemp.type][Ttemp.l][vx];
                            Ttemp.V[0].mvy = Vt.mvy - VertexTable[Ttemp.type][Ttemp.l][vy];
                            Ttemp.V[Exchange[Ttemp.l]].mvx = Ttemp.V[0].mvx + VertexTable[Ttemp.type][Exchange[Ttemp.l]][vx];
                            Ttemp.V[Exchange[Ttemp.l]].mvy = Ttemp.V[0].mvy + VertexTable[Ttemp.type][Exchange[Ttemp.l]][vy];
                            TrianInsWindFlag = VertInWind(Ttemp.V[0]) && VertInWind(Ttemp.V[Exchange[Ttemp.l]]);
                        }
                        // all vertices of triangle are inside the search window
                        if (TrianInsWindFlag)
                        {
//==================== updating Tcurrent, Vmin after translation =============
                            TrianInsWindFlag = false;
                            Tcurrent.V[Tcurrent.l].mvx = Vt.mvx;
                            Tcurrent.V[Tcurrent.l].mvy = Vt.mvy;
                            Tcurrent.SADV[Tcurrent.l] = Vt_SAD;
                            if (Tcurrent.l == 0)
                            {
                                Tcurrent.V[A].mvx = Ttemp.V[A].mvx;
                                Tcurrent.V[A].mvy = Ttemp.V[A].mvy;
                                Tcurrent.V[B].mvx = Ttemp.V[B].mvx;
                                Tcurrent.V[B].mvy = Ttemp.V[B].mvy;
                                //Tcurrent.SADV[A] = SAD_M(Tcurrent.V[A]);
                                SAD_M(Tcurrent.V[A], Tcurrent.SADV[A]);
                                //Tcurrent.SADV[B] = SAD_M(Tcurrent.V[B]);
                                SAD_M(Tcurrent.V[B], Tcurrent.SADV[B]);
                            }
                            else
                            {
                                Tcurrent.V[0].mvx = Ttemp.V[0].mvx;
                                Tcurrent.V[0].mvy = Ttemp.V[0].mvy;
                                Tcurrent.V[Exchange[Tcurrent.l]].mvx = Ttemp.V[Exchange[Tcurrent.l]].mvx;
                                Tcurrent.V[Exchange[Tcurrent.l]].mvy = Ttemp.V[Exchange[Tcurrent.l]].mvy;
                                //Tcurrent.SADV[0] = SAD_M(Tcurrent.V[0]);
                                SAD_M(Tcurrent.V[0], Tcurrent.SADV[0]);
                                //Tcurrent.SADV[Exchange[Tcurrent.l]] = SAD_M(Tcurrent.V[Exchange[Tcurrent.l]]);
                                SAD_M(Tcurrent.V[Exchange[Tcurrent.l]], Tcurrent.SADV[Exchange[Tcurrent.l]]);

                            }
                        } // all vertices of triangle are inside the search window (end)
                        else
                        {
                            TranslationFlag = 0;
                        }
//==================== updating Tcurrent after translation (END) =======

                        // updating Vmin if Vt is inside search window and Vt_SAD < Vmin_SAD
                        if (Vt_SAD < Vmin_SAD)
                        {
                            Vmin = Vt;
                            Vmin_SAD = Vt_SAD;
                        }
                    } // translation checkpoint positive (end)
                    // transaltion negative
                    else
                    {
                        TranslationFlag = 0;
                    }
                    K++;
                }
                // Transalion flag == 0
                else
                {
                    Vr.mvx = Tcurrent.V[Tcurrent.h].mvx + ReflectionTable[Tcurrent.type][Tcurrent.h][vx];
                    Vr.mvy = Tcurrent.V[Tcurrent.h].mvy + ReflectionTable[Tcurrent.type][Tcurrent.h][vy];
                    //Vr_SAD = SAD_M(Vr);
                    SAD_M(Vr, Vr_SAD);


                    // reflection positive
                    if (Vr_SAD < Tcurrent.SADV[Tcurrent.h] && Vr.mvx >= xL && Vr.mvx <= xR && Vr.mvy >= yT && Vr.mvy <= yB)
                    {
                        // trying expansion
                        if (ExpansionLevel < maxLevel)
                        {
                            Ve.mvx = Tcurrent.V[0].mvx + ExpansionTable[Tcurrent.type][Tcurrent.h][vx];
                            Ve.mvy = Tcurrent.V[0].mvy + ExpansionTable[Tcurrent.type][Tcurrent.h][vy];
                            //Ve_SAD = SAD_M(Ve);
                            SAD_M(Ve, Ve_SAD);

                            // expansion checkpoint positive
                            if (Ve_SAD < Vr_SAD && Ve.mvx >= xL && Ve.mvx <= xR && Ve.mvy >= yT && Ve.mvy <= yB)
                            {
                                Ttemp.type = Tcurrent.type;
                                Ttemp.h = Tcurrent.h;
                                if (Ttemp.type > 3)  // Tcurrent level > 0
                                {
                                    Ttemp.type = TriangleType(ExpansionTable[Ttemp.type][Ttemp.h][Ttype]);
                                    Ttemp.V[Exchange[Ttemp.h]].mvx = Ve.mvx;
                                    Ttemp.V[Exchange[Ttemp.h]].mvy = Ve.mvy;
                                    if (Ttemp.h == 0)
                                    {
                                        Ttemp.V[A].mvx = VertexTable[Ttemp.type][A][vx];
                                        Ttemp.V[A].mvy = VertexTable[Ttemp.type][A][vy];
                                        Ttemp.V[B].mvx = VertexTable[Ttemp.type][B][vx];
                                        Ttemp.V[B].mvy = VertexTable[Ttemp.type][B][vy];
                                        TrianInsWindFlag = VertInWind(Ttemp.V[A]) && VertInWind(Ttemp.V[B]);
                                    }
                                    else
                                    {
                                        Ttemp.V[0].mvx = Ve.mvx - VertexTable[Ttemp.type][Exchange[Ttemp.h]][vx];
                                        Ttemp.V[0].mvy = Ve.mvy - VertexTable[Ttemp.type][Exchange[Ttemp.h]][vy];
                                        Ttemp.V[Ttemp.h].mvx = Ttemp.V[0].mvx + VertexTable[Ttemp.type][Ttemp.h][vx];
                                        Ttemp.V[Ttemp.h].mvy = Ttemp.V[0].mvy + VertexTable[Ttemp.type][Ttemp.h][vy];
                                        TrianInsWindFlag = VertInWind(Ttemp.V[0]) && VertInWind(Ttemp.V[Ttemp.h]);
                                    }
                                }
                                else   // Tcurrent level == 0
                                {
                                    Ttemp.type = TriangleType(ExpansionTable[Ttemp.type][Ttemp.h][Ttype]);
                                    Ttemp.V[0].mvx = Ve.mvx;
                                    Ttemp.V[0].mvy = Ve.mvy;
                                    Ttemp.V[A].mvx = Ttemp.V[0].mvx + VertexTable[Ttemp.type][A][vx];
                                    Ttemp.V[A].mvy = Ttemp.V[0].mvy + VertexTable[Ttemp.type][A][vy];
                                    Ttemp.V[B].mvx = Ttemp.V[0].mvx + VertexTable[Ttemp.type][B][vx];
                                    Ttemp.V[B].mvy = Ttemp.V[0].mvy + VertexTable[Ttemp.type][B][vy];
                                    TrianInsWindFlag = VertInWind(Ttemp.V[A]) && VertInWind(Ttemp.V[B]);
                                }

                                // expansion positive
                                if (TrianInsWindFlag)
                                {
//==================== updating Tcurrent after expansion ===============
                                    TrianInsWindFlag = false;
                                    TranslationFlag = true;
                                    ExpansionLevel++;
                                    if (Tcurrent.type > 3)  // Tcurrent level > 0
                                    {
                                        Tcurrent.type = Ttemp.type;
                                        Tcurrent.V[Exchange[Ttemp.h]].mvx = Ve.mvx;
                                        Tcurrent.V[Exchange[Ttemp.h]].mvy = Ve.mvy;
                                        Tcurrent.SADV[Exchange[Tcurrent.h]] = Ve_SAD;
                                        if (Tcurrent.h == 0)
                                        {
                                            Tcurrent.V[A].mvx = Ttemp.V[A].mvx;
                                            Tcurrent.V[A].mvy = Ttemp.V[A].mvy;
                                            Tcurrent.V[B].mvx = Ttemp.V[B].mvx;
                                            Tcurrent.V[B].mvy = Ttemp.V[B].mvy;
                                            //Tcurrent.SADV[A] = SAD_M(Tcurrent.V[A]);
                                            SAD_M(Tcurrent.V[A], Tcurrent.SADV[A]);
                                            //Tcurrent.SADV[B] = SAD_M(Tcurrent.V[B]);
                                            SAD_M(Tcurrent.V[B], Tcurrent.SADV[B]);
                                        }
                                        else
                                        {
                                            Tcurrent.V[0].mvx = Ttemp.V[0].mvx;
                                            Tcurrent.V[0].mvy = Ttemp.V[0].mvy;
                                            Tcurrent.V[Ttemp.h].mvx = Ttemp.V[Ttemp.h].mvx;
                                            Tcurrent.V[Ttemp.h].mvy = Ttemp.V[Ttemp.h].mvy;
                                            //Tcurrent.SADV[0] = SAD_M(Tcurrent.V[0]);
                                            SAD_M(Tcurrent.V[0], Tcurrent.SADV[0]);
                                            //Tcurrent.SADV[Tcurrent.h] = SAD_M(Tcurrent.V[Tcurrent.h]);
                                            SAD_M(Tcurrent.V[Tcurrent.h], Tcurrent.SADV[Tcurrent.h]);

                                        }
                                    }
                                    else   // Tcurrent level == 0
                                    {
                                        Tcurrent.type = Ttemp.type;
                                        Tcurrent.V[0].mvx = Ve.mvx;
                                        Tcurrent.V[0].mvy = Ve.mvy;
                                        Tcurrent.V[A].mvx = Ttemp.V[A].mvx;
                                        Tcurrent.V[A].mvy = Ttemp.V[A].mvy;
                                        Tcurrent.V[B].mvx = Ttemp.V[B].mvx;
                                        Tcurrent.V[B].mvy = Ttemp.V[B].mvy;
                                        Tcurrent.SADV[0] = Ve_SAD;
                                        //Tcurrent.SADV[A] = SAD_M(Tcurrent.V[A]);
                                        SAD_M(Tcurrent.V[A], Tcurrent.SADV[A]);
                                        //Tcurrent.SADV[B] = SAD_M(Tcurrent.V[B]);
                                        SAD_M(Tcurrent.V[B], Tcurrent.SADV[B]);
                                    }
                                    Vd.mvx = Ve.mvx - Vr.mvx;
                                    Vd.mvy = Ve.mvy - Vr.mvy;
//==================== updating Tcurrent after expansion (END) =========
                                } // all vertices of triangle are inside the search window (end)
                                else
                                {
                                    if (Ve_SAD < Vmin_SAD)
                                    {
                                        Vmin = Ve;
                                        Vmin_SAD = Ve_SAD;
                                    }
                                    goto reflect;
                                }
                                if (Ve_SAD < Vmin_SAD)
                                {
                                    Vmin = Ve;
                                    Vmin_SAD = Ve_SAD;
                                }
                            } // expansion checkpoint positive (end)

                            // expansion nagative
                            else
                            {
                                goto reflect;
                            }
                        }
                        // don't try expansion due to ExpansionLevel == MAX
                        else
                        {
//==================== updating Tcurrent, Vmin after reflection ==============
reflect:
                            Tcurrent.V[0].mvx += ReflectionTable_V0[Tcurrent.type][Tcurrent.h][vx];
                            Tcurrent.V[0].mvy += ReflectionTable_V0[Tcurrent.type][Tcurrent.h][vy];
                            Tcurrent.type = TriangleType(ReflectionTable[Tcurrent.type][Tcurrent.h][Ttype]);

                            Tcurrent.V[A].mvx = Tcurrent.V[0].mvx + VertexTable[Tcurrent.type][A][vx];
                            Tcurrent.V[A].mvy = Tcurrent.V[0].mvy + VertexTable[Tcurrent.type][A][vy];
                            Tcurrent.V[B].mvx = Tcurrent.V[0].mvx + VertexTable[Tcurrent.type][B][vx];
                            Tcurrent.V[B].mvy = Tcurrent.V[0].mvy + VertexTable[Tcurrent.type][B][vy];

                            if (Tcurrent.h)
                            {
                                Tcurrent.SADV[Tcurrent.h] = Tcurrent.SADV[Exchange[Tcurrent.h]];
                            }
                            else
                            {
                                temp_SAD = Tcurrent.SADV[A];
                                Tcurrent.SADV[A] = Tcurrent.SADV[B];
                                Tcurrent.SADV[B] = temp_SAD;
                            }
                            Tcurrent.SADV[Exchange[Tcurrent.h]] = Vr_SAD;

                            if (Vr_SAD < Vmin_SAD)
                            {
                                Vmin = Vr;
                                Vmin_SAD = Vr_SAD;
                            }
                        }
                        K++;
//==================== updating Tcurrent, Vmin after reflection (END) ========
                    }
                    // reflection negative (perform contraction)
                    else
                    {
                        if (ExpansionLevel)
                        {
                            Ttemp.type = ContractionTable[Tcurrent.type];
                            Ttemp.l = Tcurrent.l;
                            Ttemp.V[Ttemp.l].mvx = Tcurrent.V[Tcurrent.l].mvx;
                            Ttemp.V[Ttemp.l].mvy = Tcurrent.V[Tcurrent.l].mvy;
                            if (Ttemp.l == 0)
                            {
                                Ttemp.V[A].mvx = Ttemp.V[0].mvx + VertexTable[Ttemp.type][A][vx];
                                Ttemp.V[A].mvy = Ttemp.V[0].mvy + VertexTable[Ttemp.type][A][vy];
                                Ttemp.V[B].mvx = Ttemp.V[0].mvx + VertexTable[Ttemp.type][B][vx];
                                Ttemp.V[B].mvy = Ttemp.V[0].mvy + VertexTable[Ttemp.type][B][vy];
                                TrianInsWindFlag = VertInWind(Ttemp.V[A]) && VertInWind(Ttemp.V[B]);
                            }
                            else
                            {
                                Ttemp.V[0].mvx = Ttemp.V[Ttemp.l].mvx - VertexTable[Ttemp.type][Ttemp.l][vx];
                                Ttemp.V[0].mvy = Ttemp.V[Ttemp.l].mvy - VertexTable[Ttemp.type][Ttemp.l][vy];
                                Ttemp.V[Exchange[Ttemp.l]].mvx = Ttemp.V[0].mvx + VertexTable[Ttemp.type][Exchange[Ttemp.l]][vx];
                                Ttemp.V[Exchange[Ttemp.l]].mvy = Ttemp.V[0].mvy + VertexTable[Ttemp.type][Exchange[Ttemp.l]][vy];
                                TrianInsWindFlag = VertInWind(Ttemp.V[0]) && VertInWind(Ttemp.V[Exchange[Ttemp.l]]);
                            }
                            // contraction positive
                            if (TrianInsWindFlag)
                            {
                                TrianInsWindFlag = false;
                                if (Tcurrent.l == 0)
                                {
                                    Tcurrent.type = Ttemp.type;
                                    Tcurrent.V[A].mvx = Ttemp.V[A].mvx;
                                    Tcurrent.V[A].mvy = Ttemp.V[A].mvy;
                                    Tcurrent.V[B].mvx = Ttemp.V[B].mvx;
                                    Tcurrent.V[B].mvy = Ttemp.V[B].mvy;
                                    //Tcurrent.SADV[A] = SAD_M(Tcurrent.V[A]);
                                    SAD_M(Tcurrent.V[A], Tcurrent.SADV[A]);
                                    //Tcurrent.SADV[B] = SAD_M(Tcurrent.V[B]);
                                    SAD_M(Tcurrent.V[B], Tcurrent.SADV[B]);
                                }
                                else
                                {
                                    Tcurrent.type = Ttemp.type;
                                    Tcurrent.V[0].mvx = Ttemp.V[0].mvx;
                                    Tcurrent.V[0].mvy = Ttemp.V[0].mvy;
                                    Tcurrent.V[Exchange[Tcurrent.l]].mvx = Ttemp.V[Exchange[Tcurrent.l]].mvx;
                                    Tcurrent.V[Exchange[Tcurrent.l]].mvy = Ttemp.V[Exchange[Tcurrent.l]].mvy;
                                    //Tcurrent.SADV[0] = SAD_M(Tcurrent.V[0]);
                                    SAD_M(Tcurrent.V[0], Tcurrent.SADV[0]);
                                    //Tcurrent.SADV[Exchange[Tcurrent.l]] = SAD_M(Tcurrent.V[Exchange[Tcurrent.l]]);
                                    SAD_M(Tcurrent.V[Exchange[Tcurrent.l]], Tcurrent.SADV[Exchange[Tcurrent.l]]);
                                }
                            }
                            else
                                goto local_search;
                            ExpansionLevel--;
                        }
                        else
                        {
                            goto local_search;
                        }
                    }
                }
            }

local_search:
            bMVX = Vmin.mvx;
            bMVY = Vmin.mvy;
            bSAD = Vmin_SAD;

            static const Ipp32s bdJss[8] = {0,   1, 1, 1, 0, -1, -1, -1};
            static const Ipp32s bdIss[8] = {-1, -1, 0, 1, 1,  1,  0, -1};

            for (k = 0; k < 8; k++)
            {
                j = Vmin.mvx + bdJss[k];
                i = Vmin.mvy + bdIss[k];

                if (j >= xL && j <= xR && i >= yT && i <= yB)
                {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    temp_SAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      temp_SAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                    else
                      temp_SAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    temp_SAD += mvSAD;
                    if (temp_SAD < bSAD) {
                        bSAD = temp_SAD;
                        bMVX = j;
                        bMVY = i;
                        if (bSAD <= meInfo->threshold)
                        {
                            goto end;
                        }
                    }
                }
            }

        }
        break;
//================ H264_FTS (END) ============================================
    case MV_SEARCH_TYPE_UMH:
        {
            static const Ipp32s ConvergenceThreshold = 1000;
            static const Ipp32s CrossSearchThreshold1 = 800;
            static const Ipp32s CrossSearchThreshold2 = 7000;
            static const Ipp32s bdJss[8] = {0,   1, 1, 1, 0, -1, -1, -1};
            static const Ipp32s bdIss[8] = {-1, -1, 0, 1, 1,  1,  0, -1};
            Ipp32s sr = MAX(rX,rY);

            Ipp32s xPos, yPos, n, k, l;
            Ipp8u block_id = 0;

            switch (meInfo->block.width + meInfo->block.height)
            {
            case 32:
                block_id = 0;
                break;
            case 24:
                block_id = 1;
                break;
            case 16:
                block_id = 2;
                break;
            case 12:
                block_id = 3;
                break;
            case 8:
                block_id = 4;
                break;
            }

            if (meInfo->residualPredictionFlag || meInfo->useScPredMV) // SVC case
            {
                if (block_id == 4) goto small_mode_svc;

                if (bSAD < (ConvergenceThreshold >> block_id)) // satisfy converge condition?
                    goto end;

                xPos = bMVX;
                yPos = bMVY;
                //==================== local search around best candidate ====================

                for (k = 0; k < 8; k++)
                {
                    j = xPos + bdJss[k];
                    i = yPos + bdIss[k];
                    if (j >= xL && j <= xR && i >= yT && i <= yB)
                    {
#ifdef NO_PADDING
                        meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                        meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                        if (meInfo->residualPredictionFlag)
                          cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                        else
                          cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                        MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                        cSAD += mvSAD;
                        if (cSAD < bSAD) {
                            bSAD = cSAD;
                            bMVX = j;
                            bMVY = i;
                            if (bSAD <= meInfo->threshold)
                            {
                                goto end;
                            }
                        }
                    }
                }

//==================== local search around best candidate (END) ==============

                if ((block_id != 0 && bSAD <= (CrossSearchThreshold2 >> block_id)) || (bSAD <= (CrossSearchThreshold1))) // satisfy skip intensive search condition?
                   goto up_layer_predictor_svc;

                xPos = bMVX;
                yPos = bMVY;

//==================== cross search (full in vertical dimantion)============== TODO: correct cross start position

                i = yPos;
                for (j = xPos + 1; j <= xR; j += 2)
                {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                        bSAD = cSAD;
                        bMVX = j;
                        if (bSAD <= meInfo->threshold)
                        {
                            goto end;
                        }
                    }
                }
                for (j = xPos - 1; j >= xL; j -= 2)
                {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                        bSAD = cSAD;
                        bMVX = j;
                        if (bSAD <= meInfo->threshold)
                        {
                            goto end;
                        }
                    }
                }

                j = xPos;

                for (i = yPos + 1; i <= yB; i += 2)
                {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                        bSAD = cSAD;
                        bMVY = i;
                        if (bSAD <= meInfo->threshold)
                        {
                            goto end;
                        }
                    }
                }
                for (i = yPos - 1; i >= yT; i -= 2)
                {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                        bSAD = cSAD;
                        bMVY = i;
                        if (bSAD <= meInfo->threshold)
                        {
                            goto end;
                        }
                    }
                }

//==================== cross search END ======================================

                xPos = bMVX;
                yPos = bMVY;

//==================== full search 5x5 =======================================
#if (0)
                for (i= yPos - 2; i<= yPos + 2; i++) {
                    for (j = xPos - 2; j <= xPos + 2; j ++) {
                        if (j >= xL && j <= xR && i>= yT && i<= yB)
                        {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                            cSAD += mvSAD;
                            if (cSAD < bSAD) {
                                bSAD = cSAD;
                                bMVX = i;
                                bMVY = j;
                                if (bSAD <= meInfo->threshold)
                                {
                                    goto end;
                                }
                            }
                        }
                    }
                }
#endif //(0)
//==================== full search 5x5 (END) =================================

                xPos = bMVX;
                yPos = bMVY;

//==================== Uneven multi-hexagon-grid search ======================

                static const Ipp32s bdJ[16] = {  0,  2,  4,  4, 4, 4, 4, 2, 0, -2, -4, -4, -4, -4, -4, -2};
                static const Ipp32s bdI[16] = { -4, -3, -2, -1, 0, 1, 2, 3, 4,  3,  2,  1,  0, -1, -2, -3};

                for (n = 1; n < sr/4+1; n++)
                {
                    for (k = 0; k < 16; k++)
                    {
                        j = xPos + bdJ[k]*n;
                        i = yPos + bdI[k]*n;
                        if (j >= xL && j <= xR && i >= yT && i <= yB)
                        {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            if (meInfo->residualPredictionFlag)
                              cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                            else
                              cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                            cSAD += mvSAD;
                            if (cSAD < bSAD) {
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                {
                                    goto end;
                                }
                            }
                        }
                    }
                }

//==================== Uneven multi-hexagon-grid search END ==================

    up_layer_predictor_svc:
                xPos = meInfo->candMV[0].mvx;
                yPos = meInfo->candMV[0].mvy;

//==================== local search around up layer predictor ================

                for (k = 0; k < 8; k++)
                {
                    j = xPos + bdJss[k];
                    i = yPos + bdIss[k];
                    if (j >= xL && j <= xR && i >= yT && i <= yB)
                    {
#ifdef NO_PADDING
                        meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                        meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                        if (meInfo->residualPredictionFlag)
                          cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                        else
                          cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                        MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                        cSAD += mvSAD;
                        if (cSAD < bSAD) {
                            bSAD = cSAD;
                            bMVX = j;
                            bMVY = i;
                            if (bSAD <= meInfo->threshold)
                            {
                                goto end;
                            }
                        }
                    }
                }
//==================== local search around up layer predictor (END) ==========

    small_mode_svc:

                if (bSAD < (ConvergenceThreshold >> block_id)) // satisfy converge condition?
                    goto end;

                xPos = bMVX;
                yPos = bMVY;

//==================== extended hexagon search ===============================

                static const Ipp32s bdJL[7] = {0, -1,  1, 2, 1, -1, -2};
                static const Ipp32s bdIL[7] = {0, -2, -2, 0, 2,  2,  0};

                static const Ipp32s bdJS[5] = {0,  0, 1, 0, -1};
                static const Ipp32s bdIS[5] = {0, -1, 0, 1,  0};

                static const Ipp32s bdJSF[5] = { -1,  1, 1, -1};
                static const Ipp32s bdISF[5] = { -1, -1, 1,  1};

                for (;;)
                {
                    l = 0;
                    for (k = 0; k < 7; k++)
                    {
                        j = xPos + bdJL[k];
                        i = yPos + bdIL[k];
                        if (j >= xL && j <= xR && i >= yT && i <= yB)
                        {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            if (meInfo->residualPredictionFlag)
                              cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                            else
                              cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                            cSAD += mvSAD;
                            if (cSAD < bSAD) {
                                l = k;
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                {
                                    goto end;
                                }
                            }
                        }
                    }
                    if (l == 0) break;
                    else
                    {
                        xPos += bdJL[l];
                        yPos += bdIL[l];
                    }
                }

                for(;;)
                {
                    l = 0;
                    for (k = 0; k < 5; k++)
                    {
                        j = xPos + bdJS[k];
                        i = yPos + bdIS[k];
                        if (j >= xL && j <= xR && i >= yT && i <= yB)
                        {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            if (meInfo->residualPredictionFlag)
                              cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                            else
                              cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                            cSAD += mvSAD;
                            if (cSAD < bSAD) {
                                bSAD = cSAD;
                                l = k;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                {
                                    goto end;
                                }
                            }
                        }
                    }
                    if (l == 0) break;
                    else
                    {
                        xPos += bdJS[l];
                        yPos += bdIS[l];
                    }
                }
                for (k = 0; k < 4; k++)
                {
                    j = xPos + bdJSF[k];
                    i = yPos + bdISF[k];
                    if (j >= xL && j <= xR && i >= yT && i <= yB)
                    {
#ifdef NO_PADDING
                        meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                        meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                        if (meInfo->residualPredictionFlag)
                          cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                        else
                          cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                        MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                        cSAD += mvSAD;
                        if (cSAD < bSAD) {
                            bSAD = cSAD;
                            bMVX = j;
                            bMVY = i;
                            if (bSAD <= meInfo->threshold)
                            {
                                goto end;
                            }
                        }
                    }
                }
//==================== extended hexagon search (END) =========================
            }
            else // AVC case
            {
                if (block_id == 4) goto small_mode;

                if (bSAD < (ConvergenceThreshold >> block_id)) // satisfy converge condition?
                    goto end;

                xPos = bMVX;
                yPos = bMVY;
                //==================== local search around best candidate ====================

                for (k = 0; k < 8; k++)
                {
                    j = xPos + bdJss[k];
                    i = yPos + bdIss[k];
                    if (j >= xL && j <= xR && i >= yT && i <= yB)
                    {
#ifdef NO_PADDING
                        meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                        meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                        cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                        if (cSAD < bSAD) {
                            bSAD = cSAD;
                            bMVX = j;
                            bMVY = i;
                            if (bSAD <= meInfo->threshold)
                            {
                                goto end;
                            }
                        }
                    }
                }

//==================== local search around best candidate (END) ==============

                if ((block_id != 0 && bSAD <= (CrossSearchThreshold2 >> block_id)) || (bSAD <= (CrossSearchThreshold1))) // satisfy skip intensive search condition?
                   goto up_layer_predictor;

                xPos = bMVX;
                yPos = bMVY;

//==================== cross search (full in vertical dimantion)============== TODO: correct cross start position

                i = yPos;
                for (j = xPos + 1; j <= xR; j += 2)
                {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                    if (cSAD < bSAD) {
                        bSAD = cSAD;
                        bMVX = j;
                        if (bSAD <= meInfo->threshold)
                        {
                            goto end;
                        }
                    }
                }
                for (j = xPos - 1; j >= xL; j -= 2)
                {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                    if (cSAD < bSAD) {
                        bSAD = cSAD;
                        bMVX = j;
                        if (bSAD <= meInfo->threshold)
                        {
                            goto end;
                        }
                    }
                }

                j = xPos;

                for (i = yPos + 1; i <= yB; i += 2)
                {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                    if (cSAD < bSAD) {
                        bSAD = cSAD;
                        bMVY = i;
                        if (bSAD <= meInfo->threshold)
                        {
                            goto end;
                        }
                    }
                }
                for (i = yPos - 1; i >= yT; i -= 2)
                {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                    if (cSAD < bSAD) {
                        bSAD = cSAD;
                        bMVY = i;
                        if (bSAD <= meInfo->threshold)
                        {
                            goto end;
                        }
                    }
                }

//==================== cross search END ======================================

                xPos = bMVX;
                yPos = bMVY;

//==================== full search 5x5 =======================================
#if (0)
                for (i= yPos - 2; i<= yPos + 2; i++) {
                    for (j = xPos - 2; j <= xPos + 2; j ++) {
                        if (j >= xL && j <= xR && i>= yT && i<= yB)
                        {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                            cSAD += mvSAD;
                            if (cSAD < bSAD) {
                                bSAD = cSAD;
                                bMVX = i;
                                bMVY = j;
                                if (bSAD <= meInfo->threshold)
                                {
                                    goto end;
                                }
                            }
                        }
                    }
                }
#endif //(0)
//==================== full search 5x5 (END) =================================

                xPos = bMVX;
                yPos = bMVY;

//==================== Uneven multi-hexagon-grid search ======================

                static const Ipp32s bdJ[16] = {  0,  2,  4,  4, 4, 4, 4, 2, 0, -2, -4, -4, -4, -4, -4, -2};
                static const Ipp32s bdI[16] = { -4, -3, -2, -1, 0, 1, 2, 3, 4,  3,  2,  1,  0, -1, -2, -3};

                for (n = 1; n < sr/4+1; n++)
                {
                    for (k = 0; k < 16; k++)
                    {
                        j = xPos + bdJ[k]*n;
                        i = yPos + bdI[k]*n;
                        if (j >= xL && j <= xR && i >= yT && i <= yB)
                        {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                            if (cSAD < bSAD) {
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                {
                                    goto end;
                                }
                            }
                        }
                    }
                }

//==================== Uneven multi-hexagon-grid search END ==================

    up_layer_predictor:
                xPos = meInfo->candMV[0].mvx;
                yPos = meInfo->candMV[0].mvy;

//==================== local search around up layer predictor ================

                for (k = 0; k < 8; k++)
                {
                    j = xPos + bdJss[k];
                    i = yPos + bdIss[k];
                    if (j >= xL && j <= xR && i >= yT && i <= yB)
                    {
#ifdef NO_PADDING
                        meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                        meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                        cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                        if (cSAD < bSAD) {
                            bSAD = cSAD;
                            bMVX = j;
                            bMVY = i;
                            if (bSAD <= meInfo->threshold)
                            {
                                goto end;
                            }
                        }
                    }
                }
//==================== local search around up layer predictor (END) ==========

    small_mode:

                if (bSAD < (ConvergenceThreshold >> block_id)) // satisfy converge condition?
                    goto end;

                xPos = bMVX;
                yPos = bMVY;

//==================== extended hexagon search ===============================

                static const Ipp32s bdJL[7] = {0, -1,  1, 2, 1, -1, -2};
                static const Ipp32s bdIL[7] = {0, -2, -2, 0, 2,  2,  0};

                static const Ipp32s bdJS[5] = {0,  0, 1, 0, -1};
                static const Ipp32s bdIS[5] = {0, -1, 0, 1,  0};

                static const Ipp32s bdJSF[5] = { -1,  1, 1, -1};
                static const Ipp32s bdISF[5] = { -1, -1, 1,  1};

                for (;;)
                {
                    l = 0;
                    for (k = 0; k < 7; k++)
                    {
                        j = xPos + bdJL[k];
                        i = yPos + bdIL[k];
                        if (j >= xL && j <= xR && i >= yT && i <= yB)
                        {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                            if (cSAD < bSAD) {
                                l = k;
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                {
                                    goto end;
                                }
                            }
                        }
                    }
                    if (l == 0) break;
                    else
                    {
                        xPos += bdJL[l];
                        yPos += bdIL[l];
                    }
                }

                for(;;)
                {
                    l = 0;
                    for (k = 0; k < 5; k++)
                    {
                        j = xPos + bdJS[k];
                        i = yPos + bdIS[k];
                        if (j >= xL && j <= xR && i >= yT && i <= yB)
                        {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                            if (cSAD < bSAD) {
                                bSAD = cSAD;
                                l = k;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                {
                                    goto end;
                                }
                            }
                        }
                    }
                    if (l == 0) break;
                    else
                    {
                        xPos += bdJS[l];
                        yPos += bdIS[l];
                    }
                }
                for (k = 0; k < 4; k++)
                {
                    j = xPos + bdJSF[k];
                    i = yPos + bdISF[k];
                    if (j >= xL && j <= xR && i >= yT && i <= yB)
                    {
#ifdef NO_PADDING
                        meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                        meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                        cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                        if (cSAD < bSAD) {
                            bSAD = cSAD;
                            bMVX = j;
                            bMVY = i;
                            if (bSAD <= meInfo->threshold)
                            {
                                goto end;
                            }
                        }
                    }
                }
//==================== extended hexagon search (END) =========================
            } // AVC case
        }
        break;
    case MV_SEARCH_TYPE_FULL:
        {
            Ipp32s y, x;
            for (y = yT; y <= yB; y ++) {
                for (x = xL; x <= xR; x ++) {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = x << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = y << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, x, y), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, x, y), pitchPixels, blockSize);
#endif // NO_PADDING
                    MVConstraintDelta(mvSAD, (x << SUB_PEL_SHIFT), (y << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                        bSAD = cSAD;
                        bMVX = x;
                        bMVY = y;
                        if (bSAD <= meInfo->threshold)
                            goto end;
                    }
                }
            }
        }
        break;
    case MV_SEARCH_TYPE_CLASSIC_LOG:
        {
            Ipp32s xPos, yPos, k, l;
            Ipp32s n = 0;
            Ipp32s sr = MAX(rX,rY);
            static const Ipp32s bdJ[5] = {0, 0, -1, 0, 1}, bdI[5] = {0, -1, 0, 1, 0};
            while ((sr = (sr >> 1)) != 0)
            {
                n++;
            }
            n = MAX(1 << (n - 1),2);

            xPos = bMVX;
            yPos = bMVY;

            for (;;)
            {
                l = 0;
                for (k = 0; k < 5; k++)
                {
                    j = xPos + bdJ[k]*n;
                    i = yPos + bdI[k]*n;
                    if (j >= xL && j <= xR && i >= yT && i <= yB)
                    {
#ifdef NO_PADDING
                        meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                        meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                        if (meInfo->residualPredictionFlag)
                          cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                        else
                          cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                        MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                        cSAD += mvSAD;
                        if (cSAD < bSAD)
                        {
                            l = k;
                            bSAD = cSAD;
                            bMVX = j;
                            bMVY = i;
                            if (bSAD <= meInfo->threshold)
                                goto end;
                        }
                    }
                }
                if (l == 0)
                {
                    n = (n >> 1);
                    if (n == 1)
                    {
                        Ipp32s x, y;
                        for (y = yPos - 1; y <= yPos + 1; y ++)
                        {
                            for (x = xPos - 1; x <= xPos + 1; x ++)
                            {
                                if (x >= xL && x <= xR && y >= yT && y <= yB)
                                {
#ifdef NO_PADDING
                                    meInfo->interpolateInfo.pointVector.x = x << SUB_PEL_SHIFT;
                                    meInfo->interpolateInfo.pointVector.y = y << SUB_PEL_SHIFT;
                                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                                    if (meInfo->residualPredictionFlag)
                                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, x, y), pitchPixels, blockSize);
                                    else
                                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, x, y), pitchPixels, blockSize);
#endif // NO_PADDING
                                    MVConstraintDelta(mvSAD, (x << SUB_PEL_SHIFT), (y << SUB_PEL_SHIFT));
                                    cSAD += mvSAD;
                                    if (cSAD < bSAD)
                                    {
                                        bSAD = cSAD;
                                        bMVX = x;
                                        bMVY = y;
                                        if (bSAD <= meInfo->threshold)
                                            goto end;
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
                else
                {
                    xPos += bdJ[l]*n;
                    yPos += bdI[l]*n;
                }
            }
        }
        break;
    case MV_SEARCH_TYPE_LOG:
        {
            Ipp32s k, l, m, n, xPos, yPos;
            //static const Ipp32s bdJ[5] = {0, -1, 0, 1, 0}, bdI[5] = {0, 0, -1, 0, 1};
            //static const Ipp32s bdN[5] = {4, 3, 3, 3, 3}, bdA[5][4] = {{1, 2, 3, 4}, {1, 2, 4, 0}, {1, 2, 3, 0}, {2, 3, 4, 0}, {3, 4, 1, 0}};
            static const Ipp32s bdJ[9] = {0, -1, 0, 1, 1, 1, 0, -1, -1}, bdI[9] = {0, -1, -1, -1, 0, 1, 1, 1, 0};
            static const Ipp32s bdN[9] = {8, 5, 3, 5, 3, 5, 3, 5, 3}, bdA[9][8] = {{1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 7, 8, 0, 0, 0}, {1, 2, 3, 0, 0, 0, 0, 0}, {1, 2, 3, 4, 5, 0, 0, 0}, {3, 4, 5, 0, 0, 0, 0, 0}, {3, 4, 5, 6, 7, 0, 0, 0}, {5, 6, 7, 0, 0, 0, 0, 0}, {5, 6, 7, 8, 1, 0, 0, 0}, {7, 8, 1, 0, 0, 0, 0, 0}};
            xPos = bMVX;
            yPos = bMVY;
            l = 0;

            if (meInfo->residualPredictionFlag || meInfo->useScPredMV) // SVC case
            {
                for (;;) {
                    n = l;
                    l = 0;
                    for (m = 0; m < bdN[n]; m ++) {
                        k = bdA[n][m];
                        j = xPos + bdJ[k] * rX;
                        i = yPos + bdI[k] * rY;
                        if (j >= xL && j <= xR && i >= yT && i <= yB) {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            if (meInfo->residualPredictionFlag)
                              cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                            else
                              cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                            cSAD += mvSAD;
                            if (cSAD < bSAD) {
                                l = k;
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                            }
                        }
                    }
                    if (l == 0) {
                        rX >>= 1;
                        rY >>= 1;
                        if (rX == 0 || rY == 0)
                            break;
                    } else {
                        xPos += bdJ[l] * rX;
                        yPos += bdI[l] * rY;
                    }
                }
            }
            else // AVC case
            {
                for (;;) {
                    n = l;
                    l = 0;
                    for (m = 0; m < bdN[n]; m ++) {
                        k = bdA[n][m];
                        j = xPos + bdJ[k] * rX;
                        i = yPos + bdI[k] * rY;
                        if (j >= xL && j <= xR && i >= yT && i <= yB) {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                            if (cSAD < bSAD) {
                                l = k;
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                            }
                        }
                    }
                    if (l == 0) {
                        rX >>= 1;
                        rY >>= 1;
                        if (rX == 0 || rY == 0)
                            break;
                    } else {
                        xPos += bdJ[l] * rX;
                        yPos += bdI[l] * rY;
                    }
                }
            }
        }
        break;
    case MV_SEARCH_TYPE_EPZS:
        {
            Ipp32s xPos, yPos, k, l;
            Ipp32s r;
            static const Ipp32s bdJL[9] = {0, -1, 0, 1, 2, 1, 0, -1, -2}, bdIL[9] = {0, -1, -2, -1, 0, 1, 2, 1, 0};
            static const Ipp32s bdJS[5] = {0, 0, 1, 0, -1}, bdIS[5] = {0, -1, 0, 1, 0};
            r = (yB - yT + xR - xL)/4;
            xPos = bMVX;
            yPos = bMVY;

            for (;;)
            {
                l = 0;
                for (k = 0; k < 9; k++)
                {
                    j = xPos + bdJL[k]*r;
                    i = yPos + bdIL[k]*r;
                    if (j >= xL && j <= xR && i >= yT && i <= yB)
                    {
#ifdef NO_PADDING
                        meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                        meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                        if (meInfo->residualPredictionFlag)
                          cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                        else
                          cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                        MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                        cSAD += mvSAD;
                        if (cSAD < bSAD)
                        {
                            l = k;
                            bSAD = cSAD;
                            bMVX = j;
                            bMVY = i;
                            if (bSAD <= meInfo->threshold)
                                goto end;
                        }
                    }
                }
                if (l == 0)
                {
                    r = (r >> 1);
                    if (r == 0) break;
                }
                else
                {
                    xPos += bdJL[l]*r;
                    yPos += bdIL[l]*r;
                }
            }

            xPos = bMVX;
            yPos = bMVY;

            for (k = 0; k < 5; k++)
            {
                j = xPos + bdJS[k];
                i = yPos + bdIS[k];
                if (j >= xL && j <= xR && i >= yT && i <= yB)
                {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                        bSAD = cSAD;
                        bMVX = j;
                        bMVY = i;
                        if (bSAD <= meInfo->threshold)
                            goto end;
                    }
                }
            }
        }
        break;
    case MV_SEARCH_TYPE_FULL_ORTHOGONAL:
        {
            Ipp32s xPos, yPos;
            xPos = bMVX;
            yPos = bMVY;
            i = yPos;
            for (j = xPos - rX; j < xPos + rX; j += 2)
            {
                if (j >= xL && j <= xR && i >= yT && i <= yB)  {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                    }
                }
            }
            j = bMVX;
            for (i = yPos - rY; i < yPos + rY; i += 2)
            {
                if (j >= xL && j <= xR && i >= yT && i <= yB)  {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    //cSAD += MVConstraintDelta((0 << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT)); // j or 0 &&
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                       goto end;
                        }
                }
            }
        }
        break;
    case MV_SEARCH_TYPE_LOG_ORTHOGONAL:
        {
            Ipp32s jL, jR, iT, iB, xPos, yPos,r;
            r = rX;
            i = 0;
            for (;;)
            {
                xPos = bMVX;
                jL = xPos - r;
                jR = xPos + r;
                if (jL >= xL)  {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = jL << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, jL, i), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, jL, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    //cSAD += MVConstraintDelta((jL << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    MVConstraintDelta(mvSAD, (jL << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                                bSAD = cSAD;
                                bMVX = jL;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                    }
                }
                if (jR <= xR)  {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = jR << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, jR, i), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, jR, i), pitchPixels, blockSize);
#endif // NO_PADDING
                    //cSAD += MVConstraintDelta((jR << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    MVConstraintDelta(mvSAD, (jR << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                                bSAD = cSAD;
                                bMVX = jR;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                    }
                }
                if (bMVX == xPos)
                {
                    r /= 2;
                    if (r == 0) break;
                }
            }
            j = bMVX;
            r = rY;
            for (;;)
            {
                yPos = bMVY;
                iT = yPos - r;
                iB = yPos + r;
                if (iT >= yT)  {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = iT << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, iT), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, iT), pitchPixels, blockSize);
#endif // NO_PADDING
                    //cSAD += MVConstraintDelta((j << SUB_PEL_SHIFT), (iT << SUB_PEL_SHIFT));
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (iT << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                                bSAD = cSAD;
                                bMVY = iT;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                    }
                }
                if (iB <= yB)  {
#ifdef NO_PADDING
                    meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                    meInfo->interpolateInfo.pointVector.y = iB << SUB_PEL_SHIFT;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                    cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                    if (meInfo->residualPredictionFlag)
                      cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, iB), pitchPixels, blockSize);
                    else
                      cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, iB), pitchPixels, blockSize);
#endif // NO_PADDING
                    //cSAD += MVConstraintDelta((j << SUB_PEL_SHIFT), (iB << SUB_PEL_SHIFT));
                    MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (iB << SUB_PEL_SHIFT));
                    cSAD += mvSAD;
                    if (cSAD < bSAD) {
                                bSAD = cSAD;
                                bMVY = iB;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                    }
                }
                if (bMVY == yPos)
                {
                    r /= 2;
                    if (r == 0) break;
                }
            }
        }
        break;
    case MV_SEARCH_TYPE_SMALL_DIAMOND:
        {
            Ipp32s k, l, m, n, xPos, yPos;
            static const Ipp32s bdJ[5] = {0, -1, 0, 1, 0}, bdI[5] = {0, 0, -1, 0, 1};
            static const Ipp32s bdN[5] = {4, 3, 3, 3, 3}, bdA[5][4] = {{1, 2, 3, 4}, {1, 2, 4, 0}, {1, 2, 3, 0}, {2, 3, 4, 0}, {3, 4, 1, 0}};
            xPos = bMVX;
            yPos = bMVY;
            l = 0;
            if (meInfo->residualPredictionFlag || meInfo->useScPredMV) // SVC case
            {
                    for (;;) {
                    n = l;
                    l = 0;
                    for (m = 0; m < bdN[n]; m ++) {
                        k = bdA[n][m];
                        j = xPos + bdJ[k];
                        i = yPos + bdI[k];
                        if (j >= xL && j <= xR && i >= yT && i <= yB) {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            if (meInfo->residualPredictionFlag)
                              cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                            else
                              cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                            cSAD += mvSAD;
                            if (cSAD < bSAD) {
                                l = k;
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                            }
                        }
                    }
                    if (l == 0)
                        break;
                    xPos += bdJ[l];
                    yPos += bdI[l];
                }
            }
            else // AVC case
            {
                for (;;) {
                    n = l;
                    l = 0;
                    for (m = 0; m < bdN[n]; m ++) {
                        k = bdA[n][m];
                        j = xPos + bdJ[k];
                        i = yPos + bdI[k];
                        if (j >= xL && j <= xR && i >= yT && i <= yB) {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                            if (cSAD < bSAD) {
                                l = k;
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                            }
                        }
                    }
                    if (l == 0)
                        break;
                    xPos += bdJ[l];
                    yPos += bdI[l];
                }
            }
        }
        break;
    case MV_SEARCH_TYPE_SQUARE:
    default:
        {
            Ipp32s k, l, m, n, xPos, yPos;
            static const Ipp32s bdJ[9] = {0, -1, 0, 1, 1, 1, 0, -1, -1}, bdI[9] = {0, -1, -1, -1, 0, 1, 1, 1, 0};
            static const Ipp32s bdN[9] = {8, 5, 3, 5, 3, 5, 3, 5, 3}, bdA[9][8] = {{1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 7, 8, 0, 0, 0}, {1, 2, 3, 0, 0, 0, 0, 0}, {1, 2, 3, 4, 5, 0, 0, 0}, {3, 4, 5, 0, 0, 0, 0, 0}, {3, 4, 5, 6, 7, 0, 0, 0}, {5, 6, 7, 0, 0, 0, 0, 0}, {5, 6, 7, 8, 1, 0, 0, 0}, {7, 8, 1, 0, 0, 0, 0, 0}};
            xPos = bMVX;
            yPos = bMVY;
            l = 0;
            if (meInfo->residualPredictionFlag || meInfo->useScPredMV) // SVC case
            {
                for (;;) {
                    n = l;
                    l = 0;
                    for (m = 0; m < bdN[n]; m ++) {
                        k = bdA[n][m];
                        j = xPos + bdJ[k];
                        i = yPos + bdI[k];
                        if (j >= xL && j <= xR && i >= yT && i <= yB) {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            if (meInfo->residualPredictionFlag)
                              cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                            else
                              cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            MVConstraintDelta(mvSAD, (j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));
                            cSAD += mvSAD;
                            if (cSAD < bSAD) {
                                l = k;
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                            }
                        }
                    }
                    if (l == 0)
                        break;
                    xPos += bdJ[l];
                    yPos += bdI[l];
                }
            }
            else // AVC case
            {
                for (;;) {
                    n = l;
                    l = 0;
                    for (m = 0; m < bdN[n]; m ++) {
                        k = bdA[n][m];
                        j = xPos + bdJ[k];
                        i = yPos + bdI[k];
                        if (j >= xL && j <= xR && i >= yT && i <= yB) {
#ifdef NO_PADDING
                            meInfo->interpolateInfo.pointVector.x = j << SUB_PEL_SHIFT;
                            meInfo->interpolateInfo.pointVector.y = i << SUB_PEL_SHIFT;
                            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
                            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
#endif // NO_PADDING
                            cSAD += MVConstraintDelta0((j << SUB_PEL_SHIFT), (i << SUB_PEL_SHIFT));;
                            if (cSAD < bSAD) {
                                l = k;
                                bSAD = cSAD;
                                bMVX = j;
                                bMVY = i;
                                if (bSAD <= meInfo->threshold)
                                    goto end;
                            }
                        }
                    }
                    if (l == 0)
                        break;
                    xPos += bdJ[l];
                    yPos += bdI[l];
                }
            }
        }
        break;
    }

//========================= CONTINUED_SEARCH =================================

    if (meInfo->flags & ANALYSE_ME_CONTINUED_SEARCH)
    {
        if (!pass_counter || (pass_counter && (bSAD < tempSAD)))
        {
//================================== near the border checking ================
            border_width_LR = temprX >> 2;
            border_width_TB = temprY >> 2;
            lB = ((bMVX - xL) <= border_width_LR);
            rB = ((xR - bMVX) <= border_width_LR);
            tB = ((bMVY - yT) <= border_width_TB);
            bB = ((yB - bMVY) <= border_width_TB);
            //lrB = lB || rB;
            tbB = tB || bB;

            if ((rX = meInfo->rX >> (pass_counter + 1)) < 4)
                rX = 4;
            if ((rY = meInfo->rY >> (pass_counter + 1)) < 4)
                rY = 4;

#if (1)
            if (lB)
            {
                corrX = xL - border_width_LR;
                if (corrX >= meInfo->xMin && corrX <= meInfo->xMax)
                {
                    if (!tbB)
                    {
                        if (prevBorder == 4)
                        {
                            goto out_continued_search;
                        }
                        prevBorder = 8;
                        corrY = bMVY;
                        xR = MIN(meInfo->xMax, xL - 1);
                        xL = MAX(meInfo->xMin, corrX - rX);
                        yT = MAX(meInfo->yMin, corrY - rY);
                        yB = MIN(meInfo->yMax, corrY + rY);
                    }
                    else
                    {
                        if (tB)
                        {
                            if (prevBorder == 5)
                            {
                                goto out_continued_search;
                            }
                            prevBorder = 1;
                            corrY = yT;
                        }
                        if (bB)
                        {
                            if (prevBorder == 3)
                            {
                                goto out_continued_search;
                            }
                            prevBorder = 7;
                            corrY = yB;
                        }
                        xL = MAX(meInfo->xMin, corrX - rX);
                        xR = MIN(meInfo->xMax, corrX + rX);
                        yT = MAX(meInfo->yMin, corrY - rY);
                        yB = MIN(meInfo->yMax, corrY + rY);
                    }
                }
                else
                {
                    goto out_continued_search;
                }
            }
            else if (rB)
            {
                corrX = xR + border_width_LR;
                if (corrX >= meInfo->xMin && corrX <= meInfo->xMax)
                {
                    if (!tbB)
                    {
                        if (prevBorder == 8)
                        {
                            goto out_continued_search;
                        }
                        prevBorder = 4;
                        corrY = bMVY;
                        xL = MAX(meInfo->xMin, xR + 1);
                        xR = MIN(meInfo->xMax, corrX + rX);
                        yT = MAX(meInfo->yMin, corrY - rY);
                        yB = MIN(meInfo->yMax, corrY + rY);
                    }
                    else
                    {
                        if (tB)
                        {
                            if (prevBorder == 7)
                            {
                                goto out_continued_search;
                            }
                            prevBorder = 3;
                            corrY = yT;
                        }
                        if (bB)
                        {
                            if (prevBorder == 1)
                            {
                                goto out_continued_search;
                            }
                            prevBorder = 5;
                            corrY = yB;
                        }
                        xL = MAX(meInfo->xMin, corrX - rX);
                        xR = MIN(meInfo->xMax, corrX + rX);
                        yT = MAX(meInfo->yMin, corrY - rY);
                        yB = MIN(meInfo->yMax, corrY + rY);
                    }
                }
                else
                {
                    goto out_continued_search;
                }
            }
            else if (tB)
            {
                corrY = yT - border_width_TB;
                if (corrY >= meInfo->yMin && corrY <= meInfo->yMax)
                {
                    if (prevBorder == 6)
                    {
                        goto out_continued_search;
                    }
                    prevBorder = 2;
                    corrX = bMVX;
                    xL = MAX(meInfo->xMin, corrX - rX);
                    xR = MIN(meInfo->xMax, corrX + rX);
                    yB = MIN(meInfo->yMax, yT - 1);
                    yT = MAX(meInfo->yMin, corrY - rY);

                }
                else
                {
                    goto out_continued_search;
                }
            }
            else if (bB)
            {
                corrY = yB + border_width_TB;
                if (corrY >= meInfo->yMin && corrY <= meInfo->yMax)
                {
                    if (prevBorder == 2)
                    {
                        goto out_continued_search;
                    }
                    prevBorder = 6;
                    corrX = bMVX;
                    xL = MAX(meInfo->xMin, corrX - rX);
                    xR = MIN(meInfo->xMax, corrX + rX);
                    yT = MAX(meInfo->yMin, yB + 1);
                    yB = MIN(meInfo->yMax, corrY + rY);
                }
                else
                {
                    goto out_continued_search;
                }
            }
            else
            {
                goto out_continued_search;
            }
#else
#endif
//================================== near the border checking (END) ==========

//============= updating params before next search iteration =================
            pass_counter ++;

            tempMVX = bMVX;
            tempMVY = bMVY;
            tempSAD = bSAD;
            bMVX = corrX;
            bMVY = corrY;
            temprX = rX;
            temprY = rY;

#ifdef NO_PADDING
            meInfo->interpolateInfo.pointVector.x = bMVX << SUB_PEL_SHIFT;
            meInfo->interpolateInfo.pointVector.y = bMVY << SUB_PEL_SHIFT;
            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
            cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize);
#else // NO_PADDING
            if (meInfo->residualPredictionFlag)
              cSAD = SAD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, bMVX, bMVY), pitchPixels, blockSize);
            else
              cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, bMVX, bMVY), pitchPixels, blockSize);
#endif // NO_PADDING
            //cSAD += MVConstraintDelta((bMVX << SUB_PEL_SHIFT), (bMVY << SUB_PEL_SHIFT));
            MVConstraintDelta(mvSAD, (bMVX << SUB_PEL_SHIFT), (bMVY << SUB_PEL_SHIFT));
            cSAD += mvSAD;
            if (cSAD < bSAD) {
                bSAD = cSAD;
                if (bSAD <= meInfo->threshold)
                    goto end;
            }

//============= updating params before next search iteration (END) ===========

            goto search_again;
        }
        else
        {
            if (pass_counter && (tempSAD <= bSAD))
            {
                bMVX = tempMVX;
                bMVY = tempMVY;
                bSAD = tempSAD;
            }
        }
        out_continued_search:
        pass_counter = 0;

    }

//========================= CONTINUED_SEARCH (END) ===========================

end:

    meInfo->bestMV.mvx = meInfo->bestMV_Int.mvx = (Ipp16s)bMVX << SUB_PEL_SHIFT;
    meInfo->bestMV.mvy = meInfo->bestMV_Int.mvy = (Ipp16s)bMVY << SUB_PEL_SHIFT;
    // change SAD to SATD
    if (!(meInfo->flags & ANALYSE_SAD) && !(meInfo->flags & ANALYSE_ME_SUBPEL_SAD)) {
        MVConstraintDelta(mvSAD, meInfo->bestMV.mvx, meInfo->bestMV.mvy);
#ifdef NO_PADDING
        meInfo->interpolateInfo.pointVector.x = meInfo->bestMV.mvx;
        meInfo->interpolateInfo.pointVector.y = meInfo->bestMV.mvy;
        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
        bSAD = H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, interpBlockBuf, 16, blockSize) +
            mvSAD;
#else // NO_PADDING
        if (meInfo->residualPredictionFlag)
            bSAD = SATD_ResPred(resPredBuf, 16, MVADJUST(pRef, pitchPixels, bMVX, bMVY), pitchPixels, blockSize) +
            mvSAD;
        else
            bSAD = H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, bMVX, bMVY), pitchPixels, blockSize) +
              mvSAD;
#endif // NO_PADDING
    }
    if ((meInfo->flags & ANALYSE_ME_CHROMA) && !(meInfo->flags & ANALYSE_ME_SUBPEL)) {
        __ALIGN16 PIXTYPE interpBuff[256];
        IppiSize chroma_size;
        PIXTYPE *pCurU = meInfo->pCurU;
        PIXTYPE *pCurV = meInfo->pCurV;
#ifndef NO_PADDING
        PIXTYPE *pRefU = meInfo->pRefU;
#ifndef USE_NV12
        PIXTYPE *pRefV = meInfo->pRefV;
#endif // !USE_NV12
#endif // NO_PADDING
        Ipp16s chroma_mvy_offset = meInfo->chroma_mvy_offset;

        if (meInfo->chroma_format_idc == 1) { //420
            chroma_size.width =  meInfo->block.width >> 1;
            chroma_size.height =  meInfo->block.height >> 1;
        } else if (meInfo->chroma_format_idc == 2) { //422
            chroma_size.width =  meInfo->block.width >> 1;
            chroma_size.height =  meInfo->block.height;
        } else { //444
            chroma_size =  meInfo->block;
        }
        Ipp32s chroma_block_size = chroma_size.width + (chroma_size.height >> 2);
#ifndef NO_PADDING
        Ipp32s iXType, iYType;
#endif
        H264MotionVector chroma_vec;
        chroma_vec.mvx = meInfo->bestMV.mvx; chroma_vec.mvy = meInfo->bestMV.mvy + chroma_mvy_offset;
#ifdef NO_PADDING
         meInfo->interpolateInfoChroma.pDst[0] = interpBuff;
         meInfo->interpolateInfoChroma.pDst[1] = interpBuff + 8;
         if( meInfo->chroma_format_idc == 1) //420
         {
             meInfo->interpolateInfoChroma.pointBlockPos.x = meInfo->interpolateInfo.pointBlockPos.x >> 1;
             meInfo->interpolateInfoChroma.pointBlockPos.y = meInfo->interpolateInfo.pointBlockPos.y >> 1;
             meInfo->interpolateInfoChroma.pointVector.x = chroma_vec.mvx;
             meInfo->interpolateInfoChroma.pointVector.y = chroma_vec.mvy;
         }
         else if( meInfo->chroma_format_idc == 2) //422
         {
             meInfo->interpolateInfoChroma.pointBlockPos.x = meInfo->interpolateInfo.pointBlockPos.x >> 1;
             meInfo->interpolateInfoChroma.pointBlockPos.y = meInfo->interpolateInfo.pointBlockPos.y;
             meInfo->interpolateInfoChroma.pointVector.x = chroma_vec.mvx;
             meInfo->interpolateInfoChroma.pointVector.y = chroma_vec.mvy << 1;
         }
         meInfo->interpolateInfoChroma.sizeBlock = chroma_size;
         H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(&(meInfo->interpolateInfoChroma));
#else // NO_PADDING
        Ipp32s offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo->chroma_format_idc);
#ifdef USE_NV12
        H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefU+offset, pitchPixels, interpBuff, interpBuff+8, 16, iXType, iYType, chroma_size, meInfo->bit_depth_chroma);
#else // USE_NV12
        H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefU+offset, pitchPixels, interpBuff, 16, iXType, iYType, chroma_size, meInfo->bit_depth_chroma);
        H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefV+offset, pitchPixels, interpBuff+8, 16, iXType, iYType, chroma_size, meInfo->bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING

        if (meInfo->residualPredictionFlag) {
          if (meInfo->flags & ANALYSE_SAD) {
              bSAD += SAD_ResPred(meInfo->pResU, 16, interpBuff, 16, chroma_block_size);
              bSAD += SAD_ResPred(meInfo->pResV, 16, interpBuff+8, 16, chroma_block_size);
          } else {
              bSAD += SATD_ResPred(meInfo->pResU, 16, interpBuff, 16, chroma_block_size);
              bSAD += SATD_ResPred(meInfo->pResV, 16, interpBuff+8, 16, chroma_block_size);
          }
        } else {
          if (meInfo->flags & ANALYSE_SAD) {
              bSAD += H264ENC_MAKE_NAME(SAD)(pCurU, pitchPixels, interpBuff, 16, chroma_block_size);
              bSAD += H264ENC_MAKE_NAME(SAD)(pCurV, pitchPixels, interpBuff+8, 16, chroma_block_size);
          } else {
              bSAD += H264ENC_MAKE_NAME(SATD)(pCurU, pitchPixels, interpBuff, 16, chroma_block_size);
              bSAD += H264ENC_MAKE_NAME(SATD)(pCurV, pitchPixels, interpBuff+8, 16, chroma_block_size);
          }
        }
    }
    meInfo->bestSAD = bSAD;
}


#ifdef FRAME_INTERPOLATION

#if PIXBITS == 8
void H264ENC_MAKE_NAME(InterpolateLuma)(
    const Ipp8u *pY,
    Ipp32s pitchPixels,
    Ipp8u *interpBuff,
    Ipp32s bStep,
    Ipp32s dx,
    Ipp32s dy,
    IppiSize block,
    Ipp32s bitDepth,
    Ipp32s planeSize,
    const Ipp8u **pI,
    Ipp32s *sI)
{
    const Ipp8u *pB, *pH, *pJ, *pS1, *pS2;

    pB = pY + planeSize;
    pH = pB + planeSize;
    pJ = pH + planeSize;
    if (((dx & 1) == 0) && ((dy & 1) == 0)) {
        if (dx == 0)
            pS1 = (dy == 0) ? pY : pH;
        else
            pS1 = (dy == 0) ? pB : pJ;
        if (pI != NULL) {
            *sI = pitchPixels;
            *pI = pS1;
        } else
            H264ENC_MAKE_NAME(ippiInterpolateLuma_H264)(pS1, pitchPixels, interpBuff, bStep, 0, 0, block, bitDepth);
        return;
    }
    if (dy == 0) {
        pS1 = pB;
        pS2 = pY + (dx >> 1);
    } else if (dy == 2) {
        pS1 = pJ;
        pS2 = pH + (dx >> 1);
    } else {
        if (dx == 0) {
            pS1 = pH;
            pS2 = pY + (dy >> 1) * pitchPixels;
        } else if (dx == 2) {
            pS1 = pJ;
            pS2 = pB + (dy >> 1) * pitchPixels;
        } else {
            pS1 = pB + (dy >> 1) * pitchPixels;
            pS2 = pH + (dx >> 1);
        }
    }
    H264ENC_MAKE_NAME(ippiInterpolateBlock_H264_A)(pS1, pS2, interpBuff, block.width, block.height, pitchPixels, pitchPixels, bStep);
    if (pI != NULL) {
        *pI = interpBuff;
        *sI = bStep;
    }
}


#elif PIXBITS == 16

void H264ENC_MAKE_NAME(InterpolateLuma)(
    const Ipp16u *pY,
    Ipp32s pitchPixels,
    Ipp16u *interpBuff,
    Ipp32s bStep,
    Ipp32s dx,
    Ipp32s dy,
    IppiSize block,
    Ipp32s bitDepth,
    Ipp32s planeSize,
    const Ipp16u **pI,
    Ipp32s *sI)
{
    const Ipp16u *pB, *pH, *pJ, *pS1, *pS2;

    pB = pY + planeSize;
    pH = pB + planeSize;
    pJ = pH + planeSize;
    if (((dx & 1) == 0) && ((dy & 1) == 0)) {
        if (dx == 0)
            pS1 = (dy == 0) ? pY : pH;
        else
            pS1 = (dy == 0) ? pB : pJ;
        if (pI != NULL) {
            *sI = pitchPixels;
            *pI = pS1;
        } else
            H264ENC_MAKE_NAME(ippiInterpolateLuma_H264)(pS1, pitchPixels, interpBuff, bStep, 0, 0, block, bitDepth);
        return;
    }
    if (dy == 0) {
        pS1 = pB;
        pS2 = pY + (dx >> 1);
    } else if (dy == 2) {
        pS1 = pJ;
        pS2 = pH + (dx >> 1);
    } else {
        if (dx == 0) {
            pS1 = pH;
            pS2 = pY + (dy >> 1) * pitchPixels;
        } else if (dx == 2) {
            pS1 = pJ;
            pS2 = pB + (dy >> 1) * pitchPixels;
        } else {
            pS1 = pB + (dy >> 1) * pitchPixels;
            pS2 = pH + (dx >> 1);
        }
    }
    H264ENC_MAKE_NAME(ippiInterpolateBlock_H264_A)(pS1, pS2, interpBuff, block.width, block.height, pitchPixels, pitchPixels, bStep);
    if (pI != NULL) {
        *pI = interpBuff;
        *sI = bStep;
    }
}

#endif //PIXBITS
/*
#define SubCalcSAD(j, i, useSAD) \
    const PIXTYPE *pI; \
    Ipp32s   sI; \
    H264ENC_MAKE_NAME(InterpolateLuma)(MVADJUST(pRef, pitchPixels, j >> SUB_PEL_SHIFT, i >> SUB_PEL_SHIFT), pitchPixels, interpBuff, 16, j & 3, i & 3, meInfo->block, meInfo->bit_depth_luma, meInfo->planeSize, &pI, &sI); \
    MVConstraintDelta(cSAD, j, i); \
    if (useSAD) \
        cSAD += H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, (PIXTYPE*)pI, sI, blockSize); \
    else \
        cSAD += H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, (PIXTYPE*)pI, sI, blockSize); \
    if ((meInfo->flags & ANALYSE_ME_CHROMA) && (cSAD < bSAD)) { \
         Ipp32s iXType, iYType; \
         H264MotionVector chroma_vec; \
         chroma_vec.mvx = j; chroma_vec.mvy=i+chroma_mvy_offset; \
         Ipp32s offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo->chroma_format_idc); \
         H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefU+offset, pitchPixels, interpBuff, 16, iXType, iYType, chroma_size, meInfo->bit_depth_chroma); \
         H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefV+offset, pitchPixels, interpBuff+8, 16, iXType, iYType, chroma_size, meInfo->bit_depth_chroma); \
         if (useSAD) { \
             cSAD += H264ENC_MAKE_NAME(SAD)(pCurU, pitchPixels, interpBuff, 16, chroma_block_size); \
             cSAD += H264ENC_MAKE_NAME(SAD)(pCurV, pitchPixels, interpBuff+8, 16, chroma_block_size); \
         } else { \
             cSAD += H264ENC_MAKE_NAME(SATD)(pCurU, pitchPixels, interpBuff, 16, chroma_block_size); \
             cSAD += H264ENC_MAKE_NAME(SATD)(pCurV, pitchPixels, interpBuff+8, 16, chroma_block_size); \
         } \
    }
*/

#define SubCalcSAD(j, i, useSAD) \
    const PIXTYPE *pI; \
    Ipp32s   sI; \
    H264ENC_MAKE_NAME(InterpolateLuma)(MVADJUST(pRef, pitchPixels, j >> SUB_PEL_SHIFT, i >> SUB_PEL_SHIFT), pitchPixels, interpBuff, 16, j & 3, i & 3, meInfo->block, meInfo->bit_depth_luma, meInfo->planeSize, &pI, &sI); \
    MVConstraintDelta(cSAD, j, i); \
    if (meInfo->residualPredictionFlag) {\
        if (useSAD) {\
            cSAD += SAD_ResPred(meInfo->pResY, 16, (PIXTYPE*)pI, sI, blockSize);\
        } else {\
            cSAD += SATD_ResPred(meInfo->pResY, 16, (PIXTYPE*)pI, sI, blockSize);\
        }\
    } else {\
        if (useSAD) {\
            cSAD += H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, (PIXTYPE*)pI, sI, blockSize);\
        } else {\
            cSAD += H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, (PIXTYPE*)pI, sI, blockSize);\
        }\
    }

#define SubCalcSAD_AVC(j, i, useSAD) \
    const PIXTYPE *pI; \
    Ipp32s   sI; \
    H264ENC_MAKE_NAME(InterpolateLuma)(MVADJUST(pRef, pitchPixels, j >> SUB_PEL_SHIFT, i >> SUB_PEL_SHIFT), pitchPixels, interpBuff, 16, j & 3, i & 3, meInfo->block, meInfo->bit_depth_luma, meInfo->planeSize, &pI, &sI); \
    cSAD = MVConstraintDelta0(j, i); \
    if (useSAD) {\
        cSAD += H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, (PIXTYPE*)pI, sI, blockSize);\
    } else {\
        cSAD += H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, (PIXTYPE*)pI, sI, blockSize);\
    }

#else //FRAME_INTERPOLATION

void H264ENC_MAKE_NAME(InterpolateLuma)(
    const PIXTYPE* src,
    Ipp32s src_pitch,
    PIXTYPE* dst,
    Ipp32s dst_pitch,
    Ipp32s xh,
    Ipp32s yh,
    IppiSize sz,
    Ipp32s bit_depth,
    Ipp32s planeSize,
    const PIXTYPE **pI/* = NULL*/,
    Ipp32s *sI/* = NULL*/)
{
    H264ENC_UNREFERENCED_PARAMETER(planeSize);
    H264ENC_UNREFERENCED_PARAMETER(pI);
    H264ENC_UNREFERENCED_PARAMETER(sI);
    H264ENC_MAKE_NAME(ippiInterpolateLuma_H264)(src, src_pitch, dst, dst_pitch, xh, yh, sz, bit_depth);
    if (pI != NULL) {
        *pI = dst;
        *sI = dst_pitch;
    }
}
/*
#define SubCalcSAD(j, i, useSAD) \
    H264ENC_MAKE_NAME(ippiInterpolateLuma_H264)(MVADJUST(pRef, pitchPixels, j >> SUB_PEL_SHIFT, i >> SUB_PEL_SHIFT), pitchPixels, interpBuff, 16, j & 3, i & 3, meInfo->block, meInfo->bit_depth_luma); \
    MVConstraintDelta(cSAD, j, i); \
    if (useSAD) \
        cSAD += H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBuff, 16, blockSize); \
    else \
        cSAD += H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, interpBuff, 16, blockSize); \
    if ((meInfo->flags & ANALYSE_ME_CHROMA) && (cSAD < bSAD)) { \
         Ipp32s iXType, iYType; \
         H264MotionVector chroma_vec; \
         chroma_vec.mvx = j; chroma_vec.mvy=i+chroma_mvy_offset; \
         Ipp32s offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo->chroma_format_idc); \
         H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefU+offset, pitchPixels, interpBuff, 16, iXType, iYType, chroma_size, meInfo->bit_depth_chroma); \
         H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefV+offset, pitchPixels, interpBuff+8, 16, iXType, iYType, chroma_size, meInfo->bit_depth_chroma); \
         if (useSAD) { \
             cSAD += H264ENC_MAKE_NAME(SAD)(pCurU, pitchPixels, interpBuff, 16, chroma_block_size); \
             cSAD += H264ENC_MAKE_NAME(SAD)(pCurV, pitchPixels, interpBuff+8, 16, chroma_block_size); \
         } else { \
             cSAD += H264ENC_MAKE_NAME(SATD)(pCurU, pitchPixels, interpBuff, 16, chroma_block_size); \
             cSAD += H264ENC_MAKE_NAME(SATD)(pCurV, pitchPixels, interpBuff+8, 16, chroma_block_size); \
         } \
    }
*/
#ifdef NO_PADDING
#define SubCalcSAD(j, i, useSAD) \
    meInfo->interpolateInfo.pointVector.x = j; \
    meInfo->interpolateInfo.pointVector.y = i; \
    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo)); \
    MVConstraintDelta(cSAD, j, i ); \
    if (useSAD) \
    cSAD += H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBuff, 16, blockSize); \
    else \
    cSAD += H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, interpBuff, 16, blockSize);
#else // NO_PADDING
#define SubCalcSAD(j, i, useSAD) \
    H264ENC_MAKE_NAME(ippiInterpolateLuma_H264)(MVADJUST(pRef, pitchPixels, j >> SUB_PEL_SHIFT, i >> SUB_PEL_SHIFT), pitchPixels, interpBuff, 16, j & 3, i & 3, meInfo->block, meInfo->bit_depth_luma); \
    MVConstraintDelta(cSAD, j, i ); \
    if (useSAD) \
        cSAD += H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, interpBuff, 16, blockSize); \
    else \
        cSAD += H264ENC_MAKE_NAME(SATD)(pCur, pitchPixels, interpBuff, 16, blockSize);
#endif // NO_PADDING

#endif // FRAME_INTERPOLATION

static void H264ENC_MAKE_NAME(ME_SubPel)(ME_InfType *meInfo)
{
    //if (!(meInfo->flags & ANALYSE_ME_SUBPEL))
    //    return;
    __ALIGN16 PIXTYPE interpBuff[256];
#ifdef NO_PADDING
    meInfo->interpolateInfo.pDst[0] = interpBuff;
    meInfo->interpolateInfo.sizeBlock = meInfo->block;
#endif // NO_PADDING
    PIXTYPE *pCur = meInfo->pCur;
#ifndef NO_PADDING
    PIXTYPE *pRef = meInfo->pRef;
#endif
  //  PIXTYPE *pCurU = NULL;
  //  PIXTYPE *pCurV = NULL;
  //  PIXTYPE *pRefU = NULL;
  //  PIXTYPE *pRefV = NULL;
    Ipp32s   pitchPixels = meInfo->pitchPixels;
    H264MotionVector predictedMV = meInfo->predictedMV;
    Ipp16s* pRDQM = meInfo->pRDQM;
    Ipp8u* pBitsX = BitsForMV + BITSFORMV_OFFSET - predictedMV.mvx;
    Ipp8u* pBitsY = BitsForMV + BITSFORMV_OFFSET - predictedMV.mvy;
    Ipp8u* pBitsXpr = BitsForMV + BITSFORMV_OFFSET - meInfo->scPredMV.mvx;
    Ipp8u* pBitsYpr = BitsForMV + BITSFORMV_OFFSET - meInfo->scPredMV.mvy;
    Ipp32s subPelAlgo = meInfo->searchAlgo >> 5;
    Ipp32s bSAD = meInfo->bestSAD;
    Ipp32s blockSize = meInfo->block.width + (meInfo->block.height >> 2);
    Ipp32s bMVX = meInfo->bestMV.mvx;
    Ipp32s bMVY = meInfo->bestMV.mvy;
    Ipp32s cSAD, xL, xR, yT, yB;
//    IppiSize chroma_size;
//    Ipp32s chroma_block_size = 0;
//    Ipp16s chroma_mvy_offset = 0;
    bool useSAD = (meInfo->flags & ANALYSE_SAD) || (meInfo->flags & ANALYSE_ME_SUBPEL_SAD);
/*
    if (meInfo->flags & ANALYSE_ME_CHROMA) {
        pCurU = meInfo->pCurU;
        pCurV = meInfo->pCurV;
        pRefU = meInfo->pRefU;
        pRefV = meInfo->pRefV;
        chroma_mvy_offset = meInfo->chroma_mvy_offset;
        if (meInfo->chroma_format_idc == 1) { //420
             chroma_size.width =  meInfo->block.width >> 1;
             chroma_size.height =  meInfo->block.height >> 1;
        } else if (meInfo->chroma_format_idc == 2) { //422
             chroma_size.width =  meInfo->block.width >> 1;
             chroma_size.height =  meInfo->block.height;
        } else {
             chroma_size =  meInfo->block;
        }
        chroma_block_size = chroma_size.width + (chroma_size.height >> 2);
    }
*/
    xL = bMVX - 3;
    xR = bMVX + 3;
    yT = bMVY - 3;
    yB = bMVY + 3;
    if (subPelAlgo == MV_SEARCH_TYPE_SUBPEL_FULL) {
        // full search
        Ipp32s i, j, xC, yC;
        xC = bMVX;
        yC = bMVY;
        for (i = yC - 3; i <= yC + 3; i ++) {
            for (j = xC - 3; j <= xC + 3; j ++) {
                SubCalcSAD(j, i, useSAD);
                if (cSAD < bSAD) {
                    bSAD = cSAD;
                    bMVX = j;
                    bMVY = i;
                }
            }
        }
    } else if (subPelAlgo == MV_SEARCH_TYPE_SUBPEL_HALF) {
        Ipp32s i, j, k, xPos, yPos;
        static const Ipp32s xOff[8] = {-2, 0, 2, -2, 2, -2, 0, 2}, yOff[8] = {-2, -2, -2, 0, 0, 2, 2, 2};
        xPos = bMVX;
        yPos = bMVY;
        for (k = 0; k < 8; k ++) {
            j = xPos + xOff[k];
            i = yPos + yOff[k];
            SubCalcSAD(j, i, useSAD);
            if (cSAD < bSAD) {
                bMVX = j;
                bMVY = i;
                bSAD = cSAD;
            }
        }
    } else if (subPelAlgo == MV_SEARCH_TYPE_SUBPEL_HQ) {
        Ipp32s i, j, k, xPos, yPos;
        static const Ipp32s xOff[8] = {-2, 0, 2, -2, 2, -2, 0, 2}, yOff[8] = {-2, -2, -2, 0, 0, 2, 2, 2};
        xPos = bMVX;
        yPos = bMVY;
        for (k = 0; k < 8; k ++) {
            j = bMVX + xOff[k];
            i = bMVY + yOff[k];
            SubCalcSAD(j, i, useSAD);
            if (cSAD < bSAD) {
                xPos = j;
                yPos = i;
                bSAD = cSAD;
            }
        }
        //static const Ipp32s bdJ[5] = {0, -1, 0, 1, 0}, bdI[5] = {0, 0, -1, 0, 1};
        //static const Ipp32s bdN[5] = {4, 3, 3, 3, 3}, bdA[5][4] = {{1, 2, 3, 4}, {1, 2, 4, 0}, {1, 2, 3, 0}, {2, 3, 4, 0}, {3, 4, 1, 0}};
        static const Ipp32s bdJ[9] = {0, -1, 0, 1, 1, 1, 0, -1, -1}, bdI[9] = {0, -1, -1, -1, 0, 1, 1, 1, 0};
        static const Ipp32s bdN[9] = {8, 5, 3, 5, 3, 5, 3, 5, 3}, bdA[9][8] = {{1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 7, 8, 0, 0, 0}, {1, 2, 3, 0, 0, 0, 0, 0}, {1, 2, 3, 4, 5, 0, 0, 0}, {3, 4, 5, 0, 0, 0, 0, 0}, {3, 4, 5, 6, 7, 0, 0, 0}, {5, 6, 7, 0, 0, 0, 0, 0}, {5, 6, 7, 8, 1, 0, 0, 0}, {7, 8, 1, 0, 0, 0, 0, 0}};
        int l, m, n;
        l = 0;
        for (;;) {
            n = l;
            l = 0;
            for (m = 0; m < bdN[n]; m ++) {
                k = bdA[n][m];
                j = xPos + bdJ[k];
                i = yPos + bdI[k];
                if (j >= xL && j <= xR && i >= yT && i <= yB) {
                    SubCalcSAD(j, i, useSAD);
                    if (cSAD < bSAD) {
                        l = k;
                        bSAD = cSAD;
                    }
                }
            }
            if (l == 0)
                break;
            xPos += bdJ[l];
            yPos += bdI[l];
            //break;
        }
        bMVX = xPos;
        bMVY = yPos;
    } else if (subPelAlgo == MV_SEARCH_TYPE_SUBPEL_DIAMOND) {
        Ipp32s i, j, k, l, m, n, xPos, yPos;
        static const Ipp32s bdJ[5] = {0, -1, 0, 1, 0}, bdI[5] = {0, 0, -1, 0, 1};
        static const Ipp32s bdN[5] = {4, 3, 3, 3, 3}, bdA[5][4] = {{1, 2, 3, 4}, {1, 2, 4, 0}, {1, 2, 3, 0}, {2, 3, 4, 0}, {3, 4, 1, 0}};
        xPos = bMVX;
        yPos = bMVY;
        l = 0;
        if (meInfo->residualPredictionFlag || meInfo->useScPredMV) // SVC case
        {
            for (;;) {
                n = l;
                l = 0;
                for (m = 0; m < bdN[n]; m ++) {
                    k = bdA[n][m];
                    j = xPos + bdJ[k];
                    i = yPos + bdI[k];
                    if (j >= xL && j <= xR && i >= yT && i <= yB) {
                        SubCalcSAD(j, i, useSAD);
                        if (cSAD < bSAD) {
                            l = k;
                            bSAD = cSAD;
                        }
                    }
                }
                if (l == 0)
                    break;
                xPos += bdJ[l];
                yPos += bdI[l];
            }
        }
        else // AVC case
        {
            for (;;) {
                n = l;
                l = 0;
                for (m = 0; m < bdN[n]; m ++) {
                    k = bdA[n][m];
                    j = xPos + bdJ[k];
                    i = yPos + bdI[k];
                    if (j >= xL && j <= xR && i >= yT && i <= yB) {
                        SubCalcSAD_AVC(j, i, useSAD);
                        if (cSAD < bSAD) {
                            l = k;
                            bSAD = cSAD;
                        }
                    }
                }
                if (l == 0)
                    break;
                xPos += bdJ[l];
                yPos += bdI[l];
            }
        }
        bMVX = xPos;
        bMVY = yPos;
    } else { // MV_SEARCH_TYPE_SUBPEL_SQUARE
        Ipp32s i, j, k, l, m, n, xPos, yPos;
        static const Ipp32s bdJ[9] = {0, -1, 0, 1, 1, 1, 0, -1, -1}, bdI[9] = {0, -1, -1, -1, 0, 1, 1, 1, 0};
        static const Ipp32s bdN[9] = {8, 5, 3, 5, 3, 5, 3, 5, 3}, bdA[9][8] = {{1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 7, 8, 0, 0, 0}, {1, 2, 3, 0, 0, 0, 0, 0}, {1, 2, 3, 4, 5, 0, 0, 0}, {3, 4, 5, 0, 0, 0, 0, 0}, {3, 4, 5, 6, 7, 0, 0, 0}, {5, 6, 7, 0, 0, 0, 0, 0}, {5, 6, 7, 8, 1, 0, 0, 0}, {7, 8, 1, 0, 0, 0, 0, 0}};
        xPos = bMVX;
        yPos = bMVY;
        /*
        if (((predictedMV.mvx & (~3)) == bMVX) && ((predictedMV.mvy & (~3)) == bMVY)) {
            if ((predictedMV.mvx != bMVX) || (predictedMV.mvy != bMVY)) {
                SubCalcSAD(predictedMV.mvx, predictedMV.mvy, useSAD);
                if (cSAD < bSAD) {
                    bSAD = cSAD;
                    xPos = predictedMV.mvx;
                    yPos = predictedMV.mvy;
                }
            }
        }
        */
        l = 0;
        for (;;) {
            n = l;
            l = 0;
            for (m = 0; m < bdN[n]; m ++) {
                k = bdA[n][m];
                j = xPos + bdJ[k];
                i = yPos + bdI[k];
                if (j >= xL && j <= xR && i >= yT && i <= yB) {
                    SubCalcSAD(j, i, useSAD);
                    if (cSAD < bSAD) {
                        l = k;
                        bSAD = cSAD;
                    }
                }
            }
            if (l == 0)
                break;
            xPos += bdJ[l];
            yPos += bdI[l];
        }
        bMVX = xPos;
        bMVY = yPos;
        /*
        Ipp32s i, j, k, l, m, n, xPos, yPos;
        static const Ipp32s bdJ[9] = {0, -1, 0, 1, 1, 1, 0, -1, -1}, bdI[9] = {0, -1, -1, -1, 0, 1, 1, 1, 0};
        static const Ipp32s bdN[9] = {8, 5, 3, 5, 3, 5, 3, 5, 3}, bdA[9][8] = {{1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 7, 8, 0, 0, 0}, {1, 2, 3, 0, 0, 0, 0, 0}, {1, 2, 3, 4, 5, 0, 0, 0}, {3, 4, 5, 0, 0, 0, 0, 0}, {3, 4, 5, 6, 7, 0, 0, 0}, {5, 6, 7, 0, 0, 0, 0, 0}, {5, 6, 7, 8, 1, 0, 0, 0}, {7, 8, 1, 0, 0, 0, 0, 0}};
//        static const Ipp32s xOff[16] = {-2, 0, 2, -2, 2, -2, 0, 2, -1, 0, 1, 1, 1, 0, -1, -1}, yOff[16] = {-2, -2, -2, 0, 0, 2, 2, 2, -1, -1, -1, 0, 1, 1, 1, 0};
//        static const Ipp32s xOff[16] = {-3, 0, 3, -3, 3, -3, 0, 3, -1, 0, 1, 1, 1, 0, -1, -1}, yOff[16] = {-3, -3, -3, 0, 0, 3, 3, 3, -1, -1, -1, 0, 1, 1, 1, 0};
//        static const Ipp32s xOff[22] = {-1, 0, 1, -2, -1, 0, 1, 2, -3, -2, -1, 1, 2, 3, -2, -1, 0, 1, 2, -1, 0, 1}, yOff[22] = {-2, -2, -2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2};
//        static const Ipp32s xOff[22] = {-1, 1, -2, 0, 2, -3, -1, 1, 3, -2, 2, -3, -1, 1, 3, -2, 0, 2, -1, 1}, yOff[22] = {-3, -3, -2, -2, -2, -1, -1, -1, -1, 0, 0, 1, 1, 1, 1, 2, 2, 2, 3, 3};
//        static const Ipp32s xOff[20] = {-3, 0, 3, -1, 1, -2, 0, 2, -3, -1, 1, 3, -2, 0, 2, -1, 1, -3, 0, 3}, yOff[20] = {-3, -3, -3, -2, -2, -1, -1, -1, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 3};
//        static const Ipp32s xOff[22] = {-2, 0, 2, -1, 1, -2, 2, -1, 1, -2, 0, 2}, yOff[22] = {-2, -2, -2, -1, -1, 0, 0, 1, 1, 2, 2, 2};
        static const Ipp32s xOff[16] = {-2, -1, 0, 1, 2, -2, 2, -2, 2, -2, 2, -2, -1, 0, 1, 2}, yOff[16] = {-2, -2, -2, -2, -2, -1, -1, 0, 0, 1, 1, 2, 2, 2, 2, 2};
        __ALIGN16 Ipp32s sadMap[7][7];
        memset(sadMap, -1, 7*7*sizeof(Ipp32s));
        sadMap[3][3] = bSAD;
        xPos = bMVX;
        yPos = bMVY;
        for (k = 0; k < sizeof(xOff) / sizeof(Ipp32s); k ++) {
            j = bMVX + xOff[k];
            i = bMVY + yOff[k];
            SubCalcSAD(j, i, useSAD);
            sadMap[i - bMVY + 3][j - bMVX + 3] = cSAD;
            if (cSAD < bSAD) {
                xPos = j;
                yPos = i;
                bSAD = cSAD;
            }
        }
        l = 0;
        for (;;) {
            n = l;
            l = 0;
            for (m = 0; m < bdN[n]; m ++) {
                k = bdA[n][m];
                j = xPos + bdJ[k];
                i = yPos + bdI[k];
                if (j >= xL && j <= xR && i >= yT && i <= yB) {
                    if (sadMap[i - bMVY + 3][j - bMVX + 3] == -1) {
                        SubCalcSAD(j, i, useSAD);
                        sadMap[i - bMVY + 3][j - bMVX + 3] = cSAD;
                        if (cSAD < bSAD) {
                            l = k;
                            bSAD = cSAD;
                        }
                    }
                }
            }
            if (l == 0)
                break;
            xPos += bdJ[l];
            yPos += bdI[l];
        }
        bMVX = xPos;
        bMVY = yPos;
        */
    }
    // change SAD to SATD
    if (!(meInfo->flags & ANALYSE_SAD) && (meInfo->flags & ANALYSE_ME_SUBPEL_SAD)) {
        bSAD = MAX_SAD;
        bool metric = false;
        SubCalcSAD(bMVX, bMVY, metric);
        bSAD = cSAD;
    }
    if (meInfo->flags & ANALYSE_ME_CHROMA) {
        //__ALIGN16 PIXTYPE interpBuff[256];
        IppiSize chroma_size;
        PIXTYPE *pCurU = meInfo->pCurU;
        PIXTYPE *pCurV = meInfo->pCurV;
#ifndef NO_PADDING
        PIXTYPE *pRefU = meInfo->pRefU;
#ifndef USE_NV12
        PIXTYPE *pRefV = meInfo->pRefV;
#endif // !USE_NV12
#endif
        Ipp32s chroma_mvy_offset = meInfo->chroma_mvy_offset;

        if (meInfo->chroma_format_idc == 1) { //420
            chroma_size.width =  meInfo->block.width >> 1;
            chroma_size.height =  meInfo->block.height >> 1;
        } else if (meInfo->chroma_format_idc == 2) { //422
            chroma_size.width =  meInfo->block.width >> 1;
            chroma_size.height =  meInfo->block.height;
        } else { //444
            chroma_size =  meInfo->block;
        }
        Ipp32s chroma_block_size = chroma_size.width + (chroma_size.height >> 2);
#ifndef NO_PADDING
        Ipp32s iXType, iYType;
#endif
        H264MotionVector chroma_vec;
        chroma_vec.mvx = meInfo->bestMV.mvx; chroma_vec.mvy = meInfo->bestMV.mvy + (Ipp16s)chroma_mvy_offset;
#ifdef NO_PADDING
         meInfo->interpolateInfoChroma.pDst[0] = interpBuff;
         meInfo->interpolateInfoChroma.pDst[1] = interpBuff + 8;
         if( meInfo->chroma_format_idc == 1) //420
         {
             meInfo->interpolateInfoChroma.pointBlockPos.x = meInfo->interpolateInfo.pointBlockPos.x >> 1;
             meInfo->interpolateInfoChroma.pointBlockPos.y = meInfo->interpolateInfo.pointBlockPos.y >> 1;
             meInfo->interpolateInfoChroma.pointVector.x = chroma_vec.mvx;
             meInfo->interpolateInfoChroma.pointVector.y = chroma_vec.mvy;
         }
         else if( meInfo->chroma_format_idc == 2) //422
         {
             meInfo->interpolateInfoChroma.pointBlockPos.x = meInfo->interpolateInfo.pointBlockPos.x >> 1;
             meInfo->interpolateInfoChroma.pointBlockPos.y = meInfo->interpolateInfo.pointBlockPos.y;
             meInfo->interpolateInfoChroma.pointVector.x = chroma_vec.mvx;
             meInfo->interpolateInfoChroma.pointVector.y = chroma_vec.mvy << 1;
         }
         meInfo->interpolateInfoChroma.sizeBlock = chroma_size;
         H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(&(meInfo->interpolateInfoChroma));
#else // NO_PADDING
        Ipp32s offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo->chroma_format_idc);
#ifdef USE_NV12
        H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefU+offset, pitchPixels, interpBuff, interpBuff+8, 16, iXType, iYType, chroma_size, meInfo->bit_depth_chroma);
#else // USE_NV12
        H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefU+offset, pitchPixels, interpBuff, 16, iXType, iYType, chroma_size, meInfo->bit_depth_chroma);
        H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefV+offset, pitchPixels, interpBuff+8, 16, iXType, iYType, chroma_size, meInfo->bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING

        if (meInfo->residualPredictionFlag) {
          if (meInfo->flags & ANALYSE_SAD) {
              bSAD += SAD_ResPred(meInfo->pResU, 16, interpBuff, 16, chroma_block_size);
              bSAD += SAD_ResPred(meInfo->pResV, 16, interpBuff+8, 16, chroma_block_size);
          } else {
              bSAD += SATD_ResPred(meInfo->pResU, 16, interpBuff, 16, chroma_block_size);
              bSAD += SATD_ResPred(meInfo->pResV, 16, interpBuff+8, 16, chroma_block_size);
          }
        } else {
          if (meInfo->flags & ANALYSE_SAD) {
            bSAD += H264ENC_MAKE_NAME(SAD)(pCurU, pitchPixels, interpBuff, 16, chroma_block_size);
            bSAD += H264ENC_MAKE_NAME(SAD)(pCurV, pitchPixels, interpBuff+8, 16, chroma_block_size);
          } else {
            bSAD += H264ENC_MAKE_NAME(SATD)(pCurU, pitchPixels, interpBuff, 16, chroma_block_size);
            bSAD += H264ENC_MAKE_NAME(SATD)(pCurV, pitchPixels, interpBuff+8, 16, chroma_block_size);
          }
        }
    }
    meInfo->bestMV.mvx = (Ipp16s)bMVX;
    meInfo->bestMV.mvy = (Ipp16s)bMVY;
    meInfo->bestSAD = bSAD;
}

void H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(
    ME_InfType* meInfo,
    H264MotionVector& mv)
{
    if (meInfo->candNum >= ME_MAX_CANDIDATES)
        return;
    Ipp32s cSAD, cSADpr, mvx, mvy;
    Ipp32s i;
    mvx = (mv.mvx + 2) >> SUB_PEL_SHIFT;
    mvy = (mv.mvy + 2) >> SUB_PEL_SHIFT;
    if (mvx < meInfo->xMin)
        mvx = meInfo->xMin;
    if (mvx > meInfo->xMax)
        mvx = meInfo->xMax;
    if (mvy < meInfo->yMin)
        mvy = meInfo->yMin;
    if (mvy > meInfo->yMax)
        mvy = meInfo->yMax;
    for (i = 0; i < meInfo->candNum; i ++)
        if ((meInfo->candMV[i].mvx == mvx) && (meInfo->candMV[i].mvy == mvy))
            return;
    meInfo->candMV[meInfo->candNum].mvx = (Ipp16s)mvx;
    meInfo->candMV[meInfo->candNum].mvy = (Ipp16s)mvy;
    meInfo->candNum ++;

#ifdef NO_PADDING
    PIXTYPE interpBlockBuf[256];
    meInfo->interpolateInfo.pDst[0] = interpBlockBuf;
    meInfo->interpolateInfo.sizeBlock = meInfo->block;
#endif // NO_PADDING
    PIXTYPE *pCur = meInfo->pCur;
#ifndef NO_PADDING
    PIXTYPE *pRef = meInfo->pRef;
#endif
    Ipp32s   pitchPixels = meInfo->pitchPixels;
    H264MotionVector predictedMV = meInfo->predictedMV;
    Ipp16s* pRDQM = meInfo->pRDQM;
//    if (mvx >= meInfo->xMin && mvx <= meInfo->xMax && mvy >= meInfo->yMin && mvy <= meInfo->yMax) {
    if (!(meInfo->flags & ANALYSE_ME_EXT_CANDIDATES)) {
        Ipp32s scPred = 0;
        cSAD = MVConstraint((mvx << SUB_PEL_SHIFT) - predictedMV.mvx, (mvy << SUB_PEL_SHIFT) - predictedMV.mvy, pRDQM);
        if (meInfo->useScPredMV) {
            cSADpr = MVConstraint((mvx << SUB_PEL_SHIFT) - meInfo->scPredMV.mvx, (mvy << SUB_PEL_SHIFT) - meInfo->scPredMV.mvy, pRDQM);
            if(cSADpr < cSAD || meInfo->useScPredMVAlways) {
                cSAD = cSADpr;
                scPred = 1;
            }
        }
        if (cSAD < meInfo->bestSAD) {
            Ipp32s blockSize = meInfo->block.width + (meInfo->block.height >> 2);
#ifdef NO_PADDING
            meInfo->interpolateInfo.pointVector.x = mvx << SUB_PEL_SHIFT;
            meInfo->interpolateInfo.pointVector.y = mvy << SUB_PEL_SHIFT;
            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
            cSAD += H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, meInfo->interpolateInfo.pDst[0], 16, blockSize);
#else // NO_PADDING
            if (meInfo->residualPredictionFlag)
                cSAD += SAD_ResPred(meInfo->pResY, 16, MVADJUST(pRef, pitchPixels, mvx, mvy), pitchPixels, blockSize);
            else
                cSAD += H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, mvx, mvy), pitchPixels, blockSize);
#endif // NO_PADDING
            if (cSAD < meInfo->bestSAD) {
                meInfo->bestSAD = cSAD;
                meInfo->bestMV.mvx = (Ipp16s)mvx;
                meInfo->bestMV.mvy = (Ipp16s)mvy;
                meInfo->bestPredMV = scPred ? meInfo->scPredMV : predictedMV;
            }
        }
    } else {
        int k;
        static const Ipp32s bdJ[5] = {0, -1, 0, 1, 0}, bdI[5] = {0, 0, -1, 0, 1};
        Ipp32s blockSize = meInfo->block.width + (meInfo->block.height >> 2);
        for (k = 0; k < 5; k ++) {
            Ipp32s scPred = 0;
            cSAD = MVConstraint(((mvx + bdJ[k]) << SUB_PEL_SHIFT) - predictedMV.mvx, ((mvy + bdI[k]) << SUB_PEL_SHIFT) - predictedMV.mvy, pRDQM);
            if (meInfo->useScPredMV) {
                cSADpr = MVConstraint(((mvx + bdJ[k]) << SUB_PEL_SHIFT) - meInfo->scPredMV.mvx, ((mvy + bdI[k]) << SUB_PEL_SHIFT) - meInfo->scPredMV.mvy, pRDQM);
                if(cSADpr < cSAD || meInfo->useScPredMVAlways) {
                    cSAD = cSADpr;
                    scPred = 1;
                }
            }
            if (cSAD <= meInfo->bestSAD) {
#ifdef NO_PADDING
                meInfo->interpolateInfo.pointVector.x = (mvx + bdJ[k]) << SUB_PEL_SHIFT;
                meInfo->interpolateInfo.pointVector.y = (mvy + bdI[k]) << SUB_PEL_SHIFT;
                H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo->interpolateInfo));
                cSAD += H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, meInfo->interpolateInfo.pDst[0], 16, blockSize);
#else // NO_PADDING
                if (meInfo->residualPredictionFlag)
                    cSAD += SAD_ResPred(meInfo->pResY, 16, MVADJUST(pRef, pitchPixels, mvx + bdJ[k], mvy + bdI[k]), pitchPixels, blockSize);
                else
                    cSAD += H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, mvx + bdJ[k], mvy + bdI[k]), pitchPixels, blockSize);

#endif // NO_PADDING
                if (cSAD < meInfo->bestSAD) {
                    meInfo->bestSAD = cSAD;
                    meInfo->bestMV.mvx = (Ipp16s)(mvx + bdJ[k]);
                    meInfo->bestMV.mvy = (Ipp16s)(mvy + bdI[k]);
                    meInfo->bestPredMV = scPred ? meInfo->scPredMV : predictedMV;
                    if (meInfo->bestSAD <= meInfo->threshold)
                        break;
                }
            }
        }
    }
}

#define Calc_uFwdRatio(POCoffset, POCdelta) \
    (core_enc->m_is_mvc_profile && core_enc->m_view_id) ? ((POCdelta) ? (POCoffset)/(POCdelta) : 1) : (POCoffset)/(POCdelta)

void H264ENC_MAKE_NAME(H264CoreEncoder_ME_CandList16x16)(
    void* state,
    H264SliceType* curr_slice,
    Ipp32s list_id,
    ME_InfType* meInfo,
    Ipp32s refIdx)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32s uMB = curr_slice->m_cur_mb.uMB;
    //bool bBSlice = BPREDSLICE == curr_slice->m_slice_type;
    //bool is_bwd_pred = (list_id == LIST_1);
    H264EncoderFrameType *pPrevFrm = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, list_id, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[0];
    H264MotionVector mv;

    meInfo->bestSAD = MAX_SAD;
    meInfo->candNum = 0;
    // predicted, calculated already
    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, meInfo->predictedMV);

    if(core_enc->m_Analyse_ex & ANALYSE_SUPERFAST) return; // Superfast mode early exit

    //if (meInfo->bestSAD <= meInfo->threshold)
    //    return;
    //current after FrameTypeDetection
    if (core_enc->m_Analyse & ANALYSE_FRAME_TYPE)
        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][uMB].MotionVectors[0]);
    //if (meInfo->bestSAD <= meInfo->threshold)
    //    return;
    // zero vector
    mv = null_mv;
    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, mv);
    //if (meInfo->bestSAD <= meInfo->threshold)
    //    return;
    // above MV
    Ipp32s above_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_B;
    if (above_addr >= 0) {
        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[list_id][above_addr].MotionVectors[12]);
        //if (meInfo->bestSAD <= meInfo->threshold)
        //    return;
    }
    // left MV
    Ipp32s left_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_A;
    if (left_addr >= 0) {
        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[list_id][left_addr].MotionVectors[3]);
        //if (meInfo->bestSAD <= meInfo->threshold)
        //    return;
    }
    // topleft MV
    Ipp32s topleft_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_D;
    if (topleft_addr >= 0) {
        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[list_id][topleft_addr].MotionVectors[15]);
        //if (meInfo->bestSAD <= meInfo->threshold)
        //    return;
    }
    // topright MV
    Ipp32s topright_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_C;
    if (topright_addr >= 0) {
        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[list_id][topright_addr].MotionVectors[12]);
        //if (meInfo->bestSAD <= meInfo->threshold)
        //    return;
    }
    if (curr_slice->m_is_cur_mb_field || (core_enc->m_info.coding_type == 3 && pPrevFrm->m_PictureStructureForDec < FRM_STRUCTURE))
        return;
    const Ipp32s POCcur = H264ENC_MAKE_NAME(H264EncoderFrame_PicOrderCnt)(core_enc->m_pCurrentFrame, 0, 3);
    const Ipp32s POCref = H264ENC_MAKE_NAME(H264EncoderFrame_PicOrderCnt)(H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, list_id, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx], 0, 3);
    const Ipp32s POCoffset = 256*(POCcur-POCref);
    const Ipp32s POCL0 = H264ENC_MAKE_NAME(H264EncoderFrame_PicOrderCnt)(pPrevFrm, 0, 3);
//    Ipp32s uFwdRatio;
    Ipp32s POCrefL0 = 0;
    Ipp32s POCoffsetL0 = 0;
    Ipp32s POCL0L0 = 0;
    H264EncoderFrameType *pPrevFrmL0 = NULL;
    H264EncoderFrameType *pEchoFrmL0 = NULL; // pointer to ref frame L0[refIdx] (used for list_id == LIST_1 only)
    if( list_id == LIST_1 ){
        pEchoFrmL0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx];
        if (pEchoFrmL0) {
            POCrefL0 = H264ENC_MAKE_NAME(H264EncoderFrame_PicOrderCnt)(pEchoFrmL0, 0, 3);
            pPrevFrmL0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[0];
            POCoffsetL0 = 256*(POCcur-POCrefL0);
            POCL0L0 = H264ENC_MAKE_NAME(H264EncoderFrame_PicOrderCnt)(pPrevFrmL0, 0, 3);
        }
     }
#if 1
    // ref top
    if (curr_slice->m_cur_mb.uMBy > 0) {
        Ipp32s top_addr = uMB - core_enc->m_WidthInMBs;
        const H264MotionVector &top = pPrevFrm->m_mbinfo.MV[LIST_0][top_addr].MotionVectors[12];
        Ipp32s addMV = 1;
//        if (!bBSlice) {
//            mv = top;
//        } else {
            // Forward vector, needs to be scaled, but direction is correct
            const T_RefIdx &refId = pPrevFrm->m_mbinfo.RefIdxs[LIST_0][top_addr].RefIdxs[12];
            if( refId >= 0 && !top.is_zero()){
                const Ipp32s POCL0ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(pPrevFrm, (pPrevFrm->m_mbinfo.mbs + top_addr)->slice_id, LIST_0)->m_POC[refId];
//                const Ipp32s POCL0  = pPrevFrm->PicOrderCnt(0,3);
//                const Ipp32s POCref  = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, list_id, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx]->PicOrderCnt(0,3);
//                const Ipp32s uFwdRatio = curr_slice->DistScaleFactorMV[refId];
                const Ipp32s uFwdRatio = Calc_uFwdRatio(POCoffset, POCL0 - POCL0ref);
                mv.mvx = (Ipp16s) ((uFwdRatio * top.mvx + TR_RND) >> TR_SHIFT);
                mv.mvy = (Ipp16s) ((uFwdRatio * top.mvy + TR_RND) >> TR_SHIFT);
            }else if( list_id == LIST_1 && pEchoFrmL0 ){ //Try by L0
//                H264EncoderFrameType *pPrevFrm = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[0];
                const H264MotionVector &topl1 = pPrevFrmL0->m_mbinfo.MV[LIST_0][top_addr].MotionVectors[12];
                const T_RefIdx &refIdl1 = pPrevFrmL0->m_mbinfo.RefIdxs[LIST_0][top_addr].RefIdxs[12];
                if( refIdl1 >= 0 && !topl1.is_zero()){
                    const Ipp32s POCL0ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(pPrevFrmL0, (pPrevFrm->m_mbinfo.mbs + top_addr)->slice_id, LIST_0)->m_POC[refIdl1];
//                    const Ipp32s POCL0  = pPrevFrmL0->PicOrderCnt(0,3);
//                    const Ipp32s POCref  = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx]->PicOrderCnt(0,3);
                    const Ipp32s uFwdRatio = Calc_uFwdRatio(POCoffsetL0,POCL0L0 - POCL0ref);
                    mv.mvx = (Ipp16s) ((uFwdRatio * topl1.mvx + TR_RND) >> TR_SHIFT);
                    mv.mvy = (Ipp16s) ((uFwdRatio * topl1.mvy + TR_RND) >> TR_SHIFT);
                    // Backward vector, needs both scaling and direction changed derived the backward MV from the reference MV: MVb = MVf-MV = -(1-r1)*MV
                    mv.mvx = (Ipp16s)(mv.mvx - topl1.mvx);
                    mv.mvy = (Ipp16s)(mv.mvy - topl1.mvy);
                }else addMV = 0;
            }else addMV = 0;
//        }
          if (addMV) {
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, mv);
            //if (meInfo->bestSAD <= meInfo->threshold)
            //    return;
          }
    }
    // ref left
    if (curr_slice->m_cur_mb.uMBx > 0) {
        /*Ipp32s*/ left_addr = uMB - 1;
        const H264MotionVector &left = pPrevFrm->m_mbinfo.MV[LIST_0][left_addr].MotionVectors[3];
        Ipp32s addMV = 1;
//        if (!bBSlice) {
//            mv = left;
//        } else {
            // Forward vector, needs to be scaled, but direction is correct
            const T_RefIdx &refId = pPrevFrm->m_mbinfo.RefIdxs[LIST_0][left_addr].RefIdxs[3];
            if( refId >= 0 && !left.is_zero()){
                const Ipp32s POCL0ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(pPrevFrm, (pPrevFrm->m_mbinfo.mbs + left_addr)->slice_id, LIST_0)->m_POC[refId];
//                const Ipp32s POCL0  = pPrevFrm->PicOrderCnt(0,3);
//                const Ipp32s POCref  = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, list_id, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx]->PicOrderCnt(0,3);
//                const Ipp32s uFwdRatio = curr_slice->DistScaleFactorMV[refId];
                const Ipp32s uFwdRatio = Calc_uFwdRatio(POCoffset, POCL0 - POCL0ref);
                mv.mvx = (Ipp16s) ((uFwdRatio * left.mvx + TR_RND) >> TR_SHIFT);
                mv.mvy = (Ipp16s) ((uFwdRatio * left.mvy + TR_RND) >> TR_SHIFT);
            }else if( list_id == LIST_1 && pEchoFrmL0){ //Try by L0
//                H264EncoderFrameType *pPrevFrm = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[0];
                const H264MotionVector &leftl1 = pPrevFrmL0->m_mbinfo.MV[LIST_0][left_addr].MotionVectors[3];
                const T_RefIdx &refIdl1 = pPrevFrmL0->m_mbinfo.RefIdxs[LIST_0][left_addr].RefIdxs[3];
                if (refIdl1 >= 0 && !leftl1.is_zero())
                {
                    const Ipp32s POCL0ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(pPrevFrmL0, (pPrevFrm->m_mbinfo.mbs + left_addr)->slice_id, LIST_0)->m_POC[refIdl1];
//                    const Ipp32s POCL0  = pPrevFrmL0->PicOrderCnt(0,3);
//                    const Ipp32s POCref  = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx]->PicOrderCnt(0,3);
                    const Ipp32s uFwdRatio = Calc_uFwdRatio(POCoffsetL0,POCL0L0 - POCL0ref);
                    mv.mvx = (Ipp16s) ((uFwdRatio * leftl1.mvx + TR_RND) >> TR_SHIFT);
                    mv.mvy = (Ipp16s) ((uFwdRatio * leftl1.mvy + TR_RND) >> TR_SHIFT);
                    // Backward vector, needs both scaling and direction changed derived the backward MV from the reference MV: MVb = MVf-MV = -(1-r1)*MV
                    mv.mvx = (Ipp16s)(mv.mvx - leftl1.mvx);
                    mv.mvy = (Ipp16s)(mv.mvy - leftl1.mvy);
                }else addMV = 0;
            }else addMV = 0;
//        }
          if (addMV) {
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, mv);
            //if (meInfo->bestSAD <= meInfo->threshold)
            //    return;
          }
    }
    // ref bottom
    if (curr_slice->m_cur_mb.uMBy < core_enc->m_HeightInMBs - 1) {
        Ipp32s bottom_addr = uMB + core_enc->m_WidthInMBs;
        const H264MotionVector &bottom = pPrevFrm->m_mbinfo.MV[LIST_0][bottom_addr].MotionVectors[0];
        Ipp32s addMV = 1;
//        if (!bBSlice) {
//            mv = bottom;
//        } else {
            // Forward vector, needs to be scaled, but direction is correct
            const T_RefIdx &refId = pPrevFrm->m_mbinfo.RefIdxs[LIST_0][bottom_addr].RefIdxs[0];
            if( refId >= 0 && !bottom.is_zero()){
                const Ipp32s POCL0ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(pPrevFrm, (pPrevFrm->m_mbinfo.mbs + bottom_addr)->slice_id, LIST_0)->m_POC[refId];
//                const Ipp32s POCL0  = pPrevFrm->PicOrderCnt(0,3);
//                const Ipp32s POCref  = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, list_id, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx]->PicOrderCnt(0,3);
//                const Ipp32s uFwdRatio = curr_slice->DistScaleFactorMV[refId];
                const Ipp32s uFwdRatio = Calc_uFwdRatio(POCoffset, POCL0 - POCL0ref);
                mv.mvx = (Ipp16s) ((uFwdRatio * bottom.mvx + TR_RND) >> TR_SHIFT);
                mv.mvy = (Ipp16s) ((uFwdRatio * bottom.mvy + TR_RND) >> TR_SHIFT);
            }else if( list_id == LIST_1 && pEchoFrmL0){ //Try by L0
//                H264EncoderFrameType *pPrevFrm = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[0];
                const H264MotionVector &bottoml1 = pPrevFrmL0->m_mbinfo.MV[LIST_0][bottom_addr].MotionVectors[0];
                const T_RefIdx &refIdl1 = pPrevFrmL0->m_mbinfo.RefIdxs[LIST_0][bottom_addr].RefIdxs[0];
                if( refIdl1 >= 0 && !bottoml1.is_zero()){
                    const Ipp32s POCL0ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(pPrevFrmL0, (pPrevFrm->m_mbinfo.mbs + bottom_addr)->slice_id, LIST_0)->m_POC[refIdl1];
//                    const Ipp32s POCL0  = pPrevFrmL0->PicOrderCnt(0,3);
//                    const Ipp32s POCref  = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx]->PicOrderCnt(0,3);
                    const Ipp32s uFwdRatio = Calc_uFwdRatio(POCoffsetL0,POCL0L0 - POCL0ref);
                    mv.mvx = (Ipp16s) ((uFwdRatio * bottoml1.mvx + TR_RND) >> TR_SHIFT);
                    mv.mvy = (Ipp16s) ((uFwdRatio * bottoml1.mvy + TR_RND) >> TR_SHIFT);
                    // Backward vector, needs both scaling and direction changed derived the backward MV from the reference MV: MVb = MVf-MV = -(1-r1)*MV
                    mv.mvx = (Ipp16s)(mv.mvx - bottoml1.mvx);
                    mv.mvy = (Ipp16s)(mv.mvy - bottoml1.mvy);
                }else addMV = 0;
            }else addMV = 0;
//        }
          if (addMV) {
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, mv);
            //if (meInfo->bestSAD <= meInfo->threshold)
            //    return;
          }
    }
    // ref right
    if (curr_slice->m_cur_mb.uMBx < core_enc->m_WidthInMBs - 1) {
        Ipp32s right_addr = uMB + 1;
        const H264MotionVector &right = pPrevFrm->m_mbinfo.MV[LIST_0][right_addr].MotionVectors[0];
        Ipp32s addMV = 1;
//        if (!bBSlice) {
//            mv = right;
//        } else {
            // Forward vector, needs to be scaled, but direction is correct
            const T_RefIdx &refId = pPrevFrm->m_mbinfo.RefIdxs[LIST_0][right_addr].RefIdxs[0];
            if( refId >= 0 && !right.is_zero()){
//                const Ipp32s uFwdRatio = curr_slice->DistScaleFactorMV[refId];
                const Ipp32s POCL0ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(pPrevFrm, (pPrevFrm->m_mbinfo.mbs + right_addr)->slice_id, LIST_0)->m_POC[refId];
//                const Ipp32s POCL0  = pPrevFrm->PicOrderCnt(0,3);
//                const Ipp32s POCref  = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, list_id, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx]->PicOrderCnt(0,3);
                const Ipp32s uFwdRatio = Calc_uFwdRatio(POCoffset, POCL0 - POCL0ref);
                mv.mvx = (Ipp16s) ((uFwdRatio * right.mvx + TR_RND) >> TR_SHIFT);
                mv.mvy = (Ipp16s) ((uFwdRatio * right.mvy + TR_RND) >> TR_SHIFT);
            }else if( list_id == LIST_1 && pEchoFrmL0){ //Try by L0
//                H264EncoderFrameType *pPrevFrm = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[0];
                const H264MotionVector &rightl1 = pPrevFrmL0->m_mbinfo.MV[LIST_0][right_addr].MotionVectors[0];
                const T_RefIdx &refIdl1 = pPrevFrmL0->m_mbinfo.RefIdxs[LIST_0][right_addr].RefIdxs[0];
                if( refIdl1 >= 0 && !rightl1.is_zero()){
                    const Ipp32s POCL0ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(pPrevFrmL0, (pPrevFrm->m_mbinfo.mbs + right_addr)->slice_id, LIST_0)->m_POC[refIdl1];
//                    const Ipp32s POCL0  = pPrevFrmL0->PicOrderCnt(0,3);
//                    const Ipp32s POCref  = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx]->PicOrderCnt(0,3);
                    const Ipp32s uFwdRatio = Calc_uFwdRatio(POCoffsetL0,POCL0L0 - POCL0ref);
                    mv.mvx = (Ipp16s) ((uFwdRatio * rightl1.mvx + TR_RND) >> TR_SHIFT);
                    mv.mvy = (Ipp16s) ((uFwdRatio * rightl1.mvy + TR_RND) >> TR_SHIFT);
                    // Backward vector, needs both scaling and direction changed derived the backward MV from the reference MV: MVb = MVf-MV = -(1-r1)*MV
                    mv.mvx = (Ipp16s)(mv.mvx - rightl1.mvx);
                    mv.mvy = (Ipp16s)(mv.mvy - rightl1.mvy);
                }else addMV = 0;
            }else addMV = 0;
//        }
          if (addMV) {
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, mv);
            //if (meInfo->bestSAD <= meInfo->threshold)
            //    return;
          }
    }
#endif //1
    // ref current
    H264MotionVector fwd_mv = pPrevFrm->m_mbinfo.MV[LIST_0][uMB].MotionVectors[0];
    Ipp32s addMV = 1;
//    if (!bBSlice) {
//        mv = fwd_mv;
//    } else {
        // Forward vector, needs to be scaled, but direction is correct
        const T_RefIdx &refId = pPrevFrm->m_mbinfo.RefIdxs[LIST_0][uMB].RefIdxs[0];
        if( refId >= 0 && !fwd_mv.is_zero()){
            const Ipp32s POCL0ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(pPrevFrm, (pPrevFrm->m_mbinfo.mbs + uMB)->slice_id, LIST_0)->m_POC[refId];
//            const Ipp32s POCL0  = pPrevFrm->PicOrderCnt(0,3);
//            const Ipp32s POCref  = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, list_id, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx]->PicOrderCnt(0,3);
            const Ipp32s uFwdRatio = Calc_uFwdRatio(POCoffset, POCL0 - POCL0ref);
            //const Ipp32s uFwdRatio = curr_slice->DistScaleFactorMV[refId];
            mv.mvx = (Ipp16s) ((uFwdRatio * fwd_mv.mvx + TR_RND) >> TR_SHIFT);
            mv.mvy = (Ipp16s) ((uFwdRatio * fwd_mv.mvy + TR_RND) >> TR_SHIFT);
        }else if( list_id == LIST_1 && pEchoFrmL0){ //Try by L0
//                H264EncoderFrameType *pPrevFrm = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[0];
                const H264MotionVector &fwd_mvl1 = pPrevFrmL0->m_mbinfo.MV[LIST_0][uMB].MotionVectors[0];
                const T_RefIdx &refIdl1 = pPrevFrmL0->m_mbinfo.RefIdxs[LIST_0][uMB].RefIdxs[0];
                if( refIdl1 >= 0 && !fwd_mvl1.is_zero()){
                    const Ipp32s POCL0ref = H264ENC_MAKE_NAME(H264EncoderFrame_GetRefPicList)(pPrevFrmL0, (pPrevFrm->m_mbinfo.mbs + uMB)->slice_id, LIST_0)->m_POC[refIdl1];
//                    const Ipp32s POCL0  = pPrevFrmL0->PicOrderCnt(0,3);
//                    const Ipp32s POCref  = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, uMB & 1)->m_RefPicList[refIdx]->PicOrderCnt(0,3);
                    const Ipp32s uFwdRatio = Calc_uFwdRatio(POCoffsetL0,POCL0L0 - POCL0ref);
                    mv.mvx = (Ipp16s) ((uFwdRatio * fwd_mvl1.mvx + TR_RND) >> TR_SHIFT);
                    mv.mvy = (Ipp16s) ((uFwdRatio * fwd_mvl1.mvy + TR_RND) >> TR_SHIFT);
                    // Backward vector, needs both scaling and direction changed derived the backward MV from the reference MV: MVb = MVf-MV = -(1-r1)*MV
                    mv.mvx = (Ipp16s)(mv.mvx - fwd_mvl1.mvx);
                    mv.mvy = (Ipp16s)(mv.mvy - fwd_mvl1.mvy);
               }else addMV = 0;
        }else addMV = 0;
//    }
      if (addMV) {
          H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(meInfo, mv);
          //if (meInfo->bestSAD <= meInfo->threshold)
          //    return;
      }
}

#ifdef USE_NV12
static IppStatus ippiSumsDiff8x8Blocks4x4_NV12 (const Ipp8u *pSrc,
                                                      Ipp32s SrcPitch,
                                                      const Ipp8u *pPred,
                                                      Ipp32s PredPitch,
                                                      Ipp16s *pDC,
                                                      Ipp16s *pDiff)
{
    Ipp32u i, j, m, n;
    /* compute 2x2 DC coeffs */
    if (pDiff)
    {
        for (i = 0; i < 2; i ++)
        {
            for (j = 0; j < 2; j ++)
            {
                pDC[j] = 0;

                for (m = 0; m < 4; m ++)
                {
                    for (n = 0; n < 4; n ++)
                    {
                        Ipp32s temp = pSrc[m * SrcPitch + 2*n] - pPred[m * PredPitch + n];
                        pDC[j] = (Ipp16s) (pDC[j] + temp);
                        pDiff[i*32+j*16+m*4+n] = (Ipp16s) temp;
                    }
                }
                pPred += 4;
                pSrc += 8;
            }
            pSrc += 4 * SrcPitch - 16;
            pPred += 4 * PredPitch - 8;
            pDC += 2;
        }
    }
    else
    {
        for (i = 0; i < 2; i ++)
        {
            for (j = 0; j < 2; j ++)
            {
                pDC[j] = 0;

                for (m = 0; m < 4; m ++)
                {
                    for (n = 0; n < 4; n ++)
                    {
                        Ipp32s temp = pSrc[m * SrcPitch + 2*n] - pPred[m * PredPitch + n];
                        pDC[j] = (Ipp16s) (pDC[j] + temp);
                    }
                }
                pPred += 4;
                pSrc += 8;
            }
            pSrc += 4 * SrcPitch - 16;
            pPred += 4 * PredPitch - 8;
            pDC += 2;
        }

    }

    return (IppStatus)0;

} /* IPPFUN(IppStatus, ippiSumsDiff8x8Blocks4x4_8u16s_C1, (Ipp8u *pSrc, */
#endif

bool H264ENC_MAKE_NAME(H264CoreEncoder_CheckSkip)(
    void* state,
    H264SliceType *curr_slice,
    H264MotionVector &skip_vec,
    Ipp32s residualPrediction,
    Ipp16s *pResBuf,
    Ipp16s *pResBufC)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    H264EncoderFrameType **pRefPicList0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, cur_mb.uMB & 1)->m_RefPicList;
    Ipp8s *pFields0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, cur_mb.uMB & 1)->m_Prediction;
    PIXTYPE *const pInterpBuf = curr_slice->m_pMBEncodeBuffer;
    Ipp16s* pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
    COEFFSTYPE *pTransformResult = (COEFFSTYPE*)(pDiffBuf + 16);
    T_EncodeMBOffsets *pMBOffset = &core_enc->m_pMBOffsets[cur_mb.uMB];
    Ipp32s pitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels<<curr_slice->m_is_cur_mb_field;
    PIXTYPE* pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
    Ipp32u offset = core_enc->m_pMBOffsets[cur_mb.uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
    Ipp32s   iNumCoeffs, iLastCoeff, iXType, iYType;

    PIXTYPE *pRef = pRefPicList0[0]->m_pYPlane + offset + curr_slice->m_InitialOffset[pFields0[0]];
#ifdef FRAME_INTERPOLATION
    Ipp32s planeSize = pRefPicList0[0]->m_PlaneSize;
#else //FRAME_INTERPOLATION
#ifndef NO_PADDING
    Ipp32s planeSize = 0;
#endif
#endif //FRAME_INTERPOLATION

#ifdef NO_PADDING
#if PIXBITS == 8
    IppVCInterpolateBlock_8u interpolateInfo, *pInterpolateInfo = &interpolateInfo;
#elif PIXBITS == 16
    IppVCInterpolateBlock_16u interpolateInfo, *pInterpolateInfo = &interpolateInfo;
#endif //PIXBITS
    pInterpolateInfo->pSrc[0] = pRefPicList0[0]->m_pYPlane + curr_slice->m_InitialOffset[pFields0[0]] + (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
    pInterpolateInfo->srcStep = pitchPixels;
    pInterpolateInfo->pDst[0] = pInterpBuf;
    pInterpolateInfo->dstStep = 16;
    pInterpolateInfo->sizeFrame.width = core_enc->m_WidthInMBs << 4;
    pInterpolateInfo->sizeFrame.height = core_enc->m_HeightInMBs << 4;
    pInterpolateInfo->sizeBlock = size16x16;
    pInterpolateInfo->pointBlockPos.x = cur_mb.uMBx << 4;
    pInterpolateInfo->pointBlockPos.y = cur_mb.uMBy << 4;
    pInterpolateInfo->pointVector.x = skip_vec.mvx;
    pInterpolateInfo->pointVector.y = skip_vec.mvy;
    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
    H264ENC_MAKE_NAME(InterpolateLuma)(MVADJUST(pRef, pitchPixels, skip_vec.mvx >> SUB_PEL_SHIFT, skip_vec.mvy >> SUB_PEL_SHIFT), pitchPixels, pInterpBuf, 16, skip_vec.mvx&3, skip_vec.mvy&3, size16x16, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
    Ipp32s coeffs_cost = 0;
    Ipp32u uMBQP  = cur_mb.lumaQP;
    Ipp32s  chromaQP = cur_mb.chromaQP;
    // code block
    for (Ipp32s uBlock = 0; uBlock < 16; uBlock ++) {
        PIXTYPE* pPredBuf = pInterpBuf + xoff[uBlock] + yoff[uBlock]*16;
        Ipp16s *pRes = pResBuf + xoff[uBlock] + yoff[uBlock]*16;

        if (residualPrediction) {
          Diff4x4_ResPred(pRes, pPredBuf, pDiffBuf);
        } else {
          Diff4x4(pPredBuf, pSrcPlane + offset, pitchPixels, pDiffBuf);
        }
        H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
            pDiffBuf,
            pTransformResult,
            (Ipp32s)uMBQP,
            &iNumCoeffs,
            1,
            enc_single_scan[curr_slice->m_is_cur_mb_field],
            &iLastCoeff,
            NULL,
            0,
            0,
            NULL);

        coeffs_cost += iNumCoeffs
            ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 16, dec_single_scan[curr_slice->m_is_cur_mb_field])
            : 0;
        if (coeffs_cost >= 6) return false;
        offset += core_enc->m_EncBlockOffsetInc[curr_slice->m_is_cur_mb_field][uBlock];
    }
    if (core_enc->m_PicParamSet->chroma_format_idc != 0) {
        offset = pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
        H264MotionVector chroma_skip_vec = skip_vec;
        if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
            if (!curr_slice->m_is_cur_mb_bottom_field && pFields0[0])  chroma_skip_vec.mvy += - 2;
            else if (curr_slice->m_is_cur_mb_bottom_field && !pFields0[0]) chroma_skip_vec.mvy += 2;
        }
        IppiSize chroma_size = {0, 0};
        if( core_enc->m_PicParamSet->chroma_format_idc == 1){ //420
            chroma_size =  size8x8;
        }else if( core_enc->m_PicParamSet->chroma_format_idc == 2){ //422
            chroma_size =  size8x16;
        }
        COEFFSTYPE* pQBuf    = (COEFFSTYPE*) (pTransformResult + 16);
        COEFFSTYPE* pDCBuf   = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
        Ipp16s* pMassDiffBuf = (Ipp16s*) (pDCBuf + 16);
        Ipp32s start_block = 16, last_block = 20;
        if(core_enc->m_PicParamSet->chroma_format_idc == 2) start_block = 16, last_block = 24;
        pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane;
        pRef = pRefPicList0[0]->m_pUPlane + offset + curr_slice->m_InitialOffset[pFields0[0]];
        Ipp32s mv_offset = SubpelChromaMVAdjust(&chroma_skip_vec, pitchPixels, iXType, iYType, core_enc->m_PicParamSet->chroma_format_idc);
#ifdef USE_NV12
#ifdef NO_PADDING
        pInterpolateInfo->pSrc[0] = pRefPicList0[0]->m_pUPlane + curr_slice->m_InitialOffset[pFields0[0]] + (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
        pInterpolateInfo->pSrc[1] = pRefPicList0[0]->m_pVPlane + curr_slice->m_InitialOffset[pFields0[0]] + (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
        pInterpolateInfo->pDst[0] = pInterpBuf;
        pInterpolateInfo->pDst[1] = pInterpBuf + 8;
        if (core_enc->m_PicParamSet->chroma_format_idc == 1) //420
        {
            pInterpolateInfo->sizeFrame.width >>= 1;
            pInterpolateInfo->sizeFrame.height >>= 1;
            pInterpolateInfo->pointBlockPos.x >>= 1;
            pInterpolateInfo->pointBlockPos.y >>= 1;
        }
        else if (core_enc->m_PicParamSet->chroma_format_idc == 2) //422
        {
            pInterpolateInfo->sizeFrame.width >>= 1;
            pInterpolateInfo->pointBlockPos.x >>= 1;
        }
        pInterpolateInfo->sizeBlock = chroma_size;
        pInterpolateInfo->pointVector.x = chroma_skip_vec.mvx;
        pInterpolateInfo->pointVector.y = chroma_skip_vec.mvy;
        H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
        pRef += mv_offset;
        H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRef, pitchPixels, pInterpBuf, pInterpBuf + 8, 16, iXType, iYType, chroma_size, core_enc->m_SeqParamSet->bit_depth_chroma);
#endif // NO_PADDING
        for (Ipp32s i = 0; i < 2; i++) {
            if (residualPrediction) {
                Ipp16s *pDC = pDCBuf;
                Ipp32s ii, jj, m, n;
                Ipp16s *pSrc = pResBufC + i*8;
                PIXTYPE *pPred = pInterpBuf + i*8;
                for (ii = 0; ii < 2; ii++) {
                  for (jj = 0; jj < 2; jj++) {
                    pDC[jj] = 0;
                    for (m = 0; m < 4; m++) {
                      for (n = 0; n < 4; n++) {
                        Ipp32s temp = pSrc[m*16 + n] - pPred[m*16 + n];
                        pDC[jj] = (Ipp16s)(pDC[jj] + temp);
                        pMassDiffBuf[ii*32 + jj*16 + m*4 + n] = (Ipp16s) temp;
                      }
                    }
                    pPred += 4;
                    pSrc += 4;
                  }
                  pSrc += 4 * 16 - 8;
                  pPred += 4 * 16 - 8;
                  pDC += 2;
                }
            } else {
              if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
                  H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + offset, pitchPixels, pInterpBuf + i*8, 16, pDCBuf, pMassDiffBuf);
              else
                  ippiSumsDiff8x8Blocks4x4_NV12(pSrcPlane + offset, pitchPixels, pInterpBuf + i*8, 16, pDCBuf, pMassDiffBuf);
            }
            if (core_enc->m_PicParamSet->chroma_format_idc == 2)
                H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + offset + 8 * pitchPixels, pitchPixels, pInterpBuf + 8 * 16 + i*8, 16, pDCBuf + 4, pMassDiffBuf + 64);
            // 2x2 forward transform
            switch( core_enc->m_PicParamSet->chroma_format_idc ){
                case 1:
                    H264ENC_MAKE_NAME(ippiTransformQuantChromaDC_H264)(pDCBuf, pQBuf, chromaQP, &iNumCoeffs, 0, 1, NULL);
                    break;
                case 2:
                    H264ENC_MAKE_NAME(ippiTransformQuantChroma422DC_H264)(pDCBuf, pQBuf, chromaQP, &iNumCoeffs, 0, 1, NULL);
                    break;
            }
            if (pDCBuf[0] || pDCBuf[1] || pDCBuf[2] || pDCBuf[3]) return false;
            if( core_enc->m_PicParamSet->chroma_format_idc == 2 && (pDCBuf[4] || pDCBuf[5] || pDCBuf[6] || pDCBuf[7]) ) return false;
            coeffs_cost = 0;
            for (Ipp32s uBlock = start_block; uBlock < last_block; uBlock ++) {
                Ipp16s* pTempDiffBuf = pMassDiffBuf+(uBlock - start_block)*16;
                H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                    pTempDiffBuf,
                    pTransformResult,
                    chromaQP,
                    &iNumCoeffs,
                    0,
                    enc_single_scan[curr_slice->m_is_cur_mb_field],
                    &iLastCoeff,
                    NULL,
                    0,
                    0,
                    NULL);

                coeffs_cost += iNumCoeffs
                    ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 15, &dec_single_scan[curr_slice->m_is_cur_mb_field][1])
                    : 0;
                if (coeffs_cost >= 7) return false;
                offset += core_enc->m_EncBlockOffsetInc[curr_slice->m_is_cur_mb_field][uBlock];
            }
            if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                start_block = 20;
                last_block = 24;
            }else if( core_enc->m_PicParamSet->chroma_format_idc == 2 ){
                start_block = 24;
                last_block = 32;
            }
            if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
                pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane + 8;
            else
                pSrcPlane = core_enc->m_pCurrentFrame->m_pVPlane;
            offset = pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
        }
#else // USE_NV12
#ifdef NO_PADDING
        pInterpolateInfo->pSrc[0] = pRefPicList0[0]->m_pUPlane + curr_slice->m_InitialOffset[pFields0[0]] + (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
        pInterpolateInfo->pSrc[1] = pInterpolateInfo->pSrc[0];
        pInterpolateInfo->pDst[1] = pInterpolateInfo->pDst[0];
        if (core_enc->m_PicParamSet->chroma_format_idc == 1) //420
        {
            pInterpolateInfo->sizeFrame.width >>= 1;
            pInterpolateInfo->sizeFrame.height >>= 1;
            pInterpolateInfo->pointBlockPos.x >>= 1;
            pInterpolateInfo->pointBlockPos.y >>= 1;
        }
        else if (core_enc->m_PicParamSet->chroma_format_idc == 2) //422
        {
            pInterpolateInfo->sizeFrame.width >>= 1;
            pInterpolateInfo->pointBlockPos.x >>= 1;
        }
        pInterpolateInfo->sizeBlock = chroma_size;
        pInterpolateInfo->pointVector.x = chroma_skip_vec.mvx;
        pInterpolateInfo->pointVector.y = chroma_skip_vec.mvy;
#endif // NO_PADDING
        for (Ipp32s i = 0; i < 2 ; i++) {
            pRef += mv_offset;
#ifdef NO_PADDING
            H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
            H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRef, pitchPixels, pInterpBuf, 16, iXType, iYType, chroma_size, core_enc->m_SeqParamSet->bit_depth_chroma);
#endif
            H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + offset, pitchPixels, pInterpBuf, 16, pDCBuf, pMassDiffBuf);
            if (core_enc->m_PicParamSet->chroma_format_idc == 2)
                H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + offset + 8 * pitchPixels, pitchPixels, pInterpBuf + 8 * 16, 16, pDCBuf + 4, pMassDiffBuf + 64);
            // 2x2 forward transform
            switch( core_enc->m_PicParamSet->chroma_format_idc ){
                case 1:
                    H264ENC_MAKE_NAME(ippiTransformQuantChromaDC_H264)(pDCBuf, pQBuf, chromaQP, &iNumCoeffs, 0, 1, NULL);
                    break;
                case 2:
                    H264ENC_MAKE_NAME(ippiTransformQuantChroma422DC_H264)(pDCBuf, pQBuf, chromaQP, &iNumCoeffs, 0, 1, NULL);
                    break;
            }
            if (pDCBuf[0] || pDCBuf[1] || pDCBuf[2] || pDCBuf[3]) return false;
            if( core_enc->m_PicParamSet->chroma_format_idc == 2 && (pDCBuf[4] || pDCBuf[5] || pDCBuf[6] || pDCBuf[7]) ) return false;
            coeffs_cost = 0;
            for (Ipp32s uBlock = start_block; uBlock < last_block; uBlock ++) {
                Ipp16s* pTempDiffBuf = pMassDiffBuf+(uBlock - start_block)*16;
                H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                    pTempDiffBuf,
                    pTransformResult,
                    chromaQP,
                    &iNumCoeffs,
                    0,
                    enc_single_scan[curr_slice->m_is_cur_mb_field],
                    &iLastCoeff,
                    NULL,
                    0,
                    0,
                    NULL);

                coeffs_cost += H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 15, &dec_single_scan[curr_slice->m_is_cur_mb_field][1]);
                if (coeffs_cost >= 7) return false;
                offset += core_enc->m_EncBlockOffsetInc[curr_slice->m_is_cur_mb_field][uBlock];
            }
            if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                start_block = 20;
                last_block = 24;
            }else if( core_enc->m_PicParamSet->chroma_format_idc == 2 ){
                start_block = 24;
                last_block = 32;
            }
            pSrcPlane = core_enc->m_pCurrentFrame->m_pVPlane;
            offset = pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
            pRef = pRefPicList0[0]->m_pVPlane + offset + curr_slice->m_InitialOffset[pFields0[0]];
#ifdef NO_PADDING
            pInterpolateInfo->pSrc[0] = pRefPicList0[0]->m_pVPlane + curr_slice->m_InitialOffset[pFields0[0]] + (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
            pInterpolateInfo->pSrc[1] = pInterpolateInfo->pSrc[0];
#endif // NO_PADDING
        }
#endif // USE_NV12
    }
    return true;
}


bool H264ENC_MAKE_NAME(H264CoreEncoder_CheckSkipB)(
    void* state,
    H264SliceType* curr_slice,
    H264MacroblockRefIdxs  ref_direct[2],
    H264MacroblockMVs      mvs_direct[2],
    Ipp32s residualPrediction,
    Ipp16s *pResBuf,
    Ipp16s *pResBufC)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    PIXTYPE* pInterpBuf = curr_slice->m_pPred4DirectB;
    Ipp16s* pDiffBuf    = (Ipp16s*) (curr_slice->m_pMBEncodeBuffer + 512);
    COEFFSTYPE *pTransformResult = (COEFFSTYPE*)(pDiffBuf + 16);
    T_EncodeMBOffsets *pMBOffset = &core_enc->m_pMBOffsets[cur_mb.uMB];
    Ipp32s pitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels<<curr_slice->m_is_cur_mb_field;
    PIXTYPE* pSrcPlane = core_enc->m_pCurrentFrame->m_pYPlane;
    H264ENC_MAKE_NAME(H264CoreEncoder_CDirectBOneMB_Interp)(state,  curr_slice, ref_direct, mvs_direct );
    Ipp32u offset = core_enc->m_pMBOffsets[cur_mb.uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
    Ipp32s   iNumCoeffs, iLastCoeff;
    Ipp32s CrPlaneOffset = 0;
    Ipp32s coeffs_cost = 0;
    Ipp32u uMBQP  = cur_mb.lumaQP;
    Ipp32s  chromaQP = cur_mb.chromaQP;
    // code block
    for (Ipp32s uBlock = 0; uBlock < 16; uBlock ++) {
        PIXTYPE* pPredBuf = pInterpBuf + xoff[uBlock] + yoff[uBlock]*16;
        Ipp16s *pRes = pResBuf + xoff[uBlock] + yoff[uBlock]*16;
        if (residualPrediction) {
          Diff4x4_ResPred(pRes, pPredBuf, pDiffBuf);
        } else {
          Diff4x4(pPredBuf, pSrcPlane + offset, pitchPixels, pDiffBuf);
        }

        H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
            pDiffBuf,
            pTransformResult,
            (Ipp32s)uMBQP,
            &iNumCoeffs,
            1,
            enc_single_scan[curr_slice->m_is_cur_mb_field],
            &iLastCoeff,
            NULL,
            0,
            0,
            NULL);

        coeffs_cost += iNumCoeffs
            ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 16, dec_single_scan[curr_slice->m_is_cur_mb_field])
            : 0;
        if (coeffs_cost >= 6) {
            //MC Chroma for calculation of Direct MB cost
            Ipp8s *pFields0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, cur_mb.uMB & 1)->m_Prediction;
            Ipp8s *pFields1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, curr_slice->m_is_cur_mb_field, cur_mb.uMB & 1)->m_Prediction;
            H264ENC_MAKE_NAME(H264CoreEncoder_CDirectBOneMB_Interp_Cr)(state, curr_slice, mvs_direct[LIST_0].MotionVectors, mvs_direct[LIST_1].MotionVectors, pFields0, pFields1, curr_slice->m_pMBEncodeBuffer, -1, size4x4);
            return false;
        }
        offset += core_enc->m_EncBlockOffsetInc[curr_slice->m_is_cur_mb_field][uBlock];
    }
    if (core_enc->m_PicParamSet->chroma_format_idc != 0) {
        COEFFSTYPE* pQBuf    = (COEFFSTYPE*) (pTransformResult + 16);
        COEFFSTYPE* pDCBuf   = (COEFFSTYPE*) (pQBuf + 16);   // Used for both luma and chroma DC blocks
        Ipp16s* pMassDiffBuf = (Ipp16s*) (pDCBuf + 16);
        Ipp32s start_block = 16, last_block = 20;

        //MC Chroma
        Ipp8s *pFields0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, curr_slice->m_is_cur_mb_field, cur_mb.uMB & 1)->m_Prediction;
        Ipp8s *pFields1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, curr_slice->m_is_cur_mb_field, cur_mb.uMB & 1)->m_Prediction;
        H264ENC_MAKE_NAME(H264CoreEncoder_CDirectBOneMB_Interp_Cr)(state, curr_slice, mvs_direct[LIST_0].MotionVectors, mvs_direct[LIST_1].MotionVectors, pFields0, pFields1, curr_slice->m_pMBEncodeBuffer, -1, size4x4);

        if(core_enc->m_PicParamSet->chroma_format_idc == 2) start_block = 16, last_block = 24;
        pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane;
        for (Ipp32s i = 0; i < 2 ; i++) {
            offset = pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
            pInterpBuf = curr_slice->m_pMBEncodeBuffer+CrPlaneOffset;
#ifdef USE_NV12
            if (residualPrediction) {
                Ipp16s *pDC = pDCBuf;
                Ipp32s ii, jj, m, n;
                Ipp16s *pSrc = pResBufC + i*8;
                PIXTYPE *pPred = pInterpBuf;
                for (ii = 0; ii < 2; ii++) {
                  for (jj = 0; jj < 2; jj++) {
                    pDC[jj] = 0;
                    for (m = 0; m < 4; m++) {
                      for (n = 0; n < 4; n++) {
                        Ipp32s temp = pSrc[m*16 + n] - pPred[m*16 + n];
                        pDC[jj] = (Ipp16s)(pDC[jj] + temp);
                        pMassDiffBuf[ii*32 + jj*16 + m*4 + n] = (Ipp16s) temp;
                      }
                    }
                    pPred += 4;
                    pSrc += 4;
                  }
                  pSrc += 4 * 16 - 8;
                  pPred += 4 * 16 - 8;
                  pDC += 2;
                }
            } else {
                if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
                  H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + offset, pitchPixels, pInterpBuf, 16, pDCBuf, pMassDiffBuf);
                else
                  ippiSumsDiff8x8Blocks4x4_NV12(pSrcPlane + offset, pitchPixels, pInterpBuf, 16, pDCBuf, pMassDiffBuf);
            }
#else
            H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + offset, pitchPixels, pInterpBuf, 16, pDCBuf, pMassDiffBuf);
#endif
            if( core_enc->m_PicParamSet->chroma_format_idc == 2 ){
                // Process second part of 2x4 block for DC coeffs
                 H264ENC_MAKE_NAME(ippiSumsDiff8x8Blocks4x4)(pSrcPlane + offset+8*pitchPixels,    // source pels
                                          pitchPixels,                 // source pitch
                                          pInterpBuf+8*16,               // predictor pels
                                          16,
                                          pDCBuf+4,                 // result buffer
                                          pMassDiffBuf+64);   //+Offset for second path
            }
            // 2x2 forward transform
            switch( core_enc->m_PicParamSet->chroma_format_idc ){
                case 1:
                   H264ENC_MAKE_NAME(ippiTransformQuantChromaDC_H264)(
                       pDCBuf,
                       pQBuf,
                       chromaQP,
                       &iNumCoeffs,
                       0,
                       1,
                       NULL);
                   break;
                case 2:
                    H264ENC_MAKE_NAME(ippiTransformQuantChroma422DC_H264)(
                        pDCBuf,
                        pQBuf,
                        chromaQP,
                        &iNumCoeffs,
                        0,
                        1,
                        NULL);
                    break;
            }

            if (pDCBuf[0] || pDCBuf[1] || pDCBuf[2] || pDCBuf[3])  return false;
            if( core_enc->m_PicParamSet->chroma_format_idc == 2 && (pDCBuf[4] || pDCBuf[5] || pDCBuf[6] || pDCBuf[7]) ) return false;
            coeffs_cost = 0;
            for (Ipp32s uBlock = start_block; uBlock < last_block; uBlock ++) {
                Ipp16s* pTempDiffBuf = pMassDiffBuf+(uBlock - start_block)*16;
                H264ENC_MAKE_NAME(ippiTransformQuantResidual_H264)(
                    pTempDiffBuf,
                    pTransformResult,
                    chromaQP,
                    &iNumCoeffs,
                    0,
                    enc_single_scan[curr_slice->m_is_cur_mb_field],
                    &iLastCoeff,
                    NULL,
                    0,
                    0,
                    NULL);

                coeffs_cost += iNumCoeffs
                    ? H264ENC_MAKE_NAME(CalculateCoeffsCost)(pTransformResult, 15, &dec_single_scan[curr_slice->m_is_cur_mb_field][1])
                    : 0;
                if (coeffs_cost >= 7)
                    return false;
                offset += core_enc->m_EncBlockOffsetInc[curr_slice->m_is_cur_mb_field][uBlock];
            }
            if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                start_block = 20;
                last_block = 24;
            }else if( core_enc->m_PicParamSet->chroma_format_idc == 2 ){
                start_block = 24;
                last_block = 32;
            }
            CrPlaneOffset = 8;
#ifdef USE_NV12
            if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
                pSrcPlane = core_enc->m_pCurrentFrame->m_pUPlane + 8;
            else
                pSrcPlane = core_enc->m_pCurrentFrame->m_pVPlane;
#else // USE_NV12
            pSrcPlane = core_enc->m_pCurrentFrame->m_pVPlane;
#endif // USE_NV12
        }
    }
    return true;
}


static bool H264ENC_MAKE_NAME(MVCloseToSkipMV)(H264CurrentMacroblockDescriptorType &cur_mb, H264MotionVector PredMV, H264MotionVector skipMV)
{
    if ((abs(PredMV.mvx - skipMV.mvx) <= 1) && (abs(PredMV.mvy - skipMV.mvy) <= 1)) {
        for (Ipp32s i = 0; i < 16; i ++) {
            cur_mb.MVs[LIST_0]->MotionVectors[i] = skipMV;
            cur_mb.MVs[LIST_1]->MotionVectors[i] = null_mv;
            cur_mb.RefIdxs[LIST_0]->RefIdxs[i] = (T_RefIdx)0;
            cur_mb.RefIdxs[LIST_1]->RefIdxs[i] = (T_RefIdx)-1;
        }
        cur_mb.LocalMacroblockInfo->cbp_luma = 0;
        cur_mb.LocalMacroblockInfo->cbp_chroma = 0;
        cur_mb.LocalMacroblockInfo->cbp = 0;
        cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;
        // cur_mb.GlobalMacroblockInfo->sbtype[0] = MBTYPE_SKIPPED;
        return true;
    }
    return false;
}


Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_ME_P)(
    void* state,
    H264SliceType *curr_slice)
{
    const H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s uMB = cur_mb.uMB;
    Ipp8s  mvsBudget = cur_mb.LocalMacroblockInfo->m_mvsBudget, mvsRemain = 0;
    T_EncodeMBOffsets *pMBOffset = &core_enc->m_pMBOffsets[uMB];
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    H264EncoderFrameType **ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_RefPicList;
    Ipp8s *pFields = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_Prediction;
    Ipp32s iQP = cur_mb.lumaQP51;
    Ipp32u uMBQP  = cur_mb.lumaQP;
    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && uMBQP == 0;

    Ipp16s *pRDQM = glob_RDQM[iQP];
    Ipp32s ref_idx, numRef, i, bOff, refCost[MAX_NUM_REF_FRAMES];
    H264MotionVector BestMV16x16, BestMV16x8[2], BestMV8x16[2], BestMV8x8[4], BestMV8x4[4][2], BestMV4x8[4][2], BestMV4x4[4][4], bMV16x16[MAX_NUM_REF_FRAMES];
    H264MotionVector BestPredMV16x16, BestPredMV16x8[2], BestPredMV8x16[2], BestPredMV8x8[4], /*PredMV,*/ skipMV = {0, 0};
    H264MotionVector BestPredMV8x4[4][2], BestPredMV4x8[4][2], BestPredMV4x4[4][4];
    Ipp32s  BestSAD, BestSAD16x16, BestSAD16x8, BestSAD8x16, BestSAD8x8;
    Ipp32s  BestRef16x16, BestRef16x8[2], BestRef8x16[2], BestRef8x8[4];
    Ipp32s  mpredflag = 0;
    Ipp32s  mpred16x16, mpred16x8[2], mpred8x16[2], mpred8x8[4], mpred8x4[4][2], mpred4x8[4][2], mpred4x4[4][4];
    Ipp32s  bSAD16x8[2], bSAD8x16[2], bSAD8x8[4], bSADs;
    H264MotionVector *mvs  = cur_mb.MVs[LIST_0]->MotionVectors;
    T_RefIdx *refs = cur_mb.RefIdxs[LIST_0]->RefIdxs;
    MBTypeValue *sbt = cur_mb.GlobalMacroblockInfo->sbtype, b8x8s[4];
    ME_InfType meInfo;
    Ipp32s  meFlags = core_enc->m_Analyse;
    PIXTYPE* pMBCurU = NULL;
    PIXTYPE* pMBCurV = NULL;
    Ipp32s refMin, refMax, refStep, check_threshold;
    Ipp32s rdCost16x16, rdCost8x16, rdCost16x8, rdCost8x8, minRDCost;
    bool skipAvail;
#ifdef USE_NV12
    __ALIGN16 PIXTYPE pUSrc[64];
    __ALIGN16 PIXTYPE pVSrc[64];
    PIXTYPE *pSrc;
#endif // USE_NV12
    Ipp16s *resPredBuf = cur_mb.m_pResPredDiff;
    Ipp16s *resPredBufC = cur_mb.m_pResPredDiffC;
    Ipp32s resPredFlag;
    Ipp32s RefLater8x8PackFlag = pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo);
    bool check2TransformModes;
#ifdef H264_INTRA_REFRESH
    Ipp16u curMBLine = core_enc->m_refrType == HORIZ_REFRESH ? uMB / core_enc->m_WidthInMBs : uMB % core_enc->m_WidthInMBs;
#endif // H264_INTRA_REFRESH

    resPredFlag = meInfo.residualPredictionFlag = 0;

    if (curr_slice->resPredFlag > 0)
        resPredFlag = meInfo.residualPredictionFlag = 1;

    check2TransformModes = false;
    if ((core_enc->m_spatial_resolution_change) || (resPredFlag == 0))
    {
        RefLater8x8PackFlag = 0;
        check2TransformModes = true;
    }

    meInfo.pResY = resPredBuf;
    meInfo.pResU = resPredBufC;
    meInfo.pResV = resPredBufC + 8;

    if( core_enc->m_PicParamSet->chroma_format_idc == 0 ) //mask CHROMA_ME flag
        meFlags &= ~ANALYSE_ME_CHROMA;
    cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER;
#ifdef FRAME_INTERPOLATION
    meInfo.planeSize = core_enc->m_pCurrentFrame->m_PlaneSize;
    Ipp32s planeSize = core_enc->m_pCurrentFrame->m_PlaneSize;
#else
#ifndef NO_PADDING
    Ipp32s planeSize = 0;
#endif
#endif //FRAME_INTERPOLATION
    meInfo.xMin = pMBOffset->uMVLimits_L;
    meInfo.xMax = pMBOffset->uMVLimits_R;
    meInfo.yMin = pMBOffset->uMVLimits_U;
    meInfo.yMax = pMBOffset->uMVLimits_D;
    meInfo.pRDQM = pRDQM;
    meInfo.pitchPixels = cur_mb.mbPitchPixels;
    meInfo.bit_depth_luma = core_enc->m_PicParamSet->bit_depth_luma;
    meInfo.rX = core_enc->m_info.me_search_x;
    meInfo.rY = core_enc->m_info.me_search_y;
    meInfo.flags = meFlags;
    meInfo.searchAlgo = core_enc->m_info.mv_search_method + core_enc->m_SubME_Algo * 32;
    check_threshold = meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] : 0;
    if (meFlags & ANALYSE_ME_CHROMA) {
        meInfo.pCurU = pMBCurU = core_enc->m_pCurrentFrame->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        meInfo.pCurV = pMBCurV = core_enc->m_pCurrentFrame->m_pVPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        meInfo.chroma_format_idc = core_enc->m_PicParamSet->chroma_format_idc;
        meInfo.bit_depth_chroma = core_enc->m_SeqParamSet->bit_depth_chroma;
    }
#ifdef USE_NV12
    if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
    {
        meInfo.pCurV = pMBCurV = core_enc->m_pCurrentFrame->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + 8;
        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        for(i=0;i<8;i++){
            Ipp32s s = 8*i;
            pUSrc[s+0] = pSrc[0];
            pVSrc[s+0] = pSrc[1];
            pUSrc[s+1] = pSrc[2];
            pVSrc[s+1] = pSrc[3];
            pUSrc[s+2] = pSrc[4];
            pVSrc[s+2] = pSrc[5];
            pUSrc[s+3] = pSrc[6];
            pVSrc[s+3] = pSrc[7];
            pUSrc[s+4] = pSrc[8];
            pVSrc[s+4] = pSrc[9];
            pUSrc[s+5] = pSrc[10];
            pVSrc[s+5] = pSrc[11];
            pUSrc[s+6] = pSrc[12];
            pVSrc[s+6] = pSrc[13];
            pUSrc[s+7] = pSrc[14];
            pVSrc[s+7] = pSrc[15];

            pSrc[0] = pUSrc[s+0];
            pSrc[1] = pUSrc[s+1];
            pSrc[2] = pUSrc[s+2];
            pSrc[3] = pUSrc[s+3];
            pSrc[4] = pUSrc[s+4];
            pSrc[5] = pUSrc[s+5];
            pSrc[6] = pUSrc[s+6];
            pSrc[7] = pUSrc[s+7];
            pSrc[8] = pVSrc[s+0];
            pSrc[9] = pVSrc[s+1];
            pSrc[10] = pVSrc[s+2];
            pSrc[11] = pVSrc[s+3];
            pSrc[12] = pVSrc[s+4];
            pSrc[13] = pVSrc[s+5];
            pSrc[14] = pVSrc[s+6];
            pSrc[15] = pVSrc[s+7];

            pSrc += meInfo.pitchPixels;
        }
    }
#endif // USE_NV12
    numRef = curr_slice->m_NumRefsInL0List;
#ifdef NO_PADDING
    Ipp32s offsetIfBottomField = (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (meInfo.pitchPixels >> 1);
    IppiPoint mbPos, block8x8Pos;
    mbPos.x = cur_mb.uMBx << 4;
    mbPos.y = cur_mb.uMBy << 4;
    meInfo.interpolateInfo.sizeFrame.width = core_enc->m_WidthInMBs << 4;
    meInfo.interpolateInfo.sizeFrame.height = core_enc->m_HeightInMBs << 4;
    meInfo.interpolateInfo.srcStep = cur_mb.mbPitchPixels;
    meInfo.interpolateInfo.dstStep = 16;

    if (core_enc->m_PicParamSet->chroma_format_idc == 1) //420
    {
        meInfo.interpolateInfoChroma.sizeFrame.width = meInfo.interpolateInfo.sizeFrame.width >> 1;
        meInfo.interpolateInfoChroma.sizeFrame.height = meInfo.interpolateInfo.sizeFrame.height >> 1;
    }
    else if (core_enc->m_PicParamSet->chroma_format_idc == 2) //422
    {
        meInfo.interpolateInfoChroma.sizeFrame.width = meInfo.interpolateInfo.sizeFrame.width >> 1;
        meInfo.interpolateInfoChroma.sizeFrame.height = meInfo.interpolateInfo.sizeFrame.height;
    }
    meInfo.interpolateInfoChroma.srcStep = cur_mb.mbPitchPixels;
    meInfo.interpolateInfoChroma.dstStep = 16;
#endif // NO_PADDING
/*
    if (core_enc->m_info.B_frame_rate) {
        numRef /= (core_enc->m_info.B_frame_rate + 1);
        //if (core_enc->m_info.treat_B_as_reference)
        //    numRef ++;
        if (numRef == 0)
            numRef = 1;
    }
*/
    for (i = 0; i < numRef; i ++)
        refCost[i] = RefConstraint(i, numRef, pRDQM);
    BestSAD16x16 = BestSAD16x8 = BestSAD8x16 = BestSAD8x8 = MAX_SAD;
    rdCost16x16 = rdCost8x16 = rdCost16x8 = rdCost8x8 = MAX_SAD;
    minRDCost = MAX_SAD;
    // 16x16
    meInfo.block.width = 16;
    meInfo.block.height = 16;
    meInfo.pCur = cur_mb.mbPtr;
    BestRef16x16 = 0;
    BestMV16x16.mvx = BestMV16x16.mvy = 0;

#ifdef H264_INTRA_REFRESH
    if (core_enc->m_refrType) {
        if (core_enc->m_firstLineInStripe > 0 && curMBLine < core_enc->m_firstLineInStripe) {
            Ipp16s maxMV = (ppRefPicList[0]->m_firstLineInStripe + core_enc->m_stripeWidth - 2 - curMBLine) * 16;
            if (core_enc->m_refrType == HORIZ_REFRESH)
                meInfo.yMax = maxMV;
            else if (core_enc->m_refrType == VERT_REFRESH)
                meInfo.xMax = maxMV;
        }
    }
#endif // H264_INTRA_REFRESH

    //ref = 0 is special case
    refs[0] = 0;
    H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, 0, LIST_0, 4, 4, &meInfo);
    if ((transform_bypass) || (curr_slice->resPredFlag == 1 /* adaptive mode */)) {
        skipAvail = false;
    } else {
        H264ENC_MAKE_NAME(H264CoreEncoder_Skip_MV_Predicted)(state, curr_slice, &meInfo.predictedMV, &skipMV);
        skipAvail = H264ENC_MAKE_NAME(H264CoreEncoder_CheckSkip)(state, curr_slice, skipMV, resPredFlag, resPredBuf, resPredBufC);
    }

    //meInfo.predictedMV = PredMV;
    if (skipAvail && (meFlags & ANALYSE_CHECK_SKIP_PREDICT)) {
        if (H264ENC_MAKE_NAME(MVCloseToSkipMV)(cur_mb, meInfo.predictedMV, skipMV))
        {
#ifdef USE_NV12
            if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
            {
                pSrc = core_enc->m_pCurrentFrame->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                for(i=0;i<8;i++){
                    Ipp32s s = 8*i;

                    pSrc[0] = pUSrc[s+0];
                    pSrc[1] = pVSrc[s+0];
                    pSrc[2] = pUSrc[s+1];
                    pSrc[3] = pVSrc[s+1];
                    pSrc[4] = pUSrc[s+2];
                    pSrc[5] = pVSrc[s+2];
                    pSrc[6] = pUSrc[s+3];
                    pSrc[7] = pVSrc[s+3];
                    pSrc[8] = pUSrc[s+4];
                    pSrc[9] = pVSrc[s+4];
                    pSrc[10] = pUSrc[s+5];
                    pSrc[11] = pVSrc[s+5];
                    pSrc[12] = pUSrc[s+6];
                    pSrc[13] = pVSrc[s+6];
                    pSrc[14] = pUSrc[s+7];
                    pSrc[15] = pVSrc[s+7];

                    pSrc += meInfo.pitchPixels;
                }
            }
#endif // USE_NV12
            return 0;
        }
    }
#ifdef NO_PADDING
    meInfo.interpolateInfo.pSrc[0] = ppRefPicList[0]->m_pYPlane + curr_slice->m_InitialOffset[pFields[0]] + offsetIfBottomField;
    meInfo.interpolateInfo.pointBlockPos.x = mbPos.x;
    meInfo.interpolateInfo.pointBlockPos.y = mbPos.y;
#endif // NO_PADDING
    meInfo.pRef = ppRefPicList[0]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[0]];
    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CandList16x16)(state, curr_slice, LIST_0, &meInfo, 0);
    if (skipAvail && (meFlags & ANALYSE_CHECK_SKIP_BESTCAND)) {
        H264MotionVector b;
        b.mvx = meInfo.bestMV.mvx << SUB_PEL_SHIFT;
        b.mvy = meInfo.bestMV.mvy << SUB_PEL_SHIFT;
        if (H264ENC_MAKE_NAME(MVCloseToSkipMV)(cur_mb, b, skipMV) /*|| H264ENC_MAKE_NAME(MVCloseToSkipMV)(cur_mb, b, meInfo.scPredMV)*/)
        {
#ifdef USE_NV12
            if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
            {
                pSrc = core_enc->m_pCurrentFrame->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                for(i=0;i<8;i++){
                    Ipp32s s = 8*i;

                    pSrc[0] = pUSrc[s+0];
                    pSrc[1] = pVSrc[s+0];
                    pSrc[2] = pUSrc[s+1];
                    pSrc[3] = pVSrc[s+1];
                    pSrc[4] = pUSrc[s+2];
                    pSrc[5] = pVSrc[s+2];
                    pSrc[6] = pUSrc[s+3];
                    pSrc[7] = pVSrc[s+3];
                    pSrc[8] = pUSrc[s+4];
                    pSrc[9] = pVSrc[s+4];
                    pSrc[10] = pUSrc[s+5];
                    pSrc[11] = pVSrc[s+5];
                    pSrc[12] = pUSrc[s+6];
                    pSrc[13] = pVSrc[s+6];
                    pSrc[14] = pUSrc[s+7];
                    pSrc[15] = pVSrc[s+7];

                    pSrc += meInfo.pitchPixels;
                }
            }
#endif // USE_NV12
            return 0;
        }
    }
    if (meFlags & ANALYSE_ME_CHROMA) {
         meInfo.chroma_mvy_offset = 0;
         if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
             if (!curr_slice->m_is_cur_mb_bottom_field && pFields[0]) meInfo.chroma_mvy_offset += - 2;
             else if (curr_slice->m_is_cur_mb_bottom_field && !pFields[0]) meInfo.chroma_mvy_offset += 2;
         }
         meInfo.pRefU = ppRefPicList[0]->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[0]];
         meInfo.pRefV = ppRefPicList[0]->m_pVPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[0]];
#ifdef NO_PADDING
         meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[0]->m_pUPlane + curr_slice->m_InitialOffset[pFields[0]] + offsetIfBottomField;
         meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[0]->m_pVPlane + curr_slice->m_InitialOffset[pFields[0]] + offsetIfBottomField;
#endif // NO_PADDING
    }

    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);

    if (skipAvail && (meFlags & ANALYSE_CHECK_SKIP_INTPEL)) {
        if (H264ENC_MAKE_NAME(MVCloseToSkipMV)(cur_mb, meInfo.bestMV, skipMV) /*|| H264ENC_MAKE_NAME(MVCloseToSkipMV)(cur_mb, meInfo.bestMV, meInfo.scPredMV)*/)
        {
#ifdef USE_NV12
            if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
            {
                pSrc = core_enc->m_pCurrentFrame->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                for(i=0;i<8;i++){
                    Ipp32s s = 8*i;

                    pSrc[0] = pUSrc[s+0];
                    pSrc[1] = pVSrc[s+0];
                    pSrc[2] = pUSrc[s+1];
                    pSrc[3] = pVSrc[s+1];
                    pSrc[4] = pUSrc[s+2];
                    pSrc[5] = pVSrc[s+2];
                    pSrc[6] = pUSrc[s+3];
                    pSrc[7] = pVSrc[s+3];
                    pSrc[8] = pUSrc[s+4];
                    pSrc[9] = pVSrc[s+4];
                    pSrc[10] = pUSrc[s+5];
                    pSrc[11] = pVSrc[s+5];
                    pSrc[12] = pUSrc[s+6];
                    pSrc[13] = pVSrc[s+6];
                    pSrc[14] = pUSrc[s+7];
                    pSrc[15] = pVSrc[s+7];

                    pSrc += meInfo.pitchPixels;
                }
            }
#endif // USE_NV12
             return 0;
        }
    }
    if ((meFlags & ANALYSE_ME_SUBPEL) && meInfo.bestSAD+refCost[0] > meInfo.threshold) {
        H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
        if (skipAvail && (meFlags & ANALYSE_CHECK_SKIP_SUBPEL)) {
            if (H264ENC_MAKE_NAME(MVCloseToSkipMV)(cur_mb, meInfo.bestMV, skipMV) /*|| H264ENC_MAKE_NAME(MVCloseToSkipMV)(cur_mb, meInfo.bestMV, meInfo.scPredMV)*/)
            {
#ifdef USE_NV12
                if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
                {
                    pSrc = core_enc->m_pCurrentFrame->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                    for(i=0;i<8;i++){
                        Ipp32s s = 8*i;

                        pSrc[0] = pUSrc[s+0];
                        pSrc[1] = pVSrc[s+0];
                        pSrc[2] = pUSrc[s+1];
                        pSrc[3] = pVSrc[s+1];
                        pSrc[4] = pUSrc[s+2];
                        pSrc[5] = pVSrc[s+2];
                        pSrc[6] = pUSrc[s+3];
                        pSrc[7] = pVSrc[s+3];
                        pSrc[8] = pUSrc[s+4];
                        pSrc[9] = pVSrc[s+4];
                        pSrc[10] = pUSrc[s+5];
                        pSrc[11] = pVSrc[s+5];
                        pSrc[12] = pUSrc[s+6];
                        pSrc[13] = pVSrc[s+6];
                        pSrc[14] = pUSrc[s+7];
                        pSrc[15] = pVSrc[s+7];

                        pSrc += meInfo.pitchPixels;
                    }
                }
#endif // USE_NV12
                return 0;
            }
        }
    }
    BestSAD16x16 = meInfo.bestSAD + refCost[0];
    BestMV16x16 = bMV16x16[0] = meInfo.bestMV;
    BestRef16x16 = 0;
    BestPredMV16x16 = meInfo.bestPredMV;//meInfo.predictedMV;
    mpred16x16 = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);

    //Loop other refs
    if (BestSAD16x16 > meInfo.threshold) {
        for (ref_idx = 1; ref_idx < numRef; ref_idx ++) {
#ifdef H264_INTRA_REFRESH
            if (core_enc->m_refrType) {
                if (ppRefPicList[ref_idx]->m_periodCount < core_enc->m_periodCount ||
                    (curMBLine < core_enc->m_firstLineInStripe && ppRefPicList[ref_idx]->m_firstLineInStripe < 0))
                    continue;
                else if (curMBLine < core_enc->m_firstLineInStripe) {
                    Ipp16s maxMV = (ppRefPicList[ref_idx]->m_firstLineInStripe + core_enc->m_stripeWidth - 2 - curMBLine) * 16;
                    if (core_enc->m_refrType == HORIZ_REFRESH)
                        meInfo.yMax = maxMV;
                    else if (core_enc->m_refrType == VERT_REFRESH)
                        meInfo.xMax = maxMV;
                }
            }
#endif // H264_INTRA_REFRESH
            refs[0] = ref_idx;
            H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, 0, LIST_0, 4, 4, &meInfo);
            //meInfo.predictedMV = PredMV;
            meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]];
#ifdef NO_PADDING
            meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
            if (meFlags & ANALYSE_ME_CHROMA) {
                meInfo.chroma_mvy_offset = 0;
                if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                    if (!curr_slice->m_is_cur_mb_bottom_field && pFields[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                    else if (curr_slice->m_is_cur_mb_bottom_field && !pFields[ref_idx]) meInfo.chroma_mvy_offset += 2;
                }
                meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]];
                meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]];
#ifdef NO_PADDING
                meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
                meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
            }
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CandList16x16)(state, curr_slice, LIST_0, &meInfo, ref_idx);
            H264MotionVector BestMV16x16scaled;
            BestMV16x16scaled.mvx = BestMV16x16.mvx * (ref_idx + 1) / (BestRef16x16 + 1);
            BestMV16x16scaled.mvy = BestMV16x16.mvy * (ref_idx + 1) / (BestRef16x16 + 1);
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV16x16scaled);
            H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
            if ((meFlags & ANALYSE_ME_SUBPEL) && meInfo.bestSAD+refCost[ref_idx] > meInfo.threshold)
                H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
            bMV16x16[ref_idx] = meInfo.bestMV;
            if (meInfo.bestSAD + refCost[ref_idx] < BestSAD16x16) { // !! refCost for SVC mv_pred is incorrect
                BestSAD16x16 = meInfo.bestSAD + refCost[ref_idx];
                BestMV16x16 = meInfo.bestMV;
                BestRef16x16 = ref_idx;
                BestPredMV16x16 = meInfo.bestPredMV; //meInfo.predictedMV;
                mpred16x16 =  meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                if (BestSAD16x16 <= meInfo.threshold)
                    break;
            } else //if (!(meFlags & ANALYSE_ME_ALL_REF) && !((meFlags & ANALYSE_P_8x8)))
                // TODO
                //if (meInfo.bestSAD + refCost[ref_idx] > (BestSAD16x16 * 17 >> 4))
                if (!(meFlags & ANALYSE_ME_ALL_REF))
                    if ((core_enc->m_Analyse & ANALYSE_ME_FAST_MULTIREF) && (ref_idx > (numRef >> 1)))
                        break;
        }
    }
    if (core_enc->m_Analyse & ANALYSE_RD_OPT) {
        for (i = 0; i < 16; i ++) {
            mvs[i] = BestMV16x16;
            refs[i] = BestRef16x16;
        }
#ifdef USE_NV12
        minRDCost = rdCost16x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, minRDCost, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
        minRDCost = rdCost16x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost)(state, curr_slice, 0, minRDCost );
#endif // USE_NV12
    }

    bool doSplit = (meFlags & ANALYSE_P_8x8) &&  (BestSAD16x16 > check_threshold) && (mvsBudget >= 4);
    Ipp32u BestIntraSAD16x16 = MAX_SAD, BestIntraSAD8x8 = MAX_SAD, BestIntraSAD4x4 = MAX_SAD;
    Ipp32s intra_cost_chroma = 0;
    H264MacroblockLocalInfo intraLocalMBinfo = {0,};
    MB_Type BestIntraMBType = MBTYPE_INTRA;
    Ipp32s BestIntraSAD = MAX_SAD;
    if ((core_enc->m_Analyse & ANALYSE_INTRA_IN_ME) && (!resPredFlag))
    {
        if (!(core_enc->m_Analyse & ANALYSE_RD_OPT) && (core_enc->m_Analyse & ANALYSE_RD_MODE)) {
            for (i = 0; i < 16; i ++) {
                mvs[i] = BestMV16x16;
                refs[i] = BestRef16x16;
            }
#ifdef USE_NV12
            BestSAD = rdCost16x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, minRDCost, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
            BestSAD = rdCost16x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost)(state, curr_slice, 0, minRDCost);
#endif // USE_NV12
        } else if (core_enc->m_Analyse & ANALYSE_RD_OPT)
            BestSAD = rdCost16x16;
        else
            BestSAD = BestSAD16x16;
        if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || !(BestSAD < EmptyThreshold[uMBQP])) {
            H264MacroblockLocalInfo sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
            H264MacroblockGlobalInfo sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;
            if ((core_enc->m_Analyse & ANALYSE_ME_CHROMA) && core_enc->m_PicParamSet->chroma_format_idc != 0) {
                Ipp32s uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
                if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE)) {
                    pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, false);
                    cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA;
#ifdef USE_NV12
                    intra_cost_chroma = H264ENC_MAKE_NAME(H264CoreEncoder_IntraSelectChromaRD_NV12)(
                        state,
                        curr_slice,
                        core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                        core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                        cur_mb.mbPitchPixels,
                        &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                        cur_mb.mbChromaIntra.prediction,
                        cur_mb.mbChromaIntra.prediction + 8,
                        1);
#else // USE_NV12
                    intra_cost_chroma = H264ENC_MAKE_NAME(H264CoreEncoder_IntraSelectChromaRD)(
                        state,
                        curr_slice,
                        core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                        core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                        core_enc->m_pCurrentFrame->m_pVPlane + uOffset,
                        core_enc->m_pReconstructFrame->m_pVPlane + uOffset,
                        cur_mb.mbPitchPixels,
                        &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                        cur_mb.mbChromaIntra.prediction,
                        cur_mb.mbChromaIntra.prediction + 8);
#endif // USE_NV12
                } else {
#ifdef USE_NV12
                    intra_cost_chroma = H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8_NV12)(
                            state,
                            curr_slice,
                            core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                            core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                            cur_mb.mbPitchPixels,
                            &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                            cur_mb.mbChromaIntra.prediction,
                            cur_mb.mbChromaIntra.prediction + 8,
                            1);  //up to 422 only
#else
                    Ipp8u mode;
                    intra_cost_chroma = H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8)(
                        state,
                        curr_slice,
                        core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                        core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                        core_enc->m_pCurrentFrame->m_pVPlane + uOffset,
                        core_enc->m_pReconstructFrame->m_pVPlane + uOffset,
                        cur_mb.mbPitchPixels,
                        &mode,
                        cur_mb.mbChromaIntra.prediction,
                        cur_mb.mbChromaIntra.prediction + 8);
#endif
                }
            }
            H264ENC_MAKE_NAME(H264CoreEncoder_Intra16x16SelectAndPredict)(state, curr_slice, &BestIntraSAD16x16, cur_mb.mb16x16.prediction);
            intraLocalMBinfo = *cur_mb.LocalMacroblockInfo;
            *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
            *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;
        }
        BestIntraMBType = MBTYPE_INTRA_16x16;
        BestIntraSAD = BestIntraSAD16x16;
        if (doSplit)
            if ((BestSAD > (((Ipp32s)BestIntraSAD16x16 + intra_cost_chroma) * 269 >> 8)) || (!(core_enc->m_Analyse & ANALYSE_RD_OPT) && !(core_enc->m_Analyse & ANALYSE_RD_MODE) && (BestSAD < EmptyThreshold[uMBQP]))) {
                /*
                for (i = 0; i < 16; i ++) {
                    cur_mb.RefIdxs[LIST_1]->RefIdxs[i] = (T_RefIdx)-1;
                    cur_mb.MVs[LIST_1]->MotionVectors[i] = null_mv;
                    refs[i] = (T_RefIdx)BestRef16x16;
                    mvs[i] = BestMV16x16;
                }
                sbt[0] = sbt[1] = sbt[2] = sbt[3] = (MBTypeValue)NUMBER_OF_MBTYPES;
                goto CheckIntra;
                */
                doSplit = false;
            }
    }
    if (!(meFlags & ANALYSE_ME_ALL_REF))
        numRef = BestRef16x16 + 1;
    if (core_enc->m_info.mv_search_method != MV_SEARCH_TYPE_FULL)
       meInfo.searchAlgo = MV_SEARCH_TYPE_SQUARE + core_enc->m_SubME_Algo * 32;
    if (doSplit && (core_enc->m_Analyse & ANALYSE_FLATNESS)) {
        __ALIGN16 PIXTYPE mcBlock[256];
        PIXTYPE *pR;
        pR = ppRefPicList[BestRef16x16]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[BestRef16x16]];
        const PIXTYPE *pI;
        Ipp32s   sI;
        IppiSize block = {16, 16};
#ifdef NO_PADDING
        meInfo.interpolateInfo.pSrc[0] = ppRefPicList[BestRef16x16]->m_pYPlane + curr_slice->m_InitialOffset[pFields[BestRef16x16]] + offsetIfBottomField;
        meInfo.interpolateInfo.pDst[0] = mcBlock;
        meInfo.interpolateInfo.sizeBlock = block;
        meInfo.interpolateInfo.pointBlockPos = mbPos;
        meInfo.interpolateInfo.pointVector.x = BestMV16x16.mvx;
        meInfo.interpolateInfo.pointVector.y = BestMV16x16.mvy;
        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo.interpolateInfo));
        pI = meInfo.interpolateInfo.pDst[0];
        sI = meInfo.interpolateInfo.dstStep;
#else // NO_PADDING
        H264ENC_MAKE_NAME(InterpolateLuma)(MVADJUST(pR, cur_mb.mbPitchPixels, BestMV16x16.mvx >> SUB_PEL_SHIFT, BestMV16x16.mvy >> SUB_PEL_SHIFT), cur_mb.mbPitchPixels, mcBlock, 16, BestMV16x16.mvx & 3, BestMV16x16.mvy & 3, block, meInfo.bit_depth_luma, planeSize, &pI, &sI);
#endif // NO_PADDING
        Ipp32s sad8x8[4];
        if (meInfo.residualPredictionFlag)
        {
            Ipp32s b, i, j;
            for (b = 0; b < 4; b++) {
                Ipp32s sad = 0;
                Ipp16s *pRes = resPredBuf + (b & 1)*8 + (b & 2)*4*16;
                const PIXTYPE *pPred = pI + (b & 1)*8 + (b & 2)*4*sI;
                for (i = 0; i < 8; i++)
                    for (j = 0; j < 8; j++)
                        sad += pRes[i*16 + j] - pPred[i*sI + j];
                sad8x8[b] = sad;
            }
        } else {
            sad8x8[0] = SAD8x8(cur_mb.mbPtr, cur_mb.mbPitchPixels, pI, sI);
            sad8x8[1] = SAD8x8(cur_mb.mbPtr + 8, cur_mb.mbPitchPixels, pI + 8, sI);
            sad8x8[2] = SAD8x8(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels, cur_mb.mbPitchPixels, pI + 8 * sI, sI);
            sad8x8[3] = SAD8x8(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels + 8, cur_mb.mbPitchPixels, pI + 8 * sI + 8, sI);
        }
        if ((sad8x8[0] <= (EmptyThreshold[uMBQP] >> 2)) && (sad8x8[1] <= (EmptyThreshold[uMBQP] >> 2)) && (sad8x8[2] <= (EmptyThreshold[uMBQP] >> 2)) && (sad8x8[3] <= (EmptyThreshold[uMBQP] >> 2)))
            doSplit = false;
        else if (sad8x8[0] != 0) {
#define LB 0.75
#define UB (1.0 / LB)
            double d = 1.0 / sad8x8[0], d1 = sad8x8[1] * d, d2 = sad8x8[2] * d, d3 = sad8x8[3] * d;
            if ((LB < d1) && (d1 < UB) && (LB < d2) && (d2 < UB) && (LB < d3) && (d3 < UB))
                doSplit = false;
        }
    }

    if (doSplit) {
        if (meFlags & ANALYSE_SPLIT_SMALL_RANGE) {
            meInfo.rX >>= 1;
            if (meInfo.rX == 0)
                meInfo.rX = 1;
            meInfo.rY >>= 1;
            if (meInfo.rY == 0)
                meInfo.rY = 1;
        }
        // 8x8
        meInfo.block.width = 8;
        meInfo.block.height = 8;
        meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 2 : 0;

        for (Ipp32s b = 0; b < 4; b ++) {
            bSAD8x8[b] = MAX_SAD;
            bOff = block_subblock_mapping[b*4];
            meInfo.pCur = cur_mb.mbPtr + (b >> 1) * 8 * cur_mb.mbPitchPixels + (b & 1) * 8;
            meInfo.pResY = resPredBuf + (b >> 1) * 8 * 16 + (b & 1) * 8;
#ifdef NO_PADDING
            block8x8Pos.x = (b & 1) * 8;
            block8x8Pos.y = (b >> 1) * 8;
            meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x;
            meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y;
#endif // NO_PADDING
            if (meFlags & ANALYSE_ME_CHROMA) {
                meInfo.pCurU = pMBCurU + (b >> 1) * 4 * cur_mb.mbPitchPixels + (b & 1) * 4;
                meInfo.pCurV = pMBCurV + (b >> 1) * 4 * cur_mb.mbPitchPixels + (b & 1) * 4;
                meInfo.pResU = resPredBufC + (b >> 1) * 4 * 16 + (b & 1) * 4;
                meInfo.pResV = resPredBufC + 8 + (b >> 1) * 4 * 16 + (b & 1) * 4;
            }
            for (ref_idx = 0; ref_idx < numRef; ref_idx ++) {
#ifdef H264_INTRA_REFRESH
                if (core_enc->m_refrType) {
                    if (ppRefPicList[ref_idx]->m_periodCount < core_enc->m_periodCount ||
                        (curMBLine < core_enc->m_firstLineInStripe && ppRefPicList[ref_idx]->m_firstLineInStripe < 0))
                        continue;
                    else if (curMBLine < core_enc->m_firstLineInStripe) {
                        Ipp16s maxMV = (ppRefPicList[ref_idx]->m_firstLineInStripe + core_enc->m_stripeWidth - 2 - curMBLine) * 16;
                        if (core_enc->m_refrType == HORIZ_REFRESH)
                            meInfo.yMax = maxMV;
                         else if (core_enc->m_refrType == VERT_REFRESH)
                             meInfo.xMax = maxMV;
                    }
                }
#endif // H264_INTRA_REFRESH
                refs[bOff] = ref_idx;
                meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + (b >> 1) * 8 * cur_mb.mbPitchPixels + (b & 1) * 8;
#ifdef NO_PADDING
                meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                if (meFlags & ANALYSE_ME_CHROMA) {
                    meInfo.chroma_mvy_offset = 0;
                    if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                        if (!curr_slice->m_is_cur_mb_bottom_field && pFields[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                        else if (curr_slice->m_is_cur_mb_bottom_field && !pFields[ref_idx]) meInfo.chroma_mvy_offset += 2;
                    }
#ifdef USE_NV12
                    meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + (b >> 1) * 4 * cur_mb.mbPitchPixels + (b & 1) * 8;
                    meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                    meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + (b >> 1) * 4 * cur_mb.mbPitchPixels + (b & 1) * 4;
                    meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + (b >> 1) * 4 * cur_mb.mbPitchPixels + (b & 1) * 4;
#endif // USE_NV12
#ifdef NO_PADDING
                    meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
                    meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                }

                H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 2, 2, &meInfo);
                meInfo.candNum = 0;
                meInfo.bestSAD = MAX_SAD;
                //meInfo.predictedMV = PredMV;
                H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                //H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV16x16);
                H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, bMV16x16[ref_idx]);
                //if (b > 0)
                //    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[0]);
                //if (b > 1)
                //    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[1]);
                //if (b > 2)
                //    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[2]);

                H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);

                if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCost[ref_idx] > meInfo.threshold)
                    H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);

                if (meInfo.bestSAD + refCost[ref_idx] < bSAD8x8[b]) {
                    bSAD8x8[b] = meInfo.bestSAD + refCost[ref_idx];
                    BestMV8x8[b] = meInfo.bestMV;
                    BestRef8x8[b] = ref_idx;
                    BestPredMV8x8[b] = meInfo.bestPredMV; //meInfo.predictedMV;
                    mpred8x8[b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                } else
                    if (!(meFlags & ANALYSE_ME_ALL_REF))
                        if ((core_enc->m_Analyse & ANALYSE_ME_FAST_MULTIREF) && (ref_idx > (numRef >> 1)))
                            break;
            }
            // for MV prediction in next block
            mvs[bOff] = mvs[bOff+1] = mvs[bOff+4] = mvs[bOff+5] = BestMV8x8[b];
            refs[bOff] = refs[bOff+1] = refs[bOff+4] = refs[bOff+5] = BestRef8x8[b];
            sbt[b] = (MBTypeValue)SBTYPE_8x8;
        }

        BestSAD8x8 = BITS_COST(4, pRDQM) + bSAD8x8[0] + bSAD8x8[1] + bSAD8x8[2] + bSAD8x8[3];
        b8x8s[0] = b8x8s[1] = b8x8s[2] = b8x8s[3] = (MBTypeValue)SBTYPE_8x8;

        if (RefLater8x8PackFlag)
            meFlags &= ~ANALYSE_P_4x4;

        if ((meFlags & ANALYSE_P_4x4) && (BestSAD8x8 < BestSAD16x16) && BestSAD8x8 > check_threshold && mvsBudget >= 5) {
            mvsRemain = mvsBudget;
            for (Ipp32s s = 0; s < 4; s ++) {
                mvsRemain -= (3 - s);
                ref_idx = BestRef8x8[s];
                PIXTYPE *pCur = cur_mb.mbPtr + (s >> 1) * 8 * cur_mb.mbPitchPixels + (s & 1) * 8;
                PIXTYPE *pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + (s >> 1) * 8 * cur_mb.mbPitchPixels + (s & 1) * 8;
                Ipp16s *pRes = resPredBuf + (s >> 1) * 8 * 16 + (s & 1) * 8;
#ifdef NO_PADDING
                block8x8Pos.x = (s & 1) * 8;
                block8x8Pos.y = (s >> 1) * 8;
                meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                if (core_enc->m_Analyse & ANALYSE_FLATNESS) {
                    __ALIGN16 PIXTYPE mcBlock[64];
                    const PIXTYPE *pI;
                    Ipp32s   sI;
                    IppiSize block = {8, 8};
#ifdef NO_PADDING
                    meInfo.interpolateInfo.pDst[0] = mcBlock;
                    meInfo.interpolateInfo.dstStep = 8;
                    meInfo.interpolateInfo.sizeBlock = block;
                    meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x;
                    meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y;
                    meInfo.interpolateInfo.pointVector.x = BestMV8x8[s].mvx;
                    meInfo.interpolateInfo.pointVector.y = BestMV8x8[s].mvy;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(&(meInfo.interpolateInfo));
                    pI = meInfo.interpolateInfo.pDst[0];
                    sI = meInfo.interpolateInfo.dstStep;
                    meInfo.interpolateInfo.dstStep = 16;
#else // NO_PADDING
                    H264ENC_MAKE_NAME(InterpolateLuma)(MVADJUST(pRef, cur_mb.mbPitchPixels, BestMV8x8[s].mvx >> SUB_PEL_SHIFT, BestMV8x8[s].mvy >> SUB_PEL_SHIFT), cur_mb.mbPitchPixels, mcBlock, 8, BestMV8x8[s].mvx & 3, BestMV8x8[s].mvy & 3, block, meInfo.bit_depth_luma, planeSize, &pI, &sI);
#endif // NO_PADDING
                    Ipp32s sad4x4[4];
                    if (meInfo.residualPredictionFlag) {
                      Ipp32s bl, i, j;
                      for (bl = 0; bl < 4; bl++) {
                        Ipp32s sad = 0;
                        Ipp16s *pRes4x4 = pRes + (bl & 1)*4 + (bl & 2)*2*16;
                        const PIXTYPE *pPred = pI + (bl & 1)*4 + (bl & 2)*2*sI;
                        for (i = 0; i < 4; i++)
                          for (j = 0; j < 4; j++)
                            sad += pRes4x4[i*16 + j] - pPred[i*sI + j];
                        sad4x4[bl] = sad;
                      }
                    } else {
                      sad4x4[0] = SAD4x4(pCur, cur_mb.mbPitchPixels, pI, sI);
                      sad4x4[1] = SAD4x4(pCur + 4, cur_mb.mbPitchPixels, pI + 4, sI);
                      sad4x4[2] = SAD4x4(pCur + 4 * cur_mb.mbPitchPixels, cur_mb.mbPitchPixels, pI + 4 * sI, sI);
                      sad4x4[3] = SAD4x4(pCur + 4 * cur_mb.mbPitchPixels + 4, cur_mb.mbPitchPixels, pI + 4 * sI + 4, sI);
                    }
                    if ((sad4x4[0] <= (EmptyThreshold[uMBQP] >> 4)) && (sad4x4[1] <= (EmptyThreshold[uMBQP] >> 4)) && (sad4x4[2] <= (EmptyThreshold[uMBQP] >> 4)) && (sad4x4[3] <= (EmptyThreshold[uMBQP] >> 4)))
                        continue;
                    else if (sad4x4[0] != 0) {
#define LB 0.75
#define UB (1.0 / LB)
                        double d = 1.0 / sad4x4[0], d1 = sad4x4[1] * d, d2 = sad4x4[2] * d, d3 = sad4x4[3] * d;
                        if ((LB < d1) && (d1 < UB) && (LB < d2) && (d2 < UB) && (LB < d3) && (d3 < UB))
                            continue;
                    }
                }
                PIXTYPE* pCurU = NULL;
                PIXTYPE* pCurV = NULL;
                PIXTYPE* pRefU = NULL;
                PIXTYPE* pRefV = NULL;
                Ipp16s *pResU = NULL, *pResV = NULL;
                if (meFlags & ANALYSE_ME_CHROMA) {
                    meInfo.chroma_mvy_offset = 0;
                    if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                        if (!curr_slice->m_is_cur_mb_bottom_field && pFields[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                        else if (curr_slice->m_is_cur_mb_bottom_field && !pFields[ref_idx]) meInfo.chroma_mvy_offset += 2;
                    }
                    pCurU = pMBCurU + (s >> 1) * 4 * cur_mb.mbPitchPixels + (s & 1) * 4;
                    pCurV = pMBCurV + (s >> 1) * 4 * cur_mb.mbPitchPixels + (s & 1) * 4;
                    pResU = resPredBufC + (s >> 1) * 4 * 16 + (s & 1) * 4;
                    pResV = resPredBufC + 8 + (s >> 1) * 4 * 16 + (s & 1) * 4;
#ifdef USE_NV12
                    pRefU = ppRefPicList[ref_idx]->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + (s >> 1) * 4 * cur_mb.mbPitchPixels + (s & 1) * 8;
                    pRefV = pRefU + 1;
#else // USE_NV12
                    pRefU = ppRefPicList[ref_idx]->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + (s >> 1) * 4 * cur_mb.mbPitchPixels + (s & 1) * 4;
                    pRefV = ppRefPicList[ref_idx]->m_pVPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + (s >> 1) * 4 * cur_mb.mbPitchPixels + (s & 1) * 4;
#endif // USE_NV12
#ifdef NO_PADDING
                    meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
                    meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                }
                // 4x4
                meInfo.block.width = 4;
                meInfo.block.height = 4;
                meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 4 : 0;
                bSADs = BITS_COST(5, pRDQM) + refCost[ref_idx];
                for (Ipp32s b = 0; b < 4; b ++) {
                    bOff = block_subblock_mapping[s*4+b];
                    meInfo.pCur = pCur + (b >> 1) * 4 * cur_mb.mbPitchPixels + (b & 1) * 4;
                    meInfo.pRef = pRef + (b >> 1) * 4 * cur_mb.mbPitchPixels + (b & 1) * 4;
                    meInfo.pResY = pRes + (b >> 1) * 4 * 16 + (b & 1) * 4;
#ifdef NO_PADDING
                    meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x + (b & 1) * 4;
                    meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y + (b >> 1) * 4;
#endif // NO_PADDING
                    if (meFlags & ANALYSE_ME_CHROMA) {
                        meInfo.pCurU = pCurU + (b >> 1) * 2 * cur_mb.mbPitchPixels + (b & 1) * 2;
                        meInfo.pCurV = pCurV + (b >> 1) * 2 * cur_mb.mbPitchPixels + (b & 1) * 2;
                        meInfo.pResU = pResU + (b >> 1) * 2 * 16 + (b & 1) * 2;
                        meInfo.pResV = pResV + (b >> 1) * 2 * 16 + (b & 1) * 2;
#ifdef USE_NV12
                        meInfo.pRefU = pRefU + (b >> 1) * 2 * cur_mb.mbPitchPixels + (b & 1) * 4;
                        meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                        meInfo.pRefU = pRefU + (b >> 1) * 2 * cur_mb.mbPitchPixels + (b & 1) * 2;
                        meInfo.pRefV = pRefV + (b >> 1) * 2 * cur_mb.mbPitchPixels + (b & 1) * 2;
#endif // USE_NV12
                    }
                    H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 1, 1, &meInfo);
                    meInfo.candNum = 0;
                    meInfo.bestSAD = MAX_SAD;
                    //meInfo.predictedMV = PredMV;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[s]);
                    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                    if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD > meInfo.threshold)
                        H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                    BestMV4x4[s][b] = meInfo.bestMV;
                    BestPredMV4x4[s][b] = meInfo.bestPredMV;
                    mpred4x4[s][b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                    bSADs += meInfo.bestSAD;
                    // for MV prediction in next block
                    mvs[bOff] = BestMV4x4[s][b];
                }
                if (bSADs < bSAD8x8[s]) {
                    if (mvsRemain >= 4) {
                        b8x8s[s] = (MBTypeValue)SBTYPE_4x4;
                        bSAD8x8[s] = bSADs;
                    }
                    // 8x4
                    meInfo.block.width = 8;
                    meInfo.block.height = 4;
                    meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 3 : 0;
                    bSADs = BITS_COST(3, pRDQM) + refCost[ref_idx];
                    for (Ipp32s b = 0; b < 2; b ++) {
                        bOff = block_subblock_mapping[s*4+b*2];
                        meInfo.pCur = pCur + b * 4 * cur_mb.mbPitchPixels;
                        meInfo.pRef = pRef + b * 4 * cur_mb.mbPitchPixels;
                        meInfo.pResY = pRes + b * 4 * 16;
#ifdef NO_PADDING
                        meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x;
                        meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y + b * 4;
#endif // NO_PADDING
                        if (meFlags & ANALYSE_ME_CHROMA) {
                            meInfo.pCurU = pCurU + b * 2 * cur_mb.mbPitchPixels;
                            meInfo.pCurV = pCurV + b * 2 * cur_mb.mbPitchPixels;
                            meInfo.pRefU = pRefU + b * 2 * cur_mb.mbPitchPixels;
                            meInfo.pRefV = pRefV + b * 2 * cur_mb.mbPitchPixels;
                            meInfo.pResU = pResU + b * 2 * 16;
                            meInfo.pResV = pResV + b * 2 * 16;
                        }
                        H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 2, 1, &meInfo);
                        meInfo.candNum = 0;
                        meInfo.bestSAD = MAX_SAD;
                        //meInfo.predictedMV = PredMV;
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[s]);
                        H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                        if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD > meInfo.threshold)
                            H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                        BestMV8x4[s][b] = meInfo.bestMV;
                        BestPredMV8x4[s][b] = meInfo.bestPredMV;
                        mpred8x4[s][b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                        bSADs += meInfo.bestSAD;
                        // for MV prediction in next block
                        mvs[bOff] = mvs[bOff+1] = BestMV8x4[s][b];
                    }
                    if (bSADs < bSAD8x8[s] && (mvsRemain >= 2)) {
                        b8x8s[s] = (MBTypeValue)SBTYPE_8x4;
                        bSAD8x8[s] = bSADs;
                    }
                    // 4x8
                    meInfo.block.width = 4;
                    meInfo.block.height = 8;
                    meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 3 : 0;
                    bSADs = BITS_COST(3, pRDQM) + refCost[ref_idx];
                    for (Ipp32s b = 0; b < 2; b ++) {
                        bOff = block_subblock_mapping[s*4+b];
                        meInfo.pCur = pCur + b * 4;
                        meInfo.pRef = pRef + b * 4;
                        meInfo.pResY = pRes + b * 4;
#ifdef NO_PADDING
                        meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x + b * 4;
                        meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y;
#endif // NO_PADDING
                        if (meFlags & ANALYSE_ME_CHROMA) {
#ifdef USE_NV12
                            meInfo.pRefU = pRefU + b * 4;
                            meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                            meInfo.pRefU = pRefU + b * 2;
                            meInfo.pRefV = pRefV + b * 2;
#endif // USE_NV12
                            meInfo.pCurU = pCurU + b * 2;
                            meInfo.pCurV = pCurV + b * 2;
                            meInfo.pResU = pResU + b * 2;
                            meInfo.pResV = pResV + b * 2;
                        }
                        H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 1, 2, &meInfo);
                        meInfo.candNum = 0;
                        meInfo.bestSAD = MAX_SAD;
                        //meInfo.predictedMV = PredMV;
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[s]);
                        H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                        if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD > meInfo.threshold)
                            H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                        BestMV4x8[s][b] = meInfo.bestMV;
                        BestPredMV4x8[s][b] = meInfo.bestPredMV;
                        mpred4x8[s][b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                        bSADs += meInfo.bestSAD;
                        // for MV prediction in next block
                        mvs[bOff] = mvs[bOff+4] = BestMV4x8[s][b];
                    }
                    if (bSADs < bSAD8x8[s] && (mvsRemain >= 2)) {
                        b8x8s[s] = (MBTypeValue)SBTYPE_4x8;
                        bSAD8x8[s] = bSADs;
                    }
                }
                // set correct MVs for prediction in next subblocks
                bOff = block_subblock_mapping[s*4];
                sbt[s] = b8x8s[s];
                if (b8x8s[s] == SBTYPE_8x8) {
                    mvs[bOff] = mvs[bOff+1] = mvs[bOff+4] = mvs[bOff+5] = BestMV8x8[s];
                    mvsRemain += (3 - s) - 1;
                } else if (b8x8s[s] == SBTYPE_4x4) {
                    mvs[bOff+0] = BestMV4x4[s][0];
                    mvs[bOff+1] = BestMV4x4[s][1];
                    mvs[bOff+4] = BestMV4x4[s][2];
                    mvs[bOff+5] = BestMV4x4[s][3];
                    mvsRemain += (3 - s) - 4;
                } else if (b8x8s[s] == SBTYPE_8x4) {
                    mvs[bOff+0] = mvs[bOff+1] = BestMV8x4[s][0];
                    mvs[bOff+4] = mvs[bOff+5] = BestMV8x4[s][1];
                    mvsRemain += (3 - s) - 2;
                } else {
                    mvs[bOff+0] = mvs[bOff+4] = BestMV4x8[s][0];
                    mvs[bOff+1] = mvs[bOff+5] = BestMV4x8[s][1];
                    mvsRemain += (3 - s) - 2;
                }
            }
            BestSAD8x8 = BITS_COST(4, pRDQM) + bSAD8x8[0] + bSAD8x8[1] + bSAD8x8[2] + bSAD8x8[3];
        }

        if ((core_enc->m_Analyse & ANALYSE_RD_OPT) && (BestSAD8x8 <= (BestSAD16x16 * SB_THRESH_RD))) {
            bool is_ref0 = (BestRef8x8[0] + BestRef8x8[1] + BestRef8x8[2] + BestRef8x8[3]) == 0;
            cur_mb.GlobalMacroblockInfo->mbtype = is_ref0 ? MBTYPE_INTER_8x8_REF0 : MBTYPE_INTER_8x8;
#ifdef USE_NV12
            rdCost8x8 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, minRDCost, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
            rdCost8x8 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost)(state, curr_slice, 0, minRDCost );
#endif // USE_NV12
            if( rdCost8x8 < minRDCost ) minRDCost = rdCost8x8;
        }

        //f TODO threshold
        if (BestSAD8x8 < BestSAD16x16 + (Ipp32s)BITS_COST(8, pRDQM) && BestSAD8x8 > check_threshold && (mvsBudget >= 2)) {
            // 16x8
            meInfo.block.width = 16;
            meInfo.block.height = 8;
            meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 1 : 0;
            BestSAD16x8 = BITS_COST(2, pRDQM);
            for (Ipp32s b = 0; b < 2; b ++) {
                bSAD16x8[b] = MAX_SAD;
                bOff = b * 8;
                meInfo.pCur = cur_mb.mbPtr + b * 8 * cur_mb.mbPitchPixels;
                meInfo.pResY = resPredBuf + b * 8 * 16;
#ifdef NO_PADDING
                meInfo.interpolateInfo.pointBlockPos.x = mbPos.x;
                meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + b * 8;
#endif // NO_PADDING
                if (meFlags & ANALYSE_ME_CHROMA) {
                    meInfo.pCurU = pMBCurU + b * 4 * cur_mb.mbPitchPixels;
                    meInfo.pCurV = pMBCurV + b * 4 * cur_mb.mbPitchPixels;
                    meInfo.pResU = resPredBufC + b * 4 * 16;
                    meInfo.pResV = resPredBufC + 8 + b * 4 * 16;
                }
                if (meFlags & ANALYSE_ME_ALL_REF) {
                    refMin = 0;
                    refMax = numRef - 1;
                    refStep = 1;
                } else {
                    refMin = IPP_MIN(BestRef8x8[b*2+0], BestRef8x8[b*2+1]);
                    refMax = IPP_MAX(BestRef8x8[b*2+0], BestRef8x8[b*2+1]);
                    refStep = refMax - refMin;
                    if (refStep == 0)
                        refStep = 1;
                }
                for (ref_idx = refMin; ref_idx <= refMax; ref_idx += refStep) {
#ifdef H264_INTRA_REFRESH
                    if (core_enc->m_refrType) {
                        if (ppRefPicList[ref_idx]->m_periodCount < core_enc->m_periodCount ||
                            (curMBLine < core_enc->m_firstLineInStripe && ppRefPicList[ref_idx]->m_firstLineInStripe < 0))
                            continue;
                        else if (curMBLine < core_enc->m_firstLineInStripe) {
                            Ipp16s maxMV = (ppRefPicList[ref_idx]->m_firstLineInStripe + core_enc->m_stripeWidth - 2 - curMBLine) * 16;
                            if (core_enc->m_refrType == HORIZ_REFRESH)
                                meInfo.yMax = maxMV;
                            else if (core_enc->m_refrType == VERT_REFRESH)
                                meInfo.xMax = maxMV;
                        }
                    }
#endif // H264_INTRA_REFRESH
                    refs[bOff] = ref_idx;
                    meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + b * 8 * cur_mb.mbPitchPixels;
#ifdef NO_PADDING
                    meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    if (meFlags & ANALYSE_ME_CHROMA) {
                        meInfo.chroma_mvy_offset = 0;
                        if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                            if (!curr_slice->m_is_cur_mb_bottom_field && pFields[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                            else if (curr_slice->m_is_cur_mb_bottom_field && !pFields[ref_idx]) meInfo.chroma_mvy_offset += 2;
                        }
                        meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + b * 4 * cur_mb.mbPitchPixels;
                        meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + b * 4 * cur_mb.mbPitchPixels;
#ifdef NO_PADDING
                        meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
                        meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    }
                    H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 4, 2, &meInfo);
                    meInfo.candNum = 0;
                    meInfo.bestSAD = MAX_SAD;
                    //meInfo.predictedMV = PredMV;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                    //H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV16x16);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, bMV16x16[ref_idx]);
                    //if (b > 0)
                    //    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV16x8[0]);
                    //if (b == 0) {
                    //    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[0]);
                    //    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[1]);
                    //} else {
                    //    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[2]);
                    //    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[3]);
                    //}
                    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                    if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCost[ref_idx] > meInfo.threshold)
                        H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                    if (meInfo.bestSAD + refCost[ref_idx] < bSAD16x8[b]) {
                        bSAD16x8[b] = meInfo.bestSAD + refCost[ref_idx];
                        BestMV16x8[b] = meInfo.bestMV;
                        BestRef16x8[b] = ref_idx;
                        BestPredMV16x8[b] = meInfo.bestPredMV; //meInfo.predictedMV;
                        mpred16x8[b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                    }
                }
                BestSAD16x8 += bSAD16x8[b];
                // for MV prediction in next block
                for (i = 0; i < 8; i ++) {
                    mvs[bOff+i] = BestMV16x8[b];
                    refs[bOff+i] = BestRef16x8[b];
                }
            }
            if ((core_enc->m_Analyse & ANALYSE_RD_OPT) && ((BestSAD16x8 <= (BestSAD16x16 * SB_THRESH_RD)) || (BestSAD16x8 <= (BestSAD8x8 * SB_THRESH_RD)))) {
                cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_16x8;
#ifdef USE_NV12
                rdCost16x8 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, minRDCost, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
                rdCost16x8 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost)(state, curr_slice, 0, minRDCost );
#endif // USE_NV12
                if(  rdCost16x8 < minRDCost ) minRDCost = rdCost16x8;
            }
            // 8x16
            meInfo.block.width = 8;
            meInfo.block.height = 16;
            meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 1 : 0;
            BestSAD8x16 = BITS_COST(2, pRDQM);
            for (Ipp32s b = 0; b < 2; b ++) {
                bSAD8x16[b] = MAX_SAD;
                bOff = b * 2;
                meInfo.pCur = cur_mb.mbPtr + b * 8;
                meInfo.pResY = resPredBuf + b * 8;
#ifdef NO_PADDING
                meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + b * 8;
                meInfo.interpolateInfo.pointBlockPos.y = mbPos.y;
#endif // NO_PADDING
                if (meFlags & ANALYSE_ME_CHROMA) {
                    meInfo.pCurU = pMBCurU + b * 4;
                    meInfo.pCurV = pMBCurV + b * 4;
                    meInfo.pResU = resPredBufC + b * 4;
                    meInfo.pResV = resPredBufC + 8 + b * 4;
                }
                if (meFlags & ANALYSE_ME_ALL_REF) {
                    refMin = 0;
                    refMax = numRef - 1;
                    refStep = 1;
                } else {
                    refMin = IPP_MIN(BestRef8x8[b], BestRef8x8[b+2]);
                    refMax = IPP_MAX(BestRef8x8[b], BestRef8x8[b+2]);
                    refStep = refMax - refMin;
                    if (refStep == 0)
                        refStep = 1;
                }
                for (ref_idx = refMin; ref_idx <= refMax; ref_idx += refStep) {
#ifdef H264_INTRA_REFRESH
                    if (core_enc->m_refrType) {
                        if (ppRefPicList[ref_idx]->m_periodCount < core_enc->m_periodCount ||
                            (curMBLine < core_enc->m_firstLineInStripe && ppRefPicList[ref_idx]->m_firstLineInStripe < 0))
                            continue;
                        else if (curMBLine < core_enc->m_firstLineInStripe) {
                            Ipp16s maxMV = (ppRefPicList[ref_idx]->m_firstLineInStripe + core_enc->m_stripeWidth - 2 - curMBLine) * 16;
                            if (core_enc->m_refrType == HORIZ_REFRESH)
                                meInfo.yMax = maxMV;
                            else if (core_enc->m_refrType == VERT_REFRESH)
                                meInfo.xMax = maxMV;
                        }
                    }
#endif // H264_INTRA_REFRESH
                    refs[bOff] = ref_idx;
                    meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + b * 8;
#ifdef NO_PADDING
                    meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    if (meFlags & ANALYSE_ME_CHROMA) {
                         meInfo.chroma_mvy_offset = 0;
                        if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                            if (!curr_slice->m_is_cur_mb_bottom_field && pFields[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                            else if (curr_slice->m_is_cur_mb_bottom_field && !pFields[ref_idx]) meInfo.chroma_mvy_offset += 2;
                        }
#ifdef USE_NV12
                        meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + b * 8;
                        meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                        meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + b * 4;
                        meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[ref_idx]] + b * 4;
#endif // USE_NV12
#ifdef NO_PADDING
                        meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
                        meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    }
                    H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 2, 4, &meInfo);
                    meInfo.candNum = 0;
                    meInfo.bestSAD = MAX_SAD;
                    //meInfo.predictedMV = PredMV;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                    //H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV16x16);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, bMV16x16[ref_idx]);
                    //if (b > 0)
                    //    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x16[0]);
                    //if (b == 0) {
                        //H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[0]);
                        //H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[2]);
                    //} else {
                        //H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[1]);
                        //H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8[3]);
                    //}
                    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                    if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCost[ref_idx] > meInfo.threshold)
                        H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                    if (meInfo.bestSAD + refCost[ref_idx] < bSAD8x16[b]) {
                        bSAD8x16[b] = meInfo.bestSAD + refCost[ref_idx];
                        BestMV8x16[b] = meInfo.bestMV;
                        BestRef8x16[b] = ref_idx;
                        BestPredMV8x16[b] = meInfo.bestPredMV; //meInfo.predictedMV;
                        mpred8x16[b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                    }
                }
                BestSAD8x16 += bSAD8x16[b];
                // for MV prediction in next block
                for (i = 0; i < 4; i ++) {
                    mvs[bOff+i*4] = mvs[bOff+i*4+1] = BestMV8x16[b];
                    refs[bOff+i*4] = refs[bOff+i*4+1] = BestRef8x16[b];
                }
            }
            if ((core_enc->m_Analyse & ANALYSE_RD_OPT) && ((BestSAD8x16 <= (BestSAD16x16 * SB_THRESH_RD)) || (BestSAD8x16 <= (BestSAD8x8 * SB_THRESH_RD)))) {
                cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x16;
#ifdef USE_NV12
                rdCost8x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, minRDCost, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
                rdCost8x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost)(state, curr_slice, 0, minRDCost );
#endif // USE_NV12
                if( rdCost8x16 < minRDCost ) minRDCost = rdCost8x16;
            }
        }
    }

    if (core_enc->m_Analyse & ANALYSE_RD_OPT) {
        BestSAD16x16 = rdCost16x16;
        BestSAD16x8 = rdCost16x8;
        BestSAD8x16 = rdCost8x16;
        BestSAD8x8 = rdCost8x8;
    }
    cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
    cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
    for (i = 0; i < 16; i ++){
        cur_mb.RefIdxs[LIST_1]->RefIdxs[i] = (T_RefIdx)-1;
        cur_mb.MVs[LIST_1]->MotionVectors[i] = null_mv;
    }
    if ((BestSAD16x16 <= BestSAD8x8) && (BestSAD16x16 <= BestSAD16x8) && (BestSAD16x16 <= BestSAD8x16)) {
        for (i = 0; i < 16; i ++) {
            refs[i] = (T_RefIdx)BestRef16x16;
            mvs[i] = BestMV16x16;
        }
        sbt[0] = sbt[1] = sbt[2] = sbt[3] = (MBTypeValue)NUMBER_OF_MBTYPES;
        if (mpred16x16) mpredflag |= 0xffff;
        cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER;
        cur_mb.LocalMacroblockInfo->m_mvsBudget = 1;
        if (meFlags & ANALYSE_CBP_EMPTY)
            CHECK_CBP_EMPTY_THRESH(BestSAD16x16 - MVConstraint(BestMV16x16.mvx - BestPredMV16x16.mvx, BestMV16x16.mvy - BestPredMV16x16.mvy, pRDQM) - refCost[BestRef16x16], 0xff0000, 0);
        BestSAD = BestSAD16x16;
    } else if ((BestSAD8x8 <= BestSAD16x8) && (BestSAD8x8 <= BestSAD8x16)) {
        refs[0] = refs[1] = refs[4] = refs[5] = (T_RefIdx)BestRef8x8[0];
        refs[2] = refs[3] = refs[6] = refs[7] = (T_RefIdx)BestRef8x8[1];
        refs[8] = refs[9] = refs[12] = refs[13] = (T_RefIdx)BestRef8x8[2];
        refs[10] = refs[11] = refs[14] = refs[15] = (T_RefIdx)BestRef8x8[3];
        for (Ipp32s s = 0; s < 4; s ++) {
            sbt[s] = b8x8s[s];
            bOff = block_subblock_mapping[s*4];
            if (b8x8s[s] == SBTYPE_8x8) {
                mvs[bOff] = mvs[bOff+1] = mvs[bOff+4] = mvs[bOff+5] = BestMV8x8[s];
                if (mpred8x8[s]) mpredflag |= 0x0033<<bOff;
            } else if (b8x8s[s] == SBTYPE_4x4) {
                mvs[bOff+0] = BestMV4x4[s][0];
                mvs[bOff+1] = BestMV4x4[s][1];
                mvs[bOff+4] = BestMV4x4[s][2];
                mvs[bOff+5] = BestMV4x4[s][3];
                if (mpred4x4[s][0]) mpredflag |= 0x0001<<bOff;
                if (mpred4x4[s][1]) mpredflag |= 0x0002<<bOff;
                if (mpred4x4[s][2]) mpredflag |= 0x0010<<bOff;
                if (mpred4x4[s][3]) mpredflag |= 0x0020<<bOff;
            } else if (b8x8s[s] == SBTYPE_8x4) {
                mvs[bOff+0] = mvs[bOff+1] = BestMV8x4[s][0];
                mvs[bOff+4] = mvs[bOff+5] = BestMV8x4[s][1];
                if (mpred8x4[s][0]) mpredflag |= 0x0003<<bOff;
                if (mpred8x4[s][1]) mpredflag |= 0x0030<<bOff;
            } else {
                mvs[bOff+0] = mvs[bOff+4] = BestMV4x8[s][0];
                mvs[bOff+1] = mvs[bOff+5] = BestMV4x8[s][1];
                if (mpred4x8[s][0]) mpredflag |= 0x0011<<bOff;
                if (mpred4x8[s][1]) mpredflag |= 0x0022<<bOff;
            }
        }
        bool is_ref0 = (BestRef8x8[0] + BestRef8x8[1] + BestRef8x8[2] + BestRef8x8[3]) == 0;
        cur_mb.GlobalMacroblockInfo->mbtype = is_ref0 ? MBTYPE_INTER_8x8_REF0 : MBTYPE_INTER_8x8;
        cur_mb.LocalMacroblockInfo->m_mvsBudget = mvsBudget - mvsRemain;
        if (meFlags & ANALYSE_CBP_EMPTY)
            for (i = 0; i < 4; i ++)
                if (sbt[i] == SBTYPE_8x8)
                    CHECK_CBP_EMPTY_THRESH(bSAD8x8[i] - MVConstraint(BestMV8x8[i].mvx - BestPredMV8x8[i].mvx, BestMV8x8[i].mvy - BestPredMV8x8[i].mvy, pRDQM) - refCost[BestRef8x8[i]], ~(0xf<<(i<<2)), 2);
        BestSAD = BestSAD8x8;
    } else if ((BestSAD16x8 <= BestSAD8x16)) {
        for (i = 0; i < 8; i ++) {
            refs[i] = (T_RefIdx)BestRef16x8[0];
            refs[i+8] = (T_RefIdx)BestRef16x8[1];
            mvs[i] = BestMV16x8[0];
            mvs[i+8] = BestMV16x8[1];
        }
        sbt[0] = sbt[1] = sbt[2] = sbt[3] = (MBTypeValue)NUMBER_OF_MBTYPES;
        if (mpred16x8[0]) mpredflag |= 0x00ff;
        if (mpred16x8[1]) mpredflag |= 0xff00;
        cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_16x8;
        cur_mb.LocalMacroblockInfo->m_mvsBudget = 2;
        if (meFlags & ANALYSE_CBP_EMPTY) {
            CHECK_CBP_EMPTY_THRESH(bSAD16x8[0] - MVConstraint(BestMV16x8[0].mvx - BestPredMV16x8[0].mvx, BestMV16x8[0].mvy - BestPredMV16x8[0].mvy, pRDQM) - refCost[BestRef16x8[0]], 0xffff00, 1);
            CHECK_CBP_EMPTY_THRESH(bSAD16x8[1] - MVConstraint(BestMV16x8[1].mvx - BestPredMV16x8[1].mvx, BestMV16x8[1].mvy - BestPredMV16x8[1].mvy, pRDQM) - refCost[BestRef16x8[1]], 0xff00ff, 1);
        }
        BestSAD = BestSAD16x8;
    } else {
        for (i = 0; i < 4; i ++) {
            refs[i*4] = refs[i*4+1] = (T_RefIdx)BestRef8x16[0];
            refs[i*4+2] = refs[i*4+3] = (T_RefIdx)BestRef8x16[1];
            mvs[i*4] = mvs[i*4+1] = BestMV8x16[0];
            mvs[i*4+2] = mvs[i*4+3] = BestMV8x16[1];
        }
        sbt[0] = sbt[1] = sbt[2] = sbt[3] = (MBTypeValue)NUMBER_OF_MBTYPES;
        if (mpred8x16[0]) mpredflag |= 0x3333;
        if (mpred8x16[1]) mpredflag |= 0xcccc;
        cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x16;
        cur_mb.LocalMacroblockInfo->m_mvsBudget = 2;
        if (meFlags & ANALYSE_CBP_EMPTY) {
            CHECK_CBP_EMPTY_THRESH(bSAD8x16[0] - MVConstraint(BestMV8x16[0].mvx - BestPredMV8x16[0].mvx, BestMV8x16[0].mvy - BestPredMV8x16[0].mvy, pRDQM) - refCost[BestRef8x16[0]], 0xfff0f0, 1);
            CHECK_CBP_EMPTY_THRESH(bSAD8x16[1] - MVConstraint(BestMV8x16[1].mvx - BestPredMV8x16[1].mvx, BestMV8x16[1].mvy - BestPredMV8x16[1].mvy, pRDQM) - refCost[BestRef8x16[1]], 0xff0f0f, 1);
        }
        BestSAD = BestSAD8x16;
    }

    if (!(core_enc->m_Analyse & ANALYSE_RD_OPT) && (core_enc->m_Analyse & ANALYSE_RD_MODE)) {
        if ((core_enc->m_Analyse & ANALYSE_INTRA_IN_ME) && (!resPredFlag))
        {
            if (BestSAD == BestSAD16x16)
                minRDCost = BestSAD = rdCost16x16;
            else
#ifdef USE_NV12
                minRDCost = BestSAD = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, minRDCost, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
                minRDCost = BestSAD = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost)(state, curr_slice, 0, minRDCost );
#endif // USE_NV12
        } else
#ifdef USE_NV12
          minRDCost = BestSAD = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, minRDCost, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
            minRDCost = BestSAD = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost)(state, curr_slice, 0, minRDCost );
#endif // USE_NV12
    }

    // choose best transform
    Ipp32s trans8x8 = core_enc->m_info.transform_8x8_mode_flag;
    if (trans8x8 && ((cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8_REF0) || (cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8)))
        if ((sbt[0] != SBTYPE_8x8) || (sbt[1] != SBTYPE_8x8) || (sbt[2] != SBTYPE_8x8) || (sbt[3] != SBTYPE_8x8))
            trans8x8 = 0;

    if (!check2TransformModes)
    {
        trans8x8 = RefLater8x8PackFlag;
    }

    if ((trans8x8) && (check2TransformModes)) {
        if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE)) {
#ifdef USE_NV12
            Ipp32s rdCost8x8t = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost_NV12)(state, curr_slice, 1, 1, minRDCost, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
            Ipp32s rdCost8x8t = H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost)(state, curr_slice, 1, minRDCost );
#endif // USE_NV12
            if (BestSAD <= rdCost8x8t)
                trans8x8 = 0;
            else
                BestSAD = rdCost8x8t;
        } else {
            __ALIGN16 PIXTYPE mcBlock[256];
            __ALIGN16 Ipp16s diffBuff[16][16];
            Ipp32s sad4x4t = 0;
            Ipp32s sad8x8t = 0;

            H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBLuma)(state, curr_slice, mvs, NULL, mcBlock);

            if (!meInfo.residualPredictionFlag)
            {
                sad4x4t = SATD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, mcBlock, 16);
                sad8x8t = SAT8x8D(cur_mb.mbPtr, cur_mb.mbPitchPixels, mcBlock, 16);
                sad8x8t+= SAT8x8D(cur_mb.mbPtr + 8, cur_mb.mbPitchPixels, mcBlock + 8, 16);
                sad8x8t+= SAT8x8D(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels, cur_mb.mbPitchPixels, mcBlock + 8 * 16, 16);
                sad8x8t+= SAT8x8D(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels + 8, cur_mb.mbPitchPixels, mcBlock + 8 * 16 + 8, 16);
            }
            else
            {
                Ipp32s i, j;
                for (i = 0; i < 16; i++)
                    for (j = 0; j < 16; j++)
                        diffBuff[i][j] = resPredBuf[i*16 + j] - mcBlock[i*16 + j];
                for (i = 0; i < 2; i++)
                    for (j = 0; j < 2; j++)
                        sad8x8t += Hadamard8x8_16_AbsSum(&(diffBuff[i*8][j*8]));
                for (i = 0; i < 4; i++)
                    for (j = 0; j < 4; j++)
                        sad4x4t += Hadamard4x4_16_AbsSum(&(diffBuff[i*4][j*4]));

            }

            if (sad4x4t < sad8x8t + (Ipp32s)BITS_COST(2, pRDQM))
                trans8x8 = 0;
            //else
            //    BestSAD = sad8x8t + BestSAD - sad4x4t;
        }
    }

    pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, trans8x8);

    if ((core_enc->m_Analyse & ANALYSE_INTRA_IN_ME) && (!resPredFlag))
    {
        T_AIMode intra_types_save[16];
        Ipp32u uInterCBP4x4 = cur_mb.LocalMacroblockInfo->cbp_luma;
        MBTypeValue MBTypeInter = cur_mb.GlobalMacroblockInfo->mbtype;
        Ipp32s InterMB8x8PackFlag = pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo);
        bool bIntra8x8 = false;

        if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || !(BestSAD < EmptyThreshold[uMBQP])) {
            H264MacroblockLocalInfo sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
    //        H264MacroblockGlobalInfo sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;
            *cur_mb.LocalMacroblockInfo = intraLocalMBinfo;
            if (BestIntraSAD + intra_cost_chroma <= (BestSAD * 5 >> 2)) {
                if ((core_enc->m_Analyse & ANALYSE_I_4x4) || (core_enc->m_Analyse & ANALYSE_I_8x8)) {
                    Ipp32s MBHasEdges;
                    H264ENC_MAKE_NAME(ippiEdgesDetect16x16)(cur_mb.mbPtr, cur_mb.mbPitchPixels, EdgePelDiffTable[uMBQP], EdgePelCountTable[uMBQP], &MBHasEdges);
                    if (MBHasEdges) {
                        if (core_enc->m_Analyse & ANALYSE_I_8x8) {
                            pSetMB8x8TSFlag(curr_slice->m_cur_mb.GlobalMacroblockInfo, true);
                            H264ENC_MAKE_NAME(H264CoreEncoder_AdvancedIntraModeSelectOneMacroblock8x8)(state, curr_slice, BestIntraSAD, &BestIntraSAD8x8);
                            //Save intra_types
                            MFX_INTERNAL_CPY(intra_types_save, curr_slice->m_cur_mb.intra_types, 16 * sizeof(T_AIMode));
                            if ((Ipp32s)BestIntraSAD8x8 < BestIntraSAD) {
                                BestIntraSAD = BestIntraSAD8x8;
                                BestIntraMBType = MBTYPE_INTRA;
                                bIntra8x8 = true;
                            }
                            pSetMB8x8TSFlag(curr_slice->m_cur_mb.GlobalMacroblockInfo, false);
                            if ((core_enc->m_Analyse & ANALYSE_I_4x4) && (BestIntraSAD8x8 <= (BestIntraSAD16x16 * 5 >> 2))) {
                                H264ENC_MAKE_NAME(H264CoreEncoder_AdvancedIntraModeSelectOneMacroblock)(state, curr_slice, BestIntraSAD, &BestIntraSAD4x4);
                                if ((Ipp32s)BestIntraSAD4x4 < BestIntraSAD) {
                                    BestIntraSAD = BestIntraSAD4x4;
                                    BestIntraMBType = MBTYPE_INTRA;
                                    bIntra8x8 = false;
                                }
                            }
                        } else if (core_enc->m_Analyse & ANALYSE_I_4x4) {
                            H264ENC_MAKE_NAME(H264CoreEncoder_AdvancedIntraModeSelectOneMacroblock)(state, curr_slice, BestIntraSAD, &BestIntraSAD4x4);
                            if ((Ipp32s)BestIntraSAD4x4 < BestIntraSAD) {
                                BestIntraSAD = BestIntraSAD4x4;
                                BestIntraMBType = MBTYPE_INTRA;
                                bIntra8x8 = false;
                            }
                        }
                    }
                }
            }
            BestIntraSAD += intra_cost_chroma;
    //        if (!((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE)) && (curr_slice->m_slice_type == BPREDSLICE))
    //            BestIntraSAD += BITS_COST(9, glob_RDQM[uMBQP]);
            if (BestIntraSAD < BestSAD) {
                cur_mb.GlobalMacroblockInfo->mbtype = BestIntraMBType;
                pSetMB8x8TSPackFlag(curr_slice->m_cur_mb.GlobalMacroblockInfo, bIntra8x8);
                if (BestIntraMBType == MBTYPE_INTRA && !bIntra8x8)
                    cur_mb.LocalMacroblockInfo->cbp_luma = cur_mb.m_uIntraCBP4x4;
                else if (BestIntraMBType == MBTYPE_INTRA && bIntra8x8) {
                    cur_mb.LocalMacroblockInfo->cbp_luma = cur_mb.m_uIntraCBP8x8;
                    //Restore intra_types
                    MFX_INTERNAL_CPY( curr_slice->m_cur_mb.intra_types, intra_types_save, 16*sizeof(T_AIMode));
                } else
                    cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
                cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
                curr_slice->m_Intra_MB_Counter ++;
                BestSAD = BestIntraSAD;
            } else {
                *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
                cur_mb.LocalMacroblockInfo->intra_chroma_mode = 0; //Needed for packing
                cur_mb.GlobalMacroblockInfo->mbtype = MBTypeInter;
                cur_mb.LocalMacroblockInfo->cbp_luma = uInterCBP4x4;
                pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, InterMB8x8PackFlag);
            }
        }
    }

#ifdef USE_NV12
    if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
    {
        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
        for(i=0;i<8;i++){
            Ipp32s s = 8*i;

            pSrc[0] = pUSrc[s+0];
            pSrc[1] = pVSrc[s+0];
            pSrc[2] = pUSrc[s+1];
            pSrc[3] = pVSrc[s+1];
            pSrc[4] = pUSrc[s+2];
            pSrc[5] = pVSrc[s+2];
            pSrc[6] = pUSrc[s+3];
            pSrc[7] = pVSrc[s+3];
            pSrc[8] = pUSrc[s+4];
            pSrc[9] = pVSrc[s+4];
            pSrc[10] = pUSrc[s+5];
            pSrc[11] = pVSrc[s+5];
            pSrc[12] = pUSrc[s+6];
            pSrc[13] = pVSrc[s+6];
            pSrc[14] = pUSrc[s+7];
            pSrc[15] = pVSrc[s+7];

            pSrc += meInfo.pitchPixels;
        }
    }
#endif // USE_NV12

    //  Extra check - flag should already be set in that case
    if (!core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag &&
        pGetMBCropFlag(cur_mb.GlobalMacroblockInfo) &&
        !curr_slice->m_adaptive_motion_prediction_flag && curr_slice->m_default_motion_prediction_flag)
        mpredflag = 0xffff;

    cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = 0;
    if (mpredflag)
        cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = mpredflag;
    else
        cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = 0;
    return BestSAD;
}

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_ME_B)(
    void* state,
    H264SliceType *curr_slice)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s uMB = cur_mb.uMB;
    T_EncodeMBOffsets *pMBOffset = &core_enc->m_pMBOffsets[uMB];
    Ipp32s is_cur_mb_field = curr_slice->m_is_cur_mb_field;
    H264EncoderFrameType **ppRefPicList;
    Ipp8s *pFields0 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_Prediction;
    Ipp8s *pFields1 = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, is_cur_mb_field, uMB & 1)->m_Prediction;
    Ipp32u uOffset = pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    Ipp32u uChromaOffset = pMBOffset->uChromaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
    Ipp32s iQP = cur_mb.lumaQP51;
    Ipp32u uMBQP  = cur_mb.lumaQP;
    bool transform_bypass = core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag && uMBQP == 0;

    Ipp16s *pRDQM = glob_RDQM[iQP];
    Ipp32s ref_idx, numRef, i, bOff, refCostL0[MAX_NUM_REF_FRAMES], refCostL1[MAX_NUM_REF_FRAMES];
    //4x4, 4x8, 8x4 data
    H264MotionVector BestMV4x4L0[4][4], BestMV4x4L1[4][4];
    H264MotionVector BestMV4x8L0[4][2], BestMV4x8L1[4][2];
    H264MotionVector BestMV8x4L0[4][2], BestMV8x4L1[4][2];
    Ipp32s BestRef4x4L0[4][4], BestRef4x4L1[4][4];
    //8x8 data
    H264MotionVector BestMV8x8L0, BestMV8x8L1, /*PredMV8x8L0, PredMV8x8L1,*/ BestMV8x8SL0[4], BestMV8x8SL1[4];
    H264MotionVector BestMV8x8L0R[4], BestMV8x8L1R[4];
    Ipp32s BestRef8x8L0, BestRef8x8L1, BestSAD8x8L0, BestSAD8x8L1, BestSAD8x8Bi, BestSAD8x8[4]={MAX_SAD,MAX_SAD,MAX_SAD,MAX_SAD}, BestSAD8x8S;
    Ipp32s BestRef8x8SL0[4], BestRef8x8SL1[4];
    //Ipp32s BestRef8x8L0R[4], BestRef8x8L1R[4];
    Ipp32s Best8x8Type[4];
    //8x16 16x8 data
    H264MotionVector BestMV8x16L0, BestMV8x16L1, /*PredMV8x16L0, PredMV8x16L1,*/ BestMV8x16SL0[2], BestMV8x16SL1[2];
    Ipp32s BestSAD8x16L0, BestSAD8x16L1, BestSAD8x16Bi, BestSAD8x16 = MAX_SAD;
    Ipp32s BestRef8x16L0, BestRef8x16L1, BestRef8x16SL0[2], BestRef8x16SL1[2];
    Ipp32s BestPred8x16[2], Best8x16Type = 0;

    H264MotionVector BestMV16x8L0, BestMV16x8L1, /*PredMV16x8L0, PredMV16x8L1,*/ BestMV16x8SL0[2], BestMV16x8SL1[2];
    Ipp32s BestSAD16x8L0, BestSAD16x8L1, BestSAD16x8Bi, BestSAD16x8 = MAX_SAD;
    Ipp32s BestRef16x8L0, BestRef16x8L1, BestRef16x8SL0[2], BestRef16x8SL1[2];
    Ipp32s BestPred16x8[2], Best16x8Type = 0;
    //PredMV16x8L0.mvx = 0;
    //PredMV16x8L0.mvy = 0;
    //PredMV16x8L1.mvx = 0;
    //PredMV16x8L1.mvy = 0;
#ifdef USE_NV12
    __ALIGN16 PIXTYPE pUSrc[64];
    __ALIGN16 PIXTYPE pVSrc[64];
    PIXTYPE *pSrc;
#endif // USE_NV12

    //16x16 data
    //H264MotionVector BestMV16x16;
    Ipp32s  RDCost16x16Direct = MAX_SAD, RDCost16x16L0 = MAX_SAD, RDCost16x16L1 = MAX_SAD, RDCost16x16Bi = MAX_SAD;
    Ipp32s  RDCost8x8 = MAX_SAD, RDCost8x16 = MAX_SAD, RDCost16x8 = MAX_SAD;
    Ipp32s  BestSAD16x16/*, BestRef16x16*/;
    H264MotionVector BestMV16x16L0 = null_mv;//,PredMV16x16L0=null_mv;
    Ipp32s  BestSAD16x16L0=MAX_SAD, BestRef16x16L0=0;
    H264MotionVector BestMV16x16L1 = null_mv;//, PredMV16x16L1=null_mv;
    Ipp32s  BestSAD16x16L1=MAX_SAD, BestRef16x16L1=0;
    Ipp32s  BestSAD16x16Bi=MAX_SAD;
    Ipp32s  Direct16x16SAD = MAX_SAD, Direct8x8SAD[4] = { MAX_SAD, MAX_SAD, MAX_SAD, MAX_SAD };
    Ipp32s  BestMBType;
    T_RefIdx *refsL0 = cur_mb.RefIdxs[LIST_0]->RefIdxs;
    T_RefIdx *refsL1 = cur_mb.RefIdxs[LIST_1]->RefIdxs;
    H264MotionVector *mvsL0  = cur_mb.MVs[LIST_0]->MotionVectors;
    H264MotionVector *mvsL1  = cur_mb.MVs[LIST_1]->MotionVectors;
    ME_InfType meInfo;
    meInfo.pCurU = NULL;
    meInfo.pCurV = NULL;
    Ipp32s pitchPixels = cur_mb.mbPitchPixels, check_threshold;
    MBTypeValue *sbt = cur_mb.GlobalMacroblockInfo->sbtype;
    Ipp32s  meFlags = core_enc->m_Analyse & ~ANALYSE_B_4x4; //Switch off split 4x4 for B frames for a while
    Ipp32s ModeRDOpt = ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE)) && (core_enc->m_Analyse & ANALYSE_B_RD_OPT);
//    Ipp32s  meFlags = core_enc->m_Analyse & ~ANALYSE_ME_CHROMA; //Switch off chroma ME for B frames for a while
//    Ipp32s  meFlags = core_enc->m_Analyse; //Switch off chroma ME for B frames for a while
//    Ipp32s  meFlags = ME_THRESH_EARLY_EXIT | P_EARLY_EXIT;
    PIXTYPE* pMBCurU = NULL;
    PIXTYPE* pMBCurV = NULL;
    __ALIGN16 PIXTYPE bidir16x8[256], bidir8x16[256], bidir16x16[256], bidir8x8[256];

    Ipp32s  mpredflagL0 = 0, mpredflagL1 = 0;
    H264MotionVector BestPredMV4x4L0[4][4], BestPredMV8x4L0[4][2], BestPredMV4x8L0[4][2];
    H264MotionVector BestPredMV8x8L0[4],  BestPredMV16x8L0[2], BestPredMV8x16L0[2], BestPredMV16x16L0 = {0,};
    H264MotionVector BestPredMV4x4L1[4][4], BestPredMV8x4L1[4][2], BestPredMV4x8L1[4][2];
    H264MotionVector BestPredMV8x8L1[4],  BestPredMV16x8L1[2], BestPredMV8x16L1[2], BestPredMV16x16L1 = {0,};
    Ipp32s mpred4x4L0[4][4], mpred8x4L0[4][2], mpred4x8L0[4][2];
    Ipp32s mpred8x8L0[4],  mpred16x8L0[2], mpred8x16L0[2], mpred16x16L0 = 0;
    Ipp32s mpred4x4L1[4][4], mpred8x4L1[4][2], mpred4x8L1[4][2];
    Ipp32s mpred8x8L1[4],  mpred16x8L1[2], mpred8x16L1[2], mpred16x16L1 = 0;

    H264MotionVector predBidirRefineL0[9], predBidirRefineL1[9];
    Ipp32s bestSADByMBtype[9], chromaContrToSAD[9];

    if ( meFlags & ANALYSE_ME_BIDIR_REFINE )
    {
        memset(bestSADByMBtype,0,9 * sizeof(Ipp32s));
        memset(chromaContrToSAD,0,9 * sizeof(Ipp32s));
    }

    Ipp16s *resPredBuf = cur_mb.m_pResPredDiff;
    Ipp16s *resPredBufC = cur_mb.m_pResPredDiffC;
    Ipp32s resPredFlag;
    Ipp32s RefLater8x8PackFlag = pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo);
    bool check2TransformModes;

    resPredFlag = meInfo.residualPredictionFlag = 0;

    if (curr_slice->resPredFlag > 0)
        resPredFlag = meInfo.residualPredictionFlag = 1;

    check2TransformModes = false;
    if ((core_enc->m_spatial_resolution_change) || (resPredFlag == 0))
    {
        RefLater8x8PackFlag = 0;
        check2TransformModes = true;
    }

#ifdef FRAME_INTERPOLATION
    meInfo.planeSize = core_enc->m_pCurrentFrame->m_PlaneSize;
    Ipp32s planeSize = core_enc->m_pCurrentFrame->m_PlaneSize;
#else //FRAME_INTERPOLATION
#ifndef NO_PADDING
    Ipp32s planeSize = 0;
#endif
#endif //FRAME_INTERPOLATION
    meInfo.xMin = pMBOffset->uMVLimits_L;
    meInfo.xMax = pMBOffset->uMVLimits_R;
    meInfo.yMin = pMBOffset->uMVLimits_U;
    meInfo.yMax = pMBOffset->uMVLimits_D;
    meInfo.block.width = 16;
    meInfo.block.height = 16;
    meInfo.pRDQM = pRDQM;
    meInfo.pCur = cur_mb.mbPtr;
    meInfo.pitchPixels = cur_mb.mbPitchPixels;
    meInfo.bit_depth_luma = core_enc->m_PicParamSet->bit_depth_luma;
    meInfo.rX = core_enc->m_info.me_search_x;
    meInfo.rY = core_enc->m_info.me_search_y;
    meInfo.flags = meFlags;
    meInfo.searchAlgo = core_enc->m_info.mv_search_method + core_enc->m_SubME_Algo * 32;
    check_threshold = meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] : 0;
    BestSAD16x16 = MAX_SAD;
    //BestRef16x16 = 0;
    //BestMV16x16 = null_mv;

    meInfo.pResY = resPredBuf;
    meInfo.pResU = resPredBufC;
    meInfo.pResV = resPredBufC + 8;

#ifdef NO_PADDING
#if PIXBITS == 8
    IppVCInterpolateBlock_8u interpolateInfo, *pInterpolateInfo = &interpolateInfo;
    IppVCInterpolateBlock_8u interpolateInfoChroma, *pInterpolateInfoChroma = &interpolateInfoChroma;
#elif PIXBITS == 16
    IppVCInterpolateBlock_16u interpolateInfo, *pInterpolateInfo = &interpolateInfo;
    IppVCInterpolateBlock_16u interpolateInfoChroma, *pInterpolateInfoChroma = &interpolateInfoChroma;
#endif //PIXBITS
    Ipp32s offsetIfBottomField = (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
    IppiPoint mbPos, block8x8Pos;
    mbPos.x = cur_mb.uMBx << 4;
    mbPos.y = cur_mb.uMBy << 4;
    meInfo.interpolateInfo.sizeFrame.width = core_enc->m_WidthInMBs << 4;
    meInfo.interpolateInfo.sizeFrame.height = core_enc->m_HeightInMBs << 4;
    meInfo.interpolateInfo.srcStep = cur_mb.mbPitchPixels;
    meInfo.interpolateInfo.dstStep = 16;

    if (core_enc->m_PicParamSet->chroma_format_idc == 1) //420
    {
        meInfo.interpolateInfoChroma.sizeFrame.width = meInfo.interpolateInfo.sizeFrame.width >> 1;
        meInfo.interpolateInfoChroma.sizeFrame.height = meInfo.interpolateInfo.sizeFrame.height >> 1;
    }
    else if (core_enc->m_PicParamSet->chroma_format_idc == 2) //422
    {
        meInfo.interpolateInfoChroma.sizeFrame.width = meInfo.interpolateInfo.sizeFrame.width >> 1;
        meInfo.interpolateInfoChroma.sizeFrame.height = meInfo.interpolateInfo.sizeFrame.height;
    }
    meInfo.interpolateInfoChroma.srcStep = cur_mb.mbPitchPixels;
    meInfo.interpolateInfoChroma.dstStep = 16;

    pInterpolateInfo->sizeFrame.width = core_enc->m_WidthInMBs << 4;
    pInterpolateInfo->sizeFrame.height = core_enc->m_HeightInMBs << 4;
    pInterpolateInfo->srcStep = cur_mb.mbPitchPixels;
    pInterpolateInfo->dstStep = 16;

    if (core_enc->m_PicParamSet->chroma_format_idc == 1) //420
    {
        pInterpolateInfoChroma->sizeFrame.width = pInterpolateInfo->sizeFrame.width >> 1;
        pInterpolateInfoChroma->sizeFrame.height = pInterpolateInfo->sizeFrame.height >> 1;
    }
    else if (core_enc->m_PicParamSet->chroma_format_idc == 2) //422
    {
        pInterpolateInfoChroma->sizeFrame.width = pInterpolateInfo->sizeFrame.width >> 1;
        pInterpolateInfoChroma->sizeFrame.height = pInterpolateInfo->sizeFrame.height;
    }
    pInterpolateInfoChroma->srcStep = cur_mb.mbPitchPixels;
    pInterpolateInfoChroma->dstStep = 16;
#endif // NO_PADDING

    if( core_enc->m_PicParamSet->chroma_format_idc == 0 ) //mask CHROMA_ME flag
        meFlags &= ~ANALYSE_ME_CHROMA;
    if( meFlags & ANALYSE_ME_CHROMA ){
        meInfo.pCurU = pMBCurU = core_enc->m_pCurrentFrame->m_pUPlane + uChromaOffset;
        meInfo.pCurV = pMBCurV = core_enc->m_pCurrentFrame->m_pVPlane + uChromaOffset;
        meInfo.chroma_format_idc = core_enc->m_PicParamSet->chroma_format_idc;
        meInfo.bit_depth_chroma = core_enc->m_SeqParamSet->bit_depth_chroma;
    }

#ifdef USE_NV12
    if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
    {
        meInfo.pCurV = pMBCurV = core_enc->m_pCurrentFrame->m_pUPlane + uChromaOffset + 8;
        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uChromaOffset;
        for(i=0;i<8;i++){
            Ipp32s s = 8*i;
            pUSrc[s+0] = pSrc[0];
            pVSrc[s+0] = pSrc[1];
            pUSrc[s+1] = pSrc[2];
            pVSrc[s+1] = pSrc[3];
            pUSrc[s+2] = pSrc[4];
            pVSrc[s+2] = pSrc[5];
            pUSrc[s+3] = pSrc[6];
            pVSrc[s+3] = pSrc[7];
            pUSrc[s+4] = pSrc[8];
            pVSrc[s+4] = pSrc[9];
            pUSrc[s+5] = pSrc[10];
            pVSrc[s+5] = pSrc[11];
            pUSrc[s+6] = pSrc[12];
            pVSrc[s+6] = pSrc[13];
            pUSrc[s+7] = pSrc[14];
            pVSrc[s+7] = pSrc[15];

            pSrc[0] = pUSrc[s+0];
            pSrc[1] = pUSrc[s+1];
            pSrc[2] = pUSrc[s+2];
            pSrc[3] = pUSrc[s+3];
            pSrc[4] = pUSrc[s+4];
            pSrc[5] = pUSrc[s+5];
            pSrc[6] = pUSrc[s+6];
            pSrc[7] = pUSrc[s+7];
            pSrc[8] = pVSrc[s+0];
            pSrc[9] = pVSrc[s+1];
            pSrc[10] = pVSrc[s+2];
            pSrc[11] = pVSrc[s+3];
            pSrc[12] = pVSrc[s+4];
            pSrc[13] = pVSrc[s+5];
            pSrc[14] = pVSrc[s+6];
            pSrc[15] = pVSrc[s+7];

            pSrc += meInfo.pitchPixels;
        }
    }
#endif // USE_NV12

//First check direct prediction 16x16
    H264MacroblockMVs      mvs_direct[2];
    H264MacroblockRefIdxs  ref_idxs_direct[2];
    Ipp32s sbtype_direct[4];
    bool is_could_use_direct;

    H264MacroblockMVs      mvs_direct8x8[2];
    H264MacroblockRefIdxs  ref_idxs_direct8x8[2];
    bool skipAvail;

    curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_DIRECT;
    if( !transform_bypass && (meFlags & ANALYSE_ME_AUTO_DIRECT)){ //make stat to prefer direct type that makes more skips
        if( core_enc->m_SliceHeader.direct_spatial_mv_pred_flag ) //Reverse to actual meaning
            is_could_use_direct = H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectTemporalMV)(state, curr_slice, ref_idxs_direct, mvs_direct );
        else
            is_could_use_direct = H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectSpatialMV)(state,  curr_slice, ref_idxs_direct, mvs_direct );
        if( is_could_use_direct ){
            //Copy refs to use in MC
            *cur_mb.RefIdxs[LIST_0] = ref_idxs_direct[LIST_0];
            *cur_mb.RefIdxs[LIST_1] = ref_idxs_direct[LIST_1];

            skipAvail = H264ENC_MAKE_NAME(H264CoreEncoder_CheckSkipB)(state, curr_slice, ref_idxs_direct, mvs_direct, meInfo.residualPredictionFlag, resPredBuf, resPredBufC);

            core_enc->m_DirectTypeStat[(~core_enc->m_SliceHeader.direct_spatial_mv_pred_flag)&1] += skipAvail ? 1 : 0; // not thread-safe!
        }
    }

    if (!RefLater8x8PackFlag || core_enc->m_SeqParamSet->direct_8x8_inference_flag)
    {
        if( core_enc->m_SliceHeader.direct_spatial_mv_pred_flag )
            is_could_use_direct = H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectSpatialMV)(state, curr_slice, ref_idxs_direct, mvs_direct );
        else
            is_could_use_direct = H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectTemporalMV)(state, curr_slice, ref_idxs_direct, mvs_direct );

        if( is_could_use_direct ){
            //Copy refs to use in MC
            *cur_mb.RefIdxs[LIST_0] = ref_idxs_direct[LIST_0];
            *cur_mb.RefIdxs[LIST_1] = ref_idxs_direct[LIST_1];
            for( i = 0; i<4; i++ )  sbtype_direct[i] = cur_mb.GlobalMacroblockInfo->sbtype[i];

            //Check for skip
            if (!transform_bypass) {
                skipAvail = H264ENC_MAKE_NAME(H264CoreEncoder_CheckSkipB)(state, curr_slice, ref_idxs_direct, mvs_direct, meInfo.residualPredictionFlag, resPredBuf, resPredBufC);

                if (skipAvail && (curr_slice->resPredFlag != 1 /* adaptive mode */)) {
                    *cur_mb.MVs[LIST_0] = mvs_direct[LIST_0];
                    *cur_mb.MVs[LIST_1] = mvs_direct[LIST_1];
                    curr_slice->m_cur_mb.LocalMacroblockInfo->cbp_luma = 0;
                    curr_slice->m_cur_mb.LocalMacroblockInfo->cbp_chroma = 0;
                    curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;
                    if(meFlags & ANALYSE_ME_AUTO_DIRECT) core_enc->m_DirectTypeStat[core_enc->m_SliceHeader.direct_spatial_mv_pred_flag]++; // not thread-safe!
#ifdef USE_NV12
                    if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
                    {
                        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uChromaOffset;
                        for(i=0;i<8;i++){
                            Ipp32s s = 8*i;

                            pSrc[0] = pUSrc[s+0];
                            pSrc[1] = pVSrc[s+0];
                            pSrc[2] = pUSrc[s+1];
                            pSrc[3] = pVSrc[s+1];
                            pSrc[4] = pUSrc[s+2];
                            pSrc[5] = pVSrc[s+2];
                            pSrc[6] = pUSrc[s+3];
                            pSrc[7] = pVSrc[s+3];
                            pSrc[8] = pUSrc[s+4];
                            pSrc[9] = pVSrc[s+4];
                            pSrc[10] = pUSrc[s+5];
                            pSrc[11] = pVSrc[s+5];
                            pSrc[12] = pUSrc[s+6];
                            pSrc[13] = pVSrc[s+6];
                            pSrc[14] = pUSrc[s+7];
                            pSrc[15] = pVSrc[s+7];

                            pSrc += meInfo.pitchPixels;
                        }
                    }
#endif // USE_NV12
                    return 0;
                }
            }

            if (!(meFlags & ANALYSE_B_8x8) || !core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag) {
                Direct16x16SAD = BITS_COST(1, pRDQM);
                Ipp32s chroma_block_size = BS_8x8;
                if (core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                    chroma_block_size = BS_8x16;
                if (meFlags & ANALYSE_SAD) {
                    if (!resPredFlag)
                    {
                        Direct16x16SAD += SAD16x16(cur_mb.mbPtr, pitchPixels, curr_slice->m_pPred4DirectB, 16);
                        if( meFlags & ANALYSE_ME_CHROMA ){
                            Direct16x16SAD += H264ENC_MAKE_NAME(SAD)(meInfo.pCurU, pitchPixels, curr_slice->m_pMBEncodeBuffer, 16, chroma_block_size);
                            Direct16x16SAD += H264ENC_MAKE_NAME(SAD)(meInfo.pCurV, pitchPixels, curr_slice->m_pMBEncodeBuffer+8, 16, chroma_block_size);
                        }
                    }
                    else
                    {
                        Direct16x16SAD += SAD16x16_16_ResPred(resPredBuf, curr_slice->m_pPred4DirectB);
                        if( meFlags & ANALYSE_ME_CHROMA ){
                            Direct16x16SAD += SAD8x8_16_ResPred(resPredBufC, curr_slice->m_pMBEncodeBuffer);
                            Direct16x16SAD += SAD8x8_16_ResPred(resPredBufC + 8, curr_slice->m_pMBEncodeBuffer + 8);
                        }
                    }
                } else {
                    if (!resPredFlag)
                    {
                        Direct16x16SAD += SATD16x16(cur_mb.mbPtr, pitchPixels, curr_slice->m_pPred4DirectB, 16);
                        if( meFlags & ANALYSE_ME_CHROMA ){
                            Direct16x16SAD += H264ENC_MAKE_NAME(SATD)(meInfo.pCurU, pitchPixels, curr_slice->m_pMBEncodeBuffer, 16, chroma_block_size);
                            Direct16x16SAD += H264ENC_MAKE_NAME(SATD)(meInfo.pCurV, pitchPixels, curr_slice->m_pMBEncodeBuffer+8, 16, chroma_block_size);
                        }
                    }
                    else
                    {
                        Direct16x16SAD += SATD16x16_16_ResPred(resPredBuf, curr_slice->m_pPred4DirectB);
                        if (meFlags & ANALYSE_ME_CHROMA) {
                            Direct16x16SAD += SATD8x8_16_ResPred(resPredBufC, curr_slice->m_pMBEncodeBuffer);
                            Direct16x16SAD += SATD8x8_16_ResPred(resPredBufC + 8, curr_slice->m_pMBEncodeBuffer + 8);
                        }
                    }
                }
            } else {
                Direct8x8SAD[0] = Direct8x8SAD[1] = Direct8x8SAD[2] = Direct8x8SAD[3] = BITS_COST(1, pRDQM);
                if (meFlags & ANALYSE_SAD) {
                    Direct8x8SAD[0] += SAD8x8(cur_mb.mbPtr, pitchPixels, curr_slice->m_pPred4DirectB, 16);
                    Direct8x8SAD[1] += SAD8x8(cur_mb.mbPtr + 8, pitchPixels, curr_slice->m_pPred4DirectB + 8, 16);
                    Direct8x8SAD[2] += SAD8x8(cur_mb.mbPtr + 8 * pitchPixels, pitchPixels, curr_slice->m_pPred4DirectB + 8 * 16, 16);
                    Direct8x8SAD[3] += SAD8x8(cur_mb.mbPtr + 8 * pitchPixels + 8, pitchPixels, curr_slice->m_pPred4DirectB + 8 * 16 + 8, 16);
                    if (meFlags & ANALYSE_ME_CHROMA) {
                        Ipp32s chroma_block_size = BS_4x4;
                        if (core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                            chroma_block_size = BS_4x8;
                        //U
                        Direct8x8SAD[0] += H264ENC_MAKE_NAME(SAD)( meInfo.pCurU, pitchPixels, curr_slice->m_pMBEncodeBuffer, 16, chroma_block_size);
                        Direct8x8SAD[1] += H264ENC_MAKE_NAME(SAD)( meInfo.pCurU + 4, pitchPixels, curr_slice->m_pMBEncodeBuffer + 4, 16, chroma_block_size);
                        Direct8x8SAD[2] += H264ENC_MAKE_NAME(SAD)( meInfo.pCurU + 4 * pitchPixels, pitchPixels, curr_slice->m_pMBEncodeBuffer + 4 * 16, 16, chroma_block_size);
                        Direct8x8SAD[3] += H264ENC_MAKE_NAME(SAD)( meInfo.pCurU + 4 * pitchPixels + 4, pitchPixels, curr_slice->m_pMBEncodeBuffer + 4 * 16 + 4, 16, chroma_block_size);
                        //V
                        Direct8x8SAD[0] += H264ENC_MAKE_NAME(SAD)( meInfo.pCurV, pitchPixels, curr_slice->m_pMBEncodeBuffer+8, 16, chroma_block_size);
                        Direct8x8SAD[1] += H264ENC_MAKE_NAME(SAD)( meInfo.pCurV + 4, pitchPixels, curr_slice->m_pMBEncodeBuffer +8 + 4, 16, chroma_block_size);
                        Direct8x8SAD[2] += H264ENC_MAKE_NAME(SAD)( meInfo.pCurV + 4 * pitchPixels, pitchPixels, curr_slice->m_pMBEncodeBuffer +8 + 4 * 16, 16, chroma_block_size);
                        Direct8x8SAD[3] += H264ENC_MAKE_NAME(SAD)( meInfo.pCurV + 4 * pitchPixels + 4, pitchPixels, curr_slice->m_pMBEncodeBuffer +8 + 4 * 16 + 4, 16, chroma_block_size);
                    }
                } else {
                    Direct8x8SAD[0] += SATD8x8(cur_mb.mbPtr, pitchPixels, curr_slice->m_pPred4DirectB, 16);
                    Direct8x8SAD[1] += SATD8x8(cur_mb.mbPtr + 8, pitchPixels, curr_slice->m_pPred4DirectB + 8, 16);
                    Direct8x8SAD[2] += SATD8x8(cur_mb.mbPtr + 8 * pitchPixels, pitchPixels, curr_slice->m_pPred4DirectB + 8 * 16, 16);
                    Direct8x8SAD[3] += SATD8x8(cur_mb.mbPtr + 8 * pitchPixels + 8, pitchPixels, curr_slice->m_pPred4DirectB + 8 * 16 + 8, 16);
                    if( meFlags & ANALYSE_ME_CHROMA ){
                        Ipp32s chroma_block_size = BS_4x4;
                        if (core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                            chroma_block_size = BS_4x8;
                        //U
                        Direct8x8SAD[0] += H264ENC_MAKE_NAME(SATD)( meInfo.pCurU, pitchPixels, curr_slice->m_pMBEncodeBuffer, 16, chroma_block_size);
                        Direct8x8SAD[1] += H264ENC_MAKE_NAME(SATD)( meInfo.pCurU + 4, pitchPixels, curr_slice->m_pMBEncodeBuffer + 4, 16, chroma_block_size);
                        Direct8x8SAD[2] += H264ENC_MAKE_NAME(SATD)( meInfo.pCurU + 4 * pitchPixels, pitchPixels, curr_slice->m_pMBEncodeBuffer + 4 * 16, 16, chroma_block_size);
                        Direct8x8SAD[3] += H264ENC_MAKE_NAME(SATD)( meInfo.pCurU + 4 * pitchPixels + 4, pitchPixels, curr_slice->m_pMBEncodeBuffer + 4 * 16 + 4, 16, chroma_block_size);
                        //V
                        Direct8x8SAD[0] += H264ENC_MAKE_NAME(SATD)( meInfo.pCurV, pitchPixels, curr_slice->m_pMBEncodeBuffer+8, 16, chroma_block_size);
                        Direct8x8SAD[1] += H264ENC_MAKE_NAME(SATD)( meInfo.pCurV + 4, pitchPixels, curr_slice->m_pMBEncodeBuffer +8 + 4, 16, chroma_block_size);
                        Direct8x8SAD[2] += H264ENC_MAKE_NAME(SATD)( meInfo.pCurV + 4 * pitchPixels, pitchPixels, curr_slice->m_pMBEncodeBuffer +8 + 4 * 16, 16, chroma_block_size);
                        Direct8x8SAD[3] += H264ENC_MAKE_NAME(SATD)( meInfo.pCurV + 4 * pitchPixels + 4, pitchPixels, curr_slice->m_pMBEncodeBuffer +8 + 4 * 16 + 4, 16, chroma_block_size);
                    }
                }
                Direct16x16SAD = Direct8x8SAD[0]+Direct8x8SAD[1]+Direct8x8SAD[2]+Direct8x8SAD[3]-3*BITS_COST(1, pRDQM);
            }
            if (ModeRDOpt) {
                // use direct mvs to calculate direct RD cost
                *cur_mb.MVs[LIST_0] = mvs_direct[LIST_0];
                *cur_mb.MVs[LIST_1] = mvs_direct[LIST_1];
#ifdef USE_NV12
                RDCost16x16Direct = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
                RDCost16x16Direct = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 0 );
#endif // USE_NV12
            }
        }
    }

    numRef = curr_slice->m_NumRefsInL0List;
    for (i = 0; i < numRef; i ++) refCostL0[i] = RefConstraint(i, numRef, pRDQM);
    numRef = curr_slice->m_NumRefsInL1List;
    for (i = 0; i < numRef; i ++) refCostL1[i] = RefConstraint(i, numRef, pRDQM);
    //Search L0, for B_L0_L0
    curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_FORWARD;
    ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_RefPicList;
    numRef = curr_slice->m_NumRefsInL0List;

#ifdef NO_PADDING
    meInfo.interpolateInfo.pointBlockPos.x = mbPos.x;
    meInfo.interpolateInfo.pointBlockPos.y = mbPos.y;
#endif // NO_PADDING

    for (ref_idx = 0; ref_idx < numRef; ref_idx ++) {
        meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + uOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]];
#ifdef NO_PADDING
        meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
        if(  meFlags & ANALYSE_ME_CHROMA ){
             meInfo.chroma_mvy_offset = 0;
             if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                if (!curr_slice->m_is_cur_mb_bottom_field && pFields0[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                else if (curr_slice->m_is_cur_mb_bottom_field && !pFields0[ref_idx]) meInfo.chroma_mvy_offset += 2;
             }
            meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]];
            meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]];
#ifdef NO_PADDING
            meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
            meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
        }
        refsL0[0] = ref_idx;
        H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, 0, LIST_0, 4, 4, &(meInfo));
        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CandList16x16)(state, curr_slice, LIST_0, &meInfo, ref_idx);
        if(  ref_idxs_direct[LIST_0].RefIdxs[0] == ref_idx )
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, mvs_direct[LIST_0].MotionVectors[0]);
        H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
        if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCostL0[ref_idx] > meInfo.threshold)
            H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
        if (meInfo.bestSAD + refCostL0[ref_idx] < BestSAD16x16L0) {
            BestSAD16x16L0 = meInfo.bestSAD + refCostL0[ref_idx];
            BestMV16x16L0 = meInfo.bestMV;
            BestRef16x16L0 = ref_idx;
            //PredMV16x16L0 = meInfo.predictedMV;
            BestPredMV16x16L0 = meInfo.bestPredMV;//meInfo.predictedMV;
            mpred16x16L0 = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
            if (BestSAD16x16L0 <= meInfo.threshold)
                break;
        } else //if (!(meFlags & ANALYSE_ME_ALL_REF) && !((meFlags & ANALYSE_B_8x8)))
            // TODO
            //if (meInfo.bestSAD + refCost[ref_idx] > (BestSAD16x16 * 17 >> 4))
            if (!(meFlags & ANALYSE_ME_ALL_REF))
                if ((core_enc->m_Analyse & ANALYSE_ME_FAST_MULTIREF) && (ref_idx > (numRef >> 1)))
                    break;
    }
    if (ModeRDOpt) {
        for (i = 0; i < 16; i ++) {
             mvsL0[i] = BestMV16x16L0;
             mvsL1[i] = null_mv;
             refsL0[i] = (T_RefIdx)BestRef16x16L0;
             refsL1[i] = (T_RefIdx)-1;
        }
#ifdef USE_NV12
        RDCost16x16L0 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
        RDCost16x16L0 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 0 );
#endif // USE_NV12
    }

    //Search L1, for B_L1_L1
    curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BACKWARD;
    ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, is_cur_mb_field, uMB & 1)->m_RefPicList;
    numRef = curr_slice->m_NumRefsInL1List;
    for (ref_idx = 0; ref_idx < numRef; ref_idx ++) {
        meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + uOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]];
#ifdef NO_PADDING
        meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
        if(  meFlags & ANALYSE_ME_CHROMA ){
             meInfo.chroma_mvy_offset = 0;
             if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                if (!curr_slice->m_is_cur_mb_bottom_field && pFields1[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                else if (curr_slice->m_is_cur_mb_bottom_field && !pFields1[ref_idx]) meInfo.chroma_mvy_offset += 2;
             }
            meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]];
            meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]];
#ifdef NO_PADDING
            meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
            meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
        }
        refsL1[0] = ref_idx;
        H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, 0, LIST_1, 4, 4, &(meInfo));
        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CandList16x16)(state, curr_slice, LIST_1, &meInfo, ref_idx);
        if (ref_idxs_direct[LIST_1].RefIdxs[0] == ref_idx)
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo,  mvs_direct[LIST_1].MotionVectors[0]);
        H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
        if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCostL1[ref_idx] > meInfo.threshold)
            H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
        if (meInfo.bestSAD + refCostL1[ref_idx] < BestSAD16x16L1) {
            BestSAD16x16L1 = meInfo.bestSAD + refCostL1[ref_idx];
            BestMV16x16L1 = meInfo.bestMV;
            BestRef16x16L1 = ref_idx;
            //PredMV16x16L1 = meInfo.predictedMV;
            BestPredMV16x16L1 = meInfo.bestPredMV;//meInfo.predictedMV;
            mpred16x16L1 = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
            if (BestSAD16x16L1 <= meInfo.threshold)
                break;
        } else //if (!(meFlags & ANALYSE_ME_ALL_REF) && !((meFlags & ANALYSE_B_8x8)))
            // TODO
            //if (meInfo.bestSAD + refCost[ref_idx] > (BestSAD16x16 * 17 >> 4))
            if (!(meFlags & ANALYSE_ME_ALL_REF))
                if ((core_enc->m_Analyse & ANALYSE_ME_FAST_MULTIREF) && (ref_idx > (numRef >> 1)))
                    break;
    }
    if (ModeRDOpt) {
        for (i = 0; i < 16; i ++) {
          mvsL0[i] = null_mv;
          mvsL1[i] = BestMV16x16L1;
          refsL0[i] = (T_RefIdx)-1;
          refsL1[i] = (T_RefIdx)BestRef16x16L1;
        }
#ifdef USE_NV12
        RDCost16x16L1 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
        RDCost16x16L1 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 0 );
#endif // USE_NV12
    }

    //f if (!(meFlags & ANALYSE_ME_ALL_REF)) {
    //    numRefL0 = BestRef16x16L0 + 1;
    //    numRefL1 = BestRef16x16L1 + 1;
    //}
    // Try 16x16 BiPred //Note: original code is trying predictors also
    ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_RefPicList;
    PIXTYPE *const pRef0 = ppRefPicList[BestRef16x16L0]->m_pYPlane + uOffset + curr_slice->m_InitialOffset[pFields0[BestRef16x16L0]];
    ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, is_cur_mb_field, uMB & 1)->m_RefPicList;
    PIXTYPE *const pRef1 = ppRefPicList[BestRef16x16L1]->m_pYPlane + uOffset + curr_slice->m_InitialOffset[pFields1[BestRef16x16L1]];
    Ipp32s w1 = curr_slice->DistScaleFactor[BestRef16x16L0][BestRef16x16L1]>>2;
    Ipp32s w0 = 64-w1;

    PIXTYPE *const pTmpBufL0 = curr_slice->m_pMBEncodeBuffer;
    PIXTYPE *const pTmpBufL1 = curr_slice->m_pMBEncodeBuffer+256;
    PIXTYPE *const pTmpChromaPred = curr_slice->m_pTempChromaPred;

#ifdef NO_PADDING
    pInterpolateInfo->pSrc[0] = pRef0 - uOffset + offsetIfBottomField;
    pInterpolateInfo->pDst[0] = pTmpBufL0;
    pInterpolateInfo->sizeBlock = meInfo.block;
    pInterpolateInfo->pointVector.x = BestMV16x16L0.mvx;
    pInterpolateInfo->pointVector.y = BestMV16x16L0.mvy;
    pInterpolateInfo->pointBlockPos = mbPos;
    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
    H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( pRef0, pitchPixels, BestMV16x16L0.mvx >> SUB_PEL_SHIFT, BestMV16x16L0.mvy >> SUB_PEL_SHIFT), pitchPixels,
        pTmpBufL0, 16, BestMV16x16L0.mvx&3, BestMV16x16L0.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
#ifdef NO_PADDING
    pInterpolateInfo->pSrc[0] = pRef1 - uOffset + offsetIfBottomField;
    pInterpolateInfo->pDst[0] = pTmpBufL1;
    pInterpolateInfo->sizeBlock = meInfo.block;
    pInterpolateInfo->pointVector.x = BestMV16x16L1.mvx;
    pInterpolateInfo->pointVector.y = BestMV16x16L1.mvy;
    pInterpolateInfo->pointBlockPos = mbPos;
    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
    H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( pRef1, pitchPixels, BestMV16x16L1.mvx >> SUB_PEL_SHIFT, BestMV16x16L1.mvy >> SUB_PEL_SHIFT ), pitchPixels,
        pTmpBufL1, 16, BestMV16x16L1.mvx&3, BestMV16x16L1.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING

    H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)( bidir16x16, pTmpBufL0, pTmpBufL1, 16, core_enc->use_implicit_weighted_bipred ? 2: 1,w1, w0, meInfo.block);

    if (!resPredFlag)
    {
        if (core_enc->m_Analyse & ANALYSE_SAD)
            BestSAD16x16Bi = SAD16x16(cur_mb.mbPtr, pitchPixels, bidir16x16, 16);
        else
            BestSAD16x16Bi = SATD16x16(cur_mb.mbPtr, pitchPixels, bidir16x16, 16);

    }
    else
    {
        if (core_enc->m_Analyse & ANALYSE_SAD)
            BestSAD16x16Bi = SAD16x16_16_ResPred(resPredBuf, bidir16x16);
        else
            BestSAD16x16Bi = SATD16x16_16_ResPred(resPredBuf, bidir16x16);
    }

    BestSAD16x16Bi += MVConstraint( BestMV16x16L0.mvx - BestPredMV16x16L0.mvx, BestMV16x16L0.mvy - BestPredMV16x16L0.mvy, pRDQM);
    BestSAD16x16Bi += MVConstraint( BestMV16x16L1.mvx - BestPredMV16x16L1.mvx, BestMV16x16L1.mvy - BestPredMV16x16L1.mvy, pRDQM)+
        refCostL0[BestRef16x16L0] +
        refCostL1[BestRef16x16L1];

    if(  meFlags & ANALYSE_ME_CHROMA ){
         Ipp32s iXType, iYType;
         Ipp16s chroma_mvy_offset;
         IppiSize chroma_size = {0, 0};
         H264MotionVector chroma_vec;

         ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_RefPicList;
         PIXTYPE *pRefU = ppRefPicList[BestRef16x16L0]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[BestRef16x16L0]];
         PIXTYPE *pRefV = ppRefPicList[BestRef16x16L0]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[BestRef16x16L0]];
         chroma_mvy_offset = 0;
         if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
            if (!curr_slice->m_is_cur_mb_bottom_field && pFields0[BestRef16x16L0]) chroma_mvy_offset += - 2;
            else if (curr_slice->m_is_cur_mb_bottom_field && !pFields0[BestRef16x16L0]) chroma_mvy_offset += 2;
         }
         chroma_vec.mvx = BestMV16x16L0.mvx;
         chroma_vec.mvy = BestMV16x16L0.mvy + chroma_mvy_offset;

         if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
             chroma_size =  size8x8;
         else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
             chroma_size =  size8x16;

         Ipp32s offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);

#ifdef NO_PADDING
         pInterpolateInfoChroma->pSrc[0] = ppRefPicList[BestRef16x16L0]->m_pUPlane + curr_slice->m_InitialOffset[pFields0[BestRef16x16L0]] + offsetIfBottomField;
         pInterpolateInfoChroma->pSrc[1] = ppRefPicList[BestRef16x16L0]->m_pVPlane + curr_slice->m_InitialOffset[pFields0[BestRef16x16L0]] + offsetIfBottomField;
         pInterpolateInfoChroma->pDst[0] = pTmpBufL0;
         pInterpolateInfoChroma->pDst[1] = pTmpBufL0 + 8;
         if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
         {
             pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
             pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
             pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
             pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
         }
         else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
         {
             pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
             pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
             pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
             pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
         }
         pInterpolateInfoChroma->sizeBlock = chroma_size;
         H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
#ifdef USE_NV12
         H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefU+offset, pitchPixels, pTmpBufL0, pTmpBufL0+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#else // USE_NV12
         H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefU+offset, pitchPixels, pTmpBufL0, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
         H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefV+offset, pitchPixels, pTmpBufL0+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING

         ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, is_cur_mb_field, uMB & 1)->m_RefPicList;
         pRefU = ppRefPicList[BestRef16x16L1]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[BestRef16x16L1]];
         pRefV = ppRefPicList[BestRef16x16L1]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[BestRef16x16L1]];
         chroma_mvy_offset = 0;
         if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
            if (!curr_slice->m_is_cur_mb_bottom_field && pFields1[BestRef16x16L1]) chroma_mvy_offset += - 2;
            else if (curr_slice->m_is_cur_mb_bottom_field && !pFields1[BestRef16x16L1]) chroma_mvy_offset += 2;
         }

         chroma_vec.mvx = BestMV16x16L1.mvx;
         chroma_vec.mvy = BestMV16x16L1.mvy + chroma_mvy_offset;

         offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);

#ifdef NO_PADDING
         pInterpolateInfoChroma->pSrc[0] = ppRefPicList[BestRef16x16L1]->m_pUPlane + curr_slice->m_InitialOffset[pFields1[BestRef16x16L1]] + offsetIfBottomField;
         pInterpolateInfoChroma->pSrc[1] = ppRefPicList[BestRef16x16L1]->m_pVPlane + curr_slice->m_InitialOffset[pFields1[BestRef16x16L1]] + offsetIfBottomField;
         pInterpolateInfoChroma->pDst[0] = pTmpBufL1;
         pInterpolateInfoChroma->pDst[1] = pTmpBufL1 + 8;
         if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
         {
             pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
             pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
             pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
             pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
         }
         else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
         {
             pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
             pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
             pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
             pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
         }
         pInterpolateInfoChroma->sizeBlock = chroma_size;
         H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
#ifdef USE_NV12
         H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefU+offset, pitchPixels, pTmpBufL1, pTmpBufL1+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#else // USE_NV12
         H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefU+offset, pitchPixels, pTmpBufL1, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
         H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(pRefV+offset, pitchPixels, pTmpBufL1+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
         //BiDir
         H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(pTmpBufL0, pTmpBufL1, pTmpChromaPred, chroma_size.width, chroma_size.height, 16);
         H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(pTmpBufL0+8, pTmpBufL1+8, pTmpChromaPred+8, chroma_size.width, chroma_size.height, 16);
         Ipp32s chroma_block_size = chroma_size.width + (chroma_size.height >> 2);
//       if ( meFlags & ANALYSE_ME_BIDIR_REFINE )
         {
           if (resPredFlag) {
             if (core_enc->m_Analyse & ANALYSE_SAD) {
                 chromaContrToSAD[0] = SAD8x8_16_ResPred(resPredBufC, pTmpChromaPred);
                 chromaContrToSAD[0] += SAD8x8_16_ResPred(resPredBufC + 8, pTmpChromaPred + 8);
             } else {
                 chromaContrToSAD[0] = SATD8x8_16_ResPred(resPredBufC, pTmpChromaPred);
                 chromaContrToSAD[0] += SATD8x8_16_ResPred(resPredBufC + 8, pTmpChromaPred + 8);
             }
           } else {
             if (core_enc->m_Analyse & ANALYSE_SAD) {
                 chromaContrToSAD[0] = H264ENC_MAKE_NAME(SAD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                 chromaContrToSAD[0] += H264ENC_MAKE_NAME(SAD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
             } else {
                 chromaContrToSAD[0] = H264ENC_MAKE_NAME(SATD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                 chromaContrToSAD[0] += H264ENC_MAKE_NAME(SATD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
             }
           }
           BestSAD16x16Bi += chromaContrToSAD[0];
         }
/*
         else
         {
           if (resPredFlag) {
             if (core_enc->m_Analyse & ANALYSE_SAD) {
                 BestSAD16x16Bi += SAD8x8_16_ResPred(resPredBufC, pTmpChromaPred);
                 BestSAD16x16Bi += SAD8x8_16_ResPred(resPredBufC + 8, pTmpChromaPred + 8);
             } else {
                 BestSAD16x16Bi += SATD8x8_16_ResPred(resPredBufC, pTmpChromaPred);
                 BestSAD16x16Bi += SATD8x8_16_ResPred(resPredBufC + 8, pTmpChromaPred + 8);
             }
           } else {
             if (core_enc->m_Analyse & ANALYSE_SAD) {
                 BestSAD16x16Bi += H264ENC_MAKE_NAME(SAD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                 BestSAD16x16Bi += H264ENC_MAKE_NAME(SAD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
             }else{
                 BestSAD16x16Bi += H264ENC_MAKE_NAME(SATD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                 BestSAD16x16Bi += H264ENC_MAKE_NAME(SATD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
             }
           }
         }
*/
    }

    if ( meFlags & ANALYSE_ME_BIDIR_REFINE )
    {
        bestSADByMBtype[0] = BestSAD16x16Bi;// - chromaContrToSAD[0];
        predBidirRefineL0[0] = BestPredMV16x16L0;
        predBidirRefineL1[0] = BestPredMV16x16L1;
    }

    if (ModeRDOpt) {
        //Set MVs and RefIds
        curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BIDIR;
        for (i = 0; i < 16; i ++) {
            mvsL0[i] = BestMV16x16L0;
            mvsL1[i] = BestMV16x16L1;
            refsL0[i] = (T_RefIdx)BestRef16x16L0;
            refsL1[i] = (T_RefIdx)BestRef16x16L1;
        }
        MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir16x16, 256*sizeof(PIXTYPE)); //copy buffer for mc
#ifdef USE_NV12
        RDCost16x16Bi = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
        RDCost16x16Bi = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 0 );
#endif // USE_NV12
    }
    BestSAD16x16Bi += BITS_COST(5, pRDQM);
    BestSAD16x16L0 += BITS_COST(3, pRDQM);
    BestSAD16x16L1 += BITS_COST(3, pRDQM);

    //Choose best MB type
    BestSAD16x16 = Direct16x16SAD;
    BestMBType = MBTYPE_DIRECT;
    if( BestSAD16x16L0 < BestSAD16x16 ){
        BestSAD16x16 = BestSAD16x16L0;
        BestMBType = MBTYPE_FORWARD;
    }

    if( BestSAD16x16L1 < BestSAD16x16 ){
        BestSAD16x16 = BestSAD16x16L1;
        BestMBType = MBTYPE_BACKWARD;
    }

    if( BestSAD16x16Bi < BestSAD16x16 ){
        BestSAD16x16 = BestSAD16x16Bi;
        BestMBType = MBTYPE_BIDIR;
    }

    bool doSplit = (meFlags & ANALYSE_B_8x8) && (BestSAD16x16 > check_threshold);
    Ipp32s BestInterSAD = MAX_SAD, intra_cost_chroma = 0, intra_cost_b = 0;
    Ipp32u BestIntraSAD16x16 = MAX_SAD, BestIntraSAD8x8 = MAX_SAD, BestIntraSAD4x4 = MAX_SAD;
    H264MacroblockLocalInfo intraLocalMBinfo = {0,};
    MB_Type BestIntraMBType = MBTYPE_INTRA;
    Ipp32s BestIntraSAD = MAX_SAD;
    if ((core_enc->m_Analyse & ANALYSE_INTRA_IN_ME) && (!resPredFlag)) {
        if (((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE)) && !ModeRDOpt) {
            cur_mb.GlobalMacroblockInfo->mbtype = BestMBType;
            for (i = 0; i < 4; i++)
                cur_mb.GlobalMacroblockInfo->sbtype[i] = (MBTypeValue)NUMBER_OF_MBTYPES;
            switch (BestMBType) {
                case MBTYPE_DIRECT:
                    *cur_mb.RefIdxs[LIST_0] = ref_idxs_direct[LIST_0];
                    *cur_mb.RefIdxs[LIST_1] = ref_idxs_direct[LIST_1];
                    *cur_mb.MVs[LIST_0] = mvs_direct[LIST_0];
                    *cur_mb.MVs[LIST_1] = mvs_direct[LIST_1];
                    cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
                    cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
                    for (i = 0; i < 4; i++)
                        cur_mb.GlobalMacroblockInfo->sbtype[i] = sbtype_direct[i];
                    break;
                case MBTYPE_FORWARD:
                    for (i = 0; i < 16; i ++) {
                        mvsL0[i] = BestMV16x16L0;
                        mvsL1[i] = null_mv;
                        refsL0[i] = (T_RefIdx)BestRef16x16L0;
                        refsL1[i] = (T_RefIdx)-1;
                    }
                    break;
                case MBTYPE_BACKWARD:
                    for (i = 0; i < 16; i ++) {
                        mvsL0[i] = null_mv;
                        mvsL1[i] = BestMV16x16L1;
                        refsL0[i] = (T_RefIdx)-1;
                        refsL1[i] = (T_RefIdx)BestRef16x16L1;
                    }
                    break;
                case MBTYPE_BIDIR:
                    for (i = 0; i < 16; i ++) {
                        mvsL0[i] = BestMV16x16L0;
                        mvsL1[i] = BestMV16x16L1;
                        refsL0[i] = (T_RefIdx)BestRef16x16L0;
                        refsL1[i] = (T_RefIdx)BestRef16x16L1;
                    }
                    MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir16x16, 256*sizeof(PIXTYPE)); //copy buffer for mc
                    break;
            }
#ifdef USE_NV12
            BestInterSAD = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
            BestInterSAD = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 0);
#endif // USE_NV12
        } else if (ModeRDOpt)
            BestInterSAD = IPP_MIN(IPP_MIN(RDCost16x16Direct, RDCost16x16Bi), IPP_MIN(RDCost16x16L0, RDCost16x16L1));
        else {
            BestInterSAD = BestSAD16x16;
            intra_cost_b = BITS_COST(9, glob_RDQM[uMBQP]);
        }

        if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || !(BestSAD16x16 < EmptyThreshold[uMBQP])) {
            H264MacroblockLocalInfo sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
            H264MacroblockGlobalInfo sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;
            if ((core_enc->m_Analyse & ANALYSE_ME_CHROMA) && core_enc->m_PicParamSet->chroma_format_idc != 0) {
                /*Ipp32s*/ uOffset = core_enc->m_pMBOffsets[uMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
                if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE)) {
                    pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, false);
                    cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA;
#ifdef USE_NV12
                    intra_cost_chroma = H264ENC_MAKE_NAME(H264CoreEncoder_IntraSelectChromaRD_NV12)(
                        state,
                        curr_slice,
                        core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                        core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                        cur_mb.mbPitchPixels,
                        &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                        cur_mb.mbChromaIntra.prediction,
                        cur_mb.mbChromaIntra.prediction + 8,
                        1);
#else // USE_NV12
                    intra_cost_chroma = H264ENC_MAKE_NAME(H264CoreEncoder_IntraSelectChromaRD)(
                        state,
                        curr_slice,
                        core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                        core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                        core_enc->m_pCurrentFrame->m_pVPlane + uOffset,
                        core_enc->m_pReconstructFrame->m_pVPlane + uOffset,
                        cur_mb.mbPitchPixels,
                        &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                        cur_mb.mbChromaIntra.prediction,
                        cur_mb.mbChromaIntra.prediction + 8);
#endif // USE_NV12
                } else {
#ifdef USE_NV12
                    intra_cost_chroma = H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8_NV12)(
                            state,
                            curr_slice,
                            core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                            core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                            cur_mb.mbPitchPixels,
                            &cur_mb.LocalMacroblockInfo->intra_chroma_mode,
                            cur_mb.mbChromaIntra.prediction,
                            cur_mb.mbChromaIntra.prediction + 8,
                            1);  //up to 422 only
#else
                    Ipp8u mode;
                    intra_cost_chroma = H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8)(
                        state,
                        curr_slice,
                        core_enc->m_pCurrentFrame->m_pUPlane + uOffset,
                        core_enc->m_pReconstructFrame->m_pUPlane + uOffset,
                        core_enc->m_pCurrentFrame->m_pVPlane + uOffset,
                        core_enc->m_pReconstructFrame->m_pVPlane + uOffset,
                        cur_mb.mbPitchPixels,
                        &mode,
                        cur_mb.mbChromaIntra.prediction,
                        cur_mb.mbChromaIntra.prediction + 8);
#endif
                }
            }
            H264ENC_MAKE_NAME(H264CoreEncoder_Intra16x16SelectAndPredict)(state, curr_slice, &BestIntraSAD16x16, cur_mb.mb16x16.prediction);
            intraLocalMBinfo = *cur_mb.LocalMacroblockInfo;
            *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
            *cur_mb.GlobalMacroblockInfo = sGlobalMBinfo;
        }
        BestIntraMBType = MBTYPE_INTRA_16x16;
        BestIntraSAD = BestIntraSAD16x16;
        if (doSplit)
            if ((BestInterSAD > (((Ipp32s)BestIntraSAD16x16 + intra_cost_chroma + intra_cost_b) * 269 >> 8)) || (!(core_enc->m_Analyse & ANALYSE_RD_OPT) && !(core_enc->m_Analyse & ANALYSE_RD_MODE) && (BestSAD16x16 < EmptyThreshold[uMBQP]))) {
               // BestSAD16x16 = BestInterSAD;
               // goto CheckIntra;
                doSplit = false;
            }
    }
    if (doSplit && (core_enc->m_Analyse & ANALYSE_FLATNESS)) {
        __ALIGN16 PIXTYPE mcBlock[256];
        cur_mb.GlobalMacroblockInfo->mbtype = BestMBType;
        for (i = 0; i < 4; i++)
            cur_mb.GlobalMacroblockInfo->sbtype[i] = (MBTypeValue)NUMBER_OF_MBTYPES;
        switch (BestMBType) {
            case MBTYPE_DIRECT:
                *cur_mb.RefIdxs[LIST_0] = ref_idxs_direct[LIST_0];
                *cur_mb.RefIdxs[LIST_1] = ref_idxs_direct[LIST_1];
                *cur_mb.MVs[LIST_0] = mvs_direct[LIST_0];
                *cur_mb.MVs[LIST_1] = mvs_direct[LIST_1];
                cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
                cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
                for (i = 0; i < 4; i++)
                    cur_mb.GlobalMacroblockInfo->sbtype[i] = sbtype_direct[i];
                break;
            case MBTYPE_FORWARD:
                for (i = 0; i < 16; i ++) {
                    mvsL0[i] = BestMV16x16L0;
                    mvsL1[i] = null_mv;
                    refsL0[i] = (T_RefIdx)BestRef16x16L0;
                    refsL1[i] = (T_RefIdx)-1;
                }
                break;
            case MBTYPE_BACKWARD:
                for (i = 0; i < 16; i ++) {
                    mvsL0[i] = null_mv;
                    mvsL1[i] = BestMV16x16L1;
                    refsL0[i] = (T_RefIdx)-1;
                    refsL1[i] = (T_RefIdx)BestRef16x16L1;
                }
                break;
            case MBTYPE_BIDIR:
                for (i = 0; i < 16; i ++) {
                    mvsL0[i] = BestMV16x16L0;
                    mvsL1[i] = BestMV16x16L1;
                    refsL0[i] = (T_RefIdx)BestRef16x16L0;
                    refsL1[i] = (T_RefIdx)BestRef16x16L1;
                }
                MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir16x16, 256*sizeof(PIXTYPE)); //copy buffer for mc
                break;
        }
        H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBLuma)(state, curr_slice, mvsL0, mvsL1, mcBlock);
        Ipp32s sad8x8[4];
        if (resPredFlag) {
          sad8x8[0] = SAD8x8_16_ResPred(resPredBuf, mcBlock);
          sad8x8[1] = SAD8x8_16_ResPred(resPredBuf + 8, mcBlock + 8);
          sad8x8[2] = SAD8x8_16_ResPred(resPredBuf + 8*16, mcBlock + 8*16);
          sad8x8[3] = SAD8x8_16_ResPred(resPredBuf + 8*16 + 8, mcBlock + 8*16 + 8);
        } else {
          sad8x8[0] = SAD8x8(cur_mb.mbPtr, cur_mb.mbPitchPixels, mcBlock, 16);
          sad8x8[1] = SAD8x8(cur_mb.mbPtr + 8, cur_mb.mbPitchPixels, mcBlock + 8, 16);
          sad8x8[2] = SAD8x8(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels, cur_mb.mbPitchPixels, mcBlock + 8 * 16, 16);
          sad8x8[3] = SAD8x8(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels + 8, cur_mb.mbPitchPixels, mcBlock + 8 * 16 + 8, 16);
        }
        if ((sad8x8[0] <= (EmptyThreshold[uMBQP] >> 2)) && (sad8x8[1] <= (EmptyThreshold[uMBQP] >> 2)) && (sad8x8[2] <= (EmptyThreshold[uMBQP] >> 2)) && (sad8x8[3] <= (EmptyThreshold[uMBQP] >> 2)))
            doSplit = false;
        else if (sad8x8[0] != 0) {
#define LB 0.75
#define UB (1.0 / LB)
            double d = 1.0 / sad8x8[0], d1 = sad8x8[1] * d, d2 = sad8x8[2] * d, d3 = sad8x8[3] * d;
            if ((LB < d1) && (d1 < UB) && (LB < d2) && (d2 < UB) && (LB < d3) && (d3 < UB))
                doSplit = false;
        }
    }
    if (doSplit) {
        Ipp32s b;
#ifndef NO_PADDING
        Ipp32s iXType, iYType, offset;
#endif
        IppiSize chroma_size = {0, 0};
        H264MotionVector chroma_vec;

        ref_idxs_direct8x8[0] = ref_idxs_direct[0];
        ref_idxs_direct8x8[1] = ref_idxs_direct[1];
        mvs_direct8x8[0] = mvs_direct[0];
        mvs_direct8x8[1] = mvs_direct[1];

        if (core_enc->m_info.mv_search_method != MV_SEARCH_TYPE_FULL)
           meInfo.searchAlgo = MV_SEARCH_TYPE_SQUARE + core_enc->m_SubME_Algo * 32;
        curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_B_8x8;

        if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
            chroma_size =  size4x4;
        else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
            chroma_size =  size4x8;

        BestSAD8x8S = BITS_COST(9, pRDQM);
        cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_B_8x8;
        meInfo.block.width = 8;
        meInfo.block.height = 8;
        meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 2 : 0;
        for (b = 0; b < 4; b ++) {
            Ipp32s sb_off = (b >> 1) * 8 * 16 + (b & 1) * 8;
            meInfo.pResY = resPredBuf + sb_off;
            meInfo.pResU = resPredBufC + (sb_off >> 1);
            meInfo.pResV = resPredBufC + 8  + (sb_off >> 1);
            bOff = block_subblock_mapping[b*4];
            meInfo.pCur = cur_mb.mbPtr + (b >> 1) * 8 * pitchPixels + (b & 1) * 8;
#ifdef NO_PADDING
            block8x8Pos.x = (b & 1) * 8;
            block8x8Pos.y = (b >> 1) * 8;
            meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x;
            meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y;
#endif // NO_PADDING
            if(  meFlags & ANALYSE_ME_CHROMA ){
                meInfo.pCurU = pMBCurU + (b >> 1) * 4 * pitchPixels + (b & 1) * 4;
                meInfo.pCurV = pMBCurV + (b >> 1) * 4 * pitchPixels + (b & 1) * 4;
            }

            // Direct subblocks in dependency layers
            if (!core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag) {
                H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectSubblockSpatial)( state, curr_slice, bOff, 2, 2,
                    ref_idxs_direct8x8,
                    mvs_direct8x8);
                H264ENC_MAKE_NAME(H264CoreEncoder_CDirectB8x8_Interp)(state, curr_slice, bOff,
                    ref_idxs_direct8x8,
                    mvs_direct8x8);

                Direct8x8SAD[b] = BITS_COST(1, pRDQM);
                if (!resPredFlag)
                {
                    if (meFlags & ANALYSE_SAD)
                        Direct8x8SAD[b] += SAD8x8(meInfo.pCur, pitchPixels, curr_slice->m_pPred4DirectB8x8+sb_off, 16);
                    else
                        Direct8x8SAD[b] += SATD8x8(meInfo.pCur, pitchPixels, curr_slice->m_pPred4DirectB8x8+sb_off, 16);
                }
                else
                {
                    if (meFlags & ANALYSE_SAD)
                        Direct8x8SAD[b] += SAD8x8_16_ResPred(resPredBuf+sb_off, curr_slice->m_pPred4DirectB8x8+sb_off);
                    else
                        Direct8x8SAD[b] += SATD8x8_16_ResPred(resPredBuf+sb_off, curr_slice->m_pPred4DirectB8x8+sb_off);
                }
            }

            //*** L0 search
            ref_idx = BestRef16x16L0;
            refsL0[bOff] = ref_idx;
            H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 2, 2, &(meInfo));
            ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_RefPicList;
            meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields0[ref_idx]] + (b >> 1) * 8 * pitchPixels + (b & 1) * 8;
#ifdef NO_PADDING
            meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
            if(  meFlags & ANALYSE_ME_CHROMA ){
                meInfo.chroma_mvy_offset = 0;
                if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                    if (!curr_slice->m_is_cur_mb_bottom_field && pFields0[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                    else if (curr_slice->m_is_cur_mb_bottom_field && !pFields0[ref_idx]) meInfo.chroma_mvy_offset += 2;
                }
#ifdef USE_NV12
                meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + (b >> 1) * 4 * pitchPixels + (b & 1) * 8;
                meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + (b >> 1) * 4 * pitchPixels + (b & 1) * 4;
                meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + (b >> 1) * 4 * pitchPixels + (b & 1) * 4;
#endif // USE_NV12
#ifdef NO_PADDING
                meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
                meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
            }
            meInfo.candNum = 0;
            meInfo.bestSAD = MAX_SAD;
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV16x16L0);
            H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
            if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCostL0[ref_idx] > meInfo.threshold)
                    H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
//            if (meInfo.bestSAD + refCostL0[ref_idx] < bSAD8x8[b]) {
                BestSAD8x8L0 = meInfo.bestSAD + refCostL0[ref_idx];
                BestMV8x8L0 = BestMV8x8L0R[b] = meInfo.bestMV;
                BestRef8x8L0 = /*BestRef8x8L0R[b] =*/ ref_idx;
                //PredMV8x8L0 = meInfo.predictedMV;
                BestPredMV8x8L0[b] = meInfo.bestPredMV;//meInfo.predictedMV;
                mpred8x8L0[b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
//             }
            //MC luma for weighted prediction
#ifdef NO_PADDING
            pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
            pInterpolateInfo->pDst[0] = pTmpBufL0;
            pInterpolateInfo->sizeBlock = meInfo.block;
            pInterpolateInfo->pointVector.x = BestMV8x8L0.mvx;
            pInterpolateInfo->pointVector.y = BestMV8x8L0.mvy;
            pInterpolateInfo->pointBlockPos.x = mbPos.x + block8x8Pos.x;
            pInterpolateInfo->pointBlockPos.y = mbPos.y + block8x8Pos.y;
            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
            H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, BestMV8x8L0.mvx >> SUB_PEL_SHIFT, BestMV8x8L0.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                pTmpBufL0, 16, BestMV8x8L0.mvx&3, BestMV8x8L0.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING

            if(  meFlags & ANALYSE_ME_CHROMA ){
                chroma_vec.mvx = BestMV16x16L0.mvx;
                chroma_vec.mvy = BestMV16x16L0.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                pInterpolateInfoChroma->pDst[0] = pTmpBufL0 + 8;
                pInterpolateInfoChroma->pDst[1] = pTmpBufL0 + 8 + 4;
                if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                {
                    pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                    pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                    pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                    pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                }
                else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                {
                    pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                    pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                    pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                    pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                }
                pInterpolateInfoChroma->sizeBlock = chroma_size;
                H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL0+8, pTmpBufL0+8+4, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#else // USE_NV12
                H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL0+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
                H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, pTmpBufL0+8+4, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
            }

            //*** L1 search
            ref_idx = BestRef16x16L1;
            refsL1[bOff] = ref_idx;
            H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_1, 2, 2, &(meInfo));
            ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, is_cur_mb_field, uMB & 1)->m_RefPicList;
            meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields1[ref_idx]] + (b >> 1) * 8 * pitchPixels + (b & 1) * 8;
#ifdef NO_PADDING
            meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
            if(  meFlags & ANALYSE_ME_CHROMA ){
                meInfo.chroma_mvy_offset = 0;
                if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                    if (!curr_slice->m_is_cur_mb_bottom_field && pFields1[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                    else if (curr_slice->m_is_cur_mb_bottom_field && !pFields1[ref_idx]) meInfo.chroma_mvy_offset += 2;
                }
#ifdef USE_NV12
                meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + (b >> 1) * 4 * pitchPixels + (b & 1) * 8;
                meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + (b >> 1) * 4 * pitchPixels + (b & 1) * 4;
                meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + (b >> 1) * 4 * pitchPixels + (b & 1) * 4;
#endif // USE_NV12
#ifdef NO_PADDING
                meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
                meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
            }
            meInfo.candNum = 0;
            meInfo.bestSAD = MAX_SAD;
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV16x16L1);
            H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
            if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCostL1[ref_idx] > meInfo.threshold)
                    H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
//            if (meInfo.bestSAD + refCostL1[ref_idx] < bSAD8x8[b]) {
                BestSAD8x8L1 = meInfo.bestSAD + refCostL1[ref_idx];
                BestMV8x8L1 = BestMV8x8L1R[b] = meInfo.bestMV;
                BestRef8x8L1 = /*BestRef8x8L1R[b] =*/ ref_idx;
                //PredMV8x8L1 = meInfo.predictedMV;
                BestPredMV8x8L1[b] = meInfo.bestPredMV;//meInfo.predictedMV;
                mpred8x8L1[b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
//             }
            //MC luma for weighted prediction
#ifdef NO_PADDING
            pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
            pInterpolateInfo->pDst[0] = pTmpBufL1;
            pInterpolateInfo->sizeBlock = meInfo.block;
            pInterpolateInfo->pointVector.x = BestMV8x8L1.mvx;
            pInterpolateInfo->pointVector.y = BestMV8x8L1.mvy;
            pInterpolateInfo->pointBlockPos.x = mbPos.x + block8x8Pos.x;
            pInterpolateInfo->pointBlockPos.y = mbPos.y + block8x8Pos.y;
            H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
            H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, BestMV8x8L1.mvx >> SUB_PEL_SHIFT, BestMV8x8L1.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                pTmpBufL1, 16, BestMV8x8L1.mvx&3, BestMV8x8L1.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING

            if(  meFlags & ANALYSE_ME_CHROMA ){
                chroma_vec.mvx = BestMV16x16L1.mvx;
                chroma_vec.mvy = BestMV16x16L1.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                pInterpolateInfoChroma->pDst[0] = pTmpBufL1 + 8;
                pInterpolateInfoChroma->pDst[1] = pTmpBufL1 + 8 + 4;
                if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                {
                    pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                    pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                    pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                    pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                }
                else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                {
                    pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                    pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                    pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                    pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                }
                pInterpolateInfoChroma->sizeBlock = chroma_size;
                H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL1+8, pTmpBufL1+8+4, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#else // USE_NV12
                H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL1+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
                H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, pTmpBufL1+8+4, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
            }

            //Try weighted prediction
            w1 = curr_slice->DistScaleFactor[BestRef8x8L0][BestRef8x8L1] >> 2;
            w0 = 64-w1;
            H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)( bidir8x8 + sb_off,
                pTmpBufL0, pTmpBufL1, 16, core_enc->use_implicit_weighted_bipred ? 2: 1,w1, w0, meInfo.block);

            if (!resPredFlag) {
                if (meFlags & ANALYSE_SAD)
                    BestSAD8x8Bi = SAD8x8( meInfo.pCur, pitchPixels, bidir8x8 + sb_off, 16);
                else
                    BestSAD8x8Bi = SATD8x8( meInfo.pCur, pitchPixels, bidir8x8 + sb_off, 16);
            }
            else
            {
                if (meFlags & ANALYSE_SAD)
                    BestSAD8x8Bi = SAD8x8_16_ResPred(resPredBuf + sb_off, bidir8x8 + sb_off);
                else
                    BestSAD8x8Bi = SATD8x8_16_ResPred(resPredBuf + sb_off, bidir8x8 + sb_off);
            }

            Ipp32s tmp = MVConstraint( BestMV8x8L0.mvx - BestPredMV8x8L0[b].mvx, BestMV8x8L0.mvy - BestPredMV8x8L0[b].mvy, pRDQM);
            tmp += MVConstraint( BestMV8x8L1.mvx - BestPredMV8x8L1[b].mvx, BestMV8x8L1.mvy - BestPredMV8x8L1[b].mvy, pRDQM)+
                            refCostL0[BestRef8x8L0] + refCostL1[BestRef8x8L1];

            BestSAD8x8Bi += tmp;

            if(  meFlags & ANALYSE_ME_CHROMA ){
                H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(pTmpBufL0+8, pTmpBufL1+8, pTmpChromaPred, chroma_size.width, chroma_size.height, 16);
                H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(pTmpBufL0+8+4, pTmpBufL1+8+4, pTmpChromaPred+8, chroma_size.width, chroma_size.height, 16);
                Ipp32s chroma_block_size = chroma_size.width + (chroma_size.height >> 2);
//                  if ( meFlags & ANALYSE_ME_BIDIR_REFINE )
                {
                    if (!resPredFlag)
                    {
                        if (core_enc->m_Analyse & ANALYSE_SAD) {
                            chromaContrToSAD[b + SHIFT_8x8] = H264ENC_MAKE_NAME(SAD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                            chromaContrToSAD[b + SHIFT_8x8] += H264ENC_MAKE_NAME(SAD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                            BestSAD8x8Bi += chromaContrToSAD[b + SHIFT_8x8];

                        }else{
                            chromaContrToSAD[b + SHIFT_8x8] = H264ENC_MAKE_NAME(SATD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                            chromaContrToSAD[b + SHIFT_8x8] += H264ENC_MAKE_NAME(SATD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                            BestSAD8x8Bi += chromaContrToSAD[b + SHIFT_8x8];
                        }
                    }
                    else
                    {
                        if (core_enc->m_Analyse & ANALYSE_SAD) {
                            chromaContrToSAD[b + SHIFT_8x8] = SAD4x4_16_ResPred(resPredBufC + (sb_off >> 1), pTmpChromaPred);
                            chromaContrToSAD[b + SHIFT_8x8] += SAD4x4_16_ResPred(resPredBufC + 8 + (sb_off >> 1), pTmpChromaPred+8);
                        }else{
                            chromaContrToSAD[b + SHIFT_8x8] = SATD4x4_16_ResPred(resPredBufC + (sb_off >> 1), pTmpChromaPred);
                            chromaContrToSAD[b + SHIFT_8x8] += SATD4x4_16_ResPred(resPredBufC + 8 + (sb_off >> 1), pTmpChromaPred+8);
                        }
                        BestSAD8x8Bi += chromaContrToSAD[b + SHIFT_8x8];

                    }
                }
/*
                else
                {
                    if (!resPredFlag)
                    {
                        if (core_enc->m_Analyse & ANALYSE_SAD) {
                            BestSAD8x8Bi += H264ENC_MAKE_NAME(SAD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                            BestSAD8x8Bi += H264ENC_MAKE_NAME(SAD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                        } else {
                            BestSAD8x8Bi += H264ENC_MAKE_NAME(SATD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                            BestSAD8x8Bi += H264ENC_MAKE_NAME(SATD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                    }
                    else
                    {
                        if (core_enc->m_Analyse & ANALYSE_SAD) {
                            BestSAD8x8Bi += SAD4x4_16_ResPred(resPredBufC + (sb_off >> 1), pTmpChromaPred);
                            BestSAD8x8Bi += SAD4x4_16_ResPred(resPredBufC + 8 + (sb_off >> 1), pTmpChromaPred+8);
                        } else {
                            BestSAD8x8Bi += SATD4x4_16_ResPred(resPredBufC + (sb_off >> 1), pTmpChromaPred);
                            BestSAD8x8Bi += SATD4x4_16_ResPred(resPredBufC + 8 + (sb_off >> 1), pTmpChromaPred+8);
                        }
                    }
                }
*/
            }

            BestSAD8x8Bi += BITS_COST(5, pRDQM);
            BestSAD8x8L0 += BITS_COST(3, pRDQM);
            BestSAD8x8L1 += BITS_COST(3, pRDQM);

            Ipp32s BestSAD = BestSAD8x8L0;
            Ipp32s BestSMBType = SBTYPE_FORWARD_8x8;
            if( BestSAD8x8L1 < BestSAD ){
                BestSAD = BestSAD8x8L1;
                BestSMBType = SBTYPE_BACKWARD_8x8;
            }
            if ( BestSAD8x8Bi < BestSAD ){
                BestSAD = BestSAD8x8Bi;
                BestSMBType = SBTYPE_BIDIR_8x8;
                if ( meFlags & ANALYSE_ME_BIDIR_REFINE )
                {
                    predBidirRefineL0[b + SHIFT_8x8] = BestPredMV8x8L0[b];
                    predBidirRefineL1[b + SHIFT_8x8] = BestPredMV8x8L1[b];
                    bestSADByMBtype[b + SHIFT_8x8] = BestSAD;// - chromaContrToSAD[b + SHIFT_8x8];
                }
            }

            if (!RefLater8x8PackFlag || core_enc->m_SeqParamSet->direct_8x8_inference_flag)
            {
                if ( Direct8x8SAD[b] < BestSAD ){
                    BestSAD = Direct8x8SAD[b];
                    BestSMBType = SBTYPE_DIRECT;
                }
            }

            BestSAD8x8S += (BestSAD8x8[b] = BestSAD);

            sbt[b] = BestSMBType;
            switch( BestSMBType ){
                case SBTYPE_FORWARD_8x8:
                    mvsL0[bOff] = mvsL0[bOff+1] = mvsL0[bOff+4] = mvsL0[bOff+5] = BestMV8x8L0;
                    refsL0[bOff] = refsL0[bOff+1] = refsL0[bOff+4] = refsL0[bOff+5] = BestRef8x8L0;
                    mvsL1[bOff] = mvsL1[bOff+1] = mvsL1[bOff+4] = mvsL1[bOff+5] = null_mv;
                    refsL1[bOff] = refsL1[bOff+1] = refsL1[bOff+4] = refsL1[bOff+5] = -1;
                    break;
                case SBTYPE_BACKWARD_8x8:
                    mvsL0[bOff] = mvsL0[bOff+1] = mvsL0[bOff+4] = mvsL0[bOff+5] = null_mv;
                    refsL0[bOff] = refsL0[bOff+1] = refsL0[bOff+4] = refsL0[bOff+5] = -1;
                    mvsL1[bOff] = mvsL1[bOff+1] = mvsL1[bOff+4] = mvsL1[bOff+5] = BestMV8x8L1;
                    refsL1[bOff] = refsL1[bOff+1] = refsL1[bOff+4] = refsL1[bOff+5] = BestRef8x8L1;
                    break;
                case SBTYPE_BIDIR_8x8:
                    mvsL0[bOff] = mvsL0[bOff+1] = mvsL0[bOff+4] = mvsL0[bOff+5] = BestMV8x8L0;
                    refsL0[bOff] = refsL0[bOff+1] = refsL0[bOff+4] = refsL0[bOff+5] = BestRef8x8L0;
                    mvsL1[bOff] = mvsL1[bOff+1] = mvsL1[bOff+4] = mvsL1[bOff+5] = BestMV8x8L1;
                    refsL1[bOff] = refsL1[bOff+1] = refsL1[bOff+4] = refsL1[bOff+5] = BestRef8x8L1;
                    break;
                case SBTYPE_DIRECT: //Shoud be checked
                    mvsL0[bOff]   = mvs_direct8x8[LIST_0].MotionVectors[bOff];
                    mvsL0[bOff+1] = mvs_direct8x8[LIST_0].MotionVectors[bOff+1];
                    mvsL0[bOff+4] = mvs_direct8x8[LIST_0].MotionVectors[bOff+4];
                    mvsL0[bOff+5] = mvs_direct8x8[LIST_0].MotionVectors[bOff+5];
                    mvsL1[bOff]   = mvs_direct8x8[LIST_1].MotionVectors[bOff];
                    mvsL1[bOff+1] = mvs_direct8x8[LIST_1].MotionVectors[bOff+1];
                    mvsL1[bOff+4] = mvs_direct8x8[LIST_1].MotionVectors[bOff+4];
                    mvsL1[bOff+5] = mvs_direct8x8[LIST_1].MotionVectors[bOff+5];

                    refsL0[bOff]   = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff];
                    refsL0[bOff+1] = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff+1];
                    refsL0[bOff+4] = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff+4];
                    refsL0[bOff+5] = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff+5];
                    refsL1[bOff]   = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff];
                    refsL1[bOff+1] = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff+1];
                    refsL1[bOff+4] = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff+4];
                    refsL1[bOff+5] = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff+5];
                    break;
            }

         }
         //Save the best choice
         for( b = 0; b<4; b++ ){
            bOff = block_subblock_mapping[b*4];
            BestMV8x8SL0[b] = mvsL0[bOff];
            BestMV8x8SL1[b] = mvsL1[bOff];
            BestRef8x8SL0[b] = refsL0[bOff];
            BestRef8x8SL1[b] = refsL1[bOff];
            Best8x8Type[b] = sbt[b];
         }

         if (ModeRDOpt /* && (BestSAD8x8S <= (BestSAD16x16 * SB_THRESH_RD))*/ ) {
            //Set MVs and RefIds
            curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_B_8x8;
            MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir8x8, 256*sizeof(PIXTYPE)); //copy buffer for mc
#ifdef USE_NV12
            RDCost8x8 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
            RDCost8x8 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 0 );
#endif // USE_NV12
         }

         if (RefLater8x8PackFlag)
             meFlags &= ~ANALYSE_B_4x4;

        //Analyse 4x4, 4x8, 8x4
        if((meFlags & ANALYSE_B_4x4) && (BestSAD8x8S < BestSAD16x16)) {
            Ipp32s bSADs,s;
            PIXTYPE* pRef;
            PIXTYPE* pCurU = NULL;
            PIXTYPE* pCurV = NULL;
            PIXTYPE* pRefU = NULL;
            PIXTYPE* pRefV = NULL;
            Ipp32s bSAD4x4Bi=0, bSAD4x8Bi=0, bSAD8x4Bi=0;
            IppiSize csize44 = {0, 0};
            IppiSize csize48 = {0, 0};
            IppiSize csize84 = {0, 0};
            //H264MotionVector chroma_vec;
            __ALIGN16 PIXTYPE cbufL0[128],cbufL1[128];

            BestSAD8x8S = BITS_COST(9, pRDQM);
            if( core_enc->m_PicParamSet->chroma_format_idc == 1){ //420
                csize44 =  size2x2;
                csize48 =  size2x4;
                csize84 =  size4x2;
            }else if( core_enc->m_PicParamSet->chroma_format_idc == 2){ //422
                csize44 =  size2x4;
                csize48.width = 2; csize48.height = 8;
                csize84 =  size4x4;
            }

            for (s = 0; s < 4; s ++) {
                Ipp32s sb_off = (s >> 1) * 8 * 16 + (s & 1) * 8;
                PIXTYPE *pCur = cur_mb.mbPtr + (s >> 1) * 8 * pitchPixels + (s & 1) * 8;
#ifdef NO_PADDING
                block8x8Pos.x = (s & 1) * 8;
                block8x8Pos.y = (s >> 1) * 8;
#endif // NO_PADDING
                if (meFlags & ANALYSE_ME_CHROMA) {
                    pCurU = pMBCurU + (s >> 1) * 4 * pitchPixels + (s & 1) * 4;
                    pCurV = pMBCurV + (s >> 1) * 4 * pitchPixels + (s & 1) * 4;
                }
                //L0
                ref_idx = BestRef16x16L0;
                bOff = block_subblock_mapping[s*4];
                refsL0[bOff]=refsL0[bOff+1]=refsL0[bOff+4]=refsL0[bOff+5]=ref_idx;
                ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_RefPicList;
                pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields0[ref_idx]] + (s >> 1) * 8 * pitchPixels + (s & 1) * 8;
#ifdef NO_PADDING
                meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                if (meFlags & ANALYSE_ME_CHROMA) {
                    meInfo.chroma_mvy_offset = 0;
                    if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                        if (!curr_slice->m_is_cur_mb_bottom_field && pFields0[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                        else if (curr_slice->m_is_cur_mb_bottom_field && !pFields0[ref_idx]) meInfo.chroma_mvy_offset += 2;
                    }
#ifdef USE_NV12
                    pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + (s >> 1) * 4 * pitchPixels + (s & 1) * 8;
                    pRefV = pRefU + 1;
#else // USE_NV12
                    pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + (s >> 1) * 4 * pitchPixels + (s & 1) * 4;
                    pRefV = ppRefPicList[ref_idx]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + (s >> 1) * 4 * pitchPixels + (s & 1) * 4;
#endif // USE_NV12
#ifdef NO_PADDING
                    meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
                    meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                }
                // 4x4
                meInfo.block.width = meInfo.block.height = 4;
                meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 4 : 0;
                bSADs = BITS_COST(7, pRDQM) + refCostL0[ref_idx];
                bSAD4x4Bi = bSAD4x8Bi = bSAD8x4Bi = refCostL0[ref_idx];
                w1 = curr_slice->DistScaleFactor[ref_idx][0] >> 2;

                for (b = 0; b < 4; b++) {
                    Ipp16s b4x4_off = (b >> 1) * 4 * 16 + (b & 1) * 4;
                    meInfo.pResY = resPredBuf + sb_off + b4x4_off;
                    Ipp16s *pRPredC = resPredBufC + ((sb_off + b4x4_off) >> 1);
                    meInfo.pResU = pRPredC;
                    meInfo.pResV = pRPredC + 8;

                    bOff = block_subblock_mapping[s*4+b];
                    H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 1, 1, &(meInfo));
                    meInfo.pCur = pCur + (b >> 1) * 4 * pitchPixels + (b & 1) * 4;
                    meInfo.pRef = pRef + (b >> 1) * 4 * pitchPixels + (b & 1) * 4;
#ifdef NO_PADDING
                    meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x + (b & 1) * 4;
                    meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y + (b >> 1) * 4;
#endif // NO_PADDING
                    if (meFlags & ANALYSE_ME_CHROMA) {
                        meInfo.pCurU = pCurU + (b >> 1) * 2 * pitchPixels + (b & 1) * 2;
                        meInfo.pCurV = pCurV + (b >> 1) * 2 * pitchPixels + (b & 1) * 2;
#ifdef USE_NV12
                        meInfo.pRefU = pRefU + (b >> 1) * 2 * pitchPixels + (b & 1) * 4;
                        meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                        meInfo.pRefU = pRefU + (b >> 1) * 2 * pitchPixels + (b & 1) * 2;
                        meInfo.pRefV = pRefV + (b >> 1) * 2 * pitchPixels + (b & 1) * 2;
#endif // USE_NV12
                    }
                    meInfo.candNum = 0;
                    meInfo.bestSAD = MAX_SAD;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8SL0[s]);

                    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                    if (meFlags & ANALYSE_ME_SUBPEL)
                        H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                    mvsL0[bOff] = BestMV4x4L0[s][b] = meInfo.bestMV; // for MV prediction in next block
                    bSADs += meInfo.bestSAD;
                    BestPredMV4x4L0[s][b] = meInfo.bestPredMV;
                    mpred4x4L0[s][b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                    //MC for further BiPred
#ifdef NO_PADDING
                    pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
                    pInterpolateInfo->pDst[0] = pTmpBufL0 + (b>>1)*4*16 + (b&1)*4;
                    pInterpolateInfo->sizeBlock = meInfo.block;
                    pInterpolateInfo->pointVector.x = meInfo.bestMV.mvx;
                    pInterpolateInfo->pointVector.y = meInfo.bestMV.mvy;
                    pInterpolateInfo->pointBlockPos.x = meInfo.interpolateInfo.pointBlockPos.x;
                    pInterpolateInfo->pointBlockPos.y = meInfo.interpolateInfo.pointBlockPos.y;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                    H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, meInfo.bestMV.mvx >> SUB_PEL_SHIFT, meInfo.bestMV.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                        pTmpBufL0 + (b>>1)*4*16 + (b&1)*4, 16, meInfo.bestMV.mvx&3, meInfo.bestMV.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
                    bSAD4x4Bi += MVConstraint( meInfo.bestMV.mvx - meInfo.bestPredMV.mvx, meInfo.bestMV.mvy - meInfo.bestPredMV.mvy, pRDQM);

                    if(  meFlags & ANALYSE_ME_CHROMA ){
                       chroma_vec.mvx = meInfo.bestMV.mvx;
                       chroma_vec.mvy = meInfo.bestMV.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                       pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                       pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                       pInterpolateInfoChroma->pDst[0] = cbufL0+(b>>1)*2*16+(b&1)*2;
                       pInterpolateInfoChroma->pDst[1] = cbufL0+64+(b>>1)*2*16+(b&1)*2;
                       if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                       {
                           pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                           pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                           pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                           pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                       }
                       else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                       {
                           pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                           pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                           pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                           pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                       }
                       pInterpolateInfoChroma->sizeBlock = csize44;
                       H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                       offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                       H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL0+(b>>1)*2*16+(b&1)*2, cbufL0+64+(b>>1)*2*16+(b&1)*2, 16, iXType, iYType, csize44, meInfo.bit_depth_chroma);
#else // USE_NV12
                       H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL0+(b>>1)*2*16+(b&1)*2, 16, iXType, iYType, csize44, meInfo.bit_depth_chroma);
                       H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, cbufL0+64+(b>>1)*2*16+(b&1)*2, 16, iXType, iYType, csize44, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
                    }
                }
                if (bSADs < BestSAD8x8[s]) {
                    Best8x8Type[s] = (MBTypeValue)SBTYPE_FORWARD_4x4;
                    BestSAD8x8[s] = bSADs;
                }
                    // 8x4
                    meInfo.block.width = 8;
                    meInfo.block.height = 4;
                    meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 3 : 0;
                    bSADs = BITS_COST(5, pRDQM) + refCostL0[ref_idx];
                    for (b = 0; b < 2; b ++) {
                        bOff = block_subblock_mapping[s*4+b*2];
                        H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 2, 1, &(meInfo));
                        meInfo.pCur = pCur + b * 4 * pitchPixels;
                        meInfo.pRef = pRef + b * 4 * pitchPixels;
#ifdef NO_PADDING
                        meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x;
                        meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y + b * 4;
#endif // NO_PADDING
                        if (meFlags & ANALYSE_ME_CHROMA) {
                            meInfo.pCurU = pCurU + b * 2 * pitchPixels;
                            meInfo.pCurV = pCurV + b * 2 * pitchPixels;
                            meInfo.pRefU = pRefU + b * 2 * pitchPixels;
                            meInfo.pRefV = pRefV + b * 2 * pitchPixels;
                        }
                        meInfo.candNum = 0;
                        meInfo.bestSAD = MAX_SAD;
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8SL0[s]);
                        H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                        if (meFlags & ANALYSE_ME_SUBPEL)
                            H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                        mvsL0[bOff] = mvsL0[bOff+1] = BestMV8x4L0[s][b] = meInfo.bestMV;// for MV prediction in next block
                        bSADs += meInfo.bestSAD;
                        BestPredMV8x4L0[s][b] = meInfo.bestPredMV;
                        mpred8x4L0[s][b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                        //MC for further BiPred
#ifdef NO_PADDING
                        pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
                        pInterpolateInfo->pDst[0] = pTmpBufL0 + 8 + b*4*16;
                        pInterpolateInfo->sizeBlock = meInfo.block;
                        pInterpolateInfo->pointVector.x = meInfo.bestMV.mvx;
                        pInterpolateInfo->pointVector.y = meInfo.bestMV.mvy;
                        pInterpolateInfo->pointBlockPos.x = meInfo.interpolateInfo.pointBlockPos.x;
                        pInterpolateInfo->pointBlockPos.y = meInfo.interpolateInfo.pointBlockPos.y;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                        H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, meInfo.bestMV.mvx >> SUB_PEL_SHIFT, meInfo.bestMV.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                            pTmpBufL0 + 8 + b*4*16, 16, meInfo.bestMV.mvx&3, meInfo.bestMV.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
                        bSAD8x4Bi += MVConstraint( meInfo.bestMV.mvx - meInfo.bestPredMV.mvx, meInfo.bestMV.mvy - meInfo.bestPredMV.mvy, pRDQM);

                        if(  meFlags & ANALYSE_ME_CHROMA ){
                            chroma_vec.mvx = meInfo.bestMV.mvx;
                            chroma_vec.mvy = meInfo.bestMV.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                            pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                            pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                            pInterpolateInfoChroma->pDst[0] = cbufL0+4+(b>>1)*2*16;
                            pInterpolateInfoChroma->pDst[1] = cbufL0+64+4+(b>>1)*2*16;
                            if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                            {
                                pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                                pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                                pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                                pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                            }
                            else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                            {
                                pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                                pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                                pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                                pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                            }
                            pInterpolateInfoChroma->sizeBlock = csize84;
                            H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                            offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                            H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL0+4+(b>>1)*2*16, cbufL0+64+4+(b>>1)*2*16, 16, iXType, iYType, csize84, meInfo.bit_depth_chroma);
#else // USE_NV12
                            H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL0+4+(b>>1)*2*16, 16, iXType, iYType, csize84, meInfo.bit_depth_chroma);
                            H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, cbufL0+64+4+(b>>1)*2*16, 16, iXType, iYType, csize84, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
                        }
                    }
                    if (bSADs < BestSAD8x8[s]) {
                        Best8x8Type[s] = (MBTypeValue)SBTYPE_FORWARD_8x4;
                        BestSAD8x8[s] = bSADs;
                    }
                    // 4x8
                    meInfo.block.width = 4;
                    meInfo.block.height = 8;
                    meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 3 : 0;
                    bSADs = BITS_COST(5, pRDQM) + refCostL0[ref_idx];
                    for (b = 0; b < 2; b ++) {
                        bOff = block_subblock_mapping[s*4+b];
                        H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 1, 2, &(meInfo));
                        meInfo.pCur = pCur + b * 4;
                        meInfo.pRef = pRef + b * 4;
#ifdef NO_PADDING
                        meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x + b * 4;
                        meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y;
#endif // NO_PADDING
                        if (meFlags & ANALYSE_ME_CHROMA) {
#ifdef USE_NV12
                            meInfo.pRefU = pRefU + b * 4;
                            meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                            meInfo.pRefU = pRefU + b * 2;
                            meInfo.pRefV = pRefV + b * 2;
#endif // USE_NV12
                            meInfo.pCurU = pCurU + b * 2;
                            meInfo.pCurV = pCurV + b * 2;
                        }
                        meInfo.candNum = 0;
                        meInfo.bestSAD = MAX_SAD;
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8SL0[s]);
                        H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                        if (meFlags & ANALYSE_ME_SUBPEL)
                            H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                        mvsL0[bOff] = mvsL0[bOff+4] = BestMV4x8L0[s][b] = meInfo.bestMV; // for MV prediction in next block
                        bSADs += meInfo.bestSAD;
                        BestPredMV4x8L0[s][b] = meInfo.bestPredMV;
                        mpred4x8L0[s][b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                        //MC for further BiPred
#ifdef NO_PADDING
                        pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
                        pInterpolateInfo->pDst[0] = pTmpBufL0 + 128 + b*4;
                        pInterpolateInfo->sizeBlock = meInfo.block;
                        pInterpolateInfo->pointVector.x = meInfo.bestMV.mvx;
                        pInterpolateInfo->pointVector.y = meInfo.bestMV.mvy;
                        pInterpolateInfo->pointBlockPos.x = meInfo.interpolateInfo.pointBlockPos.x;
                        pInterpolateInfo->pointBlockPos.y = meInfo.interpolateInfo.pointBlockPos.y;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                        H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, meInfo.bestMV.mvx >> SUB_PEL_SHIFT, meInfo.bestMV.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                            pTmpBufL0 + 128 + b*4, 16, meInfo.bestMV.mvx&3, meInfo.bestMV.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
                        bSAD4x8Bi += MVConstraint( meInfo.bestMV.mvx - meInfo.bestPredMV.mvx, meInfo.bestMV.mvy - meInfo.bestPredMV.mvy, pRDQM);
                        if(  meFlags & ANALYSE_ME_CHROMA ){
                           chroma_vec.mvx = meInfo.bestMV.mvx;
                           chroma_vec.mvy = meInfo.bestMV.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                           pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                           pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                           pInterpolateInfoChroma->pDst[0] = cbufL0+8+(b&1)*2;
                           pInterpolateInfoChroma->pDst[1] = cbufL0+64+8+(b&1)*2;
                           if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                           {
                               pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                               pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                               pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                               pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                           }
                           else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                           {
                               pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                               pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                               pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                               pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                           }
                           pInterpolateInfoChroma->sizeBlock = csize48;
                           H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                           offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                           H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL0+8+(b&1)*2, cbufL0+64+8+(b&1)*2, 16, iXType, iYType, csize48, meInfo.bit_depth_chroma);
#else // USE_NV12
                           H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL0+8+(b&1)*2, 16, iXType, iYType, csize48, meInfo.bit_depth_chroma);
                           H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, cbufL0+64+8+(b&1)*2, 16, iXType, iYType, csize48, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
                        }
                    }
                    if (bSADs < BestSAD8x8[s]) {
                        Best8x8Type[s] = (MBTypeValue)SBTYPE_FORWARD_4x8;
                        BestSAD8x8[s] = bSADs;
                    }
//                }

                //L1
                ref_idx = BestRef16x16L1;
                Ipp32s c = refCostL1[ref_idx];
                bSAD4x4Bi += c;
                bSAD4x8Bi += c;
                bSAD8x4Bi += c;
                ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, is_cur_mb_field, uMB & 1)->m_RefPicList;
                pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields1[ref_idx]] + (s >> 1) * 8 * pitchPixels + (s & 1) * 8;
#ifdef NO_PADDING
                meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                if (meFlags & ANALYSE_ME_CHROMA) {
                    meInfo.chroma_mvy_offset = 0;
                    if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                        if (!curr_slice->m_is_cur_mb_bottom_field && pFields1[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                        else if (curr_slice->m_is_cur_mb_bottom_field && !pFields1[ref_idx]) meInfo.chroma_mvy_offset += 2;
                    }
#ifdef USE_NV12
                    pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + (s >> 1) * 4 * pitchPixels + (s & 1) * 8;
                    pRefV = pRefU + 1;
#else // USE_NV12
                    pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + (s >> 1) * 4 * pitchPixels + (s & 1) * 4;
                    pRefV = ppRefPicList[ref_idx]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + (s >> 1) * 4 * pitchPixels + (s & 1) * 4;
#endif // USE_NV12
#ifdef NO_PADDING
                    meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
                    meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                }
                // 4x4
                meInfo.block.width = meInfo.block.height = 4;
                meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 4 : 0;
                bSADs = BITS_COST(7, pRDQM) + refCostL1[ref_idx];
                bOff = block_subblock_mapping[s*4];
                refsL1[bOff]=refsL1[bOff+1]=refsL1[bOff+4]=refsL1[bOff+5]=ref_idx;
                for (b = 0; b < 4; b ++) {
                    Ipp16s b4x4_off = (b >> 1) * 4 * 16 + (b & 1) * 4;
                    meInfo.pResY = resPredBuf + sb_off + b4x4_off;
                    Ipp16s *pRPredC = resPredBufC + ((sb_off + b4x4_off) >> 1);
                    meInfo.pResU = pRPredC;
                    meInfo.pResV = pRPredC + 8;

                    bOff = block_subblock_mapping[s*4+b];
                    H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_1, 1, 1, &(meInfo));
                    meInfo.pCur = pCur + (b >> 1) * 4 * pitchPixels + (b & 1) * 4;
                    meInfo.pRef = pRef + (b >> 1) * 4 * pitchPixels + (b & 1) * 4;
#ifdef NO_PADDING
                    meInfo.interpolateInfo.pointBlockPos.x = mbPos.x/* + block8x8Pos.x + (b & 1) * 4*/;
                    meInfo.interpolateInfo.pointBlockPos.y = mbPos.y/* + block8x8Pos.y + (b >> 1) * 4*/;
#endif // NO_PADDING
                    if (meFlags & ANALYSE_ME_CHROMA) {
                        meInfo.pCurU = pCurU + (b >> 1) * 2 * pitchPixels + (b & 1) * 2;
                        meInfo.pCurV = pCurV + (b >> 1) * 2 * pitchPixels + (b & 1) * 2;
#ifdef USE_NV12
                        meInfo.pRefU = pRefU + (b >> 1) * 2 * pitchPixels + (b & 1) * 4;
                        meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                        meInfo.pRefU = pRefU + (b >> 1) * 2 * pitchPixels + (b & 1) * 2;
                        meInfo.pRefV = pRefV + (b >> 1) * 2 * pitchPixels + (b & 1) * 2;
#endif // USE_NV12
                    }
                    meInfo.candNum = 0;
                    meInfo.bestSAD = MAX_SAD;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8SL1[s]);
                    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                    if (meFlags & ANALYSE_ME_SUBPEL)
                        H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                    mvsL1[bOff] = BestMV4x4L1[s][b] = meInfo.bestMV;// for MV prediction in next block
                    bSADs += meInfo.bestSAD;
                    BestPredMV4x4L1[s][b] = meInfo.bestPredMV;
                    mpred4x4L1[s][b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                    //MC for further BiPred
#ifdef NO_PADDING
                    pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
                    pInterpolateInfo->pDst[0] = pTmpBufL1 + (b>>1)*4*16 + (b&1)*4;
                    pInterpolateInfo->sizeBlock = meInfo.block;
                    pInterpolateInfo->pointVector.x = meInfo.bestMV.mvx;
                    pInterpolateInfo->pointVector.y = meInfo.bestMV.mvy;
                    pInterpolateInfo->pointBlockPos.x = meInfo.interpolateInfo.pointBlockPos.x;
                    pInterpolateInfo->pointBlockPos.y = meInfo.interpolateInfo.pointBlockPos.y;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                    H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, meInfo.bestMV.mvx >> SUB_PEL_SHIFT, meInfo.bestMV.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                        pTmpBufL1 + (b>>1)*4*16 + (b&1)*4, 16, meInfo.bestMV.mvx&3, meInfo.bestMV.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
                    bSAD4x4Bi += MVConstraint( meInfo.bestMV.mvx - meInfo.bestPredMV.mvx, meInfo.bestMV.mvy - meInfo.bestPredMV.mvy, pRDQM);
                    if(  meFlags & ANALYSE_ME_CHROMA ){
                       chroma_vec.mvx = meInfo.bestMV.mvx;
                       chroma_vec.mvy = meInfo.bestMV.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                       pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                       pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                       pInterpolateInfoChroma->pDst[0] = cbufL1+(b>>1)*2*16+(b&1)*2;
                       pInterpolateInfoChroma->pDst[1] = cbufL1+64+(b>>1)*2*16+(b&1)*2;
                       if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                       {
                           pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                           pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                           pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                           pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                       }
                       else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                       {
                           pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                           pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                           pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                           pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                       }
                       pInterpolateInfoChroma->sizeBlock = csize44;
                       H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                       offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                       H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL1+(b>>1)*2*16+(b&1)*2, cbufL1+64+(b>>1)*2*16+(b&1)*2, 16, iXType, iYType, csize44, meInfo.bit_depth_chroma);
#else // USE_NV12
                       H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL1+(b>>1)*2*16+(b&1)*2, 16, iXType, iYType, csize44, meInfo.bit_depth_chroma);
                       H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, cbufL1+64+(b>>1)*2*16+(b&1)*2, 16, iXType, iYType, csize44, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
                    }
                }
                if (bSADs < BestSAD8x8[s]) {
                    Best8x8Type[s] = (MBTypeValue)SBTYPE_BACKWARD_4x4;
                    BestSAD8x8[s] = bSADs;
                }
                    // 8x4
                    meInfo.block.width = 8;
                    meInfo.block.height = 4;
                    meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 3 : 0;
                    bSADs = BITS_COST(5, pRDQM) + refCostL1[ref_idx];
                    for (b = 0; b < 2; b ++) {
                        Ipp16s b8x4_off = b * 4 * 16;
                        meInfo.pResY = resPredBuf + sb_off + b8x4_off;
                        Ipp16s *pRPredC = resPredBufC + ((sb_off + b8x4_off) >> 1);
                        meInfo.pResU = pRPredC;
                        meInfo.pResV = pRPredC + 8;

                        bOff = block_subblock_mapping[s*4+b*2];
                        H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_1, 2, 1, &(meInfo));
                        meInfo.pCur = pCur + b * 4 * pitchPixels;
                        meInfo.pRef = pRef + b * 4 * pitchPixels;
#ifdef NO_PADDING
                        meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x;
                        meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y + b * 4;
#endif // NO_PADDING
                        if (meFlags & ANALYSE_ME_CHROMA) {
                            meInfo.pCurU = pCurU + b * 2 * pitchPixels;
                            meInfo.pCurV = pCurV + b * 2 * pitchPixels;
                            meInfo.pRefU = pRefU + b * 2 * pitchPixels;
                            meInfo.pRefV = pRefV + b * 2 * pitchPixels;
                        }
                        meInfo.candNum = 0;
                        meInfo.bestSAD = MAX_SAD;
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8SL1[s]);
                        H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                        if (meFlags & ANALYSE_ME_SUBPEL)
                            H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                        mvsL1[bOff] = mvsL1[bOff+1] = BestMV8x4L1[s][b] = meInfo.bestMV; // for MV prediction in next block
                        bSADs += meInfo.bestSAD;
                        BestPredMV8x4L1[s][b] = meInfo.bestPredMV;
                        mpred8x4L1[s][b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
                        //MC for further BiPred
#ifdef NO_PADDING
                        pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
                        pInterpolateInfo->pDst[0] = pTmpBufL1 + 8 + b*4*16;
                        pInterpolateInfo->sizeBlock = meInfo.block;
                        pInterpolateInfo->pointVector.x = meInfo.bestMV.mvx;
                        pInterpolateInfo->pointVector.y = meInfo.bestMV.mvy;
                        pInterpolateInfo->pointBlockPos.x = meInfo.interpolateInfo.pointBlockPos.x;
                        pInterpolateInfo->pointBlockPos.y = meInfo.interpolateInfo.pointBlockPos.y;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                        H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, meInfo.bestMV.mvx >> SUB_PEL_SHIFT, meInfo.bestMV.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                            pTmpBufL1 + 8 + b*4*16, 16, meInfo.bestMV.mvx&3, meInfo.bestMV.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
                        bSAD8x4Bi += MVConstraint( meInfo.bestMV.mvx - meInfo.bestPredMV.mvx, meInfo.bestMV.mvy - meInfo.bestPredMV.mvy, pRDQM);
                        if(  meFlags & ANALYSE_ME_CHROMA ){
                            chroma_vec.mvx = meInfo.bestMV.mvx;
                            chroma_vec.mvy = meInfo.bestMV.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                            pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                            pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                            pInterpolateInfoChroma->pDst[0] = cbufL1+4+(b>>1)*2*16;
                            pInterpolateInfoChroma->pDst[1] = cbufL1+64+4+(b>>1)*2*16;
                            if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                            {
                                pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                                pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                                pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                                pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                            }
                            else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                            {
                                pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                                pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                                pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                                pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                            }
                            pInterpolateInfoChroma->sizeBlock = csize84;
                            H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                            offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                            H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL1+4+(b>>1)*2*16, cbufL1+64+4+(b>>1)*2*16, 16, iXType, iYType, csize84, meInfo.bit_depth_chroma);
#else // USE_NV12
                            H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL1+4+(b>>1)*2*16, 16, iXType, iYType, csize84, meInfo.bit_depth_chroma);
                            H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, cbufL1+64+4+(b>>1)*2*16, 16, iXType, iYType, csize84, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
                        }
                    }
                    if (bSADs < BestSAD8x8[s]) {
                        Best8x8Type[s] = (MBTypeValue)SBTYPE_BACKWARD_8x4;
                        BestSAD8x8[s] = bSADs;
                    }
                    // 4x8
                    meInfo.block.width = 4;
                    meInfo.block.height = 8;
                    meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 3 : 0;
                    bSADs = BITS_COST(7, pRDQM) + refCostL1[ref_idx];
                    for (b = 0; b < 2; b ++) {
                      Ipp16s b4x8_off = b * 4;
                      meInfo.pResY = resPredBuf + sb_off + b4x8_off;
                      Ipp16s *pRPredC = resPredBufC + ((sb_off + b4x8_off) >> 1);
                      meInfo.pResU = pRPredC;
                      meInfo.pResV = pRPredC + 8;

                        bOff = block_subblock_mapping[s*4+b];
                        H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_1, 1, 2, &(meInfo));
                        meInfo.pCur = pCur + b * 4;
                        meInfo.pRef = pRef + b * 4;
#ifdef NO_PADDING
                        meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + block8x8Pos.x + b * 4;
                        meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + block8x8Pos.y;
#endif // NO_PADDING
                        if (meFlags & ANALYSE_ME_CHROMA) {
#ifdef USE_NV12
                            meInfo.pRefU = pRefU + b * 4;
                            meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                            meInfo.pRefU = pRefU + b * 2;
                            meInfo.pRefV = pRefV + b * 2;
#endif // USE_NV12
                            meInfo.pCurU = pCurU + b * 2;
                            meInfo.pCurV = pCurV + b * 2;
                        }
                        meInfo.candNum = 0;
                        meInfo.bestSAD = MAX_SAD;
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8SL1[s]);
                        H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                        if (meFlags & ANALYSE_ME_SUBPEL)
                            H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
                        mvsL1[bOff] = mvsL1[bOff+4] = BestMV4x8L1[s][b] = meInfo.bestMV; // for MV prediction in next block
                        bSADs += meInfo.bestSAD;
                        BestPredMV4x8L1[s][b] = meInfo.bestPredMV;
                        mpred4x8L1[s][b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
#ifdef NO_PADDING
                        pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
                        pInterpolateInfo->pDst[0] = pTmpBufL1 + 128 + b*4;
                        pInterpolateInfo->sizeBlock = meInfo.block;
                        pInterpolateInfo->pointVector.x = meInfo.bestMV.mvx;
                        pInterpolateInfo->pointVector.y = meInfo.bestMV.mvy;
                        pInterpolateInfo->pointBlockPos.x = meInfo.interpolateInfo.pointBlockPos.x;
                        pInterpolateInfo->pointBlockPos.y = meInfo.interpolateInfo.pointBlockPos.y;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                        H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, meInfo.bestMV.mvx >> SUB_PEL_SHIFT, meInfo.bestMV.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                            pTmpBufL1 + 128 + b*4, 16, meInfo.bestMV.mvx&3, meInfo.bestMV.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
                        bSAD4x8Bi += MVConstraint( meInfo.bestMV.mvx - meInfo.bestPredMV.mvx, meInfo.bestMV.mvy - meInfo.bestPredMV.mvy, pRDQM);
                        if(  meFlags & ANALYSE_ME_CHROMA ){
                           chroma_vec.mvx = meInfo.bestMV.mvx;
                           chroma_vec.mvy = meInfo.bestMV.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                           pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                           pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                           pInterpolateInfoChroma->pDst[0] = cbufL1+8+(b&1)*2;
                           pInterpolateInfoChroma->pDst[1] = cbufL1+64+8+(b&1)*2;
                           if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                           {
                               pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                               pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                               pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                               pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                           }
                           else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                           {
                               pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                               pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                               pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                               pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                           }
                           pInterpolateInfoChroma->sizeBlock = csize48;
                           H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                           offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                           H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL1+8+(b&1)*2, cbufL1+64+8+(b&1)*2, 16, iXType, iYType, csize48, meInfo.bit_depth_chroma);
#else // USE_NV12
                           H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, cbufL1+8+(b&1)*2, 16, iXType, iYType, csize48, meInfo.bit_depth_chroma);
                           H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, cbufL1+64+8+(b&1)*2, 16, iXType, iYType, csize48, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
                        }
                    }
                    if (bSADs < BestSAD8x8[s]) {
                        Best8x8Type[s] = (MBTypeValue)SBTYPE_BACKWARD_4x8;
                        BestSAD8x8[s] = bSADs;
                    }
//                }

                //BiPred 4x4
                __ALIGN16 PIXTYPE tmpBuf[256], cbuf[64];
                Ipp32s chroma_block_size = 0;
                IppiSize csize = {0, 0};
                meInfo.block.width = meInfo.block.height = 8;
                H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)( tmpBuf, pTmpBufL0, pTmpBufL1, 16, core_enc->use_implicit_weighted_bipred ? 2: 1,w1, w0, meInfo.block);
                if (resPredFlag) {
                  if (meFlags & ANALYSE_SAD)
                    bSADs = SAD8x8_16_ResPred(resPredBuf + sb_off, tmpBuf);
                  else
                    bSADs = SATD8x8_16_ResPred(resPredBuf + sb_off, tmpBuf);
                } else {
                  if (meFlags & ANALYSE_SAD)
                      bSADs = SAD8x8( pCur, pitchPixels, tmpBuf, 16);
                  else
                      bSADs = SATD8x8( pCur, pitchPixels, tmpBuf, 16);
                }
                if(  meFlags & ANALYSE_ME_CHROMA ){
                    csize.width = csize44.width*2;
                    csize.height = csize44.height*2;
                    chroma_block_size = csize.width + (csize.height >> 2);
                    H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(cbufL0, cbufL1, cbuf, csize.width, csize.height, 16);
                    H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(cbufL0+64, cbufL1+64, cbuf+4, csize.width, csize.height, 16);
                    if (resPredFlag) {
                        if (core_enc->m_Analyse & ANALYSE_SAD) {
                          bSADs += SAD4x4_16_ResPred(resPredBufC + (sb_off >> 1), cbuf);
                          bSADs += SAD4x4_16_ResPred(resPredBufC + 8 + (sb_off >> 1), cbuf+4);
                        }else{
                          bSADs += SATD4x4_16_ResPred(resPredBufC + (sb_off >> 1), cbuf);
                          bSADs += SATD4x4_16_ResPred(resPredBufC + 8 + (sb_off >> 1), cbuf+4);
                        }
                    } else {
                      if (core_enc->m_Analyse & ANALYSE_SAD) {
                          bSADs += H264ENC_MAKE_NAME(SAD)(pCurU, pitchPixels, cbuf, 16, chroma_block_size);
                          bSADs += H264ENC_MAKE_NAME(SAD)(pCurV, pitchPixels, cbuf+4, 16, chroma_block_size);
                      }else{
                          bSADs += H264ENC_MAKE_NAME(SATD)(pCurU, pitchPixels, cbuf, 16, chroma_block_size);
                          bSADs += H264ENC_MAKE_NAME(SATD)(pCurV, pitchPixels, cbuf+4, 16, chroma_block_size);
                      }
                    }
                }
                bSADs += bSAD4x4Bi + BITS_COST(7, pRDQM);
                if (bSADs < BestSAD8x8[s]) {
                     Best8x8Type[s] = (MBTypeValue)SBTYPE_BIDIR_4x4;
                     BestSAD8x8[s] = bSADs;
                }

                //BiPred 8x4
                H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)( tmpBuf+8, pTmpBufL0+8, pTmpBufL1+8, 16, core_enc->use_implicit_weighted_bipred ? 2: 1,w1, w0, meInfo.block);
                if (resPredFlag) {
                  if (meFlags & ANALYSE_SAD)
                    bSADs = SAD8x8_16_ResPred(resPredBuf + sb_off, tmpBuf+8);
                  else
                    bSADs = SATD8x8_16_ResPred(resPredBuf + sb_off, tmpBuf+8);
                } else {
                  if (meFlags & ANALYSE_SAD)
                      bSADs = SAD8x8( pCur, pitchPixels, tmpBuf+8, 16);
                  else
                      bSADs = SATD8x8( pCur, pitchPixels, tmpBuf+8, 16);
                }
                bSADs += bSAD8x4Bi + BITS_COST(7, pRDQM);
                if(  meFlags & ANALYSE_ME_CHROMA ){
                    H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(cbufL0+4, cbufL1+4, cbuf, csize.width, csize.height, 16);
                    H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(cbufL0+64+4, cbufL1+64+4, cbuf+4, csize.width, csize.height, 16);
                    if (resPredFlag) {
                      if (core_enc->m_Analyse & ANALYSE_SAD) {
                        bSADs += SAD4x4_16_ResPred(resPredBufC + (sb_off >> 1), cbuf);
                        bSADs += SAD4x4_16_ResPred(resPredBufC + 8 + (sb_off >> 1), cbuf+4);
                      }else{
                        bSADs += SATD4x4_16_ResPred(resPredBufC + (sb_off >> 1), cbuf);
                        bSADs += SATD4x4_16_ResPred(resPredBufC + 8 + (sb_off >> 1), cbuf+4);
                      }
                    } else {
                      if (core_enc->m_Analyse & ANALYSE_SAD) {
                          bSADs += H264ENC_MAKE_NAME(SAD)(pCurU, pitchPixels, cbuf, 16, chroma_block_size);
                          bSADs += H264ENC_MAKE_NAME(SAD)(pCurV, pitchPixels, cbuf+4, 16, chroma_block_size);
                      }else{
                          bSADs += H264ENC_MAKE_NAME(SATD)(pCurU, pitchPixels, cbuf, 16, chroma_block_size);
                          bSADs += H264ENC_MAKE_NAME(SATD)(pCurV, pitchPixels, cbuf+4, 16, chroma_block_size);
                      }
                    }
                }
                if (bSADs < BestSAD8x8[s]) {
                     Best8x8Type[s] = (MBTypeValue)SBTYPE_BIDIR_8x4;
                     BestSAD8x8[s] = bSADs;
                }

                //BiPred 4x8
                H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)( tmpBuf+128, pTmpBufL0+128, pTmpBufL1+128, 16, core_enc->use_implicit_weighted_bipred ? 2: 1,w1, w0, meInfo.block);
                if (resPredFlag) {
                  if (meFlags & ANALYSE_SAD)
                    bSADs = SAD8x8_16_ResPred(resPredBuf + sb_off, tmpBuf+128);
                  else
                    bSADs = SATD8x8_16_ResPred(resPredBuf + sb_off, tmpBuf+128);
                } else {
                  if (meFlags & ANALYSE_SAD)
                      bSADs = SAD8x8( pCur, pitchPixels, tmpBuf+128, 16);
                  else
                      bSADs = SATD8x8( pCur, pitchPixels, tmpBuf+128, 16);
                }
                bSADs += bSAD4x8Bi + BITS_COST(7, pRDQM);
                if(  meFlags & ANALYSE_ME_CHROMA ){
                    H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(cbufL0+8, cbufL1+8, cbuf, csize.width, csize.height, 16);
                    H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(cbufL0+64+8, cbufL1+64+8, cbuf+4, csize.width, csize.height, 16);
                    if (resPredFlag) {
                      if (core_enc->m_Analyse & ANALYSE_SAD) {
                        bSADs += SAD4x4_16_ResPred(resPredBufC + (sb_off >> 1), cbuf);
                        bSADs += SAD4x4_16_ResPred(resPredBufC + 8 + (sb_off >> 1), cbuf+4);
                      }else{
                        bSADs += SATD4x4_16_ResPred(resPredBufC + (sb_off >> 1), cbuf);
                        bSADs += SATD4x4_16_ResPred(resPredBufC + 8 + (sb_off >> 1), cbuf+4);
                      }
                    } else {
                      if (core_enc->m_Analyse & ANALYSE_SAD) {
                          bSADs += H264ENC_MAKE_NAME(SAD)(pCurU, pitchPixels, cbuf, 16, chroma_block_size);
                          bSADs += H264ENC_MAKE_NAME(SAD)(pCurV, pitchPixels, cbuf+4, 16, chroma_block_size);
                      }else{
                          bSADs += H264ENC_MAKE_NAME(SATD)(pCurU, pitchPixels, cbuf, 16, chroma_block_size);
                          bSADs += H264ENC_MAKE_NAME(SATD)(pCurV, pitchPixels, cbuf+4, 16, chroma_block_size);
                      }
                    }
                }

                if (bSADs < BestSAD8x8[s]) {
                     Best8x8Type[s] = (MBTypeValue)SBTYPE_BIDIR_4x8;
                     BestSAD8x8[s] = bSADs;
                }

                // set correct MVs for prediction in next subblocks
                bOff = block_subblock_mapping[s*4];
                sbt[s] = Best8x8Type[s];
                BestSAD8x8S += BestSAD8x8[s];
                if (Best8x8Type[s] <= SBTYPE_BIDIR_8x8 ){
                    if( Best8x8Type[s] != SBTYPE_DIRECT ){
                        mvsL0[bOff] = mvsL0[bOff+1] = mvsL0[bOff+4] = mvsL0[bOff+5] = BestMV8x8SL0[s];
                        mvsL1[bOff] = mvsL1[bOff+1] = mvsL1[bOff+4] = mvsL1[bOff+5] = BestMV8x8SL1[s];
                        refsL0[bOff] = refsL0[bOff+1] = refsL0[bOff+4] = refsL0[bOff+5] = BestRef8x8SL0[s];
                        refsL1[bOff] = refsL1[bOff+1] = refsL1[bOff+4] = refsL1[bOff+5] = BestRef8x8SL1[s];
                    }else{
                        mvsL0[bOff]   = mvs_direct8x8[LIST_0].MotionVectors[bOff];
                        mvsL0[bOff+1] = mvs_direct8x8[LIST_0].MotionVectors[bOff+1];
                        mvsL0[bOff+4] = mvs_direct8x8[LIST_0].MotionVectors[bOff+4];
                        mvsL0[bOff+5] = mvs_direct8x8[LIST_0].MotionVectors[bOff+5];
                        mvsL1[bOff]   = mvs_direct8x8[LIST_1].MotionVectors[bOff];
                        mvsL1[bOff+1] = mvs_direct8x8[LIST_1].MotionVectors[bOff+1];
                        mvsL1[bOff+4] = mvs_direct8x8[LIST_1].MotionVectors[bOff+4];
                        mvsL1[bOff+5] = mvs_direct8x8[LIST_1].MotionVectors[bOff+5];

                        refsL0[bOff]   = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff];
                        refsL0[bOff+1] = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff+1];
                        refsL0[bOff+4] = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff+4];
                        refsL0[bOff+5] = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff+5];
                        refsL1[bOff]   = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff];
                        refsL1[bOff+1] = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff+1];
                        refsL1[bOff+4] = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff+4];
                        refsL1[bOff+5] = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff+5];
                    }
                } else {
                    switch (  Best8x8Type[s] ){
                        case SBTYPE_FORWARD_8x4:
                            mvsL0[bOff+0] = mvsL0[bOff+1] = BestMV8x4L0[s][0];
                            mvsL0[bOff+4] = mvsL0[bOff+5] = BestMV8x4L0[s][1];
                            mvsL1[bOff+0] = mvsL1[bOff+1]= mvsL1[bOff+4]= mvsL1[bOff+5]= null_mv;
                            refsL0[bOff+0] = refsL0[bOff+1] = refsL0[bOff+4] = refsL0[bOff+5] = BestRef16x16L0;
                            refsL1[bOff+0] = refsL1[bOff+1]= refsL1[bOff+4]= refsL1[bOff+5] = -1;
                            break;
                        case SBTYPE_BACKWARD_8x4:
                            mvsL0[bOff+0] = mvsL0[bOff+1]= mvsL0[bOff+4]= mvsL0[bOff+5]= null_mv;
                            mvsL1[bOff+0] = mvsL1[bOff+1] = BestMV8x4L1[s][0];
                            mvsL1[bOff+4] = mvsL1[bOff+5] = BestMV8x4L1[s][1];
                            refsL1[bOff+0] = refsL1[bOff+1] = refsL1[bOff+4] = refsL1[bOff+5] = BestRef16x16L1;
                            refsL0[bOff+0] = refsL0[bOff+1]= refsL0[bOff+4]= refsL0[bOff+5]= -1;
                            break;
                        case SBTYPE_BIDIR_8x4:
                            mvsL0[bOff+0] = mvsL0[bOff+1] = BestMV8x4L0[s][0];
                            mvsL0[bOff+4] = mvsL0[bOff+5] = BestMV8x4L0[s][1];
                            mvsL1[bOff+0] = mvsL1[bOff+1] = BestMV8x4L1[s][0];
                            mvsL1[bOff+4] = mvsL1[bOff+5] = BestMV8x4L1[s][1];
                            refsL0[bOff+0] = refsL0[bOff+1] = refsL0[bOff+4] = refsL0[bOff+5] = BestRef16x16L0;
                            refsL1[bOff+0] = refsL1[bOff+1] = refsL1[bOff+4] = refsL1[bOff+5] = BestRef16x16L1;
                            Copy8x8( tmpBuf+8, 16, curr_slice->m_pTempBuff4DirectB+(s>>1)*8*16 + (s&1)*8, 16);
                            break;
                        case SBTYPE_FORWARD_4x8:
                            mvsL0[bOff+0] = mvsL0[bOff+4] = BestMV4x8L0[s][0];
                            mvsL0[bOff+1] = mvsL0[bOff+5] = BestMV4x8L0[s][1];
                            mvsL1[bOff+0] = mvsL1[bOff+1]= mvsL1[bOff+4]= mvsL1[bOff+5]= null_mv;
                            refsL0[bOff+0] = refsL0[bOff+4] = refsL0[bOff+1] = refsL0[bOff+5] = BestRef16x16L0;
                            refsL1[bOff+0] = refsL1[bOff+1]= refsL1[bOff+4]= refsL1[bOff+5]= -1;
                            break;
                        case SBTYPE_BACKWARD_4x8:
                            mvsL0[bOff+0] = mvsL0[bOff+1]= mvsL0[bOff+4]= mvsL0[bOff+5]= null_mv;
                            mvsL1[bOff+0] = mvsL1[bOff+4] = BestMV4x8L1[s][0];
                            mvsL1[bOff+1] = mvsL1[bOff+5] = BestMV4x8L1[s][1];
                            refsL0[bOff+0] = refsL0[bOff+1]= refsL0[bOff+4]= refsL0[bOff+5]= -1;
                            refsL1[bOff+0] = refsL1[bOff+1]= refsL1[bOff+4]= refsL1[bOff+5]= BestRef16x16L1;
                            break;
                        case SBTYPE_BIDIR_4x8:
                            mvsL0[bOff+0] = mvsL0[bOff+4] = BestMV4x8L0[s][0];
                            mvsL0[bOff+1] = mvsL0[bOff+5] = BestMV4x8L0[s][1];
                            mvsL1[bOff+0] = mvsL1[bOff+4] = BestMV4x8L1[s][0];
                            mvsL1[bOff+1] = mvsL1[bOff+5] = BestMV4x8L1[s][1];
                            refsL0[bOff+0] = refsL0[bOff+1]= refsL0[bOff+4]= refsL0[bOff+5]= BestRef16x16L0;
                            refsL1[bOff+0] = refsL1[bOff+1]= refsL1[bOff+4]= refsL1[bOff+5]= BestRef16x16L1;
                            Copy8x8( tmpBuf+128, 16, curr_slice->m_pTempBuff4DirectB+(s>>1)*8*16 + (s&1)*8, 16);
                            break;
                        case SBTYPE_FORWARD_4x4:
                            mvsL0[bOff+0] = BestMV4x4L0[s][0];
                            mvsL0[bOff+1] = BestMV4x4L0[s][1];
                            mvsL0[bOff+4] = BestMV4x4L0[s][2];
                            mvsL0[bOff+5] = BestMV4x4L0[s][3];
                            mvsL1[bOff+0] = mvsL1[bOff+1]= mvsL1[bOff+4]= mvsL1[bOff+5]= null_mv;
                            refsL0[bOff+0] = refsL0[bOff+1] = refsL0[bOff+4] = refsL0[bOff+5] = BestRef16x16L0;
                            refsL1[bOff+0] = refsL1[bOff+1] = refsL1[bOff+4] = refsL1[bOff+5] = -1;
                            break;
                        case SBTYPE_BACKWARD_4x4:
                            mvsL0[bOff+0] = mvsL0[bOff+1]= mvsL0[bOff+4]= mvsL0[bOff+5]= null_mv;
                            mvsL1[bOff+0] = BestMV4x4L1[s][0];
                            mvsL1[bOff+1] = BestMV4x4L1[s][1];
                            mvsL1[bOff+4] = BestMV4x4L1[s][2];
                            mvsL1[bOff+5] = BestMV4x4L1[s][3];
                            refsL0[bOff+0] = refsL0[bOff+1] = refsL0[bOff+4] = refsL0[bOff+5] = -1;
                            refsL1[bOff+0] = refsL1[bOff+1] = refsL1[bOff+4] = refsL1[bOff+5] = BestRef16x16L1;
                            break;
                        case SBTYPE_BIDIR_4x4:
                            mvsL0[bOff+0] = BestMV4x4L0[s][0];
                            mvsL0[bOff+1] = BestMV4x4L0[s][1];
                            mvsL0[bOff+4] = BestMV4x4L0[s][2];
                            mvsL0[bOff+5] = BestMV4x4L0[s][3];

                            mvsL1[bOff+0] = BestMV4x4L1[s][0];
                            mvsL1[bOff+1] = BestMV4x4L1[s][1];
                            mvsL1[bOff+4] = BestMV4x4L1[s][2];
                            mvsL1[bOff+5] = BestMV4x4L1[s][3];
                            refsL0[bOff+0] = refsL0[bOff+1] = refsL0[bOff+4] = refsL0[bOff+5] = BestRef16x16L0;
                            refsL1[bOff+0] = refsL1[bOff+1] = refsL1[bOff+4] = refsL1[bOff+5] = BestRef16x16L1;
                            Copy8x8( tmpBuf, 16, curr_slice->m_pTempBuff4DirectB+(s>>1)*8*16 + (s&1)*8, 16);
                            break;
                    }
                }
            }

            for( b = 0; b<4; b++ ){
                bOff = block_subblock_mapping[b*4];
                BestMV4x4L0[b][0] = mvsL0[bOff];
                BestMV4x4L0[b][1] = mvsL0[bOff+1];
                BestMV4x4L0[b][2] = mvsL0[bOff+4];
                BestMV4x4L0[b][3] = mvsL0[bOff+5];
                BestMV4x4L1[b][0] = mvsL1[bOff];
                BestMV4x4L1[b][1] = mvsL1[bOff+1];
                BestMV4x4L1[b][2] = mvsL1[bOff+4];
                BestMV4x4L1[b][3] = mvsL1[bOff+5];

                BestRef4x4L0[b][0] = refsL0[bOff];
                BestRef4x4L0[b][1] = refsL0[bOff+1];
                BestRef4x4L0[b][2] = refsL0[bOff+4];
                BestRef4x4L0[b][3] = refsL0[bOff+5];
                BestRef4x4L1[b][0] = refsL1[bOff];
                BestRef4x4L1[b][1] = refsL1[bOff+1];
                BestRef4x4L1[b][2] = refsL1[bOff+4];
                BestRef4x4L1[b][3] = refsL1[bOff+5];
                Best8x8Type[b] = sbt[b];
            }
        }

        //Analyse 8x16 and 16x8
        if( BestSAD8x8S < BestSAD16x16 + (Ipp32s)BITS_COST(8, pRDQM) && (BestSAD8x8S > check_threshold) ){
            Ipp32s typeOffset[3][4] = {{0,4,12,-1},{6,2,14,-1},{8,10,16,-1}};
            Ipp32s typeCost[3][4] = {{5,7,7,-1},{7,5,7,-1},{9,9,9,-1}};

            //TODO do we need to check if partition sizes are equal?
            meInfo.threshold = (meFlags & ANALYSE_ME_EARLY_EXIT) ? core_enc->m_BestOf5EarlyExitThres[iQP] >> 1 : 0;
            //------ 16x8
            BestSAD16x8 = 0;
            if(  meFlags & ANALYSE_ME_CHROMA ){
                    if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                        chroma_size =  size8x4;
                    else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                        chroma_size =  size16x8;
            }

            for (b = 0; b < 2; b++ ) {
                Ipp32s sb_off = b * 8 * 16;
                meInfo.block.width = 16;
                meInfo.block.height = 8;
                meInfo.threshold = 0;
                bOff = b * 8;
                meInfo.pCur = cur_mb.mbPtr + b * 8 * pitchPixels;

                meInfo.pResY = resPredBuf + sb_off;
                meInfo.pResU = resPredBufC + (sb_off >> 1);
                meInfo.pResV = resPredBufC + 8 + (sb_off >> 1);
#ifdef NO_PADDING
                meInfo.interpolateInfo.pointBlockPos.x = mbPos.x;
                meInfo.interpolateInfo.pointBlockPos.y = mbPos.y + b * 8;
#endif // NO_PADDING
                if(  meFlags & ANALYSE_ME_CHROMA ){
                    meInfo.pCurU = pMBCurU + b * 4 * pitchPixels;
                    meInfo.pCurV = pMBCurV + b * 4 * pitchPixels;
                }
                //**** L0 search
//                for (ref_idx = 0; ref_idx < numRef; ref_idx ++) {
                    ref_idx = BestRef16x16L0;
                    refsL0[bOff] = ref_idx;
                    H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 4, 2, &(meInfo));
                    ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_RefPicList;
                    meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields0[ref_idx]] + b * 8 * pitchPixels;
#ifdef NO_PADDING
                    meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    if(  meFlags & ANALYSE_ME_CHROMA ){
                        meInfo.chroma_mvy_offset = 0;
                        if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                            if (!curr_slice->m_is_cur_mb_bottom_field && pFields0[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                            else if (curr_slice->m_is_cur_mb_bottom_field && !pFields0[ref_idx]) meInfo.chroma_mvy_offset += 2;
                        }
                        meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + b * 4 * pitchPixels;
                        meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + b * 4 * pitchPixels;
#ifdef NO_PADDING
                        meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
                        meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    }
                    meInfo.candNum = 0;
                    meInfo.bestSAD = MAX_SAD;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8L0R[2*b]);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8L0R[2*b+1]);
                    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                    if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCostL0[ref_idx] > meInfo.threshold)
                        H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
//                    if (meInfo.bestSAD + refCost[ref_idx] < bSAD16x8[b]) {
                        BestSAD16x8L0 = meInfo.bestSAD + refCostL0[ref_idx];
                        BestMV16x8L0 = meInfo.bestMV;
                        BestRef16x8L0 = ref_idx;
                        //PredMV16x8L0 = meInfo.predictedMV;
                        BestPredMV16x8L0[b] = meInfo.bestPredMV;
                        mpred16x8L0[b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
//                    }
#ifdef NO_PADDING
                    pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
                    pInterpolateInfo->pDst[0] = pTmpBufL0;
                    pInterpolateInfo->sizeBlock = meInfo.block;
                    pInterpolateInfo->pointVector.x = BestMV16x8L0.mvx;
                    pInterpolateInfo->pointVector.y = BestMV16x8L0.mvy;
                    pInterpolateInfo->pointBlockPos.x = meInfo.interpolateInfo.pointBlockPos.x;
                    pInterpolateInfo->pointBlockPos.y = meInfo.interpolateInfo.pointBlockPos.y;
                    H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, BestMV16x8L0.mvx >> SUB_PEL_SHIFT, BestMV16x8L0.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                    pTmpBufL0, 16, BestMV16x8L0.mvx&3, BestMV16x8L0.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
                if(  meFlags & ANALYSE_ME_CHROMA ){
                    chroma_vec.mvx = BestMV16x16L0.mvx;
                    chroma_vec.mvy = BestMV16x16L0.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                    pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                    pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                    pInterpolateInfoChroma->pDst[0] = pTmpBufL0+8*16;
                    pInterpolateInfoChroma->pDst[1] = pTmpBufL0+8*16 + 8;
                    if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                    {
                        pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                        pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                        pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                        pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                    }
                    else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                    {
                        pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                        pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                        pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                        pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                    }
                    pInterpolateInfoChroma->sizeBlock = chroma_size;
                    H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                    offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                    H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL0+8*16, pTmpBufL0+8*16+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#else // USE_NV12
                    H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL0+8*16, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
                    H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, pTmpBufL0+8*16+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
                }

                //**** L1 search
//                for (ref_idx = 0; ref_idx < numRef; ref_idx ++) {
                    ref_idx = BestRef16x16L1;
                    refsL1[bOff] = ref_idx;
                    H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_1, 4, 2, &(meInfo));
                    ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, is_cur_mb_field, uMB & 1)->m_RefPicList;
                    meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields1[ref_idx]] + b * 8 * pitchPixels;
#ifdef NO_PADDING
                    meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    if(  meFlags & ANALYSE_ME_CHROMA ){
                        meInfo.chroma_mvy_offset = 0;
                        if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                            if (!curr_slice->m_is_cur_mb_bottom_field && pFields0[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                            else if (curr_slice->m_is_cur_mb_bottom_field && !pFields0[ref_idx]) meInfo.chroma_mvy_offset += 2;
                        }
                        meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + b * 4 * pitchPixels;
                        meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + b * 4 * pitchPixels;
#ifdef NO_PADDING
                        meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
                        meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    }
                    meInfo.candNum = 0;
                    meInfo.bestSAD = MAX_SAD;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8L1R[2*b]);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8L1R[2*b+1]);
                    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                    if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCostL1[ref_idx] > meInfo.threshold)
                        H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
//                    if (meInfo.bestSAD + refCost[ref_idx] < bSAD16x8[b]) {
                        BestSAD16x8L1 = meInfo.bestSAD + refCostL1[ref_idx];
                        BestMV16x8L1 = meInfo.bestMV;
                        BestRef16x8L1 = ref_idx;
                        //PredMV16x8L1 = meInfo.predictedMV;
                        BestPredMV16x8L1[b] = meInfo.bestPredMV;
                        mpred16x8L1[b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
//                    }
#ifdef NO_PADDING
                        pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
                        pInterpolateInfo->pDst[0] = pTmpBufL1;
                        pInterpolateInfo->sizeBlock = meInfo.block;
                        pInterpolateInfo->pointVector.x = BestMV16x8L1.mvx;
                        pInterpolateInfo->pointVector.y = BestMV16x8L1.mvy;
                        pInterpolateInfo->pointBlockPos.x = meInfo.interpolateInfo.pointBlockPos.x;
                        pInterpolateInfo->pointBlockPos.y = meInfo.interpolateInfo.pointBlockPos.y;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, BestMV16x8L1.mvx >> SUB_PEL_SHIFT, BestMV16x8L1.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                    pTmpBufL1, 16, BestMV16x8L1.mvx&3, BestMV16x8L1.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
                if(  meFlags & ANALYSE_ME_CHROMA ){
                   chroma_vec.mvx = BestMV16x16L1.mvx;
                    chroma_vec.mvy = BestMV16x16L1.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                    pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                    pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                    pInterpolateInfoChroma->pDst[0] = pTmpBufL1+8*16;
                    pInterpolateInfoChroma->pDst[1] = pTmpBufL1+8*16 + 8;
                    if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                    {
                        pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                        pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                        pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                        pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                    }
                    else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                    {
                        pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                        pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                        pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                        pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                    }
                    pInterpolateInfoChroma->sizeBlock = chroma_size;
                    H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                    offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                    H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL1+8*16, pTmpBufL1+8*16+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#else // USE_NV12
                    H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL1+8*16, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
                    H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, pTmpBufL1+8*16+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
                }

            //Try weighted prediction
                w1 = curr_slice->DistScaleFactor[BestRef16x8L0][BestRef16x8L1]>>2;
                w0 = 64-w1;
                H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)( bidir16x8+b*16*8, pTmpBufL0, pTmpBufL1, 16, core_enc->use_implicit_weighted_bipred ? 2: 1,w1, w0, meInfo.block);

                // currently check only rpred on OR off for 16x8 and 8x16
                if (resPredFlag) {
                  if (meFlags & ANALYSE_SAD)
                    BestSAD16x8Bi = SAD16x8_16_ResPred(meInfo.pResY, bidir16x8 + sb_off);
                  else
                    BestSAD16x8Bi = SATD16x8_16_ResPred(meInfo.pResY, bidir16x8 + sb_off);
                } else {
                  if (meFlags & ANALYSE_SAD)
                      BestSAD16x8Bi = SAD16x8(meInfo.pCur, pitchPixels, bidir16x8+b*16*8, 16);
                  else
                      BestSAD16x8Bi = SATD16x8(meInfo.pCur, pitchPixels, bidir16x8+b*16*8, 16);
                }

                BestSAD16x8Bi += MVConstraint( BestMV16x8L0.mvx - BestPredMV16x8L0[b].mvx, BestMV16x8L0.mvy - BestPredMV16x8L0[b].mvy, pRDQM);
                BestSAD16x8Bi += MVConstraint( BestMV16x8L1.mvx - BestPredMV16x8L1[b].mvx, BestMV16x8L1.mvy - BestPredMV16x8L1[b].mvy, pRDQM)+
                                 refCostL0[BestRef16x8L0] +
                                 refCostL1[BestRef16x8L1];
                if(  meFlags & ANALYSE_ME_CHROMA ){
                    H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(pTmpBufL0+16*8, pTmpBufL1+16*8, pTmpChromaPred, chroma_size.width, chroma_size.height, 16);
                    H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(pTmpBufL0+8+16*8, pTmpBufL1+8+16*8, pTmpChromaPred+8, chroma_size.width, chroma_size.height, 16);
                    Ipp32s chroma_block_size = chroma_size.width + (chroma_size.height >> 2);
                    if ( meFlags & ANALYSE_ME_BIDIR_REFINE )
                    {
                        if (!resPredFlag)
                        {
                            if (core_enc->m_Analyse & ANALYSE_SAD) {
                                chromaContrToSAD[b + SHIFT_16x8] += H264ENC_MAKE_NAME(SAD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                                chromaContrToSAD[b + SHIFT_16x8] += H264ENC_MAKE_NAME(SAD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                                BestSAD16x8Bi += chromaContrToSAD[b + SHIFT_16x8];

                            }else{
                                chromaContrToSAD[b + SHIFT_16x8] += H264ENC_MAKE_NAME(SATD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                                chromaContrToSAD[b + SHIFT_16x8] += H264ENC_MAKE_NAME(SATD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                                BestSAD16x8Bi += chromaContrToSAD[b + SHIFT_16x8];
                            }
                        }
                        else
                        {
                            if (core_enc->m_Analyse & ANALYSE_SAD) {
                                chromaContrToSAD[b + SHIFT_16x8] = SAD_ResPred(meInfo.pResU, 16, pTmpChromaPred, 16, chroma_block_size);
                                chromaContrToSAD[b + SHIFT_16x8] += SAD_ResPred(meInfo.pResV, 16, pTmpChromaPred+8, 16, chroma_block_size);
                                BestSAD16x8Bi += chromaContrToSAD[b + SHIFT_16x8];
                            } else{
                                chromaContrToSAD[b + SHIFT_16x8] = SATD_ResPred(meInfo.pResU, 16, pTmpChromaPred, 16, chroma_block_size);
                                chromaContrToSAD[b + SHIFT_16x8] += SATD_ResPred(meInfo.pResV, 16, pTmpChromaPred+8, 16, chroma_block_size);
                                BestSAD16x8Bi += chromaContrToSAD[b + SHIFT_16x8];
                            }
                        }
                    }
                    else
                    {
                        if (!resPredFlag)
                        {
                            if (core_enc->m_Analyse & ANALYSE_SAD) {
                                BestSAD16x8Bi += H264ENC_MAKE_NAME(SAD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                                BestSAD16x8Bi += H264ENC_MAKE_NAME(SAD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                            }else{
                                BestSAD16x8Bi += H264ENC_MAKE_NAME(SATD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                                BestSAD16x8Bi += H264ENC_MAKE_NAME(SATD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                            }
                        }
                        else
                        {
                            if (core_enc->m_Analyse & ANALYSE_SAD) {
                                BestSAD16x8Bi += SAD_ResPred(meInfo.pResU, 16, pTmpChromaPred, 16, chroma_block_size);
                                BestSAD16x8Bi += SAD_ResPred(meInfo.pResV, 16, pTmpChromaPred+8, 16, chroma_block_size);
                            } else{
                                BestSAD16x8Bi += SATD_ResPred(meInfo.pResU, 16, pTmpChromaPred, 16, chroma_block_size);
                                BestSAD16x8Bi += SATD_ResPred(meInfo.pResV, 16, pTmpChromaPred+8, 16, chroma_block_size);
                            }
                        }
                    }
                }

                Ipp32s BestSAD16x8T = BestSAD16x8L0;
                Ipp32s BestPred16x8T = 0;
                if( BestSAD16x8L1 < BestSAD16x8T ){
                    BestSAD16x8T = BestSAD16x8L1;
                    BestPred16x8T = 1;
                }
                if( BestSAD16x8Bi < BestSAD16x8T ){
                    BestSAD16x8T = BestSAD16x8Bi;
                    BestPred16x8T = 2;
                    if ( meFlags & ANALYSE_ME_BIDIR_REFINE )
                    {
                        bestSADByMBtype[b + SHIFT_16x8] = BestSAD16x8Bi;// - chromaContrToSAD[b + SHIFT_16x8];
                        predBidirRefineL0[b + SHIFT_16x8] = BestPredMV16x8L0[b];
                        predBidirRefineL1[b + SHIFT_16x8] = BestPredMV16x8L1[b];
                    }
                }

                BestSAD16x8 += BestSAD16x8T;
                BestPred16x8[b] = BestPred16x8T;
                switch( BestPred16x8T ){
                    case 0:
                        for (i = 0; i < 8; i ++) {
                            mvsL0[bOff+i] = BestMV16x8L0;
                            refsL0[bOff+i] = BestRef16x8L0;
                            mvsL1[bOff+i] = null_mv;
                            refsL1[bOff+i] = -1;
                        }
                        break;
                    case 1:
                        for (i = 0; i < 8; i ++) {
                            mvsL0[bOff+i] = null_mv;
                            refsL0[bOff+i] = -1;
                            mvsL1[bOff+i] = BestMV16x8L1;
                            refsL1[bOff+i] = BestRef16x8L1;
                        }
                        break;
                    case 2:
                        for (i = 0; i < 8; i ++) {
                            mvsL0[bOff+i] = BestMV16x8L0;
                            refsL0[bOff+i] = BestRef16x8L0;
                            mvsL1[bOff+i] = BestMV16x8L1;
                            refsL1[bOff+i] = BestRef16x8L1;
                        }
                        break;
                }
                BestMV16x8SL0[b] = mvsL0[bOff];
                BestMV16x8SL1[b] = mvsL1[bOff];
                BestRef16x8SL0[b] = refsL0[bOff];
                BestRef16x8SL1[b] = refsL1[bOff];
            }

            Best16x8Type = MBTYPE_FWD_FWD_16x8 + typeOffset[BestPred16x8[0]][BestPred16x8[1]];
            BestSAD16x8 += BITS_COST(typeCost[BestPred16x8[0]][BestPred16x8[1]], pRDQM);

            if (ModeRDOpt /* && (BestSAD16x8 <= (BestSAD16x16 * SB_THRESH_RD)) */) {
                curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = Best16x8Type;
                MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir16x8, 256*sizeof(PIXTYPE)); //copy buffer for mc
#ifdef USE_NV12
                RDCost16x8 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
                RDCost16x8 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 0 );
#endif // USE_NV12
            }

            //------ 8x16
            BestSAD8x16 = 0;
            if(  meFlags & ANALYSE_ME_CHROMA ){
                if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                    chroma_size =  size4x8;
                else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                    chroma_size =  size8x16;
            }
            for (b = 0; b < 2; b ++) {
                Ipp32s sb_off = b * 8;

                meInfo.block.width  = 8;
                meInfo.block.height = 16;
                meInfo.threshold = 0;
                bOff = b * 2;
                meInfo.pCur = cur_mb.mbPtr + b * 8;

                meInfo.pResY = resPredBuf + sb_off;
                meInfo.pResU = resPredBufC + (sb_off >> 1);
                meInfo.pResV = resPredBufC + 8 + (sb_off >> 1);

#ifdef NO_PADDING
                meInfo.interpolateInfo.pointBlockPos.x = mbPos.x + b * 8;
                meInfo.interpolateInfo.pointBlockPos.y = mbPos.y;
#endif // NO_PADDING
                if(  meFlags & ANALYSE_ME_CHROMA ){
                    meInfo.pCurU = pMBCurU + b * 4;
                    meInfo.pCurV = pMBCurV + b * 4;
                }
                //**** L0 search
//                for (ref_idx = 0; ref_idx < numRef; ref_idx ++) {
                    ref_idx = BestRef16x16L0;
                    refsL0[bOff] = ref_idx;
                    H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_0, 2, 4, &(meInfo));
                    ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_RefPicList;
                    meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields0[ref_idx]] + b * 8;
#ifdef NO_PADDING
                    meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    if(  meFlags & ANALYSE_ME_CHROMA ){
                        meInfo.chroma_mvy_offset = 0;
                        if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                            if (!curr_slice->m_is_cur_mb_bottom_field && pFields0[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                            else if (curr_slice->m_is_cur_mb_bottom_field && !pFields0[ref_idx]) meInfo.chroma_mvy_offset += 2;
                        }
#ifdef USE_NV12
                        meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + b * 8;
                        meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                        meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + b * 4;
                        meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields0[ref_idx]] + b * 4;
#endif // USE_NV12
#ifdef NO_PADDING
                        meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
                        meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields0[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    }
                    meInfo.candNum = 0;
                    meInfo.bestSAD = MAX_SAD;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8L0R[b]);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8L0R[b+2]);
                    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                    if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCostL0[ref_idx] > meInfo.threshold)
                        H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
//                    if (meInfo.bestSAD + refCost[ref_idx] < bSAD16x8[b]) {
                        BestSAD8x16L0 = meInfo.bestSAD + refCostL0[ref_idx];
                        BestMV8x16L0 = meInfo.bestMV;
                        BestRef8x16L0 = ref_idx;
                        //PredMV8x16L0 = meInfo.predictedMV;
                        BestPredMV8x16L0[b] = meInfo.bestPredMV;
                        mpred8x16L0[b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
//                    }
#ifdef NO_PADDING
                        pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
                        pInterpolateInfo->pDst[0] = pTmpBufL0;
                        pInterpolateInfo->sizeBlock = meInfo.block;
                        pInterpolateInfo->pointVector.x = BestMV8x16L0.mvx;
                        pInterpolateInfo->pointVector.y = BestMV8x16L0.mvy;
                        pInterpolateInfo->pointBlockPos.x = meInfo.interpolateInfo.pointBlockPos.x;
                        pInterpolateInfo->pointBlockPos.y = meInfo.interpolateInfo.pointBlockPos.y;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, BestMV8x16L0.mvx >> SUB_PEL_SHIFT, BestMV8x16L0.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                    pTmpBufL0, 16, BestMV8x16L0.mvx&3, BestMV8x16L0.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
                if(  meFlags & ANALYSE_ME_CHROMA ){
                    chroma_vec.mvx = BestMV16x16L0.mvx;
                    chroma_vec.mvy = BestMV16x16L0.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                    pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                    pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                    pInterpolateInfoChroma->pDst[0] = pTmpBufL0+8;
                    pInterpolateInfoChroma->pDst[1] = pTmpBufL0+8+4;
                    if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                    {
                        pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                        pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                        pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                        pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                    }
                    else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                    {
                        pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                        pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                        pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                        pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                    }
                    pInterpolateInfoChroma->sizeBlock = chroma_size;
                    H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                    offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                    H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL0+8, pTmpBufL0+8+4, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#else // USE_NV12
                    H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL0+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
                    H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, pTmpBufL0+8+4, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
                }

                //**** L1 search
//                for (ref_idx = 0; ref_idx < numRef; ref_idx ++) {
                    ref_idx = BestRef16x16L1;
                    refsL1[bOff] = ref_idx;
                    H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, bOff, LIST_1, 2, 4, &(meInfo));
                    ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, is_cur_mb_field, uMB & 1)->m_RefPicList;
                    meInfo.pRef = ppRefPicList[ref_idx]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields1[ref_idx]] + b * 8;
#ifdef NO_PADDING
                    meInfo.interpolateInfo.pSrc[0] = ppRefPicList[ref_idx]->m_pYPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    if(  meFlags & ANALYSE_ME_CHROMA ){
                        meInfo.chroma_mvy_offset = 0;
                        if( core_enc->m_PicParamSet->chroma_format_idc == 1 ){
                            if (!curr_slice->m_is_cur_mb_bottom_field && pFields0[ref_idx]) meInfo.chroma_mvy_offset += - 2;
                            else if (curr_slice->m_is_cur_mb_bottom_field && !pFields0[ref_idx]) meInfo.chroma_mvy_offset += 2;
                       }
#ifdef USE_NV12
                        meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + b * 8;
                        meInfo.pRefV = meInfo.pRefU + 1;
#else // USE_NV12
                        meInfo.pRefU = ppRefPicList[ref_idx]->m_pUPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + b * 4;
                        meInfo.pRefV = ppRefPicList[ref_idx]->m_pVPlane + uChromaOffset + curr_slice->m_InitialOffset[pFields1[ref_idx]] + b * 4;
#endif // USE_NV12
#ifdef NO_PADDING
                        meInfo.interpolateInfoChroma.pSrc[0] = ppRefPicList[ref_idx]->m_pUPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
                        meInfo.interpolateInfoChroma.pSrc[1] = ppRefPicList[ref_idx]->m_pVPlane + curr_slice->m_InitialOffset[pFields1[ref_idx]] + offsetIfBottomField;
#endif // NO_PADDING
                    }
                    meInfo.candNum = 0;
                    meInfo.bestSAD = MAX_SAD;
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8L1R[b]);
                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, BestMV8x8L1R[b+2]);
                    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                    if (meFlags & ANALYSE_ME_SUBPEL && meInfo.bestSAD+refCostL1[ref_idx] > meInfo.threshold)
                        H264ENC_MAKE_NAME(ME_SubPel)(&meInfo);
//                    if (meInfo.bestSAD + refCost[ref_idx] < bSAD16x8[b]) {
                        BestSAD8x16L1 = meInfo.bestSAD + refCostL1[ref_idx];
                        BestMV8x16L1 = meInfo.bestMV;
                        BestRef8x16L1 = ref_idx;
                        //PredMV8x16L1 = meInfo.predictedMV;
                        BestPredMV8x16L1[b] = meInfo.bestPredMV;
                        mpred8x16L1[b] = meInfo.useScPredMVAlways || (meInfo.useScPredMV && meInfo.bestPredMV == meInfo.scPredMV);
//                    }
#ifdef NO_PADDING
                        pInterpolateInfo->pSrc[0] = meInfo.interpolateInfo.pSrc[0];
                        pInterpolateInfo->pDst[0] = pTmpBufL1;
                        pInterpolateInfo->sizeBlock = meInfo.block;
                        pInterpolateInfo->pointVector.x = BestMV8x16L1.mvx;
                        pInterpolateInfo->pointVector.y = BestMV8x16L1.mvy;
                        pInterpolateInfo->pointBlockPos.x = meInfo.interpolateInfo.pointBlockPos.x;
                        pInterpolateInfo->pointBlockPos.y = meInfo.interpolateInfo.pointBlockPos.y;
                        H264ENC_MAKE_NAME(ippiInterpolateLumaBlock_H264)(pInterpolateInfo);
#else // NO_PADDING
                    H264ENC_MAKE_NAME(InterpolateLuma)( MVADJUST( meInfo.pRef, pitchPixels, BestMV8x16L1.mvx >> SUB_PEL_SHIFT, BestMV8x16L1.mvy >> SUB_PEL_SHIFT ), pitchPixels,
                    pTmpBufL1, 16, BestMV8x16L1.mvx&3, BestMV8x16L1.mvy&3, meInfo.block, core_enc->m_PicParamSet->bit_depth_luma, planeSize, 0, 0);
#endif // NO_PADDING
                    if(  meFlags & ANALYSE_ME_CHROMA ){
                       chroma_vec.mvx = BestMV16x16L1.mvx;
                        chroma_vec.mvy = BestMV16x16L1.mvy + meInfo.chroma_mvy_offset;
#ifdef NO_PADDING
                        pInterpolateInfoChroma->pSrc[0] = meInfo.interpolateInfoChroma.pSrc[0];
                        pInterpolateInfoChroma->pSrc[1] = meInfo.interpolateInfoChroma.pSrc[1];
                        pInterpolateInfoChroma->pDst[0] = pTmpBufL1+8;
                        pInterpolateInfoChroma->pDst[1] = pTmpBufL1+8+4;
                        if( core_enc->m_PicParamSet->chroma_format_idc == 1) //420
                        {
                            pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                            pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y >> 1;
                            pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                            pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy;
                        }
                        else if( core_enc->m_PicParamSet->chroma_format_idc == 2) //422
                        {
                            pInterpolateInfoChroma->pointBlockPos.x = pInterpolateInfo->pointBlockPos.x >> 1;
                            pInterpolateInfoChroma->pointBlockPos.y = pInterpolateInfo->pointBlockPos.y;
                            pInterpolateInfoChroma->pointVector.x = chroma_vec.mvx;
                            pInterpolateInfoChroma->pointVector.y = chroma_vec.mvy << 1;
                        }
                        pInterpolateInfoChroma->sizeBlock = chroma_size;
                        H264ENC_MAKE_NAME(ippiInterpolateChromaBlock_H264)(pInterpolateInfoChroma);
#else // NO_PADDING
                        offset = SubpelChromaMVAdjust(&chroma_vec, pitchPixels, iXType, iYType, meInfo.chroma_format_idc);
#ifdef USE_NV12
                        H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL1+8, pTmpBufL1+8+4, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#else // USE_NV12
                        H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefU+offset, pitchPixels, pTmpBufL1+8, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
                        H264ENC_MAKE_NAME(ippiInterpolateChroma_H264)(meInfo.pRefV+offset, pitchPixels, pTmpBufL1+8+4, 16, iXType, iYType, chroma_size, meInfo.bit_depth_chroma);
#endif // USE_NV12
#endif // NO_PADDING
                    }

                //Try weighted prediction
                    w1 = curr_slice->DistScaleFactor[BestRef8x16L0][BestRef8x16L1] >> 2;
                    w0 = 64-w1;
                    H264ENC_MAKE_NAME(DirectB_PredictOneMB_Lu)( bidir8x16+b*8, pTmpBufL0, pTmpBufL1, 16, core_enc->use_implicit_weighted_bipred ? 2: 1,w1, w0, meInfo.block);

                    if (resPredFlag) {
                      if (meFlags & ANALYSE_SAD)
                        BestSAD8x16Bi = SAD8x16_16_ResPred(meInfo.pResY, bidir8x16 + sb_off);
                      else
                        BestSAD8x16Bi = SATD8x16_16_ResPred(meInfo.pResY, bidir8x16 + sb_off);
                    } else {
                        if (meFlags & ANALYSE_SAD)
                            BestSAD8x16Bi = SAD8x16(meInfo.pCur, pitchPixels,  bidir8x16+sb_off, 16);
                        else
                            BestSAD8x16Bi = SATD8x16(meInfo.pCur, pitchPixels,  bidir8x16+sb_off, 16);
                    }

                    BestSAD8x16Bi += MVConstraint( BestMV8x16L0.mvx - BestPredMV16x8L0[b].mvx, BestMV8x16L0.mvy - BestPredMV16x8L0[b].mvy, pRDQM);
                    BestSAD8x16Bi += MVConstraint( BestMV8x16L1.mvx - BestPredMV16x8L1[b].mvx, BestMV8x16L1.mvy - BestPredMV16x8L1[b].mvy, pRDQM)+
                                     refCostL0[BestRef8x16L0] +
                                     refCostL1[BestRef8x16L1];

                    if(  meFlags & ANALYSE_ME_CHROMA ){
                      H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(pTmpBufL0+8, pTmpBufL1+8, pTmpChromaPred, chroma_size.width, chroma_size.height, 16);
                      H264ENC_MAKE_NAME(ippiInterpolateBlock_H264)(pTmpBufL0+8+4, pTmpBufL1+8+4, pTmpChromaPred+8, chroma_size.width, chroma_size.height, 16);
                      Ipp32s chroma_block_size = chroma_size.width + (chroma_size.height >> 2);
                      if ( meFlags & ANALYSE_ME_BIDIR_REFINE )
                      {
                          if (!resPredFlag)
                          {
                              if (core_enc->m_Analyse & ANALYSE_SAD) {
                                  chromaContrToSAD[b + SHIFT_8x16] += H264ENC_MAKE_NAME(SAD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                                  chromaContrToSAD[b + SHIFT_8x16] += H264ENC_MAKE_NAME(SAD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                                  BestSAD8x16Bi += chromaContrToSAD[b + SHIFT_8x16];
                              }else{
                                  chromaContrToSAD[b + SHIFT_8x16] += H264ENC_MAKE_NAME(SATD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                                  chromaContrToSAD[b + SHIFT_8x16] += H264ENC_MAKE_NAME(SATD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                                  BestSAD8x16Bi += chromaContrToSAD[b + SHIFT_8x16];
                              }
                          }
                          else
                          {
                              if (core_enc->m_Analyse & ANALYSE_SAD) {
                                  chromaContrToSAD[b + SHIFT_8x16] = SAD_ResPred(meInfo.pResU, 16, pTmpChromaPred, 16, chroma_block_size);
                                  chromaContrToSAD[b + SHIFT_8x16] += SAD_ResPred(meInfo.pResV, 16, pTmpChromaPred+8, 16, chroma_block_size);
                                  BestSAD8x16Bi += chromaContrToSAD[b + SHIFT_8x16];
                              } else{
                                  chromaContrToSAD[b + SHIFT_8x16] = SATD_ResPred(meInfo.pResU, 16, pTmpChromaPred, 16, chroma_block_size);
                                  chromaContrToSAD[b + SHIFT_8x16] += SATD_ResPred(meInfo.pResV, 16, pTmpChromaPred+8, 16, chroma_block_size);
                                  BestSAD8x16Bi += chromaContrToSAD[b + SHIFT_8x16];
                              }
                          }
                      }
                      else
                      {
                          if (!resPredFlag)
                          {
                              if (core_enc->m_Analyse & ANALYSE_SAD) {
                                  BestSAD8x16Bi += H264ENC_MAKE_NAME(SAD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                                  BestSAD8x16Bi += H264ENC_MAKE_NAME(SAD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                              }else{
                                  BestSAD8x16Bi += H264ENC_MAKE_NAME(SATD)(meInfo.pCurU, pitchPixels, pTmpChromaPred, 16, chroma_block_size);
                                  BestSAD8x16Bi += H264ENC_MAKE_NAME(SATD)(meInfo.pCurV, pitchPixels, pTmpChromaPred+8, 16, chroma_block_size);
                              }
                          }
                          else
                          {
                              if (core_enc->m_Analyse & ANALYSE_SAD) {
                                  BestSAD8x16Bi += SAD_ResPred(meInfo.pResU, 16, pTmpChromaPred, 16, chroma_block_size);
                                  BestSAD8x16Bi += SAD_ResPred(meInfo.pResV, 16, pTmpChromaPred+8, 16, chroma_block_size);
                              } else{
                                  BestSAD8x16Bi += SATD_ResPred(meInfo.pResU, 16, pTmpChromaPred, 16, chroma_block_size);
                                  BestSAD8x16Bi += SATD_ResPred(meInfo.pResV, 16, pTmpChromaPred+8, 16, chroma_block_size);
                              }
                          }
                      }
                    }

                    Ipp32s BestSAD8x16T = BestSAD8x16L0;
                    Ipp32s BestPred8x16T = 0;
                    if( BestSAD8x16L1 < BestSAD8x16T ){
                        BestSAD8x16T = BestSAD8x16L1;
                        BestPred8x16T = 1;
                    }
                    if( BestSAD8x16Bi < BestSAD8x16T ){
                        BestSAD8x16T = BestSAD8x16Bi;
                        BestPred8x16T = 2;
                        if ( meFlags & ANALYSE_ME_BIDIR_REFINE )
                        {
                            bestSADByMBtype[b + SHIFT_8x16] = BestSAD8x16Bi;// - chromaContrToSAD[b + SHIFT_8x16];
                            predBidirRefineL0[b + SHIFT_8x16] = BestPredMV8x16L0[b];
                            predBidirRefineL1[b + SHIFT_8x16] = BestPredMV8x16L1[b];
                        }
                    }

                    BestSAD8x16 += BestSAD8x16T;
                    BestPred8x16[b] = BestPred8x16T;
                    switch( BestPred8x16T ){
                        case 0:
                            for ( i = 0; i < 4; i ++) {
                                mvsL0[bOff+i*4] = mvsL0[bOff+i*4+1] = BestMV8x16L0;
                                refsL0[bOff+i*4] = refsL0[bOff+i*4+1] = BestRef8x16L0;
                                mvsL1[bOff+i*4] = mvsL1[bOff+i*4+1] = null_mv;
                                refsL1[bOff+i*4] = refsL1[bOff+i*4+1] = -1;
                            }
                            break;
                        case 1:
                            for ( i = 0; i < 4; i ++) {
                                mvsL0[bOff+i*4] = mvsL0[bOff+i*4+1] = null_mv;
                                refsL0[bOff+i*4] = refsL0[bOff+i*4+1] = -1;
                                mvsL1[bOff+i*4] = mvsL1[bOff+i*4+1] = BestMV8x16L1;
                                refsL1[bOff+i*4] = refsL1[bOff+i*4+1] = BestRef8x16L1;
                            }
                            break;
                        case 2:
                            for ( i = 0; i < 4; i ++) {
                                mvsL0[bOff+i*4] = mvsL0[bOff+i*4+1] = BestMV8x16L0;
                                refsL0[bOff+i*4] = refsL0[bOff+i*4+1] = BestRef8x16L0;
                                mvsL1[bOff+i*4] = mvsL1[bOff+i*4+1] = BestMV8x16L1;
                                refsL1[bOff+i*4] = refsL1[bOff+i*4+1] = BestRef8x16L1;
                            }
                            break;
                    }
                    BestMV8x16SL0[b] = mvsL0[bOff];
                    BestMV8x16SL1[b] = mvsL1[bOff];
                    BestRef8x16SL0[b] = refsL0[bOff];
                    BestRef8x16SL1[b] = refsL1[bOff];
                }
                Best8x16Type = MBTYPE_FWD_FWD_8x16 + typeOffset[BestPred8x16[0]][BestPred8x16[1]];
                BestSAD8x16 += BITS_COST(typeCost[BestPred8x16[0]][BestPred8x16[1]], pRDQM);

                if (ModeRDOpt /* && (BestSAD8x16 <= (BestSAD16x16 * SB_THRESH_RD)) */) {
                    curr_slice->m_cur_mb.GlobalMacroblockInfo->mbtype = Best8x16Type;
                    MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir8x16, 256*sizeof(PIXTYPE)); //copy buffer for mc
#ifdef USE_NV12
                    RDCost8x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
                    RDCost8x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 0 );
#endif // USE_NV12
                }
        }

        if( BestSAD8x8S < BestSAD16x16 ){
            BestSAD16x16 = BestSAD8x8S;
            BestMBType = MBTYPE_B_8x8;
        }
        if( BestSAD16x8 < BestSAD16x16 ){
            BestSAD16x16 = BestSAD16x8;
            BestMBType = Best16x8Type;
        }
        if( BestSAD8x16 < BestSAD16x16 ){
            BestSAD16x16 = BestSAD8x16;
            BestMBType = Best8x16Type;
        }
    }

    //Choose the best with RD
    if( ModeRDOpt ){
        BestMBType = MBTYPE_DIRECT;
        BestSAD16x16 = RDCost16x16Direct;
        if( RDCost16x16L0 < BestSAD16x16 ){
            BestMBType = MBTYPE_FORWARD;
            BestSAD16x16 = RDCost16x16L0;
        }
        if( RDCost16x16L1 < BestSAD16x16 ){
            BestMBType = MBTYPE_BACKWARD;
            BestSAD16x16 = RDCost16x16L1;
        }
        if( RDCost16x16Bi < BestSAD16x16 ){
            BestMBType = MBTYPE_BIDIR;
            BestSAD16x16 = RDCost16x16Bi;
        }
        if( RDCost8x8 < BestSAD16x16 ){
            BestMBType = MBTYPE_B_8x8;
            BestSAD16x16 = RDCost8x8;
        }
        if( RDCost8x16 < BestSAD16x16 ){
            BestMBType = Best8x16Type;
            BestSAD16x16 = RDCost8x16;
        }
        if( RDCost16x8 < BestSAD16x16 ){
            BestMBType = Best16x8Type;
            BestSAD16x16 = RDCost16x8;
        }
    }

    for( i = 0; i<4; i++ )  cur_mb.GlobalMacroblockInfo->sbtype[i] = (MBTypeValue)NUMBER_OF_MBTYPES;

    Ipp32s trans8x8 = core_enc->m_info.transform_8x8_mode_flag;
    cur_mb.GlobalMacroblockInfo->mbtype = BestMBType;
    switch( BestMBType ){
        case MBTYPE_DIRECT:
            *cur_mb.RefIdxs[LIST_0] = ref_idxs_direct[LIST_0];
            *cur_mb.RefIdxs[LIST_1] = ref_idxs_direct[LIST_1];
            *cur_mb.MVs[LIST_0] = mvs_direct[LIST_0];
            *cur_mb.MVs[LIST_1] = mvs_direct[LIST_1];
            cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
            cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
            for( i = 0; i<4; i++ )  cur_mb.GlobalMacroblockInfo->sbtype[i] = sbtype_direct[i];
            if( trans8x8 && !core_enc->m_SeqParamSet->direct_8x8_inference_flag ) trans8x8 = 0;
            break;
        case MBTYPE_FORWARD:
            for (i = 0; i < 16; i ++) {
                mvsL0[i] = BestMV16x16L0;
                mvsL1[i] = null_mv;
                refsL0[i] = (T_RefIdx)BestRef16x16L0;
                refsL1[i] = (T_RefIdx)-1;
            }
            if (mpred16x16L0) mpredflagL0 = 0xffff;
            break;
        case MBTYPE_BACKWARD:
            for (i = 0; i < 16; i ++) {
                mvsL0[i] = null_mv;
                mvsL1[i] = BestMV16x16L1;
                refsL0[i] = (T_RefIdx)-1;
                refsL1[i] = (T_RefIdx)BestRef16x16L1;
            }
            if (mpred16x16L1) mpredflagL1 = 0xffff;
            break;
        case MBTYPE_BIDIR:
            for (i = 0; i < 16; i ++) {
                mvsL0[i] = BestMV16x16L0;
                mvsL1[i] = BestMV16x16L1;
                refsL0[i] = (T_RefIdx)BestRef16x16L0;
                refsL1[i] = (T_RefIdx)BestRef16x16L1;
            }
            if (mpred16x16L0) mpredflagL0 = 0xffff;
            if (mpred16x16L1) mpredflagL1 = 0xffff;
            MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir16x16, 256*sizeof(PIXTYPE)); //copy buffer for mc
            break;
        case MBTYPE_FWD_FWD_16x8:
        case MBTYPE_BWD_BWD_16x8:
        case MBTYPE_FWD_BWD_16x8:
        case MBTYPE_BWD_FWD_16x8:
        case MBTYPE_BIDIR_FWD_16x8:
        case MBTYPE_BIDIR_BWD_16x8:
        case MBTYPE_FWD_BIDIR_16x8:
        case MBTYPE_BWD_BIDIR_16x8:
        case MBTYPE_BIDIR_BIDIR_16x8:
            for( Ipp32s b = 0; b<2; b++ ){
                bOff = b * 8;
                for ( i = 0; i < 8; i ++) {
                    mvsL0[bOff+i] = BestMV16x8SL0[b];
                    refsL0[bOff+i] = BestRef16x8SL0[b];
                    mvsL1[bOff+i] = BestMV16x8SL1[b];
                    refsL1[bOff+i] = BestRef16x8SL1[b];
                }
            }
            MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir16x8, 256*sizeof(PIXTYPE)); //copy buffer for mc
            switch( BestMBType ){
                case MBTYPE_FWD_FWD_16x8:
                    if (mpred16x8L0[0]) mpredflagL0  = 0x00ff;
                    if (mpred16x8L0[1]) mpredflagL0 |= 0xff00;
                    break;
                case MBTYPE_BWD_BWD_16x8:
                    if (mpred16x8L1[0]) mpredflagL1  = 0x00ff;
                    if (mpred16x8L1[1]) mpredflagL1 |= 0xff00;
                    break;
                case MBTYPE_FWD_BWD_16x8:
                    if (mpred16x8L0[0]) mpredflagL0  = 0x00ff;
                    if (mpred16x8L1[1]) mpredflagL1 |= 0xff00;
                    break;
                case MBTYPE_BWD_FWD_16x8:
                    if (mpred16x8L1[0]) mpredflagL1  = 0x00ff;
                    if (mpred16x8L0[1]) mpredflagL0 |= 0xff00;
                    break;
                case MBTYPE_BIDIR_FWD_16x8:
                    if (mpred16x8L0[0]) mpredflagL0  = 0x00ff;
                    if (mpred16x8L1[0]) mpredflagL1  = 0x00ff;
                    if (mpred16x8L0[1]) mpredflagL0 |= 0xff00;
                    break;
                case MBTYPE_BIDIR_BWD_16x8:
                    if (mpred16x8L0[0]) mpredflagL0  = 0x00ff;
                    if (mpred16x8L1[0]) mpredflagL1  = 0x00ff;
                    if (mpred16x8L1[1]) mpredflagL1 |= 0xff00;
                    break;
                case MBTYPE_FWD_BIDIR_16x8:
                    if (mpred16x8L0[0]) mpredflagL0  = 0x00ff;
                    if (mpred16x8L0[1]) mpredflagL0 |= 0xff00;
                    if (mpred16x8L1[1]) mpredflagL1 |= 0xff00;
                    break;
                case MBTYPE_BWD_BIDIR_16x8:
                    if (mpred16x8L1[0]) mpredflagL1  = 0x00ff;
                    if (mpred16x8L0[1]) mpredflagL0 |= 0xff00;
                    if (mpred16x8L1[1]) mpredflagL1 |= 0xff00;
                    break;
                case MBTYPE_BIDIR_BIDIR_16x8:
                    if (mpred16x8L0[0]) mpredflagL0  = 0x00ff;
                    if (mpred16x8L1[0]) mpredflagL1  = 0x00ff;
                    if (mpred16x8L0[1]) mpredflagL0 |= 0xff00;
                    if (mpred16x8L1[1]) mpredflagL1 |= 0xff00;
                    break;
                default:
                    break;
            }
            break;
        case MBTYPE_FWD_FWD_8x16:
        case MBTYPE_BWD_BWD_8x16:
        case MBTYPE_FWD_BWD_8x16:
        case MBTYPE_BWD_FWD_8x16:
        case MBTYPE_BIDIR_FWD_8x16:
        case MBTYPE_BIDIR_BWD_8x16:
        case MBTYPE_FWD_BIDIR_8x16:
        case MBTYPE_BWD_BIDIR_8x16:
        case MBTYPE_BIDIR_BIDIR_8x16:
            for( Ipp32s b = 0; b<2; b++ ){
                bOff = b * 2;
                for ( i = 0; i < 4; i ++) {
                    mvsL0[bOff+i*4] = mvsL0[bOff+i*4+1] = BestMV8x16SL0[b];
                    refsL0[bOff+i*4] = refsL0[bOff+i*4+1] = BestRef8x16SL0[b];
                    mvsL1[bOff+i*4] = mvsL1[bOff+i*4+1] = BestMV8x16SL1[b];
                    refsL1[bOff+i*4] = refsL1[bOff+i*4+1] = BestRef8x16SL1[b];
                }
                //MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir8x16, 256*sizeof(PIXTYPE)); //copy buffer for mc
            }
            MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir8x16, 256*sizeof(PIXTYPE)); //copy buffer for mc
            switch( BestMBType ){
                case MBTYPE_FWD_FWD_8x16:
                    if (mpred8x16L0[0]) mpredflagL0  = 0x3333;
                    if (mpred8x16L0[1]) mpredflagL0 |= 0xcccc;
                    break;
                case MBTYPE_BWD_BWD_8x16:
                    if (mpred8x16L1[0]) mpredflagL1  = 0x3333;
                    if (mpred8x16L1[1]) mpredflagL1 |= 0xcccc;
                    break;
                case MBTYPE_FWD_BWD_8x16:
                    if (mpred8x16L0[0]) mpredflagL0  = 0x3333;
                    if (mpred8x16L1[1]) mpredflagL1 |= 0xcccc;
                    break;
                case MBTYPE_BWD_FWD_8x16:
                    if (mpred8x16L1[0]) mpredflagL1  = 0x3333;
                    if (mpred8x16L0[1]) mpredflagL0 |= 0xcccc;
                    break;
                case MBTYPE_BIDIR_FWD_8x16:
                    if (mpred8x16L0[0]) mpredflagL0  = 0x3333;
                    if (mpred8x16L1[0]) mpredflagL1  = 0x3333;
                    if (mpred8x16L0[1]) mpredflagL0 |= 0xcccc;
                    break;
                case MBTYPE_BIDIR_BWD_8x16:
                    if (mpred8x16L0[0]) mpredflagL0  = 0x3333;
                    if (mpred8x16L1[0]) mpredflagL1  = 0x3333;
                    if (mpred8x16L1[1]) mpredflagL1 |= 0xcccc;
                    break;
                case MBTYPE_FWD_BIDIR_8x16:
                    if (mpred8x16L0[0]) mpredflagL0  = 0x3333;
                    if (mpred8x16L0[1]) mpredflagL0 |= 0xcccc;
                    if (mpred8x16L1[1]) mpredflagL1 |= 0xcccc;
                    break;
                case MBTYPE_BWD_BIDIR_8x16:
                    if (mpred8x16L1[0]) mpredflagL1  = 0x3333;
                    if (mpred8x16L0[1]) mpredflagL0 |= 0xcccc;
                    if (mpred8x16L1[1]) mpredflagL1 |= 0xcccc;
                    break;
                case MBTYPE_BIDIR_BIDIR_8x16:
                    if (mpred8x16L0[0]) mpredflagL0  = 0x3333;
                    if (mpred8x16L1[0]) mpredflagL1  = 0x3333;
                    if (mpred8x16L0[1]) mpredflagL0 |= 0xcccc;
                    if (mpred8x16L1[1]) mpredflagL1 |= 0xcccc;
                default:
                    break;
            }
            break;
        case MBTYPE_B_8x8:
            for (Ipp32s s = 0; s < 4; s++ ) {
                sbt[s] = Best8x8Type[s];
                if( (sbt[s] == SBTYPE_DIRECT && !core_enc->m_SeqParamSet->direct_8x8_inference_flag) || sbt[s] > SBTYPE_BIDIR_8x8 ) trans8x8 = 0;
                bOff = block_subblock_mapping[s*4];
                if (Best8x8Type[s] <= SBTYPE_BIDIR_8x8) {
                    if( Best8x8Type[s] != SBTYPE_DIRECT ){
                        refsL0[bOff] = refsL0[bOff+1] = refsL0[bOff+4] = refsL0[bOff+5] = (T_RefIdx)BestRef8x8SL0[s];
                        refsL1[bOff] = refsL1[bOff+1] = refsL1[bOff+4] = refsL1[bOff+5] = (T_RefIdx)BestRef8x8SL1[s];
                        mvsL0[bOff] = mvsL0[bOff+1] = mvsL0[bOff+4] = mvsL0[bOff+5] = BestMV8x8SL0[s];
                        mvsL1[bOff] = mvsL1[bOff+1] = mvsL1[bOff+4] = mvsL1[bOff+5] = BestMV8x8SL1[s];
                        if ((Best8x8Type[s] == SBTYPE_FORWARD_8x8  || Best8x8Type[s] == SBTYPE_BIDIR_8x8) && mpred8x8L0[s])
                            mpredflagL0 |= 0x33<<bOff;
                        if ((Best8x8Type[s] == SBTYPE_BACKWARD_8x8 || Best8x8Type[s] == SBTYPE_BIDIR_8x8) && mpred8x8L1[s])
                            mpredflagL1 |= 0x33<<bOff;
                    }else{
                        mvsL0[bOff]   = mvs_direct8x8[LIST_0].MotionVectors[bOff];
                        mvsL0[bOff+1] = mvs_direct8x8[LIST_0].MotionVectors[bOff+1];
                        mvsL0[bOff+4] = mvs_direct8x8[LIST_0].MotionVectors[bOff+4];
                        mvsL0[bOff+5] = mvs_direct8x8[LIST_0].MotionVectors[bOff+5];
                        mvsL1[bOff]   = mvs_direct8x8[LIST_1].MotionVectors[bOff];
                        mvsL1[bOff+1] = mvs_direct8x8[LIST_1].MotionVectors[bOff+1];
                        mvsL1[bOff+4] = mvs_direct8x8[LIST_1].MotionVectors[bOff+4];
                        mvsL1[bOff+5] = mvs_direct8x8[LIST_1].MotionVectors[bOff+5];

                        refsL0[bOff]   = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff];
                        refsL0[bOff+1] = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff+1];
                        refsL0[bOff+4] = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff+4];
                        refsL0[bOff+5] = ref_idxs_direct8x8[LIST_0].RefIdxs[bOff+5];
                        refsL1[bOff]   = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff];
                        refsL1[bOff+1] = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff+1];
                        refsL1[bOff+4] = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff+4];
                        refsL1[bOff+5] = ref_idxs_direct8x8[LIST_1].RefIdxs[bOff+5];
                    }
                }else{
                    mvsL0[bOff]   = BestMV4x4L0[s][0];
                    mvsL0[bOff+1] = BestMV4x4L0[s][1];
                    mvsL0[bOff+4] = BestMV4x4L0[s][2];
                    mvsL0[bOff+5] = BestMV4x4L0[s][3];
                    mvsL1[bOff]   = BestMV4x4L1[s][0];
                    mvsL1[bOff+1] = BestMV4x4L1[s][1];
                    mvsL1[bOff+4] = BestMV4x4L1[s][2];
                    mvsL1[bOff+5] = BestMV4x4L1[s][3];

                    refsL0[bOff]   = BestRef4x4L0[s][0];
                    refsL0[bOff+1] = BestRef4x4L0[s][1];
                    refsL0[bOff+4] = BestRef4x4L0[s][2];
                    refsL0[bOff+5] = BestRef4x4L0[s][3];
                    refsL1[bOff]   = BestRef4x4L1[s][0];
                    refsL1[bOff+1] = BestRef4x4L1[s][1];
                    refsL1[bOff+4] = BestRef4x4L1[s][2];
                    refsL1[bOff+5] = BestRef4x4L1[s][3];
                    Ipp32s bl;
                    for (bl = 0; bl < 2; bl++ ) {
                        if ((Best8x8Type[s] == SBTYPE_FORWARD_8x4  || Best8x8Type[s] == SBTYPE_BIDIR_8x4) && mpred8x4L0[s][bl])
                            mpredflagL0 |= 0x03<<block_subblock_mapping[s*4 + bl*2];
                        if ((Best8x8Type[s] == SBTYPE_BACKWARD_8x4 || Best8x8Type[s] == SBTYPE_BIDIR_8x4) && mpred8x4L1[s][bl])
                            mpredflagL1 |= 0x03<<block_subblock_mapping[s*4 + bl*2];
                        if ((Best8x8Type[s] == SBTYPE_FORWARD_4x8  || Best8x8Type[s] == SBTYPE_BIDIR_4x8) && mpred4x8L0[s][bl])
                            mpredflagL0 |= 0x11<<block_subblock_mapping[s*4 + bl];
                        if ((Best8x8Type[s] == SBTYPE_BACKWARD_4x8 || Best8x8Type[s] == SBTYPE_BIDIR_4x8) && mpred4x8L1[s][bl])
                            mpredflagL1 |= 0x11<<block_subblock_mapping[s*4 + bl];
                    }
                    for (bl = 0; bl < 4; bl++ ) {
                        if ((Best8x8Type[s] == SBTYPE_FORWARD_4x4  || Best8x8Type[s] == SBTYPE_BIDIR_4x4) && mpred4x4L0[s][bl])
                            mpredflagL0 |= 0x01<<block_subblock_mapping[s*4 + bl];
                        if ((Best8x8Type[s] == SBTYPE_BACKWARD_4x4 || Best8x8Type[s] == SBTYPE_BIDIR_4x4) && mpred4x4L1[s][bl])
                            mpredflagL1 |= 0x01<<block_subblock_mapping[s*4 + bl];
                    }
                }
            }
            MFX_INTERNAL_CPY(curr_slice->m_pPred4BiPred, bidir8x8, 256*sizeof(PIXTYPE)); //copy buffer for mc
            break;
    }

    if ((meFlags & ANALYSE_ME_BIDIR_REFINE) && (BestMBType == MBTYPE_BIDIR || BestMBType >= MBTYPE_BIDIR_FWD_16x8))
    {
        Ipp32s refCostBidir = refCostL0[BestRef16x16L0] + refCostL1[BestRef16x16L1];
        for (i = 0; i < 9; i ++)
        {
            if (bestSADByMBtype[i])
            {
                bestSADByMBtype[i] -= (chromaContrToSAD[i] + refCostBidir);
                if (i == 0 || i >= SHIFT_8x8)
                    bestSADByMBtype[i] -= BITS_COST(5, pRDQM);
            }
        }
#ifdef NO_PADDING
        PIXTYPE *pRefL0, *pRefL1;
        ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_RefPicList;
        pRefL0 = ppRefPicList[BestRef16x16L0]->m_pYPlane + curr_slice->m_InitialOffset[pFields0[BestRef16x16L0]] + offsetIfBottomField;
        ppRefPicList = H264ENC_MAKE_NAME(GetRefPicList)(curr_slice, LIST_1, is_cur_mb_field, uMB & 1)->m_RefPicList;
        pRefL1 = ppRefPicList[BestRef16x16L1]->m_pYPlane + curr_slice->m_InitialOffset[pFields1[BestRef16x16L1]] + offsetIfBottomField;
        H264ENC_MAKE_NAME(H264CoreEncoder_BidirRefine)(curr_slice, core_enc, pRefL0, pRefL1, \
            predBidirRefineL0, predBidirRefineL1, bestSADByMBtype, &BestSAD16x16);
#else // NO_PADDING
        H264ENC_MAKE_NAME(H264CoreEncoder_BidirRefine)(curr_slice, core_enc, pRef0, pRef1, predBidirRefineL0, predBidirRefineL1, bestSADByMBtype, &BestSAD16x16);
#endif // NO_PADDING
    }

    if (((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE)) && !ModeRDOpt) {
        if ((core_enc->m_Analyse & ANALYSE_INTRA_IN_ME) && (!resPredFlag)) {
            if ((BestMBType == MBTYPE_DIRECT) || (BestMBType == MBTYPE_BIDIR) || (BestMBType == MBTYPE_FORWARD) || (BestMBType == MBTYPE_BACKWARD))
                BestSAD16x16 = BestInterSAD;
            else
            {
#ifdef USE_NV12
                BestSAD16x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
                BestSAD16x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 0 );
#endif // USE_NV12
            }
        } else
        {
#ifdef USE_NV12
            BestSAD16x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, RefLater8x8PackFlag, 1, resPredFlag ? resPredBuf : NULL, resPredFlag ? resPredBufC : NULL);
#else // USE_NV12
            BestSAD16x16 = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 0 );
#endif // USE_NV12
        }
    }

    if (!check2TransformModes)
    {
        trans8x8 = RefLater8x8PackFlag;
    }

    // choose best transform
    if ((trans8x8) && (check2TransformModes)) {
        if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || ModeRDOpt) {
        //if ( ModeRDOpt ) {
#ifdef USE_NV12
            Ipp32s rdCost8x8t = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(state, curr_slice, 1, 1);
#else // USE_NV12
            Ipp32s rdCost8x8t = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(state, curr_slice, 1 );
#endif // USE_NV12
            if (BestSAD16x16 <= rdCost8x8t)
                trans8x8 = 0;
            else
                BestSAD16x16 = rdCost8x8t;
        } else {
            __ALIGN16 PIXTYPE mcBlock[256];
            __ALIGN16 Ipp16s diffBuff[16][16];
            Ipp32s sad4x4t = 0;
            Ipp32s sad8x8t = 0;

            H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBLuma)(state, curr_slice, mvsL0, mvsL1, mcBlock);
            if (!meInfo.residualPredictionFlag)
            {
                sad4x4t = SATD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, mcBlock, 16);
                sad8x8t = SAT8x8D(cur_mb.mbPtr, cur_mb.mbPitchPixels, mcBlock, 16);
                sad8x8t+= SAT8x8D(cur_mb.mbPtr + 8, cur_mb.mbPitchPixels, mcBlock + 8, 16);
                sad8x8t+= SAT8x8D(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels, cur_mb.mbPitchPixels, mcBlock + 8 * 16, 16);
                sad8x8t+= SAT8x8D(cur_mb.mbPtr + 8 * cur_mb.mbPitchPixels + 8, cur_mb.mbPitchPixels, mcBlock + 8 * 16 + 8, 16);
            }
            else
            {
                Ipp32s i, j;
                for (i = 0; i < 16; i++)
                    for (j = 0; j < 16; j++)
                        diffBuff[i][j] = resPredBuf[i*16 + j] - mcBlock[i*16 + j];
                for (i = 0; i < 2; i++)
                    for (j = 0; j < 2; j++)
                        sad8x8t += Hadamard8x8_16_AbsSum(&(diffBuff[i*8][j*8]));
                for (i = 0; i < 4; i++)
                    for (j = 0; j < 4; j++)
                        sad4x4t += Hadamard4x4_16_AbsSum(&(diffBuff[i*4][j*4]));

            }

            if (sad4x4t < sad8x8t + (Ipp32s)BITS_COST(2, pRDQM))
                trans8x8 = 0;
            //        else BestSAD16x16 += sad8x8t + BITS_COST(2, pRDQM) - sad4x4t;
        }
    }
    pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, trans8x8);
    if ((core_enc->m_Analyse & ANALYSE_INTRA_IN_ME) && (!resPredFlag)) {
        T_AIMode intra_types_save[16];
        Ipp32u uInterCBP4x4 = cur_mb.LocalMacroblockInfo->cbp_luma;
        MBTypeValue MBTypeInter = cur_mb.GlobalMacroblockInfo->mbtype;
        Ipp32s InterMB8x8PackFlag = pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo);
        bool bIntra8x8 = false;

        if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || !(BestSAD16x16 < EmptyThreshold[uMBQP])) {
            H264MacroblockLocalInfo sLocalMBinfo = *cur_mb.LocalMacroblockInfo;
            //H264MacroblockGlobalInfo sGlobalMBinfo = *cur_mb.GlobalMacroblockInfo;
            *cur_mb.LocalMacroblockInfo = intraLocalMBinfo;
            if (BestIntraSAD + intra_cost_chroma + intra_cost_b <= (BestSAD16x16 * 5 >> 2)) {
                if ((core_enc->m_Analyse & ANALYSE_I_4x4) || (core_enc->m_Analyse & ANALYSE_I_8x8)) {
                    Ipp32s MBHasEdges;
                    H264ENC_MAKE_NAME(ippiEdgesDetect16x16)(cur_mb.mbPtr, cur_mb.mbPitchPixels, EdgePelDiffTable[uMBQP], EdgePelCountTable[uMBQP], &MBHasEdges);
                    if (MBHasEdges) {
                        if (core_enc->m_Analyse & ANALYSE_I_8x8) {
                            pSetMB8x8TSFlag(curr_slice->m_cur_mb.GlobalMacroblockInfo, true);
                            H264ENC_MAKE_NAME(H264CoreEncoder_AdvancedIntraModeSelectOneMacroblock8x8)(state, curr_slice, BestIntraSAD, &BestIntraSAD8x8);
                            //Save intra_types
                            MFX_INTERNAL_CPY(intra_types_save, curr_slice->m_cur_mb.intra_types, 16 * sizeof(T_AIMode));
                            if ((Ipp32s)BestIntraSAD8x8 < BestIntraSAD) {
                                BestIntraSAD = BestIntraSAD8x8;
                                BestIntraMBType = MBTYPE_INTRA;
                                bIntra8x8 = true;
                            }
                            pSetMB8x8TSFlag(curr_slice->m_cur_mb.GlobalMacroblockInfo, false);
                            if ((core_enc->m_Analyse & ANALYSE_I_4x4) && (BestIntraSAD8x8 <= (BestIntraSAD16x16 * 5 >> 2))) {
                                H264ENC_MAKE_NAME(H264CoreEncoder_AdvancedIntraModeSelectOneMacroblock)(state, curr_slice, BestIntraSAD, &BestIntraSAD4x4);
                                if ((Ipp32s)BestIntraSAD4x4 < BestIntraSAD) {
                                    BestIntraSAD = BestIntraSAD4x4;
                                    BestIntraMBType = MBTYPE_INTRA;
                                    bIntra8x8 = false;
                                }
                            }
                        } else if (core_enc->m_Analyse & ANALYSE_I_4x4) {
                            H264ENC_MAKE_NAME(H264CoreEncoder_AdvancedIntraModeSelectOneMacroblock)(state, curr_slice, BestIntraSAD, &BestIntraSAD4x4);
                            if ((Ipp32s)BestIntraSAD4x4 < BestIntraSAD) {
                                BestIntraSAD = BestIntraSAD4x4;
                                BestIntraMBType = MBTYPE_INTRA;
                                bIntra8x8 = false;
                            }
                        }
                    }
                }
            }
            BestIntraSAD += intra_cost_chroma + intra_cost_b;
            if (BestIntraSAD < BestSAD16x16) {
                cur_mb.GlobalMacroblockInfo->mbtype = BestIntraMBType;
                pSetMB8x8TSPackFlag(curr_slice->m_cur_mb.GlobalMacroblockInfo, bIntra8x8);
                if (BestIntraMBType == MBTYPE_INTRA && !bIntra8x8)
                    cur_mb.LocalMacroblockInfo->cbp_luma = cur_mb.m_uIntraCBP4x4;
                else if (BestIntraMBType == MBTYPE_INTRA && bIntra8x8) {
                    cur_mb.LocalMacroblockInfo->cbp_luma = cur_mb.m_uIntraCBP8x8;
                    //Restore intra_types
                    MFX_INTERNAL_CPY( curr_slice->m_cur_mb.intra_types, intra_types_save, 16*sizeof(T_AIMode));
                } else
                    cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;
                cur_mb.LocalMacroblockInfo->cbp_chroma = 0xffffffff;
                curr_slice->m_Intra_MB_Counter ++;
                BestSAD16x16 = BestIntraSAD;
            } else {
                *cur_mb.LocalMacroblockInfo = sLocalMBinfo;
                cur_mb.LocalMacroblockInfo->intra_chroma_mode = 0; //Needed for packing
                cur_mb.GlobalMacroblockInfo->mbtype = MBTypeInter;
                cur_mb.LocalMacroblockInfo->cbp_luma = uInterCBP4x4;
                pSetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo, InterMB8x8PackFlag);
            }
        }
    }
#ifdef USE_NV12
    if ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_ME_CHROMA))
    {
        pSrc = core_enc->m_pCurrentFrame->m_pUPlane + uChromaOffset;
        for(i=0;i<8;i++){
            Ipp32s s = 8*i;

            pSrc[0] = pUSrc[s+0];
            pSrc[1] = pVSrc[s+0];
            pSrc[2] = pUSrc[s+1];
            pSrc[3] = pVSrc[s+1];
            pSrc[4] = pUSrc[s+2];
            pSrc[5] = pVSrc[s+2];
            pSrc[6] = pUSrc[s+3];
            pSrc[7] = pVSrc[s+3];
            pSrc[8] = pUSrc[s+4];
            pSrc[9] = pVSrc[s+4];
            pSrc[10] = pUSrc[s+5];
            pSrc[11] = pVSrc[s+5];
            pSrc[12] = pUSrc[s+6];
            pSrc[13] = pVSrc[s+6];
            pSrc[14] = pUSrc[s+7];
            pSrc[15] = pVSrc[s+7];

            pSrc += meInfo.pitchPixels;
        }
    }
#endif // USE_NV12

    //  Extra check - flags should already be set in that case
    if (!core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag &&
        pGetMBCropFlag(cur_mb.GlobalMacroblockInfo) &&
        !curr_slice->m_adaptive_motion_prediction_flag && curr_slice->m_default_motion_prediction_flag)
        mpredflagL0 = mpredflagL1 = 0xffff;

    if (mpredflagL0)
        cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = mpredflagL0;
    else
        cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = 0;
    if (mpredflagL1)
        cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = mpredflagL1;
    else
        cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = 0;
    return BestSAD16x16;
}

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_BidirRefine)(
    H264SliceType *curr_slice,
    H264CoreEncoderType * core_enc,
    PIXTYPE * pRefL0,
    PIXTYPE * pRefL1,
    H264MotionVector * predictedMVsL0,
    H264MotionVector * predictedMVsL1,
    Ipp32s * bestSAD,
    Ipp32s * bestSAD16x16
    )
{
    static const Ipp16s bdX[9] = {0, -1, 0, 1, 1, 1, 0, -1, -1}, bdY[9] = {0, -1, -1, -1, 0, 1, 1, 1, 0};
    static const Ipp16s bPC[9] = {8, 1, 5, 1, 5, 1, 5, 1, 5};
    static const Ipp16s bdL1[9][2][9] = {
        { {-1, 0, 1, 1, 1, 0,-1,-1, 0}, {-1,-1,-1, 0, 1, 1, 1, 0, 0} },
        { { 0, 0, 0, 0, 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0, 0, 0, 0, 0} },
        { { 0, 0, 1, 0,-1, 0, 0, 0, 0}, { 0,-1, 0, 1, 0, 0, 0, 0, 0} },
        { { 0, 0, 0, 0, 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0, 0, 0, 0, 0} },
        { { 0, 0, 1, 0,-1, 0, 0, 0, 0}, { 0,-1, 0, 1, 0, 0, 0, 0, 0} },
        { { 0, 0, 0, 0, 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0, 0, 0, 0, 0} },
        { { 0, 0, 1, 0,-1, 0, 0, 0, 0}, { 0,-1, 0, 1, 0, 0, 0, 0, 0} },
        { { 0, 0, 0, 0, 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0, 0, 0, 0, 0} },
        { { 0, 0, 1, 0,-1, 0, 0, 0, 0}, { 0,-1, 0, 1, 0, 0, 0, 0, 0} }
    };
    static const Ipp16s bdL1PosBuf[9][8] = {
        { 1, 2, 3, 4, 5, 6, 7, 8},
        { 0, 0, 0, 0, 0, 0, 0, 0},
        { 0, 2, 4, 6, 8, 0, 0, 0},
        { 0, 0, 0, 0, 0, 0, 0, 0},
        { 0, 2, 4, 6, 8, 0, 0, 0},
        { 0, 0, 0, 0, 0, 0, 0, 0},
        { 0, 2, 4, 6, 8, 0, 0, 0},
        { 0, 0, 0, 0, 0, 0, 0, 0},
        { 0, 2, 4, 6, 8, 0, 0, 0}
    };


    H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
    Ipp32s i, b, posL0, posL1, blockSize;
    Ipp32s pitchPixels = cur_mb.mbPitchPixels;
    Ipp32s cSAD, bSAD, bInitialSAD, prRDCost, bInitailMBSAD = *bestSAD16x16;
    PIXTYPE *pRefCurL0, *pRefCurL1, *mcCurBuff, *pCur;
    __ALIGN16 PIXTYPE addTmpBufL0[9][256];
    __ALIGN16 PIXTYPE addTmpBufL1[9][256];
    __ALIGN16 PIXTYPE bidirBuff[256];
    __ALIGN16 PIXTYPE bestRefinedBlockMC[256];
    __ALIGN16 PIXTYPE initialMBMC[256];
    PIXTYPE * mcBuff = curr_slice->m_pPred4BiPred;
    PIXTYPE * pBestRefinedBlockMC;
    Ipp32s bestMBType = cur_mb.GlobalMacroblockInfo->mbtype;
    Ipp32s bestSBType;
    Ipp32s bOff, pOff;
    IppiSize block;
    Ipp32s isRefined = 0;
    Ipp32s isRefinedMB = 0;
    H264MotionVector bestMVL0, bestMVL1, curMVL0, curMVL1, bMVL0, bMVL1, predictedMVL0, predictedMVL1;
    H264MotionVector initialMotionVectorsL0[16], initialMotionVectorsL1[16];
    Ipp32s ModeRDOpt = ((core_enc->m_Analyse & ANALYSE_RD_OPT) || (core_enc->m_Analyse & ANALYSE_RD_MODE)) && (core_enc->m_Analyse & ANALYSE_B_RD_OPT);

#ifdef FRAME_INTERPOLATION
    Ipp32s planeSize = core_enc->m_pCurrentFrame->m_PlaneSize;
#else
#ifndef NO_PADDING
    Ipp32s planeSize = 0;
#endif
#endif

#ifdef NO_PADDING
#if PIXBITS == 8
    IppVCInterpolateBlock_8u interpolateInfo, *pInterpolateInfo = &interpolateInfo;
#elif PIXBITS == 16
    IppVCInterpolateBlock_16u interpolateInfo, *pInterpolateInfo = &interpolateInfo;
#endif //PIXBITS
    IppiPoint mbPos;
    mbPos.x = cur_mb.uMBx << 4;
    mbPos.y = cur_mb.uMBy << 4;
    pInterpolateInfo->srcStep = pitchPixels;
    pInterpolateInfo->dstStep = 16;
    pInterpolateInfo->sizeFrame.width = core_enc->m_WidthInMBs << 4;
    pInterpolateInfo->sizeFrame.height = core_enc->m_HeightInMBs << 4;
#endif //NO_PADDING

    MFX_INTERNAL_CPY(initialMotionVectorsL0,cur_mb.MVs[LIST_0]->MotionVectors, 16 * sizeof(H264MotionVector));
    MFX_INTERNAL_CPY(initialMotionVectorsL1,cur_mb.MVs[LIST_1]->MotionVectors, 16 * sizeof(H264MotionVector));
    MFX_INTERNAL_CPY(initialMBMC,mcBuff,256 * sizeof(PIXTYPE));

    switch (bestMBType)
    {
    case MBTYPE_BIDIR: // try to refine B_Bi_16x16
        block.width = 16;
        block.height = 16;
        blockSize = block.width + (block.height >> 2); // TODO: change to value of BlockSize

        predictedMVL0 = *predictedMVsL0;
        predictedMVL1 = *predictedMVsL1;
        bSAD = *bestSAD;
        bInitialSAD = bSAD;

        bestMVL0 = cur_mb.MVs[LIST_0]->MotionVectors[0];
        bestMVL1 = cur_mb.MVs[LIST_1]->MotionVectors[0];
        bMVL0 = bestMVL0;
        bMVL1 = bestMVL1;

        pCur = cur_mb.mbPtr;
        pRefCurL0 = pRefL0;
        pRefCurL1 = pRefL1;
        mcCurBuff = mcBuff;

#ifdef NO_PADDING
        pInterpolateInfo->sizeBlock = block;
        pInterpolateInfo->pointBlockPos = mbPos;
#endif // NO_PADDING

        REFINE_BIDIR

            if (isRefined)
            {
                for (i = 0; i < 16; i ++)
                {
                    cur_mb.MVs[LIST_0]->MotionVectors[i] = bMVL0;
                    cur_mb.MVs[LIST_1]->MotionVectors[i] = bMVL1;
                }

                COPY_BLOCK

                    if (ModeRDOpt)
                    {
                        prRDCost = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(core_enc, curr_slice, 0 );
                        if (prRDCost > bInitailMBSAD)
                        {
                            MFX_INTERNAL_CPY(cur_mb.MVs[LIST_0]->MotionVectors, initialMotionVectorsL0, 16 * sizeof(H264MotionVector));
                            MFX_INTERNAL_CPY(cur_mb.MVs[LIST_1]->MotionVectors, initialMotionVectorsL1, 16 * sizeof(H264MotionVector));
                            MFX_INTERNAL_CPY(mcBuff, initialMBMC, 256 * sizeof(PIXTYPE) );
                            isRefined = 0;
                        }
                        else
                        {
                            *bestSAD16x16 = prRDCost;
                        }

                    }
                    else
                    {
                        *bestSAD16x16 -= bInitialSAD;
                        *bestSAD16x16 += bSAD;
                    }
            }
            break;

    case MBTYPE_BIDIR_FWD_16x8: // try to refine B_Bi_L{0|1}_16x8
    case MBTYPE_BIDIR_BWD_16x8:
        block.width = 16;
        block.height = 8;
        blockSize = block.width + (block.height >> 2); // TODO: change to value of BlockSize

        predictedMVL0 = *(predictedMVsL0 + 1);
        predictedMVL1 = *(predictedMVsL1 + 1);
        bSAD = *(bestSAD + 1);
        bInitialSAD = bSAD;

        bestMVL0 = cur_mb.MVs[LIST_0]->MotionVectors[0];
        bestMVL1 = cur_mb.MVs[LIST_1]->MotionVectors[0];
        bMVL0 = bestMVL0;
        bMVL1 = bestMVL1;

        pCur = cur_mb.mbPtr;
        pRefCurL0 = pRefL0;
        pRefCurL1 = pRefL1;
        mcCurBuff = mcBuff;

#ifdef NO_PADDING
        pInterpolateInfo->sizeBlock = block;
        pInterpolateInfo->pointBlockPos = mbPos;
#endif // NO_PADDING

        REFINE_BIDIR

            if (isRefined)
            {
                for (i = 0; i < 8; i ++)
                {
                    cur_mb.MVs[LIST_0]->MotionVectors[i] = bMVL0;
                    cur_mb.MVs[LIST_1]->MotionVectors[i] = bMVL1;
                }
                COPY_BLOCK

                    if (ModeRDOpt)
                    {
                        prRDCost = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(core_enc, curr_slice, 0 );
                        if (prRDCost > bInitailMBSAD)
                        {
                            MFX_INTERNAL_CPY(cur_mb.MVs[LIST_0]->MotionVectors, initialMotionVectorsL0, 16 * sizeof(H264MotionVector));
                            MFX_INTERNAL_CPY(cur_mb.MVs[LIST_1]->MotionVectors, initialMotionVectorsL1, 16 * sizeof(H264MotionVector));
                            MFX_INTERNAL_CPY(mcBuff, initialMBMC, 256 * sizeof(PIXTYPE) );
                            isRefined = 0;
                        }
                        else
                        {
                            *bestSAD16x16 = prRDCost;
                        }
                    }
                    else
                    {
                        *bestSAD16x16 -= bInitialSAD;
                        *bestSAD16x16 += bSAD;
                    }
            }

            break; // try to refine B_Bi_L{0|1}_16x8

    case MBTYPE_BIDIR_FWD_8x16:     // try to refine B_Bi_L{0|1}_8x16
    case MBTYPE_BIDIR_BWD_8x16:
        block.width = 8;
        block.height = 16;
        blockSize = block.width + (block.height >> 2); // TODO: change to value of BlockSize

        predictedMVL0 = *(predictedMVsL0 + 3);
        predictedMVL1 = *(predictedMVsL1 + 3);
        bSAD = *(bestSAD + 3);
        bInitialSAD = bSAD;

        bestMVL0 = cur_mb.MVs[LIST_0]->MotionVectors[0];
        bestMVL1 = cur_mb.MVs[LIST_1]->MotionVectors[0];
        bMVL0 = bestMVL0;
        bMVL1 = bestMVL1;

        pCur = cur_mb.mbPtr;
        pRefCurL0 = pRefL0;
        pRefCurL1 = pRefL1;
        mcCurBuff = mcBuff;

#ifdef NO_PADDING
        pInterpolateInfo->sizeBlock = block;
        pInterpolateInfo->pointBlockPos = mbPos;
#endif // NO_PADDING

        REFINE_BIDIR

            if (isRefined)
            {
                for (i = 0; i < 16; i += 4)
                {
                    cur_mb.MVs[LIST_0]->MotionVectors[i] = bMVL0;
                    cur_mb.MVs[LIST_0]->MotionVectors[i + 1] = bMVL0;
                    cur_mb.MVs[LIST_1]->MotionVectors[i] = bMVL1;
                    cur_mb.MVs[LIST_1]->MotionVectors[i + 1] = bMVL1;
                }
                COPY_BLOCK

                    if (ModeRDOpt)
                    {
                        prRDCost = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(core_enc, curr_slice, 0 );
                        if (prRDCost > bInitailMBSAD)
                        {
                            MFX_INTERNAL_CPY(cur_mb.MVs[LIST_0]->MotionVectors, initialMotionVectorsL0, 16 * sizeof(H264MotionVector));
                            MFX_INTERNAL_CPY(cur_mb.MVs[LIST_1]->MotionVectors, initialMotionVectorsL1, 16 * sizeof(H264MotionVector));
                            MFX_INTERNAL_CPY(mcBuff, initialMBMC, 256 * sizeof(PIXTYPE) );
                            isRefined = 0;
                        }
                        else
                        {
                            *bestSAD16x16 = prRDCost;
                        }
                    }
                    else
                    {
                        *bestSAD16x16 -= bInitialSAD;
                        *bestSAD16x16 += bSAD;
                    }
            }

            break; //try to refine B_Bi_L{0|1}_8x16

    case MBTYPE_FWD_BIDIR_16x8: // try to refine B_L{0|1}_Bi_16x8
    case MBTYPE_BWD_BIDIR_16x8:

        block.width = 16;
        block.height = 8;
        blockSize = block.width + (block.height >> 2); // TODO: change to value of BlockSize

        predictedMVL0 = *(predictedMVsL0 + 2);
        predictedMVL1 = *(predictedMVsL1 + 2);
        bSAD = *(bestSAD + 2);
        bInitialSAD = bSAD;

        bestMVL0 = cur_mb.MVs[LIST_0]->MotionVectors[8];
        bestMVL1 = cur_mb.MVs[LIST_1]->MotionVectors[8];
        bMVL0 = bestMVL0;
        bMVL1 = bestMVL1;

        pCur = cur_mb.mbPtr + (pitchPixels << 3);
        pRefCurL0 = pRefL0 + (pitchPixels << 3);
        pRefCurL1 = pRefL1 + (pitchPixels << 3);
        mcCurBuff = mcBuff + (16 << 3);

#ifdef NO_PADDING
        pInterpolateInfo->sizeBlock = block;
        pInterpolateInfo->pointBlockPos.x = mbPos.x;
        pInterpolateInfo->pointBlockPos.y = mbPos.y + 8;
#endif // NO_PADDING

        REFINE_BIDIR

            if (isRefined)
            {
                for (i = 8; i < 16; i ++)
                {
                    cur_mb.MVs[LIST_0]->MotionVectors[i] = bMVL0;
                    cur_mb.MVs[LIST_1]->MotionVectors[i] = bMVL1;
                }
                COPY_BLOCK

                    if (ModeRDOpt)
                    {
                        prRDCost = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(core_enc, curr_slice, 0 );
                        if (prRDCost > bInitailMBSAD)
                        {
                            MFX_INTERNAL_CPY(cur_mb.MVs[LIST_0]->MotionVectors, initialMotionVectorsL0, 16 * sizeof(H264MotionVector));
                            MFX_INTERNAL_CPY(cur_mb.MVs[LIST_1]->MotionVectors, initialMotionVectorsL1, 16 * sizeof(H264MotionVector));
                            MFX_INTERNAL_CPY(mcBuff, initialMBMC, 256 * sizeof(PIXTYPE) );
                            isRefined = 0;
                        }
                        else
                        {
                            *bestSAD16x16 = prRDCost;
                        }
                    }
                    else
                    {
                        *bestSAD16x16 -= bInitialSAD;
                        *bestSAD16x16 += bSAD;
                    }
            }

            break; // try to refine B_L{0|1}_Bi_16x8

    case MBTYPE_FWD_BIDIR_8x16: // try to refine B_L{0|1}_Bi_8x16
    case MBTYPE_BWD_BIDIR_8x16:

        block.width = 8;
        block.height = 16;
        blockSize = block.width + (block.height >> 2); // TODO: change to value of BlockSize

        predictedMVL0 = *(predictedMVsL0 + 4);
        predictedMVL1 = *(predictedMVsL1 + 4);
        bSAD = *(bestSAD + 4);
        bInitialSAD = bSAD;

        bestMVL0 = cur_mb.MVs[LIST_0]->MotionVectors[2];
        bestMVL1 = cur_mb.MVs[LIST_1]->MotionVectors[2];
        bMVL0 = bestMVL0;
        bMVL1 = bestMVL1;

        pCur = cur_mb.mbPtr + 8;
        pRefCurL0 = pRefL0 + 8;
        pRefCurL1 = pRefL1 + 8;
        mcCurBuff = mcBuff + 8;

#ifdef NO_PADDING
        pInterpolateInfo->sizeBlock = block;
        pInterpolateInfo->pointBlockPos.x = mbPos.x + 8;
        pInterpolateInfo->pointBlockPos.y = mbPos.y;
#endif // NO_PADDING

        REFINE_BIDIR

            if (isRefined)
            {
                bOff = 2;
                for (i = 0; i < 16; i += 4)
                {
                    cur_mb.MVs[LIST_0]->MotionVectors[bOff + i] = bMVL0;
                    cur_mb.MVs[LIST_0]->MotionVectors[bOff + i + 1] = bMVL0;
                    cur_mb.MVs[LIST_1]->MotionVectors[bOff + i] = bMVL1;
                    cur_mb.MVs[LIST_1]->MotionVectors[bOff + i + 1] = bMVL1;
                }
                COPY_BLOCK

                    if (ModeRDOpt)
                    {
                        prRDCost = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(core_enc, curr_slice, 0 );
                        if (prRDCost > bInitailMBSAD)
                        {
                            MFX_INTERNAL_CPY(cur_mb.MVs[LIST_0]->MotionVectors, initialMotionVectorsL0, 16 * sizeof(H264MotionVector));
                            MFX_INTERNAL_CPY(cur_mb.MVs[LIST_1]->MotionVectors, initialMotionVectorsL1, 16 * sizeof(H264MotionVector));
                            MFX_INTERNAL_CPY(mcBuff, initialMBMC, 256 * sizeof(PIXTYPE) );
                            isRefined = 0;
                        }
                        else
                        {
                            *bestSAD16x16 = prRDCost;
                        }
                    }
                    else
                    {
                        *bestSAD16x16 -= bInitialSAD;
                        *bestSAD16x16 += bSAD;
                    }
            }

            break; // try to refine B_L{0|1}_Bi_8x16

    case MBTYPE_BIDIR_BIDIR_16x8: // try to refine B_Bi_Bi_16x8

        block.width = 16;
        block.height = 8;
        blockSize = block.width + (block.height >> 2); // TODO: change to value of BlockSize
#ifdef NO_PADDING
        pInterpolateInfo->sizeBlock = block;
#endif // NO_PADDING

        for (b = 0; b < 2; b ++)
        {
            predictedMVL0 = *(predictedMVsL0 + 1 + b);
            predictedMVL1 = *(predictedMVsL1 + 1 + b);
            bSAD = *(bestSAD + 1 + b);
            bInitialSAD = bSAD;

            bOff = b << 3;
            bestMVL0 = cur_mb.MVs[LIST_0]->MotionVectors[bOff];
            bestMVL1 = cur_mb.MVs[LIST_1]->MotionVectors[bOff];
            bMVL0 = bestMVL0;
            bMVL1 = bestMVL1;

            pOff = bOff * pitchPixels;
            pRefCurL0 = pRefL0 + pOff;
            pRefCurL1 = pRefL1 + pOff;
            pCur = cur_mb.mbPtr + pOff;
            mcCurBuff = mcBuff + bOff * 16;

#ifdef NO_PADDING
            pInterpolateInfo->pointBlockPos.x = mbPos.x;
            pInterpolateInfo->pointBlockPos.y = mbPos.y + bOff;
#endif // NO_PADDING

            REFINE_BIDIR

                if (isRefined)
                {
                    for (i = 0; i < 8; i ++)
                    {
                        cur_mb.MVs[LIST_0]->MotionVectors[bOff + i] = bMVL0;
                        cur_mb.MVs[LIST_1]->MotionVectors[bOff + i] = bMVL1;
                    }

                    if (!ModeRDOpt)
                    {
                        *bestSAD16x16 -= bInitialSAD;
                        *bestSAD16x16 += bSAD;
                    }
                    COPY_BLOCK

                        isRefinedMB = 1;
                    isRefined = 0;
                }
        }

        isRefined |= isRefinedMB;

        if (isRefined && ModeRDOpt)
        {
            prRDCost = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(core_enc, curr_slice, 0 );
            if (prRDCost > bInitailMBSAD)
            {
                MFX_INTERNAL_CPY(cur_mb.MVs[LIST_0]->MotionVectors, initialMotionVectorsL0, 16 * sizeof(H264MotionVector));
                MFX_INTERNAL_CPY(cur_mb.MVs[LIST_1]->MotionVectors, initialMotionVectorsL1, 16 * sizeof(H264MotionVector));
                MFX_INTERNAL_CPY(mcBuff, initialMBMC, 256 * sizeof(PIXTYPE) );
                isRefined = 0;
            }
            else
            {
                *bestSAD16x16 = prRDCost;
            }
        }

        break; // try to refine B_Bi_Bi_16x8

    case MBTYPE_BIDIR_BIDIR_8x16: // try to refine B_Bi_Bi_8x16
        block.width = 8;
        block.height = 16;
        blockSize = block.width + (block.height >> 2); // TODO: change to value of BlockSize
#ifdef NO_PADDING
        pInterpolateInfo->sizeBlock = block;
#endif // NO_PADDING

        for (b = 0; b < 2; b ++)
        {
            predictedMVL0 = *(predictedMVsL0 + 3 + b);
            predictedMVL1 = *(predictedMVsL1 + 3 + b);
            bSAD = *(bestSAD + 3 + b);
            bInitialSAD = bSAD;

            bOff = b << 3;
            pRefCurL0 = pRefL0 + bOff;
            pRefCurL1 = pRefL1 + bOff;
            pCur = cur_mb.mbPtr + bOff;
            mcCurBuff = mcBuff + bOff;

            bOff = b << 1;

            bestMVL0 = cur_mb.MVs[LIST_0]->MotionVectors[bOff];
            bestMVL1 = cur_mb.MVs[LIST_1]->MotionVectors[bOff];
            bMVL0 = bestMVL0;
            bMVL1 = bestMVL1;

#ifdef NO_PADDING
            pInterpolateInfo->pointBlockPos.x = mbPos.x + (b << 3);
            pInterpolateInfo->pointBlockPos.y = mbPos.y;
#endif // NO_PADDING

            REFINE_BIDIR

                if (isRefined)
                {
                    for (i = 0; i < 16; i += 4)
                    {
                        cur_mb.MVs[LIST_0]->MotionVectors[bOff + i] = bMVL0;
                        cur_mb.MVs[LIST_0]->MotionVectors[bOff + i + 1] = bMVL0;
                        cur_mb.MVs[LIST_1]->MotionVectors[bOff + i] = bMVL1;
                        cur_mb.MVs[LIST_1]->MotionVectors[bOff + i + 1] = bMVL1;
                    }
                    if (!ModeRDOpt)
                    {
                        *bestSAD16x16 -= bInitialSAD;
                        *bestSAD16x16 += bSAD;
                    }
                    COPY_BLOCK

                        isRefinedMB = 1;
                    isRefined = 0;
                }
        }

        isRefined |= isRefinedMB;

        if (isRefined && ModeRDOpt)
        {
            prRDCost = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(core_enc, curr_slice, 0 );
            if (prRDCost > bInitailMBSAD)
            {
                MFX_INTERNAL_CPY(cur_mb.MVs[LIST_0]->MotionVectors, initialMotionVectorsL0, 16 * sizeof(H264MotionVector));
                MFX_INTERNAL_CPY(cur_mb.MVs[LIST_1]->MotionVectors, initialMotionVectorsL1, 16 * sizeof(H264MotionVector));
                MFX_INTERNAL_CPY(mcBuff, initialMBMC, 256 * sizeof(PIXTYPE) );
                isRefined = 0;
            }
            else
            {
                *bestSAD16x16 = prRDCost;
            }
        }
        break; // try to refine B_Bi_Bi_8x16

    case MBTYPE_B_8x8: // try to refine B_Bi_8x8
        block.width = 8;
        block.height = 8;
        blockSize = block.width + (block.height >> 2); // TODO: change to value of BlockSize
#ifdef NO_PADDING
        pInterpolateInfo->sizeBlock = block;
#endif // NO_PADDING

        for (b = 0; b < 4; b ++)
        {
            bestSBType = cur_mb.GlobalMacroblockInfo->sbtype[b];

            if (bestSBType == SBTYPE_BIDIR_8x8)
            {
                predictedMVL0 = *(predictedMVsL0 + 5 + b);
                predictedMVL1 = *(predictedMVsL1 + 5 + b);
                bSAD = *(bestSAD + 5 + b);
                bInitialSAD = bSAD;

                bOff = ((b >> 1) << 3) * pitchPixels + ((b & 1) << 3);
                pRefCurL0 = pRefL0 + bOff;
                pRefCurL1 = pRefL1 + bOff;
                pCur = cur_mb.mbPtr + bOff;
                mcCurBuff = mcBuff + (b >> 1) * 8 * 16 + (b & 1) * 8;

                bOff = ((b >> 1) << 3) + ((b&1) << 1);
                bestMVL0 = cur_mb.MVs[LIST_0]->MotionVectors[bOff];
                bestMVL1 = cur_mb.MVs[LIST_1]->MotionVectors[bOff];
                bMVL0 = bestMVL0;
                bMVL1 = bestMVL1;

#ifdef NO_PADDING
                pInterpolateInfo->pointBlockPos.x = mbPos.x + ((b & 1) << 3);
                pInterpolateInfo->pointBlockPos.y = mbPos.y + ((b >> 1) << 3);
#endif // NO_PADDING

                REFINE_BIDIR

                    if (isRefined)
                    {
                        for (i = 0; i < 4; i ++)
                        {
                            cur_mb.MVs[LIST_0]->MotionVectors[bOff] = bMVL0;
                            cur_mb.MVs[LIST_0]->MotionVectors[bOff + 1] = bMVL0;
                            cur_mb.MVs[LIST_0]->MotionVectors[bOff + 4] = bMVL0;
                            cur_mb.MVs[LIST_0]->MotionVectors[bOff + 5] = bMVL0;
                            cur_mb.MVs[LIST_1]->MotionVectors[bOff] = bMVL1;
                            cur_mb.MVs[LIST_1]->MotionVectors[bOff + 1] = bMVL1;
                            cur_mb.MVs[LIST_1]->MotionVectors[bOff + 4] = bMVL1;
                            cur_mb.MVs[LIST_1]->MotionVectors[bOff + 5] = bMVL1;
                        }
                        if (!ModeRDOpt)
                        {
                            *bestSAD16x16 -= bInitialSAD;
                            *bestSAD16x16 += bSAD;
                        }
                        COPY_BLOCK
                            isRefinedMB = 1;
                        isRefined = 0;
                    }
            }
        }

        isRefined |= isRefinedMB;

        if (isRefined && ModeRDOpt)
        {
            prRDCost = H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(core_enc, curr_slice, 0 );
            if (prRDCost > bInitailMBSAD)
            {
                MFX_INTERNAL_CPY(cur_mb.MVs[LIST_0]->MotionVectors, initialMotionVectorsL0, 16 * sizeof(H264MotionVector));
                MFX_INTERNAL_CPY(cur_mb.MVs[LIST_1]->MotionVectors, initialMotionVectorsL1, 16 * sizeof(H264MotionVector));
                MFX_INTERNAL_CPY(mcBuff, initialMBMC, 256 * sizeof(PIXTYPE) );
                isRefined = 0;
            }
            else
            {
                *bestSAD16x16 = prRDCost;
            }
        }
        break;  // try to refine B_Bi_8x8
    }

    return isRefined;
}

//#define FTD_MBQ
//#define FTD_INTRA_DC
#define FTD_INTRA_ABSDEV
//#define FTD_INTRA_SATD

#ifdef FTD_INTRA_DC
inline void MemorySet(Ipp8u *dst, Ipp32s val, Ipp32s length)
{
    memset(dst, val, length);
}

inline void MemorySet(Ipp16u *dst, Ipp32s val, Ipp32s length)
{
    ippsSet_16s((Ipp16s)val, (Ipp16s*)dst, length);
}
#endif //FTD_INTRA_DC


#ifdef FTD_INTRA_ABSDEV

#if PIXBITS == 8
inline void H264ENC_MAKE_NAME(MeanAbsDev16x16)(const PIXTYPE* pSrc, Ipp32s srcStep, Ipp32s *pDst)
{
    ippiMeanAbsDev16x16_8u32s_C1R(pSrc, srcStep, pDst);
}
#elif PIXBITS == 16
inline void H264ENC_MAKE_NAME(MeanAbsDev16x16)(const PIXTYPE* pSrc, Ipp32s srcStep, Ipp32s *pDst)
{
    int    i, j;
    Ipp32s mean = 0, dev = 0;
    const Ipp16u  *p;

    p = pSrc;
    for (i = 0; i < 16; i ++, p += srcStep)
        for (j = 0; j < 16; j ++)
            mean += p[j];
    mean = (mean + 128) >> 8;
    p = pSrc;
    for (i = 0; i < 16; i ++, p += srcStep)
        for (j = 0; j < 16; j ++)
            dev += abs(p[j] - mean);
    *pDst = dev;
}
#endif //PIXBITS

#ifdef FRAME_TYPE_DETECT_DS

#if PIXBITS == 8
inline void H264ENC_MAKE_NAME(MeanAbsDev8x8)(const PIXTYPE* pSrc, Ipp32s srcStep, Ipp32s *pDst)
{
    ippiMeanAbsDev8x8_8u32s_C1R(pSrc, srcStep, pDst);
}
#elif PIXBITS == 16
inline void H264ENC_MAKE_NAME(MeanAbsDev8x8)(const PIXTYPE* pSrc, Ipp32s srcStep, Ipp32s *pDst)
{
    int    i, j;
    Ipp32s mean = 0, dev = 0;
    const Ipp16u  *p;

    p = pSrc;
    for (i = 0; i < 8; i ++, p += srcStep)
        for (j = 0; j < 8; j ++)
            mean += p[j];
    mean = (mean + 32) >> 6;
    p = pSrc;
    for (i = 0; i < 8; i ++, p += srcStep)
        for (j = 0; j < 8; j ++)
            dev += abs(p[j] - mean);
    *pDst = dev;
}
#endif //PIXBITS
#endif //FRAME_TYPE_DETECT_DS
#endif //FTD_INTRA_ABSDEV


#ifdef FRAME_TYPE_DETECT_DS
void H264ENC_MAKE_NAME(ME_FTD)(ME_InfType* meInfo)
{
    Ipp32s cSAD, bSAD, xL, xR, yT, yB, rX, rY, bMVX, bMVY, blockSize;
    PIXTYPE *pCur = meInfo->pCur;
    PIXTYPE *pRef = meInfo->pRef;
    Ipp32s   pitchPixels = meInfo->pitchPixels;
    blockSize = meInfo->block.width + (meInfo->block.height >> 2);
    bMVX = meInfo->bestMV.mvx;
    bMVY = meInfo->bestMV.mvy;
    bSAD = meInfo->bestSAD;
    if (bSAD <= meInfo->threshold)
        goto end;
    xL = MAX(meInfo->xMin, bMVX - meInfo->rX);
    xR = MIN(meInfo->xMax, bMVX + meInfo->rX);
    yT = MAX(meInfo->yMin, bMVY - meInfo->rY);
    yB = MIN(meInfo->yMax, bMVY + meInfo->rY);
    rX = meInfo->rX;
    rY = meInfo->rY;
    switch (meInfo->searchAlgo & 15) {
        case MV_SEARCH_TYPE_SMALL_DIAMOND:
        {
            Ipp32s i, j, k, l, m, n, xPos, yPos;
            static const Ipp32s bdJ[5] = {0, -1, 0, 1, 0}, bdI[5] = {0, 0, -1, 0, 1};
            static const Ipp32s bdN[5] = {4, 3, 3, 3, 3}, bdA[5][4] = {{1, 2, 3, 4}, {1, 2, 4, 0}, {1, 2, 3, 0}, {2, 3, 4, 0}, {3, 4, 1, 0}};
            xPos = bMVX;
            yPos = bMVY;
            l = 0;
            for (;;) {
                n = l;
                l = 0;
                for (m = 0; m < bdN[n]; m ++) {
                    k = bdA[n][m];
                    j = xPos + bdJ[k];
                    i = yPos + bdI[k];
                    if (j >= xL && j <= xR && i >= yT && i <= yB) {
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                        if (cSAD < bSAD) {
                            l = k;
                            bSAD = cSAD;
                            bMVX = j;
                            bMVY = i;
                        }
                    }
                }
                if (l == 0)
                    break;
                xPos += bdJ[l];
                yPos += bdI[l];
            }
        }
        break;
    case MV_SEARCH_TYPE_SQUARE:
    default:
        {
            Ipp32s i, j, k, l, m, n, xPos, yPos;
            static const Ipp32s bdJ[9] = {0, -1, 0, 1, 1, 1, 0, -1, -1}, bdI[9] = {0, -1, -1, -1, 0, 1, 1, 1, 0};
            static const Ipp32s bdN[9] = {8, 5, 3, 5, 3, 5, 3, 5, 3}, bdA[9][8] = {{1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 7, 8, 0, 0, 0}, {1, 2, 3, 0, 0, 0, 0, 0}, {1, 2, 3, 4, 5, 0, 0, 0}, {3, 4, 5, 0, 0, 0, 0, 0}, {3, 4, 5, 6, 7, 0, 0, 0}, {5, 6, 7, 0, 0, 0, 0, 0}, {5, 6, 7, 8, 1, 0, 0, 0}, {7, 8, 1, 0, 0, 0, 0, 0}};
            xPos = bMVX;
            yPos = bMVY;
            l = 0;
            for (;;) {
                n = l;
                l = 0;
                for (m = 0; m < bdN[n]; m ++) {
                    k = bdA[n][m];
                    j = xPos + bdJ[k];
                    i = yPos + bdI[k];
                    if (j >= xL && j <= xR && i >= yT && i <= yB) {
                        cSAD = H264ENC_MAKE_NAME(SAD)(pCur, pitchPixels, MVADJUST(pRef, pitchPixels, j, i), pitchPixels, blockSize);
                        if (cSAD < bSAD) {
                            l = k;
                            bSAD = cSAD;
                            bMVX = j;
                            bMVY = i;
                        }
                    }
                }
                if (l == 0)
                    break;
                xPos += bdJ[l];
                yPos += bdI[l];
            }
        }
        break;
    }
end:
    meInfo->bestMV.mvx = meInfo->bestMV_Int.mvx = (Ipp16s)bMVX << (SUB_PEL_SHIFT + 1);
    meInfo->bestMV.mvy = meInfo->bestMV_Int.mvy = (Ipp16s)bMVY << (SUB_PEL_SHIFT + 1);
    meInfo->bestSAD = bSAD;
}


void H264ENC_MAKE_NAME(ME_CheckCandidate_FTD)(
    ME_InfType* meInfo,
    H264MotionVector& mv)
{
    if (meInfo->candNum >= ME_MAX_CANDIDATES)
        return;
    Ipp32s cSAD, mvx, mvy, i;
    mvx = (mv.mvx + 3) >> (SUB_PEL_SHIFT + 1);
    mvy = (mv.mvy + 3) >> (SUB_PEL_SHIFT + 1);
    if (mvx < meInfo->xMin)
        mvx = meInfo->xMin;
    if (mvx > meInfo->xMax)
        mvx = meInfo->xMax;
    if (mvy < meInfo->yMin)
        mvy = meInfo->yMin;
    if (mvy > meInfo->yMax)
        mvy = meInfo->yMax;
    for (i = 0; i < meInfo->candNum; i ++)
        if ((meInfo->candMV[i].mvx == mvx) && (meInfo->candMV[i].mvy == mvy))
            return;
    meInfo->candMV[meInfo->candNum].mvx = (Ipp16s)mvx;
    meInfo->candMV[meInfo->candNum].mvy = (Ipp16s)mvy;
    meInfo->candNum ++;
    Ipp32s   pitchPixels = meInfo->pitchPixels;
    Ipp32s blockSize = meInfo->block.width + (meInfo->block.height >> 2);
    cSAD = H264ENC_MAKE_NAME(SAD)(meInfo->pCur, pitchPixels, MVADJUST(meInfo->pRef, pitchPixels, mvx, mvy), pitchPixels, blockSize);
    if (cSAD < meInfo->bestSAD) {
        meInfo->bestSAD = cSAD;
        meInfo->bestMV.mvx = (Ipp16s)mvx;
        meInfo->bestMV.mvy = (Ipp16s)mvy;
    }
}

#if PIXBITS == 8
void H264ENC_MAKE_NAME(DownSample)(const PIXTYPE *pSrc, Ipp32s srcStep, PIXTYPE *pDst, Ipp32s dstStep, Ipp32s wD, Ipp32s hD)
{
    Ipp32s i, j;
    for (i = 0; i < hD; i ++) {
#ifdef INTRINSIC_OPT
        __ALIGN16 __m128i  _p_0, _p_1, _p_2, _p_3, _p_zero;
        __ALIGN16 Ipp16u _p_two[8] = {2, 2, 2, 2, 2, 2, 2, 2};
        __ALIGN16 Ipp16u _p_msk[8] = {0xffff, 0, 0xffff, 0, 0xffff, 0, 0xffff, 0};

        _p_zero = _mm_setzero_si128();
        for (j = 0; j < wD; j += 8) {
            _p_0 = _mm_loadl_epi64((__m128i*)(&pSrc[j * 2]));
            _p_1 = _mm_loadl_epi64((__m128i*)(&pSrc[j * 2 + srcStep]));
            _p_0 = _mm_unpacklo_epi8(_p_0, _p_zero);
            _p_1 = _mm_unpacklo_epi8(_p_1, _p_zero);
            _p_0 = _mm_add_epi16(_p_0, _p_1);
            _p_1 = _mm_srli_si128(_p_0, 2);
            _p_0 = _mm_add_epi16(_p_0, _p_1);
            _p_0 = _mm_add_epi16(_p_0, *(__m128i*)_p_two);
            _p_0 = _mm_srli_epi16(_p_0, 2);
            _p_0 = _mm_and_si128(_p_0, *(__m128i*)_p_msk);
            _p_0 = _mm_packs_epi32(_p_0, _p_0);
            _p_0 = _mm_packus_epi16(_p_0, _p_0);
            *(Ipp32s*)(pDst + j) = _mm_cvtsi128_si32(_p_0);
            _p_2 = _mm_loadl_epi64((__m128i*)(&pSrc[j * 2 + 8]));
            _p_3 = _mm_loadl_epi64((__m128i*)(&pSrc[j * 2 + 8 + srcStep]));
            _p_2 = _mm_unpacklo_epi8(_p_2, _p_zero);
            _p_3 = _mm_unpacklo_epi8(_p_3, _p_zero);
            _p_2 = _mm_add_epi16(_p_2, _p_3);
            _p_3 = _mm_srli_si128(_p_2, 2);
            _p_2 = _mm_add_epi16(_p_2, _p_3);
            _p_2 = _mm_add_epi16(_p_2, *(__m128i*)_p_two);
            _p_2 = _mm_srli_epi16(_p_2, 2);
            _p_2 = _mm_and_si128(_p_2, *(__m128i*)_p_msk);
            _p_2 = _mm_packs_epi32(_p_2, _p_2);
            _p_2 = _mm_packus_epi16(_p_2, _p_2);
            *(Ipp32s*)(pDst + j + 4) = _mm_cvtsi128_si32(_p_2);
        }
#else //INTRINSIC_OPT
        for (j = 0; j < wD; j ++) {
            pDst[j] = (Ipp8u)((pSrc[j * 2] + pSrc[j * 2 + 1] + pSrc[j * 2 + srcStep] + pSrc[j * 2 + 1 + srcStep] + 2) >> 2);
        }
#endif //INTRINSIC_OPT
        pDst += dstStep;
        pSrc += srcStep * 2;
    }
}
#elif PIXBITS == 16
void H264ENC_MAKE_NAME(DownSample)(const PIXTYPE *pSrc, Ipp32s srcStep, PIXTYPE *pDst, Ipp32s dstStep, Ipp32s wD, Ipp32s hD)
{
    Ipp32s i, j;
    for (i = 0; i < hD; i ++) {
        for (j = 0; j < wD; j ++) {
            pDst[j] = (Ipp16u)((pSrc[j * 2] + pSrc[j * 2 + 1] + pSrc[j * 2 + srcStep] + pSrc[j * 2 + 1 + srcStep] + 2) >> 2);
        }
        pDst += dstStep;
        pSrc += srcStep * 2;
    }
}
#endif //PIXBITS
#endif //FRAME_TYPE_DETECT_DS

void H264ENC_MAKE_NAME(H264CoreEncoder_FrameTypeDetect)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
#ifdef FRAME_TYPE_DETECT_DS
    Ipp32s  i, j, stepDS, wDS, hDS, BestSAD_Inter, BestSAD_Intra;

    wDS  = core_enc->m_pCurrentFrame->m_paddedParsedFrameDataSize.width >> 1;
    hDS  = core_enc->m_pCurrentFrame->m_paddedParsedFrameDataSize.height >> 1;
    stepDS = wDS + 16;
    H264ENC_MAKE_NAME(DownSample)(core_enc->m_pCurrentFrame->m_pYPlane, core_enc->m_pCurrentFrame->m_pitchPixels, core_enc->m_pCurrentFrame->m_pYPlane_DS + 8 * stepDS + 8, stepDS, wDS, hDS);
    H264ENC_MAKE_NAME(ExpandPlane)(core_enc->m_pCurrentFrame->m_pYPlane_DS + 8 * stepDS + 8, wDS, hDS, stepDS, 8);
#endif
    if (core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC)
        return;
    H264EncoderFrameType *pRefFrame, *pF;
    bool refCPB = false;
    pRefFrame = pF = core_enc->m_dpb.m_pTail;
    while (pF) {
        if (pF != core_enc->m_pCurrentFrame) {
            if (core_enc->m_pCurrentFrame->m_PicCodType == PREDPIC) {
                if ((pF->m_PicCodType != BPREDPIC) && (pRefFrame->m_PicOrderCnt[0] < pF->m_PicOrderCnt[0]) && (pF->m_PicOrderCnt[0] < core_enc->m_pCurrentFrame->m_PicOrderCnt[0]))
                    pRefFrame = pF;
            } else
                if ((pF->m_PicCodType != BPREDPIC || pF->m_RefPic) && (pRefFrame->m_PicOrderCnt[0] < pF->m_PicOrderCnt[0]) && (pF->m_PicOrderCnt[0] < core_enc->m_pCurrentFrame->m_PicOrderCnt[0]))
                    pRefFrame = pF;
        }
        pF = pF->m_pPreviousFrame;
    }
    if (!pRefFrame)
        return;
    pF = core_enc->m_cpb.m_pTail;
    while (pF) {
        if (pF != core_enc->m_pCurrentFrame) {
            if (core_enc->m_pCurrentFrame->m_PicCodType == PREDPIC) {
                if ((pF->m_PicCodType != BPREDPIC) && (pRefFrame->m_PicOrderCnt[0] < pF->m_PicOrderCnt[0]) && (pF->m_PicOrderCnt[0] < core_enc->m_pCurrentFrame->m_PicOrderCnt[0])) {
                    pRefFrame = pF;
                    refCPB = true;
                }
            } else
                if ((pF->m_PicCodType != BPREDPIC || pF->m_RefPic) && (pRefFrame->m_PicOrderCnt[0] < pF->m_PicOrderCnt[0]) && (pF->m_PicOrderCnt[0] < core_enc->m_pCurrentFrame->m_PicOrderCnt[0])) {
                    pRefFrame = pF;
                    refCPB = true;
                }
        }
        pF = pF->m_pPreviousFrame;
    }
    if (!pRefFrame)
        return;
#ifdef FRAME_TYPE_DETECT_DS
    Ipp32s hMB = core_enc->m_HeightInMBs;
    if (core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE)
        hMB += hMB;
    Ipp32u uNumMBs = hMB * core_enc->m_WidthInMBs;
    Ipp32s costIntra = 0, costInter = 0, costBest = 0, nMBi = 0;
    #define Inter_Intra_Ratio 0.55
    #define To_I_Ratio 0.75
    #define To_P_Ratio 0.3
    Ipp32s thresh_IntraMB_I = (int)(To_I_Ratio * uNumMBs), thresh_IntraMB_P = (int)(To_P_Ratio * uNumMBs);
    ME_InfType meInfo;
    meInfo.bit_depth_luma = core_enc->m_PicParamSet->bit_depth_luma;
    meInfo.rX = core_enc->m_info.me_search_x >> 1;
    meInfo.rY = core_enc->m_info.me_search_y >> 1;
    meInfo.searchAlgo = MV_SEARCH_TYPE_SMALL_DIAMOND;
    meInfo.block.width = 8;
    meInfo.block.height = 8;
    meInfo.pitchPixels = stepDS;
    Ipp32s curr_addr = 0;
    for (i = 0; i < hMB; i ++) {
        for (j = 0; j < core_enc->m_WidthInMBs; j ++) {
            meInfo.xMin = -MIN(63, j * 8 + 7);
            meInfo.xMax =  MIN(63, (wDS - 8) - j * 8 + 7);
            meInfo.yMin = -MIN(63, i * 8 + 7);
            meInfo.yMax =  MIN(63, (hDS - 8) - i * 8 + 7);
            // Inter_8x8
            PIXTYPE *pCur, *pRef;
            pCur = core_enc->m_pCurrentFrame->m_pYPlane_DS + 8 * stepDS + 8 + j * 8 + i * 8 * stepDS;
            pRef = pRefFrame->m_pYPlane_DS + 8 * stepDS + 8 + j * 8 + i * 8 * stepDS;
            meInfo.pCur = pCur;
            meInfo.pRef = pRef;
            meInfo.bestSAD = MAX_SAD;
            meInfo.candNum = 0;
            H264ENC_MAKE_NAME(ME_CheckCandidate_FTD)(&meInfo, pRefFrame->m_mbinfo.MV[0][curr_addr].MotionVectors[0]);
            if (i > 0)
                 H264ENC_MAKE_NAME(ME_CheckCandidate_FTD)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][curr_addr - core_enc->m_WidthInMBs].MotionVectors[0]);
            if (j > 0)
                 H264ENC_MAKE_NAME(ME_CheckCandidate_FTD)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][curr_addr - 1].MotionVectors[0]);
            if (i > 0 && j > 0)
                 H264ENC_MAKE_NAME(ME_CheckCandidate_FTD)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][curr_addr - core_enc->m_WidthInMBs - 1].MotionVectors[0]);
            if (i > 0 && j < core_enc->m_WidthInMBs - 1)
                 H264ENC_MAKE_NAME(ME_CheckCandidate_FTD)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][curr_addr - core_enc->m_WidthInMBs + 1].MotionVectors[0]);
            H264ENC_MAKE_NAME(ME_FTD)(&meInfo);
            BestSAD_Inter = meInfo.bestSAD;
            costInter += BestSAD_Inter;
            // Intra_8x8
#ifdef FTD_INTRA_DC
            {
                __ALIGN16 PIXTYPE pred[64];
                Ipp32u uDC = 0;
                bool topAvailable = i > 0;
                bool leftAvailable = j > 0;
                if (!leftAvailable && !topAvailable) {
                    uDC = 1 << (core_enc->m_PicParamSet->bit_depth_luma - 1);
                } else {
                    if (topAvailable) {
                        PIXTYPE *pAbove = pCur - stepDS;
                        for (int i = 0; i < 8; i ++)
                            uDC += pAbove[i];
                    }
                    if (leftAvailable) {
                        PIXTYPE *pLeft = pCur - 1;
                        for (int i = 0; i < 8; i ++) {
                            uDC += *pLeft;
                            pLeft += stepDS;
                        }
                    }
                    if (!topAvailable || !leftAvailable)
                        uDC <<= 1;
                    uDC = (uDC + 8) >> 4;
                }
                MemorySet(pred, (PIXTYPE)uDC, 64);
                BestSAD_Intra = SAD8x8(pCur, stepDS, pred, 8);
            }
#elif defined(FTD_INTRA_ABSDEV)
            H264ENC_MAKE_NAME(MeanAbsDev8x8)(pCur, stepDS, &BestSAD_Intra);
#endif //FTD_INTRA_ABSDEV
            costIntra += BestSAD_Intra;
            if (BestSAD_Intra < BestSAD_Inter) {
                costBest += BestSAD_Intra;
                nMBi ++;
                if (nMBi >= thresh_IntraMB_I)
                    goto type_detect;
            } else
                costBest += BestSAD_Inter;
            core_enc->m_pCurrentFrame->m_mbinfo.MV[0][curr_addr].MotionVectors[0] = meInfo.bestMV;
            curr_addr ++;
        }
    }
#else //FRAME_TYPE_DETECT_DS
    Ipp32s is_cur_mb_field = core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE;
    Ipp32s pitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels << is_cur_mb_field;
    EnumSliceType default_slice_type = (core_enc->m_pCurrentFrame->m_PicCodType == PREDPIC) ? PREDSLICE : (core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC) ? INTRASLICE : BPREDSLICE;
    core_enc->m_is_cur_pic_afrm = (Ipp32s)(core_enc->m_pCurrentFrame->m_PictureStructureForDec == AFRM_STRUCTURE);
    core_enc->m_PicParamSet->bit_depth_luma = core_enc->m_SeqParamSet->bit_depth_luma;
    Ipp32u uNumMBs = core_enc->m_HeightInMBs * core_enc->m_WidthInMBs;
    Ipp32u totMB = uNumMBs;
    if (core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE)
        uNumMBs >>= 1;
    Ipp32s costIntra = 0, costInter = 0, costBest = 0, nMBi = 0, slice;
#if defined(FTD_INTRA_DC) || defined(FTD_INTRA_ABSDEV)
    #define Inter_Intra_Ratio 0.55
    #define To_I_Ratio 0.75 // 0.8 for 1/4
    #define To_P_Ratio 0.33
#else //defined(FTD_INTRA_DC) || defined(FTD_INTRA_ABSDEV)
#ifdef FTD_INTRA_SATD
    #define Inter_Intra_Ratio 0.4
    #define To_I_Ratio 0.5
    #define To_P_Ratio 0.35
#else //FTD_INTRA_SATD
    #define Inter_Intra_Ratio 0.5
    #define To_I_Ratio 0.75
    #define To_P_Ratio 0.27
#endif //FTD_INTRA_SATD
#endif ////defined(FTD_INTRA_DC) || defined(FTD_INTRA_ABSDEV)
#ifndef FTD_MBQ
    Ipp32s thresh_IntraMB_I = (int)(To_I_Ratio * totMB), thresh_IntraMB_P = (int)(To_P_Ratio * totMB);
#else
    Ipp32s thresh_IntraMB_I = (int)(To_I_Ratio * 0.25 * totMB), thresh_IntraMB_P = (int)(To_P_Ratio * 0.25 * totMB);
#endif
    Ipp32s saveAnalyse = core_enc->m_Analyse;
#ifdef FTD_INTRA_SATD
    core_enc->m_Analyse = 0;
#else
    core_enc->m_Analyse = ANALYSE_SAD;
#endif
#ifndef UMC_RESTRICTED_CODE_MBT
#if defined(MB_THREADING) && !defined(MB_THREADING_VM)
    if (core_enc->useMBT) {
        Ipp32s i;
        slice = 0;
        core_enc->m_pReconstructFrame = core_enc->m_pCurrentFrame;
        H264ENC_MAKE_NAME(H264CoreEncoder_SetSliceHeaderCommon)(state, core_enc->m_pCurrentFrame);
        H264ENC_MAKE_NAME(H264CoreEncoder_Start_Picture)(state, &core_enc->m_PicClass, core_enc->m_pCurrentFrame->m_PicCodType);
        for (i = 0; i < core_enc->m_info.numThreads; i ++) {
            core_enc->m_Slices_MBT[i].m_slice_qp_delta = core_enc->m_Slices[0].m_slice_qp_delta;
            core_enc->m_Slices_MBT[i].m_slice_number = 0;
            core_enc->m_Slices_MBT[i].m_slice_type = default_slice_type;
            core_enc->m_Slices_MBT[i].m_InitialOffset = core_enc->m_InitialOffsets[core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]];
            core_enc->m_Slices_MBT[i].m_is_cur_mb_field = core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE;
            core_enc->m_Slices_MBT[i].m_is_cur_mb_bottom_field = core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index] == 1;
            core_enc->m_Slices_MBT[i].m_use_transform_for_intra_decision = 0;
        }
        Ipp32s row = 0;
        for (i = 0; i < core_enc->m_HeightInMBs; i ++)
            core_enc->mbReady_MBT[i] = -1;
#ifdef _OPENMP
#pragma omp parallel num_threads(core_enc->m_info.numThreads)
#endif // _OPENMP
        {
            Ipp32s tRow, tCol, tNum;
#ifdef _OPENMP
            tNum = omp_get_thread_num();
#else
            tNum = 0;
#endif // _OPENMP
            tRow = row;
            H264SliceType *curr_slice = &core_enc->m_Slices_MBT[tNum];
            H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
            Ipp32s costIntra_t = 0, costInter_t = 0, costBest_t = 0, nMBi_t = 0;
            ME_InfType meInfo;
            meInfo.bit_depth_luma = core_enc->m_PicParamSet->bit_depth_luma;
            meInfo.rX = core_enc->m_info.me_search_x;
            meInfo.rY = core_enc->m_info.me_search_y;
            meInfo.flags = ANALYSE_SAD;
            meInfo.searchAlgo = MV_SEARCH_TYPE_SMALL_DIAMOND + MV_SEARCH_TYPE_SUBPEL_DIAMOND * 32;
            meInfo.block.width = 16;
            meInfo.block.height = 16;
            meInfo.pitchPixels = pitchPixels;
#ifdef FRAME_INTERPOLATION
            meInfo.planeSize = pRefFrame->m_PlaneSize;
#endif
            while (tRow < core_enc->m_HeightInMBs) {
#ifdef _OPENMP
#pragma omp critical
#endif // _OPENMP
                {
                    tRow = row;
                    row ++;
#ifdef MB_THREADING_TW
                    if (tRow < core_enc->m_HeightInMBs)
                        for (tCol = 0; tCol < core_enc->m_WidthInMBs; tCol ++)
                            omp_set_lock(core_enc->mLockMT + tRow * core_enc->m_WidthInMBs + tCol);
#endif // MB_THREADING_TW
                }
                if (tRow < core_enc->m_HeightInMBs) {
                    cur_mb.mbPitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels << curr_slice->m_is_cur_mb_field;
                    for (tCol = 0; tCol < core_enc->m_WidthInMBs; tCol ++) {
                        Ipp32u uMB = tRow * core_enc->m_WidthInMBs + tCol;
                        cur_mb.uMB = uMB;
                        cur_mb.uMBx = tCol;
                        cur_mb.uMBy = tRow;
                        cur_mb.mbPtr = core_enc->m_pCurrentFrame->m_pYPlane + core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
                        H264ENC_MAKE_NAME(H264CoreEncoder_LoadPredictedMBInfo)(state, curr_slice);
                        H264ENC_MAKE_NAME(H264CoreEncoder_UpdateCurrentMBInfo)(state, curr_slice);
                        cur_mb.lumaQP = getLumaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                        cur_mb.lumaQP51 = getLumaQP51(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                        pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 0);
                        T_EncodeMBOffsets *pMBOffset = &core_enc->m_pMBOffsets[uMB];
                        Ipp32s iQP = cur_mb.lumaQP51;
                        Ipp32s BestSAD_Inter, BestSAD_Intra;
                        if (!refCPB) {
                            meInfo.xMin = pMBOffset->uMVLimits_L;
                            meInfo.xMax = pMBOffset->uMVLimits_R;
                            meInfo.yMin = pMBOffset->uMVLimits_U;
                            meInfo.yMax = pMBOffset->uMVLimits_D;
                        } else {
                            meInfo.xMin = -MIN(127, cur_mb.uMBx * 16 + 0);
                            meInfo.xMax =  MIN(127, (core_enc->m_pCurrentFrame->m_paddedParsedFrameDataSize.width - 16) - cur_mb.uMBx * 16 + 0);
                            meInfo.yMin = -MIN(127, cur_mb.uMBy * 16 + 0);
                            meInfo.yMax =  MIN(127, (core_enc->m_pCurrentFrame->m_paddedParsedFrameDataSize.height - 16) - cur_mb.uMBy * 16 + 0);
                        }
                        meInfo.pRDQM = glob_RDQM[iQP];
                        meInfo.threshold = core_enc->m_BestOf5EarlyExitThres[iQP];
                        meInfo.pCur = cur_mb.mbPtr;
                        meInfo.pRef = pRefFrame->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[core_enc->m_field_index];
#ifdef MB_THREADING_TW
                        if ((tRow > 0) && (tCol < (core_enc->m_WidthInMBs - 1))) {
                            omp_set_lock(core_enc->mLockMT + (tRow - 1) * core_enc->m_WidthInMBs + tCol + 1);
                            omp_unset_lock(core_enc->mLockMT + (tRow - 1) * core_enc->m_WidthInMBs + tCol + 1);
                        }
#else
                        if ((tRow > 0) && (tCol < (core_enc->m_WidthInMBs - 1)))
                            while (core_enc->mbReady_MBT[tRow-1] <= tCol);
#endif // MB_THREADING_TW
                        Ipp32s left_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_A;
                        Ipp32s top_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_B;
                        Ipp32s topright_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_C;
                        Ipp32s topleft_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_D;
                        if (left_addr < 0 && top_addr < 0) {
                            meInfo.predictedMV = null_mv;
                        } else if (left_addr < 0) {
                            meInfo.predictedMV = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][top_addr].MotionVectors[0];
                        } else {
                            if (topright_addr < 0)
                                meInfo.predictedMV = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][left_addr].MotionVectors[0];
                            else {
                                H264MotionVector mvA, mvB, mvC;
                                mvA = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][left_addr].MotionVectors[0];
                                mvB = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][top_addr].MotionVectors[0];
                                mvC = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][topright_addr].MotionVectors[0];
                                meInfo.predictedMV.mvx = MIN(mvA.mvx, mvB.mvx) ^ MIN(mvB.mvx, mvC.mvx) ^ MIN(mvC.mvx, mvA.mvx);
                                meInfo.predictedMV.mvy = MIN(mvA.mvy, mvB.mvy) ^ MIN(mvB.mvy, mvC.mvy) ^ MIN(mvC.mvy, mvA.mvy);
                            }
                        }
                        // Inter_16x16
                        meInfo.bestSAD = MAX_SAD;
                        meInfo.candNum = 0;
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, pRefFrame->m_mbinfo.MV[0][uMB].MotionVectors[0]);
                        if (left_addr >= 0)
                            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][left_addr].MotionVectors[0]);
                        if (top_addr >= 0)
                            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][top_addr].MotionVectors[0]);
                        if (topright_addr >= 0)
                            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][topright_addr].MotionVectors[0]);
                        if (topleft_addr >= 0)
                            H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][topleft_addr].MotionVectors[0]);
                        H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                        BestSAD_Inter = meInfo.bestSAD;
                        costInter_t += BestSAD_Inter;
                        //Intra_16x16
#ifdef FTD_INTRA_DC
                        {
                            PIXTYPE *pSrc = cur_mb.mbPtr, *pRef = cur_mb.mbPtr;
                            Ipp32u uDC = 0;
                            bool topAvailable = above_addr >= 0;
                            bool leftAvailable = left_addr >= 0;
                            if (!leftAvailable && !topAvailable) {
                                uDC = 1 << (core_enc->m_PicParamSet->bit_depth_luma - 1);
                            } else {
                                if (topAvailable) {
                                    PIXTYPE *pAbove = pRef - pitchPixels;
                                    for (i = 0; i < 16; i ++)
                                        uDC += pAbove[i];
                                }
                                if (leftAvailable) {
                                    PIXTYPE *pLeft = pRef - 1;
                                    for (i = 0; i < 16; i ++) {
                                        uDC += *pLeft;
                                        pLeft += pitchPixels;
                                    }
                                }
                                if (!topAvailable || !leftAvailable)
                                    uDC <<= 1;
                                uDC = (uDC + 16) >> 5;
                            }
                            MemorySet(cur_mb.mb16x16.prediction, (PIXTYPE)uDC, 256);
                            BestSAD_Intra = BITS_COST(3, glob_RDQM[iQP]) + SAD16x16(pSrc, pitchPixels, cur_mb.mb16x16.prediction, 16);
                        }
#elif defined(FTD_INTRA_ABSDEV)
                        H264ENC_MAKE_NAME(MeanAbsDev16x16)(cur_mb.mbPtr, pitchPixels, &BestSAD_Intra);
                        BestSAD_Intra += BITS_COST(3, glob_RDQM[iQP]);
#else
                        T_AIMode BestMode;
                        BestSAD_Intra = AIModeSelectOneMB_16x16(curr_slice, cur_mb.mbPtr, cur_mb.mbPtr, cur_mb.mbPitchPixels, &BestMode, cur_mb.mb16x16.prediction);
#endif
                        costIntra_t += BestSAD_Intra;
                        if (BestSAD_Intra < BestSAD_Inter) {
                            costBest_t += BestSAD_Intra;
                            nMBi_t ++;
                            // if (nMBi_t >= thresh_IntraMB_I) {
                            //     goto type_detect;
                            // }
                        } else
                            costBest_t += BestSAD_Inter;
                        cur_mb.MVs[LIST_0]->MotionVectors[0] = meInfo.bestMV;
#ifdef MB_THREADING_TW
                        omp_unset_lock(core_enc->mLockMT + tRow * core_enc->m_WidthInMBs + tCol);
#endif // MB_THREADING_TW
                        core_enc->mbReady_MBT[tRow] ++;
                    }
                }
                tRow ++;
            }
#ifdef _OPENMP
#pragma omp critical
#endif // _OPENMP
            {
                costIntra += costIntra_t;
                costInter += costInter_t;
                costBest += costBest;
                nMBi += nMBi_t;
            }
        }
        goto type_detect;
    }
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT
    ME_InfType meInfo;
    meInfo.bit_depth_luma = core_enc->m_PicParamSet->bit_depth_luma;
    meInfo.rX = core_enc->m_info.me_search_x;
    meInfo.rY = core_enc->m_info.me_search_y;
    meInfo.flags = ANALYSE_SAD;
    meInfo.searchAlgo = MV_SEARCH_TYPE_SMALL_DIAMOND + MV_SEARCH_TYPE_SUBPEL_DIAMOND * 32;
    meInfo.block.width = 16;
    meInfo.block.height = 16;
    meInfo.pitchPixels = pitchPixels;
#ifdef FRAME_INTERPOLATION
    meInfo.planeSize = pRefFrame->m_PlaneSize; //ppRefPicList[0]->m_PlaneSize;
#endif
    for (core_enc->m_field_index = 0; core_enc->m_field_index <= (Ipp8u)(core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE); core_enc->m_field_index ++) {
        core_enc->m_pReconstructFrame = core_enc->m_pCurrentFrame;
        H264ENC_MAKE_NAME(H264CoreEncoder_SetSliceHeaderCommon)(state, core_enc->m_pCurrentFrame);
        H264ENC_MAKE_NAME(H264CoreEncoder_Start_Picture)(state, &core_enc->m_PicClass, core_enc->m_pCurrentFrame->m_PicCodType);
        Ipp32s slice_qp_delta_default = core_enc->m_Slices[0].m_slice_qp_delta;
        //UpdateRefPicListCommon();

#ifndef NO_PADDING
        if (!(pRefFrame->m_PicCodType == INTRAPIC || pRefFrame->m_PicCodType == PREDPIC || (core_enc->m_info.treat_B_as_reference && pRefFrame->m_RefPic)))
        {
            Ipp32s shift_x = 0;
            Ipp32s shift_y = 0;

            switch( core_enc->m_PicParamSet->chroma_format_idc ){
                case 1:
                    shift_x=shift_y=1;
                    break;
                case 2:
                    shift_x=1;
                    shift_y=0;
                    break;
                case 3:
                    shift_x=shift_y=0;
                    break;
            }

            Ipp32s chromaPadding = LUMA_PADDING >> 1;
            /*if (VideoData_GetColorFormat(&pRefFrame->m_data) == YUV422
#ifdef ALPHA_BLENDING_H264
                || VideoData_GetColorFormat(&pRefFrame->m_data) == YUV422A   // TODO: resolve chromaPadding for YUV422
#endif
                )
                chromaPadding = LUMA_PADDING;*/
            switch (pRefFrame->m_PictureStructureForDec){
            case FRM_STRUCTURE:
                H264ENC_MAKE_NAME(ExpandPlane)(pRefFrame->m_pYPlane, core_enc->m_WidthInMBs*16, core_enc->m_HeightInMBs*16, pitchPixels, LUMA_PADDING);
#ifdef USE_NV12
                H264ENC_MAKE_NAME(ExpandPlane_NV12)(
                    pRefFrame->m_pUPlane,
                    core_enc->m_WidthInMBs * 16,
                    core_enc->m_HeightInMBs * 16 >> shift_y,
                    pitchPixels,
                    chromaPadding);
#else
                if(core_enc->m_PicParamSet->chroma_format_idc){
                    H264ENC_MAKE_NAME(ExpandPlane)(
                        pRefFrame->m_pUPlane,
                        core_enc->m_WidthInMBs * 16 >> shift_x,
                        core_enc->m_HeightInMBs * 16 >> shift_y,
                        pitchPixels,
                        chromaPadding);
                    H264ENC_MAKE_NAME(ExpandPlane)(
                        pRefFrame->m_pVPlane,
                        core_enc->m_WidthInMBs * 16 >> shift_x,
                        core_enc->m_HeightInMBs * 16 >> shift_y,
                        pitchPixels,
                        chromaPadding);
                }
#endif
                break;
            case AFRM_STRUCTURE:
                if(!shift_y) shift_y=1;
                else shift_y <<= 1;
                H264ENC_MAKE_NAME(ExpandPlane)(pRefFrame->m_pYPlane, core_enc->m_WidthInMBs*16, core_enc->m_HeightInMBs*16>>1, pitchPixels*2, LUMA_PADDING);
                if(core_enc->m_PicParamSet->chroma_format_idc){
                    H264ENC_MAKE_NAME(ExpandPlane)(pRefFrame->m_pUPlane, core_enc->m_WidthInMBs*16>>shift_x, core_enc->m_HeightInMBs*16>>shift_y, pitchPixels*2, chromaPadding);
                    H264ENC_MAKE_NAME(ExpandPlane)(pRefFrame->m_pVPlane, core_enc->m_WidthInMBs*16>>shift_x, core_enc->m_HeightInMBs*16>>shift_y, pitchPixels*2, chromaPadding);
                }
                H264ENC_MAKE_NAME(ExpandPlane)(pRefFrame->m_pYPlane + pitchPixels, core_enc->m_WidthInMBs*16, core_enc->m_HeightInMBs*16>>1, pitchPixels*2, LUMA_PADDING);
                if(core_enc->m_PicParamSet->chroma_format_idc){
                    H264ENC_MAKE_NAME(ExpandPlane)(pRefFrame->m_pUPlane + pitchPixels, core_enc->m_WidthInMBs*16>>shift_x, core_enc->m_HeightInMBs*16>>shift_y, pitchPixels*2, chromaPadding);
                    H264ENC_MAKE_NAME(ExpandPlane)(pRefFrame->m_pVPlane + pitchPixels, core_enc->m_WidthInMBs*16>>shift_x, core_enc->m_HeightInMBs*16>>shift_y, pitchPixels*2, chromaPadding);
                }
                break;
            case FLD_STRUCTURE:
                H264ENC_MAKE_NAME(ExpandPlane)(pRefFrame->m_pYPlane + (pitchPixels >> 1)*pRefFrame->m_bottom_field_flag[core_enc->m_field_index],
                    core_enc->m_WidthInMBs*16, core_enc->m_HeightInMBs*16, pitchPixels, LUMA_PADDING);
                if(core_enc->m_PicParamSet->chroma_format_idc) {
#ifdef USE_NV12
                    H264ENC_MAKE_NAME(ExpandPlane_NV12)(
                        pRefFrame->m_pUPlane + (pitchPixels >> 1)*pRefFrame->m_bottom_field_flag[core_enc->m_field_index],
                        core_enc->m_WidthInMBs * 16,
                        core_enc->m_HeightInMBs * 16 >> shift_y,
                        pitchPixels,
                        chromaPadding);
#else // USE_NV12
                    H264ENC_MAKE_NAME(ExpandPlane)(pRefFrame->m_pUPlane + (pitchPixels >> 1)*pRefFrame->m_bottom_field_flag[core_enc->m_field_index],
                        core_enc->m_WidthInMBs*16>>shift_x, core_enc->m_HeightInMBs*16>>shift_y, pitchPixels, chromaPadding);
                    H264ENC_MAKE_NAME(ExpandPlane)(pRefFrame->m_pVPlane + (pitchPixels >> 1)*pRefFrame->m_bottom_field_flag[core_enc->m_field_index],
                        core_enc->m_WidthInMBs*16>>shift_x, core_enc->m_HeightInMBs*16>>shift_y, pitchPixels, chromaPadding);
#endif // USE_NV12
                }
                break;
            }
        }
#endif // NO_PADDING

#ifdef FRAMETYPE_DETECT_ST
#pragma omp parallel for private(slice) firstprivate(meInfo)
#endif
        for (slice = (Ipp32s)core_enc->m_info.num_slices * core_enc->m_field_index; slice < core_enc->m_info.num_slices * (core_enc->m_field_index + 1); slice ++) {
            core_enc->m_Slices[slice].m_slice_qp_delta = (Ipp8s)slice_qp_delta_default;
            core_enc->m_Slices[slice].m_slice_number = slice;
            core_enc->m_Slices[slice].m_slice_type = default_slice_type;
            //UpdateRefPicList(core_enc->m_Slices + slice, core_enc->m_pCurrentFrame->GetRefPicLists(slice), m_SliceHeader, &m_ReorderInfoL0, &m_ReorderInfoL1);
            H264SliceType *curr_slice = &core_enc->m_Slices[slice];
            Ipp32u uFirstMB = core_enc->m_field_index * uNumMBs;
            curr_slice->m_InitialOffset = core_enc->m_InitialOffsets[core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]];
            curr_slice->m_is_cur_mb_field = is_cur_mb_field;
            curr_slice->m_is_cur_mb_bottom_field = core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index] == 1;
            //curr_slice->m_use_transform_for_intra_decision = 0;
            H264CurrentMacroblockDescriptorType &cur_mb = curr_slice->m_cur_mb;
            cur_mb.mbPitchPixels = pitchPixels;

//            __ALIGN16 Ipp16s resPredBuf[256];
            Ipp16s *resPredBuf = cur_mb.m_pResPredDiff;
            Ipp32s resPredFlag = meInfo.residualPredictionFlag = 0;
            Ipp32s resPitchPixels = core_enc->m_WidthInMBs * 16;
            //    if (!core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag)
            if (curr_slice->m_adaptive_residual_prediction_flag)
              resPredFlag =  1;
            else if (curr_slice->m_default_residual_prediction_flag)
              resPredFlag =  2;

            Ipp32s baseMode = 0;
            if (!core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag) {
                if (curr_slice->m_adaptive_prediction_flag)
                    baseMode = 1;
                else if (curr_slice->m_default_base_mode_flag)
                    baseMode = 2;
            }

#ifdef NO_PADDING
            meInfo.interpolateInfo.pSrc[0] = pRefFrame->m_pYPlane + curr_slice->m_InitialOffset[core_enc->m_field_index] + (core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]) * (pitchPixels >> 1);
            meInfo.interpolateInfo.sizeFrame.width = core_enc->m_WidthInMBs << 4;
            meInfo.interpolateInfo.sizeFrame.height = core_enc->m_HeightInMBs << 4;
            meInfo.interpolateInfo.srcStep = meInfo.pitchPixels;
            meInfo.interpolateInfo.dstStep = 16;
#endif // NO_PADDING
            for (Ipp32u uMB = uFirstMB; uMB < uFirstMB + uNumMBs; uMB ++) {
                if (core_enc->m_pCurrentFrame->m_mbinfo.mbs[uMB].slice_id != slice)
                    continue;
                cur_mb.uMB = uMB;
                cur_mb.uMBx = (Ipp16u)(uMB % core_enc->m_WidthInMBs);
                cur_mb.uMBy = (Ipp16u)(uMB / core_enc->m_WidthInMBs);
                if (core_enc->m_field_index)
                    cur_mb.uMBy -= core_enc->m_HeightInMBs;
                cur_mb.mbPtr = core_enc->m_pCurrentFrame->m_pYPlane + core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field];
                H264ENC_MAKE_NAME(H264CoreEncoder_LoadPredictedMBInfo)(state, curr_slice);
                H264ENC_MAKE_NAME(H264CoreEncoder_UpdateCurrentMBInfo)(state, curr_slice);
                cur_mb.lumaQP = getLumaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                cur_mb.lumaQP51 = getLumaQP51(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                T_EncodeMBOffsets *pMBOffset = &core_enc->m_pMBOffsets[uMB];
                //H264EncoderFrameType **ppRefPicList = GetRefPicList(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_RefPicList;
                //Ipp8s *pFields = GetRefPicList(curr_slice, LIST_0, is_cur_mb_field, uMB & 1)->m_Prediction;
                Ipp32s iQP = cur_mb.lumaQP51;
                Ipp32s BestSAD_Inter = MAX_SAD, BestSAD_Intra;
                //H264MotionVector *mvs = cur_mb.MVs[LIST_0]->MotionVectors;
                //T_RefIdx *refs = cur_mb.RefIdxs[LIST_0]->RefIdxs;
                //MBTypeValue *sbt = cur_mb.GlobalMacroblockInfo->sbtype;

                Ipp32s tryBaseMode = baseMode;
                if (tryBaseMode) {
                    H264ENC_MAKE_NAME(H264CoreEncoder_setInCropWindow)(state, curr_slice);
                    if (!pGetMBCropFlag(cur_mb.GlobalMacroblockInfo))
                        tryBaseMode = 0;
                }

#ifdef FTD_MBQ
                Ipp32s i;
                if (cur_mb.uMBx & 1 || cur_mb.uMBy & 1) {
                    cur_mb.MVs[LIST_0]->MotionVectors[0] = meInfo.predictedMV;
                    continue;
                }
#endif

                meInfo.residualPredictionFlag = resPredFlag | ((resPredFlag & 2) << 7);
                if (resPredFlag) {
                    Ipp16s *y_residual = core_enc->m_pYResidual + cur_mb.uMBy * core_enc->m_WidthInMBs * 256 + cur_mb.uMBx * 16;

                    if (resPredFlag) {
                        PIXTYPE *pSrc = cur_mb.mbPtr;
                        Ipp32s i, j;
                        Ipp16s *pRes = y_residual;
                        for (i = 0; i < 16; i++) {
                            for (j = 0; j < 16; j++) {
                                resPredBuf[i*16 + j] = pSrc[j] - pRes[j];
                            }
                            pSrc += cur_mb.mbPitchPixels;
                            pRes += resPitchPixels;
                        }
                    }
                    meInfo.pResY = resPredBuf;
                }

                if (tryBaseMode < 2) {
                    if (!refCPB) {
                        meInfo.xMin = pMBOffset->uMVLimits_L;
                        meInfo.xMax = pMBOffset->uMVLimits_R;
                        meInfo.yMin = pMBOffset->uMVLimits_U;
                        meInfo.yMax = pMBOffset->uMVLimits_D;
                    } else {
                        meInfo.xMin = -MIN(127, cur_mb.uMBx * 16 + 0);
                        meInfo.xMax =  MIN(127, (core_enc->m_pCurrentFrame->m_paddedParsedFrameDataSize.width - 16) - cur_mb.uMBx * 16 + 0);
                        meInfo.yMin = -MIN(127, cur_mb.uMBy * 16 + 0);
                        meInfo.yMax =  MIN(127, (core_enc->m_pCurrentFrame->m_paddedParsedFrameDataSize.height - 16) - cur_mb.uMBy * 16 + 0);
                    }
                    meInfo.pRDQM = glob_RDQM[iQP];
                    meInfo.threshold = core_enc->m_BestOf5EarlyExitThres[iQP];
                    // Inter_16x16
                    meInfo.pCur = cur_mb.mbPtr;
                    //refs[0] = 0;
                    //cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER;
                    //H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(state, curr_slice, 0, LIST_0, 4, 4, &meInfo);
                    Ipp32s left_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_A;
                    Ipp32s top_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_B;
                    Ipp32s topright_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_C;
                    Ipp32s topleft_addr = curr_slice->m_cur_mb.MacroblockNeighbours.mb_D;

                    if (left_addr < 0 && top_addr < 0) {
                        if( topright_addr < 0 )
                            meInfo.predictedMV = null_mv;
                        else  meInfo.predictedMV = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][topright_addr].MotionVectors[0];
                    } else if (left_addr < 0) {
                        if( topright_addr < 0 )
                            meInfo.predictedMV = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][top_addr].MotionVectors[0];
                        else {
                            H264MotionVector mvA, mvB, mvC;
                            mvA = null_mv;
                            mvB = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][top_addr].MotionVectors[0];
                            mvC = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][topright_addr].MotionVectors[0];
                            meInfo.predictedMV.mvx = MIN(mvA.mvx, mvB.mvx) ^ MIN(mvB.mvx, mvC.mvx) ^ MIN(mvC.mvx, mvA.mvx);
                            meInfo.predictedMV.mvy = MIN(mvA.mvy, mvB.mvy) ^ MIN(mvB.mvy, mvC.mvy) ^ MIN(mvC.mvy, mvA.mvy);
                        }
                    } else if (top_addr < 0 ) {
                        if (topright_addr < 0)
                            meInfo.predictedMV = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][left_addr].MotionVectors[0];
                        else {
                            H264MotionVector mvA, mvB, mvC;
                            mvA = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][left_addr].MotionVectors[0];
                            mvB = null_mv;
                            mvC = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][topright_addr].MotionVectors[0];
                            meInfo.predictedMV.mvx = MIN(mvA.mvx, mvB.mvx) ^ MIN(mvB.mvx, mvC.mvx) ^ MIN(mvC.mvx, mvA.mvx);
                            meInfo.predictedMV.mvy = MIN(mvA.mvy, mvB.mvy) ^ MIN(mvB.mvy, mvC.mvy) ^ MIN(mvC.mvy, mvA.mvy);
                        }
                    }else{
                        H264MotionVector mvA, mvB, mvC;
                        if (topright_addr < 0) mvC = null_mv;
                        else mvC = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][topright_addr].MotionVectors[0];

                        mvA = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][left_addr].MotionVectors[0];
                        mvB = core_enc->m_pCurrentFrame->m_mbinfo.MV[0][top_addr].MotionVectors[0];
                        meInfo.predictedMV.mvx = MIN(mvA.mvx, mvB.mvx) ^ MIN(mvB.mvx, mvC.mvx) ^ MIN(mvC.mvx, mvA.mvx);
                        meInfo.predictedMV.mvy = MIN(mvA.mvy, mvB.mvy) ^ MIN(mvB.mvy, mvC.mvy) ^ MIN(mvC.mvy, mvA.mvy);
                    }
                    //meInfo.pRef = ppRefPicList[0]->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[pFields[0]];
                    meInfo.pRef = pRefFrame->m_pYPlane + pMBOffset->uLumaOffset[core_enc->m_is_cur_pic_afrm][is_cur_mb_field] + curr_slice->m_InitialOffset[core_enc->m_field_index];
                    meInfo.bestSAD = MAX_SAD;
                    meInfo.candNum = 0;
                    meInfo.useScPredMV = 0;
                    meInfo.useScPredMVAlways = 0;
    #ifdef NO_PADDING
                    meInfo.interpolateInfo.pointBlockPos.x = cur_mb.uMBx << 4;
                    meInfo.interpolateInfo.pointBlockPos.y = cur_mb.uMBy << 4;
    #endif // NO_PADDING

                    //H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, meInfo.predictedMV);
                    //H264MotionVector mv = null_mv;
                    //H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, mv);

                    H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, pRefFrame->m_mbinfo.MV[0][uMB].MotionVectors[0]);
                    if (left_addr >= 0 )
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][left_addr].MotionVectors[0]);
                    if (top_addr >= 0)
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][top_addr].MotionVectors[0]);
                    if (topright_addr >= 0)
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][topright_addr].MotionVectors[0]);
                    if (topleft_addr >= 0)
                        H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)(&meInfo, core_enc->m_pCurrentFrame->m_mbinfo.MV[0][topleft_addr].MotionVectors[0]);
                    H264ENC_MAKE_NAME(ME_IntPel)(&meInfo);
                    //ME_SubPel(&meInfo);
                    BestSAD_Inter = meInfo.bestSAD;
                    cur_mb.MVs[LIST_0]->MotionVectors[0] = meInfo.bestMV;
                }

                if (tryBaseMode > 0) {
                    PIXTYPE *pRef = core_enc->m_pReconstructFrame->m_pYPlane +
                        core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
                    Ipp32s ref_intra = IS_TRUEINTRA_MBTYPE(cur_mb.GlobalMacroblockInfo->mbtype);

                    if (!ref_intra) {
                        PIXTYPE *pPredBuf = cur_mb.mbBaseMode.prediction;
                        H264CoreEncoder_MCOneMBLuma_8u16s(state, curr_slice, cur_mb.MVs[LIST_0]->MotionVectors, cur_mb.MVs[LIST_1]->MotionVectors, pPredBuf);

                        if (!core_enc->m_SliceHeader.MbaffFrameFlag  && core_enc->m_spatial_resolution_change) {
                            //RestoreIntraPrediction - copy intra predicted parts
                            Ipp32s i,j;
                            for (i=0; i<2; i++) {
                                for (j=0; j<2; j++) {
                                    if (!cur_mb.GlobalMacroblockInfo->i_pred[2*j+i]) continue;
                                    Ipp32s offx = core_enc->svc_ResizeMap[0][cur_mb.uMBx].border;
                                    Ipp32s offy = core_enc->svc_ResizeMap[1][cur_mb.uMBy].border;
                                    IppiSize roiSize = {i?16-offx:offx, j?16-offy:offy};
                                    if(!i) offx = 0;
                                    if(!j) offy = 0;

                                    ippiCopy_8u_C1R(core_enc->m_pReconstructFrame->m_pYPlane +
                                        (cur_mb.uMBy*16 + offy) * core_enc->m_pReconstructFrame->m_pitchPixels + cur_mb.uMBx*16 + offx,
                                        core_enc->m_pReconstructFrame->m_pitchPixels,
                                        pPredBuf + offy*16 + offx,
                                        16,
                                        roiSize);
                                }
                            }
                        }
                    }

                    PIXTYPE *pPredBuf = (ref_intra ? pRef : cur_mb.mbBaseMode.prediction);
                    Ipp32s step = (ref_intra ? cur_mb.mbPitchPixels : 16);
                    Ipp32s sad = MAX_SAD, sadR = MAX_SAD;

                    if (resPredFlag < 2)
                        sad = SAD16x16(cur_mb.mbPtr, cur_mb.mbPitchPixels, pPredBuf, step);
                    if (resPredFlag > 0)
                        sadR = SAD16x16_16_ResPred(resPredBuf, pPredBuf);
                    if (sadR < sad)
                        sad = sadR;


                    if (tryBaseMode > 1 || sad < BestSAD_Inter)
                        BestSAD_Inter = sad;
                }

#ifdef FRAMETYPE_DETECT_ST
#pragma omp atomic
#endif
                costInter += BestSAD_Inter;
                //Intra_16x16
#ifdef FTD_INTRA_DC
                {
                    PIXTYPE *pSrc = cur_mb.mbPtr, *pRef = cur_mb.mbPtr;
                    Ipp32u uDC = 0;
                    bool topAvailable = above_addr >= 0;
                    bool leftAvailable = left_addr >= 0;
                    if (!leftAvailable && !topAvailable) {
                        uDC = 1 << (core_enc->m_PicParamSet->bit_depth_luma - 1);
                    } else {
                        if (topAvailable) {
                            PIXTYPE *pAbove = pRef - pitchPixels;
                            for (i = 0; i < 16; i ++)
                                uDC += pAbove[i];
                        }
                        if (leftAvailable) {
                            PIXTYPE *pLeft = pRef - 1;
                            for (i = 0; i < 16; i ++) {
                                uDC += *pLeft;
                                pLeft += pitchPixels;
                            }
                        }
                        if (!topAvailable || !leftAvailable)
                            uDC <<= 1;
                        uDC = (uDC + 16) >> 5;
                    }
                    MemorySet(cur_mb.mb16x16.prediction, (PIXTYPE)uDC, 256);
                    BestSAD_Intra = BITS_COST(3, glob_RDQM[iQP]) + SAD16x16(pSrc, pitchPixels, cur_mb.mb16x16.prediction, 16);
                }
#elif defined(FTD_INTRA_ABSDEV)
                H264ENC_MAKE_NAME(MeanAbsDev16x16)(cur_mb.mbPtr, pitchPixels, &BestSAD_Intra);
                BestSAD_Intra += BITS_COST(3, glob_RDQM[iQP]);
#else
                T_AIMode BestMode;
                BestSAD_Intra = AIModeSelectOneMB_16x16(curr_slice, cur_mb.mbPtr, cur_mb.mbPtr, cur_mb.mbPitchPixels, &BestMode, cur_mb.mb16x16.prediction);
#endif
#ifdef FRAMETYPE_DETECT_ST
#pragma omp atomic
#endif
                    costIntra += BestSAD_Intra;
                    if (BestSAD_Intra < BestSAD_Inter) {
#ifdef _OPENMP
#pragma omp atomic
#endif
                        costBest += BestSAD_Intra;
#ifdef FRAMETYPE_DETECT_ST
#pragma omp atomic
#endif
                        nMBi ++;
                        if (nMBi >= thresh_IntraMB_I) break;
                            //goto type_detect;
                    } else
#ifdef FRAMETYPE_DETECT_ST
#pragma omp atomic
#endif
                        costBest += BestSAD_Inter;
//                    cur_mb.MVs[LIST_0]->MotionVectors[0] = meInfo.bestMV;
                //for (i = 0; i < 16; i ++) {
                //    mvs[i] = meInfo.bestMV;
                //    refs[i] = (T_RefIdx)0;
                //}
                //sbt[0] = sbt[1] = sbt[2] = sbt[3] = (MBTypeValue)NUMBER_OF_MBTYPES;
                /*
                // early exit disabled because we lost MV's and cost
                nMB ++;
                if (m_pCurrentFrame->m_PicCodType == PREDPIC) {
                    if (totMB - nMB < thresh_IntraMB_I - nMBi)
                        goto type_detect;
                } else { // BPREDPIC
                    if (totMB - nMB < thresh_IntraMB_P - nMBi) {
                        costInter = 0;
                        goto type_detect;
                    }
                }
                */
            }
        }
        if (core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE)
            core_enc->m_HeightInMBs <<= 1;
    }
#ifndef UMC_RESTRICTED_CODE_MBT
#if defined(MB_THREADING) && !defined(MB_THREADING_VM)
type_detect:
#endif // MB_THREADING && !MB_THREADING_VM
#endif // UMC_RESTRICTED_CODE_MBT
    core_enc->m_Analyse = saveAnalyse;
#endif //FRAME_TYPE_DETECT_DS
#ifdef FRAME_TYPE_DETECT_DS
type_detect:
#endif //FRAME_TYPE_DETECT_DS
    if (nMBi >= thresh_IntraMB_I) {
        core_enc->m_pCurrentFrame->m_PicCodType = INTRAPIC;
        core_enc->m_pCurrentFrame->m_RefPic = 1;
        core_enc->m_uIntraFrameInterval = core_enc->m_info.key_frame_controls.interval + 1;
        //core_enc->m_iProfileIndex = core_enc->profile_frequency - 1;
        //if (core_enc->m_iProfileIndex >= core_enc->profile_frequency)
            core_enc->m_iProfileIndex = 0;
        //m_bMakeNextFrameIDR = true;
        //if (m_bMakeNextFrameIDR) {
           // core_enc->m_pCurrentFrame->m_bIsIDRPic = true;
           // m_PicOrderCnt_Accu += m_PicOrderCnt;
           // m_PicOrderCnt = 0;
           // m_bMakeNextFrameIDR = false;
           // core_enc->m_cpb.IncreaseRefPicListResetCount(core_enc->m_pCurrentFrame);
        //}
        //Ipp32s qp = H264_AVBR_GetQP(&core_enc->avbr, PREDPIC);
        //H264_AVBR_SetQP(&core_enc->avbr, PREDPIC, qp - 1);
    } else {
        if (core_enc->m_pCurrentFrame->m_PicCodType == BPREDPIC) {
            if (costInter > (Ipp32s)(Inter_Intra_Ratio * costIntra) || nMBi >= thresh_IntraMB_P) {
                core_enc->m_pCurrentFrame->m_PicCodType = PREDPIC;
                core_enc->m_pCurrentFrame->m_RefPic = 1;
                core_enc->m_iProfileIndex = 1;
                if (core_enc->m_iProfileIndex >= core_enc->profile_frequency)
                    core_enc->m_iProfileIndex = 0;
            }
        }
    }
}

#ifdef USE_PSYINFO
#define HADT_4(i0, i1, i2, i3, r0, r1, r2, r3) { \
    Ipp32s a = i0 + i1, b = i0 - i1; \
    Ipp32s c = i2 + i3, d = i2 - i3; \
    r0 = a + c; r1 = a - c; \
    r2 = b - d; r3 = b + d; \
}
/*#define HAAR_4(i0, i1, i2, i3, r0, r1, r2, r3) { \
    Ipp32s a = i0 + i1, b = i0 - i1; \
    Ipp32s c = i2 + i3, d = i2 - i3; \
    r0 = a + c; r1 = a - c; \
    r2 = b    ; r3 =     d; \
}*/
void HADT_4x4 (const Ipp8u* src, Ipp32s step, Ipp32s* dst)
{
    HADT_4(src[0], src[1], src[2], src[3], dst[ 0], dst[ 1], dst[ 2], dst[ 3]);
    src += step;
    HADT_4(src[0], src[1], src[2], src[3], dst[ 4], dst[ 5], dst[ 6], dst[ 7]);
    src += step;
    HADT_4(src[0], src[1], src[2], src[3], dst[ 8], dst[ 9], dst[10], dst[11]);
    src += step;
    HADT_4(src[0], src[1], src[2], src[3], dst[12], dst[13], dst[14], dst[15]);

    HADT_4(dst[ 0], dst[ 4], dst[ 8], dst[12], dst[ 0], dst[ 4], dst[ 8], dst[12]);
    HADT_4(dst[ 1], dst[ 5], dst[ 9], dst[13], dst[ 1], dst[ 5], dst[ 9], dst[13]);
    HADT_4(dst[ 2], dst[ 6], dst[10], dst[14], dst[ 2], dst[ 6], dst[10], dst[14]);
    HADT_4(dst[ 3], dst[ 7], dst[11], dst[15], dst[ 3], dst[ 7], dst[11], dst[15]);
    return;
}

//// previous frame in display order, latest if pCurr is 0
//H264EncoderFrameType * H264ENC_MAKE_NAME(H264CoreEncoder_PrevFrame)(void* state, H264EncoderFrameType * pCurr)
//{
//    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
//    H264EncoderFrameType *pPrev, *pF;
//    // find previous frame
//    pF = core_enc->m_cpb.m_pTail;
//    pPrev = 0;
//    while (pF) {
//        if (!pF->m_wasEncoded && (!pCurr || pF->m_PicOrderCnt[0] < pCurr->m_PicOrderCnt[0]) && (pPrev == 0 || pF->m_PicOrderCnt[0] > pPrev->m_PicOrderCnt[0]) )
//            pPrev = pF;
//        pF = pF->m_pPreviousFrame;
//    }
//    return pPrev;
//}
//
//// next frame in display order, 0 if pCurr is 0
//H264EncoderFrameType * H264ENC_MAKE_NAME(H264CoreEncoder_NextFrame)(void* state, H264EncoderFrameType * pCurr)
//{
//    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
//    H264EncoderFrameType *pNext, *pF;
//    // find previous frame
//    if( !pCurr)
//        return 0;
//    pF = core_enc->m_cpb.m_pHead;
//    pNext = 0;
//    while (pF) {
//        if (!pF->m_wasEncoded && pF->m_PicOrderCnt[0] > pCurr->m_PicOrderCnt[0] && (pNext == 0 || pF->m_PicOrderCnt[0] > pNext->m_PicOrderCnt[0]) )
//            pNext = pF;
//        pF = pF->m_pFutureFrame;
//    }
//    return pNext;
//}

void H264ENC_MAKE_NAME(H264CoreEncoder_FrameAnalysis)(void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264EncoderFrameType *pPrev, *pCurr;
    pCurr = core_enc->m_pCurrentFrame;
    if (!pCurr || !(core_enc->m_Analyse_ex & ANALYSE_PSY))
        return;
    Ipp32s is_cur_mb_field = 0; //core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE;
    Ipp32s pitchPixels = pCurr->m_pitchPixels << is_cur_mb_field;
    Ipp32s uNumMBs = core_enc->m_HeightInMBs * core_enc->m_WidthInMBs >> is_cur_mb_field;

    //Ipp32s isIntraPic = core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC;
    //if (core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC)
    //    return;

    //Ipp8u *cy = new Ipp8u[2000*1100/2];
    //Ipp8u *cu = new Ipp8u[2000*1100/2/4]; memset(cu, 128, 2000*1100/2/4);
    //Ipp8u *cv = new Ipp8u[2000*1100/2/4]; memset(cv, 128, 2000*1100/2/4);
    //Ipp32s sx = core_enc->m_WidthInMBs;
    //Ipp32s sy = core_enc->m_HeightInMBs;

    // find previous frame
    if (core_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD)
        pPrev = core_enc->m_pNextFrame; // next to encode on previous frame iteration
    else
        pPrev = 0;

    Ipp32u uMB, uFirstMB = 0;
    PsyInfo psyStatic = {0,};
    PsyInfo psyDyna = {0,};
    Ipp32s offsx = 0, offsy = 0;

    for (uMB = uFirstMB; uMB < uFirstMB + uNumMBs; uMB ++, offsx += 16) {
        Ipp32s uBl;
        PsyInfo *psyStaticBl;
        PsyInfo psyStaticMB = {0,};
        PsyInfo psyDynaMB = {0,};
        if(offsx == core_enc->m_WidthInMBs*16) {
            offsx = 0;
            offsy += 16;
        }

        psyStaticBl = pCurr->m_mbinfo.mbs[uMB].psyStaticBl;

        PIXTYPE* mbPtr = pCurr->m_pYPlane + offsx + offsy*pitchPixels;
        for (uBl = 0; uBl<16; uBl++) {
            Ipp32s had[16];
            HADT_4x4(mbPtr + 4*(uBl&3) + pitchPixels * (uBl>>2)*4, pitchPixels, had);
            if (pPrev) { // compare previous to current
                Ipp32s had2[16];
                // assume the same pitch for a while
                PIXTYPE* mbPtr2 = pPrev->m_pYPlane + offsx + offsy*pitchPixels;
                HADT_4x4(mbPtr2 + 4*(uBl&3) + pitchPixels * (uBl>>2)*4, pitchPixels, had2);
                ippsSub_32s_ISfs(had, had2, 16, 0);
                ippsAbs_32s_I(had2, 16);
                Ipp32s a0,a1,a2,a3;
                psyDynaMB.dc      += a0 = had2[0];
                psyDynaMB.bgshift += a1 = had2[1] + had2[4] + had2[5];
                psyDynaMB.details += a2 = had2[2] + had2[3] + had2[6] + had2[7] + had2[8] + had2[9] + had2[10] + had2[11] + had2[15];
                psyDynaMB.interl  += a3 = had2[12] + had2[13] + had2[14];
                //cy[offsx/4 + (uBl&3) + (offsy/4+(uBl>>2) + sy*4)*sx*16         ] = a0/16 ;
                //cy[offsx/4 + (uBl&3) + (offsy/4+(uBl>>2) + sy*4)*sx*16 + sx*4  ] = a1/4/2;
                //cy[offsx/4 + (uBl&3) + (offsy/4+(uBl>>2) + sy*4)*sx*16 + sx*4*2] = a2/4/2;
                //cy[offsx/4 + (uBl&3) + (offsy/4+(uBl>>2) + sy*4)*sx*16 + sx*4*3] = a3/4/2;
            }
            ippsAbs_32s_I(had, 16);
            psyStaticBl[uBl].dc = had[0];
            psyStaticBl[uBl].bgshift = had[1] + had[4] + had[5];
            psyStaticBl[uBl].details = had[2] + had[3] + had[6] + had[7] + had[8] + had[9] + had[10] + had[11] + had[15];
            psyStaticBl[uBl].interl  = had[12] + had[13] + had[14];

            //cy[offsx/4 + (uBl&3) + (offsy/4+(uBl>>2))*sx*16         ] = psyStaticBl[uBl].dc/16;
            //cy[offsx/4 + (uBl&3) + (offsy/4+(uBl>>2))*sx*16 + sx*4  ] = psyStaticBl[uBl].bgshift/4/2;
            //cy[offsx/4 + (uBl&3) + (offsy/4+(uBl>>2))*sx*16 + sx*4*2] = psyStaticBl[uBl].details/4/2;
            //cy[offsx/4 + (uBl&3) + (offsy/4+(uBl>>2))*sx*16 + sx*4*3] = psyStaticBl[uBl].interl/4/2;
            psyStaticMB.dc += psyStaticBl[uBl].dc;
            psyStaticMB.bgshift += psyStaticBl[uBl].bgshift;
            psyStaticMB.details += psyStaticBl[uBl].details;
            psyStaticMB.interl  += psyStaticBl[uBl].interl;
        }
        pCurr->m_mbinfo.mbs[uMB].psyStaticMB = psyStaticMB;
        pCurr->m_mbinfo.mbs[uMB].psyDynaMB[0] = psyDynaMB;
        if (pPrev)
            pPrev->m_mbinfo.mbs[uMB].psyDynaMB[1] = psyDynaMB;

        psyStatic.dc += psyStaticMB.dc;
        psyStatic.bgshift += psyStaticMB.bgshift;
        psyStatic.details += psyStaticMB.details;
        psyStatic.interl  += psyStaticMB.interl;
        psyDyna.dc += psyDynaMB.dc;
        psyDyna.bgshift += psyDynaMB.bgshift;
        psyDyna.details += psyDynaMB.details;
        psyDyna.interl  += psyDynaMB.interl;
    }

    pCurr->psyStatic.dc = psyStatic.dc / uNumMBs;
    pCurr->psyStatic.bgshift = psyStatic.bgshift / uNumMBs;
    pCurr->psyStatic.details = psyStatic.details / uNumMBs;
    pCurr->psyStatic.interl  = psyStatic.interl  / uNumMBs;
    if ( !pPrev) {
        pCurr->psyDyna[0].dc = -uNumMBs;
        pCurr->psyDyna[0].bgshift = -uNumMBs;
        pCurr->psyDyna[0].details = -uNumMBs;
        pCurr->psyDyna[0].interl = -uNumMBs;
    } else {
        pCurr->psyDyna[0].dc = psyDyna.dc / uNumMBs;
        pCurr->psyDyna[0].bgshift = psyDyna.bgshift / uNumMBs;
        pCurr->psyDyna[0].details = psyDyna.details / uNumMBs;
        pCurr->psyDyna[0].interl  = psyDyna.interl  / uNumMBs;
    }
    if (pPrev) {
        pPrev->psyDyna[1] = pCurr->psyDyna[0];
        pCurr->psyDyna[2] = pPrev->psyDyna[0]; // copy prev-prev
    }

    //char fname[1000];
    //sprintf_s(fname, 999, "spec_%dx%d.yuv", sx*16, sy*8);
    //FILE* file = fopen(fname, "ab");
    //fwrite(cy, 1, sx*16*sy*8, file);
    //fwrite(cu, 1, sx*16*sy*8/4, file);
    //fwrite(cv, 1, sx*16*sy*8/4, file);
    //fclose(file);

    //delete cv;
    //delete cu;
    //delete cy;

    return;
}

#endif // USE_PSYINFO

#undef EncoderRefPicListStructType
#undef EncoderRefPicListType
#undef H264EncoderFrameType
#undef ME_InfType
#undef H264CurrentMacroblockDescriptorType
#undef H264SliceType
#undef H264CoreEncoderType
#undef H264BsFakeType
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE

