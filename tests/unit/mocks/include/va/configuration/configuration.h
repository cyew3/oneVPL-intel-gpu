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

#include "mocks/include/va/configuration/base.h"
#include "mocks/include/va/display/base.h"

namespace mocks { namespace va
{
    namespace detail
    {
        inline
        void mock_config(configuration_tag, configuration& c, display* d)
        {
            c.display = d;
            d->configs.push_back(c.shared_from_this());
        }

        inline
        void mock_config(configuration_tag, configuration& c, VAConfigID id)
        {
            c.id = id;
        }

        template <typename Attributes, typename T>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_base_of<VAConfigAttrib, Attributes>
            >::value
        >::type
        mock_config(configuration_tag, configuration& c, std::tuple<VAProfile, VAEntrypoint, std::tuple<Attributes*, T> > params)
        {
            c.profile                  = std::get<0>(params);
            c.entrypoint               = std::get<1>(params);
            c.attributes.assign(
                std::get<0>(std::get<2>(params)),                                   // VAConfigAttrib*
                std::get<0>(std::get<2>(params)) + std::get<1>(std::get<2>(params)) // VAConfigAttrib* + size
            );

            EXPECT_CALL(c, QueryConfigAttributes(testing::NotNull(), testing::NotNull(), testing::NotNull(), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 1, 2, 3>(testing::Invoke(
                    [&c](VAProfile* profile, VAEntrypoint* entrypoint, VAConfigAttrib* list, int* count)
                    {
                        *profile    = c.profile;
                        *entrypoint = c.entrypoint;
                        *count      = (int)c.attributes.size();
                        std::copy_n(c.attributes.data(), *count, list);

                        return VA_STATUS_SUCCESS;
                    }))
                );
        }

        inline
        void mock_config(configuration_tag, configuration& c, std::tuple<VAProfile, VAEntrypoint> params)
        {
            mock_config(configuration_tag{}, c,
                std::make_tuple(
                    std::get<0>(params), std::get<1>(params),
                    std::make_tuple(static_cast<VAConfigAttrib*>(nullptr), 0)
                )
            );
        }

        inline
        void mock_config(configuration_tag, configuration& c, VASurfaceAttrib attribute)
        {
            c.surface_attributes.push_back(attribute);
        }

        template <typename ...Attributes>
        inline
        typename std::enable_if<
            std::conjunction<std::is_same<Attributes, VASurfaceAttrib>...>::value
        >::type
        mock_config(configuration_tag, configuration& c, std::tuple<Attributes...> params)
        {
            return for_each_tuple(
                [&c](auto attribute)
                { mock_config(configuration_tag{}, c, attribute); },
                params
            );
        }

        template <typename Attributes, typename T>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_base_of<VASurfaceAttrib, Attributes>
            >::value
        >::type
        mock_config(configuration_tag, configuration& c, std::tuple<Attributes*, T> params)
        {
            std::for_each(std::get<0>(params), std::get<0>(params) + std::get<1>(params),
                [&c](VASurfaceAttrib attribute)
                { mock_config(configuration_tag{}, c, attribute); }
            );
        }
    }

} }

extern "C"
{
    inline MOCKS_FORCE_USE_SYMBOL
    VAStatus vaGetConfigAttributes(VADisplay d, VAProfile profile, VAEntrypoint entrypoint, VAConfigAttrib* attributes, int count)
    {
        return
            static_cast<mocks::va::display*>(d)->GetConfigAttributes(profile, entrypoint, attributes, count);
    }

    inline MOCKS_FORCE_USE_SYMBOL
    VAStatus vaQuerySurfaceAttributes(VADisplay d, VAConfigID config, VASurfaceAttrib* attributes, unsigned int* count)
    {
        return
            static_cast<mocks::va::display*>(d)->QuerySurfaceAttributes(config, attributes, count);
    }
}
