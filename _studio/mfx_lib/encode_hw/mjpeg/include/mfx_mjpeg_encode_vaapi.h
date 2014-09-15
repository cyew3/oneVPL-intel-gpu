/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_MJPEG_ENCODE_VAAPI_H__
#define __MFX_MJPEG_ENCODE_VAAPI_H__

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_LINUX)

#include <vector>
#include <assert.h>

#include "mfx_ext_buffers.h"
#include "mfxpcp.h"

#include "mfx_mjpeg_encode_hw_utils.h"
#include "mfx_mjpeg_encode_interface.h"


namespace MfxHwMJpegEncode
{
    class VAAPIEncoder : public DriverEncoder
    {
    public:
        VAAPIEncoder();

        virtual
        ~VAAPIEncoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE * core,
            mfxU32      width,
            mfxU32      height,
            bool        isTemporal = false);

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par);

        virtual
        mfxStatus RegisterBitstreamBuffer(
            mfxFrameAllocResponse & response);

        virtual
        mfxStatus Execute(DdiTask &task, mfxHDL surface);

        virtual
        mfxStatus QueryBitstreamBufferInfo(
            mfxFrameAllocRequest & request);

        virtual
        mfxStatus QueryEncodeCaps(
            JpegEncCaps & caps);

        virtual
        mfxStatus QueryStatus(
            DdiTask & task);

        virtual
        mfxStatus UpdateBitstream(
            mfxMemId    MemId,
            DdiTask   & task);

        virtual
        mfxStatus Destroy();

    private:
        VAAPIEncoder(VAAPIEncoder const &);              // no implementation
        VAAPIEncoder & operator =(VAAPIEncoder const &); // no implementation

        VideoCORE       * m_core;
        mfxU32            m_width;
        mfxU32            m_height;

        // ToDo change ENCODE_CAPS_JPEG to libVA analog
        //ENCODE_CAPS_JPEG  m_caps;
        bool              m_infoQueried;

        //std::vector<ENCODE_COMP_BUFFER_INFO>     m_compBufInfo;
        //std::vector<D3DDDIFORMAT>                m_uncompBufInfo;
        //std::vector<ENCODE_QUERY_STATUS_PARAMS>  m_feedbackUpdate;
        //CachedFeedback                           m_feedbackCached;
    };

}; // namespace

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_LINUX)
#endif // __MFX_MJPEG_ENCODE_VAAPI_H__
