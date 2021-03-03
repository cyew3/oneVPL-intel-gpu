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

#include <va/va.h>

namespace mocks { namespace va
{
    template <typename F, typename... Args>
    struct is_invocable :
        std::is_constructible<
            std::function<void(Args ...)>,
            std::reference_wrapper<typename std::remove_reference<F>::type>
        >
    {};

    template <typename R, typename F, typename... Args>
    struct is_invocable_r :
        std::is_constructible<
            std::function<R(Args ...)>,
            std::reference_wrapper<typename std::remove_reference<F>::type>
        >
    {};

    struct context;
    struct buffer
        : std::enable_shared_from_this<buffer>
    {
        MOCK_METHOD2(MapBuffer, VAStatus(VABufferID, void**));
        MOCK_METHOD1(UnmapBuffer, VAStatus(VABufferID));

        struct context* context = nullptr;
        VABufferID      id = VA_INVALID_ID;
    };

    struct image
        : std::enable_shared_from_this<image>
    {
        struct display*            display = nullptr;
        VAImage                    info = {VA_INVALID_ID, 0, VA_INVALID_ID};

        VAImageID&                 id     = info.image_id;
        VABufferID&                buf_id = info.buf;
    };

    struct surface
        : std::enable_shared_from_this<surface>
    {
        MOCK_METHOD2(DeriveImage, VAStatus(VASurfaceID, VAImage*));

        struct display*              display = nullptr;
        VASurfaceID                  id = VA_INVALID_ID;

        unsigned int                 width  = 0;
        unsigned int                 height = 0;
        unsigned int                 format = 0;
        unsigned int                 fourcc = 0;
        std::vector<VASurfaceAttrib> attributes;

        std::function<void*(unsigned int, unsigned int, unsigned int, unsigned int)> memory_accessor = nullptr;
    };

    namespace detail
    {
        //Need for ADL
        struct buffer_tag {};
        struct image_tag {};
        struct surface_tag {};

        template <typename T>
        inline
        void mock_buffer(buffer_tag, buffer&, T)
        {
            /* If you trap here it means that you passed an argument to [make_buffer]
                which is not supported by library, you need to overload [make_buffer] for that argument's type */
            static_assert(
                std::conjunction<std::false_type, std::bool_constant<sizeof(T) != 0> >::value,
                "Unknown argument was passed to [make_buffer]"
            );
        }

        template <typename ...Args>
        inline
        void dispatch(buffer& b, Args&&... args)
        {
            for_each(
                [&b](auto&& x) { mock_buffer(buffer_tag{}, b, std::forward<decltype(x)>(x)); },
                std::forward<Args>(args)...
            );
        }

        template <typename T, typename U = T>
        inline constexpr
        U make_id(T parent, size_t count)
        {
            return U(
                (parent << CHAR_BIT * sizeof(parent) / 2) | count
            );
        }

        constexpr auto DISPLAY_PARENT_ID = 0;
        template <typename T, typename U = T>
        inline constexpr
        U get_parent_id(T parent)
        {
            return U(parent >> CHAR_BIT * sizeof(parent) / 2);
        }

        template <typename T>
        inline
        void mock_image(image_tag, image&, T)
        {
            /* If you trap here it means that you passed an argument to [make_image]
                which is not supported by library, you need to overload [make_image] for that argument's type */
            static_assert(
                std::conjunction<std::false_type, std::bool_constant<sizeof(T) != 0> >::value,
                "Unknown argument was passed to [make_image]"
            );
        }

        template <typename ...Args>
        inline
        void dispatch(image& i, Args&&... args)
        {
            for_each(
                [&i](auto&& x) { mock_image(image_tag{}, i, std::forward<decltype(x)>(x)); },
                std::forward<Args>(args)...
            );
        }

        template <typename T>
        inline
        void mock_surface(surface_tag, surface&, T)
        {
            /* If you trap here it means that you passed an argument to [make_surface]
                which is not supported by library, you need to overload [make_surface] for that argument's type */
            static_assert(
                std::conjunction<std::false_type, std::bool_constant<sizeof(T) != 0> >::value,
                "Unknown argument was passed to [make_surface]"
            );
        }

