//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined(MFX_VA) && defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && (defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) || defined(MFX_ENABLE_H264_VIDEO_FEI_PAK))
#include "fei_common.h"

#define I_SLICE(SliceType) ((SliceType) == 2 || (SliceType) == 7)
#define P_SLICE(SliceType) ((SliceType) == 0 || (SliceType) == 5)
#define B_SLICE(SliceType) ((SliceType) == 1 || (SliceType) == 6)

using namespace MfxHwH264Encode;

#if MFX_VERSION >= 1023

template void MfxH264FEIcommon::ConfigureTaskFEI<>(
    DdiTask             & task,
    DdiTask       const & prevTask,
    MfxVideoParam const & video,
    mfxENCInput * inParams);

template void MfxH264FEIcommon::ConfigureTaskFEI<>(
    DdiTask             & task,
    DdiTask       const & prevTask,
    MfxVideoParam const & video,
    mfxPAKInput * inParams);

#else

template void MfxH264FEIcommon::ConfigureTaskFEI<>(
    DdiTask             & task,
    DdiTask       const & prevTask,
    MfxVideoParam const & video,
    mfxENCInput * inParams,
    mfxENCInput * outParams,
    std::map<mfxU32, mfxU32> &      frameOrder_frameNum);

template void MfxH264FEIcommon::ConfigureTaskFEI<>(
    DdiTask             & task,
    DdiTask       const & prevTask,
    MfxVideoParam const & video,
    mfxPAKInput * inParams,
    mfxPAKOutput* outParams,
    std::map<mfxU32, mfxU32> &      frameOrder_frameNum);

#endif // MFX_VERSION >= 1023

#if MFX_VERSION >= 1023

template <typename T>
void MfxH264FEIcommon::ConfigureTaskFEI(
    DdiTask             & task,
    DdiTask       const & prevTask,
    MfxVideoParam const & video,
    T                   * inParams)

#else

template <typename T, typename U>
void MfxH264FEIcommon::ConfigureTaskFEI(
    DdiTask             & task,
    DdiTask       const & prevTask,
    MfxVideoParam const & video,
    T                   * inParams,
    U                   * outParams,
    std::map<mfxU32, mfxU32> &      frameOrder_frameNum)

#endif // MFX_VERSION >= 1023

{
    bool FrameRepacking = prevTask.m_encOrder != 0xffffffff && task.m_frameOrder == prevTask.m_frameOrder;

    mfxExtCodingOption  const & extOpt  = GetExtBufferRef(video);
    mfxExtCodingOption2 const & extOpt2 = GetExtBufferRef(video);
    mfxExtSpsHeader     const & extSps  = GetExtBufferRef(video);

    mfxU32 const FRAME_NUM_MAX = 1 << (extSps.log2MaxFrameNumMinus4 + 4);

    mfxU8 idrPicFlag   = !!(task.GetFrameType() & MFX_FRAMETYPE_IDR);
    mfxU8 intraPicFlag = !!(task.GetFrameType() & MFX_FRAMETYPE_I);
    mfxU8 refPicFlag   = !!(task.GetFrameType() & MFX_FRAMETYPE_REF);

    if (!FrameRepacking)
    {
        mfxU8 prevIdrFrameFlag = !!(prevTask.GetFrameType() & MFX_FRAMETYPE_IDR);
        mfxU8 prevIFrameFlag   = !!(prevTask.GetFrameType() & MFX_FRAMETYPE_I);
        mfxU8 prevRefPicFlag   = !!(prevTask.GetFrameType() & MFX_FRAMETYPE_REF);

        mfxU8 frameNumIncrement = (prevRefPicFlag || prevTask.m_nalRefIdc[0]) ? 1 : 0;

        task.m_frameOrderIdr = idrPicFlag   ? task.m_frameOrder : prevTask.m_frameOrderIdr;
        task.m_frameOrderI   = intraPicFlag ? task.m_frameOrder : prevTask.m_frameOrderI;
        task.m_encOrder      = prevTask.m_encOrder + 1;
        task.m_encOrderIdr   = prevIdrFrameFlag ? prevTask.m_encOrder : prevTask.m_encOrderIdr;
        task.m_encOrderI     = prevIFrameFlag   ? prevTask.m_encOrder : prevTask.m_encOrderI;


        task.m_frameNum = idrPicFlag ? 0 : mfxU16((prevTask.m_frameNum + frameNumIncrement) % FRAME_NUM_MAX);

        task.m_picNum[1] = task.m_picNum[0] = task.m_frameNum * (task.m_fieldPicFlag + 1) + task.m_fieldPicFlag;

        task.m_idrPicId = prevTask.m_idrPicId + idrPicFlag;
    }
    else
    {
        task.m_frameOrderIdr = prevTask.m_frameOrderIdr;
        task.m_frameOrderI   = prevTask.m_frameOrderI;
        task.m_encOrder      = prevTask.m_encOrder;
        task.m_encOrderIdr   = prevTask.m_encOrderIdr;
        task.m_encOrderI     = prevTask.m_encOrderI;

        task.m_picNum[1] = task.m_picNum[0] = task.m_frameNum = prevTask.m_frameNum;

        task.m_idrPicId = prevTask.m_idrPicId;
    }

    mfxU32 ffid = task.m_fid[0];
    mfxU32 sfid = !ffid;

    task.m_pid = 0;

    task.m_insertAud[ffid] = IsOn(extOpt.AUDelimiter);
    task.m_insertAud[sfid] = IsOn(extOpt.AUDelimiter);
    task.m_insertSps[ffid] = intraPicFlag;
    task.m_insertSps[sfid] = 0;
    task.m_insertPps[ffid] = task.m_insertSps[ffid] || IsOn(extOpt2.RepeatPPS);
    task.m_insertPps[sfid] = task.m_insertSps[sfid] || IsOn(extOpt2.RepeatPPS);

    task.m_reference[ffid] = task.m_nalRefIdc[ffid] = !!(task.m_type[ffid] & MFX_FRAMETYPE_REF);
    task.m_reference[sfid] = task.m_nalRefIdc[sfid] = !!(task.m_type[sfid] & MFX_FRAMETYPE_REF);

    if (video.calcParam.tempScalabilityMode)
    {
        task.m_insertPps[ffid] = task.m_insertSps[ffid];
        task.m_insertPps[sfid] = task.m_insertSps[sfid];
        task.m_nalRefIdc[ffid] = idrPicFlag ? 3 : (task.m_reference[ffid] ? 2 : 0);
        task.m_nalRefIdc[sfid] = task.m_reference[sfid] ? 2 : 0;
    }

    task.m_cqpValue[0] = GetQpValue(video, task.m_ctrl, task.m_type[0]);
    task.m_cqpValue[1] = GetQpValue(video, task.m_ctrl, task.m_type[1]);

#if MFX_VERSION >= 1023

    mfxU16 field_type_mask[2] = { MFX_PICTYPE_FRAME | MFX_PICTYPE_TOPFIELD, MFX_PICTYPE_FRAME | MFX_PICTYPE_BOTTOMFIELD },
        last_frame_ref = 0;

    mfxU32 fieldMaxCount = 1 + task.m_fieldPicFlag;
    for (mfxU32 field = 0; field < fieldMaxCount; ++field)
    {
        mfxU32 fieldParity = (video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF)? (1 - field) : field;

        // Fill Deblocking parameters
        mfxExtFeiSliceHeader * extFeiSlice = GetExtBufferFEI(inParams, field);

        for (size_t i = 0; i < extFeiSlice->NumSlice; ++i)
        {
            task.m_disableDeblockingIdc[fieldParity].push_back(extFeiSlice->Slice[i].DisableDeblockingFilterIdc);
            task.m_sliceAlphaC0OffsetDiv2[fieldParity].push_back(extFeiSlice->Slice[i].SliceAlphaC0OffsetDiv2);
            task.m_sliceBetaOffsetDiv2[fieldParity].push_back(extFeiSlice->Slice[i].SliceBetaOffsetDiv2);
        }

        // Fill DPB
        mfxExtFeiPPS * pDataPPS = GetExtBufferFEI(inParams, field);

        std::vector<mfxFrameSurface1*> dpb_frames;
        dpb_frames.reserve(16);

        mfxU16 indexFromPPSHeader;
        mfxU8  list_item;
        mfxU8  refParity; // 0 - top field / progressive frame; 1 - bottom field
        mfxFrameSurface1* pMfxFrame;
        mfxI32 poc_base;

        task.m_dpb[fieldParity].Resize(0);
        for (mfxU32 dpb_idx = 0; dpb_idx < 16; ++dpb_idx)
        {
            indexFromPPSHeader = pDataPPS->DpbBefore[dpb_idx].Index;

            if (indexFromPPSHeader == 0xffff) break;

            pMfxFrame = inParams->L0Surface[indexFromPPSHeader];
            dpb_frames.push_back(pMfxFrame);

            DpbFrame frame;
            poc_base             = 2 * ((pMfxFrame->Data.FrameOrder - task.m_frameOrderIdr) & 0x7fffffff);
            frame.m_poc          = PairI32(poc_base + (TFIELD != ffid), poc_base + (BFIELD != ffid));
            frame.m_frameOrder   = pMfxFrame->Data.FrameOrder;
            //frame.m_frameNumWrap = (frame.m_frameNum > task.m_frameNum) ? frame.m_frameNum - FRAME_NUM_MAX : frame.m_frameNum;
            frame.m_frameNumWrap = pDataPPS->DpbBefore[dpb_idx].FrameNumWrap;

            frame.m_picNum[0] = task.m_fieldPicFlag ? 2 * frame.m_frameNumWrap + ( !fieldParity) : frame.m_frameNumWrap; // in original here field = ffid / sfid
            frame.m_picNum[1] = task.m_fieldPicFlag ? 2 * frame.m_frameNumWrap + (!!fieldParity) : frame.m_frameNumWrap;

            frame.m_refPicFlag[ffid] =  !!(pDataPPS->DpbBefore[dpb_idx].PicType & field_type_mask[ fieldParity]);
            frame.m_refPicFlag[sfid] = (!!(pDataPPS->DpbBefore[dpb_idx].PicType & field_type_mask[!fieldParity])) *
                (task.m_fieldPicFlag ? (frame.m_frameOrder != task.m_frameOrder) : 1);

            // Store passed flag for last frame in DPB
            last_frame_ref = !!(pDataPPS->DpbBefore[dpb_idx].PicType & field_type_mask[!fieldParity]);

            frame.m_longterm = pDataPPS->DpbBefore[dpb_idx].LongTermFrameIdx != 0xffff;

            if (frame.m_longterm)
            {
                frame.m_longTermIdxPlus1 = pDataPPS->DpbBefore[dpb_idx].LongTermFrameIdx + 1;

                frame.m_longTermPicNum[0] = task.m_fieldPicFlag ? 2 * (frame.m_longTermIdxPlus1 - 1) + ( !fieldParity) : frame.m_longTermIdxPlus1 - 1;
                frame.m_longTermPicNum[1] = task.m_fieldPicFlag ? 2 * (frame.m_longTermIdxPlus1 - 1) + (!!fieldParity) : frame.m_longTermIdxPlus1 - 1;
            }

            task.m_dpb[fieldParity].PushBack(frame);
        }

        // Get default reflists
        InitRefPicList(task, fieldParity);
        ArrayU8x33 initList0 = task.m_list0[fieldParity];
        ArrayU8x33 initList1 = task.m_list1[fieldParity];

        // Fill reflists
        mfxExtFeiSliceHeader * pDataSliceHeader = extFeiSlice;
        // Number of reference handling
        mfxU32 maxNumRefL0 = pDataSliceHeader->Slice[0].NumRefIdxL0Active,
               maxNumRefL1 = pDataSliceHeader->Slice[0].NumRefIdxL1Active;

        mfxU16 indexFromSliceHeader;

        task.m_list0[fieldParity].Resize(0);
        for (mfxU32 list0_idx = 0; list0_idx < maxNumRefL0; ++list0_idx)
        {
            indexFromSliceHeader = pDataSliceHeader->Slice[0].RefL0[list0_idx].Index;
            refParity            = pDataSliceHeader->Slice[0].RefL0[list0_idx].PictureType == MFX_PICTYPE_BOTTOMFIELD;

            pMfxFrame = inParams->L0Surface[indexFromSliceHeader];

            list_item = static_cast<mfxU8>(std::distance(dpb_frames.begin(), std::find(dpb_frames.begin(), dpb_frames.end(), pMfxFrame)));

            task.m_list0[fieldParity].PushBack((refParity << 7) | list_item);
        }

        task.m_list1[fieldParity].Resize(0);
        for (mfxU32 list1_idx = 0; list1_idx < maxNumRefL1; ++list1_idx)
        {
            indexFromSliceHeader = pDataSliceHeader->Slice[0].RefL1[list1_idx].Index;
            refParity            = pDataSliceHeader->Slice[0].RefL1[list1_idx].PictureType == MFX_PICTYPE_BOTTOMFIELD;

            pMfxFrame = inParams->L0Surface[indexFromSliceHeader];

            list_item = static_cast<mfxU8>(std::distance(dpb_frames.begin(), std::find(dpb_frames.begin(), dpb_frames.end(), pMfxFrame)));

            task.m_list1[fieldParity].PushBack((refParity << 7) | list_item);
        }

        initList0.Resize(task.m_list0[fieldParity].Size());
        initList1.Resize(task.m_list1[fieldParity].Size());

        // Fill reflists modificators
        task.m_refPicList0Mod[fieldParity] = CreateRefListMod(task.m_dpb[fieldParity], initList0, task.m_list0[fieldParity], task.m_viewIdx, task.m_picNum[fieldParity], true);
        task.m_refPicList1Mod[fieldParity] = CreateRefListMod(task.m_dpb[fieldParity], initList1, task.m_list1[fieldParity], task.m_viewIdx, task.m_picNum[fieldParity], true);

    } // for (mfxU32 field = 0; field < fieldMaxCount; ++field)

    // Update Ref flag
    if (task.m_dpb[sfid].Size())
        task.m_dpb[sfid].Back().m_refPicFlag[sfid] = last_frame_ref;

    mfxFrameSurface1* pMfxFrame;
    mfxI32 poc_base;
    mfxU16 indexFromPPSHeader;
    mfxExtFeiPPS * pDataPPS = GetExtBufferFEI(inParams, 0);
    mfxU32 fieldParity = !!(video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF);

    task.m_dpbPostEncoding.Resize(0);
    for (mfxU32 dpb_idx = 0; dpb_idx < 16; ++dpb_idx)
    {
        indexFromPPSHeader = pDataPPS->DpbAfter[dpb_idx].Index;

        if (indexFromPPSHeader == 0xffff) break;

        pMfxFrame = inParams->L0Surface[indexFromPPSHeader];

        DpbFrame frame;
        poc_base           = 2 * ((pMfxFrame->Data.FrameOrder - task.m_frameOrderIdr) & 0x7fffffff);
        frame.m_poc        = PairI32(poc_base + (TFIELD != ffid), poc_base + (BFIELD != ffid));
        frame.m_frameOrder = pMfxFrame->Data.FrameOrder;
        //frame.m_frameNumWrap = (frame.m_frameNum > task.m_frameNum) ? frame.m_frameNum - FRAME_NUM_MAX : frame.m_frameNum;
        frame.m_frameNumWrap = pDataPPS->DpbAfter[dpb_idx].FrameNumWrap;

        //frame.m_picNum[0] = task.m_fieldPicFlag ? 2 * frame.m_frameNumWrap + ( !fieldParity) : frame.m_frameNumWrap; // in original here field = ffid / sfid
        //frame.m_picNum[1] = task.m_fieldPicFlag ? 2 * frame.m_frameNumWrap + (!!fieldParity) : frame.m_frameNumWrap;

        frame.m_refPicFlag[ffid] = !!(pDataPPS->DpbAfter[dpb_idx].PicType & field_type_mask[ fieldParity]);
        frame.m_refPicFlag[sfid] = !!(pDataPPS->DpbAfter[dpb_idx].PicType & field_type_mask[!fieldParity]);

        frame.m_longterm = pDataPPS->DpbAfter[dpb_idx].LongTermFrameIdx != 0xffff;

        if (frame.m_longterm)
        {
            frame.m_longTermIdxPlus1 = pDataPPS->DpbAfter[dpb_idx].LongTermFrameIdx + 1;

            //frame.m_longTermPicNum[0] = task.m_fieldPicFlag ? 2 * (frame.m_longTermIdxPlus1 - 1) + ( !fieldParity) : frame.m_longTermIdxPlus1 - 1;
            //frame.m_longTermPicNum[1] = task.m_fieldPicFlag ? 2 * (frame.m_longTermIdxPlus1 - 1) + (!!fieldParity) : frame.m_longTermIdxPlus1 - 1;
        }

        task.m_dpbPostEncoding.PushBack(frame);
    }

    // Section below configures MMCO
    for (mfxU32 field = 0; field < fieldMaxCount; ++field)
    {
        mfxU32 fieldParity = (video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF) ? (1 - field) : field;

        task.m_decRefPicMrk[fieldParity].Clear();

        ArrayDpbFrame & DPB_before = task.m_dpb[fieldParity],
            DPB_after = (field + 1 == fieldMaxCount) ? task.m_dpbPostEncoding : task.m_dpb[!fieldParity];

        // Current frame is reference and DPB has no free slots. Find frame to remove with sliding window
        DpbFrame * toRemoveDefault = NULL;
        if (DPB_before.Size() == video.mfx.NumRefFrame && refPicFlag)
            toRemoveDefault = std::min_element(DPB_before.Begin(), DPB_before.End(), OrderByFrameNumWrap);

        DecRefPicMarkingInfo & marking = task.m_decRefPicMrk[fieldParity];
        DpbFrame* instance_after;
        for (DpbFrame* instance = DPB_before.Begin(); instance < DPB_before.End(); ++instance)
        {
            instance_after = std::find_if(DPB_after.Begin(), DPB_after.End(), FindByFrameOrder(instance->m_frameOrder));

            // Removed non-default frame from DPB
            if (instance_after == DPB_after.End())
            {
                if (instance != toRemoveDefault)
                {
                    // TODO: what if current picstruct is interlaced, but frame to remove is progressive
                    if (instance->m_longterm)
                    {
                        if (instance->m_refPicFlag[ffid])                        marking.PushBack(MMCO_LT_TO_UNUSED, instance->m_longTermPicNum[ffid]);
                        if (instance->m_refPicFlag[sfid] && task.m_fieldPicFlag) marking.PushBack(MMCO_LT_TO_UNUSED, instance->m_longTermPicNum[sfid]);
                    }
                    else
                    {
                        if (instance->m_refPicFlag[ffid])                        marking.PushBack(MMCO_ST_TO_UNUSED, task.m_picNum[fieldParity] - instance->m_picNum[ffid] - 1);
                        if (instance->m_refPicFlag[sfid] && task.m_fieldPicFlag) marking.PushBack(MMCO_ST_TO_UNUSED, task.m_picNum[fieldParity] - instance->m_picNum[sfid] - 1);
                    }
                }
            }
            else
            {
                if (instance->m_longterm != instance_after->m_longterm)
                {
                    // Convert to LT
                    if (instance->m_refPicFlag[ffid])
                        marking.PushBack(MMCO_ST_TO_LT, task.m_picNum[fieldParity] - instance->m_picNum[ffid] - 1, instance_after->m_longTermIdxPlus1 - 1);

                    if (instance->m_refPicFlag[sfid] && task.m_fieldPicFlag)
                        marking.PushBack(MMCO_ST_TO_LT, task.m_picNum[fieldParity] - instance->m_picNum[sfid] - 1, instance_after->m_longTermIdxPlus1 - 1);
                }

                // Remove one field from pair if required
                if (instance->m_refPicFlag[ffid] != instance_after->m_refPicFlag[ffid] || instance->m_refPicFlag[sfid] != instance_after->m_refPicFlag[sfid])
                {
                    mfxU32 fid = instance->m_refPicFlag[ffid] != instance_after->m_refPicFlag[ffid] ? ffid : sfid;

                    if (instance_after->m_longterm)
                        marking.PushBack(MMCO_LT_TO_UNUSED, instance_after->m_longTermPicNum[fid]);
                    else
                        marking.PushBack(MMCO_ST_TO_UNUSED, task.m_picNum[fieldParity] - instance_after->m_picNum[fid] - 1);
                }

            }
        }

        if (!field)
        {
            // Current frame is not in DPB_before yet
            instance_after = std::find_if(DPB_after.Begin(), DPB_after.End(), FindByFrameOrder(task.m_frameOrder));

            if (instance_after != DPB_after.End())
            {
                if (instance_after->m_longterm)
                {
                    // Mark current frame as LT
                    marking.PushBack(MMCO_CURR_TO_LT, instance_after->m_longTermIdxPlus1 - 1);
                }

                // Remove one field from pair if required
                if (!instance_after->m_refPicFlag[ffid] || !instance_after->m_refPicFlag[sfid])
                {
                    mfxU32 fid = !instance_after->m_refPicFlag[ffid] ? ffid : sfid;

                    if (instance_after->m_longterm)
                        marking.PushBack(MMCO_LT_TO_UNUSED, instance_after->m_longTermPicNum[fid]);
                    else
                        marking.PushBack(MMCO_ST_TO_UNUSED, task.m_picNum[fieldParity] - instance_after->m_picNum[fid] - 1);
                }
            }
        }

        // If MMCO already present, delete default reference explicitly
        if (marking.mmco.Size() && toRemoveDefault)
        {
            if (std::find_if(DPB_after.Begin(), DPB_after.End(), FindByFrameOrder(toRemoveDefault->m_frameOrder)) == DPB_after.End())
            {
                if (toRemoveDefault->m_longterm)
                {
                    if (toRemoveDefault->m_refPicFlag[ffid])                        marking.PushBack(MMCO_LT_TO_UNUSED, toRemoveDefault->m_longTermPicNum[ffid]);
                    if (toRemoveDefault->m_refPicFlag[sfid] && task.m_fieldPicFlag) marking.PushBack(MMCO_LT_TO_UNUSED, toRemoveDefault->m_longTermPicNum[sfid]);
                }
                else
                {
                    if (toRemoveDefault->m_refPicFlag[ffid])                        marking.PushBack(MMCO_ST_TO_UNUSED, task.m_picNum[fieldParity] - toRemoveDefault->m_picNum[ffid] - 1);
                    if (toRemoveDefault->m_refPicFlag[sfid] && task.m_fieldPicFlag) marking.PushBack(MMCO_ST_TO_UNUSED, task.m_picNum[fieldParity] - toRemoveDefault->m_picNum[sfid] - 1);
                }
            }
        }
    } // for (mfxU32 field = 0; field < fieldMaxCount; ++field)
#else
    frameOrder_frameNum[task.m_frameOrder] = task.m_frameNum;

    // Fill Deblocking parameters
    mfxU32 fieldMaxCount = (video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;
    for (mfxU32 field = 0; field < fieldMaxCount; ++field)
    {
        mfxU32 fieldParity = (video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF)? (1 - field) : field;

        mfxExtFeiSliceHeader * extFeiSlice = GetExtBufferFEI(outParams, field);

        for (size_t i = 0; i < extFeiSlice->NumSlice; ++i)
        {
            mfxU8 disableDeblockingIdc   = (mfxU8) extFeiSlice->Slice[i].DisableDeblockingFilterIdc;
            mfxI8 sliceAlphaC0OffsetDiv2 = (mfxI8) extFeiSlice->Slice[i].SliceAlphaC0OffsetDiv2;
            mfxI8 sliceBetaOffsetDiv2    = (mfxI8) extFeiSlice->Slice[i].SliceBetaOffsetDiv2;

            // Store per-slice values in task
            task.m_disableDeblockingIdc[fieldParity].push_back(disableDeblockingIdc);
            task.m_sliceAlphaC0OffsetDiv2[fieldParity].push_back(sliceAlphaC0OffsetDiv2);
            task.m_sliceBetaOffsetDiv2[fieldParity].push_back(sliceBetaOffsetDiv2);
        } // for (size_t i = 0; i < GetMaxNumSlices(video); i++)

        // Fill DPB
        mfxExtFeiPPS * pDataPPS = GetExtBufferFEI(outParams, field);

        std::vector<mfxFrameSurface1*> dpb_frames;

        mfxU16 indexFromPPSHeader;
        mfxU8  list_item;
        mfxU8  refParity; // 0 - top field / progressive frame; 1 - bottom field
        mfxFrameSurface1* pMfxFrame;
        mfxI32 poc_base;

        task.m_dpb[fieldParity].Resize(0);
        for (mfxU32 dpb_idx = 0; dpb_idx < 16; ++dpb_idx)
        {
            indexFromPPSHeader = pDataPPS->ReferenceFrames[dpb_idx];

            if (indexFromPPSHeader == 0xffff) break;

            pMfxFrame = inParams->L0Surface[indexFromPPSHeader];
            dpb_frames.push_back(pMfxFrame);

            DpbFrame frame;
            poc_base = 2 * ((pMfxFrame->Data.FrameOrder - task.m_frameOrderIdr) & 0x7fffffff);
            frame.m_poc          = PairI32(poc_base + (TFIELD != task.m_fid[0]), poc_base + (BFIELD != task.m_fid[0]));
            frame.m_frameOrder   = pMfxFrame->Data.FrameOrder;
            frame.m_frameNum     = frameOrder_frameNum[frame.m_frameOrder];
            frame.m_frameNumWrap = (frame.m_frameNum > task.m_frameNum) ? frame.m_frameNum - FRAME_NUM_MAX : frame.m_frameNum;

            frame.m_picNum[0] = task.m_fieldPicFlag ? 2 * frame.m_frameNumWrap + ( !fieldParity) : frame.m_frameNumWrap; // in original here field = ffid / sfid
            frame.m_picNum[1] = task.m_fieldPicFlag ? 2 * frame.m_frameNumWrap + (!!fieldParity) : frame.m_frameNumWrap;

            frame.m_refPicFlag[task.m_fid[0]] = 1;
            frame.m_refPicFlag[task.m_fid[1]] = task.m_fieldPicFlag ? (frame.m_frameOrder != task.m_frameOrder) : 1;

            frame.m_longterm  = 0;

            task.m_dpb[fieldParity].PushBack(frame);
        }

        // Get default reflists
        MfxHwH264Encode::InitRefPicList(task, fieldParity);
        ArrayU8x33 initList0 = task.m_list0[fieldParity];
        ArrayU8x33 initList1 = task.m_list1[fieldParity];

        // Fill reflists
        mfxExtFeiSliceHeader * pDataSliceHeader = extFeiSlice;
        /* Number of reference handling */
        mfxU32 maxNumRefL0 = pDataSliceHeader->Slice[0].NumRefIdxL0Active;
        mfxU32 maxNumRefL1 = pDataSliceHeader->Slice[0].NumRefIdxL1Active;

        mfxU16 indexFromSliceHeader;

        task.m_list0[fieldParity].Resize(0);
        for (mfxU32 list0_idx = 0; list0_idx < maxNumRefL0; ++list0_idx)
        {
            indexFromSliceHeader = pDataSliceHeader->Slice[0].RefL0[list0_idx].Index;
            refParity            = pDataSliceHeader->Slice[0].RefL0[list0_idx].PictureType == MFX_PICTYPE_BOTTOMFIELD;

            pMfxFrame = inParams->L0Surface[indexFromSliceHeader];

            list_item = static_cast<mfxU8>(std::distance(dpb_frames.begin(), std::find(dpb_frames.begin(), dpb_frames.end(), pMfxFrame)));

            task.m_list0[fieldParity].PushBack((refParity << 7) | list_item);
        }

        task.m_list1[fieldParity].Resize(0);
        for (mfxU32 list1_idx = 0; list1_idx < maxNumRefL1; ++list1_idx)
        {
            indexFromSliceHeader = pDataSliceHeader->Slice[0].RefL1[list1_idx].Index;
            refParity            = pDataSliceHeader->Slice[0].RefL1[list1_idx].PictureType == MFX_PICTYPE_BOTTOMFIELD;

            pMfxFrame = inParams->L0Surface[indexFromSliceHeader];

            list_item = static_cast<mfxU8>(std::distance(dpb_frames.begin(), std::find(dpb_frames.begin(), dpb_frames.end(), pMfxFrame)));

            task.m_list1[fieldParity].PushBack((refParity << 7) | list_item);
        }

        initList0.Resize(task.m_list0[fieldParity].Size());
        initList1.Resize(task.m_list1[fieldParity].Size());

        // Fill reflists modificators
        task.m_refPicList0Mod[fieldParity] = MfxHwH264Encode::CreateRefListMod(task.m_dpb[fieldParity], initList0, task.m_list0[fieldParity], task.m_viewIdx, task.m_picNum[fieldParity], true);
        task.m_refPicList1Mod[fieldParity] = MfxHwH264Encode::CreateRefListMod(task.m_dpb[fieldParity], initList1, task.m_list1[fieldParity], task.m_viewIdx, task.m_picNum[fieldParity], true);

    } // for (mfxU32 field = 0; field < fieldMaxCount; ++field)
#endif // MFX_VERSION >= 1023
} // void MfxH264FEIcommon::ConfigureTaskFEI

mfxStatus MfxH264FEIcommon::CheckInitExtBuffers(const MfxVideoParam & owned_video, const mfxVideoParam & passed_video)
{
    // Slice number control through CO3 is not allowed
    mfxExtCodingOption3 const * extOpt3 = GetExtBuffer(owned_video);

    MFX_CHECK(extOpt3->NumSliceI == owned_video.mfx.NumSlice, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(extOpt3->NumSliceP == owned_video.mfx.NumSlice, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(extOpt3->NumSliceB == owned_video.mfx.NumSlice, MFX_ERR_INVALID_VIDEO_PARAM);


    // Internal headers should be updated before CreateAccelerationService call
    const mfxExtFeiSPS* pDataSPS = GetExtBuffer(passed_video);
    if (pDataSPS)
    {
        // Check correctness of provided PPS parameters
        MFX_CHECK(pDataSPS->SPSId                 <=  31, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataSPS->Log2MaxPicOrderCntLsb <=  16, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataSPS->Log2MaxPicOrderCntLsb >=   4, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataSPS->PicOrderCntType       <=   2, MFX_ERR_INVALID_VIDEO_PARAM);

        // Update internal SPS if FEI SPS buffer attached
        mfxExtSpsHeader * extSps = GetExtBuffer(owned_video);

        extSps->seqParameterSetId           = pDataSPS->SPSId;
        extSps->picOrderCntType             = pDataSPS->PicOrderCntType;
        extSps->log2MaxPicOrderCntLsbMinus4 = pDataSPS->Log2MaxPicOrderCntLsb - 4;
    }

    const mfxExtFeiPPS* pDataPPS = GetExtBuffer(passed_video);
    if (pDataPPS)
    {
        MFX_CHECK(pDataPPS->SPSId                     <=  31, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataPPS->PPSId                     <= 255, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataPPS->NumRefIdxL0Active         <=  32, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataPPS->NumRefIdxL1Active         <=  32, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataPPS->ChromaQPIndexOffset       <=  12, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataPPS->ChromaQPIndexOffset       >= -12, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataPPS->SecondChromaQPIndexOffset <=  12, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataPPS->SecondChromaQPIndexOffset >= -12, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pDataPPS->Transform8x8ModeFlag      <=   1, MFX_ERR_INVALID_VIDEO_PARAM);

        // Update internal PPS if FEI PPS buffer attached
        mfxExtPpsHeader* extPps = GetExtBuffer(owned_video);
        MFX_CHECK(extPps, MFX_ERR_INVALID_VIDEO_PARAM);

        extPps->seqParameterSetId              = pDataPPS->SPSId;
        extPps->picParameterSetId              = pDataPPS->PPSId;

        extPps->picInitQpMinus26               = pDataPPS->PicInitQP - 26;
        extPps->numRefIdxL0DefaultActiveMinus1 = (std::max)(pDataPPS->NumRefIdxL0Active, mfxU16(1)) - 1;;
        extPps->numRefIdxL1DefaultActiveMinus1 = (std::max)(pDataPPS->NumRefIdxL1Active, mfxU16(1)) - 1;;

        extPps->chromaQpIndexOffset            = pDataPPS->ChromaQPIndexOffset;
        extPps->secondChromaQpIndexOffset      = pDataPPS->SecondChromaQPIndexOffset;
        extPps->transform8x8ModeFlag           = pDataPPS->Transform8x8ModeFlag;
    }

    // mfxExtFeiSliceHeader is useless at init stage for ENC and PAK (unlike the FEI ENCODE)
    const mfxExtFeiSliceHeader * pDataSliceHeader = GetExtBuffer(passed_video);
    MFX_CHECK(pDataSliceHeader == NULL, MFX_ERR_INVALID_VIDEO_PARAM);

    return MFX_ERR_NONE;
}


bool MfxH264FEIcommon::IsRunTimeInputExtBufferIdSupported(MfxVideoParam const & owned_video, mfxU32 id)
{
    mfxExtFeiParam const * feiParam = GetExtBuffer(owned_video);
    switch(feiParam->Func)
    {
        case MFX_FEI_FUNCTION_PAK:
            return (  id == MFX_EXTBUFF_FEI_PPS
                   || id == MFX_EXTBUFF_FEI_SLICE
                   || id == MFX_EXTBUFF_FEI_PAK_CTRL
                   || id == MFX_EXTBUFF_FEI_ENC_MV
                   );
        case MFX_FEI_FUNCTION_ENC:
            return (  id == MFX_EXTBUFF_FEI_PPS
                   || id == MFX_EXTBUFF_FEI_SLICE
                   || id == MFX_EXTBUFF_FEI_ENC_CTRL
                   || id == MFX_EXTBUFF_FEI_ENC_MV_PRED
                   || id == MFX_EXTBUFF_FEI_ENC_MB
                   || id == MFX_EXTBUFF_FEI_ENC_QP
                   );
        default:
            return true;
    }
}

bool MfxH264FEIcommon::IsRunTimeOutputExtBufferIdSupported(MfxVideoParam const & owned_video, mfxU32 id)
{
    return (  id == MFX_EXTBUFF_FEI_ENC_MV
           || id == MFX_EXTBUFF_FEI_ENC_MB_STAT
           || id == MFX_EXTBUFF_FEI_PAK_CTRL
           );
}

template mfxStatus MfxH264FEIcommon::CheckRuntimeExtBuffers<>(mfxENCInput* input, mfxENCOutput* output, const MfxVideoParam & owned_video);
#if MFX_VERSION >= 1023
template mfxStatus MfxH264FEIcommon::CheckRuntimeExtBuffers<>(mfxPAKInput* input, mfxPAKOutput* output, const MfxVideoParam & owned_video);
#else
template mfxStatus MfxH264FEIcommon::CheckRuntimeExtBuffers<>(mfxPAKOutput* input, mfxPAKOutput* output, const MfxVideoParam & owned_video);
#endif // MFX_VERSION >= 1023

template <typename T, typename U>
mfxStatus MfxH264FEIcommon::CheckRuntimeExtBuffers(T* input, U* output, const MfxVideoParam & owned_video)
{

    MFX_CHECK_NULL_PTR2(input, output);

    for (mfxU32 i = 0; i < input->NumExtParam; ++i)
    {
        MFX_CHECK_NULL_PTR1(input->ExtParam[i]);
        MFX_CHECK(MfxH264FEIcommon::IsRunTimeInputExtBufferIdSupported(owned_video, input->ExtParam[i]->BufferId), MFX_ERR_INVALID_VIDEO_PARAM);
    }

    for (mfxU32 i = 0; i < output->NumExtParam; ++i)
    {
        MFX_CHECK_NULL_PTR1(output->ExtParam[i]);
        MFX_CHECK(MfxH264FEIcommon::IsRunTimeOutputExtBufferIdSupported(owned_video, output->ExtParam[i]->BufferId), MFX_ERR_INVALID_VIDEO_PARAM);
    }

    // SPS at runtime is not allowed
    mfxExtFeiSPS* extFeiSPSinRuntime = GetExtBufferFEI(input, 0);
    MFX_CHECK(!extFeiSPSinRuntime, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU32 fieldMaxCount = (owned_video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;
    for (mfxU32 field = 0; field < fieldMaxCount; field++)
    {
        // SliceHeader is required to pass reference lists
        mfxExtFeiSliceHeader * extFeiSliceInRintime = GetExtBufferFEI(input, field);
        MFX_CHECK(extFeiSliceInRintime,                                           MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(extFeiSliceInRintime->Slice,                                    MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(extFeiSliceInRintime->NumSlice,                                 MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(extFeiSliceInRintime->NumSlice <= GetMaxNumSlices(owned_video), MFX_ERR_UNDEFINED_BEHAVIOR);
        /*
        if (task.m_numSlice[feiFieldId]){
            MFX_CHECK(extFeiSliceInRintime->NumSlice == task.m_numSlice[feiFieldId], MFX_ERR_UNDEFINED_BEHAVIOR);
        }
        else*/
        {
            MFX_CHECK(extFeiSliceInRintime->NumSlice == owned_video.mfx.NumSlice, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        for (mfxU32 i = 0; i < extFeiSliceInRintime->NumSlice; ++i)
        {
            MFX_CHECK(  I_SLICE(extFeiSliceInRintime->Slice[i].SliceType)
                     || P_SLICE(extFeiSliceInRintime->Slice[i].SliceType)
                     || B_SLICE(extFeiSliceInRintime->Slice[i].SliceType), MFX_ERR_INVALID_VIDEO_PARAM);

            MFX_CHECK(extFeiSliceInRintime->Slice[i].PPSId                      <= 255, MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(extFeiSliceInRintime->Slice[i].CabacInitIdc               <=   2, MFX_ERR_INVALID_VIDEO_PARAM);

            MFX_CHECK(extFeiSliceInRintime->Slice[i].DisableDeblockingFilterIdc <=   2, MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(extFeiSliceInRintime->Slice[i].SliceAlphaC0OffsetDiv2     <=   6, MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(extFeiSliceInRintime->Slice[i].SliceAlphaC0OffsetDiv2     >=  -6, MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(extFeiSliceInRintime->Slice[i].SliceBetaOffsetDiv2        <=   6, MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(extFeiSliceInRintime->Slice[i].SliceBetaOffsetDiv2        >=  -6, MFX_ERR_INVALID_VIDEO_PARAM);
        }

        mfxExtFeiPPS* extFeiPPSinRuntime = GetExtBufferFEI(input, field);
        MFX_CHECK(extFeiPPSinRuntime, MFX_ERR_UNDEFINED_BEHAVIOR);

        // Check that parameters from previous init kept unchanged
        {
            mfxExtPpsHeader* extPps = GetExtBuffer(owned_video);

            MFX_CHECK(extPps->seqParameterSetId              == extFeiPPSinRuntime->SPSId,                                        MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(extPps->picParameterSetId              == extFeiPPSinRuntime->PPSId,                                        MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(extPps->picInitQpMinus26               == extFeiPPSinRuntime->PicInitQP - 26,                               MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(extPps->numRefIdxL0DefaultActiveMinus1 == (std::max)(extFeiPPSinRuntime->NumRefIdxL0Active, mfxU16(1)) - 1, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(extPps->numRefIdxL1DefaultActiveMinus1 == (std::max)(extFeiPPSinRuntime->NumRefIdxL1Active, mfxU16(1)) - 1, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(extPps->chromaQpIndexOffset            == extFeiPPSinRuntime->ChromaQPIndexOffset,                          MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(extPps->secondChromaQpIndexOffset      == extFeiPPSinRuntime->SecondChromaQPIndexOffset,                    MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(extPps->transform8x8ModeFlag           == extFeiPPSinRuntime->Transform8x8ModeFlag,                         MFX_ERR_UNDEFINED_BEHAVIOR);
        }

#if MFX_VERSION >= 1023
        // Check DPB correctness
        mfxStatus sts = CheckDPBpairCorrectness(input, output, extFeiPPSinRuntime, owned_video);
        MFX_CHECK_STS(sts);
#endif // MFX_VERSION >= 1023
    }

#if MFX_VERSION >= 1023
    // Check that DPB_after of first field is equal to DPB_before of second
    if (fieldMaxCount == 2)
    {
        mfxExtFeiPPS *extFeiPPS_f = GetExtBufferFEI(input, 0),
                     *extFeiPPS_s = GetExtBufferFEI(input, 1);

    }
#endif

    return MFX_ERR_NONE;
}

#if MFX_VERSION >= 1023

template <typename T, typename U>
mfxStatus MfxH264FEIcommon::CheckDPBpairCorrectness(T * input, U* output, mfxExtFeiPPS* extFeiPPSinRuntime, const MfxVideoParam & owned_video)
{
    mfxStatus sts = CheckOneDPBCorrectness(extFeiPPSinRuntime->DpbBefore, owned_video);
    MFX_CHECK_STS(sts);

    sts = CheckOneDPBCorrectness(extFeiPPSinRuntime->DpbAfter, owned_video, extFeiPPSinRuntime->FrameType & MFX_FRAMETYPE_IDR);
    MFX_CHECK_STS(sts);

    std::vector<mfxU16> dpb_before;
    dpb_before.reserve(16);

    mfxU16 i;
    // Check that same surface is not repeated in DPB
    for (i = 0; i < 16; ++i)
    {
        if (extFeiPPSinRuntime->DpbBefore[i].Index == 0xffff)
            break;

        dpb_before.push_back(extFeiPPSinRuntime->DpbBefore[i].Index);
    }

    std::vector<mfxU16> dpb_after;
    dpb_after.reserve(16);

    for (i = 0; i < 16; ++i)
    {
        if (extFeiPPSinRuntime->DpbAfter[i].Index == 0xffff)
            break;

        dpb_after.push_back(extFeiPPSinRuntime->DpbAfter[i].Index);
    }

    // Check that new frame in dpb_after is current reference frame.
    // And that rest parameters for same frames matched.
    std::vector<mfxU16> newFrames;
    std::vector<mfxU16>::iterator result;
    mfxExtFeiPPS::mfxExtFeiPpsDPB *dpb_frame_before, *dpb_frame_after;

    for (i = 0; i < dpb_after.size(); ++i)
    {
        result = std::find(dpb_before.begin(), dpb_before.end(), dpb_after[i]);

        if (result == dpb_before.end())
        {
            newFrames.push_back(dpb_after[i]);
        }
        else
        {
            dpb_frame_before = extFeiPPSinRuntime->DpbBefore + std::distance(dpb_before.begin(), result);
            dpb_frame_after  = extFeiPPSinRuntime->DpbAfter  + i;

            // FrameNumWrap shouldn't change
            MFX_CHECK_WITH_ASSERT(dpb_frame_before->FrameNumWrap     == dpb_frame_after->FrameNumWrap,                                                     MFX_ERR_UNDEFINED_BEHAVIOR);
            // LongTermFrameIdx shouldn't change
            MFX_CHECK_WITH_ASSERT(dpb_frame_before->LongTermFrameIdx == dpb_frame_after->LongTermFrameIdx || dpb_frame_before->LongTermFrameIdx == 0xffff, MFX_ERR_UNDEFINED_BEHAVIOR);
            // Allowed transformation of PicType is following: unchanged or one field became unused. The rest are forbidden.
            MFX_CHECK_WITH_ASSERT((dpb_frame_before->PicType & dpb_frame_after->PicType) && (dpb_frame_before->PicType >= dpb_frame_after->PicType),       MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }
    MFX_CHECK_WITH_ASSERT(newFrames.size() <= 1, MFX_ERR_UNDEFINED_BEHAVIOR);
    if (!newFrames.empty())
    {
        MFX_CHECK_WITH_ASSERT(input->L0Surface[newFrames[0]] == output->OutSurface && (extFeiPPSinRuntime->FrameType & MFX_FRAMETYPE_REF), MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    return MFX_ERR_NONE;
}

mfxStatus MfxH264FEIcommon::CheckOneDPBCorrectness(mfxExtFeiPPS::mfxExtFeiPpsDPB* DPB, const MfxVideoParam & owned_video, bool is_IDR_field)
{
    MFX_CHECK_WITH_ASSERT(DPB, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU32 i, dpb_size;

    std::vector<mfxU16> dpb_setting;
    dpb_setting.reserve(16);

    // Check that same surface is not repeated in DPB
    for (i = 0; i < 16; ++i)
    {
        if (DPB[i].Index == 0xffff)
            break;

        dpb_setting.push_back(DPB[i].Index);
    }
    MFX_CHECK_WITH_ASSERT(std::unique(dpb_setting.begin(), dpb_setting.end()) == dpb_setting.end(), MFX_ERR_UNDEFINED_BEHAVIOR);

    // Check that DPB size not exceeds the limit
    MFX_CHECK_WITH_ASSERT(dpb_setting.size() <= (is_IDR_field ? 1 : owned_video.mfx.NumRefFrame), MFX_ERR_UNDEFINED_BEHAVIOR);

    // Check that same LT ref index is not assigned to different frames
    dpb_size = dpb_setting.size();
    dpb_setting.clear();

    for (i = 0; i < dpb_size; ++i)
    {
        if (DPB[i].LongTermFrameIdx != 0xffff) dpb_setting.push_back(DPB[i].LongTermFrameIdx);
    }
    MFX_CHECK_WITH_ASSERT(std::unique(dpb_setting.begin(), dpb_setting.end()) == dpb_setting.end(), MFX_ERR_UNDEFINED_BEHAVIOR);

    // Check that same FrameNumWrap is not assigned to different frames
    dpb_setting.clear();

    for (i = 0; i < dpb_size; ++i)
    {
        dpb_setting.push_back(DPB[i].FrameNumWrap);
    }
    MFX_CHECK_WITH_ASSERT(std::unique(dpb_setting.begin(), dpb_setting.end()) == dpb_setting.end(), MFX_ERR_UNDEFINED_BEHAVIOR);

    return MFX_ERR_NONE;
}

#endif // MFX_VERSION >= 1023

#endif // defined(MFX_VA) && defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)
