//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_DENOISE_VPP_H
#define __MFX_DENOISE_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_DN_IN_NUM_FRAMES_REQUIRED  (1)
#define VPP_DN_OUT_NUM_FRAMES_REQUIRED (2)

class MFXVideoVPPDenoise : public FilterVPP{
public:

  static mfxU8 GetInFramesCountExt( void ) { return VPP_DN_IN_NUM_FRAMES_REQUIRED; };
  static mfxU8 GetOutFramesCountExt(void) { return VPP_DN_OUT_NUM_FRAMES_REQUIRED; };

  // this function is used by VPP Pipeline Query to correct application request
  static mfxStatus Query( mfxExtBuffer* pHint );

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
  MFXVideoVPPDenoise(VideoCORE *core, mfxStatus* sts);
  virtual ~MFXVideoVPPDenoise();

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

  typedef struct DenoiseCASTState{

    struct ownWorkMemPool{

        mfxU8 *pBordFrame;
        mfxU8 *pU8Blk0;
        mfxU8 *pU8Blk1;
        mfxU8 *pU8Blk2;
        mfxU8 *pU8Blk3;
        mfxU8 *pU8Blk4;
        mfxU8 *pS16Blk0;
        mfxU8 *pS16Blk1;

    };

    mfxI32 gaussianThresholdY;

    ownWorkMemPool memPool;

    mfxI32 nextNoiseLevel;
    mfxI32 firstFrame;

    mfxU16 picStruct;

  }owniDenoiseCASTState;


  /* noise estimation */
  mfxStatus ownGaussNoiseEstimation_8u_C1R_v2(IppiSize size,
                                              mfxI32* pThres,
                                              owniDenoiseCASTState* pState);

  /* noise removal */
  mfxStatus owniFilterDenoiseCASTGetSize(IppiSize roiSize, mfxU32* pHistBufSize);

  mfxStatus owniFilterDenoiseCASTInit(owniDenoiseCASTState* ppState, IppiSize roiSize);

  mfxStatus owniFilterDenoiseCAST_8u_C1R(const mfxU8* pSrcPrev,
                                         mfxI32 srcStepPrev,
                                         IppiSize srcRoiSize,
                                         mfxU8* pDst,
                                         mfxI32 dstStep,
                                         owniDenoiseCASTState* pState);

   owniDenoiseCASTState  m_stateY;

   // reference frame.
   mfxFrameSurface1*     m_pRefSurface;

   //////////////////////////////////////////////////////////////////////////
   // Noise Level from application [0, 64]. 0 is weak denoise, 64 is strong denoise
   mfxU16  m_denoiseFactor;
   // internal Noise Level = f( m_denoiseFactor, m_bAutoAdjust )
   mfxI32  m_internalDenoiseFactor;
   // if application use denoise filter wo denoiseFactor it mean auto denoise
   bool   m_bAutoAdjust;
   //////////////////////////////////////////////////////////////////////////

   // params depends from m_denoiseFactor
   mfxI32 m_md_diff_th;       //[128, 144]. def is 128
   mfxU8  m_temporal_diff_th; //[10, 14].   def is 12
   mfxI32 m_temp_diff_low;    //[4, 8].     def is 8

   mfxI32 m_blockASDThreshold;//[32, 40]. def is 32
   mfxI32 m_blockSCMThreshold;//[32, 40]. def is 32

   mfxI32 m_lowNoiseLevel;
   mfxI32 m_strongNoiseLevel;

   mfxI32 m_sobelEdgeThreshold;
   mfxI32 m_localDiffThreshold;

   // mask size for Noise Estimation algorithm
   mfxI32 m_blockHeightNE;
   mfxI32 m_blockWidthNE;

   // mask size for DeNoise algorithm
   mfxU8 m_blockWidthDN;
   mfxU8 m_blockHeightDN;

   mfxU8 m_historyMax;    // default 192, range [144, 208]
   mfxU8 m_historyDelta;  // default 8,   range [4, 8]

   mfxU8 m_numberOfMotionPixelsThreshold;    // default   0 - range [0,  16]

   mfxU8* m_pHistoryWeight;
   mfxI32 m_historyStep;
#endif
};

#endif // __MFX_DENOISE_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */
