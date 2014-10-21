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

#ifdef SAVE_FEI_STATE

static char *fileNameParams =     "h265_fei_params.txt";
static char *fileNameIntraDist  = "h265_fei_intradist.txt";
static char *fileNameIntraAngle = "h265_fei_intramodes.txt";
static char *fileNameInterMV    = "h265_fei_intermv.txt";
static char *fileNameInterDist  = "h265_fei_interdist.txt";
static const char *fileNameInterpolate[3] = { 
    "h265_fei_interp_H.yuv",
    "h265_fei_interp_V.yuv",
    "h265_fei_interp_D.yuv",
};

static FILE *outfileParams = 0; 
static FILE *outfileIntraModes = 0;
static FILE *outfileIntraDist = 0;
static FILE *outfileInterMV = 0;
static FILE *outfileInterDist = 0;
static FILE *outfileInterpolate[3] = { 0 };

static unsigned char bwBuf[2048];

void InitOutputFiles(mfxFEIH265Param *param)
{
    int i;

    outfileParams = fopen(fileNameParams, "wt");
    if (!outfileParams) {
        printf("Warning - failed to open %s", fileNameParams);
        return;
    }

    fprintf(outfileParams, "Width=%d\n",         param->Width);
    fprintf(outfileParams, "Height=%d\n",        param->Height);
    fprintf(outfileParams, "MaxCUSize=%d\n",     param->MaxCUSize);
    fprintf(outfileParams, "MPMode=%d\n",        param->MPMode);
    fprintf(outfileParams, "NumRefFrames=%d\n",  param->NumRefFrames);
    fprintf(outfileParams, "NumIntraModes=%d\n", param->NumIntraModes);
    fprintf(outfileParams, "\n");

    outfileIntraDist = fopen(fileNameIntraDist, "wt");
    if (!outfileIntraDist) {
        printf("Warning - failed to open %s", fileNameIntraDist);
        return;
    }
    
    outfileIntraModes = fopen(fileNameIntraAngle, "wt");
    if (!outfileIntraModes) {
        printf("Warning - failed to open %s", fileNameIntraAngle);
        return;
    }

    outfileInterMV = fopen(fileNameInterMV, "wt");
    if (!outfileInterMV) {
        printf("Warning - failed to open %s", fileNameInterMV);
        return;
    }
    
    outfileInterDist = fopen(fileNameInterDist, "wt");
    if (!outfileInterDist) {
        printf("Warning - failed to open %s", fileNameInterDist);
        return;
    }

    /* half-pel interpolated places (3 files - H,V,D) */
    for (i = 0; i < 3; i++) {
        outfileInterpolate[i] = fopen(fileNameInterpolate[i], "wb");
        if (!outfileInterpolate[i]) {
            printf("Warning - failed to open %s", fileNameInterpolate[i]);
            return;
        }
    }

    /* fill U/V planes with 0x80 (YUV420)) */
    memset(bwBuf, 0x80, sizeof(bwBuf));
}

void AddFrameParamsFile(mfxFEIH265Input *feiH265In, H265Frame *frameIn, H265Slice *sliceIn, FEIFrame *feiFrame, mfxFEISyncPoint *syncp, Ipp32s prevFrameDone, Ipp32s RefPicOrder[2][FEI_MAX_NUM_REF_FRAMES])
{
    int i;

    FEIFrameType frameType;
    switch (sliceIn->slice_type) {
    case I_SLICE:   frameType = FEI_I_FRAME; break;
    case P_SLICE:   frameType = FEI_P_FRAME; break;
    case B_SLICE:   frameType = FEI_B_FRAME; break;
    }

    fprintf(outfileParams, "NewFrame\n");
    fprintf(outfileParams, "FrameType = %d\n", frameType);
    fprintf(outfileParams, "EncOrder = %d\n",  frameIn->m_encOrder);
    fprintf(outfileParams, "PicOrder = %d\n",  frameIn->m_poc);
    
    fprintf(outfileParams, "RefNum = %d\n", (sliceIn->num_ref_idx[0] + sliceIn->num_ref_idx[1]));
    fprintf(outfileParams, "RefEncOrder = ");
    for (i = 0; i < sliceIn->num_ref_idx[0]; i++)
       fprintf(outfileParams, "%d,", feiFrame->RefEncOrder[0][i]);
    for (i = 0; i < sliceIn->num_ref_idx[1]; i++)
       fprintf(outfileParams, "%d,", feiFrame->RefEncOrder[1][i]);
    fprintf(outfileParams, "\n");
    
    fprintf(outfileParams, "RefPicOrder = ");
    for (i = 0; i < sliceIn->num_ref_idx[0]; i++)
       fprintf(outfileParams, "%d,", RefPicOrder[0][i]);
    for (i = 0; i < sliceIn->num_ref_idx[1]; i++)
       fprintf(outfileParams, "%d,", RefPicOrder[1][i]);
    fprintf(outfileParams, "\n");

    fprintf(outfileParams, "EndFrame\n\n");
}

void WriteFrameIntraModes(mfxFEIH265Output *feiOut)
{
    int i, x, y;

    fprintf(outfileIntraModes, "Intra Modes 4x4\n");
    for (y = 0; y < feiOut->PaddedHeight / 4; y++) {
        for (x = 0; x < feiOut->PaddedWidth / 4; x++) {
            fprintf(outfileIntraModes, "block [% 5d, % 5d]: ", y*4, x*4);
            for (i = 0; i < feiOut->IntraMaxModes; i++) {
                fprintf(outfileIntraModes, "%d ",   feiOut->IntraModes4x4[feiOut->IntraMaxModes*(y*feiOut->IntraPitch4x4 + x) + i]);
            }
            fprintf(outfileIntraModes, "\n");
        }
    }
    fprintf(outfileIntraModes, "\n");

    fprintf(outfileIntraModes, "Intra Modes 8x8\n");
    for (y = 0; y < feiOut->PaddedHeight / 8; y++) {
        for (x = 0; x < feiOut->PaddedWidth / 8; x++) {
            fprintf(outfileIntraModes, "block [% 5d, % 5d]: ", y*8, x*8);
            for (i = 0; i < feiOut->IntraMaxModes; i++) {
                fprintf(outfileIntraModes, "%d ", feiOut->IntraModes8x8[feiOut->IntraMaxModes*(y*feiOut->IntraPitch8x8 + x) + i]);
            }
            fprintf(outfileIntraModes, "\n");
        }
    }
    fprintf(outfileIntraModes, "\n");

    fprintf(outfileIntraModes, "Intra Modes 16x16\n");
    for (y = 0; y < feiOut->PaddedHeight / 16; y++) {
        for (x = 0; x < feiOut->PaddedWidth / 16; x++) {
            fprintf(outfileIntraModes, "block [% 5d, % 5d]: ", y*16, x*16);
            for (i = 0; i < feiOut->IntraMaxModes; i++) {
                fprintf(outfileIntraModes, "%d ", feiOut->IntraModes16x16[feiOut->IntraMaxModes*(y*feiOut->IntraPitch16x16 + x) + i]);
            }
            fprintf(outfileIntraModes, "\n");
        }
    }
    fprintf(outfileIntraModes, "\n");

    fprintf(outfileIntraModes, "Intra Modes 32x32\n");
    for (y = 0; y < feiOut->PaddedHeight / 32; y++) {
        for (x = 0; x < feiOut->PaddedWidth / 32; x++) {
            fprintf(outfileIntraModes, "block [% 5d, % 5d]: ", y*32, x*32);
            for (i = 0; i < feiOut->IntraMaxModes; i++) {
                fprintf(outfileIntraModes, "%d ", feiOut->IntraModes32x32[feiOut->IntraMaxModes*(y*feiOut->IntraPitch32x32 + x) + i]);
            }
            fprintf(outfileIntraModes, "\n");
        }
    }
    fprintf(outfileIntraModes, "\n");
}

void WriteFrameIntraDist(mfxFEIH265Output *feiOut)
{
    int x, y;

    fprintf(outfileIntraDist, "Intra Distortion 16x16\n");
    for (y = 0; y < feiOut->PaddedHeight / 16; y++) {
        fprintf(outfileIntraDist, "Row % 5d", y*16);
        for (x = 0; x < feiOut->PaddedWidth / 16; x++) {
            fprintf(outfileIntraDist, "% 6d", feiOut->IntraDist[y*feiOut->IntraPitch + x].Dist);
        }
        fprintf(outfileIntraDist, "\n");
    }
    fprintf(outfileIntraDist, "\n");

}

