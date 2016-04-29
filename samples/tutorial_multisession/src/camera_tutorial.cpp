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
#include "camera_tutorial.h"
#include <windows.h>
#include <sstream>


static const mfxU8 CAMERA_PIPE_UID[]  = {0x54, 0x54, 0x26, 0x16, 0x24, 0x33, 0x41, 0xe6, 0x93, 0xae, 0x89, 0x99, 0x42, 0xce, 0x73, 0x55};

void PrintHelp(char *strAppName, const char *strErrorMessage)
{
    printf("Intel(R) Media SDK Professional Camera Pack Simple Sample \n\n");

    if (strErrorMessage)
    {
        printf("Error: %s\n", strErrorMessage);
    }

    printf("Usage: %s [Options] -i InputRAWfile -o OutputBmpFile\n", strAppName);
    printf("Options: \n");
    printf("   [-camera_plugin ver] - define camera plugin version\n");
    printf("   [-i filename]        - define input file name\n");
    printf("   [-o filename <NumFiles>]        - define output file name base and number of files to dump \n");
    printf("   [-bit xx]            - bitDepth at sensor\n");
    printf("   [-w xxxx]            - bayer width \n");
    printf("   [-h xxxx]            - bayer height \n");
    printf("   [-bggr]              - BGGR bayer pattern input\n");
    printf("   [-rggb]              - RGGB bayer pattern input\n");
    printf("   [-grbg]              - GRBG bayer pattern input\n");
    printf("   [-gbrg]              - GBRG bayer pattern input\n");
    printf("   [-d3d9]              - output to video memory (d3d9) \n");
    printf("   [-d3d11]             - output to video memory (d3d11) \n");
    printf("   [-out16]             - 16bit/pixel out \n");
    printf("   [-n numFrames]       - define number of frames to process \n");
    printf("   [-pp]                - postprocessing demo ON \n");
    printf("\n");
}


mfxStatus ParseInputString(char* strInput[], mfxU8 nArgNum, sInputParams* pParams)
{
    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], NULL);
        return MFX_ERR_UNSUPPORTED;
    }
    pParams->deviceHandle = 0;
    pParams->bOutput = false; // 8 bit default:

    CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        // multi-character options
        if (0 == strcmp(strInput[i], "-plugin_version"))
        {
            sscanf_s(strInput[++i], "%d", &pParams->CameraPluginVersion);
        }
        else if (0 == strcmp(strInput[i], "-n"))
        {
            sscanf_s(strInput[++i], "%d", &pParams->nFramesToProceed);

        }
        else if (0 == strcmp(strInput[i], "-bit"))
        {
            sscanf_s(strInput[++i], "%d", &pParams->bitDepth);

        }
        else if (0 == strcmp(strInput[i], "-w"))
        {
            sscanf_s(strInput[++i], "%d", &pParams->raw_width);
        }
        else if (0 == strcmp(strInput[i], "-h"))
        {
            sscanf_s(strInput[++i], "%d", &pParams->raw_height);
        }
        else if (0 == strcmp(strInput[i], "-i"))
        {
            strcpy_s(pParams->strSrcFile, strInput[++i]);
        }
        else if (0 == strcmp(strInput[i], "-d3d9"))
        {
            pParams->memTypeOut = D3D9_MEMORY;
        }
        else if (0 == strcmp(strInput[i], "-d3d11"))
        {
            pParams->memTypeOut = D3D11_MEMORY;
        }
        else if (0 == strcmp(strInput[i], "-bggr"))
        {
            pParams->rawType = MFX_CAM_BAYER_BGGR;
        }
        else if (0 == strcmp(strInput[i], "-rggb"))
        {
            pParams->rawType = MFX_CAM_BAYER_RGGB;
        }
        else if (0 == strcmp(strInput[i], "-grbg"))
        {
            pParams->rawType = MFX_CAM_BAYER_GRBG;
        }
        else if (0 == strcmp(strInput[i], "-gbrg"))
        {
            pParams->rawType = MFX_CAM_BAYER_GBRG;
        }
        else if (0 == strcmp(strInput[i], "-out16"))
        {
            pParams->bOutARGB16 = true;
        }
        else if (0 == strcmp(strInput[i], "-o"))
        {
            strcpy_s(pParams->strDstFile, strInput[++i]);
            pParams->bOutput = true;
            if (i + 1 < nArgNum)  {
                int n;
                if (sscanf_s(strInput[i+1], "%d", &n) == 1) {
                    pParams->maxNumBmpFiles = n;
                    i++;
                }
            }
        }
        else if (0 == strcmp(strInput[i], "-pp"))
        {
            //pParams->bPostProcess = true;
            pParams->bSwitchRequired = true;
        }
        else if (0 == strcmp(strInput[i], "-?"))
        {
            PrintHelp(strInput[0], NULL);
            return MFX_ERR_UNSUPPORTED;
        }
    }

    if (0 == strlen(pParams->strSrcFile))
    {
        PrintHelp(strInput[0], "Source file name not found");
        return MFX_ERR_UNSUPPORTED;
    }

    if (0 == strlen(pParams->strDstFile))
    {
        pParams->bOutput = false;
    }

    return MFX_ERR_NONE;
}



void CloseFrameInfo(sInputParams *pParams)
{
    if (pParams->pRawData) {
        free(pParams->pRawData);
        pParams->pRawData = 0;
    }
}


mfxStatus InitMfxParams(sInputParams *pParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    pParams->mfx_videoParams.vpp.In.CropX = pParams->frameInfo[VPP_IN].CropX;
    pParams->mfx_videoParams.vpp.In.CropY = pParams->frameInfo[VPP_IN].CropY;
    pParams->mfx_videoParams.vpp.In.CropW = pParams->frameInfo[VPP_IN].CropW;
    pParams->mfx_videoParams.vpp.In.CropH = pParams->frameInfo[VPP_IN].CropH;
    pParams->mfx_videoParams.vpp.In.Width = pParams->frameInfo[VPP_IN].nWidth;
    pParams->mfx_videoParams.vpp.In.Height = pParams->frameInfo[VPP_IN].nHeight;
    pParams->mfx_videoParams.vpp.In.FourCC = pParams->frameInfo[VPP_IN].FourCC;

    pParams->mfx_videoParams.vpp.In.ChromaFormat = 0;

    pParams->mfx_videoParams.vpp.In.BitDepthLuma = 14;

    pParams->mfx_videoParams.vpp.Out.CropX = pParams->frameInfo[VPP_OUT].CropX;
    pParams->mfx_videoParams.vpp.Out.CropY = pParams->frameInfo[VPP_OUT].CropY;
    pParams->mfx_videoParams.vpp.Out.CropW = pParams->frameInfo[VPP_OUT].CropW;
    pParams->mfx_videoParams.vpp.Out.CropH = pParams->frameInfo[VPP_OUT].CropH;
    pParams->mfx_videoParams.vpp.Out.Width = pParams->frameInfo[VPP_OUT].nWidth;
    pParams->mfx_videoParams.vpp.Out.Height = pParams->frameInfo[VPP_OUT].nHeight;
    pParams->mfx_videoParams.vpp.Out.FourCC = pParams->frameInfo[VPP_OUT].FourCC;

    pParams->mfx_videoParams.vpp.Out.ChromaFormat = 3;

    // specify memory type
    if (pParams->memTypeIn != SYSTEM_MEMORY)
        pParams->mfx_videoParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    else
        pParams->mfx_videoParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    if (pParams->memTypeOut != SYSTEM_MEMORY)
        pParams->mfx_videoParams.IOPattern |= MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    else
        pParams->mfx_videoParams.IOPattern |= MFX_IOPATTERN_OUT_SYSTEM_MEMORY;


    pParams->mfx_videoParams.AsyncDepth = 1;

    return sts;
}

#define ALIGNMENT_4K 0x1000

mfxStatus Init(sInputParams *pParams)
{
    CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    pParams->CameraPluginVersion = 1;

    // init video params
    memset(&pParams->mfx_videoParams, 0, sizeof(pParams->mfx_videoParams));


    // API version
    mfxVersion version =  {MFX_VERSION_MINOR, MFX_VERSION_MAJOR};

    // Init session
    {
        // try searching on all display adapters
        mfxIMPL impl = MFX_IMPL_HARDWARE_ANY;

        // if d3d11 surfaces are used ask the library to run acceleration through D3D11
        // feature may be unsupported due to OS or MSDK API version
        if (D3D11_MEMORY == pParams->memTypeIn || D3D11_MEMORY == pParams->memTypeOut)
            impl |= MFX_IMPL_VIA_D3D11;
        else if (D3D9_MEMORY == pParams->memTypeIn || D3D9_MEMORY == pParams->memTypeOut)
            impl |= MFX_IMPL_VIA_D3D9;

        sts = MFXInit(impl,  &version, &pParams->mfx_session);
        if(pParams->bPostProcess){
            sts = MFXInit(impl,  &version, &pParams->mfx_session_postprocess);
        }
    }
    // not supported yet
    //else
    //    sts = m_mfxSession.Init(MFX_IMPL_SOFTWARE, &version);
        //sts = m_mfxSession.Init(MFX_IMPL_SOFTWARE | MFX_IMPL_RT, &version);

    CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    ///////////////////////////////////////////////////
    // -d3d specific
    ///////////////////////////////////////////////////


    if (pParams->memTypeIn == D3D11_MEMORY || pParams->memTypeOut == D3D11_MEMORY)
    {
        // D3D11 stuff
        sts = CreateHWDevice_dx11(pParams->mfx_session, &pParams->deviceHandle, NULL);
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = MFXVideoCORE_SetHandle(pParams->mfx_session, MFX_HANDLE_D3D11_DEVICE, pParams->deviceHandle);
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        pParams->mfxAllocator.pthis  = pParams->mfx_session; // ??? For this sample we use mfxSession as the allocation identifier
        pParams->mfxAllocator.Alloc  = simple_alloc_dx11;
        pParams->mfxAllocator.Free   = simple_free_dx11;
        pParams->mfxAllocator.Lock   = simple_lock_dx11;
        pParams->mfxAllocator.Unlock = simple_unlock_dx11;
        pParams->mfxAllocator.GetHDL = simple_gethdl_dx11;

        // When using video memory we must provide Media SDK with an external allocator
        sts = MFXVideoCORE_SetFrameAllocator(pParams->mfx_session, &pParams->mfxAllocator);
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    }
    else if (pParams->memTypeIn == D3D9_MEMORY || pParams->memTypeOut == D3D9_MEMORY)
    {
        sts = CreateHWDevice(pParams->mfx_session, &pParams->deviceHandle, NULL);
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = MFXVideoCORE_SetHandle(pParams->mfx_session, MFX_HANDLE_D3D9_DEVICE_MANAGER, pParams->deviceHandle);
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        pParams->mfxAllocator.pthis  = pParams->mfx_session; // ??? For this sample we use mfxSession as the allocation identifier
        pParams->mfxAllocator.Alloc  = simple_alloc;
        pParams->mfxAllocator.Free   = simple_free;
        pParams->mfxAllocator.Lock   = simple_lock;
        pParams->mfxAllocator.Unlock = simple_unlock;
        pParams->mfxAllocator.GetHDL = simple_gethdl;

        // When using video memory we must provide Media SDK with an external allocator
        sts = MFXVideoCORE_SetFrameAllocator(pParams->mfx_session, &pParams->mfxAllocator);
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    }
    if(pParams->bPostProcess){
        if (pParams->memPostPTypeIn == D3D9_MEMORY || pParams->memPostPTypeOut == D3D9_MEMORY) {
            if(!pParams->deviceHandle){
                sts = CreateHWDevice(pParams->mfx_session_postprocess, &pParams->deviceHandle, NULL);
                CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            sts = MFXVideoCORE_SetHandle(pParams->mfx_session_postprocess, MFX_HANDLE_D3D9_DEVICE_MANAGER, pParams->deviceHandle);
            CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            pParams->mfxAllocator_postProc.pthis  = pParams->mfx_session_postprocess;
            pParams->mfxAllocator_postProc.Alloc  = simple_alloc;
            pParams->mfxAllocator_postProc.Free   = simple_free;
            pParams->mfxAllocator_postProc.Lock   = simple_lock;
            pParams->mfxAllocator_postProc.Unlock = simple_unlock;
            pParams->mfxAllocator_postProc.GetHDL = simple_gethdl;
            sts = MFXVideoCORE_SetFrameAllocator(pParams->mfx_session_postprocess, &pParams->mfxAllocator_postProc);
            CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        else if (pParams->memPostPTypeIn == D3D11_MEMORY || pParams->memPostPTypeOut == D3D11_MEMORY){
            if(!pParams->deviceHandle){
                sts = CreateHWDevice_dx11(pParams->mfx_session_postprocess, &pParams->deviceHandle, NULL);
                CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            sts = MFXVideoCORE_SetHandle(pParams->mfx_session_postprocess, MFX_HANDLE_D3D11_DEVICE, pParams->deviceHandle);
            CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            pParams->mfxAllocator_postProc.pthis  = pParams->mfx_session_postprocess;
            pParams->mfxAllocator_postProc.Alloc  = simple_alloc_dx11;
            pParams->mfxAllocator_postProc.Free   = simple_free_dx11;
            pParams->mfxAllocator_postProc.Lock   = simple_lock_dx11;
            pParams->mfxAllocator_postProc.Unlock = simple_unlock_dx11;
            pParams->mfxAllocator_postProc.GetHDL = simple_gethdl_dx11;
            // When using video memory we must provide Media SDK with an external allocator
            sts = MFXVideoCORE_SetFrameAllocator(pParams->mfx_session_postprocess, &pParams->mfxAllocator_postProc);
        }
    }
    //Load library plug-in
    {
        memcpy(pParams->UID_Camera.Data, CAMERA_PIPE_UID, 16);
        sts = MFXVideoUSER_Load(pParams->mfx_session, &pParams->UID_Camera, pParams->CameraPluginVersion);
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        if(pParams->bPostProcess){
            //no Load required, it'll be done by legacy VPP from libmfxhw, but not Camera plugin.
        }
    }

    ///////////////////////////////////////////////////
    // Camera Pipe Capability module query
    // ////////////////////////////////////////////////

    mfxU32 filterList[2];
    mfxExtBuffer *extBuffers[1];
    mfxExtVPPDoUse vppDoUse;
    vppDoUse.Header.BufferId = MFX_EXTBUFF_VPP_DOUSE;
    vppDoUse.Header.BufferSz = sizeof(vppDoUse);
    vppDoUse.AlgList = filterList;
    vppDoUse.NumAlg =2;
    filterList[0] = MFX_EXTBUF_CAM_PADDING;
    filterList[1] = MFX_EXTBUF_CAM_GAMMA_CORRECTION;
    extBuffers[0] = (mfxExtBuffer*)&vppDoUse;


    memset(&pParams->mfx_videoParams, 0, sizeof(&pParams->mfx_videoParams));
    pParams->mfx_videoParams.ExtParam = extBuffers;
    pParams->mfx_videoParams.NumExtParam = 1;

    sts = MFXVideoVPP_Query(pParams->mfx_session, NULL, &pParams->mfx_videoParams);
    //if (sts == MFX_ERR_NONE) printf("OK!\n");
    //else printf("NG!!!\n");

    for (int i = 0; i < (int)vppDoUse.NumAlg; i++) {
        switch(filterList[i])
        {
            case MFX_EXTBUF_CAM_PADDING:
                printf("filter[%d]:MFX_EXTBUF_CAM_PADDING Supported\n", i);
                break;
            case MFX_EXTBUF_CAM_GAMMA_CORRECTION:
                printf("filter[%d]:MFX_EXTBUF_CAM_GAMMA_CORRECTION Supported\n", i);
                break;
            case MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3:
                printf("filter[%d]:MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3 Supported\n", i);
                break;
            default:
                printf("filter[%d]:NOT SUPPORTED\n", i);
        }
    }


    ///////////////////////////////////////////////////
    // Set up camera specific extended buffer
    // ////////////////////////////////////////////////

    // Bayer type
    int numExtBuffers = 0;
    pParams->m_PipeControl.Header.BufferId = MFX_EXTBUF_CAM_PIPECONTROL;
    pParams->m_PipeControl.Header.BufferSz = sizeof(pParams->m_PipeControl);

    // switch the bayer pattern for the source here
    pParams->m_PipeControl.RawFormat = (mfxU16)pParams->rawType;


    pParams->m_ExtBuffers[numExtBuffers] = (mfxExtBuffer *)&pParams->m_PipeControl;
    numExtBuffers++;


    ////////////////////////////////
    // Gamma parameters set up
    ////////////////////////////////

    mfxU32 max_input_level = 1<<(pParams->bitDepth);
    pParams->m_GammaCorrection.NumPoints = 64;
    mfxU32 num_gamma_pt = pParams->m_GammaCorrection.NumPoints;

    double gamma_value = 2.2;

    for (int i = 0; i < 64; i++)
    {
        pParams->m_GammaCorrection.GammaPoint[i] = (mfxU16)(((max_input_level) / num_gamma_pt) * i);
    }

    for (int i = 0; i < 64; i++)
    {
        pParams->m_GammaCorrection.GammaCorrected[i] = (mfxU16)(pow((double)pParams->m_GammaCorrection.GammaPoint[i] / (double)max_input_level, 1.0 / gamma_value) * (double)max_input_level);
    }

    printf("const SHORT init_gamma_point[%d] = ", num_gamma_pt);
    for (int i = 0; i < 64; i++) printf("%d,", pParams->m_GammaCorrection.GammaPoint[i]);
    printf("}\n");

    printf("const SHORT init_gamma_correct[%d] = ", num_gamma_pt);
    for (int i = 0; i < 64; i++) printf("%d,", pParams->m_GammaCorrection.GammaCorrected[i]);
    printf("}\n");


    // set up extended buffer for gamma

    pParams->m_GammaCorrection.Header.BufferId = MFX_EXTBUF_CAM_GAMMA_CORRECTION;
    pParams->m_GammaCorrection.Header.BufferSz = sizeof(pParams->m_GammaCorrection);
    pParams->m_GammaCorrection.Mode = MFX_CAM_GAMMA_LUT;
    pParams->m_ExtBuffers[numExtBuffers] = (mfxExtBuffer *)&pParams->m_GammaCorrection;
    numExtBuffers++;

    // attach extended buffer
    pParams->mfx_videoParams.ExtParam = pParams->m_ExtBuffers;
    pParams->mfx_videoParams.NumExtParam = (mfxU16)numExtBuffers;


    ///////////////////////////////////////////////////
    // Initialize frame Info
    // ////////////////////////////////////////////////

    mfxI32 frameWidthSamples = pParams->raw_width;
    mfxI32 frameHeight       = pParams->raw_height;

    pParams->fileFramePitchBytes = frameWidthSamples * sizeof(short);
    pParams->fileFrameHeight     = frameHeight;


    // Init videoParams VPP info
    pParams->mfx_videoParams.vpp.In.CropX  = pParams->mfx_videoParams.vpp.In.CropY = 0;
    pParams->mfx_videoParams.vpp.In.CropW  = (mfxU16)frameWidthSamples;
    pParams->mfx_videoParams.vpp.In.CropH  = (mfxU16)frameHeight;


    pParams->mfx_videoParams.vpp.In.Width  = (mfxU16)frameWidthSamples;
    pParams->mfx_videoParams.vpp.In.Height = (mfxU16)frameHeight;


    pParams->mfx_videoParams.vpp.In.FourCC = MFX_FOURCC_R16;

    pParams->mfx_videoParams.vpp.In.BitDepthLuma = (mfxU16)pParams->bitDepth;

    //Only R16 input supported now, should use chroma format monochrome
    pParams->mfx_videoParams.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_MONOCHROME;


    pParams->mfx_videoParams.vpp.Out.CropX  = pParams->mfx_videoParams.vpp.Out.CropY = 0;
    pParams->mfx_videoParams.vpp.Out.CropW  = (mfxU16)frameWidthSamples;
    pParams->mfx_videoParams.vpp.Out.CropH  = (mfxU16)frameHeight;
    pParams->mfx_videoParams.vpp.Out.Width  = (mfxU16)frameWidthSamples;
    pParams->mfx_videoParams.vpp.Out.Height = (mfxU16)frameHeight;

    //Only ARGB onput supported now, should use chroma format 444
    pParams->mfx_videoParams.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV444;

    // To change 8/16bit out, change here please.
    if (pParams->bOutARGB16) {
        pParams->mfx_videoParams.vpp.Out.FourCC = MFX_FOURCC_ARGB16;
        pParams->mfx_videoParams.vpp.Out.BitDepthLuma = (mfxU16)pParams->bitDepth;
    } else {
        pParams->mfx_videoParams.vpp.Out.FourCC = MFX_FOURCC_RGB4;
        pParams->mfx_videoParams.vpp.Out.BitDepthLuma = 8;
    }

    // specify memory type
    if (pParams->memTypeIn != SYSTEM_MEMORY)
        pParams->mfx_videoParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    else
        pParams->mfx_videoParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    if (pParams->memTypeOut != SYSTEM_MEMORY)
        pParams->mfx_videoParams.IOPattern |= MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    else
        pParams->mfx_videoParams.IOPattern |= MFX_IOPATTERN_OUT_SYSTEM_MEMORY;


    pParams->mfx_videoParams.AsyncDepth = 1;

    ///////////////////////////////////////////////////
    // Query and Init VPP
    // ////////////////////////////////////////////////

    sts = MFXVideoVPP_Query(pParams->mfx_session, &pParams->mfx_videoParams, &pParams->mfx_videoParams);

//    IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //if (sts == MFX_ERR_NONE) printf("OK!\n");
    //else printf("NG!!!\n");

    sts = MFXVideoVPP_Init(pParams->mfx_session, &pParams->mfx_videoParams);
    if (MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        printf("WARNING: partial acceleration\n");
        IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    }
    CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    if(pParams->bPostProcess){
        memcpy(&pParams->mfx_videoPostProcessParams,&pParams->mfx_videoParams,sizeof(mfxVideoParam));
        // Additional session with Legasy VPP will be used to copy from postprocessed and converted to 8 bit frame to D3D memory for best performance.
        pParams->mfx_videoPostProcessParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        pParams->mfx_videoPostProcessParams.NumExtParam=0;
        pParams->mfx_videoPostProcessParams.ExtParam=NULL;
        // for legasy VPP we need to set Framerate, because it handle it if need Frame Rate Conversion
        pParams->mfx_videoPostProcessParams.vpp.Out.FrameRateExtD = 1;
        pParams->mfx_videoPostProcessParams.vpp.Out.FrameRateExtN = 24;
        // for legasy VPP we need to set PicStruct, because it handle it if need deinterlace
        pParams->mfx_videoPostProcessParams.vpp.Out.PicStruct = 1;
        pParams->mfx_videoPostProcessParams.vpp.Out.FourCC = MFX_FOURCC_RGB4;
        pParams->mfx_videoPostProcessParams.vpp.Out.BitDepthLuma = 8;
        pParams->mfx_videoPostProcessParams.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        pParams->mfx_videoPostProcessParams.vpp.In=pParams->mfx_videoPostProcessParams.vpp.Out;

        //no capabilities quering required Legasy VPP copy always accessible
        //sts = MFXVideoVPP_Query(pParams->mfx_session_postprocess, &pParams->mfx_videoPostProcessParams, &pParams->mfx_videoPostProcessParams);

        sts = MFXVideoVPP_Init(pParams->mfx_session_postprocess, &pParams->mfx_videoPostProcessParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            printf("WARNING: partial acceleration\n");
            IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    mfxFrameAllocRequest VPPRequest[2];  // [0] - in, [1] - out
    mfxFrameAllocRequest VPPRequestPost[2];  // [0] - in, [1] - out
    memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest)*2);
    sts = MFXVideoVPP_QueryIOSurf(pParams->mfx_session, &pParams->mfx_videoParams, VPPRequest);
    CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    if(pParams->bPostProcess){
        sts = MFXVideoVPP_QueryIOSurf(pParams->mfx_session_postprocess, &pParams->mfx_videoPostProcessParams, VPPRequestPost);
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    mfxFrameSurface1 *pSurfIn  = &pParams->mfx_surfaces[VPP_IN];
    mfxFrameSurface1 *pSurfOut = &pParams->mfx_surfaces[VPP_OUT];
    mfxFrameSurface1 *pSurfPostProcIn = &pParams->mfx_surfaces_postprocess[VPP_IN];
    mfxFrameSurface1 *pSurfPostProcOut = &pParams->mfx_surfaces_postprocess[VPP_OUT];


    pSurfIn->Info = pParams->mfx_videoParams.vpp.In;
    pSurfOut->Info = pParams->mfx_videoParams.vpp.Out;
    if(pParams->bPostProcess){
        pSurfPostProcIn->Info = pParams->mfx_videoPostProcessParams.vpp.In;
        pSurfPostProcOut->Info = pParams->mfx_videoPostProcessParams.vpp.Out;
    }
    memset((void*)&pSurfIn->Data, 0, sizeof(pSurfIn->Data));
    memset((void*)&pSurfOut->Data, 0, sizeof(pSurfOut->Data));
    memset((void*)&pSurfPostProcIn->Data, 0, sizeof(pSurfPostProcOut->Data));
    memset((void*)&pSurfPostProcOut->Data, 0, sizeof(pSurfPostProcOut->Data));



    int allocSize = 0;
    if (pParams->memTypeOut == SYSTEM_MEMORY)
    {
        if (pParams->mfx_videoParams.vpp.Out.FourCC == MFX_FOURCC_ARGB16){
            pSurfOut->Data.Pitch = pParams->mfx_videoParams.vpp.Out.Width << 3;
        } else {
            pSurfOut->Data.Pitch = pParams->mfx_videoParams.vpp.Out.Width << 2;
        }
        allocSize += pSurfOut->Data.Pitch * pSurfOut->Info.Height + ALIGNMENT_4K;
    }
    else
    { // (pParams->memTypeOut == D3D11_MEMORY)
        VPPRequest[1].Type |= WILL_READ; // Hint to DX11 memory handler that application will read data from output surfaces
        sts = pParams->mfxAllocator.Alloc(pParams->mfxAllocator.pthis, &VPPRequest[1], &pParams->mfxResponseOut);
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        pSurfOut->Data.MemId = pParams->mfxResponseOut.mids[0];
    }

    if (pParams->memTypeIn == SYSTEM_MEMORY)
    {
        pSurfIn->Data.Pitch = pParams->mfx_videoParams.vpp.In.Width << 1;
        allocSize += pSurfIn->Data.Pitch * pSurfIn->Info.Height + ALIGNMENT_4K;
    }
    else
    { // (pParams->memTypeIn == D3D11_MEMORY)
        VPPRequest[0].Type |= WILL_WRITE; // Hint to DX11 memory handler that application will write data to input surfaces
        sts = pParams->mfxAllocator.Alloc(pParams->mfxAllocator.pthis, &VPPRequest[0], &pParams->mfxResponseIn);
        CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        pSurfIn->Data.MemId = pParams->mfxResponseIn.mids[0];
    }
    if(pParams->bPostProcess){
        if (pParams->memPostPTypeIn == SYSTEM_MEMORY){
            pSurfPostProcIn->Data.Pitch = pParams->mfx_videoParams.vpp.In.Width << 2;
            //no 4K alignment required for legasy VPP session, only 16
            allocSize += pSurfIn->Data.Pitch * pSurfIn->Info.Height + 0x10;
        }else{
            //not implemented
        }
        if (pParams->memPostPTypeOut == SYSTEM_MEMORY){
            pSurfPostProcOut->Data.Pitch = pParams->mfx_videoParams.vpp.Out.Width << 2;
            allocSize += pSurfPostProcOut->Data.Pitch * pSurfPostProcOut->Info.Height + ALIGNMENT_4K;
        }else{
            VPPRequestPost[1].Type |= WILL_READ; // Hint to DX11 memory handler that application will read data from output surfaces
            sts = pParams->mfxAllocator_postProc.Alloc(pParams->mfxAllocator_postProc.pthis, &VPPRequestPost[1], &pParams->mfxResponseOut_postprocess);
            CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            pSurfPostProcOut->Data.MemId = pParams->mfxResponseOut_postprocess.mids[0];
        }
    }

    if (allocSize > 0)
    {
        pParams->pMemoryAllocated = malloc(allocSize);
        CHECK_POINTER(pParams->pMemoryAllocated, MFX_ERR_MEMORY_ALLOC);

        mfxU8 *tmpptr = (mfxU8*)pParams->pMemoryAllocated;
        if (pParams->memTypeIn == SYSTEM_MEMORY)
        {
            pSurfIn->Data.Y16 = (mfxU16 *)((size_t)(tmpptr + ALIGNMENT_4K) &~ (size_t)(ALIGNMENT_4K - 1));
            tmpptr = (mfxU8 *)pSurfIn->Data.Y16 + pSurfIn->Data.Pitch * pSurfIn->Info.Height;
        }
        if (pParams->memTypeOut == SYSTEM_MEMORY)
        {
            if(pParams->bOutARGB16){
                pSurfOut->Data.B = (mfxU8*)((size_t)(tmpptr + ALIGNMENT_4K) &~ (size_t)(ALIGNMENT_4K - 1));
                pSurfOut->Data.G = (mfxU8*)(((mfxU16*)pSurfOut->Data.B) + 1);
                pSurfOut->Data.R = (mfxU8*)(((mfxU16*)pSurfOut->Data.B) + 2);
            }else{
                pSurfOut->Data.B = (mfxU8*)((size_t)(tmpptr + ALIGNMENT_4K) &~ (size_t)(ALIGNMENT_4K - 1));
                pSurfOut->Data.G = pSurfOut->Data.B + 1;
                pSurfOut->Data.R = pSurfOut->Data.B + 2;
            }


        }
        if(pParams->bPostProcess){
            pSurfPostProcIn->Data.B = (mfxU8*)((size_t)(tmpptr + 0x10) &~ (size_t)(0x10 - 1));
            pSurfPostProcIn->Data.G = pSurfPostProcIn->Data.B + 1;
            pSurfPostProcIn->Data.R = pSurfPostProcIn->Data.B + 2;
            pSurfPostProcIn->Data.A = pSurfPostProcIn->Data.B + 3;
            pSurfPostProcIn->Info.PicStruct = 1;
        }
    }




    return MFX_ERR_NONE;
}


void Close(sInputParams *pParams)
{
    if (pParams->pRawData) {
        free(pParams->pRawData);
        pParams->pRawData = 0;
    }

    if (pParams->pMemoryAllocated) {
        free(pParams->pMemoryAllocated);
        pParams->pMemoryAllocated = 0;
    }

    if (pParams->memTypeIn != SYSTEM_MEMORY)
        pParams->mfxAllocator.Free(pParams->mfxAllocator.pthis, &pParams->mfxResponseIn);

    if (pParams->memTypeOut != SYSTEM_MEMORY)
        pParams->mfxAllocator.Free(pParams->mfxAllocator.pthis, &pParams->mfxResponseOut);

}


mfxStatus LoadFrame(sInputParams *pParams, char *fname)
{

    mfxU32 nRead;
    FILE *f;

    mfxFrameData* pData = &pParams->mfx_surfaces[VPP_IN].Data;
//    mfxFrameInfo *pInfo = &pParams->mfx_surfaces[VPP_IN].Info;

    fopen_s(&f, fname, "rb");
    CHECK_POINTER(f, MFX_ERR_MORE_DATA);

    mfxU32 filePitchU16 = pParams->fileFramePitchBytes >> 1;
    mfxI32 frameSize = filePitchU16*(pParams->fileFrameHeight);

    nRead = (mfxU32)fread((void*)pData->Y16, sizeof(mfxU16), frameSize, f);
    if (nRead != (mfxU32)frameSize)
        return MFX_ERR_MORE_DATA;


    fclose(f);
    return MFX_ERR_NONE;
}

mfxStatus WriteFrame(sInputParams *pParams, char *fname)
{
    FILE *f;

    fopen_s(&f, fname, "wb");
    CHECK_POINTER(f, MFX_ERR_NULL_PTR);

    mfxU32 nbytes;
    BITMAPFILEHEADER m_bfh;
    BITMAPINFOHEADER m_bih;

    memset(&m_bfh, 0, sizeof(m_bfh));
    memset(&m_bih, 0, sizeof(m_bih));

    int buffSize = pParams->mfx_videoParams.vpp.Out.CropW *  pParams->mfx_videoParams.vpp.Out.CropH * 4; //RGBA-8

    m_bfh.bfType = 0x4D42;
    m_bfh.bfSize = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER) + buffSize;
    m_bfh.bfOffBits = sizeof(BITMAPINFOHEADER) + sizeof(BITMAPFILEHEADER);

    m_bih.biSize   = sizeof(BITMAPINFOHEADER);
    m_bih.biHeight = pParams->mfx_videoParams.vpp.Out.CropH;
    m_bih.biWidth  = pParams->mfx_videoParams.vpp.Out.CropW;
    m_bih.biBitCount = 32;
    m_bih.biPlanes = 1;
    m_bih.biCompression = BI_RGB;

    nbytes = (mfxU32)fwrite(&m_bfh, 1, sizeof(BITMAPFILEHEADER), f);
    if (nbytes != sizeof(BITMAPFILEHEADER))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    nbytes = (mfxU32)fwrite(&m_bih, 1, sizeof(BITMAPINFOHEADER), f);
    if (nbytes != sizeof(BITMAPINFOHEADER))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxU32 width_bytes = m_bih.biWidth * 4;

    mfxFrameData *pData = &pParams->mfx_surfaces[VPP_OUT].Data;
    if(!pParams->bPostProcess){
        pData = &pParams->mfx_surfaces[VPP_OUT].Data;
        if (pParams->mfx_videoParams.vpp.Out.FourCC == MFX_FOURCC_ARGB16)
        {
            mfxU8 *pOut = new mfxU8[width_bytes];
            for (int y = m_bih.biHeight - 1; y >= 0; y--)
            {
                for (int x = 0; x < (int)width_bytes; x++)
                {
                    // be careful. data location might be changed later. N.Iwamoto Intel Corp
                    pOut[x] = (mfxU8)(pData->V16[x + y*width_bytes] >> (pParams->mfx_videoParams.vpp.In.BitDepthLuma - 8));
                    //pOut[x] = (mfxU8)((pData->B[2*x + y*width_bytes*2] | (pData->B[2*x + 1 + 2*y*width_bytes] << 8)) >> 2);
                }
                nbytes = (mfxU32)fwrite(pOut, 1, width_bytes, f);
                if(nbytes != width_bytes)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
        else
        for (int y = m_bih.biHeight - 1; y >= 0; y--)
        {
            nbytes = (mfxU32)fwrite((mfxU8*)pData->B + y*width_bytes, 1, width_bytes, f);
            if(nbytes != width_bytes)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }else{
        pData = &pParams->mfx_surfaces_postprocess[VPP_OUT].Data;
        if (pParams->mfx_videoPostProcessParams.vpp.Out.FourCC == MFX_FOURCC_ARGB16)
        {
            mfxU8 *pOut = new mfxU8[width_bytes];
            for (int y = m_bih.biHeight - 1; y >= 0; y--)
            {
                for (int x = 0; x < (int)width_bytes; x++)
                {
                    // be careful. data location might be changed later. N.Iwamoto Intel Corp
                    pOut[x] = (mfxU8)(pData->V16[x + y*width_bytes] >> (pParams->mfx_videoPostProcessParams.vpp.In.BitDepthLuma - 8));
                    //pOut[x] = (mfxU8)((pData->B[2*x + y*width_bytes*2] | (pData->B[2*x + 1 + 2*y*width_bytes] << 8)) >> 2);
                }
                nbytes = (mfxU32)fwrite(pOut, 1, width_bytes, f);
                if(nbytes != width_bytes)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
        else
        for (int y = m_bih.biHeight - 1; y >= 0; y--)
        {
            nbytes = (mfxU32)fwrite((mfxU8*)pData->B + y*width_bytes, 1, width_bytes, f);
            if(nbytes != width_bytes)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }




    fclose(f);
    return MFX_ERR_NONE;
}

int main(int argc, char *argv[])
{
    sInputParams      Params;   // input parameters from command line
    sInputParams      Params1;
    memset(&Params, 0, sizeof(Params));
    memset(&Params1, 0, sizeof(Params));

    mfxStatus sts = MFX_ERR_NONE; // return value check

    Params.memTypeIn = SYSTEM_MEMORY;
    Params.memTypeOut = D3D9_MEMORY;

    Params.bitDepth = 10;
    Params.nFramesToProceed = 1;
    Params.maxNumBmpFiles = 1;

    sts = ParseInputString(argv, (mfxU8)argc, &Params);
    CHECK_RESULT(sts, MFX_ERR_NONE, 1);




    sts = Init(&Params);
    CHECK_RESULT(sts, MFX_ERR_NONE, 1);


    printf("IN FILE:%s\n", Params.strSrcFile);
    printf("OUT FILE:%s\n", Params.strDstFile);


    printf("Camera pipe started\n");

//    MFXVideoVPP_Close(Params.mfx_session);
//    sts = MFXVideoVPP_Init(Params.mfx_session, &Params.mfx_videoParams);

    int fileNum = 0;
    char fileName[MAX_FILENAME_LEN];
    mfxSyncPoint syncpoint;

    while (MFX_ERR_NONE <= sts && fileNum < (int)Params.nFramesToProceed )
    {
        fileNum++;

        sprintf_s(fileName, "%s", Params.strSrcFile);

        sts = LoadFrame(&Params, fileName);
        BREAK_ON_ERROR(sts);

        sts = MFXVideoVPP_RunFrameVPPAsync(Params.mfx_session, &Params.mfx_surfaces[VPP_IN], &Params.mfx_surfaces[VPP_OUT], NULL, &syncpoint);
        BREAK_ON_ERROR(sts);

        sts = MFXVideoCORE_SyncOperation(Params.mfx_session, syncpoint, VPP_WAIT_INTERVAL);
        BREAK_ON_ERROR(sts);

        if (Params.bOutput && (fileNum <= Params.maxNumBmpFiles)) {

            sprintf_s(fileName, "%s%d.bmp", Params.strDstFile, fileNum);

            if (Params.memTypeOut != SYSTEM_MEMORY) {
                sts = Params.mfxAllocator.Lock(Params.mfxAllocator.pthis, Params.mfx_surfaces[VPP_OUT].Data.MemId, &Params.mfx_surfaces[VPP_OUT].Data);
                BREAK_ON_ERROR(sts);
            }

            sts = WriteFrame(&Params, fileName);
            BREAK_ON_ERROR(sts);

            if (Params.memTypeOut != SYSTEM_MEMORY) {
                sts = Params.mfxAllocator.Unlock(Params.mfxAllocator.pthis, Params.mfx_surfaces[VPP_OUT].Data.MemId, &Params.mfx_surfaces[VPP_OUT].Data);
                BREAK_ON_ERROR(sts);
            }

        }
        printf("Processed frame %d \r", fileNum);
    }

    //we need to do partial close here: Close VPP only, Even D3D surfaces will be reused later fo rendering
    //Close(&Params);


    if(Params.bSwitchRequired){
        int filenum_postprocess=0;
        if(Params.bSwitchRequired){
            memcpy(&Params1,&Params,sizeof(sInputParams));
            Params1.bOutARGB16 = true;
            Params1.bPostProcess = true;
            Params1.memTypeOut = SYSTEM_MEMORY;
            Params1.memPostPTypeIn = SYSTEM_MEMORY;
            Params1.memPostPTypeOut = D3D9_MEMORY;
        }
        Init(&Params1);

        while (MFX_ERR_NONE <= sts && filenum_postprocess < (int)Params1.nFramesToProceed )
        {
            filenum_postprocess++;

            sprintf_s(fileName, "%s", Params1.strSrcFile);

            sts = LoadFrame(&Params1, fileName);
            BREAK_ON_ERROR(sts);

            sts = MFXVideoVPP_RunFrameVPPAsync(Params1.mfx_session, &Params1.mfx_surfaces[VPP_IN], &Params1.mfx_surfaces[VPP_OUT], NULL, &syncpoint);
            BREAK_ON_ERROR(sts);

            sts = MFXVideoCORE_SyncOperation(Params1.mfx_session, syncpoint, VPP_WAIT_INTERVAL);
            BREAK_ON_ERROR(sts);


            if(Params1.bPostProcess)
            {
                mfxFrameSurface1 *ppSurf = &Params1.mfx_surfaces[VPP_OUT];
                mfxFrameSurface1 *ppSurfVPP = &Params1.mfx_surfaces_postprocess[VPP_IN];
                int i, j;
                mfxU16 B,G,R;

                for (i = 0; i < ppSurf->Info.Height; i++) {
                    for (j = 0; j < ppSurf->Info.Width; j++) {

                        B=((mfxU16*)(ppSurf->Data.B))[i*(ppSurf->Data.Pitch>>1) + (j<<2)];
                        G=((mfxU16*)(ppSurf->Data.G))[i*(ppSurf->Data.Pitch>>1) + (j<<2)];
                        R=((mfxU16*)(ppSurf->Data.R))[i*(ppSurf->Data.Pitch>>1) + (j<<2)];
                        //preprocessing(make torn picture), if comment next code should be bitexact with first run before switch
                        B+=(mfxU16)(((i + j)<<8) & 0xffff);
                        G+=(mfxU16)(((i + j)<<8) & 0xffff);
                        R+=(mfxU16)(((i + j)<<8) & 0xffff);
                        //downscale from 16 to 8 bit before rendering
                        ppSurfVPP->Data.B[i*ppSurfVPP->Data.Pitch + (j<<2)]=(mfxU8)(B>>8);
                        ppSurfVPP->Data.G[i*ppSurfVPP->Data.Pitch + (j<<2)]=(mfxU8)(G>>8);
                        ppSurfVPP->Data.R[i*ppSurfVPP->Data.Pitch + (j<<2)]=(mfxU8)(R>>8);
                        ppSurfVPP->Data.A[i*ppSurfVPP->Data.Pitch + (j<<2)] = 0;
                    }
                }
                //postprocessing session with legasy VPP, just Copy from system to D3D memory
                sts = MFXVideoVPP_RunFrameVPPAsync(Params1.mfx_session_postprocess, &Params1.mfx_surfaces_postprocess[VPP_IN], &Params1.mfx_surfaces_postprocess[VPP_OUT], NULL, &syncpoint);
                BREAK_ON_ERROR(sts);

                sts = MFXVideoCORE_SyncOperation(Params1.mfx_session_postprocess, syncpoint, VPP_WAIT_INTERVAL);
                BREAK_ON_ERROR(sts);

            }

            if (Params1.bOutput && (fileNum <= Params1.maxNumBmpFiles)) {

                sprintf_s(fileName, "%s%d.bmp", Params1.strDstFile, fileNum+filenum_postprocess);

                if (Params1.memPostPTypeOut != SYSTEM_MEMORY) {
                    sts = Params.mfxAllocator.Lock(Params1.mfxAllocator_postProc.pthis, Params1.mfx_surfaces_postprocess[VPP_OUT].Data.MemId, &Params1.mfx_surfaces_postprocess[VPP_OUT].Data);
                    BREAK_ON_ERROR(sts);
                }

                sts = WriteFrame(&Params1, fileName);
                BREAK_ON_ERROR(sts);

                if (Params1.memPostPTypeOut != SYSTEM_MEMORY) {
                    sts = Params.mfxAllocator.Unlock(Params1.mfxAllocator_postProc.pthis, Params1.mfx_surfaces_postprocess[VPP_OUT].Data.MemId, &Params1.mfx_surfaces_postprocess[VPP_OUT].Data);
                    BREAK_ON_ERROR(sts);
                }

            }
            printf("Processed frame %d \r", fileNum+filenum_postprocess);
        }
    }
    printf("\nCamera pipe finished\n");
    Close(&Params);
    Close(&Params1);
    return 0;
}
