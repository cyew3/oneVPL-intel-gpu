/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MF_MFX_PARAMS__
#define __MF_MFX_PARAMS__

#include <objbase.h>
#include <initguid.h>

#include "mfxstructures.h"

// {4084132D-CF06-4331-B899-F2FD9A6B5D03}
static const GUID IID_IConfigureMfxEncoder =
{ 0x4084132d, 0xcf06, 0x4331, { 0xb8, 0x99, 0xf2, 0xfd, 0x9a, 0x6b, 0x5d, 0x3 } };

// {FF987C70-BE2C-42e1-BA63-0CE3169C3811}
static const GUID IID_IConfigureMfxDecoder =
{ 0xff987c70, 0xbe2c, 0x42e1, { 0xba, 0x63, 0xc, 0xe3, 0x16, 0x9c, 0x38, 0x11 } };

// {5B4A85CA-453C-422e-B8DB-4D9A7C86ADC2}
static const GUID IID_IConfigureMfxVpp =
{ 0x5b4a85ca, 0x453c, 0x422e, { 0xb8, 0xdb, 0x4d, 0x9a, 0x7c, 0x86, 0xad, 0xc2 } };

struct mfxCodecInfo
{
    // total codec live time
    mfxF64    m_LiveTime;
    // total codec work time
    mfxF64    m_WorkTime;
    // ProcessInput time
    mfxF64    m_ProcessInputTime;
    // ProcessOutput time
    mfxF64    m_ProcessOutputTime;
    // MSDK implementation
    mfxIMPL   m_MSDKImpl;
    // Status of codec initialization
    mfxStatus m_InitStatus;
    // VPP usage
    bool      m_bVppUsed;
    // number of input frames
    mfxU32    m_nInFrames;
    // type of input frames (SW, HW), if applicable
    mfxU16    m_uiInFramesType;
    // number of VPP output frames
    mfxU32    m_nVppOutFrames;
    // type of VPP output frames (SW, HW)
    mfxU16    m_uiVppOutFramesType;
    // number of output frames
    mfxU32    m_nOutFrames;
    // type of output frames (SW, HW), if applicable
    mfxU16    m_uiOutFramesType;
    // error occured during processing
    mfxStatus m_ErrorStatus;
    HRESULT   m_hrErrorStatus;
    // number of errors occured during processing
    HRESULT   m_uiErrorResetCount;
};

class IConfigureMfxCodec: public IUnknown
{
public:
    STDMETHOD(SetParams)(mfxVideoParam *pattern, mfxVideoParam *params) PURE;
    STDMETHOD(GetParams)(mfxVideoParam *params) PURE;
    STDMETHOD(GetInfo)(mfxCodecInfo *info) PURE;
};

#endif // #ifndef __MF_MFX_PARAMS__
