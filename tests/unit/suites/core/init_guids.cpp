// Copyright (c) 2020 Intel Corporation
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

#if defined (MFX_ENABLE_UNIT_TEST_CORE)

#define ENABLE_MFX_INTEL_GUID_ENCODE
#define ENABLE_MFX_INTEL_GUID_DECODE
#define ENABLE_MFX_INTEL_GUID_PRIVATE

#define MFX_ENABLE_HEVCE_SCC
#define MFX_ENABLE_AV1_VIDEO_DECODE
#define MFX_ENABLE_AV1_VIDEO_ENCODE

#include <initguid.h>
#include "mocks/include/mfx/guids.h"
#include "mocks/include/dxva/traits.h"

#endif //MFX_ENABLE_UNIT_TEST_CORE
