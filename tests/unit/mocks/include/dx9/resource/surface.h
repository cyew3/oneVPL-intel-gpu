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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <memory>
#include <vector>

#include "mocks/include/dx9/winapi/surface.hxx"

#include "utils/align.h"

namespace mocks { namespace dx9
{
    struct surface
        : winapi::surface_impl
    {
        MOCK_METHOD0(GetType, D3DRESOURCETYPE());
        MOCK_METHOD1(GetDesc, HRESULT(D3DSURFACE_DESC*));
        MOCK_METHOD3(LockRect, HRESULT(D3DLOCKED_RECT*, CONST RECT*, DWORD));
    };

    namespace detail
    {
        inline
        void mock_surface(surface& s, D3DSURFACE_DESC sd)
        {
            using testing::_;

            EXPECT_CALL(s, GetDesc(testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(sd),
                    testing::Return(S_OK))
                );

            EXPECT_CALL(s, LockRect(testing::NotNull(), _, testing::Truly(
                [](DWORD flags)
                {
                    return
                        !(flags & ~(D3DLOCK_READONLY | D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE | D3DLOCK_NOSYSLOCK | D3DLOCK_DONOTWAIT | D3DLOCK_NO_DIRTY_UPDATE));
                })))
                .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                    [width = sd.Width](D3DLOCKED_RECT* rect)
                    {
                        rect->Pitch = utils::align2(width, 128);

                        int dummy = 0;
                        rect->pBits = &dummy;

                        return S_OK;
                    }))
                );
        }

        template <typename T>
        inline
        void mock_surface(surface&, T)
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

        EXPECT_CALL(*s, GetType())
            .WillRepeatedly(testing::Return(D3DRTYPE_SURFACE));

        EXPECT_CALL(*s, GetDesc(_))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        EXPECT_CALL(*s, LockRect(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        mock_surface(*s, std::forward<Args>(args)...);

        return s;
    }

} }
