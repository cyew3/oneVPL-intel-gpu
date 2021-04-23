// Copyright (c) 2021 Intel Corporation
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
#include "mocks/include/mfx/common.h"
#include "mocks/include/mfx/detail/allocator.hxx"

#include <list>
#include <numeric>

namespace mocks { namespace mfx
{
    struct allocator
        : api::allocator
    {
        MOCK_METHOD2(Alloc,  mfxStatus(mfxFrameAllocRequest*, mfxFrameAllocResponse*));
        MOCK_METHOD2(Lock,   mfxStatus(mfxMemId, mfxFrameData*));
        MOCK_METHOD2(Unlock, mfxStatus(mfxMemId, mfxFrameData*));
        MOCK_METHOD2(GetHDL, mfxStatus(mfxMemId, mfxHDL* handle));
        MOCK_METHOD1(Free,   mfxStatus(mfxFrameAllocResponse*));

        std::list<std::unique_ptr<mfxMemId[]> > mids;
    };

    namespace detail
    {
        //Need for ADL
        struct allocator_tag {};

        void mock_allocator(allocator_tag, allocator& a, std::tuple<mfxMemId, mfxFrameData> params)
        {
            EXPECT_CALL(a, Lock(
                testing::Eq(std::get<0>(params)),
                testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<1>(std::get<1>(params)),
                    testing::Return(MFX_ERR_NONE)
                ));

            EXPECT_CALL(a, Unlock(
                testing::Eq(std::get<0>(params)),
                testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<1>(std::get<1>(params)),
                    testing::Return(MFX_ERR_NONE)
                ));
        }

        inline
        void mock_allocator(allocator_tag, allocator& a, std::tuple<mfxFrameAllocRequest, mfxFrameAllocResponse, mfxFrameData> params)
        {
            //mfxStatus Alloc(mfxFrameAllocRequest*, mfxFrameAllocResponse*)
            EXPECT_CALL(a, Alloc(
                testing::AllOf(
                    testing::NotNull(),
                    testing::Pointee(testing::Eq(std::get<0>(params)))
                ),
                testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<1>(std::get<1>(params)),
                    testing::Return(MFX_ERR_NONE)
                ));

            EXPECT_CALL(a, GetHDL(
                testing::AnyOfArray(std::get<1>(params).mids, std::get<1>(params).NumFrameActual),
                testing::NotNull()))
                .WillRepeatedly(testing::Invoke(
                    [](mfxMemId mid, mfxHDL* hdl)
                    {
                        *hdl = mid;
                        return MFX_ERR_NONE;
                    }
                ));

            EXPECT_CALL(a, Free(testing::AllOf(
                testing::NotNull(),
                //Do not use common's operator== for 'mfxFrameAllocResponse'
                //because values in the 'mids' array are allowed to be re-ordered
                testing::Truly([r = std::get<1>(params)](mfxFrameAllocResponse* xr)
                {
                    return
                            r.AllocId       == xr->AllocId
                        && r.NumFrameActual == xr->NumFrameActual
                        && r.MemType        == xr->MemType
                        && testing::Matches(testing::UnorderedElementsAreArray(
                            r.mids, r.mids + r.NumFrameActual
                        ))(std::make_tuple(xr->mids, xr->NumFrameActual))
                        ;
                }))))
                .WillRepeatedly(testing::Return(MFX_ERR_NONE));

            std::for_each(std::get<1>(params).mids, std::get<1>(params).mids + std::get<1>(params).NumFrameActual,
                [&a, fd = std::get<2>(params)](mfxMemId mid)
                { mock_allocator(allocator_tag{}, a, std::make_tuple(mid, fd)); }
            );
        }

        inline
        void mock_allocator(allocator_tag, allocator& a, std::tuple<mfxFrameAllocRequest, mfxFrameAllocResponse> params)
        {
            return mock_allocator(allocator_tag{}, a,
                std::tuple_cat(
                    params,
                    std::make_tuple(mfxFrameData{})
                )
            );
        }

        template <typename F>
        inline
        typename std::enable_if<
            std::is_convertible<decltype(std::declval<F>()()), mfxMemId>::value
        >::type mock_allocator(allocator_tag, allocator& a, std::tuple<mfxFrameAllocRequest, F, mfxFrameData> params)
        {
            auto const& request = std::get<0>(params);
            a.mids.emplace_back(
                new mfxMemId[request.NumFrameSuggested]
            );
            auto mids = a.mids.back().get();

            //Generate 'mids' w/ given functor 'F'
            std::generate(
                mids, mids + request.NumFrameSuggested,
                std::get<1>(params)
            );

            return mock_allocator(allocator_tag{}, a,
                std::make_tuple(std::get<0>(params),
                    mfxFrameAllocResponse{
                        request.AllocId,
                        {},
                        mids,
                        request.NumFrameSuggested,
                        request.Type
                    }
                )
            );
        }

        template <typename F>
        inline
        typename std::enable_if<
            std::is_convertible<decltype(std::declval<F>()()), mfxMemId>::value
        >::type mock_allocator(allocator_tag, allocator& a, std::tuple<mfxFrameAllocRequest, F> params)
        {
            return mock_allocator(allocator_tag{}, a,
                std::tuple_cat(
                    params,
                    std::make_tuple(mfxFrameData{})
                )
            );
        }

        inline
        void mock_allocator(allocator_tag, allocator& a, std::tuple<mfxFrameAllocRequest, mfxFrameData> params)
        {
            size_t mid = 0;
            return mock_allocator(allocator_tag{}, a,
                std::make_tuple(
                    std::get<0>(params),
                    [&mid]() { return mfxMemId(mid++); },
                    std::get<1>(params)
                )
            );
        }

        inline
        void mock_allocator(allocator_tag, allocator& a, mfxFrameAllocRequest request)
        {
            size_t mid = 0;
            return mock_allocator(allocator_tag{}, a,
                std::make_tuple(request, mfxFrameData{})
            );
        }

        template <typename T>
        inline
        void mock_allocator(allocator_tag, allocator&, T)
        {
            /* If you trap here it means that you passed an argument to [make_allocator]
                which is not supported by library, you need to overload [make_allocator] for that argument's type */
            static_assert(
                std::conjunction<std::false_type, std::bool_constant<sizeof(T) != 0> >::value,
                "Unknown argument was passed to [make_allocator]"
            );
        }

        template <typename ...Args>
        inline
        void dispatch(allocator& a, Args&&... args)
        {
            for_each(
                [&a](auto&& x) { mock_allocator(allocator_tag{}, a, std::forward<decltype(x)>(x)); },
                std::forward<Args>(args)...
            );
        }
    }

    template <typename ...Args>
    inline
    allocator& mock_allocator(allocator& a, Args&&... args)
    {
        detail::dispatch(a, std::forward<Args>(args)...);

        return a;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<allocator>
    make_allocator(Args&&... args)
    {
        auto a = std::unique_ptr<testing::NiceMock<allocator> >{
            new testing::NiceMock<allocator>{}
        };

        using testing::_;

        EXPECT_CALL(*a, Alloc(_, _))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));
        EXPECT_CALL(*a, Alloc(testing::IsNull(), _))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));
        EXPECT_CALL(*a, Alloc(_, testing::IsNull()))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*a, Lock(_, _))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));
        EXPECT_CALL(*a, Lock(_, testing::IsNull()))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*a, Unlock(_, _))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));
        EXPECT_CALL(*a, Unlock(_, testing::IsNull()))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*a, GetHDL(_, _))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));
        EXPECT_CALL(*a, GetHDL(_, testing::IsNull()))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        EXPECT_CALL(*a, Free(_))
            .WillRepeatedly(testing::Return(MFX_ERR_UNSUPPORTED));
        EXPECT_CALL(*a, Free(testing::IsNull()))
            .WillRepeatedly(testing::Return(MFX_ERR_NULL_PTR));

        mock_allocator(*a, std::forward<Args>(args)...);

        return a;
    }

} }
