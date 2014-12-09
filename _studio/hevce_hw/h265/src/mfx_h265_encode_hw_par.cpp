//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_h265_encode_hw_utils.h"
#include "mfx_h265_encode_hw_ddi.h"
#include <assert.h>

namespace MfxHwH265Encode
{

const mfxU32 TableA1[][6] = 
{
//  Level   |MaxLumaPs |    MaxCPB    | MaxSlice  | MaxTileRows | MaxTileCols
//                                      Segments  |
//                                      PerPicture|
/*  1   */ {   36864,      350,    350,        16,            1,           1},
/*  2   */ {  122880,     1500,   1500,        16,            1,           1},
/*  2.1 */ {  245760,     3000,   3000,        20,            1,           1},
/*  3   */ {  552960,     6000,   6000,        30,            2,           2},
/*  3.1 */ {  983040,    10000,  10000,        40,            3,           3},
/*  4   */ { 2228224,    12000,  30000,        75,            5,           5},
/*  4.1 */ { 2228224,    20000,  50000,        75,            5,           5},
/*  5   */ { 8912896,    25000, 100000,       200,           11,          10},
/*  5.1 */ { 8912896,    40000, 160000,       200,           11,          10},
/*  5.2 */ { 8912896,    60000, 240000,       200,           11,          10},
/*  6   */ {35651584,    60000, 240000,       600,           22,          20},
/*  6.1 */ {35651584,   120000, 480000,       600,           22,          20},
/*  6.2 */ {35651584,   240000, 800000,       600,           22,          20},
};

const mfxU32 TableA2[][4] = 
{
//  Level   | MaxLumaSr |      MaxBR   | MinCr
/*  1    */ {    552960,    128,    128,    2},
/*  2    */ {   3686400,   1500,   1500,    2},
/*  2.1  */ {   7372800,   3000,   3000,    2},
/*  3    */ {  16588800,   6000,   6000,    2},
/*  3.1  */ {  33177600,  10000,  10000,    2},
/*  4    */ {  66846720,  12000,  30000,    4},
/*  4.1  */ { 133693440,  20000,  50000,    4},
/*  5    */ { 267386880,  25000, 100000,    6},
/*  5.1  */ { 534773760,  40000, 160000,    8},
/*  5.2  */ {1069547520,  60000, 240000,    8},
/*  6    */ {1069547520,  60000, 240000,    8},
/*  6.1  */ {2139095040, 120000, 480000,    8},
/*  6.2  */ {4278190080, 240000, 800000,    6},
};

const mfxU16 MaxLidx = (sizeof(TableA1) / sizeof(TableA1[0]));

const mfxU16 LevelIdxToMfx[] = 
{
    MFX_LEVEL_HEVC_1 ,
    MFX_LEVEL_HEVC_2 ,
    MFX_LEVEL_HEVC_21,
    MFX_LEVEL_HEVC_3 ,
    MFX_LEVEL_HEVC_31,
    MFX_LEVEL_HEVC_4 ,
    MFX_LEVEL_HEVC_41,
    MFX_LEVEL_HEVC_5 ,
    MFX_LEVEL_HEVC_51,
    MFX_LEVEL_HEVC_52,
    MFX_LEVEL_HEVC_6 ,
    MFX_LEVEL_HEVC_61,
    MFX_LEVEL_HEVC_62,
};

inline mfxU16 MaxTidx(mfxU16 l) { return (LevelIdxToMfx[l] >= MFX_LEVEL_HEVC_4); }

mfxU16 LevelIdx(mfxU16 mfx_level)
{
    switch (mfx_level & 0xFF)
    {
    case  MFX_LEVEL_HEVC_1  : return  0;
    case  MFX_LEVEL_HEVC_2  : return  1;
    case  MFX_LEVEL_HEVC_21 : return  2;
    case  MFX_LEVEL_HEVC_3  : return  3;
    case  MFX_LEVEL_HEVC_31 : return  4;
    case  MFX_LEVEL_HEVC_4  : return  5;
    case  MFX_LEVEL_HEVC_41 : return  6;
    case  MFX_LEVEL_HEVC_5  : return  7;
    case  MFX_LEVEL_HEVC_51 : return  8;
    case  MFX_LEVEL_HEVC_52 : return  9;
    case  MFX_LEVEL_HEVC_6  : return 10;
    case  MFX_LEVEL_HEVC_61 : return 11;
    case  MFX_LEVEL_HEVC_62 : return 12;
    default: break;
    }
    assert("unknown level");
    return 0;
}

mfxU16 TierIdx(mfxU16 mfx_level)
{
    return !!(mfx_level & MFX_TIER_HEVC_HIGH);
}

mfxU16 MfxLevel(mfxU16 l, mfxU16 t){ return LevelIdxToMfx[l] | (MFX_TIER_HEVC_HIGH * !!t); }

mfxU32 GetMaxDpbSize(mfxU32 PicSizeInSamplesY, mfxU32 MaxLumaPs, mfxU32 maxDpbPicBuf)
{
    if (PicSizeInSamplesY <= (MaxLumaPs >> 2))
        return Min(4 * maxDpbPicBuf, 16U);
    else if (PicSizeInSamplesY <= (MaxLumaPs >> 1))
        return Min( 2 * maxDpbPicBuf, 16U);
    else if (PicSizeInSamplesY <= ((3 * MaxLumaPs) >> 2))
        return Min( (4 * maxDpbPicBuf ) / 3, 16U);

    return maxDpbPicBuf;
}

mfxStatus CheckProfile(mfxVideoParam& par)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    switch (par.mfx.CodecProfile)
    {
    case 0:
        break;

    case MFX_PROFILE_HEVC_MAIN10:
        break;

    case MFX_PROFILE_HEVC_MAINSP:
        if (par.mfx.GopPicSize > 1)
        {
            par.mfx.GopPicSize = 1;
            sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

    case MFX_PROFILE_HEVC_MAIN:
        if (par.mfx.FrameInfo.BitDepthLuma > 8)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        break;

    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return sts;
}

mfxU32 GetMaxDpbSizeByLevel(MfxVideoParam const & par)
{
    assert(par.mfx.CodecLevel != 0);
    
    mfxU32 MaxLumaPs         = TableA1[LevelIdx(par.mfx.CodecLevel)][0]; 
    mfxU32 PicSizeInSamplesY = (mfxU32)par.m_ext.HEVCParam.PicWidthInLumaSamples * par.m_ext.HEVCParam.PicHeightInLumaSamples;

    return GetMaxDpbSize(PicSizeInSamplesY, MaxLumaPs, 6);
}

mfxU32 GetMaxKbpsByLevel(MfxVideoParam const & par)
{
    assert(par.mfx.CodecLevel != 0);

    return TableA2[LevelIdx(par.mfx.CodecLevel)][1+TierIdx(par.mfx.CodecLevel)] * 1100 / 1000;
}

mfxF64 GetMaxFrByLevel(MfxVideoParam const & par)
{
    assert(par.mfx.CodecLevel != 0);

    mfxU32 MaxLumaSr   = TableA2[LevelIdx(par.mfx.CodecLevel)][0];
    mfxU32 PicSizeInSamplesY = (mfxU32)par.m_ext.HEVCParam.PicWidthInLumaSamples * par.m_ext.HEVCParam.PicHeightInLumaSamples;

    return (mfxF64)MaxLumaSr / PicSizeInSamplesY;
}

mfxU32 GetMaxCpbInKBByLevel(MfxVideoParam const & par)
{
    assert(par.mfx.CodecLevel != 0);

    return TableA1[LevelIdx(par.mfx.CodecLevel)][1+TierIdx(par.mfx.CodecLevel)] * 1100 / 8000;
}

mfxStatus CorrectLevel(MfxVideoParam& par, bool query)
{
    mfxStatus sts = MFX_ERR_NONE;
    //mfxU32 CpbBrVclFactor    = 1000;
    mfxU32 CpbBrNalFactor    = 1100;
    mfxU32 PicSizeInSamplesY = (mfxU32)par.m_ext.HEVCParam.PicWidthInLumaSamples * par.m_ext.HEVCParam.PicHeightInLumaSamples;
    mfxU32 LumaSr            = Ceil(mfxF64(PicSizeInSamplesY) / par.mfx.FrameInfo.FrameRateExtN * par.mfx.FrameInfo.FrameRateExtD);
    mfxU16 NewLevel          = par.mfx.CodecLevel;

    mfxU16 lidx = LevelIdx(par.mfx.CodecLevel);
    mfxU16 tidx = TierIdx(par.mfx.CodecLevel);

    if (tidx > MaxTidx(lidx))
        tidx = 0;

    while (lidx < MaxLidx)
    {
        mfxU32 MaxLumaPs   = TableA1[lidx][0];    
        mfxU32 MaxCPB      = TableA1[lidx][1+tidx];
        mfxU32 MaxSSPP     = TableA1[lidx][3];    
        //mfxU32 MaxTileRows = TableA1[lidx][4];    
        //mfxU32 MaxTileCols = TableA1[lidx][5];    
        mfxU32 MaxLumaSr   = TableA2[lidx][0];    
        mfxU32 MaxBR       = TableA2[lidx][1+tidx];
        //mfxU32 MinCR       = TableA2[lidx][3];    
        mfxU32 MaxDpbSize  = GetMaxDpbSize(PicSizeInSamplesY, MaxLumaPs, 6);
        
        if (   PicSizeInSamplesY > MaxLumaPs
            || par.m_ext.HEVCParam.PicWidthInLumaSamples > sqrt(MaxLumaPs * 8)
            || par.m_ext.HEVCParam.PicHeightInLumaSamples > sqrt(MaxLumaPs * 8)
            || (mfxU32)par.mfx.NumRefFrame + 1 > MaxDpbSize
            //|| (mfxU32)par.m_pps.num_tile_columns_minus1 + 1 > MaxTileCols
            //|| (mfxU32)par.m_pps.num_tile_rows_minus1 + 1 > MaxTileRows
            || (mfxU32)par.mfx.NumSlice > MaxSSPP)
        {
            lidx ++;
            continue;
        }

        //if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        {
            if (   par.BufferSizeInKB * 8000 > CpbBrNalFactor * MaxCPB
                || LumaSr > MaxLumaSr
                || par.MaxKbps * 1000 > CpbBrNalFactor * MaxBR)
            {
                if (tidx >= MaxTidx(lidx))
                {
                    lidx ++;
                    tidx = 0;
                }
                else
                    tidx ++;

                continue;
            }
        }

        break;
    }

    NewLevel = MfxLevel(lidx, tidx);

    if (par.mfx.CodecLevel != NewLevel)
    {
        if (par.mfx.CodecLevel)
            sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        if (!query)
            par.mfx.CodecLevel = NewLevel;
    }

    return sts;
}

mfxStatus CheckVideoParam(MfxVideoParam& par, ENCODE_CAPS_HEVC const & caps)
{
    mfxU32 unsupported = 0, changed = 0, incompatible = 0;
    mfxStatus sts = MFX_ERR_NONE;

    if (!IsAligned(par.mfx.FrameInfo.Width, HW_SURF_ALIGN_W))
    {
        par.mfx.FrameInfo.Width = Align(par.mfx.FrameInfo.Width, HW_SURF_ALIGN_W);
        changed ++;
    }

    if (!IsAligned(par.mfx.FrameInfo.Height, HW_SURF_ALIGN_H))
    {
        par.mfx.FrameInfo.Height = Align(par.mfx.FrameInfo.Width, HW_SURF_ALIGN_H);
        changed ++;
    }

    if (   par.m_ext.HEVCParam.PicWidthInLumaSamples  > par.mfx.FrameInfo.Width 
        || par.m_ext.HEVCParam.PicHeightInLumaSamples > par.mfx.FrameInfo.Height
        || !IsAligned(par.m_ext.HEVCParam.PicWidthInLumaSamples,  CODED_PIC_ALIGN_W)
        || !IsAligned(par.m_ext.HEVCParam.PicHeightInLumaSamples, CODED_PIC_ALIGN_H))
    {
        par.m_ext.HEVCParam.PicWidthInLumaSamples  = 0;
        par.m_ext.HEVCParam.PicHeightInLumaSamples = 0;
        unsupported ++;
    }

    if (par.mfx.FrameInfo.Width > caps.MaxPicWidth)
    {
        par.mfx.FrameInfo.Width = 0;
        unsupported ++;
    }
        
    if (par.mfx.FrameInfo.Height > caps.MaxPicHeight)
    {
        par.mfx.FrameInfo.Height = 0;
        unsupported ++;
    }

    if (par.mfx.FrameInfo.BitDepthLuma > 8)
    {
        par.mfx.FrameInfo.BitDepthLuma = 0;
        unsupported ++;
    }

    if (par.mfx.FrameInfo.BitDepthChroma && par.mfx.FrameInfo.BitDepthChroma != par.mfx.FrameInfo.BitDepthLuma)
    {
        par.mfx.FrameInfo.BitDepthChroma = 0;
        unsupported ++;
    }

    if (par.mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED)
    {
        par.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
        changed ++;
    }

    if (par.mfx.GopRefDist > 1 && caps.SliceIPOnly)
    {
        par.mfx.GopRefDist = 1;
        changed ++;
    }

    if (par.Protected != 0)
    {
        par.Protected = 0;
        unsupported ++;
    }

    if (par.IOPattern != 0)
    {
        if ((par.IOPattern & MFX_IOPATTERN_IN_MASK) != par.IOPattern)
        {
            par.IOPattern &= MFX_IOPATTERN_IN_MASK;
            changed ++;
        }

        if (   par.IOPattern != MFX_IOPATTERN_IN_VIDEO_MEMORY
            && par.IOPattern != MFX_IOPATTERN_IN_SYSTEM_MEMORY
            && par.IOPattern != MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
            changed ++;
        }
    }

    if (   par.mfx.GopPicSize > 0
        && par.mfx.GopRefDist > 1
        && par.mfx.GopRefDist >= par.mfx.GopPicSize)
    {
        par.mfx.GopRefDist = par.mfx.GopPicSize - 1;
        changed ++;
    }

    switch (par.mfx.RateControlMethod)
    {
    case 0:
    case MFX_RATECONTROL_CBR:
    case MFX_RATECONTROL_VBR:
    case MFX_RATECONTROL_CQP:
        break;
    default:
        par.mfx.RateControlMethod = 0;
        changed ++;
        break;
    }

    if (   par.mfx.FrameInfo.PicStruct != 0 
        && par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
    {
        par.mfx.FrameInfo.PicStruct = 0;
        changed ++;
    }

    if (par.m_ext.HEVCParam.PicWidthInLumaSamples > 0)
    {
        if (par.mfx.FrameInfo.CropX > par.m_ext.HEVCParam.PicWidthInLumaSamples)
        {
            par.mfx.FrameInfo.CropX = 0;
            incompatible ++;
        }

        if (par.mfx.FrameInfo.CropX + par.mfx.FrameInfo.CropW > par.m_ext.HEVCParam.PicWidthInLumaSamples)
        {
            par.mfx.FrameInfo.CropW = par.m_ext.HEVCParam.PicWidthInLumaSamples - par.mfx.FrameInfo.CropX;
            incompatible ++;
        }
    }

    if (par.m_ext.HEVCParam.PicHeightInLumaSamples > 0)
    {
        if (par.mfx.FrameInfo.CropY > par.m_ext.HEVCParam.PicHeightInLumaSamples)
        {
            par.mfx.FrameInfo.CropY = 0;
            incompatible ++;
        }

        if (par.mfx.FrameInfo.CropY + par.mfx.FrameInfo.CropH > par.m_ext.HEVCParam.PicHeightInLumaSamples)
        {
            par.mfx.FrameInfo.CropH = par.m_ext.HEVCParam.PicHeightInLumaSamples - par.mfx.FrameInfo.CropY;
            incompatible ++;
        }
    }

    if (par.mfx.NumSlice != 0)
    {
        par.mfx.NumSlice = 0;
        unsupported ++;
    }

    if (   par.mfx.FrameInfo.ChromaFormat != 0
        && par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
    {
        par.mfx.FrameInfo.ChromaFormat = 0;
        unsupported ++;
    }
        
    if (   par.mfx.FrameInfo.FourCC != 0
        && par.mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
    {
        par.mfx.FrameInfo.FourCC = 0;
        unsupported ++;
    }

    if ((par.mfx.FrameInfo.FrameRateExtN == 0) !=
        (par.mfx.FrameInfo.FrameRateExtD == 0))
    {
        par.mfx.FrameInfo.FrameRateExtN = 0;
        par.mfx.FrameInfo.FrameRateExtD = 0;
        incompatible ++;
    }

    if (par.NumRefLX[0] > caps.MaxNum_Reference0)
    {
        par.NumRefLX[0] = caps.MaxNum_Reference0;
        changed ++;
    }
        
    if (par.NumRefLX[1] > caps.MaxNum_Reference1)
    {
        par.NumRefLX[1] = caps.MaxNum_Reference1;
        changed ++;
    }

    if (   par.mfx.RateControlMethod == MFX_RATECONTROL_VBR
        && par.MaxKbps != 0
        && par.TargetKbps != 0
        && par.MaxKbps < par.TargetKbps)
    {
        par.MaxKbps = par.TargetKbps;
        changed ++;
    }

    if (par.BufferSizeInKB != 0)
    {
        mfxU32 rawBytes = par.m_ext.HEVCParam.PicWidthInLumaSamples * par.m_ext.HEVCParam.PicHeightInLumaSamples * 3 / 2 / 1000;

        if (   par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
            && par.BufferSizeInKB < rawBytes)
        {
            par.BufferSizeInKB = rawBytes;
            changed ++;
        }
        else if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        {
            mfxF64 fr = par.mfx.FrameInfo.FrameRateExtD ? (mfxF64)par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD : 30.;
            mfxU32 avgFS = Ceil((mfxF64)par.TargetKbps / fr / 8);

            if (par.BufferSizeInKB < avgFS * 2 + 1)
            {
                par.BufferSizeInKB = avgFS * 2 + 1;
                changed ++;
            }
        }
    }

    sts = CheckProfile(par);
        
    if (sts >= MFX_ERR_NONE)
        sts = CorrectLevel(par, true);

    if (sts == MFX_ERR_NONE && changed)
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    if (sts >= MFX_ERR_NONE && unsupported)
        sts = MFX_ERR_UNSUPPORTED;

    if (sts >= MFX_ERR_NONE && incompatible)
        sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    return sts;
}

void SetDefaults(
    MfxVideoParam &          par,
    ENCODE_CAPS_HEVC const & hwCaps)
{
    mfxU64 rawBits = (mfxU64)par.m_ext.HEVCParam.PicWidthInLumaSamples * par.m_ext.HEVCParam.PicHeightInLumaSamples * 3 / 2 * 8;
    mfxF64 maxFR   = 300.;
    mfxU32 maxBR   = 0xFFFFFFFF;
    mfxU32 maxBuf  = 0xFFFFFFFF;
    mfxU32 maxDPB  = 16;

    if (par.mfx.CodecLevel)
    {
        maxFR  = GetMaxFrByLevel(par);
        maxBR  = GetMaxKbpsByLevel(par);
        maxBuf = GetMaxCpbInKBByLevel(par);
        maxDPB = GetMaxDpbSizeByLevel(par);
    }
    
    if (!par.AsyncDepth)
        par.AsyncDepth = 1;

    if (!par.mfx.CodecProfile)
        par.mfx.CodecProfile = mfxU16(par.mfx.FrameInfo.BitDepthLuma > 8 ? MFX_PROFILE_HEVC_MAIN10 : MFX_PROFILE_HEVC_MAIN);

    if (!par.mfx.TargetUsage)
        par.mfx.TargetUsage = 4;

    if (!par.mfx.NumSlice)
        par.mfx.NumSlice = 1;

    if (!par.mfx.FrameInfo.FourCC)
        par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

    if (!par.mfx.FrameInfo.CropW)
        par.mfx.FrameInfo.CropW = par.m_ext.HEVCParam.PicWidthInLumaSamples - par.mfx.FrameInfo.CropX;

    if (!par.mfx.FrameInfo.CropH)
        par.mfx.FrameInfo.CropH = par.m_ext.HEVCParam.PicHeightInLumaSamples - par.mfx.FrameInfo.CropY;

    if (!par.mfx.FrameInfo.FrameRateExtN || !par.mfx.FrameInfo.FrameRateExtD)
    {
        par.mfx.FrameInfo.FrameRateExtD = 1001;
        par.mfx.FrameInfo.FrameRateExtN = mfxU32(Min(30000./par.mfx.FrameInfo.FrameRateExtD, maxFR) * par.mfx.FrameInfo.FrameRateExtD);
    }

    if (!par.mfx.FrameInfo.AspectRatioW)
        par.mfx.FrameInfo.AspectRatioW = 1;

    if (!par.mfx.FrameInfo.AspectRatioH)
        par.mfx.FrameInfo.AspectRatioH = 1;
    
    if (!par.mfx.FrameInfo.PicStruct)
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    if (!par.mfx.FrameInfo.ChromaFormat)
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    if (!par.mfx.FrameInfo.BitDepthLuma)
        par.mfx.FrameInfo.BitDepthLuma = 8;

    if (!par.mfx.FrameInfo.BitDepthChroma)
        par.mfx.FrameInfo.BitDepthChroma = par.mfx.FrameInfo.BitDepthLuma;

    if (!par.mfx.RateControlMethod)
        par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        if (!par.mfx.QPI)
            par.mfx.QPI = 26;
        if (!par.mfx.QPP)
            par.mfx.QPP = 26;
        if (!par.mfx.QPB)
            par.mfx.QPB = 26;

        if (!par.BufferSizeInKB)
            par.BufferSizeInKB = Min(maxBuf, mfxU32(rawBits / 8000));
    }
    else if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
    {
        if (!par.TargetKbps)
            par.TargetKbps = Min(maxBR, mfxU32(rawBits * par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD / 150000));
        if (!par.MaxKbps)
            par.MaxKbps = par.TargetKbps;
        if (!par.BufferSizeInKB)
            par.BufferSizeInKB = Min(maxBuf, par.MaxKbps * 8 / 2);
        if (!par.InitialDelayInKB)
            par.InitialDelayInKB = par.BufferSizeInKB / 2;
    }
    
    if (!par.mfx.GopOptFlag)
        par.mfx.GopOptFlag = MFX_GOP_CLOSED;

    if (!par.mfx.GopPicSize)
        par.mfx.GopPicSize = (par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAINSP ? 1 : 0xFFFF);

    if (!par.NumRefLX[1] && par.mfx.GopRefDist != 1)
        par.NumRefLX[1] = MFX_MIN(1, hwCaps.MaxNum_Reference1);

    if (!par.NumRefLX[0])
    {
        par.NumRefLX[0] = MFX_MIN(2, hwCaps.MaxNum_Reference0);

        if (mfxU16(maxDPB - 1) < par.NumRefLX[0] + par.NumRefLX[1])
        {
            par.NumRefLX[0] = mfxU16(maxDPB - 1);

            if (par.NumRefLX[0] > 1 && hwCaps.MaxNum_Reference1 > 0)
            {
                par.NumRefLX[0] -= 1;
                par.NumRefLX[1]  = 1;
            }
            par.NumRefLX[0] = Min(par.NumRefLX[0], mfxU16(hwCaps.MaxNum_Reference0));
        }
    }

    //if (!par.LTRInterval && par.NumRefLX[0] > 1 && par.mfx.GopPicSize > 32)
    //    par.LTRInterval = 16;

    if (!par.mfx.NumRefFrame)
        par.mfx.NumRefFrame = par.NumRefLX[0] + par.NumRefLX[1];

    if (!par.mfx.GopRefDist)
        par.mfx.GopRefDist = mfxU16((par.mfx.GopPicSize > 1 && par.NumRefLX[1] && !hwCaps.SliceIPOnly) ? Min(par.mfx.GopPicSize - 1, 3) : 1);

    if (!par.mfx.CodecLevel)
        CorrectLevel(par, false);
}

} //namespace MfxHwH265Encode