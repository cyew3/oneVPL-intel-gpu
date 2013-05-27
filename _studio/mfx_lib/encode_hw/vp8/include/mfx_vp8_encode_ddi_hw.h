/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012 Intel Corporation. All Rights Reserved.
//
*/
#include "mfx_common.h"
#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE) && defined(MFX_VA)

#ifndef _MFX_VP8_ENCODE_DDI_HW_H_
#define _MFX_VP8_ENCODE_DDI_HW_H_

#include "encoding_ddi.h"
#include "auxiliary_device.h"
#include <vector>
#include "mfx_vp8_encode_utils_hw.h"

#define D3DDDIFMT_MOTIONVECTORBUFFER D3DFORMAT(0) //fixme

namespace MFX_VP8ENC
{
    class DriverEncoder;

    mfxStatus QueryHwCaps(eMFXVAType va_type, mfxU32 adapterNum, ENCODE_CAPS_VP8 & caps);
    mfxStatus CheckVideoParam(mfxVideoParam const & par, ENCODE_CAPS_VP8 const &caps);

    DriverEncoder* CreatePlatformVp8Encoder(VideoCORE* core);

   /* Convert MediaSDK into DDI */

    void FillSpsBuffer(mfxVideoParam const & par, 
        ENCODE_SET_SEQUENCE_PARAMETERS_VP8 & sps);

    mfxStatus FillPpsBuffer(Task const &pTask, mfxVideoParam const & par, 
        ENCODE_SET_PICTURE_PARAMETERS_VP8 & pps);

    mfxStatus FillQuantBuffer(Task const &task, mfxVideoParam const & par,
        ENCODE_SET_Qmatrix_VP8 & quant);


    class DriverEncoder
    {
    public:

        virtual ~DriverEncoder(){}

        virtual mfxStatus CreateAuxilliaryDevice(
                        VideoCORE * core,
                        GUID        guid,
                        mfxU32      width,
                        mfxU32      height) = 0;

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par) = 0;

        virtual
        mfxStatus Reset(
            mfxVideoParam const & par) = 0;

        virtual 
        mfxStatus Register(
            mfxFrameAllocResponse & response,
            D3DDDIFORMAT            type) = 0;

        virtual 
        mfxStatus Execute(
            Task const &task, 
            mfxHDL surface) = 0;

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request) = 0;

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS_VP8 & caps) = 0;

        //virtual
        //mfxStatus QueryStatus( 
        //    Task & task ) = 0;

        virtual
        mfxStatus Destroy() = 0;    
    };

    class AuxiliaryDeviceHlp : public AuxiliaryDevice
    {
    public:
        using AuxiliaryDevice::Execute;

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

    };    
    
    // syncop functionality
    class CachedFeedback
    {
    public:
        typedef ENCODE_QUERY_STATUS_PARAMS Feedback;
        typedef std::vector<Feedback> FeedbackStorage;

        void Reset(mfxU32 cacheSize);

        mfxStatus Update(FeedbackStorage const & update);

        const Feedback * Hit(mfxU32 feedbackNumber) const;

        mfxStatus Remove(mfxU32 feedbackNumber);

    private:
        FeedbackStorage m_cache;

    };

    // encoder
    class D3D9Encoder : public DriverEncoder
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
            mfxU32      height);

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            mfxVideoParam const & par);

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
            Task const&task, 
            mfxHDL surface);

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS_VP8 & caps);

        virtual
        mfxStatus QueryEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS & caps);

        virtual
        mfxStatus SetEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS const & caps);

        //virtual
        //mfxStatus QueryStatus(
        //    Task & task);

        //virtual
        //mfxStatus UpdateBitstream(
        //    mfxMemId    MemId,
        //    Task      & task);

        virtual
        mfxStatus Destroy();

    private:
        D3D9Encoder(D3D9Encoder const &);              // no implementation
        D3D9Encoder & operator =(D3D9Encoder const &); // no implementation

        VideoCORE *                       m_core;
        std::auto_ptr<AuxiliaryDeviceHlp> m_auxDevice;

        ENCODE_SET_SEQUENCE_PARAMETERS_VP8 m_sps;        
        ENCODE_SET_PICTURE_PARAMETERS_VP8  m_pps;
        ENCODE_SET_Qmatrix_VP8             m_quant;
        
        GUID                 m_guid;
        mfxU32               m_width;
        mfxU32               m_height;        
        ENCODE_CAPS_VP8      m_caps;
        ENCODE_ENC_CTRL_CAPS m_capsQuery; // from ENCODE_ENC_CTRL_CAPS_ID
        ENCODE_ENC_CTRL_CAPS m_capsGet;  
        bool                 m_infoQueried;

        std::vector<ENCODE_COMP_BUFFER_INFO>     m_compBufInfo;
        std::vector<D3DDDIFORMAT>                m_uncompBufInfo;
        std::vector<ENCODE_QUERY_STATUS_PARAMS>  m_feedbackUpdate;
        CachedFeedback                           m_feedbackCached;        

        ENCODE_MBDATA_LAYOUT m_layout; 
        VP8MfxParam          m_video;
    };
#if defined(MFX_D3D11_ENABLED)
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
            mfxU32      height);

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            mfxVideoParam const & par);

        virtual
        mfxStatus Register(
            mfxMemId     memId,
            D3DDDIFORMAT type);

        virtual
        mfxStatus Register(
            mfxFrameAllocResponse & response,
            D3DDDIFORMAT            type);

        virtual
        mfxStatus Execute(const Task &task, mfxHDL surface);

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS_VP8 & caps);

        virtual
        mfxStatus QueryEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS & caps);

        virtual
        mfxStatus SetEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS const & caps);

        //virtual
        //mfxStatus QueryStatus(
        //    Task & task);

        //virtual
        //mfxStatus UpdateBitstream(
        //    mfxMemId    MemId,
         //   Task   & task);

        virtual
        mfxStatus Destroy();

    private:
        D3D11Encoder(D3D11Encoder const &);              // no implementation
        D3D11Encoder & operator =(D3D11Encoder const &); // no implementation

        VideoCORE       * m_core;        
        //ExecuteBuffers  * m_pDdiData;
        GUID              m_guid;
        mfxU32            m_width;
        mfxU32            m_height;
        //mfxU32            m_counter;
        ENCODE_CAPS_VP8   m_caps;
        bool              m_infoQueried;
        VP8MfxParam       m_video;

        std::vector<ENCODE_COMP_BUFFER_INFO>     m_compBufInfo;
        std::vector<D3DDDIFORMAT>                m_uncompBufInfo;
        //std::vector<ENCODE_QUERY_STATUS_PARAMS>  m_feedbackUpdate;
        //CachedFeedback                           m_feedbackCached;

        ENCODE_ENC_CTRL_CAPS m_capsQuery; // from ENCODE_ENC_CTRL_CAPS_ID
        ENCODE_ENC_CTRL_CAPS m_capsGet;   // from ENCODE_ENC_CTRL_GET_ID
    };
#endif


}
#endif
#endif