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

#include "mocks/include/va/resource/base.h"
#include "mocks/include/va/context/base.h"


namespace mocks { namespace va
{
    namespace detail
    {
        inline
        VABufferID make_buffer_id(const context& c)
        {
            return make_id<VABufferID>(
                c.id, c.buffers.size()
            );
        }

        inline
        void mock_buffer(buffer_tag, buffer& b, display* d)
        {
            b.id = make_id<VABufferID>(DISPLAY_PARENT_ID, d->buffers.size());
            d->buffers.push_back(b.shared_from_this());
        }

        inline
        void mock_buffer(buffer_tag, buffer& b, context* c)
        {
            b.context = c;
            b.id = make_buffer_id(*c);
            c->buffers.push_back(b.shared_from_this());
        }

        inline
        void mock_buffer(buffer_tag, buffer& b, VABufferID id)
        {
            b.id = id;
        }

        /* Mocks buffer from given (VABufferType), provided functor
           pointer = F(VABufferType, T(size)) is invoked
           and its results are used as (type, pointer) */
        template <typename T, typename F>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                is_invocable_r<void*, F, VABufferType, T>
            >::value
        >::type
        mock_buffer(buffer_tag, buffer& b, std::tuple<VABufferType, T /*size*/, F /*functor*/> params)
        {
            //VAStatus vaMapBuffer(VADisplay, VABufferID, void**)
            EXPECT_CALL(b, MapBuffer(testing::Eq(b.id), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1>(testing::Invoke(
                    [params](void** result)
                    {
                        *result = std::get<2>(params)(
                            std::get<0>(params), //type
                            std::get<1>(params)  //size
                        );

                        return VA_STATUS_SUCCESS;
                    }
                )));
        }

        /* Mocks buffer from given 'VABufferType', size and pointer */
        template <typename T, typename P>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_pointer<P>
            >::value
        >::type
        mock_buffer(buffer_tag, buffer& b, std::tuple<VABufferType, T /*size*/, P /*pointer*/> params)
        {
            return mock_buffer(buffer_tag{}, b,
                std::make_tuple(
                    std::get<0>(params),
                    std::get<1>(params),
                    [p = std::get<2>(params)](VABufferType, T /*size*/)
                    { return p; }
                )
            );
        }
    }

} }

extern "C"
{
    inline MOCKS_FORCE_USE_SYMBOL
    VAStatus vaMapBuffer(VADisplay d, VABufferID id, void** p)
    {
        return
            static_cast<mocks::va::display*>(d)->MapBuffer(id, p);
    }

    inline MOCKS_FORCE_USE_SYMBOL
    VAStatus vaUnmapBuffer(VADisplay d, VABufferID id)
    {
        return
            static_cast<mocks::va::display*>(d)->UnmapBuffer(id);
    }
}
