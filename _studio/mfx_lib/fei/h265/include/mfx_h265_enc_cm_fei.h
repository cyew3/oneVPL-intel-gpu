//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_H265_ENC_CM_FEI_H
#define _MFX_H265_ENC_CM_FEI_H

#include "mfx_h265_fei.h"
#include "cmrt_cross_platform.h"

#pragma warning(disable: 4351)

/* generic declaration in mfxcommon.h, defined here */
struct _mfxSyncPoint {
    CmEvent *lastEvent;
    void *feiOut;
    mfxI32 curIdx;
};

namespace H265Enc {

#define FEI_DEPTH   2   /* max number of frames in flight (curr, next) */

enum H265VMEMode {
    VME_MODE_SMALL = 0,
    VME_MODE_LARGE,
    VME_MODE_REFINE,
};

enum H265HMELevel {
    HME_LEVEL_LOW = 0,  // 4x
    HME_LEVEL_HIGH,     // 16x
};

typedef struct {
    Ipp32s       encOrder;
    Ipp32s       picOrder;
    CmSurface2D *bufOrig;
    CmSurface2D *bufDown2x;
    CmSurface2D *bufDown4x;
    CmSurface2D *bufDown8x;
    CmSurface2D *bufDown16x;

    CmSurface2DUP *bufHPelH;
    CmSurface2DUP *bufHPelV;
    CmSurface2DUP *bufHPelD;
} PicBufGpu;

/* internal - the public API only returns list of top modes now */
typedef struct
{
    mfxU16 Histogram[FEI_MAX_INTRA_MODES + 2];  /* 33 + 2 for DC and planar */
    mfxU16 reserved[5];
} MbIntraGrad;

class H265CmCtx {

private:
    /* internal helper functions */
    Ipp32s GetCurIdx(int encOrder);
    Ipp32s GetGPUBuf(mfxFEIH265Input *feiIn, Ipp32s poc, Ipp32s getRef);
    Ipp32s AddGPUBufInput(mfxFEIH265Input *feiIn, mfxFEIH265Frame *frameIn, int idx);
    Ipp32s AddGPUBufRef(mfxFEIH265Input *feiIn, mfxFEIH265Frame *frameRef, int idx);
    void RunVmeKernel(CmEvent **lastEvent, CmSurface2DUP **dist, CmSurface2DUP **mv, PicBufGpu *picBufInput, PicBufGpu *picBufRef);

    /* Cm elements */
    CmDevice * device;
    CmQueue * queue;
    CmProgram * program;
    CmTask * task;
    CmSurface2DUP * mbIntraDist[FEI_DEPTH];
    CmSurface2DUP * mbIntraGrad4x4[FEI_DEPTH];
    CmSurface2DUP * mbIntraGrad8x8[FEI_DEPTH];
    CmSurface2DUP * mbIntraGrad16x16[FEI_DEPTH];
    CmSurface2DUP * mbIntraGrad32x32[FEI_DEPTH];
    CmSurface2DUP * distGpu[FEI_DEPTH][FEI_MAX_NUM_REF_FRAMES][FEI_BLK_MAX];
    CmSurface2DUP * mvGpu[FEI_DEPTH][FEI_MAX_NUM_REF_FRAMES][FEI_BLK_MAX];
    CmBuffer * curbe;
    CmBuffer * me1xControl;
    CmBuffer * me2xControl;
    CmBuffer * me4xControl;
    CmBuffer * me8xControl;
    CmBuffer * me16xControl;
    CmEvent * lastEvent[FEI_DEPTH];

    /* Cm kernels to load  */
    CmKernel * kernelDownSample2tiers;
    CmKernel * kernelDownSample4tiers;
    CmKernel * kernelMeIntra;
    CmKernel * kernelGradient;
    CmKernel * kernelMe16;
    CmKernel * kernelMe32;
    CmKernel * kernelRefine32x32;
    CmKernel * kernelRefine32x16;
    CmKernel * kernelRefine16x32;
    CmKernel * kernelInterpolateFrame;
    CmKernel * kernelIme;
    CmKernel * kernelImeWithPred;

    /* CPU-side: user-provided memory */
    mfxU32                um_intraPitch;
    mfxFEIH265IntraDist * um_mbIntraDist[FEI_DEPTH];
    mfxI32    * um_mbIntraGrad4x4[FEI_DEPTH];
    mfxI32    * um_mbIntraGrad8x8[FEI_DEPTH];
    mfxI32    * um_mbIntraGrad16x16[FEI_DEPTH];
    mfxI32    * um_mbIntraGrad32x32[FEI_DEPTH];
    mfxU32      um_pitchGrad4x4[FEI_DEPTH];
    mfxU32      um_pitchGrad8x8[FEI_DEPTH];
    mfxU32      um_pitchGrad16x16[FEI_DEPTH];
    mfxU32      um_pitchGrad32x32[FEI_DEPTH];

