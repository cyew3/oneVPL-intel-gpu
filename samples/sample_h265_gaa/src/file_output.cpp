/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sample_fei_h265.h"

static const char *fileNameIntraModes = "h265_fei_intramodes_sample.txt";
static const char *fileNameIntraDist  = "h265_fei_intradist_sample.txt";
static const char *fileNameInterMV    = "h265_fei_intermv_sample.txt";
static const char *fileNameInterDist  = "h265_fei_interdist_sample.txt";
static const char *fileNameInterpolate[3] = {
    "h265_fei_interp_H_sample.yuv",
    "h265_fei_interp_V_sample.yuv",
    "h265_fei_interp_D_sample.yuv",
};

static FILE *outfileIntraModes = 0;
static FILE *outfileIntraDist = 0;
static FILE *outfileInterMV = 0;
static FILE *outfileInterDist = 0;
static FILE *outfileInterpolate[3] = { 0 };

#define MAX_BW_WIDTH    8192
static unsigned char bwBuf[MAX_BW_WIDTH];

void OpenOutputFiles(void)
{
    int i, err;

    /* intra modes - sorted list of best candidates */
    err = fopen_s(&outfileIntraModes, fileNameIntraModes, "wt");
    if (err) {
        printf("Warning - failed to open %s", fileNameIntraModes);
        return;
    }

    /* intra distortion estimates on 16x16 grid */
    err = fopen_s(&outfileIntraDist, fileNameIntraDist, "wt");
    if (err) {
        printf("Warning - failed to open %s", fileNameIntraDist);
        return;
    }

    /* inter motion vectors */
    err = fopen_s(&outfileInterMV, fileNameInterMV, "wt");
    if (err) {
        printf("Warning - failed to open %s", fileNameInterMV);
        return;
    }

    /* inter distortion estimates */
    err = fopen_s(&outfileInterDist, fileNameInterDist, "wt");
    if (err) {
        printf("Warning - failed to open %s", fileNameInterDist);
        return;
    }

    /* half-pel interpolated places (3 files - H,V,D) */
    for (i = 0; i < 3; i++) {
        err = fopen_s(&outfileInterpolate[i], fileNameInterpolate[i], "wb");
        if (err) {
            printf("Warning - failed to open %s", fileNameInterpolate[i]);
            return;
        }
    }

    /* fill U/V planes with 0x80 (YUV420)) */
    memset(bwBuf, 0x80, sizeof(bwBuf));
}

void CloseOutputFiles(void)
{
    if (outfileIntraModes)      fclose(outfileIntraModes);
    if (outfileIntraDist)       fclose(outfileIntraDist);

    if (outfileInterMV)         fclose(outfileInterMV);
    if (outfileInterDist)       fclose(outfileInterDist);

    if (outfileInterpolate[0])  fclose(outfileInterpolate[0]);
    if (outfileInterpolate[1])  fclose(outfileInterpolate[1]);
    if (outfileInterpolate[2])  fclose(outfileInterpolate[2]);
}

