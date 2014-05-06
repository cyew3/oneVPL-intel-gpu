    /*//////////////////////////////////////////////////////////////////////////////
    //
    //                  INTEL CORPORATION PROPRIETARY INFORMATION
    //     This software is supplied under the terms of a license agreement or
    //     nondisclosure agreement with Intel Corporation and may not be copied
    //     or disclosed except in accordance with the terms of that agreement.
    //          Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.
    //
    */

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined(MFX_VA)

#ifndef __MFX_MJPEG_ENCODE_INTERFACE__H
#define __MFX_MJPEG_ENCODE_INTERFACE__H

#include <vector>
#include <assert.h>
#include "mfxdefs.h"
#include "mfxvideo++int.h"

#include "mfx_mjpeg_encode_hw_utils.h"

#include "encoding_ddi.h"
#include "encoder_ddi.hpp"

namespace MfxHwMJpegEncode
{

    class DriverEncoder
    {
    public:

        virtual ~DriverEncoder(){}

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE * core,
            GUID        guid,
            mfxU32      width,
            mfxU32      height,
            bool        isTemporal = false) = 0;

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par) = 0;

        virtual
        mfxStatus Register(
            mfxMemId     memId,
            D3DDDIFORMAT type) = 0;

        virtual
        mfxStatus Register(
            mfxFrameAllocResponse & response,
            D3DDDIFORMAT            type) = 0;

        virtual
        mfxStatus Execute(DdiTask &task, mfxHDL surface) = 0;

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request) = 0;

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS_JPEG & caps) = 0;

        virtual
        mfxStatus QueryEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS & caps) = 0;

        virtual
        mfxStatus SetEncCtrlCaps(
            ENCODE_ENC_CTRL_CAPS const & caps) = 0;

        virtual
        mfxStatus QueryStatus(
            DdiTask & task) = 0;

        virtual
        mfxStatus UpdateBitstream(
            mfxMemId    MemId,
            DdiTask   & task) = 0;

        virtual
        mfxStatus Destroy() = 0;
    };

    DriverEncoder* CreatePlatformMJpegEncoder( VideoCORE* core );

}; // namespace

#endif // __MFX_MJPEG_ENCODE_INTERFACE__H
#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined(MFX_VA_WIN)
