/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#if defined (MFX_VA)

#include "mfx_h265_enc_fei.h"

namespace H265Enc {

FeiContext::FeiContext(const H265VideoParam *param, VideoCORE *core)
{
    /* set once at init */
    m_feiParam.Width         = param->SourceWidth;
    m_feiParam.Height        = param->SourceHeight;
    m_feiParam.MaxCUSize     = param->MaxCUSize;
    m_feiParam.MPMode        = param->partModes;
    m_feiParam.NumRefFrames  = param->csps->sps_max_dec_pic_buffering[0];

    feiInIdx = 0; // 0 or 1

    /* see above - Cm presets usually just pick the max (1 candidate) */
    Ipp32s maxIntraNumCand = 1;
    for (Ipp32s j = 0; j < 4; j++) {    // among all frame types
        for (Ipp32s i = 2; i <= 6; i++) {
            if (param->num_cand_0[j][i] > maxIntraNumCand)
                maxIntraNumCand = param->num_cand_0[j][i];
        }
    }
    m_feiParam.NumIntraModes = maxIntraNumCand;

    for (Ipp32s j = 0; j < 2; j++)
        ResetFEIFrame(&m_feiFrame[j]);

    mfxStatus err;
    if ((err = H265FEI_Init(&m_feiH265, &m_feiParam, core)) != MFX_ERR_NONE)
        throw std::exception();

    feiH265Out = &m_feiH265Out;
}


FeiContext::~FeiContext()
{
///    H265FEI_Close();
}

void FeiContext::ResetFEIFrame(FEIFrame *feiFrame)
{
    /* zero out all "done" fields, set order to -1 to indicate uninitialized */
    memset(feiFrame, 0, sizeof(FEIFrame));
    feiFrame->EncOrder = -1;
}

/* NOTES:
 * - removed check in kernel for frames in list1 being present in list0 (B frames, see m_mapRefIdxL1ToL0)
 *     caller should do this to avoid redundant work
 */
void FeiContext::UpdateFrameStateFEI(mfxFEIH265Input *feiIn, H265Frame *frameIn, H265Frame *frameRef, Ipp32s refIdx, Ipp32s sliceType)
{
    Ipp32s i, refList;
    H265Frame *ref;

    //fprintf(stderr, "frameIn->eoc = %d, frameRef[0]->eoc = %d, frameRef[1]->eoc = %d\n", frameIn->EncOrderNum(), frameRef[0]->EncOrderNum(), (frameRef[1] ? frameRef[1]->EncOrderNum() : -1));

    /* allow FEI lib to skip missing frames */
    memset(&feiIn->FEIFrameIn, 0, sizeof(mfxFEIH265Frame));
    memset(&feiIn->FEIFrameRef, 0, sizeof(mfxFEIH265Frame));

    if (!frameIn)
        return;

    /* assume all slices are the same type (m_slices points to first/only slice) 
     * NOTE: with GeneralizedPB on, P frames are treated as special case of B frames - see SetSlice()
     */
    switch (sliceType) {
    case I_SLICE:
        feiIn->FrameType = FEI_I_FRAME; 
        break;
    case P_SLICE:
        feiIn->FrameType = FEI_P_FRAME; 
        break;
    case B_SLICE:
        feiIn->FrameType = FEI_B_FRAME; 
        break;
    }

    /* index of the buffer in mfxFEIH265Output to store results in for this reference frame/pair */
    feiIn->RefIdx = refIdx;

    /* copy state for current input frame */
    feiIn->FEIFrameIn.YPlane   = frameIn->y;
    feiIn->FEIFrameIn.YPitch   = frameIn->pitch_luma_bytes;
    feiIn->FEIFrameIn.EncOrder = frameIn->m_encOrder;

    /* copy state for 1 reference to be processed */
    if (frameRef) {
        feiIn->FEIFrameRef.YPlane   = frameRef->y;
        feiIn->FEIFrameRef.YPitch   = frameRef->pitch_luma_bytes;
        feiIn->FEIFrameRef.EncOrder = frameRef->m_encOrder;
    }

}

///void FeiContext::ProcessFrameFEI(mfxFEIH265Input *feiH265In, H265Frame *frameIn, H265Slice *sliceIn, FEIFrame *feiFrame, H265Frame **dpb, Ipp32s dpbSize, mfxFEISyncPoint *syncp, Ipp32s prevFrameDone)
void FeiContext::ProcessFrameFEI(mfxI32 feiInIdx, H265Frame *frameIn, H265Slice *sliceIn, H265Frame **dpb, Ipp32s dpbSize, Ipp8u prevFrameDone)
{
    Ipp32s refIdx, refIdxB;
    H265Frame *frameRef;

    char *tname = "ProcCur";
    if (!prevFrameDone) tname = "ProcNext";

    static mfxTraceStaticHandle _trace_static_handle;
    MFXTraceTask _mfx_trace_task(MFX_TRACE_PARAMS, MFX_TRACE_LEVEL_API, tname, false);

    // take right ptrs
    mfxFEIH265Input *feiH265In = &m_feiH265In[feiInIdx];
    FEIFrame *feiFrame = &m_feiFrame[feiInIdx];
    mfxFEISyncPoint *syncp = &m_syncp[feiInIdx];

    feiFrame->EncOrder = frameIn->m_encOrder;

    /* run intra analysis on all frame types */
    if (feiFrame->IntraDone == 0) {
        if (sliceIn->sliceIntraAngMode == INTRA_ANG_MODE_GRADIENT)
            feiH265In->FEIOp = (FEIOperation)(FEI_INTRA_DIST | FEI_INTRA_MODE);
        else
            feiH265In->FEIOp = (FEIOperation)(FEI_INTRA_DIST);
        UpdateFrameStateFEI(feiH265In, frameIn, 0, 0, sliceIn->slice_type);
        H265FEI_ProcessFrameAsync(m_feiH265, feiH265In, feiH265Out, syncp);
        feiFrame->IntraDone = 1;
    }

    if (sliceIn->slice_type == P_SLICE || sliceIn->slice_type == B_SLICE) {

        feiH265In->FEIOp = (FEIOperation)(FEI_INTER_ME/* | FEI_INTERPOLATE*/);
        for (refIdx = 0; refIdx < sliceIn->num_ref_idx[0]; refIdx++) {
            frameRef = frameIn->m_refPicList[0].m_refFrames[refIdx];

            if (!prevFrameDone && (frameRef->m_encOrder == feiFrame->EncOrder - 1))
                continue;

            feiFrame->RefEncOrder[0][refIdx] = frameRef->m_encOrder;

            if (feiFrame->RefDone[0][refIdx] == 0) {
                UpdateFrameStateFEI(feiH265In, frameIn, frameRef, refIdx, sliceIn->slice_type);
                H265FEI_ProcessFrameAsync(m_feiH265, feiH265In, feiH265Out, syncp);
                feiFrame->RefDone[0][refIdx] = 1;
            }  
        }

        if (sliceIn->slice_type == B_SLICE) {
            for (refIdxB = 0; refIdxB < sliceIn->num_ref_idx[1]; refIdxB++, refIdx++) {
                frameRef = frameIn->m_refPicList[1].m_refFrames[refIdxB];

                if (!prevFrameDone && (frameRef->m_encOrder == feiFrame->EncOrder - 1) )
                    continue;

                feiFrame->RefEncOrder[1][refIdxB] = frameRef->m_encOrder;

                if (feiFrame->RefDone[1][refIdxB] == 0) {
                    UpdateFrameStateFEI(feiH265In, frameIn, frameRef, refIdx, sliceIn->slice_type);
                    H265FEI_ProcessFrameAsync(m_feiH265, feiH265In, feiH265Out, syncp);
                    feiFrame->RefDone[1][refIdxB] = 1;
                }  
            }
        }
    }

    _mfx_trace_task.Stop();

    if (prevFrameDone) {
        H265FEI_SyncOperation(m_feiH265, m_syncp[feiInIdx], -1);
        ResetFEIFrame(&m_feiFrame[feiInIdx]);
    }

}

} // namespace

#endif // MFX_VA


#endif // MFX_ENABLE_H265_VIDEO_ENCODE
