/******************************************************************************* *\

Copyright (C) 2010-2015 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: serial_sample.cpp

*******************************************************************************/

#include "mfxcommon.h"
#include "mfxstructures.h"
#include "mfx_serializer.h"

#include <string>
#include <iostream>

#pragma once

void CleanExtBuf(mfxVideoParam* par)
{
    for (int i = 0; i < par->NumExtParam; i++)
    {
        if (!par->ExtParam[i])
            continue;

        delete par->ExtParam[i];
        par->ExtParam[i] = 0;
    }

    if (par->ExtParam)
    {
        delete[] par->ExtParam;
        par->ExtParam = 0;
    }
}

void SerializeVideoParamList(mfxVideoParam* inMFX, mfxVideoParam* inVPP, mfxVideoParam* outMFX, mfxVideoParam* outVPP)
{
    bool res = SerializeToList(*inMFX, "mfxVideoParamMFX", "mfxVideoParam.txt");
    res      = SerializeToList(*inVPP, "mfxVideoParamVPP", "mfxVideoParam.txt", SerialFlags::VPP);

    res = DeSerializeFromList(*outMFX, "mfxVideoParamMFX", "mfxVideoParam.txt");
    res = DeSerializeFromList(*outVPP, "mfxVideoParamVPP", "mfxVideoParam.txt");
};

void SerializeVideoParamYaml(mfxVideoParam* inMFX, mfxVideoParam* inVPP, mfxVideoParam* outMFX, mfxVideoParam* outVPP)
{
    bool res = SerializeToYaml(*inMFX, "mfxVideoParamMFX", "mfxVideoParam.yaml");
    res      = SerializeToYaml(*inVPP, "mfxVideoParamVPP", "mfxVideoParam.yaml", SerialFlags::VPP);

    DeSerializeFromYaml(*outMFX, "mfxVideoParamMFX", "mfxVideoParam.yaml");
    DeSerializeFromYaml(*outVPP, "mfxVideoParamVPP", "mfxVideoParam.yaml");
};

int main(int argc, char *argv[])
{
    //configure parameters
    mfxVideoParam inMFX = {0}, inVPP = {0}, outMFX = {0}, outVPP = {0};

    inMFX.AsyncDepth = 5;
    inMFX.mfx.NumThread = 4;
    inMFX.mfx.CodecId = MFX_CODEC_HEVC;

    inMFX.mfx.FrameInfo.Width  = 1920;
    inMFX.mfx.FrameInfo.Height = 1080;
    inMFX.mfx.FrameInfo.CropW  = 1920;
    inMFX.mfx.FrameInfo.CropH  = 1080;
    inMFX.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    inMFX.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    mfxExtHEVCParam HEVCParam = {0};

    HEVCParam.Header.BufferId = MFX_EXTBUFF_HEVC_PARAM;
    HEVCParam.Header.BufferSz = sizeof(mfxExtHEVCParam);

    HEVCParam.PicWidthInLumaSamples  = 1920;
    HEVCParam.PicHeightInLumaSamples = 1080;
    HEVCParam.GeneralConstraintFlags = 123456789;

    mfxExtHEVCTiles HEVCTiles = {0};

    HEVCTiles.Header.BufferId = MFX_EXTBUFF_HEVC_TILES;
    HEVCTiles.Header.BufferSz = sizeof(mfxExtHEVCTiles);

    HEVCTiles.NumTileRows = 100;
    HEVCTiles.NumTileColumns = 1000;

    mfxExtBuffer* ExtBuffer1[2] = {0};

    ExtBuffer1[0] = (mfxExtBuffer*)&HEVCParam;
    ExtBuffer1[1] = (mfxExtBuffer*)&HEVCTiles;

    inMFX.NumExtParam = 2;
    inMFX.ExtParam    = ExtBuffer1;

    inVPP.AsyncDepth = 5;
    inVPP.vpp.In.Width  = 1920;
    inVPP.vpp.In.Height = 1080;
    inVPP.vpp.In.CropW  = 1920;
    inVPP.vpp.In.CropH  = 1080;
    inVPP.vpp.In.FrameRateExtN = 1;
    inVPP.vpp.In.FrameRateExtD = 30;
    inVPP.vpp.In.FourCC        = MFX_FOURCC_NV12;
    inVPP.vpp.In.PicStruct     = MFX_PICSTRUCT_FIELD_TFF;

    inVPP.vpp.Out.Width  = 1920;
    inVPP.vpp.Out.Height = 1080;
    inVPP.vpp.Out.CropW  = 1920;
    inVPP.vpp.Out.CropH  = 1080;
    inVPP.vpp.Out.FrameRateExtN = 1;
    inVPP.vpp.Out.FrameRateExtD = 15;
    inVPP.vpp.Out.FourCC        = MFX_FOURCC_RGB4;
    inVPP.vpp.Out.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;

    mfxExtVPPFrameRateConversion VPPFrameRateConversion = {0};

    VPPFrameRateConversion.Header.BufferId = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
    VPPFrameRateConversion.Header.BufferSz = sizeof(mfxExtVPPFrameRateConversion);

    VPPFrameRateConversion.Algorithm = 3;

    mfxExtBuffer* ExtBuffer2[1] = {0};
    ExtBuffer2[0] = (mfxExtBuffer*)&VPPFrameRateConversion;

    inVPP.ExtParam    = ExtBuffer2;
    inVPP.NumExtParam = 1;

    //serialize to list & deserialize from list
    SerializeVideoParamList(&inMFX, &inVPP, &outMFX, &outVPP);

    //clean resources
    CleanExtBuf(&outMFX);
    CleanExtBuf(&outVPP);
    memset(&outMFX, 0, sizeof(outMFX));
    memset(&outVPP, 0, sizeof(outVPP));

    //serialize to yaml & deserialize from yaml
    SerializeVideoParamYaml(&inMFX, &inVPP, &outMFX, &outVPP);

    //clean resources
    CleanExtBuf(&outMFX);
    CleanExtBuf(&outVPP);
    memset(&outMFX, 0, sizeof(outMFX));
    memset(&outVPP, 0, sizeof(outVPP));

    return 0;
}