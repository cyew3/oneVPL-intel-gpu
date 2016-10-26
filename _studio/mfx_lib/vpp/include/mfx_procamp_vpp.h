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

#ifndef __MFX_PROCAMP_VPP_H
#define __MFX_PROCAMP_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_PROCAMP_IN_NUM_FRAMES_REQUIRED  (1)
#define VPP_PROCAMP_OUT_NUM_FRAMES_REQUIRED (1)

class MFXVideoVPPProcAmp : public FilterVPP{
public:

    static mfxU8 GetInFramesCountExt( void ) { return VPP_PROCAMP_IN_NUM_FRAMES_REQUIRED; };
    static mfxU8 GetOutFramesCountExt(void) { return VPP_PROCAMP_OUT_NUM_FRAMES_REQUIRED; };

    // this function is used by VPP Pipeline Query to correct application request
    static mfxStatus Query( mfxExtBuffer* pHint );

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
    MFXVideoVPPProcAmp(VideoCORE *core, mfxStatus* sts);
    virtual ~MFXVideoVPPProcAmp();

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

    mfxF64    m_brightness;            // brightness value from application is stored here.
    mfxF64    m_contrast;              // contrast value from application is stored here.
    mfxF64    m_hue;                   // hue value from application is stored here.
    mfxF64    m_saturation;            // saturation value from application is stored here.

    mfxI32    m_internalBrightness;
    mfxI32    m_internalContrast;
    mfxI32    m_internalSinCS;
    mfxI32    m_internalCosCS;

    // in case of default params filter can be excluded from pipeline to high performance
    bool      m_isFilterActive;

    void CalculateInternalParams( void );
#endif
};

#endif // __MFX_PROCAMP_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */