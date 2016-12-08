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
#ifdef ALT_GAA_PLUGIN_API
    ,
    m_feiAlloc(),
    m_feiH265OutNew(),
    m_feiInIdx(),
    m_feiInEncOrderLast(),

    m_surfInSource(),
    m_surfInRecon(),
    
    m_numSourceSurfaces(),
    m_numReconSurfaces(),
    m_surfInSourceMap(),
    m_surfInReconMap(),

    m_surfIntraMode(),
    m_surfIntraDist(),
    m_surfInterDist(),
    m_surfInterMV(),
    m_surfInterp(),

    m_IntraMode(),
    m_IntraDist(),
    m_InterDist(),
    m_InterMV(),
    m_Interp()
#endif
{
    *sts = MFX_ERR_NONE;
} 

VideoENC_H265FEI::~VideoENC_H265FEI()
{
    Close();
}

#ifdef ALT_GAA_PLUGIN_API
mfxStatus VideoENC_H265FEI::AllocateAllSurfaces(void)
{
    mfxStatus err = MFX_ERR_NONE;

    /* fixed for now - these can be changed for deeper lookahead */
    m_numSourceSurfaces = 2;
    m_numReconSurfaces = (mfxI32)m_feiH265Param.NumRefFrames + 1;

    /* allocate 2 source input surfaces for lookahead */
    for (Ipp32s i = 0; i < m_numSourceSurfaces; i++) {
        err = AllocateInputSurface(&m_surfInSource[i]);
        m_surfInSourceMap[i] = -1;
        MFX_CHECK_STS(err);
    }

    /* allocate numRefFrames + 1 recon input surfaces  */
    for (Ipp32s i = 0; i < m_numReconSurfaces; i++) {
        err = AllocateInputSurface(&m_surfInRecon[i]);
        m_surfInReconMap[i] = -1;
        MFX_CHECK_STS(err);
    }

    /* allocate output surfaces */
    for (Ipp32s i = 0; i < 2; i++) {
        /* intra modes: 4x4, 8x8, 16x16, 32x32 */
        for (Ipp32s j = 0; j < 4; j++) {
            m_IntraMode[i][j] = (mfxU32 *)CM_ALIGNED_MALLOC(m_feiAlloc.IntraMode[j].size, m_feiAlloc.IntraMode[j].align);
            err = AllocateOutputSurface((mfxU8 *)m_IntraMode[i][j], &m_feiAlloc.IntraMode[j], &m_surfIntraMode[i][j]);
            MFX_CHECK_STS(err);
        }

        /* intra distortion */
        m_IntraDist[i] = (mfxFEIH265IntraDist *)CM_ALIGNED_MALLOC(m_feiAlloc.IntraDist.size, m_feiAlloc.IntraDist.align);
        err = AllocateOutputSurface((mfxU8 *)m_IntraDist[i], &m_feiAlloc.IntraDist, &m_surfIntraDist[i]);
        MFX_CHECK_STS(err);

        /* inter distortion and MV's */
        for (Ipp32s j = 0; j < (Ipp32s)m_feiH265Param.NumRefFrames; j++) {
            for (Ipp32s k = MFX_FEI_H265_BLK_32x32; k <= MFX_FEI_H265_BLK_8x8; k++) {
                m_InterDist[i][j][k] = (mfxU32 *)CM_ALIGNED_MALLOC(m_feiAlloc.InterDist[k].size, m_feiAlloc.InterDist[k].align);
                err = AllocateOutputSurface((mfxU8 *)m_InterDist[i][j][k], &m_feiAlloc.InterDist[k], &m_surfInterDist[i][j][k]);
                MFX_CHECK_STS(err);

                m_InterMV[i][j][k] = (mfxI16Pair *)CM_ALIGNED_MALLOC(m_feiAlloc.InterMV[k].size, m_feiAlloc.InterMV[k].align);
                err = AllocateOutputSurface((mfxU8 *)m_InterMV[i][j][k], &m_feiAlloc.InterMV[k], &m_surfInterMV[i][j][k]);
                MFX_CHECK_STS(err);
            }
        }
    }

    /* half-pel interpolated frames */
    for (Ipp32s i = 0; i < 2; i++) {
        for (Ipp32s j = 0; j < (Ipp32s)m_feiH265Param.NumRefFrames; j++) {
            for (Ipp32s k = 0; k < 3; k++) {
                m_Interp[i][j][k] = (mfxU8 *)CM_ALIGNED_MALLOC(m_feiAlloc.Interpolate[k].size, m_feiAlloc.Interpolate[k].align);
                err = AllocateOutputSurface((mfxU8 *)m_Interp[i][j][k], &m_feiAlloc.Interpolate[k], &m_surfInterp[i][j][k]);
                MFX_CHECK_STS(err);
            }
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoENC_H265FEI::FreeAllSurfaces(void)
{
    /* destroy input surfaces */
    for (Ipp32s i = 0; i < m_numSourceSurfaces; i++) {
        FreeSurface(m_surfInSource[i]);
    }

    for (Ipp32s i = 0; i < m_numReconSurfaces; i++) {
        FreeSurface(m_surfInRecon[i]);
    }

    /* destroy output surfaces and user memory */
    for (Ipp32s i = 0; i < 2; i++) {
        for (Ipp32s j = 0; j < 4; j++) {
            FreeSurface(m_surfIntraMode[i][j]);
            CM_ALIGNED_FREE(m_IntraMode[i][j]);
        }

        FreeSurface(m_surfIntraDist[i]);
        CM_ALIGNED_FREE(m_IntraDist[i]);

        for (Ipp32s j = 0; j < (Ipp32s)m_feiH265Param.NumRefFrames; j++) {
            for (Ipp32s k = MFX_FEI_H265_BLK_32x32; k <= MFX_FEI_H265_BLK_8x8; k++) {
                FreeSurface(m_surfInterDist[i][j][k]);
                FreeSurface(m_surfInterMV[i][j][k]);

                CM_ALIGNED_FREE(m_InterDist[i][j][k]);
                CM_ALIGNED_FREE(m_InterMV[i][j][k]);
            }
        }
    }

    for (Ipp32s i = 0; i < 2; i++) {
        for (Ipp32s j = 0; j < (Ipp32s)m_feiH265Param.NumRefFrames; j++) {
            for (Ipp32s k = 0; k < 3; k++) {
                FreeSurface(m_surfInterp[i][j][k]);
                CM_ALIGNED_FREE(m_Interp[i][j][k]);
            }
        }
    }

    return MFX_ERR_NONE;
}

mfxU32 VideoENC_H265FEI::MapOP(mfxU32 FEIOpPublic)
{
    mfxU32 FEIOpNew = 0;

    if (FEIOpPublic & MFX_FEI_H265_OP_NOP)          FEIOpNew |= MFX_FEI_H265_OP_NEW_NOP;
    if (FEIOpPublic & MFX_FEI_H265_OP_INTRA_MODE)   FEIOpNew |= MFX_FEI_H265_OP_NEW_INTRA_MODE;
    if (FEIOpPublic & MFX_FEI_H265_OP_INTRA_DIST)   FEIOpNew |= MFX_FEI_H265_OP_NEW_INTRA_DIST;
    if (FEIOpPublic & MFX_FEI_H265_OP_INTER_ME)     FEIOpNew |= MFX_FEI_H265_OP_NEW_INTER_ME;
    if (FEIOpPublic & MFX_FEI_H265_OP_INTERPOLATE)  FEIOpNew |= MFX_FEI_H265_OP_NEW_INTERPOLATE;
    
    return FEIOpNew;
}
    
mfxStatus VideoENC_H265FEI::GetInputSurface(mfxFEIH265Frame *inFrame, mfxHDL *surfIn, mfxI32 *surfInMap, mfxI32 numSurfaces)
{
    mfxI32 i, frameIdxOldest;

    for (i = 0; i < numSurfaces; i++) {
        if (surfInMap[i] == (mfxI32)inFrame->EncOrder)
            break;
    }

    /* frame already copied - set handle to surface */
    if (i < numSurfaces) {
        inFrame->copyFrame = 0;
        inFrame->surfIn = surfIn[i];
        return MFX_ERR_NONE;
    }
            
    /* frame not copied - replace oldest input frame in queue */
    frameIdxOldest = 0;
    for (i = 1; i < numSurfaces; i++) {
        if (surfInMap[i] < surfInMap[frameIdxOldest])
            frameIdxOldest = i;
    }
    surfInMap[frameIdxOldest] = inFrame->EncOrder;

    inFrame->copyFrame = 1;
    inFrame->surfIn = surfIn[frameIdxOldest];

    return MFX_ERR_NONE;
}

#endif

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

#ifdef ALT_GAA_PLUGIN_API
    m_feiH265Param.BitDepth      = 8;
    m_feiH265Param.TargetUsage   = 0;

    memset(&m_feiAlloc, 0, sizeof(mfxExtFEIH265Alloc));
    H265FEI_GetSurfaceDimensions(m_core, m_feiH265Param.Width, m_feiH265Param.Height, &m_feiAlloc);
#else
    // TODO - validate bounds
    m_feiH265Param.BitDepth      = pParams->BitDepth;
    m_feiH265Param.TargetUsage   = pParams->TargetUsage;
#endif

    /* validate parameters - update these limits as necessary in future versions */
    MFX_CHECK ((m_feiH265Param.Width > 0)         && (m_feiH265Param.Height > 0),     MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK ((m_feiH265Param.Width <= 4088)     && (m_feiH265Param.Height) <= 4088, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK ((m_feiH265Param.Width & 0x0f) == 0 && (m_feiH265Param.Height & 0x0f) == 0, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK ((m_feiH265Param.NumRefFrames <= MFX_FEI_H265_MAX_NUM_REF_FRAMES), MFX_ERR_INVALID_VIDEO_PARAM);
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

#ifdef ALT_GAA_PLUGIN_API
    err = AllocateAllSurfaces();
    m_feiInIdx = 0;
#endif

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

    if (!m_feiH265)
        return err;

#ifdef ALT_GAA_PLUGIN_API
    FreeAllSurfaces();
#endif
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
 
#ifdef ALT_GAA_PLUGIN_API
    H265FEI_ProcessFrameAsync(pFEI->m_feiH265, &(out->feiH265In), out->feiH265OutNew, &(out->feiH265SyncPoint));
#else
    H265FEI_ProcessFrameAsync(pFEI->m_feiH265, &(out->feiH265In), &(out->feiH265Out), &(out->feiH265SyncPoint));
#endif

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

    mfxStatus tskRes = H265FEI_SyncOperation(pFEI->m_feiH265, (out->feiH265SyncPoint), 0xFFFFFFFF);
    if (tskRes != MFX_ERR_NONE)
        return tskRes;

    H265FEI_DestroySavedSyncPoint(pFEI->m_feiH265, (out->feiH265SyncPoint));

    if (pParam)
    {
        delete (sAsyncParams *)pParam;
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

    sp->feiH265In.SaveSyncPoint = 1;    // automatically destroy sync points

    /* copy frame info into params struct */
    if (input->InSurface) {
        sp->feiH265In.FEIFrameIn.EncOrder = input->InSurface->Data.FrameOrder;
        sp->feiH265In.FEIFrameIn.PicOrder = input->InSurface->Data.FrameOrder;      // unused in FEI core currently, but leave field if needed later
#ifdef ALT_GAA_PLUGIN_API
        sp->feiH265In.FEIFrameIn.YPitch   = input->InSurface->Data.Pitch;
        sp->feiH265In.FEIFrameIn.YPlane   = input->InSurface->Data.Y;
        sp->feiH265In.FEIFrameIn.YBitDepth = 8;
#else
        sp->feiH265In.FEIFrameIn.YPitch   = input->InSurface->Data.PitchLow + ((mfxU32)(input->InSurface->Data.PitchHigh) << 16);
        sp->feiH265In.FEIFrameIn.YPlane   = input->InSurface->Data.Y;
        sp->feiH265In.FEIFrameIn.YBitDepth = input->InSurface->Info.BitDepthLuma;
#endif
        MFX_CHECK_NULL_PTR1(sp->feiH265In.FEIFrameIn.YPlane);
    } 

    if (input->L0Surface) {
        MFX_CHECK_NULL_PTR1(input->L0Surface[0]);
        sp->feiH265In.FEIFrameRef.EncOrder = input->L0Surface[0]->Data.FrameOrder;
        sp->feiH265In.FEIFrameRef.PicOrder = input->L0Surface[0]->Data.FrameOrder;
#ifdef ALT_GAA_PLUGIN_API
        sp->feiH265In.FEIFrameRef.YPitch   = input->L0Surface[0]->Data.Pitch;
        sp->feiH265In.FEIFrameRef.YPlane   = input->L0Surface[0]->Data.Y;
        sp->feiH265In.FEIFrameRef.YBitDepth = 8;
#else
        sp->feiH265In.FEIFrameRef.YPitch   = input->L0Surface[0]->Data.PitchLow + ((mfxU32)(input->L0Surface[0]->Data.PitchHigh) << 16);
        sp->feiH265In.FEIFrameRef.YPlane   = input->L0Surface[0]->Data.Y;
        sp->feiH265In.FEIFrameRef.YBitDepth = input->L0Surface[0]->Info.BitDepthLuma;
#endif
        MFX_CHECK_NULL_PTR1(sp->feiH265In.FEIFrameRef.YPlane);
    }

    /* copy parameters from input/output mfxExtBufs into params struct */ 
    mfxExtFEIH265Input  *pIn  = (mfxExtFEIH265Input *)  GetExtBuffer(input->ExtParam,  input->NumExtParam,  MFX_EXTBUFF_FEI_H265_INPUT);
    mfxExtFEIH265Output *pOut = (mfxExtFEIH265Output *) GetExtBuffer(output->ExtParam, output->NumExtParam, MFX_EXTBUFF_FEI_H265_OUTPUT);

    MFX_CHECK_NULL_PTR1(pIn);
    MFX_CHECK_NULL_PTR1(pOut);

#ifdef ALT_GAA_PLUGIN_API
    mfxU32 k;

    sp->feiH265In.FEIOp     = MapOP(pIn->FEIOp);
    sp->feiH265In.FrameType = pIn->FrameType;

    /* when input frame is updated, flip double buffer (max 2 frames in flight) */
    if (sp->feiH265In.FEIFrameIn.EncOrder > m_feiInEncOrderLast) {
        m_feiInIdx = 1 - m_feiInIdx;
        m_feiInEncOrderLast = sp->feiH265In.FEIFrameIn.EncOrder;
    }

    //fprintf(stderr, "Inframe % 4d, feiInIdx = %d\n", sp->feiH265In.FEIFrameIn.EncOrder, m_feiInIdx);
    
    if (sp->feiH265In.FEIOp & (MFX_FEI_H265_OP_NEW_INTRA_MODE | MFX_FEI_H265_OP_NEW_INTRA_DIST | MFX_FEI_H265_OP_NEW_INTER_ME)) {
        /* get source frame surface, uploading to GPU if necessary */
        GetInputSurface(&sp->feiH265In.FEIFrameIn, m_surfInSource, m_surfInSourceMap, m_numSourceSurfaces);
        if (sp->feiH265In.FEIFrameIn.copyFrame)    
            sp->feiH265In.FEIOp |= MFX_FEI_H265_OP_NEW_GPU_COPY_SRC;
    }

    if (sp->feiH265In.FEIOp & (MFX_FEI_H265_OP_NEW_INTER_ME | MFX_FEI_H265_OP_NEW_INTERPOLATE)) {
        /* get recon frame surface, uploading to GPU if necessary */
        GetInputSurface(&sp->feiH265In.FEIFrameRef, m_surfInRecon,  m_surfInReconMap,  m_numReconSurfaces);
        if (sp->feiH265In.FEIFrameRef.copyFrame)    
            sp->feiH265In.FEIOp |= MFX_FEI_H265_OP_NEW_GPU_COPY_REF;
    }

    /* pass pointers to output surfaces */
    for (k = 0; k < 4; k++) {
        m_feiH265OutNew[m_feiInIdx][pIn->RefIdx].SurfIntraMode[k] = m_surfIntraMode[m_feiInIdx][k];              
    }
    m_feiH265OutNew[m_feiInIdx][pIn->RefIdx].SurfIntraDist = m_surfIntraDist[m_feiInIdx];

    /* pass pointers to output surfaces */
    for (k = MFX_FEI_H265_BLK_32x32; k <= MFX_FEI_H265_BLK_8x8; k++) {
        m_feiH265OutNew[m_feiInIdx][pIn->RefIdx].SurfInterDist[k] = m_surfInterDist[m_feiInIdx][pIn->RefIdx][k];
        m_feiH265OutNew[m_feiInIdx][pIn->RefIdx].SurfInterMV[k]   = m_surfInterMV[m_feiInIdx][pIn->RefIdx][k];
    }

    // TODO - proper queue for interp ref frames (no need to process same frame more than once) - this would be handled by caller in new API
    for (k = 0; k < 3; k++) {
        m_feiH265OutNew[m_feiInIdx][pIn->RefIdx].SurfInterp[k]    = m_surfInterp[m_feiInIdx][pIn->RefIdx][k];
    }

    /* repeat, filling in pointers contained in caller's struct */
    pOut->feiOut->IntraDist = m_IntraDist[m_feiInIdx];

    pOut->feiOut->IntraModes4x4   = m_IntraMode[m_feiInIdx][0];     pOut->feiOut->IntraPitch4x4   = m_feiAlloc.IntraMode[0].pitch / sizeof(mfxU32);
    pOut->feiOut->IntraModes8x8   = m_IntraMode[m_feiInIdx][1];     pOut->feiOut->IntraPitch8x8   = m_feiAlloc.IntraMode[1].pitch / sizeof(mfxU32);
    pOut->feiOut->IntraModes16x16 = m_IntraMode[m_feiInIdx][2];     pOut->feiOut->IntraPitch16x16 = m_feiAlloc.IntraMode[2].pitch / sizeof(mfxU32);
    pOut->feiOut->IntraModes32x32 = m_IntraMode[m_feiInIdx][3];     pOut->feiOut->IntraPitch32x32 = m_feiAlloc.IntraMode[3].pitch / sizeof(mfxU32);
    pOut->feiOut->IntraMaxModes   = m_feiH265Param.NumIntraModes;

    pOut->feiOut->IntraDist       = m_IntraDist[m_feiInIdx];        pOut->feiOut->IntraPitch      = m_feiAlloc.IntraDist.pitch / sizeof(mfxFEIH265IntraDist);

    for (Ipp32s k = MFX_FEI_H265_BLK_32x32; k <= MFX_FEI_H265_BLK_8x8; k++) {
        pOut->feiOut->Dist[pIn->RefIdx][k] = m_InterDist[m_feiInIdx][pIn->RefIdx][k];
        pOut->feiOut->MV[pIn->RefIdx][k]   = m_InterMV[m_feiInIdx][pIn->RefIdx][k];

        pOut->feiOut->PitchMV[k]   = m_feiAlloc.InterMV[k].pitch / sizeof(mfxI16Pair);
        pOut->feiOut->PitchDist[k] = m_feiAlloc.InterDist[k].pitch / sizeof(mfxU32);
    }

    for (Ipp32s k = 0; k < 3; k++) {
        pOut->feiOut->Interp[pIn->RefIdx][k] = m_Interp[m_feiInIdx][pIn->RefIdx][k];
        pOut->feiOut->InterpolatePitch  = m_feiAlloc.Interpolate[k].pitch;
    }

    pOut->feiOut->PaddedWidth =  (m_feiH265Param.Width + 15) & ~0x0f;
    pOut->feiOut->PaddedHeight = (m_feiH265Param.Height + 15) & ~0x0f;

    pOut->feiOut->InterpolateWidth =  pOut->feiOut->PaddedWidth + 2*MFX_FEI_H265_INTERP_BORDER;
    pOut->feiOut->InterpolateHeight = pOut->feiOut->PaddedHeight + 2*MFX_FEI_H265_INTERP_BORDER;

    sp->feiH265OutNew = &m_feiH265OutNew[m_feiInIdx][pIn->RefIdx];
    sp->feiH265Out = pOut->feiOut;
#else
    sp->feiH265In.FEIOp     = pIn->FEIOp;
    sp->feiH265In.FrameType = pIn->FrameType;

    sp->feiH265In.FEIFrameIn.surfIn  = pIn->SurfInSrc;
    sp->feiH265In.FEIFrameRef.surfIn = pIn->SurfInRec;

    /* save a local copy of the mfxExtFEIH265Output struct allocated by the user (need copy since this operates asynchronously with FEI core) */
    sp->feiH265Out = *pOut;
#endif

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

#ifndef ALT_GAA_PLUGIN_API
        pParams->BitDepth = 1;
        pParams->TargetUsage = 1;
#endif
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

#ifndef ALT_GAA_PLUGIN_API
        pParams_out->BitDepth      = pParams_in->BitDepth;
        pParams_out->TargetUsage   = pParams_in->TargetUsage;
#endif


        MFX_CHECK ((pParams_out->MPMode >= 1) && (pParams_out->MPMode <= 3), MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK ((pParams_out->NumIntraModes == 1), MFX_ERR_INVALID_VIDEO_PARAM);

#ifndef ALT_GAA_PLUGIN_API
        /* fill out extBuf with required surface dimensions so user can allocate memory */
        mfxExtFEIH265Alloc *pAlloc_in  = (mfxExtFEIH265Alloc *)GetExtBuffer(in->ExtParam,  in->NumExtParam, MFX_EXTBUFF_FEI_H265_ALLOC);
        mfxExtFEIH265Alloc *pAlloc_out = (mfxExtFEIH265Alloc *)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_FEI_H265_ALLOC);

        MFX_CHECK_NULL_PTR1(pAlloc_in);
        MFX_CHECK_NULL_PTR1(pAlloc_out);

        H265FEI_GetSurfaceDimensions(pCore, out->mfx.FrameInfo.Width, out->mfx.FrameInfo.Height, pAlloc_out);
#endif
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

