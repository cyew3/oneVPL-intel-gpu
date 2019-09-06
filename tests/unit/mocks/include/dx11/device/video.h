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

#include "mocks/include/dx11/device/base.h"

#include "mocks/include/dx11/decoder/decoder.h"
#include "mocks/include/dx11/decoder/view.h"

namespace mocks { namespace dx11
{
    namespace detail
    {
        inline
        void mock_device(device& d, std::tuple<D3D11_VIDEO_DECODER_DESC, D3D11_VIDEO_DECODER_CONFIG> params)
        {
            auto const& vd = std::get<0>(params);
            auto const& vc = std::get<1>(params);

            d.profiles.push_back(vd);
            //UINT GetVideoDecoderProfileCount()
            EXPECT_CALL(d, GetVideoDecoderProfileCount())
                .WillRepeatedly(testing::Invoke(
                    [&d]() { return UINT(d.profiles.size()); }
                ));

            //HRESULT GetVideoDecoderProfile(UINT, GUID*)
            EXPECT_CALL(d, GetVideoDecoderProfile(testing::Truly(
                [&d](UINT p) { return p < UINT(d.profiles.size()); }
                ), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                    [&d](UINT idx, GUID* id)
                    { *id = d.profiles[idx].Guid; return S_OK; }))
                );

            auto MatchVD = testing::Truly([vd](D3D11_VIDEO_DECODER_DESC const* xd)
            {
                    return
                           xd->Guid         == vd.Guid
                        && xd->OutputFormat == vd.OutputFormat
                        && xd->SampleWidth  <= vd.SampleWidth
                        && xd->SampleHeight <= vd.SampleHeight
                        ;
            });

            //HRESULT CreateVideoDecoder(const D3D11_VIDEO_DECODER_DESC*, const D3D11_VIDEO_DECODER_CONFIG*, ID3D11VideoDecoder**)
            EXPECT_CALL(d, CreateVideoDecoder(testing::AllOf(
                    testing::NotNull(), MatchVD
                ), testing::AllOf(
                    testing::NotNull(),
                    testing::Truly([vc](D3D11_VIDEO_DECODER_CONFIG const* xc) { return equal(*xc, vc); })
                ), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 1, 2>(testing::Invoke(
                    [](D3D11_VIDEO_DECODER_DESC const* vd, D3D11_VIDEO_DECODER_CONFIG const* vc, ID3D11VideoDecoder** d)
                    { *d = make_decoder(*vd, *vc).release(); return S_OK; }
                )));

            //HRESULT CheckVideoDecoderFormat(const GUID*, DXGI_FORMAT, BOOL*)
            EXPECT_CALL(d, CheckVideoDecoderFormat(testing::AllOf(
                    testing::NotNull(), testing::Pointee(testing::Eq(vd.Guid))
                ), testing::Eq(vd.OutputFormat), testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<2>(TRUE),
                    testing::Return(S_OK)
                ));

            d.configs[vd.Guid].push_back(vc);
            auto& configs = d.configs[vd.Guid];

            //HRESULT GetVideoDecoderConfigCount(const D3D11_VIDEO_DECODER_DESC*, UINT*)
            EXPECT_CALL(d, GetVideoDecoderConfigCount(testing::AllOf(
                    testing::NotNull(), MatchVD
                ), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1>(testing::Invoke(
                    [&configs](UINT* count)
                    { *count = UINT(configs.size()); return S_OK; }
                )));

            //HRESULT GetVideoDecoderConfig(const D3D11_VIDEO_DECODER_DESC*, UINT, D3D11_VIDEO_DECODER_CONFIG*)
            EXPECT_CALL(d, GetVideoDecoderConfig(testing::AllOf(
                    testing::NotNull(), MatchVD
                ), testing::Truly([&configs](UINT idx) { return idx < configs.size(); }),
                    testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
                    [&configs](UINT idx, D3D11_VIDEO_DECODER_CONFIG* xc)
                    { *xc = configs[idx]; return S_OK; }
                )));
        }

        template <typename ...Configs>
        inline
        typename std::enable_if<
            std::conjunction<std::is_same<Configs, D3D11_VIDEO_DECODER_CONFIG>...>::value
        >::type
        mock_device(device& d, std::tuple<D3D11_VIDEO_DECODER_DESC, std::tuple<Configs...> > params)
        {
            for_each_tuple(
                [&d, vd = std::get<0>(params)](auto p)
                { mock_device(d, std::make_tuple(vd, p)); },
                std::get<1>(params)
            );
        }

        inline
        void mock_device(device& d, D3D11_VIDEO_DECODER_DESC vd)
        {
            return mock_device(d,
                std::make_tuple(vd, D3D11_VIDEO_DECODER_CONFIG{})
            );
        }

        inline
        void mock_device(device& d, D3D11_VIDEO_DECODER_CONFIG vc)
        {
            return mock_device(d,
                std::make_tuple(D3D11_VIDEO_DECODER_DESC{}, vc)
            );
        }

        inline
        void mock_device(device& d, D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC od)
        {
            //HRESULT CreateVideoDecoderOutputView(ID3D11Resource*, const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC*, ID3D11VideoDecoderOutputView**)
            EXPECT_CALL(d, CreateVideoDecoderOutputView(testing::NotNull(), testing::AllOf(
                    testing::NotNull(),
                    testing::Truly([od](D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC const* xd) { return equal(*xd, od); })
                ), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
                    [](D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC const* od, ID3D11VideoDecoderOutputView** v)
                    { *v = make_view(*od).release(); return S_OK; }
                )));
        }
    }
} }
