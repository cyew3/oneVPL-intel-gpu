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

namespace H265Enc {

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
    CmSurface2D *bufOrig;
    CmSurface2D *bufDown2x;
    CmSurface2D *bufDown4x;
    CmSurface2D *bufDown8x;
    CmSurface2D *bufDown16x;

    CmSurface2DUP *bufHPelH;
    CmSurface2DUP *bufHPelV;
    CmSurface2DUP *bufHPelD;
} PicBufGpu;

class H265CmCtx {

private:
    /* internal helper functions */
    void RunVmeKernel(CmEvent **lastEvent, CmSurface2DUP **dist, CmSurface2DUP **mv, mfxFEIH265InputSurface *picBufInput, mfxFEIH265InputSurface *picBufRef);
    void ConvertBitDepth(void *inPtr, mfxU32 inBits, mfxU32 inPitch, void *outPtr, mfxU32 outBits);
    mfxStatus CopyInputFrameToGPU(CmEvent **lastEvent, mfxHDL pInSurf, void *YPlane, mfxU32 YPitch, mfxU32 YBitDepth);

    /* Cm elements */
    CmDevice  * device;
    CmQueue   * queue;
    CmProgram * program;
    CmTask    * task;
    CmEvent   * lastEventSaved;
    mfxU32      saveSyncPoint;

    CmBuffer * curbe;
    CmBuffer * me1xControl;
    CmBuffer * me2xControl;
    CmBuffer * me4xControl;
    CmBuffer * me8xControl;
    CmBuffer * me16xControl;

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
    CmKernel * kernelIme3tiers;

    /* set once at init */
    mfxU32 sourceWidth;
    mfxU32 sourceHeight;
    mfxU32 width;
    mfxU32 height;
    mfxU32 numRefFrames;
    mfxU32 numIntraModes;
    mfxU32 bitDepth;
    mfxU8 *bitDepthBuffer;
    mfxU32 targetUsage;
    mfxU32 rectParts;

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

public:
    /* called from C wrapper (public) */
    void * AllocateSurface(mfxFEIH265SurfaceType surfaceType, void *sysMem, mfxSurfInfoENC *surfInfo);
    mfxStatus FreeSurface(mfxFEIH265Surface *s);

    mfxStatus AllocateCmResources(mfxFEIH265Param *param, void *core);
    void FreeCmResources(void);
    void * RunVme(mfxFEIH265Input *feiIn, mfxExtFEIH265Output *feiOut);
    mfxStatus SyncCurrent(void *syncp, mfxU32 wait);
    mfxStatus DestroySavedSyncPoint(void *syncp);

    /* zero-init all state variables */
    H265CmCtx() :
        device(),
        queue(),
        program(),
        task(),
        lastEventSaved(),
        saveSyncPoint(),

        curbe(),
        me1xControl(),
        me2xControl(),
        me4xControl(),
        me8xControl(),
        me16xControl(),

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
        kernelIme3tiers(),

        width(),
        height(),
        numRefFrames(),
        numIntraModes(),
        bitDepth(), 
        bitDepthBuffer(),
        targetUsage(),

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
        hmeLevel()
    { }

    ~H265CmCtx() { }
};

}

#endif /* _MFX_H265_ENC_CM_FEI_H */
