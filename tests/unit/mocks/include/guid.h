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
#include <guiddef.h>

namespace mocks
{
    template <GUID const* id>
    struct guid
        : std::integral_constant<GUID const*, id>
    {};
}

namespace std
{
    template <GUID const* G>
    inline
    string to_string(mocks::guid<G> g)
    {
        constexpr REFGUID id = *g.value;

        string s; s.resize(64);
        snprintf(&s[0], s.size(),
            "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            id.Data1,    id.Data2,    id.Data3,
            id.Data4[0], id.Data4[1], id.Data4[2],
            id.Data4[3], id.Data4[4], id.Data4[5],
            id.Data4[6], id.Data4[7]
        );

        return s;
    }
}

namespace mocks
{
    template <GUID const* G>
    inline
    std::ostream& operator<<(std::ostream& os, guid<G> g)
    { return os << std::to_string(g); }
}
