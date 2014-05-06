//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VP8_VIDEO_ENCODE)

#include "mfxdefs.h"
#include "mfx_vp8_encode.h"
#include "mfx_platform_headers.h"
#include "mfx_task.h"
#include "mfx_session.h"
#include "vm_interlocked.h"
#include "mfx_enc_common.h"
#include "mfx_vp8_enc_common.h"
#include "mfx_tools.h"
#include "ipps.h"
#include "umc_defs.h"

#pragma warning(disable:4505)
namespace WEBM_VP8
{
#include "vpx_config.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
}

#include "vm_event.h"
#include "vm_sys_info.h"

#define VP8_ENC_WRITE_IVF_HEADERS

#define VP8_Malloc  ippMalloc
#define VP8_Free    ippFree
#define vpx_interface (WEBM_VP8::vpx_codec_vp8_cx())

#define VP8ENC_UNREFERENCED_PARAMETER(X) X=X

#define CHECK_VERSION(ver)
#define CHECK_CODEC_ID(id, myid)
#define CHECK_FUNCTION_ID(id, myid)

#if defined (__ICL)
//non-pointer conversion from "unsigned __int64" to "Ipp32s={signed int}" may lose significant bits
#pragma warning(disable:2259)
#endif


#define CHECK_OPTION(input, output, corcnt) \
  if ( input != MFX_CODINGOPTION_OFF &&     \
      input != MFX_CODINGOPTION_ON  &&      \
      input != MFX_CODINGOPTION_UNKNOWN ) { \
    output = MFX_CODINGOPTION_UNKNOWN;      \
    (corcnt) ++;                            \
  } else output = input;

#define CHECK_OPTION_RANGE(input, output, lo, hi) \
if (input >= ((lo)+1) && input <= ((hi)+1)) { \
    output = input; \
} else { \
    output = MFX_CODINGOPTION_UNKNOWN; \
    isCorrected ++; \
}

#define SET_OPTION_FLAG(input, output) \
if (input != MFX_CODINGOPTION_UNKNOWN) { \
    output = (input == MFX_CODINGOPTION_ON) ? 1 : 0; \
}

#define SET_OPTION(input, output) \
if (input != MFX_CODINGOPTION_UNKNOWN) { \
    output = input - 1; \
}
#define SET_OPTION_EXT(input, flag) \
if (input != MFX_CODINGOPTION_UNKNOWN ) { \
    arg_ctrls[*arg_ctrl_cnt][0] = flag; \
    arg_ctrls[*arg_ctrl_cnt][1] = input - 1; \
    (*arg_ctrl_cnt)++; \
}


#define CORRECT_FLAG(flag, value, corcnt) \
  if ( flag != 0 && flag != 1 ) { \
    flag = value;      \
    (corcnt) ++; }

#define CHECK_ZERO(input, corcnt) \
  if ( input != 0 ) { \
    input =0 ;      \
    (corcnt) ++; }

#define CHECK_VPX_STATUS(status) \
if ((status) != WEBM_VP8::VPX_CODEC_OK) \
return vp8enc_ConvertStatus(status);

#ifdef VP8_ENC_WRITE_IVF_HEADERS

static void mem_put_le16(mfxU8 *mem, unsigned int val) {
    mem[0] = (mfxU8)(val);
    mem[1] = (mfxU8)(val>>8);
}

static void mem_put_le32(mfxU8 *mem, unsigned int val) {
    mem[0] = (mfxU8)(val);
    mem[1] = (mfxU8)(val>>8);
    mem[2] = (mfxU8)(val>>16);
    mem[3] = (mfxU8)(val>>24);
}

#define fourcc    0x30385056

static void write_ivf_file_header(mfxU8 *header,
                                  const WEBM_VP8::vpx_codec_enc_cfg_t *cfg,
                                  int frame_cnt) {
    if(cfg->g_pass != WEBM_VP8::VPX_RC_ONE_PASS && cfg->g_pass != WEBM_VP8::VPX_RC_LAST_PASS)
        return;
    header[0] = 'D';
    header[1] = 'K';
    header[2] = 'I';
    header[3] = 'F';
    mem_put_le16(header+4,  0);                   /* version */
    mem_put_le16(header+6,  32);                  /* headersize */
    mem_put_le32(header+8,  fourcc);              /* headersize */
    mem_put_le16(header+12, cfg->g_w);            /* width */
    mem_put_le16(header+14, cfg->g_h);            /* height */
    mem_put_le32(header+16, cfg->g_timebase.den); /* rate */
    mem_put_le32(header+20, cfg->g_timebase.num); /* scale */
    mem_put_le32(header+24, frame_cnt);           /* length */
    mem_put_le32(header+28, 0);                   /* unused */
}


