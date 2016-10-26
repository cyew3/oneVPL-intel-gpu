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

//#if defined (MFX_VA_LINUX)
    #include "mfx_h264_encode_vaapi.h"
//#endif

#if defined(_DEBUG)
#define mdprintf fprintf
#else
#define mdprintf(...)
#endif

//using namespace MfxEncPREENC;

namespace MfxEncENC{
static mfxU16 GetGopSize(mfxVideoParam *par)
{
    return  par->mfx.GopPicSize == 0 ?  500 : par->mfx.GopPicSize ;
}
static mfxU16 GetRefDist(mfxVideoParam *par)
{
    mfxU16 GopSize = GetGopSize(par);
    mfxU16 refDist = par->mfx.GopRefDist == 0 ?  2 : par->mfx.GopRefDist;
    return refDist <= GopSize ? refDist: GopSize;
}
static mfxU16 GetAsyncDeph(mfxVideoParam *par)
{
    return par->AsyncDepth == 0 ? 3 : par->AsyncDepth;
}

static bool IsVideoParamExtBufferIdSupported(mfxU32 id)
{
    return
        id == MFX_EXTBUFF_CODING_OPTION             ||
        id == MFX_EXTBUFF_CODING_OPTION_SPSPPS      ||
        id == MFX_EXTBUFF_DDI                       ||
#ifdef MFX_UNDOCUMENTED_DUMP_FILES
        id == MFX_EXTBUFF_DUMP                      ||
#endif
        id == MFX_EXTBUFF_PAVP_OPTION               ||
        id == MFX_EXTBUFF_MVC_SEQ_DESC              ||
        id == MFX_EXTBUFF_VIDEO_SIGNAL_INFO         ||
        id == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION ||
        id == MFX_EXTBUFF_PICTURE_TIMING_SEI        ||
        id == MFX_EXTBUFF_AVC_TEMPORAL_LAYERS       ||
        id == MFX_EXTBUFF_CODING_OPTION2            ||
        id == MFX_EXTBUFF_SVC_SEQ_DESC              ||
        id == MFX_EXTBUFF_SVC_RATE_CONTROL          ||
        id == MFX_EXTBUFF_ENCODER_RESET_OPTION      ||
        id == MFX_EXTBUFF_ENCODER_CAPABILITY        ||
        id == MFX_EXTBUFF_ENCODER_WIDI_USAGE        ||
        id == MFX_EXTBUFF_ENCODER_ROI               ||
        id == MFX_EXTBUFF_FEI_PARAM;
}

mfxExtBuffer* GetExtBuffer(mfxExtBuffer** ebuffers, mfxU32 nbuffers, mfxU32 BufferId)
{
    if (!ebuffers) return 0;
    for(mfxU32 i=0; i<nbuffers; i++) {
        if (!ebuffers[i]) continue;
        if (ebuffers[i]->BufferId == BufferId) {
            return ebuffers[i];
        }
    }
    return 0;
}

static mfxStatus CheckExtBufferId(mfxVideoParam const & par)
{
    for (mfxU32 i = 0; i < par.NumExtParam; i++)
    {
        if (par.ExtParam[i] == 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (!MfxEncENC::IsVideoParamExtBufferIdSupported(par.ExtParam[i]->BufferId))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        // check if buffer presents twice in video param
        if (MfxEncENC::GetExtBuffer(
            par.ExtParam + i + 1,
            par.NumExtParam - i - 1,
            par.ExtParam[i]->BufferId) != 0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    return MFX_ERR_NONE;
}

static mfxU16  GetFrameType(mfxU32 frameOrder, int gopSize, int gopRefDist)
{

    if (frameOrder % gopSize == 0)
    {
        return MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|((frameOrder == 0) ? MFX_FRAMETYPE_IDR : 0);
    }
    else if ((frameOrder % gopSize) % gopRefDist == 0)
    {
        return MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF;
    }
    return MFX_FRAMETYPE_B;
}

static bool CheckExtenedBuffer(mfxVideoParam *par)
{
    mfxU32  BufferId[2] = {MFX_EXTBUFF_LOOKAHEAD_CTRL, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION};
    mfxU32  num = sizeof(BufferId)/sizeof(BufferId[0]);
    if (!par->ExtParam) return true;

    for (mfxU32 i = 0; i < par->NumExtParam; i++)
    {
        mfxU32 j = 0;
        for (; j < num; j++)
        {
            if (par->ExtParam[i] != 0 && par->ExtParam[i]->BufferId == BufferId[j]) break;
        }
        if (j == num) return false;
    }
    return true;
}
static
mfxStatus GetNativeSurface(
    VideoCORE & core,
    MfxHwH264Encode::MfxVideoParam const & video,
    mfxFrameSurface1 *  opaq_surface,
    mfxFrameSurface1 ** native_surface)
{
    mfxStatus sts = MFX_ERR_NONE;
   *native_surface = opaq_surface;

    if (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        *native_surface = core.GetNativeSurface(opaq_surface);
        MFX_CHECK_NULL_PTR1(*native_surface);

        (*native_surface)->Info            = opaq_surface->Info;
        (*native_surface)->Data.TimeStamp  = opaq_surface->Data.TimeStamp;
        (*native_surface)->Data.FrameOrder = opaq_surface->Data.FrameOrder;
        (*native_surface)->Data.Corrupted  = opaq_surface->Data.Corrupted;
        (*native_surface)->Data.DataFlag   = opaq_surface->Data.DataFlag;
    }
    return sts;
}

static
mfxStatus GetOpaqSurface(
    VideoCORE & core,
    MfxHwH264Encode::MfxVideoParam const & video,
    mfxFrameSurface1 *native_surface,
    mfxFrameSurface1 ** opaq_surface)
{
    mfxStatus sts = MFX_ERR_NONE;

   *opaq_surface = native_surface;

    if (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        *opaq_surface = core.GetOpaqSurface(native_surface->Data.MemId);
        MFX_CHECK_NULL_PTR1(*opaq_surface);

        (*opaq_surface)->Info            = native_surface->Info;
        (*opaq_surface)->Data.TimeStamp  = native_surface->Data.TimeStamp;
        (*opaq_surface)->Data.FrameOrder = native_surface->Data.FrameOrder;
        (*opaq_surface)->Data.Corrupted  = native_surface->Data.Corrupted;
        (*opaq_surface)->Data.DataFlag   = native_surface->Data.DataFlag;
    }
    return sts;
}
} // namespace MfxEncENC

bool bEnc_ENC(mfxVideoParam *par)
{
    mfxExtFeiParam *pControl = NULL;
//    mfxExtFeiParam *pControl = (mfxExtFeiParam *) GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_FEI_PARAM);
    for (mfxU16 i = 0; i < par->NumExtParam; i++)
        if (par->ExtParam[i] != 0 && par->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM){ // assuming aligned buffers
            pControl = (mfxExtFeiParam *)par->ExtParam[i];
            break;
        }

    bool res = pControl ? (pControl->Func == MFX_FEI_FUNCTION_ENC) : false;
    return res;
}

static mfxStatus AsyncRoutine(void * state, void * param, mfxU32, mfxU32)
{
    VideoENC_ENC & impl = *(VideoENC_ENC *)state;
    return  impl.RunFrameVmeENC(0,0);
}

void TEMPORAL_HACK_WITH_DPB(
    ArrayDpbFrame &             dpb,
    mfxMemId const *            mids,
    std::vector<mfxU32> const & fo);

mfxStatus VideoENC_ENC::RunFrameVmeENC(mfxENCInput *in, mfxENCOutput *out)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoENC_ENC::RunFrameVmeENC");
    mfxExtSpsHeader const &         extSps         = GetExtBufferRef(m_video);
    mfxU32 const FRAME_NUM_MAX = 1 << (extSps.log2MaxFrameNumMinus4 + 4);

    mfxStatus sts = MFX_ERR_NONE;
    DdiTask & task = m_incoming.front();
    mfxU32 f = 0, f_start = 0;
    mfxU32 fieldCount = task.m_fieldPicFlag;


//    task.m_picStruct    = GetPicStruct(m_video, task);
//    task.m_fieldPicFlag = task.m_picStruct[ENC] != MFX_PICSTRUCT_PROGRESSIVE;
//    task.m_fid[0]       = task.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF;
//    task.m_fid[1]       = task.m_fieldPicFlag - task.m_fid[0];
//
//    mfxU32 prevsfid         = m_prevTask.m_fid[1];
//    mfxU8  idrPicFlag       = !!(task.GetFrameType() & MFX_FRAMETYPE_IDR);
//    mfxU8  intraPicFlag     = !!(task.GetFrameType() & MFX_FRAMETYPE_I);
//    mfxU8  prevIdrFrameFlag = !!(m_prevTask.GetFrameType() & MFX_FRAMETYPE_IDR);
//    mfxU8  prevRefPicFlag   = !!(m_prevTask.GetFrameType() & MFX_FRAMETYPE_REF);
//    mfxU8  prevIdrPicFlag   = !!(m_prevTask.m_type[prevsfid] & MFX_FRAMETYPE_IDR);
//
//    task.m_frameOrderIdr = idrPicFlag ? task.m_frameOrder : m_prevTask.m_frameOrderIdr;
//    task.m_frameOrderI   = intraPicFlag ? task.m_frameOrder : m_prevTask.m_frameOrderI;
//    task.m_encOrder      = m_prevTask.m_encOrder + 1;
//    task.m_encOrderIdr   = prevIdrFrameFlag ? m_prevTask.m_encOrder : m_prevTask.m_encOrderIdr;
//
//    task.m_statusReportNumber[0] = 2 * task.m_encOrder;
//    task.m_statusReportNumber[1] = 2 * task.m_encOrder + 1;
//
//    task.m_frameNum = mfxU16((m_prevTask.m_frameNum + prevRefPicFlag) % FRAME_NUM_MAX);
//    if (idrPicFlag)
//        task.m_frameNum = 0;
//
//    task.m_picNum[0] = task.m_frameNum * (task.m_fieldPicFlag + 1) + task.m_fieldPicFlag;
//    task.m_picNum[1] = task.m_picNum[0];
//
//    task.m_idrPicId = m_prevTask.m_idrPicId + idrPicFlag;
//
//    task.m_idx    = FindFreeResourceIndex(m_rec);
//    task.m_midRaw = AcquireResource(m_rec, task.m_idx);
//    task.m_numSlice = {m_video.mfx.NumSlice, m_video.mfx.NumSlice};
//    task.m_cqpValue[0] = GetQpValue(m_video, task.m_ctrl, task.m_type[0]);
//    task.m_cqpValue[1] = GetQpValue(m_video, task.m_ctrl, task.m_type[1]);

    sts = GetNativeHandleToRawSurface(*m_core, m_video, task, task.m_handleRaw);
    if (sts != MFX_ERR_NONE)
         return Error(sts);

//    sts = CopyRawSurfaceToVideoMemory(*m_core, m_video, task);
//    if (sts != MFX_ERR_NONE)
//          return Error(sts);
    /* Passing deblocking params */
    mfxENCInput* inParams = (mfxENCInput*)task.m_userData[0];
    mfxENCOutput* outParams = (mfxENCOutput*)task.m_userData[1];
    mfxExtCodingOption2 const *   extOpt2        = GetExtBuffer(m_video);
    mfxExtCodingOption2 const *   extOpt2Runtime = GetExtBuffer(task.m_ctrl);
    const mfxExtCodingOption2* extOpt2Cur = (extOpt2Runtime ? extOpt2Runtime : extOpt2);
    mfxU32 fieldMaxCount = (m_video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;
    for (mfxU32 field = 0; field < fieldMaxCount; field++)
    {
        mfxU32 fieldParity = (m_video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF)? (1 - field) : field;

        mfxExtFeiSliceHeader * extFeiSlice = GetExtBuffer(m_video, fieldParity);
        /* To change de-blocking params in runtime we need to take params from runtime control */
        mfxExtFeiSliceHeader * extFeiSliceInRintime = GetExtBufferFEI(inParams, fieldParity);
        /*And runtime params has priority before iInit() params */
        if (NULL != extFeiSliceInRintime)
            extFeiSlice = extFeiSliceInRintime;

        for (size_t i = 0; i < GetMaxNumSlices(m_video); i++)
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
            task.m_disableDeblockingIdc[field].push_back(disableDeblockingIdc);
            task.m_sliceAlphaC0OffsetDiv2[field].push_back(sliceAlphaC0OffsetDiv2);
            task.m_sliceBetaOffsetDiv2[field].push_back(sliceBetaOffsetDiv2);
        } // for (size_t i = 0; i < GetMaxNumSlices(video); i++)
    } // for (mfxU32 field = 0; field < fieldMaxCount; field++)

//    mfxHDL handle_src, handle_rec;
//    sts = m_core->GetExternalFrameHDL(inParams->InSurface->Data.MemId, &handle_src);
//    MFX_CHECK(MFX_ERR_NONE == sts, MFX_ERR_INVALID_HANDLE);
//    for(mfxU32 i = 0; i < m_rec.NumFrameActual; i++)
//    {
//        task.m_midRec    = AcquireResource(m_rec, i);
//        sts = m_core->GetFrameHDL(m_rec.mids[i], &handle_rec);
//        MFX_CHECK(MFX_ERR_NONE == sts, MFX_ERR_INVALID_HANDLE);
//        mfxU32* src_surf_id = (mfxU32 * )handle_src;
//        mfxU32* rec_surf_id = (mfxU32 * )handle_rec;
//        if ((*src_surf_id) == (*rec_surf_id))
//        {
//            task.m_idxRecon = i;
//            break;
//        }
//        else
//            ReleaseResource(m_rec, task.m_midRec);
//    }

//    task.m_idxRecon = FindFreeResourceIndex(m_rec);
//    task.m_midRec    = AcquireResource(m_rec, task.m_idxRecon);

        mfxHDL handle_src, handle_rec;
        sts = m_core->GetExternalFrameHDL(outParams->OutSurface->Data.MemId, &handle_src);
        MFX_CHECK(MFX_ERR_NONE == sts, MFX_ERR_INVALID_HANDLE);
        for(mfxU32 i = 0; i < m_rec.NumFrameActual; i++)
        {
            task.m_midRec    = AcquireResource(m_rec, i);
            sts = m_core->GetFrameHDL(m_rec.mids[i], &handle_rec);
            MFX_CHECK(MFX_ERR_NONE == sts, MFX_ERR_INVALID_HANDLE);
            mfxU32* src_surf_id = (mfxU32 * )handle_src;
            mfxU32* rec_surf_id = (mfxU32 * )handle_rec;
            if ((*src_surf_id) == (*rec_surf_id))
            {
                task.m_idxRecon = i;
                break;
            }
            else
                ReleaseResource(m_rec, task.m_midRec);
        }

    //!!! HACK !!!
    m_recFrameOrder[task.m_idxRecon] = task.m_frameOrder;
    TEMPORAL_HACK_WITH_DPB(task.m_dpb[0],          m_rec.mids, m_recFrameOrder);
    TEMPORAL_HACK_WITH_DPB(task.m_dpb[1],          m_rec.mids, m_recFrameOrder);
    TEMPORAL_HACK_WITH_DPB(task.m_dpbPostEncoding, m_rec.mids, m_recFrameOrder);

    MfxHwH264Encode::ConfigureTask(task, m_prevTask, m_video, false);

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
    {
        fieldCount = f_start = m_firstFieldDone; // 0 or 1
    }

    for (f = f_start; f <= fieldCount; f++)
    {
        //fprintf(stderr,"handle=0x%x mid=0x%x\n", task.m_handleRaw, task.m_midRaw );
        sts = m_ddi->Execute(task.m_handleRaw.first, task, task.m_fid[f], m_sei);
        if (sts != MFX_ERR_NONE)
                 return Error(sts);
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

    mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
    mfxExtFeiEncFrameCtrl* feiCtrl = GetExtBufferFEI(in, 0);

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
    {
        f_start = fieldCount = 1 - m_firstFieldDone;
    }

    for (f = f_start; f <= fieldCount; f++)
    {
        sts = m_ddi->QueryStatus(task, task.m_fid[f]);
        if (sts == MFX_WRN_DEVICE_BUSY)
            return MFX_TASK_BUSY;
        if (sts != MFX_ERR_NONE)
            return Error(sts);
    }

    mfxENCInput *input = (mfxENCInput *)task.m_userData[0];
    m_core->DecreaseReference(&(input->InSurface->Data));

    UMC::AutomaticUMCMutex guard(m_listMutex);
    //move that task to free tasks from m_incoming
    //m_incoming
    std::list<DdiTask>::iterator it = std::find(m_incoming.begin(), m_incoming.end(), task);
    if(it != m_incoming.end())
        m_free.splice(m_free.end(), m_incoming, it);
    else
        return MFX_ERR_NOT_FOUND;

//    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
//        m_firstFieldDone = MFX_CODINGOPTION_UNKNOWN;

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

    if (par->mfx.NumRefFrame > 4)
    {
        m_video.mfx.NumRefFrame = tmp.mfx.NumRefFrame = 4;
    }

    sts = ReadSpsPpsHeaders(tmp);
    MFX_CHECK_STS(sts);

    sts = CheckWidthAndHeight(tmp);
    MFX_CHECK_STS(sts);

    sts = CopySpsPpsToVideoParam(tmp);
    if (sts < MFX_ERR_NONE)
        return sts;

    m_ddi.reset( new MfxHwH264Encode::VAAPIFEIENCEncoder );
    if (m_ddi.get() == 0)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = m_ddi->CreateAuxilliaryDevice(
        m_core,
        DXVA2_Intel_Encode_AVC,
        GetFrameWidth(m_video),
        GetFrameHeight(m_video));
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    const mfxExtFeiParam* params = GetExtBuffer(m_video);
    if (NULL == params)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else
    {
        if ((MFX_CODINGOPTION_ON == params->SingleFieldProcessing) &&
             ((MFX_PICSTRUCT_FIELD_TFF == m_video.mfx.FrameInfo.PicStruct) ||
              (MFX_PICSTRUCT_FIELD_BFF == m_video.mfx.FrameInfo.PicStruct)) )
            m_singleFieldProcessingMode = MFX_CODINGOPTION_ON;
    }

    sts = m_ddi->QueryEncodeCaps(m_caps);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    m_currentPlatform = m_core->GetHWType();
    m_currentVaType   = m_core->GetVAType();

    mfxStatus spsppsSts = MfxHwH264Encode::CopySpsPpsToVideoParam(m_video);

    mfxStatus checkStatus = MfxHwH264Encode::CheckVideoParam(m_video, m_caps, m_core->IsExternalFrameAllocator(), m_currentPlatform, m_currentVaType);
    if (checkStatus == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_WRN_PARTIAL_ACCELERATION;
    else if (checkStatus < MFX_ERR_NONE)
        return checkStatus;
    else if (checkStatus == MFX_ERR_NONE)
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
    request.Type = MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;
    request.NumFrameMin = m_video.mfx.GopRefDist * 2 + (m_video.AsyncDepth-1) + 1 + m_video.mfx.NumRefFrame + 1;
    request.NumFrameSuggested = request.NumFrameMin;
    request.AllocId = par->AllocId;

    //sts = m_core->AllocFrames(&request, &m_rec);
    sts = m_rec.Alloc(m_core,request, false);
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_rec, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    m_recFrameOrder.resize(request.NumFrameMin, 0xffffffff);

    sts = m_ddi->CreateAccelerationService(m_video);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    m_inputFrameType =
        m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY || m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY
            ? MFX_IOPATTERN_IN_SYSTEM_MEMORY
            : MFX_IOPATTERN_IN_VIDEO_MEMORY;


    m_free.resize(m_video.AsyncDepth);
    m_incoming.clear();

    m_bInit = true;
    return sts;
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

    if ((NULL == input) || (NULL == output))
        return MFX_ERR_NULL_PTR;

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
        bool is_IDR_field = !extFeiPPSinRuntime || (extFeiPPSinRuntime && extFeiPPSinRuntime->IDRPicFlag);

        mfxU8 type = 0, reference_IP_flag = is_reference_IP_field ? MFX_FRAMETYPE_REF : 0,
            reference_B_flag = is_reference_B_field ? MFX_FRAMETYPE_REF : 0,
            IDR_flag = is_IDR_field ? MFX_FRAMETYPE_IDR : 0;

        mfxU16 numL0Active = extFeiSliceInRintime->Slice[0].NumRefIdxL0Active,
               numL1Active = extFeiSliceInRintime->Slice[0].NumRefIdxL1Active;

        if ((numL0Active == 0) && (numL1Active == 0))
            type = MFX_FRAMETYPE_I | reference_IP_flag | IDR_flag;
        else if ((0 != numL0Active) && (0 == numL1Active))
            type = MFX_FRAMETYPE_P | reference_IP_flag;
        else if ((0 != numL0Active) || (0 != numL1Active))
            type = MFX_FRAMETYPE_B | reference_B_flag;

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

    pEntryPoints[0].pState = this;
    pEntryPoints[0].pParam = &m_incoming.front();
    pEntryPoints[0].pCompleteProc = 0;
    pEntryPoints[0].pGetSubTaskProc = 0;
    pEntryPoints[0].pCompleteSubTaskProc = 0;
    pEntryPoints[0].requiredNumThreads = 1;
    pEntryPoints[0].pRoutineName = "AsyncRoutine";
    pEntryPoints[0].pRoutine = AsyncRoutine;
    pEntryPoints[1] = pEntryPoints[0];
    pEntryPoints[1].pRoutineName = "Async Query";
    pEntryPoints[1].pRoutine = AsyncQuery;
    pEntryPoints[1].pParam = &m_incoming.front();

    numEntryPoints = 2;

    return MFX_ERR_NONE;
} 

static mfxStatus CopyRawSurfaceToVideoMemory(  VideoCORE &  core,
                                        MfxHwH264Encode::MfxVideoParam const & video,
                                        mfxFrameSurface1 *  src_sys, 
                                        mfxMemId            dst_d3d,
                                        mfxHDL&             handle)
{
    mfxExtOpaqueSurfaceAlloc const * extOpaq = GetExtBuffer(video);
    /* KW fix */
    if (NULL == extOpaq)
        return MFX_ERR_NOT_FOUND;

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
    if (!m_bInit) return MFX_ERR_NONE;
    m_bInit = false;
    m_ddi->Destroy();

    m_core->FreeFrames(&m_rec);
    //m_core->FreeFrames(&m_opaqHren);

    return MFX_ERR_NONE;
} 

#endif  // if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)
#endif  // MFX_VA

/* EOF */

