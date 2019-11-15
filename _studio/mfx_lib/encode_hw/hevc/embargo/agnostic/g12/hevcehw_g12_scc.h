// Copyright (c) 2019 Intel Corporation
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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_base.h"
#include "hevcehw_g12_data.h"

namespace HEVCEHW
{
namespace Gen12
{
class SCC
    : public FeatureBase
{
public:
#define DECL_BLOCK_LIST\
    DECL_BLOCK(SetLowPowerDefault)\
    DECL_BLOCK(SetGUID)\
    DECL_BLOCK(CheckProfile)\
    DECL_BLOCK(SetDefaults)\
    DECL_BLOCK(Init)\
    DECL_BLOCK(SetSPSExt)\
    DECL_BLOCK(SetPPSExt)\
    DECL_BLOCK(LoadSPSPPS)\
    DECL_BLOCK(PatchSliceHeader)\
    DECL_BLOCK(PatchDDITask)
#define DECL_FEATURE_NAME "G12_SCC"
#include "hevcehw_decl_blocks.h"

    SCC(mfxU32 FeatureId)
        : FeatureBase(FeatureId)
    {}

protected:
    virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push) override;
    virtual void InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push) override;
    virtual void PostReorderTask(const FeatureBlocks& /*blocks*/, TPushPostRT Push) override;

    virtual void SetExtraGUIDs(StorageRW& /*strg*/) {};

    static bool ReadSpsExt(StorageRW& strg, const Gen11::SPS&, mfxU8 id, Gen11::IBsReader& bs);
    static bool ReadPpsExt(StorageRW& strg, const Gen11::PPS&, mfxU8 id, Gen11::IBsReader& bs);
    static bool PackSpsExt(StorageRW& strg, const Gen11::SPS&, mfxU8 id, Gen11::IBsWriter& bs);
    static bool PackPpsExt(StorageRW& strg, const Gen11::PPS&, mfxU8 id, Gen11::IBsWriter& bs);

    static const GUID DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main;
    static const GUID DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main10;
    static const GUID DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444;
    static const GUID DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444_10;
    bool m_bPatchNextDDITask = false;
};

} //Gen12
} //namespace HEVCEHW

#endif
