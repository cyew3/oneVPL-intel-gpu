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

#ifndef _MFX_H265_ENC_CM_PLUGIN_H_
#define _MFX_H265_ENC_CM_PLUGIN_H_

#ifdef AS_H265FEI_PLUGIN

// map current ENC interface to new "repipelined" Gacc code
// will be replaced when new ENC API is finalized (e.g. adding new functions to alloc/free CM surfaces, etc.)
#define ALT_GAA_PLUGIN_API

#include "mfx_enc_ext.h"

#ifdef ALT_GAA_PLUGIN_API

#include "mfxfeih265.h"     /* inherit public API - this file should only contain non-public definitions */

#pragma warning(disable: 4351)  // VS2012: disable default initialization warning

typedef void *mfxFEIH265;
typedef void *mfxFEISyncPoint;

// NOTE - these definitions are changed in new API - must match corresponding values in mfx_h265_fei.h
// use different names ("new") to avoid conflicts with public definitions (mfxfeih265.h)
// calling code needs to map old->new before ProcessFrameAsync()
typedef struct
{
    mfxExtBuffer       Header;

    mfxHDL             SurfIntraMode[4];
    mfxHDL             SurfIntraDist;
    mfxHDL             SurfInterDist[64];
    mfxHDL             SurfInterMV[64];
    mfxHDL             SurfInterp[3];

    mfxU16             reserved[24];
} mfxFEIH265OutputNew;

enum
{
    MFX_FEI_H265_OP_NEW_NOP          = 0x00,
    MFX_FEI_H265_OP_NEW_GPU_COPY_SRC = 0x01,
    MFX_FEI_H265_OP_NEW_GPU_COPY_REF = 0x02,
    MFX_FEI_H265_OP_NEW_INTRA_MODE   = 0x04,
    MFX_FEI_H265_OP_NEW_INTRA_DIST   = 0x08,
    MFX_FEI_H265_OP_NEW_INTER_ME     = 0x10,
    MFX_FEI_H265_OP_NEW_INTERPOLATE  = 0x20,
};
// END - new definitions

// NOTE - these definitions are the same in the new API
// these are duplicates so should be removed from this file (will get pulled in automatically once mfxfeih265.h is updated)

/* input parameters - set once at init */
typedef struct
{
    mfxU32 Width;
    mfxU32 Height;
    mfxU32 MaxCUSize;
    mfxU32 MPMode;
    mfxU32 NumRefFrames;
    mfxU32 NumIntraModes;
    mfxU32 BitDepth;
    mfxU32 TargetUsage;
} mfxFEIH265Param;

/* basic info for current and reference frames */
typedef struct
{
    void  *YPlane;
    mfxU32 YPitch;
    mfxU32 YBitDepth;
    mfxU32 PicOrder;
    mfxU32 EncOrder;

    mfxU32 copyFrame;
    mfxHDL surfIn;
} mfxFEIH265Frame;

/* FEI input - update before each call to ProcessFrameAsync */
typedef struct
{
    mfxU32 FEIOp;
    mfxU32 FrameType;
    mfxU32 SaveSyncPoint;

    mfxFEIH265Frame FEIFrameIn;
    mfxFEIH265Frame FEIFrameRef;

} mfxFEIH265Input;

enum UnsupportedBlockSizes {
    /* public API allocates 64 slots so this could be increased later (update SyncCurrent()) */
    MFX_FEI_H265_BLK_MAX      = 12,
};

/* required buffer sizes - returned from call to Query() */
typedef struct
{
    mfxU32 size;
    mfxU32 pitch;
    mfxU32 align;
    mfxU32 numBytesInRow;
    mfxU32 numRows;
   
    mfxU32 reserved[3];     /* do not modify */
} mfxSurfInfoENC;

typedef struct
{
    mfxExtBuffer       Header;

    mfxSurfInfoENC     IntraMode[4];    /* 4 = intra block sizes (square 4,8,16,32) */
    mfxSurfInfoENC     IntraDist;
    mfxSurfInfoENC     InterDist[64];   /* 64 = max number of inter block sizes */
    mfxSurfInfoENC     InterMV[64];
    mfxSurfInfoENC     Interpolate[3];  /* 3 = half-pel planes (H,V,D) */

    mfxU16             reserved[32];
} mfxExtFEIH265Alloc;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* FEI functions - LIB interface */
// VideoCore* is needed for multisession mode to have its own CmDevice in each session (number of Cm surfases is restricted)
mfxStatus H265FEI_Init(mfxFEIH265 *feih265, mfxFEIH265Param *param, void *core = NULL);
mfxStatus H265FEI_Close(mfxFEIH265 feih265);
mfxStatus H265FEI_ProcessFrameAsync(mfxFEIH265 feih265, mfxFEIH265Input *in, void *out, mfxFEISyncPoint *syncp);  // NOTE void * to avoid name conflicts (must point to NEW mfxExtFEIH265Output)
mfxStatus H265FEI_SyncOperation(mfxFEIH265 feih265, mfxFEISyncPoint syncp, mfxU32 wait);
mfxStatus H265FEI_DestroySavedSyncPoint(mfxFEIH265 feih265, mfxFEISyncPoint syncp);

