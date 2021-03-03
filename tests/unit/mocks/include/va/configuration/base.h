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
    struct display;
    struct configuration
        : std::enable_shared_from_this<configuration>
    {
        MOCK_METHOD4(QueryConfigAttributes, VAStatus(VAProfile*, VAEntrypoint*, VAConfigAttrib*, int*));
        MOCK_METHOD2(QuerySurfaceAttributes, VAStatus(VASurfaceAttrib*, unsigned int*));

        struct display*              display = nullptr;
        VAConfigID                   id = VA_INVALID_ID;

        VAProfile                    profile = VAProfileNone;
        VAEntrypoint                 entrypoint = VAEntrypoint(0);
        std::vector<VAConfigAttrib>  attributes;
        std::vector<VASurfaceAttrib> surface_attributes;
    };

    namespace detail
    {
        //Need for ADL
        struct configuration_tag {};

        template <typename T>
        inline
        void mock_config(configuration_tag, configuration&, T)
        {
            /* If you trap here it means that you passed an argument (tuple's element) to [make_display]
                which is not supported by library, you need to overload [make_display] for that argument's type */
            static_assert(
                std::conjunction<std::false_type, std::bool_constant<sizeof(T) != 0> >::value,
                "Unknown argument was passed to [make_config]"
            );
        }

        template <typename ...Args>
        inline
        void dispatch(configuration& c, Args&&... args)
        {
            for_each(
                [&c](auto&& x) { mock_config(configuration_tag{}, c, std::forward<decltype(x)>(x)); },
                std::forward<Args>(args)...
            );
        }
    }

    template <typename ...Args>
    inline
    configuration& mock_config(configuration& c, Args&&... args)
    {
        detail::dispatch(c, std::forward<Args>(args)...);

        return c;
    }

    template <typename ...Args>
    inline
    std::shared_ptr<configuration>
    make_config(Args&&... args)
    {
        auto c = std::shared_ptr<testing::NiceMock<configuration> >{
            new testing::NiceMock<configuration>{}
        };

        using testing::_;

        EXPECT_CALL(*c, QueryConfigAttributes(_, _, _, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        EXPECT_CALL(*c, QuerySurfaceAttributes(_, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        EXPECT_CALL(*c, QuerySurfaceAttributes(_, _))
            .WillRepeatedly(testing::Return(VA_STATUS_ERROR_INVALID_PARAMETER));

        EXPECT_CALL(*c, QuerySurfaceAttributes(testing::IsNull(), testing::NotNull()))
            .WillRepeatedly(testing::WithArgs<1>(testing::Invoke(
                [c = c.get()](unsigned int* count)
                {
                    *count = static_cast<unsigned int>(c->surface_attributes.size());

                    return VA_STATUS_SUCCESS;
                }))
            );

        EXPECT_CALL(*c, QuerySurfaceAttributes(testing::NotNull(), testing::NotNull()))
            .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                [c = c.get()](VASurfaceAttrib* attributes, unsigned int* count)
                {
                    *count = static_cast<unsigned int>(c->surface_attributes.size());
                    std::copy(std::begin(c->surface_attributes), std::end(c->surface_attributes), attributes);

                    return VA_STATUS_SUCCESS;
                }))
            );

        mock_config(*c, std::forward<Args>(args)...);

        return c;
    }
}
    template <>
    struct default_result<std::shared_ptr<va::configuration> >
    {
        using type = int;
        static constexpr type value = VA_STATUS_ERROR_INVALID_CONFIG;
        constexpr type operator()() const noexcept
        { return value; }
    };
}
