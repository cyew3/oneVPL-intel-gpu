//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_H264_ENCODE_D3D9__H
#define __MFX_H264_ENCODE_D3D9__H

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#include <vector>
#include <assert.h>

#include "encoding_ddi.h"
#include "auxiliary_device.h"

#include "mfx_ext_buffers.h"
#include "mfxpcp.h"

#include "mfx_h264_enc_common_hw.h"
#include "mfx_h264_encode_d3d_common.h"

#define IDX_NOT_FOUND 0xffffffff

namespace MfxHwH264Encode
{
    // Class intoduces few helpers for Execute method of AuxiliaryDevice.
    class AuxiliaryDeviceHlp : public AuxiliaryDevice
    {
    public:
        // concealed method
        using AuxiliaryDevice::Execute;

        // 3 helpers which evaluate sizeof automaticaly.
        // Two additional overloaded methods for cases when
        // instead of input or output passed 0.
        template <typename T, typename U>
        HRESULT Execute(mfxU32 func, T& in, U& out)
        {
            return Execute(func, &in, sizeof(in), &out, sizeof(out));
        }

        template <typename T>
        HRESULT Execute(mfxU32 func, T& in, void*)
        {
            return Execute(func, &in, sizeof(in), 0, 0);
        }

        template <typename U>
        HRESULT Execute(mfxU32 func, void*, U& out)
        {
            return Execute(func, 0, 0, &out, sizeof(out));
        }
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        HRESULT Execute(mfxU32 func, GPU_SYNC_EVENT_HANDLE &in)
        {
            return Execute(func, &in, sizeof(in), 0, 0);
        }
#endif

    }; // class AuxiliaryDeviceHlp : public AuxiliaryDevice


    // syncop functionality
    class CachedFeedback
    {
    public:
        typedef ENCODE_QUERY_STATUS_PARAMS Feedback;
        typedef std::vector<Feedback> FeedbackStorage;

        void Reset(mfxU32 cacheSize);

        void Update(FeedbackStorage const & update);

        const Feedback * Hit(mfxU32 feedbackNumber) const;

        void Remove(mfxU32 feedbackNumber);

    private:
        FeedbackStorage m_cache;
    }; // class CachedFeedback



    // filling encoder structures
    void FillSpsBuffer(
        MfxVideoParam const &                 par,
        ENCODE_SET_SEQUENCE_PARAMETERS_H264 & sps);

    void FillVaringPartOfSpsBuffer(
        ENCODE_SET_SEQUENCE_PARAMETERS_H264 & sps,
        DdiTask const & task,
        mfxU32 fieldId);

    void FillVuiBuffer(
        MfxVideoParam const &      par,
        ENCODE_SET_VUI_PARAMETER & vui);

    void FillConstPartOfPpsBuffer(
        MfxVideoParam const &                par,
        ENCODE_CAPS const &                  hwCaps,
        ENCODE_SET_PICTURE_PARAMETERS_H264 & pps);

    void FillVaringPartOfPpsBuffer(
        DdiTask const &                      task,
        mfxU32                               fieldId,
        ENCODE_SET_PICTURE_PARAMETERS_H264 & pps,
        std::vector<ENCODE_RECT> &           dirtyRects,
        std::vector<MOVE_RECT> &             movingRects);

    void FillConstPartOfSliceBuffer(
        MfxVideoParam const &                       par,
        std::vector<ENCODE_SET_SLICE_HEADER_H264> & slice);

    void FillVaringPartOfSliceBuffer(
        ENCODE_CAPS const &                         hwCaps,
        DdiTask const &                             task,
        mfxU32                                      fieldId,
        ENCODE_SET_SEQUENCE_PARAMETERS_H264 const & sps,
        ENCODE_SET_PICTURE_PARAMETERS_H264 const &  pps,
        std::vector<ENCODE_SET_SLICE_HEADER_H264> & slice);

    mfxStatus FillVaringPartOfSliceBufferSizeLimited(
        ENCODE_CAPS const &                         hwCaps,
        DdiTask const &                             task,
        mfxU32                                      fieldId,
        ENCODE_SET_SEQUENCE_PARAMETERS_H264 const & sps,
        ENCODE_SET_PICTURE_PARAMETERS_H264 const &  pps,
        std::vector<ENCODE_SET_SLICE_HEADER_H264> & slice);


    mfxStatus FillSpsBuffer(
        MfxVideoParam const &                par,
        ENCODE_SET_SEQUENCE_PARAMETERS_SVC & sps,
        mfxU32                               did,
        mfxU32                               qid,
        mfxU32                               tid);

    void FillConstPartOfPpsBuffer(
        MfxVideoParam const &                par,
        ENCODE_SET_PICTURE_PARAMETERS_SVC &  pps,
        mfxU32                               did,
        mfxU32                               qid,
        mfxU32                               tid);

    void FillVaringPartOfPpsBuffer(
        DdiTask const &                     task,
        mfxU32                              fieldId,
        ENCODE_SET_PICTURE_PARAMETERS_SVC & pps);

    void FillConstPartOfSliceBuffer(
        MfxVideoParam const &                      par,
        std::vector<ENCODE_SET_SLICE_HEADER_SVC> & slice);

