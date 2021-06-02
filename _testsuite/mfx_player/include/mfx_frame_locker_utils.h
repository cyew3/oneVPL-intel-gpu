/* ****************************************************************************** *\
INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019-2021 Intel Corporation. All Rights Reserved.
File Name: .h
\* ****************************************************************************** */

#ifndef __MFX_FRAME_LOCKER__UTILS_H
#define __MFX_FRAME_LOCKER__UTILS_H

#if defined(_WIN32) || defined(_WIN64)
class MFXFrameLocker
{
public:
    MFXFrameLocker(IHWDevice* pHWDevice);

    virtual mfxStatus MapFrame(mfxFrameData* ptr, mfxHDL handle);
    virtual mfxStatus UnmapFrame(mfxFrameData* ptr);

protected:
    ID3D11Texture2D* m_pStaging;
    ID3D11Device* m_pDevice;
    CComPtr<ID3D11DeviceContext> m_pDeviceContext;
};
#endif
#endif //__MFX_FRAME_LOCKER__UTILS_H