//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#include "mfx_h264_enc.h"

#ifdef MFX_VA
#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)

#include <algorithm>
#include <numeric>

#include "mfx_session.h"
#include "mfx_task.h"
#include "libmfx_core.h"
#include "libmfx_core_hw.h"
#include "libmfx_core_interface.h"
#include "mfx_ext_buffers.h"

#include "mfx_h264_encode_hw.h"
#include "mfx_enc_common.h"
#include "mfx_h264_enc_common_hw.h"
#include "mfx_h264_encode_hw_utils.h"
#include "umc_va_dxva2_protected.h"

#include "mfx_h264_enc.h"
#include "mfx_ext_buffers.h"

#include "mfx_h264_encode_vaapi.h"

#if defined(_DEBUG)
#define mdprintf fprintf
#else
#define mdprintf(...)
#endif

namespace MfxEncENC
{

static bool IsVideoParamExtBufferIdSupported(mfxU32 id)
{
    return
        id == MFX_EXTBUFF_CODING_OPTION  ||
        id == MFX_EXTBUFF_CODING_OPTION2 ||
        id == MFX_EXTBUFF_CODING_OPTION3 ||
        id == MFX_EXTBUFF_FEI_PARAM;
}

mfxExtBuffer* GetExtBuffer(mfxExtBuffer** ebuffers, mfxU32 nbuffers, mfxU32 BufferId)
{
    if (!ebuffers) return NULL;
    for (mfxU32 i = 0; i<nbuffers; ++i) {
        if (!ebuffers[i]) continue;
        if (ebuffers[i]->BufferId == BufferId) {
            return ebuffers[i];
        }
    }
    return NULL;
}

static mfxStatus CheckExtBufferId(mfxVideoParam const & par)
{
    for (mfxU32 i = 0; i < par.NumExtParam; ++i)
    {
        MFX_CHECK(par.ExtParam[i], MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(MfxEncENC::IsVideoParamExtBufferIdSupported(par.ExtParam[i]->BufferId), MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(!MfxEncENC::GetExtBuffer(
            par.ExtParam + i + 1,
            par.NumExtParam - i - 1,
            par.ExtParam[i]->BufferId), MFX_ERR_INVALID_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;
}

} // namespace MfxEncENC

bool bEnc_ENC(mfxVideoParam *par)
{
    MFX_CHECK(par, false);

    mfxExtFeiParam *pControl = NULL;

    for (mfxU16 i = 0; i < par->NumExtParam; ++i)
        if (par->ExtParam[i] != 0 && par->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM){ // assuming aligned buffers
            pControl = reinterpret_cast<mfxExtFeiParam *>(par->ExtParam[i]);
            break;
        }

    return pControl ? (pControl->Func == MFX_FEI_FUNCTION_ENC) : false;
}

static mfxStatus AsyncRoutine(void * state, void * param, mfxU32, mfxU32)
{
    VideoENC_ENC & impl = *(VideoENC_ENC *)state;
    return  impl.RunFrameVmeENC(NULL, NULL);
}

void TEMPORAL_HACK_WITH_DPB(
    ArrayDpbFrame &             dpb,
    mfxMemId const *            mids,
    std::vector<mfxU32> const & fo);

void ConfigureTask_FEI_ENC(
    DdiTask &                       task,
    DdiTask const &                 prevTask,
    MfxVideoParam const &           video,
    std::map<mfxU32, mfxU32> &      frameOrder_frameNum)
{
    mfxExtCodingOption2 const &        extOpt2             = GetExtBufferRef(video);
    mfxExtCodingOption2 const *        extOpt2Runtime      = GetExtBuffer(task.m_ctrl);
    mfxExtSpsHeader     const &        extSps              = GetExtBufferRef(video);


    mfxU32 const FRAME_NUM_MAX = 1 << (extSps.log2MaxFrameNumMinus4 + 4);

    mfxU32 numReorderFrames = GetNumReorderFrames(video);
    mfxU32 prevsfid         = prevTask.m_fid[1];
    mfxU8  idrPicFlag       = !!(task.GetFrameType() & MFX_FRAMETYPE_IDR);
    mfxU8  intraPicFlag     = !!(task.GetFrameType() & MFX_FRAMETYPE_I);
    mfxU8  prevIdrFrameFlag = !!(prevTask.GetFrameType() & MFX_FRAMETYPE_IDR);
    mfxU8  prevIFrameFlag   = !!(prevTask.GetFrameType() & MFX_FRAMETYPE_I);
    mfxU8  prevRefPicFlag   = !!(prevTask.GetFrameType() & MFX_FRAMETYPE_REF);
    mfxU8  prevIdrPicFlag   = !!(prevTask.m_type[prevsfid] & MFX_FRAMETYPE_IDR);

    mfxU8  frameNumIncrement = (prevRefPicFlag || prevTask.m_nalRefIdc[0]) ? 1 : 0;

    task.m_frameOrderIdr = idrPicFlag   ? task.m_frameOrder : prevTask.m_frameOrderIdr;
    task.m_frameOrderI   = intraPicFlag ? task.m_frameOrder : prevTask.m_frameOrderI;
    task.m_encOrder      = prevTask.m_encOrder + 1;
    task.m_encOrderIdr   = prevIdrFrameFlag ? prevTask.m_encOrder : prevTask.m_encOrderIdr;
    task.m_encOrderI     = prevIFrameFlag   ? prevTask.m_encOrder : prevTask.m_encOrderI;


    task.m_frameNum = idrPicFlag ? 0 : mfxU16((prevTask.m_frameNum + frameNumIncrement) % FRAME_NUM_MAX);


    task.m_picNum[1] = task.m_picNum[0] = task.m_frameNum * (task.m_fieldPicFlag + 1) + task.m_fieldPicFlag;

    task.m_idrPicId = prevTask.m_idrPicId + idrPicFlag;

    mfxU32 ffid = task.m_fid[0];
    mfxU32 sfid = !ffid;

    task.m_pid = 0;

    task.m_reference[ffid] = task.m_nalRefIdc[ffid] = !!(task.m_type[ffid] & MFX_FRAMETYPE_REF);
    task.m_reference[sfid] = task.m_nalRefIdc[sfid] = !!(task.m_type[sfid] & MFX_FRAMETYPE_REF);

    if (video.calcParam.lyncMode)
    {
        task.m_insertPps[ffid] = task.m_insertSps[ffid];
        task.m_insertPps[sfid] = task.m_insertSps[sfid];
        task.m_nalRefIdc[ffid] = idrPicFlag ? 3 : (task.m_reference[ffid] ? 2 : 0);
        task.m_nalRefIdc[sfid] = task.m_reference[sfid] ? 2 : 0;
    }

    task.m_cqpValue[0] = GetQpValue(video, task.m_ctrl, task.m_type[0]);
    task.m_cqpValue[1] = GetQpValue(video, task.m_ctrl, task.m_type[1]);

    const mfxExtCodingOption2* extOpt2Cur = (extOpt2Runtime ? extOpt2Runtime : &extOpt2);

    mfxENCInput* inParams = (mfxENCInput*)task.m_userData[0];

    frameOrder_frameNum[task.m_frameOrder] = task.m_frameNum;

    // Fill Deblocking parameters
    mfxU32 fieldMaxCount = (video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;
    for (mfxU32 field = 0; field < fieldMaxCount; ++field)
    {
        mfxU32 fieldParity = (video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF)? (1 - field) : field;

        /* To change de-blocking params in runtime we need to take params from runtime control */
        /* And runtime params has priority before iInit() params */
        mfxExtFeiSliceHeader * extFeiSlice = GetExtBufferFEI(inParams, field);
        if (!extFeiSlice)
            extFeiSlice = GetExtBuffer(video, fieldParity);

        for (size_t i = 0; i < GetMaxNumSlices(video); ++i)
        {
            /* default parameters */
            mfxU8 disableDeblockingIdc   = 0;
            mfxI8 sliceAlphaC0OffsetDiv2 = 0;
            mfxI8 sliceBetaOffsetDiv2    = 0;

            if (NULL != extOpt2Cur)
            {
                disableDeblockingIdc = (mfxU8)extOpt2Cur->DisableDeblockingIdc;
                if (disableDeblockingIdc > 2)
                    disableDeblockingIdc = 0;
            }

            if (NULL != extFeiSlice && NULL != extFeiSlice->Slice)
            {
                disableDeblockingIdc   = (mfxU8) extFeiSlice->Slice[i].DisableDeblockingFilterIdc;
                if (disableDeblockingIdc > 2)
                    disableDeblockingIdc = 0;
                sliceAlphaC0OffsetDiv2 = (mfxI8) extFeiSlice->Slice[i].SliceAlphaC0OffsetDiv2;
                sliceBetaOffsetDiv2    = (mfxI8) extFeiSlice->Slice[i].SliceBetaOffsetDiv2;
            }

            /* Now put values */
            task.m_disableDeblockingIdc[fieldParity].push_back(disableDeblockingIdc);
            task.m_sliceAlphaC0OffsetDiv2[fieldParity].push_back(sliceAlphaC0OffsetDiv2);
            task.m_sliceBetaOffsetDiv2[fieldParity].push_back(sliceBetaOffsetDiv2);
        } // for (size_t i = 0; i < GetMaxNumSlices(video); i++)

        // Fill DPB
        mfxExtFeiPPS * pDataPPS = GetExtBufferFEI(inParams, field);

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
            frame.m_refPicFlag[task.m_fid[1]] = task.m_fieldPicFlag ? 0 : 1;

            //frame.m_frameIdx   = idx;
            frame.m_longterm   = 0;

            task.m_dpb[fieldParity].PushBack(frame);
        }

        // Get default reflists
        MfxHwH264Encode::InitRefPicList(task, fieldParity);
        ArrayU8x33 initList0 = task.m_list0[fieldParity];
        ArrayU8x33 initList1 = task.m_list1[fieldParity];

        // Fill reflists
        mfxExtFeiSliceHeader * pDataSliceHeader = extFeiSlice;
        /* Number of reference handling */
        //mfxU32 ref_counter_l0 = 0, ref_counter_l1 = 0;
        mfxU32 maxNumRefL0 = 1, maxNumRefL1 = 1;
        if (pDataSliceHeader && pDataSliceHeader->Slice)
        {
            maxNumRefL0 = pDataSliceHeader->Slice[0].NumRefIdxL0Active;
            maxNumRefL1 = pDataSliceHeader->Slice[0].NumRefIdxL1Active;
            if ((maxNumRefL1 > 2) && task.m_fieldPicFlag)
                maxNumRefL1 = 2;
        }

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

}

mfxStatus VideoENC_ENC::RunFrameVmeENC(mfxENCInput *in, mfxENCOutput *out)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoENC_ENC::RunFrameVmeENC");

    mfxStatus sts = MFX_ERR_NONE;
    DdiTask & task = m_incoming.front();
    mfxU32 f = 0, f_start = 0;
    mfxU32 fieldCount = task.m_fieldPicFlag;

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
    {
        fieldCount = f_start = m_firstFieldDone; // 0 or 1
    }

    if (!task.m_fieldPicFlag || (task.m_fieldPicFlag && f_start != fieldCount))
    {
        sts = GetNativeHandleToRawSurface(*m_core, m_video, task, task.m_handleRaw);
        MFX_CHECK(sts == MFX_ERR_NONE, Error(sts));

        mfxENCOutput* outParams = reinterpret_cast<mfxENCOutput*>(task.m_userData[1]);

        mfxHDL handle_src, handle_rec;
        sts = m_core->GetExternalFrameHDL(outParams->OutSurface->Data.MemId, &handle_src);
        MFX_CHECK(MFX_ERR_NONE == sts, MFX_ERR_INVALID_HANDLE);

        mfxU32* src_surf_id = (mfxU32 *)handle_src, *rec_surf_id;

        for (mfxU32 i = 0; i < m_rec.NumFrameActual; ++i)
        {
            task.m_midRec = AcquireResource(m_rec, i);

            sts = m_core->GetFrameHDL(m_rec.mids[i], &handle_rec);
            MFX_CHECK(MFX_ERR_NONE == sts, MFX_ERR_INVALID_HANDLE);

            rec_surf_id = (mfxU32 *)handle_rec;
            if ((*src_surf_id) == (*rec_surf_id))
            {
                task.m_idxRecon = i;
                break;
            }
            else
                ReleaseResource(m_rec, task.m_midRec);
        }

        ConfigureTask_FEI_ENC(task, m_prevTask, m_video, m_frameOrder_frameNum);

        //!!! HACK !!!
        m_recFrameOrder[task.m_idxRecon] = task.m_frameOrder;
        TEMPORAL_HACK_WITH_DPB(task.m_dpb[0],          m_rec.mids, m_recFrameOrder);
        TEMPORAL_HACK_WITH_DPB(task.m_dpb[1],          m_rec.mids, m_recFrameOrder);
        TEMPORAL_HACK_WITH_DPB(task.m_dpbPostEncoding, m_rec.mids, m_recFrameOrder);
    }

    for (f = f_start; f <= fieldCount; f++)
    {
        //fprintf(stderr,"handle=0x%x mid=0x%x\n", task.m_handleRaw, task.m_midRaw );
        sts = m_ddi->Execute(task.m_handleRaw.first, task, task.m_fid[f], m_sei);
        MFX_CHECK(sts == MFX_ERR_NONE, Error(sts));
    }

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
    {
        m_firstFieldDone = 1 - m_firstFieldDone;
    }

    if (0 == m_firstFieldDone)
    {
        m_prevTask = task;
    }

    return sts;
}

static mfxStatus AsyncQuery(void * state, void * param, mfxU32 /*threadNumber*/, mfxU32 /*callNumber*/)
{
    VideoENC_ENC & impl = *(VideoENC_ENC *)state;
    DdiTask& task = *(DdiTask *)param;
    return impl.Query(task);
}

mfxStatus VideoENC_ENC::Query(DdiTask& task)
{
    mdprintf(stderr,"query\n");
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 f = 0, f_start = 0;
    mfxU32 fieldCount = task.m_fieldPicFlag;

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
    {
        f_start = fieldCount = 1 - m_firstFieldDone;
    }

    for (f = f_start; f <= fieldCount; f++)
    {
        sts = m_ddi->QueryStatus(task, task.m_fid[f]);
        MFX_CHECK(sts != MFX_WRN_DEVICE_BUSY, MFX_TASK_BUSY);
        MFX_CHECK(sts == MFX_ERR_NONE, Error(sts));
    }

    mfxENCInput *input = (mfxENCInput *)task.m_userData[0];
    m_core->DecreaseReference(&(input->InSurface->Data));

    UMC::AutomaticUMCMutex guard(m_listMutex);
    //move that task to free tasks from m_incoming
    //m_incoming
    std::list<DdiTask>::iterator it = std::find(m_incoming.begin(), m_incoming.end(), task);
    MFX_CHECK(it != m_incoming.end(), MFX_ERR_NOT_FOUND);

    m_free.splice(m_free.end(), m_incoming, it);

    ReleaseResource(m_rec, task.m_midRec);

    return MFX_ERR_NONE;
}

mfxStatus VideoENC_ENC::QueryIOSurf(VideoCORE* , mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(par,request);

    mfxU32 inPattern = par->IOPattern & MfxHwH264Encode::MFX_IOPATTERN_IN_MASK;
    MFX_CHECK(
        inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY,
        MFX_ERR_INVALID_VIDEO_PARAM);

    if (inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
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
    /* 2*par->mfx.GopRefDist */
    request->NumFrameMin         = 2*par->mfx.GopRefDist + par->AsyncDepth;
    request->NumFrameSuggested   = request->NumFrameMin;
    request->Info                = par->mfx.FrameInfo;
    return MFX_ERR_NONE;
}


VideoENC_ENC::VideoENC_ENC(VideoCORE *core,  mfxStatus * sts)
    : m_bInit(false)
    , m_core(core)
    , m_prevTask()
    , m_singleFieldProcessingMode(0)
    , m_firstFieldDone(0)
{
    *sts = MFX_ERR_NONE;
}


VideoENC_ENC::~VideoENC_ENC()
{
    Close();
}

mfxStatus VideoENC_ENC::Init(mfxVideoParam *par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoENC_ENC::Init");
    mfxStatus sts = MfxEncENC::CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    m_video = *par;
    //add ext buffers from par to m_video

    MfxVideoParam tmp(*par);

    sts = ReadSpsPpsHeaders(tmp);
    MFX_CHECK_STS(sts);

    sts = CheckWidthAndHeight(tmp);
    MFX_CHECK_STS(sts);

    sts = CopySpsPpsToVideoParam(tmp);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    m_ddi.reset(new MfxHwH264Encode::VAAPIFEIENCEncoder);

    sts = m_ddi->CreateAuxilliaryDevice(
        m_core,
        DXVA2_Intel_Encode_AVC,
        GetFrameWidth(m_video),
        GetFrameHeight(m_video));

    MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

    const mfxExtFeiParam* params = GetExtBuffer(m_video);
    MFX_CHECK(params, MFX_ERR_INVALID_VIDEO_PARAM);

    if ((MFX_CODINGOPTION_ON == params->SingleFieldProcessing) &&
         ((MFX_PICSTRUCT_FIELD_TFF == m_video.mfx.FrameInfo.PicStruct) ||
          (MFX_PICSTRUCT_FIELD_BFF == m_video.mfx.FrameInfo.PicStruct)) )
        m_singleFieldProcessingMode = MFX_CODINGOPTION_ON;

    sts = m_ddi->QueryEncodeCaps(m_caps);
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

    m_currentPlatform = m_core->GetHWType();
    m_currentVaType   = m_core->GetVAType();

    mfxStatus spsppsSts = MfxHwH264Encode::CopySpsPpsToVideoParam(m_video);

    mfxStatus checkStatus = MfxHwH264Encode::CheckVideoParam(m_video, m_caps, m_core->IsExternalFrameAllocator(), m_currentPlatform, m_currentVaType);
    MFX_CHECK(checkStatus != MFX_WRN_PARTIAL_ACCELERATION, MFX_WRN_PARTIAL_ACCELERATION);
    MFX_CHECK(checkStatus >= MFX_ERR_NONE, checkStatus);

    if (checkStatus == MFX_ERR_NONE)
        checkStatus = spsppsSts;

    //raw surfaces should be created before accel service
    mfxFrameAllocRequest request = { };
    request.Info = m_video.mfx.FrameInfo;

    /* ENC does not generate real reconstruct surface,
     * and this surface should be unchanged
     * BUT (!)
     * (1): this surface should be from reconstruct surface pool which was passed to
     * component when vaCreateContext was called
     * (2): And it should be same surface which will be used for PAK reconstructed in next call
     * (3): And main rule: ENC (N number call) and PAK (N number call) should have same exactly
     * same reference /reconstruct list !
     * */
    request.Type              = MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;
    request.NumFrameMin       = m_video.mfx.GopRefDist * 2 + (m_video.AsyncDepth-1) + 1 + m_video.mfx.NumRefFrame + 1;
    request.NumFrameSuggested = request.NumFrameMin;
    request.AllocId           = par->AllocId;

    //sts = m_core->AllocFrames(&request, &m_rec);
    sts = m_rec.Alloc(m_core,request, false);
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_rec, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    m_recFrameOrder.resize(request.NumFrameMin, 0xffffffff);

    sts = m_ddi->CreateAccelerationService(m_video);
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

    m_inputFrameType =
        (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY || m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            ? MFX_IOPATTERN_IN_SYSTEM_MEMORY
            : MFX_IOPATTERN_IN_VIDEO_MEMORY;


    m_free.resize(m_video.AsyncDepth);
    m_incoming.clear();

    m_bInit = true;
    return checkStatus;
}
mfxStatus VideoENC_ENC::Reset(mfxVideoParam *par)
{
    Close();
    return Init(par);
}

mfxStatus VideoENC_ENC::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    for (mfxU32 i = 0; i < par->NumExtParam; i++)
    {
        if (mfxExtBuffer * buf = MfxEncENC::GetExtBuffer(m_video.ExtParam, m_video.NumExtParam, par->ExtParam[i]->BufferId))
        {
            MFX_INTERNAL_CPY(par->ExtParam[i], buf, par->ExtParam[i]->BufferSz);
        }
        else
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    mfxExtBuffer ** ExtParam = par->ExtParam;
    mfxU16    NumExtParam = par->NumExtParam;

    MFX_INTERNAL_CPY(par, &(static_cast<mfxVideoParam &>(m_video)), sizeof(mfxVideoParam));

    par->ExtParam    = ExtParam;
    par->NumExtParam = NumExtParam;

    return MFX_ERR_NONE;
}

mfxStatus VideoENC_ENC::RunFrameVmeENCCheck(
                    mfxENCInput *input,
                    mfxENCOutput *output,
                    MFX_ENTRY_POINT pEntryPoints[],
                    mfxU32 &numEntryPoints)
{
    MFX_CHECK(m_bInit, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR2(input, output);

    //set frame type
    mfxU8 mtype_first_field = 0, mtype_second_field = 0;

    if ( ((0 == input->NumFrameL0)  && (0 == input->NumFrameL1)) ||
        (0 == input->InSurface->Data.FrameOrder))
        mtype_first_field = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;
    else if ((0 != input->NumFrameL0)  && (0 == input->NumFrameL1))
        mtype_first_field = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
    else if ((0 != input->NumFrameL0)  || (0 != input->NumFrameL1))
        mtype_first_field = MFX_FRAMETYPE_B;

    mtype_second_field = mtype_first_field;

    if (MFX_PICSTRUCT_PROGRESSIVE != m_video.mfx.FrameInfo.PicStruct)
    {
        /* !!!
         * Actually here is an issue
         * Ref list for second field is different from ref list from first field
         * BUT in (mfxENCInput *input) described only one list, which is same for
         * first and second field.
         * */
        if ( ((0 == input->NumFrameL0)  && (0 == input->NumFrameL1)) ||
            (0 == input->InSurface->Data.FrameOrder))
            mtype_second_field = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;
        else if ((0 != input->NumFrameL0)  && (0 == input->NumFrameL1))
            mtype_second_field = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        else if ((0 != input->NumFrameL0)  || (0 != input->NumFrameL1))
            mtype_second_field = MFX_FRAMETYPE_B;
        /* WA for IP pair */
        if (0 == input->InSurface->Data.FrameOrder)
        {
                mtype_second_field = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                //mtype_second_field = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;
        }
    }

    /* New way for Frame type definition */
    /* WA !!! */
    mfxU32 fieldMaxCount = (m_video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;
    for (mfxU32 field = 0; field < fieldMaxCount; field++)
    {
        mfxU32 fieldParity = (m_video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF)? (1 - field) : field;

        mfxExtFeiSliceHeader * extFeiSliceInRintime = GetExtBufferFEI(input, fieldParity);

        if (!extFeiSliceInRintime) continue; /* cannot extract type for this field in runtime without SliceHeader */

        mfxExtFeiPPS* extFeiPPSinRuntime = GetExtBufferFEI(input, fieldParity);
        /*And runtime params has priority before iInit() params */

        bool is_reference_B_field  = extFeiPPSinRuntime && extFeiPPSinRuntime->ReferencePicFlag;
        bool is_reference_IP_field = !extFeiPPSinRuntime || is_reference_B_field;
        /* each I without PPS is IDR */
        /* TODO: take this information from GOP structure, if no PPS provided */
        bool is_IDR_field = !extFeiPPSinRuntime || extFeiPPSinRuntime->IDRPicFlag;

        mfxU8 type = 0, reference_IP_flag = is_reference_IP_field ? MFX_FRAMETYPE_REF : 0,
                        reference_B_flag  = is_reference_B_field  ? MFX_FRAMETYPE_REF : 0,
                        IDR_flag = is_IDR_field ? MFX_FRAMETYPE_IDR : 0;

        mfxU16 numL0Active = extFeiSliceInRintime->Slice[0].NumRefIdxL0Active,
               numL1Active = extFeiSliceInRintime->Slice[0].NumRefIdxL1Active;

        if      ((numL0Active == 0) && (numL1Active == 0)) type = MFX_FRAMETYPE_I | reference_IP_flag | IDR_flag;
        else if ((numL0Active != 0) && (numL1Active == 0)) type = MFX_FRAMETYPE_P | reference_IP_flag;
        else                                               type = MFX_FRAMETYPE_B | reference_B_flag;

        if (fieldParity == 0)
        {
            mtype_first_field  = type;
        }
        else
        {
            mtype_second_field = type;
        }
    }

    bool first_field_is_IDR = mtype_first_field  & MFX_FRAMETYPE_IDR,
        second_field_is_IDR = mtype_second_field & MFX_FRAMETYPE_IDR;

    m_free.front().m_insertSps[0] = first_field_is_IDR;
    m_free.front().m_insertSps[1] = second_field_is_IDR;

    if (first_field_is_IDR || second_field_is_IDR)
    {
        m_free.front().m_frameOrderIdr = input->InSurface->Data.FrameOrder;
    }

    if ((mtype_first_field & MFX_FRAMETYPE_I) || (mtype_second_field & MFX_FRAMETYPE_I))
    {
        m_free.front().m_frameOrderI = input->InSurface->Data.FrameOrder;
    }

    /* By default insert PSS for each frame */
    m_free.front().m_insertPps[0] = 1;
    m_free.front().m_insertPps[1] = 1;

    UMC::AutomaticUMCMutex guard(m_listMutex);

    m_free.front().m_yuv = input->InSurface;
    //m_free.front().m_ctrl = 0;
    m_free.front().m_type = Pair<mfxU8>(mtype_first_field, mtype_second_field);

    m_free.front().m_extFrameTag = input->InSurface->Data.FrameOrder;
    m_free.front().m_frameOrder  = input->InSurface->Data.FrameOrder;
    m_free.front().m_timeStamp   = input->InSurface->Data.TimeStamp;
    m_free.front().m_userData.resize(2);
    m_free.front().m_userData[0] = input;
    m_free.front().m_userData[1] = output;

    m_incoming.splice(m_incoming.end(), m_free, m_free.begin());
    DdiTask& task = m_incoming.front();
    task.m_picStruct    = GetPicStruct(m_video, task);
    task.m_fieldPicFlag = task.m_picStruct[ENC] != MFX_PICSTRUCT_PROGRESSIVE;
    task.m_fid[0]       = task.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF;
    task.m_fid[1]       = task.m_fieldPicFlag - task.m_fid[0];

    if (task.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF)
    {
        std::swap(task.m_type.top, task.m_type.bot);
        std::swap(task.m_insertSps[0],task.m_insertSps[1]);
    }

    m_core->IncreaseReference(&input->InSurface->Data);

    pEntryPoints[0].pState               = this;
    pEntryPoints[0].pParam               = &m_incoming.front();
    pEntryPoints[0].pCompleteProc        = 0;
    pEntryPoints[0].pGetSubTaskProc      = 0;
    pEntryPoints[0].pCompleteSubTaskProc = 0;
    pEntryPoints[0].requiredNumThreads   = 1;
    pEntryPoints[0].pRoutineName         = "AsyncRoutine";
    pEntryPoints[0].pRoutine             = AsyncRoutine;
    pEntryPoints[1] = pEntryPoints[0];
    pEntryPoints[1].pRoutineName         = "Async Query";
    pEntryPoints[1].pRoutine             = AsyncQuery;
    pEntryPoints[1].pParam               = &m_incoming.front();

    numEntryPoints = 2;

    return MFX_ERR_NONE;
} 

static mfxStatus CopyRawSurfaceToVideoMemory(VideoCORE &    core,
                                        MfxHwH264Encode::MfxVideoParam const & video,
                                        mfxFrameSurface1 *  src_sys,
                                        mfxMemId            dst_d3d,
                                        mfxHDL&             handle)
{
    mfxExtOpaqueSurfaceAlloc const * extOpaq = GetExtBuffer(video);
    /* KW fix */
    MFX_CHECK(extOpaq, MFX_ERR_NOT_FOUND);

    mfxFrameData d3dSurf = {0};

    if (video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
    {
        mfxFrameData sysSurf = src_sys->Data;
        d3dSurf.MemId = dst_d3d;

        MfxHwH264Encode::FrameLocker lock2(&core, sysSurf, true);

        MFX_CHECK_NULL_PTR1(sysSurf.Y)
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "Copy input (sys->d3d)");
            MFX_CHECK_STS(MfxHwH264Encode::CopyFrameDataBothFields(&core, d3dSurf, sysSurf, video.mfx.FrameInfo));
        }

        MFX_CHECK_STS(lock2.Unlock());
    }
    else
    {
        d3dSurf.MemId =  src_sys->Data.MemId;
    }

    if (video.IOPattern != MFX_IOPATTERN_IN_OPAQUE_MEMORY) 
       MFX_CHECK_STS(core.GetExternalFrameHDL(d3dSurf.MemId, &handle))
    else
       MFX_CHECK_STS(core.GetFrameHDL(d3dSurf.MemId, &handle));
    
    return MFX_ERR_NONE;
}


mfxStatus VideoENC_ENC::Close(void)
{
    MFX_CHECK(m_bInit, MFX_ERR_NONE);
    m_bInit = false;
    m_ddi->Destroy();

    m_core->FreeFrames(&m_rec);
    //m_core->FreeFrames(&m_opaqHren);

    return MFX_ERR_NONE;
} 

#endif  // if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)
#endif  // MFX_VA

/* EOF */
