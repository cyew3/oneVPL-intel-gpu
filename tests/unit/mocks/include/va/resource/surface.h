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

namespace mocks { namespace va
{
    namespace detail
    {
        inline
        VASurfaceID make_surface_id(const display& d)
        {
            return VASurfaceID(d.surfaces.size());
        }

        inline
        void mock_surface(surface_tag, surface& s, display* d)
        {
            s.display = d;
            s.id = make_surface_id(*d);
            d->surfaces.push_back(s.shared_from_this());
        }

        template <typename T, typename U>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_integral<U>
            >::value
        >::type
        mock_surface(surface_tag, surface& s, std::tuple<T/*format*/, U /*width*/, U /*height*/> params)
        {
            s.width  = std::get<1>(params);
            s.height = std::get<2>(params);
            s.format = std::get<0>(params);
            // Intentionally don't provide implementation of DeriveImage for this params match
        }

        template <typename T, typename U, typename F>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_integral<U>,
                is_invocable_r<void*, F, T/*format*/, T/*fourcc*/, U/*width*/, U/*height*/>
            >::value
        >::type
        mock_surface(surface_tag, surface& s, std::tuple<T/*format*/, T/*fourcc*/, U /*width*/, U /*height*/, F /*functor*/> params)
        {
            s.width  = std::get<2>(params);
            s.height = std::get<3>(params);
            s.format = std::get<0>(params);
            s.fourcc = std::get<1>(params);
            s.memory_accessor = std::get<4>(params);
            //VAStatus vaDeriveImage(VADisplay, VASurfaceID, VAImage*)
            EXPECT_CALL(s, DeriveImage(testing::Eq(s.id), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1>(testing::Invoke(
                    [&s](VAImage* im)
                    {
                        void* mem = s.memory_accessor(
                            s.format, s.fourcc,
                            s.width, s.height
                        );

                        auto b = make_buffer(s.display,
                            std::make_tuple(VAImageBufferType, 0 /*don't care about actual size*/, mem)
                        );

                        VAImageFormat im_format = {};
                        if(s.format == VA_RT_FORMAT_RGB32_10BPP && s.fourcc == VA_FOURCC_ARGB)
                            im_format.fourcc = VA_FOURCC_A2R10G10B10;
                        else
                            im_format.fourcc = s.fourcc;

                        // TODO: fill byte_order, bits_per_pixel, depth, etc

                        auto img = make_image(s.display,
                            std::make_tuple(b->id,
                                im_format,
                                s.width, s.height
                            )
                        );

                        *im = img->info;
                        return VA_STATUS_SUCCESS;
                    }
                )));
        }

        template <typename T, typename U, typename P>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>,
                std::is_integral<U>,
                std::is_pointer<P>
            >::value
        >::type
        mock_surface(surface_tag, surface& s, std::tuple<T/*format*/, T/*fourcc*/, U /*width*/, U /*height*/, P /*pointer*/> params)
        {
            return mock_surface(surface_tag{}, s,
                std::make_tuple(
                    std::get<0>(params), std::get<1>(params),
                    std::get<2>(params), std::get<3>(params),
                    [p = std::get<4>(params)](T/*format*/, T/*fourcc*/, U /*width*/, U /*height*/)
                    { return p; }
                )
            );
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_surface(surface_tag, surface& s, std::tuple<VASurfaceAttrib*, T> params)
        {
            s.attributes.insert(s.attributes.end(),
                std::get<0>(params), std::get<0>(params) + std::get<1>(params)
            );
        }
    }

} }

extern "C"
{
    inline MOCKS_FORCE_USE_SYMBOL
    VAStatus vaDeriveImage(VADisplay d, VASurfaceID id, VAImage* image)
    {
        return
            static_cast<mocks::va::display*>(d)->DeriveImage(id, image);
    }
}
