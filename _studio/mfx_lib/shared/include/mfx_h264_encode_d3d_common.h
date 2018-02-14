//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_H264_ENCODE_D3D_COMMON_H
#define __MFX_H264_ENCODE_D3D_COMMON_H

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined (MFX_VA_WIN)

#include "mfx_interface_scheduler.h"

#include "encoding_ddi.h"
#include "mfx_h264_encode_interface.h"

namespace MfxHwH264Encode
{
    class D3DXCommonEncoder : public DriverEncoder
    {
    public:
        D3DXCommonEncoder();

        virtual
        ~D3DXCommonEncoder();

        virtual
            mfxStatus QueryStatus(DdiTask & task, mfxU32 fieldId);

        // Init
        virtual
            mfxStatus Init(VideoCORE *core);

        // Destroy
        virtual
            mfxStatus Destroy();

    protected:
        // async call
        virtual mfxStatus QueryStatusAsync(DdiTask & task, mfxU32 fieldId) = 0;

        // sync call
        virtual mfxStatus WaitTaskSync(DdiTask & task, mfxU32 fieldId, mfxU32 timeOutM);

        MFXIScheduler2 *pSheduler;
        bool m_bSingleThreadMode;

    private:
        D3DXCommonEncoder(D3DXCommonEncoder const &); // no implementation
        D3DXCommonEncoder & operator =(D3DXCommonEncoder const &); // no implementation

    };
}; // namespace

#endif // #if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && (MFX_VA_WIN)
#endif // __MFX_H264_ENCODE_D3D_COMMON_H
/* EOF */
