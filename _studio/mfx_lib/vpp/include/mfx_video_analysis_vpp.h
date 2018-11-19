// Copyright (c) 2008-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
    void AnalyzeIntraMB(const uint8_t *pSrc, int32_t srcStep,UMC_SCENE_INFO *pMbInfo);

    virtual // optimized motion estimation alhorithm (logarithmic search)
    void AnalyzeInterMBMotionOpt(const uint8_t *pRef, int32_t refStep,mfxSize refMbDim,const uint8_t *pSrc, int32_t srcStep,uint32_t mbX, uint32_t mbY,IppiPoint *prevMV,UMC_SCENE_INFO *pMbInfo);

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