    mfxU32         * um_distCpu[FEI_DEPTH][FEI_MAX_NUM_REF_FRAMES][FEI_BLK_MAX];
    mfxI16Pair     * um_mvCpu[FEI_DEPTH][FEI_MAX_NUM_REF_FRAMES][FEI_BLK_MAX];
    mfxU32           um_pitchDist[FEI_BLK_MAX];
    mfxU32           um_pitchMv[FEI_BLK_MAX];

    /* interpolated frames correspond to ref buffer, so need 1 extra for lookahead */
    mfxU32           um_interpolatePitch;
    mfxU8          * um_interpolateData[FEI_MAX_NUM_REF_FRAMES+1][3];
    mfxU8          * um_pInterpolateData[FEI_DEPTH][2][FEI_MAX_NUM_REF_FRAMES+1][3];

    mfxI16         * um_mbIntraModeTop4[FEI_DEPTH];
    mfxI16         * um_mbIntraModeTop8[FEI_DEPTH];
    mfxI16         * um_mbIntraModeTop16[FEI_DEPTH];
    mfxI16         * um_mbIntraModeTop32[FEI_DEPTH];

    /* internal buffers to track input and ref frames that have been uploaded to GPU (avoid redundant work) */
    PicBufGpu picBufInput[FEI_DEPTH];
    PicBufGpu picBufRef[FEI_MAX_NUM_REF_FRAMES + 1];

    /* set once at init */
    mfxU32 width;
    mfxU32 height;
    mfxU32 numRefFrames;
    mfxI32 numIntraModes;

    /* derived from initialization parameters */
    mfxU32 width32;
    mfxU32 height32;
    mfxU32 width2x;
    mfxU32 height2x;
    mfxU32 width4x;
    mfxU32 height4x;
    mfxU32 width8x;
    mfxU32 height8x;
    mfxU32 width16x;
    mfxU32 height16x;
    mfxU32 interpWidth;
    mfxU32 interpHeight;
    mfxU32 interpBlocksW;
    mfxU32 interpBlocksH;
    mfxU32 numRefBufs;
    H265VMEMode vmeMode;
    H265HMELevel hmeLevel;

    /* state tracking for up to 2 frames (one lookahead) */
    mfxI32 refBufMap[FEI_DEPTH];
    mfxI32 curIdx;
    struct _mfxSyncPoint m_syncPoint[FEI_DEPTH];

public:
    /* called from C wrapper (public) */
    mfxStatus AllocateCmResources(mfxFEIH265Param *param, void *core);
    void FreeCmResources(void);
    mfxFEISyncPoint RunVme(mfxFEIH265Input *feiIn, mfxFEIH265Output *feiOut);
    mfxStatus SyncCurrent(mfxSyncPoint syncp, mfxU32 wait);

    /* zero-init all state variables */
    H265CmCtx() :
        device(),
        queue(),
        program(),
        task(),
        mbIntraDist(),
        mbIntraGrad4x4(),
        mbIntraGrad8x8(),
        mbIntraGrad16x16(),
        mbIntraGrad32x32(),
        distGpu(),
        mvGpu(),
        curbe(),
        me1xControl(),
        me2xControl(),
        me4xControl(),
        me8xControl(),
        me16xControl(),
        lastEvent(),

        kernelDownSample2tiers(),
        kernelDownSample4tiers(),
        kernelMeIntra(),
        kernelGradient(),
        kernelMe16(),
        kernelMe32(),
        kernelRefine32x32(),
        kernelRefine32x16(),
        kernelRefine16x32(),
        kernelInterpolateFrame(),
        kernelIme(),
        kernelImeWithPred(),

        um_intraPitch(),
        um_mbIntraDist(),
        um_mbIntraGrad4x4(),
        um_mbIntraGrad8x8(),
        um_mbIntraGrad16x16(),
        um_mbIntraGrad32x32(),
        um_pitchGrad4x4(),
        um_pitchGrad8x8(),
        um_pitchGrad16x16(),
        um_pitchGrad32x32(),
        um_distCpu(),
        um_mvCpu(),
        um_pitchDist(),
        um_pitchMv(),

        um_interpolatePitch(),
        um_interpolateData(),
        um_pInterpolateData(),

        um_mbIntraModeTop4(),
        um_mbIntraModeTop8(),
        um_mbIntraModeTop16(),
        um_mbIntraModeTop32(),

        picBufInput(),
        picBufRef(),

        width(),
        height(),
        numRefFrames(),
        numIntraModes(),

        width32(),
        height32(),
        width2x(),
        height2x(),
        width4x(),
        height4x(),
        width8x(),
        height8x(),
        width16x(),
        height16x(),
        interpWidth(),
        interpHeight(),
        interpBlocksW(),
        interpBlocksH(),
        numRefBufs(),
        vmeMode(),
        hmeLevel(),

        refBufMap(),
        curIdx(),
        m_syncPoint()
    { }

    ~H265CmCtx() { }
};

}

#endif /* _MFX_H265_ENC_CM_FEI_H */