    void FillVaringPartOfSliceBuffer(
        ENCODE_CAPS const &                        hwCaps,
        mfxExtSVCSeqDesc const &                   extSvc,
        DdiTask const &                            task,
        mfxU32                                     fieldId,
        ENCODE_SET_SEQUENCE_PARAMETERS_SVC const & sps,
        ENCODE_SET_PICTURE_PARAMETERS_SVC const &  pps,
        std::vector<ENCODE_SET_SLICE_HEADER_SVC> & slice);

    mfxU32 AddEmulationPreventionAndCopy(
        ENCODE_PACKEDHEADER_DATA const & data,
        mfxU8*                           bsDataStart,
        mfxU8*                           bsEnd,
        bool                             bEmulationByteInsertion);

    void PackSlice(
        OutputBitstream &                   obs,
        DdiTask const &                     task,
        mfxExtSpsHeader const &             sps,
        mfxExtPpsHeader const &             pps,
        ENCODE_SET_SLICE_HEADER_SVC const & slice,
        mfxU32                              fieldId);

    // encoder
    class D3D9Encoder : public D3DXCommonEncoder
    {
    public:
        D3D9Encoder();

        virtual
        ~D3D9Encoder();

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

    protected:
        // async call
        virtual
        mfxStatus QueryStatusAsync(
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
            bool        isTemporal)
        {
            core; guid; isTemporal;
            return MFX_ERR_UNSUPPORTED;
        }

    private:
        D3D9Encoder(D3D9Encoder const &); // no implementation
        D3D9Encoder & operator =(D3D9Encoder const &); // no implementation

        VideoCORE *                       m_core;
        std::auto_ptr<AuxiliaryDeviceHlp> m_auxDevice;

        ENCODE_SET_SEQUENCE_PARAMETERS_H264         m_sps;
        ENCODE_SET_VUI_PARAMETER                    m_vui;
        ENCODE_SET_PICTURE_PARAMETERS_H264          m_pps;
        std::vector<ENCODE_SET_SLICE_HEADER_H264>   m_slice;
        std::vector<ENCODE_COMP_BUFFER_INFO>        m_compBufInfo;
        std::vector<D3DDDIFORMAT>                   m_uncompBufInfo;
        std::vector<ENCODE_QUERY_STATUS_PARAMS>     m_feedbackUpdate;
        std::vector<ENCODE_COMPBUFFERDESC>          m_compBufDesc;
        CachedFeedback                              m_feedbackCached;
        HeaderPacker                                m_headerPacker;

        GUID                 m_guid;
        mfxU32               m_width;
        mfxU32               m_height;
        ENCODE_CAPS          m_caps;
        ENCODE_ENC_CTRL_CAPS m_capsQuery; // from ENCODE_ENC_CTRL_CAPS_ID
        ENCODE_ENC_CTRL_CAPS m_capsGet;   // from ENCODE_ENC_CTRL_GET_ID
        bool                 m_infoQueried;

        mfxU16               m_forcedCodingFunction;
        mfxU8                m_numSkipFrames;
        mfxU32               m_sizeSkipFrames;
        mfxU32               m_skipMode;
        mfxU32               m_timeoutForTDR;

        std::vector<ENCODE_RECT> m_dirtyRects;
        std::vector<MOVE_RECT>   m_movingRects;
    };

    class D3D9SvcEncoder : public DriverEncoder
    {
    public:
        D3D9SvcEncoder();

        virtual
        ~D3D9SvcEncoder();

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

        //mfxStatus QueryEncCtrlCaps(
        //    ENCODE_ENC_CTRL_CAPS & caps);

        /*mfxStatus SetEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS const & caps);*/

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
            bool        isTemporal)
        {
            core; guid; isTemporal;
            return MFX_ERR_UNSUPPORTED;
        }

    private:
        D3D9SvcEncoder(D3D9SvcEncoder const &); // no implementation
        D3D9SvcEncoder & operator =(D3D9SvcEncoder const &); // no implementation

        VideoCORE *                                     m_core;
        std::auto_ptr<AuxiliaryDeviceHlp>               m_auxDevice;

        std::vector<ENCODE_SET_SEQUENCE_PARAMETERS_SVC> m_sps;
        std::vector<ENCODE_SET_PICTURE_PARAMETERS_SVC>  m_pps;
        std::vector<ENCODE_SET_SLICE_HEADER_SVC>        m_slice;

        std::vector<ENCODE_COMP_BUFFER_INFO>            m_compBufInfo;
        std::vector<D3DDDIFORMAT>                       m_uncompBufInfo;
        std::vector<ENCODE_QUERY_STATUS_PARAMS>         m_feedbackUpdate;
        CachedFeedback                                  m_feedbackCached;
        HeaderPacker                                    m_headerPacker;

        MfxVideoParam const * m_video;
        GUID                  m_guid;
        ENCODE_CAPS           m_caps;
        ENCODE_ENC_CTRL_CAPS  m_capsQuery; // from ENCODE_ENC_CTRL_CAPS_ID
        ENCODE_ENC_CTRL_CAPS  m_capsGet;   // from ENCODE_ENC_CTRL_GET_ID
        bool                  m_infoQueried;

        mfxU16                m_forcedCodingFunction;
    };

}; // namespace

#endif // MFX_ENABLE_H264_VIDEO_ENCODE && MFX_VA_WIN
#endif // __MFX_H264_ENCODE_D3D9__H
/* EOF */
