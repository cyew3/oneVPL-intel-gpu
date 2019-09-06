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

#include <atlbase.h>
#include <detours.h>

#include <algorithm>

namespace mocks
{
    template <typename T>
    struct hook
    {
        template <typename Iterator>
        hook(T* object, Iterator begin, Iterator end)
            : current(nullptr)
        {
            std::transform(begin, end, std::back_inserter(functions),
                [&](auto p)
                { return std::make_pair(p.first, p.second); }
            );

            attach(object);
        }

        ~hook()
        {
            detach();
        }

        static
        T* owner()
        { return hook::instance; }
        static
        void owner(T* object)
        { hook::instance = object; }

    private:

        void attach(T* object)
        {
            assert(!current);

            detach();

            DetourRestoreAfterWith();

            auto sts = commit(DetourAttach,
                std::begin(functions), std::end(functions)
            );
            if (sts != NO_ERROR)
                throw std::system_error(sts, std::system_category());

            owner(current = object);
        }

        void detach()
        {
            if (current != owner() || !owner())
                //a new instance of factory hooked target functions
                //don't need to detach 'em
                return;

            commit(DetourDetach,
                std::begin(functions), std::end(functions)
            );

            owner(current = nullptr);
        }

        template <typename F, typename Iterator>
        static
        long commit(F&& f, Iterator begin, Iterator end)
        {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());

            std::for_each(begin, end,
                [&](auto& p) { std::forward<F>(f)(&p.first, p.second); }
            );

            return
                DetourTransactionCommit();
        }

    protected:

        T*                                    current;
        static T*                             instance;

    private:

        std::vector<std::pair<void*, void*> > functions;
    };

    template <typename T>
    __declspec(selectany)
    T* hook<T>::instance = nullptr;
}
