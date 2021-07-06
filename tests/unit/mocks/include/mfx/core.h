// Copyright (c) 2020 Intel Corporation
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
#include "mocks/include/mfx/detail/core.hxx"

namespace mocks { namespace mfx
{
    struct core : api::core
    {
        MOCK_METHOD2(GetHandle, mfxStatus(mfxHandleType, mfxHDL*));
        MOCK_METHOD2(SetHandle, mfxStatus(mfxHandleType, mfxHDL));
        MOCK_METHOD1(SetFrameAllocator, mfxStatus(mfxFrameAllocator*));

        MOCK_METHOD0(GetHWType, eMFXHWType());
        MOCK_METHOD1(SetCoreId, bool(mfxU32));
        MOCK_CONST_METHOD0(GetVAType, eMFXVAType());
        MOCK_METHOD1(QueryCoreInterface, void*(const MFX_GUID&));

        MOCK_METHOD3(GetFrameHDL, mfxStatus(mfxMemId, mfxHDL*, bool));

        MOCK_METHOD3(AllocFrames, mfxStatus(mfxFrameAllocRequest*, mfxFrameAllocResponse*, bool));
        MOCK_METHOD2(LockFrame, mfxStatus(mfxMemId, mfxFrameData*));
        MOCK_METHOD2(UnlockFrame, mfxStatus(mfxMemId, mfxFrameData*));
        MOCK_METHOD2(FreeFrames, mfxStatus(mfxFrameAllocResponse*, bool));
    };

    namespace detail
    {
        //Need for ADL
        struct core_tag {};

        inline
        void mock_core(core_tag, core& c, std::tuple<mfxHandleType, mfxHDL> params)
        {
            auto const type   = std::get<0>(params);
            auto const handle = std::get<1>(params);

            EXPECT_CALL(c, SetHandle(testing::Eq(type), testing::Eq(handle)))
                .WillRepeatedly(testing::Return(MFX_ERR_NONE));
            EXPECT_CALL(c, GetHandle(testing::Eq(type), testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<1>(handle),
                    testing::Return(MFX_ERR_NONE)
                ));
        }

        inline
        void mock_core(core_tag, core& c, mfxFrameAllocator* alloc)
        {
            EXPECT_CALL(c, SetFrameAllocator(testing::Eq(alloc)))
                .WillRepeatedly(testing::Return(MFX_ERR_NONE));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_pointer<T>::value
        >::type
        mock_core(core_tag, core& c, std::tuple<MFX_GUID, T> params)
        {
            auto const guid = std::get<0>(params);
            auto const ptr  = std::get<1>(params);

            EXPECT_CALL(c, QueryCoreInterface(testing::Eq(guid)))
                .WillRepeatedly(testing::Return(ptr));
        }

        inline
        void mock_core(core_tag, core& c, eMFXHWType type)
        {
            EXPECT_CALL(c, GetHWType())
                .WillRepeatedly(testing::Return(type));
        }

        inline
        void mock_core(core_tag, core& c, eMFXVAType type)
        {
            EXPECT_CALL(c, GetVAType())
                .WillRepeatedly(testing::Return(type));
        }

        template <typename Matcher>
        inline
        typename std::enable_if<
            is_matcher<Matcher>::value
        >::type
        mock_core(core_tag, core& c, std::tuple<Matcher, bool> params)
        {
            EXPECT_CALL(c, SetCoreId(std::get<0>(params)))
                .WillRepeatedly(testing::Return(std::get<1>(params)));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_core(core_tag, core& c, std::tuple<T, bool> params)
        {
            return mock_core(core_tag{}, c,
                std::make_tuple(testing::Eq(std::get<0>(params)), std::get<1>(params))
            );
        }

        inline
        void mock_core(core_tag, core& c, std::tuple<mfxMemId, mfxHDL, bool> params)
        {
            auto const mid      = std::get<0>(params);
            auto const hdl      = std::get<1>(params);
            auto const extended = std::get<2>(params);

            EXPECT_CALL(c, GetFrameHDL(
                testing::Eq(mid),
                testing::NotNull(),
                testing::Eq(extended)))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<1>(hdl),
                    testing::Return(MFX_ERR_NONE)
                ));
        }

        inline
        void mock_core(core_tag, core& c, std::tuple<mfxMemId, mfxHDL> params)
        {
            return mock_core(core_tag{}, c,
                std::tuple_cat(params, std::make_tuple(true))
            );
        }

