//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2009 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#ifdef MFX_ENABLE_H264_VIDEO_PAK

#include "ippdefs.h"
#include "ippvc.h"
#include "mfx_h264_pak_deblocking.h"
#include "mfx_h264_pak_utils.h"

using namespace H264Pak;

enum
{
    DEBLOCK_VERT = 0,
    DEBLOCK_HORZ = 1
};

struct DeblockingParams
{
    mfxU8 Strength[2][16];
    mfxU32 DeblockingFlag[2];
    mfxU32 ExternalEdgeFlag[2];
    mfxI32 nMBAddr;
    mfxI32 nMaxMVector;
    mfxI32 nNeighbour[2];
    mfxI32 MBFieldCoded;
    mfxI32 nAlphaC0Offset;
    mfxI32 nBetaOffset;
    mfxU8* pY;
    mfxU8* pU;
    mfxU8* pV;
    mfxU32 pitchPixels;
};

#define ClampVal(x)  (ClampTbl[256 + (x)])
#define clipd1(x, limit) min(limit, max(x,-limit))

template <typename T>
inline T IClip(T Min, T Max, T Val)
{
    return Val < Min ? Min : Val > Max ? Max : Val;
}

const mfxU8 ClampTbl[768] =
{
     0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x01 ,0x02 ,0x03 ,0x04 ,0x05 ,0x06 ,0x07
    ,0x08 ,0x09 ,0x0a ,0x0b ,0x0c ,0x0d ,0x0e ,0x0f
    ,0x10 ,0x11 ,0x12 ,0x13 ,0x14 ,0x15 ,0x16 ,0x17
    ,0x18 ,0x19 ,0x1a ,0x1b ,0x1c ,0x1d ,0x1e ,0x1f
    ,0x20 ,0x21 ,0x22 ,0x23 ,0x24 ,0x25 ,0x26 ,0x27
    ,0x28 ,0x29 ,0x2a ,0x2b ,0x2c ,0x2d ,0x2e ,0x2f
    ,0x30 ,0x31 ,0x32 ,0x33 ,0x34 ,0x35 ,0x36 ,0x37
    ,0x38 ,0x39 ,0x3a ,0x3b ,0x3c ,0x3d ,0x3e ,0x3f
    ,0x40 ,0x41 ,0x42 ,0x43 ,0x44 ,0x45 ,0x46 ,0x47
    ,0x48 ,0x49 ,0x4a ,0x4b ,0x4c ,0x4d ,0x4e ,0x4f
    ,0x50 ,0x51 ,0x52 ,0x53 ,0x54 ,0x55 ,0x56 ,0x57
    ,0x58 ,0x59 ,0x5a ,0x5b ,0x5c ,0x5d ,0x5e ,0x5f
    ,0x60 ,0x61 ,0x62 ,0x63 ,0x64 ,0x65 ,0x66 ,0x67
    ,0x68 ,0x69 ,0x6a ,0x6b ,0x6c ,0x6d ,0x6e ,0x6f
    ,0x70 ,0x71 ,0x72 ,0x73 ,0x74 ,0x75 ,0x76 ,0x77
    ,0x78 ,0x79 ,0x7a ,0x7b ,0x7c ,0x7d ,0x7e ,0x7f
    ,0x80 ,0x81 ,0x82 ,0x83 ,0x84 ,0x85 ,0x86 ,0x87
    ,0x88 ,0x89 ,0x8a ,0x8b ,0x8c ,0x8d ,0x8e ,0x8f
    ,0x90 ,0x91 ,0x92 ,0x93 ,0x94 ,0x95 ,0x96 ,0x97
    ,0x98 ,0x99 ,0x9a ,0x9b ,0x9c ,0x9d ,0x9e ,0x9f
    ,0xa0 ,0xa1 ,0xa2 ,0xa3 ,0xa4 ,0xa5 ,0xa6 ,0xa7
    ,0xa8 ,0xa9 ,0xaa ,0xab ,0xac ,0xad ,0xae ,0xaf
    ,0xb0 ,0xb1 ,0xb2 ,0xb3 ,0xb4 ,0xb5 ,0xb6 ,0xb7
    ,0xb8 ,0xb9 ,0xba ,0xbb ,0xbc ,0xbd ,0xbe ,0xbf
    ,0xc0 ,0xc1 ,0xc2 ,0xc3 ,0xc4 ,0xc5 ,0xc6 ,0xc7
    ,0xc8 ,0xc9 ,0xca ,0xcb ,0xcc ,0xcd ,0xce ,0xcf
    ,0xd0 ,0xd1 ,0xd2 ,0xd3 ,0xd4 ,0xd5 ,0xd6 ,0xd7
    ,0xd8 ,0xd9 ,0xda ,0xdb ,0xdc ,0xdd ,0xde ,0xdf
    ,0xe0 ,0xe1 ,0xe2 ,0xe3 ,0xe4 ,0xe5 ,0xe6 ,0xe7
    ,0xe8 ,0xe9 ,0xea ,0xeb ,0xec ,0xed ,0xee ,0xef
    ,0xf0 ,0xf1 ,0xf2 ,0xf3 ,0xf4 ,0xf5 ,0xf6 ,0xf7
    ,0xf8 ,0xf9 ,0xfa ,0xfb ,0xfc ,0xfd ,0xfe ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
};