mfxStatus H265FEI_GetSurfaceDimensions(void *core, mfxU32 width, mfxU32 height, mfxExtFEIH265Alloc *extAlloc);
mfxStatus H265FEI_AllocateInputSurface(mfxFEIH265 feih265, mfxHDL *pInSurf);
mfxStatus H265FEI_AllocateOutputSurface(mfxFEIH265 feih265, mfxU8 *sysMem, mfxSurfInfoENC *surfInfo, mfxHDL *pOutSurf);
mfxStatus H265FEI_FreeSurface(mfxFEIH265 feih265, mfxHDL pSurf);

// END - duplicate definitions

#ifdef __cplusplus
}
#endif

#else
#include "mfx_h265_fei.h"
#endif

#include "cmrt_cross_platform.h"

#include <memory>
#include <list>
#include <vector>


typedef struct
{
    mfxFEIH265Input      feiH265In;
#ifdef ALT_GAA_PLUGIN_API
    mfxFEIH265OutputNew *feiH265OutNew;
    mfxFEIH265Output    *feiH265Out;
#else
    mfxExtFEIH265Output  feiH265Out;
#endif
    mfxFEISyncPoint      feiH265SyncPoint;

} sAsyncParams;

class VideoENC_H265FEI:  public VideoENC_Ext
{
public:

     VideoENC_H265FEI(VideoCORE *core, mfxStatus *sts);

    // Destructor
    virtual
    ~VideoENC_H265FEI(void);

    virtual
    mfxStatus Init(mfxVideoParam *par) ;
    virtual
    mfxStatus Reset(mfxVideoParam *par);
    virtual
    mfxStatus Close(void);
    virtual
    mfxStatus AllocateInputSurface(mfxHDL *surfIn);
    virtual
    mfxStatus AllocateOutputSurface(mfxU8 *sysMem, mfxSurfInfoENC *surfInfo, mfxHDL *surfOut);
    virtual
    mfxStatus FreeSurface(mfxHDL surf);

    static 
    mfxStatus Query(VideoCORE*, mfxVideoParam *in, mfxVideoParam *out);
    static 
    mfxStatus QueryIOSurf(VideoCORE*, mfxVideoParam *par, mfxFrameAllocRequest *request);

    virtual
    mfxStatus GetVideoParam(mfxVideoParam *par);
    
    virtual
    mfxStatus GetFrameParam(mfxFrameParam *par);

    virtual
    mfxStatus RunFrameVmeENCCheck(mfxENCInput *input, mfxENCOutput *output, MFX_ENTRY_POINT pEntryPoints[], mfxU32 &numEntryPoints);

    mfxStatus ResetTaskCounters();

    /* defined in internal header mfx_h265_fei.h */
    mfxFEIH265              m_feiH265;
    mfxFEIH265Param         m_feiH265Param;
    mfxFEIH265Input         m_feiH265In;
    mfxFEIH265Output        m_feiH265Out;

#ifdef ALT_GAA_PLUGIN_API
    mfxStatus               AllocateAllSurfaces(void);
    mfxStatus               FreeAllSurfaces(void);
    mfxStatus               GetInputSurface(mfxFEIH265Frame *inFrame, mfxHDL *surfIn, mfxI32 *surfInMap, mfxI32 numSurfaces);
    mfxU32                  MapOP(mfxU32 FEIOpPublic);

    mfxExtFEIH265Alloc      m_feiAlloc;
    mfxFEIH265OutputNew     m_feiH265OutNew[2][MFX_FEI_H265_MAX_NUM_REF_FRAMES];
    mfxU32                  m_feiInIdx;
    mfxU32                  m_feiInEncOrderLast;

    /* handles to GPU input/output surfaces */
    mfxHDL                  m_surfInSource[2];
    mfxHDL                  m_surfInRecon[MFX_FEI_H265_MAX_NUM_REF_FRAMES+1];

    /* mapping of input frames to GPU surfaces (encOrder) */
    mfxI32                  m_numSourceSurfaces;
    mfxI32                  m_numReconSurfaces;
    mfxI32                  m_surfInSourceMap[2];
    mfxI32                  m_surfInReconMap[MFX_FEI_H265_MAX_NUM_REF_FRAMES+1];

    mfxHDL                  m_surfIntraMode[2][4];
    mfxHDL                  m_surfIntraDist[2];
    mfxHDL                  m_surfInterDist[2][MFX_FEI_H265_MAX_NUM_REF_FRAMES][MFX_FEI_H265_BLK_MAX];
    mfxHDL                  m_surfInterMV[2][MFX_FEI_H265_MAX_NUM_REF_FRAMES][MFX_FEI_H265_BLK_MAX];
    mfxHDL                  m_surfInterp[2][MFX_FEI_H265_MAX_NUM_REF_FRAMES+1][3];

    /* pointers to user-allocated memory */
    mfxU32                * m_IntraMode[2][4];
    mfxFEIH265IntraDist   * m_IntraDist[2];
    mfxU32                * m_InterDist[2][MFX_FEI_H265_MAX_NUM_REF_FRAMES][MFX_FEI_H265_BLK_MAX];
    mfxI16Pair            * m_InterMV[2][MFX_FEI_H265_MAX_NUM_REF_FRAMES][MFX_FEI_H265_BLK_MAX];
    mfxU8                 * m_Interp[2][MFX_FEI_H265_MAX_NUM_REF_FRAMES+1][3];
#endif

private:
    bool                    m_bInit;
    VideoCORE*              m_core;

};

#endif  // AS_H265FEI_PLUGIN

#endif  // _MFX_H265_ENC_CM_PLUGIN_H_

