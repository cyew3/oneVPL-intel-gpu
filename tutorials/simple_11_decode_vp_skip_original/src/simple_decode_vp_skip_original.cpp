// Copyright (c) 2020 Intel Corporation
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
#include <memory>

static void usage(CmdOptionsCtx* ctx)
{
    printf(
        "Decode INPUT and apply VPP filters.\n"
        "Ignore OUTPUT from decode and save from VPP filter (resize)"
        "\n"
        "Usage: %s [options] INPUT OUTPUT\n", ctx->program);
}

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
    options.values.impl = MFX_IMPL_AUTO_ANY;
    options.values.adapterType = mfxMediaAdapterType::MFX_MEDIA_UNKNOWN;

    // here we parse options
    ParseOptions(argc, argv, &options);

    if (!options.values.SourceName[0]) {
        printf("error: source file name not set (mandatory)\n");
        return -1;
    }

    // Open input H.264 elementary stream (ES) file
    FILE* fSource;
    MSDK_FOPEN(fSource, options.values.SourceName, "rb");
    MSDK_CHECK_POINTER(fSource, MFX_ERR_NULL_PTR);

    // Create output YUV file
    FILE* fSink = NULL;
    MSDK_FOPEN(fSink, options.values.SinkName, "wb");
    MSDK_CHECK_POINTER(fSink, MFX_ERR_NULL_PTR);

    // Initialize Intel Media SDK session
    // - MFX_IMPL_AUTO_ANY selects HW acceleration if available (on any adapter)

    mfxVersion minVer = { {2, 2} };
    mfxIMPL impl = options.values.impl;
    mfxU16  adapterType = options.values.adapterType;
    mfxU32  adapterNum = options.values.adapterNum;
    MainLoader loader;
    MainVideoSession session;
    sts = Initialize(impl, minVer, adapterType, adapterNum, &loader, &session, NULL);
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

    // Set required video parameters for decode
    mfxVideoParam mfxVideoParams = {};
    mfxVideoParams.mfx.CodecId = MFX_CODEC_AVC;
    mfxVideoParams.mfx.SkipOutput = true; // no need to specify IOPattern for decode. It will be selected by MSDK.
    mfxVideoParams.mfx.FrameInfo.ChannelId = 0; // 0 is reserved by MSDK for decode output.

    sts = mfxDecVP.DecodeHeader(&mfxBS, &mfxVideoParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Set required video parameters for VP (resize)
    mfxVideoChannelParam mfxChannelParamResize = {};
    mfxChannelParamResize.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    mfxChannelParamResize.VPP = mfxVideoParams.mfx.FrameInfo;
    mfxChannelParamResize.VPP.ChannelId = 1;
    mfxChannelParamResize.VPP.CropW = MSDK_ALIGN16(mfxChannelParamResize.VPP.Width / 2);
    mfxChannelParamResize.VPP.CropH = MSDK_ALIGN16(mfxChannelParamResize.VPP.Height / 2);
    mfxChannelParamResize.VPP.Width = mfxChannelParamResize.VPP.CropW;
    mfxChannelParamResize.VPP.Height = mfxChannelParamResize.VPP.CropH;

    mfxVideoChannelParam* mfxChannelParams[1];
    mfxChannelParams[0] = &mfxChannelParamResize;

    mfxU32 numChannels = 1;

    // Initialize the Media SDK decoder+vp
    sts = mfxDecVP.Init(&mfxVideoParams, &mfxChannelParams[0], numChannels);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // ===============================================================
    // Start decoding the frames
    //
    mfxTime tStart, tEnd;
    mfxGetTime(&tStart);

    mfxSurfaceArray* pmfxOutSurfaceArr = NULL;
    mfxFrameSurface1* workingSurf = NULL;
    mfxU32 nFrame = 0;

    //
    // Stage 1: Main decoding loop
    //
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts) {
        if (MFX_WRN_DEVICE_BUSY == sts)
            MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

        if (MFX_ERR_MORE_DATA == sts) {
            sts = ReadBitStreamData(&mfxBS, fSource);       // Read more data into input bit stream
            MSDK_BREAK_ON_ERROR(sts);
        }

        // Decode and process frames asychronously (returns immediately)
        sts = mfxDecVP.DecodeFrameAsync(&mfxBS, NULL, 0, &pmfxOutSurfaceArr);

        if (MFX_ERR_NONE <= sts)
        {
            workingSurf = pmfxOutSurfaceArr->Surfaces[0]; // only one surface (resized) in array due to skipped decode output
            sts = workingSurf->FrameInterface->Synchronize(workingSurf, 60000); // Wait until frame is ready
        }

        // Write processed frame to file
        if (MFX_ERR_NONE == sts)
        {
            ++nFrame;
            sts = WriteRawFrame(workingSurf, fSink);
            MSDK_BREAK_ON_ERROR(sts);

            printf("Frame number: %d\r", nFrame);
        }

        // Free surface array
        if (pmfxOutSurfaceArr)
        {
            pmfxOutSurfaceArr->Release(pmfxOutSurfaceArr);
            pmfxOutSurfaceArr = NULL;
        }
    }

    // MFX_ERR_MORE_DATA means that file has ended, need to go to buffering loop, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //
    // Stage 2: Retrieve the buffered decoded frames
    //
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts) {
        if (MFX_WRN_DEVICE_BUSY == sts)
            MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

        if (MFX_ERR_MORE_DATA == sts) {
            sts = ReadBitStreamData(&mfxBS, fSource);       // Read more data into input bit stream
            MSDK_BREAK_ON_ERROR(sts);
        }

        // Decode and process frames asychronously (returns immediately)
        sts = mfxDecVP.DecodeFrameAsync(NULL, NULL, 0, &pmfxOutSurfaceArr);

        if (MFX_ERR_NONE <= sts)
        {
            workingSurf = pmfxOutSurfaceArr->Surfaces[0]; // only one surface (resized) in array due to skipped decode output
            sts = workingSurf->FrameInterface->Synchronize(workingSurf, 60000); // Wait until frame is ready
        }

        // Write processed frame to file
        if (MFX_ERR_NONE == sts)
        {
            ++nFrame;
            sts = WriteRawFrame(workingSurf, fSink);
            MSDK_BREAK_ON_ERROR(sts);

            printf("Frame number: %d\r", nFrame);
        }

        // Free surface array
        if (pmfxOutSurfaceArr)
        {
            pmfxOutSurfaceArr->Release(pmfxOutSurfaceArr);
            pmfxOutSurfaceArr = NULL;
        }
    }

    // MFX_ERR_MORE_DATA indicates that all buffers has been fetched, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxGetTime(&tEnd);
    double elapsed = TimeDiffMsec(tEnd, tStart) / 1000;
    double fps = ((double)nFrame / elapsed);
    printf("\nExecution time: %3.2f s (%3.2f fps)\n", elapsed, fps);

    // ===================================================================
    // Clean up resources
    //  - Surface already leaned during pmfxOutSurfaceArr->Release(pmfxOutSurfaceArr) call

    mfxDecVP.Close();

    // Free surface array
    if (pmfxOutSurfaceArr)
    {
        pmfxOutSurfaceArr->Release(pmfxOutSurfaceArr);
    }

    // session closed automatically on destruction
    MSDK_SAFE_DELETE_ARRAY(mfxBS.Data);

    fclose(fSource);
    fclose(fSink);

    return 0;
}