static const mfxU8 g_AlphaTable[52] =
{
    0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,
    4,   4,   5,   6,   7,   8,   9,   10,
    12,  13,  15,  17,  20,  22,  25,  28,
    32,  36,  40,  45,  50,  56,  63,  71,
    80,  90,  101, 113, 127, 144, 162, 182,
    203, 226, 255, 255
};

static const mfxU8 g_BetaTable[52] =
{
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,
    2,  2,  2,  3,  3,  3,  3,  4,
    4,  4,  6,  6,  7,  7,  8,  8,
    9,  9,  10, 10, 11, 11, 12, 12,
    13, 13, 14, 14, 15, 15, 16, 16,
    17, 17, 18, 18
};

static const mfxU8 g_ClipTable[52][5] =
{
    { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0},
    { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0},
    { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0},
    { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0},
    { 0, 0, 0, 0, 0}, { 0, 0, 0, 1, 1}, { 0, 0, 0, 1, 1}, { 0, 0, 0, 1, 1},
    { 0, 0, 0, 1, 1}, { 0, 0, 1, 1, 1}, { 0, 0, 1, 1, 1}, { 0, 1, 1, 1, 1},
    { 0, 1, 1, 1, 1}, { 0, 1, 1, 1, 1}, { 0, 1, 1, 1, 1}, { 0, 1, 1, 2, 2},
    { 0, 1, 1, 2, 2}, { 0, 1, 1, 2, 2}, { 0, 1, 1, 2, 2}, { 0, 1, 2, 3, 3},
    { 0, 1, 2, 3, 3}, { 0, 2, 2, 3, 3}, { 0, 2, 2, 4, 4}, { 0, 2, 3, 4, 4},
    { 0, 2, 3, 4, 4}, { 0, 3, 3, 5, 5}, { 0, 3, 4, 6, 6}, { 0, 3, 4, 6, 6},
    { 0, 4, 5, 7, 7}, { 0, 4, 5, 8, 8}, { 0, 4, 6, 9, 9}, { 0, 5, 7,10,10},
    { 0, 6, 8,11,11}, { 0, 6, 8,13,13}, { 0, 7,10,14,14}, { 0, 8,11,16,16},
    { 0, 9,12,18,18}, { 0,10,13,20,20}, { 0,11,15,23,23}, { 0,13,17,25,25}
};

