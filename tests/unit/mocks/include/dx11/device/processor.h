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

#include "mocks/include/dx11/device/base.h"
#include "mocks/include/dx11/processor/enumerator.h"
#include "mocks/include/dx11/processor/processor.h"

namespace mocks { namespace dx11
{
    namespace detail
    {
        inline
        void mock_device(device& d, ID3D11VideoProcessorEnumerator* pe)
        {
            d.enumerator = pe;
            if (!d.enumerator)
                //create default enumerator and take ownership here
                d.enumerator.Attach(static_cast<ID3D11VideoProcessorEnumerator*>(make_enum().release()));

            //HRESULT CreateVideoProcessorEnumerator(const D3D11_VIDEO_PROCESSOR_CONTENT_DESC*, ID3D11VideoProcessorEnumerator**)
            EXPECT_CALL(d, CreateVideoProcessorEnumerator(testing::NotNull(), testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
                    [pe](D3D11_VIDEO_PROCESSOR_CONTENT_DESC const* xd, ID3D11VideoProcessorEnumerator** result)
                    {
                        D3D11_VIDEO_PROCESSOR_CONTENT_DESC cd;
                        return
                            SUCCEEDED(pe->GetVideoProcessorContentDesc(&cd)) &&
                            equal(*xd, const_cast<D3D11_VIDEO_PROCESSOR_CONTENT_DESC const&>(cd)) ?
                            pe->QueryInterface(result) : E_FAIL;
                    }
                )));

            //HRESULT CreateVideoProcessor(ID3D11VideoProcessorEnumerator*, UINT, ID3D11VideoProcessor**)
            EXPECT_CALL(d, CreateVideoProcessor(testing::Eq(pe), testing::_, testing::NotNull()))
                .WillRepeatedly(testing::WithArgs<0, 1, 2>(testing::Invoke(
                    [](ID3D11VideoProcessorEnumerator* pe, UINT index, ID3D11VideoProcessor** result)
                    {
                        D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rc;
                        HRESULT hr = pe->GetVideoProcessorRateConversionCaps(index, &rc);
                        if (FAILED(hr)) return hr;

                        *result = make_processor(rc).release();
                        return S_OK;
                    }
                )));
        }
    }

} }
