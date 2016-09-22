//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//

#include <string>
#include <assert.h>

#include <ipp.h>

#include "test_vpp_utils.h"
#include "mfxvideo++int.h"
#include "vm_time.h"
#include "sample_utils.h"

#include "test_vpp_pts.h"

#include "sysmem_allocator.h"
#include "d3d_allocator.h"

#include "d3d11_allocator.h"

#define MFX_CHECK_STS(sts) {if (MFX_ERR_NONE != sts) return sts;}

/* ******************************************************************* */

static
void WipeFrameProcessor(sFrameProcessor* pProcessor);

static
void WipeMemoryAllocator(sMemoryAllocator* pAllocator);

IppStatus cc_NV12_to_YV12( 
                          mfxFrameData* inData,  
                          mfxFrameInfo* inInfo,
                          mfxFrameData* outData, 
                          mfxFrameInfo* outInfo );
/* ******************************************************************* */

static
const vm_char* FourCC2Str( mfxU32 FourCC )
{
    switch ( FourCC )
    {
    case MFX_FOURCC_NV12:
        return VM_STRING("NV12");
    case MFX_FOURCC_YV12:
        return VM_STRING("YV12");
    case MFX_FOURCC_YUY2:
        return VM_STRING("YUY2");
    case MFX_FOURCC_RGB3:
        return VM_STRING("RGB3");
    case MFX_FOURCC_RGB4:
        return VM_STRING("RGB4");
    case MFX_FOURCC_YUV400:
        return VM_STRING("YUV400");
    case MFX_FOURCC_YUV411:
        return VM_STRING("YUV411");
    case MFX_FOURCC_YUV422H:
        return VM_STRING("YUV422H");
    case MFX_FOURCC_YUV422V:
        return VM_STRING("YUV422V");
    case MFX_FOURCC_YUV444:
        return VM_STRING("YUV444");
    case MFX_FOURCC_P010:
        return VM_STRING("P010");
    case MFX_FOURCC_P210:
        return VM_STRING("P210");
    case MFX_FOURCC_NV16:
        return VM_STRING("NV16");
    case MFX_FOURCC_A2RGB10:
        return VM_STRING("A2RGB10");
    case MFX_FOURCC_UYVY:
        return VM_STRING("UYVY");
    case MFX_FOURCC_AYUV:
        return VM_STRING("AYUV");
    case MFX_FOURCC_Y210:
        return VM_STRING("Y210");
    case MFX_FOURCC_Y410:
        return VM_STRING("Y410");
    default:
        return VM_STRING("Unknown");
    }
}

const vm_char* IOpattern2Str( mfxU32 IOpattern)
{
    switch ( IOpattern )
    {
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY:
        return VM_STRING("sys_to_sys");
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY:
        return VM_STRING("sys_to_d3d");
    case MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY:
        return VM_STRING("d3d_to_sys");
    case MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY:
        return VM_STRING("d3d_to_d3d");
    default:
        return VM_STRING("Not defined");
    }
}

static const
vm_char* sptr2Str( mfxU32 sptr)
{
    switch ( sptr)
    {
    case NO_PTR:
        return VM_STRING("MemID is used for in and out");
    case INPUT_PTR:
        return VM_STRING("Ptr is used for in, MemID for out");
    case OUTPUT_PTR:
        return VM_STRING("Ptr is used for out, MemID for in");
    case ALL_PTR:
        return VM_STRING("Ptr is used for in and out");
    default:
        return VM_STRING("Not defined");
    }
}

/* ******************************************************************* */

//static 
const vm_char* PicStruct2Str( mfxU16  PicStruct )
{
    switch (PicStruct)
    {
        case MFX_PICSTRUCT_PROGRESSIVE:
            return VM_STRING("progressive");
        case MFX_PICSTRUCT_FIELD_TFF:
            return VM_STRING("interlace (TFF)");
        case MFX_PICSTRUCT_FIELD_BFF:
            return VM_STRING("interlace (BFF)");
        case MFX_PICSTRUCT_UNKNOWN:
            return VM_STRING("unknown");
        default:
            return VM_STRING("interlace (no detail)");
    }
}

/* ******************************************************************* */