static void ResetDeblockingVariables(
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const mfxFrameSurface& fs,
    const MbDescriptor& mbInfo,
    DeblockingParams& params)
{
    mfxU32 bottomMbFlag = fp.FrameType & MFX_FRAMETYPE_BFF ? 1 : 0;
    mfxFrameData& recFrame = *fs.Data[fp.RecFrameLabel];
    mfxU32 recPitch = recFrame.Pitch << fp.FieldPicFlag;
    mfxU32 curMbCnt = mbInfo.mbN.MbYcnt * (fp.FrameWinMbMinus1 + 1) + mbInfo.mbN.MbXcnt;

    params.ExternalEdgeFlag[DEBLOCK_VERT] = mbInfo.mbA != 0;
    params.ExternalEdgeFlag[DEBLOCK_HORZ] = mbInfo.mbB != 0;

    params.Strength[DEBLOCK_VERT][0] = 0;
    params.Strength[DEBLOCK_VERT][1] = 0;
    params.Strength[DEBLOCK_VERT][2] = 0;
    params.Strength[DEBLOCK_VERT][3] = 0;
    params.Strength[DEBLOCK_HORZ][0] = 0;
    params.Strength[DEBLOCK_HORZ][1] = 0;
    params.Strength[DEBLOCK_HORZ][2] = 0;
    params.Strength[DEBLOCK_HORZ][3] = 0;

    params.nMBAddr = curMbCnt + bottomMbFlag * (fp.FrameWinMbMinus1 + 1) * (fp.FrameHinMbMinus1 + 1);
    params.nNeighbour[DEBLOCK_VERT] = params.nMBAddr - 1;
    params.nNeighbour[DEBLOCK_HORZ] = params.nMBAddr - fp.FrameWinMbMinus1 - 1;

    params.DeblockingFlag[DEBLOCK_VERT] = 0;
    params.DeblockingFlag[DEBLOCK_HORZ] = 0;

    params.pY = recFrame.Y +
        16 * (mbInfo.mbN.MbYcnt * recPitch + mbInfo.mbN.MbXcnt) +
        bottomMbFlag * recFrame.Pitch;

    params.pU = recFrame.UV +
        8 * mbInfo.mbN.MbYcnt * recPitch + 16 * mbInfo.mbN.MbXcnt +
        bottomMbFlag * recFrame.Pitch;

    params.pV = params.pU + 1;

    params.pitchPixels = recPitch;
    params.nMaxMVector = fp.FieldPicFlag ? 2 : 4;
    params.MBFieldCoded = fp.FieldPicFlag;

    params.nAlphaC0Offset = sp.DeblockAlphaC0OffsetDiv2;
    params.nBetaOffset = sp.DeblockBetaOffsetDiv2;
}

static void PrepareDeblockingParametersI(
    DeblockingParams& params)
{
    // set deblocking flag(s)
    params.DeblockingFlag[DEBLOCK_VERT] = 1;
    params.DeblockingFlag[DEBLOCK_HORZ] = 1;

    if (params.ExternalEdgeFlag[DEBLOCK_VERT])
    {
        memset(params.Strength[DEBLOCK_VERT] + 0, 4, 4);
    }

    memset(params.Strength[DEBLOCK_VERT] + 4, 3, 12);

    if (params.ExternalEdgeFlag[DEBLOCK_HORZ])
    {
        memset(params.Strength[DEBLOCK_HORZ] + 0, 4 - params.MBFieldCoded, 4);
    }

    memset(params.Strength[DEBLOCK_HORZ] + 4, 3, 12);
}

static const mfxU8 MapBlock4x4ToStrengthIdx[2][16] =
{
    { 0, 4, 1, 5, 8, 12, 9, 13, 2, 6, 3, 7, 10, 14, 11, 15 }, // vertical
    { 0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15 }, // horizontal
};