        template <typename ...Args>
        inline
        void dispatch(surface& s, Args&&... args)
        {
            for_each(
                [&s](auto&& x) { mock_surface(surface_tag{}, s, std::forward<decltype(x)>(x)); },
                std::forward<Args>(args)...
            );
        }
    }

    template <typename ...Args>
    inline
    buffer& mock_buffer(buffer& b, Args&&... args)
    {
        detail::dispatch(b, std::forward<Args>(args)...);

        return b;
    }

    template <typename ...Args>
    inline
    std::shared_ptr<buffer>
    make_buffer(Args&&... args)
    {
        auto b = std::shared_ptr<testing::NiceMock<buffer> >{
            new testing::NiceMock<buffer>{}
        };

        //VAStatus vaMapBuffer(VADisplay, VABufferID, void**)
        EXPECT_CALL(*b, MapBuffer(testing::_, testing::_))
            .WillRepeatedly((testing::InvokeWithoutArgs(
                []() { assert(!"No mock provided"); return VA_STATUS_ERROR_UNIMPLEMENTED; }
            )));

        EXPECT_CALL(*b, MapBuffer(testing::Truly(
            [b = b.get()](VABufferID id)
            { return b->id != id; }), testing::_))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_BUFFER));

        EXPECT_CALL(*b, MapBuffer(testing::_, testing::IsNull()))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        //VAStatus vaUnmapBuffer(VADisplay, VABufferID);
        EXPECT_CALL(*b, UnmapBuffer(testing::_))
            .WillRepeatedly((testing::InvokeWithoutArgs(
                []() { assert(!"No mock provided"); return VA_STATUS_ERROR_UNIMPLEMENTED; }
            )));

        EXPECT_CALL(*b, UnmapBuffer(testing::Truly(
            [b = b.get()](VABufferID id)
            { return b->id != id; })))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_BUFFER));

        EXPECT_CALL(*b, UnmapBuffer(testing::Truly(
            [b = b.get()](VABufferID id)
            { return b->id == id; })))
            .WillRepeatedly(testing::Return(VA_STATUS_SUCCESS));

        mock_buffer(*b, std::forward<Args>(args)...);

        return b;
    }

    template <typename ...Args>
    inline
    image& mock_image(image& i, Args&&... args)
    {
        detail::dispatch(i, std::forward<Args>(args)...);

        return i;
    }

    template <typename ...Args>
    inline
    std::shared_ptr<image>
    make_image(Args&&... args)
    {
        auto i = std::shared_ptr<testing::NiceMock<image> >{
            new testing::NiceMock<image>{}
        };

        mock_image(*i, std::forward<Args>(args)...);

        return i;
    }

    template <typename ...Args>
    inline
    surface& mock_surface(surface& s, Args&&... args)
    {
        detail::dispatch(s, std::forward<Args>(args)...);

        return s;
    }

    template <typename ...Args>
    inline
    std::shared_ptr<surface>
    make_surface(Args&&... args)
    {
        auto s = std::shared_ptr<testing::NiceMock<surface> >{
            new testing::NiceMock<surface>{}
        };

        //VAStatus vaDeriveImage(VADisplay, VASurfaceID, VAImage*)
        EXPECT_CALL(*s, DeriveImage(testing::_, testing::_))
            .WillRepeatedly((testing::InvokeWithoutArgs(
                []() { return VA_STATUS_ERROR_UNIMPLEMENTED; }
            )));
        mock_surface(*s, std::forward<Args>(args)...);

        return s;
    }

}
    template <>
    struct default_result<std::shared_ptr<va::buffer> >
    {
        using type = int;
        static constexpr type value = VA_STATUS_ERROR_INVALID_BUFFER;
        constexpr type operator()() const noexcept
        { return value; }
    };

    template <>
    struct default_result<std::shared_ptr<va::image> >
    {
        using type = int;
        static constexpr type value = VA_STATUS_ERROR_INVALID_IMAGE;
        constexpr type operator()() const noexcept
        { return value; }
    };

    template <>
    struct default_result<std::shared_ptr<va::surface> >
    {
        using type = int;
        static constexpr type value = VA_STATUS_ERROR_INVALID_SURFACE;
        constexpr type operator()() const noexcept
        { return value; }
    };
}
