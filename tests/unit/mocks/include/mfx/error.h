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

#include <system_error>
#include "mfxdefs.h"

namespace mocks { namespace mfx
{
    struct error_category
        : std::error_category
    {
    public:

        const char* name() const noexcept override
        { return "mfx"; }

        std::string message(int code) const noexcept override
        {
            struct code2name
            {
                mfxStatus   code;
                char const* name;
            } const map[] =
            {
                MFX_ERR_UNKNOWN,                  "Unknown error",
                MFX_ERR_NULL_PTR,                 "Null pointer",
                MFX_ERR_UNSUPPORTED,              "Undeveloped feature",
                MFX_ERR_MEMORY_ALLOC,             "Failed to allocate memory",
                MFX_ERR_NOT_ENOUGH_BUFFER,        "Insufficient buffer at input/output",
                MFX_ERR_INVALID_HANDLE,           "Invalid handle",
                MFX_ERR_LOCK_MEMORY,              "Failed to lock the memory block",
                MFX_ERR_NOT_INITIALIZED,          "Member function called before initialization",
                MFX_ERR_NOT_FOUND,                "The specified object is not found",
                MFX_ERR_MORE_DATA,                "Expect more data at input",
                MFX_ERR_MORE_SURFACE,             "Expect more surface at output",
                MFX_ERR_ABORTED,                  "Operation aborted",
                MFX_ERR_DEVICE_LOST,              "Lose the HW acceleration device",
                MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, "Incompatible video parameters",
                MFX_ERR_INVALID_VIDEO_PARAM,      "Invalid video parameters",
                MFX_ERR_UNDEFINED_BEHAVIOR,       "Undefined behavior",
                MFX_ERR_DEVICE_FAILED,            "Device operation failure",
                MFX_ERR_MORE_BITSTREAM,           "Expect more bitstream buffers at output",
                MFX_ERR_GPU_HANG,                 "Device operation failure caused by GPU hang",
                MFX_ERR_REALLOC_SURFACE,          "Bigger output surface required"
            };

            auto i = std::find_if(std::begin(map), std::end(map),
                [code](code2name const& n) { return n.code == code; }
            );

            std::string msg("MFX: ");
            msg +=
                i != std::end(map) ? (*i).name : "Unknown error";

            return msg;
        }
    };

    inline
    std::error_category const& category()
    {
        static const error_category c;
        return c;
    }

    inline
    std::error_code make_error_code(int code)
    { return std::error_code(int(code), category()); }

    inline
    std::error_condition make_error_condition(int code)
    { return std::error_condition(int(code), category()); }
} }

namespace std
{
    template<>
    struct is_error_condition_enum<mfxStatus>
        : public true_type
    {};

    inline
    error_code make_error_code(mfxStatus sts)
    { return mocks::mfx::make_error_code(sts); }

    inline
    error_condition make_error_condition(mfxStatus sts)
    { return mocks::mfx::make_error_condition(sts); }
}