static void PrepareDeblockingParametersP(
    const mfxExtAvcRefPicInfo& refInfo,
    const MbDescriptor& mbInfo,
    DeblockingParams& params)
{
    mfxExtAvcRefSliceInfo& refInfoN = refInfo.SliceInfo[mbInfo.sliceIdN];
    mfxExtAvcRefSliceInfo& refInfoA = refInfo.SliceInfo[mbInfo.sliceIdA];
    mfxExtAvcRefSliceInfo& refInfoB = refInfo.SliceInfo[mbInfo.sliceIdA];

    Ipp8u* strengthA = params.Strength[0];
    Ipp8u* strengthB = params.Strength[1];
    Ipp32u& deblockingFlagA = params.DeblockingFlag[0];
    Ipp32u& deblockingFlagB = params.DeblockingFlag[1];

    for (mfxU32 block = 0; block < 16; block++)
    {
        Block4x4 blkN = mbInfo.GetCurrent(block);
        Block4x4 blkA = mbInfo.GetLeft(block);
        Block4x4 blkB = mbInfo.GetAbove(block);

        if (blkA.mb && blkA.mb->IntraMbFlag == 0)
        {
            if (blkN.mb->CodedPattern4x4Y & (1 << blkN.block) ||
                blkA.mb->CodedPattern4x4Y & (1 << blkA.block))
            {
                strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 2;
                deblockingFlagA = 1;
            }
            else
            {
                mfxU8 refIdxN = blkN.mb->RefPicSelect[0][blkN.block / 4];
                mfxU8 refPicN = (refIdxN != 0xff) ? refInfoN.RefPicList[0][refIdxN] : 0xff;

                mfxU8 refIdxA = blkA.mb->RefPicSelect[0][blkA.block / 4];
                mfxU8 refPicA = (refIdxA != 0xff) ? refInfoA.RefPicList[0][refIdxA] : 0xff;

                assert(refPicN != 0xff || refPicA != 0xff);
                if (refPicN == refPicA)
                {
                    mfxI16Pair vecN = blkN.mv->mv[0][g_Geometry[blkN.block].offBlk4x4];
                    mfxI16Pair vecA = blkA.mv->mv[0][g_Geometry[blkA.block].offBlk4x4];

                    if (abs(vecN.x - vecA.x) >= 4 ||
                        abs(vecN.y - vecA.y) >= params.nMaxMVector)
                    {
                        strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 1;
                        deblockingFlagA = 1;
                    }
                    else
                    {
                        strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 0;
                    }
                }
                else
                {
                    strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 1;
                    deblockingFlagA = 1;
                }
            }
        }
        else if (blkA.mb != 0)
        {
            strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 4;
            deblockingFlagA = 1;
        }
        else
        {
            strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 0;
        }

        if (blkB.mb && blkB.mb->IntraMbFlag == 0)
        {
            if (blkN.mb->CodedPattern4x4Y & (1 << blkN.block) ||
                blkB.mb->CodedPattern4x4Y & (1 << blkB.block))
            {
                strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 2;
                deblockingFlagB = 1;
            }
            else
            {
                mfxU8 refIdxN = blkN.mb->RefPicSelect[0][blkN.block / 4];
                mfxU8 refPicN = (refIdxN != 0xff) ? refInfoN.RefPicList[0][refIdxN] : 0xff;

                mfxU8 refIdxB = blkB.mb->RefPicSelect[0][blkB.block / 4];
                mfxU8 refPicB = (refIdxB != 0xff) ? refInfoB.RefPicList[0][refIdxB] : 0xff;

                assert(refPicN != 0xff || refPicB != 0xff);
                if (refPicN == refPicB)
                {
                    mfxI16Pair vecN = blkN.mv->mv[0][g_Geometry[blkN.block].offBlk4x4];
                    mfxI16Pair vecB = blkB.mv->mv[0][g_Geometry[blkB.block].offBlk4x4];

                    if (abs(vecN.x - vecB.x) >= 4 ||
                        abs(vecN.y - vecB.y) >= params.nMaxMVector)
                    {
                        strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 1;
                        deblockingFlagB = 1;
                    }
                    else
                    {
                        strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 0;
                    }
                }
                else
                {
                    strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 1;
                    deblockingFlagB = 1;
                }
            }
        }
        else if (blkB.mb != 0)
        {
            strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 4 - blkN.mb->FieldMbFlag;
            deblockingFlagB = 1;
        }
        else
        {
            strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 0;
        }
    }
}

