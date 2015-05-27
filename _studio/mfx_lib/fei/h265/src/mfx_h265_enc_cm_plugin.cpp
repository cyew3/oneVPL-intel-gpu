/*//////////////////////////////////////////////////////////////////////////////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#include "mfx_enc_common.h"
#include "mfx_h265_enc_cm_plugin.h"

#include "mfx_task.h"

#ifdef AS_H265FEI_PLUGIN

#ifdef MFX_VA

VideoENC_H265FEI::VideoENC_H265FEI(VideoCORE *core,  mfxStatus * sts) :
    m_feiH265(),
    m_feiH265Param(),
    m_feiH265In(),
    m_feiH265Out(),
    m_bInit(false),
    m_core(core)
{
    *sts = MFX_ERR_NONE;
} 

VideoENC_H265FEI::~VideoENC_H265FEI()
{
    Close();
}

mfxStatus VideoENC_H265FEI::Init(mfxVideoParam *par)
{ 
    mfxStatus err = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1( par );
    MFX_CHECK( m_bInit == false, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxExtFEIH265Param *pParams = (mfxExtFEIH265Param *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_FEI_H265_PARAM);
    MFX_CHECK_NULL_PTR1(pParams);

    m_feiH265Param.Width         = par->mfx.FrameInfo.Width;
    m_feiH265Param.Height        = par->mfx.FrameInfo.Height;
    m_feiH265Param.NumRefFrames  = par->mfx.NumRefFrame;

    m_feiH265Param.MaxCUSize     = pParams->MaxCUSize;
    m_feiH265Param.MPMode        = pParams->MPMode;
    m_feiH265Param.NumIntraModes = pParams->NumIntraModes;

    // TODO - validate bounds
    m_feiH265Param.BitDepth      = pParams->BitDepth;
    m_feiH265Param.TargetUsage   = pParams->TargetUsage;

    /* validate parameters - update these limits as necessary in future versions */
    MFX_CHECK ((m_feiH265Param.Width > 0)         && (m_feiH265Param.Height > 0),     MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK ((m_feiH265Param.Width <= 4088)     && (m_feiH265Param.Height) <= 4088, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK ((m_feiH265Param.Width & 0x0f) == 0 && (m_feiH265Param.Height & 0x0f) == 0, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK ((m_feiH265Param.NumRefFrames >= 0) && (m_feiH265Param.NumRefFrames <= MFX_FEI_H265_MAX_NUM_REF_FRAMES), MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK ((m_feiH265Param.MaxCUSize == 16)   || (m_feiH265Param.MaxCUSize == 32), MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK ((m_feiH265Param.MPMode >= 1)       && (m_feiH265Param.MPMode <= 3), MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK ((m_feiH265Param.NumIntraModes == 1), MFX_ERR_INVALID_VIDEO_PARAM);

    /* initialize FEI library */
    err = H265FEI_Init(&m_feiH265, &m_feiH265Param, m_core);
    if (err) {
        //printf("Error initializing FEI library\n");
        return err;
    }
    memset(&m_feiH265In,  0, sizeof(m_feiH265In));
    memset(&m_feiH265Out, 0, sizeof(m_feiH265Out));

    return err;
}

mfxStatus VideoENC_H265FEI::Reset(mfxVideoParam *par)
{ 
    /* shut down and reopen */
    Close();
    return Init(par);
}

mfxStatus VideoENC_H265FEI::Close()
{
    mfxStatus err = MFX_ERR_NONE;

    if (m_feiH265)
        err = H265FEI_Close(m_feiH265);
    m_feiH265 = 0;

    return err;
}

mfxStatus VideoENC_H265FEI::AllocateInputSurface(mfxHDL *surfIn)
{
    mfxStatus err = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1(surfIn);

    if (m_feiH265)
        err = H265FEI_AllocateInputSurface(m_feiH265, surfIn);

    return err;
}

mfxStatus VideoENC_H265FEI::AllocateOutputSurface(mfxU8 *sysMem, mfxSurfInfoENC *surfInfo, mfxHDL *surfOut)
{
    mfxStatus err = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1(sysMem);
    MFX_CHECK_NULL_PTR1(surfInfo);
    MFX_CHECK_NULL_PTR1(surfOut);

    if (m_feiH265)
        err = H265FEI_AllocateOutputSurface(m_feiH265, sysMem, surfInfo, surfOut);

    return err;
}

mfxStatus VideoENC_H265FEI::FreeSurface(mfxHDL surf)
{
    mfxStatus err = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1(surf);

    if (m_feiH265)
        err = H265FEI_FreeSurface(m_feiH265, surf);

    return err;
}

/* TODO - anything to do here? */
mfxStatus VideoENC_H265FEI::GetVideoParam(mfxVideoParam *par)
{ 
    MFX_CHECK_NULL_PTR1(par);

    return MFX_ERR_NONE;
}

/* TODO - anything to do here? */
mfxStatus VideoENC_H265FEI::GetFrameParam(mfxFrameParam *par)
{ 
    MFX_CHECK_NULL_PTR1(par);

    return MFX_ERR_NONE;
}

/* Submit/Query/Complete are called from the scheduler core (could be from any child thread) */

static mfxStatus SubmitFrameH265FEIRoutine(void *pState, void *pParam, mfxU32 /* threadNumber */, mfxU32 /* callNumber */)
{
    mfxStatus tskRes = MFX_ERR_NONE;

    VideoENC_H265FEI *pFEI = (VideoENC_H265FEI *)pState;
    sAsyncParams *out = (sAsyncParams *)pParam;
 
    H265FEI_ProcessFrameAsync(pFEI->m_feiH265, &(out->feiH265In), &(out->feiH265Out), &(out->feiH265SyncPoint));

    return tskRes;

}

/* TODO - anything to do here? */
static mfxStatus QueryFrameH265FEIRoutine(void * /* pState */, void * /* pParam */, mfxU32 /* threadNumber */, mfxU32 /* callNumber */)
{
    mfxStatus tskRes = MFX_ERR_NONE;

    //VideoENC_H265FEI *pFEI = (VideoENC_H265FEI *)pState;
    //sAsyncParams *out = (sAsyncParams *)pParam;
  
    return tskRes;

}

static mfxStatus CompleteFrameH265FEIRoutine(void *pState, void * pParam , mfxStatus /* taskRes */)
{
    VideoENC_H265FEI *pFEI = (VideoENC_H265FEI *)pState;
    sAsyncParams *out = (sAsyncParams *)pParam;

    H265FEI_SyncOperation(pFEI->m_feiH265, (out->feiH265SyncPoint), 0xFFFFFFFF);

    if (pParam)
    {
        delete (sAsyncParams *)pParam;   // NOT SAFE !!!
    }
    return pFEI->ResetTaskCounters();
} 

mfxStatus VideoENC_H265FEI::ResetTaskCounters()
{
    // TO DO
    return MFX_TASK_DONE;
} 

mfxStatus VideoENC_H265FEI::RunFrameVmeENCCheck(mfxENCInput *input, mfxENCOutput *output, MFX_ENTRY_POINT pEntryPoints[], mfxU32 &numEntryPoints)
{
    MFX_CHECK_NULL_PTR1(input);
    MFX_CHECK_NULL_PTR1(output);

    /* FEI assumes one src/ref combination per call (inter only) to minimize delays 
     * (run with lookahead unless ref frame is the frame immediately before src frame)
     */
    VM_ASSERT(input->NumFrameL0 == 0 || input->NumFrameL0 == 1);
    VM_ASSERT(input->NumFrameL1 == 0);

    /* create state object (parameters struct) */
    sAsyncParams *sp;
    std::auto_ptr<sAsyncParams> pAsyncParams(new sAsyncParams);
    memset(pAsyncParams.get(), 0, sizeof(sAsyncParams));
    sp = pAsyncParams.get();

    /* copy frame info into params struct */
    if (input->InSurface) {
        sp->feiH265In.FEIFrameIn.EncOrder = input->InSurface->Data.FrameOrder;
        sp->feiH265In.FEIFrameIn.PicOrder = input->InSurface->Data.FrameOrder;      // unused in FEI core currently, but leave field if needed later
        sp->feiH265In.FEIFrameIn.YPitch   = input->InSurface->Data.PitchLow + ((mfxU32)(input->InSurface->Data.PitchHigh) << 16);
        sp->feiH265In.FEIFrameIn.YPlane   = input->InSurface->Data.Y;
        sp->feiH265In.FEIFrameIn.YBitDepth = input->InSurface->Info.BitDepthLuma;
        MFX_CHECK_NULL_PTR1(sp->feiH265In.FEIFrameIn.YPlane);
    } 

    if (input->L0Surface) {
        MFX_CHECK_NULL_PTR1(input->L0Surface[0]);
        sp->feiH265In.FEIFrameRef.EncOrder = input->L0Surface[0]->Data.FrameOrder;
        sp->feiH265In.FEIFrameRef.PicOrder = input->L0Surface[0]->Data.FrameOrder;
        sp->feiH265In.FEIFrameRef.YPitch   = input->L0Surface[0]->Data.PitchLow + ((mfxU32)(input->L0Surface[0]->Data.PitchHigh) << 16);
        sp->feiH265In.FEIFrameRef.YPlane   = input->L0Surface[0]->Data.Y;
        sp->feiH265In.FEIFrameRef.YBitDepth = input->L0Surface[0]->Info.BitDepthLuma;
        MFX_CHECK_NULL_PTR1(sp->feiH265In.FEIFrameRef.YPlane);
    }

    /* copy parameters from input/output mfxExtBufs into params struct */ 
    mfxExtFEIH265Input  *pIn  = (mfxExtFEIH265Input *)  GetExtBuffer(input->ExtParam,  input->NumExtParam,  MFX_EXTBUFF_FEI_H265_INPUT);
    mfxExtFEIH265Output *pOut = (mfxExtFEIH265Output *) GetExtBuffer(output->ExtParam, output->NumExtParam, MFX_EXTBUFF_FEI_H265_OUTPUT);

    MFX_CHECK_NULL_PTR1(pIn);
    MFX_CHECK_NULL_PTR1(pOut);

    sp->feiH265In.FEIOp     = pIn->FEIOp;
    sp->feiH265In.FrameType = pIn->FrameType;

    sp->feiH265In.FEIFrameIn.surfIn  = pIn->SurfInSrc;
    sp->feiH265In.FEIFrameRef.surfIn = pIn->SurfInRec;

    /* save a local copy of the mfxExtFEIH265Output struct allocated by the user (need copy since this operates asynchronously with FEI core) */
    sp->feiH265Out = *pOut;

    /* basic error checking */
    if (sp->feiH265In.FrameType != MFX_FRAMETYPE_I && sp->feiH265In.FrameType != MFX_FRAMETYPE_P && sp->feiH265In.FrameType != MFX_FRAMETYPE_B)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    pEntryPoints[0].pRoutine = &SubmitFrameH265FEIRoutine;
    pEntryPoints[0].pCompleteProc =0;
    pEntryPoints[0].pState = this;
    pEntryPoints[0].requiredNumThreads = 1;
    pEntryPoints[0].pParam = pAsyncParams.get();

    pEntryPoints[1].pRoutine = &QueryFrameH265FEIRoutine;
    pEntryPoints[1].pCompleteProc = &CompleteFrameH265FEIRoutine;
    pEntryPoints[1].pState = this;
    pEntryPoints[1].requiredNumThreads = 1;
    pEntryPoints[1].pParam = pAsyncParams.get();

    numEntryPoints = 2;

    pAsyncParams.release(); // memory will be freed in CompleteFrameH265FEIRoutine()
    return MFX_ERR_NONE;
}

#ifndef MAX
#define MAX(a,b)    (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))
#endif

static const mfxU16 MFX_IOPATTERN_IN_MASK = (MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY);

mfxStatus VideoENC_H265FEI::Query(VideoCORE* pCore, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR2(pCore, out);

    if (in == 0)
    {
        MFX_CHECK(out->NumExtParam > 0, MFX_ERR_UNDEFINED_BEHAVIOR);
        mfxExtFEIH265Param *pParams = (mfxExtFEIH265Param *)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_FEI_H265_PARAM);
        MFX_CHECK_NULL_PTR1(pParams);

        out->AsyncDepth = 1;
        out->IOPattern = 1;
        out->Protected = 0;
        memset (&out->mfx,0,sizeof(out->mfx));

        out->mfx.CodecId = 1;
        out->mfx.NumRefFrame = 1;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.FrameInfo.Height = 1;

        pParams->MaxCUSize = 1;
        pParams->MPMode = 1;
        pParams->NumIntraModes = 1;

        pParams->BitDepth = 1;
        pParams->TargetUsage = 1;
    }
    else
    {
        MFX_CHECK(in->NumExtParam  > 0, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(out->NumExtParam > 0, MFX_ERR_UNDEFINED_BEHAVIOR);

        mfxExtFEIH265Param *pParams_in  = (mfxExtFEIH265Param *)GetExtBuffer(in->ExtParam,  in->NumExtParam, MFX_EXTBUFF_FEI_H265_PARAM);
        mfxExtFEIH265Param *pParams_out = (mfxExtFEIH265Param *)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_FEI_H265_PARAM);

        MFX_CHECK_NULL_PTR1(pParams_in);
        MFX_CHECK_NULL_PTR1(pParams_out);

        out->AsyncDepth = in->AsyncDepth;
        out->IOPattern  = in->IOPattern;
        out->Protected = 0;

        /* currently input frames are in system memory */
        mfxU32 inPattern = in->IOPattern & MFX_IOPATTERN_IN_MASK;
        MFX_CHECK(inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY, MFX_ERR_INVALID_VIDEO_PARAM);
        memset(&out->mfx, 0, sizeof(out->mfx));

        out->mfx.CodecId          = in->mfx.CodecId;
        out->mfx.NumRefFrame      = in->mfx.NumRefFrame;
        out->mfx.FrameInfo.Width  = in->mfx.FrameInfo.Width;
        out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;

        MFX_CHECK ((out->mfx.FrameInfo.Width & 0x0f) == 0 && (out->mfx.FrameInfo.Height & 0x0f) == 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK ((out->mfx.NumRefFrame <= MFX_FEI_H265_MAX_NUM_REF_FRAMES), MFX_ERR_INVALID_VIDEO_PARAM);

        /* these checks may need to be updated when new features/modes are added in future versions */
        pParams_out->MaxCUSize     = pParams_in->MaxCUSize;
        pParams_out->MPMode        = pParams_in->MPMode;
        pParams_out->NumIntraModes = pParams_in->NumIntraModes;

        pParams_out->BitDepth      = pParams_in->BitDepth;
        pParams_out->TargetUsage   = pParams_in->TargetUsage;


        MFX_CHECK ((pParams_out->MPMode >= 1) && (pParams_out->MPMode <= 3), MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK ((pParams_out->NumIntraModes == 1), MFX_ERR_INVALID_VIDEO_PARAM);

        /* fill out extBuf with required surface dimensions so user can allocate memory */
        mfxExtFEIH265Alloc *pAlloc_in  = (mfxExtFEIH265Alloc *)GetExtBuffer(in->ExtParam,  in->NumExtParam, MFX_EXTBUFF_FEI_H265_ALLOC);
        mfxExtFEIH265Alloc *pAlloc_out = (mfxExtFEIH265Alloc *)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_FEI_H265_ALLOC);

        MFX_CHECK_NULL_PTR1(pAlloc_in);
        MFX_CHECK_NULL_PTR1(pAlloc_out);

        H265FEI_GetSurfaceDimensions(pCore, out->mfx.FrameInfo.Width, out->mfx.FrameInfo.Height, pAlloc_out);
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoENC_H265FEI::QueryIOSurf(VideoCORE* , mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    mfxU16 NumRefFrame;

    MFX_CHECK_NULL_PTR2(par, request);

    mfxExtFEIH265Param *pParams = (mfxExtFEIH265Param *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_FEI_H265_PARAM);
    MFX_CHECK(pParams, MFX_ERR_UNDEFINED_BEHAVIOR);

    NumRefFrame = par->mfx.NumRefFrame;
    if (NumRefFrame > MFX_FEI_H265_MAX_NUM_REF_FRAMES)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    /* currently input frames are in system memory */
    mfxU32 inPattern = par->IOPattern & MFX_IOPATTERN_IN_MASK;
    MFX_CHECK(inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY, MFX_ERR_INVALID_VIDEO_PARAM);

    if (inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request->Type =
            MFX_MEMTYPE_EXTERNAL_FRAME |
            MFX_MEMTYPE_FROM_ENCODE |
            MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else // MFX_IOPATTERN_IN_VIDEO_MEMORY || MFX_IOPATTERN_IN_OPAQUE_MEMORY
    {
        /* unused for now */
    }
    request->NumFrameMin         = 1;
    request->NumFrameSuggested   = NumRefFrame + MAX(par->AsyncDepth, 1);
    request->Info                = par->mfx.FrameInfo;

    return MFX_ERR_NONE;
} 

#endif  // AS_H265FEI_PLUGIN

#endif  // MFX_VA

/* EOF */