void PrintInfo(sInputParams* pParams, mfxVideoParam* pMfxParams, MFXVideoSession *pMfxSession)
{
    mfxFrameInfo Info;

    CHECK_POINTER_NO_RET(pParams);
    CHECK_POINTER_NO_RET(pMfxParams);    

    Info = pMfxParams->vpp.In;
    vm_string_printf(VM_STRING("Input format\t%s\n"), FourCC2Str( Info.FourCC ));  
    vm_string_printf(VM_STRING("Resolution\t%dx%d\n"), Info.Width, Info.Height);
    vm_string_printf(VM_STRING("Crop X,Y,W,H\t%d,%d,%d,%d\n"), Info.CropX, Info.CropY, Info.CropW, Info.CropH);
    vm_string_printf(VM_STRING("Frame rate\t%.2f\n"), (mfxF64)Info.FrameRateExtN / Info.FrameRateExtD);
    vm_string_printf(VM_STRING("PicStruct\t%s\n"), PicStruct2Str(Info.PicStruct));

    Info = pMfxParams->vpp.Out;
    vm_string_printf(VM_STRING("Output format\t%s\n"), FourCC2Str( Info.FourCC ));  
    vm_string_printf(VM_STRING("Resolution\t%dx%d\n"), Info.Width, Info.Height);
    vm_string_printf(VM_STRING("Crop X,Y,W,H\t%d,%d,%d,%d\n"), Info.CropX, Info.CropY, Info.CropW, Info.CropH);
    vm_string_printf(VM_STRING("Frame rate\t%.2f\n"), (mfxF64)Info.FrameRateExtN / Info.FrameRateExtD);
    vm_string_printf(VM_STRING("PicStruct\t%s\n"), PicStruct2Str(Info.PicStruct));

    vm_string_printf(VM_STRING("\n"));
    vm_string_printf(VM_STRING("Video Enhancement Algorithms\n"));
    vm_string_printf(VM_STRING("Deinterlace\t%s\n"), (pParams->frameInfoIn[0].PicStruct != pParams->frameInfoOut[0].PicStruct) ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("Signal info\t%s\n"),   (VPP_FILTER_DISABLED != pParams->videoSignalInfoParam[0].mode) ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("Scaling\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->bScaling) ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("Denoise\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->denoiseParam[0].mode) ? VM_STRING("ON"): VM_STRING("OFF"));

    vm_string_printf(VM_STRING("SceneDetection\t%s\n"),    (VPP_FILTER_DISABLED != pParams->vaParam[0].mode)      ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("StructDetection\t%s\n"),(VPP_FILTER_DISABLED != pParams->idetectParam[0].mode)      ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("VarianceReport\t%s\n"),    (VPP_FILTER_DISABLED != pParams->varianceParam[0].mode)      ? VM_STRING("ON"): VM_STRING("OFF"));

    vm_string_printf(VM_STRING("ProcAmp\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->procampParam[0].mode) ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("DetailEnh\t%s\n"),      (VPP_FILTER_DISABLED != pParams->detailParam[0].mode)  ? VM_STRING("ON"): VM_STRING("OFF"));
    if(VPP_FILTER_DISABLED != pParams->frcParam[0].mode)
    {
        if(MFX_FRCALGM_FRAME_INTERPOLATION == pParams->frcParam[0].algorithm)
        {
            vm_string_printf(VM_STRING("FRC:Interp\tON\n"));
        }
        else if(MFX_FRCALGM_DISTRIBUTED_TIMESTAMP == pParams->frcParam[0].algorithm)
        {
            vm_string_printf(VM_STRING("FRC:AdvancedPTS\tON\n"));
        }
        else
        {
            vm_string_printf(VM_STRING("FRC:\t\tON\n"));
        }
    }
    //vm_string_printf(VM_STRING("FRC:Advanced\t%s\n"),   (VPP_FILTER_DISABLED != pParams->frcParam.mode)  ? VM_STRING("ON"): VM_STRING("OFF"));
    // MSDK 3.0
    vm_string_printf(VM_STRING("GamutMapping \t%s\n"),       (VPP_FILTER_DISABLED != pParams->gamutParam[0].mode)      ? VM_STRING("ON"): VM_STRING("OFF") );
    vm_string_printf(VM_STRING("ColorSaturation\t%s\n"),     (VPP_FILTER_DISABLED != pParams->tccParam[0].mode)   ? VM_STRING("ON"): VM_STRING("OFF") );
    vm_string_printf(VM_STRING("ContrastEnh  \t%s\n"),       (VPP_FILTER_DISABLED != pParams->aceParam[0].mode)   ? VM_STRING("ON"): VM_STRING("OFF") );
    vm_string_printf(VM_STRING("SkinToneEnh  \t%s\n"),       (VPP_FILTER_DISABLED != pParams->steParam[0].mode)       ? VM_STRING("ON"): VM_STRING("OFF") );
    vm_string_printf(VM_STRING("MVC mode    \t%s\n"),       (VPP_FILTER_DISABLED != pParams->multiViewParam[0].mode)  ? VM_STRING("ON"): VM_STRING("OFF") );
    // SVC mode
    vm_string_printf(VM_STRING("SVC mode    \t%s\n"),        (VPP_FILTER_DISABLED != pParams->svcParam[0].mode)  ? VM_STRING("ON"): VM_STRING("OFF") );
    // MSDK 6.0
    vm_string_printf(VM_STRING("ImgStab    \t%s\n"),            (VPP_FILTER_DISABLED != pParams->istabParam[0].mode)  ? VM_STRING("ON"): VM_STRING("OFF") );
    vm_string_printf(VM_STRING("\n"));

    vm_string_printf(VM_STRING("IOpattern type               \t%s\n"), IOpattern2Str( pParams->IOPattern ));
    vm_string_printf(VM_STRING("Pointers and MemID settings  \t%s\n"), sptr2Str( pParams->sptr ));
    vm_string_printf(VM_STRING("Default allocator            \t%s\n"), pParams->bDefAlloc ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("Number of asynchronious tasks\t%d\n"), pParams->asyncNum);
    if ( pParams->bInitEx )
    {
    vm_string_printf(VM_STRING("GPU Copy mode                \t%d\n"), pParams->GPUCopyValue);
    }
    vm_string_printf(VM_STRING("Time stamps checking         \t%s\n"), pParams->ptsCheck ? VM_STRING("ON"): VM_STRING("OFF"));

    // info abour ROI testing
    if( ROI_FIX_TO_FIX == pParams->roiCheckParam.mode )
    {
        vm_string_printf(VM_STRING("ROI checking                 \tOFF\n"));  
    }
    else
    {
        vm_string_printf(VM_STRING("ROI checking                 \tON (seed1 = %i, seed2 = %i)\n"),pParams->roiCheckParam.srcSeed, pParams->roiCheckParam.dstSeed );  
    }

    vm_string_printf(VM_STRING("\n"));

    //-------------------------------------------------------
    mfxIMPL impl;
    pMfxSession->QueryIMPL(&impl);
    bool isHWlib = (MFX_IMPL_HARDWARE & impl) ? true:false;

    const vm_char* sImpl = (isHWlib) ? VM_STRING("hw") : VM_STRING("sw");
    vm_string_printf(VM_STRING("MediaSDK impl\t%s"), sImpl);

#ifndef LIBVA_SUPPORT
    if (isHWlib || (pParams->vaType & (ALLOC_IMPL_VIA_D3D9 | ALLOC_IMPL_VIA_D3D11)))
    {
        bool  isD3D11 = (( ALLOC_IMPL_VIA_D3D11 == pParams->vaType) || (pParams->ImpLib == (MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11))) ?  true : false;
        const vm_char* sIface = ( isD3D11 ) ? VM_STRING("VIA_D3D11") : VM_STRING("VIA_D3D9");  
        vm_string_printf(VM_STRING(" | %s"), sIface);
    } 
#endif
    vm_string_printf(VM_STRING("\n"));
    //-------------------------------------------------------

    if (isHWlib && !pParams->bPartialAccel)
        vm_string_printf(VM_STRING("HW accelaration is enabled\n"));
    else
        vm_string_printf(VM_STRING("HW accelaration is disabled\n"));

    mfxVersion ver;
    pMfxSession->QueryVersion(&ver);
    vm_string_printf(VM_STRING("MediaSDK ver\t%d.%d\n"), ver.Major, ver.Minor);

    return;
}

/* ******************************************************************* */

mfxStatus ParseGUID(vm_char strPlgGuid[MAX_FILELEN], mfxU8 DataGUID[16])
{
    const vm_char *uid = strPlgGuid;
    mfxU32 i   = 0;
    mfxU32 hex = 0;
    for(i = 0; i != 16; i++) 
    {
        hex = 0;
#if defined(_WIN32) || defined(_WIN64)
        if (1 != _stscanf_s(uid + 2*i, L"%2x", &hex))
#else
        if (1 != sscanf(uid + 2*i, "%2x", &hex))
#endif
        {
            vm_string_printf(VM_STRING("Failed to parse plugin uid: %s"), uid);
            return MFX_ERR_UNKNOWN;
        }
#if defined(_WIN32) || defined(_WIN64)
        if (hex == 0 && (uid + 2*i != _tcsstr(uid + 2*i, L"00")))
#else
        if (hex == 0 && (uid + 2*i != strstr(uid + 2*i, "00")))
#endif
        {
            vm_string_printf(VM_STRING("Failed to parse plugin uid: %s"), uid);
            return MFX_ERR_UNKNOWN;
        }
        DataGUID[i] = (mfxU8)hex;
    }

    return MFX_ERR_NONE;
}
mfxStatus InitParamsVPP(mfxVideoParam* pParams, sInputParams* pInParams, mfxU32 paramID)
{
    CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);
    CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR);

    if (pInParams->frameInfoIn[paramID].nWidth == 0 || pInParams->frameInfoIn[paramID].nHeight == 0 ){
        return MFX_ERR_UNSUPPORTED;
    }
    if (pInParams->frameInfoOut[paramID].nWidth == 0 || pInParams->frameInfoOut[paramID].nHeight == 0 ){
        return MFX_ERR_UNSUPPORTED;
    }

    memset(pParams, 0, sizeof(mfxVideoParam));                                   

    /* input data */  
    pParams->vpp.In.Shift           = pInParams->frameInfoIn[paramID].Shift;
    pParams->vpp.In.FourCC          = pInParams->frameInfoIn[paramID].FourCC;
    pParams->vpp.In.ChromaFormat    = MFX_CHROMAFORMAT_YUV420;  

    pParams->vpp.In.CropX = pInParams->frameInfoIn[paramID].CropX;
    pParams->vpp.In.CropY = pInParams->frameInfoIn[paramID].CropY;
    pParams->vpp.In.CropW = pInParams->frameInfoIn[paramID].CropW;
    pParams->vpp.In.CropH = pInParams->frameInfoIn[paramID].CropH;

    // width must be a multiple of 16 
    // height must be a multiple of 16 in case of frame picture and 
    // a multiple of 32 in case of field picture  
    pParams->vpp.In.Width = ALIGN16(pInParams->frameInfoIn[paramID].nWidth);
    pParams->vpp.In.Height= (MFX_PICSTRUCT_PROGRESSIVE == pInParams->frameInfoIn[paramID].PicStruct)?
        ALIGN16(pInParams->frameInfoIn[paramID].nHeight) : ALIGN32(pInParams->frameInfoIn[paramID].nHeight);

    pParams->vpp.In.PicStruct = pInParams->frameInfoIn[paramID].PicStruct;

    ConvertFrameRate(pInParams->frameInfoIn[paramID].dFrameRate,
        &pParams->vpp.In.FrameRateExtN, 
        &pParams->vpp.In.FrameRateExtD);

    /* output data */
    pParams->vpp.Out.Shift           = pInParams->frameInfoOut[paramID].Shift;
    pParams->vpp.Out.FourCC          = pInParams->frameInfoOut[paramID].FourCC;
    pParams->vpp.Out.ChromaFormat    = MFX_CHROMAFORMAT_YUV420;             

    pParams->vpp.Out.CropX = pInParams->frameInfoOut[paramID].CropX;
    pParams->vpp.Out.CropY = pInParams->frameInfoOut[paramID].CropY;
    pParams->vpp.Out.CropW = pInParams->frameInfoOut[paramID].CropW;
    pParams->vpp.Out.CropH = pInParams->frameInfoOut[paramID].CropH;

    // width must be a multiple of 16 
    // height must be a multiple of 16 in case of frame picture and 
    // a multiple of 32 in case of field picture  
    pParams->vpp.Out.Width = ALIGN16(pInParams->frameInfoOut[paramID].nWidth);
    pParams->vpp.Out.Height= (MFX_PICSTRUCT_PROGRESSIVE == pInParams->frameInfoOut[paramID].PicStruct)?
        ALIGN16(pInParams->frameInfoOut[paramID].nHeight) : ALIGN32(pInParams->frameInfoOut[paramID].nHeight);
    if(pInParams->need_plugin)
    {
        mfxPluginUID mfxGuid;
        ParseGUID(pInParams->strPlgGuid, mfxGuid.Data);
        if(!memcmp(&mfxGuid,&MFX_PLUGINID_ITELECINE_HW,sizeof(mfxPluginUID)))
        {
            //CM PTIR require equal input and output frame sizes
            pParams->vpp.Out.Height = pParams->vpp.In.Height;
        }
    }

    pParams->vpp.Out.PicStruct = pInParams->frameInfoOut[paramID].PicStruct;

    ConvertFrameRate(pInParams->frameInfoOut[paramID].dFrameRate,
        &pParams->vpp.Out.FrameRateExtN, 
        &pParams->vpp.Out.FrameRateExtD);

    pParams->IOPattern = pInParams->IOPattern;

    // async depth
    pParams->AsyncDepth = pInParams->asyncNum;

    if(VPP_FILTER_DISABLED != pInParams->svcParam[paramID].mode)
    {
        for(mfxU32 did = 0; did < 8; did++)
        {
            // aya: svc crop processing should be corrected if crop will be enabled via cmp line options
            pInParams->svcParam[paramID].descr[did].cropX = 0;
            pInParams->svcParam[paramID].descr[did].cropY = 0;
            pInParams->svcParam[paramID].descr[did].cropW = pInParams->svcParam[paramID].descr[did].width;
            pInParams->svcParam[paramID].descr[did].cropH = pInParams->svcParam[paramID].descr[did].height;

            // it is OK
            pInParams->svcParam[paramID].descr[did].height = ALIGN16(pInParams->svcParam[paramID].descr[did].height);
            pInParams->svcParam[paramID].descr[did].width  = ALIGN16(pInParams->svcParam[paramID].descr[did].width);
        }
    }

    return MFX_ERR_NONE;
}

