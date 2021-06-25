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

#if defined (MFX_ENABLE_UNIT_TEST_MEMORY)

#include "mfx_config.h"

#if defined(MFX_VA_WIN)
    #include "test_memory_windows.h"
#elif defined(MFX_VA_LINUX)
    #include "test_memory_linux.h"
#endif

namespace test
{
    TEST_P(Memory, MapSurfaceNullSurface)
    {
        EXPECT_EQ(
            surface->FrameInterface->Map(nullptr, MFX_MAP_READ),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, MapSurfaceRead)
    {
        EXPECT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ),
            MFX_ERR_NONE
        );
        EXPECT_NE(
            surface->Data.UV,
            nullptr
        );
    }

    TEST_P(Memory, MapSurfaceWrite)
    {
        EXPECT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_WRITE),
            MFX_ERR_NONE
        );

        EXPECT_NE(
            surface->Data.UV,
            nullptr
        );
    }

    TEST_P(Memory, MapSurfaceReadWrite)
    {
        EXPECT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ_WRITE),
            MFX_ERR_NONE
        );

        EXPECT_NE(
            surface->Data.UV,
            nullptr
        );
    }

    TEST_P(Memory, MapSurfaceWrongFlags)
    {
        EXPECT_EQ(
            surface->FrameInterface->Map(surface, 0x42),
            MFX_ERR_LOCK_MEMORY
        );
    }

    TEST_P(Memory, MapReadReadSameThread)
    {
        ASSERT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ),
            MFX_ERR_NONE
        );
    }

    TEST_P(Memory, UnmapSurfaceNullSurface)
    {
        EXPECT_EQ(
            surface->FrameInterface->Unmap(nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, UnmapSurfaceNotMapped)
    {
        EXPECT_EQ(
            surface->FrameInterface->Unmap(surface),
            MFX_ERR_UNDEFINED_BEHAVIOR
        );
    }

    TEST_P(Memory, UnmapSurface)
    {
        ASSERT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            surface->FrameInterface->Unmap(surface),
            MFX_ERR_NONE
        );
        EXPECT_FALSE(
            surface->Data.Y ||
            surface->Data.U ||
            surface->Data.V ||
            surface->Data.A
        );
    }

    TEST_P(Memory, ReleaseSurfaceNullSurface)
    {
        EXPECT_EQ(
            surface->FrameInterface->Release(nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, ReleaseMappedSurface)
    {
        ASSERT_EQ(
            surface->FrameInterface->Map(surface, MFX_MAP_READ),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            surface->FrameInterface->Release(surface),
            MFX_ERR_NONE
        );
    }

    TEST_P(Memory, ReleaseSurface)
    {
        EXPECT_EQ(
            surface->FrameInterface->Release(surface),
            MFX_ERR_NONE
        );
    }

    TEST_P(Memory, GetNativeHandleNullSurface)
    {
        mfxHDL hdl;
        mfxResourceType type;
        EXPECT_EQ(
            surface->FrameInterface->GetNativeHandle(nullptr, &hdl, &type),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetNativeHandleNullResource)
    {
        mfxResourceType type;
        EXPECT_EQ(
            surface->FrameInterface->GetNativeHandle(surface, nullptr, &type),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetNativeHandleNullType)
    {
        mfxResourceType type;
        mfxHDL hdl;
        EXPECT_EQ(
            surface->FrameInterface->GetNativeHandle(surface, &hdl, nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetNativeHandle)
    {
        mfxHDL hdl;
        mfxResourceType type;
        if (GetParam().second & (MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
            EXPECT_EQ(
                surface->FrameInterface->GetNativeHandle(surface, &hdl, &type),
                MFX_ERR_UNSUPPORTED
            );
        else
        {
            EXPECT_EQ(
                surface->FrameInterface->GetNativeHandle(surface, &hdl, &type),
                MFX_ERR_NONE
            );

#if defined(MFX_VA_WIN)
            EXPECT_EQ(
                type, MFX_RESOURCE_DX11_TEXTURE
            );
#elif defined(MFX_VA_LINUX)
            EXPECT_EQ(
                type, MFX_RESOURCE_VA_SURFACE
            );
#endif
        }
    }

    TEST_P(Memory, GetDeviceHandleNullSurface)
    {
        mfxHDL hdl;
        mfxHandleType type;
        EXPECT_EQ(
            surface->FrameInterface->GetDeviceHandle(nullptr, &hdl, &type),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetDeviceHandleNullDevice)
    {
        mfxHandleType type;
        EXPECT_EQ(
            surface->FrameInterface->GetDeviceHandle(surface, nullptr, &type),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetDeviceHandleNullType)
    {
        mfxResourceType type;
        mfxHDL hdl;
        EXPECT_EQ(
            surface->FrameInterface->GetDeviceHandle(surface, &hdl, nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_P(Memory, GetDeviceHandle)
    {
        mfxHDL hdl;
        mfxHandleType type;
        if (GetParam().second & (MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
            EXPECT_EQ(
                surface->FrameInterface->GetDeviceHandle(surface, &hdl, &type),
                MFX_ERR_UNSUPPORTED
            );
        else
        {
            EXPECT_EQ(
                surface->FrameInterface->GetDeviceHandle(surface, &hdl, &type),
                MFX_ERR_NONE
            );

#if defined(MFX_VA_WIN)
            EXPECT_EQ(
                type, MFX_HANDLE_D3D11_DEVICE
            );

            EXPECT_EQ(
                hdl, component->device.p
            );
#elif defined(MFX_VA_LINUX)
            EXPECT_EQ(
                type, MFX_HANDLE_VA_DISPLAY
            );

            EXPECT_EQ(
                hdl, display.get()
            );
#endif
        }
    }

    
}



#endif // MFX_ENABLE_UNIT_TEST_MEMORY
