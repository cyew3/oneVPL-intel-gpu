//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2013 Intel Corporation. All Rights Reserved.
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

void IncreaseReference(mfxFrameData *ptr)
{
    vm_interlocked_inc16((volatile Ipp16u*)&ptr->Locked);
}

void DecreaseReference(mfxFrameData *ptr)
{
    vm_interlocked_dec16((volatile Ipp16u*)&ptr->Locked);
}

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
vm_char* FourCC2Str( mfxU32 FourCC )
{
    vm_char* strFourCC = VM_STRING("YV12");//default

    switch ( FourCC )
    {
    case MFX_FOURCC_NV12:
        strFourCC = VM_STRING("NV12");
        break;

    case MFX_FOURCC_YV12:
        strFourCC = VM_STRING("YV12");
        break;

    case MFX_FOURCC_YUY2:
        strFourCC = VM_STRING("YUY2");
        break;

    case MFX_FOURCC_RGB3:
        strFourCC = VM_STRING("RGB3");
        break;

    case MFX_FOURCC_RGB4:
        strFourCC = VM_STRING("RGB4");
        break;

    case MFX_FOURCC_YUV400:
        strFourCC = VM_STRING("YUV400");
        break;

    case MFX_FOURCC_YUV411:
        strFourCC = VM_STRING("YUV411");
        break;

    case MFX_FOURCC_YUV422H:
        strFourCC = VM_STRING("YUV422H");
        break;

    case MFX_FOURCC_YUV422V:
        strFourCC = VM_STRING("YUV422V");
        break;

    case MFX_FOURCC_YUV444:
        strFourCC = VM_STRING("YUV444");
        break;

    case MFX_FOURCC_P010:
        strFourCC = VM_STRING("P010");
        break;
    case MFX_FOURCC_P210:
        strFourCC = VM_STRING("P210");
        break;
    case MFX_FOURCC_NV16:
        strFourCC = VM_STRING("NV16");
        break;
    case MFX_FOURCC_A2RGB10:
        strFourCC = VM_STRING("A2RGB10");
        break;
    case MFX_FOURCC_UYVY:
        strFourCC = VM_STRING("UYVY");
        break;

    }

    return strFourCC;
}

vm_char* IOpattern2Str( mfxU32 IOpattern)
{
    vm_char* strIOpattern = VM_STRING("Not defined");

    switch ( IOpattern )
    {
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY:
        strIOpattern = VM_STRING("sys_to_sys");
        break;

    case MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY:
        strIOpattern = VM_STRING("sys_to_d3d");
        break;

    case MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY:
        strIOpattern = VM_STRING("d3d_to_sys");
        break;
    case MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY:
        strIOpattern = VM_STRING("d3d_to_d3d");
        break;
    default:
        break;
    }

    return strIOpattern;
}

static 
vm_char* sptr2Str( mfxU32 sptr)
{
    vm_char* str_sptr = VM_STRING("Not defined");

    switch ( sptr)
    {
    case NO_PTR:
        str_sptr = VM_STRING("MemID is used for in and out");
        break;

    case INPUT_PTR:
        str_sptr = VM_STRING("Ptr is used for in, MemID for out");
        break;

    case OUTPUT_PTR:
        str_sptr = VM_STRING("Ptr is used for out, MemID for in");
        break;
    case ALL_PTR:
        str_sptr = VM_STRING("Ptr is used for in and out");
        break;
    default:
        break;
    }
    return str_sptr;
}

/* ******************************************************************* */

