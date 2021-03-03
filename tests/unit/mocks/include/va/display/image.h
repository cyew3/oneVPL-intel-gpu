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

#include <numeric>

bool operator==(VAImageFormat f1, VAImageFormat f2)
{  return mocks::equal(f1, f2); }

namespace mocks { namespace va
{
    namespace detail
    {
        template <typename T, typename ImageFormat, typename F>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_base_of<VAImageFormat, ImageFormat>,
                is_invocable_r<void*, F, VAImageFormat, T, T>
            >::value
        >::type
        mock_display(display_tag, display& d, std::tuple<ImageFormat, T/*width*/, T /*height*/, F /*functor*/> params)
        {
            auto const format = std::get<0>(params);

            //VAStatus vaCreateImage(VADisplay, VAImageFormat*, int, int, VAImage*)
            EXPECT_CALL(d, CreateImage(
                testing::Truly(
                    [format](VAImageFormat* f)
                    { return f && *f == format; }
                ),
                testing::Eq(std::get<1>(params)), testing::Eq(std::get<2>(params)),
                testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<3>(testing::Invoke(
                    [&d, params](VAImage* image)
                    {
                        void *result = std::get<3>(params)(
                            std::get<0>(params),
                            std::get<1>(params),
                            std::get<2>(params)
                        );
                        auto b = make_buffer(&d,
                            std::make_tuple(VAImageBufferType, 0 /*don't care about actual size*/, result)
                        );

                        auto img = make_image(&d,
                            std::make_tuple(b->id,
                                std::get<0>(params),
                                std::get<1>(params), std::get<2>(params)
                            )
                        );
                        *image = img->info;

                        return VA_STATUS_SUCCESS;
                    }))
                );
        }
    }

} }

extern "C"
{
    inline VAStatus vaCreateImage(VADisplay d, VAImageFormat* format, int width, int height, VAImage* image)
    {
        return
            static_cast<mocks::va::display*>(d)->CreateImage(format, width, height, image);
    }
}
