/*//////////////////////////////////////////////////////////////////////////////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2015 Intel Corporation. All Rights Reserved.
//
*/

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
        id == MFX_EXTBUFF_DUMP                      ||
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
        id == MFX_EXTBUFF_ENCODER_ROI ||
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
}

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

mfxStatus VideoENC_ENC::RunFrameVmeENC(mfxENCInput *in, mfxENCOutput *out)
{
    mfxExtSpsHeader const &         extSps         = GetExtBufferRef(m_video);
    mfxU32 const FRAME_NUM_MAX = 1 << (extSps.log2MaxFrameNumMinus4 + 4);

    mfxStatus sts = MFX_ERR_NONE;
    DdiTask & task = m_incoming.front();

    mfxU32 prevsfid         = m_prevTask.m_fid[1];
    mfxU8  idrPicFlag       = !!(task.GetFrameType() & MFX_FRAMETYPE_IDR);
    mfxU8  intraPicFlag     = !!(task.GetFrameType() & MFX_FRAMETYPE_I);
    mfxU8  prevIdrFrameFlag = !!(m_prevTask.GetFrameType() & MFX_FRAMETYPE_IDR);
    mfxU8  prevRefPicFlag   = !!(m_prevTask.GetFrameType() & MFX_FRAMETYPE_REF);
    mfxU8  prevIdrPicFlag   = !!(m_prevTask.m_type[prevsfid] & MFX_FRAMETYPE_IDR);

    task.m_frameOrderIdr = idrPicFlag ? task.m_frameOrder : m_prevTask.m_frameOrderIdr;
    task.m_frameOrderI   = intraPicFlag ? task.m_frameOrder : m_prevTask.m_frameOrderI;
    task.m_encOrder      = m_prevTask.m_encOrder + 1;
    task.m_encOrderIdr   = prevIdrFrameFlag ? m_prevTask.m_encOrder : m_prevTask.m_encOrderIdr;

    task.m_statusReportNumber[0] = 2 * task.m_encOrder;
    task.m_statusReportNumber[1] = 2 * task.m_encOrder + 1;

    task.m_frameNum = mfxU16((m_prevTask.m_frameNum + prevRefPicFlag) % FRAME_NUM_MAX);
    if (idrPicFlag)
        task.m_frameNum = 0;

    task.m_picNum[0] = task.m_frameNum * (task.m_fieldPicFlag + 1) + task.m_fieldPicFlag;
    task.m_picNum[1] = task.m_picNum[0];

    task.m_idrPicId = m_prevTask.m_idrPicId + idrPicFlag;

    task.m_idx    = FindFreeResourceIndex(m_raw);
    task.m_midRaw = AcquireResource(m_raw, task.m_idx);
    task.m_numSlice = {m_video.mfx.NumSlice, m_video.mfx.NumSlice};
    task.m_cqpValue[0] = GetQpValue(m_video, task.m_ctrl, task.m_type[0]);
    task.m_cqpValue[1] = GetQpValue(m_video, task.m_ctrl, task.m_type[1]);

    sts = GetNativeHandleToRawSurface(*m_core, m_video, task, task.m_handleRaw);
    if (sts != MFX_ERR_NONE)
         return Error(sts);

    sts = CopyRawSurfaceToVideoMemory(*m_core, m_video, task);
    if (sts != MFX_ERR_NONE)
          return Error(sts);

    for (mfxU32 f = 0; f <= task.m_fieldPicFlag; f++)
    {
        //fprintf(stderr,"handle=0x%x mid=0x%x\n", task.m_handleRaw, task.m_midRaw );
        sts = m_ddi->Execute(task.m_handleRaw.first, task, task.m_fid[f], m_sei);
        if (sts != MFX_ERR_NONE)
                 return Error(sts);
    }

    m_prevTask = task;

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
    
    for (mfxU32 f = 0; f <= task.m_fieldPicFlag; f++)
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
    {
        m_free.splice(m_free.end(), m_incoming, it);
    }else
        return MFX_ERR_NOT_FOUND;

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
    : m_bInit( false )
    , m_core( core )
    , m_prevTask()
{
    *sts = MFX_ERR_NONE;
} 


VideoENC_ENC::~VideoENC_ENC()
{
    Close();

} 

mfxStatus VideoENC_ENC::Init(mfxVideoParam *par)
{
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
    request.Type = MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;
    request.NumFrameMin = m_video.mfx.GopRefDist*2;
    request.NumFrameSuggested = request.NumFrameMin + m_video.AsyncDepth;
    request.AllocId = par->AllocId;

    sts = m_core->AllocFrames(&request, &m_raw);
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_raw, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

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
    //MFX_CHECK(CheckExtenedBuffer(par), MFX_ERR_UNDEFINED_BEHAVIOR);
    return MFX_ERR_NONE;
}

mfxStatus VideoENC_ENC::RunFrameVmeENCCheck(
                    mfxENCInput *input, 
                    mfxENCOutput *output,
                    MFX_ENTRY_POINT pEntryPoints[],
                    mfxU32 &numEntryPoints)
{
    MFX_CHECK( m_bInit, MFX_ERR_UNDEFINED_BEHAVIOR);

    //set frame type
    mfxU8 mtype = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
    if (input->NumFrameL0 > 0) {
        mtype = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        if (input->NumFrameL1 > 0)
            mtype = MFX_FRAMETYPE_B;
    }

    if (mtype & MFX_FRAMETYPE_IDR)
    {
        m_free.front().m_frameOrderIdr = input->InSurface->Data.FrameOrder;
        /* Need to insert SPS for IDR frame */
        m_free.front().m_insertSps[0] = 1;
        m_free.front().m_insertSps[1] = 0;
    }

    if (mtype & MFX_FRAMETYPE_I)
    {
        m_free.front().m_frameOrderI = input->InSurface->Data.FrameOrder;
    }

    /* By default insert PSS for each frame */
    m_free.front().m_insertPps[0] = 1;
    m_free.front().m_insertPps[1] = 1;

    UMC::AutomaticUMCMutex guard(m_listMutex);

    m_free.front().m_yuv = input->InSurface;
    //m_free.front().m_ctrl = 0;
    m_free.front().m_type = Pair<mfxU8>(mtype, mtype); //ExtendFrameType(ctrl->FrameType);

    m_free.front().m_extFrameTag = input->InSurface->Data.FrameOrder;
    m_free.front().m_frameOrder = input->InSurface->Data.FrameOrder;
    m_free.front().m_timeStamp = input->InSurface->Data.TimeStamp;
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
        std::swap(task.m_type.top, task.m_type.bot);

    m_core->IncreaseReference(&input->InSurface->Data);

    pEntryPoints[0].pState = this;
    pEntryPoints[0].pParam = &task;
    pEntryPoints[0].pCompleteProc = 0;
    pEntryPoints[0].pGetSubTaskProc = 0;
    pEntryPoints[0].pCompleteSubTaskProc = 0;
    pEntryPoints[0].requiredNumThreads = 1;
    pEntryPoints[0].pRoutineName = "AsyncRoutine";
    pEntryPoints[0].pRoutine = AsyncRoutine;
    pEntryPoints[1] = pEntryPoints[0];
    pEntryPoints[1].pRoutineName = "Async Query";
    pEntryPoints[1].pRoutine = AsyncQuery;
    pEntryPoints[1].pParam = &task;

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
        video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
    {
        mfxFrameData sysSurf = src_sys->Data;
        d3dSurf.MemId = dst_d3d;

        MfxHwH264Encode::FrameLocker lock2(&core, sysSurf, true);

        MFX_CHECK_NULL_PTR1(sysSurf.Y)
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Copy input (sys->d3d)");
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

    m_core->FreeFrames(&m_raw);
    //m_core->FreeFrames(&m_opaqHren);

    return MFX_ERR_NONE;
} 

#endif  
#endif  // MFX_VA

/* EOF */

