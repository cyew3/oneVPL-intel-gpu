// Copyright (c) 2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "common_utils.h"
#include "cmd_options.h"
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

static void usage(CmdOptionsCtx* ctx)
{
    printf(
        "Decodes INPUT and apply VPP filters.\n"
        "Save OUTPUT from decode and from all VPP flters"
        "\n"
        "Usage: %s [options] INPUT OUTPUT\n", ctx->program);
}

mfxStatus WriteRawSection(mfxU8* plane, mfxU16 chunksize,
                          mfxFrameData* pData, mfxU32 i,
                          mfxU32 j, FILE* fSink)
{
    if (chunksize !=
        fwrite(plane +
               i * pData->Pitch + j, 1, chunksize, fSink))
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    return MFX_ERR_NONE;
}

mfxStatus WriteRawVideoFrame(mfxFrameSurface1* pSurface, FILE* fSink)
{
    mfxFrameInfo* pInfo = &pSurface->Info;
    mfxFrameData* pData = &pSurface->Data;
    mfxU32 nByteWrite;
    mfxU16 i, j, h, w, pitch;
    mfxU8* ptr;
    mfxStatus sts = MFX_ERR_NONE;

    w = pInfo->Width;
    h = pInfo->Height;

    if (pInfo->FourCC == MFX_FOURCC_RGB4 || pInfo->FourCC == MFX_FOURCC_A2RGB10)
    {
        pitch = pData->Pitch;
        ptr = (std::min)(pData->R, (std::min)(pData->G, pData->B));

        for (i = 0; i < h; i++)
        {
            nByteWrite = (mfxU32)fwrite(ptr + i * pitch, 1, w * 4, fSink);
            if ((mfxU32)(w * 4) != nByteWrite)
            {
                return MFX_ERR_MORE_DATA;
            }
        }
    }
    else
    {
        for (i = 0; i < h; i++)
        {
            sts = WriteRawSection(pData->Y, w, pData, i, 0, fSink);
            if (sts != MFX_ERR_NONE) return sts;
        }

        h = h / 2;
        for (i = 0; i < h; i++)
        {
            for (j = 0; j < w; j += 2)
            {
                sts = WriteRawSection(pData->UV, 1, pData, i, j, fSink);
                if (sts != MFX_ERR_NONE) return sts;
            }
        }
        for (i = 0; i < h; i++)
        {
            for (j = 1; j < w; j += 2)
            {
                sts = WriteRawSection(pData->UV, 1, pData, i, j, fSink);
                if (sts != MFX_ERR_NONE) return sts;
            }
        }
    }

    return sts;
}

void LoadParFile(char *ParFile, std::vector<std::vector<mfxU32>> &skipChannelPresets)
{
    if (!ParFile[0]) return; //param file not supplied
    try
    {
        std::ifstream parFile;
        parFile.open(ParFile, std::ios::in);
        mfxU32 outChannels = 0;
        int value;

        for (;;)
        {
            std::string name;

            parFile >> name;
            if (name == "end_of_file") {
                break;
            } else if (name == "out_channels") {
                parFile >> outChannels;
                if (outChannels != 3) {
                    printf("error: tutorial currently supports exactly 3 output channels\n");
                    exit(-1);
                }
            } else if (name == "skip_pattern") {
                skipChannelPresets.emplace_back();
                for (mfxU32 i=0; i<outChannels; i++) {
                    parFile >> value;
                    if (value >= 0) {
                        skipChannelPresets.back().push_back(value);
                    }
                }
            } else {
                throw std::exception();
            }
        }
    }
    catch(...)
    {
      printf("error: wrong par file has been provided\n");
      exit(-1);
    }
}

// NOTE: This tutorial assumes that input stream resolution is multiple of 64
// in order to support output crop without a side channel

