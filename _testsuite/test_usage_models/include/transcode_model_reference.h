//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __TRANSCODE_MODEL_REFERENCE_H__
#define __TRANSCODE_MODEL_REFERENCE_H__

#include "test_usage_models_utils.h"
#include "test_usage_models_bitstream_reader.h"
#include "test_usage_models_bitstream_writer.h"
#include "test_usage_models_surfaces_manager.h"
#include "test_usage_models_bitstream_manager.h"
#include "transcode_model_api.h"

class TranscodeModelReference : TranscodeModel
{
public:
    TranscodeModelReference(AppParam& params);
    ~TranscodeModelReference(void);

    mfxStatus Init( AppParam& params );
    mfxStatus Run(void);
    mfxStatus Close(void);

    int GetProcessedFramesCount(void) { return m_framesCount; }

protected:
// session manager
    mfxStatus ConfigMFXComponents( AppParam& params );
    mfxStatus InitMFXComponents( void );

    mfxStatus InitMFXSessions( SessionMode mode, std::map<const msdk_char*, mfxIMPL> impl );
    mfxStatus CloseMFXSessions( SessionMode mode );
    mfxStatus SetAllocatorMFXSessions( mfxU16 IOPattern );

    mfxStatus JoinMFXSessions( SessionMode mode );
    mfxStatus DisjoinMFXSessions( SessionMode mode );

// auxiliary transcoding primitives
    mfxStatus ReadBitstreamHeader(mfxVideoParam& bsHeaderParam);
    mfxStatus AllocEnoughBuffer(mfxBitstream* pBS);

// universal transcoding primitives
    // [BSReader]
    mfxStatus ReadOneFrame( void );
    mfxBitstream* GetSrcPtr( void );

    // [decode]
    mfxStatus DecodeOneFrame(mfxBitstream *pBS, 
                             TUMSurfacesManager *pSurfacePool, 
                             mfxFrameSurfaceEx *pSurfaceEx);

    // [vpp]    
    mfxStatus VPPOneFrame(mfxFrameSurface1   *pSurfaceIn,
                          TUMSurfacesManager *pSurfacePool,
                          mfxFrameSurfaceEx *pSurfaceEx);

    // [encode]
    mfxStatus EncodeOneFrame(mfxFrameSurface1 *pSurface, mfxBitstreamEx *pBSEx);

    // [BSWriter]    
    mfxStatus WriteOneFrame( mfxBitstream* pBS );
    mfxBitstream* GetDstPtr( void );
    // ex function
    mfxStatus PutBS( mfxBitstreamEx *pBSEx );

    // [synchronization]
    mfxStatus SynchronizeDecode( mfxSyncPoint syncp );
    mfxStatus SynchronizeVPP( mfxSyncPoint syncp );
    mfxStatus SynchronizeEncode( mfxSyncPoint syncp );

    // [BHVR functions of transcode]
    bool IsVPPEnable( void ) { return m_bVPPEnable; }
    bool IsJoinSessionEnable( void );
    bool IsDecodeSyncOperationRequired( SessionMode mode );
    bool IsVppSyncOperationRequired( SessionMode mode );

private:

    MFXVideoSession*    m_pSessionDecode;
    MFXVideoSession*    m_pSessionVPP;
    MFXVideoSession*    m_pSessionEncode;

    TUMBitstreamReader m_bsReader;
    TUMBitstreamWriter m_bsWriter;

    MFXVideoDECODE*    m_pmfxDEC;
    MFXVideoVPP*       m_pmfxVPP;    
    MFXVideoENCODE*    m_pmfxENC;

    mfxVideoParam      m_decodeVideoParam;
    mfxVideoParam      m_vppVideoParam;
    mfxVideoParam      m_encodeVideoParam;

    sMemoryAllocator   m_allocator;    

    bool               m_bInited;
    bool               m_bVPPEnable;

    mfxStatus CreateAllocator( mfxU16 IOPattern );
    void DestroyAllocator(void);

protected:
    TUMSurfacesManager m_dec2vppSurfManager; // container/manager of available memory for surfaces btw dec&vpp
    TUMSurfacesManager m_vpp2encSurfManager; // container/manager of available memory for surfaces btw vpp&enc

    TUMSurfacesManager m_dec2encSurfManager; // container/manager of available memory for surfaces btw dec&enc
    int                m_framesCount;
    SessionMode        m_sessionMode;

};

#endif //__TRANSCODE_MODEL_REFERENCE_H__
/* EOF */