//static 
vm_char* PicStruct2Str( mfxU16  PicStruct )
{
    vm_char* strPicStruct = NULL;

    if(PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
    {
        strPicStruct = VM_STRING("progressive");
    }
    else if(PicStruct == MFX_PICSTRUCT_FIELD_TFF)
    {
        strPicStruct = VM_STRING("interlace (TFF)");
    }
    else if(PicStruct == MFX_PICSTRUCT_FIELD_BFF)
    {
        strPicStruct = VM_STRING("interlace (BFF)");
    }
    else if(PicStruct == MFX_PICSTRUCT_UNKNOWN)
    {
        strPicStruct = VM_STRING("unknown");
    }
    else
    {
        strPicStruct = VM_STRING("interlace (no detail)");
    }

    return strPicStruct;
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
    vm_string_printf(VM_STRING("Deinterlace\t%s\n"), (pParams->frameInfo[VPP_IN].PicStruct != pParams->frameInfo[VPP_OUT].PicStruct) ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("Denoise\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->denoiseParam.mode) ? VM_STRING("ON"): VM_STRING("OFF"));

    vm_string_printf(VM_STRING("SceneDetection\t%s\n"),    (VPP_FILTER_DISABLED != pParams->vaParam.mode)      ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("StructDetection\t%s\n"),(VPP_FILTER_DISABLED != pParams->idetectParam.mode)      ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("VarianceReport\t%s\n"),    (VPP_FILTER_DISABLED != pParams->varianceParam.mode)      ? VM_STRING("ON"): VM_STRING("OFF"));

    vm_string_printf(VM_STRING("ProcAmp\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->procampParam.mode) ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("DetailEnh\t%s\n"),      (VPP_FILTER_DISABLED != pParams->detailParam.mode)  ? VM_STRING("ON"): VM_STRING("OFF"));
    if(VPP_FILTER_DISABLED != pParams->frcParam.mode)
    {
        if(MFX_FRCALGM_FRAME_INTERPOLATION == pParams->frcParam.algorithm)
        {
            vm_string_printf(VM_STRING("FRC:Interp\tON\n"));
        }
        else if(MFX_FRCALGM_DISTRIBUTED_TIMESTAMP == pParams->frcParam.algorithm)
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
    vm_string_printf(VM_STRING("GamutMapping \t%s\n"),       (VPP_FILTER_DISABLED != pParams->gamutParam.mode)      ? VM_STRING("ON"): VM_STRING("OFF") );
    vm_string_printf(VM_STRING("ColorSaturation\t%s\n"),     (VPP_FILTER_DISABLED != pParams->tccParam.mode)   ? VM_STRING("ON"): VM_STRING("OFF") );
    vm_string_printf(VM_STRING("ContrastEnh  \t%s\n"),       (VPP_FILTER_DISABLED != pParams->aceParam.mode)   ? VM_STRING("ON"): VM_STRING("OFF") );
    vm_string_printf(VM_STRING("SkinToneEnh  \t%s\n"),       (VPP_FILTER_DISABLED != pParams->steParam.mode)       ? VM_STRING("ON"): VM_STRING("OFF") );
    vm_string_printf(VM_STRING("MVC mode    \t%s\n"),       (VPP_FILTER_DISABLED != pParams->multiViewParam.mode)  ? VM_STRING("ON"): VM_STRING("OFF") );
    // SVC mode
    vm_string_printf(VM_STRING("SVC mode    \t%s\n"),        (VPP_FILTER_DISABLED != pParams->svcParam.mode)  ? VM_STRING("ON"): VM_STRING("OFF") );
    // MSDK 6.0
    vm_string_printf(VM_STRING("ImgStab    \t%s\n"),            (VPP_FILTER_DISABLED != pParams->istabParam.mode)  ? VM_STRING("ON"): VM_STRING("OFF") );
    vm_string_printf(VM_STRING("\n"));

    vm_string_printf(VM_STRING("IOpattern type               \t%s\n"), IOpattern2Str( pParams->IOPattern ));
    vm_string_printf(VM_STRING("Pointers and MemID settings  \t%s\n"), sptr2Str( pParams->sptr ));
    vm_string_printf(VM_STRING("Default allocator            \t%s\n"), pParams->bDefAlloc ? VM_STRING("ON"): VM_STRING("OFF"));
    vm_string_printf(VM_STRING("Number of asynchronious tasks\t%d\n"), pParams->asyncNum);
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
mfxStatus InitParamsVPP(mfxVideoParam* pParams, sInputParams* pInParams)
{
    CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);
    CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR);

    if (pInParams->frameInfo[VPP_IN].nWidth == 0 || pInParams->frameInfo[VPP_IN].nHeight == 0 ){
        return MFX_ERR_UNSUPPORTED;
    }
    if (pInParams->frameInfo[VPP_OUT].nWidth == 0 || pInParams->frameInfo[VPP_OUT].nHeight == 0 ){
        return MFX_ERR_UNSUPPORTED;
    }

    memset(pParams, 0, sizeof(mfxVideoParam));                                   

    /* input data */  
    pParams->vpp.In.Shift           = pInParams->frameInfo[VPP_IN].Shift;
    pParams->vpp.In.FourCC          = pInParams->frameInfo[VPP_IN].FourCC;
    pParams->vpp.In.ChromaFormat    = MFX_CHROMAFORMAT_YUV420;  

    pParams->vpp.In.CropX = pInParams->frameInfo[VPP_IN].CropX;
    pParams->vpp.In.CropY = pInParams->frameInfo[VPP_IN].CropY; 
    pParams->vpp.In.CropW = pInParams->frameInfo[VPP_IN].CropW;
    pParams->vpp.In.CropH = pInParams->frameInfo[VPP_IN].CropH;

    // width must be a multiple of 16 
    // height must be a multiple of 16 in case of frame picture and 
    // a multiple of 32 in case of field picture  
    pParams->vpp.In.Width = ALIGN16(pInParams->frameInfo[VPP_IN].nWidth);
    pParams->vpp.In.Height= (MFX_PICSTRUCT_PROGRESSIVE == pInParams->frameInfo[VPP_IN].PicStruct)?
        ALIGN16(pInParams->frameInfo[VPP_IN].nHeight) : ALIGN32(pInParams->frameInfo[VPP_IN].nHeight);

    pParams->vpp.In.PicStruct = pInParams->frameInfo[VPP_IN].PicStruct;  

    ConvertFrameRate(pInParams->frameInfo[VPP_IN].dFrameRate, 
        &pParams->vpp.In.FrameRateExtN, 
        &pParams->vpp.In.FrameRateExtD);

    /* output data */
    pParams->vpp.Out.Shift           = pInParams->frameInfo[VPP_OUT].Shift;
    pParams->vpp.Out.FourCC          = pInParams->frameInfo[VPP_OUT].FourCC;      
    pParams->vpp.Out.ChromaFormat    = MFX_CHROMAFORMAT_YUV420;             

    pParams->vpp.Out.CropX = pInParams->frameInfo[VPP_OUT].CropX;
    pParams->vpp.Out.CropY = pInParams->frameInfo[VPP_OUT].CropY; 
    pParams->vpp.Out.CropW = pInParams->frameInfo[VPP_OUT].CropW;
    pParams->vpp.Out.CropH = pInParams->frameInfo[VPP_OUT].CropH;

    // width must be a multiple of 16 
    // height must be a multiple of 16 in case of frame picture and 
    // a multiple of 32 in case of field picture  
    pParams->vpp.Out.Width = ALIGN16(pInParams->frameInfo[VPP_OUT].nWidth); 
    pParams->vpp.Out.Height= (MFX_PICSTRUCT_PROGRESSIVE == pInParams->frameInfo[VPP_OUT].PicStruct)?
        ALIGN16(pInParams->frameInfo[VPP_OUT].nHeight) : ALIGN32(pInParams->frameInfo[VPP_OUT].nHeight);
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

    pParams->vpp.Out.PicStruct = pInParams->frameInfo[VPP_OUT].PicStruct;

    ConvertFrameRate(pInParams->frameInfo[VPP_OUT].dFrameRate, 
        &pParams->vpp.Out.FrameRateExtN, 
        &pParams->vpp.Out.FrameRateExtD);

    pParams->IOPattern = pInParams->IOPattern;

    // async depth
    pParams->AsyncDepth = pInParams->asyncNum;

    if(VPP_FILTER_DISABLED != pInParams->svcParam.mode)
    {
        for(mfxU32 did = 0; did < 8; did++)
        {
            // aya: svc crop processing should be corrected if crop will be enabled via cmp line options
            pInParams->svcParam.descr[did].cropX = 0;
            pInParams->svcParam.descr[did].cropY = 0;
            pInParams->svcParam.descr[did].cropW = pInParams->svcParam.descr[did].width;
            pInParams->svcParam.descr[did].cropH = pInParams->svcParam.descr[did].height;

            // it is OK
            pInParams->svcParam.descr[did].height = ALIGN16(pInParams->svcParam.descr[did].height);
            pInParams->svcParam.descr[did].width  = ALIGN16(pInParams->svcParam.descr[did].width);
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
    sts = pProcessor->mfxSession.Init(impl, &version);
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

    mfxFrameAllocResponse *response = new mfxFrameAllocResponse;
    
    sts = pAllocator->pMfxAllocator->Alloc(pAllocator->pMfxAllocator->pthis, pRequest, response);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to Alloc frames\n"));  WipeMemoryAllocator(pAllocator);});

    pAllocator->response.push_back(*response);
    nFrames = pAllocator->response[indx].NumFrameActual;
    pAllocator->pSurfaces.push_back(new mfxFrameSurface1[nFrames]);

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
    mfxU32 indx, 
    bool isPtr,
    sSVCLayerDescr* pSvcDesc
    )
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU16    nFrames = pAllocator->response[indx].NumFrameActual, i, did;
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
    sInputParams* pInParams,
    bool needCreate,
    int poolIndex)
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

    if ( needCreate )
    {
        pAllocator->pMfxAllocator =  new GeneralAllocator;
    }
    bool isHWLib       = (MFX_IMPL_HARDWARE & pInParams->ImpLib) ? true : false;
    bool isExtVideoMem = (pInParams->IOPattern != (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) ? true : false;

    bool isNeedExtAllocator = (isHWLib || isExtVideoMem);

    if( isNeedExtAllocator )
    {
#ifdef D3D_SURFACES_SUPPORT
        if( ((MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9) == pInParams->ImpLib) || (ALLOC_IMPL_VIA_D3D9 == pInParams->vaType) )
        {
            D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;
            if ( needCreate)
            {
                // prepare device manager
                sts = CreateDeviceManager(&(pAllocator->pd3dDeviceManager));
                CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to CreateDeviceManager\n")); WipeMemoryAllocator(pAllocator);});
            }
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

            if ( needCreate)
            {
                // prepare device manager
                sts = CreateD3D11Device(&(pAllocator->pD3D11Device), &(pAllocator->pD3D11DeviceContext));
                CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to CreateD3D11Device\n"));  WipeMemoryAllocator(pAllocator);});
            }
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

        if ( needCreate)
        {
            p_vaapiAllocParams->m_dpy = pAllocator->libvaKeeper->GetVADisplay();
            pAllocator->pAllocatorParams = p_vaapiAllocParams;
        }
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
    //if ( needCreate)
    {
        sts = pAllocator->pMfxAllocator->Init(pAllocator->pAllocatorParams);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to pMfxAllocator->Init\n"));  WipeMemoryAllocator(pAllocator);});
    }

    return MFX_ERR_NONE;
}


mfxStatus AllocateSurfaces(
    sFrameProcessor* pProcessor, 
    sMemoryAllocator* pAllocator, 
    mfxVideoParam* pParams, 
    sInputParams* pInParams,
    mfxU16 &additionalInputSurfs,
    mfxU16 &additionalOutputSurfs,
    int poolIndex)
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


    // see the spec
    // (1) MFXInit()
    // (2) MFXQueryIMPL()
    // (3) MFXVideoCORE_SetHandle()
    // after (1-3), call of any MSDK function is OK
    sts = pProcessor->pmfxVPP->QueryIOSurf(pParams, request);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed in QueryIOSurf\n"));  WipeMemoryAllocator(pAllocator);});

    // alloc frames for vpp
    // [IN]
    mfxU16 numInputSurfs = 0;
    numInputSurfs = request[VPP_IN].NumFrameSuggested;
    request[VPP_IN].NumFrameMin       += additionalInputSurfs;
    request[VPP_IN].NumFrameSuggested += additionalInputSurfs;

    additionalInputSurfs = numInputSurfs;
    sts = InitSurfaces(pAllocator, &(request[VPP_IN]), &(pParams->vpp.In), poolIndex + VPP_IN, isInPtr);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to InitSurfaces\n"));  WipeMemoryAllocator(pAllocator);});

    // [OUT]

    mfxU16 numOutputSurfs = 0;
    numOutputSurfs = request[VPP_OUT].NumFrameSuggested;
    request[VPP_OUT].NumFrameMin       += additionalOutputSurfs;
    request[VPP_OUT].NumFrameSuggested += additionalOutputSurfs;
    additionalOutputSurfs   = numOutputSurfs;
    if( VPP_FILTER_DISABLED == pInParams->svcParam.mode )
    {
        sts = InitSurfaces(
            pAllocator, 
            &(request[VPP_OUT]), 
            &(pParams->vpp.Out), 
            poolIndex + VPP_OUT, 
            isOutPtr);
    }
    else
    {
        sts = InitSvcSurfaces(
            pAllocator, 
            &(request[VPP_OUT]), 
            &(pParams->vpp.Out), 
            poolIndex + VPP_OUT, 
            isOutPtr,
            &(pInParams->svcParam.descr[0]));
    }
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to InitSurfaces\n"));  WipeMemoryAllocator(pAllocator);});

    return MFX_ERR_NONE;
}


mfxStatus QuerySurfaces(
    sFrameProcessor* pProcessor, 
    sMemoryAllocator* pAllocator, 
    mfxVideoParam* pParams, 
    sInputParams* pInParams,
    mfxU16 &additionalInputSurfs,
    mfxU16 &additionalOutputSurfs,
    int poolIndex)
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


    // see the spec
    // (1) MFXInit()
    // (2) MFXQueryIMPL()
    // (3) MFXVideoCORE_SetHandle()
    // after (1-3), call of any MSDK function is OK
    sts = pProcessor->pmfxVPP->QueryIOSurf(pParams, request);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed in QueryIOSurf\n"));  WipeMemoryAllocator(pAllocator);});

    // alloc frames for vpp
    // [IN]
    mfxU16 numInputSurfs = 0;
    numInputSurfs = request[VPP_IN].NumFrameSuggested;
    request[VPP_IN].NumFrameMin       += additionalInputSurfs;
    request[VPP_IN].NumFrameSuggested += additionalInputSurfs;
    additionalInputSurfs = numInputSurfs;
    //sts = InitSurfaces(pAllocator, &(request[VPP_IN]), &(pParams->vpp.In), poolIndex + VPP_IN, isInPtr);
    //CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to InitSurfaces\n"));  WipeMemoryAllocator(pAllocator);});

    // [OUT]

    mfxU16 numOutputSurfs = 0;
    numOutputSurfs = request[VPP_OUT].NumFrameSuggested;
    request[VPP_IN].NumFrameMin       += additionalOutputSurfs;
    request[VPP_IN].NumFrameSuggested += additionalOutputSurfs;
    additionalOutputSurfs   = numOutputSurfs;
    

    return MFX_ERR_NONE;
}

/* ******************************************************************* */

mfxStatus InitResources(sAppResources* pResources, mfxVideoParam* pParams, sInputParams* pInParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    CHECK_POINTER(pResources, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);
    sts = CreateFrameProcessor(pResources->pProcessor, pParams, pInParams);
    return sts;
}

mfxStatus InitAllocator(sAppResources* pResources, mfxVideoParam* pParams, sInputParams* pInParams, bool needCreate, int poolIndex)
{
    mfxStatus sts = MFX_ERR_NONE;

    CHECK_POINTER(pResources, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);

    sts = InitMemoryAllocator(pResources->pProcessor, pResources->pAllocator, pParams, pInParams, needCreate, poolIndex);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to InitMemoryAllocator\n")); WipeResources(pResources);});

    return sts;
}

mfxStatus InitVPP(sAppResources* pResources, mfxVideoParam* pParams, sInputParams* pInParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    pInParams;
    CHECK_POINTER(pResources, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);

    sts = InitFrameProcessor(pResources->pProcessor, pParams);

    if (MFX_WRN_PARTIAL_ACCELERATION == sts || MFX_WRN_FILTER_SKIPPED == sts)
        return sts;
    else
    {
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { vm_string_printf(VM_STRING("Failed to InitFrameProcessor\n")); WipeResources(pResources);});
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


void WipeResources(sAppResources* Resources)
{
    //WipeFrameProcessor(Resources.front()->pProcessor);
    //WipeMemoryAllocator(Resources.front()->pAllocator);

    //for ( int i = 0; i < Resources.size(); i++)
    //{
    //    if (Resources[i]->pSrcFileReader)
    //    {
    //        Resources[i]->pSrcFileReader->Close();
    //    }

    //    if (Resources[i]->pDstFileWriter)
    //    {
    //        Resources[i]->pDstFileWriter->Close();
    //    }

    //    WipeConfigParam( Resources[i] );
    //}

} // void WipeResources(sAppResources* pResources)

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
    else 
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}


mfxStatus CRawVideoReader::GetNextInputFrame(sMemoryAllocator* pAllocator, mfxFrameInfo* pInfo, mfxFrameSurface1** pSurface, mfxU32 poolIndex)
{
    mfxStatus sts;
    if (!m_isPerfMode)
    {
        GetFreeSurface(pAllocator->pSurfaces[poolIndex + VPP_IN], pAllocator->response[poolIndex + VPP_IN].NumFrameActual, pSurface);
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


int sAppResources::Process()
{
    mfxStatus             sts = MFX_ERR_NONE;
    bool                  bFrameNumLimit = false;
    mfxFrameSurface1*     pSurf[3];// 0 - in, 1 - out, 2 - work
    mfxU32                numGetFrames = 0;
    mfxSyncPoint          syncPoint;

    if (m_pParams->bPerf && m_pSrcFileReader)
    {
        sts = m_pSrcFileReader->PreAllocateFrameChunk(&m_mfxParamsVideo, m_pParams, pAllocator->pMfxAllocator);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to yuvReader.PreAllocateFrameChunk\n")); WipeResources(this);});
    }
    else if (m_pParams->numFrames)
    {
        bFrameNumLimit = true;
    }

    sts = MFX_ERR_NONE;
    nFrames = 0;

    mfxU32 StartJumpFrame = 13;
#if 0 
    // need to correct jumping parameters due to async mode
    if (ptsMaker.get() && Params.front()->ptsJump && (Params.front()->asyncNum > 1))
    {
        if (Params.front()->asyncNum > StartJumpFrame)
        {
            StartJumpFrame = Params.front()->asyncNum;
        }
        else
        {
            StartJumpFrame = StartJumpFrame/Params.front()->asyncNum*Params.front()->asyncNum;
        }
    }
#endif

    bool bDoNotUpdateIn = false;
    if(m_pParams->use_extapi)
        bDoNotUpdateIn = true;

    bool bSvcMode          = (VPP_FILTER_ENABLED_CONFIGURED == m_pParams->svcParam.mode) ? true : false;
    mfxU32 lastActiveLayer = 0;
    for(mfxI32 did = 7; did >= 0; did--)
    {
        if( m_pParams->svcParam.descr[did].active )
        {
            lastActiveLayer = did;
            break;
        }
    }

    // pre-multi-view preparation
    bool bMultiView   = (VPP_FILTER_ENABLED_CONFIGURED == m_pParams->multiViewParam.mode) ? true : false;
    std::vector<bool> bMultipleOutStore(m_pParams->multiViewParam.viewCount, false);  
    mfxFrameSurface1* viewSurfaceStore[MULTI_VIEW_COUNT_MAX];

    ViewGenerator  viewGenerator( (bSvcMode) ? 8 : m_pParams->multiViewParam.viewCount );

    if( bMultiView )
    {
        memset(viewSurfaceStore, 0, m_pParams->multiViewParam.viewCount * sizeof( mfxFrameSurface1* )); 

        if( bFrameNumLimit )
        {
            m_pParams->numFrames *= m_pParams->multiViewParam.viewCount;
        }
    }
//---------------------------------------------------------

    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || bDoNotUpdateIn )
    {
        mfxU16 viewID = 0;
        mfxU16 viewIndx = 0;
        mfxU16 did = 0;

        if( bSvcMode )
        {
            do{
                did = viewID = viewGenerator.GetNextViewID();
            } while (0 == m_pParams->svcParam.descr[did].active);
        }

        // update for multiple output of multi-view
        if( bMultiView )
        {
            viewID = viewGenerator.GetNextViewID();
            viewIndx = viewGenerator.GetViewIndx( viewID );

            if( bMultipleOutStore[viewIndx] )
            {              
                pSurf[VPP_IN] = viewSurfaceStore[viewIndx];
                bDoNotUpdateIn = true;           

                bMultipleOutStore[viewIndx] = false;
            }
            else
            {
                bDoNotUpdateIn = false;  
            }
        }

        if( !bDoNotUpdateIn )
        {
            sts = m_pSrcFileReader->GetNextInputFrame(
                this->pAllocator, 
                &(this->realFrameInfo[VPP_IN]), 
                &pSurf[VPP_IN],
                this->indexInPipe);
            BREAK_ON_ERROR(sts);

            if( bMultiView )
            {
                pSurf[VPP_IN]->Info.FrameId.ViewId = viewID;
            }

            if (numGetFrames++ == m_pParams->numFrames && bFrameNumLimit)
            {
                sts = MFX_ERR_MORE_DATA;
                break;
            }
        }

        // VPP processing    
        bDoNotUpdateIn = false;

        if( !bSvcMode )
        {
            sts = GetFreeSurface(
                this->pAllocator->pSurfaces[VPP_OUT + indexInPipe], 
                this->pAllocator->response[VPP_OUT + indexInPipe].NumFrameActual , 
                &pSurf[this->work_surf]);
            BREAK_ON_ERROR(sts);
        }
        else
        {
            sts = GetFreeSurface(
                this->pAllocator->pSvcSurfaces[did], 
                this->pAllocator->svcResponse[did].NumFrameActual , 
                &pSurf[this->work_surf]);
            BREAK_ON_ERROR(sts);
        }

        if( this->bROITest[VPP_IN] )
        {
            this->inROIGenerator.SetROI(  &(pSurf[VPP_IN]->Info) );
        }
        if( this->bROITest[VPP_OUT] )
        {
            this->outROIGenerator.SetROI( &(pSurf[this->work_surf]->Info) );
        }

        mfxExtVppAuxData* pExtData = NULL;
        if( VPP_FILTER_DISABLED != m_pParams->vaParam.mode || 
            VPP_FILTER_DISABLED != m_pParams->varianceParam.mode || 
            VPP_FILTER_DISABLED != m_pParams->idetectParam.mode)
        {
            pExtData = reinterpret_cast<mfxExtVppAuxData*>(&this->pExtVPPAuxData[this->surfStore.m_SyncPoints.size()]);
        }       

        if ( m_pParams->use_extapi ) 
        {
            sts = this->pProcessor->pmfxVPP->RunFrameVPPAsyncEx( 
                pSurf[VPP_IN], 
                pSurf[VPP_WORK],
                //pExtData, 
                &pSurf[VPP_OUT],
                &syncPoint);

            while(MFX_WRN_DEVICE_BUSY == sts)
            {
                vm_time_sleep(500);
                sts = this->pProcessor->pmfxVPP->RunFrameVPPAsyncEx( 
                    pSurf[VPP_IN], 
                    pSurf[VPP_WORK],
                    //pExtData, 
                    &pSurf[VPP_OUT],
                    &syncPoint);
            }

            if(MFX_ERR_MORE_DATA != sts)
                bDoNotUpdateIn = true;
        }
        else 
        {
            sts = this->pProcessor->pmfxVPP->RunFrameVPPAsync( 
                pSurf[VPP_IN], 
                pSurf[VPP_OUT],
                pExtData, 
                &syncPoint);
        }
        
        if (MFX_ERR_MORE_DATA == sts)
        {
            if(bSvcMode)
            {
                if( lastActiveLayer != did )
                {
                    bDoNotUpdateIn = true;
                }
            }
            continue;
        }

        //MFX_ERR_MORE_SURFACE (&& !use_extapi) means output is ready but need more surface
        //because VPP produce multiple out. example: Frame Rate Conversion 30->60
        //MFX_ERR_MORE_SURFACE && use_extapi means that component need more work surfaces
        if (MFX_ERR_MORE_SURFACE == sts)
        {       
            sts = MFX_ERR_NONE;

            if( bMultiView )
            {                      
                if( viewSurfaceStore[viewIndx] )
                {
                    DecreaseReference( &(viewSurfaceStore[viewIndx]->Data) );
                }

                viewSurfaceStore[viewIndx] = pSurf[VPP_IN];
                IncreaseReference( &(viewSurfaceStore[viewIndx]->Data) );            

                bMultipleOutStore[viewIndx] = true;
            }
            else
            {
                bDoNotUpdateIn = true;
            }
            
            if ( m_pParams->use_extapi )
            {
                // RunFrameAsyncEx is used
                continue;
            }
        }
#if 0
        else if (MFX_ERR_NONE == sts && !((nFrames+1)% StartJumpFrame) && ptsMaker.get() && Params.ptsJump) // pts jump 
        {
            ptsMaker.get()->JumpPTS();
        }
#endif
        else if ( MFX_ERR_NONE == sts && bMultiView )
        {       
            if( viewSurfaceStore[viewIndx] )
            {
                DecreaseReference( &(viewSurfaceStore[viewIndx]->Data) );
                viewSurfaceStore[viewIndx] = NULL;
            }
        }
        else if(bSvcMode)
        {
            if( lastActiveLayer != did )
            {
                bDoNotUpdateIn = true;
            }
        }

        BREAK_ON_ERROR(sts);
        this->surfStore.m_SyncPoints.push_back(SurfaceVPPStore::SyncPair(syncPoint,pSurf[VPP_OUT]));
        IncreaseReference(&pSurf[VPP_OUT]->Data);
        if (this->surfStore.m_SyncPoints.size() != (size_t)(m_pParams->asyncNum * m_pParams->multiViewParam.viewCount))
        {
            continue;
        }

        sts = OutputProcessFrame(*this, this->realFrameInfo, nFrames);
        BREAK_ON_ERROR(sts);

    } // main while loop
//---------------------------------------------------------



    //process remain sync points 
    if (MFX_ERR_MORE_DATA == sts)
    {
        sts = OutputProcessFrame(*this, this->realFrameInfo, nFrames);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to process remain sync points\n")); WipeResources(this);});
    }

    // means that file has ended, need to go to buffering loop
    IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Exit in case of other errors\n")); WipeResources(this);});


    // loop to get buffered frames from VPP
    while (MFX_ERR_NONE <= sts)
    {
        mfxU32 did;

        if( bSvcMode )
        {
            do{
                did = viewGenerator.GetNextViewID();
            } while (0 == m_pParams->svcParam.descr[did].active);
        }

        /*sts = GetFreeSurface(
            Allocator.pSurfaces[VPP_OUT], 
            Allocator.response[VPP_OUT].NumFrameActual, 
            &pSurf[VPP_OUT]);
        BREAK_ON_ERROR(sts);*/
        if( !bSvcMode )
        {
            sts = GetFreeSurface(
                this->pAllocator->pSurfaces[VPP_OUT], 
                this->pAllocator->response[VPP_OUT].NumFrameActual , 
                &pSurf[this->work_surf]);
            BREAK_ON_ERROR(sts);
        }
        else
        {
            sts = GetFreeSurface(
                this->pAllocator->pSvcSurfaces[did], 
                this->pAllocator->svcResponse[did].NumFrameActual , 
                &pSurf[this->work_surf]);
            BREAK_ON_ERROR(sts);
        }

        bDoNotUpdateIn = false;

        if ( m_pParams->use_extapi )
        {
            sts = this->pProcessor->pmfxVPP->RunFrameVPPAsyncEx( 
                NULL, 
                pSurf[VPP_WORK],
                //(VPP_FILTER_DISABLED != Params.vaParam.mode || VPP_FILTER_DISABLED != Params.varianceParam.mode)? reinterpret_cast<mfxExtVppAuxData*>(&extVPPAuxData[0]) : NULL, 
                &pSurf[VPP_OUT],
                &syncPoint );
            while(MFX_WRN_DEVICE_BUSY == sts)
            {
                vm_time_sleep(500);
                sts = this->pProcessor->pmfxVPP->RunFrameVPPAsyncEx( 
                    NULL, 
                    pSurf[VPP_WORK],
                    //(VPP_FILTER_DISABLED != Params.vaParam.mode || VPP_FILTER_DISABLED != Params.varianceParam.mode)? reinterpret_cast<mfxExtVppAuxData*>(&extVPPAuxData[0]) : NULL, 
                    &pSurf[VPP_OUT],
                    &syncPoint );
            }
        }
        else                     
        {
            sts = this->pProcessor->pmfxVPP->RunFrameVPPAsync( 
                NULL, 
                pSurf[VPP_OUT],
                (VPP_FILTER_DISABLED != m_pParams->vaParam.mode || VPP_FILTER_DISABLED != m_pParams->varianceParam.mode)? reinterpret_cast<mfxExtVppAuxData*>(& this->pExtVPPAuxData[0]) : NULL, 
                &syncPoint );
        }
        IGNORE_MFX_STS(sts, MFX_ERR_MORE_SURFACE);
        BREAK_ON_ERROR(sts);

        sts = this->pProcessor->mfxSession.SyncOperation(
            syncPoint, 
            VPP_WAIT_INTERVAL);
        if(sts)
            vm_string_printf(VM_STRING("SyncOperation wait interval exceeded\n"));
        BREAK_ON_ERROR(sts);

        sts = m_pDstFileWriter->PutNextFrame(
            this->pAllocator, 
            (bSvcMode) ?  &(this->realSvcOutFrameInfo[pSurf[VPP_OUT]->Info.FrameId.DependencyId]) : &(this->realFrameInfo[VPP_OUT]), 
            pSurf[VPP_OUT]);
        if(sts)
            vm_string_printf(VM_STRING("Failed to write frame to disk\n"));
        CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

        if(bSvcMode)
        {
            if( lastActiveLayer != did )
            {
                bDoNotUpdateIn = true;
            }
        }

        nFrames++;

        //VPP progress
        if( VPP_FILTER_DISABLED == m_pParams->vaParam.mode )
        {
            if (!m_pParams->bPerf)
                vm_string_printf(VM_STRING("Frame number: %d\r"), nFrames);
            else
            {
                if (!(nFrames % 100))
                    vm_string_printf(VM_STRING("."));
            }
        } 
        else 
        {
            if (!m_pParams->bPerf)
                vm_string_printf(VM_STRING("Frame number: %d, spatial: %hd, temporal: %d, scd: %d \n"), nFrames,
                this->pExtVPPAuxData[0].SpatialComplexity,
                this->pExtVPPAuxData[0].TemporalComplexity,
                this->pExtVPPAuxData[0].SceneChangeRate);
            else
            {
                if (!(nFrames % 100))
                    vm_string_printf(VM_STRING("."));

            }
        }
    }
}


int sAppResources::ProcessInChain(mfxFrameSurface1 *inputSurface)
{
    mfxStatus sts = MFX_ERR_NONE;
    bool                         bFrameNumLimit = false;
    mfxFrameSurface1*   pSurf[3];// 0 - in, 1 - out, 2 - work
    mfxU32 numGetFrames = 0;
    mfxSyncPoint        syncPoint;

    mfxU32 StartJumpFrame = 13;

#if 0 
    // need to correct jumping parameters due to async mode
    if (ptsMaker.get() && Params.front()->ptsJump && (Params.front()->asyncNum > 1))
    {
        if (Params.front()->asyncNum > StartJumpFrame)
        {
            StartJumpFrame = Params.front()->asyncNum;
        }
        else
        {
            StartJumpFrame = StartJumpFrame/Params.front()->asyncNum*Params.front()->asyncNum;
        }
    }

#endif
    bool bDoNotUpdateIn = false;
    if(m_pParams->use_extapi)
        bDoNotUpdateIn = true;

    bool bSvcMode          = (VPP_FILTER_ENABLED_CONFIGURED == m_pParams->svcParam.mode) ? true : false;
    mfxU32 lastActiveLayer = 0;
    for(mfxI32 did = 7; did >= 0; did--)
    {
        if( m_pParams->svcParam.descr[did].active )
        {
            lastActiveLayer = did;
            break;
        }
    }

    // pre-multi-view preparation
    bool bMultiView   = (VPP_FILTER_ENABLED_CONFIGURED == m_pParams->multiViewParam.mode) ? true : false;
    std::vector<bool> bMultipleOutStore(m_pParams->multiViewParam.viewCount, false);  
    mfxFrameSurface1* viewSurfaceStore[MULTI_VIEW_COUNT_MAX];

    ViewGenerator  viewGenerator( (bSvcMode) ? 8 : m_pParams->multiViewParam.viewCount );

    if( bMultiView )
    {
        memset(viewSurfaceStore, 0, m_pParams->multiViewParam.viewCount * sizeof( mfxFrameSurface1* )); 

        if( bFrameNumLimit )
        {
            m_pParams->numFrames *= m_pParams->multiViewParam.viewCount;
        }
    }
//---------------------------------------------------------


    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || bDoNotUpdateIn )
    {
        mfxU16 viewID = 0;
        mfxU16 viewIndx = 0;
        mfxU16 did = 0;

        if( bSvcMode )
        {
            do{
                did = viewID = viewGenerator.GetNextViewID();
            } while (0 == m_pParams->svcParam.descr[did].active);
        }

        // update for multiple output of multi-view
        if( bMultiView )
        {
            viewID = viewGenerator.GetNextViewID();
            viewIndx = viewGenerator.GetViewIndx( viewID );

            if( bMultipleOutStore[viewIndx] )
            {              
                pSurf[VPP_IN] = viewSurfaceStore[viewIndx];
                bDoNotUpdateIn = true;           

                bMultipleOutStore[viewIndx] = false;
            }
            else
            {
                bDoNotUpdateIn = false;  
            }
        }

        if( !bDoNotUpdateIn )
        {
            memcpy(&this->realFrameInfo[VPP_IN], &(inputSurface->Info), sizeof(mfxFrameInfo));
            pSurf[VPP_IN] = inputSurface;

            if( bMultiView )
            {
                pSurf[VPP_IN]->Info.FrameId.ViewId = viewID;
            }

            if (numGetFrames++ == m_pParams->numFrames && bFrameNumLimit)
            {
                sts = MFX_ERR_MORE_DATA;
                break;
            }
        }

        // VPP processing    
        bDoNotUpdateIn = false;

        if( !bSvcMode )
        {
            sts = GetFreeSurface(
                this->pAllocator->pSurfaces[VPP_OUT + indexInPipe], 
                this->pAllocator->response[VPP_OUT + indexInPipe].NumFrameActual , 
                &pSurf[this->work_surf]);
            BREAK_ON_ERROR(sts);
        }
        else
        {
            sts = GetFreeSurface(
                this->pAllocator->pSvcSurfaces[did], 
                this->pAllocator->svcResponse[did].NumFrameActual , 
                &pSurf[this->work_surf]);
            BREAK_ON_ERROR(sts);
        }

        if( this->bROITest[VPP_IN] )
        {
            this->inROIGenerator.SetROI(  &(pSurf[VPP_IN]->Info) );
        }
        if( this->bROITest[VPP_OUT] )
        {
            this->outROIGenerator.SetROI( &(pSurf[this->work_surf]->Info) );
        }

        mfxExtVppAuxData* pExtData = NULL;
        if( VPP_FILTER_DISABLED != m_pParams->vaParam.mode || 
            VPP_FILTER_DISABLED != m_pParams->varianceParam.mode || 
            VPP_FILTER_DISABLED != m_pParams->idetectParam.mode)
        {
            pExtData = reinterpret_cast<mfxExtVppAuxData*>(&this->pExtVPPAuxData[this->surfStore.m_SyncPoints.size()]);
        }       

        if ( m_pParams->use_extapi ) 
        {
            sts = this->pProcessor->pmfxVPP->RunFrameVPPAsyncEx( 
                pSurf[VPP_IN], 
                pSurf[VPP_WORK],
                //pExtData, 
                &pSurf[VPP_OUT],
                &syncPoint);

            while(MFX_WRN_DEVICE_BUSY == sts)
            {
                vm_time_sleep(500);
                sts = this->pProcessor->pmfxVPP->RunFrameVPPAsyncEx( 
                    pSurf[VPP_IN], 
                    pSurf[VPP_WORK],
                    //pExtData, 
                    &pSurf[VPP_OUT],
                    &syncPoint);
            }

            if(MFX_ERR_MORE_DATA != sts)
                bDoNotUpdateIn = true;
        }
        else 
        {
            sts = this->pProcessor->pmfxVPP->RunFrameVPPAsync( 
                pSurf[VPP_IN], 
                pSurf[VPP_OUT],
                pExtData, 
                &syncPoint);
        }
        
        if (MFX_ERR_MORE_DATA == sts)
        {
            if(bSvcMode)
            {
                if( lastActiveLayer != did )
                {
                    bDoNotUpdateIn = true;
                }
            }
            continue;
        }

        //MFX_ERR_MORE_SURFACE (&& !use_extapi) means output is ready but need more surface
        //because VPP produce multiple out. example: Frame Rate Conversion 30->60
        //MFX_ERR_MORE_SURFACE && use_extapi means that component need more work surfaces
        if (MFX_ERR_MORE_SURFACE == sts)
        {       
            sts = MFX_ERR_NONE;

            if( bMultiView )
            {                      
                if( viewSurfaceStore[viewIndx] )
                {
                    DecreaseReference( &(viewSurfaceStore[viewIndx]->Data) );
                }

                viewSurfaceStore[viewIndx] = pSurf[VPP_IN];
                IncreaseReference( &(viewSurfaceStore[viewIndx]->Data) );            

                bMultipleOutStore[viewIndx] = true;
            }
            else
            {
                bDoNotUpdateIn = true;
            }
            
            if ( m_pParams->use_extapi )
            {
                // RunFrameAsyncEx is used
                continue;
            }
        }
#if 0
        else if (MFX_ERR_NONE == sts && !((nFrames+1)% StartJumpFrame) && ptsMaker.get() && Params.ptsJump) // pts jump 
        {
            ptsMaker.get()->JumpPTS();
        }
#endif
        else if ( MFX_ERR_NONE == sts && bMultiView )
        {       
            if( viewSurfaceStore[viewIndx] )
            {
                DecreaseReference( &(viewSurfaceStore[viewIndx]->Data) );
                viewSurfaceStore[viewIndx] = NULL;
            }
        }
        else if(bSvcMode)
        {
            if( lastActiveLayer != did )
            {
                bDoNotUpdateIn = true;
            }
        }

        BREAK_ON_ERROR(sts);
        this->surfStore.m_SyncPoints.push_back(SurfaceVPPStore::SyncPair(syncPoint,pSurf[VPP_OUT]));
        IncreaseReference(&pSurf[VPP_OUT]->Data);
        if (this->surfStore.m_SyncPoints.size() != (size_t)(m_pParams->asyncNum * m_pParams->multiViewParam.viewCount))
        {
            continue;
        }

        sts = OutputProcessFrame(*this, this->realFrameInfo, nFrames);
        if ( bDoNotUpdateIn )
            continue;

        return sts;

    } // main while loop
//---------------------------------------------------------



    //process remain sync points 
    if (MFX_ERR_MORE_DATA == sts)
    {
        sts = OutputProcessFrame(*this, this->realFrameInfo, nFrames);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to process remain sync points\n")); WipeResources(this);});
    }

    // means that file has ended, need to go to buffering loop
    IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Exit in case of other errors\n")); WipeResources(this);});


    // loop to get buffered frames from VPP
    while (MFX_ERR_NONE <= sts)
    {
        mfxU32 did;

        if( bSvcMode )
        {
            do{
                did = viewGenerator.GetNextViewID();
            } while (0 == m_pParams->svcParam.descr[did].active);
        }

        /*sts = GetFreeSurface(
            Allocator.pSurfaces[VPP_OUT], 
            Allocator.response[VPP_OUT].NumFrameActual, 
            &pSurf[VPP_OUT]);
        BREAK_ON_ERROR(sts);*/
        if( !bSvcMode )
        {
            sts = GetFreeSurface(
                this->pAllocator->pSurfaces[VPP_OUT], 
                this->pAllocator->response[VPP_OUT].NumFrameActual , 
                &pSurf[this->work_surf]);
            BREAK_ON_ERROR(sts);
        }
        else
        {
            sts = GetFreeSurface(
                this->pAllocator->pSvcSurfaces[did], 
                this->pAllocator->svcResponse[did].NumFrameActual , 
                &pSurf[this->work_surf]);
            BREAK_ON_ERROR(sts);
        }

        bDoNotUpdateIn = false;

        if ( m_pParams->use_extapi )
        {
            sts = this->pProcessor->pmfxVPP->RunFrameVPPAsyncEx( 
                NULL, 
                pSurf[VPP_WORK],
                //(VPP_FILTER_DISABLED != Params.vaParam.mode || VPP_FILTER_DISABLED != Params.varianceParam.mode)? reinterpret_cast<mfxExtVppAuxData*>(&extVPPAuxData[0]) : NULL, 
                &pSurf[VPP_OUT],
                &syncPoint );
            while(MFX_WRN_DEVICE_BUSY == sts)
            {
                vm_time_sleep(500);
                sts = this->pProcessor->pmfxVPP->RunFrameVPPAsyncEx( 
                    NULL, 
                    pSurf[VPP_WORK],
                    //(VPP_FILTER_DISABLED != Params.vaParam.mode || VPP_FILTER_DISABLED != Params.varianceParam.mode)? reinterpret_cast<mfxExtVppAuxData*>(&extVPPAuxData[0]) : NULL, 
                    &pSurf[VPP_OUT],
                    &syncPoint );
            }
        }
        else                     
        {
            sts = this->pProcessor->pmfxVPP->RunFrameVPPAsync( 
                NULL, 
                pSurf[VPP_OUT],
                (VPP_FILTER_DISABLED != m_pParams->vaParam.mode || VPP_FILTER_DISABLED != m_pParams->varianceParam.mode)? reinterpret_cast<mfxExtVppAuxData*>(& this->pExtVPPAuxData[0]) : NULL, 
                &syncPoint );
        }
        IGNORE_MFX_STS(sts, MFX_ERR_MORE_SURFACE);
        BREAK_ON_ERROR(sts);

        sts = this->pProcessor->mfxSession.SyncOperation(
            syncPoint, 
            VPP_WAIT_INTERVAL);
        if(sts)
            vm_string_printf(VM_STRING("SyncOperation wait interval exceeded\n"));
        BREAK_ON_ERROR(sts);

        sts = m_pDstFileWriter->PutNextFrame(
            this->pAllocator, 
            (bSvcMode) ?  &(this->realSvcOutFrameInfo[pSurf[VPP_OUT]->Info.FrameId.DependencyId]) : &(this->realFrameInfo[VPP_OUT]), 
            pSurf[VPP_OUT]);
        if(sts)
            vm_string_printf(VM_STRING("Failed to write frame to disk\n"));
        CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

        if(bSvcMode)
        {
            if( lastActiveLayer != did )
            {
                bDoNotUpdateIn = true;
            }
        }

        nFrames++;

        //VPP progress
        if( VPP_FILTER_DISABLED == m_pParams->vaParam.mode )
        {
            if (!m_pParams->bPerf)
                vm_string_printf(VM_STRING("Frame number: %d\r"), nFrames);
            else
            {
                if (!(nFrames % 100))
                    vm_string_printf(VM_STRING("."));
            }
        } 
        else 
        {
            if (!m_pParams->bPerf)
                vm_string_printf(VM_STRING("Frame number: %d, spatial: %hd, temporal: %d, scd: %d \n"), nFrames,
                this->pExtVPPAuxData[0].SpatialComplexity,
                this->pExtVPPAuxData[0].TemporalComplexity,
                this->pExtVPPAuxData[0].SceneChangeRate);
            else
            {
                if (!(nFrames % 100))
                    vm_string_printf(VM_STRING("."));

            }
        }
    }
}


mfxStatus sAppResources::OutputProcessFrame(
    sAppResources Resources, 
    mfxFrameInfo* frameInfo, 
    mfxU32 &nFrames)
{ 
    mfxStatus sts;
    mfxFrameSurface1*   pProcessedSurface;
    mfxU32 counter = 0;
    bool bSvcMode = false;

    for(int did = 0; did < 8; did++)
    {
        if( Resources.svcConfig.DependencyLayer[did].Active )
        {
            bSvcMode = true;
            break;
        }
    }

    for(;!Resources.pSurfStore->m_SyncPoints.empty();Resources.pSurfStore->m_SyncPoints.pop_front())
    {
        sts = Resources.pProcessor->mfxSession.SyncOperation(
            Resources.pSurfStore->m_SyncPoints.front().first, 
            VPP_WAIT_INTERVAL);
        if(sts)
            vm_string_printf(VM_STRING("SyncOperation wait interval exceeded\n"));
        CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

        pProcessedSurface = Resources.pSurfStore->m_SyncPoints.front().second.pSurface;

        DecreaseReference(&pProcessedSurface->Data);

        if ( Resources.m_pDstFileWriter )
        {
        sts = Resources.m_pDstFileWriter->PutNextFrame(
            Resources.pAllocator, 
            //(bSvcMode) ? &(pProcessedSurface->Info) : &(frameInfo[VPP_OUT]),
            (bSvcMode) ? &(Resources.realSvcOutFrameInfo[pProcessedSurface->Info.FrameId.DependencyId]) : &(frameInfo[VPP_OUT]),
            pProcessedSurface);
        if(sts)
            vm_string_printf(VM_STRING("Failed to write frame to disk\n"));
        
        }
        if ( Resources.next )
        {
            sts  = (mfxStatus) Resources.next->ProcessInChain(pProcessedSurface);
        }

        CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);
        nFrames++;

        //VPP progress
        if( VPP_FILTER_DISABLED == Resources.m_pParams->vaParam.mode && 
            VPP_FILTER_DISABLED == Resources.m_pParams->varianceParam.mode &&
            VPP_FILTER_DISABLED == Resources.m_pParams->idetectParam.mode)
        {
            if (!Resources.m_pParams->bPerf)
                vm_string_printf(VM_STRING("Frame number: %d\r"), nFrames);
            else
            {
                if (!(nFrames % 100))
                    vm_string_printf(VM_STRING("."));
            }
        } 
        else 
        {
            if (!Resources.m_pParams->bPerf)
            {
                if(VPP_FILTER_DISABLED != Resources.m_pParams->vaParam.mode)
                {
                    vm_string_printf(VM_STRING("Frame number: %hd, spatial: %hd, temporal: %hd, scd: %hd \n"), nFrames,
                        Resources.pExtVPPAuxData[counter].SpatialComplexity,
                        Resources.pExtVPPAuxData[counter].TemporalComplexity,
                        Resources.pExtVPPAuxData[counter].SceneChangeRate);
                }
                else if(VPP_FILTER_DISABLED != Resources.m_pParams->varianceParam.mode)
                {
                    mfxExtVppReport* pReport = reinterpret_cast<mfxExtVppReport*>(&Resources.pExtVPPAuxData[counter]);
                    vm_string_printf(VM_STRING("Frame number: %hd, variance:"), nFrames);
                    for(int vIdx = 0; vIdx < 11; vIdx++)
                    {
                        mfxU32 variance = pReport->Variances[vIdx];
                        vm_string_printf(VM_STRING("%i "), variance);
                    }

                    vm_string_printf(VM_STRING("\n"));
                }
                else if(VPP_FILTER_DISABLED != Resources.m_pParams->idetectParam.mode)
                {
                    vm_string_printf(
                        VM_STRING("Frame number: %hd PicStruct = %s\n"), 
                        nFrames, 
                        PicStruct2Str(Resources.pExtVPPAuxData[counter].PicStruct));
                }
                counter++;
            }
            else
            {
                if (!(nFrames % 100))
                    vm_string_printf(VM_STRING("."));

            }
        }
    }
    return MFX_ERR_NONE;

} // mfxStatus OutputProcessFrame(



/* ******************************************************************* */

/* EOF */