static void PrepareDeblockingParametersB(
    const mfxExtAvcRefPicInfo& refInfo,
    const MbDescriptor& mbInfo,
    DeblockingParams& params)
{
    mfxExtAvcRefSliceInfo& refInfoN = refInfo.SliceInfo[mbInfo.sliceIdN];
    mfxExtAvcRefSliceInfo& refInfoA = refInfo.SliceInfo[mbInfo.sliceIdA];
    mfxExtAvcRefSliceInfo& refInfoB = refInfo.SliceInfo[mbInfo.sliceIdA];

    Ipp8u* strengthA = params.Strength[0];
    Ipp8u* strengthB = params.Strength[1];
    Ipp32u& deblockingFlagA = params.DeblockingFlag[0];
    Ipp32u& deblockingFlagB = params.DeblockingFlag[1];

    for (mfxU32 block = 0; block < 16; block++)
    {
        Block4x4 blkN = mbInfo.GetCurrent(block);
        Block4x4 blkA = mbInfo.GetLeft(block);
        Block4x4 blkB = mbInfo.GetAbove(block);

        if (blkA.mb && blkA.mb->IntraMbFlag == 0)
        {
            if (blkN.mb->CodedPattern4x4Y & (1 << blkN.block) ||
                blkA.mb->CodedPattern4x4Y & (1 << blkA.block))
            {
                strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 2;
                deblockingFlagA = 1;
            }
            else
            {
                mfxU8 refIdx0N = blkN.mb->RefPicSelect[0][blkN.block / 4];
                mfxU8 refPic0N = (refIdx0N != 0xff) ? refInfoN.RefPicList[0][refIdx0N] : 0xff;

                mfxU8 refIdx1N = blkN.mb->RefPicSelect[1][blkN.block / 4];
                mfxU8 refPic1N = (refIdx1N != 0xff) ? refInfoN.RefPicList[1][refIdx1N] : 0xff;

                mfxU8 refIdx0A = blkA.mb->RefPicSelect[0][blkA.block / 4];
                mfxU8 refPic0A = (refIdx0A != 0xff) ? refInfoA.RefPicList[0][refIdx0A] : 0xff;

                mfxU8 refIdx1A = blkA.mb->RefPicSelect[1][blkA.block / 4];
                mfxU8 refPic1A = (refIdx1A != 0xff) ? refInfoA.RefPicList[1][refIdx1A] : 0xff;

                if ((refPic0N == refPic0A && refPic1N == refPic1A) ||
                    (refPic0N == refPic1A && refPic1N == refPic0A))
                {
                    strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 0;

                    mfxI16Pair vec0N = refPic0N != 0xff
                        ? blkN.mv->mv[0][g_Geometry[blkN.block].offBlk4x4]
                        : nullMv;

                    mfxI16Pair vec1N = refPic1N != 0xff
                        ? blkN.mv->mv[1][g_Geometry[blkN.block].offBlk4x4]
                        : nullMv;

                    mfxI16Pair vec0A = refPic0A != 0xff
                        ? blkA.mv->mv[0][g_Geometry[blkA.block].offBlk4x4]
                        : nullMv;

                    mfxI16Pair vec1A = refPic1A != 0xff
                        ? blkA.mv->mv[1][g_Geometry[blkA.block].offBlk4x4]
                        : nullMv;

                    if (refPic0N != refPic1N)
                    {
                        if (refPic0N != refPic0A)
                        {
                            mfxI16Pair tmp = vec0A;
                            vec0A = vec1A;
                            vec1A = tmp;
                        }

                        if (abs(vec0N.x - vec0A.x) >= 4 ||
                            abs(vec1N.x - vec1A.x) >= 4 ||
                            abs(vec0N.y - vec0A.y) >= params.nMaxMVector ||
                            abs(vec1N.y - vec1A.y) >= params.nMaxMVector)
                        {
                            strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 1;
                            deblockingFlagA = 1;
                        }
                    }
                    else
                    {
                        if (abs(vec0N.x - vec0A.x) >= 4 ||
                            abs(vec1N.x - vec1A.x) >= 4 ||
                            abs(vec0N.y - vec0A.y) >= params.nMaxMVector ||
                            abs(vec1N.y - vec1A.y) >= params.nMaxMVector)
                        {
                            if (abs(vec0N.x - vec1A.x) >= 4 ||
                                abs(vec1N.x - vec0A.x) >= 4 ||
                                abs(vec0N.y - vec1A.y) >= params.nMaxMVector ||
                                abs(vec1N.y - vec0A.y) >= params.nMaxMVector)
                            {
                                strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 1;
                                deblockingFlagA = 1;
                            }
                        }
                    }
                }
                else
                {
                    strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 1;
                    deblockingFlagA = 1;
                }
            }
        }
        else if (blkA.mb != 0)
        {
            strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 4;
            deblockingFlagA = 1;
        }
        else
        {
            strengthA[MapBlock4x4ToStrengthIdx[0][block]] = 0;
        }

        if (blkB.mb && blkB.mb->IntraMbFlag == 0)
        {
            if (blkN.mb->CodedPattern4x4Y & (1 << blkN.block) ||
                blkB.mb->CodedPattern4x4Y & (1 << blkB.block))
            {
                strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 2;
                deblockingFlagB = 1;
            }
            else
            {
                mfxU8 refIdx0N = blkN.mb->RefPicSelect[0][blkN.block / 4];
                mfxU8 refPic0N = (refIdx0N != 0xff) ? refInfoN.RefPicList[0][refIdx0N] : 0xff;

                mfxU8 refIdx1N = blkN.mb->RefPicSelect[1][blkN.block / 4];
                mfxU8 refPic1N = (refIdx1N != 0xff) ? refInfoN.RefPicList[1][refIdx1N] : 0xff;

                mfxU8 refIdx0B = blkB.mb->RefPicSelect[0][blkB.block / 4];
                mfxU8 refPic0B = (refIdx0B != 0xff) ? refInfoB.RefPicList[0][refIdx0B] : 0xff;

                mfxU8 refIdx1B = blkB.mb->RefPicSelect[1][blkB.block / 4];
                mfxU8 refPic1B = (refIdx1B != 0xff) ? refInfoB.RefPicList[1][refIdx1B] : 0xff;

                if ((refPic0N == refPic0B && refPic1N == refPic1B) ||
                    (refPic0N == refPic1B && refPic1N == refPic0B))
                {
                    strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 0;

                    mfxI16Pair vec0N = refPic0N != 0xff
                        ? blkN.mv->mv[0][g_Geometry[blkN.block].offBlk4x4]
                        : nullMv;

                    mfxI16Pair vec1N = refPic1N != 0xff
                        ? blkN.mv->mv[1][g_Geometry[blkN.block].offBlk4x4]
                        : nullMv;

                    mfxI16Pair vec0B = refPic0B != 0xff
                        ? blkB.mv->mv[0][g_Geometry[blkB.block].offBlk4x4]
                        : nullMv;

                    mfxI16Pair vec1B = refPic1B != 0xff
                        ? blkB.mv->mv[1][g_Geometry[blkB.block].offBlk4x4]
                        : nullMv;

                    if (refPic0N != refPic1N)
                    {
                        if (refPic0N != refPic0B)
                        {
                            mfxI16Pair tmp = vec0B;
                            vec0B = vec1B;
                            vec1B = tmp;
                        }

                        if (abs(vec0N.x - vec0B.x) >= 4 ||
                            abs(vec1N.x - vec1B.x) >= 4 ||
                            abs(vec0N.y - vec0B.y) >= params.nMaxMVector ||
                            abs(vec1N.y - vec1B.y) >= params.nMaxMVector)
                        {
                            strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 1;
                            deblockingFlagB = 1;
                        }
                    }
                    else
                    {
                        if (abs(vec0N.x - vec0B.x) >= 4 ||
                            abs(vec1N.x - vec1B.x) >= 4 ||
                            abs(vec0N.y - vec0B.y) >= params.nMaxMVector ||
                            abs(vec1N.y - vec1B.y) >= params.nMaxMVector)
                        {
                            if (abs(vec0N.x - vec1B.x) >= 4 ||
                                abs(vec1N.x - vec0B.x) >= 4 ||
                                abs(vec0N.y - vec1B.y) >= params.nMaxMVector ||
                                abs(vec1N.y - vec0B.y) >= params.nMaxMVector)
                            {
                                strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 1;
                                deblockingFlagB = 1;
                            }
                        }
                    }
                }
                else
                {
                    strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 1;
                    deblockingFlagB = 1;
                }
            }
        }
        else if (blkB.mb != 0)
        {
            strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 4 - blkN.mb->FieldMbFlag;
            deblockingFlagB = 1;
        }
        else
        {
            strengthB[MapBlock4x4ToStrengthIdx[1][block]] = 0;
        }
    }
}

