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

#include "child.hxx"

namespace mocks { namespace dx12 { namespace winapi
{
    struct queue_impl
        : child_impl<unknown_impl<ID3D12CommandQueue> >
    {
        queue_impl()
        {
            mock_unknown<ID3D12Pageable>(*this);
        }

        void UpdateTileMappings(ID3D12Resource*, UINT /*NumResourceRegions*/, const D3D12_TILED_RESOURCE_COORDINATE*, const D3D12_TILE_REGION_SIZE*,
            ID3D12Heap*, UINT /*NumRanges*/, const D3D12_TILE_RANGE_FLAGS*, const UINT* /*pHeapRangeStartOffsets*/, const UINT* /*pRangeTileCounts*/, D3D12_TILE_MAPPING_FLAGS) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void CopyTileMappings(ID3D12Resource*, const D3D12_TILED_RESOURCE_COORDINATE* /*pDstRegionStartCoordinate*/, ID3D12Resource*,
            const D3D12_TILED_RESOURCE_COORDINATE* /*pSrcRegionStartCoordinate*/, const D3D12_TILE_REGION_SIZE* , D3D12_TILE_MAPPING_FLAGS /*Flags*/)
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void ExecuteCommandLists(UINT /*NumCommandLists*/, ID3D12CommandList* const*)
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void SetMarker(UINT /*Metadata*/, const void* /*pData*/, UINT /*Size*/)
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void BeginEvent(UINT /*Metadata*/, const void* /*pData*/, UINT /*Size*/)
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void EndEvent()
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT Signal(ID3D12Fence*, UINT64)
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT Wait(ID3D12Fence*, UINT64)
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetTimestampFrequency(UINT64*)
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetClockCalibration(UINT64* /*pGpuTimestamp*/, UINT64* /*pCpuTimestamp*/)
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        D3D12_COMMAND_QUEUE_DESC GetDesc()
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };
 
} } }
