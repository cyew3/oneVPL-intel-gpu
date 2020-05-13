// Copyright (c) 2019-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"
#include "hevcehw_base_d3d11_win.h"

namespace HEVCEHW
{
namespace Windows
{
namespace Base
{
    class Protected
        : public FeatureBase
    {
    public:
#define DECL_BLOCK_LIST\
    DECL_BLOCK(SetDeviceParam)\
    DECL_BLOCK(CheckAndFix)\
    DECL_BLOCK(SetDefaults)\
    DECL_BLOCK(Init)\
    DECL_BLOCK(Reset)\
    DECL_BLOCK(SetTaskCounter)\
    DECL_BLOCK(GetFeedback)\
    DECL_BLOCK(SetBsInfo)\
    DECL_BLOCK(IncCounter)
#define DECL_FEATURE_NAME "Base_Protected"
#include "hevcehw_decl_blocks.h"

        Protected(mfxU32 FeatureId)
            : FeatureBase(FeatureId)
        {}

        void SetRecFlag(mfxU16 f) { m_recFlag = f; }

    protected:
        virtual void SetSupported(ParamSupport& par) override;
        virtual void InitInternal(const FeatureBlocks& blocks, TPushII Push) override;
        virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
        virtual void SetDefaults(const FeatureBlocks& blocks, TPushSD Push) override;
        virtual void FrameSubmit(const FeatureBlocks& blocks, TPushFS Push) override;
        virtual void InitTask(const FeatureBlocks& blocks, TPushIT Push) override;
        virtual void PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push) override;
        virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;

        mfxAES128CipherCounter m_aesCounter = {};
        mfxAES128CipherCounter m_prevCounter = {};
        std::map<void*, mfxAES128CipherCounter> m_task;
        bool   m_bWiDi   = false;
        mfxU16 m_recFlag = 0;

        struct DX9Par
        {
            DXVADDI_VIDEODESC    Desc;
            ENCODE_CREATEDEVICE  ECD;
            D3DAES_CTR_IV        AES;
            PAVP_ENCRYPTION_MODE Mode;
        } m_dx9 = {};

        struct DX11Par
        {
            DDI_D3D11::CreateDeviceIn   ResetDeviceIn;
            DDI_D3D11::CreateDeviceOut  ResetDeviceOut;
            D3D11_AES_CTR_IV            AES;
            PAVP_ENCRYPTION_MODE        Mode;
            ENCODE_ENCRYPTION_SET       Set;
        } m_dx11 = {};
    };

} //Base
} //Windows
} //namespace HEVCEHW

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)