static void DeblockChroma(
    const mfxFrameParamAVC& fp,
    const MbDescriptor& mbInfo,
    const DeblockingParams& params,
    Ipp32u dir)
{
    mfxU8 *pU = params.pU;
    Ipp32s pic_pitchPixels = params.pitchPixels;
    Ipp8u Clipping[16];
    Ipp8u Alpha[2];
    Ipp8u Beta[2];
    Ipp32s AlphaC0Offset = params.nAlphaC0Offset;
    Ipp32s BetaOffset = params.nBetaOffset;
    Ipp32s pmq_QP = mbInfo.mbN.QpPrimeY;

    if (params.DeblockingFlag[dir])
    {
        const mfxU8 *pClipTab;
        mfxI32 QP;
        mfxI32 index;
        const mfxU8 *pStrength = params.Strength[dir];

        mfxI32 chroma_qp_offset = fp.ChromaQp1stOffset;

        if (params.ExternalEdgeFlag[dir])
        {
            QP = g_MapLumaQpToChromaQp[IClip(0, 51, pmq_QP + chroma_qp_offset)];

            index = IClip(0, 51, QP + BetaOffset);
            Beta[0] = g_BetaTable[index];

            index = IClip(0, 51, QP + AlphaC0Offset);
            Alpha[0] = g_AlphaTable[index];
            pClipTab = g_ClipTable[index];

            // create clipping values
            Clipping[0] = pClipTab[pStrength[0]];
            Clipping[1] = pClipTab[pStrength[1]];
            Clipping[2] = pClipTab[pStrength[2]];
            Clipping[3] = pClipTab[pStrength[3]];
        }
        else
        {
            Alpha[0] = 0;
        }

        QP = g_MapLumaQpToChromaQp[IClip(0, 51, pmq_QP + chroma_qp_offset)];

        index = IClip(0, 51, QP + BetaOffset);
        Beta[1] = g_BetaTable[index];

        index = IClip(0, 51, QP + AlphaC0Offset);
        Alpha[1] = g_AlphaTable[index];
        pClipTab = g_ClipTable[index];

        if (fp.ChromaFormatIdc == 2 && DEBLOCK_HORZ == dir)
        {
            Clipping[4] = (Ipp8u) (pClipTab[pStrength[4]]);
            Clipping[5] = (Ipp8u) (pClipTab[pStrength[5]]);
            Clipping[6] = (Ipp8u) (pClipTab[pStrength[6]]);
            Clipping[7] = (Ipp8u) (pClipTab[pStrength[7]]);
            Clipping[8] = (Ipp8u) (pClipTab[pStrength[8]]);
            Clipping[9] = (Ipp8u) (pClipTab[pStrength[9]]);
            Clipping[10] = (Ipp8u) (pClipTab[pStrength[10]]);
            Clipping[11] = (Ipp8u) (pClipTab[pStrength[11]]);
            Clipping[12] = (Ipp8u) (pClipTab[pStrength[12]]);
            Clipping[13] = (Ipp8u) (pClipTab[pStrength[13]]);
            Clipping[14] = (Ipp8u) (pClipTab[pStrength[14]]);
            Clipping[15] = (Ipp8u) (pClipTab[pStrength[15]]);
        }
        else
        {
            Clipping[4] = (Ipp8u) (pClipTab[pStrength[8]]);
            Clipping[5] = (Ipp8u) (pClipTab[pStrength[9]]);
            Clipping[6] = (Ipp8u) (pClipTab[pStrength[10]]);
            Clipping[7] = (Ipp8u) (pClipTab[pStrength[11]]);
        }

        if (dir == DEBLOCK_VERT)
        {
            ippiFilterDeblockingChroma_VerEdge_H264_8u_C2IR(
                pU,
                pic_pitchPixels,
                Alpha,
                Beta,
                Clipping,
                pStrength);
        }
        else
        {
            ippiFilterDeblockingChroma_HorEdge_H264_8u_C2IR(
                pU,
                pic_pitchPixels,
                Alpha,
                Beta,
                Clipping,
                pStrength);
        }
    }
}

