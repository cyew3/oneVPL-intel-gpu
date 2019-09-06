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

#include "mocks/include/hook.h"
#include "mocks/include/unknown.h"

#include "mocks/include/dx12/winapi/device.hxx"

#include <array>

namespace mocks { namespace dx12
{
    struct device
        : winapi::device_impl
    {
        MOCK_METHOD4(CreateDevice, HRESULT(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**));

        MOCK_METHOD3(CreateCommandQueue, HRESULT(const D3D12_COMMAND_QUEUE_DESC*, REFIID, void**));
        MOCK_METHOD3(CreateCommandAllocator, HRESULT(D3D12_COMMAND_LIST_TYPE, REFIID, void**));
        MOCK_METHOD6(CreateCommandList, HRESULT(UINT, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator*, ID3D12PipelineState*, REFIID, void**));
        MOCK_METHOD5(CreateSharedHandle, HRESULT(ID3D12DeviceChild*, const SECURITY_ATTRIBUTES*, DWORD /*Access*/, LPCWSTR /*Name*/, HANDLE*));
        MOCK_METHOD3(OpenSharedHandle, HRESULT(HANDLE, REFIID, void**));
        MOCK_METHOD4(CreateFence, HRESULT(UINT64, D3D12_FENCE_FLAGS, REFIID, void**));

        void detour(IUnknown* a)
        {
            std::shared_ptr<device> mapper(
                this,
                [a](device* instance)
                { hook<device>::owner()->map[a] = instance; }
            );

            if (thunk)
                return;

            if (hook<device>::owner())
                //copy adapters map from current instance
                map = hook<device>::owner()->map;

            std::array<std::pair<void*, void*>, 1> functions{
                std::make_pair(D3D12CreateDevice,  CreateDeviceT),
            };

            thunk.reset(
                new hook<device>(this, std::begin(functions), std::end(functions))
            );
        }

    private:

        std::map<IUnknown*, device*>   map;
        std::shared_ptr<hook<device> > thunk;

        static
        HRESULT CreateDeviceT(IUnknown* adapter, D3D_FEATURE_LEVEL level, REFIID id, void** result)
        {
            auto instance = hook<device>::owner();
            assert(instance && "No device instance but \'D3D12CreateDevice\' is hooked");

            instance = instance->map[adapter];
            return
                instance ? instance->CreateDevice(adapter, level, id, result) : E_FAIL;
        }
    };

    namespace detail
    {
        template <typename T>
        inline
        typename std::enable_if<
            !std::is_base_of<IUnknown, typename std::remove_pointer<T>::type>::value
        >::type
        mock_device(device&, T)
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

        EXPECT_CALL(*d, CreateDevice(_, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        //HRESULT CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC*, REFIID, void**)
        EXPECT_CALL(*d, CreateCommandQueue(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        //HRESULT CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE, REFIID, void**)
        EXPECT_CALL(*d, CreateCommandAllocator(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        //HRESULT CreateCommandList(UINT /*nodeMask*/, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator*, ID3D12PipelineState*, REFIID, void**)
        EXPECT_CALL(*d, CreateCommandList(_, _, _, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        //HRESULT CreateSharedHandle(ID3D12DeviceChild*, const SECURITY_ATTRIBUTES*, DWORD /*Access*/, LPCWSTR /*Name*/, HANDLE*)
        EXPECT_CALL(*d, CreateSharedHandle(_, _, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        //HRESULT OpenSharedHandle(HANDLE, REFIID, void** /*ppvObj*/)
        EXPECT_CALL(*d, OpenSharedHandle(_, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        //HRESULT CreateFence(UINT64 /*InitialValue*/, D3D12_FENCE_FLAGS, REFIID, void**)
        EXPECT_CALL(*d, CreateFence(_, _, _, _))
            .WillRepeatedly(testing::Return(E_FAIL))
        ;

        mock_device(*d, std::forward<Args>(args)...);

        return d;
    }

} }