int main(int argc, char* argv[])
{
    mfxStatus sts = MFX_ERR_NONE;

    // =====================================================================
    // Intel Media SDK decode+vp pipeline setup
    // - In this example we are decoding an AVC (H.264) stream
    // - Internal memory allocation used, MSDK selects most suitable memory type
    //   VPP frame processing included in pipeline (resize)
    //   Original output skipped.

    // Read options from the command line (if any is given)
    CmdOptions options = {};
    options.ctx.options = OPTIONS_DECODE | OPTIONS_VPP;
    options.ctx.usage = usage;

    //CORE20 doesn't support DX9 and auto chooses DX9, so choose DX11 explicitly
#if defined(_WIN32) || defined(_WIN64)
    options.values.impl = MFX_IMPL_VIA_D3D11;
#else
    options.values.impl = MFX_IMPL_VIA_VAAPI;
#endif

    // here we parse options
    ParseOptions(argc, argv, &options);

    // skip channel presets (load from param file if any is given)
    std::vector<std::vector<mfxU32>> SkipChannelPresets;
    LoadParFile(options.values.ParFile, SkipChannelPresets);

    //choose IOPattern
    const mfxU16 IOPattern = static_cast<mfxU16>(options.values.VideoMemory ? MFX_IOPATTERN_OUT_VIDEO_MEMORY : MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (strlen(options.values.SinkName) > MSDK_MAX_PATH - 2) // We will append <ChannelId>_ to SinkName for all channels
    {
        printf("error: destination file name is too long\n");
        return -1;
    }

    if (!options.values.SourceName[0]) {
        printf("error: source file name not set (mandatory)\n");
        return -1;
    }

    // Open input H.264 elementary stream (ES) file
    FILE* fSource;
    MSDK_FOPEN(fSource, options.values.SourceName, "rb");
    MSDK_CHECK_POINTER(fSource, MFX_ERR_NULL_PTR);

    // Initialize Intel Media SDK session
    // - MFX_IMPL_AUTO_ANY selects HW acceleration if available (on any adapter)

    mfxIMPL impl = options.values.impl;
    mfxVersion ver = { {2, 2} };
    MFXVideoSession session;
    sts = Initialize(impl, ver, &session, NULL);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Prepare Media SDK bit stream buffer
    // - Arbitrary buffer size for this example
    mfxBitstream mfxBS = {};
    mfxBS.MaxLength = 1024 * 1024;
    std::unique_ptr<mfxU8[]> bsData(new mfxU8[mfxBS.MaxLength]);
    mfxBS.Data = bsData.get();
    MSDK_CHECK_POINTER(mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

    // Read a chunk of data from stream file into bit stream buffer
    // - Parse bit stream, searching for header and fill video parameters structure
    // - Abort if bit stream header is not found in the first bit stream buffer chunk
    sts = ReadBitStreamData(&mfxBS, fSource);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    // Create Media SDK decoder+vp
    MFXVideoDECODE_VPP mfxDecVP(session);

    // Set required video parameters for decode (1st output)
    mfxVideoParam decVideoParams = {};
    decVideoParams.mfx.CodecId = MFX_CODEC_AVC;
    decVideoParams.IOPattern = IOPattern;
    decVideoParams.mfx.FrameInfo.ChannelId = 0; // 0 is reserved by MSDK for decode output

    sts = mfxDecVP.DecodeHeader(&mfxBS, &decVideoParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    // Set required video parameters for VPP
    static const mfxU32 numChannels = 2; // num of VPP channels
    mfxVideoChannelParam vppChannelParams[numChannels]{};

    //init scaling buffers
    mfxExtBuffer* extBuffers[numChannels]{};
    mfxExtVPPScaling scalingBuffers[numChannels]{};
    for(mfxU32 i=0; i<numChannels; i++){
      scalingBuffers[i].Header.BufferId = MFX_EXTBUFF_VPP_SCALING;
      scalingBuffers[i].Header.BufferSz = sizeof(mfxExtVPPScaling);
      extBuffers[i] = &scalingBuffers[i].Header;
      vppChannelParams[i].ExtParam = &extBuffers[i];
      vppChannelParams[i].NumExtParam = 1;
    }

    //first VPP channel (2nd output)
    mfxU32 channelIdx = 0; // VPP channel index
    vppChannelParams[channelIdx].IOPattern = IOPattern;
    vppChannelParams[channelIdx].VPP = decVideoParams.mfx.FrameInfo;
    vppChannelParams[channelIdx].VPP.ChannelId = channelIdx + 1;

    if (options.values.VppOutCrop > 0) {
        vppChannelParams[channelIdx].VPP.CropX = MSDK_ALIGN2(options.values.VppOutCrop);
        vppChannelParams[channelIdx].VPP.CropY = MSDK_ALIGN2(options.values.VppOutCrop);
        vppChannelParams[channelIdx].VPP.CropW = MSDK_ALIGN2(vppChannelParams[channelIdx].VPP.CropW - options.values.VppOutCrop*2);
        vppChannelParams[channelIdx].VPP.CropH = MSDK_ALIGN2(vppChannelParams[channelIdx].VPP.CropH - options.values.VppOutCrop*2);
        vppChannelParams[channelIdx].VPP.Width = MSDK_ALIGN16(vppChannelParams[channelIdx].VPP.Width);
        vppChannelParams[channelIdx].VPP.Height = MSDK_ALIGN16(vppChannelParams[channelIdx].VPP.Height);
    }
    else {
        vppChannelParams[channelIdx].VPP.CropW = MSDK_ALIGN2(vppChannelParams[channelIdx].VPP.CropW / 2);
        vppChannelParams[channelIdx].VPP.CropH = MSDK_ALIGN2(vppChannelParams[channelIdx].VPP.CropH / 2);
        vppChannelParams[channelIdx].VPP.Width = MSDK_ALIGN16(vppChannelParams[channelIdx].VPP.Width / 2);
        vppChannelParams[channelIdx].VPP.Height = MSDK_ALIGN16(vppChannelParams[channelIdx].VPP.Height / 2);
    }

    //we can change color format from command line, but only for first VPP channel
    if (options.values.VppOutFOURCC == MFX_FOURCC_RGB4)
    {
        vppChannelParams[channelIdx].VPP.ChromaFormat = 0;
        vppChannelParams[channelIdx].VPP.FourCC = MFX_FOURCC_RGB4;
    }
    scalingBuffers[channelIdx].ScalingMode = MFX_SCALING_MODE_INTEL_GEN_COMPUTE;
    if (options.values.VppHwPath) {
        scalingBuffers[channelIdx].ScalingMode = options.values.VppHwPath;
    }

    //second VPP channel (3rd output)
    channelIdx = 1; // VPP channel index
    vppChannelParams[channelIdx].IOPattern = IOPattern;
    vppChannelParams[channelIdx].VPP = decVideoParams.mfx.FrameInfo;
    vppChannelParams[channelIdx].VPP.ChannelId = channelIdx + 1;
    vppChannelParams[channelIdx].VPP.CropW = MSDK_ALIGN2(decVideoParams.mfx.FrameInfo.CropW / 4);
    vppChannelParams[channelIdx].VPP.CropH = MSDK_ALIGN2(decVideoParams.mfx.FrameInfo.CropH / 4);
    vppChannelParams[channelIdx].VPP.Width = MSDK_ALIGN16(vppChannelParams[channelIdx].VPP.Width / 4);
    vppChannelParams[channelIdx].VPP.Height = MSDK_ALIGN16(vppChannelParams[channelIdx].VPP.Height / 4);
    scalingBuffers[channelIdx].ScalingMode = MFX_SCALING_MODE_INTEL_GEN_COMPUTE;
    if (options.values.VppHwPath) {
        scalingBuffers[channelIdx].ScalingMode = options.values.VppHwPath;
    }

    mfxVideoChannelParam* vppChannelParamsPtrs[numChannels]{};
    vppChannelParamsPtrs[0] = &vppChannelParams[0];
    vppChannelParamsPtrs[1] = &vppChannelParams[1];


    // Initialize the Media SDK decoder+vp
    sts = mfxDecVP.Init(&decVideoParams, &vppChannelParamsPtrs[0], numChannels);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    // Create output YUV files
    // - File for decode output + files for VPP outputs
    const mfxU32 numFiles = numChannels + 1;
    FILE* fSink[numFiles];
    for (mfxU32 i = 0; i < numFiles; i++)
    {
        std::string fileName(options.values.SinkName);
        size_t pathLen = fileName.find_last_of("/\\");

        std::string path = fileName.substr(0, pathLen + 1);
        std::string name = fileName.substr(pathLen + 1);

        name = name + "." + std::to_string(i); // that is why SinkName length should be less or equal that MSDK_MAX_PATH - 2
        fileName = path + name;

        MSDK_FOPEN(fSink[i], fileName.c_str(), "wb");
        MSDK_CHECK_POINTER(fSink[i], MFX_ERR_NULL_PTR);
    }



    // ===============================================================
    // Start decoding the frames
    //
    mfxTime tStart, tEnd;
    mfxGetTime(&tStart);

    mfxU32 nFrame = 0;

    //
    // Main decoding+processing loop
    //
    bool EndOfStream = false;
    for (;;)
    {
        // Skip channels vector
        std::vector<mfxU32> SkipChannels;
        if (nFrame < SkipChannelPresets.size())
        {
            SkipChannels = SkipChannelPresets[nFrame];
        }

        // Decode and process frames asychronously (returns immediately)
        mfxSurfaceArray* pmfxOutSurfaceArr = nullptr;
        sts = mfxDecVP.DecodeFrameAsync(EndOfStream ? nullptr : &mfxBS,
            SkipChannels.empty() ? nullptr : SkipChannels.data(),
            SkipChannels.size(),
            &pmfxOutSurfaceArr);

        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync
            continue;
        }
        else if (EndOfStream && sts == MFX_ERR_MORE_DATA)
        {
            //stream ended and all buffered frames have been retrieved, finish
            break;
        }
        else if (!EndOfStream && MFX_ERR_MORE_DATA == sts)
        {
            //need more data from bitstream
            sts = ReadBitStreamData(&mfxBS, fSource);
            if (sts == MFX_ERR_NONE) {
                continue;
            }
            //bitstream ended, retrieve buffered frames from DVP
            EndOfStream = true;
            continue;
        }
        //check for error
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        bool NoOutputFromDVP = SkipChannels.size() == numChannels + 1;
        if (!NoOutputFromDVP)
        {
            for (mfxU32 i = 0; i < pmfxOutSurfaceArr->NumSurfaces; i++)
            {
                mfxFrameSurface1* OutSurf = pmfxOutSurfaceArr->Surfaces[i];
                sts = OutSurf->FrameInterface->Map(OutSurf, MFX_MAP_READ);
                MSDK_BREAK_ON_ERROR(sts);

                // Write processed frame to file
                sts = WriteRawVideoFrame(OutSurf, fSink[OutSurf->Info.ChannelId]);
                MSDK_BREAK_ON_ERROR(sts);

                sts = OutSurf->FrameInterface->Unmap(OutSurf);
                MSDK_BREAK_ON_ERROR(sts);
            }

            //release output surfaces and surface array
            for (mfxU32 i = 0; i < pmfxOutSurfaceArr->NumSurfaces; i++)
            {
                mfxFrameSurface1* OutSurf = pmfxOutSurfaceArr->Surfaces[i];
                OutSurf->FrameInterface->Release(OutSurf);
            }
            pmfxOutSurfaceArr->Release(pmfxOutSurfaceArr);
        }

        printf("Frame number: %d\r", ++nFrame);
    }

    mfxGetTime(&tEnd);
    double elapsed = TimeDiffMsec(tEnd, tStart) / 1000;
    double fps = ((double)nFrame / elapsed);
    printf("\nExecution time: %3.2f s (%3.2f fps)\n", elapsed, fps);

    // ===================================================================
    // Clean up resources
    //  - Surface already leaned during pmfxOutSurfaceArr->Release(pmfxOutSurfaceArr) call

    mfxDecVP.Close();

    fclose(fSource);
    for (mfxU32 i = 0; i < numFiles; i++)
    {
        fclose(fSink[i]);
    }

    return 0;
}