/* ******************************************************************* */

mfxStatus CreateFrameProcessor(sFrameProcessor* pProcessor, mfxVideoParam* pParams, sInputParams* pInParams)
{
    mfxStatus  sts = MFX_ERR_NONE;
    mfxVersion version = {MFX_VERSION_MINOR, MFX_VERSION_MAJOR};
    mfxIMPL    impl    = pInParams->ImpLib;

    CHECK_POINTER(pProcessor, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);

    WipeFrameProcessor(pProcessor);

    //MFX session
    if ( ! pInParams->bInitEx )
        sts = pProcessor->mfxSession.Init(impl, &version);
    else
    {
        mfxInitParam initParams = {};
        initParams.GPUCopy         = pInParams->GPUCopyValue;
        initParams.Implementation  = impl;
        initParams.Version         = version;
        sts = pProcessor->mfxSession.InitEx(initParams);
    }
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to Init mfx session\n"));  WipeFrameProcessor(pProcessor);});

    // Plug-in
    if ( pInParams->need_plugin ) 
    {
        pProcessor->plugin = true;
        ParseGUID(pInParams->strPlgGuid, pProcessor->mfxGuid.Data);
    }

    if ( pProcessor->plugin )
    {
        sts = MFXVideoUSER_Load(pProcessor->mfxSession, &(pProcessor->mfxGuid), 1);
        if (MFX_ERR_NONE != sts)
        {
            vm_string_printf(VM_STRING("Failed to load plugin\n"));
            return sts;
        }
    }

    // VPP
    pProcessor->pmfxVPP = new MFXVideoVPP(pProcessor->mfxSession);

    return MFX_ERR_NONE;
}

/* ******************************************************************* */

#ifdef D3D_SURFACES_SUPPORT

#ifdef MFX_D3D11_SUPPORT

mfxStatus CreateD3D11Device(ID3D11Device** ppD3D11Device, ID3D11DeviceContext** ppD3D11DeviceContext)
{

    HRESULT hRes = S_OK;

    static D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    hRes = D3D11CreateDevice(NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0,
        FeatureLevels,
        sizeof(FeatureLevels) / sizeof(D3D_FEATURE_LEVEL),
        D3D11_SDK_VERSION,
        ppD3D11Device,
        &pFeatureLevelsOut,
        ppD3D11DeviceContext);

    if (FAILED(hRes))
    {
        return MFX_ERR_DEVICE_FAILED;
    }


    CComQIPtr<ID3D10Multithread> p_mt(*ppD3D11DeviceContext);

    if (p_mt)
        p_mt->SetMultithreadProtected(true);
    else
        return MFX_ERR_DEVICE_FAILED;


    return MFX_ERR_NONE;
}

#endif

mfxStatus CreateDeviceManager(IDirect3DDeviceManager9** ppManager)
{
    CHECK_POINTER(ppManager, MFX_ERR_NULL_PTR);

    //CComPtr<IDirect3D9> d3d = Direct3DCreate9(D3D_SDK_VERSION);
    CComPtr<IDirect3D9> d3d;
    d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));

    if (!d3d)
    {
        return MFX_ERR_NULL_PTR;
    }

    POINT point = {0, 0};
    HWND window = WindowFromPoint(point);

    D3DPRESENT_PARAMETERS d3dParams;
    memset(&d3dParams, 0, sizeof(d3dParams));
    d3dParams.Windowed = TRUE;
    d3dParams.hDeviceWindow = window;
    d3dParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dParams.Flags = D3DPRESENTFLAG_VIDEO;
    d3dParams.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    d3dParams.BackBufferCount = 1;
    d3dParams.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dParams.BackBufferWidth = 0;
    d3dParams.BackBufferHeight = 0;

    CComPtr<IDirect3DDevice9> d3dDevice = 0;
    HRESULT hr = d3d->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &d3dParams,
        &d3dDevice);
    if (FAILED(hr) || !d3dDevice)
        return MFX_ERR_NULL_PTR;

    UINT resetToken = 0;
    CComPtr<IDirect3DDeviceManager9> d3dDeviceManager = 0;
    hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &d3dDeviceManager);
    if (FAILED(hr) || !d3dDeviceManager)
        return MFX_ERR_NULL_PTR;

    hr = d3dDeviceManager->ResetDevice(d3dDevice, resetToken);
    if (FAILED(hr))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    *ppManager = d3dDeviceManager.Detach();

    if (NULL == *ppManager)
    {
        return MFX_ERR_NULL_PTR;
    }

    return MFX_ERR_NONE;
}
#endif

mfxStatus InitFrameProcessor(sFrameProcessor* pProcessor, mfxVideoParam* pParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    CHECK_POINTER(pProcessor,          MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,             MFX_ERR_NULL_PTR);
    CHECK_POINTER(pProcessor->pmfxVPP, MFX_ERR_NULL_PTR);

    // close VPP in case it was initialized
    sts = pProcessor->pmfxVPP->Close(); 
    IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
    CHECK_RESULT(sts,   MFX_ERR_NONE, sts);


    // init VPP
    sts = pProcessor->pmfxVPP->Init(pParams);
    return sts;
}

/* ******************************************************************* */

mfxStatus InitSurfaces(
    sMemoryAllocator* pAllocator, 
    mfxFrameAllocRequest* pRequest, 
    mfxFrameInfo* pInfo, 
    mfxU32 indx, 
    bool isPtr)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU16    nFrames, i;

    sts = pAllocator->pMfxAllocator->Alloc(pAllocator->pMfxAllocator->pthis, pRequest, &(pAllocator->response[indx]));
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to Alloc frames\n"));  WipeMemoryAllocator(pAllocator);});

    nFrames = pAllocator->response[indx].NumFrameActual;
    pAllocator->pSurfaces[indx] = new mfxFrameSurface1 [nFrames];

    for (i = 0; i < nFrames; i++)
    {       
        memset(&(pAllocator->pSurfaces[indx][i]), 0, sizeof(mfxFrameSurface1));
        pAllocator->pSurfaces[indx][i].Info = *pInfo;

        if( !isPtr )
        {
            pAllocator->pSurfaces[indx][i].Data.MemId = pAllocator->response[indx].mids[i];
        }
        else
        {
            sts = pAllocator->pMfxAllocator->Lock(pAllocator->pMfxAllocator->pthis, 
                pAllocator->response[indx].mids[i], 
                &(pAllocator->pSurfaces[indx][i].Data));
            CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to lock frames\n"));  WipeMemoryAllocator(pAllocator);});
        }
    }

    return sts;
}

/* ******************************************************************* */

