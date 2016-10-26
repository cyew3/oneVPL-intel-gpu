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

#ifndef __MFX_VIDEO_ANALYSIS_VPP_H
#define __MFX_VIDEO_ANALYSIS_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

using namespace UMC;

#define VPP_VA_IN_NUM_FRAMES_REQUIRED  (1)
#define VPP_VA_OUT_NUM_FRAMES_REQUIRED (1)

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
/* UMC */
#include "umc_scene_analyzer.h"
#include "umc_scene_analyzer_base.h"
#include "umc_scene_analyzer_mb_func.h"

// extension of UMC SceneAnalyser[Params]
class VPPSceneAnalyzerParams: public SceneAnalyzerParams
{
public:
  // Default constructor
  VPPSceneAnalyzerParams(void);
  // Destructor
  virtual
    ~VPPSceneAnalyzerParams(void);
};

class VPPSceneAnalyzer : SceneAnalyzer{

public:
  // Default constructor
  VPPSceneAnalyzer(void);
  // Destructor
  virtual
    ~VPPSceneAnalyzer(void);

  // Initialize the analyzer
  // parameter should has type SceneAnalyzerParams
  virtual
    Status Init(BaseCodecParams *pParams);

  // Decompress the next frame
  virtual
    Status GetFrame(MediaData *pSource, MediaData *pDestination);

    // Get statistics/metrics
    Status GetMetrics(mfxExtVppAuxData *aux);

  // Release all resources
  virtual
    Status Close(void);

    virtual
    void AnalyzeIntraMB(const Ipp8u *pSrc, Ipp32s srcStep,UMC_SCENE_INFO *pMbInfo);

    virtual // optimized motion estimation alhorithm (logarithmic search)
    void AnalyzeInterMBMotionOpt(const Ipp8u *pRef, Ipp32s refStep,IppiSize refMbDim,const Ipp8u *pSrc, Ipp32s srcStep,Ipp32u mbX, Ipp32u mbY,IppiPoint *prevMV,UMC_SCENE_INFO *pMbInfo);

    virtual // optimized function, special threshold built in to make calls ME rare
    Status AnalyzePicture(SceneAnalyzerPicture *pRef, SceneAnalyzerPicture *pSrc);

    virtual
    Status GetFrameP(MediaData *pSource);

    mfxU32  m_spatialComplexity; //intra metrics = avg( sum4x4( abs(Luma[i] - MeanLuma) ))
    mfxU32  m_temporalComplexity; //inter(with ME) metrics = avg( sum4x4( abs( (Luma[i] - LumaRef[i]) - MeanDiff ) ) )

    IppiPoint m_pSliceMV[1024];
};
#endif
//------------------------------------------------------------

class MFXVideoVPPVideoAnalysis : public FilterVPP{
public:

  static mfxU8 GetInFramesCountExt( void ) { return VPP_VA_IN_NUM_FRAMES_REQUIRED; };
  static mfxU8 GetOutFramesCountExt(void) { return VPP_VA_OUT_NUM_FRAMES_REQUIRED; };

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
  MFXVideoVPPVideoAnalysis(VideoCORE *core, mfxStatus* sts);
  virtual ~MFXVideoVPPVideoAnalysis();

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

  mfxStatus SceneAnalysis(mfxFrameData* inData, mfxFrameInfo* inInfo,
                          mfxFrameData* outData, mfxFrameInfo* outInfo, mfxExtVppAuxData *aux);

  VPPSceneAnalyzer       m_analyser;
  VPPSceneAnalyzerParams m_analyserParams;
#endif
};

#endif // __MFX_VIDEO_ANALYSIS_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */
