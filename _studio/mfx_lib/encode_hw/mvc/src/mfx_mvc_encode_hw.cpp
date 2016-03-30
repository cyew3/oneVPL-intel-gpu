/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "ipps.h"

#include "mfx_common.h"

#ifdef MFX_ENABLE_MVC_VIDEO_ENCODE_HW

#include <algorithm>
#include <functional>
#include "mfx_h264_encode_hw_utils.h"
#include "mfx_task.h"
#include "vm_time.h"

#pragma warning(disable: 4127)

using namespace MfxHwH264Encode;

namespace
{
    mfxStatus CheckVideoParamMvc(
        MfxVideoParam &     par,
        ENCODE_CAPS const & hwCaps,
        bool                setExtAlloc,
        eMFXHWType          platform = MFX_HW_UNKNOWN)
    {
        mfxStatus sts = CheckVideoParam(par, hwCaps, setExtAlloc, platform);

        /*
        mfxExtMVCSeqDesc * extMvc = GetExtBuffer(par);

        for (mfxU32 i = 1; i < extMvc->NumView; i++)
        {
            if (extMvc->View[i].NumNonAnchorRefsL0 > 0)
                extMvc->View[i].NumNonAnchorRefsL0 = 0;

            if (extMvc->View[i].NumAnchorRefsL1 > 0)
                extMvc->View[i].NumAnchorRefsL1 = 0;

            if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0)
            {
                if (extMvc->View[i].NumAnchorRefsL0 > 0)
                    extMvc->View[i].NumAnchorRefsL0 = 0;

                if (extMvc->View[i].NumNonAnchorRefsL1 > 0)
                    extMvc->View[i].NumNonAnchorRefsL1 = 0;
            }

            if (extMvc->View[i].NumAnchorRefsL0 > 0 ||
                extMvc->View[i].NumAnchorRefsL1 > 0 ||
                extMvc->View[i].NumNonAnchorRefsL0 > 0 ||
                extMvc->View[i].NumNonAnchorRefsL1 > 0)
            {
                par.mfx.GopRefDist = 1;
            }
        }
        */

        return sts;
    }

    mfxStatus ParseAvcBitstream(BitstreamDesc & bitsDesc)
    {
        Zero(bitsDesc.aud);
        Zero(bitsDesc.sps);
        Zero(bitsDesc.pps);
        Zero(bitsDesc.sei);
        Zero(bitsDesc.slice);

        for (NaluIterator nalu(bitsDesc.begin, bitsDesc.end); nalu != NaluIterator(); ++nalu)
        {
            if (nalu->type == 9)
            {
                MFX_CHECK(
                    bitsDesc.aud.begin   == 0 &&
                    bitsDesc.sps.begin   == 0 &&
                    bitsDesc.pps.begin   == 0 &&
                    bitsDesc.slice.begin == 0,
                    MFX_ERR_UNDEFINED_BEHAVIOR);

                bitsDesc.aud = *nalu;
            }
            else if (nalu->type == 7)
            {
                MFX_CHECK(
                    bitsDesc.sps.begin   == 0 &&
                    bitsDesc.pps.begin   == 0 &&
                    bitsDesc.slice.begin == 0,
                    MFX_ERR_UNDEFINED_BEHAVIOR);

                bitsDesc.sps = *nalu;
            }
            else if (nalu->type == 8)
            {
                MFX_CHECK(
                    bitsDesc.pps.begin   == 0 &&
                    bitsDesc.slice.begin == 0,
                    MFX_ERR_UNDEFINED_BEHAVIOR);

                bitsDesc.pps = *nalu;
            }
            else if (nalu->type == 6 && bitsDesc.sei.begin == 0)
            {
                MFX_CHECK(
                    bitsDesc.slice.begin == 0,
                    MFX_ERR_UNDEFINED_BEHAVIOR);

                bitsDesc.sei = *nalu;
            }
            else if (nalu->type == 1 || nalu->type == 5)
            {
                bitsDesc.slice = *nalu;
                break;
            }
        }

        assert(bitsDesc.slice.begin != 0 && "slice not found");
        MFX_CHECK(bitsDesc.slice.begin != 0, MFX_ERR_UNDEFINED_BEHAVIOR);

        return MFX_ERR_NONE;
    }

    mfxU8 * PatchSequenceHeader(
        mfxU8 *               mvcSpsBegin,
        mfxU8 *               mvcSpsEnd,
        mfxU8 const *         avcSpsBegin,
        mfxU8 const *         avcSpsEnd,
        mfxVideoParam const & video)
    {
        mfxExtMVCSeqDesc const * extMvc = GetExtBuffer(video);

        mfxExtSpsHeader sps = { 0 };

        InputBitstream ibs(avcSpsBegin, avcSpsEnd);
        ReadSpsHeader(ibs, sps);

        OutputBitstream obs(mvcSpsBegin,  mvcSpsEnd);

        sps.maxNumRefFrames /= 3;

        if (sps.seqParameterSetId == 0)
        { // base view
            WriteSpsHeader(obs, sps);
        }
        else
        { // dependent view
            sps.profileIdc  = mfxU8(video.mfx.CodecProfile);
            sps.nalUnitType = 15;
            WriteSpsHeader(obs, sps, extMvc->Header);
        }

        return mvcSpsBegin + (obs.GetNumBits() + 7) / 8;
    }

    mfxU8 * RePackSequenceHeader(
        mfxU8 *               mvcSpsBegin,
        mfxU8 *               mvcSpsEnd,
        mfxU8 const *         avcSpsBegin,
        mfxU8 const *         avcSpsEnd,
        mfxVideoParam const & video)
    {
        mfxExtMVCSeqDesc * extMvc = GetExtBuffer(video);

        OutputBitstream writer(mvcSpsBegin, mvcSpsEnd);

        // copy avc sequence header except trailing bits
        if (*(avcSpsEnd - 1) == 0)
        {
            writer.PutRawBytes(avcSpsBegin, avcSpsEnd - 2);
            mfxU32 last7bits = *(avcSpsEnd - 2) >> 1;
            writer.PutBits(last7bits, 7);
        }
        else
        {
            writer.PutRawBytes(avcSpsBegin, avcSpsEnd - 1);

            mfxU32 numTrailingBits = 0;
            mfxU32 lastByte = *(avcSpsEnd - 1);
            while (((lastByte >> numTrailingBits) & 1) == 0)
                numTrailingBits++;
            numTrailingBits++;

            writer.PutBits((lastByte >> numTrailingBits), 8 - numTrailingBits);
        }

        writer.PutBit(1);                                   // bit_equal_to_one
        writer.PutUe(extMvc->NumView - 1);                  // num_views_minus1

        for (mfxU32 i = 0; i < extMvc->NumView; i++)
            writer.PutUe(extMvc->View[i].ViewId);           // view_id[i]

        for (mfxU32 i = 1; i < extMvc->NumView; i++)
        {
            writer.PutUe(extMvc->View[i].NumAnchorRefsL0);  // num_anchor_refs_l0[i]
            for (mfxU32 j = 0; j < extMvc->View[i].NumAnchorRefsL0; j++)
                writer.PutUe(extMvc->View[i].AnchorRefL0[j]);

            writer.PutUe(extMvc->View[i].NumAnchorRefsL1);  // num_anchor_refs_l1[i]
            for (mfxU32 j = 0; j < extMvc->View[i].NumAnchorRefsL1; j++)
                writer.PutUe(extMvc->View[i].AnchorRefL1[j]);
        }

        for (mfxU32 i = 1; i < extMvc->NumView; i++)
        {
            writer.PutUe(extMvc->View[i].NumNonAnchorRefsL0);
            for (mfxU32 j = 0; j < extMvc->View[i].NumNonAnchorRefsL0; j++)
                writer.PutUe(extMvc->View[i].NonAnchorRefL0[j]);

            writer.PutUe(extMvc->View[i].NumNonAnchorRefsL1);
            for (mfxU32 j = 0; j < extMvc->View[i].NumNonAnchorRefsL1; j++)
                writer.PutUe(extMvc->View[i].NonAnchorRefL1[j]);
        }

        writer.PutUe(extMvc->NumOP - 1);

        for (mfxU32 i = 0; i < extMvc->NumOP; i++)
        {
            writer.PutBits(video.mfx.CodecLevel, 8);        // level_idc
            writer.PutUe(0);                                // num_applicable_ops_minus1

            for (mfxU32 j = 0; j < 1; j++)
            {
                writer.PutBits(extMvc->OP[i].TemporalId, 3);
                writer.PutUe(extMvc->OP[i].NumTargetViews - 1);
                for (mfxU32 k = 0; k < extMvc->OP[i].NumTargetViews; k++)
                    writer.PutUe(extMvc->OP[i].TargetViewId[k]);

                writer.PutUe(extMvc->OP[i].NumViews - 1);   // applicable_op_num_views_minus1
            }
        }

        writer.PutBit(0);                                   // mvc_vui_parameters_present_flag
        writer.PutBit(0);                                   // additional_extension2_flag
        writer.PutTrailingBits();

        mfxU8 * endOfMvcSps = mvcSpsBegin + writer.GetNumBits() / 8;

        // patch nal_unit_type by substitute 0x7 with 0xf
        // and profile_idc by substitute 100 with 128/118
        while (*mvcSpsBegin == 0)
            mvcSpsBegin++;
        mvcSpsBegin++;

        *mvcSpsBegin = (*mvcSpsBegin & 0xe0) | 0xf;
        mvcSpsBegin++;
        *mvcSpsBegin = mfxU8(video.mfx.CodecProfile);

        return endOfMvcSps;
    }

    mfxU8 * RePackAudToDrd(
        mfxU8 *               mvcDrdBegin,
        mfxU8 *               mvcDrdEnd,
        mfxU8 const *         avcAudBegin,
        mfxU8 const *         avcAudEnd)
    {
        OutputBitstream writer(mvcDrdBegin, mvcDrdEnd);
        writer.PutRawBytes(avcAudBegin, avcAudEnd);
        mfxU8 * endOfMvcDrd = mvcDrdBegin + writer.GetNumBits() / 8;

        while (*mvcDrdBegin == 0)
            mvcDrdBegin++;
        mvcDrdBegin++;

        *mvcDrdBegin = (*mvcDrdBegin & 0xe0) | 0x18;

        return endOfMvcDrd;
    }

    // add idr_pic_id, dec_ref_pic_marking()
    mfxU8 * RePackNonIdrSliceToIdr(
        mfxU8 *               mvcSliceBegin,
        mfxU8 *               mvcSliceEnd,
        mfxU8 *               avcSliceBegin,
        mfxU8 *               avcSliceEnd,
        MfxVideoParam const & par,
        DdiTask const &       task,
        mfxU32                fieldId)
    {
        mfxExtSpsHeader * extSps = GetExtBuffer(par);
        mfxExtPpsHeader * extPps = GetExtBuffer(par);

        mfxU32 sliceType    = 0;
        mfxU32 fieldPicFlag = 0;

        mfxU32 tmp = 0;

        if (extPps->entropyCodingModeFlag == 0)
        {
            // remove start code emulation prevention bytes when doing full repack for CAVLC
            mfxU32 zeroCount = 0;
            mfxU8 * write = avcSliceBegin;
            for (mfxU8 * read = write; read != avcSliceEnd; ++read)
            {
                if (*read == 0x03 && zeroCount >= 2 && read + 1 != avcSliceEnd && (*(read + 1) & 0xfc) == 0)
                {
                    // skip start code emulation prevention byte
                    zeroCount = 0; // drop zero count
                }
                else
                {
                    *(write++) = *read;
                    zeroCount = (*read == 0) ? zeroCount + 1 : 0;
                }
            }
        }

#ifdef AVC_BS
        // copy 0x00
        while (*avcSliceBegin == 0)
        {
            *mvcSliceBegin = *avcSliceBegin;
            avcSliceBegin++;
            mvcSliceBegin++;
        }
        // copy 0x01
        *mvcSliceBegin = *avcSliceBegin;
        avcSliceBegin++;
        mvcSliceBegin++;

        // copy nal_ref_idc, nal_unit_type
        *mvcSliceBegin = *avcSliceBegin;
        avcSliceBegin++;

        *mvcSliceBegin = (*mvcSliceBegin & 0xe0) | 0x5;
        mvcSliceBegin++;

#endif // AVC_BS

        InputBitstream  reader(avcSliceBegin, avcSliceEnd, true, extPps->entropyCodingModeFlag == 1);
        OutputBitstream writer(mvcSliceBegin, mvcSliceEnd);

        writer.PutUe(reader.GetUe());                      // first_mb_in_slice
        writer.PutUe(sliceType = reader.GetUe());          // slice_type
        assert(sliceType % 5 == 0 || sliceType % 5 == 2);
        writer.PutUe(reader.GetUe());                      // pic_parameter_set_id

        mfxU32 log2MaxFrameNum = extSps->log2MaxFrameNumMinus4 + 4;
        writer.PutBits(reader.GetBits(log2MaxFrameNum), log2MaxFrameNum);

        if (!extSps->frameMbsOnlyFlag)
        {
            writer.PutBit(fieldPicFlag = reader.GetBit()); // field_pic_flag
            if (fieldPicFlag)
                writer.PutBit(reader.GetBit());            // bottom_field_flag
        }

        if (task.m_type[fieldId] & MFX_FRAMETYPE_IDR)
            writer.PutUe(task.m_idrPicId);                 // idr_pic_id

        if (extSps->picOrderCntType == 0)
        {
            mfxU32 log2MaxPicOrderCntLsb = extSps->log2MaxPicOrderCntLsbMinus4 + 4;
            writer.PutBits(reader.GetBits(log2MaxPicOrderCntLsb), log2MaxPicOrderCntLsb);
        }

        if (sliceType % 5 == 0)
        {
            writer.PutBit(tmp = reader.GetBit());          // num_ref_idx_active_override_flag
            if (tmp)
                writer.PutUe(reader.GetUe());              // num_ref_idx_l0_active_minus1
            ReadRefPicListModification(reader);            // read ref_pic_list_modification()
            writer.PutBit(0);                              // write ref_pic_list_modification_flag_l0 = 0
        }

        if (task.m_type[fieldId] & MFX_FRAMETYPE_REF)
        {
            bool idrPicFlag = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) != 0;
            ReadDecRefPicMarking(reader, 0); // we repack non-IDR to IDR so IdrPicFlag is zero for source slice
            DecRefPicMarkingInfo tmpInfo;
            tmpInfo.long_term_reference_flag = 0;
            tmpInfo.no_output_of_prior_pics_flag = 0;
            WriteDecRefPicMarking(writer, tmpInfo, idrPicFlag);
        }

        if (extPps->entropyCodingModeFlag && (sliceType % 5 == 0))
            writer.PutUe(reader.GetUe());                  // read cabac_init_idc

        writer.PutSe(reader.GetSe());                      // slice_qp_delta

        if (1/*deblocking_filter_control_present_flag*/)
        {
            writer.PutUe(tmp = reader.GetUe());            // disable_deblocking_filter_idc
            if (tmp != 1)
            {
                writer.PutSe(reader.GetSe());              // slice_alpha_c0_offset_div2
                writer.PutSe(reader.GetSe());              // slice_beta_offset_div2
            }
        }

