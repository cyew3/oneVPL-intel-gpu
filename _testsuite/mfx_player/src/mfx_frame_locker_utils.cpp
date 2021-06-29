/******************************************************************************* *\
INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2021 Intel Corporation. All Rights Reserved.
File Name: .cpp
\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_renders.h"
#include "mfx_frame_locker_utils.h"

//////////////////////////////////////////////////////////////////////////

#if defined(_WIN32) || defined(_WIN64)
MFXFrameLocker::MFXFrameLocker(IHWDevice* pHWDevice)
{
    m_pStaging = nullptr;
    pHWDevice->GetHandle(MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&m_pDevice);
    m_pDeviceContext.Release();
    m_pDevice->GetImmediateContext(&m_pDeviceContext);
}

MFXFrameLocker::~MFXFrameLocker()
{
    Close();
}

mfxStatus MFXFrameLocker::MapFrame(mfxFrameData* ptr, mfxHDL handle)
{
    HRESULT hRes = S_OK;

    D3D11_TEXTURE2D_DESC desc = { 0 };
    D3D11_MAPPED_SUBRESOURCE lockedRect = { 0 };

    D3D11_MAP mapType = D3D11_MAP_READ;
    UINT mapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;

    ID3D11Texture2D* pTexture = (ID3D11Texture2D*)((mfxHDLPair*)handle)->first;
    UINT subResource = (UINT)(UINT_PTR)((mfxHDLPair*)handle)->second;

    pTexture->GetDesc(&desc);

    if (DXGI_FORMAT_NV12 != desc.Format &&
        DXGI_FORMAT_420_OPAQUE != desc.Format &&
        DXGI_FORMAT_P010 != desc.Format &&
        DXGI_FORMAT_YUY2 != desc.Format &&
        DXGI_FORMAT_P8 != desc.Format &&
        DXGI_FORMAT_B8G8R8A8_UNORM != desc.Format &&
        DXGI_FORMAT_R8G8B8A8_UNORM != desc.Format &&
        DXGI_FORMAT_R10G10B10A2_UNORM != desc.Format &&
        DXGI_FORMAT_AYUV != desc.Format &&
        DXGI_FORMAT_R16_UINT != desc.Format &&
        DXGI_FORMAT_R16_UNORM != desc.Format &&
        DXGI_FORMAT_R16_TYPELESS != desc.Format &&
        DXGI_FORMAT_Y210 != desc.Format &&
        DXGI_FORMAT_Y410 != desc.Format &&
        DXGI_FORMAT_P016 != desc.Format &&
        DXGI_FORMAT_Y216 != desc.Format &&
        DXGI_FORMAT_Y416 != desc.Format)
    {
        return MFX_ERR_LOCK_MEMORY;
    }

    std::vector<ID3D11Texture2D*> stagingTexture;

    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    hRes = m_pDevice->CreateTexture2D(&desc, NULL, &m_pStaging);

    if (FAILED(hRes))
    {
        vm_string_printf(VM_STRING("Create staging texture failed hr = 0x%X\n"), hRes);
        return MFX_ERR_MEMORY_ALLOC;
    }
    stagingTexture.push_back(m_pStaging);

    m_pDeviceContext->CopySubresourceRegion(m_pStaging, 0, 0, 0, 0, pTexture, subResource, NULL);

    do
    {
        hRes = m_pDeviceContext->Map(m_pStaging, 0, mapType, mapFlags, &lockedRect);
        if (S_OK != hRes && DXGI_ERROR_WAS_STILL_DRAWING != hRes)
        {
            vm_string_printf(VM_STRING("ERROR: m_pDeviceContext->Map = 0x%X\n"), hRes);
        }
    } while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);

    MFX_CHECK(!FAILED(hRes), MFX_ERR_LOCK_MEMORY);

    ptr->PitchHigh = (mfxU16)(lockedRect.RowPitch / (1 << 16));
    ptr->PitchLow = (mfxU16)(lockedRect.RowPitch % (1 << 16));

    switch (desc.Format)
    {
    case DXGI_FORMAT_NV12:
        ptr->Y = (mfxU8*)lockedRect.pData;
        ptr->U = (mfxU8*)lockedRect.pData + desc.Height * lockedRect.RowPitch;
        ptr->V = ptr->U + 1;
        break;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        ptr->Y = (mfxU8*)lockedRect.pData;
        ptr->U = (mfxU8*)lockedRect.pData + desc.Height * lockedRect.RowPitch;
        ptr->V = ptr->U + 1;
        break;

    case DXGI_FORMAT_420_OPAQUE: // can be unsupported by standard ms guid
        ptr->Y = (mfxU8*)lockedRect.pData;
        ptr->V = ptr->Y + desc.Height * lockedRect.RowPitch;
        ptr->U = ptr->V + (desc.Height * lockedRect.RowPitch) / 4;

        break;

    case DXGI_FORMAT_YUY2:
        ptr->Y = (mfxU8*)lockedRect.pData;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;

        break;

    case DXGI_FORMAT_P8:
        ptr->Y = (mfxU8*)lockedRect.pData;
        ptr->U = 0;
        ptr->V = 0;

        break;

    case DXGI_FORMAT_B8G8R8A8_UNORM:
        ptr->B = (mfxU8*)lockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;

        break;

    case DXGI_FORMAT_R8G8B8A8_UNORM:
        ptr->R = (mfxU8*)lockedRect.pData;
        ptr->G = ptr->R + 1;
        ptr->B = ptr->R + 2;
        ptr->A = ptr->R + 3;

        break;

    case DXGI_FORMAT_R10G10B10A2_UNORM:
        ptr->B = (mfxU8*)lockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;

        break;

    case DXGI_FORMAT_AYUV:
        ptr->B = (mfxU8*)lockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_TYPELESS:
        ptr->Y16 = (mfxU16*)lockedRect.pData;
        ptr->U16 = 0;
        ptr->V16 = 0;

        break;

    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        ptr->Y16 = (mfxU16*)lockedRect.pData;
        ptr->U16 = ptr->Y16 + 1;
        ptr->V16 = ptr->U16 + 3;
        break;

    case DXGI_FORMAT_Y410:
        ptr->Y410 = (mfxY410*)lockedRect.pData;
        ptr->Y = 0;
        ptr->V = 0;
        ptr->A = 0;
        break;

    case DXGI_FORMAT_Y416:
        ptr->U16 = (mfxU16*)lockedRect.pData;
        ptr->Y16 = ptr->U16 + 1;
        ptr->V16 = ptr->Y16 + 1;
        ptr->A = (mfxU8*)(ptr->V16 + 1);
        break;

    default:

        return MFX_ERR_LOCK_MEMORY;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXFrameLocker::UnmapFrame(mfxFrameData* ptr)
{
    m_pDeviceContext->Unmap(m_pStaging, 0);

    if (ptr)
    {
        ptr->PitchHigh = 0;
        ptr->PitchLow = 0;
        ptr->U = ptr->V = ptr->Y = 0;
        ptr->A = ptr->R = ptr->G = ptr->B = 0;
    }

    m_pStaging.Release();

    return MFX_ERR_NONE;
}

mfxStatus MFXFrameLocker::Close()
{
    m_pDeviceContext.Release();
    m_pStaging.Release();

    return MFX_ERR_NONE;
}
#endif