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
#include "mocks/include/va/context/context.h"

#include <numeric>

namespace mocks { namespace va
{
    namespace detail
    {
        template <typename Attributes, typename T, typename U, typename V>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_base_of<VAConfigAttrib, Attributes>,
                std::is_integral<T>,
                std::is_integral<U>,
                std::is_integral<V>
            >::value
        >::type
        mock_display(display_tag, display& d, std::tuple< std::tuple<Attributes*, V>, T /*width*/, T /*height*/, U /*flags*/ > params)
        {
            //VAStatus vaCreateContext(VADisplay, VAConfigID, int width, int height, int flags, VASurfaceID*, int count, VAContextID*)
            EXPECT_CALL(d, CreateContext(
                testing::_,                                                         //VAConfigID -> is checked inside CreateContext body
                testing::Eq(std::get<1>(params)), testing::Eq(std::get<2>(params)), //width & height
                testing::Eq(std::get<3>(params)),                                   //flags
                testing::_, testing::_,                                             //VASurfaceID* & count -> match .With(testing::Args<4, 5>(...))
                testing::NotNull()))
                .With(testing::Args<4, 5>(testing::Truly(
                    [](std::tuple<VASurfaceID*, int> surfaces)
                    {
                        return
                               !std::get<1>(surfaces)  // count must be either zero
                             || std::get<0>(surfaces); // or pointer non-zero
                    }
                )))
                .WillRepeatedly(testing::WithArgs<0, 6>(testing::Invoke(
                    [&d, params](VAConfigID config_id, VAContextID* context)
                    {
                        assert(
                            ((d.contexts.size() + 1) < std::numeric_limits<unsigned short>::max())
                            && "Too many contexts"
                        );

                        auto config = std::find_if(std::begin(d.configs), std::end(d.configs),
                            [&](auto&& c) { return c->id == config_id; }
                        );
                        if (config == std::end(d.configs))
                            return VA_STATUS_ERROR_INVALID_CONFIG;

                        auto entrypoints = d.entrypoints.find((*config)->profile);
                        if (entrypoints == std::end(d.entrypoints))
                            return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;

                        auto entrypoint = std::find_if(std::begin(entrypoints->second), std::end(entrypoints->second),
                            [&](auto&& e) { return e == (*config)->entrypoint; }
                        );
                        if (entrypoint == std::end(entrypoints->second))
                            return VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT;

                        auto attributes = d.attributes.find(std::make_pair((*config)->profile, (*config)->entrypoint));
                        if (attributes == std::end(d.attributes))
                            return VA_STATUS_ERROR_ATTR_NOT_SUPPORTED;

                        // +1 below because the zero value is reserved to assign buffers, created by vaCreateSurfaces/vaCreateImage, to the display:
                        *context = VAContextID(d.contexts.size() + 1);
                        auto c = make_context(&d,
                            std::make_tuple(*context, config_id),
                            std::make_tuple(std::get<1>(params), std::get<2>(params), std::get<3>(params))
                        );

                        d.contexts.push_back(c);

                        return VA_STATUS_SUCCESS;
                    }))
                );
        }

        template <typename ...Attributes, typename T, typename U>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_base_of<VAConfigAttrib, Attributes>... ,
                std::is_integral<T>,
                std::is_integral<U>
            >::value
        >::type
        mock_display(display_tag, display& d, std::tuple< std::tuple<Attributes...>, T /*width*/, T /*height*/, U /*flags*/> params)
        {
            std::vector<VAConfigAttrib> attributes;
            for_each_tuple(
                [&](auto attribute)
                { attributes.push_back(attribute); },
                std::get<0>(params)
            );

            return mock_display(display_tag{}, d,
                std::make_tuple(
                    std::make_tuple(attributes.data(), attributes.size()),
                    std::get<1>(params), std::get<2>(params),
                    std::get<3>(params)
                )
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
        mock_display(display_tag, display& d, std::tuple<T /*width*/, T /*height*/, U /*flags*/> params)
        {
            return mock_display(display_tag{}, d,
                std::make_tuple(
                    std::make_tuple(static_cast<VAConfigAttrib*>(nullptr), 0),
                    std::get<0>(params), std::get<1>(params),
                    std::get<2>(params)
                )
            );
        }

    }

} }

extern "C"
{
    inline MOCKS_FORCE_USE_SYMBOL
    VAStatus vaCreateContext(VADisplay d, VAConfigID id, int width, int height, int flags, VASurfaceID* targets, int count, VAContextID* ctx)
    {
        return
            static_cast<mocks::va::display*>(d)->CreateContext(id, width, height, flags, targets, count, ctx);
    }

    inline MOCKS_FORCE_USE_SYMBOL
    VAStatus vaDestroyContext(VADisplay d, VAContextID ctx)
    {
        return
            static_cast<mocks::va::display*>(d)->DestroyContext(ctx);
    }
}