void WriteFrameIntraModes(mfxFEIH265Output *feiOut)
{
    unsigned int i, x, y;

    fprintf(outfileIntraModes, "Intra Modes 4x4\n");
    for (y = 0; y < feiOut->PaddedHeight / 4; y++) {
        for (x = 0; x < feiOut->PaddedWidth / 4; x++) {
            fprintf(outfileIntraModes, "block [% 5d, % 5d]: ", y*4, x*4);
            for (i = 0; i < feiOut->IntraMaxModes; i++) {
                fprintf(outfileIntraModes, "%d ",   feiOut->IntraModes4x4[feiOut->IntraMaxModes*(y*feiOut->IntraPitch4x4 + x) + i]);
            }
            fprintf(outfileIntraModes, "\n");
        }
    }
    fprintf(outfileIntraModes, "\n");

    fprintf(outfileIntraModes, "Intra Modes 8x8\n");
    for (y = 0; y < feiOut->PaddedHeight / 8; y++) {
        for (x = 0; x < feiOut->PaddedWidth / 8; x++) {
            fprintf(outfileIntraModes, "block [% 5d, % 5d]: ", y*8, x*8);
            for (i = 0; i < feiOut->IntraMaxModes; i++) {
                fprintf(outfileIntraModes, "%d ", feiOut->IntraModes8x8[feiOut->IntraMaxModes*(y*feiOut->IntraPitch8x8 + x) + i]);
            }
            fprintf(outfileIntraModes, "\n");
        }
    }
    fprintf(outfileIntraModes, "\n");

    fprintf(outfileIntraModes, "Intra Modes 16x16\n");
    for (y = 0; y < feiOut->PaddedHeight / 16; y++) {
        for (x = 0; x < feiOut->PaddedWidth / 16; x++) {
            fprintf(outfileIntraModes, "block [% 5d, % 5d]: ", y*16, x*16);
            for (i = 0; i < feiOut->IntraMaxModes; i++) {
                fprintf(outfileIntraModes, "%d ", feiOut->IntraModes16x16[feiOut->IntraMaxModes*(y*feiOut->IntraPitch16x16 + x) + i]);
            }
            fprintf(outfileIntraModes, "\n");
        }
    }
    fprintf(outfileIntraModes, "\n");

    fprintf(outfileIntraModes, "Intra Modes 32x32\n");
    for (y = 0; y < feiOut->PaddedHeight / 32; y++) {
        for (x = 0; x < feiOut->PaddedWidth / 32; x++) {
            fprintf(outfileIntraModes, "block [% 5d, % 5d]: ", y*32, x*32);
            for (i = 0; i < feiOut->IntraMaxModes; i++) {
                fprintf(outfileIntraModes, "%d ", feiOut->IntraModes32x32[feiOut->IntraMaxModes*(y*feiOut->IntraPitch32x32 + x) + i]);
            }
            fprintf(outfileIntraModes, "\n");
        }
    }
    fprintf(outfileIntraModes, "\n");
}

void WriteFrameIntraDist(mfxFEIH265Output *feiOut)
{
    unsigned int x, y;

    fprintf(outfileIntraDist, "Intra Distortion 16x16\n");
    for (y = 0; y < feiOut->PaddedHeight / 16; y++) {
        fprintf(outfileIntraDist, "Row % 5d", y*16);
        for (x = 0; x < feiOut->PaddedWidth / 16; x++) {
            fprintf(outfileIntraDist, "% 6d", feiOut->IntraDist[y*feiOut->IntraPitch + x].Dist);
        }
        fprintf(outfileIntraDist, "\n");
    }
    fprintf(outfileIntraDist, "\n");

}

