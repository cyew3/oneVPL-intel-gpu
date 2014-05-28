//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2011-2011 Intel Corporation. All Rights Reserved.
//

#ifndef __S3D_UTIL_H
#define __S3D_UTIL_H

#include "igfx_s3dcontrol.h"
#include "common.h"

class CS3DUtil
{
public:
    IGFX_S3D_CAPS_STRUCT m_S3DCaps;
    BOOL m_bPlatformS3DSupport;
    CS3DUtil(void);
    ~CS3DUtil(void);
    bool GetMonitorUid(DWORD dwDeviceTypeReq, DWORD* uidMonitorReq);
    bool GetPrimaryMonitorUid(DWORD* uidMonitorReq);
    bool TestS3D();
    bool GetS3DCaps();
    bool SetS3DMode(IGFX_S3D_CONTROL_STRUCT S3DControl);
private:
    DWORD m_dwDisplayUID;
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  MFXS3DWrapper implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class MFXS3DWrapper : public IGFXS3DControl
{
public:
    virtual ~MFXS3DWrapper() {};

    virtual HRESULT GetS3DCaps(IGFX_S3DCAPS *pCaps);
    virtual HRESULT SetDevice(IDirect3DDeviceManager9 *pDeviceManager);
    virtual HRESULT SwitchTo3D(IGFX_DISPLAY_MODE *pMode = NULL);
    virtual HRESULT SwitchTo2D(IGFX_DISPLAY_MODE *pMode = NULL);

    virtual HRESULT SelectLeftView();
    virtual HRESULT SelectRightView();

protected:

    CS3DUtil  m_utilS3D;
    std::auto_ptr<CPrivateQueryDevice> m_privateDevice;

    HRESULT SwitchToMode(BOOL isS3D, IGFX_DISPLAY_MODE *pMode = NULL);
    HRESULT SelectView(BOOL isLeft);
};


#endif // __S3D_UTIL_H