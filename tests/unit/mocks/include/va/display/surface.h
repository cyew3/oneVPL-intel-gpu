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

#include "mocks/include/va/display/base.h"
#include "mocks/include/va/resource/buffer.h"
#include "mocks/include/va/resource/image.h"
#include "mocks/include/va/resource/surface.h"

#include <numeric>

inline bool operator==(VAGenericValue v1, VAGenericValue v2)
{
    if (v1.type != v2.type)
        return false;

    switch (v1.type)
    {
    case VAGenericValueTypeInteger: return mocks::equal(v1.value.i, v2.value.i);
    case VAGenericValueTypeFloat:   return mocks::equal(v1.value.f, v2.value.f);
    case VAGenericValueTypePointer: return mocks::equal(v1.value.p, v2.value.p);
    case VAGenericValueTypeFunc:    return mocks::equal(v1.value.fn, v2.value.fn);
    default: assert(!"wrong type");   return false;
    }
}

inline bool operator==(VASurfaceAttrib a1, VASurfaceAttrib a2)
{
    return a1.type  == a2.type  &&
           a1.flags == a2.flags &&
           a1.value == a2.value;
}

namespace mocks { namespace va
{
    namespace detail
    {
        template <typename T, typename U, typename V, typename Attributes, typename F>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_integral<U>,
                std::is_integral<V>,
                std::is_base_of<VASurfaceAttrib, Attributes>,
                is_invocable_r<void*, F, T/*format*/, T/*fourcc*/, U/*width*/, U/*height*/>
            >::value
        >::type
        mock_display(display_tag, display& d, std::tuple<T/*format*/, U /*width*/, U /*height*/, std::tuple<Attributes*, V>, F /*functor*/ > params)
        {
            const std::vector<VASurfaceAttrib> attributes(
                std::get<0>(std::get<3>(params)),                                   // VASurfaceAttrib*
                std::get<0>(std::get<3>(params)) + std::get<1>(std::get<3>(params)) // VASurfaceAttrib* + size
            );
            //VAStatus vaCreateSurfaces(VADisplay d, unsigned int format, unsigned int width, unsigned int height, VASurfaceID* surfaces,
            //                          unsigned int num_surfaces, VASurfaceAttrib* attributes, unsigned int num_attribs)
            EXPECT_CALL(d, CreateSurfaces(
                testing::Eq(std::get<0>(params)), // format
                testing::Eq(std::get<1>(params)), testing::Eq(std::get<2>(params)), // width, height
                testing::NotNull(), testing::Ne(0),       //surfaces & num_surfaces
                testing::_, testing::_))                  //VASurfaceAttrib* & num_attribs -> match .With(testing::Args<5, 6>(...))
                .With(testing::Args<5, 6>(testing::Truly(
                    [attributes](std::tuple<VASurfaceAttrib*, int> xa)
                    {
                        return
                            !std::get<1>(xa) ||           //count must be either zero
                            (std::get<0>(xa) &&           //or all given attributes must present at [std::tuple<Attributes*, V>]
                                testing::Matches(testing::Each(
                                    testing::AnyOfArray(attributes.begin(), attributes.end())
                                ))(xa));
                    }
                )))
                .WillRepeatedly(testing::WithArgs<3, 4, 5, 6>(testing::Invoke(
                    [&d, params](VASurfaceID* surfaces, unsigned int num_surfaces, VASurfaceAttrib* attributes, unsigned int num_attribs)
                    {
                        std::generate_n(surfaces, num_surfaces,
                            [&]()
                            {
                                assert(
                                    d.surfaces.size() < std::numeric_limits<VASurfaceID>::max()
                                    && "Too many surfaces"
                                );

                                auto format = std::get<0>(params);
                                // by default use format/rt_format which is mandatory
                                auto fourcc = format;

                                // PixelFormat is an optional attribute
                                auto fourcc_attribute = std::find_if(attributes, attributes + num_attribs,
                                    [&](auto&& a) { return (a.flags == VA_SURFACE_ATTRIB_SETTABLE) && (a.type == VASurfaceAttribPixelFormat); }
                                );
                                if (fourcc_attribute != (attributes + num_attribs))
                                {
                                    assert(fourcc_attribute->value.type == VAGenericValueTypeInteger);
                                    fourcc = fourcc_attribute->value.value.i;
                                }

                                auto s = make_surface(&d,
                                    std::make_tuple(format, fourcc, std::get<1>(params), std::get<2>(params), std::get<4>(params)),
                                    std::make_tuple(static_cast<VASurfaceAttrib*>(attributes), num_attribs)
                                );

                                return s->id;
                            });
                        return VA_STATUS_SUCCESS;
                    }))
                );
        }

        template <typename T, typename U, typename ...Attributes, typename F>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_integral<U>,
                std::is_base_of<VASurfaceAttrib, Attributes>... ,
                is_invocable_r<void*, F, T/*rt_format*/, T/*fourcc*/, U, U>
            >::value
        >::type
        mock_display(display_tag, display& d, std::tuple<T/*rt_format*/, U /*width*/, U /*height*/, std::tuple<Attributes...>, F /*functor*/> params)
        {
            std::vector<VASurfaceAttrib> attributes;
            for_each_tuple(
                [&](auto attribute)
                { attributes.push_back(attribute); },
                std::get<3>(params)
            );

            return mock_display(display_tag{}, d,
                std::make_tuple(
                    std::get<0>(params),
                    std::get<1>(params), std::get<2>(params),
                    std::make_tuple(attributes.data(), attributes.size()),
                    std::get<4>(params)
                )
            );
        }

        template <typename T, typename U, typename F>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_integral<U>,
                is_invocable_r<void*, F, T/*rt_format*/, T/*fourcc*/, U, U>
            >::value
        >::type
        mock_display(display_tag, display& d, std::tuple<T/*rt_format*/, U /*width*/, U /*height*/, F /*functor*/> params)
        {
                return mock_display(display_tag{}, d,
                std::make_tuple(
                    std::get<0>(params),
                    std::get<1>(params), std::get<2>(params),
                    std::make_tuple(static_cast<VASurfaceAttrib*>(nullptr), 0),
                    std::get<3>(params)
                )
            );
        }
    }

} }

extern "C"
{
    inline FORCE_USE_SYMBOL
    VAStatus vaCreateSurfaces(VADisplay d, unsigned int format, unsigned int width, unsigned int height, VASurfaceID* surfaces, 
                                     unsigned int num_surfaces, VASurfaceAttrib* attributes, unsigned int num_attribs)
    {
        return
            static_cast<mocks::va::display*>(d)->CreateSurfaces(format, width, height, surfaces, num_surfaces, attributes, num_attribs);
    }

    inline FORCE_USE_SYMBOL
    VAStatus vaDestroySurfaces(VADisplay d, VASurfaceID* surfaces, int count)
    {
        return
            static_cast<mocks::va::display*>(d)->DestroySurfaces(surfaces, count);
    }
}