mfxStatus InitSvcSurfaces(
    sMemoryAllocator* pAllocator, 
    mfxFrameAllocRequest* pRequest, 
    mfxFrameInfo* pInfo, 
    mfxU32 /*indx*/, 
    bool isPtr,
    sSVCLayerDescr* pSvcDesc
    )
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU16    nFrames = pAllocator->response->NumFrameActual, i, did;
    mfxFrameAllocRequest layerRequest[8];

    for( did = 0; did < 8; did++ )
    {
        if( pSvcDesc[did].active )
        {
            layerRequest[did] = *pRequest;
            
            layerRequest[did].Info.CropX  = pSvcDesc[did].cropX;
            layerRequest[did].Info.CropY  = pSvcDesc[did].cropY;
            layerRequest[did].Info.CropW  = pSvcDesc[did].cropW;
            layerRequest[did].Info.CropH  = pSvcDesc[did].cropH;
            
            layerRequest[did].Info.Height = pSvcDesc[did].height;
            layerRequest[did].Info.Width  = pSvcDesc[did].width;

            layerRequest[did].Info.PicStruct = pInfo->PicStruct;

            layerRequest[did].Info.FrameId.DependencyId = did;

            sts = pAllocator->pMfxAllocator->Alloc(
                pAllocator->pMfxAllocator->pthis, 
                &(layerRequest[did]), 
                &(pAllocator->svcResponse[did]));
            CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to alloc frames\n"));  WipeMemoryAllocator(pAllocator);});

            pAllocator->pSvcSurfaces[did] = new mfxFrameSurface1 [nFrames];
        }
    } 
    

    for( did = 0; did < 8; did++ )
    {
        if( 0 == pSvcDesc[did].active ) continue;
        
        for (i = 0; i < nFrames; i++)
        {       
            memset(&(pAllocator->pSvcSurfaces[did][i]), 0, sizeof(mfxFrameSurface1));

            pAllocator->pSvcSurfaces[did][i].Info = layerRequest[did].Info;

            if( !isPtr )
            {
                pAllocator->pSvcSurfaces[did][i].Data.MemId = pAllocator->svcResponse[did].mids[i];
            }
            else
            {
                sts = pAllocator->pMfxAllocator->Lock(
                    pAllocator->pMfxAllocator->pthis, 
                    pAllocator->svcResponse[did].mids[i], 
                    &(pAllocator->pSvcSurfaces[did][i].Data));
                CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to lock frames\n"));  WipeMemoryAllocator(pAllocator);});
            }
        }
    }

    return sts;
}

/* ******************************************************************* */

mfxStatus InitMemoryAllocator(
    sFrameProcessor* pProcessor, 
    sMemoryAllocator* pAllocator, 
    mfxVideoParam* pParams, 
    sInputParams* pInParams)
{
    mfxStatus sts = MFX_ERR_NONE;  
    mfxFrameAllocRequest request[2];// [0] - in, [1] - out

    CHECK_POINTER(pProcessor,          MFX_ERR_NULL_PTR);
    CHECK_POINTER(pAllocator,          MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,             MFX_ERR_NULL_PTR);
    CHECK_POINTER(pProcessor->pmfxVPP, MFX_ERR_NULL_PTR);

    ZERO_MEMORY(request[VPP_IN]);
    ZERO_MEMORY(request[VPP_OUT]);

    // VppRequest[0] for input frames request, VppRequest[1] for output frames request
    //sts = pProcessor->pmfxVPP->QueryIOSurf(pParams, request);
    //aya; async take into consediration by VPP
    /*  request[0].NumFrameMin = request[0].NumFrameMin + pInParams->asyncNum;
    request[1].NumFrameMin = request[1].NumFrameMin  + pInParams->asyncNum;

    request[0].NumFrameSuggested = request[0].NumFrameSuggested + pInParams->asyncNum;
    request[1].NumFrameSuggested = request[1].NumFrameSuggested + pInParams->asyncNum;*/

    IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

    bool isInPtr = (pInParams->sptr & INPUT_PTR)?true:false;
    bool isOutPtr = (pInParams->sptr & OUTPUT_PTR)?true:false;

    pAllocator->pMfxAllocator =  new GeneralAllocator;

    bool isHWLib       = (MFX_IMPL_HARDWARE & pInParams->ImpLib) ? true : false;
    bool isExtVideoMem = (pInParams->IOPattern != (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) ? true : false;

    bool isNeedExtAllocator = (isHWLib || isExtVideoMem);

    if( isNeedExtAllocator )
    {
#ifdef D3D_SURFACES_SUPPORT
        if( ((MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9) == pInParams->ImpLib) || (ALLOC_IMPL_VIA_D3D9 == pInParams->vaType) )
        {
            D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;  
            // prepare device manager
            sts = CreateDeviceManager(&(pAllocator->pd3dDeviceManager));
            CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to CreateDeviceManager\n")); WipeMemoryAllocator(pAllocator);});

            sts = pProcessor->mfxSession.SetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, pAllocator->pd3dDeviceManager);
            CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to SetHandle\n"));  WipeMemoryAllocator(pAllocator);});

            // prepare allocator
            pd3dAllocParams->pManager = pAllocator->pd3dDeviceManager;
            pAllocator->pAllocatorParams = pd3dAllocParams;

            /* In case of video memory we must provide mediasdk with external allocator 
            thus we demonstrate "external allocator" usage model.
            Call SetAllocator to pass allocator to mediasdk */
            sts = pProcessor->mfxSession.SetFrameAllocator(pAllocator->pMfxAllocator);
            CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to SetFrameAllocator\n"));  WipeMemoryAllocator(pAllocator);});
        }
        else if ( ((MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11) == pInParams->ImpLib) || ((ALLOC_IMPL_VIA_D3D11 == pInParams->vaType)))
        {
#ifdef MFX_D3D11_SUPPORT

            D3D11AllocatorParams *pd3d11AllocParams = new D3D11AllocatorParams;  

            // prepare device manager
            sts = CreateD3D11Device(&(pAllocator->pD3D11Device), &(pAllocator->pD3D11DeviceContext));
            CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to CreateD3D11Device\n"));  WipeMemoryAllocator(pAllocator);});

            sts = pProcessor->mfxSession.SetHandle(MFX_HANDLE_D3D11_DEVICE, pAllocator->pD3D11Device);
            CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to SetHandle\n"));  WipeMemoryAllocator(pAllocator);});

            // prepare allocator
            pd3d11AllocParams->pDevice = pAllocator->pD3D11Device;
            pAllocator->pAllocatorParams = pd3d11AllocParams;

            sts = pProcessor->mfxSession.SetFrameAllocator(pAllocator->pMfxAllocator);
            CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to SetFrameAllocator\n"));  WipeMemoryAllocator(pAllocator);});

            ((GeneralAllocator *)(pAllocator->pMfxAllocator))->SetDX11();

#endif
        }
#endif
#ifdef LIBVA_SUPPORT
        vaapiAllocatorParams *p_vaapiAllocParams = new vaapiAllocatorParams;

        p_vaapiAllocParams->m_dpy = pAllocator->libvaKeeper->GetVADisplay();
        pAllocator->pAllocatorParams = p_vaapiAllocParams;

        sts = pProcessor->mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL)pAllocator->libvaKeeper->GetVADisplay());
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        /* In case of video memory we must provide mediasdk with external allocator 
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to mediasdk */
        sts = pProcessor->mfxSession.SetFrameAllocator(pAllocator->pMfxAllocator);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to SetFrameAllocator\n"));  WipeMemoryAllocator(pAllocator);});
#endif
    }  
    else if (pAllocator->bUsedAsExternalAllocator)
    {
        sts = pProcessor->mfxSession.SetFrameAllocator(pAllocator->pMfxAllocator);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to SetFrameAllocator\n"));  WipeMemoryAllocator(pAllocator);});
    }

    //((GeneralAllocator *)(pAllocator->pMfxAllocator))->setDxVersion(pInParams->ImpLib);

    sts = pAllocator->pMfxAllocator->Init(pAllocator->pAllocatorParams);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to pMfxAllocator->Init\n"));  WipeMemoryAllocator(pAllocator);});

    // see the spec
    // (1) MFXInit()
    // (2) MFXQueryIMPL()
    // (3) MFXVideoCORE_SetHandle()
    // after (1-3), call of any MSDK function is OK
    sts = pProcessor->pmfxVPP->QueryIOSurf(pParams, request);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed in QueryIOSurf\n"));  WipeMemoryAllocator(pAllocator);});

    // alloc frames for vpp
    // [IN]
    sts = InitSurfaces(pAllocator, &(request[VPP_IN]), &(pParams->vpp.In), VPP_IN, isInPtr);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to InitSurfaces\n"));  WipeMemoryAllocator(pAllocator);});

    // [OUT]
    if( VPP_FILTER_DISABLED == pInParams->svcParam[0].mode )
    {
        sts = InitSurfaces(
            pAllocator, 
            &(request[VPP_OUT]), 
            &(pParams->vpp.Out), 
            VPP_OUT, 
            isOutPtr);
    }
    else
    {
        sts = InitSvcSurfaces(
            pAllocator, 
            &(request[VPP_OUT]), 
            &(pParams->vpp.Out), 
            VPP_OUT, 
            isOutPtr,
            &(pInParams->svcParam[0].descr[0]));
    }
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to InitSurfaces\n"));  WipeMemoryAllocator(pAllocator);});

    return MFX_ERR_NONE;
}

/* ******************************************************************* */

mfxStatus InitResources(sAppResources* pResources, mfxVideoParam* pParams, sInputParams* pInParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    CHECK_POINTER(pResources, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);
    sts = CreateFrameProcessor(pResources->pProcessor, pParams, pInParams);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to CreateFrameProcessor\n")); WipeResources(pResources); WipeParams(pInParams);});

    sts = InitMemoryAllocator(pResources->pProcessor, pResources->pAllocator, pParams, pInParams);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to InitMemoryAllocator\n")); WipeResources(pResources); WipeParams(pInParams);});

    sts = InitFrameProcessor(pResources->pProcessor, pParams);

    if (MFX_WRN_PARTIAL_ACCELERATION == sts || MFX_WRN_FILTER_SKIPPED == sts)
        return sts;
    else
    {
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to InitFrameProcessor\n")); WipeResources(pResources); WipeParams(pInParams);});
    }

    return sts;
}

/* ******************************************************************* */

void WipeFrameProcessor(sFrameProcessor* pProcessor)
{
    CHECK_POINTER_NO_RET(pProcessor);

    SAFE_DELETE(pProcessor->pmfxVPP);

    if ( pProcessor->plugin ) 
    {
        MFXVideoUSER_UnLoad(pProcessor->mfxSession, &(pProcessor->mfxGuid));
    }

    pProcessor->mfxSession.Close();
}

void WipeMemoryAllocator(sMemoryAllocator* pAllocator)
{
    CHECK_POINTER_NO_RET(pAllocator);

    SAFE_DELETE_ARRAY(pAllocator->pSurfaces[VPP_IN]);
    SAFE_DELETE_ARRAY(pAllocator->pSurfaces[VPP_OUT]);

    mfxU32 did;
    for(did = 0; did < 8; did++)
    {
        SAFE_DELETE_ARRAY(pAllocator->pSvcSurfaces[did]);
    }

    // delete frames
    if (pAllocator->pMfxAllocator)
    {
        pAllocator->pMfxAllocator->Free(pAllocator->pMfxAllocator->pthis, &pAllocator->response[VPP_IN]);
        pAllocator->pMfxAllocator->Free(pAllocator->pMfxAllocator->pthis, &pAllocator->response[VPP_OUT]);

        for(did = 0; did < 8; did++)
        {
            pAllocator->pMfxAllocator->Free(pAllocator->pMfxAllocator->pthis, &pAllocator->svcResponse[did]);
        }
    }

    // delete allocator
    SAFE_DELETE(pAllocator->pMfxAllocator);

#ifdef D3D_SURFACES_SUPPORT 
    // release device manager
    if (pAllocator->pd3dDeviceManager)
    {
        pAllocator->pd3dDeviceManager->Release();
        pAllocator->pd3dDeviceManager = NULL;
    }
#endif

    // delete allocator parameters
    SAFE_DELETE(pAllocator->pAllocatorParams);

} // void WipeMemoryAllocator(sMemoryAllocator* pAllocator)


void WipeConfigParam( sAppResources* pResources )
{

    if( pResources->multiViewConfig.View )
    {
        delete [] pResources->multiViewConfig.View;
    }

} // void WipeConfigParam( sAppResources* pResources )


void WipeResources(sAppResources* pResources)
{
    CHECK_POINTER_NO_RET(pResources);

    WipeFrameProcessor(pResources->pProcessor);

    WipeMemoryAllocator(pResources->pAllocator);

    if (pResources->pSrcFileReader)
    {
        pResources->pSrcFileReader->Close();
    }

    if (pResources->pDstFileWriters)
    {
        for (mfxU32 i = 0; i < pResources->dstFileWritersN; i++)
        {
            pResources->pDstFileWriters[i].Close();
        }
        delete[] pResources->pDstFileWriters;
        pResources->dstFileWritersN = 0;
        pResources->pDstFileWriters = 0;
    }

    WipeConfigParam( pResources );

} // void WipeResources(sAppResources* pResources)

/* ******************************************************************* */

void WipeParams(sInputParams* pParams)
{
    for (mfxU32 i = 0; i < pParams->strDstFiles.size(); i++)
    {
        if (pParams->strDstFiles[i])
        {
            delete[] pParams->strDstFiles[i];
        }
    }
    pParams->strDstFiles.clear();

} // void WipeParams(sInputParams* pParams)

/* ******************************************************************* */

CRawVideoReader::CRawVideoReader()
{
    m_fSrc = 0;
    m_isPerfMode = false;
    m_Repeat = 0;
    m_pPTSMaker = 0;
}

mfxStatus CRawVideoReader::Init(const vm_char *strFileName, PTSMaker *pPTSMaker)
{
    Close();

    CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);

    m_fSrc =vm_file_fopen(strFileName, VM_STRING("rb"));
    CHECK_POINTER(m_fSrc, MFX_ERR_ABORTED);

    m_pPTSMaker = pPTSMaker;

    return MFX_ERR_NONE;
}

CRawVideoReader::~CRawVideoReader()
{
    Close();
}

void CRawVideoReader::Close()
{
    if (m_fSrc != 0)
    {
        vm_file_fclose(m_fSrc);
        m_fSrc = 0;
    }
    m_SurfacesList.clear();

}

mfxStatus CRawVideoReader::LoadNextFrame(mfxFrameData* pData, mfxFrameInfo* pInfo)
{
    CHECK_POINTER(pData, MFX_ERR_NOT_INITIALIZED);
    CHECK_POINTER(pInfo, MFX_ERR_NOT_INITIALIZED);

    mfxU32 w, h, i, pitch;
    mfxU32 nBytesRead;
    mfxU8 *ptr;

    if (pInfo->CropH > 0 && pInfo->CropW > 0) 
    {
        w = pInfo->CropW;
        h = pInfo->CropH;
    } 
    else 
    {
        w = pInfo->Width;
        h = pInfo->Height;
    }

    pitch = pData->Pitch;

    if(pInfo->FourCC == MFX_FOURCC_YV12)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        w     >>= 1;
        h     >>= 1;
        pitch >>= 1;
        // load V
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else   if(pInfo->FourCC == MFX_FOURCC_YUV400)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        int i = 0;
        i;
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV411)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        w /= 4;

        // load V
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV422H)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        w     >>= 1;

        // load V
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV422V)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        h     >>= 1;

        // load V
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV444)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        // load V
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_NV12 )
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        // load UV
        h     >>= 1;
        ptr = pData->UV + pInfo->CropX + (pInfo->CropY >> 1) * pitch;
        for (i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_NV16 )
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        // load UV
        ptr = pData->UV + pInfo->CropX + (pInfo->CropY >> 1) * pitch;
        for (i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_P010 )
    {
        ptr = pData->Y + pInfo->CropX * 2 + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w * 2, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w*2, MFX_ERR_MORE_DATA);
        }

        // load UV
        h     >>= 1;
        ptr = pData->UV + pInfo->CropX + (pInfo->CropY >> 1) * pitch;
        for (i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w*2, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w*2, MFX_ERR_MORE_DATA);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_P210 )
    {
        ptr = pData->Y + pInfo->CropX * 2 + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w * 2, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w*2, MFX_ERR_MORE_DATA);
        }

        // load UV
        ptr = pData->UV + pInfo->CropX + (pInfo->CropY >> 1) * pitch;
        for (i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w*2, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w*2, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_RGB3)
    {
        CHECK_POINTER(pData->R, MFX_ERR_NOT_INITIALIZED);
        CHECK_POINTER(pData->G, MFX_ERR_NOT_INITIALIZED);
        CHECK_POINTER(pData->B, MFX_ERR_NOT_INITIALIZED);

        ptr = IPP_MIN( IPP_MIN(pData->R, pData->G), pData->B );
        ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, 3*w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, 3*w, MFX_ERR_MORE_DATA);
        }
    } 
    else if (pInfo->FourCC == MFX_FOURCC_RGB4)
    {
        CHECK_POINTER(pData->R, MFX_ERR_NOT_INITIALIZED);
        CHECK_POINTER(pData->G, MFX_ERR_NOT_INITIALIZED);
        CHECK_POINTER(pData->B, MFX_ERR_NOT_INITIALIZED);
        // there is issue with A channel in case of d3d, so A-ch is ignored
        //CHECK_POINTER(pData->A, MFX_ERR_NOT_INITIALIZED); 

        ptr = IPP_MIN( IPP_MIN(pData->R, pData->G), pData->B );
        ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, 4*w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, 4*w, MFX_ERR_MORE_DATA);
        }
    } 
    else if (pInfo->FourCC == MFX_FOURCC_YUY2)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, 2*w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, 2*w, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_UYVY)
    {
        ptr = pData->U + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, 2*w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, 2*w, MFX_ERR_MORE_DATA);
        }
    } 
    else if (pInfo->FourCC == MFX_FOURCC_IMC3)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        h     >>= 1;

        // load V
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_AYUV)
    {
        ptr = IPP_MIN( IPP_MIN(pData->Y, pData->U), IPP_MIN(pData->V, pData->A) );
        ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, 4*w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, 4*w, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_Y210)
    {
        ptr = (mfxU8*)pData->Y16 + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, 4*w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, 4*w, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_Y410)
    {
        ptr = (mfxU8*)pData->Y410 + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)vm_file_fread(ptr + i * pitch, 1, 4*w, m_fSrc);
            IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, 4*w, MFX_ERR_MORE_DATA);
        }
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}


mfxStatus CRawVideoReader::GetNextInputFrame(sMemoryAllocator* pAllocator, mfxFrameInfo* pInfo, mfxFrameSurface1** pSurface)
{
    mfxStatus sts;
    if (!m_isPerfMode)
    {
        GetFreeSurface(pAllocator->pSurfaces[VPP_IN], pAllocator->response[VPP_IN].NumFrameActual, pSurface);
        mfxFrameSurface1* pCurSurf = *pSurface;
        if (pCurSurf->Data.MemId)
        {
            // get YUV pointers
            sts = pAllocator->pMfxAllocator->Lock(pAllocator->pMfxAllocator->pthis, pCurSurf->Data.MemId, &pCurSurf->Data);
            MFX_CHECK_STS(sts);
            sts = LoadNextFrame(&pCurSurf->Data, pInfo);
            MFX_CHECK_STS(sts);
            sts = pAllocator->pMfxAllocator->Unlock(pAllocator->pMfxAllocator->pthis, pCurSurf->Data.MemId, &pCurSurf->Data);
            MFX_CHECK_STS(sts);
        }
        else
        {
            sts = LoadNextFrame( &pCurSurf->Data, pInfo);
            MFX_CHECK_STS(sts);
        }
    }
    else 
    {
        sts = GetPreAllocFrame(pSurface);
        MFX_CHECK_STS(sts);
    }

    if (m_pPTSMaker)
    {
        if (!m_pPTSMaker->SetPTS(*pSurface))
            return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}


mfxStatus  CRawVideoReader::GetPreAllocFrame(mfxFrameSurface1 **pSurface)
{
    if (m_it == m_SurfacesList.end())
    {
        m_Repeat--;
        m_it = m_SurfacesList.begin();
    }

    if (m_it->Data.Locked)
        return MFX_ERR_ABORTED;

    *pSurface = &(*m_it);
    m_it++;
    if (0 == m_Repeat)
        return MFX_ERR_MORE_DATA;

    return MFX_ERR_NONE;

}


mfxStatus  CRawVideoReader::PreAllocateFrameChunk(mfxVideoParam* pVideoParam, 
                                                  sInputParams* pParams, 
                                                  MFXFrameAllocator* pAllocator)
{
    mfxStatus sts;
    mfxFrameAllocRequest  request;
    mfxFrameAllocResponse response;
    mfxFrameSurface1      surface;
    m_isPerfMode = true;
    m_Repeat = pParams->numRepeat;
    request.Info = pVideoParam->vpp.In;
    request.Type = (pParams->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)?(MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET):
        (MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY);
    request.NumFrameSuggested = request.NumFrameMin = (mfxU16)pParams->numFrames;
    sts = pAllocator->Alloc(pAllocator, &request, &response);
    MFX_CHECK_STS(sts);
    for(;m_SurfacesList.size() < pParams->numFrames;)
    {
        surface.Data.Locked = 0;
        surface.Data.MemId = response.mids[m_SurfacesList.size()];
        surface.Info = pVideoParam->vpp.In;
        sts = pAllocator->Lock(pAllocator->pthis, surface.Data.MemId, &surface.Data);
        MFX_CHECK_STS(sts);
        sts = LoadNextFrame(&surface.Data, &pVideoParam->vpp.In);
        MFX_CHECK_STS(sts);
        sts = pAllocator->Unlock(pAllocator->pthis, surface.Data.MemId, &surface.Data);
        MFX_CHECK_STS(sts);
        m_SurfacesList.push_back(surface);
    }
    m_it = m_SurfacesList.begin();
    return MFX_ERR_NONE;
}
/* ******************************************************************* */

CRawVideoWriter::CRawVideoWriter()
{
    m_fDst = 0;
    m_pPTSMaker = 0;

    return;
}

mfxStatus CRawVideoWriter::Init(const vm_char *strFileName, PTSMaker *pPTSMaker, bool outYV12, bool need_crc )
{
    Close();

    m_pPTSMaker = pPTSMaker;
    m_crc32c = 0;
    // no need to generate output
    if (0 == strFileName)
        return MFX_ERR_NONE;

    //CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);

    m_fDst = vm_file_fopen(strFileName, VM_STRING("wb"));
    CHECK_POINTER(m_fDst, MFX_ERR_ABORTED);
    m_outYV12  = outYV12;
    m_need_crc = need_crc;

    return MFX_ERR_NONE;
}

CRawVideoWriter::~CRawVideoWriter()
{
    Close();

    return;
}


mfxStatus CRawVideoWriter::CRC32(mfxU8 *data, mfxU32 len) 
{
    if (! data || len <= 0 ) return MFX_ERR_ABORTED;

    CHECK_NOT_EQUAL(ippStsNoErr, ippsCRC32_8u( data, (Ipp32u) len, &m_crc32c ), MFX_ERR_ABORTED);

    return MFX_ERR_NONE;
}

void CRawVideoWriter::Close()
{
    if (m_fDst != 0){

        vm_file_fclose(m_fDst);
        m_fDst = 0;
    }

    return;
}

mfxU32 CRawVideoWriter::GetCRC()
{
    return m_crc32c;
}


mfxStatus CRawVideoWriter::PutNextFrame(
                                        sMemoryAllocator* pAllocator, 
                                        mfxFrameInfo* pInfo, 
                                        mfxFrameSurface1* pSurface)
{
    mfxStatus sts;
    if (m_fDst)
    {
        if (pSurface->Data.MemId) 
        {
            // get YUV pointers
            sts = pAllocator->pMfxAllocator->Lock(pAllocator->pMfxAllocator->pthis, pSurface->Data.MemId, &(pSurface->Data));
            CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

            sts = WriteFrame( &(pSurface->Data), pInfo);
            CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

            sts = pAllocator->pMfxAllocator->Unlock(pAllocator->pMfxAllocator->pthis, pSurface->Data.MemId, &(pSurface->Data));
            CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);
        }
        else 
        {
            sts = WriteFrame( &(pSurface->Data), pInfo);
            CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);
        }
    }
    else // performance mode
    {
        if (pSurface->Data.MemId) 
        {
            sts = pAllocator->pMfxAllocator->Lock(pAllocator->pMfxAllocator->pthis, pSurface->Data.MemId, &(pSurface->Data));
            CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);
            sts = pAllocator->pMfxAllocator->Unlock(pAllocator->pMfxAllocator->pthis, pSurface->Data.MemId, &(pSurface->Data));
            CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);
        }
    }
    if (m_pPTSMaker)
        return m_pPTSMaker->CheckPTS(pSurface)?MFX_ERR_NONE:MFX_ERR_ABORTED;

    return MFX_ERR_NONE;
}
mfxStatus CRawVideoWriter::WriteFrame(
                                      mfxFrameData* pData, 
                                      mfxFrameInfo* pInfo)
{
    mfxI32 nBytesRead   = 0;

    mfxI32 i, h, w, pitch;
    mfxU8* ptr;

    CHECK_POINTER(pData, MFX_ERR_NOT_INITIALIZED);
    CHECK_POINTER(pInfo, MFX_ERR_NOT_INITIALIZED);
    //-------------------------------------------------------
    //aya: yv12 output is usefull 
    mfxFrameData outData = *pData;


    if( pInfo->FourCC == MFX_FOURCC_NV12 && m_outYV12 )
    {
        if(m_outSurfYV12.get() == 0)
        {
            mfxU32 sizeOfFrame = (pInfo->Width * pInfo->Height * 3) >> 1;
            m_outSurfYV12.reset( new mfxU8[sizeOfFrame] );
            if(m_outSurfYV12.get() == 0)
            {
                printf("\nERROR on internal Allocation\n");
            }
        }
        outData.Y = m_outSurfYV12.get();
        outData.V = outData.Y + pInfo->Width * pInfo->Height;
        outData.U = outData.V + (pInfo->Width >> 1) * (pInfo->Height >> 1);
        outData.Pitch = pInfo->Width;

        mfxFrameInfo outFrameInfo = *pInfo;
        outFrameInfo.FourCC = MFX_FOURCC_YV12;

        IppStatus ippSts = cc_NV12_to_YV12(
            pData,
            pInfo,
            &outData,
            &outFrameInfo);

        if( ippSts )
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    //-------------------------------------------------------

    if (pInfo->CropH > 0 && pInfo->CropW > 0) 
    {
        w = pInfo->CropW;
        h = pInfo->CropH;
    } 
    else 
    {
        w = pInfo->Width;
        h = pInfo->Height;
    }

    pitch = outData.Pitch;

    if(pInfo->FourCC == MFX_FOURCC_YV12 || m_outYV12)
    {

        ptr   = outData.Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL(vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        w     >>= 1;
        h     >>= 1;
        pitch >>= 1;

        ptr  = outData.V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        ptr  = outData.U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fwrite(ptr + i * pitch, 1, w, m_fDst);
            CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }
    } 
    else if(pInfo->FourCC == MFX_FOURCC_YUV400)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        w     >>= 1;
        h     >>= 1;
        pitch >>= 1;

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fwrite(ptr + i * pitch, 1, w, m_fDst);
            CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }
    } 
    else if(pInfo->FourCC == MFX_FOURCC_YUV411)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        w     /= 4;
        //pitch /= 4;

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fwrite(ptr + i * pitch, 1, w, m_fDst);
            CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }
    } 
    else if(pInfo->FourCC == MFX_FOURCC_YUV422H)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        w     >>= 1;
        //pitch >>= 1;

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fwrite(ptr + i * pitch, 1, w, m_fDst);
            CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }
    } 
    else if(pInfo->FourCC == MFX_FOURCC_YUV422V)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        h     >>= 1;

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fwrite(ptr + i * pitch, 1, w, m_fDst);
            CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }
    } 
    else if(pInfo->FourCC == MFX_FOURCC_YUV444)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++) 
        {
            nBytesRead = (mfxU32)vm_file_fwrite(ptr + i * pitch, 1, w, m_fDst);
            CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }
    } 
    else if( pInfo->FourCC == MFX_FOURCC_NV12 )
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        // write UV data
        h     >>= 1;
        ptr  = pData->UV + (pInfo->CropX ) + (pInfo->CropY >> 1) * pitch;

        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_NV16 )
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        // write UV data
        ptr  = pData->UV + (pInfo->CropX ) + (pInfo->CropY >> 1) * pitch;

        for(i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_P010 )
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w * 2, m_fDst), w * 2, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w * 2);
        }

        // write UV data
        h     >>= 1;
        ptr  = pData->UV + (pInfo->CropX ) + (pInfo->CropY >> 1) * pitch ;

        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w*2, m_fDst), w*2, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w * 2);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_P210 )
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w * 2, m_fDst), w * 2, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w * 2);
        }

        // write UV data
        ptr  = pData->UV + (pInfo->CropX ) + (pInfo->CropY >> 1) * pitch ;

        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w*2, m_fDst), w*2, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w * 2);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_YUY2 )
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, 2*w, m_fDst), 2*w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, 2*w);
        }
    }
    else if ( pInfo->FourCC == MFX_FOURCC_IMC3 )
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        w     >>= 1;
        h     >>= 1;

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)vm_file_fwrite(ptr + i * pitch, 1, w, m_fDst);
            CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
            if ( m_need_crc ) CRC32(ptr + i * pitch, w);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_RGB4 || pInfo->FourCC == MFX_FOURCC_A2RGB10)
    {
        CHECK_POINTER(pData->R, MFX_ERR_NOT_INITIALIZED);
        CHECK_POINTER(pData->G, MFX_ERR_NOT_INITIALIZED);
        CHECK_POINTER(pData->B, MFX_ERR_NOT_INITIALIZED);
        // there is issue with A channel in case of d3d, so A-ch is ignored
        //CHECK_POINTER(pData->A, MFX_ERR_NOT_INITIALIZED); 

        ptr = IPP_MIN( IPP_MIN(pData->R, pData->G), pData->B );
        ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr + i * pitch, 1, 4*w, m_fDst), 4*w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, 4*w);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_AYUV)
    {
        ptr = IPP_MIN( IPP_MIN(pData->Y, pData->U), IPP_MIN(pData->V, pData->A) );
        ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr + i * pitch, 1, 4*w, m_fDst), 4*w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, 4*w);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_Y210)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++) 
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr+ i * pitch, 1, 4*w, m_fDst), 4*w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, 4*w);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_Y410)
    {
        ptr = (mfxU8*)pData->Y410 + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            CHECK_NOT_EQUAL( vm_file_fwrite(ptr + i * pitch, 1, 4*w, m_fDst), 4*w, MFX_ERR_UNDEFINED_BEHAVIOR);
            if ( m_need_crc ) CRC32(ptr + i * pitch, 4*w);
        }
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

/* ******************************************************************* */

GeneralWriter::GeneralWriter()
{
};


GeneralWriter::~GeneralWriter()
{
    Close();
};


void GeneralWriter::Close()
{
    for(mfxU32 did = 0; did < 8; did++)
    {
        m_ofile[did].reset();
    }
};


mfxStatus GeneralWriter::Init(
    const vm_char *strFileName, 
    PTSMaker *pPTSMaker,
    sSVCLayerDescr*  pDesc,
    bool outYV12,
    bool need_crc)
{
    mfxStatus sts = MFX_ERR_UNKNOWN;;

    mfxU32 didCount = (pDesc) ? 8 : 1;
    m_svcMode = (pDesc) ? true : false;

    for(mfxU32 did = 0; did < didCount; did++)
    {
        if( (1 == didCount) || (pDesc[did].active) )
        {
            m_ofile[did].reset(new CRawVideoWriter() );
            if(0 == m_ofile[did].get())
            {
                return MFX_ERR_UNKNOWN;
            }

            vm_char drive[MAX_PATH];
            vm_char dir[MAX_PATH];
            vm_char fname[MAX_PATH];
            vm_char ext[MAX_PATH];

            vm_string_splitpath(
                strFileName,
                drive,
                dir,
                fname,
                ext);
            
            vm_char out_buf[1024];
            vm_string_sprintf_s(out_buf, VM_STRING("%s%s%s_layer%i.yuv"), drive, dir, fname, did);

            sts = m_ofile[did]->Init(
                (1 == didCount) ? strFileName : out_buf,
                pPTSMaker,
                outYV12,
                need_crc);

            if(sts != MFX_ERR_NONE) break;
        }
    }

    return sts;
};

mfxU32  GeneralWriter::GetCRC(
        mfxFrameSurface1* pSurface)
{
    mfxU32 did = (m_svcMode) ? pSurface->Info.FrameId.DependencyId : 0;//aya: for MVC we have 1 out file only

    return m_ofile[did]->GetCRC();
};

mfxStatus  GeneralWriter::PutNextFrame(
        sMemoryAllocator* pAllocator, 
        mfxFrameInfo* pInfo, 
        mfxFrameSurface1* pSurface)
{
    mfxU32 did = (m_svcMode) ? pSurface->Info.FrameId.DependencyId : 0;//aya: for MVC we have 1 out file only
    
    mfxStatus sts = m_ofile[did]->PutNextFrame(pAllocator, pInfo, pSurface);

    return sts;
};

/* ******************************************************************* */

mfxStatus UpdateSurfacePool(mfxFrameInfo SurfacesInfo, mfxU16 nPoolSize, mfxFrameSurface1* pSurface)
{
    CHECK_POINTER(pSurface,     MFX_ERR_NULL_PTR);
    if (pSurface)
    {
        for (mfxU16 i = 0; i < nPoolSize; i++)
        {
            pSurface[i].Info = SurfacesInfo;
        }
    }
    return MFX_ERR_NONE;
}
mfxStatus GetFreeSurface(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize, mfxFrameSurface1** ppSurface)
{
    CHECK_POINTER(pSurfacesPool, MFX_ERR_NULL_PTR); 
    CHECK_POINTER(ppSurface,     MFX_ERR_NULL_PTR);

    mfxU32 timeToSleep = 10; // milliseconds
    mfxU32 numSleeps = VPP_WAIT_INTERVAL / timeToSleep + 1; // at least 1

    mfxU32 i = 0;

    //wait if there's no free surface
    while ((INVALID_SURF_IDX == GetFreeSurfaceIndex(pSurfacesPool, nPoolSize)) && (i < numSleeps))
    {        
        vm_time_sleep(timeToSleep);
        i++;
    }

    mfxU16 index = GetFreeSurfaceIndex(pSurfacesPool, nPoolSize);

    if (index < nPoolSize)
    {
        *ppSurface = &(pSurfacesPool[index]);
        return MFX_ERR_NONE;
    }

    return MFX_ERR_NOT_ENOUGH_BUFFER;
}

