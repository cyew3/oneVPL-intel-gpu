//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_MPEG2_ENCODE_D3D11__H
#define __MFX_MPEG2_ENCODE_D3D11__H

#include "mfx_common.h"

#if defined(MFX_VA) && defined(MFX_D3D11_ENABLED)
#if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC)

#include <vector>
#include <assert.h>

#include "encoding_ddi.h"
#include "auxiliary_device.h"

#include "mfx_ext_buffers.h"
#include "mfxpcp.h"

#include "mfx_mpeg2_enc_common_hw.h"
//#include "mfx_mpeg2_enc_ddi_hw.h"
#include "mfx_mpeg2_encode_interface.h"

#include <d3d11.h>


namespace MfxHwMpeg2Encode
{
    class D3D11Encoder : public DriverEncoder
    {
    public:
        explicit D3D11Encoder(VideoCORE* core);

        virtual ~D3D11Encoder();

        virtual mfxStatus QueryEncodeCaps(ENCODE_CAPS & caps);
        
        virtual mfxStatus Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId);                 

        virtual mfxStatus Execute(ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData = 0, mfxU32 userDataLen = 0);              
        
        virtual mfxStatus Close();

        virtual bool      IsFullEncode() const { return m_bENC_PAK;}
        virtual mfxStatus RegisterRefFrames(const mfxFrameAllocResponse* pResponse);

        virtual mfxStatus FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers);

        virtual mfxStatus FillBSBuffer(mfxU32 nFeedback,mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt);

        virtual mfxStatus SetFrames (ExecuteBuffers* pExecuteBuffers); 

    private:
        // Declare private copy constructor to avoid accidental assignment
        // and klocwork complaining.
        D3D11Encoder(const D3D11Encoder &);
        D3D11Encoder & operator = (const D3D11Encoder &);

        mfxStatus Init_MPEG2_ENC(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);         
        mfxStatus Init_MPEG2_ENCODE (ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);

        mfxStatus Execute_ENC    (ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData, mfxU32 userDataLen);        
        mfxStatus Execute_ENCODE (ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData, mfxU32 userDataLen);        
        
        mfxStatus Init(
            GUID guid,
            ENCODE_FUNC func,
            ID3D11VideoDevice  *pVideoDevice, 
            ID3D11VideoContext *pVideoContext,
            ExecuteBuffers* pExecuteBuffers);

        mfxStatus Execute(
            ExecuteBuffers* pExecuteBuffers, 
            mfxU32 func,
            mfxU8* pUserData, mfxU32 userDataLen); 

        mfxStatus GetBuffersInfo();

        mfxStatus QueryMbDataLayout();

        mfxStatus QueryCompBufferInfo(
            D3D11_DDI_VIDEO_ENCODER_BUFFER_TYPE type, 
            mfxFrameAllocRequest* pRequest);

        mfxStatus CreateMBDataBuffer(
            mfxU32 numRefFrames);

        mfxStatus CreateBSBuffer(
            mfxU32 numRefFrames);


        mfxStatus CreateCompBuffers(
            ExecuteBuffers* pExecuteBuffers, 
            mfxU32 numRefFrames);
        

        mfxStatus Register(
            const mfxFrameAllocResponse* pResponse, 
            D3D11_DDI_VIDEO_ENCODER_BUFFER_TYPE type);

        mfxI32    GetRecFrameIndex (mfxMemId memID);
        mfxI32    GetRawFrameIndex (mfxMemId memIDe, bool bAddFrames);   


        VideoCORE* m_core;

        // d3d11 specific
        ID3D11VideoDevice *                       m_pVideoDevice;
        ID3D11VideoContext *                  m_pVideoContext;
        ID3D11VideoDecoder *                      m_pDecoder;
        GUID                                      m_guid;
        ID3D11VideoDecoderOutputView *            m_pVDOView;

        ENCODE_CAPS                               m_caps;

        std::vector<ENCODE_COMP_BUFFER_INFO>      m_compBufInfo;
        std::vector<D3DDDIFORMAT>                 m_uncompBufInfo;
        ENCODE_MBDATA_LAYOUT                      m_layout;
        bool                                      m_infoQueried;

        mfxFrameAllocResponse                     m_allocResponseMB;
        std::vector<mfxFrameAllocResponse>        m_realResponseMB;
        std::vector<mfxMemId>                     m_midMB;

        mfxFrameAllocResponse                     m_allocResponseBS;
        std::vector<mfxFrameAllocResponse>        m_realResponseBS;
        std::vector<mfxMemId>                     m_midBS;

        mfxRecFrames                              m_reconFrames;
        mfxRawFrames                              m_rawFrames;

        std::vector<mfxHDLPair>                   m_bsQueue;
        std::vector<mfxHDLPair>                   m_mbQueue;
        std::vector<mfxHDLPair>                   m_reconQueue;

        static const mfxU32 MAX_PACKED_SPSPPS_SIZE = 1024;        
        mfxU8 m_packedSpsPpsBuffer[MAX_PACKED_SPSPPS_SIZE]; // sps, pps
        bool                                      m_bENC_PAK; 
        mfxFeedback                               m_feedback;

    };
};

#endif // #if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC)
#endif // #if defined(MFX_VA) && defined(MFX_D3D11_ENABLED)
#endif // #ifndef __MFX_MPEG2_ENCODE_D3D11__H
/* EOF */