        if (extPps->entropyCodingModeFlag)
        {
            while (reader.NumBitsRead() % 8)
                reader.GetBit();                           // cabac_alignment_one_bit

            mfxU32 numAlignmentBits = (8 - (writer.GetNumBits() & 0x7)) & 0x7;
            writer.PutBits(0xff, numAlignmentBits);

            avcSliceBegin += reader.NumBitsRead() / 8;
            mvcSliceBegin += writer.GetNumBits() / 8;

            MFX_INTERNAL_CPY(mvcSliceBegin, avcSliceBegin, (Ipp32u)(avcSliceEnd - avcSliceBegin));
            mvcSliceBegin += avcSliceEnd - avcSliceBegin;
        }
        else
        {
            mfxU32 bitsLeft = reader.NumBitsLeft();

            for (; bitsLeft > 31; bitsLeft -= 32)
                writer.PutBits(reader.GetBits(32), 32);

            writer.PutBits(reader.GetBits(bitsLeft), bitsLeft);
            writer.PutBits(0, (8 - (writer.GetNumBits() & 7)) & 7); // trailing zeroes

            assert((reader.NumBitsRead() & 7) == 0);
            assert((writer.GetNumBits()  & 7) == 0);

            avcSliceBegin += (reader.NumBitsRead() + 7) / 8;
            mvcSliceBegin += (writer.GetNumBits()  + 7) / 8;
        }

        return mvcSliceBegin;
    }

    mfxU8 * PackPrefixNalUnit(
        mfxU8 *               mvcSliceBegin,
        mfxU8 *               mvcSliceEnd,
        mfxU32                numZeros,
        mfxU32                nalRefIdc,
        mfxU32                nalUnitType,
        mfxU32                viewId,
        mfxU32                nonIdrFlag,
        mfxU32                anchorPicFlag,
        mfxU32                interViewFlag)
    {
        // add nal header extention
        OutputBitstream writer(mvcSliceBegin, mvcSliceEnd, false);

        while (numZeros > 0)
        {
            writer.PutBits(0, 8);
            numZeros--;
        }

        writer.PutBits(1, 8);
        writer.PutBits(0, 1);               // forbidden_zero_bit
        writer.PutBits(nalRefIdc, 2);       // nal_ref_idc
        writer.PutBits(nalUnitType, 5);     // nal_unit_type
        writer.PutBit(0);                   // svc_extension_flag
        writer.PutBit(nonIdrFlag);          // non_idr_flag
        writer.PutBits(0, 6);               // priority_id
        writer.PutBits(viewId, 10);         // view_id
        writer.PutBits(0, 3);               // temporal_id
        writer.PutBit(anchorPicFlag);       // anchor_pic_flag
        writer.PutBit(interViewFlag);       // inter_view_flag
        writer.PutBit(1);                   // reserved_one_bit

        return mvcSliceBegin + writer.GetNumBits() / 8;
    }

    mfxU16 ViewId2Idx(mfxExtMVCSeqDesc const & desc, mfxU32 id)
    {
        for (mfxU16 i = 0; i < desc.NumView; i++)
            if (desc.View[i].ViewId == id)
                return i;
        assert(!"bad view id");
        return 0xffff;
    }

    mfxU32 GetInterViewFlag(mfxExtMVCSeqDesc const & desc, mfxU32 viewIdx, mfxU32 anchorPicFlag)
    {
        mfxU32 viewId = desc.View[viewIdx].ViewId;

        for (mfxU16 i = 0; i < desc.NumView; i++)
        {
            if (anchorPicFlag)
            {
                for (mfxU32 j = 0; j < desc.View[i].NumAnchorRefsL0; j++)
                    if (desc.View[i].AnchorRefL0[j] == viewId)
                        return 1;
                for (mfxU32 j = 0; j < desc.View[i].NumAnchorRefsL1; j++)
                    if (desc.View[i].AnchorRefL1[j] == viewId)
                        return 1;
            }
            else
            {
                for (mfxU32 j = 0; j < desc.View[i].NumNonAnchorRefsL0; j++)
                    if (desc.View[i].NonAnchorRefL0[j] == viewId)
                        return 1;
                for (mfxU32 j = 0; j < desc.View[i].NumNonAnchorRefsL1; j++)
                    if (desc.View[i].NonAnchorRefL1[j] == viewId)
                        return 1;
            }
        }

        return 0;
    }

    ArrayU8x33 ModifyList(
        ArrayDpbFrame const & dpb,
        ArrayU8x33 const &    initList,
        mfxU32                curViewIdx)
    {
        ArrayU8x33 modList;

        for (mfxU32 i = 0; i < initList.Size(); i++)
            if (dpb[initList[i] & 0x7f].m_viewIdx != curViewIdx)
                modList.PushBack(initList[i]);

        for (mfxU32 i = 0; i < initList.Size(); i++)
            if (dpb[initList[i] & 0x7f].m_viewIdx == curViewIdx)
                modList.PushBack(initList[i]);

        return modList;
    }
};

TaskManagerMvc::TaskManagerMvc()
: m_core(0)
{
}

TaskManagerMvc::~TaskManagerMvc()
{
    Close();
}

void TaskManagerMvc::Init(
    VideoCORE *           core,
    MfxVideoParam const & video)
{
    m_core = core;

    mfxExtMVCSeqDesc const * extMvc = GetExtBuffer(video);
    m_numView = extMvc->NumView;
// MVC BD {
    mfxExtCodingOption * extOpt = GetExtBuffer(video);
    m_viewOut                   = extOpt->ViewOutput == MFX_CODINGOPTION_ON;
// MVC BD }

    mfxExtCodingOptionDDI * extDdi = GetExtBuffer(video);
    m_numTask = video.mfx.GopRefDist + (video.AsyncDepth - 1);
    if (IsOn(extDdi->RefRaw))
        m_numTask += video.mfx.NumRefFrame;

    m_viewMan.reset(new TaskManager[m_numView]);
    m_task.reset(new MvcTask[m_numTask]);

    for (mfxU32 i = 0; i < m_numView; i++)
        m_viewMan[i].Init(m_core, video, i);

    for (mfxU32 i = 0; i < m_numTask; i++)
        m_task[i].Allocate(m_numView);

    m_currentView = 0;
    m_seqDesc = *(mfxExtMVCSeqDesc *)GetExtBuffer(video);
}

void TaskManagerMvc::Reset(MfxVideoParam const & video)
{
    m_seqDesc = *(mfxExtMVCSeqDesc *)GetExtBuffer(video);

    for (mfxU32 i = 0; i < m_numView; i++)
        m_viewMan[i].Reset(video);
}

void TaskManagerMvc::Close()
{
    m_viewMan.reset(0);
    m_task.reset(0);
}

mfxStatus TaskManagerMvc::AssignTask(
    MfxVideoParam const & m_video,
    mfxEncodeCtrl *       ctrl,
    mfxFrameSurface1 *    surface,
    mfxBitstream *        bs,
    MvcTask *&            newTask)
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    if (m_viewOut == 0 && surface == 0 && m_currentView != 0)
        return Error(MFX_ERR_UNDEFINED_BEHAVIOR);

    if (m_currentView == 0)
    {
        m_currentTask = std::find_if(
            m_task.cptr(),
            m_task.cptr() + m_numTask,
            std::mem_fun_ref(&DdiTask::IsFree));

        if (m_currentTask == m_task.cptr() + m_numTask)
            return Warning(MFX_WRN_DEVICE_BUSY);

        // clean
        for (mfxU32 i = 0; i < m_currentTask->NumView(); i++)
            (*m_currentTask)[i] = 0;

        // one frame type for all views
        m_baseCtrlFrameType = ctrl ? ctrl->FrameType : 0;
    }

    newTask = m_currentTask;

    if (surface == 0)
    {
        if (m_viewOut)
        {
            // in ViewOutput mode Assign 1 task per call
            mfxStatus sts = AssignAndConfirmAvcTask(m_video, ctrl, 0, bs, m_currentView);
            if (sts == MFX_ERR_NONE)
                m_currentView++;
            MFX_CHECK_STS(sts);
        }
        else
            for (; m_currentView < m_numView; ++m_currentView)
            {
                mfxStatus sts = AssignAndConfirmAvcTask(m_video, ctrl, 0, bs, m_currentView);
                MFX_CHECK_STS(sts);
            }
    }
    else
    {
        if (ctrl)
            ctrl->FrameType = m_baseCtrlFrameType;

        mfxU32 viewIdx = ViewId2Idx(m_seqDesc, surface->Info.FrameId.ViewId);
        mfxStatus sts = AssignAndConfirmAvcTask(m_video, ctrl, surface, bs, viewIdx);
        if (sts == MFX_ERR_NONE || sts == MFX_ERR_MORE_DATA)
        {
            m_currentView++;
            if (sts == MFX_ERR_MORE_DATA)
                m_currentView %= m_numView;
        }
        MFX_CHECK_STS(sts);
    }

// MVC BD {
    if (m_currentView < m_numView && m_viewOut == 0)
// MVC BD }
        return MFX_ERR_MORE_DATA;

    /*
    mfxU32 idrPicFlag = !!((*m_currentTask)[0]->m_type.top & MFX_FRAMETYPE_IDR);

    // modify ref lists and dpb for interview prediction
    for (mfxU32 viewIdx = 1; viewIdx < m_numView; viewIdx++)
    {
        mfxMVCViewDependency & viewDesc = m_seqDesc.View[viewIdx];

        DdiTask & subTask  = *(*m_currentTask)[viewIdx];
        mfxU16   numRefsL0 = idrPicFlag ? viewDesc.NumAnchorRefsL0 : viewDesc.NumNonAnchorRefsL0;
        mfxU16   numRefsL1 = idrPicFlag ? viewDesc.NumAnchorRefsL1 : viewDesc.NumNonAnchorRefsL1;
        mfxU16 * refL0     = idrPicFlag ? viewDesc.AnchorRefL0 : viewDesc.NonAnchorRefL0;
        mfxU16 * refL1     = idrPicFlag ? viewDesc.AnchorRefL1 : viewDesc.NonAnchorRefL1;

        // add inter-view prediction if needed (to each field)
        for (mfxU32 f = 0; f < subTask.GetPictureCount(); f++)
        {
            mfxU8 fieldFlag = mfxU8(f * 0x80);

            ArrayDpbFrame & dpb = subTask.m_dpb[f];

            if (subTask.m_type[f] & (MFX_FRAMETYPE_P | MFX_FRAMETYPE_B))
            {
                ArrayU8x33 & list0 = subTask.m_list0[f];

                for (mfxU32 i = 0; i < numRefsL0; i++)
                {
                    if (list0.m_numElem == 32 || dpb.m_numElem == 16)
                        break; // no more place in dpb or list0

                    mfxU16 refViewIdx = ViewId2Idx(m_seqDesc, refL0[i]);
                    assert(refViewIdx < viewIdx);

                    DdiTask const & subTaskRef = *(*m_currentTask)[refViewIdx];
                    list0.PushBack(mfxU8(dpb.m_numElem | fieldFlag));

                    dpb[dpb.m_numElem].m_poc.top  = subTaskRef.GetPoc(TFIELD);
                    dpb[dpb.m_numElem].m_poc.bot  = subTaskRef.GetPoc(BFIELD);
                    dpb[dpb.m_numElem].m_viewIdx  = refViewIdx;
                    dpb[dpb.m_numElem].m_frameIdx = mfxU8(subTaskRef.m_idxRecon);
                    dpb.m_numElem++;
                }

                ArrayU8x33 oldList0 = list0;
                list0 = ModifyList(dpb, oldList0, viewIdx);

                subTask.m_refPicList0Mod[f] = CreateRefListMod(
                    dpb,
                    m_viewMan[viewIdx].m_recons,
                    oldList0,
                    list0,
                    viewIdx,
                    subTask.m_picNum[f]);
            }

            if (subTask.m_type[f] & MFX_FRAMETYPE_B)
            {
                ArrayU8x33 & list1 = subTask.m_list1[f];

                for (mfxU32 idxL1 = 0; idxL1 < numRefsL1; idxL1++)
                {
                    if (list1.m_numElem == 32 || dpb.m_numElem == 16)
                        break; // no more place in dpb or list1

                    mfxU16 refViewIdx = ViewId2Idx(m_seqDesc, refL1[idxL1]);
                    assert(refViewIdx < viewIdx);
                    DdiTask const & subTaskRef = *(*m_currentTask)[refViewIdx];
                    list1.PushBack(mfxU8(dpb.m_numElem | fieldFlag));

                    dpb[dpb.m_numElem].m_poc.top  = subTaskRef.GetPoc(TFIELD);
                    dpb[dpb.m_numElem].m_poc.bot  = subTaskRef.GetPoc(BFIELD);
                    dpb[dpb.m_numElem].m_viewIdx  = refViewIdx;
                    dpb[dpb.m_numElem].m_frameIdx = mfxU8(subTaskRef.m_idxRecon);
                    dpb.m_numElem++;
                }

                ArrayU8x33 oldList1 = list1;

                list1 = ModifyList(dpb, oldList1, viewIdx);

                subTask.m_refPicList1Mod[f] = CreateRefListMod(
                    dpb,
                    m_viewMan[viewIdx].m_recons,
                    oldList1,
                    list1,
                    viewIdx,
                    subTask.m_picNum[f]);
            }
        }
    }
    */

// MVC BD {
    if (m_viewOut == 0 || (m_viewOut && m_currentView == 1))
        m_currentTask->SetFree(false);

    if (m_currentView < m_numView && m_viewOut)
        return MFX_ERR_MORE_BITSTREAM;
// MVC BD }
    m_currentView = 0;

    return MFX_ERR_NONE;
}

void TaskManagerMvc::CompleteTask(MvcTask & task)
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    for (mfxU32 viewIdx = 0; viewIdx < m_numView; viewIdx++)
        m_viewMan[viewIdx].CompleteTask(*task[viewIdx]);

    task.SetFree(true);
}

// MVC BD {
// returns MvcTask for passed DdiTask
MvcTask * TaskManagerMvc::GetMvcTask(DdiTask & task)
{
    UMC::AutomaticUMCMutex guard(m_mutex);
    for (mfxU32 taskIdx = 0; taskIdx < m_numTask; taskIdx ++)
    {
        MvcTask & mvcTask = m_task[taskIdx];
        for (mfxU32 i = 0; i < m_numView; i ++)
            if (mvcTask[i] == &task && mvcTask.IsFree() == false)
                return &mvcTask;
    }

    assert(0);
    return 0;
}
// check if all subtasks of MvcTask were successfully encoded
bool TaskManagerMvc::IsMvcTaskFinished(MvcTask & task)
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    bool b_ready = true;
    for (mfxU32 i = 0; i < m_numView; i ++)
    {
        if (task[i] == 0 ||
            (task[i] && task[i]->m_bsDataLength[0] == 0 && task[i]->m_bsDataLength[1] == 0))
            b_ready = false;
    }

    return b_ready;
}

void TaskManagerMvc::CompleteTask(DdiTask & task)
{
    UMC::AutomaticUMCMutex guard(m_mutex);
    MvcTask & taskToFree = *(GetMvcTask(task));

    for (mfxU32 viewIdx = 0; viewIdx < m_numView; viewIdx++)
        m_viewMan[viewIdx].CompleteTask(*taskToFree[viewIdx]);

    taskToFree.SetFree(true);
}
// MVC BD }

mfxStatus TaskManagerMvc::AssignAndConfirmAvcTask(
    MfxVideoParam const & m_video,
    mfxEncodeCtrl *       ctrl,
    mfxFrameSurface1 *    surface,
    mfxBitstream *        bs,
    mfxU32                viewIdx)
{
    mfxStatus sts = m_viewMan[viewIdx].AssignTask(ctrl, surface, bs, (*m_currentTask)[viewIdx]);
    MFX_CHECK_STS(sts);

// MVC BD {
    DdiTask * task = (*m_currentTask)[viewIdx];
    if ((task->GetFrameType() & MFX_FRAMETYPE_IDR) && (m_viewMan[viewIdx].CountRunningTasks() > 0))
    {
        mfxExtCodingOption * opt = GetExtBuffer(m_video);
        if (IsOn(opt->VuiNalHrdParameters) || IsOn(opt->VuiVclHrdParameters))
            return MFX_WRN_DEVICE_BUSY;
    }
// MVC BD }

    /*
    if (viewIdx > 0)
    {
        mfxMVCViewDependency & viewDesc = m_seqDesc.View[viewIdx];
        mfxU32 idrPicFlag = !!((*m_currentTask)[viewIdx]->m_type.top & MFX_FRAMETYPE_IDR);
        mfxU16 numRefsL0  = idrPicFlag ? viewDesc.NumAnchorRefsL0 : viewDesc.NumNonAnchorRefsL0;
        mfxU16 numRefsL1  = idrPicFlag ? viewDesc.NumAnchorRefsL1 : viewDesc.NumNonAnchorRefsL1;

        DdiTask & subTask = *(*m_currentTask)[viewIdx];

        Pair<mfxU8> & frameType   = subTask.m_type;
        Pair<ArrayDpbFrame> & dpb = subTask.m_dpb;
        Pair<ArrayU8x33> & list0  = subTask.m_list0;
        Pair<ArrayU8x33> & list1  = subTask.m_list1;

        bool frameTypeChanged = false;
        mfxU32 pictures = (subTask.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;

        for (mfxU32 f = 0; f < pictures; f++)
        {
            if ((frameType[f] & MFX_FRAMETYPE_I) != 0 && numRefsL0 > 0 && dpb[f].m_numElem < 16 && list0[f].m_numElem < 32)
            {
                frameType[f] &= ~MFX_FRAMETYPE_IPB;
                frameType[f] |= MFX_FRAMETYPE_P;
                frameTypeChanged = true;
            }

            if ((frameType[f] & MFX_FRAMETYPE_B) == 0 && numRefsL1 > 0 && dpb[f].m_numElem < 16 && list1[f].m_numElem < 32)
            {
                frameType[f] &= ~MFX_FRAMETYPE_IPB;
                frameType[f] |= MFX_FRAMETYPE_B;
                frameTypeChanged = true;
            }
        }

        if (frameTypeChanged)
            m_viewMan[viewIdx].BuildDpbAndRefLists(subTask);
    }*/

    m_viewMan[viewIdx].ConfirmTask(*(*m_currentTask)[viewIdx]);

#ifdef MVC_ADD_REF
    mfxExtCodingOption * extOpt = GetExtBuffer(m_video);
    // for all IDRs of dependent view inter-view prediction should be used to avoid I-slices in dependent view
    // reserve surface in 2nd encoder's reconstructed chain to use it for all inter-view references during encoder
    if (viewIdx == 1 && extOpt->ViewOutput == MFX_CODINGOPTION_ON && task->m_type[0] & MFX_FRAMETYPE_IDR && task->m_frameOrder == 0)
    {
        // use last chain entry
        mfxU32 interViewReconIdx = (mfxU32)m_viewMan[viewIdx].m_recons.size() - 1;
        // mark recon as used and as reference
        m_viewMan[viewIdx].m_recons[interViewReconIdx].SetFree(false);
        m_viewMan[viewIdx].m_recons[interViewReconIdx].m_reference[TFIELD] = 1;
        m_viewMan[viewIdx].m_recons[interViewReconIdx].m_reference[BFIELD] = 1;

    }
#endif // MVC_ADD_REF

    return MFX_ERR_NONE;
}

mfxStatus ImplementationMvc::Query(
    VideoCORE *     core,
    mfxVideoParam * in,
    mfxVideoParam * out)
{
    return ImplementationAvc::Query(core, in, out);
}

