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

#include "mocks/include/dx12/device/base.h"

namespace mocks { namespace dx12
{
    namespace detail
    {
        template <typename T>
        inline
        typename std::enable_if<
            std::is_base_of<IUnknown, T>::value
        >::type
        mock_device(device& d, std::tuple<T*, D3D_FEATURE_LEVEL, GUID> params)
        {
            d.detour(std::get<0>(params));

            using testing::_;

            //HRESULT D3D12CreateDevice(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID, void**);
            EXPECT_CALL(d, CreateDevice(testing::Eq(std::get<0>(params)), testing::Le(std::get<1>(params)),
                testing::Eq(std::get<2>(params)), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<3>(testing::Invoke(
                    [&d](void** result)
                    {
                        d.AddRef();
                        *result = &d;

                        return S_OK;
                    }
                )));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_base_of<IUnknown, T>::value
        >::type
        mock_device(device& d, std::tuple<T*, D3D_FEATURE_LEVEL> params)
        {
            return mock_device(d,
                std::tuple_cat(params, std::make_tuple(uuidof<ID3D12Device>()))
            );
        }

        inline
        void mock_device(device& d, IUnknown* adapter)
        {
            return mock_device(d,
                std::make_tuple(adapter, D3D_FEATURE_LEVEL_12_0)
            );
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_base_of<ID3D12CommandQueue, T>::value
        >::type mock_device(device& d, std::tuple<D3D12_COMMAND_QUEUE_DESC, T*> params)
        {
            //HRESULT CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC*, REFIID, void**)
            EXPECT_CALL(d, CreateCommandQueue(
                testing::AllOf(
                    testing::NotNull(),
                    testing::Truly([params](D3D12_COMMAND_QUEUE_DESC const* xd) { return equal(*xd, std::get<0>(params)); })
                ),
                testing::_, testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
                    [queue = std::get<1>(params)](REFIID id, void** result)
                    { return queue ? queue->QueryInterface(id, result) : E_FAIL; }
                )));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_base_of<ID3D12CommandAllocator, T>::value
        >::type mock_device(device& d, std::tuple<D3D12_COMMAND_LIST_TYPE, T*> params)
        {
            //HRESULT CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE, REFIID, void**)
            EXPECT_CALL(d, CreateCommandAllocator(testing::Eq(std::get<0>(params)), testing::_, testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
                    [allocator = std::get<1>(params)](REFIID id, void** result)
                    { return allocator->QueryInterface(id, result); }
                )));
        }

        template <typename T, typename U, typename V>
        inline
        typename std::enable_if<std::conjunction<
            std::is_integral<T>,
            std::is_base_of<ID3D12CommandAllocator, U>,
            std::is_base_of<ID3D12CommandList, V> >::value
        >::type
        mock_device(device& d, std::tuple<T, D3D12_COMMAND_LIST_TYPE, U*, V*> params)
        {
            //HRESULT CreateCommandList(UINT /*nodeMask*/, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator*, ID3D12PipelineState*, REFIID, void**)
            EXPECT_CALL(d, CreateCommandList(
                testing::Eq(UINT(std::get<0>(params))),
                testing::Eq(std::get<1>(params)),
                testing::Eq(std::get<2>(params)),
                testing::_, //ID3D12PipelineState is optional
                testing::_,
                testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<4, 5>(testing::Invoke(
                    [list = std::get<3>(params)](REFIID id, void** result)
                    { return list ? list->QueryInterface(id, result) : E_FAIL; }
                )));
        }

        template <typename T /*value*/, typename U /* flags*/, typename V /* fence */>
        inline
        typename std::enable_if<std::conjunction<
            std::is_integral<T>,
            std::disjunction<std::is_integral<U>, std::is_same<U, D3D12_FENCE_FLAGS> >,
            std::is_base_of<ID3D12Fence, V> >::value
        >::type
        mock_device(device& d, std::tuple<T, U, V*> params)
        {
            //HRESULT CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID, void**)
            EXPECT_CALL(d, CreateFence(testing::Eq(UINT64(std::get<0>(params))), testing::Eq(std::get<1>(params)),
                testing::_, testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<2, 3>(testing::Invoke(
                    [fence = std::get<2>(params)](REFIID id, void** result)
                    {
                        return
                            fence ? fence->QueryInterface(id, result) : E_FAIL;
                    }
                )));
        }

        template <typename T>
        inline
        typename std::enable_if<
            std::is_base_of<ID3D12DeviceChild, T>::value
        >::type
        mock_device(device& d, std::tuple<T*, SECURITY_ATTRIBUTES, LPCWSTR> params)
        {
            //HRESULT CreateSharedHandle(ID3D12DeviceChild*, const SECURITY_ATTRIBUTES*, DWORD /*Access*/, LPCWSTR /*Name*/, HANDLE*)
            EXPECT_CALL(d, CreateSharedHandle(
                testing::Eq(std::get<0>(params)),
                testing::AnyOf(
                    testing::IsNull(),
                    testing::AllOf(
                        testing::NotNull(),
                        testing::Truly([params](SECURITY_ATTRIBUTES const* xa) { return equal(*xa, std::get<1>(params)); })
                    )
                ),
                testing::Eq(DWORD(GENERIC_ALL)),
                testing::AnyOf(
                    testing::AllOf(testing::NotNull(), testing::StrEq(std::get<2>(params))),
                    testing::IsNull()
                ),
                testing::NotNull()))
                .WillRepeatedly(testing::DoAll(
                    testing::SetArgPointee<4>(HANDLE(&d)),
                    testing::Return(S_OK)
                ));
        }

        inline
        void mock_device(device& d, std::tuple<HANDLE, GUID> params)
        {
            //HRESULT OpenSharedHandle(HANDLE, REFIID, void** /*ppvObj*/)
            EXPECT_CALL(d, OpenSharedHandle(
                testing::Eq(std::get<0>(params)),
                testing::Eq(std::get<1>(params)),
                testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 2>(testing::Invoke(
                    [](HANDLE handle, void** result)
                    {
                        *result = handle;
                        return S_OK;
                    }
                )));
        }
    }

    inline
    std::unique_ptr<device>
    make_device()
    {
        return
            make_device(static_cast<IUnknown*>(nullptr));
    }

} }
