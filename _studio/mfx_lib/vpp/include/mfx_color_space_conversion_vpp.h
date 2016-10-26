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

#ifndef __MFX_COLOR_SPACE_CONVERSION_VPP_H
#define __MFX_COLOR_SPACE_CONVERSION_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_CSC_IN_NUM_FRAMES_REQUIRED  (1)
#define VPP_CSC_OUT_NUM_FRAMES_REQUIRED (1)

class MFXVideoVPPColorSpaceConversion : public FilterVPP{
public:

  static mfxU8 GetInFramesCountExt( void ) { return VPP_CSC_IN_NUM_FRAMES_REQUIRED; };
  static mfxU8 GetOutFramesCountExt(void) { return VPP_CSC_OUT_NUM_FRAMES_REQUIRED; };

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
  MFXVideoVPPColorSpaceConversion(VideoCORE *core, mfxStatus* sts);
  virtual ~MFXVideoVPPColorSpaceConversion();

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
    mfxFrameData    m_yv12Data;
    bool            m_bAVX2;
#endif
};

#endif // __MFX_COLOR_SPACE_CONVERSION_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */
