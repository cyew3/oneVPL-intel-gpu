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

#include "mocks/include/mfx/detail/h265/encoder.hxx"

#include "mfx_common_int.h"

#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <numeric>

namespace mocks { namespace mfx { namespace h265 { namespace dx11
{
    namespace detail
    {
        template <typename Gen>
        inline
        typename std::enable_if<mfx::is_gen<Gen>::value, int>::type
        max_chroma(Gen, mfxVideoParam const& vp)
        {
            auto co3 = reinterpret_cast<mfxExtCodingOption3 const*>(
                GetExtendedBuffer(vp.ExtParam, vp.NumExtParam, MFX_EXTBUFF_CODING_OPTION3)
            );
            auto chroma = co3 && co3->TargetChromaFormatPlus1 ?
                co3->TargetChromaFormatPlus1 - 1 : fourcc::chroma_of(vp.mfx.FrameInfo.FourCC);

            auto const profile  = vp.mfx.CodecProfile;
            auto const p = reinterpret_cast<mfxExtHEVCParam const*>(GetExtendedBuffer(vp.ExtParam, vp.NumExtParam, MFX_EXTBUFF_HEVC_PARAM));
            auto const flags = (profile >= MFX_PROFILE_HEVC_REXT && p) ? p->GeneralConstraintFlags : 0;

            auto const& c = mfx::h265::caps(Gen{}, vp);
            if (!c.YUV444ReconSupport && !c.YUV422ReconSupport)
                chroma = MFX_CHROMAFORMAT_YUV420;

            auto max = (chroma == MFX_CHROMAFORMAT_YUV420
                || profile == MFX_PROFILE_HEVC_MAIN
                || profile == MFX_PROFILE_HEVC_MAINSP
                || profile == MFX_PROFILE_HEVC_MAIN10
                || (flags & MFX_HEVC_CONSTR_REXT_MAX_420CHROMA)) * MFX_CHROMAFORMAT_YUV420;

            max += (chroma == MFX_CHROMAFORMAT_YUV422
                || (flags & MFX_CHROMAFORMAT_YUV422)
                && c.YUV422ReconSupport) * MFX_CHROMAFORMAT_YUV422;

            max += (chroma == MFX_CHROMAFORMAT_YUV444
                || (flags & MFX_CHROMAFORMAT_YUV444)
                && c.YUV444ReconSupport) * MFX_CHROMAFORMAT_YUV444;

            return max;
        }

        template <typename Gen>
        inline
        typename std::enable_if<mfx::is_gen<Gen>::value, int>::type
        max_LCU_size(Gen, mfxVideoParam const& vp)
        {
            auto const& c = mfx::h265::caps(Gen{}, vp);
            return
                1 << (::mfx::CeilLog2(c.LCUSizeSupported + 1) + 3);
        }

        inline
        D3D11_TEXTURE2D_DESC make_rec_desc(gen0, mfxVideoParam const&, D3D11_USAGE, UINT /*bind*/, UINT /*access*/)
        {
            assert(!"H.265 is not supported by this Gen");
            return D3D11_TEXTURE2D_DESC{};
        }

        inline
        D3D11_TEXTURE2D_DESC make_rec_desc(gen9, mfxVideoParam const& vp, D3D11_USAGE usage, UINT bind, UINT access)
        {
            auto const chroma =
                max_chroma(gen9{}, vp);

            auto vp_rec = vp;
            switch (chroma)
            {
                case MFX_CHROMAFORMAT_YUV420:
                    vp_rec.mfx.FrameInfo.FourCC = vp_rec.mfx.FrameInfo.BitDepthLuma > 8 ? MFX_FOURCC_P010 : MFX_FOURCC_NV12;
                    bind |= D3D11_BIND_VIDEO_ENCODER;
                    break;

                case MFX_CHROMAFORMAT_YUV422:
                    vp_rec.mfx.FrameInfo.FourCC = vp_rec.mfx.FrameInfo.BitDepthLuma > 8 ? MFX_FOURCC_Y210 : MFX_FOURCC_YUY2;
                    vp_rec.mfx.FrameInfo.Width  /= 2;
                    vp_rec.mfx.FrameInfo.Height *= 2;
                    break;

                case MFX_CHROMAFORMAT_YUV444:
                    if (!(vp_rec.mfx.FrameInfo.BitDepthLuma > 8))
                    {
                        vp_rec.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
                        vp_rec.mfx.FrameInfo.Width  = ::mfx::align2_value<mfxU16>(vp_rec.mfx.FrameInfo.Width, 512 / 4);
                        vp_rec.mfx.FrameInfo.Height = ::mfx::align2_value<mfxU16>(mfxU16(vp_rec.mfx.FrameInfo.Height * 3 / 4), 8);
                    }
                    else
                    {
                        vp_rec.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
                        vp_rec.mfx.FrameInfo.Width  = ::mfx::align2_value<mfxU16>(vp_rec.mfx.FrameInfo.Width, 256 / 4);
                        vp_rec.mfx.FrameInfo.Height = ::mfx::align2_value<mfxU16>(mfxU16(vp_rec.mfx.FrameInfo.Height * 3 / 2), 8);
                    }
                    break;

                default: assert(!"Unsupported chroma format");
            };

            return mfx::dx11::make_texture_desc(
                vp_rec, usage, usage == D3D11_USAGE_STAGING ? 0 : bind, access
            );
        }

        inline
        D3D11_TEXTURE2D_DESC make_rec_desc(gen12, mfxVideoParam const& vp, D3D11_USAGE usage, UINT bind, UINT access)
        {
            auto co3 = reinterpret_cast<mfxExtCodingOption3 const*>(
                GetExtendedBuffer(vp.ExtParam, vp.NumExtParam, MFX_EXTBUFF_CODING_OPTION3)
            );

            auto const bits   = co3 && co3->TargetBitDepthLuma ?
                co3->TargetBitDepthLuma - 1 : vp.mfx.FrameInfo.BitDepthLuma;
            auto const chroma = max_chroma(gen12{}, vp);

            if (bits < 10 || (bits == 10 && chroma == MFX_CHROMAFORMAT_YUV444))
                return make_rec_desc(gen11{}, vp, usage, bind, access);

            auto vp_rec = vp;
            switch (chroma)
            {
                case MFX_CHROMAFORMAT_YUV420:
                    vp_rec.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
                    vp_rec.mfx.FrameInfo.Width  = ::mfx::align2_value(vp_rec.mfx.FrameInfo.Width, 32) * 2;
                    bind |= D3D11_BIND_VIDEO_ENCODER;
                    break;

                case MFX_CHROMAFORMAT_YUV422:
                    vp_rec.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
                    vp_rec.mfx.FrameInfo.Width  /= 2;
                    vp_rec.mfx.FrameInfo.Height *= 2;
                    break;

                case MFX_CHROMAFORMAT_YUV444:
                    vp_rec.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
                    vp_rec.mfx.FrameInfo.Width  = ::mfx::align2_value(vp_rec.mfx.FrameInfo.Width, 256 / 4);
                    vp_rec.mfx.FrameInfo.Height = ::mfx::align2_value(mfxU16(vp_rec.mfx.FrameInfo.Height * 3 / 2), 8);
                    break;

                default: assert(!"Unsupported chroma format");
            };

            return mfx::dx11::make_texture_desc(
                vp_rec, usage, usage == D3D11_USAGE_STAGING ? 0 : bind, access
            );
        }

        template <int Type>
        inline
        D3D11_TEXTURE2D_DESC make_rec_desc(std::integral_constant<int, Type>, mfxVideoParam const& vp, D3D11_USAGE usage, UINT bind, UINT access)
        {
            return make_rec_desc(gen_of<Type>::type{},
                vp, usage, bind, access
            );
        }

        inline
        D3D11_TEXTURE2D_DESC make_rec_desc(int type, mfxVideoParam const& vp, D3D11_USAGE usage, UINT bind, UINT access)
        {
            return invoke(type, MOCKS_LIFT(make_rec_desc),
                vp, usage, bind, access
            );
        }