void WriteFrameInterLarge(mfxFEIH265Output *feiOut, int refIdx, int blockSize)
{
    int x, y, w, h, i;

    int pitchMv   = feiOut->PitchMV[blockSize];
    int pitchDist = feiOut->PitchDist[blockSize];

    switch (blockSize) {
    case FEI_32x32:
        w = 32;
        h = 32;
        break;
    case FEI_32x16:
        w = 32;
        h = 16;
        break;
    case FEI_16x32:
        w = 16;
        h = 32;
        break;
    }

    /* for large blocks, 9 dist estimates are returned (spaced 16 units apart) 
     * dist[0] through dist[8] correspond to the 9 possible MV's centered around x = MV.x and y = MV.y
     * 
     *  (x-1,y-1) | (x+0,y-1) | (x+1,y-1)        dist[0] | dist[1] | dist[2]
     *  (x-1,y+0) | (x+0,y+0) | (x+1,y+0)  -->   dist[3] | dist[4] | dist[5] 
     *  (x-1,y+1) | (x+0,y+1) | (x+1,y+1)        dist[6] | dist[7] | dist[8]
     *
     * dist[9] through dist[15] are undefined (padding)
     * 
     * NOTE: this is only performed for 32x16 and 16x32 blocks when mfxFEIH265Param->MPMode is > 1
     */
    fprintf(outfileInterMV,   "MV - %dx%d blocks\n", w, h);
    fprintf(outfileInterDist, "Dist - %dx%d blocks\n", w, h);

    for (y = 0; y < feiOut->PaddedHeight / h; y++) {
        fprintf(outfileInterMV, "Row % 5d: ", y*h);
        fprintf(outfileInterDist, "Row % 5d: ", y*h);
        for (x = 0; x < feiOut->PaddedWidth / w; x++) {
            fprintf(outfileInterMV, "[% 4d, % 4d] ", feiOut->MV[refIdx][blockSize][y*pitchMv + x].x, feiOut->MV[refIdx][blockSize][y*pitchMv + x].y);
            for (i = 0; i < 9; i++)
                fprintf(outfileInterDist, "% 6d ", (int)feiOut->Dist[refIdx][blockSize][y*pitchDist + 16*x + i]);
            fprintf(outfileInterDist, "|| ");
        }
        fprintf(outfileInterMV, "\n");
        fprintf(outfileInterDist, "\n");
    }
    fprintf(outfileInterMV, "\n");
    fprintf(outfileInterDist, "\n");
}

void WriteFrameInterSmall(mfxFEIH265Output *feiOut, int refIdx, int blockSize)
{
    int x, y, w, h;

    int pitchMv   = feiOut->PitchMV[blockSize];
    int pitchDist = feiOut->PitchDist[blockSize];

    switch (blockSize) {
    case FEI_16x16:
        w = 16;
        h = 16;
        break;
    case FEI_8x8:
        w = 8;
        h = 8;
        break;
    }

    fprintf(outfileInterMV,   "MV - %dx%d blocks\n", w, h);
    fprintf(outfileInterDist, "Dist - %dx%d blocks\n", w, h);
    for (y = 0; y < feiOut->PaddedHeight / h; y++) {
        fprintf(outfileInterMV, "Row % 5d: ", y*h);
        fprintf(outfileInterDist, "Row % 5d: ", y*h);
        for (x = 0; x < feiOut->PaddedWidth / w; x++) {
            fprintf(outfileInterMV, "[% 4d, % 4d] ", feiOut->MV[refIdx][blockSize][y*pitchMv + x].x, feiOut->MV[refIdx][blockSize][y*pitchMv + x].y);
            fprintf(outfileInterDist, "% 6d", (int)feiOut->Dist[refIdx][blockSize][y*pitchDist + x]);
        }
        fprintf(outfileInterMV, "\n");
        fprintf(outfileInterDist, "\n");
    }
    fprintf(outfileInterMV, "\n");
    fprintf(outfileInterDist, "\n");
}

void WriteFrameInterpolate(mfxFEIH265Output *feiOut, int refIdx)
{
    int i, j;

    /* write Y plane to file, fill U/V planes with 0x80 (zero in 8-bit YUV420) */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < feiOut->InterpolateHeight; j++)
            fwrite(feiOut->Interp[refIdx][i] + j*feiOut->InterpolatePitch, 1, feiOut->InterpolateWidth, outfileInterpolate[i]);
        for (j = 0; j < feiOut->InterpolateHeight / 2; j++)
            fwrite(bwBuf, 1, feiOut->InterpolateWidth, outfileInterpolate[i]);
    }
}

/* refList = 0 or 1 (ref frames) */
void WriteFrameInter(mfxFEIH265Output *feiOut, int refIdx, int puSize, int MPMode)
{
    WriteFrameInterSmall(feiOut, refIdx, FEI_8x8);
    WriteFrameInterSmall(feiOut, refIdx, FEI_16x16);
    WriteFrameInterLarge(feiOut, refIdx, FEI_32x32);
    if (MPMode > 1) {
        /* non-square blocks */
        WriteFrameInterLarge(feiOut, refIdx, FEI_16x32);
        WriteFrameInterLarge(feiOut, refIdx, FEI_32x16);
    }
}

void CloseOutputFiles(void)
{
    if (outfileParams)
        fclose(outfileParams);
    
    outfileParams = 0;
}

#endif  // SAVE_FEI_STATE

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

#ifdef SAVE_FEI_STATE
    m_feiParam.MPMode = 2;
    m_feiParam.NumIntraModes = 1;
    InitOutputFiles(&m_feiParam);
#endif

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
#ifdef SAVE_FEI_STATE
        CloseOutputFiles();
#endif
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

#ifdef SAVE_FEI_STATE
        /* force it to always produce intra for testing */
        feiH265In->FEIOp = (FEIOperation)(FEI_INTRA_DIST | FEI_INTRA_MODE);
#endif

        UpdateFrameStateFEI(feiH265In, frameIn, 0, 0, sliceIn->slice_type);
        H265FEI_ProcessFrameAsync(m_feiH265, feiH265In, feiH265Out, syncp);
        feiFrame->IntraDone = 1;
    }

    if (sliceIn->slice_type == P_SLICE || sliceIn->slice_type == B_SLICE) {

        feiH265In->FEIOp = (FEIOperation)(FEI_INTER_ME/* | FEI_INTERPOLATE*/);

#ifdef SAVE_FEI_STATE
        /* force it to always produce interpolated frames for testing */
        feiH265In->FEIOp = (FEIOperation)(FEI_INTER_ME | FEI_INTERPOLATE);
#endif

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

#ifdef SAVE_FEI_STATE
    if (prevFrameDone) {
        /* requires that GPU operation was done */
        H265FEI_SyncOperation(m_feiH265, *syncp, -1);

        /* for testing, we need the display order to load correct frame from recon.yuv */
        Ipp32s RefPicOrder[2][FEI_MAX_NUM_REF_FRAMES]; 
        for (refIdx = 0; refIdx < sliceIn->num_ref_idx[0]; refIdx++)
            RefPicOrder[0][refIdx] = frameIn->m_refPicList[0].m_refFrames[refIdx]->m_poc;
        for (refIdx = 0; refIdx < sliceIn->num_ref_idx[1]; refIdx++)
            RefPicOrder[1][refIdx] = frameIn->m_refPicList[1].m_refFrames[refIdx]->m_poc;

        AddFrameParamsFile(feiH265In, frameIn, sliceIn, feiFrame, syncp, prevFrameDone, RefPicOrder);
        
        WriteFrameIntraModes(feiH265Out);

        if (sliceIn->slice_type == P_SLICE || sliceIn->slice_type == B_SLICE) {
            int i;
            WriteFrameIntraDist(feiH265Out);

            for (i = 0; i < sliceIn->num_ref_idx[0] + sliceIn->num_ref_idx[1]; i++) {
                WriteFrameInterSmall(feiH265Out, i, FEI_8x8);
                WriteFrameInterSmall(feiH265Out, i, FEI_16x16);

                /* large blocks enabled */
                if (m_feiParam.MaxCUSize >= 32)
                    WriteFrameInterLarge(feiH265Out, i, FEI_32x32);
                /* non-square blocks enabled */
                if (m_feiParam.MaxCUSize >= 32 && m_feiParam.MPMode > 1) {
                    WriteFrameInterLarge(feiH265Out, i, FEI_16x32);
                    WriteFrameInterLarge(feiH265Out, i, FEI_32x16);
                }

                WriteFrameInterpolate(feiH265Out, i);
            }
        }
    }
#endif

    _mfx_trace_task.Stop();

    if (prevFrameDone) {
#ifndef SAVE_FEI_STATE
        H265FEI_SyncOperation(m_feiH265, m_syncp[feiInIdx], -1);
#endif
        ResetFEIFrame(&m_feiFrame[feiInIdx]);
    }

}

} // namespace

#endif // MFX_VA


#endif // MFX_ENABLE_H265_VIDEO_ENCODE
