// Copyright (c) 2021 Intel Corporation
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

#include "mfxvideo.h"
#include "mocks/include/mfx/error.h"

namespace mocks { namespace mfx
{
    namespace api
    {
        struct allocator
            : mfxFrameAllocator
        {
            allocator()
            {
                mfxFrameAllocator::Alloc  = _Alloc;
                mfxFrameAllocator::Lock   = _Lock;
                mfxFrameAllocator::GetHDL = _GetHDL;
                mfxFrameAllocator::Unlock = _Unlock;
                mfxFrameAllocator::Free   = _Free;

                pthis = this;
            }

            virtual mfxStatus Alloc(mfxFrameAllocRequest*, mfxFrameAllocResponse*)
            { throw std::system_error(std::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            virtual mfxStatus Lock(mfxMemId, mfxFrameData*)
            { throw std::system_error(std::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            virtual mfxStatus Unlock(mfxMemId, mfxFrameData*)
            { throw std::system_error(std::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            virtual mfxStatus GetHDL(mfxMemId, mfxHDL*)
            { throw std::system_error(std::make_error_code(MFX_ERR_NOT_INITIALIZED)); }
            virtual mfxStatus Free(mfxFrameAllocResponse*)
            { throw std::system_error(std::make_error_code(MFX_ERR_NOT_INITIALIZED)); }

        private:

#define IMPL_FUNC(name, DECL, ARGS) \
            static mfxStatus _##name DECL \
            { \
                return \
                    instance ? static_cast<allocator*>(instance)->name ARGS : MFX_ERR_INVALID_HANDLE; \
            }

            IMPL_FUNC(Alloc,  (mfxHDL instance, mfxFrameAllocRequest* request, mfxFrameAllocResponse* response), (request, response))
            IMPL_FUNC(Lock,   (mfxHDL instance, mfxMemId mid, mfxFrameData* ptr),                                (mid, ptr))
            IMPL_FUNC(Unlock, (mfxHDL instance, mfxMemId mid, mfxFrameData* ptr),                                (mid, ptr))
            IMPL_FUNC(GetHDL, (mfxHDL instance, mfxMemId mid, mfxHDL* handle),                                   (mid, handle))
            IMPL_FUNC(Free,   (mfxHDL instance, mfxFrameAllocResponse* response),                                (response))
#undef IMPL_FUNC
        };
    }

} }