mfxStatus ImplementationMvc::QueryIOSurf(
    VideoCORE *            core,
    mfxVideoParam *        par,
    mfxFrameAllocRequest * request)
{
    core;

    mfxU32 inPattern = par->IOPattern & MFX_IOPATTERN_IN_MASK;
    MFX_CHECK(
        inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY,
        MFX_ERR_INVALID_VIDEO_PARAM);

    ENCODE_CAPS hwCaps = { 0 };
    mfxStatus sts = QueryHwCaps(core, hwCaps, MSDK_Private_Guid_Encode_AVC_Query);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    MfxVideoParam tmp(*par);

    sts = CheckWidthAndHeight(tmp);
    MFX_CHECK_STS(sts);

    mfxStatus checkSts = CheckVideoParamQueryLike(tmp, hwCaps);
    if (checkSts == MFX_WRN_PARTIAL_ACCELERATION)
        return Warning(MFX_WRN_PARTIAL_ACCELERATION); // return immediately
    else if (checkSts == MFX_ERR_INVALID_VIDEO_PARAM)
        return Error(MFX_ERR_INVALID_VIDEO_PARAM); // return immediately

    SetDefaults(tmp, hwCaps, true);

    if (tmp.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        // in case of system memory
        // encode need enough input frames for reordering
        // then reordered surface is copied into video memory
        request->Type =
            MFX_MEMTYPE_EXTERNAL_FRAME |
            MFX_MEMTYPE_FROM_ENCODE |
            MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else // MFX_IOPATTERN_IN_VIDEO_MEMORY || MFX_IOPATTERN_IN_OPAQUE_MEMORY
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        request->Type |= (inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            ? MFX_MEMTYPE_OPAQUE_FRAME
            : MFX_MEMTYPE_EXTERNAL_FRAME;
    }

    // get FrameInfo from original VideoParam
    request->Info = par->mfx.FrameInfo;

    request->NumFrameMin = CalcNumFrameMin(tmp);
    request->NumFrameSuggested = request->NumFrameMin;

    return MFX_ERR_NONE;
}

ImplementationMvc::ImplementationMvc(VideoCORE * core)
    : m_core(core)
{
}

mfxStatus ImplementationMvc::Init(mfxVideoParam *par)
{
    mfxStatus sts(MFX_ERR_NONE);

    sts = CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    sts = CheckWidthAndHeight(*par);
    MFX_CHECK_STS(sts);

// MVC BD {
    m_ddi[0].reset(CreatePlatformH264Encoder(m_core));
    if (m_ddi[0].get() == 0)
        return MFX_ERR_DEVICE_FAILED;
    sts = m_ddi[0]->CreateAuxilliaryDevice(
// MVC BD }
        m_core,
        DXVA2_Intel_Encode_AVC,
        par->mfx.FrameInfo.Width,
        par->mfx.FrameInfo.Height);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

// MVC BD {
    sts = m_ddi[0]->QueryEncodeCaps(m_ddiCaps);
// MVC BD }
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    m_currentPlatform = m_core->GetHWType();

    m_video = *par;
    mfxStatus checkStatus = CheckVideoParamMvc(m_video, m_ddiCaps, m_core->IsExternalFrameAllocator(), m_currentPlatform);
    if (checkStatus == MFX_WRN_PARTIAL_ACCELERATION)
        return Warning(MFX_WRN_PARTIAL_ACCELERATION);
    else if (checkStatus < MFX_ERR_NONE)
        return Error(checkStatus);

// MVC BD {
    mfxExtCodingOption *       extOpt  = GetExtBuffer(m_video);
// MVC BD }
    mfxExtMVCSeqDesc *         extMvc  = GetExtBuffer(m_video);
    mfxExtOpaqueSurfaceAlloc * extOpaq = GetExtBuffer(m_video);

// MVC BD {
    m_numEncs = 1;
    if (extOpt->ViewOutput == MFX_CODINGOPTION_ON) {
        m_ddi[1].reset(CreatePlatformH264Encoder(m_core));
        if (m_ddi[1].get() == 0)
            return MFX_ERR_DEVICE_FAILED;
        sts = m_ddi[1]->CreateAuxilliaryDevice(
            m_core,
            MSDK_Private_Guid_Encode_MVC_Dependent_View,
            par->mfx.FrameInfo.Width,
            par->mfx.FrameInfo.Height);
        if (sts != MFX_ERR_NONE)
            return MFX_WRN_PARTIAL_ACCELERATION;
        m_numEncs = 2;
    }
// MVC BD }

    m_hrd.resize(extMvc->NumView);

// MVC BD {
    const size_t MAX_FILLER_SIZE = m_video.mfx.FrameInfo.Width * m_video.mfx.FrameInfo.Height;
    mfxU32 perViewBytes = (mfxU32)(m_video.mfx.FrameInfo.Width * m_video.mfx.FrameInfo.Height * 3 / 2 + MAX_FILLER_SIZE);
    if (extOpt->ViewOutput == MFX_CODINGOPTION_ON)
        m_sysMemBits.resize(perViewBytes); // only one view intermediate output buffer is required for ViewOutput mode
    else
        m_sysMemBits.resize(perViewBytes * extMvc->NumView);
// MVC BD }
    m_bitsDesc.resize(extMvc->NumView);
// MVC BD {
    const size_t MAX_SEI_SIZE    = 10 * 1024;
    m_sei.Alloc(mfxU32(MAX_SEI_SIZE + MAX_FILLER_SIZE));
// MVC BD }

    for (mfxU32 i = 0; i < extMvc->NumView; i++)
        m_hrd[i].Setup(m_video);

// MVC BD {
    for (mfxU32 i = 0; i < m_numEncs; i++) {
        sts = m_ddi[i]->CreateAccelerationService(m_video);
        if (sts != MFX_ERR_NONE)
            return MFX_WRN_PARTIAL_ACCELERATION;
    }
// MVC BD }

    // allocate raw/recon/bitstream surfaces
    mfxFrameAllocRequest request = { 0 };
    request.Info = m_video.mfx.FrameInfo;

    // allocate raw surfaces
    // required only in case of system memory input
    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request.Type        = MFX_MEMTYPE_D3D_INT;
        request.NumFrameMin = mfxU16(CalcNumSurfRaw(m_video) * (m_numEncs > 1 ? 1 : extMvc->NumView));
// MVC BD {
        for (mfxU32 i = 0; i < m_numEncs; i ++)
        {
            sts = m_raw[i].Alloc(m_core, request);
            MFX_CHECK_STS(sts);
        }
// MVC BD }
    }
    else if (m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        request.Type        = extOpaq->In.Type;
        request.NumFrameMin = extOpaq->In.NumSurface;

        sts = m_opaqHren.Alloc(m_core, request, extOpaq->In.Surfaces, extOpaq->In.NumSurface);
        MFX_CHECK_STS(sts);

        if (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            request.Type        = MFX_MEMTYPE_D3D_INT;
            request.NumFrameMin = mfxU16(m_numEncs ? extOpaq->In.NumSurface / extMvc->NumView : extOpaq->In.NumSurface); // FIXME: NumFrameMin should be devided by NumView if m_numEncs > 1
// MVC BD {
            for (mfxU32 i = 0; i < m_numEncs; i ++)
            {
                sts = m_raw[i].Alloc(m_core, request);
                MFX_CHECK_STS(sts);
            }
// MVC BD }
        }
    }

    m_inputFrameType =
        m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        (m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            ? MFX_IOPATTERN_IN_SYSTEM_MEMORY
            : MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // reconstruct surfaces are in separate chain
    request.Type        = MFX_MEMTYPE_D3D_INT;
// MVC BD {
    request.NumFrameMin = mfxU16(CalcNumSurfRecon(m_video) * (m_numEncs > 1 ? 1 : extMvc->NumView));
    for (mfxU32 i = 0; i < m_numEncs; i ++)
    {
#ifdef MVC_ADD_REF
        if (i && extOpt->ViewOutput == MFX_CODINGOPTION_ON)
        {
            request.NumFrameMin ++; // allocate additional reconstruct surface  for dep view to place recon from base view
            request.NumFrameSuggested = request.NumFrameMin;
        }
#endif // MVC_ADD_REF
        sts = m_recon[i].Alloc(m_core, request);
        MFX_CHECK_STS(sts);
    }
// MVC BD }

    // Allocate surfaces for bitstreams
    // Need at least one such surface and more for async-mode
    request.Type        = MFX_MEMTYPE_D3D_INT;
// MVC BD {
    request.NumFrameMin = mfxU16(CalcNumSurfBitstream(m_video) * (m_numEncs > 1 ? 1 : extMvc->NumView));
    for (mfxU32 i = 0; i < m_numEncs; i ++)
    {
        sts = m_ddi[i]->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, request);
        MFX_CHECK_STS(sts);
        // driver may suggest too small buffer for bitstream
        request.Info.Width  = IPP_MAX(request.Info.Width,  m_video.mfx.FrameInfo.Width);
        mfxU8 hMult = m_video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ? 5 : 3;
        request.Info.Height = IPP_MAX(request.Info.Height, m_video.mfx.FrameInfo.Height * hMult / 2);

        sts = m_bitstream[i].Alloc(m_core, request);
        MFX_CHECK_STS(sts);
    }
    // register common reconstructed chain
    for (mfxU32 i = 0; i < m_numEncs; i++)
    {
        sts = m_ddi[i]->Register(m_recon[i], D3DDDIFMT_NV12);
        MFX_CHECK_STS(sts);
    }
// MVC BD }

    // register bitstream chain
// MVC BD {
    for (mfxU32 i = 0; i < m_numEncs; i++)
    {
        sts = m_ddi[i]->Register(m_bitstream[i], D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
        MFX_CHECK_STS(sts);
    }

    m_spsSubsetSpsDiff = 0;
// MVC BD }

    m_taskMan.Init(m_core, m_video);

    // FIXME: w/a for SNB issue with HRD at high bitrates
    m_useWAForHighBitrates = (MFX_HW_HSW > m_currentPlatform || MFX_HW_VLV == m_currentPlatform); // HRD WA for high bitrates isn't required for HSW and beyond
    if (m_useWAForHighBitrates)
    {
        m_submittedPicStructs[0].reserve(m_video.AsyncDepth * 2);
        m_submittedPicStructs[1].reserve(m_video.AsyncDepth * 2);
    }

    m_videoInit = m_video;

    return checkStatus;
}

mfxStatus ImplementationMvc::Close()
{
    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy ImplementationMvc::GetThreadingPolicy()
{
    return MFX_TASK_THREADING_DEDICATED;//MFX_TASK_THREADING_DEDICATED_WAIT;
}

mfxStatus ImplementationMvc::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    mfxExtPAVPOption * optPavp = GetExtBuffer(*par);
    MFX_CHECK(optPavp == 0, MFX_ERR_INVALID_VIDEO_PARAM); // mfxExtPAVPOption should not come to Reset

    MfxVideoParam newPar(*par);

    mfxExtOpaqueSurfaceAlloc * optOpaqNew = GetExtBuffer(newPar);
    mfxExtOpaqueSurfaceAlloc * optOpaqOld = GetExtBuffer(m_video);
    MFX_CHECK(
        optOpaqOld->In.Type       == optOpaqNew->In.Type       &&
        optOpaqOld->In.NumSurface == optOpaqNew->In.NumSurface,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    InheritDefaultValues(m_video, newPar);

    mfxStatus checkStatus = CheckVideoParamMvc(newPar, m_ddiCaps, m_core->IsExternalFrameAllocator(), m_currentPlatform);
    if (checkStatus == MFX_WRN_PARTIAL_ACCELERATION)
        return Error(MFX_ERR_INVALID_VIDEO_PARAM);
    else if (checkStatus < MFX_ERR_NONE)
        return Error(checkStatus);

    MFX_CHECK(
        IsMvcProfile(newPar.mfx.CodecProfile)                                   &&
        m_video.AsyncDepth                 == newPar.AsyncDepth                 &&
        m_video.IOPattern                  == newPar.IOPattern                  &&
        m_videoInit.mfx.GopRefDist         >= newPar.mfx.GopRefDist             &&
        m_video.mfx.NumSlice               == newPar.mfx.NumSlice               &&
        m_video.mfx.NumRefFrame            >= newPar.mfx.NumRefFrame            &&
        m_video.mfx.RateControlMethod      == newPar.mfx.RateControlMethod      &&
        m_video.mfx.InitialDelayInKB       == newPar.mfx.InitialDelayInKB       &&
        m_video.mfx.BufferSizeInKB         == newPar.mfx.BufferSizeInKB         &&
        m_videoInit.mfx.FrameInfo.Width    >= newPar.mfx.FrameInfo.Width        &&
        m_videoInit.mfx.FrameInfo.Height   >= newPar.mfx.FrameInfo.Height       &&
        m_video.mfx.FrameInfo.ChromaFormat == newPar.mfx.FrameInfo.ChromaFormat,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    mfxExtMVCSeqDesc const * extMvcOld = GetExtBuffer(m_video);
    mfxExtMVCSeqDesc const * extMvcNew = GetExtBuffer(newPar);
    if (extMvcOld->NumView < extMvcNew->NumView)
        return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    if (m_video.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        MFX_CHECK(
            m_video.mfx.Accuracy    == newPar.mfx.Accuracy    &&
            m_video.mfx.Convergence == newPar.mfx.Convergence,
            MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    }

    bool brcReset =
        m_video.mfx.TargetKbps != newPar.mfx.TargetKbps ||
        m_video.mfx.MaxKbps    != newPar.mfx.MaxKbps;

    mfxExtCodingOption const * extOptNew = GetExtBuffer(newPar);
    if (brcReset && IsOn(extOptNew->NalHrdConformance) &&
        m_video.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    m_video = newPar;
    m_taskMan.Reset(m_video);
// MVC BD {
    for (mfxU32 i = 0; i < m_numEncs; i++)
        m_ddi[i]->Reset(m_video);

    m_spsSubsetSpsDiff = 0;
// MVC BD }

    return checkStatus;
}

mfxStatus ImplementationMvc::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    par->mfx = m_video.mfx;
    par->Protected = m_video.Protected;
    par->IOPattern = m_video.IOPattern;
    par->AsyncDepth = m_video.AsyncDepth;

    for (mfxU32 i = 0; i < par->NumExtParam; i++)
    {
        if (mfxExtBuffer* buf = GetExtBuffer(m_video.ExtParam, m_video.NumExtParam, par->ExtParam[i]->BufferId))
        {
            if (par->ExtParam[i]->BufferId == MFX_EXTBUFF_MVC_SEQ_DESC)
            {
                // mfxExtMVCSeqDesc has complex structure, deep-copy is required
                mfxExtMVCSeqDesc & src = *(mfxExtMVCSeqDesc *)buf;
                mfxExtMVCSeqDesc & dst = *(mfxExtMVCSeqDesc *)par->ExtParam[i];

                mfxStatus sts = CheckBeforeCopy(dst, src);
                MFX_CHECK_STS(sts);

                Copy(dst, src);
            }
            else
            {
                // shallow-copy
                MFX_INTERNAL_CPY(par->ExtParam[i], buf, par->ExtParam[i]->BufferSz);
            }
        }
        else
        {
            return Error(MFX_ERR_UNSUPPORTED);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus ImplementationMvc::GetFrameParam(mfxFrameParam *par)
{
    par;
    return MFX_ERR_NONE;
}

mfxStatus ImplementationMvc::GetEncodeStat(mfxEncodeStat *stat)
{
    MFX_CHECK_NULL_PTR1(stat);
    return MFX_ERR_NONE;
}

mfxStatus ImplementationMvc::EncodeFrameCheck(
    mfxEncodeCtrl *           ctrl,
    mfxFrameSurface1 *        surface,
    mfxBitstream *            bs,
    mfxFrameSurface1 **       reordered_surface,
    mfxEncodeInternalParams * internalParams,
    MFX_ENTRY_POINT *         entryPoints,
    mfxU32 &                  numEntryPoints)
{
    internalParams;
    mfxFrameSurface1 * inSurf = surface;

    mfxStatus checkSts = CheckEncodeFrameParam(
        m_video,
        ctrl,
        surface,
        bs,
        m_core->IsExternalFrameAllocator());
    MFX_CHECK(checkSts >= MFX_ERR_NONE, checkSts);

    if (surface && m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        //mfxFrameSurface1 * opaqSurf = surface;
        surface = m_core->GetNativeSurface(inSurf);
        if (surface == 0)
            return Error(MFX_ERR_UNDEFINED_BEHAVIOR);

        surface->Info            = inSurf->Info;
        surface->Data.TimeStamp  = inSurf->Data.TimeStamp;
        surface->Data.FrameOrder = inSurf->Data.FrameOrder;
        surface->Data.Corrupted  = inSurf->Data.Corrupted;
        surface->Data.DataFlag   = inSurf->Data.DataFlag;
    }

    MvcTask * task = 0;
    mfxStatus assignSts = m_taskMan.AssignTask(m_video, ctrl, surface, bs, task);

// MVC BD {
    mfxExtCodingOption * opt = GetExtBuffer(m_video);

    if (assignSts == MFX_ERR_NONE || assignSts == MFX_ERR_MORE_BITSTREAM)
// MVC BD }
    {
        *reordered_surface = inSurf;

        entryPoints[0].pState               = this;
// MVC BD {
        if (opt->ViewOutput == MFX_CODINGOPTION_ON)
        {
            mfxU32 viewIdx = assignSts == MFX_ERR_MORE_BITSTREAM ? 0 : 1;
            GetInitialHrdState(*task, viewIdx);
            // in case of ViewOutput mode DdiTask is passed to async part
            entryPoints[0].pParam           = (*task)[viewIdx];
            entryPoints[0].pRoutine         = TaskRoutineSubmitOneView;
        }
        else
        {
            entryPoints[0].pParam           = task;
            entryPoints[0].pRoutine         = TaskRoutineSubmit;
        }

// MVC BD }
        entryPoints[0].pCompleteProc        = 0;
        entryPoints[0].pGetSubTaskProc      = 0;
        entryPoints[0].pCompleteSubTaskProc = 0;
        entryPoints[0].requiredNumThreads   = 1;
        entryPoints[1]                      = entryPoints[0];
// MVC BD {
        if (opt->ViewOutput == MFX_CODINGOPTION_ON)
            entryPoints[1].pRoutine             = TaskRoutineQueryOneView;
        else
            entryPoints[1].pRoutine             = TaskRoutineQuery;
// MVC BD }
        entryPoints[0].pRoutineName         = "Encode Submit";
        entryPoints[1].pRoutineName         = "Encode Query";
        numEntryPoints                      = 2;

        // TaskRoutineQuery always updates bitstream from base view task
// MVC BD {
        if (opt->ViewOutput == MFX_CODINGOPTION_OFF)
// MVC BD }
        (*task)[0]->m_bs = bs;


// MVC BD {
        if (assignSts == MFX_ERR_MORE_BITSTREAM)
            return assignSts;
        else
            return checkSts;
// MVC BD }

    }
    else if (assignSts == MFX_ERR_MORE_DATA)
    {
        *reordered_surface = inSurf;

        entryPoints[0].pState               = 0;
        entryPoints[0].pParam               = 0;
        entryPoints[0].pRoutine             = TaskRoutineDoNothing;
        entryPoints[0].pCompleteProc        = 0;
        entryPoints[0].pGetSubTaskProc      = 0;
        entryPoints[0].pCompleteSubTaskProc = 0;
        entryPoints[0].requiredNumThreads   = 1;
        entryPoints[0].pRoutineName         = "Encode Submit";
        numEntryPoints                      = 1;

        return mfxStatus(MFX_ERR_MORE_DATA_RUN_TASK);
    }
    else
    {
        return assignSts;
    }
}

namespace
{
    struct FindByPocAndViewIdx
    {
        FindByPocAndViewIdx(DpbFrame const & target)
            : m_target(target)
        {
        }

        bool operator ()(DpbFrame const & other)
        {
            return m_target.m_poc == other.m_poc && m_target.m_viewIdx == other.m_viewIdx;
        }

        DpbFrame m_target;
    };
}

// MVC BD {
mfxU32 PaddingBytesToWorkAroundHrdIssue(
    MfxVideoParam const &       video,
    Hrd                         hrd,
    std::vector<mfxU16> const & submittedPicStruct,
    mfxU16                      currentPicStruct,
    mfxI32                      bufferSizeMod,
    mfxU32                      minCurPicSize = 0)
{
    mfxExtCodingOption const * extOpt = GetExtBuffer(video);

    if (video.mfx.RateControlMethod != MFX_RATECONTROL_CBR || IsOff(extOpt->NalHrdConformance))
        return 0;

    mfxF64 frameRate = mfxF64(video.mfx.FrameInfo.FrameRateExtN) / video.mfx.FrameInfo.FrameRateExtD;
    mfxU32 avgFrameSize = mfxU32(1000 * video.calcParam.targetKbps / frameRate);
    if (avgFrameSize <= 128 * 1024 * 8 && bufferSizeMod == 0 )
        return 0;

    for (size_t i = 0; i < submittedPicStruct.size(); i++)
    {
        hrd.RemoveAccessUnit(
            0,
            !(submittedPicStruct[i] & MFX_PICSTRUCT_PROGRESSIVE),
            false);
    }

    hrd.RemoveAccessUnit(
        minCurPicSize,
        !(currentPicStruct & MFX_PICSTRUCT_PROGRESSIVE),
        false);

    mfxU32 bufsize  = 8000 * video.calcParam.mvcPerViewPar.bufferSizeInKB;
    bufsize += bufferSizeMod;
    mfxU32 bitrate  = GetMaxBitrateValue(video.calcParam.mvcPerViewPar.maxKbps) << 6;
    mfxU32 delay    = hrd.GetInitCpbRemovalDelay();
    mfxU32 fullness = mfxU32(mfxU64(delay) * bitrate / 90000.0);

    const mfxU32 maxFrameSize = video.mfx.FrameInfo.Width * video.mfx.FrameInfo.Height;
    if (fullness > bufsize)
        return IPP_MIN((fullness - bufsize + 7) / 8, maxFrameSize);

    return 0;
}
// MVC BD }


mfxStatus ImplementationMvc::TaskRoutineDoNothing(
    void * /*state*/,
    void * /*param*/,
    mfxU32 /*threadNumber*/,
    mfxU32 /*callNumber*/)
{
    return MFX_TASK_DONE;
}

mfxStatus ImplementationMvc::TaskRoutineSubmit(
    void * state,
    void * param,
    mfxU32 /*threadNumber*/,
    mfxU32 /*callNumber*/)
{
    ImplementationMvc & impl = *(ImplementationMvc *)state;
    MvcTask &           task = *(MvcTask *)param;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Encode Submit");

    mfxExtMVCSeqDesc * opt = GetExtBuffer(impl.m_video);

    mfxU32 firstFieldId = task[0]->GetFirstField();

    mfxStatus sts = MFX_ERR_NONE;
    mfxHDLPair surfaceHDL = {0};

    for (mfxU32 i = 0; i < opt->NumView; i++)
    {
        sts = impl.CopyRawSurface(*(task[i]));
        MFX_CHECK_STS(sts);

        sts = impl.GetRawSurfaceHandle(*(task[i]), surfaceHDL);
        if (sts != MFX_ERR_NONE)
            return Error(MFX_ERR_UNDEFINED_BEHAVIOR);

        // IMPORTANT: non-trivial work-around is required here since snb/ivb driver know nothing about multi-view
        // dpb should contain all reference frames which will be used in future
        // if any ref frame absent driver will immediately retire internal resources connected to the ref frame
        // to prevent this from happening we have to combine dpbs of all views together
        // dpb of views with curViewIdx > 0 should also contain reconstruction of all views with viewIdx < curViewIdx
        // IMPORTANT2: reference frames ordered so that first frames from list0 list1 goes in the beginning of dpb
        // it is important since driver seems to be able to maintain only 4 reference frames
        // PatchTask() function does all required patches to current task
        DdiTask curTask = *task[i];
        impl.PatchTask(task, curTask, firstFieldId);

        if (i == 0)
            PrepareSeiMessageBuffer(impl.m_video, curTask, firstFieldId, impl.m_sei);
        else
            PrepareSeiMessageBufferDepView(impl.m_video, curTask, firstFieldId, impl.m_sei);

        if (MFX_HW_D3D11 == impl.m_core->GetVAType())
            sts = impl.m_ddi[0]->Execute(
                surfaceHDL.first,
                curTask,
                firstFieldId,
                impl.m_sei);
        else
            sts = impl.m_ddi[0]->Execute(
                surfaceHDL.first,
                curTask,
                firstFieldId,
                impl.m_sei);

        MFX_CHECK_STS(sts);

        if ((task[0]->GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0)
        {
            // same patching for second field
            DdiTask curTask = *task[i];
            impl.PatchTask(task, curTask, !firstFieldId);

            sts = impl.GetRawSurfaceHandle(*(task[i]), surfaceHDL);
            if (sts != MFX_ERR_NONE)
                return Error(MFX_ERR_UNDEFINED_BEHAVIOR);

            if (i == 0)
                PrepareSeiMessageBuffer(impl.m_video, curTask, !firstFieldId, impl.m_sei);
            else
                PrepareSeiMessageBufferDepView(impl.m_video, curTask, !firstFieldId, impl.m_sei);

            if (MFX_HW_D3D11 == impl.m_core->GetVAType()) // use of (mfxHDL)pSurfaceHdl is a hack. Should be fixed!
                sts = impl.m_ddi[0]->Execute(
                    surfaceHDL.first,
                    curTask,
                    !firstFieldId,
                    impl.m_sei);
            else
                sts = impl.m_ddi[0]->Execute(
                    surfaceHDL.first,
                    curTask,
                    !firstFieldId,
                    impl.m_sei);
            MFX_CHECK_STS(sts);
        }
    }

    return MFX_TASK_DONE;
}

// MVC BD {
// submit one view to driver
// used within ViewOutput (Blu-ray compatible) encoding mode
mfxStatus ImplementationMvc::TaskRoutineSubmitOneView(
    void * state,
    void * param,
    mfxU32 /*threadNumber*/,
    mfxU32 /*callNumber*/)
{
    ImplementationMvc & impl = *(ImplementationMvc *)state;
    DdiTask &       realTask = *(DdiTask *)param;
    MvcTask & mvcTask = *(impl.m_taskMan.GetMvcTask(realTask));
    mfxExtCodingOption * extOpt = GetExtBuffer(impl.m_video);
    //mfxHDL nativeSurface;
    mfxU32 encIdx = impl.m_numEncs > 1 ? realTask.m_viewIdx : 0;
    mfxStatus sts = MFX_ERR_NONE;

    mfxHDLPair surfaceHDL = {0};

    mfxU32 firstFieldId = realTask.GetFirstField();

    DdiTask task = realTask;

    /* Aka simple patch task */
    bool interlace = (task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0;
    for (mfxU32 i = 0; i < GetMaxNumSlices(impl.m_video); ++ i)
    {
        mfxExtCodingOption2 const &     extOpt2        = GetExtBufferRef(impl.m_video);
        mfxU8 disableDeblockingIdc = mfxU8(extOpt2.DisableDeblockingIdc);
        task.m_disableDeblockingIdc[firstFieldId].push_back(disableDeblockingIdc);
        task.m_sliceAlphaC0OffsetDiv2[firstFieldId].push_back( 0);
        task.m_sliceBetaOffsetDiv2[firstFieldId].push_back(0);
    }
    if (interlace)
    {
        for (mfxU32 i = 0; i < GetMaxNumSlices(impl.m_video); ++ i)
        {
            mfxExtCodingOption2 const &     extOpt2        = GetExtBufferRef(impl.m_video);
            mfxU8 disableDeblockingIdc = mfxU8(extOpt2.DisableDeblockingIdc);
            task.m_disableDeblockingIdc[!firstFieldId].push_back(disableDeblockingIdc);
            task.m_sliceAlphaC0OffsetDiv2[!firstFieldId].push_back( 0);
            task.m_sliceBetaOffsetDiv2[!firstFieldId].push_back(0);
        }
    }

    if (impl.m_numEncs > 1)
    {
#ifdef MVC_ADD_REF
        // extOpt->ViewOutput == MFX_CODINGOPTION_ON means Blu-ray/AVCHD compatible MVC encoding
        // in this mode I-slices are prohibited in dependent view
        // SNB/IVB driver can't correctly encode P-frames w/o references (artifacts appear)
        // so all IDRs of dependent view should be encoded with inter-view references
        if (encIdx == 1 && realTask.m_viewIdx == 1 && extOpt->ViewOutput == MFX_CODINGOPTION_ON)
        {
            bool interlace = (realTask.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0;
            // at the beginning of encoding with 2nd encoder driver has no registered references so
            // 2nd encoder perform 'dummy' encoding of 1st IDR of base view to get reconstruct and register it as reference inside driver
            if (realTask.m_type[firstFieldId] & MFX_FRAMETYPE_IDR && realTask.m_frameOrder == 0)
            {
                // 'dummy' task is used for encoding of 1st frame of base view
                DdiTask dummyTask = *mvcTask[0];
                dummyTask.m_dpb[firstFieldId].Resize(0);
                dummyTask.m_viewIdx = 1;
                dummyTask.m_idxRecon = CalcNumSurfRecon(impl.m_video); // inter-view recon is always last in recounstruct chain of 2nd encoder
                /* Aka simple patch task */
                for (mfxU32 i = 0; i < GetMaxNumSlices(impl.m_video); ++ i)
                {
                    mfxExtCodingOption2 const &     extOpt2        = GetExtBufferRef(impl.m_video);
                    mfxU8 disableDeblockingIdc = mfxU8(extOpt2.DisableDeblockingIdc);
                    dummyTask.m_disableDeblockingIdc[firstFieldId].push_back(disableDeblockingIdc);
                    dummyTask.m_sliceAlphaC0OffsetDiv2[firstFieldId].push_back( 0);
                    dummyTask.m_sliceBetaOffsetDiv2[firstFieldId].push_back(0);
                }
                if (interlace)
                {
                    for (mfxU32 i = 0; i < GetMaxNumSlices(impl.m_video); ++ i)
                    {
                        mfxExtCodingOption2 const &     extOpt2        = GetExtBufferRef(impl.m_video);
                        mfxU8 disableDeblockingIdc = mfxU8(extOpt2.DisableDeblockingIdc);
                        dummyTask.m_disableDeblockingIdc[!firstFieldId].push_back(disableDeblockingIdc);
                        dummyTask.m_sliceAlphaC0OffsetDiv2[!firstFieldId].push_back( 0);
                        dummyTask.m_sliceBetaOffsetDiv2[!firstFieldId].push_back(0);
                    }
                }

                // speicfy correct DPB and ref list for 2nd field of 'dummy' frame
                if (interlace)
                {
                    dummyTask.m_dpb[!firstFieldId].Resize(0);
                    DpbFrame newDpbFrame;
                    newDpbFrame.m_frameIdx = (mfxU8)dummyTask.m_idxRecon;
                    newDpbFrame.m_poc      = dummyTask.GetPoc();
                    newDpbFrame.m_viewIdx  = 0;
                    newDpbFrame.m_longterm = 0;
                    dummyTask.m_dpb[!firstFieldId].PushBack(newDpbFrame);
                    dummyTask.m_list0[!firstFieldId].Resize(0);
                    dummyTask.m_list0[!firstFieldId].PushBack(0);
                }

                sts = impl.CopyRawSurface(dummyTask);
                    MFX_CHECK_STS(sts);

                sts = impl.GetRawSurfaceHandle(dummyTask, surfaceHDL);
                if (sts != MFX_ERR_NONE)
                    return Error(MFX_ERR_UNDEFINED_BEHAVIOR);

                // submit 'dummy' task to driver
                if (MFX_HW_D3D11 == impl.m_core->GetVAType())
                    sts = impl.m_ddi[encIdx]->Execute(
                        surfaceHDL.first,
                        dummyTask,
                        firstFieldId,
                        impl.m_sei);
                else
                    sts = impl.m_ddi[encIdx]->Execute(
                        surfaceHDL.first,
                        dummyTask,
                        firstFieldId,
                        impl.m_sei);

                MFX_CHECK_STS(sts);


                // submit 2nd field of 'dummy' task to driver
                if (interlace)
                {
                    if (MFX_HW_D3D11 == impl.m_core->GetVAType())
                        sts = impl.m_ddi[encIdx]->Execute(
                            surfaceHDL.first,
                            dummyTask,
                            !firstFieldId,
                            impl.m_sei);
                    else
                        sts = impl.m_ddi[encIdx]->Execute(
                            surfaceHDL.first,
                            dummyTask,
                            !firstFieldId,
                            impl.m_sei);

                    MFX_CHECK_STS(sts);
                }

                do
                {
                    sts = impl.m_ddi[encIdx]->QueryStatus(dummyTask, interlace ? !firstFieldId : firstFieldId);
                    if (sts == MFX_WRN_DEVICE_BUSY)
                        vm_time_sleep(5);
                } while (sts == MFX_WRN_DEVICE_BUSY);

                if (sts != MFX_ERR_NONE)
                    return Error(MFX_ERR_DEVICE_FAILED);

                if (interlace)
                {
                    sts = impl.m_ddi[encIdx]->QueryStatus(dummyTask, firstFieldId);
                    if (sts != MFX_ERR_NONE)
                        return Error(MFX_ERR_DEVICE_FAILED);
                }

                mfxI32 m_initCpbRemovalDiff = impl.m_hrd[task.m_viewIdx].GetInitCpbRemovalDelay();
                // modify HRD state after 'dummy' encoding
                impl.m_hrd[dummyTask.m_viewIdx].RemoveAccessUnit(
                    dummyTask.m_bsDataLength[firstFieldId],
                    interlace,
                    (dummyTask.m_type[firstFieldId] & MFX_FRAMETYPE_IDR) != 0);

                if (interlace)
                {
                    impl.m_hrd[dummyTask.m_viewIdx].RemoveAccessUnit(
                        dummyTask.m_bsDataLength[!firstFieldId],
                        interlace,
                        (dummyTask.m_type[!firstFieldId] & MFX_FRAMETYPE_IDR) != 0);
                }

                m_initCpbRemovalDiff -= impl.m_hrd[task.m_viewIdx].GetInitCpbRemovalDelay();

                impl.m_bufferSizeModifier = -mfxI32(m_initCpbRemovalDiff / 90000.0 * impl.m_video.calcParam.mvcPerViewPar.maxKbps * 1000);

                if (task.m_frameOrder == 0)
                {
                    // get actual HRD state
                    task.m_initCpbRemoval = impl.m_hrd[task.m_viewIdx].GetInitCpbRemovalDelay();
                    task.m_initCpbRemovalOffset = impl.m_hrd[task.m_viewIdx].GetInitCpbRemovalDelayOffset();
                }
            }

            // for 2nd and subsequent IDRs base view reconstruct is copied to surface that is already registered in 2nd encoder as reference
            // need to check if base view frame is already reconstructed and ready to be used as inter-view ref
            if (realTask.m_type[firstFieldId] & MFX_FRAMETYPE_IDR && realTask.m_frameOrder)
                if (mvcTask[0] == 0 || (!interlace && mvcTask[0]->m_bsDataLength[0] == 0) ||
                    (interlace && (mvcTask[0]->m_bsDataLength[0] == 0 || mvcTask[0]->m_bsDataLength[1] == 0)))
                {
                    return MFX_TASK_BUSY;
                }
        }
#endif // MVC_ADD_REF

        // disable surface offsets in case of several AVC encoders since every encoder works with it's own set of surfaces dedicated to one view
        task.m_idxReconOffset = realTask.m_idxReconOffset = 0;
        task.m_idxBsOffset = realTask.m_idxBsOffset = 0;
    }

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Encode Submit");

    sts = impl.CopyRawSurface(task);
    MFX_CHECK_STS(sts);

    sts = impl.GetRawSurfaceHandle(task, surfaceHDL);
    if (sts != MFX_ERR_NONE)
        return Error(MFX_ERR_UNDEFINED_BEHAVIOR);

    // w/a for SNB/IVB: calculate size of padding to compensate re-pack  of AVC headers to MVC
    task.m_addRepackSize[firstFieldId] = impl.CalcPaddingToCompensateRepack(task, firstFieldId);

    if (impl.m_useWAForHighBitrates)
    {
        task.m_fillerSize[firstFieldId] = PaddingBytesToWorkAroundHrdIssue(
            impl.m_video,
            impl.m_hrd[task.m_viewIdx],
            impl.m_submittedPicStructs[task.m_viewIdx],
            task.GetPicStructForEncode(),
            task.m_viewIdx ? impl.m_bufferSizeModifier : 0,
            task.m_addRepackSize[firstFieldId]);
    }

    if (task.m_viewIdx == 0)
        PrepareSeiMessageBuffer(impl.m_video, task, firstFieldId, impl.m_sei);
    else
    {
        // BD/AVCHD compatible encoding: values in BP SEI should be equal in base and dependent views
#ifndef AVC_BS
        // AVC_BS is dedicated to check HRD conformance of dep view so real HRD state is encoded for AVC_BS
        task.m_initCpbRemoval = mvcTask[0]->m_initCpbRemoval;
        task.m_initCpbRemovalOffset = mvcTask[0]->m_initCpbRemovalOffset;
#endif // AVC_BS
        PrepareSeiMessageBufferDepView(impl.m_video, task, firstFieldId, impl.m_sei);
    }

    impl.PatchTask(mvcTask, task, firstFieldId);

    realTask.m_addRepackSize[firstFieldId] = task.m_addRepackSize[firstFieldId]; // need to save padding size to remove it from encoded frame

    // here is workaround for SNB/IVB. Need to use 1 encoder per view to guarantee HRD conformance for each view
    if (MFX_HW_D3D11 == impl.m_core->GetVAType())
        sts = impl.m_ddi[encIdx]->Execute(
            surfaceHDL.first,
            task,
            firstFieldId,
            impl.m_sei);
    else
        sts = impl.m_ddi[encIdx]->Execute(
            surfaceHDL.first,
            task,
            firstFieldId,
            impl.m_sei);

    MFX_CHECK_STS(sts);

    // FIXME: w/a for SNB issue with HRD at high bitrates
    if (impl.m_useWAForHighBitrates)
        impl.m_submittedPicStructs[task.m_viewIdx].push_back(task.GetPicStructForEncode());

    if ((task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0)
    {
        // w/a for SNB/IVB: calculate size of padding to compensate re-pack  of AVC headers to MVC
        task.m_addRepackSize[!firstFieldId] = impl.CalcPaddingToCompensateRepack(task, !firstFieldId);

        if (impl.m_useWAForHighBitrates)
        {
            task.m_fillerSize[!firstFieldId] = PaddingBytesToWorkAroundHrdIssue(
                impl.m_video,
                impl.m_hrd[task.m_viewIdx],
                impl.m_submittedPicStructs[task.m_viewIdx],
                task.GetPicStructForEncode(),
                task.m_viewIdx ? impl.m_bufferSizeModifier : 0,
                task.m_addRepackSize[!firstFieldId]);
        }

        if (task.m_viewIdx == 0)
            PrepareSeiMessageBuffer(impl.m_video, task, !firstFieldId, impl.m_sei);
        else
        {
            // BD/AVCHD compatible encoding: values in BP SEI should be equal in base and dependent views
#ifndef AVC_BS
            // AVC_BS is dedicated to check HRD conformance of dep view so real HRD state is encoded for AVC_BS
            task.m_initCpbRemoval = mvcTask[0]->m_initCpbRemoval;
            task.m_initCpbRemovalOffset = mvcTask[0]->m_initCpbRemovalOffset;
#endif // AVC_BS
            PrepareSeiMessageBufferDepView(impl.m_video, task, !firstFieldId, impl.m_sei);
        }

        realTask.m_addRepackSize[!firstFieldId] = task.m_addRepackSize[!firstFieldId]; // need to save padding size to remove it from encoded frame

        // here is workaround for SNB/IVB. Need to use 1 encoder per view to guarantee HRD conformance for each view
        if (MFX_HW_D3D11 == impl.m_core->GetVAType())
            sts = impl.m_ddi[encIdx]->Execute(
                surfaceHDL.first,
                task,
                !firstFieldId,
                impl.m_sei);
        else
            if (MFX_HW_D3D11 == impl.m_core->GetVAType())
            sts = impl.m_ddi[encIdx]->Execute(
                surfaceHDL.first,
                task,
                !firstFieldId,
                impl.m_sei);

        MFX_CHECK_STS(sts);

        // FIXME: w/a for SNB issue with HRD at high bitrates
        if (impl.m_useWAForHighBitrates)
            impl.m_submittedPicStructs[task.m_viewIdx].push_back(task.GetPicStructForEncode());
    }

    return MFX_TASK_DONE;
}
// MVC BD }



mfxStatus ImplementationMvc::TaskRoutineQuery(
    void * state,
    void * param,
    mfxU32 /*threadNumber*/,
    mfxU32 /*callNumber*/)
{
    ImplementationMvc & impl = *(ImplementationMvc *) state;
    MvcTask &           task = *(MvcTask *)param;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Encode Query");

    mfxExtMVCSeqDesc * opt = GetExtBuffer(impl.m_video);

    // all subtasks has same picstruct
    mfxU32 firstFieldId = task[0]->GetFirstField();

    if (task[0]->GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE)
    {
        // wait for last submitted task
// MVC BD {
        mfxStatus sts = impl.m_ddi[0]->QueryStatus(*task[opt->NumView - 1], firstFieldId);
// MVC BD }
        if (sts == MFX_WRN_DEVICE_BUSY)
            return MFX_TASK_BUSY;

        // need to call TaskManager::CompleteTask
        // even if error occurs during status query
        CompleteTaskOnExitMvc completer(impl.m_taskMan, task);

        // Task submitted earlier should be ready by this moment
        // otherwise report device error
        for (mfxU32 i = 0; i < opt->NumView - 1; i++)
        {
// MVC BD {
            sts = impl.m_ddi[0]->QueryStatus(*task[i], firstFieldId);
// MVC BD }
            if (sts != MFX_ERR_NONE)
                return Error(MFX_ERR_DEVICE_FAILED);
        }

        sts = impl.UpdateBitstream(task, firstFieldId);
        MFX_CHECK_STS(sts);
    }
    else
    {
        // wait for last submitted task
// MVC BD {
        mfxStatus sts = impl.m_ddi[0]->QueryStatus(*task[opt->NumView - 1], !firstFieldId);
// MVC BD }
        if (sts == MFX_WRN_DEVICE_BUSY)
            return MFX_TASK_BUSY;

        // need to call TaskManager::CompleteTask
        // even if error occurs during status query
        CompleteTaskOnExitMvc completer(impl.m_taskMan, task);

        // Task submitted earlier should be ready by this moment
        // otherwise report device error
// MVC BD {
        sts = impl.m_ddi[0]->QueryStatus(*task[opt->NumView - 1], firstFieldId);
// MVC BD }
        if (sts != MFX_ERR_NONE)
            return Error(MFX_ERR_DEVICE_FAILED);

        for (mfxU32 i = 0; i < opt->NumView - 1; i++)
        {
// MVC BD {
            sts = impl.m_ddi[0]->QueryStatus(*task[i], firstFieldId);
// MVC BD }
            if (sts != MFX_ERR_NONE)
                return Error(MFX_ERR_DEVICE_FAILED);

// MVC BD {
            sts = impl.m_ddi[0]->QueryStatus(*task[i], !firstFieldId);
// MVC BD }
            if (sts != MFX_ERR_NONE)
                return Error(MFX_ERR_DEVICE_FAILED);
        }

        sts = impl.UpdateBitstream(task, firstFieldId);
        MFX_CHECK_STS(sts);

        sts = impl.UpdateBitstream(task, !firstFieldId);
        MFX_CHECK_STS(sts);
    }

#if 0 // removed dependency from fwrite(). Custom writing to file shouldn't be present in MSDK releases w/o documentation and testing
    mfxExtDumpFiles * extDump = GetExtBuffer(impl.m_video);
    extDump;

    if (vm_file * file = 0)//OpenFile(extDump->ReconFilename, (task[0]->m_frameOrder == 0) ? _T("wb") : _T("ab")))
    {
        for (mfxU32 i = 0; i < opt->NumView; i++)
        {
            mfxFrameData data = { 0 };
// MVC BD {
            data.MemId = impl.m_recon[0].mids[task[i]->m_idxRecon + task[i]->m_idxReconOffset]; // doesn't work for multiple encoders, just to compile
// MVC BD }
            WriteFrameData(file, impl.m_core, data, impl.m_video.mfx.FrameInfo);
        }
        vm_file_fclose(file);
    }
    if (vm_file * file = 0)//OpenFile(extDump->InputFramesFilename, (task[0]->m_frameOrder == 0) ? _T("wb") : _T("ab")))
    {
        for (mfxU32 i = 0; i < opt->NumView; i++)
        {
            mfxFrameData data = { 0 };
            FrameLocker lock(
                impl.m_core,
                data,
                impl.GetRawSurfaceMemId(*(task[i])),
                impl.m_inputFrameType == MFX_IOPATTERN_IN_VIDEO_MEMORY);
            WriteFrameData(file, impl.m_core, data, impl.m_video.mfx.FrameInfo);
        }
        vm_file_fclose(file);
    }
#endif // removed dependency from fwrite(). Custom writing to file shouldn't be present in MSDK releases w/o documentation and testing

    return MFX_ERR_NONE;
}

// MVC BD {
// get coded representation of one view from driver
// used within ViewOutput (Blu-ray compatible) encoding mode
mfxStatus ImplementationMvc::TaskRoutineQueryOneView(
    void * state,
    void * param,
    mfxU32 /*threadNumber*/,
    mfxU32 /*callNumber*/)
{
    ImplementationMvc & impl = *(ImplementationMvc *) state;
    DdiTask &           task = *(DdiTask *)param;
    MvcTask & mvcTask = *(impl.m_taskMan.GetMvcTask(task));

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Encode Query");

    mfxU32 encIdx = impl.m_numEncs > 1 ? task.m_viewIdx : 0;

    mfxU32 firstFieldId = task.GetFirstField();

    if (task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE)
    {
        // wait for submitted task
        mfxStatus sts = impl.m_ddi[encIdx]->QueryStatus(task, firstFieldId);

        if (sts == MFX_WRN_DEVICE_BUSY)
            return MFX_TASK_BUSY;

        // need to call TaskManager::CompleteTask
        // even if error occurs during status query
        CompleteTaskOnExitMvcOneView completer(impl.m_taskMan, task);

        if (sts != MFX_ERR_NONE)
            return Error(MFX_ERR_DEVICE_FAILED);

        if (task.m_viewIdx == 0)
            sts = impl.UpdateBitstreamBaseView(task, firstFieldId);
        else
            sts = impl.UpdateBitstreamDepView(task, firstFieldId);
        MFX_CHECK_STS(sts);

        if (false == impl.m_taskMan.IsMvcTaskFinished(mvcTask))
            completer.Cancel(); // don't complete task if it's not finished
    }
    else
    {
        // wait for last submitted task
        mfxStatus sts = impl.m_ddi[encIdx]->QueryStatus(task, !firstFieldId);

        if (sts == MFX_WRN_DEVICE_BUSY)
            return MFX_TASK_BUSY;

        // need to call TaskManager::CompleteTask
        // even if error occurs during status query
        CompleteTaskOnExitMvcOneView completer(impl.m_taskMan, task);

        // Task submitted earlier should be ready by this moment
        // otherwise report device error
        sts = impl.m_ddi[encIdx]->QueryStatus(task, firstFieldId);
        if (sts != MFX_ERR_NONE)
            return Error(MFX_ERR_DEVICE_FAILED);

        if (task.m_viewIdx == 0)
        {
            sts = impl.UpdateBitstreamBaseView(task, firstFieldId);
            MFX_CHECK_STS(sts);
            sts = impl.UpdateBitstreamBaseView(task, !firstFieldId);
        }
        else
        {
            sts = impl.UpdateBitstreamDepView(task, firstFieldId);
            MFX_CHECK_STS(sts);
            sts = impl.UpdateBitstreamDepView(task, !firstFieldId);
        }
        MFX_CHECK_STS(sts);

        if (false == impl.m_taskMan.IsMvcTaskFinished(mvcTask))
            completer.Cancel(); // don't complete task if it's not finished
    }

    // FIXME: w/a for SNB issue with HRD at high bitrates
    if (impl.m_useWAForHighBitrates)
    {
        if (task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE)
        {
            assert(impl.m_submittedPicStructs[task.m_viewIdx].size() > 0);
            impl.m_submittedPicStructs[task.m_viewIdx].erase(
                impl.m_submittedPicStructs[task.m_viewIdx].begin());
        }
        else
        {
            assert(impl.m_submittedPicStructs[task.m_viewIdx].size() > 1);
            impl.m_submittedPicStructs[task.m_viewIdx].erase(
                impl.m_submittedPicStructs[task.m_viewIdx].begin(),
                impl.m_submittedPicStructs[task.m_viewIdx].begin() + 2);
        }
    }

    return MFX_ERR_NONE;
}
// MVC BD }

mfxStatus ImplementationMvc::UpdateBitstream(
    MvcTask & task,
    mfxU32    fieldId) // 0 - top/progressive, 1 - bottom
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Copy and repack bitstream");

    mfxExtMVCSeqDesc * extMvc = GetExtBuffer(m_video);

    DdiTask &      task0  = *task[0];
    mfxBitstream & bs     = *task0.m_bs;

    bs.TimeStamp = task0.m_yuv->Data.TimeStamp;
    bs.DecodeTimeStamp = CalcDTSFromPTS(m_video.mfx.FrameInfo, mfxU16(task0.m_dpbOutputDelay), bs.TimeStamp);
    bs.FrameType = task0.m_type[task0.GetFirstField()];
    if ((task0.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0)
        bs.FrameType = mfxU16(bs.FrameType | (task0.m_type[!task0.GetFirstField()] << 8));

    mfxU8 * mvcBitsBegin = bs.Data + bs.DataOffset + bs.DataLength;
    mfxU8 * mvcBitsEnd   = bs.Data + bs.MaxLength;

    mfxU32 bytesPerView = mfxU32(m_sysMemBits.size() / extMvc->NumView);

    // copy per-view bitstreams to system memory
    for (mfxU32 viewIdx = 0; viewIdx < extMvc->NumView; viewIdx++)
    {
        mfxFrameData d3dBits = { 0, };
        DdiTask & subTask = *task[viewIdx];

        FrameLocker lock(
            m_core,
            d3dBits,
// MVC BD {
            m_bitstream[0].mids[subTask.m_idxBs[fieldId] + subTask.m_idxBsOffset]); // doesn't work for multiple encoders, just to compile
// MVC BD }
        MFX_CHECK(d3dBits.Y != 0, MFX_ERR_LOCK_MEMORY);

        m_bitsDesc[viewIdx].begin = &m_sysMemBits[viewIdx * bytesPerView];
        m_bitsDesc[viewIdx].end   = &m_sysMemBits[viewIdx * bytesPerView] + subTask.m_bsDataLength[fieldId];

        assert(subTask.m_bsDataLength[fieldId] <= bytesPerView);

        FastCopyBufferVid2Sys(
            m_bitsDesc[viewIdx].begin,
            d3dBits.Y,
            subTask.m_bsDataLength[fieldId]);

        mfxStatus sts = ParseAvcBitstream(m_bitsDesc[viewIdx]);
        MFX_CHECK_STS(sts);
    }

    // now assemble per-view bitsteams into one mvc bitstream

    if (m_bitsDesc[0].aud.begin)
    {
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[0].aud.begin,
            m_bitsDesc[0].aud.end);
    }

    if (m_bitsDesc[0].sps.begin)
    {
        // base view has sps
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[0].sps.begin,
            m_bitsDesc[0].sps.end);

        for (mfxU32 viewIdx = 1; viewIdx < extMvc->NumView; viewIdx++)
        {
            if (m_bitsDesc[viewIdx].sps.begin)
            {
                // convert sps of one of dependent views to subset sps
                mvcBitsBegin = RePackSequenceHeader(
                    mvcBitsBegin,
                    mvcBitsEnd,
                    m_bitsDesc[viewIdx].sps.begin,
                    m_bitsDesc[viewIdx].sps.end,
                    m_video);

                break;
            }
        }
    }

    if (m_bitsDesc[0].pps.begin)
    {
        // base view has pps
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[0].pps.begin,
            m_bitsDesc[0].pps.end);

        for (mfxU32 viewIdx = 1; viewIdx < extMvc->NumView; viewIdx++)
        {
            if (m_bitsDesc[viewIdx].pps.begin)
            {
                mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
                    mvcBitsBegin,
                    mvcBitsEnd,
                    m_bitsDesc[viewIdx].pps.begin,
                    m_bitsDesc[viewIdx].pps.end);
                break;
            }
        }
    }

    if (m_bitsDesc[0].sei.begin)
    {
        for (mfxU32 viewIdx = 0; viewIdx < extMvc->NumView; viewIdx++)
        {
            // copy base view sei only
            NaluIterator nalu(m_bitsDesc[viewIdx].sei, m_bitsDesc[viewIdx].slice.begin);
            for (; nalu->type == 6; ++nalu)
                mvcBitsBegin = CheckedMFX_INTERNAL_CPY(mvcBitsBegin, mvcBitsEnd, nalu->begin, nalu->end);
        }
    }

    mfxU32 nonIdrFlag    = m_bitsDesc[0].slice.type == 0x01;
    mfxU32 anchorPicFlag = m_bitsDesc[0].slice.type == 0x05;
    mfxU32 nalRefIdc     = (task[0]->m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;

    NaluIterator nalu(m_bitsDesc[0].slice, m_bitsDesc[0].end);
    for (bool firstSlice = true; nalu->type == 1 || nalu->type == 5; ++nalu, firstSlice = false)
    {
        assert(nalu->type == m_bitsDesc[0].slice.type && "idr and non-idr slices in one au");

        if (firstSlice)
        {
            mvcBitsBegin = PackPrefixNalUnit(
                mvcBitsBegin,
                mvcBitsEnd,
                nalu->numZero,
                nalRefIdc,
                14,
                extMvc->View[0].ViewId,
                nonIdrFlag,
                anchorPicFlag,
                1); // base view always has inter_view_flag = 1
        }

        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(mvcBitsBegin, mvcBitsEnd, nalu->begin, nalu->end);
    }

    for (mfxU32 viewIdx = 1; viewIdx < extMvc->NumView; viewIdx++)
    {
        DdiTask const & subTask = *task[viewIdx];
        mfxU32 interViewFlag = GetInterViewFlag(*extMvc, viewIdx, anchorPicFlag);
        mfxU32 nalRefIdc     = (subTask.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;

        NaluIterator nalu(m_bitsDesc[viewIdx].slice, m_bitsDesc[viewIdx].end);
        for (; nalu->type == 1 || nalu->type == 5; ++nalu)
        {
            assert(nalu->type == m_bitsDesc[0].slice.type && "idr and non-idr slices in one au");

            mvcBitsBegin = PackPrefixNalUnit(
                mvcBitsBegin,
                mvcBitsEnd,
                nalu->numZero,
                nalRefIdc,
                20,
                extMvc->View[viewIdx].ViewId,
                nonIdrFlag,
                anchorPicFlag,
                interViewFlag);

            if (subTask.m_refPicList0Mod[fieldId].Size() > 0 ||
                subTask.m_refPicList1Mod[fieldId].Size() > 0)
            {
                mvcBitsBegin = RePackSlice(
                    mvcBitsBegin,
                    mvcBitsEnd,
                    nalu->begin + nalu->numZero + 2,
                    nalu->end,
                    m_video,
                    subTask,
                    fieldId);
            }
            else
            {
                mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
                    mvcBitsBegin,
                    mvcBitsEnd,
                    nalu->begin + nalu->numZero + 2,
                    nalu->end);
            }
        }
    }

    bs.DataLength = mfxU32(mvcBitsBegin - bs.Data - bs.DataOffset);

    return MFX_ERR_NONE;
}

// MVC BD {
// perform re-packing of bitstream for base view
// used within ViewOutput (Blu-ray compatible) encoding mode
mfxStatus ImplementationMvc::UpdateBitstreamBaseView(
    DdiTask & task,
    mfxU32    fieldId) // 0 - top/progressive, 1 - bottom
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Copy and repack bitstream");

    mfxBitstream & bs     = *task.m_bs;

    bs.TimeStamp = task.m_yuv->Data.TimeStamp;
    bs.DecodeTimeStamp = CalcDTSFromPTS(m_video.mfx.FrameInfo, mfxU16(task.m_dpbOutputDelay), bs.TimeStamp);
    bs.FrameType = task.m_type[task.GetFirstField()];
    if ((task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0)
        bs.FrameType = mfxU16(bs.FrameType | (task.m_type[!task.GetFirstField()] << 8));

    mfxU8 * mvcBitsBegin = bs.Data + bs.DataOffset + bs.DataLength;
    mfxU8 * mvcBitsEnd   = bs.Data + bs.MaxLength;

    // copy per-view bitstreams to system memory
    mfxFrameData d3dBits = { 0, };

    FrameLocker lock(
        m_core,
        d3dBits,
        m_bitstream[0].mids[task.m_idxBs[fieldId] + task.m_idxBsOffset]);
    MFX_CHECK(d3dBits.Y != 0, MFX_ERR_LOCK_MEMORY);

    m_bitsDesc[0].begin = &m_sysMemBits[0];
    m_bitsDesc[0].end   = &m_sysMemBits[0] + task.m_bsDataLength[fieldId];

    mfxU32   bsSizeToCopy  = task.m_bsDataLength[fieldId];
    mfxU32   bsSizeAvail   = task.m_bs->MaxLength - task.m_bs->DataOffset - task.m_bs->DataLength;

    assert(bsSizeToCopy <= bsSizeAvail);
    if (bsSizeToCopy > bsSizeAvail)
        bsSizeToCopy = bsSizeAvail;

    FastCopyBufferVid2Sys(
        m_bitsDesc[0].begin,
        d3dBits.Y,
        bsSizeToCopy);

    mfxStatus sts = ParseAvcBitstream(m_bitsDesc[0]);
    MFX_CHECK_STS(sts);

    // copy AUD and SPS for base view
    if (m_bitsDesc[0].aud.begin)
    {
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[0].aud.begin,
            m_bitsDesc[0].aud.end);
    }

    if (m_bitsDesc[0].sps.begin)
    {
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[0].sps.begin,
            m_bitsDesc[0].sps.end);
    }


    if (m_bitsDesc[0].pps.begin)
    {
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[0].pps.begin,
            m_bitsDesc[0].pps.end);
}

    if (m_bitsDesc[0].sei.begin)
    {
        // copy base view sei only
        NaluIterator nalu(m_bitsDesc[0].sei, m_bitsDesc[0].slice.begin);
        for (; nalu->type == 6; ++nalu)
            mvcBitsBegin = CheckedMFX_INTERNAL_CPY(mvcBitsBegin, mvcBitsEnd, nalu->begin, nalu->end);
    }

    NaluIterator nalu(m_bitsDesc[0].slice, m_bitsDesc[0].end);
    for (bool firstSlice = true; nalu->type == 1 || nalu->type == 5; ++nalu, firstSlice = false)
    {
        assert(nalu->type == m_bitsDesc[0].slice.type && "idr and non-idr slices in one au");
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(mvcBitsBegin, mvcBitsEnd, nalu->begin, nalu->end);
    }

    // update hrd state after encoding of access unit
    bool interlace = (task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0;
    m_hrd[0].RemoveAccessUnit(
        task.m_bsDataLength[fieldId],
        interlace,
        (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) != 0);

    bs.DataLength = mfxU32(mvcBitsBegin - bs.Data - bs.DataOffset);

    return MFX_ERR_NONE;
}

// perform re-packing of bitstream for dependent view
// used within ViewOutput (Blu-ray compatible) encoding mode
mfxStatus ImplementationMvc::UpdateBitstreamDepView(
    DdiTask & task,
    mfxU32    fieldId) // 0 - top/progressive, 1 - bottom
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Copy and repack bitstream");

#ifndef AVC_BS
    mfxExtMVCSeqDesc * extMvc = GetExtBuffer(m_video);
#endif // AVC_BS

    mfxBitstream & bs     = *task.m_bs;

    bs.TimeStamp = task.m_yuv->Data.TimeStamp;
    bs.DecodeTimeStamp = CalcDTSFromPTS(m_video.mfx.FrameInfo, mfxU16(task.m_dpbOutputDelay), bs.TimeStamp);
    bs.FrameType = task.m_type[task.GetFirstField()];
    if ((task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0)
        bs.FrameType = mfxU16(bs.FrameType | (task.m_type[!task.GetFirstField()] << 8));

    mfxU8 * mvcBitsBegin = bs.Data + bs.DataOffset + bs.DataLength;
    mfxU8 * mvcBitsEnd   = bs.Data + bs.MaxLength;

    // copy per-view bitstreams to system memory
    mfxFrameData d3dBits = { 0, };
    mfxU32 encIdx = m_numEncs > 1 ? task.m_viewIdx : 0;

    FrameLocker lock(
        m_core,
        d3dBits,
        m_bitstream[encIdx].mids[task.m_idxBs[fieldId] + task.m_idxBsOffset]);
    MFX_CHECK(d3dBits.Y != 0, MFX_ERR_LOCK_MEMORY);

    m_bitsDesc[task.m_viewIdx].begin = &m_sysMemBits[0];
    m_bitsDesc[task.m_viewIdx].end   = &m_sysMemBits[0] + task.m_bsDataLength[fieldId];

    mfxU32   bsSizeToCopy  = task.m_bsDataLength[fieldId];
    mfxU32   bsSizeAvail   = task.m_bs->MaxLength - task.m_bs->DataOffset - task.m_bs->DataLength;

    assert(bsSizeToCopy <= bsSizeAvail);
    if (bsSizeToCopy > bsSizeAvail)
        bsSizeToCopy = bsSizeAvail;

    FastCopyBufferVid2Sys(
        m_bitsDesc[task.m_viewIdx].begin,
        d3dBits.Y,
        bsSizeToCopy);

    mfxStatus sts = ParseAvcBitstream(m_bitsDesc[task.m_viewIdx]);
    MFX_CHECK_STS(sts);

#ifndef AVC_BS // if 0 then regular AVC bitstream is generated for dependent view

    // repack AUD to DRD
    if (m_bitsDesc[task.m_viewIdx].aud.begin)
    {
        mvcBitsBegin = RePackAudToDrd(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[task.m_viewIdx].aud.begin,
            m_bitsDesc[task.m_viewIdx].aud.end);
    }

    // repack SPS to Subset SPS for dependent view
    if (m_bitsDesc[task.m_viewIdx].sps.begin)
    {
        // convert sps of one of dependent views to subset sps
        mvcBitsBegin = RePackSequenceHeader(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[task.m_viewIdx].sps.begin,
            m_bitsDesc[task.m_viewIdx].sps.end,
            m_video);
    }

    // no need to repack pps, just copy
    if (m_bitsDesc[task.m_viewIdx].pps.begin)
    {
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[task.m_viewIdx].pps.begin,
            m_bitsDesc[task.m_viewIdx].pps.end);
    }

    if (m_bitsDesc[task.m_viewIdx].sei.begin)
    {
        // copy sei
        NaluIterator nalu(m_bitsDesc[task.m_viewIdx].sei, m_bitsDesc[task.m_viewIdx].slice.begin);
        while (nalu->type == 6)
        {
            mfxU8 *begin =  nalu->begin;
            mfxU8 *end =  nalu->end;
            nalu++;
            mfxU32 paddingSize = nalu->type == 6 ? 0 : task.m_addRepackSize[fieldId];
            mvcBitsBegin = CheckedMFX_INTERNAL_CPY(mvcBitsBegin, mvcBitsEnd, begin, end - paddingSize); // don't copy compensative padding
        }
    }

    mfxU32 nonIdrFlag    = !(task.m_type[fieldId] & MFX_FRAMETYPE_IDR);
    mfxU32 anchorPicFlag = !nonIdrFlag;
    mfxU32 interViewFlag = GetInterViewFlag(*extMvc, task.m_viewIdx, anchorPicFlag);
    mfxU32 nalRefIdc     = (task.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;

    NaluIterator nalu(m_bitsDesc[task.m_viewIdx].slice, m_bitsDesc[task.m_viewIdx].end);
    for (; nalu->type == 1 || nalu->type == 5; ++nalu)
    {
        // Concern: can't check equality of slice types for base and dep view components of one AU since no sync between views:
        // frame 1 view 0 could be processed before frame 0 view 1.
        // assert(nalu->type == m_bitsDesc[0].slice.type && "idr and non-idr slices in one au"); // disabled

        // pack nal_unit_header_mvc_extension()
        mvcBitsBegin = PackPrefixNalUnit(
            mvcBitsBegin,
            mvcBitsEnd,
            nalu->numZero,
            nalRefIdc,
            20,
            extMvc->View[task.m_viewIdx].ViewId,
            nonIdrFlag,
            anchorPicFlag,
            interViewFlag);

#ifdef I_TO_P
        if (((task.m_type[fieldId] & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_I) &&
            task.m_type[fieldId] & MFX_FRAMETYPE_IDR && task.m_viewIdx)
            mvcBitsBegin = RePackNonIdrSliceToIdr(mvcBitsBegin, mvcBitsEnd, nalu->begin + nalu->numZero + 2, nalu->end, m_video, task, fieldId);
        else
#endif // I_TO_P
        // copy encoded slice
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            nalu->begin + nalu->numZero + 2,
            nalu->end);
    }
    mfxU32 dataLenAfterRepack = mfxU32(mvcBitsBegin - bs.Data - bs.DataOffset - bs.DataLength);

    assert(dataLenAfterRepack <= bsSizeToCopy); // repack overhead shouldn't be greater then padding size

    while (dataLenAfterRepack < bsSizeToCopy)
    {
        *mvcBitsBegin = 0; // write trailing_zero_8bits
        mvcBitsBegin ++;
        dataLenAfterRepack ++;
    }

    bs.DataLength = mfxU32(mvcBitsBegin - bs.Data - bs.DataOffset);
#else // AVC_BS
    // copy AUD and SPS for base view
    if (m_bitsDesc[task.m_viewIdx].aud.begin)
    {
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[task.m_viewIdx].aud.begin,
            m_bitsDesc[task.m_viewIdx].aud.end);
    }

    if (m_bitsDesc[task.m_viewIdx].sps.begin)
    {
        // base view has sps
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[task.m_viewIdx].sps.begin,
            m_bitsDesc[task.m_viewIdx].sps.end);
    }


    if (m_bitsDesc[task.m_viewIdx].pps.begin)
    {
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(
            mvcBitsBegin,
            mvcBitsEnd,
            m_bitsDesc[task.m_viewIdx].pps.begin,
            m_bitsDesc[task.m_viewIdx].pps.end);
}

    if (m_bitsDesc[task.m_viewIdx].sei.begin)
    {
        // copy sei
        NaluIterator nalu(m_bitsDesc[task.m_viewIdx].sei, m_bitsDesc[task.m_viewIdx].slice.begin);
        while (nalu->type == 6)
        {
            mfxU8 *begin =  nalu->begin;
            mfxU8 *end =  nalu->end;
            nalu++;
            mfxU32 paddingSize = nalu->type == 6 ? 0 : task.m_addRepackSize[fieldId];
            mvcBitsBegin = CheckedMFX_INTERNAL_CPY(mvcBitsBegin, mvcBitsEnd, begin, end - paddingSize); // don't copy compensative padding
        }
    }

    NaluIterator nalu(m_bitsDesc[task.m_viewIdx].slice, m_bitsDesc[task.m_viewIdx].end);
    for (bool firstSlice = true; nalu->type == 1 || nalu->type == 5; ++nalu, firstSlice = false)
    {
#ifdef I_TO_P
        if (((task.m_type[fieldId] & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_I) &&
            task.m_type[fieldId] & MFX_FRAMETYPE_IDR && task.m_viewIdx)
            mvcBitsBegin = RePackNonIdrSliceToIdr (mvcBitsBegin, mvcBitsEnd, nalu->begin, nalu->end, m_video, task, fieldId);
        else
#endif // I_TO_P
        mvcBitsBegin = CheckedMFX_INTERNAL_CPY(mvcBitsBegin, mvcBitsEnd, nalu->begin, nalu->end);
    }

    mfxU32 dataLenAfterRepack = mfxU32(mvcBitsBegin - bs.Data - bs.DataOffset - bs.DataLength);

    assert(dataLenAfterRepack <= bsSizeToCopy); // repack overhead shouldn't be greater then padding size

    while (dataLenAfterRepack < bsSizeToCopy)
    {
        *mvcBitsBegin = 0; // write trailing_zero_8bits
        mvcBitsBegin ++;
        dataLenAfterRepack ++;
    }

    bs.DataLength = mfxU32(mvcBitsBegin - bs.Data - bs.DataOffset);

#endif // AVC_BS

    // update hrd state after encoding of access unit
    bool interlace = (task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0;
    m_hrd[task.m_viewIdx].RemoveAccessUnit(
        task.m_bsDataLength[fieldId],
        interlace,
        (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) != 0);

    return MFX_ERR_NONE;
}

// w/a for SNB/IVB: calculate size of padding to compensate re-pack  of AVC headers to MVC
mfxU32 ImplementationMvc::CalcPaddingToCompensateRepack(
    DdiTask & task,
    mfxU32 fieldId)
{
    mfxU32 paddingSize = 0;
    mfxExtMVCSeqDesc * extMvc = GetExtBuffer(m_video);
    mfxExtCodingOption * extCO = GetExtBuffer(m_video);

    if (m_spsSubsetSpsDiff == 0 && task.m_viewIdx) // difference between SPS and Subset SPS isn't calculated yet (it isn't zero anyway)
    {
        mfxExtSpsHeader * extSps = GetExtBuffer(m_video);
        mfxExtSpsHeader sps = *extSps;
        sps.seqParameterSetId = 1; // set sps id to 1 for non-base views (can change size of SPS RBSP)
        sps.profileIdc = 100; // set High profile to calculate size of AVC SPS (doesn't change size of SPS RBSP)
        OutputBitstream writerSps(&m_sysMemBits[0], 1000); // FIXME: take appropriate size
        WriteSpsHeader(writerSps, sps); // encode SPS
        mfxU16 sizeOfSps = mfxU16((writerSps.GetNumBits() + 7) / 8);
        sps.profileIdc = 128; // set Stereo High profile to calculate size of MVC Subset SPS (doesn't change size of Subset SPS RBSP)
        OutputBitstream writerSubsetSps(&m_sysMemBits[0], 1000); // FIXME: take appropriate size
        WriteSpsHeader(writerSubsetSps, sps, extMvc->Header); // encode Subset SPS
        mfxU16 sizeOfSubsetSps = mfxU16((writerSubsetSps.GetNumBits() + 7) / 8);
        m_spsSubsetSpsDiff = sizeOfSubsetSps - sizeOfSps;
    }

    if (task.m_viewIdx > 0) // isn't base view
    {
        if (task.m_insertSps[fieldId])
        {
            paddingSize += m_spsSubsetSpsDiff;
            paddingSize += 1; // to assure that Subset SPS will fit into size (SPS + m_spsSubsetSpsDiff)
        }
        paddingSize += 3 * m_video.mfx.NumSlice; // 3 additional bytes to NALu header per slice for MVC Slice Ext

#ifdef I_TO_P
        if (((task.m_type[fieldId] & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_I) &&
            task.m_type[fieldId] & MFX_FRAMETYPE_IDR && task.m_viewIdx
            && extCO->ViewOutput == MFX_CODINGOPTION_ON)
            paddingSize += 5 * m_video.mfx.NumSlice; // additional place for future repack of non-IDR slices to IDR
#endif // I_TO_P
    }
    else // base view
    {
        // TODO: calculate size of Prefix NALu here
    }

    return paddingSize;
}

void ImplementationMvc::GetInitialHrdState(
    MvcTask & task,
    mfxU32 viewIdx)
{
    if (task[viewIdx])
    {
        task[viewIdx]->m_initCpbRemoval = m_hrd[viewIdx].GetInitCpbRemovalDelay();
        task[viewIdx]->m_initCpbRemovalOffset = m_hrd[viewIdx].GetInitCpbRemovalDelayOffset();
    }
}

void ImplementationMvc::PatchTask(MvcTask const & mvcTask, DdiTask & curTask, mfxU32 fieldId)
{
    mfxU32 viewIdx = curTask.m_viewIdx;

#ifdef I_TO_P
    mfxExtCodingOption * extOpt = GetExtBuffer(m_video);

    // extOpt->ViewOutput == MFX_CODINGOPTION_ON means Blu-ray/AVCHD compatible MVC encoding
    // in this mode I-slices are prohibited in dependent view
    // SNB/IVB driver can't correctly encode P-frames w/o references (artifacts appear)
    // so all dependent view IDRs should be encoded with inter-view references
    if (extOpt->ViewOutput == MFX_CODINGOPTION_ON)
    {
        if (((curTask.m_type[fieldId] & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_I) && curTask.m_viewIdx)
        {
            // change picture type from I-picture to P-picture
            curTask.m_type[fieldId] &= ~MFX_FRAMETYPE_I;
            curTask.m_type[fieldId] |= MFX_FRAMETYPE_P;
            // remove IDR flag and add inter-view reference IDR picture
            if (curTask.m_type[fieldId] & MFX_FRAMETYPE_IDR)
            {
                curTask.m_type[fieldId] &= ~MFX_FRAMETYPE_IDR;
#ifdef MVC_ADD_REF
                ArrayDpbFrame & curDpb = curTask.m_dpb[0];
                mfxFrameData sourceD3DBits = { 0, };
                mfxFrameData distD3DBits = { 0, };

                // add inter-view reference to DPB
                DpbFrame newDpbFrame;
                newDpbFrame.m_frameIdx = (mfxU8)CalcNumSurfRecon(m_video);
                newDpbFrame.m_poc      = curTask.GetPoc();
                newDpbFrame.m_viewIdx  = 0;
                newDpbFrame.m_longterm = 0;
                curDpb.PushBack(newDpbFrame);

                // add inter-view reference to ref list
                ArrayU8x33 & list0  = curTask.m_list0[0];
                list0.PushFront((mfxU8)curDpb.Size() - 1);

                // for 2nd and subsequent IDRs need to copy reconstruct from base view
                if (curTask.m_frameOrder)
                {
                    DdiTask const* pBaseViewTask = mvcTask[0];

                    // tried to copy via FastCopy. It doesn't work as well ass lock + memcpy
                    /*mfxFrameSurface1 src = { 0, };
                    mfxFrameSurface1 dst = { 0, };

                    src.Info = dst.Info = m_video.mfx.FrameInfo;
                    src.Data.MemId = m_recon[0].mids[pBaseViewTask->m_idxRecon];
                    dst.Data.MemId = m_recon[1].mids[newDpbFrame.m_frameIdx];

                    m_core->DoFastCopyWrapper(&dst, MFX_MEMTYPE_D3D_INT, &src, MFX_MEMTYPE_D3D_INT);*/

                    // lock base view recon
                    FrameLocker lockSrc(
                        m_core,
                        sourceD3DBits,
                        m_recon[0].mids[pBaseViewTask->m_idxRecon]);

                    // lock dep view recon
                    FrameLocker lockDst(
                        m_core,
                        distD3DBits,
                        m_recon[1].mids[newDpbFrame.m_frameIdx]);
                    assert(sourceD3DBits.Y && distD3DBits.Y);

                    // copy base view reconstruct to dep view recon surface
                    mfxU32 lumaSize = sourceD3DBits.Pitch * m_video.mfx.FrameInfo.Height;
                    MFX_INTERNAL_CPY(distD3DBits.Y, sourceD3DBits.Y, lumaSize);
                    MFX_INTERNAL_CPY(distD3DBits.UV, sourceD3DBits.UV, lumaSize / 2);
                }

                // workaround for DX11: copy from D3D surface to D3D surface doesn't work w/o repeated lock/unlock
                // begin to work after copying of DEFAULT surface to STAGING during repeated lock
                if (curTask.m_frameOrder && m_core->GetVAType() == MFX_HW_D3D11)
                {
                    FrameLocker lockDst(
                        m_core,
                        distD3DBits,
                        m_recon[1].mids[newDpbFrame.m_frameIdx]);

#if 0 // dump copied frame
                    char fileName[100];
                    sprintf(fileName, "recon_%d.yuv", curTask.m_frameOrder);

                    FILE * file = fopen(fileName, "w + b");
                    mfxI16 i, j;
                    mfxU8 buf[2048];
                    mfxI16 W = m_video.mfx.FrameInfo.Width;
                    mfxI16 H = m_video.mfx.FrameInfo.Height;
                    mfxU8 * p = distD3DBits.Y;
                    for (i = 0; i < H; i++) {
                        fwrite(p, 1, W, file);
                        p += distD3DBits.Pitch;
                    }
                    p = distD3DBits.U;
                    for (i = 0; i < H >> 1; i++) {
                        for (j = 0; j < W >> 1; j++) {
                            buf[j] = p[2*j];
                        }
                        fwrite(buf, 1, W >> 1, file);
                        p += distD3DBits.Pitch;
                    }
                    p = distD3DBits.V;
                    for (i = 0; i < H >> 1; i++) {
                        for (j = 0; j < W >> 1; j++) {
                            buf[j] = p[2*j];
                        }
                        fwrite(buf, 1, W >> 1, file);
                        p += distD3DBits.Pitch;
                    }
                    fclose(file);
#endif // dump copied frame
                }

#else // MVC_ADD_REF
                curTask.m_dpb[0].Resize(0);
#endif // MVC_ADD_REF
            }
        }
    }
#endif // I_TO_P

    // if all views are produced with one AVC encoder then ref lists and dpb for AVC task should be modified
    if (m_numEncs == 1)
    {
        ArrayDpbFrame & curDpb = curTask.m_dpb[fieldId];
        curDpb.Resize(0);

        for (mfxU32 v = 0; v < mvcTask.NumView(); v++)
        {
            DdiTask const &       otherTask  = *mvcTask[v];
            ArrayDpbFrame const & otherDpb   = otherTask.m_dpb[fieldId];
            ArrayU8x33 const &    otherList0 = otherTask.m_list0[fieldId];
            ArrayU8x33 const &    otherList1 = otherTask.m_list1[fieldId];

            // insert first reference frame from list0 of each view
            // for interlaced encoding don't insert references from views with lower viewIdx
            if (otherList0.Size() > 0 && ((curTask.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) || (v >= viewIdx)))
            {
                if (std::find_if(curDpb.Begin(), curDpb.End(), FindByPocAndViewIdx(otherDpb[otherList0[0] & 0x7f])) == curDpb.End())
                {
                    curDpb.PushBack(otherDpb[otherList0[0] & 0x7f]);
                    curDpb.Back().m_frameIdx = mfxU8(curDpb.Back().m_frameIdx + otherTask.m_idxReconOffset);
                }
            }

            // insert first reference frame from list1 of each view
            if (otherList1.Size() > 0)
            {
                if (std::find_if(curDpb.Begin(), curDpb.End(), FindByPocAndViewIdx(otherDpb[otherList1[0] & 0x7f])) == curDpb.End())
                {
                    curDpb.PushBack(otherDpb[otherList1[0] & 0x7f]);
                    curDpb.Back().m_frameIdx = mfxU8(curDpb.Back().m_frameIdx + otherTask.m_idxReconOffset);
                }
            }

            // insert reconstruction of previous views
            if (v < viewIdx && otherTask.m_type[fieldId] & MFX_FRAMETYPE_REF)
            {
                DpbFrame prevViewRecon;
                prevViewRecon.m_frameIdx = mfxU8(otherTask.m_idxRecon + otherTask.m_idxReconOffset);
                prevViewRecon.m_viewIdx  = mfxU8(otherTask.m_viewIdx);
                prevViewRecon.m_poc      = otherTask.GetPoc();

                if (std::find_if(curDpb.Begin(), curDpb.End(), FindByPocAndViewIdx(prevViewRecon)) == curDpb.End())
                    curDpb.PushBack(prevViewRecon);
            }

            // for 2nd field of reference field pair reconstruct of current view should be also inserted to DPB
            bool bIs2ndFieldOfRefPair = (curTask.GetFirstField() != fieldId) && (curTask.m_type[fieldId] & MFX_FRAMETYPE_REF);
            if (v == viewIdx && bIs2ndFieldOfRefPair == true)
            {
                DpbFrame curViewRecon;
                curViewRecon.m_frameIdx = mfxU8(otherTask.m_idxRecon + otherTask.m_idxReconOffset);
                curViewRecon.m_viewIdx  = mfxU8(otherTask.m_viewIdx);
                curViewRecon.m_poc      = otherTask.GetPoc();

                if (std::find_if(curDpb.Begin(), curDpb.End(), FindByPocAndViewIdx(curViewRecon)) == curDpb.End())
                    curDpb.PushBack(curViewRecon);
            }

        }

        // now insert remaining reference frames
        /*
        for (mfxU32 v = 0; v < mvcTask.NumView(); v++)
        {
            DdiTask const &       otherTask  = *mvcTask[v];
            ArrayDpbFrame const & otherDpb   = otherTask.m_dpb[fieldId];
            ArrayU8x33 const &    otherList0 = otherTask.m_list0[fieldId];
            ArrayU8x33 const &    otherList1 = otherTask.m_list1[fieldId];

            for (mfxU32 j = 1; j < otherList0.Size(); j++)
            {
                if (std::find_if(curDpb.Begin(), curDpb.End(), FindByPocAndViewIdx(otherDpb[otherList0[j] & 0x7f])) == curDpb.End())
                {
                    curDpb.PushBack(otherDpb[otherList0[j] & 0x7f]);
                    curDpb.Back().m_frameIdx = mfxU8(curDpb.Back().m_frameIdx + otherTask.m_idxReconOffset);
                }
            }

            for (mfxU32 j = 1; j < otherList1.Size(); j++)
            {
                if (std::find_if(curDpb.Begin(), curDpb.End(), FindByPocAndViewIdx(otherDpb[otherList1[j] & 0x7f])) == curDpb.End())
                {
                    curDpb.PushBack(otherDpb[otherList1[j] & 0x7f]);
                    curDpb.Back().m_frameIdx = mfxU8(curDpb.Back().m_frameIdx + otherTask.m_idxReconOffset);
                }
            }
        }
        */

        ArrayDpbFrame const & oldDpb = mvcTask[viewIdx]->m_dpb[fieldId];

        // update first entries of list0 and list1, since dpb entries were reordered
        // only first entries since others are not used
        ArrayU8x33 & curList0 = curTask.m_list0[fieldId];
        if (curList0.Size() > 0)
        {
            mfxU8 posInNewDpb =
                mfxU8(std::find_if(curDpb.Begin(), curDpb.End(), FindByPocAndViewIdx(oldDpb[curList0[0] & 0x7f])) - curDpb.Begin());
            curList0.Fill((curList0[0] & 0x80) + posInNewDpb);
        }

        ArrayU8x33 & curList1 = curTask.m_list1[fieldId];
        if (curList1.Size() > 0)
        {
            mfxU8 posInNewDpb =
                mfxU8(std::find_if(curDpb.Begin(), curDpb.End(), FindByPocAndViewIdx(oldDpb[curList1[0] & 0x7f])) - curDpb.Begin());
            curList1.Fill((curList1[0] & 0x80) + posInNewDpb);
        }

        curTask.m_idxRecon       += curTask.m_idxReconOffset;
        curTask.m_idxBs[fieldId] += curTask.m_idxBsOffset;
    }

    for (size_t i = 0; i < GetMaxNumSlices(m_video); ++ i)
    {
        mfxExtCodingOption2 const &     extOpt2        = GetExtBufferRef(m_video);
        mfxU8 disableDeblockingIdc = mfxU8(extOpt2.DisableDeblockingIdc);
        curTask.m_disableDeblockingIdc[fieldId].push_back(disableDeblockingIdc);
        curTask.m_sliceAlphaC0OffsetDiv2[fieldId].push_back( 0);
        curTask.m_sliceBetaOffsetDiv2[fieldId].push_back(0);
    }
}
// MVC BD }


mfxStatus ImplementationMvc::CopyRawSurface(DdiTask const & task)
{
    if (m_inputFrameType == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        mfxFrameData d3dSurf = { 0 };
        mfxFrameData sysSurf = task.m_yuv->Data;

        mfxU32 numRawSurfPerView = CalcNumSurfRaw(m_video);

        mfxU32 encIdx = m_numEncs > 1 ? task.m_viewIdx : 0;
        //FrameLocker lock1(m_core, d3dSurf, m_raw[encIdx].mids[task.m_idx + m_numEncs > 1 ? 0 : task.m_viewIdx * numRawSurfPerView]);
        //MFX_CHECK(d3dSurf.Y != 0, MFX_ERR_LOCK_MEMORY);
        d3dSurf.MemId = m_raw[encIdx].mids[task.m_idx + m_numEncs > 1 ? 0 : task.m_viewIdx * numRawSurfPerView];
        FrameLocker lock2(m_core, sysSurf, true);
        MFX_CHECK(sysSurf.Y != 0, MFX_ERR_LOCK_MEMORY);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Copy input (sys->d3d)");
            mfxStatus sts = CopyFrameDataBothFields(m_core, d3dSurf, sysSurf, m_video.mfx.FrameInfo);
            MFX_CHECK_STS(sts);
        }

        mfxStatus sts = lock2.Unlock();
        MFX_CHECK_STS(sts);

        //sts = lock1.Unlock();
        //MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

mfxMemId ImplementationMvc::GetRawSurfaceMemId(DdiTask const & task)
{
    mfxU32 encIdx = m_numEncs > 1 ? task.m_viewIdx : 0;
    return m_inputFrameType == MFX_IOPATTERN_IN_SYSTEM_MEMORY
        ? m_raw[encIdx].mids[task.m_idx + m_numEncs > 1 ? 0 : task.m_viewIdx * CalcNumSurfRaw(m_video)]
        : task.m_yuv->Data.MemId;
}

mfxStatus ImplementationMvc::GetRawSurfaceHandle(DdiTask const & task, mfxHDLPair & hdl)
{
    mfxMemId mid = GetRawSurfaceMemId(task);

    mfxStatus sts = (m_video.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY)
        ? m_core->GetExternalFrameHDL(mid, (mfxHDL *)&hdl)
        : m_core->GetFrameHDL(mid, (mfxHDL *)&hdl);

    return sts;
}
// MVC BD }

#endif // MFX_ENABLE_MVC_VIDEO_ENCODE_HW
