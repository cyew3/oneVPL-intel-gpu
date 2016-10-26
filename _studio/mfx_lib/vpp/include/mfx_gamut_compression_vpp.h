//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_GAMUT_COMPRESSION_VPP_H
#define __MFX_GAMUT_COMPRESSION_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_GAMUT_IN_NUM_FRAMES_REQUIRED  (1)
#define VPP_GAMUT_OUT_NUM_FRAMES_REQUIRED (1)

// internal gamut compression params
#define GCC_DIV_TABLE_SIZE_BITS  (8)
#define GCC_DIV_TABLE_PREC_BITS  (9)

class MFXVideoVPPGamutCompression : public FilterVPP{
public:

  static mfxU8 GetInFramesCountExt( void ) { return VPP_GAMUT_IN_NUM_FRAMES_REQUIRED; };
  static mfxU8 GetOutFramesCountExt(void) { return VPP_GAMUT_OUT_NUM_FRAMES_REQUIRED; };

  // this function is used by VPP Pipeline Query to correct application request
  static mfxStatus Query( mfxExtBuffer* pHint );

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
  MFXVideoVPPGamutCompression(VideoCORE *core, mfxStatus* sts);
  virtual ~MFXVideoVPPGamutCompression();

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

    Surface1_32f m_frameYUV;
    Surface1_32f m_frameLCH;
    Surface1_32f m_frameCompressedYUV;
    Surface1_32f m_frameCompressedLCH;

    // tables based on BT.709/601
    int m_YUV2RGB[9];
    int m_LumaVertex[512];
    int m_SatVertex[512];

    // div tables
    int m_DivTable[1<<GCC_DIV_TABLE_SIZE_BITS];

    // flag & mode
    bool m_bBT601;
    mfxGamutMode m_mode;

    // control params for
    // - base mode
    int m_CompressionFactor;
    // - advanced mode
    int m_Din;
    int m_Dout;
    int m_Din_Default;
    int m_Dout_Default;


    void CalculateInternalParams( void );

    mfxStatus FixedHueCompression_NV12_32f( Surface1_32f* pDst );
    int HWDivision(int nom, int denom, int outBitPrec);
#endif
};


#if !defined (MFX_ENABLE_HW_ONLY_VPP)
// external configuration functions
mfxStatus CreateDivTable(int* pTable);
mfxStatus CreateColorTables(int* pLumaVertexTab, int* pSatVertexTab, int* pYUV2RGBTab, bool bBT601 );
#endif 

#endif // __MFX_GAMUT_COMPRESSION_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */