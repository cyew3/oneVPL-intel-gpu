/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_ddi.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_ddi.h"
#include "mfx_screen_capture_d3d9.h"
#include "mfx_screen_capture_d3d11.h"
#include "mfx_screen_capture_sw_d3d9.h"
#include "mfx_screen_capture_sw_d3d11.h"

namespace MfxCapture
{

dispDescr GetTargetId(mfxU32 dispIndex)
{
    dispDescr display;
    memset(&display,0,sizeof(display));

    if(dispIndex > MAX_DISPLAYS)
        return display;

    displaysDescr displays;
    FindAllConnectedDisplays(displays);

    if(displays.n < dispIndex)
        return display;

    if(0 != dispIndex)
        dispIndex -= 1;

    return displays.display[dispIndex];
}

mfxU32 GetDisplayIndex(mfxU32 targetId)
{
    dispDescr display;
    memset(&display,0,sizeof(display));

    displaysDescr displays;
    FindAllConnectedDisplays(displays);

    for(mfxU32 i = 0; i < displays.n; ++i)
    {
        if(targetId == displays.display[i].TargetID)
            return i;
    }
    return 0;
}

void FindAllConnectedDisplays(displaysDescr& displays)
{
    unsigned int n = 0;

    memset(&displays, 0, sizeof(displays));

    UINT32 pNumPathArrayElements = 0;
    DISPLAYCONFIG_PATH_INFO *pPathInfoArray;
    UINT32 pNumModeInfoArrayElements = 0;
    DISPLAYCONFIG_MODE_INFO *pModeInfoArray;
    LONG lReturn = 0;

    lReturn = GetDisplayConfigBufferSizes(/*QDC_ALL_PATHS*/QDC_ONLY_ACTIVE_PATHS, &pNumPathArrayElements, &pNumModeInfoArrayElements);
    if (ERROR_SUCCESS == lReturn)
    {
        pPathInfoArray = (DISPLAYCONFIG_PATH_INFO*)malloc((pNumPathArrayElements)* sizeof(DISPLAYCONFIG_PATH_INFO));
        if(!pPathInfoArray)
            return;
        memset(pPathInfoArray, 0, (pNumPathArrayElements)* sizeof(DISPLAYCONFIG_PATH_INFO));
        pModeInfoArray = (DISPLAYCONFIG_MODE_INFO*)malloc((pNumModeInfoArrayElements)* sizeof(DISPLAYCONFIG_MODE_INFO));
        if(!pModeInfoArray)
        {
            free(pPathInfoArray);
            pPathInfoArray = 0;
            return;
        }
        memset(pModeInfoArray, 0, (pNumModeInfoArrayElements)* sizeof(DISPLAYCONFIG_MODE_INFO));
 
        lReturn = QueryDisplayConfig(/*QDC_ALL_PATHS*/QDC_ONLY_ACTIVE_PATHS, &pNumPathArrayElements, pPathInfoArray, &pNumModeInfoArrayElements, pModeInfoArray, NULL);
 
        if (ERROR_SUCCESS == lReturn)
        {
            for (unsigned int index = 0; index < pNumPathArrayElements; index++)
            {
                if (pPathInfoArray[index].targetInfo.targetAvailable)
                {
                    unsigned int targetID = pPathInfoArray[index].targetInfo.id;

                    displays.display[n].IndexNumber = n + 1;
                    displays.display[n].DXGIAdapterLuid = pPathInfoArray[index].targetInfo.adapterId;
                    displays.display[n].TargetID = targetID;
                    displays.display[n].width    = pModeInfoArray[pPathInfoArray[index].sourceInfo.modeInfoIdx].sourceMode.width;
                    displays.display[n].height   = pModeInfoArray[pPathInfoArray[index].sourceInfo.modeInfoIdx].sourceMode.height;
                    displays.display[n].position = pModeInfoArray[pPathInfoArray[index].sourceInfo.modeInfoIdx].sourceMode.position;
                    ++n;
                    displays.n = n;

                    if( (sizeof(displays.display) / sizeof(displays.display[0])) == n)
                    {
                        //so many displays...
                        break;
                    }
                }
            }
        }
        if(pPathInfoArray)
        {
            free(pPathInfoArray);
            pPathInfoArray = 0;
        }
        if(pModeInfoArray)
        {
            free(pModeInfoArray);
            pModeInfoArray = 0;
        }
    }
}

Capturer* CreatePlatformCapturer(mfxCoreInterface* core)
{
    try
    {
        if (core)
        {
            mfxCoreParam par = {}; 

            if (core->GetCoreParam(core->pthis, &par))
                return 0;

            if(MFX_IMPL_SOFTWARE != MFX_IMPL_BASETYPE(par.Impl))
            {
                switch(par.Impl & 0xF00)
                {
                case MFX_IMPL_VIA_D3D9:
                    return new D3D9_Capturer(core);
                case MFX_IMPL_VIA_D3D11:
                    return new D3D11_Capturer(core);
                default:
                    return 0;
                }
            }
            else
            {
                return new SW_D3D9_Capturer(core);
            }
        }
    }
    catch(...)
    {
        return 0;
    }

    return 0;
}

Capturer* CreateSWCapturer(mfxCoreInterface* core)
{
    try
    {
        if (core)
        {
            return new SW_D3D9_Capturer(core);
        }
    }
    catch(...)
    {
        return 0;
    }

    return 0;
}

DXGI_FORMAT MfxFourccToDxgiFormat(const mfxU32& fourcc)
{
    switch (fourcc)
    {
        case MFX_FOURCC_NV12:
            return DXGI_FORMAT_NV12;
        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_AYUV_RGB4:
        case DXGI_FORMAT_AYUV:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

D3DFORMAT MfxFourccToD3dFormat(const mfxU32& fourcc)
{
    switch (fourcc)
    {
        case MFX_FOURCC_NV12:
            return D3DFMT_NV12;
        case MFX_FOURCC_RGB4:
            return D3DFMT_A8R8G8B8;
        default:
            return D3DFMT_UNKNOWN;
    }
}

//CMFastCopy::CMFastCopy(mfxCoreInterface* _core)
//{
//    _core;
//}
//
//mfxStatus CMFastCopy::CMCopySysToGpu(mfxFrameSurface1& in, mfxFrameSurface1& out)
//{
//    in;
//    out;
//    return MFX_ERR_UNSUPPORTED;
//}

OwnResizeFilter::OwnResizeFilter()
{
    m_pSpec         = 0;
    m_pRGBSpec      = 0;
    m_pWorkBuffer   = 0;
    m_interpolation = ippHahn;

    memset(&m_in,0,sizeof(m_in));
    memset(&m_out,0,sizeof(m_out));
}

OwnResizeFilter::~OwnResizeFilter()
{
    if(m_pRGBSpec)
    {
        delete[] (mfxU8*) m_pRGBSpec;
        m_pRGBSpec = 0;
    }
    if(m_pWorkBuffer)
    {
        delete[] (mfxU8*) m_pWorkBuffer;
        m_pWorkBuffer = 0;
    }
    if(m_pSpec)
    {
        delete[] (mfxU8*) m_pSpec;
        m_pSpec = 0;
    }
}

mfxStatus OwnResizeFilter::Init(const mfxFrameInfo& in, const mfxFrameInfo& out)
{
    if(in.FourCC != out.FourCC)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    m_in = in;
    m_out = out;

    if(MFX_FOURCC_NV12 == in.FourCC)
    {
        IppStatus sts;
        int SpecSize    = 0;
        int InitBufSize = 0;

        IppiSize  srcSize = {0, 0};
        IppiSize  dstSize = {0, 0};

        srcSize.width = in.CropW ? in.CropW : in.Width;
        srcSize.height = in.CropH ? in.CropH : in.Height;

        if(!srcSize.width || !srcSize.height)
            return MFX_ERR_INVALID_VIDEO_PARAM; //avoid division by zero

        dstSize.width  = out.CropW ? out.CropW : out.Width;
        dstSize.height = out.CropH ? out.CropH : out.Height;

        mfxF64 xFactor, yFactor;
        xFactor = (mfxF64)dstSize.width  / (mfxF64)srcSize.width;
        yFactor = (mfxF64)dstSize.height / (mfxF64)srcSize.height;

        if( xFactor < 1.0 && yFactor < 1.0 )
            m_interpolation = ippSuper;
        else
            m_interpolation = ippLanczos;

        mfxU8* pInitBuffer;
        Ipp32s BufSize = 0;

        sts = ippiResizeGetSize_8u(srcSize, dstSize, m_interpolation, 0, &SpecSize, &InitBufSize);
        if(sts) return MFX_ERR_INVALID_VIDEO_PARAM;

        try{
            m_pSpec = (IppiResizeYUV420Spec*) new mfxU8[SpecSize];
            memset(m_pSpec, 0, SpecSize);
        } catch (...) {
            return MFX_ERR_MEMORY_ALLOC;
        }

        try{
            pInitBuffer = new mfxU8[InitBufSize];
            memset(pInitBuffer, 0, InitBufSize);
        } catch (...) {
            return MFX_ERR_MEMORY_ALLOC;
        }

        if(ippSuper == m_interpolation)
        {
            sts = ippiResizeYUV420SuperInit(srcSize, dstSize, m_pSpec);
        }
        else if(ippLanczos == m_interpolation)
        {
            int num_lobes = 2;
            sts = ippiResizeYUV420LanczosInit(srcSize, dstSize, num_lobes, m_pSpec, pInitBuffer);
        }
        else
        {
            delete[] pInitBuffer;
            pInitBuffer = 0;
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if(sts)
        {
            delete[] pInitBuffer;
            pInitBuffer = 0;
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        sts = ippiResizeYUV420GetBufferSize(m_pSpec, dstSize, &BufSize);
        if(sts)
        {
            delete[] pInitBuffer;
            pInitBuffer = 0;
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        try{
            m_pWorkBuffer = new mfxU8[BufSize];
            memset(m_pWorkBuffer, 0, BufSize);
        } catch (...) {
            delete[] pInitBuffer;
            pInitBuffer = 0;
            return MFX_ERR_MEMORY_ALLOC;
        }

        if(pInitBuffer)
        {
            delete[] pInitBuffer;
            pInitBuffer = 0;
        }

        return MFX_ERR_NONE;
    }
    else if(MFX_FOURCC_RGB4 == in.FourCC || DXGI_FORMAT_AYUV == in.FourCC || MFX_FOURCC_AYUV_RGB4 == in.FourCC)
    {
        IppStatus sts     = ippStsNotSupportedModeErr;
        IppiSize  srcSize = {0,  0};
        IppiSize  dstSize = {0, 0};
        mfxF64    xFactor = 0.0, yFactor = 0.0;


        srcSize.width = in.CropW ? in.CropW : in.Width;
        srcSize.height = in.CropH ? in.CropH : in.Height;

        if(!srcSize.width || !srcSize.height)
            return MFX_ERR_INVALID_VIDEO_PARAM; //avoid division by zero

        dstSize.width  = out.CropW ? out.CropW : out.Width;
        dstSize.height = out.CropH ? out.CropH : out.Height;

        xFactor = (mfxF64)dstSize.width  / (mfxF64)srcSize.width;
        yFactor = (mfxF64)dstSize.height / (mfxF64)srcSize.height;
    
        if( xFactor < 1.0 && yFactor < 1.0 )
            m_interpolation = ippSuper;
        else
            m_interpolation = ippLanczos;

        int SpecSize    = 0;
        int InitBufSize = 0;

        mfxU8* pInitBuffer;
        Ipp32s BufSize = 0;

        sts = ippiResizeGetSize_8u(srcSize, dstSize, m_interpolation, 0, &SpecSize, &InitBufSize);
        if(sts) return MFX_ERR_INVALID_VIDEO_PARAM;

        try{
            m_pRGBSpec = (IppiResizeSpec_32f*) new mfxU8[SpecSize];
            memset(m_pRGBSpec, 0, SpecSize);
        } catch (...) {
            return MFX_ERR_MEMORY_ALLOC;
        }

        try{
            pInitBuffer = new mfxU8[InitBufSize];
            memset(pInitBuffer, 0, InitBufSize);
        } catch (...) {
            return MFX_ERR_MEMORY_ALLOC;
        }

        if(ippSuper == m_interpolation)
        {
            sts = ippiResizeSuperInit_8u(srcSize, dstSize, m_pRGBSpec);
        }
        else if(ippLanczos == m_interpolation)
        {
            int num_lobes = 2;
            sts = ippiResizeLanczosInit_8u(srcSize, dstSize, num_lobes, m_pRGBSpec, pInitBuffer);
        }
        else
        {
            delete[] pInitBuffer;
            pInitBuffer = 0;
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        
        if(sts)
        {
            delete[] pInitBuffer;
            pInitBuffer = 0;
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        sts = ippiResizeGetBufferSize_8u(m_pRGBSpec, dstSize, 4,&BufSize);
        if(sts)
        {
            delete[] pInitBuffer;
            pInitBuffer = 0;
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        try{
            m_pWorkBuffer = new mfxU8[BufSize];
            memset(m_pWorkBuffer, 0, BufSize);
        } catch (...) {
            delete[] pInitBuffer;
            pInitBuffer = 0;
            return MFX_ERR_MEMORY_ALLOC;
        }

        if(pInitBuffer)
        {
            delete[] pInitBuffer;
            pInitBuffer = 0;
        }

        return MFX_ERR_NONE;
    }
    else
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
}

mfxStatus OwnResizeFilter::RunFrameVPP(mfxFrameSurface1& in, mfxFrameSurface1& out)
{
    if(in.Info.FourCC != out.Info.FourCC)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if(MFX_FOURCC_NV12 == in.Info.FourCC)
        return rs_NV12(&in, &out);
    else if(MFX_FOURCC_RGB4 == in.Info.FourCC)
        return rs_RGB4(&in, &out);
    else
        return MFX_ERR_INVALID_VIDEO_PARAM;
}

mfxStatus OwnResizeFilter::rs_RGB4( mfxFrameSurface1* in, mfxFrameSurface1* out)
{
    IppStatus sts     = ippStsNotSupportedModeErr;
    IppiSize  srcSize = {0,  0};
    IppiSize  dstSize = {0, 0};

    mfxU16 cropX = 0, cropY = 0;
    
    mfxU32 inOffset0  = 0;
    mfxU32 outOffset0 = 0;
    
    /* [IN] */
    mfxFrameData* inData = &in->Data;
    mfxFrameInfo* inInfo = &in->Info;

    VPP_GET_CROPX(inInfo, cropX);
    VPP_GET_CROPY(inInfo, cropY);
    inOffset0  = cropX*4  + cropY*inData->Pitch;
    
    mfxU8* ptrStart = IPP_MIN( IPP_MIN(inData->R, inData->G), inData->B );
    const mfxU8* pSrc[1] = {ptrStart + inOffset0};
    mfxI32 pSrcStep[1] = {inData->Pitch};
    
    /* OUT */
    mfxFrameData* outData= &out->Data;
    mfxFrameInfo* outInfo= &out->Info;
    
    VPP_GET_CROPX(outInfo, cropX);
    VPP_GET_CROPY(outInfo, cropY);
    outOffset0  = cropX*4  + cropY*outData->Pitch;
    
    ptrStart = IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );
    
    mfxU8* pDst[1]   = {ptrStart + outOffset0};
    
    mfxI32 pDstStep[1] = {outData->Pitch};
    
    /* common part */
    VPP_GET_WIDTH(inInfo,  srcSize.width);
    VPP_GET_HEIGHT(inInfo, srcSize.height);
    
    VPP_GET_WIDTH(outInfo,  dstSize.width);
    VPP_GET_HEIGHT(outInfo, dstSize.height);

    if(ippSuper == m_interpolation)
    {
        IppiPoint dstOffset = {0,0};
        sts = ippiResizeSuper_8u_C4R(pSrc[0], pSrcStep[0], pDst[0], pDstStep[0], dstOffset, dstSize, m_pRGBSpec, m_pWorkBuffer);
    }
    else if (ippLanczos == m_interpolation)
    {
        IppiPoint dstOffset = {0,0};
        IppiBorderType border = ippBorderRepl;
        sts = ippiResizeLanczos_8u_C4R(pSrc[0], pSrcStep[0], pDst[0], pDstStep[0], dstOffset, dstSize, border, 0, m_pRGBSpec, m_pWorkBuffer);
    }
    else
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if(ippStsNoErr == sts)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_DEVICE_FAILED;
} // IppStatus rs_RGB32( mfxFrameSurface1* in, mfxFrameSurface1* out)

mfxStatus OwnResizeFilter::rs_NV12( mfxFrameSurface1* in, mfxFrameSurface1* out)
{
    IppStatus sts     = ippStsNotSupportedModeErr;
    IppiSize  srcSize = {0,  0};
    IppiSize  dstSize = {0, 0};

    mfxU16 cropX = 0, cropY = 0;

    mfxU32 inOffset0  = 0, inOffset1  = 0;
    mfxU32 outOffset0 = 0, outOffset1 = 0;

    mfxFrameData* inData = &in->Data;
    mfxFrameInfo* inInfo = &in->Info;
    mfxFrameData* outData= &out->Data;
    mfxFrameInfo* outInfo= &out->Info;

    /* [IN] */
    VPP_GET_CROPX(inInfo, cropX);
    VPP_GET_CROPY(inInfo, cropY);
    inOffset0 = cropX + cropY*inData->Pitch;
    inOffset1 = (cropX) + (cropY>>1)*(inData->Pitch);

    /* [OUT] */
    VPP_GET_CROPX(outInfo, cropX);
    VPP_GET_CROPY(outInfo, cropY);
    outOffset0 = cropX + cropY*outData->Pitch;
    outOffset1 = (cropX) + (cropY>>1)*(outData->Pitch);

    const mfxU8* pSrc[2] = {(mfxU8*)inData->Y + inOffset0,
                            (mfxU8*)inData->UV + inOffset1};

    mfxI32 pSrcStep[2] = {inData->Pitch, inData->Pitch};

    mfxU8* pDst[2]   = {(mfxU8*)outData->Y + outOffset0,
                        (mfxU8*)outData->UV + outOffset1};

    mfxI32 pDstStep[2] = {outData->Pitch, outData->Pitch};

    /* common part */
    VPP_GET_WIDTH(inInfo,  srcSize.width);
    VPP_GET_HEIGHT(inInfo, srcSize.height);

    VPP_GET_WIDTH(outInfo,  dstSize.width);
    VPP_GET_HEIGHT(outInfo, dstSize.height);

    if(ippSuper == m_interpolation)
    {
        IppiPoint dstOffset = {0,0};
        sts = ippiResizeYUV420Super_8u_P2R(pSrc[0], pSrcStep[0], pSrc[1], pSrcStep[1], pDst[0], pDstStep[0], pDst[1], pDstStep[1], dstOffset, dstSize, m_pSpec, m_pWorkBuffer);
    }
    else if (ippLanczos == m_interpolation)
    {
        IppiPoint dstOffset = {0,0};
        IppiBorderType border = ippBorderRepl;
        sts = ippiResizeYUV420Lanczos_8u_P2R(pSrc[0], pSrcStep[0], pSrc[1], pSrcStep[1], pDst[0], pDstStep[0], pDst[1], pDstStep[1], dstOffset, dstSize, border, 0, m_pSpec, m_pWorkBuffer);
    }
    else
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if(ippStsNoErr == sts)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_DEVICE_FAILED;
}

mfxStatus OwnResizeFilter::GetParam(mfxFrameInfo& in, mfxFrameInfo& out)
{
    in = m_in;
    out = m_out;

    return MFX_ERR_NONE;
}

bool GetOsVersion(RTL_OSVERSIONINFOEXW* pk_OsVer)
{
    if(!pk_OsVer)
        return false;
    typedef LONG (WINAPI* tRtlGetVersion)(RTL_OSVERSIONINFOEXW*);

    memset(pk_OsVer, 0, sizeof(RTL_OSVERSIONINFOEXW));
    pk_OsVer->dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);

    HMODULE h_NtDll = GetModuleHandleW(L"ntdll.dll");
    if (!h_NtDll)
        return false; //should never happen
    tRtlGetVersion f_RtlGetVersion = (tRtlGetVersion)GetProcAddress(h_NtDll, "RtlGetVersion");

    if (!f_RtlGetVersion)
        return false; //should never happen

    LONG Status = f_RtlGetVersion(pk_OsVer);
    if(Status)
        return false;
    else
        return true;
}

bool IsWin10orLater()
{
    RTL_OSVERSIONINFOEXW ver = {};
    if( !GetOsVersion(&ver) )
        return false;

    if(ver.dwMajorVersion < 6)
        return false;

    //if > Win8.1
    if(ver.dwMajorVersion > 6 || ver.dwMinorVersion > 3)
        return true;

    return false;
}

} //namespace MfxCapture