//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_H264_ENCODE_D3D11__H
#define __MFX_H264_ENCODE_D3D11__H

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined (MFX_D3D11_ENABLED)

#include <vector>
#include <assert.h>

#include "encoding_ddi.h"

#include "mfx_ext_buffers.h"
#include "mfxpcp.h"

#include "mfx_h264_enc_common_hw.h"
#include "mfx_h264_encode_interface.h"

#include <d3d11.h>
#include "mfx_h264_encode_d3d9.h" // suggest that the same syncop based on cache functionality is used


namespace MfxHwH264Encode
{
    class OutputBitstream;


    class D3D11Encoder : public DriverEncoder
    {
    public:
        D3D11Encoder();

        virtual
        ~D3D11Encoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE * core,
            GUID        guid,
            mfxU32      width,
            mfxU32      height,
            bool        isTemporal = false);

        virtual
        mfxStatus CreateAccelerationService(
            MfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            MfxVideoParam const & par);

        virtual
        mfxStatus Register(
            mfxMemId     memId,
            D3DDDIFORMAT type);

        virtual
        mfxStatus Register(
            mfxFrameAllocResponse & response,
            D3DDDIFORMAT            type);

        virtual
        mfxStatus Execute(
            mfxHDL                     surface,
            DdiTask const &            task,
            mfxU32                     fieldId,
            PreAllocatedVector const & sei);

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS & caps);

        virtual
        mfxStatus QueryMbPerSec(
            mfxVideoParam const & par,
            mfxU32              (&mbPerSec)[16]);

        virtual
        mfxStatus QueryEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS & caps);

        virtual
        mfxStatus GetEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS & caps);

        virtual
        mfxStatus SetEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS const & caps);

        //mfxStatus QueryMbDataLayout(
        //    ENCODE_SET_SEQUENCE_PARAMETERS_H264 & sps,
        //    ENCODE_MBDATA_LAYOUT &                layout);

        virtual
        mfxStatus QueryStatus(
            DdiTask & task,
            mfxU32    fieldId);

        virtual
        mfxStatus Destroy();

        void ForceCodingFunction (mfxU16 codingFunction)
        {
            m_forcedCodingFunction = codingFunction;
        }
        
        virtual
        mfxStatus QueryHWGUID(
            VideoCORE * core,
            GUID        guid,
            bool        isTemporal);

    private:
        D3D11Encoder(D3D11Encoder const &); // no implementation
        D3D11Encoder & operator =(D3D11Encoder const &); // no implementation

        mfxStatus Init(
            GUID guid,
            ID3D11VideoDevice *pVideoDevice,
            ID3D11VideoContext *pVideoContext,
            mfxU32      width,
            mfxU32      height,
            mfxExtPAVPOption const * extPavp);

    private:
        VideoCORE *                                 m_core;
        ID3D11VideoDevice *                         m_pVideoDevice;
        ID3D11VideoContext *                        m_pVideoContext;
        ID3D11VideoDecoder *                        m_pDecoder;
        GUID                                        m_guid;
        GUID                                        m_requestedGuid;

        ENCODE_CAPS                                 m_caps;
        ENCODE_ENC_CTRL_CAPS                        m_capsQuery; // from ENCODE_ENC_CTRL_CAPS_ID
        ENCODE_ENC_CTRL_CAPS                        m_capsGet;   // from ENCODE_ENC_CTRL_GET_ID
        std::vector<ENCODE_COMP_BUFFER_INFO>        m_compBufInfo;
        std::vector<D3DDDIFORMAT>                   m_uncompBufInfo;
        bool                                        m_infoQueried;

        ENCODE_SET_SEQUENCE_PARAMETERS_H264         m_sps;
        ENCODE_SET_VUI_PARAMETER                    m_vui;
        ENCODE_SET_PICTURE_PARAMETERS_H264          m_pps;
        std::vector<ENCODE_SET_SLICE_HEADER_H264>   m_slice;
        std::vector<ENCODE_COMPBUFFERDESC>          m_compBufDesc;

        std::vector<ENCODE_QUERY_STATUS_PARAMS>     m_feedbackUpdate;
        CachedFeedback                              m_feedbackCached;
        HeaderPacker                                m_headerPacker;

        std::vector<mfxHDLPair>                     m_reconQueue;
        std::vector<mfxHDLPair>                     m_bsQueue;
        std::vector<mfxHDLPair>                     m_mbqpQueue;

        mfxU16                                      m_forcedCodingFunction;
        mfxU8                                       m_numSkipFrames;
        mfxU32                                      m_sizeSkipFrames;
        mfxU32                                      m_skipMode;

        std::vector<ENCODE_RECT>                    m_dirtyRects;
        std::vector<MOVE_RECT>                      m_movingRects;
    };


    class D3D11SvcEncoder : public DriverEncoder
    {
    public:
        D3D11SvcEncoder();

        virtual
        ~D3D11SvcEncoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE * core,
            GUID        guid,
            mfxU32      width,
            mfxU32      height,
            bool        isTemporal = false);

        virtual
        mfxStatus CreateAccelerationService(
            MfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            MfxVideoParam const & par);

        virtual
        mfxStatus Register(
            mfxMemId     memId,
            D3DDDIFORMAT type);

        virtual
        mfxStatus Register(
            mfxFrameAllocResponse & response,
            D3DDDIFORMAT            type);

        virtual
        mfxStatus Execute(
            mfxHDL                     surface,
            DdiTask const &            task,
            mfxU32                     fieldId,
            PreAllocatedVector const & sei);

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS & caps);

        virtual
        mfxStatus QueryMbPerSec(
            mfxVideoParam const & par,
            mfxU32              (&mbPerSec)[16]);

        virtual
        mfxStatus QueryStatus(
            DdiTask & task,
            mfxU32    fieldId);

        virtual
        mfxStatus Destroy();

        void ForceCodingFunction (mfxU16 codingFunction)
        {
            m_forcedCodingFunction = codingFunction;
        }

        virtual
        mfxStatus QueryHWGUID(
            VideoCORE * core,
            GUID        guid,
            bool        isTemporal);

    private:
        D3D11SvcEncoder(D3D11SvcEncoder const &); // no implementation
        D3D11SvcEncoder & operator =(D3D11SvcEncoder const &); // no implementation

        mfxStatus Init(
            GUID                 guid,
            ID3D11VideoDevice *  pVideoDevice,
            ID3D11VideoContext * pVideoContext,
            mfxU32               width,
            mfxU32               height);

        void PackSlice(
            OutputBitstream & obs,
            DdiTask const &   task,
            mfxU32            fieldId,
            mfxU32            sliceId) const;

    private:
        VideoCORE *                                     m_core;
        ID3D11VideoDevice *                             m_pVideoDevice;
        ID3D11VideoContext *                            m_pVideoContext;
        ID3D11VideoDecoder *                            m_pDecoder;
        GUID                                            m_guid;
        ID3D11VideoDecoderOutputView *                  m_pVDOView;

        ENCODE_CAPS                                     m_caps;
        std::vector<ENCODE_COMP_BUFFER_INFO>            m_compBufInfo;
        std::vector<D3DDDIFORMAT>                       m_uncompBufInfo;
        bool                                            m_infoQueried;

        std::vector<ENCODE_SET_SEQUENCE_PARAMETERS_SVC> m_sps;
        std::vector<ENCODE_SET_PICTURE_PARAMETERS_SVC>  m_pps;
        ENCODE_PACKEDHEADER_DATA                        m_packedSpsPps;
        ENCODE_PACKEDHEADER_DATA                        m_packedPps;
        std::vector<ENCODE_SET_SLICE_HEADER_SVC>        m_slice;
        std::vector<ENCODE_PACKEDHEADER_DATA>           m_packedSlice;

        std::vector<ENCODE_QUERY_STATUS_PARAMS>         m_feedbackUpdate;
        CachedFeedback                                  m_feedbackCached;

        std::vector<mfxHDLPair>                         m_reconQueue[8];
        std::vector<mfxHDLPair>                         m_bsQueue[8];

        mfxExtSVCSeqDesc                                m_extSvc;
        mfxU32                                          m_numdl;
        mfxU32                                          m_numql;
        mfxU32                                          m_numtl;

        static const mfxU32 MAX_PACKED_SPSPPS_SIZE = 4096;
        static const mfxU32 MAX_PACKED_SLICE_SIZE  = 1024;
        mfxU8 m_packedSpsPpsBuffer[MAX_PACKED_SPSPPS_SIZE]; // aud, sps, pps, sei
        mfxU8 m_packedSliceBuffer[MAX_PACKED_SLICE_SIZE];   // all slices of picture

        mfxU16                                           m_forcedCodingFunction;
    };

}; // namespace

#endif // #if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && (MFX_D3D11_ENABLED)
#endif // __MFX_H264_ENCODE_D3D11__H
/* EOF */
