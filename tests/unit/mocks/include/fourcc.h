// Copyright (c) 2019 Intel Corporation
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

#include <type_traits>
#include <string>

#if defined(ENABLE_MOCKS_MFX)
#include "umc_structures.h"
#endif

namespace mocks { namespace fourcc
{
    template <unsigned F>
    struct tag
        : std::integral_constant<unsigned, F>
    {};

    constexpr
    inline unsigned make(char a, char b, char c, char d)
    {
        return
            (unsigned(a) + (unsigned(b) << 8) + (unsigned(c) << 16) + (unsigned(d) << 24));
    }
} }

constexpr
unsigned int operator"" _fourcc(char const* fourcc, std::size_t)
{
    return
        mocks::fourcc::make(fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
}

namespace mocks { namespace fourcc
{
    constexpr auto RGB3 = "RGB3"_fourcc;
    constexpr auto RGB4 = "RGB4"_fourcc;
    constexpr auto RGBA = "RGBA"_fourcc;
    constexpr auto BGR4 = "BGR4"_fourcc;
    constexpr auto ARGB = "ARGB"_fourcc;
    constexpr auto BGRA = "BGRA"_fourcc;

    //420
    constexpr auto YV12 = "YV12"_fourcc;
    constexpr auto YV16 = "YV16"_fourcc;
    constexpr auto NV12 = "NV12"_fourcc;
    constexpr auto NV16 = "NV16"_fourcc;
    constexpr auto P010 = "P010"_fourcc;

    //422
    constexpr auto YUY2 = "YUY2"_fourcc;
    constexpr auto YUYV = "YUYV"_fourcc;
    constexpr auto UYVY = "UYVY"_fourcc;
    constexpr auto Y210 = "Y210"_fourcc;
    constexpr auto Y216 = "Y216"_fourcc;
    constexpr auto P016 = "P016"_fourcc;

    //444
    constexpr auto AYUV = "AYUV"_fourcc;
    constexpr auto Y410 = "Y410"_fourcc;
    constexpr auto Y416 = "Y416"_fourcc;

    constexpr auto P8      = 41;
    constexpr auto R16     = "R16U"_fourcc;
    constexpr auto ARGB16  = "RG16"_fourcc;
    constexpr auto ABGR16  = "BG16"_fourcc;
    constexpr auto A2RGB10 = "RG10"_fourcc;

#if defined(ENABLE_MOCKS_MFX)
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::YV12>)
    { return make('Y', 'V', '1', '2'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::NV12>)
    { return make('N', 'V', '1', '2'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::NV16>)
    { return make('N', 'V', '1', '6'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::IMC3>)
    { return make('I', 'M', 'C', '3'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::YUY2>)
    { return make('Y', 'U', 'Y', '2'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::YUV411>)
    { return make('4', '1', '1', 'P'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::YUV444>)
    { return make('4', '4', '4', 'P'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::YUV444A>)
    { return make('A', 'Y', 'U', 'V'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::AYUV>)
    { return to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::YUV444A>{}); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::RGB24>)
    { return make('R', 'G', 'B', '3'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::RGB32>)
    { return make('R', 'G', 'B', '4'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::P010>)
    { return make('P', '0', '1', '0'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::P016>)
    { return make('P', '0', '1', '6'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::P210>)
    { return make('P', '2', '1', '0'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::Y210>)
    { return make('Y', '2', '1', '0'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::Y216>)
    { return make('Y', '2', '1', '6'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::Y410>)
    { return make('Y', '4', '1', '0'); }
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::Y416>)
    { return make('Y', '4', '1', '6'); }

    template <UMC::ColorFormat F>
    struct format
        : std::integral_constant<UMC::ColorFormat, F>
    {
        using fourcc_t = tag<to_fourcc(std::integral_constant<UMC::ColorFormat, F>{})>;
        static constexpr
        typename fourcc_t::value_type fourcc_v = fourcc_t::value;
    };

    namespace detail
    {
        constexpr
        inline unsigned to_fourcc_impl(UMC::ColorFormat, std::integral_constant<UMC::ColorFormat, UMC::Y416>)
        { return to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::Y416>{}); }

        template <UMC::ColorFormat F>
        constexpr
        inline unsigned to_fourcc_impl(UMC::ColorFormat format, std::integral_constant<UMC::ColorFormat, F>)
        {
            switch (format)
            {
                case F:             return to_fourcc(std::integral_constant<UMC::ColorFormat, F>{});
                default:            return to_fourcc_impl(format, std::integral_constant<UMC::ColorFormat, static_cast<UMC::ColorFormat>(F + 1)>{});

            }
        }
    }

    constexpr
    inline unsigned to_fourcc(UMC::ColorFormat format)
    {
        return detail::to_fourcc_impl(
            format, std::integral_constant<UMC::ColorFormat, UMC::YV12>{}
        );
    }
#endif
} }

namespace std
{
    template <unsigned F>
    inline
    string to_string(mocks::fourcc::tag<F> const& f)
    {
        return
            f.value < '0' ? to_string(f.value) : std::string{ reinterpret_cast<char const*>(&f.value), 4 };
    }
}

namespace mocks
{
    template <unsigned F>
    inline
    std::ostream& operator<<(std::ostream& os, fourcc::tag<F> const& f)
    { return os << std::to_string(f); }
}