        template <int Type>
        inline
        D3D11_TEXTURE2D_DESC make_mbqp_desc(std::integral_constant<int, Type>, mfxVideoParam const& vp, D3D11_USAGE usage, UINT bind, UINT access)
        {
            auto width  = ::mfx::CeilDiv(vp.mfx.FrameInfo.Width,  mfxU16(16));
            auto height = ::mfx::CeilDiv(vp.mfx.FrameInfo.Height, mfxU16(16));

            auto hevc = reinterpret_cast<mfxExtHEVCParam const*>(
                GetExtendedBuffer(vp.ExtParam, vp.NumExtParam, MFX_EXTBUFF_HEVC_PARAM)
            );

            auto blocks =
                (hevc ? mfxU16(hevc->LCUSize) : max_LCU_size(gen_of<Type>::type{}, vp)) / 16;
            width = ::mfx::align2_value(width, blocks);

            auto vp_mbqp = vp;
            vp_mbqp.mfx.FrameInfo.FourCC       = MFX_FOURCC_P8_TEXTURE;
            vp_mbqp.mfx.FrameInfo.ChromaFormat = 0;
            vp_mbqp.mfx.FrameInfo.Width        = width;
            vp_mbqp.mfx.FrameInfo.Height       = height;

            return mfx::dx11::make_texture_desc(
                vp_mbqp, usage, bind, access
            );
        }

        inline
        D3D11_TEXTURE2D_DESC make_mbqp_desc(int type, mfxVideoParam const& vp, D3D11_USAGE usage, UINT bind, UINT access)
        {
            return invoke(type, MOCKS_LIFT(make_mbqp_desc),
                vp, usage, bind, access
            );
        }

        template <int Type>
        inline
        D3D11_BUFFER_DESC make_bs_desc(std::integral_constant<int, Type>, mfxVideoParam const& vp, D3D11_USAGE usage, UINT bind, UINT access)
        {
            auto co3 = reinterpret_cast<mfxExtCodingOption3 const*>(
                GetExtendedBuffer(vp.ExtParam, vp.NumExtParam, MFX_EXTBUFF_CODING_OPTION3)
            );
            auto const bits   = co3 && co3->TargetBitDepthLuma ?
                co3->TargetBitDepthLuma : vp.mfx.FrameInfo.BitDepthLuma;

            auto const chroma = co3 && co3->TargetChromaFormatPlus1 ?
                co3->TargetChromaFormatPlus1 - 1 : vp.mfx.FrameInfo.ChromaFormat;

            auto hevc = reinterpret_cast<mfxExtHEVCParam const*>(
                GetExtendedBuffer(vp.ExtParam, vp.NumExtParam, MFX_EXTBUFF_HEVC_PARAM)
            );

            auto width  = hevc ? hevc->PicWidthInLumaSamples :
                vp.mfx.FrameInfo.CropW ? (vp.mfx.FrameInfo.CropW + vp.mfx.FrameInfo.CropX) : vp.mfx.FrameInfo.Width;
            auto height = hevc ? hevc->PicHeightInLumaSamples :
                vp.mfx.FrameInfo.CropH ? (vp.mfx.FrameInfo.CropH + vp.mfx.FrameInfo.CropY) : vp.mfx.FrameInfo.Height;

            auto k = 2.0
                + ((bits   == 10)                        * 0.3) //TODO: check if bits > 10 needs spec. coefficient
                + ((chroma == MFX_CHROMAFORMAT_YUV422)   * 0.5)
                + ((chroma == MFX_CHROMAFORMAT_YUV444)   * 1.5);

            auto size = mfxU32(k * (width * height));
            auto vp_bs = vp;
            if (size)
            {
                vp_bs.mfx.FrameInfo.FourCC       = MFX_FOURCC_P8;
                vp_bs.mfx.FrameInfo.ChromaFormat = 0;
                vp_bs.mfx.FrameInfo.Height       = mfxU16(
                    ::mfx::CeilDiv<mfxU32>(size, vp_bs.mfx.FrameInfo.Width)
                );
            }

            return mfx::dx11::make_buffer_desc(
                vp_bs, usage, bind, access
            );
        }

        inline
        D3D11_BUFFER_DESC make_bs_desc(int type, mfxVideoParam const& vp, D3D11_USAGE usage, UINT bind, UINT access)
        {
            return invoke(type, MOCKS_LIFT(make_bs_desc),
                vp, usage, bind, access
            );
        }

        template <int Type>
        inline
        mocks::dx11::device&
        mock_encoder(gen0, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const&)
        {
            assert(!"H.265 is not supported by this Gen");
            return d;
        }

