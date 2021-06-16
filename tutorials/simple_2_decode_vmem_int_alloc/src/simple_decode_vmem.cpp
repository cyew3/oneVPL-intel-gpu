// Copyright (c) 2019-2020 Intel Corporation
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

//#define USE_INTERNAL_ALLOC_MODEL3
#define USE_INTERNAL_ALLOC_MODEL2
#define USE_SYSMEM

#if defined(DX9_D3D)
#undef USE_INTERNAL_ALLOC_MODEL2
#endif

static void usage(CmdOptionsCtx* ctx)
{
    printf(
        "Decodes INPUT and optionally writes OUTPUT.\n"
        "\n"
        "Usage: %s [options] INPUT [OUTPUT]\n", ctx->program);
}

int main(int argc, char** argv)
{
    mfxStatus sts = MFX_ERR_NONE;
    bool bEnableOutput; // if true, removes all YUV file writing
    CmdOptions options;

    // =====================================================================
    // Intel Media SDK decode pipeline setup
    // - In this example we are decoding an AVC (H.264) stream
    // - Video memory surfaces are used to store the decoded frames
    //   (Note that when using HW acceleration video surfaces are preferred, for better performance)
    //

    // Read options from the command line (if any is given)
    memset(&options, 0, sizeof(CmdOptions));
    options.ctx.options = OPTIONS_DECODE;
    options.ctx.usage = usage;
    // Set default values:
    options.values.impl = MFX_IMPL_AUTO_ANY;
    options.values.adapterType = mfxMediaAdapterType::MFX_MEDIA_UNKNOWN;

    // here we parse options
    ParseOptions(argc, argv, &options);

    if (!options.values.SourceName[0]) {
        printf("error: source file name not set (mandatory)\n");
        return -1;
    }

    bEnableOutput = (options.values.SinkName[0] != '\0');
    // Open input H.264 elementary stream (ES) file
    fileUniPtr fSource(OpenFile(options.values.SourceName, "rb"), &CloseFile);
    MSDK_CHECK_POINTER(fSource, MFX_ERR_NULL_PTR);
    // Create output YUV file
    fileUniPtr fSink(nullptr, &CloseFile);
    if (bEnableOutput) {
        fSink.reset(OpenFile(options.values.SinkName, "wb"));
        MSDK_CHECK_POINTER(fSink, MFX_ERR_NULL_PTR);
    }

    // Initialize Intel Media SDK session
    // - MFX_IMPL_AUTO_ANY selects HW acceleration if available (on any adapter)
    // OS specific notes
    // - On Windows both SW and HW libraries may present
    // - On Linux only HW library only is available
    //   If more recent API features are needed, change the version accordingly
    mfxVersion minVer = { {2, 2} };
    mfxIMPL impl = options.values.impl;
    mfxU16  adapterType = options.values.adapterType;
    mfxU32  adapterNum = options.values.adapterNum;
    MainLoader loader;
    MainVideoSession session;

#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
    mfxFrameAllocator mfxAllocator;
    sts = Initialize(impl, minVer, adapterType, adapterNum, &loader, &session, &mfxAllocator);
#else
    sts = Initialize(impl, minVer, adapterType, adapterNum, &loader, &session, nullptr);
#endif
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Create Media SDK decoder
    MFXVideoDECODE mfxDEC(session);

    // Set required video parameters for decode
    mfxVideoParam mfxVideoParams;
    memset(&mfxVideoParams, 0, sizeof(mfxVideoParams));
    mfxVideoParams.mfx.CodecId = MFX_CODEC_AVC;
#ifdef USE_SYSMEM
    mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
#else
    mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
#endif

    // Prepare Media SDK bit stream buffer
    // - Arbitrary buffer size for this example
    mfxBitstream mfxBS;
    memset(&mfxBS, 0, sizeof(mfxBS));
    mfxBS.MaxLength = 1024 * 1024;
    std::vector<mfxU8> bstData(mfxBS.MaxLength);
    mfxBS.Data = bstData.data();


    // Read a chunk of data from stream file into bit stream buffer
    // - Parse bit stream, searching for header and fill video parameters structure
    // - Abort if bit stream header is not found in the first bit stream buffer chunk
    sts = ReadBitStreamData(&mfxBS, fSource.get());
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = mfxDEC.DecodeHeader(&mfxBS, &mfxVideoParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Validate video decode parameters (optional)
    // sts = mfxDEC.Query(&mfxVideoParams, &mfxVideoParams);

#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
    // Query number of required surfaces for decoder
    mfxFrameAllocRequest Request;
    memset(&Request, 0, sizeof(Request));
    sts = mfxDEC.QueryIOSurf(&mfxVideoParams, &Request);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxU16 numSurfaces = Request.NumFrameSuggested;

    Request.Type |= WILL_READ; // This line is only required for Windows DirectX11 to ensure that surfaces can be retrieved by the application
    // Allocate surfaces for decoder
    mfxFrameAllocResponse mfxResponse;
    sts = mfxAllocator.Alloc(mfxAllocator.pthis, &Request, &mfxResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Allocate surface headers (mfxFrameSurface1) for decoder
    std::vector<mfxFrameSurface1> pmfxSurfaces(numSurfaces);
    for (int i = 0; i < numSurfaces; i++) {
        memset(&pmfxSurfaces[i], 0, sizeof(mfxFrameSurface1));
        pmfxSurfaces[i].Info = mfxVideoParams.mfx.FrameInfo;
        pmfxSurfaces[i].Data.MemId = mfxResponse.mids[i];      // MID (memory id) represents one video NV12 surface
    }
#endif

    // Initialize the Media SDK decoder
    sts = mfxDEC.Init(&mfxVideoParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // ===============================================================
    // Start decoding the frames
    //

    mfxTime tStart, tEnd;
    mfxGetTime(&tStart);

    mfxSyncPoint syncp;
    mfxFrameSurface1* pmfxOutSurface = NULL, *surf = nullptr;
    int nIndex = 0;
    mfxU32 nFrame = 0;

    //
    // Stage 1: Main decoding loop
    //
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts) {
        if (MFX_WRN_DEVICE_BUSY == sts)
            MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

        if (MFX_ERR_MORE_DATA == sts) {
            sts = ReadBitStreamData(&mfxBS, fSource.get());       // Read more data into input bit stream
            MSDK_BREAK_ON_ERROR(sts);
        }

        if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts) {
#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
            nIndex = GetFreeSurfaceIndex(pmfxSurfaces);        // Find free frame surface
            MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nIndex, MFX_ERR_MEMORY_ALLOC);
#else
#ifndef USE_INTERNAL_ALLOC_MODEL3
            sts = mfxDEC.GetSurface(&surf);
            MSDK_BREAK_ON_ERROR(sts);
#endif
#endif
        }
        // Decode a frame asynchronously (returns immediately)
        //  - If input bitstream contains multiple frames DecodeFrameAsync will start decoding multiple frames, and remove them from bitstream
#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
        sts = mfxDEC.DecodeFrameAsync(&mfxBS, &pmfxSurfaces[nIndex], &pmfxOutSurface, &syncp);
#else
#if defined USE_INTERNAL_ALLOC_MODEL2
        sts = mfxDEC.DecodeFrameAsync(&mfxBS, surf, &pmfxOutSurface, &syncp);

        mfxStatus sts_release = surf->FrameInterface->Release(surf);
        MSDK_BREAK_ON_ERROR(sts_release);
#else
        sts = mfxDEC.DecodeFrameAsync(&mfxBS, nullptr, &pmfxOutSurface, &syncp);
#endif
#endif

        // Ignore warnings if output is available,
        // if no output and no action required just repeat the DecodeFrameAsync call
        if (MFX_ERR_NONE < sts && syncp)
            sts = MFX_ERR_NONE;

        if (MFX_ERR_NONE == sts)
            sts = session.SyncOperation(syncp, 60000);      // Synchronize. Wait until decoded frame is ready

        if (MFX_ERR_NONE == sts) {
            ++nFrame;
            if (bEnableOutput) {
#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
                //Surface locking required when read/write video surfaces
                sts = mfxAllocator.Lock(mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
#else
                sts = pmfxOutSurface->FrameInterface->Map(pmfxOutSurface, MFX_MAP_READ);
#endif
                MSDK_BREAK_ON_ERROR(sts);

                sts = WriteRawFrame(pmfxOutSurface, fSink.get());
                MSDK_BREAK_ON_ERROR(sts);

#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
                sts = mfxAllocator.Unlock(mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
                MSDK_BREAK_ON_ERROR(sts);
#else
                sts = pmfxOutSurface->FrameInterface->Unmap(pmfxOutSurface);
                MSDK_BREAK_ON_ERROR(sts);

                sts = pmfxOutSurface->FrameInterface->Release(pmfxOutSurface);
                MSDK_BREAK_ON_ERROR(sts);
#endif

                printf("Frame number: %d\r", nFrame);
                fflush(stdout);
            }
        }
    }

    // MFX_ERR_MORE_DATA means that file has ended, need to go to buffering loop, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //
    // Stage 2: Retrieve the buffered decoded frames
    //
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_SURFACE == sts) {
        if (MFX_WRN_DEVICE_BUSY == sts)
            MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
        nIndex = GetFreeSurfaceIndex(pmfxSurfaces);        // Find free frame surface
        MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nIndex, MFX_ERR_MEMORY_ALLOC);
#else
#ifndef USE_INTERNAL_ALLOC_MODEL3
        sts = mfxDEC.GetSurface(&surf);
        MSDK_BREAK_ON_ERROR(sts);
#endif
#endif

        // Decode a frame asynchronously (returns immediately)
#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
        sts = mfxDEC.DecodeFrameAsync(nullptr, &pmfxSurfaces[nIndex], &pmfxOutSurface, &syncp);
#else
#if defined USE_INTERNAL_ALLOC_MODEL2
        sts = mfxDEC.DecodeFrameAsync(nullptr, surf, &pmfxOutSurface, &syncp);

        mfxStatus sts_release = surf->FrameInterface->Release(surf);
        MSDK_BREAK_ON_ERROR(sts_release);
#else
        sts = mfxDEC.DecodeFrameAsync(nullptr, nullptr, &pmfxOutSurface, &syncp);
#endif
#endif

        // Ignore warnings if output is available,
        // if no output and no action required just repeat the DecodeFrameAsync call
        if (MFX_ERR_NONE < sts && syncp)
            sts = MFX_ERR_NONE;

        if (MFX_ERR_NONE == sts)
            sts = session.SyncOperation(syncp, 60000);      // Synchronize. Waits until decoded frame is ready

        if (MFX_ERR_NONE == sts) {
            ++nFrame;
            if (bEnableOutput) {
                // Surface locking required when read/write D3D surfaces
#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
                sts = mfxAllocator.Lock(mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
#else
                sts = pmfxOutSurface->FrameInterface->Map(pmfxOutSurface, MFX_MAP_READ);
#endif
                MSDK_BREAK_ON_ERROR(sts);

                sts = WriteRawFrame(pmfxOutSurface, fSink.get());
                MSDK_BREAK_ON_ERROR(sts);

#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
                sts = mfxAllocator.Unlock(mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
                MSDK_BREAK_ON_ERROR(sts);
#else
                sts = pmfxOutSurface->FrameInterface->Unmap(pmfxOutSurface);
                MSDK_BREAK_ON_ERROR(sts);

                sts = pmfxOutSurface->FrameInterface->Release(pmfxOutSurface);
                MSDK_BREAK_ON_ERROR(sts);
#endif

                printf("Frame number: %d\r", nFrame);
                fflush(stdout);
            }
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
    //  - It is recommended to close Media SDK components first, before releasing allocated surfaces, since
    //    some surfaces may still be locked by internal Media SDK resources.

    mfxDEC.Close();
    // session closed automatically on destruction

#if !defined USE_INTERNAL_ALLOC_MODEL2 && !defined USE_INTERNAL_ALLOC_MODEL3
    mfxAllocator.Free(mfxAllocator.pthis, &mfxResponse);
#endif

    Release();

    return 0;
}