static void DeblockLuma(
    const MbDescriptor& mbInfo,
    DeblockingParams& params,
    Ipp32u dir)
{
    mfxU8 *pY = params.pY;
    mfxI32 pic_pitchPixels = params.pitchPixels;
    mfxU8 Clipping[16];
    mfxU8 Alpha[2];
    mfxU8 Beta[2];
    mfxI32 AlphaC0Offset = params.nAlphaC0Offset;
    mfxI32 BetaOffset = params.nBetaOffset;
    mfxI32 pmq_QP = mbInfo.mbN.QpPrimeY;

    if (params.DeblockingFlag[dir])
    {
        const mfxU8* pClipTab;
        mfxI32 QP;
        mfxI32 index;
        mfxU8 *pStrength = params.Strength[dir];

        if (mbInfo.mbN.TransformFlag)
        {
            memset(pStrength + 4, 0, 4);
            memset(pStrength + 12, 0, 4);
        }

        if (params.ExternalEdgeFlag[dir])
        {
            mfxI32 pmp_QP = (dir == DEBLOCK_VERT) ? mbInfo.mbA->QpPrimeY : mbInfo.mbB->QpPrimeY;

            // luma variables
            QP = (pmp_QP + pmq_QP + 1) >> 1 ;

            // external edge variables
            index = IClip(0, 51, QP + BetaOffset);
            Beta[0] = g_BetaTable[index];

            index = IClip(0, 51, QP + AlphaC0Offset);
            Alpha[0] = g_AlphaTable[index];
            pClipTab = g_ClipTable[index];

            // create clipping values
            Clipping[0] = pClipTab[pStrength[0]];
            Clipping[1] = pClipTab[pStrength[1]];
            Clipping[2] = pClipTab[pStrength[2]];
            Clipping[3] = pClipTab[pStrength[3]];

        }
        else
            Alpha[0] = 0;

        // internal edge variables
        QP = pmq_QP;

        index = IClip(0, 51, QP + BetaOffset);
        Beta[1] = g_BetaTable[index];

        index = IClip(0, 51, QP + AlphaC0Offset);
        Alpha[1] = g_AlphaTable[index];
        pClipTab = g_ClipTable[index];

        // create clipping values
        {
            Ipp32u edge;

            for (edge = 1;edge < 4;edge += 1)
            {
                if (*((Ipp32u *) (pStrength + edge * 4)))
                {
                    // create clipping values
                    Clipping[edge * 4 + 0] = pClipTab[pStrength[edge * 4 + 0]];
                    Clipping[edge * 4 + 1] = pClipTab[pStrength[edge * 4 + 1]];
                    Clipping[edge * 4 + 2] = pClipTab[pStrength[edge * 4 + 2]];
                    Clipping[edge * 4 + 3] = pClipTab[pStrength[edge * 4 + 3]];
                }
            }
        }

        // perform deblocking
        if (dir == DEBLOCK_VERT)
        {
            ippiFilterDeblockingLuma_VerEdge_H264_8u_C1IR(
                pY,
                pic_pitchPixels,
                Alpha,
                Beta,
                Clipping,
                pStrength);
        }
        else
        {
            ippiFilterDeblockingLuma_HorEdge_H264_8u_C1IR(
                pY,
                pic_pitchPixels,
                Alpha,
                Beta,
                Clipping,
                pStrength);
        }
    }
}

