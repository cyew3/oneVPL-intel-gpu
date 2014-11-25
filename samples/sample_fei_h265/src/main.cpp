/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sample_fei_h265.h"
#include "plugin_wrapper.h"

#define YPLANE_ALIGN    16

static void Usage(void)
{
    printf("Usage: sample_fei.exe -i sourceFile.yuv [options]\n\n");
    printf("Options:\n");
    printf("   -i sourceFile.yuv   uncompressed input file (8-bit YUV420)\n");
    printf("   -r reconFile.yuv    reconstructed frames (optional - if not provided use source as recon)\n");
    printf("   -p params.txt       read file/frame parameters from config file\n");
    printf("   -n nFrames          maximum number of frames to process\n");
    printf("   -h height           height in pixels (only used if params.txt not provided)\n");
    printf("   -w width            width in pixels  (only used if params.txt not provided)\n");
    printf("   -c MaxCUSize        maximum coding unit size (16 or 32)\n");
    printf("   -m MPMode           motion partitioning mode (1 = square only, 2 = symmetric partitions, 3 = any)\n");
    printf("   -y                  write output to text files\n");
    printf("\nExamples:\n");
    printf(">> sample_h265_fei.exe -i sourceFile.yuv -w 1280 -h 720 -n 50\n");
    printf(">> sample_h265_fei.exe -i sourceFile.yuv -r reconFile.yuv -p params.txt\n");
    exit(-1);
}

static void ParseCmdLine(int argc, char **argv, SampleParams *sp)
{
    int i, err;

    memset(sp, 0, sizeof(SampleParams));

    for (i = 1; i < argc; ) {
        if (argv[i][0] != '-') {
            printf("Error - bad arguments\n");
            Usage();
        }

        switch (argv[i+0][1]) {
        case 'i':
            err = fopen_s(&sp->SourceFile, argv[i+1], "rb");
            if (err) {
                printf("Error - failed to open %s\n", argv[i+1]);
                Usage();
            }
            i += 2;
            break;
        case 'r':
            err = fopen_s(&sp->ReconFile, argv[i+1], "rb");
            if (err) {
                printf("Error - failed to open %s\n", argv[i+1]);
                Usage();
            }
            i += 2;
            break;
        case 'p':
            err = fopen_s(&sp->ParamsFile, argv[i+1], "rt");
            if (err) {
                printf("Error - failed to open %s\n", argv[i+1]);
                Usage();
            }
            i += 2;
            break;
        case 'n':
            sp->NFrames = atoi(argv[i+1]);
            if (sp->NFrames <= 0) {
                printf("Error - invalid nFrames\n");
                Usage();
            }
            i += 2;
            break;
        case 'h':
            sp->Height = atoi(argv[i+1]);
            if (sp->Height <= 0) {
                printf("Error - invalid height\n");
                Usage();
            }
            i += 2;
            break;
        case 'w':
            sp->Width = atoi(argv[i+1]);
            if (sp->Width <= 0) {
                printf("Error - invalid width\n");
                Usage();
            }
            i += 2;
            break;
        case 'c':
            sp->MaxCUSize = atoi(argv[i+1]);
            if (sp->MaxCUSize != 16 && sp->MaxCUSize != 32) {
                printf("Error - invalid MaxCUSize\n");
                Usage();
            }
            i += 2;
            break;
        case 'm':
            sp->MPMode = atoi(argv[i+1]);
            if (sp->MPMode != 1 && sp->MPMode != 2 && sp->MPMode != 3) {
                printf("Error - invalid MPMode\n");
                Usage();
            }
            i += 2;
            break;
        case 'y':
            sp->WriteFiles = 1;
            i += 1;
            break;
        default:
            printf("Error - invalid argument\n");
            Usage();
        }
    }

    /* input frames should be padded up to multiple of 16 */
    if (sp->Width)
        sp->PaddedWidth  = (sp->Width + 15) & ~0x0f;
    if (sp->Height)
        sp->PaddedHeight = (sp->Height + 15) & ~0x0f;

}

