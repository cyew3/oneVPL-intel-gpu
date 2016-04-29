/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mf_utils.h"
#ifdef CM_COPY_RESOURCE

#include "mf_cm_mem_copy.h"
#include "mf_utest_iunknown_helper.h"
#include "d3d11_allocator.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

struct D3D11DeviceCreator : public ::testing::Test {

    D3D11DeviceCreator () {
    }
    CComPtr<ID3D11Device> CreateDevice(){
        CComPtr<ID3D11Device> m_pD3D11Device;
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
        D3D_FEATURE_LEVEL featureLevel;
        HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevels, _countof(featureLevels), D3D11_SDK_VERSION, &m_pD3D11Device, &featureLevel, NULL);
        EXPECT_EQ(S_OK, hr);
        return m_pD3D11Device;
    }
};

TEST_F(D3D11DeviceCreator, CM_COPY_factory_should_create_same_object_on_second_call_dx11) {
    CMCopyFactory factory;
    CComPtr<ID3D11Device> device(CreateDevice());
    CmCopyWrapper<ID3D11Device>& wrapper1 = factory.CreateCMCopyWrapper(device);
    CmCopyWrapper<ID3D11Device>& wrapper2 = factory.CreateCMCopyWrapper(device);

    EXPECT_EQ(&wrapper1, &wrapper2);
}

TEST_F(D3D11DeviceCreator, CM_COPY_factory_should_create_different_object_for_different_device) {
    CMCopyFactory factory;
    CComPtr<ID3D11Device> device1(CreateDevice());
    CComPtr<ID3D11Device> device2(CreateDevice());

    CmCopyWrapper<ID3D11Device>& wrapper1 = factory.CreateCMCopyWrapper(device1);
    CmCopyWrapper<ID3D11Device>& wrapper2 = factory.CreateCMCopyWrapper(device2);

    EXPECT_NE(&wrapper1, &wrapper2);
}

TEST_F(D3D11DeviceCreator, CM_COPY_verify_block_5x5) {

    CMCopyFactory factory;
    CComPtr<ID3D11Device> device(CreateDevice());
    CmCopyWrapper<ID3D11Device>& copier = factory.CreateCMCopyWrapper(device);

    //creating surface
    D3D11AllocatorParams params;
    params.pDevice = device;
    D3D11FrameAllocator alloc;
    EXPECT_EQ(0, alloc.Init(&params));

    mfxFrameAllocRequest request;
    mfxFrameAllocResponse response;

    //64x alignment restiction of current cm copy
    request.Info.Width = 64;
    request.Info.Height = 16;
    request.Info.CropW = 64;
    request.Info.CropH = 16;
    request.Info.FourCC = MFX_FOURCC_NV12;
    request.NumFrameMin = 2;
    request.NumFrameSuggested = 2;
    request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

    EXPECT_EQ(0, alloc.AllocFrames(&request, &response));

    char data[6][4] = {{1,2,3,4}, {5,6,7,8},{9,10,11,12},{13,14,15,16}, {17,18,19,20}, {21,22,23,24}};

    mfxFrameSurface1 input_surface = {};
    mfxFrameSurface1 output_surface = {};

    input_surface.Data.MemId = response.mids[0];
    output_surface.Data.MemId = response.mids[1];

    EXPECT_EQ(MFX_ERR_NONE, alloc.LockFrame(input_surface.Data.MemId, &input_surface.Data));
    MSDK_MEMCPY(input_surface.Data.Y, data, 16);
    MSDK_MEMCPY(input_surface.Data.UV, (char*)data + 16, 8);
    EXPECT_EQ(MFX_ERR_NONE, alloc.UnlockFrame(input_surface.Data.MemId, &input_surface.Data));

    mfxHDLPair pinput;
    alloc.GetFrameHDL(input_surface.Data.MemId, (mfxHDL*)&pinput);
    mfxHDLPair poutput;
    alloc.GetFrameHDL(output_surface.Data.MemId, (mfxHDL*)&poutput);

    //infact copying
    EXPECT_EQ(MFX_ERR_NONE, copier.CopyVideoToVideoMemory(CComPtr<ID3D11Texture2D>((ID3D11Texture2D*)poutput.first), (int)poutput.second, CComPtr<ID3D11Texture2D>((ID3D11Texture2D*)pinput.first), (int)pinput.second));

    EXPECT_EQ(MFX_ERR_NONE, alloc.LockFrame(output_surface.Data.MemId, &output_surface.Data));
    //verify that both planes gets copied
    EXPECT_EQ(0, memcmp(output_surface.Data.Y, data, 16));
    EXPECT_EQ(0, memcmp(output_surface.Data.UV, (char*)data + 16, 8));
    EXPECT_EQ(MFX_ERR_NONE, alloc.UnlockFrame(output_surface.Data.MemId, &output_surface.Data));

}
#endif