void H264Pak::DeblockMb(
    const mfxFrameParamAVC& fp,
    const mfxSliceParamAVC& sp,
    const mfxExtAvcRefPicInfo& refInfo,
    const MbDescriptor& mbInfo,
    const mfxFrameSurface& fs)
{
    H264PAK_ALIGN16 DeblockingParams params;

    // prepare deblocking parameters
    ResetDeblockingVariables(fp, sp, fs, mbInfo, params);

    if (mbInfo.mbN.IntraMbFlag == 1)
    {
        PrepareDeblockingParametersI(params);
    }
    else if (sp.SliceType & MFX_SLICETYPE_P)
    {
        PrepareDeblockingParametersP(refInfo, mbInfo, params);
    }
    else // MFX_SLICETYPE_B
    {
        PrepareDeblockingParametersB(refInfo, mbInfo, params);
    }

    if (fp.ChromaFormatIdc)
    {
        DeblockChroma(fp, mbInfo, params, DEBLOCK_VERT);
        DeblockChroma(fp, mbInfo, params, DEBLOCK_HORZ);
    }

    DeblockLuma(mbInfo, params, DEBLOCK_VERT);
    DeblockLuma(mbInfo, params, DEBLOCK_HORZ);
}

#endif // MFX_ENABLE_H264_VIDEO_PAK