static void write_ivf_frame_header(mfxU8 *header,
                                   const WEBM_VP8::vpx_codec_cx_pkt_t *pkt)
{
    WEBM_VP8::vpx_codec_pts_t  pts;

    if(pkt->kind != WEBM_VP8::VPX_CODEC_CX_FRAME_PKT)
        return;

    pts = pkt->data.frame.pts;
    mem_put_le32(header, UINT(pkt->data.frame.sz));
    mem_put_le32(header+4, (mfxU32)(pts&0xFFFFFFFF));
    mem_put_le32(header+8, (mfxU32)(pts >> 32));
}

#endif

#define ARG_CTRL_CNT_MAX 10

static mfxStatus vp8enc_ConvertStatus(mfxI32 status)
{
    switch( status ){
        case WEBM_VP8::VPX_CODEC_OK:
            return MFX_ERR_NONE;
        case WEBM_VP8::VPX_CODEC_ERROR:
            return MFX_ERR_UNKNOWN;
        case WEBM_VP8::VPX_CODEC_MEM_ERROR:
            return MFX_ERR_MEMORY_ALLOC;
        case WEBM_VP8::VPX_CODEC_INVALID_PARAM:
            return MFX_ERR_INVALID_VIDEO_PARAM;
        case WEBM_VP8::VPX_CODEC_ABI_MISMATCH:
        case WEBM_VP8::VPX_CODEC_INCAPABLE:
        case WEBM_VP8::VPX_CODEC_UNSUP_BITSTREAM: // decoder option
        case WEBM_VP8::VPX_CODEC_UNSUP_FEATURE: // decoder option
        case WEBM_VP8::VPX_CODEC_CORRUPT_FRAME: // decoder option
        default:
            return MFX_ERR_UNSUPPORTED;
    }
}

mfxStatus MFXVideoENCODEVP8::InitVpxCfg(mfxHDL _cfg, mfxVideoParam *par, mfxExtCodingOptionVP8 *opts, int (*arg_ctrls)[2], int *arg_ctrl_cnt) 
{
    WEBM_VP8::vpx_codec_enc_cfg_t *cfg = (WEBM_VP8::vpx_codec_enc_cfg_t *)_cfg;
    
    *arg_ctrl_cnt = 0;
    
    cfg->msdk_class_ptr = this;

    cfg->g_profile = par->mfx.CodecProfile - 1;
 
    cfg->g_w = (par->mfx.FrameInfo.CropW)? 
                            par->mfx.FrameInfo.CropW:
                            par->mfx.FrameInfo.Width;
    cfg->g_h = (par->mfx.FrameInfo.CropH)?
                            par->mfx.FrameInfo.CropH:
                            par->mfx.FrameInfo.Height;

    cfg->g_pass = WEBM_VP8::VPX_RC_ONE_PASS;

    cfg->g_timebase.den = 1000;

    cfg->kf_mode = (par->mfx.GopOptFlag & MFX_GOP_STRICT)? 
                            WEBM_VP8::VPX_KF_FIXED:WEBM_VP8::VPX_KF_AUTO;
      
    cfg->kf_min_dist = 0;
    cfg->kf_max_dist = par->mfx.GopPicSize;

    switch(par->mfx.RateControlMethod)
    {
    case MFX_RATECONTROL_CBR:
        cfg->rc_end_usage       = WEBM_VP8::VPX_CBR;
        cfg->rc_target_bitrate  = par->mfx.TargetKbps;
        cfg->rc_buf_sz          = par->mfx.BufferSizeInKB * 8000 / cfg->rc_target_bitrate;
        cfg->rc_buf_initial_sz  = par->mfx.InitialDelayInKB * 8000 / cfg->rc_target_bitrate;
        cfg->rc_buf_optimal_sz  = (cfg->rc_buf_sz + cfg->rc_buf_initial_sz)/2;
        break;
    case MFX_RATECONTROL_VBR:
        cfg->rc_end_usage       = WEBM_VP8::VPX_VBR;
        cfg->rc_target_bitrate  = par->mfx.TargetKbps;
        cfg->rc_buf_sz          = par->mfx.BufferSizeInKB * 8000 / cfg->rc_target_bitrate;
        cfg->rc_buf_initial_sz  = par->mfx.InitialDelayInKB * 8000 / cfg->rc_target_bitrate;
        cfg->rc_buf_optimal_sz  = (cfg->rc_buf_sz + cfg->rc_buf_initial_sz)/2;
        break;
    case MFX_RATECONTROL_CQP:
        cfg->rc_end_usage       = WEBM_VP8::VPX_CQ;
        cfg->rc_max_quantizer   = par->mfx.QPI > par->mfx.QPP ? par->mfx.QPI:par->mfx.QPP;
        cfg->rc_min_quantizer   = par->mfx.QPI < par->mfx.QPP ? par->mfx.QPI:par->mfx.QPP;
        SET_OPTION_EXT(par->mfx.QPI + 1, WEBM_VP8::VP8E_SET_CQ_LEVEL);
        break;
    case MFX_RATECONTROL_AVBR:
        cfg->rc_end_usage       = WEBM_VP8::VPX_VBR;
        cfg->rc_target_bitrate  = par->mfx.TargetKbps;
        cfg->rc_buf_sz          = 10000;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }   

    cfg->g_threads = par->mfx.NumThread; 
    cfg->ts_number_layers = 1;

    vpx_arg_deadline = (par->mfx.TargetUsage == MFX_TARGETUSAGE_BEST_QUALITY)?
                        VPX_DL_BEST_QUALITY:VPX_DL_REALTIME;


    if (opts->EnableAutoAltRef == MFX_CODINGOPTION_ON ) 
    {
        arg_ctrls[*arg_ctrl_cnt][0] = WEBM_VP8::VP8E_SET_ENABLEAUTOALTREF;
        arg_ctrls[*arg_ctrl_cnt][1] = 1;
        (*arg_ctrl_cnt)++;
    }  
    SET_OPTION_EXT(opts->TokenPartitions,WEBM_VP8::VP8E_SET_TOKEN_PARTITIONS);

    return MFX_ERR_NONE;
} 


