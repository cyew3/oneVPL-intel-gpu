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

#include "mocks/include/dxgi/resource/resource.h"
#include "mocks/include/dx11/winapi/texture.hxx"

namespace mocks { namespace dx11
{
    struct texture
        : winapi::texture_impl
    {
        MOCK_METHOD1(GetDesc, void(D3D11_TEXTURE2D_DESC*));
    };

    namespace detail
    {
        inline
        void mock_texture(texture& t, dxgi::resource& resource)
        {
            mock_composite(resource, &t);
            mock_unknown(t, &resource,
                __uuidof(IDXGIResource),
                __uuidof(IDXGIResource1)
            );
        }

        inline
        void mock_texture(texture& t, D3D11_TEXTURE2D_DESC td)
        {
            //void GetDesc(D3D11_TEXTURE2D_DESC*)
            EXPECT_CALL(t, GetDesc(testing::NotNull()))
                .WillRepeatedly(testing::SetArgPointee<0>(td))
                ;
        }

        template <typename T>
        inline
        void mock_texture(texture&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_texture]"
            );
        }
    }

    template <typename ...Args>
    inline
    texture& mock_texture(texture& t, Args&&... args)
    {
        for_each(
            [&t](auto&& x) { detail::mock_texture(t, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return t;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<texture>
    make_texture(Args&&... args)
    {
        using testing::_;

        auto t = std::unique_ptr<testing::NiceMock<texture> >{
            new testing::NiceMock<texture>{}
        };

        mock_texture(*t, std::forward<Args>(args)...);

        return t;
    }

} }