        inline
        void mock_core(core_tag, core& c, std::tuple<mfxFrameAllocRequest, mfxFrameAllocResponse, bool> params)
        {
            auto const request  = std::get<0>(params);
            auto const response = std::get<1>(params);
            auto const copy     = std::get<2>(params);

            EXPECT_CALL(c, AllocFrames(
                testing::AllOf(
                    testing::NotNull(),
                    testing::Truly([request](mfxFrameAllocRequest* xr) { return equal(request, *xr); })
                ),
                testing::NotNull(),
                testing::Eq(copy)))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<1>(response),
                    testing::Return(MFX_ERR_NONE)
                ));
        }

        inline
        void mock_core(core_tag, core& c, std::tuple<mfxFrameAllocRequest, mfxFrameAllocResponse> params)
        {
            return mock_core(core_tag{}, c,
                std::tuple_cat(params, std::make_tuple(true))
            );
        }

        template <typename T>
        inline
        void mock_core(core_tag, core&, T)
        {
            /* If you trap here it means that you passed an argument to [make_core]
                which is not supported by library, you need to overload [make_core] for that argument's type */
            static_assert(
                std::conjunction<std::false_type, std::bool_constant<sizeof(T) != 0> >::value,
                "Unknown argument was passed to [make_core]"
            );
        }

        template <typename ...Args>
        inline
        void dispatch(core& c, Args&&... args)
        {
            for_each(
                [&c](auto&& x) { mock_core(core_tag{}, c, std::forward<decltype(x)>(x)); },
                std::forward<Args>(args)...
            );
        }
    }

    template <typename ...Args>
    inline
    core& mock_core(core& c, Args&&... args)
    {
        detail::dispatch(c, std::forward<Args>(args)...);

        return c;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<core>
    make_core(Args&&... args)
    {
        auto c = std::unique_ptr<testing::NiceMock<core> >{
            new testing::NiceMock<core>{}
        };

        using testing::_;

        EXPECT_CALL(*c, GetHandle(_, _))
            .WillRepeatedly(testing::Return(MFX_ERR_NOT_FOUND));
        EXPECT_CALL(*c, GetHandle(_, testing::IsNull()))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*c, SetHandle(_, _))
            .WillRepeatedly(testing::Return(MFX_ERR_UNDEFINED_BEHAVIOR));
        EXPECT_CALL(*c, SetHandle(_, testing::IsNull()))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*c, SetFrameAllocator(_))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));
        EXPECT_CALL(*c, SetFrameAllocator(testing::IsNull()))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*c, GetHWType())
            .WillRepeatedly(testing::Return(MFX_HW_UNKNOWN));

        EXPECT_CALL(*c, SetCoreId(_))
            .WillRepeatedly(testing::Return(false));
        EXPECT_CALL(*c, QueryCoreInterface(_))
            .WillRepeatedly(testing::Return(nullptr));

        EXPECT_CALL(*c, GetFrameHDL(_, _, _))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));
        EXPECT_CALL(*c, GetFrameHDL(testing::IsNull(), _, _))
            .WillRepeatedly(testing::Return(MFX_ERR_INVALID_HANDLE));
        EXPECT_CALL(*c, GetFrameHDL(_, testing::IsNull(), _))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*c, AllocFrames(_, _, _))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));
        EXPECT_CALL(*c, AllocFrames(_, _, _))
            .With(testing::Args<0, 1>(testing::Truly(
                [](std::tuple<mfxFrameAllocRequest*, mfxFrameAllocResponse*> p)
                { return !std::get<0>(p) || !std::get<1>(p); }
            )))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*c, LockFrame(_, _))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));
        EXPECT_CALL(*c, LockFrame(testing::IsNull(), _))
            .WillRepeatedly(testing::Return(MFX_ERR_INVALID_HANDLE));
        EXPECT_CALL(*c, LockFrame(_, testing::IsNull()))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*c, UnlockFrame(_, _))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));
        EXPECT_CALL(*c, UnlockFrame(testing::IsNull(), _))
            .WillRepeatedly(testing::Return(MFX_ERR_INVALID_HANDLE));
        EXPECT_CALL(*c, UnlockFrame(_, testing::IsNull()))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*c, FreeFrames(_, _))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));

        mock_core(*c, std::forward<Args>(args)...);

        return c;
    }

} }
