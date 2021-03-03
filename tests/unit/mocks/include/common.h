// Copyright (c) 2019-2020 Intel Corporation
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
        /* 'std::unwrap_reference' is available since C++20 */
        template <typename T>
        struct unwrap_reference
        { using type = T; };

        template <typename T>
        struct unwrap_reference<std::reference_wrapper<T> >
        { using type = T&; };

        template <typename T, typename U>
        constexpr inline
        bool equal(T&& l, U&& r)
        {
            typename unwrap_reference<typename std::decay<T>::type>::type
                lu(std::forward<T>(l));
            typename unwrap_reference<typename std::decay<U>::type>::type
                ru(std::forward<U>(r));

            return std::equal(
                reinterpret_cast<char const*>(&lu),
                reinterpret_cast<char const*>(&lu) + sizeof(lu),
                reinterpret_cast<char const*>(&ru)
            );
        }
    }

    template <typename T, typename Enable = void>
    struct is_b2b_comparable
        : std::false_type
    {};

    template <typename T>
    struct is_b2b_comparable<
        T, typename std::enable_if<std::is_pod<T>::value>::type
    > : std::true_type
    {};

    template <typename T>
    struct is_b2b_comparable<std::reference_wrapper<T> >
        : is_b2b_comparable<typename std::reference_wrapper<T>::type >
    {};

    template <typename T, typename U, typename Bool>
    constexpr inline
    bool equal(T&&, U&&, Bool)
    {
        //TODO: g++ issues compilation error here, even when no 'equal' invokation exists, it is enough to include <common.h> WTF???
        static_assert(std::conjunction<Bool>::value,
            "Arguments have to be either PODs or equal(T&&, U&&, std::true_type) have to be used"
        );

        return false;
    }

    template <typename T, typename U>
    constexpr inline
    bool equal(T&& l, U&& r, std::true_type)
    {
        return detail::equal(
            std::forward<T>(l), std::forward<U>(r)
        );
    }

    /* Compares two POD struct b2b*/
    template <typename T, typename U>
    constexpr inline
    bool equal(T&& l, U&& r)
    {
        return mocks::equal(std::forward<T>(l), std::forward<U>(r),
            std::bool_constant<std::conjunction<
                typename is_b2b_comparable<typename std::decay<T>::type>::type,
                typename is_b2b_comparable<typename std::decay<U>::type>::type
            >::value>{}
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

    /* A trick to compiler's restriction of calling overloaded function:
       ---------------------
       void foo(int);
       void foo(char);
       template <typename F>
       void call(F f);
       call(foo); <--- compiler doesn't know which 'foo' overload we want call.
       ---------------------
       So, use a trick to 'lift' the overloading resolution into a lambda's templated operator():
     */
    //([](auto&&... args) -> decltype(F(std::forward<decltype(args)>(args)...))
    #define MOCKS_LIFT(F) \
        ([](auto&&... args) -> decltype(auto) \
        { return F(std::forward<decltype(args)>(args)...); })

    /* Defines default value that is returned by 'invoke_if' if no element found in give range */
    template <typename T>
    struct default_result
    {
        using type = T;
        static constexpr type value = type{};
        constexpr type operator()() const noexcept
        { return value; }
    };

    template <>
    struct default_result<void>
    {
        using type = void;
        constexpr type operator()() const noexcept
        {}
    };

    /* Invokes 'F(*iterator)' if element in the range [begin, end) satisfies specific criteria 'P(*iterator)' */
    template <typename Iterator, typename F, typename P>
    inline constexpr
    auto invoke_if(Iterator begin, Iterator end, F&& f, P&& p) ->
        typename std::result_of<
            F(typename std::iterator_traits<Iterator>::value_type)
        >::type
    {
        auto x = std::find_if(begin, end, std::forward<P>(p));

        return x != end ?
            std::forward<F>(f)(*x) :
            default_result<
                typename std::iterator_traits<Iterator>::value_type
            >{}();
        ;
    }

    template <typename Container, typename F, typename P>
    inline constexpr
    auto invoke_if(Container&& c, F&& f, P&& p)
    {
        return invoke_if(
            std::begin(c), std::end(c),
            std::forward<F>(f), std::forward<P>(p)
        );
    }

    namespace detail
    {
        template <typename, typename...>
        struct contains;

        template <typename T>
        struct contains<T> : std::false_type {};
        template <typename T, typename... Rest>
        struct contains<T, T, Rest...> : std::true_type {};

        template <typename T, typename U, typename... Rest>
        struct contains<T, U, Rest...>
            : contains<T, Rest...>
        {};
    }

    /* 'std::true' if type [T] is found in given 'Types' sequence, 'std::false' otherwise */
    template <typename T, typename... Types>
    struct contains
        : detail::contains<T, Types...>
    {};

    template <typename... Types>
    struct pack
    {
        //using type = Types...;
        static constexpr auto value = sizeof...(Types);
    };

    template <typename T, typename... Types>
    struct contains<T, pack<Types...> >
        : contains<T, Types...>
    {};

    template <typename... Types>
    struct cat;
    template <typename... Types1, typename... Types2>
    struct cat<pack<Types1...>, pack<Types2...> >
    {
        using type = pack<Types1..., Types2...>;
    };

    namespace detail
    {
        template <typename R, typename F, typename P, typename... Args>
        inline constexpr
        R invoke_if(F&&, P&&, pack<>, Args&&...)
        {
            //end of recursion over 'pack', return default value
            return default_result<R>{}();
        }

        template <typename R, typename F, typename P, typename T, typename... Types, typename... Args>
        inline constexpr
        R invoke_if(F&& f, P&& p, pack<T, Types...>, Args&&... args)
        {
            return std::forward<P>(p)(T{}) ?
                std::forward<F>(f)(T{}, std::forward<Args>(args)...) :
                invoke_if<R>(
                    std::forward<F>(f),
                    std::forward<P>(p),
                    pack<Types...>{},
                    std::forward<Args>(args)...
                );
        }
    }

    /* Invokes 'F(Type, args...)' if element in the pack<Types...> satisfies specific criteria 'P(Type)' */
    template <typename F, typename P, typename T, typename... Types, typename... Args>
    inline constexpr
    auto invoke_if(F&& f, P&& p, pack<T, Types...>, Args&&... args) -> decltype(auto)
    {
        using R = decltype(
            std::forward<F>(f)(T{}, std::forward<Args>(args)...)
        );

        return detail::invoke_if<R>(
            std::forward<F>(f),
            std::forward<P>(p),
            pack<T, Types...>{},
            std::forward<Args>(args)...
        );
    }

    /* specialize for empty 'pack' */
    template <typename F, typename P, typename... Args>
    inline constexpr
    void invoke_if(F&&, P&&, pack<>, Args&&...)
    {}
}
