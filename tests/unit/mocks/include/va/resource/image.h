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
#include "mocks/include/va/resource/traits.h"

namespace mocks { namespace va
{
    namespace detail
    {
        inline
        VAImageID make_image_id(const display& d)
        {
            assert(
                d.images.size() < std::numeric_limits<unsigned short>::max()
                && "Too many images"
            );
            return VAImageID(d.images.size());
        }

        inline
        void mock_image(image_tag, image& i, display* d)
        {
            i.display = d;
            i.id = make_image_id(*d);
            d->images.push_back(i.shared_from_this());
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::conjunction<
                std::is_integral<T>
            >::value
        >::type
        mock_image(image_tag, image& i, std::tuple<VABufferID, VAImageFormat, T/*width*/, T /*height*/> params)
        {
            i.info.format   = std::get<1>(params);
            i.info.buf      = std::get<0>(params);
            i.info.width    = std::get<2>(params);
            i.info.height   = std::get<3>(params);
            fill_offsets(i.info);
        }
    }

} }

extern "C"
{
    inline VAStatus vaDestroyImage(VADisplay d, VAImageID id)
    {
        return
            static_cast<mocks::va::display*>(d)->DestroyImage(id);
    }
}
