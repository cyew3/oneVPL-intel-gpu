/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//
//
//          RangeMapVC1 Video Pre\Post Processing
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_RANGE_MAP_VC1_VPP_H
#define __MFX_RANGE_MAP_VC1_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_RM_VC1_IN_NUM_FRAMES_REQUIRED  (1)
#define VPP_RM_VC1_OUT_NUM_FRAMES_REQUIRED (1)

#define VPP_RM_VC1_DEFAULT_VALUE           (0)

class MFXVideoVPPRangeMapVC1 : public FilterVPP{
public:
  virtual mfxU8 GetInFramesCount( void ){ return VPP_RM_VC1_IN_NUM_FRAMES_REQUIRED; };
  virtual mfxU8 GetOutFramesCount( void ){ return VPP_RM_VC1_OUT_NUM_FRAMES_REQUIRED; };

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
  MFXVideoVPPRangeMapVC1(VideoCORE *core, mfxStatus* sts);
  virtual ~MFXVideoVPPRangeMapVC1();

  // VideoVPP
  virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam* pParam);

  // VideoBase methods
  virtual mfxStatus Close(void);
  virtual mfxStatus Init(mfxFrameInfo* In, mfxFrameInfo* Out);

  // tuning parameters
  virtual mfxStatus SetParam( mfxExtBuffer* pHint );
  virtual mfxStatus Reset(mfxVideoParam *par);

  // work buffer management
  virtual mfxStatus GetBufferSize( mfxU32* pBufferSize );
  virtual mfxStatus SetBuffer( mfxU8* pBuffer );

  virtual mfxStatus CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out );
  virtual bool IsReadyOutput( mfxRequestType requestType );

protected:
   mfxStatus PassThrough(mfxFrameSurface1* /*in*/, mfxFrameSurface1* /*out*/) { return MFX_ERR_UNKNOWN; };

private:

  mfxStatus  GetMapCoeffs( void );

  mfxHDL memID;

  mfxU8 RangeMap;

  mfxU8 RANGE_MAPY_FLAG;
  mfxU8 RANGE_MAPY;

  mfxU8 RANGE_MAPUV;
  mfxU8 RANGE_MAPUV_FLAG;
#endif
};

#endif // __MFX_RANGE_MAP_VC1_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */
