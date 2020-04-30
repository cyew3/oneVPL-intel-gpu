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

#include <mfxdefs.h>
#include "mocks/include/guid.h"

#include <windows.h>
#include <d3d9types.h>

#include <initguid.h>

#include <dxva.h>
#include <dxva2api.h>

#include <type_traits>

#ifndef __ENCODING_DDI_H__

DEFINE_GUID(DXVA2_Intel_Encode_AVC,
    0x97688186, 0x56a8, 0x4094, 0xb5, 0x43, 0xfc, 0x9d, 0xaa, 0xa4, 0x9f, 0x4b);

#if (MFX_VERSION >= MFX_VERSION_NEXT) && !defined(STRIP_EMBARGO)
DEFINE_GUID(DXVA2_Intel_LowpowerEncode_AV1_420_8b,
    0x8090a09c, 0x6fc5, 0x48ef, 0x9f, 0x40, 0x65, 0xbf, 0x37, 0xf7, 0xac, 0xc4);

DEFINE_GUID(DXVA2_Intel_LowpowerEncode_AV1_420_10b,
    0xeea5b11, 0x88c0, 0x4a9f, 0x81, 0xac, 0xb1, 0xf, 0xd6, 0x6b, 0x22, 0xea);

#endif

#endif // !__ENCODING_DDI_H__

DEFINE_GUID(DXVA2_Intel_Encode_HEVC_Main,
    0x28566328, 0xf041, 0x4466, 0x8b, 0x14, 0x8f, 0x58, 0x31, 0xe7, 0x8f, 0x8b);

DEFINE_GUID(DXVA2_Intel_Encode_HEVC_Main10,
    0x6b4a94db, 0x54fe, 0x4ae1, 0x9b, 0xe4, 0x7a, 0x7d, 0xad, 0x00, 0x46, 0x00);

// {E484DCB8-CAC9-4859-99F5-5C0D45069089}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main422_10Profile,
    0xe484dcb8, 0xcac9, 0x4859, 0x99, 0xf5, 0x5c, 0xd, 0x45, 0x6, 0x90, 0x89);

// {6A6A81BA-912A-485D-B57F-CCD2D37B8D94}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main444_10Profile,
    0x6a6a81ba, 0x912a, 0x485d, 0xb5, 0x7f, 0xcc, 0xd2, 0xd3, 0x7b, 0x8d, 0x94);

// {8FF8A3AA-C456-4132-B6EF-69D9DD72571D}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main12Profile,
    0x8ff8a3aa, 0xc456, 0x4132, 0xb6, 0xef, 0x69, 0xd9, 0xdd, 0x72, 0x57, 0x1d);

// {C23DD857-874B-423C-B6E0-82CEAA9B118A}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main422_12Profile,
    0xc23dd857, 0x874b, 0x423c, 0xb6, 0xe0, 0x82, 0xce, 0xaa, 0x9b, 0x11, 0x8a);

// {5B08E35D-0C66-4C51-A6F1-89D00CB2C197}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_Main444_12Profile,
    0x5b08e35d, 0xc66, 0x4c51, 0xa6, 0xf1, 0x89, 0xd0, 0xc, 0xb2, 0xc1, 0x97);

#if (MFX_VERSION >= 1032)
// {0E4BC693-5D2C-4936-B125-AEFE32B16D8A}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_SCC_Main_Profile,
    0xe4bc693, 0x5d2c, 0x4936, 0xb1, 0x25, 0xae, 0xfe, 0x32, 0xb1, 0x6d, 0x8a);

// {2F08B5B1-DBC2-4d48-883A-4E7B8174CFF6}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_SCC_Main_10Profile,
    0x2f08b5b1, 0xdbc2, 0x4d48, 0x88, 0x3a, 0x4e, 0x7b, 0x81, 0x74, 0xcf, 0xf6);

// {5467807A-295D-445d-BD2E-CBA8C2457C3D}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_SCC_Main444_Profile,
    0x5467807a, 0x295d, 0x445d, 0xbd, 0x2e, 0xcb, 0xa8, 0xc2, 0x45, 0x7c, 0x3d);

