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

#include "mocks/include/unknown.h"

#include "mocks/include/dx11/context/base.h"

namespace mocks { namespace dx11
{
    namespace detail
    {
        inline
        void mock_context(context& c, D3D11_DEVICE_CONTEXT_TYPE type)
        {
            EXPECT_CALL(c, GetType())
                .WillRepeatedly(testing::Return(type));
        }

        /* Mocks context for given [ID3D11Resource & D3D11_MAP], provided functor
           tuple(pointer, pitch) = F(ID3D11Resource*, D3D11_MAP, UINT flags) is invoked
           and its results are used as (D3D11_MAPPED_SUBRESOURCE) */
        template <typename T, typename F>
        typename std::enable_if<
            std::is_base_of<ID3D11Resource, T>::value
        >::type
        mock_context(context& c, std::tuple<T, D3D11_MAP, F> params)
        {
            //HRESULT Map(ID3D11Resource*, UINT Subresource, D3D11_MAP, UINT flags, D3D11_MAPPED_SUBRESOURCE*);
            EXPECT_CALL(c, Map(testing::Eq(std::get<0>(params)), 0, testing::Eq(std::get<1>(params)), testing::AnyOf(0, D3D11_MAP_FLAG_DO_NOT_WAIT), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 2, 3, 4>(testing::Invoke(
                    [params](ID3D11Resource* resource, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE* m)
                    {
                        auto r = std::get<2>(params)(resource, type, flags);
                        m->pData    = std::get<0>(r);
                        m->RowPitch = static_cast<INT>(std::get<1>(r));
                        m->DepthPitch = 0;

                        return S_OK;
                    }
                )));

            EXPECT_CALL(c, Unmap(testing::NotNull(), 0))
                .WillRepeatedly(testing::Return());
        }

        /* Mocks context from given [D3D11_MAP], provided functor
           tuple(pointer, pitch) = F(ID3D11Resource*, D3D11_MAP, UINT flags) is invoked
           and its results are used as (D3D11_MAPPED_SUBRESOURCE) */
        template <typename F>
        inline
        auto mock_context(context& c, std::tuple<D3D11_MAP, F> params)
            -> decltype(std::declval<F>()(std::declval<ID3D11Resource*>(), std::declval<D3D11_MAP>(), std::declval<UINT>()), void())
        {
            //HRESULT Map(ID3D11Resource*, UINT Subresource, D3D11_MAP, UINT flags, D3D11_MAPPED_SUBRESOURCE*);
            EXPECT_CALL(c, Map(testing::_, 0, testing::Eq(std::get<0>(params)), testing::AnyOf(0, D3D11_MAP_FLAG_DO_NOT_WAIT), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 2, 3, 4>(testing::Invoke(
                    [params](ID3D11Resource* resource, D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE* m)
                    {
                        auto r = std::get<1>(params)(resource, type, flags);
                        m->pData    = std::get<0>(r);
                        m->RowPitch = static_cast<INT>(std::get<1>(r));
                        m->DepthPitch = 0;

                        return S_OK;
                    }
                )));

            EXPECT_CALL(c, Unmap(testing::NotNull(), 0))
                .WillRepeatedly(testing::Return());
        }

        /* Mocks context from given D3D11_MAP, provided functor
           tuple(pointer, pitch) = F(D3D11_MAP, UINT flags) is invoked
           and its results are used as (D3D11_MAPPED_SUBRESOURCE) */
        template <typename F>
        inline
        auto mock_context(context& c, std::tuple<D3D11_MAP, F> params)
            -> decltype(std::declval<F>()(std::declval<D3D11_MAP>(), std::declval<UINT>()), void())
        {
            mock_context(c,
                std::make_tuple(std::get<0>(params),
                    [params](ID3D11Resource*, D3D11_MAP type, UINT flags)
                    { return std::get<1>(params)(type, flags); }
                )
            );
        }

        /* Mocks buffer from given (pointer, pitch) */
        template <typename P, typename T>
        inline
        typename std::enable_if<
            std::conjunction<std::is_pointer<P>, std::is_integral<T> >::value
        >::type
        mock_context(context& c, std::tuple<D3D11_MAP, P, T> params)
        {
            mock_context(c, std::make_tuple(std::get<0>(params),
                [params](D3D11_MAP, UINT) { return std::make_tuple(std::get<1>(params), std::get<2>(params)); })
            );
        }

        /* Mocks [CopySubresourceRegion] from given args, provided functor
           F(ID3D11Resource* src, ID3D11Resource* dst) is invoked  */
        template <typename T, typename F>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_context(context& c, std::tuple<T /*DstSubresource*/, T /*DstX*/, T /*DstY*/, T /*DstZ*/, T /*SrcSubresource*/, D3D11_BOX, F> params)
        {
            //void CopySubresourceRegion(ID3D11Resource*, UINT DstSubresource, UINT, UINT, UINT, ID3D11Resource*, UINT SrcSubresource, const D3D11_BOX*)
            EXPECT_CALL(c, CopySubresourceRegion(testing::NotNull(),
                testing::Eq(std::get<0>(params)), /*DstSubresource*/
                testing::Eq(std::get<1>(params)), testing::Eq(std::get<2>(params)), testing::Eq(std::get<3>(params)),
                testing::NotNull(),
                testing::Eq(std::get<4>(params)), /*SrcSubresource*/
                testing::AnyOf(
                    testing::IsNull(),
                    testing::Truly([box = std::get<5>(params)](D3D11_BOX const* xb) { return equal(*xb, box); })
                )))
                .WillRepeatedly(testing::WithArgs<0, 5>(testing::Invoke(
                    [params](ID3D11Resource* dst, ID3D11Resource* src)
                    { std::get<6>(params)(src, dst); }
                )));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_context(context& c, std::tuple<T /*DstSubresource*/, T /*DstX*/, T /*DstY*/, T /*DstZ*/, T /*SrcSubresource*/, D3D11_BOX> params)
        {
            return mock_context(c,
                std::tuple_cat(params, std::make_tuple([](ID3D11Resource*, ID3D11Resource*) {}))
            );
        }
    }

    inline
    std::unique_ptr<context>
    make_context()
    {
        return
            make_context(D3D11_DEVICE_CONTEXT_IMMEDIATE);
    }

} }