void WriteFrameInterLarge(mfxFEIH265Output *feiOut, int refIdx, int blockSize)
{
    unsigned int x, y, w, h, i;

    unsigned int pitchMv   = feiOut->PitchMV[blockSize];
    unsigned int pitchDist = feiOut->PitchDist[blockSize];

    switch (blockSize) {
    case MFX_FEI_H265_BLK_32x32:
        w = 32;
        h = 32;
        break;
    case MFX_FEI_H265_BLK_32x16:
        w = 32;
        h = 16;
        break;
    case MFX_FEI_H265_BLK_16x32:
        w = 16;
        h = 32;
        break;
    default:
        w = 0;
        h = 0;
        return;
    }

    /* for large blocks, 9 dist estimates are returned (spaced 16 units apart)
     * dist[0] through dist[8] correspond to the 9 possible MV's centered around x = MV.x and y = MV.y
     *
     *  (x-1,y-1) | (x+0,y-1) | (x+1,y-1)        dist[0] | dist[1] | dist[2]
     *  (x-1,y+0) | (x+0,y+0) | (x+1,y+0)  -->   dist[3] | dist[4] | dist[5]
     *  (x-1,y+1) | (x+0,y+1) | (x+1,y+1)        dist[6] | dist[7] | dist[8]
     *
     * dist[9] through dist[15] are undefined (padding)
     *
     * NOTE: this is only performed for 32x16 and 16x32 blocks when mfxFEIH265Param->MPMode is > 1
     */
    fprintf(outfileInterMV,   "MV - %dx%d blocks\n", w, h);
    fprintf(outfileInterDist, "Dist - %dx%d blocks\n", w, h);

    for (y = 0; y < feiOut->PaddedHeight / h; y++) {
        fprintf(outfileInterMV, "Row % 5d: ", y*h);
        fprintf(outfileInterDist, "Row % 5d: ", y*h);
        for (x = 0; x < feiOut->PaddedWidth / w; x++) {
            fprintf(outfileInterMV, "[% 4d, % 4d] ", feiOut->MV[refIdx][blockSize][y*pitchMv + x].x, feiOut->MV[refIdx][blockSize][y*pitchMv + x].y);
            for (i = 0; i < 9; i++)
                fprintf(outfileInterDist, "% 6d ", (int)feiOut->Dist[refIdx][blockSize][y*pitchDist + 16*x + i]);
            fprintf(outfileInterDist, "|| ");
        }
        fprintf(outfileInterMV, "\n");
        fprintf(outfileInterDist, "\n");
    }
    fprintf(outfileInterMV, "\n");
    fprintf(outfileInterDist, "\n");
}

void WriteFrameInterSmall(mfxFEIH265Output *feiOut, int refIdx, int blockSize)
{
    unsigned int x, y, w, h;

    int pitchMv   = feiOut->PitchMV[blockSize];
    int pitchDist = feiOut->PitchDist[blockSize];

    switch (blockSize) {
    case MFX_FEI_H265_BLK_16x16:
        w = 16;
        h = 16;
        break;
    case MFX_FEI_H265_BLK_8x8:
        w = 8;
        h = 8;
        break;
    case MFX_FEI_H265_BLK_8x16:
        w = 8;
        h = 16;
        break;
    case MFX_FEI_H265_BLK_16x8:
        w = 16;
        h = 8;
        break;
    default:
        w = 0;
        h = 0;
        return;
    }

    fprintf(outfileInterMV,   "MV - %dx%d blocks\n", w, h);
    fprintf(outfileInterDist, "Dist - %dx%d blocks\n", w, h);
    for (y = 0; y < feiOut->PaddedHeight / h; y++) {
        fprintf(outfileInterMV, "Row % 5d: ", y*h);
        fprintf(outfileInterDist, "Row % 5d: ", y*h);
        for (x = 0; x < feiOut->PaddedWidth / w; x++) {
            fprintf(outfileInterMV, "[% 4d, % 4d] ", feiOut->MV[refIdx][blockSize][y*pitchMv + x].x, feiOut->MV[refIdx][blockSize][y*pitchMv + x].y);
            fprintf(outfileInterDist, "% 6d", (int)feiOut->Dist[refIdx][blockSize][y*pitchDist + x]);
        }
        fprintf(outfileInterMV, "\n");
        fprintf(outfileInterDist, "\n");
    }
    fprintf(outfileInterMV, "\n");
    fprintf(outfileInterDist, "\n");
}

void WriteFrameInterpolate(mfxFEIH265Output *feiOut, int refIdx)
{
    unsigned int i, j;

    /* write Y plane to file, fill U/V planes with 0x80 (zero in 8-bit YUV420) */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < feiOut->InterpolateHeight; j++)
            fwrite(feiOut->Interp[refIdx][i] + j*feiOut->InterpolatePitch, 1, feiOut->InterpolateWidth, outfileInterpolate[i]);
        for (j = 0; j < feiOut->InterpolateHeight / 2; j++)
            fwrite(bwBuf, 1, feiOut->InterpolateWidth, outfileInterpolate[i]);
    }
}