MFXVideoENCODEVP8::MFXVideoENCODEVP8(VideoCORE *core, mfxStatus *stat)
: VideoENCODE(),
  m_core(core),
  m_frameCountSync(0),
  m_totalBits(0),
  m_encodedFrames(0),
  m_isInitialized(false),
  m_useAuxInput(false),
  m_initFrameW(0),
  m_initFrameH(0)
{
    ippStaticInit();
    *stat = MFX_ERR_NONE;

    vpx_codec = VP8_Malloc(sizeof(WEBM_VP8::vpx_codec_ctx_t));
    vpx_cfg = VP8_Malloc(sizeof(WEBM_VP8::vpx_codec_enc_cfg_t));

    if (!vpx_codec || !vpx_cfg) 
    {
        if (vpx_codec) VP8_Free(vpx_codec);
        if (vpx_cfg) VP8_Free(vpx_cfg);

        *stat = MFX_ERR_NOT_INITIALIZED;
    }

} // MFXVideoENCODEVP8::MFXVideoENCODEVP8(VideoCORE *core, mfxStatus *stat)


MFXVideoENCODEVP8::~MFXVideoENCODEVP8()
{
    Close();

    if (vpx_codec) VP8_Free(vpx_codec);
    if (vpx_cfg) VP8_Free(vpx_cfg);

} // MFXVideoENCODEVP8::~MFXVideoENCODEVP8()


void WEBM_VP8::VP8EncodeParallelRegionStart(void *class_ptr, threads_fun_type threads_fun, int num_threads, void *threads_data, int threads_data_size) 
{
    MFXVideoENCODEVP8 *state = (MFXVideoENCODEVP8 *)class_ptr;

    if (state) 
    {
        state->ParallelRegionStart(threads_fun, num_threads, threads_data, threads_data_size);
    }

} // void VP8EncodeParallelRegionStart(void *class_ptr, threads_fun_type threads_fun, int num_threads, void *threads_data, int threads_data_size) 


void WEBM_VP8::VP8EncodeParallelRegionEnd(void *ptr) 
{
    MFXVideoENCODEVP8 *state = (MFXVideoENCODEVP8 *)ptr;

    if (state) 
    {
        state->ParallelRegionEnd();
    }

} // void VP8EncodeParallelRegionEnd(void *ptr) 


void MFXVideoENCODEVP8::ParallelRegionStart(threads_fun_type threads_fun, int num_threads, void *threads_data, int threads_data_size) 
{
    m_taskParams.thread_number = 0;
    m_taskParams.parallel_executing_threads = 0;
    m_taskParams.threads_fun = threads_fun;
    m_taskParams.num_threads = num_threads;
    m_taskParams.threads_data = threads_data;
    m_taskParams.threads_data_size = threads_data_size;
    m_taskParams.parallel_execution_in_progress = true;

    // Signal scheduler that all busy threads should be unleashed
    m_core->INeedMoreThreadsInside(this);

} // void MFXVideoENCODEVP8::ParallelRegionStart(threads_fun_type threads_fun, int num_threads, void *threads_data, int threads_data_size) 


void MFXVideoENCODEVP8::ParallelRegionEnd() 
{
    // Disable parallel task execution
    m_taskParams.parallel_execution_in_progress = false;

    // Wait for other threads to finish encoding their last macroblock row
    while(m_taskParams.parallel_executing_threads > 0)
    {
        vm_event_wait(&m_taskParams.parallel_region_done);
    }

} // void MFXVideoENCODEVP8::ParallelRegionEnd() 


