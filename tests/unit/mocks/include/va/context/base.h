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

#include "mocks/include/va/resource/base.h"

namespace mocks { namespace va
{
    struct display;
    struct context
        : std::enable_shared_from_this<context>
    {
        MOCK_METHOD5(CreateBuffer, VAStatus(VABufferType, unsigned int, unsigned int, void*, VABufferID*));
        MOCK_METHOD1(DestroyBuffer, VAStatus(VABufferID));
        MOCK_METHOD2(MapBuffer, VAStatus(VABufferID, void**));
        MOCK_METHOD1(UnmapBuffer, VAStatus(VABufferID));

        struct display*          display = nullptr;
        VAContextID              id = VA_INVALID_ID;
        VAConfigID               configuration = VA_INVALID_ID;
        int                      width  = 0;
        int                      height = 0;
        int                      flags  = 0;

        std::vector<std::shared_ptr<buffer> >  buffers;
    };

    namespace detail
    {
        //Need for ADL
        struct context_tag {};

        template <typename T>
        inline
        void mock_context(context_tag, context&, T)
        {
            /* If you trap here it means that you passed an argument to [make_context]
                which is not supported by library, you need to overload [make_context] for that argument's type */
            static_assert(
                std::conjunction<std::false_type, std::bool_constant<sizeof(T) != 0> >::value,
                "Unknown argument was passed to [make_context]"
            );
        }

        template <typename ...Args>
        inline
        void dispatch(context& c, Args&&... args)
        {
            for_each(
                [&c](auto&& x) { mock_context(context_tag{}, c, std::forward<decltype(x)>(x)); },
                std::forward<Args>(args)...
            );
        }
    }

    template <typename ...Args>
    inline
    context& mock_context(context& c, Args&&... args)
    {
        detail::dispatch(c, std::forward<Args>(args)...);

        return c;
    }

    template <typename ...Args>
    inline
    std::shared_ptr<context>
    make_context(Args&&... args)
    {
        auto c = std::shared_ptr<testing::NiceMock<context> >{
            new testing::NiceMock<context>{}
        };

        using testing::_;

        //VAStatus vaCreateBuffer(VADisplay, VAContextID, VABufferType, unsigned int, unsigned int, void*, VABufferID*);
        EXPECT_CALL(*c, CreateBuffer(_, _, _, _, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_BUFFER));

        //VAStatus vaDestroyBuffer(VADisplay, VABufferID)
        EXPECT_CALL(*c, DestroyBuffer(_))
            .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                [c = c.get()](VABufferID id)
                {
                    auto b = std::find_if(std::begin(c->buffers), std::end(c->buffers),
                        [id](auto&& c) { return c->id == id; }
                    );

                    return b != std::end(c->buffers) ?
                        c->buffers.erase(b), VA_STATUS_SUCCESS : VA_STATUS_ERROR_INVALID_BUFFER;
                }
            )));

        //VAStatus vaMapBuffer(VADisplay dpy, VABufferID buf_id, void **data);
        EXPECT_CALL(*c, MapBuffer(_, _))
            .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                [c = c.get()](VABufferID id, void** data)
                {
                    return invoke_if(c->buffers,
                        [&](auto&& b) { return b->MapBuffer(id, data); },
                        [&](auto&& b) { return b->id == id; }
                    );
                }))
            );

        //VAStatus vaUnmapBuffer(VADisplay, VABufferID)
        EXPECT_CALL(*c, UnmapBuffer(_))
            .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                [c = c.get()](VABufferID id)
                {
                    return invoke_if(c->buffers,
                        [&](auto&& b) { return b->UnmapBuffer(id); },
                        [&](auto&& b) { return b->id == id; }
                    );
                }))
            );

        mock_context(*c, std::forward<Args>(args)...);

        return c;
    }
}
    template <>
    struct default_result<std::shared_ptr<va::context> >
    {
        using type = int;
        static constexpr type value = VA_STATUS_ERROR_INVALID_CONTEXT;
        constexpr type operator()() const noexcept
        { return value; }
    };
}
