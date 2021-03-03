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

#include "mocks/include/dx9/winapi/manager.hxx"

namespace mocks { namespace dx9
{
    struct manager
        : winapi::manager_impl
    {
        MOCK_METHOD2(ResetDevice, HRESULT(IDirect3DDevice9*, UINT));
        MOCK_METHOD1(OpenDeviceHandle, HRESULT(HANDLE*));
        MOCK_METHOD1(CloseDeviceHandle, HRESULT(HANDLE));
        MOCK_METHOD1(TestDevice, HRESULT(HANDLE));
        MOCK_METHOD3(LockDevice, HRESULT(HANDLE, IDirect3DDevice9**, BOOL));
        MOCK_METHOD2(UnlockDevice, HRESULT(HANDLE, BOOL));
    };

    namespace detail
    {
        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_manager(manager& m, T token)
        {
            //HRESULT ResetDevice(IDirect3DDevice9*, UINT /*resetToken*/);
            EXPECT_CALL(m, ResetDevice(testing::NotNull(), testing::Eq(token)))
                .WillRepeatedly(testing::Return(S_OK));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_integral<T>::value
        >::type
        mock_manager(manager& m, std::tuple<HANDLE, T /*block*/> params)
        {
            //HRESULT OpenDeviceHandle(HANDLE* /*phDevice*/)
            EXPECT_CALL(m, OpenDeviceHandle(testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<0>(std::get<0>(params)),
                    testing::Return(S_OK)
                ));

            //HRESULT CloseDeviceHandle(HANDLE /*hDevice*/)
            EXPECT_CALL(m, CloseDeviceHandle(testing::Eq(std::get<0>(params))))
                .WillRepeatedly(testing::Return(S_OK));

            //HRESULT TestDevice(HANDLE /*hDevice*/)
            EXPECT_CALL(m, TestDevice(testing::Eq(std::get<0>(params))))
                .WillRepeatedly(testing::Return(S_OK));

            //HRESULT LockDevice(HANDLE /*hDevice*/, IDirect3DDevice9**, BOOL /*fBlock*/)
            EXPECT_CALL(m, LockDevice(
                testing::Eq(std::get<0>(params)), testing::NotNull(), testing::Eq(std::get<1>(params))))
                .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                    [](HANDLE handle, IDirect3DDevice9** result)
                    {
                        auto d = reinterpret_cast<IDirect3DDevice9*>(handle);
                        d->AddRef();
                        *result = d;

                        return S_OK;
                    }
                )));

            //HRESULT UnlockDevice(HANDLE /*hDevice*/, BOOL /*fSaveState*/)
            EXPECT_CALL(m, UnlockDevice(testing::Eq(std::get<0>(params)), testing::Eq(std::get<1>(params))))
                .WillRepeatedly(testing::WithArgs<0>(testing::Invoke(
                    [](HANDLE handle)
                    {
                        auto d = reinterpret_cast<IDirect3DDevice9*>(handle);
                        d->Release();

                        return S_OK;
                    }
                )));
        }

        inline
        void mock_manager(manager& m, HANDLE handle)
        {
            return mock_manager(m,
                std::make_tuple(handle, TRUE)
            );
        }

        template <typename T>
        inline
        typename std::enable_if<
            !std::is_integral<T>::value
        >::type mock_manager(manager&, T)
        {
            /* If you trap here it means that you passed an argument to [make_manager]
                which is not supported by library, you need to overload [mock_manager] for that argument's type */
            static_assert(false,
                "Unknown argument was passed to [make_manager]"
            );
        }
    }

    template <typename ...Args>
    inline
    manager& mock_manager(manager& m, Args&&... args)
    {
        for_each(
            [&m](auto&& x) { detail::mock_manager(m, std::forward<decltype(x)>(x)); },
            std::forward<Args>(args)...
        );

        return m;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<manager>
    make_manager(Args&&... args)
    {
        auto m = std::unique_ptr<testing::NiceMock<manager> >{
            new testing::NiceMock<manager>{}
        };

        using testing::_;

        //HRESULT ResetDevice(IDirect3DDevice9*, UINT /*resetToken*/)
        EXPECT_CALL(*m, ResetDevice(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT OpenDeviceHandle(HANDLE* /*phDevice*/)
        EXPECT_CALL(*m, OpenDeviceHandle(_))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT CloseDeviceHandle(HANDLE /*hDevice*/)
        EXPECT_CALL(*m, CloseDeviceHandle(_))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT TestDevice(HANDLE /*hDevice*/)
        EXPECT_CALL(*m, TestDevice(_))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT LockDevice(HANDLE /*hDevice*/, IDirect3DDevice9**, BOOL /*fBlock*/)
        EXPECT_CALL(*m, LockDevice(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        //HRESULT UnlockDevice(HANDLE /*hDevice*/, BOOL /*fSaveState*/)
        EXPECT_CALL(*m, UnlockDevice(_, _))
            .WillRepeatedly(testing::Return(E_FAIL));

        mock_manager(*m, std::forward<Args>(args)...);

        return m;
    }

} }