mfxStatus MFXVideoENCODEVP8::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams, MFX_ENTRY_POINT *pEntryPoint)
{
    pEntryPoint->pRoutine = TaskRoutine;
    pEntryPoint->pCompleteProc = TaskCompleteProc;
    pEntryPoint->pState = this;
    pEntryPoint->requiredNumThreads = m_mfxVideoParam.mfx.NumThread;

    mfxStatus status = EncodeFrameCheck(ctrl, surface, bs, reordered_surface, pInternalParams);

    mfxFrameSurface1 *realSurface = GetOriginalSurface(surface);

    if (surface && !realSurface)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if (MFX_ERR_NONE == status || MFX_ERR_MORE_DATA_RUN_TASK == status || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == status || MFX_ERR_MORE_BITSTREAM == status)
    {
        // lock surface. If input surface is opaque core will lock both opaque and associated realSurface
        if (realSurface) 
        {
            m_core->IncreaseReference(&(realSurface->Data));
        }

        VP8EncodeTaskInputParams *m_pTaskInputParams = (VP8EncodeTaskInputParams*)VP8_Malloc(sizeof(VP8EncodeTaskInputParams));
        // MFX_ERR_MORE_DATA_RUN_TASK means that frame will be buffered and will be encoded later. Output bitstream isn't required for this task
        m_pTaskInputParams->bs = (status == MFX_ERR_MORE_DATA_RUN_TASK) ? 0 : bs;
        m_pTaskInputParams->ctrl = ctrl;
        m_pTaskInputParams->surface = surface;
        pEntryPoint->pParam = m_pTaskInputParams;
    }

    return status;

} // mfxStatus MFXVideoENCODEVP8::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams, MFX_ENTRY_POINT *pEntryPoint)


mfxStatus MFXVideoENCODEVP8::TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    VP8ENC_UNREFERENCED_PARAMETER(threadNumber);
    VP8ENC_UNREFERENCED_PARAMETER(callNumber);

    if (pState == NULL || pParam == NULL)
    {
        return MFX_ERR_NULL_PTR;
    }

    MFXVideoENCODEVP8 *th = static_cast<MFXVideoENCODEVP8 *>(pState);

    VP8EncodeTaskInputParams *pTaskParams = (VP8EncodeTaskInputParams*)pParam;

    if (th->m_taskParams.parallel_encoding_finished)
    {
        return MFX_TASK_DONE;
    }

    mfxI32 single_thread_selected = vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.single_thread_selected), 1, 0);
    if (!single_thread_selected)
    {
        // This task performs all single threaded regions
        mfxStatus status = th->EncodeFrame(pTaskParams->ctrl, NULL, pTaskParams->surface, pTaskParams->bs);
        th->m_taskParams.parallel_encoding_finished = true;
        VP8_Free(pParam);

        return status;
    }
    else
    {
        if (th->m_taskParams.parallel_execution_in_progress)
        {
            // Here we get only when thread that performs single thread work sent event
            // In this case m_taskParams.thread_number is set to 0, this is the number of single thread task
            // All other tasks receive their number > 0
            mfxI32 thread_number = vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.thread_number));

            mfxStatus status = MFX_ERR_NONE;

            // Some thread finished its work and was called again, but there is no work left for it since
            // if it finished, the only work remaining is being done by other currently working threads. No
            // work is possible so thread goes away waiting for maybe another parallel region
            if (thread_number > th->m_taskParams.num_threads)
            {
                return MFX_TASK_BUSY;
            }

            vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.parallel_executing_threads));

            (*(th->m_taskParams.threads_fun))((mfxU8*)th->m_taskParams.threads_data + th->m_taskParams.threads_data_size * (thread_number - 1));

            vm_interlocked_dec32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.parallel_executing_threads));

            // We're done, let single thread to continue
            vm_event_signal(&th->m_taskParams.parallel_region_done);

            if (MFX_ERR_NONE == status)
            {
                return MFX_TASK_WORKING;
            }
            else
            {
                return status;
            }
        }
        else
        {
            return MFX_TASK_BUSY;
        }
    }

} // mfxStatus MFXVideoENCODEVP8::TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)


mfxStatus MFXVideoENCODEVP8::TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes)
{
    VP8ENC_UNREFERENCED_PARAMETER(pParam);
    VP8ENC_UNREFERENCED_PARAMETER(taskRes);
    if (pState == NULL)
    {
        return MFX_ERR_NULL_PTR;
    }

    MFXVideoENCODEVP8 *th = static_cast<MFXVideoENCODEVP8 *>(pState);

    th->m_taskParams.single_thread_selected = 0;
    th->m_taskParams.thread_number = 0;
    th->m_taskParams.parallel_encoding_finished = false;
    th->m_taskParams.parallel_execution_in_progress = false;

    return MFX_TASK_DONE;

} // mfxStatus MFXVideoENCODEVP8::TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes)


mfxFrameSurface1* MFXVideoENCODEVP8::GetOriginalSurface(mfxFrameSurface1 *surface)
{
    if (m_mfxVideoParam.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        return m_core->GetNativeSurface(surface);
    }
    return surface;

} // mfxFrameSurface1* MFXVideoENCODEVP8::GetOriginalSurface(mfxFrameSurface1 *surface)


