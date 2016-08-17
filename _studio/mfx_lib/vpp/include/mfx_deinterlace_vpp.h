/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//
//
//          Deinterlace Video Pre\Post Processing
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_DEINTERLACE_VPP_H
#define __MFX_DEINTERLACE_VPP_H

#include "ippvc.h"
#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_DI_IN_NUM_FRAMES_REQUIRED  (3)
#define VPP_DI_OUT_NUM_FRAMES_REQUIRED (1)

#define VAR_ERR_SIZE                   (7)

/* MediaSDK has frame based logic
 * So, 30i means 30 interlaced frames
 * 30p means progressive frames.
 * Constants was corrected according to this logic*/
typedef enum {
    VPP_SIMPLE_DEINTERLACE   = 0x0001, //example: 30i -> 30p, default mode
    VPP_DEINTERLACE_MODE30i60p = 0x0002, //example: 30i -> 60p
    VPP_INVERSE_TELECINE     = 0x003   //example: 30i -> 24p

} mfxDIMode;

class MFXVideoVPPDeinterlace : public FilterVPP{
public:

  static mfxU8 GetInFramesCountExt( void ) { return VPP_DI_IN_NUM_FRAMES_REQUIRED; };
  static mfxU8 GetOutFramesCountExt(void) { return VPP_DI_OUT_NUM_FRAMES_REQUIRED; };

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
  MFXVideoVPPDeinterlace(VideoCORE *core, mfxStatus* sts);
  virtual ~MFXVideoVPPDeinterlace();

  // VideoVPP
  virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam* pParam);

  // VideoBase methods
  virtual mfxStatus Close(void);
  virtual mfxStatus Init(mfxFrameInfo* In, mfxFrameInfo* Out);
  virtual mfxStatus Reset(mfxVideoParam *par);

  // tuning parameters
  virtual mfxStatus SetParam( mfxExtBuffer* pHint );

  virtual mfxU8 GetInFramesCount( void ){ return GetInFramesCountExt(); };
  virtual mfxU8 GetOutFramesCount( void ){ return GetOutFramesCountExt(); };

  // work buffer management
  virtual mfxStatus GetBufferSize( mfxU32* pBufferSize );
  virtual mfxStatus SetBuffer( mfxU8* pBuffer );

  virtual mfxStatus CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out );
  virtual bool      IsReadyOutput( mfxRequestType requestType );

protected:

    //mfxStatus PassThrough(mfxFrameSurface1 *in, mfxFrameSurface1 *out);

private:

  /* ********** */
  /* structures */
  /* ********** */

  typedef struct{
    mfxFrameSurface1 Surface1;
    mfxMemId*        mids;

  } sDynamicFramesPool;

  typedef struct {
    mfxI32 frp;
    mfxI32 pending_b;
    mfxI32 pending_t;
    mfxI32 var[VAR_ERR_SIZE];

    mfxF32 t2t_thresh_factor;
    mfxF32 b2b_thresh_factor;

    mfxI32 pat_ind;
    mfxU32 cur_frpattern;
    mfxI32 cadence_locked;
    mfxI32 patPhase;
    mfxI32 histLength;
    mfxI32 expected_frp;

    mfxI32 firstFrame;
    mfxI32 isFrameInBuf;

    sDynamicFramesPool outBuf;
    mfxU8              isInited;

  } sITCState;

  typedef struct {
    IppiDeinterlaceBlendState_8u_C1* pDIBlendStateY;
    IppiDeinterlaceBlendSpec_8u_C2*  pDIBlendStateUV;
    mfxFrameSurface1* refList[2];// Blend requires prev + cur + next frames
    mfxU16            refPicStructList[2];
    mfxU8             numProcessedFrames; // first 3 frames are important only

    bool bSyncRefList[2];// flags for sync part of advanced DI. analog of "async refList"

    mfxU64 prevTimeStamp;
  } sDIState;

  // structure describing readiness filter to produce or deliver output
  typedef struct {

    mfxU32 numProcessedFrames;
    bool   bReadyOutput;

    // only for sync part
    mfxU64 TimeStamp;
    mfxU32 frameOrder;
    mfxFrameInfo defferedFrameInfo;
    mfxU64 delayedTimeStamp_AdvancedMode;

  } sReadinessState;

  // DI
  // NV12
  mfxStatus di_LQ_NV12( mfxFrameSurface1* in, mfxFrameSurface1* out, mfxU16 picStruct);

  mfxStatus di_HQ_NV12( mfxFrameSurface1* ref1,
                        mfxFrameSurface1* ref0,
                        mfxFrameSurface1* in,
                        mfxFrameSurface1* out,
                        mfxU16 picStruct);

  // dispatcher for NV12/YV12 format
  mfxStatus di_LQ_dispatcher( mfxFrameSurface1* in, mfxFrameSurface1* out, mfxU16 picStruct);

  mfxStatus di_HQ_dispatcher( mfxFrameSurface1* ref1,
                              mfxFrameSurface1* ref0,
                              mfxFrameSurface1* in,
                              mfxFrameSurface1* out,
                              mfxU16 picStruct);

  // common DI algorithms
  mfxStatus di_AdvancedProcessing( mfxFrameSurface1* in, mfxFrameSurface1* out, mfxU16 picStruct);

  mfxStatus di_Processing( mfxFrameSurface1* in,
                           mfxFrameSurface1* out,
                           sDIState* pState,
                           InternalParam* pParam = NULL);

  // ITC
  mfxStatus itc_GetThreshold(sITCState* pState);

  mfxStatus itc_NV12( mfxFrameSurface1* in,
                      mfxFrameSurface1* out,
                      sITCState* pState,
                      sDIState* pDIState,
                      InternalParam* pParam);

  // FrameRate operations
  mfxU32 CalcFrameRate_32U(mfxU32 FrameRateExtN, mfxU32 FrameRateExtD);
  mfxF64 CalcFrameRate_64F(mfxU32 FrameRateExtN, mfxU32 FrameRateExtD);
  mfxStatus CheckFrameRate(mfxFrameInfo* In, mfxFrameInfo* Out);

  // in case of (30->24) and using DI, every 5 frame must be skipped
  bool IsSkippedFrame( mfxU32 frame );

  mfxStatus IncrementNumRealProcessedFrames( void );

  // second branch in case of 60i->60p
  mfxStatus CheckProduceOutputForMode60i60p( mfxFrameSurface1* in, mfxFrameSurface1* out);

  /* ********** */
  /* parameters */
  /* ********** */
  sITCState       m_itcState;
  sDIState        m_diState;

  sReadinessState m_syncReadinessState;
  sReadinessState m_processReadinessState;

  mfxDIMode       m_mode;       // DI mode: simple, advanced or ITC
  mfxU64          m_deltaTimeStamp;
#endif
};

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
class MFXVideoVPPDeinterlaceHW : public MFXVideoVPPDeinterlace
{
public:
    MFXVideoVPPDeinterlaceHW(VideoCORE *core, mfxStatus* sts);
    virtual mfxStatus CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out );
};
#endif

#endif // __MFX_DEINTERLACE_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */
