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

#include "mocks/include/dx12/winapi/list.hxx"

namespace mocks { namespace dx12
{
    struct list
        : winapi::list_impl
    {
        MOCK_METHOD0(GetType, D3D12_COMMAND_LIST_TYPE());
        MOCK_METHOD0(Close, HRESULT());
        MOCK_METHOD2(Reset, HRESULT(ID3D12CommandAllocator*, ID3D12PipelineState*));
		MOCK_METHOD6(CopyTextureRegion, void(const D3D12_TEXTURE_COPY_LOCATION*, UINT, UINT, UINT, const D3D12_TEXTURE_COPY_LOCATION*, const D3D12_BOX*));
    };

    namespace detail
    {
        inline
        void mock_list(list& l, D3D12_COMMAND_LIST_TYPE type)
        {
            //D3D12_COMMAND_LIST_TYPE GetType()
            EXPECT_CALL(l, GetType())
                .WillRepeatedly(testing::Return(type))
            ;
        }

		template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_list(list& l, std::tuple<D3D12_TEXTURE_COPY_LOCATION /*dst*/, T /*DstX*/, T /*DstY*/, T /*DstZ*/,
                                      D3D12_TEXTURE_COPY_LOCATION /*src*/, D3D12_BOX> params)
        {
            //void CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* /*pDst*/, UINT /*DstX*/, UINT /*DstY*/, UINT /*DstZ*/,
            //                       const D3D12_TEXTURE_COPY_LOCATION* /*pSrc*/, const D3D12_BOX* /*pSrcBox*/)
            EXPECT_CALL(l, CopyTextureRegion(
                testing::AllOf(
                    testing::NotNull(),
                    testing::Truly([loc = std::get<0>(params)](D3D12_TEXTURE_COPY_LOCATION const* xl) { return equal(*xl, loc); })
                ),
                testing::_, testing::_, testing::_,
                testing::AllOf(
                    testing::NotNull(),
                    testing::Truly([loc = std::get<4>(params)](D3D12_TEXTURE_COPY_LOCATION const* xl) { return equal(*xl, loc); })
                ),
                testing::AnyOf(
                    testing::IsNull(),
                    testing::Truly([box = std::get<5>(params)](D3D12_BOX const* xb) { return equal(*xb, box); })
                )))
                .Times(testing::AnyNumber());
        }

        template <typename T, typename U>
        inline
        typename std::enable_if<std::conjunction<
            std::is_base_of<ID3D12CommandAllocator, T>,
            std::is_base_of<ID3D12PipelineState, U>
        >::value>::type
        mock_list(list& l, std::tuple<T*, U*> params)
        {
            //HRESULT Reset(ID3D12CommandAllocator*, ID3D12PipelineState*)
            EXPECT_CALL(l, Reset(
                testing::Eq(std::get<0>(params)),
                testing::AnyOf(
                    testing::Eq(std::get<1>(params)),
                    testing::IsNull())
                ))
                .WillRepeatedly(testing::Return(S_OK))
            ;
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_base_of<ID3D12CommandAllocator, T>::value
        >::type
        mock_list(list& l, T* allocator)
        {
            return mock_list(l,
                std::make_tuple(allocator, static_cast<ID3D12PipelineState*>(nullptr))
            );
        }

        template <typename T>
        inline
        void mock_list(list&, T)
        {
            static_assert(false,
                "Unknown argument was passed to [make_list]"
            );
        }
    }

    template <typename ...Args>
    inline
    list& mock_list(list& l, Args&&... args)
    {
        for_each(
            [&l](auto&& x) { detail::mock_list(l, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return l;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<list>
    make_list(Args&&... args)
    {
        auto l = std::unique_ptr<testing::NiceMock<list> >{
            new testing::NiceMock<list>{}
        };

        //HRESULT Close()
        EXPECT_CALL(*l, Close())
                .WillRepeatedly(testing::Return(S_OK))
            ;

        using testing::_;

        //HRESULT Reset(ID3D12CommandAllocator*, ID3D12PipelineState*)
        EXPECT_CALL(*l, Reset(_, _))
            .WillRepeatedly(testing::Return(E_FAIL))
            ;

        //void CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* /*pDst*/, UINT /*DstX*/, UINT /*DstY*/, UINT /*DstZ*/,
        //                       const D3D12_TEXTURE_COPY_LOCATION* /*pSrc*/, const D3D12_BOX* /*pSrcBox*/)
        EXPECT_CALL(*l, CopyTextureRegion(_, _, _, _, _, _))
            .WillRepeatedly(testing::WithoutArgs(testing::Invoke(
                []() { throw std::system_error(E_FAIL, std::system_category()); }
            )));

        mock_list(*l, std::forward<Args>(args)...);

        return l;
    }

} }