mfxStatus MFXVideoENCODEVP8::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams)
{
    mfxStatus st = MFX_ERR_NONE;

    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(bs);
    MFX_CHECK_NULL_PTR1(bs->Data);

    VP8ENC_UNREFERENCED_PARAMETER(pInternalParams);
    MFX_CHECK(bs->MaxLength > (bs->DataOffset + bs->DataLength),MFX_ERR_UNDEFINED_BEHAVIOR);

    if (surface) 
    { // check frame parameters
        MFX_CHECK(surface->Info.ChromaFormat == m_mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(surface->Info.Width  == m_mfxVideoParam.mfx.FrameInfo.Width, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(surface->Info.Height == m_mfxVideoParam.mfx.FrameInfo.Height, MFX_ERR_INVALID_VIDEO_PARAM);
      
        if (surface->Data.Y) 
        {
            MFX_CHECK (surface->Data.Pitch < 0x8000 && surface->Data.Pitch!=0, MFX_ERR_UNDEFINED_BEHAVIOR);
        } 
    }

    *reordered_surface = GetOriginalSurface(surface);

    if (ctrl && (ctrl->FrameType != MFX_FRAMETYPE_UNKNOWN) && ((ctrl->FrameType & 0xff) != (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR)))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    m_frameCountSync++;
    if (!surface) 
    {
        return MFX_ERR_MORE_DATA;
    }

    return st;

} // mfxStatus MFXVideoENCODEVP8::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams)

#define IS_VIDEO_OPAQ(sOpaq) (sOpaq.In.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
#define IS_SYS_OPAQ(sOpaq)   (sOpaq.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
#define IS_OPAQ(par)         (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)

#define IS_VIDEO_MEM(par,sOpaq) ((par->IOPattern&MFX_IOPATTERN_IN_VIDEO_MEMORY)||(IS_OPAQ(par) && IS_VIDEO_OPAQ(sOpaq)))



mfxStatus MFXVideoENCODEVP8::Init(mfxVideoParam* par_in)
{
    mfxStatus   temp_sts    = MFX_ERR_NONE;
    mfxStatus   sts         = MFX_ERR_NONE;
    mfxVideoParam* par      = 0;

    MFX_CHECK(!m_isInitialized, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR1(par_in);

    memset(&m_mfxVideoParam,0,sizeof(mfxVideoParam));
    memset(&m_extOption,    0,sizeof(mfxExtCodingOptionVP8));
    memset(&m_extOpaqAlloc, 0,sizeof(mfxExtOpaqueSurfaceAlloc));
    memset(&m_extVP8Par,    0,sizeof(mfxExtCodingOptionVP8Param));

    mfxExtCodingOptionVP8Param * extVP8ParIn = (mfxExtCodingOptionVP8Param*)GetExtBuffer(par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_VP8_PARAM);
    if (extVP8ParIn)
        m_extVP8Par = *extVP8ParIn;
    memset(&m_auxInput, 0, sizeof(m_auxInput));
    memset(&m_response_alien, 0, sizeof(m_response_alien));
    memset(&m_response, 0, sizeof(m_response));

    sts = VP8_encoder::CheckParametersAndSetDefault(par_in, &m_mfxVideoParam,&m_extOption,
                                            &m_extOpaqAlloc,m_core->IsExternalFrameAllocator());
    MFX_CHECK(sts >= 0, sts);
    
    m_initFrameW = par_in->mfx.FrameInfo.Width;
    m_initFrameH = par_in->mfx.FrameInfo.Height;

    par = &m_mfxVideoParam;
    if (IS_OPAQ(par))     
    {
        mfxFrameAllocRequest request={0};

        request.Info        =   par->mfx.FrameInfo;
        request.Type        =  (mfxU16)m_extOpaqAlloc.In.Type;
        request.NumFrameMin =  (mfxU16)m_extOpaqAlloc.In.NumSurface;
        request.NumFrameSuggested = request.NumFrameMin;

        temp_sts = m_core->AllocFrames(&request,
                                       IS_VIDEO_OPAQ(m_extOpaqAlloc) ? &m_response_alien: &m_response,
                                       m_extOpaqAlloc.In.Surfaces,
                                       m_extOpaqAlloc.In.NumSurface);
        MFX_CHECK_STS(temp_sts);
    }
    if (IS_VIDEO_MEM(par,m_extOpaqAlloc))
    {
        /*only one frame for input frame, reconstructed frame are used from WebM*/

        mfxFrameAllocRequest request={0};

        request.Info =   par->mfx.FrameInfo;
        request.Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
        request.NumFrameMin = 1;
        request.NumFrameSuggested = 1;
        temp_sts  = m_core->AllocFrames(&request, &m_response, false);
        MFX_CHECK_STS(temp_sts);

        m_useAuxInput = true;
        m_auxInput.Data.MemId = m_response.mids[0];
        m_auxInput.Info = request.Info;
    }

    m_frameCountSync = 0;
    m_frameCount = 0;
    m_taskParams.threads_data = NULL;
    vm_event_set_invalid(&m_taskParams.parallel_region_done);

    if (VM_OK != vm_event_init(&m_taskParams.parallel_region_done, 0, 0))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_taskParams.single_thread_selected = 0;
    m_taskParams.thread_number = 0;
    m_taskParams.parallel_encoding_finished = false;
    m_taskParams.parallel_execution_in_progress = false;

    {
        /*Init WebM encoder*/

        mfxI32 status = 0;
        status = vpx_codec_enc_config_default(vpx_interface, (WEBM_VP8::vpx_codec_enc_cfg_t *)vpx_cfg, 0);
        CHECK_VPX_STATUS(status);

        int  arg_ctrls[ARG_CTRL_CNT_MAX][2], arg_ctrl_cnt = 0;

        InitVpxCfg(vpx_cfg, par, &m_extOption, arg_ctrls, &arg_ctrl_cnt);

        status = vpx_codec_enc_init((WEBM_VP8::vpx_codec_ctx_t *)vpx_codec, vpx_interface, (WEBM_VP8::vpx_codec_enc_cfg_t *)vpx_cfg, 0);
        CHECK_VPX_STATUS(status);

        for (int i = 0; i < arg_ctrl_cnt; i++) 
        {
            status = vpx_codec_control_((WEBM_VP8::vpx_codec_ctx_t *)vpx_codec, arg_ctrls[0][0], arg_ctrls[0][1]);
            CHECK_VPX_STATUS(status);
        }
    }

    m_isInitialized = true;
    return sts;    

} // mfxStatus MFXVideoENCODEVP8::Init(mfxVideoParam* par_in)


mfxStatus MFXVideoENCODEVP8::Close()
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    vpx_codec_destroy((WEBM_VP8::vpx_codec_ctx_t *)vpx_codec);
    vm_event_destroy(&m_taskParams.parallel_region_done);

    if (m_response.NumFrameActual > 0)
    {
        m_core->FreeFrames(&m_response);
        m_response.NumFrameActual = 0;    
    }
    if (m_response_alien.NumFrameActual > 0)
    {
        m_core->FreeFrames(&m_response_alien);
        m_response_alien.NumFrameActual = 0;    
    }
    return MFX_ERR_NONE;

} // mfxStatus MFXVideoENCODEVP8::Close()


mfxStatus MFXVideoENCODEVP8::Reset(mfxVideoParam *par_in)
{
    mfxStatus   temp_sts    = MFX_ERR_NONE;
    mfxStatus   sts         = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1(par_in);
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK(m_initFrameH >= par_in->mfx.FrameInfo.Height,   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(m_initFrameW >= par_in->mfx.FrameInfo.Width,    MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(par_in->IOPattern == m_mfxVideoParam.IOPattern, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
  
    temp_sts = VP8_encoder::CheckParametersAndSetDefault(par_in, &m_mfxVideoParam,&m_extOption,
                                            &m_extOpaqAlloc,m_core->IsExternalFrameAllocator(), true);
    MFX_CHECK(temp_sts >= 0, temp_sts);
    sts = (temp_sts > 0)? temp_sts:sts;

    //Reset frame count

    m_frameCountSync = 0;
    m_frameCount = 0;

    m_taskParams.single_thread_selected = 0;
    m_taskParams.thread_number = 0;
    m_taskParams.parallel_encoding_finished = false;
    m_taskParams.parallel_execution_in_progress = false;

    {
        mfxI32 status = 0;
        status = WEBM_VP8::vpx_codec_enc_config_default(vpx_interface, (WEBM_VP8::vpx_codec_enc_cfg_t *)vpx_cfg, 0);
        CHECK_VPX_STATUS(status);

        int  arg_ctrls[ARG_CTRL_CNT_MAX][2], arg_ctrl_cnt = 0;

        InitVpxCfg(vpx_cfg, &m_mfxVideoParam, &m_extOption, arg_ctrls, &arg_ctrl_cnt);

        status = WEBM_VP8::vpx_codec_destroy((WEBM_VP8::vpx_codec_ctx_t *)vpx_codec);
        CHECK_VPX_STATUS(status);

        status = WEBM_VP8::vpx_codec_enc_init((WEBM_VP8::vpx_codec_ctx_t *)vpx_codec, vpx_interface, (WEBM_VP8::vpx_codec_enc_cfg_t *)vpx_cfg, 0);
        CHECK_VPX_STATUS(status);

        for (int i = 0; i < arg_ctrl_cnt; i++) 
        {
            status = vpx_codec_control_((WEBM_VP8::vpx_codec_ctx_t *)vpx_codec, arg_ctrls[0][0], arg_ctrls[0][1]);
            CHECK_VPX_STATUS(status);
        }
    }
    return sts;

    
} // mfxStatus MFXVideoENCODEVP8::Reset(mfxVideoParam *par_in)


mfxStatus MFXVideoENCODEVP8::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus st = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(out)

    if( in == NULL )
    {
        st = VP8_encoder::SetSupportedParameters(out);
    } 
    else 
    { 
        st = VP8_encoder::CheckParameters(in,out);
    }
    MFX_CHECK_STS(st);
    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;

} // mfxStatus MFXVideoENCODEVP8::Query(mfxVideoParam *in, mfxVideoParam *out)


mfxStatus MFXVideoENCODEVP8::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR1(par)
    MFX_CHECK_NULL_PTR1(request)

    // check for valid IOPattern
    mfxU16 IOPatternIn = par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY);
    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0) ||
        (IOPatternIn != MFX_IOPATTERN_IN_VIDEO_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_SYSTEM_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_OPAQUE_MEMORY))
    {
       return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (par->Protected != 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mfxU16 nFrames = 1; // no reordering at all

    request->NumFrameMin = nFrames;
    request->NumFrameSuggested = IPP_MAX(nFrames,par->AsyncDepth);

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    }
    else if(par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) 
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    } 
    else 
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;

} // mfxStatus MFXVideoENCODEVP8::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)


