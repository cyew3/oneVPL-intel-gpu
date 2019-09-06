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

#include "mocks/include/dxgi/winapi/surface.hxx"

namespace mocks { namespace dxgi
{
    struct surface
        : winapi::surface_impl
    {
        MOCK_METHOD1(GetDesc, HRESULT (DXGI_SURFACE_DESC*));
        MOCK_METHOD2(Map, HRESULT(DXGI_MAPPED_RECT*, UINT));
        MOCK_METHOD0(Unmap, HRESULT());
    };

    namespace detail
    {
        /* Mocks surface from given DXGI_SURFACE_DESC, provided functor
           tuple(pitch, pointer) = F(UINT flags) is invoked
           and its results are used as (DXGI_MAPPED_RECT) */
        template <typename F>
        inline
        void mock_surface(surface& s, std::tuple<DXGI_SURFACE_DESC, F> params)
        {
            EXPECT_CALL(s, GetDesc(testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(std::get<0>(params)),
                    testing::Return(S_OK)
                ));

            //HRESULT Map(DXGI_MAPPED_RECT*, UINT)
            EXPECT_CALL(s, Map(testing::NotNull(), testing::Truly(
                [](DWORD flags)
                {
                    return
                        !(flags & ~(DXGI_MAP_READ | DXGI_MAP_WRITE | DXGI_MAP_DISCARD));
                })))
                .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                    [params](DXGI_MAPPED_RECT* m, UINT flags)
                    {
                        auto r = std::get<1>(params)(flags);
                        m->Pitch = static_cast<INT>(std::get<0>(r));
                        m->pBits = reinterpret_cast<BYTE*>(std::get<1>(r));
                        return S_OK;
                    }
                )));
        }

        /* Mocks buffer from given (DXGI_SURFACE_DESC, pitch, pointer) */
        template <typename T>
        inline
        typename std::enable_if<std::is_integral<T>::value>::type
        mock_surface(surface& f, std::tuple<DXGI_SURFACE_DESC, T, void*> params)
        {
            return mock_surface(f, std::make_tuple(std::get<0>(params),
                [params](UINT) { return std::make_tuple(std::get<1>(params), std::get<2>(params)); })
            );
        }

        inline
        void mock_surface(surface& f, DXGI_SURFACE_DESC sd)
        {
            return mock_surface(f, std::make_tuple(sd, 
                [](UINT) { return std::make_tuple(0, nullptr); })
            );
        }

        template <typename T>
        inline
        typename std::enable_if<!std::is_integral<T>::value>::type
        mock_surface(surface&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_surface]"
            );
        }
    }

    template <typename ...Args>
    inline
    surface& mock_surface(surface& s, Args&&... args)
    {
        for_each(
            [&s](auto&& x) { detail::mock_surface(s, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return s;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<surface>
    make_surface(Args&&... args)
    {
        auto s = std::unique_ptr<testing::NiceMock<surface> >{
            new testing::NiceMock<surface>{}
        };

        using testing::_;

        EXPECT_CALL(*s, GetDesc(_))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT Map(DXGI_MAPPED_RECT*, UINT)
        EXPECT_CALL(*s, Map(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        EXPECT_CALL(*s, Unmap())
            .WillRepeatedly(testing::Return(S_OK));

        mock_surface(*s, std::forward<Args>(args)...);

        return s;
    }

    inline
    std::unique_ptr<surface>
    make_surface()
    {
        return
            make_surface(DXGI_SURFACE_DESC{});
    }

} }