        template <int Type>
        inline
        mocks::dx11::device&
        mock_encoder(gen9, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
        {
            return mfx::dx11::mock_component(std::integral_constant<int, Type>{}, d
                , std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main>{}, vp)
            );
        }

        template <int Type>
        inline
        mocks::dx11::device&
        mock_encoder(gen10, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
        {
            mock_encoder(gen9{}, std::integral_constant<int, Type>{}, d, vp);

            return mfx::dx11::mock_component(std::integral_constant<int, Type>{}, d
                , std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main10>{}, vp)
            );
        }

        template <int Type>
        inline
        mocks::dx11::device&
        mock_encoder(gen11, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
        {
            mock_encoder(gen10{}, std::integral_constant<int, Type>{}, d, vp);

            return mfx::dx11::mock_component(std::integral_constant<int, Type>{}, d
                , std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main422>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main422_10>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main444>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main444_10>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main10>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main422>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main422_10>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main444>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_HEVC_Main444_10>{}, vp)
            );
        }

        template <int Type>
        inline
        mocks::dx11::device&
        mock_encoder(gen12, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
        {
            mock_encoder(gen11{}, std::integral_constant<int, Type>{}, d, vp);

            return mfx::dx11::mock_component(std::integral_constant<int, Type>{}, d
                , std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main12>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main422_12>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main444_12>{}, vp)
    #if defined(MFX_ENABLE_HEVCE_SCC)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main10>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444_10>{}, vp)
    #endif
            );
        };
    } //detail

