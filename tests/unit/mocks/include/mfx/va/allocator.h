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

#include "umc_frame_data.h"
#include "umc_frame_allocator.h"

#include "mocks/include/common.h"
#include "mocks/include/va/format.h"

#include "mocks/include/va/display/surface.h"
#include "mocks/include/va/resource/surface.h"

namespace mocks { namespace mfx { namespace va
{
    struct allocator
        : UMC::FrameAllocator
        , std::enable_shared_from_this<allocator>
    {
        mocks::va::display* display = nullptr;

        std::vector<std::pair<
            UMC::FrameData, std::shared_ptr<mocks::va::surface>
        > > pool;

        MOCK_METHOD1(Init, UMC::Status(UMC::FrameAllocatorParams*));
        MOCK_METHOD0(Close, UMC::Status());
        MOCK_METHOD0(Reset, UMC::Status());
        MOCK_METHOD3(Alloc, UMC::Status(UMC::FrameMemID*, const UMC::VideoDataInfo*, uint32_t));
        MOCK_METHOD2(GetFrameHandle, UMC::Status(UMC::FrameMemID, void*));
        MOCK_METHOD1(Lock, const UMC::FrameData*(UMC::FrameMemID));
        MOCK_METHOD1(Unlock, UMC::Status(UMC::FrameMemID));
        MOCK_METHOD1(IncreaseReference, UMC::Status(UMC::FrameMemID));
        MOCK_METHOD1(DecreaseReference, UMC::Status(UMC::FrameMemID));
    };

    namespace detail
    {
        //Need for ADL
        struct allocator_tag {};

        inline
        void mock_allocator(allocator_tag, allocator& a, mocks::va::display* d)
        {
            a.display = d;
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_allocator(allocator_tag, allocator& a, std::tuple<T, UMC::VideoDataInfo> params)
        {
            using testing::_;

            //Status Alloc(FrameMemID*, const VideoDataInfo*, uint32_t flags)
            EXPECT_CALL(a, Alloc(testing::NotNull(), testing::AllOf(
                testing::NotNull(), testing::Truly(
                    [vi = std::get<1>(params)](UMC::VideoDataInfo const* xi)
                    {
                        bool eq =
                               xi->GetColorFormat() == vi.GetColorFormat()
                            && xi->GetNumPlanes()   == vi.GetNumPlanes()
                            && xi->GetWidth()       == vi.GetWidth()
                            && xi->GetHeight()      == vi.GetHeight()
                            && xi->GetSize()        == vi.GetSize()
                            && equal(xi->GetPictureStructure(), vi.GetPictureStructure())
                            ;
                        if (!eq)
                            return false;

                        int32_t xh, xv; xi->GetAspectRatio(&xh, &xv);
                        int32_t h, v; vi.GetAspectRatio(&h, &v);
                        if (xh != h || xv != v)
                            return false;

                        auto n = xi->GetNumPlanes();
                        for (uint32_t i = 0; i < n; ++i)
                            if (!equal(*xi->GetPlaneInfo(i), *vi.GetPlaneInfo(i)))
                                return false;

                        return true;
                    })), _))
                .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                    [&a, params](UMC::FrameMemID* mid, UMC::VideoDataInfo const* xi)
                    {
                        if (!(a.pool.size() < size_t(std::get<0>(params))))
                        {
                            *mid = UMC::FRAME_MID_INVALID;
                            return UMC::UMC_ERR_ALLOC;
                        }
                        else
                        {
                            auto const& vi = std::get<1>(params);

                            auto s = mocks::va::make_surface(a.display,
                                std::make_tuple(
                                    mocks::va::to_native(fourcc::to_fourcc(vi.GetColorFormat())),
                                    vi.GetWidth(),
                                    vi.GetHeight()
                                )
                            );

                            *mid = UMC::FrameMemID(s->id);

                            UMC::FrameData fd{};
                            fd.Init(&vi, *mid, nullptr);

                            a.pool.emplace_back(fd, s);
                            return UMC::UMC_OK;
                        }
                    }
                )));

            //need to check actual 'pool' size (after 'Alloc'), not max which is 'std::get<0>(params)'
            auto CheckMid = testing::AllOf(
                testing::Ge(0),
                testing::Truly(
                    [&a](UMC::FrameMemID mid)
                    { return mid < UMC::FrameMemID(a.pool.size()); }
                )
            );

            EXPECT_CALL(a, GetFrameHandle(_, testing::NotNull()))
                .WillRepeatedly(testing::Return(UMC::UMC_ERR_ALLOC));
            EXPECT_CALL(a, GetFrameHandle(CheckMid, testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                    [&a](UMC::FrameMemID mid, void* handle)
                    {
                        assert(mid < UMC::FrameMemID(a.pool.size()));

                        auto p = reinterpret_cast<mfxHDLPair*>(handle);
                        p->first  = a.pool[mid].second.get();
                        p->second = nullptr;

                        return UMC::UMC_OK;
                    })));

            EXPECT_CALL(a, Lock(CheckMid))
                .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                    [&a](UMC::FrameMemID mid)
                    { return &a.pool[mid].first; }
                )));

            EXPECT_CALL(a, Unlock(CheckMid))
                .WillRepeatedly(testing::Return(UMC::UMC_OK));

            EXPECT_CALL(a, IncreaseReference(CheckMid))
                .WillRepeatedly(testing::Return(UMC::UMC_OK));

            EXPECT_CALL(a, DecreaseReference(CheckMid))
                .WillRepeatedly(testing::Return(UMC::UMC_OK));
        }

        template <typename T>
        inline
        typename std::enable_if<
            !std::is_integral<T>::value
        >::type
        mock_allocator(allocator_tag, allocator&, T)
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
    std::shared_ptr<allocator>
    make_allocator(Args&&... args)
    {
        auto a = std::unique_ptr<testing::NiceMock<allocator> >{
            new testing::NiceMock<allocator>{}
        };

        using testing::_;

        EXPECT_CALL(*a, Close())
            .WillRepeatedly(testing::Return(UMC::UMC_OK));
        EXPECT_CALL(*a, Reset())
            .WillRepeatedly(testing::Return(UMC::UMC_OK));

        EXPECT_CALL(*a, Alloc(_, _, _))
            .WillRepeatedly(testing::Return(UMC::UMC_ERR_NULL_PTR));

        //Status GetFrameHandle(UMC::FrameMemID, void* handle)
        EXPECT_CALL(*a, GetFrameHandle(_, _))
            .WillRepeatedly(testing::Return(UMC::UMC_ERR_ALLOC));

        //const FrameData* Lock(UMC::FrameMemID)
        EXPECT_CALL(*a, Lock(_))
            .WillRepeatedly(testing::Return(nullptr));

        //Status Unlock(UMC::FrameMemID)
        EXPECT_CALL(*a, Unlock(_))
            .WillRepeatedly(testing::Return(UMC::UMC_ERR_FAILED));

        //Status IncreaseReference(UMC::FrameMemID)
        EXPECT_CALL(*a, IncreaseReference(_))
            .WillRepeatedly(testing::Return(UMC::UMC_ERR_FAILED));

        //Status DecreaseReference(UMC::FrameMemID)
        EXPECT_CALL(*a, DecreaseReference(_))
            .WillRepeatedly(testing::Return(UMC::UMC_ERR_FAILED));

        mock_allocator(*a, std::forward<Args>(args)...);

        return a;
    }

} } }
