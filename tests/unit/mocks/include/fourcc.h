// Copyright (c) 2019-2021 Intel Corporation
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

#include "mocks/include/common.h"

#if defined(ENABLE_MOCKS_MFX)
#include "umc_structures.h"
#endif

namespace mocks { namespace fourcc
{
    constexpr
    inline unsigned make(char a, char b, char c, char d)
    {
        return
            (unsigned(a) + (unsigned(b) << 8) + (unsigned(c) << 16) + (unsigned(d) << 24));
    }

    enum
    {
        CHROMA_SUBSAMPLING_400,
        CHROMA_SUBSAMPLING_420,
        CHROMA_SUBSAMPLING_422,
        CHROMA_SUBSAMPLING_444,
        CHROMA_SUBSAMPLING_411,
        CHROMA_SUBSAMPLING_410,
    };

    template <unsigned>
    struct subsampling;

    template <>
    struct subsampling<CHROMA_SUBSAMPLING_400>
    {
        static size_t constexpr v = size_t(-1);
        static size_t constexpr h = size_t(-1);
    };

    template <>
    struct subsampling<CHROMA_SUBSAMPLING_410>
    {
        static size_t constexpr v = 4;
        static size_t constexpr h = 4;
    };

    template <>
    struct subsampling<CHROMA_SUBSAMPLING_411>
    {
        static size_t constexpr v = 4;
        static size_t constexpr h = 1;
    };

    template <>
    struct subsampling<CHROMA_SUBSAMPLING_420>
    {
        static size_t constexpr v = 2;
        static size_t constexpr h = 2;
    };

    template <>
    struct subsampling<CHROMA_SUBSAMPLING_422>
    {
        static size_t constexpr v = 2;
        static size_t constexpr h = 1;
    };

    template <>
    struct subsampling<CHROMA_SUBSAMPLING_444>
    {
        static size_t constexpr v = 1;
        static size_t constexpr h = 1;
    };
} }

constexpr
unsigned int operator"" _fourcc(char const* fourcc, std::size_t)
{
    return
        mocks::fourcc::make(fourcc[0], fourcc[1], fourcc[2], fourcc[3]);
}

namespace mocks { namespace fourcc
{
    namespace detail
    {
        template <unsigned F, unsigned B, unsigned P, int C>
        struct format_impl
            : std::integral_constant<unsigned, F>
        {
            using bits      = std::integral_constant<unsigned, B>;
            using planes    = std::integral_constant<unsigned, P>;
            using chroma    = std::integral_constant<unsigned, C>;
            using subsample = subsampling<C>;
        };
    };

    template <unsigned F>
    struct format : std::integral_constant<unsigned, F> {};

