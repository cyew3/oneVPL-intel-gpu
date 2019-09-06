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

#include "mocks/include/dx12/winapi/queue.hxx"

#include <atlbase.h>

namespace mocks { namespace dx12
{
    struct queue
        : winapi::queue_impl
    {
        std::vector<CComPtr<ID3D12CommandList> > lists;

        MOCK_METHOD0(GetDesc, D3D12_COMMAND_QUEUE_DESC());
        MOCK_METHOD2(ExecuteCommandLists, void(UINT, ID3D12CommandList* const*));
        MOCK_METHOD2(Signal, HRESULT(ID3D12Fence*, UINT64));
    };

    namespace detail
    {
        inline
        void mock_queue(queue& q, D3D12_COMMAND_QUEUE_DESC qd)
        {
            EXPECT_CALL(q, GetDesc())
                .WillRepeatedly(testing::Return(qd));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_base_of<ID3D12CommandList, T>::value
        >::type
        mock_queue(queue& q, T* list)
        {
            q.lists.push_back(list);

            EXPECT_CALL(q, ExecuteCommandLists(
                testing::Truly([&q](UINT n) { return !(n > q.lists.size()); }),
                testing::NotNull()))
                .With(testing::Args<0, 1>(testing::Truly(
                    [&q](std::tuple<UINT, ID3D12CommandList* const*> lists)
                    {
                        return std::all_of(std::get<1>(lists), std::get<1>(lists) + std::get<0>(lists),
                            [&](ID3D12CommandList* l)
                            {
                                return
                                    std::find(std::begin(q.lists), std::end(q.lists), l) != std::end(q.lists);
                            }
                        );
                    })))
                .Times(testing::AnyNumber());
        }

        template <typename T, typename U>
        inline
        typename std::enable_if<std::conjunction<
            std::is_base_of<ID3D12Fence, T>,
            std::is_integral<U>
        >::value>::type
        mock_queue(queue& q, std::tuple<T*, U> params)
        {
            //HRESULT Signal(ID3D12Fence*, UINT64)
            EXPECT_CALL(q, Signal(testing::Eq(std::get<0>(params)), testing::Eq(UINT64(std::get<1>(params)))))
                .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                    [](ID3D12Fence* fence, UINT64 value) { return fence->Signal(value); }
                )));
            ;
        }

        template <typename T>
        inline
        void mock_queue(queue&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_queue]"
            );
        }
    }

    template <typename ...Args>
    inline
    queue& mock_queue(queue& q, Args&&... args)
    {
        for_each(
            [&q](auto&& x) { detail::mock_queue(q, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return q;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<queue>
    make_queue(Args&&... args)
    {
        auto q = std::unique_ptr<testing::NiceMock<queue> >{
            new testing::NiceMock<queue>{}
        };

        using testing::_;

        //void ExecuteCommandLists(UINT /*NumCommandLists*/, ID3D12CommandList* const*)
        EXPECT_CALL(*q, ExecuteCommandLists(_, _))
            .WillRepeatedly(testing::WithoutArgs(testing::Invoke(
                []() { throw std::system_error(E_FAIL, std::system_category()); }
            )));

        //HRESULT Signal(ID3D12Fence*, UINT64)
        EXPECT_CALL(*q, Signal(_, _))
            .WillRepeatedly(testing::Return(E_FAIL))
            ;

        mock_queue(*q, std::forward<Args>(args)...);

        return q;
    }

} }