int main(int argc, char **argv)
{
    int nRead, feiOutIdx;
    unsigned int i, encOrder;
    unsigned char *srcFrame, *refFrame[MFX_FEI_H265_MAX_NUM_REF_FRAMES];
    clock_t startTime, endTime;
    mfxStatus err;
    SampleParams sp;
    FrameInfo fi;
    ProcessInfo pi;

    /* defined in public header mfxfeih265.h */
    mfxFEIH265Output feiH265Out[2];
    mfxSyncPoint     feiH265Syncp_Intra, feiH265Syncp_Inter[MFX_FEI_H265_MAX_NUM_REF_FRAMES];

    CH265FEI *hFEI;

    if (argc < 2)
        Usage();

    ParseCmdLine(argc, argv, &sp);

    if (sp.ParamsFile) {
        /* read file header */
        ReadFileParams(&sp);
        if (!sp.Width && !sp.Height) {
            /* can specify these on cmd-line to allow unpadded source file, otherwise let width = paddedWidth (from params.txt) */
            sp.Width  = sp.PaddedWidth;
            sp.Height = sp.PaddedHeight;
        }
    } else {
        SetFileDefaults(&sp);
    }

    hFEI = new CH265FEI;
    err = hFEI->Init(&sp);

    /* initialize FEI library */
    if (err) {
        printf("Error initializing FEI library (err = %d)\n", err);

        /* free resources (avoid KW warnings) */
        if (sp.ParamsFile)  fclose(sp.ParamsFile);
        if (sp.SourceFile)  fclose(sp.SourceFile);
        if (sp.ReconFile)   fclose(sp.ReconFile);
        delete hFEI;

        return -1;
    }
    memset(&pi,  0, sizeof(ProcessInfo));
    memset(&feiH265Out[0], 0, sizeof(mfxFEIH265Output));
    memset(&feiH265Out[1], 0, sizeof(mfxFEIH265Output));

    /* allocate one input frame and two reference frames (must be 16-byte aligned for GPU processing) */
    srcFrame = (unsigned char *)_aligned_malloc(sp.PaddedWidth*sp.PaddedHeight*3/2, YPLANE_ALIGN);
    for (i = 0; i < MFX_FEI_H265_MAX_NUM_REF_FRAMES; i++)
        refFrame[i] = (unsigned char *)_aligned_malloc(sp.PaddedWidth*sp.PaddedHeight*3/2, YPLANE_ALIGN);

    if (sp.WriteFiles)
        OpenOutputFiles();

    printf("Running...\n");
    encOrder = 0;
    feiOutIdx = 0;
    startTime = clock();
    while (1) {
        /* get frame parameters */
        if (sp.ParamsFile) {
            nRead = ReadFrameParams(sp.ParamsFile, &fi);
            if (nRead == EOF)
                break;
        } else {
            SetFrameDefaults(encOrder, &fi);
        }
        encOrder++;

        //printf("Processing frame %d...\r", fi.EncOrder);

        if (LoadFrame(sp.SourceFile, sp.Width, sp.Height, sp.PaddedWidth, sp.PaddedHeight, fi.PicOrder, srcFrame))
            break;

        /* intra processing */
        pi.FEIOp = (MFX_FEI_H265_OP_INTRA_MODE | MFX_FEI_H265_OP_INTRA_DIST);
        pi.FrameType = fi.FrameType;

        pi.YPlaneSrc   = srcFrame;
        pi.YPitchSrc   = sp.PaddedWidth;
        pi.EncOrderSrc = fi.EncOrder;
        pi.PicOrderSrc = fi.PicOrder;

        hFEI->ProcessFrameAsync(&pi, &feiH265Out[feiOutIdx], &feiH265Syncp_Intra);

        /* inter ME and half-pel interpolation */
        pi.FEIOp = (MFX_FEI_H265_OP_INTER_ME | MFX_FEI_H265_OP_INTERPOLATE);
        for (i = 0; i < fi.RefNum; i++) {
            pi.RefIdx = i;

            /* load first reference frame (for P or B) */
            if (sp.ReconFile)   LoadFrame(sp.ReconFile, sp.PaddedWidth, sp.PaddedHeight, sp.PaddedWidth, sp.PaddedHeight, fi.RefPicOrder[i], refFrame[i]);
            else                LoadFrame(sp.SourceFile, sp.Width, sp.Height, sp.PaddedWidth, sp.PaddedHeight, fi.RefPicOrder[i], refFrame[i]);

            pi.YPlaneRef   = refFrame[i];
            pi.YPitchRef   = sp.PaddedWidth;
            pi.EncOrderRef = fi.RefEncOrder[i];
            pi.PicOrderRef = fi.RefPicOrder[i];

            hFEI->ProcessFrameAsync(&pi, &feiH265Out[feiOutIdx], &feiH265Syncp_Inter[i]);
        }

        /* wait for intra async processing to finish */
        hFEI->SyncOperation(feiH265Syncp_Intra);

        if (sp.WriteFiles) {
            /* write intra prediction modes */
            WriteFrameIntraModes(&feiH265Out[feiOutIdx]);

            /* write intra distortion estimates */
            if (fi.FrameType == MFX_FRAMETYPE_P || fi.FrameType == MFX_FRAMETYPE_B)
                WriteFrameIntraDist(&feiH265Out[feiOutIdx]);
        }

        /* wait for inter async processing to finish */
        for (i = 0; i < fi.RefNum; i++)
            hFEI->SyncOperation(feiH265Syncp_Inter[i]);

        if (sp.WriteFiles) {
            /* write inter estimates and interpolated frames */
            for (i = 0; i < fi.RefNum; i++) {
                WriteFrameInterSmall(&feiH265Out[feiOutIdx], i, MFX_FEI_H265_BLK_8x8);
                WriteFrameInterSmall(&feiH265Out[feiOutIdx], i, MFX_FEI_H265_BLK_16x16);

                /* large blocks enabled */
                if (sp.MaxCUSize >= 32)
                    WriteFrameInterLarge(&feiH265Out[feiOutIdx], i, MFX_FEI_H265_BLK_32x32);

                /* non-square blocks enabled */
                if (sp.MaxCUSize >= 32 && sp.MPMode > 1) {
                    WriteFrameInterLarge(&feiH265Out[feiOutIdx], i, MFX_FEI_H265_BLK_16x32);
                    WriteFrameInterLarge(&feiH265Out[feiOutIdx], i, MFX_FEI_H265_BLK_32x16);
                }
                WriteFrameInterpolate(&feiH265Out[feiOutIdx], i);
            }
        }

        if (sp.NFrames > 0 && encOrder >= sp.NFrames)
            break;

        /* FEI can process up to 2 src frames at once, allowing lookahead */
        feiOutIdx = 1 - feiOutIdx;
    }
    endTime = clock();

    /* shutdown and cleanup */
    hFEI->Close();
    delete hFEI;

    if (sp.WriteFiles)
        CloseOutputFiles();

    _aligned_free(srcFrame);
    for (i = 0; i < MFX_FEI_H265_MAX_NUM_REF_FRAMES; i++)
        _aligned_free(refFrame[i]);

    printf("Finished!\n");
    printf("Processed %d frames in %.2f secs --> %.1f FPS\n", encOrder, (float)(endTime - startTime) / CLOCKS_PER_SEC, (float)encOrder * CLOCKS_PER_SEC / (endTime - startTime));

    return 0;
}
