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

#include <unknwn.h>

#include "mocks/include/common.h"

namespace mocks
{
    namespace detail
    {
        /* Need metafunction to separate types directly assosiated w/ GUID (declared w/ [__declspec(uuid(x))]) */
        template <typename T, typename Enable = void>
        struct remove_impl
        { using type = T; };
        template <typename T>
        struct remove_impl<T, typename std::enable_if<std::is_base_of<IUnknown, typename T::type>::value>::type>
        { using type = typename T::type; };
    }

    template <typename T>
    GUID uuidof()
    { return __uuidof(typename detail::remove_impl<T>::type); }

    template <typename ...T>
    struct unknown_impl
        : T...
    {
        using base_type = unknown_impl<T...>;

        IUnknown* owner = nullptr;
        ULONG counter   = 1;

        virtual ~unknown_impl() = default;

        template <typename ...Args>
        unknown_impl(Args&&... args);
        template <typename ...Args>
        unknown_impl(IUnknown* owner, Args&&... args);

        MOCK_METHOD0(AddRef, ULONG());
        MOCK_METHOD0(Release, ULONG());
        MOCK_METHOD2(QueryInterface, HRESULT(REFIID, void**));
    };

    namespace detail
    {
        template <typename ...T, typename U>
        inline
        typename std::enable_if<std::is_base_of<IUnknown, U>::value>::type
        mock_unknown(unknown_impl<T...>& u, U* ptr, REFGUID id)
        {
            EXPECT_CALL(u, AddRef())
                .WillRepeatedly(testing::Invoke([&u]() { return ++u.counter; }));

            EXPECT_CALL(u, Release())
                .WillRepeatedly(testing::Invoke(
                    [&u]()
                    {
                        assert(u.counter);

                        auto r = --u.counter;
                        if (!r) delete &u;
                        return r;
                    }));

            EXPECT_CALL(u, QueryInterface(testing::Eq(id), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1>(testing::Invoke(
                    [ptr](void** result)
                    {
                        *result = ptr;
                        ptr->AddRef();

                        return S_OK;
                    }
                )));
        }

        template <typename ...T, typename U>
        inline
        void mock_unknown(unknown_impl<T...>&, U)
        {
            /* If you trap here it means that you passed an argument to [unknown_impl] ctor */
            static_assert(false,
                "Unknown argument was passed to [unknown_impl ctor]"
            );
        }
    }

    /* Use as mock_unknown(unk, obj, GUIDs...) */
    template <typename ...T, typename U, typename ...Args>
    inline
    unknown_impl<T...>& mock_unknown(unknown_impl<T...>& u, U* obj, Args&&... args)
    {
        for_each(
            [&](auto p) { detail::mock_unknown(u, p.first, p.second); },
            std::make_pair(obj, std::forward<Args>(args))...
        );

        return u;
    }

    /* Use as mock_unknown<Interface1, Interface1, ...>(unk) */
    template <typename ...U, typename ...T>
    inline
    unknown_impl<T...>& mock_unknown(unknown_impl<T...>& u)
    {
        for_each(
            [&](auto p) { mock_unknown(u, p.first, p.second); },
            std::make_pair(static_cast<U*>(&u), uuidof<U>())...
        );

        return u;
    }

    /* Use as mock_unknown(unknown_impl u, unknown_impl other) - make 'u' to support all interfaces from 'other' */
    template <typename ...T, typename ...U>
    inline
    unknown_impl<T...>& mock_unknown(unknown_impl<T...>& u, unknown_impl<U...>& other)
    {
        return mock_unknown(u, &other,
            uuidof<U>()...
        );
    }

    /* Make all IUnknown's methods of given 'unknown_impl' go to give 'outer' object */
    template <typename ...T, typename U>
    inline
    unknown_impl<T...>& mock_contained(unknown_impl<T...>& u, U* outer)
    {
        assert(!u.owner);
        assert(u.counter);

        //steal object's reference counter
        ULONG counter = 1;
        std::swap(counter, u.counter);
        for (ULONG i = 1; i < counter; ++i)
            outer->AddRef();

        EXPECT_CALL(u, AddRef())
            .WillRepeatedly(testing::Invoke([outer]() { return outer->AddRef(); }));

        EXPECT_CALL(u, Release())
            .WillRepeatedly(testing::Invoke([outer]() { return outer->Release(); }));

        EXPECT_CALL(u, QueryInterface(testing::_, testing::_))
            .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                [outer](REFGUID id, void** result) { return outer->QueryInterface(id, result); }
            )));

        return u;
    }

    template <typename ...T>
    template <typename ...Args>
    inline
    unknown_impl<T...>::unknown_impl(IUnknown* owner, Args&&... args)
        : owner(owner)
    {
        EXPECT_CALL(*this, QueryInterface(testing::_, testing::_))
            .WillRepeatedly(testing::Return(E_NOINTERFACE));

        for_each(
            [this](auto p) { detail::mock_unknown(*this, p.first, p.second); },
            std::make_pair(this, IID_IUnknown),
            std::make_pair(static_cast<T*>(this), uuidof<T>())..., //mock all Ts from 'unknown_impl<T...>'
            std::make_pair(this, std::forward<Args>(args))...      //mock all GUIDs passed as 'args'
        );

        if (owner)
            //if object has an owner - it is contained object, need mock all its IUnknown's methods to go to owner (outer object)
            mock_contained(*this, owner);
    }

    template <typename ...T>
    template <typename ...Args>
    inline
    unknown_impl<T...>::unknown_impl(Args&&... args)
        : unknown_impl<T...>(static_cast<IUnknown*>(nullptr), std::forward<Args>(args)...)
    {}

}
