//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2014 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_MJPEG_ENCODE_INTERFACE_H__
#define __MFX_MJPEG_ENCODE_INTERFACE_H__

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA)

#include <vector>
#include <assert.h>
#include "mfxdefs.h"
#include "mfxvideo++int.h"

#include "mfx_mjpeg_encode_hw_utils.h"

namespace MfxHwMJpegEncode
{

    class DriverEncoder
    {
    public:

        virtual ~DriverEncoder(){}

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE * core,
            mfxU32      width,
            mfxU32      height,
            bool        isTemporal = false) = 0;

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par) = 0;

        virtual
        mfxStatus RegisterBitstreamBuffer(
            mfxFrameAllocResponse & response) = 0;

        virtual
        mfxStatus Execute(DdiTask &task, mfxHDL surface) = 0;

        virtual
        mfxStatus QueryBitstreamBufferInfo(
            mfxFrameAllocRequest & request) = 0;

        virtual
        mfxStatus QueryEncodeCaps(
            JpegEncCaps & caps) = 0;

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

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA)
#endif // __MFX_MJPEG_ENCODE_INTERFACE_H__
