//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_MJPEG_ENCODE_D3D_COMMON_H
#define __MFX_MJPEG_ENCODE_D3D_COMMON_H

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#include "encoding_ddi.h"
#include "mfx_mjpeg_encode_interface.h"

namespace MfxHwMJpegEncode
{
    class D3DXCommonEncoder : public DriverEncoder
    {
    public:
        D3DXCommonEncoder();

        virtual ~D3DXCommonEncoder();

        virtual mfxStatus QueryStatus(DdiTask & task);

        virtual mfxStatus Execute(DdiTask &task, mfxHDL surface);

    protected:
        // async call
        virtual mfxStatus QueryStatusAsync(DdiTask & task) = 0;

        // sync call
        virtual mfxStatus WaitTaskSync(DdiTask & task, mfxU32 timeOutMs);

        virtual mfxStatus ExecuteImpl(DdiTask &task, mfxHDL surface) = 0;

    private:
        D3DXCommonEncoder(D3DXCommonEncoder const &); // no implementation
        D3DXCommonEncoder & operator =(D3DXCommonEncoder const &); // no implementation

    };
}; // namespace

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && (MFX_VA_WIN)
#endif // __MFX_MJPEG_ENCODE_D3D_COMMON_H
/* EOF */
