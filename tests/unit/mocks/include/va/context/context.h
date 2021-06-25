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

#include "mocks/include/va/context/base.h"
#include "mocks/include/va/resource/buffer.h"

namespace mocks { namespace va
{
    namespace detail
    {
        inline
        void mock_context(context_tag, context& c, display* d)
        {
            c.display = d;
            d->contexts.push_back(c.shared_from_this());
        }

        inline
        void mock_context(context_tag, context& c, std::tuple<VAContextID, VAConfigID> params)
        {
            c.id            = std::get<0>(params);
            c.configuration = std::get<1>(params);
        }

        template <typename T, typename U>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_integral<U>
            >::value
        >::type
        mock_context(context_tag, context& c, std::tuple<T /*width*/, T /*height*/, U /*flags*/> params)
        {
            c.width  = std::get<0>(params);
            c.height = std::get<1>(params);
            c.flags  = std::get<2>(params);
        }


        // Mocks 'vaCreateBuffer' w/ given buffer's type, size and count as well as 'vaMapBuffer' for resulting buffer ID.
        // Given 'pointer' tuple's element will be returned from 'vaMapBuffer'. If 'data' parameter is passed to
        // 'vaCreateBuffer' then its data is copied to buffer pointed by 'pointer'.
        template <typename T, typename U, typename P>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_integral<U>,
                std::disjunction<
                    std::is_pointer<P>,
                    std::is_null_pointer<P>
                >
            >::value
        >::type
        mock_context(context_tag, context& c, std::tuple<VABufferType, T /*size*/, U /*count*/, P /*pointer*/> params)
        {
            //VAStatus vaCreateBuffer(VADisplay, VAContextID, VABufferType, unsigned int size, unsigned int count, void* data, VABufferID*)
            EXPECT_CALL(c, CreateBuffer(
                testing::Eq(std::get<0>(params)), //type
                testing::Eq(std::get<1>(params)), //size
                testing::Le(std::get<2>(params)), //count
                testing::_,                       //data
                testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<3, 4>(testing::Invoke(
                    [&c, params](void* data, VABufferID* id)
                    {
                        assert(
                            c.buffers.size() < std::numeric_limits<unsigned short>::max()
                            && "Too many buffers"
                        );

                        auto size  = std::get<1>(params);
                        auto count = std::get<2>(params);
                        auto buffer_size = size * count;
                        auto src = static_cast<char*>(data);
                        auto dst = static_cast<char*>(static_cast<void*>(std::get<3>(params)));
                        if (src && dst)
                            std::copy(src, src + buffer_size, dst);
                        *id = make_buffer_id(c);
                        auto b = make_buffer(*id,
                            std::make_tuple(std::get<0>(params), buffer_size, dst)
                        );

                        c.buffers.push_back(b);

                        return VA_STATUS_SUCCESS;
                    }))
                );
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_context(context_tag, context& c, std::tuple<VABufferType, T /*size*/> params)
        {
            return mock_context(context_tag{}, c,
                std::tuple_cat(params, std::make_tuple(1, nullptr))
            );
        }

        template <typename T, typename U>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_integral<U>
            >::value
        >::type
        mock_context(context_tag, context& c, std::tuple<VABufferType, T /*size*/, U /*count*/> params)
        {
            return mock_context(context_tag{}, c,
                std::tuple_cat(params, std::make_tuple(nullptr))
            );
        }
    }

} }

extern "C"
{
    inline MOCKS_FORCE_USE_SYMBOL
    VAStatus vaCreateBuffer(VADisplay d, VAContextID ctx, VABufferType type, unsigned int size, unsigned int count, void* data, VABufferID* id)
    {
        return
            static_cast<mocks::va::display*>(d)->CreateBuffer(ctx, type, size, count, data, id);
    }

    inline MOCKS_FORCE_USE_SYMBOL
    VAStatus vaDestroyBuffer(VADisplay d, VABufferID id)
    {
        return
            static_cast<mocks::va::display*>(d)->DestroyBuffer(id);
    }
}
