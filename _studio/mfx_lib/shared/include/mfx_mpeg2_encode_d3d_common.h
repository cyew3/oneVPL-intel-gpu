//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2020 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_MPEG2_ENCODE_D3D_COMMON_H
#define __MFX_MPEG2_ENCODE_D3D_COMMON_H

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#include "encoding_ddi.h"
#include "mfx_mpeg2_enc_common_hw.h"
#include "mfx_mpeg2_encode_interface.h"

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
#include "mfx_win_event_cache.h"

namespace MfxHwMpeg2Encode
{
    class D3DXCommonEncoder : public DriverEncoder
    {
    public:
        D3DXCommonEncoder();

        virtual
        ~D3DXCommonEncoder();

        mfxStatus InitCommonEnc(VideoCORE * pCore);

        virtual mfxStatus FillBSBuffer(mfxU32 nFeedback, mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt, GPU_SYNC_EVENT_HANDLE *pEvent);
        virtual mfxStatus FillBSBuffer(mfxU32 nFeedback, mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt) = 0;

        D3DXCommonEncoder(D3DXCommonEncoder const &) = delete;
        D3DXCommonEncoder & operator =(D3DXCommonEncoder const &) = delete;

    protected:
        // async call
        virtual mfxStatus QueryStatusAsync(mfxU32 nFeedback, mfxU32 &bitstreamSize) = 0;

        // sync call
        virtual mfxStatus WaitTaskSync(mfxU32 timeOutMs, GPU_SYNC_EVENT_HANDLE *pEvent);

        std::unique_ptr<EventCache> m_EventCache;
        bool m_bIsBlockingTaskSyncEnabled;

    };
}; // namespace

#endif // MFX_ENABLE_HW_BLOCKING_TASK_SYNC
#endif // #if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE) && (MFX_VA_WIN)
#endif // __MFX_MPEG2_ENCODE_D3D_COMMON_H
/* EOF */
