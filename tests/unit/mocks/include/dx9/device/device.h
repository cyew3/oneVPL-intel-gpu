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

#include "mocks/include/dx9/device/query.h"
#include "mocks/include/dx9/winapi/device.hxx"

namespace mocks { namespace dx9
{
    struct device
        : winapi::device_impl
    {
        MOCK_METHOD1(GetDirect3D, HRESULT(IDirect3D9**));
        MOCK_METHOD1(GetCreationParameters, HRESULT(D3DDEVICE_CREATION_PARAMETERS*));
        MOCK_METHOD2(CreateQuery, HRESULT(D3DQUERYTYPE, IDirect3DQuery9**));
    };

    namespace detail
    {
        template <typename T>
        inline
        typename std::enable_if<std::is_base_of<IDirect3D9, T>::value>::type
        mock_device(device& d, T* d3d)
        {
            //HRESULT GetDirect3D(IDirect3D9**)
            EXPECT_CALL(d, GetDirect3D(testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                    [d3d](IDirect3D9** result) { return d3d->QueryInterface(__uuidof(IDirect3D9), reinterpret_cast<void**>(result)); }
                )));
        }

        inline
        void mock_device(device& d, D3DDEVICE_CREATION_PARAMETERS params)
        {
            //HRESULT GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS*)
            EXPECT_CALL(d, GetCreationParameters(testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(params),
                    testing::Return(S_OK)
                ));
        }

        inline
        void mock_device(device& d, D3DQUERYTYPE type)
        {
            //HRESULT CreateQuery(D3DQUERYTYPE, IDirect3DQuery9**)
            EXPECT_CALL(d, CreateQuery(testing::Eq(type), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1>(testing::Invoke(
                    [type](IDirect3DQuery9** result)
                    {
                        *result = make_query(type).release();
                        return S_OK;
                    }
                )));
        }

        template <typename T>
        inline
        void mock_device(device&, T)
        {
            /* If you trap here it means that you passed an argument to [make_device]
                which is not supported by library, you need to overload [mock_device] for that argument's type */
            static_assert(false,
                "Unknown argument was passed to [make_device]"
            );
        }
    }

    template <typename ...Args>
    inline
    device& mock_device(device& d, Args&&... args)
    {
        for_each(
            [&d](auto&& x) { detail::mock_device(d, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return d;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<device>
    make_device(Args&&... args)
    {
        auto d = std::unique_ptr<testing::NiceMock<device> >{
            new testing::NiceMock<device>{}
        };

        using testing::_;

        //HRESULT GetDirect3D(IDirect3D9**)
        EXPECT_CALL(*d, GetDirect3D(_))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS*)
        EXPECT_CALL(*d, GetCreationParameters(_))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT CreateQuery(D3DQUERYTYPE, IDirect3DQuery9**)
        EXPECT_CALL(*d, CreateQuery(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        mock_device(*d, std::forward<Args>(args)...);

        return d;
    }

} }