// {AE0D4E15-2360-40a8-BF82-028E6A0DD827}
DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_SCC_Main444_10Profile,
    0xae0d4e15, 0x2360, 0x40a8, 0xbf, 0x82, 0x2, 0x8e, 0x6a, 0xd, 0xd8, 0x27);
#endif

#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
DEFINE_GUID(DXVA2_Intel_Encode_HEVC_Main12,
    0xd6d6bc4f, 0xd51a, 0x4712, 0x97, 0xe8, 0x75, 0x09, 0x17, 0xc8, 0x60, 0xfd);
#endif

DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report,
    0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);

namespace mocks { namespace dxva
{
    template <GUID const* Id>
    using is_h264_decoder = std::disjunction<
        std::is_same<guid<Id>, guid<&DXVA2_ModeH264_VLD_NoFGT> >
    >;

    template <GUID const* Id>
    using is_h265_decoder = std::disjunction<
          std::is_same<guid<Id>, guid<&DXVA_ModeHEVC_VLD_Main> >
        , std::is_same<guid<Id>, guid<&DXVA_ModeHEVC_VLD_Main10> >
        , std::is_same<guid<Id>, guid<&DXVA_Intel_ModeHEVC_VLD_Main422_10Profile> >
        , std::is_same<guid<Id>, guid<&DXVA_Intel_ModeHEVC_VLD_Main444_10Profile> >
#if (MFX_VERSION >= 1032)
        , std::is_same<guid<Id>, guid<&DXVA_Intel_ModeHEVC_VLD_Main12Profile> >
        , std::is_same<guid<Id>, guid<&DXVA_Intel_ModeHEVC_VLD_Main422_12Profile> >
        , std::is_same<guid<Id>, guid<&DXVA_Intel_ModeHEVC_VLD_Main444_12Profile> >
        , std::is_same<guid<Id>, guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main_Profile> >
        , std::is_same<guid<Id>, guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main_10Profile> >
        , std::is_same<guid<Id>, guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main444_Profile> >
        , std::is_same<guid<Id>, guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main444_10Profile> >
#endif
    >;

    template <GUID const* Id>
    using is_decoder = std::disjunction<
          is_h264_decoder<Id>
        , is_h265_decoder<Id>
    >;

    template <GUID const* Id>
    using is_h264_encoder = std::disjunction<
        std::is_same<guid<Id>, guid<&DXVA2_Intel_Encode_AVC> >
    >;

    template <GUID const* Id>
    using is_h265_encoder = std::disjunction<
          std::is_same<guid<Id>, guid<&DXVA2_Intel_Encode_HEVC_Main> >
        , std::is_same<guid<Id>, guid<&DXVA2_Intel_Encode_HEVC_Main10> >
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
        , std::is_same<guid<Id>, guid<&DXVA2_Intel_Encode_HEVC_Main12> >
#endif
    >;

#if (MFX_VERSION >= MFX_VERSION_NEXT) && !defined(STRIP_EMBARGO)
    template <GUID const* Id>
    using is_av1_encoder = std::disjunction<
          std::is_same<guid<Id>, guid<&DXVA2_Intel_LowpowerEncode_AV1_420_8b> >
        , std::is_same<guid<Id>, guid<&DXVA2_Intel_LowpowerEncode_AV1_420_10b> >
    >;
#endif

    template <GUID const* Id>
    using is_encoder = std::disjunction<
          is_h264_encoder<Id>
        , is_h265_encoder<Id>
#if (MFX_VERSION >= MFX_VERSION_NEXT) && !defined(STRIP_EMBARGO)
        , is_av1_encoder<Id>
#endif
    >;

    template <GUID const* Id>
    using is_h264_codec = std::disjunction<
        is_h264_decoder<Id>,
        is_h264_encoder<Id>
    >;

    template <GUID const* Id>
    using is_h265_codec = std::disjunction<
        is_h265_decoder<Id>,
        is_h265_encoder<Id>
    >;

#if (MFX_VERSION >= MFX_VERSION_NEXT) && !defined(STRIP_EMBARGO)
    template <GUID const* Id>
    using is_av1_codec = is_av1_encoder<Id>;
#endif

} }