// Wrapper on standard allocator for concurrent allocation of 
// D3D and system surfaces
GeneralAllocator::GeneralAllocator() 
{
#ifdef MFX_D3D11_SUPPORT
    m_D3D11Allocator.reset(new D3D11FrameAllocator);
#endif
#ifdef D3D_SURFACES_SUPPORT
    m_D3DAllocator.reset(new D3DFrameAllocator);
#endif
#ifdef LIBVA_SUPPORT
    m_vaapiAllocator.reset(new vaapiFrameAllocator);
#endif
    m_SYSAllocator.reset(new SysMemFrameAllocator);

    m_isDx11 = false;

};
GeneralAllocator::~GeneralAllocator() 
{
};
mfxStatus GeneralAllocator::Init(mfxAllocatorParams *pParams)
{
    mfxStatus sts = MFX_ERR_NONE;
#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        sts = m_D3D11Allocator.get()->Init(pParams);
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        sts = m_D3DAllocator.get()->Init(pParams);
#endif

    CHECK_RESULT(MFX_ERR_NONE, sts, sts);

#ifdef LIBVA_SUPPORT
    sts = m_vaapiAllocator.get()->Init(pParams);
    CHECK_RESULT(MFX_ERR_NONE, sts, sts);
#endif

    sts = m_SYSAllocator.get()->Init(0);
    CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    return sts;
}
mfxStatus GeneralAllocator::Close()
{
    mfxStatus sts = MFX_ERR_NONE;

#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        sts = m_D3D11Allocator.get()->Close();
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        sts = m_D3DAllocator.get()->Close();
#endif
#ifdef LIBVA_SUPPORT
    sts = m_vaapiAllocator.get()->Close();
#endif
    CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    sts = m_SYSAllocator.get()->Close();
    CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    m_isDx11 = false;

    return sts;
}

mfxStatus GeneralAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        return isD3DMid(mid) ? m_D3D11Allocator.get()->Lock(m_D3D11Allocator.get(), mid, ptr):
        m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        return isD3DMid(mid) ? m_D3DAllocator.get()->Lock(m_D3DAllocator.get(), mid, ptr):
        m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);
#elif LIBVA_SUPPORT
        return isD3DMid(mid)?m_vaapiAllocator.get()->Lock(m_vaapiAllocator.get(), mid, ptr):
        m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);
#else
        return m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);
#endif
}
mfxStatus GeneralAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        return isD3DMid(mid)?m_D3D11Allocator.get()->Unlock(m_D3D11Allocator.get(), mid, ptr):
        m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        return isD3DMid(mid)?m_D3DAllocator.get()->Unlock(m_D3DAllocator.get(), mid, ptr):
        m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);
#elif LIBVA_SUPPORT
        return isD3DMid(mid)?m_vaapiAllocator.get()->Unlock(m_vaapiAllocator.get(), mid, ptr):
        m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);
#else 
        return m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);
#endif
}

mfxStatus GeneralAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        return isD3DMid(mid)?m_D3D11Allocator.get()->GetHDL(m_D3D11Allocator.get(), mid, handle):
        m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        return isD3DMid(mid)?m_D3DAllocator.get()->GetHDL(m_D3DAllocator.get(), mid, handle):
        m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);

#elif LIBVA_SUPPORT
        return isD3DMid(mid)?m_vaapiAllocator.get()->GetHDL(m_vaapiAllocator.get(), mid, handle):
        m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);
#else
        return m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);
#endif
}

mfxStatus GeneralAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    // try to ReleaseResponsevia D3D allocator
#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        return isD3DMid(response->mids[0])?m_D3D11Allocator.get()->Free(m_D3D11Allocator.get(),response):
        m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        return isD3DMid(response->mids[0])?m_D3DAllocator.get()->Free(m_D3DAllocator.get(),response):
        m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);
#elif LIBVA_SUPPORT
        return isD3DMid(response->mids[0])?m_vaapiAllocator.get()->Free(m_vaapiAllocator.get(),response):
        m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);
#else
        return m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);
#endif 
}
mfxStatus GeneralAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus sts;
    if (request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET || request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)
    {
#ifdef MFX_D3D11_SUPPORT
        if (true == m_isDx11)
            sts = m_D3D11Allocator.get()->Alloc(m_D3D11Allocator.get(), request, response);
        else
#endif
#ifdef D3D_SURFACES_SUPPORT
            sts = m_D3DAllocator.get()->Alloc(m_D3DAllocator.get(), request, response);
#endif
#ifdef LIBVA_SUPPORT
        sts = m_vaapiAllocator.get()->Alloc(m_vaapiAllocator.get(), request, response);
#endif
        StoreFrameMids(true, response);
    }
    else
    {
        sts = m_SYSAllocator.get()->Alloc(m_SYSAllocator.get(), request, response);
        StoreFrameMids(false, response);
    }
    return sts;
}
void GeneralAllocator::StoreFrameMids(bool isD3DFrames, mfxFrameAllocResponse *response)
{
    for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        m_Mids.insert(std::pair<mfxHDL, bool>(response->mids[i], isD3DFrames));
}
bool GeneralAllocator::isD3DMid(mfxHDL mid)
{
    std::map<mfxHDL, bool>::iterator it;
    it = m_Mids.find(mid);
    return it->second;

}

void GeneralAllocator::SetDX11(void)
{
    m_isDx11 = true;
}

//---------------------------------------------------------

void PrintDllInfo()
{
#if defined(_WIN32) || defined(_WIN64)
    HANDLE   hCurrent = GetCurrentProcess();
    HMODULE *pModules;
    DWORD    cbNeeded;
    int      nModules;
    if (NULL == EnumProcessModules(hCurrent, NULL, 0, &cbNeeded))
        return;

    nModules = cbNeeded / sizeof(HMODULE);

    pModules = new HMODULE[nModules];
    if (NULL == pModules)
    {
        return;
    }
    if (NULL == EnumProcessModules(hCurrent, pModules, cbNeeded, &cbNeeded))
    {
        delete []pModules;
        return;
    }

    for (int i = 0; i < nModules; i++)
    {
        vm_char buf[2048];
        GetModuleFileName(pModules[i], buf, ARRAYSIZE(buf));
        if (_tcsstr(buf, VM_STRING("libmfx")))
        {
            vm_string_printf(VM_STRING("MFX dll         %s\n"),buf);
        }
    }
    delete []pModules;
#endif
} // void PrintDllInfo()


// internal feature
IppStatus cc_NV12_to_YV12( 
                          mfxFrameData* inData,  
                          mfxFrameInfo* inInfo,
                          mfxFrameData* outData, 
                          mfxFrameInfo* /*outInfo*/ )
{
    IppStatus sts = ippStsNoErr;
    IppiSize  roiSize = {inInfo->Width, inInfo->Height};
    mfxU16  cropX = 0, cropY = 0;

    mfxU32  inOffset0 = 0, inOffset1  = 0;
    mfxU32  outOffset0= 0, outOffset1 = 0;

    //VPP_GET_WIDTH(inInfo, roiSize.width);
    //VPP_GET_HEIGHT(inInfo, roiSize.height);

    //VPP_GET_CROPX(inInfo,  cropX);
    //VPP_GET_CROPY(inInfo,  cropY);
    inOffset0  = cropX  + cropY*inData->Pitch;
    inOffset1  = (cropX) + (cropY>>1)*(inData->Pitch);

    //VPP_GET_CROPX(outInfo, cropX);
    //VPP_GET_CROPY(outInfo, cropY);
    outOffset0  = cropX        + cropY*outData->Pitch;
    outOffset1  = (cropX >> 1) + (cropY >> 1)*(outData->Pitch >> 1);

    const mfxU8* pSrc[2] = {(mfxU8*)inData->Y + inOffset0,
        (mfxU8*)inData->UV+ inOffset1};

    mfxI32 pSrcStep[2] = {inData->Pitch,
        inData->Pitch >> 0};

    mfxU8* pDst[3]   = {(mfxU8*)outData->Y + outOffset0,
        (mfxU8*)outData->U + outOffset1,
        (mfxU8*)outData->V + outOffset1};

    mfxI32 pDstStep[3] = {outData->Pitch,
        outData->Pitch >> 1,
        outData->Pitch >> 1};

    //disable at the moment
    /*sts = ippiYCbCr420_8u_P2P3R(pSrc[0], pSrcStep[0],
    pSrc[1], pSrcStep[1],
    pDst,    pDstStep,
    roiSize);*/
    pSrc;
    pSrcStep;
    pDst;
    pDstStep;
    roiSize;

    return sts;

} // IppStatus cc_NV12_to_YV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)
/* ******************************************************************* */

/* EOF */
