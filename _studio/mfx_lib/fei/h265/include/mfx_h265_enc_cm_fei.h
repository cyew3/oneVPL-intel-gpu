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
#include "mfx_h265_enc_cm_utils.h"
#include "cmrt_cross_platform.h"

#pragma warning(disable: 4351)

namespace H265Enc {

enum H265VMEMode {
    VME_MODE_SMALL = 0,
    VME_MODE_LARGE,
    VME_MODE_REFINE,
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
    void RunVmeKernel(CmEvent **lastEvent, CmSurface2DUP **dist, CmSurface2DUP **mv, mfxFEIH265InputSurface *picBufInput, mfxFEIH265ReconSurface *picBufRef);
    void ConvertBitDepth(void *inPtr, mfxU32 inBits, mfxU32 inPitch, void *outPtr, mfxU32 outBits);
    mfxStatus CopyInputFrameToGPU(CmEvent **lastEvent, mfxFEIH265Input *in);
    mfxStatus CopyReconFrameToGPU(CmEvent **lastEvent, mfxFEIH265Input *in);

    /* Cm elements */
    CmDevice  * device;
    CmQueue   * queue;
    CmProgram * programPrepareSrc;
    CmProgram * programMeIntra;
    CmProgram * programGradient;
    CmProgram * programHmeMe32;
    CmProgram * programMe16Refine32x32;
    CmProgram * programRefine32x32sad;
    CmProgram * programRefine32x16;
    CmProgram * programRefine16x32;
    CmProgram * programInterpolateFrame;
    CmProgram * programDeblock;
    CmProgram * programSaoEstimate;
    CmProgram * programSaoApply;
    CmEvent   * lastEventSaved;
    mfxU32      saveSyncPoint;

    CmBuffer * curbe;
    CmBuffer * me1xControl;
    CmBuffer * me2xControl;
    CmBuffer * me4xControl;
    CmBuffer * me8xControl;
    CmBuffer * me16xControl;

    CmSurface2D * deblocked;
    CmBuffer    * saoStat;

    /* Cm kernels to load  */
    H265Enc::Kernel kernelPrepareSrc;
    H265Enc::Kernel kernelPrepareRef;
    H265Enc::Kernel kernelMeIntra;
    H265Enc::Kernel kernelGradient;
    H265Enc::Kernel kernelHmeMe32;
    H265Enc::Kernel kernelMe16Refine32x32;
    H265Enc::Kernel kernelRefine32x32sad;
    H265Enc::Kernel kernelRefine32x16;
    H265Enc::Kernel kernelRefine16x32;
    H265Enc::Kernel kernelInterpolateFrame;
    H265Enc::Kernel kernelDeblock;
    H265Enc::Kernel kernelSaoStat;
    H265Enc::Kernel kernelSaoEstimate;
    H265Enc::Kernel kernelSaoApply;

    /* set once at init */
    mfxU32 sourceWidth;
    mfxU32 sourceHeight;
    mfxU32 width;
    mfxU32 height;
    mfxU32 padding;
    mfxU32 widthChroma;
    mfxU32 heightChroma;
    mfxU32 paddingChroma;
    mfxU32 numRefFrames;
    mfxU32 numIntraModes;
    mfxU32 fourcc;
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

public:
    /* called from C wrapper (public) */
    void * AllocateSurface(mfxFEIH265SurfaceType surfaceType, void *sysMem1, void *sysMem2, mfxSurfInfoENC *surfInfo);
    mfxStatus FreeSurface(mfxFEIH265Surface *s);
    mfxStatus FreeBuffer(mfxFEIH265Surface *s);

    mfxStatus AllocateCmResources(mfxFEIH265Param *param, void *core);
    void FreeCmResources(void);
    void * RunVme(mfxFEIH265Input *feiIn, mfxExtFEIH265Output *feiOut);
    mfxStatus SyncCurrent(void *syncp, mfxU32 wait);
    mfxStatus DestroySavedSyncPoint(void *syncp);

    /* zero-init all state variables */
    H265CmCtx() :
        device(),
        queue(),
        programPrepareSrc(),
        programMeIntra(),
        programGradient(),
        programHmeMe32(),
        programMe16Refine32x32(),
        programRefine32x32sad(),
        programRefine32x16(),
        programRefine16x32(),
        programInterpolateFrame(),
        programDeblock(),
        programSaoEstimate(),
        programSaoApply(),
        lastEventSaved(),
        saveSyncPoint(),

        curbe(),
        me1xControl(),
        me2xControl(),
        me4xControl(),
        me8xControl(),
        me16xControl(),

        kernelPrepareSrc(),
        kernelPrepareRef(),
        kernelMeIntra(),
        kernelGradient(),
        kernelMe16Refine32x32(),
        kernelRefine32x32sad(),
        kernelRefine32x16(),
        kernelRefine16x32(),
        kernelInterpolateFrame(),
        kernelHmeMe32(),
        kernelDeblock(),
        kernelSaoStat(),
        kernelSaoEstimate(),
        kernelSaoApply(),

        width(),
        height(),
        numRefFrames(),
        numIntraModes(),
        fourcc(), 
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
        vmeMode()
    { }

    ~H265CmCtx() { }
};

}

#endif /* _MFX_H265_ENC_CM_FEI_H */
