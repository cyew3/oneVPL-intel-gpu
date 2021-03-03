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

#include "mocks/include/dx11/resource/buffer.h"
#include "mocks/include/dx11/resource/texture.h"

#include "mocks/include/dx11/device/base.h"

namespace mocks { namespace dx11
{
    namespace detail
    {
        inline
        void mock_device(device& d, D3D11_TEXTURE2D_DESC td)
        {
            //HRESULT CreateTexture2D(const D3D11_TEXTURE2D_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture2D**));
            EXPECT_CALL(d, CreateTexture2D(
                testing::AllOf(
                    testing::NotNull(),
                    testing::Truly([td](D3D11_TEXTURE2D_DESC const* xd)
                    { return equal(*xd, td); })
                ), testing::_, testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 2>(testing::Invoke(
                    [](D3D11_TEXTURE2D_DESC const* xd, ID3D11Texture2D** t)
                    { *t = make_texture(*xd).release(); return S_OK; }
                )));
        }

        inline
        void mock_device(device& d, std::reference_wrapper<D3D11_TEXTURE2D_DESC> td)
        { return mock_device(d, td.get()); }

        inline
        void mock_device(device& d, D3D11_BUFFER_DESC bd)
        {
            //HRESULT CreateBuffer(const D3D11_BUFFER_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Buffer**)
            EXPECT_CALL(d, CreateBuffer(testing::AllOf(
                testing::NotNull(),
                testing::Truly([bd](D3D11_BUFFER_DESC const* xd)
                { return equal(*xd, bd); })
            ), testing::_, testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 2>(testing::Invoke(
                    [](D3D11_BUFFER_DESC const* xd, ID3D11Buffer** b)
                    { *b = make_buffer(*xd).release(); return S_OK; }
                )));
        }

        inline
        void mock_device(device& d, std::reference_wrapper<D3D11_BUFFER_DESC> bd)
        { return mock_device(d, bd.get()); }
    }

} }