mfxStatus MFXVideoENCODEVP8::GetEncodeStat(mfxEncodeStat *stat)
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(stat)
    memset(stat, 0, sizeof(mfxEncodeStat));
    stat->NumCachedFrame = 0;
    stat->NumBit = m_totalBits;
    stat->NumFrame = m_encodedFrames;

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoENCODEVP8::GetEncodeStat(mfxEncodeStat *stat)


mfxStatus MFXVideoENCODEVP8::EncodeFrame(mfxEncodeCtrl *, mfxEncodeInternalParams *, mfxFrameSurface1 *inputSurface, mfxBitstream *bs)
{
    mfxStatus st = MFX_ERR_NONE;
    mfxU8* dataPtr = 0;
    mfxU32 initialDataLength = 0;
    WEBM_VP8::vpx_image_t  _img, *img = &_img;
    bool locked = false;

    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(bs); 

    initialDataLength = bs->DataLength;

    // inputSurface can be opaque. Get real surface from core
    mfxFrameSurface1 *surface = GetOriginalSurface(inputSurface);

    if (inputSurface != surface) 
    { // input surface is opaque surface
        surface->Data.FrameOrder = inputSurface->Data.FrameOrder;
        surface->Data.TimeStamp = inputSurface->Data.TimeStamp;
        surface->Data.Corrupted = inputSurface->Data.Corrupted;
        surface->Data.DataFlag = inputSurface->Data.DataFlag;
        surface->Info = inputSurface->Info;
    }

    if (surface)
    {

        if (m_useAuxInput) 
        { // copy from d3d to internal frame in system memory
            // Lock internal. FastCopy to use locked, to avoid additional lock/unlock

            st = m_core->LockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
            MFX_CHECK_STS(st);

            st = m_core->DoFastCopyWrapper(&m_auxInput,
                MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                surface,
                MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
            MFX_CHECK_STS(st);

            //            m_core->DecreaseReference(&(surface->Data)); // not here to keep related mfxEncodeCtrl

            m_auxInput.Data.FrameOrder = surface->Data.FrameOrder;
            m_auxInput.Data.TimeStamp = surface->Data.TimeStamp;
            surface = &m_auxInput; // replace input pointer
        } 
        else 
        {
            if (surface->Data.Y == 0 && surface->Data.U == 0 &&
                surface->Data.V == 0 && surface->Data.A == 0)
            {
                st = m_core->LockExternalFrame(surface->Data.MemId, &(surface->Data));
                MFX_CHECK_STS (st);
                locked = true;
             }
        }

        if (!surface->Data.Y || surface->Data.Pitch > 0x7fff || surface->Data.Pitch < m_mfxVideoParam.mfx.FrameInfo.Width || !surface->Data.Pitch ||
            surface->Info.FourCC == MFX_FOURCC_NV12 && !surface->Data.UV ||
            surface->Info.FourCC == MFX_FOURCC_YV12 && (!surface->Data.U || !surface->Data.V) )
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }

    WEBM_VP8::vpx_codec_enc_cfg_t *cfg = (WEBM_VP8::vpx_codec_enc_cfg_t *)vpx_cfg;
    /*WEBM_VP8::*/int64_t frame_start = (cfg->g_timebase.den * (/*WEBM_VP8::*/int64_t)(m_frameCount)*cfg->g_timebase.num*m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD) / cfg->g_timebase.num/m_mfxVideoParam.mfx.FrameInfo.FrameRateExtN;
    /*WEBM_VP8::*/int64_t next_frame_start = (cfg->g_timebase.den * (/*WEBM_VP8::*/int64_t)(m_frameCount + 1)*cfg->g_timebase.num*m_mfxVideoParam.mfx.FrameInfo.FrameRateExtD) / cfg->g_timebase.num/m_mfxVideoParam.mfx.FrameInfo.FrameRateExtN;

    if (surface) 
    {
        img->fmt = WEBM_VP8::VPX_IMG_FMT_NV12;
        img->w = img->d_w = cfg->g_w;
        img->h = img->d_h = cfg->g_h;
        img->x_chroma_shift = img->y_chroma_shift = 1;
        img->planes[VPX_PLANE_Y] = surface->Data.Y;
        img->planes[VPX_PLANE_U] = surface->Data.UV;
        img->planes[VPX_PLANE_V] = surface->Data.UV+1;
        img->stride[VPX_PLANE_Y] = surface->Data.Pitch;
        img->stride[VPX_PLANE_U] = surface->Data.Pitch;
        img->stride[VPX_PLANE_V] = surface->Data.Pitch;
        img->user_priv = NULL;
        img->img_data = NULL;
        img->bps = img->img_data_owner = img->self_allocd = 0;
    } 
    else 
    {
        return MFX_ERR_NONE;
    }


    mfxI32 status;
    status = WEBM_VP8::vpx_codec_encode((WEBM_VP8::vpx_codec_ctx_t *)vpx_codec, inputSurface ? img : NULL, frame_start,
        (unsigned long)(next_frame_start - frame_start), 0, vpx_arg_deadline);
    CHECK_VPX_STATUS(status);

    WEBM_VP8::vpx_codec_iter_t iter = NULL;
    const WEBM_VP8::vpx_codec_cx_pkt_t *pkt;
    
    dataPtr = bs->Data + bs->DataOffset + bs->DataLength;

    pkt = WEBM_VP8::vpx_codec_get_cx_data((WEBM_VP8::vpx_codec_ctx_t *)vpx_codec, &iter);
    if (pkt->kind == WEBM_VP8::VPX_CODEC_CX_FRAME_PKT) 
    {
#ifdef VP8_ENC_WRITE_IVF_HEADERS
        if (m_extVP8Par.WriteIVFHeaders != MFX_CODINGOPTION_OFF)
        {
            if (m_frameCount == 0)
            {
                write_ivf_file_header(dataPtr, (WEBM_VP8::vpx_codec_enc_cfg_t *)vpx_cfg, 10000); 
                bs->DataLength += 32;
                dataPtr += 32;
            }

            write_ivf_frame_header(dataPtr, pkt);
            bs->DataLength += 12;
            dataPtr += 12;
        }
#endif

        MFX_INTERNAL_CPY(dataPtr, pkt->data.frame.buf,pkt->data.frame.sz);
        bs->DataLength += (mfxU32)pkt->data.frame.sz;
        m_frameCount++;
        if(bs->DataLength - initialDataLength) 
        {
            m_encodedFrames++;
            m_totalBits += (bs->DataLength - initialDataLength) * 8;
        }
    }

    if (m_useAuxInput) 
    {
        m_core->UnlockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
    } 
    else 
    {
        if (locked)
        {
            m_core->UnlockExternalFrame(surface->Data.MemId, &(surface->Data));
        }
    }

    mfxFrameSurface1* surf = 0;
    surf = inputSurface;
    if (surf) 
    {
        bs->TimeStamp = surf->Data.TimeStamp;
        m_core->DecreaseReference(&(surf->Data));
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoENCODEVP8::EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *inputSurface, mfxBitstream *bs)


mfxStatus MFXVideoENCODEVP8::GetVideoParam(mfxVideoParam *par)
{
    mfxExtOpaqueSurfaceAlloc * pOpaq=0;
    mfxExtCodingOptionVP8* pEx = 0;

    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par)
    CHECK_VERSION(par->Version);

    MFX_CHECK(VP8_encoder::CheckExtendedBuffers(par) == MFX_ERR_NONE, MFX_ERR_UNSUPPORTED);

    pOpaq =(mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(
                                              par->ExtParam, 
                                              par->NumExtParam, 
                                              MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
    pEx = (mfxExtCodingOptionVP8*)GetExtBuffer(par->ExtParam, 
                                               par->NumExtParam,
                                               MFX_EXTBUFF_VP8_EX_CODING_OPT);

    par->mfx = m_mfxVideoParam.mfx;
    par->Protected = m_mfxVideoParam.Protected;
    par->AsyncDepth = m_mfxVideoParam.AsyncDepth;
    par->IOPattern = m_mfxVideoParam.IOPattern;
    if (pOpaq) 
    {
        *pOpaq = m_extOpaqAlloc;
    }
    if (pEx)
    {
        *pEx = m_extOption;
    }

    return MFX_ERR_NONE;

} // mfxStatus MFXVideoENCODEVP8::GetVideoParam(mfxVideoParam *par)


mfxStatus MFXVideoENCODEVP8::GetFrameParam(mfxFrameParam *)
{
   return MFX_ERR_UNSUPPORTED;

} // mfxStatus MFXVideoENCODEVP8::GetFrameParam(mfxFrameParam *par)

#endif // MFX_ENABLE_H264_VIDEO_ENCODE
/* EOF */
