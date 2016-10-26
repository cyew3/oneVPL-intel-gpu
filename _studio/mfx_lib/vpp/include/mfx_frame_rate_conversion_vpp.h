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

#ifndef __MFX_FRAME_RATE_CONVERSION_VPP_H
#define __MFX_FRAME_RATE_CONVERSION_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_FRC_IN_NUM_FRAMES_REQUIRED  (2+1)
#define VPP_FRC_OUT_NUM_FRAMES_REQUIRED (1)

class MFXVideoVPPFrameRateConversion : public FilterVPP{
public:

    // not correct. must be based on in/out frame rates
    static mfxU8 GetInFramesCountExt( void ) { return VPP_FRC_IN_NUM_FRAMES_REQUIRED; };
    static mfxU8 GetOutFramesCountExt(void) { return VPP_FRC_OUT_NUM_FRAMES_REQUIRED; };

    // this function is used by VPP Pipeline Query to correct application request
    static mfxStatus Query( mfxExtBuffer* pHint );

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
    MFXVideoVPPFrameRateConversion(VideoCORE *core, mfxStatus* sts);
    virtual ~MFXVideoVPPFrameRateConversion();

    static bool IsParamSupported(mfxFrameInfo* In, mfxFrameInfo* Out);

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

    typedef struct 
    {
        mfxF64  deltaTime;

        bool         bReadyOutput;

        mfxFrameInfo defferedFrameInfo;

        mfxU16       refPicStruct;

    } NativeFRCState;


    typedef struct
    {
        bool   bIsSetTimeOffset;
    
        mfxU64 timeOffset;
        mfxU64 expectedTimeStamp;
        mfxU64 timeStampJump;
        mfxU32 numOutputFrames;
        
        bool   bReadyOutput;
        mfxU64 defferedInputTimeStamp;
        mfxU16 refPicStruct[2];

    } AdvancedFRCState;


    mfxStatus ResetNativeState( NativeFRCState* pState );
    mfxStatus ResetAdvancedState( AdvancedFRCState* pState );

    mfxU64    UpdatePTS( mfxFrameSurface1 *in );
    mfxU32    UpdateFrameOrder( mfxFrameSurface1 *in );

    mfxU64    GetExpectedPTS( mfxU32 frameNumber, mfxU64 timeOffset, mfxU64 timeJump )
    {
        mfxU64 expectedPTS = (((mfxU64)frameNumber * m_errPrtctState.Out.FrameRateExtD * MFX_TIME_STAMP_FREQUENCY) / m_errPrtctState.Out.FrameRateExtN +
                               timeOffset + timeJump);

        return expectedPTS;
    }

    // RunFrameVPP() is wrapper for these functions
    mfxStatus RunFrameVPP_AdvancedFRC(mfxFrameSurface1 *in, 
                                      mfxFrameSurface1 *out, 
                                      InternalParam* pParam);

    mfxStatus RunFrameVPP_NativeFRC(mfxFrameSurface1 *in, 
                                    mfxFrameSurface1 *out, 
                                    InternalParam* pParam);

    // CheckProduceOutput()is wrapper for these functions
    mfxStatus CheckProduceOutput_AdvancedFRC(mfxFrameSurface1 *in, mfxFrameSurface1 *out );
    mfxStatus CheckProduceOutput_NativeFRC(mfxFrameSurface1 *in, mfxFrameSurface1 *out );

    // nativeFRC
    mfxU64         m_timeStamp;      // only for sync part
    mfxU64         m_deltaTimeStamp; // only for sync part

    mfxF64    m_inFrameTime;
    mfxF64    m_outFrameTime;
    mfxF64    m_timeFrameInterval;    

    NativeFRCState m_nativeSyncState;
    NativeFRCState m_nativeProcessState;

    mfxFrameSurface1* m_pRefSurface[2];

    // advanced mode of FRC (PTS based)
    bool m_bAdvancedMode;

    mfxU64 m_minDeltaTime;

    AdvancedFRCState m_advSyncState;
    AdvancedFRCState m_advProcessState;
#endif
};

#endif // __MFX_FRAME_RATE_CONVERSION_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */
