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

#include "mocks/include/common.h"
#include "mocks/include/guid.h"

#include <windows.h>
#include <d3d9types.h>

#if defined(DXVA2_API_DEFINED) && !defined(INITGUID)
//We need this because [DXVA2_ModeH264_VLD_NoFGT == DXVA2_ModeH264_E], which is defined at <dxva2api.h>
//But since this file contains a guard (DXVA2_API_DEFINED) it could be included before, w/o [INITGUID] is defined
//So, we could have a linker issue (unresolved external) for [DXVA2_ModeH264_E]
DEFINE_GUID(DXVA2_ModeH264_E,
    0x1b81be68, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5
);

#endif

#include <initguid.h>
#include <dxva.h>
#include <dxva2api.h>

#include <type_traits>

namespace mocks { namespace dxva
{
    using h264_decoders = pack<
          guid<&DXVA2_ModeH264_VLD_NoFGT>
        , guid<&DXVA_ModeH264_VLD_Multiview_NoFGT>
        , guid<&DXVA_ModeH264_VLD_Stereo_NoFGT>
        , guid<&DXVA_ModeH264_VLD_Stereo_Progressive_NoFGT>
#if defined(ENABLE_MFX_INTEL_GUID_DECODE)
        , guid<&DXVA_Intel_ModeH264_VLD_MVC>
#endif
    >;

    template <GUID const* Id>
    inline constexpr
    bool is_h264_decoder(guid<Id>)
    { return contains<guid<Id>, h264_decoders>::value; };

    using h265_decoders = pack<
          guid<&DXVA_ModeHEVC_VLD_Main>
        , guid<&DXVA_ModeHEVC_VLD_Main10>
#if defined(ENABLE_MFX_INTEL_GUID_DECODE)
        , guid<&DXVA_Intel_ModeHEVC_VLD_Main422_10Profile>
        , guid<&DXVA_Intel_ModeHEVC_VLD_Main444_10Profile>
        , guid<&DXVA_Intel_ModeHEVC_VLD_Main12Profile>
        , guid<&DXVA_Intel_ModeHEVC_VLD_Main422_12Profile>
        , guid<&DXVA_Intel_ModeHEVC_VLD_Main444_12Profile>
        , guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main_Profile>
        , guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main_10Profile>
        , guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main444_Profile>
        , guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main444_10Profile>
#endif //ENABLE_MFX_INTEL_GUID_DECODE
    >;

    template <GUID const* Id>
    inline constexpr
    bool is_h265_decoder(guid<Id>)
    { return contains<guid<Id>, h265_decoders>::value; };
    using av1_decoders = pack<
#if defined(ENABLE_MFX_INTEL_GUID_DECODE)
#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)
          guid<&DXVA_ModeAV1_VLD_Profile0> // TEMP: should be defined at MS public API
        , guid<&DXVA_Intel_ModeAV1_VLD>
        , guid<&DXVA_Intel_ModeAV1_VLD_420_10b>
#endif //MFX_ENABLE_AV1_VIDEO_DECODE
#endif //ENABLE_MFX_INTEL_GUID_DECODE
    >;

    template <GUID const* Id>
    inline constexpr
    bool is_av1_decoder(guid<Id>)
    { return contains<guid<Id>, av1_decoders>::value; };

    template <GUID const* Id>
    inline constexpr
    bool is_decoder(guid<Id>)
    {
        return
               is_h264_decoder(guid<Id>{})
            || is_h265_decoder(guid<Id>{})
            || is_av1_decoder (guid<Id>{});
    };

    using h264_encoders = pack<
#if defined(MFX_INTEL_GUID_ENCODE)
        guid<&DXVA2_Intel_Encode_AVC>
#endif //MFX_INTEL_GUID_ENCODE
    >;

    template <GUID const* Id>
    inline constexpr
    bool is_h264_encoder(guid<Id>)
    { return contains<guid<Id>, h264_encoders>::value; };

    using h265_encoders = pack<
#if defined(MFX_INTEL_GUID_ENCODE)
          guid<&DXVA2_Intel_Encode_HEVC_Main>
        , guid<&DXVA2_Intel_Encode_HEVC_Main10>
        , guid<&DXVA2_Intel_Encode_HEVC_Main422>
        , guid<&DXVA2_Intel_Encode_HEVC_Main422_10>
        , guid<&DXVA2_Intel_Encode_HEVC_Main444>
        , guid<&DXVA2_Intel_Encode_HEVC_Main444_10>
        , guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main>
        , guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main10>
        , guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main422>
        , guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main422_10>
        , guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main444>
        , guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main444_10>
        , guid<&DXVA2_Intel_Encode_HEVC_Main12>
        , guid<&DXVA2_Intel_Encode_HEVC_Main422_12>
        , guid<&DXVA2_Intel_Encode_HEVC_Main444_12>
#if defined(MFX_ENABLE_HEVCE_SCC)
        , guid<&DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main>
        , guid<&DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main10>
        , guid<&DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444>
        , guid<&DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444_10>
#endif //MFX_ENABLE_HEVCE_SCC
#endif //MFX_INTEL_GUID_ENCODE
    >;

    template <GUID const* Id>
    inline constexpr
    bool is_h265_encoder(guid<Id>)
    { return contains<guid<Id>, h265_encoders>::value; };

    using av1_encoders = pack<
#if defined(MFX_INTEL_GUID_ENCODE)
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
          guid<&DXVA2_Intel_LowpowerEncode_AV1_420_8b>
        , guid<&DXVA2_Intel_LowpowerEncode_AV1_420_10b>
#endif //MFX_ENABLE_AV1_VIDEO_DECODE
#endif //MFX_INTEL_GUID_ENCODE
    >;

    template <GUID const* Id>
    inline constexpr
    bool is_av1_encoder(guid<Id>)
    { return contains<guid<Id>, av1_encoders>::value; };

    template <GUID const* Id>
    inline constexpr
    bool is_encoder(guid<Id>)
    {
        return
               is_h264_encoder(guid<Id>{})
            || is_h265_encoder(guid<Id>{})
            || is_av1_encoder (guid<Id>{})
            ;
    }

    using h264_codec = typename cat<
        h264_decoders,
        h264_encoders
    >::type;

    using h265_codec = typename cat<
        h265_decoders,
        h265_encoders
    >::type;

    using av1_codec = typename cat<
        av1_decoders,
        av1_encoders
    >::type;

    template <GUID const* Id>
    inline constexpr
    bool is_h264_codec(guid<Id>)
    {
        return
               is_h264_decoder(guid<Id>{})
            || is_h264_encoder(guid<Id>{})
            ;
    }

    template <GUID const* Id>
    inline constexpr
    bool is_h265_codec(guid<Id>)
    {
        return
               is_h265_decoder(guid<Id>{})
            || is_h265_encoder(guid<Id>{})
            ;
    }

    template <GUID const* Id>
    inline constexpr
    bool is_av1_codec(guid<Id>)
    {
        return
               is_av1_decoder(guid<Id>{})
            || is_av1_encoder(guid<Id>{})
            ;
    }

    inline constexpr
    bool is_codec(GUID const&, pack<>)
    { return false; }

    template <typename... Codecs>
    inline constexpr
    bool is_codec(GUID const& id, pack<Codecs...>)
    {
        return invoke_if(
            [](auto&&) { return true; },
            [id](auto&& x) { return *(x.value) == id; },
            pack<Codecs...>{}
        );
    }

    inline constexpr
    bool is_h264_decoder(GUID const& id)
    { return is_codec(id, h264_decoders{}); }

    inline constexpr
    bool is_h265_decoder(GUID const& id)
    { return is_codec(id, h265_decoders{}); }

    inline constexpr
    bool is_av1_decoder(GUID const& id)
    { return is_codec(id, av1_decoders{}); }

    inline constexpr
    bool is_h264_encoder(GUID const& id)
    { return is_codec(id, h264_encoders{}); }

    inline constexpr
    bool is_h265_encoder(GUID const& id)
    { return is_codec(id, h265_encoders{}); }

    inline constexpr
    bool is_av1_encoder(GUID const& id)
    { return is_codec(id, av1_encoders{}); }

    inline constexpr
    bool is_h264_codec(GUID const& id)
    {
        return
               is_h264_decoder(id)
            || is_h264_encoder(id)
            ;
    }

    inline constexpr
    bool is_h265_codec(GUID const& id)
    {
        return
               is_h265_decoder(id)
            || is_h265_encoder(id)
            ;
    }

    inline constexpr
    bool is_av1_codec(GUID const& id)
    {
        return
               is_av1_decoder(id)
            || is_av1_encoder(id)
            ;
    }

    inline constexpr
    bool is_decoder(GUID const& id)
    {
        return
               is_h264_decoder(id)
            || is_h265_decoder(id)
            || is_av1_decoder (id)
            ;
    }

    inline constexpr
    bool is_encoder(GUID const& id)
    {
        return
               is_h264_encoder(id)
            || is_h265_encoder(id)
            || is_av1_encoder (id)
            ;
    }

} }
