//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_H265_ENCODE_HW_D3D_COMMON_H
#define __MFX_H265_ENCODE_HW_D3D_COMMON_H

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#include "mfx_interface_scheduler.h"

#include "encoding_ddi.h"
#include "mfx_h265_encode_hw_utils.h"
#include "mfx_h265_encode_hw.h"

namespace MfxHwH265Encode
{
    class D3DXCommonEncoder : public DriverEncoder
    {
    public:
        D3DXCommonEncoder();

        virtual
        ~D3DXCommonEncoder();

        // It may be blocking or not blocking call
        // depend on define MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        virtual
        mfxStatus QueryStatus(
            Task & task);

        virtual
        mfxStatus Execute(
            Task const & task, mfxHDLPair surface);

        // Init
        virtual
        mfxStatus Init(MFXCoreInterface *pCore);

        // Destroy
        virtual
            mfxStatus Destroy();

    protected:
        // async call
        virtual
        mfxStatus QueryStatusAsync(
            Task & task)=0;

        // sync call
        virtual
        mfxStatus WaitTaskSync(
            Task & task,
            mfxU32 timeOutMs);

        virtual
        mfxStatus ExecuteImpl(
            Task const & task, mfxHDLPair surface) = 0;

        MFXIScheduler2 *pSheduler;
        bool m_bSingleThreadMode;
    private:
        D3DXCommonEncoder(D3DXCommonEncoder const &); // no implementation
        D3DXCommonEncoder & operator =(D3DXCommonEncoder const &); // no implementation

    };
}; // namespace

#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE_HW) && (MFX_VA_WIN)
#endif // __MFX_H265_ENCODE_HW_D3D_COMMON_H
/* EOF */
