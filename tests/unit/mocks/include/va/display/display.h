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
#include "mocks/include/va/configuration/configuration.h"

#include <numeric>

namespace mocks { namespace va
{
    namespace detail
    {
        inline
        void mock_display(display_tag, display& d, std::tuple<int, int> version)
        {
            //VAStatus vaInitialize(VADisplay, int*, int*)
            EXPECT_CALL(d, Initialize(testing::NotNull(), testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(std::get<0>(version)),
                    testing::SetArgPointee<1>(std::get<1>(version)),
                    testing::Return(VA_STATUS_SUCCESS))
                );
        }

        inline
        void mock_display(display_tag, display& d, int major)
        {
            mock_display(display_tag{}, d,
                std::make_tuple(major, 0)
            );
        }

        inline
        void mock_display(display_tag, display& d, std::tuple<VAMessageCallback, void*> callback)
        {
            using testing::_;

            //VAMessageCallback vaSetErrorCallback(VAMessageCallback, void*)
            /*EXPECT_CALL(d, SetErrorCallback(testing::NotNull(), _))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(std::get<0>(callback)),
                    testing::SetArgPointee<1>(std::get<1>(callback)),
                    testing::Return(VAMessageCallback(nullptr)))
                );*/
        }

        inline
        void mock_display(display_tag, display& d, char const* vendor)
        {
            //char const* vaQueryVendorString()
            EXPECT_CALL(d, QueryVendorString())
                .WillRepeatedly(testing::Invoke(
                    [v = std::string(vendor)]() { return v.c_str(); })
                );
        }

        template <typename Attributes, typename T>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_base_of<VAConfigAttrib, Attributes>
            >::value
        >::type
        mock_display(display_tag, display& d, std::tuple<VAProfile, VAEntrypoint, std::tuple<Attributes*, T> > params)
        {
            auto const& profile    = std::get<0>(params);
            auto const& entrypoint = std::get<1>(params);
            auto const& attribute  = std::get<2>(params);

            d.profiles.push_back(profile);

            EXPECT_CALL(d, QueryConfigProfiles(testing::NotNull(), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                    [&d](VAProfile* list, int* count)
                    {
                        std::copy(std::begin(d.profiles), std::end(d.profiles), list);
                        *count = int(d.profiles.size());
                        return VA_STATUS_SUCCESS;
                    }))
                );

            d.entrypoints[profile].push_back(entrypoint);
            //Number of entry points for all profiles
            EXPECT_CALL(d, MaxNumEntrypoints())
                .WillRepeatedly(testing::Invoke(
                    [&d]()
                    {
                        return std::accumulate(std::begin(d.entrypoints), std::end(d.entrypoints), 0,
                            [](int count, typename decltype(d.entrypoints)::value_type const& entrypoints)
                            { return count + int(entrypoints.second.size()); }
                        );
                    }
                ));

            //VAStatus vaQueryConfigEntrypoints(VAProfile, VAEntrypoint*, int*)
            EXPECT_CALL(d, QueryConfigEntrypoints(testing::Eq(profile), testing::NotNull(), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 1, 2>(testing::Invoke(
                    [&d](VAProfile profile, VAEntrypoint* list, int* count)
                    {
                        auto& entrypoints = d.entrypoints[profile];
                        std::copy(std::begin(entrypoints), std::end(entrypoints), list);
                        *count = int(entrypoints.size());

                        return VA_STATUS_SUCCESS;
                    }
                )));

            d.attributes[std::make_pair(profile, entrypoint)].assign(
                std::get<0>(attribute),                                             // VAConfigAttrib*
                std::get<0>(std::get<2>(params)) + std::get<1>(std::get<2>(params)) // VAConfigAttrib* + size
            );

            //Number of entry points for all profiles
            EXPECT_CALL(d, MaxNumConfigAttributes())
                .WillRepeatedly(testing::Invoke(
                    [&d]()
                    {
                        return std::accumulate(std::begin(d.attributes), std::end(d.attributes), 0,
                            [](int count, typename decltype(d.attributes)::value_type const& attributes)
                            { return count + int(attributes.second.size()); }
                        );
                    }
                ));

            auto& attributes = d.attributes[std::make_pair(profile, entrypoint)];
            //[vaGetConfigAttributes] returns attribute list for given profile/entrypoint pair
            //need a map profile/entrypoint -> attributes ???
            EXPECT_CALL(d, GetConfigAttributes(testing::Eq(profile), testing::Eq(entrypoint), testing::NotNull(), testing::_))
                .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
                    [&attributes](VAConfigAttrib* list, int count)
                    {
                        std::for_each(list, list + count, [&attributes](VAConfigAttrib & out_attr)
                            {
                                auto it = std::find_if(std::begin(attributes), std::end(attributes),
                                    [&out_attr](const VAConfigAttrib & attrib) { return (out_attr.type == attrib.type); }
                                );
                                out_attr.value = it != std::end(attributes) ? it->value : VA_ATTRIB_NOT_SUPPORTED;
                            });

                        return VA_STATUS_SUCCESS;
                    }))
                );

            //VAStatus vaCreateConfig(VADisplay, VAProfile, VAEntrypoint, VAConfigAttrib*, int, VAConfigID*)
            EXPECT_CALL(d, CreateConfig(testing::Eq(profile), testing::Eq(entrypoint),
                    testing::NotNull(), testing::Le(attributes.size()),
                    testing::NotNull()))
                .With(testing::Args<2, 3>(testing::Contains(
                    testing::Truly([&attributes](VAConfigAttrib ca) //[VAConfigAttrib*, int] should be valid for registered attributes
                    {
                        return std::find_if(std::begin(attributes), std::end(attributes),
                            [ca](VAConfigAttrib xa) { return equal(xa, ca); }
                        ) != std::end(attributes);
                    }))))
                .WillRepeatedly(testing::WithArgs<2, 3, 4>(testing::Invoke(
                    [&d, profile, entrypoint, &attributes](VAConfigAttrib* list, int count, VAConfigID* id)
                    {
                        *id = VAConfigID(d.configs.size());
                        auto c = make_config(*id,
                            std::make_tuple(profile, entrypoint,
                                std::make_tuple(attributes.data(), attributes.size())
                            )
                        );

                        d.configs.push_back(c);

                        return VA_STATUS_SUCCESS;
                    }))
                );
        }

        template <typename ...Attributes>
        inline
        typename std::enable_if<
            std::conjunction<std::is_base_of<VAConfigAttrib, Attributes>...>::value
        >::type
        mock_display(display_tag, display& d, std::tuple<VAProfile, VAEntrypoint, std::tuple<Attributes...> > params)
        {
            std::vector<VAConfigAttrib> attributes;
            for_each_tuple(
                [&](auto attribute)
                { attributes.push_back(attribute); },
                std::get<2>(params)
            );

            return mock_display(display_tag{}, d,
                std::make_tuple(
                    std::get<0>(params), std::get<1>(params),
                    std::make_tuple(attributes.data(), attributes.size())
                )
            );
        }

        inline
        void mock_display(display_tag, display& d, std::tuple<VAProfile, VAEntrypoint> params)
        {
            return mock_display(display_tag{}, d,
                std::make_tuple(std::get<0>(params), std::get<1>(params), std::make_tuple(static_cast<VAConfigAttrib*>(nullptr), 0))
            );
        }

    }

} }
