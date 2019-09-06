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

        /* Mocks context from given D3D11_MAP, provided functor
           tuple(pointer, pitch) = F(D3D11_MAP, UINT flags) is invoked
           and its results are used as (D3D11_MAPPED_SUBRESOURCE) */
        template <typename F>
        inline
        void mock_context(context& c, std::tuple<D3D11_MAP, F> params)
        {
            using testing::_;

            //HRESULT Map(ID3D11Resource*, UINT Subresource, D3D11_MAP, UINT flags, D3D11_MAPPED_SUBRESOURCE*);
            EXPECT_CALL(c, Map(testing::NotNull(), 0, testing::Eq(std::get<0>(params)), testing::AnyOf(0, D3D11_MAP_FLAG_DO_NOT_WAIT), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<2, 3, 4>(testing::Invoke(
                    [params](D3D11_MAP type, UINT flags, D3D11_MAPPED_SUBRESOURCE* m)
                    {
                        auto r = std::get<1>(params)(type, flags);
                        m->pData    = std::get<0>(r);
                        m->RowPitch = static_cast<INT>(std::get<1>(r));
                        m->DepthPitch = 0;

                        return S_OK;
                    }
                )));

            EXPECT_CALL(c, Unmap(testing::NotNull(), 0))
                .WillRepeatedly(testing::Return());
        }

        /* Mocks buffer from given (pointer, pitch) */
        template <typename T>
        inline
        typename std::enable_if<std::is_integral<T>::value>::type
        mock_context(context& c, std::tuple<D3D11_MAP, void*, T> params)
        {
            mock_context(c, std::make_tuple(std::get<0>(params),
                [params](D3D11_MAP, UINT) { return std::make_tuple(std::get<1>(params), std::get<2>(params)); })
            );
        }

        inline
        void mock_context(context& c, D3D11_MAP type)
        {
            mock_context(c, std::make_tuple(type,
                [](D3D11_MAP, UINT) { return std::make_tuple(nullptr, 0); })
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