    template <unsigned F>
    struct format_of
    { using type = format<F>; };

#define MAKE_FOURCC_EX(Name, Format, Bits, Planes, Chroma) \
    constexpr auto Name = Format; \
    template <>                   \
    struct format<Name> : detail::format_impl<Name, Bits, Planes, Chroma> {};

#define MAKE_FOURCC(Name, Bits, Planes, Chroma) \
    MAKE_FOURCC_EX(Name, #Name##_fourcc, Bits, Planes, Chroma)

    MAKE_FOURCC(RGB3,  8, 1, CHROMA_SUBSAMPLING_444)

    MAKE_FOURCC(RGB4,  8, 1, CHROMA_SUBSAMPLING_444)
    MAKE_FOURCC(RGBA,  8, 1, CHROMA_SUBSAMPLING_444)
    MAKE_FOURCC(BGR4,  8, 1, CHROMA_SUBSAMPLING_444)
    MAKE_FOURCC(ARGB,  8, 1, CHROMA_SUBSAMPLING_444)
    MAKE_FOURCC(BGRA,  8, 1, CHROMA_SUBSAMPLING_444)
    MAKE_FOURCC(RGBP,  8, 3, CHROMA_SUBSAMPLING_444)

    //420
    MAKE_FOURCC(YV12,  8, 3, CHROMA_SUBSAMPLING_420)
    MAKE_FOURCC(YV16,  8, 3, CHROMA_SUBSAMPLING_420)
    MAKE_FOURCC(NV12,  8, 2, CHROMA_SUBSAMPLING_420)
    MAKE_FOURCC(NV16,  8, 2, CHROMA_SUBSAMPLING_420)
    MAKE_FOURCC(P010, 10, 2, CHROMA_SUBSAMPLING_420)
    MAKE_FOURCC(P016, 16, 2, CHROMA_SUBSAMPLING_420)

    //422
    MAKE_FOURCC(YUY2,  8, 1, CHROMA_SUBSAMPLING_422)
    MAKE_FOURCC(YUYV,  8, 1, CHROMA_SUBSAMPLING_422)
    MAKE_FOURCC(UYVY,  8, 1, CHROMA_SUBSAMPLING_422)
    MAKE_FOURCC(Y210, 10, 1, CHROMA_SUBSAMPLING_422)
    MAKE_FOURCC(Y216, 16, 1, CHROMA_SUBSAMPLING_422)

    //444
    MAKE_FOURCC(AYUV,  8, 1, CHROMA_SUBSAMPLING_444)
    MAKE_FOURCC(Y410, 10, 1, CHROMA_SUBSAMPLING_444)
    MAKE_FOURCC(Y416, 16, 1, CHROMA_SUBSAMPLING_444)

    MAKE_FOURCC_EX(A2RGB10, "RG10"_fourcc, 10, 1, CHROMA_SUBSAMPLING_444)

    MAKE_FOURCC_EX(R16,    "R16U"_fourcc, 16, 1, CHROMA_SUBSAMPLING_400)
    MAKE_FOURCC_EX(ARGB16, "RG16"_fourcc, 16, 1, CHROMA_SUBSAMPLING_444)
    MAKE_FOURCC_EX(ABGR16, "BG16"_fourcc, 16, 1, CHROMA_SUBSAMPLING_444)
    MAKE_FOURCC_EX(P8,                41,  8, 1, CHROMA_SUBSAMPLING_400)
    MAKE_FOURCC_EX(P8MB,   "P8MB"_fourcc,  8, 1, CHROMA_SUBSAMPLING_400)
    MAKE_FOURCC_EX(RGB565, "RGB2"_fourcc, 16, 1, CHROMA_SUBSAMPLING_444)

    using formats_rgb = pack<
          format<RGB3>
        , format<RGB4>
        , format<RGBA>
        , format<BGR4>
        , format<ARGB>
        , format<BGRA>
        , format<A2RGB10>
        , format<ARGB16>
        , format<ABGR16>
        , format<RGB565>
    >;

    using formats_420 = pack<
          format<YV12>
        , format<YV16>
        , format<NV12>
        , format<NV16>
        , format<P010>
        , format<P016>
    >;

    using formats_422 = pack<
          format<YUY2>
        , format<YUYV>
        , format<UYVY>
        , format<Y210>
        , format<Y216>
    >;

    using formats_444 = pack<
          format<AYUV>
        , format<Y410>
        , format<Y416>
    >;

    using formats_all = cat<
        typename cat<
            typename cat<
                formats_420, formats_422
            >::type, formats_444
        >::type, formats_rgb
    >::type;

    template <typename F, typename... Types>
    inline constexpr
    auto attribute_of(unsigned format, F&& f, pack<Types...>)
    {
        return invoke_if(
            std::forward<F>(f),
            [format](auto&& xf) { return xf == format; },
            pack<Types...>{}
        );
    }

    template <typename F>
    inline constexpr
    auto attribute_of(unsigned format, F&& f)
    {
        return attribute_of(
            format,
            std::forward<F>(f),
            formats_all{}
        );
    }

    inline constexpr
    unsigned bits_of(unsigned format)
    {
        return attribute_of(
            format,
            [](auto&& f)
            { return std::decay<decltype(f)>::type::bits::value; }
        );
    };

    inline constexpr
    unsigned chroma_of(unsigned format)
    {
        return attribute_of(
            format,
            [](auto&& f)
            { return std::decay<decltype(f)>::type::chroma::value; }
        );
    };

    inline constexpr
    unsigned planes_of(unsigned format)
    {
        return attribute_of(
            format,
            [](auto&& f)
            { return std::decay<decltype(f)>::type::planes::value; }
        );
    };

#if defined(ENABLE_MOCKS_MFX)

    /* stub for unknown/unsupported color formats */
    template <UMC::ColorFormat F>
    constexpr
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, F>)
    { return 0; }

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
    inline unsigned to_fourcc(std::integral_constant<UMC::ColorFormat, UMC::P216>)
    { return make('P', '2', '1', '6'); }
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
                case F:  return to_fourcc(std::integral_constant<UMC::ColorFormat, F>{});
                default: return to_fourcc_impl(format, std::integral_constant<UMC::ColorFormat, static_cast<UMC::ColorFormat>(F + 1)>{});
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
    string to_string(mocks::fourcc::format<F> const& f)
    {
        return
            f.value < '0' ? to_string(f.value) : std::string{ reinterpret_cast<char const*>(&f.value), 4 };
    }
}

namespace mocks
{
    template <unsigned F, unsigned C>
    inline
    std::ostream& operator<<(std::ostream& os, fourcc::format<F> const& f)
    { return os << std::to_string(f); }
}
