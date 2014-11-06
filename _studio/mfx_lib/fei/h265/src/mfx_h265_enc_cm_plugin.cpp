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

    mfxExtFEIH265Param *pParams = (mfxExtFEIH265Param *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_FEIH265_PARAM);

    memcpy(&m_feiH265Param, &(pParams->feiParam), sizeof(mfxExtFEIH265Param));

    /* initialize FEI library */
    if (err = H265FEI_Init(&m_feiH265, &m_feiH265Param, m_core)) {
        printf("Error initializing FEI library\n");
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

    return MFX_ERR_NONE;
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
 
    H265FEI_ProcessFrameAsync(pFEI->m_feiH265, &(out->feiH265In), out->feiH265Out, &(out->feiH265SyncPoint));

    return tskRes;

}

/* TODO - anything to do here? */
static mfxStatus QueryFrameH265FEIRoutine(void *pState, void *pParam, mfxU32 /* threadNumber */, mfxU32 /* callNumber */)
{
    mfxStatus tskRes = MFX_ERR_NONE;

    VideoENC_H265FEI *pFEI = (VideoENC_H265FEI *)pState;
    sAsyncParams *out = (sAsyncParams *)pParam;
  
    return tskRes;

}

static mfxStatus CompleteFrameH265FEIRoutine(void *pState, void * pParam , mfxStatus /* taskRes */)
{
    VideoENC_H265FEI *pFEI = (VideoENC_H265FEI *)pState;
    sAsyncParams *out = (sAsyncParams *)pParam;

    H265FEI_SyncOperation(pFEI->m_feiH265, (out->feiH265SyncPoint), -1);

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
        sp->feiH265In.FEIFrameIn.PicOrder = input->InSurface->Data.FrameOrder;  // TMP - remove from FEI core
        sp->feiH265In.FEIFrameIn.YPitch   = input->InSurface->Data.Pitch;
        sp->feiH265In.FEIFrameIn.YPlane   = input->InSurface->Data.Y;
    } 

    if (input->L0Surface) {
        sp->feiH265In.FEIFrameRef.EncOrder = input->L0Surface[0]->Data.FrameOrder;
        sp->feiH265In.FEIFrameRef.PicOrder = input->L0Surface[0]->Data.FrameOrder;  // TMP - remove from FEI core
        sp->feiH265In.FEIFrameRef.YPitch   = input->L0Surface[0]->Data.Pitch;
        sp->feiH265In.FEIFrameRef.YPlane   = input->L0Surface[0]->Data.Y;
    }

    /* copy pointers to user-allocated FEIInput, FEIOutput structs into params struct */ 
    mfxExtFEIH265Input  *pIn  = (mfxExtFEIH265Input *)  GetExtBuffer(input->ExtParam,  input->NumExtParam,  MFX_EXTBUFF_FEIH265_INPUT);
    mfxExtFEIH265Output *pOut = (mfxExtFEIH265Output *) GetExtBuffer(output->ExtParam, output->NumExtParam, MFX_EXTBUFF_FEIH265_OUTPUT);

    sp->feiH265In.FEIOp     = pIn->feiIn.FEIOp;
    sp->feiH265In.FrameType = pIn->feiIn.FrameType;
    sp->feiH265In.RefIdx    = pIn->feiIn.RefIdx;

    /* we save a pointer to the mfxFEIH265Output struct allocated by the user */
    sp->feiH265Out = pOut->feiOut;

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

#endif  // AS_H265FEI_PLUGIN

#endif  // MFX_VA

/* EOF */

