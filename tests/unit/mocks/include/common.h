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
#include <tuple>
#include <algorithm>

namespace mocks
{
    namespace detail
    {
        template <typename T, typename U>
        bool equal(T&& l, U&& r)
        {
            return std::equal(
                reinterpret_cast<char const*>(&l),
                reinterpret_cast<char const*>(&l) + sizeof(T),
                reinterpret_cast<char const*>(&r)
            );
        }
    }

    template <typename T, typename U>
    inline
    bool equal(T&& l, U&& r, std::true_type)
    { return detail::equal(std::forward<T>(l), std::forward<U>(r)); }

    template <typename T, typename U>
    inline
    bool equal(T&&, U&&, std::false_type)
    {
        static_assert(false,
            "Arguments have to be either PODs or equal(T&&, U&&, std::true_type) have to be used"
        );
        return false;
    }

    /* Compares two POD struct b2b*/
    template <typename T, typename U>
    inline
    bool equal(T&& l, U&& r)
    {
        return mocks::equal(std::forward<T>(l), std::forward<U>(r),
            std::conjunction<std::is_pod<typename std::decay<T>::type>, std::is_pod<typename std::decay<U>::type> >{}
        );
    }

    namespace detail
    {
        template <typename>
        struct is_tuple : std::false_type {};
        template <typename ...T>
        struct is_tuple<std::tuple<T...> > : std::true_type {};
    }

    template <typename T>
    struct is_tuple : detail::is_tuple<typename std::decay<T>::type> {};

    namespace detail
    {
        template <typename F, typename Tuple, std::size_t... I>
        constexpr
        void for_each_tuple_impl(F&& f, Tuple&& t, std::index_sequence<I...>)
        {
            std::initializer_list<int>{ (std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))), 0)... };
        }
    }

    /* Invokes void F(element) for each element of given tuple */
    template <typename F, typename Tuple>
    constexpr
    typename std::enable_if<is_tuple<Tuple>::value>::type
    for_each_tuple(F&& f, Tuple&& t)
    {
        return detail::for_each_tuple_impl(std::forward<F>(f),
            std::forward<Tuple>(t),
            std::make_index_sequence<std::tuple_size<typename std::remove_reference<Tuple>::type>::value>{}
        );
    }

    /* Invokes F for each element of given tuple */
    template <typename F, typename ...Args>
    constexpr
    void for_each(F&& f, Args&&... args)
    {
        std::initializer_list<int>{ (std::forward<F>(f)(std::forward<Args>(args)), 0)... };
    }

}