    template <int Type>
    inline
    mocks::dx11::device&
    mock_encoder(std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
    {
        detail::mock_encoder(gen_of<Type>::type{}, std::integral_constant<int, Type>{},
            d, vp
        );

        return mfx::dx11::mock_component(std::integral_constant<int, Type>{}, d,
            mfx::dx11::make_texture_desc(vp, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER, 0/*no CPU access*/),
            mfx::dx11::make_texture_desc(vp, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ),

            //'CopySysToRaw' - input (in video memory) texture
            mfx::dx11::make_texture_desc(vp, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER, 0/*no CPU access*/),
            //'CopySysToRaw' - input (in video memory) writable staging texture
            mfx::dx11::make_texture_desc(vp, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_WRITE),

            //'AllocRec' - reconstruction texture
            detail::make_rec_desc(std::integral_constant<int, Type>{}, vp, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER, 0/*no CPU access*/),
            //'AllocRec' - reconstruction staging texture
            detail::make_rec_desc(std::integral_constant<int, Type>{}, vp, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ),

            //mbqp texture
            detail::make_mbqp_desc(std::integral_constant<int, Type>{}, vp, D3D11_USAGE_DEFAULT, 0, 0),
            //mbqp staging texture
            detail::make_mbqp_desc(std::integral_constant<int, Type>{}, vp, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ),

            //compressed bitstream texture
            detail::make_bs_desc(std::integral_constant<int, Type>{}, vp, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER, 0),
            //compressed bitstream staging texture
            detail::make_bs_desc(std::integral_constant<int, Type>{}, vp, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ)
        );
    }

    inline
    mocks::dx11::device&
    mock_encoder(int type, mocks::dx11::device& d, mfxVideoParam const& vp)
    {
        return invoke(type,
            MOCKS_LIFT(mock_encoder),
            d, vp
        );
    }

    namespace detail
    {
        struct encoder : mfx::dx11::encoder
        {
            std::mutex                                     guard;
            std::vector<char>                              buffer;

            std::deque<std::pair<UINT, ID3D11Resource*> >  feedbacks;
            std::map<ID3D11Resource*, std::vector<char> >  bitstream;

            encoder(int type, IDXGIAdapter* adapter, mfxVideoParam const& vp)
                : mfx::dx11::encoder(type, adapter, vp)
            {
                //CreateAuxilliaryDevice uses this dimension as default
                auto vp_def = vp;
                vp_def.mfx.FrameInfo.Width  = 1920;
                vp_def.mfx.FrameInfo.Height = 1088;
                h265::dx11::mock_encoder(
                    type, *device, vp_def
                );

                //need DX11 device mimics DXGI device
                //dx11::device will hold reference to DXGI device itself
                CComPtr<IDXGIDevice> d;
                d.Attach(mocks::dxgi::make_device().release());
                mock_device(*device, d.p);

                reset(type, vp);
            }

            void reset(int type, mfxVideoParam const& vp) override
            {
                assert(device);
                assert(context);

                //TODO: calc proper texture size for input video
                buffer.resize(
                    vp.mfx.FrameInfo.Width * vp.mfx.FrameInfo.Height * 4
                );

                mock_context(*context,
                    //'CopySysToRaw -> [ID3D11DeviceContext :: Map]
                    std::make_tuple(D3D11_MAP_WRITE, buffer.data(), vp.mfx.FrameInfo.Width),

                    //'CreateAuxilliaryDevice' queries this decoder's extension
                    std::make_tuple(
                        D3D11_VIDEO_DECODER_EXTENSION{ ENCODE_QUERY_ACCEL_CAPS_ID },
                        [type, vp](D3D11_VIDEO_DECODER_EXTENSION const* de)
                        {
                            assert(de->pPrivateOutputData);
                            assert(de->PrivateOutputDataSize == sizeof(ENCODE_CAPS_HEVC));

                            auto c = mocks::mfx::h265::caps(type, vp);
                            *reinterpret_cast<ENCODE_CAPS_HEVC*>(de->pPrivateOutputData) = c;

                            return S_OK;
                        }
                    ),

                    //'InitAlloc -> QueryCompBufferInfo' queries this decoder's extension
                    std::make_tuple(
                        D3D11_VIDEO_DECODER_EXTENSION{ ENCODE_FORMAT_COUNT_ID },
                        [vp](D3D11_VIDEO_DECODER_EXTENSION const* de)
                        {
                            assert(de->pPrivateOutputData);
                            assert(de->PrivateOutputDataSize == sizeof(ENCODE_FORMAT_COUNT));

                            auto count = reinterpret_cast<ENCODE_FORMAT_COUNT*>(de->pPrivateOutputData);
                            count->CompressedBufferInfoCount = std::extent<decltype(comp_buffers)>::value;
                            count->UncompressedFormatCount   = 1;

                            return S_OK;
                        }
                    ),

                    //'InitAlloc -> QueryCompBufferInfo' queries this decoder's extension
                    std::make_tuple(
                        D3D11_VIDEO_DECODER_EXTENSION{ ENCODE_FORMATS_ID },
                        [fi = vp.mfx.FrameInfo](D3D11_VIDEO_DECODER_EXTENSION const* de)
                        {
                            assert(de->pPrivateOutputData);
                            assert(de->PrivateOutputDataSize);

                            auto formats = reinterpret_cast<ENCODE_FORMATS*>(de->pPrivateOutputData);
                            auto info    = reinterpret_cast<ENCODE_COMP_BUFFER_INFO*>(formats->pCompressedBufferInfo);
                            auto count   = std::min(
                                std::extent<decltype(comp_buffers)>::value,
                                formats->CompressedBufferInfoSize / sizeof(ENCODE_COMP_BUFFER_INFO)
                            );
                            assert(count);

                            std::transform(comp_buffers, comp_buffers + count, info,
                                [fi](ENCODE_COMP_BUFFER_INFO const& bi)
                                {
                                    ENCODE_COMP_BUFFER_INFO res{ bi };
                                    switch (D3D11_DDI_VIDEO_ENCODER_BUFFER_TYPE(bi.Type))
                                    {
                                        case D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA:
                                            res.CreationWidth  = fi.Width;
                                            res.CreationHeight = fi.Height;
                                            break;

                                        case D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA:
                                            //UMD allocates buffer in macroblocks (not CUs)
                                            res.CreationWidth  = ::mfx::CeilDiv<USHORT>(fi.Width,  16);
                                            res.CreationHeight = ::mfx::CeilDiv<USHORT>(fi.Height, 16);
                                            break;
                                    }

                                    return res;
                                }
                            );

                            return S_OK;
                        }
                    ),

                    //QueryDDI MbPerSec::Query1WithCaps queries this decoder's extension
                    std::make_tuple(
                        D3D11_VIDEO_DECODER_EXTENSION{ ENCODE_QUERY_MAX_MB_PER_SEC_ID },
                        [vp](D3D11_VIDEO_DECODER_EXTENSION const* de)
                        {
                            assert(de->pPrivateOutputData);
                            assert(de->PrivateOutputDataSize == sizeof(mfxU32[16]));

                            auto mb_per_sec = reinterpret_cast<char*>(de->pPrivateOutputData);
                            std::fill(mb_per_sec, mb_per_sec + de->PrivateOutputDataSize, char(0));

                            return S_OK;
                        }
                    ),

                    //'Runtime' invokes this decoder's extension
                    std::make_tuple(
                        D3D11_VIDEO_DECODER_EXTENSION{ ENCODE_ENC_PAK_ID },
                        [&](D3D11_VIDEO_DECODER_EXTENSION const* de)
                        {
                            assert(de->pPrivateInputData);
                            assert(de->PrivateInputDataSize == sizeof(ENCODE_EXECUTE_PARAMS));

                            auto params = reinterpret_cast<ENCODE_EXECUTE_PARAMS const*>(de->pPrivateInputData);
                            auto i = std::find_if(params->pCompressedBuffers, params->pCompressedBuffers + params->NumCompBuffers,
                                [](ENCODE_COMPBUFFERDESC const& bd)
                                { return bd.CompressedBufferType == D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA; }
                            );
                            if (i == params->pCompressedBuffers + params->NumCompBuffers)
                                return E_FAIL;

                            auto id = *reinterpret_cast<mfxU32 const*>((*i).pCompBuffer);
                            if (!(id < de->ResourceCount))
                                return E_FAIL;

                            i = std::find_if(params->pCompressedBuffers, params->pCompressedBuffers + params->NumCompBuffers,
                                [](ENCODE_COMPBUFFERDESC const& bd)
                                { return bd.CompressedBufferType == D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA; }
                            );
                            if (i == params->pCompressedBuffers + params->NumCompBuffers)
                                return E_FAIL;

                            auto pps = reinterpret_cast<ENCODE_SET_PICTURE_PARAMETERS_HEVC const*>((*i).pCompBuffer);
                            if (!pps)
                                return E_FAIL;

                            std::vector<ENCODE_COMPBUFFERDESC> headers;
                            std::copy_if(params->pCompressedBuffers, params->pCompressedBuffers + params->NumCompBuffers, std::back_inserter(headers),
                                [](ENCODE_COMPBUFFERDESC const& bd)
                                { return bd.CompressedBufferType == D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA; }
                            );
                            std::copy_if(params->pCompressedBuffers, params->pCompressedBuffers + params->NumCompBuffers, std::back_inserter(headers),
                                [](ENCODE_COMPBUFFERDESC const& bd)
                                { return bd.CompressedBufferType == D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA; }
                            );

                            //calculate desired buffer size
                            auto size = std::accumulate(std::begin(headers), std::end(headers), size_t(0),
                                [](size_t size, ENCODE_COMPBUFFERDESC const& bd)
                                {
                                    auto ph = reinterpret_cast<ENCODE_PACKEDHEADER_DATA const*>(
                                        reinterpret_cast<char const*>(bd.pCompBuffer) + bd.DataOffset
                                    );

                                    return size + ph->BufferSize;
                                }
                            );

                            auto bs = de->ppResourceList[id];

                            std::unique_lock<std::mutex> l(guard);
                            bitstream[bs].resize(size + 1);

                            auto dst = bitstream[bs].data();
                            std::for_each(std::begin(headers), std::end(headers),
                                [&dst](ENCODE_COMPBUFFERDESC const& bd)
                                {
                                    auto ph = reinterpret_cast<ENCODE_PACKEDHEADER_DATA const*>(
                                        reinterpret_cast<char const*>(bd.pCompBuffer) + bd.DataOffset
                                    );

                                    auto src = reinterpret_cast<char const*>(ph->pData) + ph->DataOffset;
                                    //ph->DataLength is bytes for all headers excepting SH, use ph->BufferSize instead
                                    std::copy(src, src + ph->BufferSize, dst);
                                    dst += ph->BufferSize;
                                }
                            );

                            bitstream[bs][size] = 0;
                            feedbacks.push_back(
                                std::make_pair(pps->StatusReportFeedbackNumber, bs)
                            );

                            return S_OK;
                        }
                    ),

    #if defined (MFX_ENABLE_HEVC_CUSTOM_QMATRIX)
                    //'QMatrix -> UpdateDDISubmit' queries this buffer through this decoder's extension
                    D3D11_VIDEO_DECODER_EXTENSION{ D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA },
    #endif

    #ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
                    //'BlockingSync -> InitAlloc' queries this decoder's extension
                    std::make_tuple(
                        D3D11_VIDEO_DECODER_EXTENSION{ DXVA2_SET_GPU_TASK_EVENT_HANDLE },
                        [](D3D11_VIDEO_DECODER_EXTENSION const* de)
                        {
                            //TODO: this struct is defined at [mfx_common.h] but this header brings
                            //a lot of other definitions that conflicts w/ MSDK TS ones
                            struct event
                            {
                                uint8_t component; //GPU_COMPONENT_ID
                                HANDLE  handle;
                            };;

                            assert(de->pPrivateInputData);
                            assert(de->PrivateInputDataSize == sizeof(event));

                            auto e = reinterpret_cast<event const*>(de->pPrivateInputData);
                            if (e->component != GPU_COMPONENT_ENCODE)
                                return E_FAIL;

                            return
                                SetEvent(e->handle) ? S_OK : E_FAIL;
                        }
                    ),
    #endif //MFX_ENABLE_HW_BLOCKING_TASK_SYNC

                    //'SyncOperation' requires this decoder's extension
                    std::make_tuple(
                        D3D11_VIDEO_DECODER_EXTENSION{ ENCODE_QUERY_STATUS_ID },
                        [&](D3D11_VIDEO_DECODER_EXTENSION const* de)
                        {
                            assert(de->pPrivateInputData);
                            assert(de->PrivateInputDataSize == sizeof(ENCODE_QUERY_STATUS_PARAMS_DESCR));

                            auto desc = reinterpret_cast<ENCODE_QUERY_STATUS_PARAMS_DESCR const*>(de->pPrivateInputData);
                            if (desc->StatusParamType != QUERY_STATUS_PARAM_FRAME)
                                return S_OK;

                            assert(de->pPrivateOutputData);
                            assert(!(de->PrivateOutputDataSize < sizeof(ENCODE_QUERY_STATUS_PARAMS)));

                            std::unique_lock<std::mutex> l(guard);

                            auto status = reinterpret_cast<ENCODE_QUERY_STATUS_PARAMS*>(de->pPrivateOutputData);
                            auto end = status + de->PrivateOutputDataSize / desc->SizeOfStatusParamStruct;
                            for (; !feedbacks.empty() && status != end; ++status)
                            {
                                auto feedback = feedbacks.front();
                                status->StatusReportFeedbackNumber = feedback.first;
                                status->bitstreamSize = UINT(bitstream[feedback.second].size());
                                feedbacks.pop_front();
                            }

                            return S_OK;
                        }
                    ),

                    //'CopyBS -> [ID3D11DeviceContext :: Map]
                    std::make_tuple(D3D11_MAP_READ,
                        [&](ID3D11Resource* resource, D3D11_MAP, UINT /*flags*/)
                        {
                            std::tuple<void*, size_t> result{};
                            std::unique_lock<std::mutex> l(guard);

                            auto bs = bitstream.find(resource);
                            if (bs != std::end(bitstream))
                                result = std::make_tuple((*bs).second.data(), vp.mfx.FrameInfo.Width);
                            else
                            {
                                D3D11_RESOURCE_DIMENSION type;
                                resource->GetType(&type);
                                if (type != D3D11_RESOURCE_DIMENSION_BUFFER)
                                    return result;

                                CComPtr<ID3D11Buffer> buffer;
                                if (FAILED(resource->QueryInterface(&buffer)))
                                    return result;

                                D3D11_BUFFER_DESC bd{};
                                buffer->GetDesc(&bd);
                                auto r = bitstream.emplace(resource, bd.ByteWidth);
                                result = std::make_tuple((*r.first).second.data(), bd.ByteWidth);
                            }

                            return result;
                        }
                    )
                );

                //update device & context w/ new [mfxVideoParam]
                mock_device(*device, context.p);
                h265::dx11::mock_encoder(
                    type, *device, vp
                );
            }
        };
    } //detail

    inline
    std::unique_ptr<mfx::dx11::encoder>
    make_encoder(int type, IDXGIAdapter* adapter, mfxVideoParam const& vp)
    { return std::unique_ptr<mfx::dx11::encoder>(new detail::encoder(type, adapter, vp)); }

} } } }

